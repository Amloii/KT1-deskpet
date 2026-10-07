/*
 * ===========================================================================
 *  KT1 DeskPet — desk companion + movement coach
 * ===========================================================================
 *  Board  : Freenove ESP32-S3 Display FNK0104B (2.8" 240x320 ILI9341 + FT6336U)
 *  Extra  : GY-521 (MPU6050) on the board I2C connector (IO16/IO15), TTP223 (GPIO 2),
 *           DHT11 (GPIO 14), LDR KY-018 (GPIO 3)
 *  PC     : pc-agent (Python) -> POST http://kt1.local/pet  ·  bridge.py (voice/OpenCode)
 *
 *  Pages (arrows < >):
 *    FACE · EXERCISE · POMODORO · PLANTS · VITAL · WEATHER · CLOCK · CHAT · SOUND · SCREEN · TIMERS
 *
 *  "Active focus" cycle (Cornell Ergonomics 20-8-2):
 *    20 min sitting -> 8 min standing -> 2 min active break with dumbbells.
 *    Every 4 cycles, a long break (legs + back + arms + mobility circuit).
 *    Does not interrupt during calls or mid-sentence (pc-agent data).
 *
 *  Code split into modules (.h in this folder):
 *    sound.h  speaker beeps (ES8311)              net.h    Wi-Fi, APIs, server
 *    imu.h    MPU6050, shakes, moving desk        face.h   eyes and moods
 *    focus.h  cycle, coach, statistics            pages.h  dashboard pages
 *    exercises.h  exercise library (editable)
 *    pomodoro.h · plants.h · vital.h · pet.h · touch.h
 *    chat.h   voice with Gemini + TTS        remote.h bridge.py client
 *
 *  Arduino IDE -> Tools: ESP32S3 Dev Module · USB CDC On Boot Enabled ·
 *  OPI PSRAM · Flash 16MB · Partition Huge APP (3MB No OTA/1MB SPIFFS)
 * ===========================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <HTTPUpdate.h>   // OTA: update() against http://PC:8750/firmware/kt1.bin
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include <Preferences.h>
#include <time.h>
#include <math.h>
#include "esp_heap_caps.h"
#include "ESP_I2S.h"
#include "es8311.h"
#include "Freenove_WS2812_Lib_for_ESP32.h"
// Note: the FT6336U touch is read directly over I2C (touch.h). The Freenove FT6336U
// library retries in an endless loop if a read fails and froze KT1.
#include "exercises.h"

#if __has_include("secrets.h")
  #include "secrets.h"
#else
  #error "Missing secrets.h: copy secrets.example.h to secrets.h and fill in your data"
#endif

// ===========================================================================
//  CONFIGURATION (edit here)
// ===========================================================================
// ---- Location and time (CITY_NAME, LATITUDE and LONGITUDE are defined in secrets.h) ----
#if !defined(CITY_NAME) || !defined(LATITUDE) || !defined(LONGITUDE)
  #error "Define CITY_NAME, LATITUDE and LONGITUDE in secrets.h (see secrets.example.h)"
#endif
#ifndef TZ_INFO
#define TZ_INFO          "CET-1CEST,M3.5.0,M10.5.0/3"   // mainland Spain; can be redefined in secrets.h
#endif
#define NTP_SERVER_1     "pool.ntp.org"
#define NTP_SERVER_2     "time.google.com"

// ---- Language (first boot; then changed on the Screen settings page and saved in NVS) ----
#ifndef DEFAULT_LANG
#define DEFAULT_LANG     LANG_EN  // LANG_EN or LANG_ES
#endif

// ---- Screen ----
#define SCREEN_ROTATION  1        // 1 or 3 (landscape)
#define SCR_W            320
#define SCR_H            240
// Brightness: 0 = backlight always on (like TaskManager/Ente) and brightness is simulated
// by dimming the ink color. 1 = real PWM on GPIO 45 (on this board it left the screen black).
#define BL_PWM           0

// ---- FT6336U touch (internal bus, Wire = I2C0, shared with the codec) ----
#define TOUCH_SDA        16
#define TOUCH_SCL        15
#define TOUCH_RST        18
#define TOUCH_INT        17
#define TOUCH_FLIP_X     0
#define TOUCH_FLIP_Y     0
#define TOUCH_DEBUG      0        // 1 = on-screen diagnostic line + crosshair + log on press/release

// ---- IMU GY-521 / MPU6050 ----
// Plugged into the board I2C connector (3.3V, GND, IO16 = SDA, IO15 = SCL): shares the bus with
// the touch (0x38) and the codec (0x18); no conflict since each has its own address.
// If you ever move it to other pins (e.g. 2/14), a separate bus (Wire1) is used automatically.
#define IMU_ENABLED      1
#define IMU_SDA          16
#define IMU_SCL          15
#define IMU_ADDR         0x68
#define IMU_EYE_X_AXIS   1
#define IMU_EYE_X_SIGN   1
#define IMU_EYE_Y_AXIS   0
#define IMU_EYE_Y_SIGN   1
#define IMU_GAIN_X       120.0f
#define IMU_GAIN_Y       90.0f
#define IMU_SHAKE_G      1.10f     // real strong shake (avoids false positives when tapping)
#define IMU_DEBUG        0

// ---- TTP223 petting touch sensor (DO high while touched) ----
// Module AT 3.3V (at 5V its DO would output 5V and burn the GPIO). GPIO 2 verified
// free; do not use 9/21 (21 is pulled to GND on this board).
#define PET_ENABLED    1
#define PET_PIN        2
#define PET_TAP_MS     400      // short tap = greeting
#define PET_CARESS_MS  900      // hold = love + purr

// ---- Plants v2: DHT11 (temp+humidity) + LDR KY-018 (light) ----
// Digital DHT11 on GPIO14, analog LDR on GPIO3 (ADC1, usable with WiFi).
// LDR with a 10k divider to VCC: 3.3V -> 10k -> AO -> LDR -> GND (AO drops with light).
#define PLANT_ENABLED  1
#define DHT_PIN        14
#define LDR_PIN        3
#define PLANT_MEAS_MS  8000UL   // measures ~8 s and averages

// ---- Pomodoro v3 (configurable in Settings + inline, saved in NVS) ----
#define POMO_FOCUS_DEFAULT 25   // min
#define POMO_BREAK_DEFAULT 10   // min
#define POMO_LONG_DEFAULT 20    // min (every 4th break)

// ---- Posture: manual signal only (buttons, face, sensor, pages).
// (There used to be an IMU desk vibration detector: it gave false
// positives with the speaker/fan and was removed.)

// ---- Sound (ES8311 codec + board speaker) ----
#define SOUND_ENABLED    1
#define SOUND_VOLUME     70       // codec volume 0-100
#define SOUND_AMP        9000     // tone amplitude (max 32767)
#define AMP_PIN          1        // amplifier AP_ENABLE
#define AMP_ON_LEVEL     LOW      // if nothing sounds, try HIGH
#define I2S_MCK          4
#define I2S_BCK          5
#define I2S_DIN          6
#define I2S_WS           7
#define I2S_DOUT         8

// ---- RGB LED ----
#define LED_ENABLED      1
#define LED_PIN          42
#define LED_BRIGHT       12

// ---- Internet data ----
#define WEATHER_EVERY_MS (15UL * 60UL * 1000UL)
#define GITHUB_EVERY_MS  (30UL * 60UL * 1000UL)
#define RETRY_MS         (60UL * 1000UL)

// ---- "Active focus" cycle (see README §Health: Cornell 20-8-2, Buckley 2015, WHO/NHS) ----
#define SIT_MIN            20     // sitting work
#define STAND_MIN          8      // standing work
#define MOVE_MIN           2      // active break (1 set)
#define LONG_EVERY         4      // every N cycles, long break (~2 h)
#define LONGBREAK_MIN           10     // max length of the long break
#define CIRCUIT_ROUNDS     1      // long-break circuit rounds (1-2)
#define REST_SEC           30     // rest between circuit exercises
#define STAND_GOAL_MIN     120    // daily standing + movement goal (Buckley: 2 h -> 4 h)
#define STRENGTH_DAYS_GOAL 2      // strength days/week for all groups (WHO/NHS)
#define SIT_TOO_LONG_MIN   45     // sitting longer than this in a row -> sad eyes
// v2: no pc-agent. Everything local: manual start, no waiting for calls/typing.
#define WORK_START_H       8
#define WORK_END_H         20
#define WORK_WEEKDAYS_ONLY 1
#define AUTO_START_WITH_PC 0
#define FOCUS_GRACE_MIN    0
#define CALL_DEFER_MAX_MIN 0
#define RENUDGE_MIN        3      // repeat the nudge if ignored (max 3 times)

// ---- Anti-sedentary (complements the 20-8-2 cycle: safety net if you
//      ignore the stand-up nudge and keep sitting) ----
#define SED_EVERY_DEFAULT  45     // default interval (min sitting in a row)
#define SED_OFF_MIN        0      // value meaning "disabled"

// ---- pc-agent ----
#define MDNS_NAME          "kt1"            // -> http://kt1.local
#define FW_VERSION         "3.5.0"          // OTA: the bridge serves the .bin if the version differs
#define AGENT_TIMEOUT_MS   15000UL          // no messages -> agent disconnected
#define AWAY_IDLE_S        300              // 5 min without keyboard/mouse -> "away"

// ---- General behavior ----
#define AUTO_RETURN_MS   90000UL
#define MANUAL_MOOD_MS   20000UL
#define SLEEPY_IDLE_MS   (10UL * 60UL * 1000UL)
#define NIGHT_START_H    23
#define NIGHT_END_H      7

// ---- Gestures ----
#define SWIPE_MIN_PX     55
#define TAP_MOVE_PX      18
#define TAP_MAX_MS       450
#define LONG_PRESS_MS    800
#define RELEASE_MS       50

static_assert(SCREEN_ROTATION == 1 || SCREEN_ROTATION == 3, "Use SCREEN_ROTATION 1 or 3");

// ===========================================================================
//  TYPES (at the very top: the Arduino IDE generates prototypes that use them)
// ===========================================================================
enum Page    { PAGE_FACE, PAGE_EXERCISE, PAGE_POMO, PAGE_PLANTS, PAGE_VITAL, PAGE_WEATHER,
               PAGE_CLOCK, PAGE_CHAT,
               PAGE_SET_SOUND, PAGE_SET_SCREEN, PAGE_SET_TIME, PAGE_COUNT };
enum Mood    { M_NEUTRAL, M_HAPPY, M_LOVE, M_SURPRISED, M_ANGRY, M_SAD, M_SLEEPY, M_DIZZY, M_SPORT, M_THINK, M_COUNT };
enum Gesture { G_NONE, G_TAP, G_LONG, G_SWIPE_L, G_SWIPE_R, G_SWIPE_U, G_SWIPE_D };
enum NetState{ NET_CONNECTING, NET_OK, NET_FAIL };
enum Phase   { PH_OFF, PH_SIT, PH_STAND, PH_MOVE, PH_LONG, PH_AWAY };
enum Posture { POS_SIT, POS_STAND };
enum CoachState { CO_OFF, CO_READY, CO_GO, CO_WORK, CO_SWITCH, CO_REST, CO_DONE };
enum Sound : uint8_t { SND_TICK, SND_GO, SND_SIDE, SND_STAND, SND_MOVE, SND_SIT, SND_DONE, SND_HELLO, SND_PURR,
  SND_PW_GO, SND_PE_GO, SND_PO_GO, SND_PW_END, SND_PE_END, SND_PO_END };

struct WeatherData {
  bool  ok = false;
  float temp = NAN, feels = NAN, wind = NAN, tmax = NAN, tmin = NAN;
  int   hum = -1, code = -1, isDay = 1, pop = -1;
  char  sunrise[6] = "--:--", sunset[6] = "--:--";
  time_t updated = 0;
  char  err[40] = "";
};
struct GithubData {
  bool ok = false;
  char login[40] = "", name[40] = "";
  int  repos = 0, followers = 0, following = 0, stars = -1;
  time_t updated = 0;
  char err[40] = "";
};
struct AgentData {                 // what pc-agent sends
  uint32_t lastMs = 0;
  char category[12] = "";          // "classic" category (idle if inactive)
  char fgCategory[12] = "";        // actual category of the foreground app
  char emotion[10] = "";
  char app[32] = "";
  int  idle = 0;                   // seconds without keyboard/mouse
  bool call = false;               // microphone in use (call / meeting)
  bool greet = false;              // the PC asks for a greeting (back from idle)
};
struct DayStats {                  // saved in NVS
  uint32_t day = 0;                // YYYYMMDD
  uint32_t sitS = 0, standS = 0, awayS = 0, moveS = 0;
  uint16_t breaksDone = 0, breaksSkip = 0, changes = 0;
  uint16_t sets[GR_COUNT] = { 0, 0, 0, 0 };
};
struct WeekStats {
  uint32_t week = 0;               // YYYY*100 + ISO week
  uint8_t  mask[7] = { 0 };        // bits of groups trained per day (Mon..Sun)
};
struct Nudge {
  bool active = false;
  char text[34] = "";
  Posture target = POS_STAND;
  uint32_t since = 0, lastRing = 0;
  int rings = 0;
};
struct Coach {
  bool active = false;
  CoachState st = CO_OFF;
  bool circuit = false;
  int  list[10];
  int  n = 0, pos = 0, ex = 0;
  int  rep = 0, side = 0, lastCount = -1;
  uint32_t t0 = 0, lastBeat = 0;
  bool anyDone = false;
};
struct EyeShape { float w, h, r, dy; };
struct TouchState { bool down = false; int x0, y0, x, y; uint32_t t0, lastSeen; bool longFired; };

// ===========================================================================
//  OBJECTS AND GLOBAL STATE
// ===========================================================================
TFT_eSPI     tft = TFT_eSPI();

// 16-bit framebuffer FORCED INTO PSRAM.
// On ESP32 core 3.x (IDF 5) TFT_eSPI does not recognize CONFIG_SPIRAM_SUPPORT, so
// createSprite() would put the 153 KB canvas in internal RAM and starve HTTPS TLS
// (weather/github would give "connection refused"). Here we replicate createSprite()
// but allocate the buffer in PSRAM, leaving internal RAM free for Wi-Fi/TLS.
class PsramSprite : public TFT_eSprite {
public:
  PsramSprite(TFT_eSPI* t) : TFT_eSprite(t) {}
  bool createInPsram(int16_t w, int16_t h) {
    if (_created) return true;
    _bpp = 16;
    size_t bytes = (size_t)w * h * 2 + 2;
    uint8_t* buf = (uint8_t*) heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) return false;
    memset(buf, 0, bytes);
    _iwidth = _dwidth = _bitwidth = w;
    _iheight = _dheight = h;
    cursor_x = cursor_y = 0;
    _sx = _sy = 0; _sw = w; _sh = h; _scolor = TFT_BLACK;
    _img8 = _img8_1 = _img8_2 = buf;
    _img  = (uint16_t*) buf; _img4 = buf;
    _created = true;
    rotation = 0;
    setViewport(0, 0, _dwidth, _dheight);
    setPivot(_iwidth / 2, _iheight / 2);
    return true;
  }
};
PsramSprite  spr(&tft);                     // 16-bit RGB565 canvas in PSRAM (150 KB)
Preferences  prefs;
WebServer    server(80);
#if LED_ENABLED
Freenove_ESP32_WS2812 led(1, LED_PIN, 0, TYPE_GRB);
#endif

// ---------------------------------------------------------------------------
//  PALETTE / THEME (RGB565). Rendering is 16-bit color.
//  "Brightness" dims the whole palette in software (backlight PWM left the
//  screen black on this board), and "Ink" now picks the ACCENT color.
// ---------------------------------------------------------------------------
constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
// Undimmed base colors (dark dashboard theme)
const uint16_t C_BG     = rgb(  9,  12,  20);   // bluish near-black background
const uint16_t C_CARD   = rgb( 26,  33,  49);   // cards / boxes
const uint16_t C_LINE   = rgb( 52,  64,  88);   // separators / borders
const uint16_t C_INK    = rgb(232, 238, 244);   // main text
const uint16_t C_INKDIM = rgb(140, 153, 168);   // secondary text
const uint16_t C_OK     = rgb( 52, 211, 153);   // green
const uint16_t C_WARN   = rgb(251, 191,  36);   // amber
const uint16_t C_DANGER = rgb(248, 113, 113);   // red
const uint16_t C_INFO   = rgb( 96, 165, 250);   // blue
const uint16_t ACCENTS[] = { rgb(34, 211, 238), rgb(251, 191, 36), rgb(52, 211, 153), rgb(244, 114, 182) };
const int      INK_COUNT   = 4;
const char* const INK_NAMES_L[LANG_COUNT][INK_COUNT] = {
  { "Cyan", "Amber", "Green", "Pink" },
  { "Cian", "Ambar", "Verde", "Rosa" },
};
#define INK_NAMES (INK_NAMES_L[gLang])

struct Pal { uint16_t bg, card, line, ink, inkDim, accent, ok, warn, danger, info; };
Pal      P = { C_BG, C_CARD, C_LINE, C_INK, C_INKDIM, ACCENTS[0], C_OK, C_WARN, C_DANGER, C_INFO };
uint16_t INK   = C_INK;     // compatibility: foreground color (text/lines)
uint16_t PAPER = C_BG;      // compatibility: background color
#if BL_PWM
const uint8_t  BRIGHT_LEVELS[] = { 6, 30, 80, 160, 255 };     // backlight PWM duty
#else
const uint8_t  BRIGHT_LEVELS[] = { 45, 85, 135, 195, 255 };   // ink intensity (0-255)
#endif
const int      BRIGHT_COUNT    = 5;

// UI tables, one row per language (lang.h): TABLE[gLang][i]
const char* const PAGE_NAMES_L[LANG_COUNT][PAGE_COUNT] = {
  { "Face", "Exercise", "Pomodoro", "Plants", "Vital", "Weather", "Clock", "Chat", "Sound", "Screen", "Timers" },
  { "Cara", "Ejercicio", "Pomodoro", "Plantas", "Vital", "Tiempo", "Reloj", "Chat", "Sonido", "Pantalla", "Tiempos" },
};
const char* const DAYS_L[LANG_COUNT][7] = {
  { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" },
  { "Domingo", "Lunes", "Martes", "Miercoles", "Jueves", "Viernes", "Sabado" },
};
const char* const MONTHS_L[LANG_COUNT][12] = {
  { "January", "February", "March", "April", "May", "June", "July",
    "August", "September", "October", "November", "December" },
  { "Enero", "Febrero", "Marzo", "Abril", "Mayo", "Junio", "Julio",
    "Agosto", "Septiembre", "Octubre", "Noviembre", "Diciembre" },
};
const char* const MONTHS_SHORT_L[LANG_COUNT][12] = {
  { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" },
  { "Ene", "Feb", "Mar", "Abr", "May", "Jun", "Jul", "Ago", "Sep", "Oct", "Nov", "Dic" },
};
// Weekday initials, Monday first
const char* const WEEK_INITIALS_L[LANG_COUNT][7] = {
  { "M", "T", "W", "T", "F", "S", "S" },
  { "L", "M", "X", "J", "V", "S", "D" },
};
#define PAGE_NAMES    (PAGE_NAMES_L[gLang])
#define DAYS          (DAYS_L[gLang])
#define MONTHS        (MONTHS_L[gLang])
#define MONTHS_SHORT  (MONTHS_SHORT_L[gLang])
#define WEEK_INITIALS (WEEK_INITIALS_L[gLang])

// Data shared between tasks (protect with dataMtx)
SemaphoreHandle_t dataMtx;
WeatherData gWeather;
GithubData  gGithub;
AgentData   gAgent;
volatile NetState gNet = NET_CONNECTING;
volatile bool reqWeather = false, reqGithub = false;
volatile bool serverUp = false;

// UI state (core 1 only, except what is marked volatile)
int      gPage = PAGE_FACE;
int      gInk = 0, gBright = 3, gVolume = 70;
bool     gSoundOn = true;
bool     gDirty = true;
uint32_t lastInteraction = 0;
char     toastText[40] = "";
uint32_t toastUntil = 0;
TouchState ts;

// Focus / coach / statistics
Phase    gPhase = PH_OFF;
Posture  gPosture = POS_SIT;
bool     gPaused = false;
uint32_t phaseStart = 0, pauseStart = 0, deferSince = 0, sitSince = 0;
int      cycleNo = 0;
uint32_t autoStartDay = 0;
Nudge    nudge;
Coach    coach;
DayStats stats;
WeekStats week;
int      todayIdx = 0;
char     waitReason[34] = "";

// Anti-sedentary (configurable in Settings, saved in NVS)
int      sedEveryMin = SED_EVERY_DEFAULT;  // 0 = OFF, otherwise 15/30/45/60
bool     sedEnabled = true;
uint32_t sedLastMoveMs = 0;                // last non-sedentary moment (standing / moving / away)

// IMU (written by the IMU task: gaze + shakes)
volatile float imuGazeX = 0, imuGazeY = 0;
volatile uint32_t dizzyUntil = 0;
bool imuOk = false;

// Face
float    squash = 0;
Mood     manualMood = M_NEUTRAL;
uint32_t manualUntil = 0;

// ===========================================================================
//  FORWARD DECLARATIONS (used across modules)
// ===========================================================================
void toast(const char* s, uint32_t ms = 1200);
void sound(Sound s);
bool getLocal(struct tm& t);
bool isNight();
bool agentConnected();
AgentData agentSnap();
bool agentAway();
bool callActive();
bool isPresent();
// v2: pomodoro + plants + vital (keep running when you change page)
void pomoTick();
void pomoTap(int x, int y);
bool pomoRunning();
bool pomoIsFlow();
uint32_t pomoRemainS();
uint32_t pomoElapsed();
void plantTick();
void plantTap(int x, int y);
void vitalTick();
void vitalTap(int x, int y);
void vitalInit();
void confirmPosture(Posture p, bool fromDesk);
void coachOpen(bool circuit);
void coachClose();
void vitalStop();
void onSensorTap();
bool onSensorHold();
void sedTick();
void sedMarkMoved();
int sedRemainingS();
Mood currentMood();
void txt(const char* s, int x, int y, int font, uint8_t datum, uint16_t col = INK);
void thickLine(float x0, float y0, float x1, float y1, int r, uint16_t col = INK);
void changeBrightness(int delta, bool wrap);
void changeInk();
void toggleSound();
extern int inkLevel;
uint16_t dim565(uint16_t c, uint8_t lvl);
uint16_t tint(uint16_t base);
void buildPalette();

void toast(const char* s, uint32_t ms) {
  strlcpy(toastText, s, sizeof(toastText));
  toastUntil = millis() + ms;
  gDirty = true;
}

// ===========================================================================
//  TIME UTILITIES
// ===========================================================================
bool getLocal(struct tm& t) {
  time_t n = time(nullptr);
  if (n < 1700000000) return false;
  localtime_r(&n, &t);
  return true;
}
bool isNight() {
  struct tm t;
  if (!getLocal(t)) return false;
  return (t.tm_hour >= NIGHT_START_H || t.tm_hour < NIGHT_END_H);
}
bool inWorkHours() {
  struct tm t;
  if (!getLocal(t)) return false;
#if WORK_WEEKDAYS_ONLY
  if (t.tm_wday == 0 || t.tm_wday == 6) return false;
#endif
  return t.tm_hour >= WORK_START_H && t.tm_hour < WORK_END_H;
}
void fmtHM(time_t ts, char* out, size_t n) {
  if (ts < 1700000000) { strlcpy(out, "--:--", n); return; }
  struct tm t; localtime_r(&ts, &t);
  snprintf(out, n, "%02d:%02d", t.tm_hour, t.tm_min);
}
void fmtDur(uint32_t secs, char* out, size_t n) {         // "1h 05m" / "12m"
  uint32_t m = secs / 60;
  if (m >= 60) snprintf(out, n, "%luh %02lum", (unsigned long)(m / 60), (unsigned long)(m % 60));
  else         snprintf(out, n, "%lum", (unsigned long)m);
}
bool isLeap(int y) { return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0); }
int  daysInMonth(int y, int m0) {
  static const int d[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  return (m0 == 1 && isLeap(y)) ? 29 : d[m0];
}
int isoWeeksInYear(int y) {
  auto p = [](int yy) { return (yy + yy / 4 - yy / 100 + yy / 400) % 7; };
  return 52 + ((p(y) == 4 || p(y - 1) == 3) ? 1 : 0);
}
int isoWeek(const struct tm& t) {
  int wdayMon = (t.tm_wday + 6) % 7;
  int w = (t.tm_yday - wdayMon + 10) / 7;
  int y = t.tm_year + 1900;
  if (w < 1) return isoWeeksInYear(y - 1);
  if (w > isoWeeksInYear(y)) return 1;
  return w;
}

// ===========================================================================
//  MODULES
// ===========================================================================
#include "touch.h"
#include "sound.h"
#include "net.h"
#include "imu.h"
#include "face.h"
#include "pet.h"
#include "focus.h"
#include "pomodoro.h"
#include "plants.h"
#include "vital.h"
#include "remote.h"   // v3: bridge.py client (used by chat.h and pages.h)
#include "chat.h"     // v3: Gemini voice + TTS (uses soundAudioSuspend/Resume)
#include "pages.h"

// ===========================================================================
//  TOUCH + GESTURES
// ===========================================================================
void mapTouch(int tx, int ty, int& x, int& y) {
#if SCREEN_ROTATION == 1
  x = ty;        y = 239 - tx;
#else
  x = 319 - ty;  y = tx;
#endif
#if TOUCH_FLIP_X
  x = SCR_W - 1 - x;
#endif
#if TOUCH_FLIP_Y
  y = SCR_H - 1 - y;
#endif
  x = constrain(x, 0, SCR_W - 1);
  y = constrain(y, 0, SCR_H - 1);
}

Gesture pollTouch() {
  int rx = 0, ry = 0;
  bool pressed = touchRead(rx, ry);
  uint32_t now = millis();
  if (pressed) {
    int x, y;
    mapTouch(rx, ry, x, y);
#if TOUCH_DEBUG
    if (!ts.down) Serial.printf("[TOUCH] PRESS raw=(%d,%d) -> screen=(%d,%d)\n", rx, ry, x, y);
#endif
    if (!ts.down) { ts.down = true; ts.x0 = x; ts.y0 = y; ts.t0 = now; ts.longFired = false; }
    ts.x = x; ts.y = y; ts.lastSeen = now;
    if (!ts.longFired && now - ts.t0 > LONG_PRESS_MS &&
        abs(x - ts.x0) < TAP_MOVE_PX && abs(y - ts.y0) < TAP_MOVE_PX) {
      ts.longFired = true;
      return G_LONG;
    }
    return G_NONE;
  }
  if (ts.down && now - ts.lastSeen > RELEASE_MS) {
    ts.down = false;
#if TOUCH_DEBUG
    Serial.printf("[TOUCH] RELEASE at (%d,%d) after %lu ms, moved (%d,%d)\n", ts.x, ts.y,
                  (unsigned long)(ts.lastSeen - ts.t0), ts.x - ts.x0, ts.y - ts.y0);
#endif
    if (ts.longFired) return G_NONE;
    int dx = ts.x - ts.x0, dy = ts.y - ts.y0;
    if (abs(dx) >= SWIPE_MIN_PX && abs(dx) > abs(dy)) return dx < 0 ? G_SWIPE_L : G_SWIPE_R;
    if (abs(dy) >= SWIPE_MIN_PX)                        return dy < 0 ? G_SWIPE_U : G_SWIPE_D;
    if (abs(dx) < TAP_MOVE_PX && abs(dy) < TAP_MOVE_PX && ts.lastSeen - ts.t0 < TAP_MAX_MS) return G_TAP;
  }
  return G_NONE;
}

int inkLevel = 255;              // global color intensity (software brightness)

// Dims an RGB565 color to the given level (0-255)
uint16_t dim565(uint16_t c, uint8_t lvl) {
  if (lvl >= 255) return c;
  uint32_t r = (c >> 11) & 0x1F, g = (c >> 5) & 0x3F, b = c & 0x1F;
  r = r * lvl / 255; g = g * lvl / 255; b = b * lvl / 255;
  return (uint16_t)((r << 11) | (g << 5) | b);
}
// Applies the current brightness to a one-off color (outside the palette)
uint16_t tint(uint16_t base) { return dim565(base, inkLevel); }

// Rebuilds the live palette from the theme (gInk) and brightness (inkLevel)
void buildPalette() {
  uint8_t l = inkLevel;
  P.bg     = dim565(C_BG,     l);
  P.card   = dim565(C_CARD,   l);
  P.line   = dim565(C_LINE,   l);
  P.ink    = dim565(C_INK,    l);
  P.inkDim = dim565(C_INKDIM, l);
  P.accent = dim565(ACCENTS[gInk], l);
  P.ok     = dim565(C_OK,     l);
  P.warn   = dim565(C_WARN,   l);
  P.danger = dim565(C_DANGER, l);
  P.info   = dim565(C_INFO,   l);
  INK = P.ink; PAPER = P.bg;
}

void goPage(int delta) {
  gPage = (gPage + delta + PAGE_COUNT) % PAGE_COUNT;
  toast(PAGE_NAMES[gPage], 700);
  Serial.printf("[NAV] arrow -> page %d of %d (%s)\n", gPage + 1, PAGE_COUNT, PAGE_NAMES[gPage]);
}
void changeBrightness(int delta, bool wrap) {
  if (wrap) gBright = (gBright + delta + BRIGHT_COUNT) % BRIGHT_COUNT;
  else      gBright = constrain(gBright + delta, 0, BRIGHT_COUNT - 1);
  prefs.putUChar("bri", gBright);
  char b[24]; snprintf(b, sizeof(b), TR("Brightness %d/%d", "Brillo %d/%d"), gBright + 1, BRIGHT_COUNT);
  toast(b, 900);
}
void changeInk() {
  gInk = (gInk + 1) % INK_COUNT;
  buildPalette();
  prefs.putUChar("ink", gInk);
  char b[24]; snprintf(b, sizeof(b), TR("Theme: %s", "Tema: %s"), INK_NAMES[gInk]);
  toast(b, 1000);
}
void toggleSound() {
  gSoundOn = !gSoundOn; prefs.putBool("snd", gSoundOn);
  toast(gSoundOn ? TR("Sound on", "Sonido activado") : TR("Sound muted", "Sonido silenciado"));
  if (gSoundOn) sound(SND_HELLO);
}

void handleGesture(Gesture g) {
  lastInteraction = millis();
  gDirty = true;
  if (g == G_TAP) Serial.printf("[TOUCH] tap at (%d, %d)\n", ts.x, ts.y);   // helps calibrate
  if (coach.active) { coachGesture(g); return; }

  switch (g) {
    case G_SWIPE_L: goPage(+1); break;          // swipes still work if your touch panel reports them
    case G_SWIPE_R: goPage(-1); break;
    case G_SWIPE_U: changeBrightness(+1, false); break;
    case G_SWIPE_D: changeBrightness(-1, false); break;
    case G_TAP: {
      int nav = navHit(ts.x, ts.y);             // bottom < > buttons
      if (nav) { goPage(nav); break; }
      if (gPage == PAGE_FACE) {
        if (vitalAlertActive()) vitalConfirm();
        else if (nudge.active) confirmPosture(nudge.target, false);   // "done it"
        else              nextMoodByTap();
      }
      else if (gPage == PAGE_EXERCISE) focusTap(ts.x, ts.y);
      else if (gPage == PAGE_POMO)   pomoTap(ts.x, ts.y);
      else if (gPage == PAGE_PLANTS) plantTap(ts.x, ts.y);
      else if (gPage == PAGE_VITAL) vitalTap(ts.x, ts.y);
      else if (gPage == PAGE_SET_SOUND) setSoundTap(ts.x, ts.y);
      else if (gPage == PAGE_SET_SCREEN) setScreenTap(ts.x, ts.y);
      else if (gPage == PAGE_SET_TIME) setTimeTap(ts.x, ts.y);
      else if (gPage == PAGE_CHAT) chatTap(ts.x, ts.y);
      else if (gPage == PAGE_WEATHER) { reqWeather = true; toast(TR("Updating weather...", "Actualizando tiempo...")); }
      else if (gPage == PAGE_CLOCK) { toast(TR("Use the < > buttons", "Usa los botones < >"), 900); }
      else toast(TR("Use the < > buttons", "Usa los botones < >"), 900);
    } break;
    case G_LONG:
      if (gPage == PAGE_CHAT) { chatReplay(); break; }   // hold = listen again
      if (gPage == PAGE_EXERCISE) { cycleStop(); toast(TR("Exercise off", "Ejercicio apagado"), 1500); break; }
      if (gPage == PAGE_VITAL) { vitalTogglePause(); break; }
      changeInk();
      break;
    default: break;
  }
}

// ===========================================================================
//  Contextual TTP223 SENSOR (tap = main action, hold = strong action).
//  Priority: coach > posture nudge > vital alert > pomo > vital phase > plants.
//  With nothing active: the usual greeting / love. Always toast+sound (S6).
// ===========================================================================
void onSensorTap() {
  Serial.println("[PET] sensor tap");
  if (gPage == PAGE_CHAT) { chatReplay(); return; }   // v3: tap = listen again (with reply)
  if (coach.active) {
    switch (coach.st) {
      case CO_READY:  coachBegin(); break;                        // start
      case CO_WORK:   exerciseSideDone(); break;                  // done, next
      case CO_SWITCH: coach.t0 = millis() - 4000; break;          // continue now
      case CO_REST:   coachBegin(); break;                        // skip rest
      case CO_DONE:   coachClose(); break;
      default: break;
    }
    sound(SND_TICK);
    return;
  }
  if (nudge.active) { confirmPosture(nudge.target, false); return; }  // "done it"
  if (vitalAlertActive()) { vitalConfirm(); return; }                 // vital turn: confirm posture
  if (pomoRunning()) { pomoTogglePause(); sound(SND_TICK); return; }  // blind pause
  if (pomoState == POMO_READY) { pomoStartFocus(); return; }          // start the focus
  if (vitalPhase != VIT_OFF) { vitalAdvance(); return; }             // end relax / skip phase
  if (plantState == PL_MEASURING) {
    plantState = PL_IDLE; toast(TR("Measuring off", "Medicion off"), 1000); sound(SND_TICK); gDirty = true;
    return;
  }
  setMoodFor(M_HAPPY, MANUAL_MOOD_MS);
  dizzyUntil = 0;
  squash = 1.0f;
  toast(TR("Hi!", "Hola!"), 900);
  sound(SND_HELLO);
  gDirty = true;
}

bool onSensorHold() {
  Serial.println("[PET] sensor hold");
  if (gPage == PAGE_CHAT) { chatTurn(); return false; }  // v3: push-to-talk voice
  if (coach.active) {
    toast(coach.anyDone ? TR("Break done", "Pausa terminada") : TR("Break skipped", "Pausa saltada"), 1000);
    coachClose(); sound(SND_TICK);
    return false;
  }
  if (nudge.active) {
    nudge.active = false; toast(TR("Nudge cleared", "Aviso quitado"), 1200); sound(SND_TICK); gDirty = true;
    return false;
  }
  if (pomoRunning()) { pomoStop(); toast(TR("Pomo ended", "Pomo terminado"), 1200); sound(SND_TICK); return false; }
  if (vitalPhase != VIT_OFF) { vitalTogglePause(); sound(SND_TICK); return false; }
  if (plantState == PL_MEASURING) {
    plantState = PL_IDLE; toast(TR("Measuring off", "Medicion off"), 1000); sound(SND_TICK); gDirty = true;
    return false;
  }
  setMoodFor(M_LOVE, MANUAL_MOOD_MS);
  squash = 1.0f;
  toast(TR("Cuddles!", "Mimos!"), 1200);
  sound(SND_PURR);
  gDirty = true;
  return true;   // keep purring while held
}

// ===========================================================================
//  RENDER, BRIGHTNESS AND LED
// ===========================================================================
void render() {
  spr.fillSprite(PAPER);
  if (coach.active) {
    drawCoach();
  } else {
    switch (gPage) {
      case PAGE_FACE:    drawFacePage();    break;
      case PAGE_EXERCISE: drawFocusPage();  break;
      case PAGE_POMO:    drawPomoPage();    break;
      case PAGE_PLANTS:  drawPlantPage();   break;
      case PAGE_VITAL:   drawVitalPage();   break;
      case PAGE_WEATHER: drawWeatherPage(); break;
      case PAGE_CLOCK:   drawClockDatePage(); break;
      case PAGE_CHAT:    drawChatPage(); break;
      case PAGE_SET_SOUND: drawSetSoundPage(); break;
      case PAGE_SET_SCREEN: drawSetScreenPage(); break;
      case PAGE_SET_TIME: drawSetTimePage(); break;
    }
    if (gPage == PAGE_FACE && !coach.active) drawVitalBanner();   // vital turn: what's next + countdown
    drawPageDots();
  }
  drawToast();
#if TOUCH_DEBUG
  {  // touch diagnostic line (disable with TOUCH_DEBUG 0)
    char d[64];
    snprintf(d, sizeof(d), "T:%s ok%lu err%lu n%d ev%d raw%d,%d %s", touchOk ? "YES" : "NO",
             (unsigned long)tdbg.ok, (unsigned long)tdbg.fail, tdbg.n, tdbg.ev, tdbg.rx, tdbg.ry, ts.down ? "DOWN" : "");
    spr.fillRect(0, 196, SCR_W, 11, PAPER);
    txt(d, 2, 198, 1, TL_DATUM);
  }
  if (ts.down) { spr.drawFastHLine(ts.x - 12, ts.y, 25, INK); spr.drawFastVLine(ts.x, ts.y - 12, 25, INK); }
#endif
  spr.pushSprite(0, 0);
  static int pv = -2;
  int shown = coach.active ? -1 : gPage;
  if (shown != pv) { pv = shown; Serial.printf("[RENDER] showing: %s\n", coach.active ? "ACTIVE BREAK" : PAGE_NAMES[gPage]); }
}

int appliedDuty = -1;
void updateBacklight() {
  int duty = BRIGHT_LEVELS[gBright];
  if (isNight() && millis() - lastInteraction > 60000 && !coach.active && !nudge.active) duty = BRIGHT_LEVELS[0];
  if (duty == appliedDuty) return;
  appliedDuty = duty;
#if BL_PWM && defined(TFT_BL)
  ledcWrite(TFT_BL, duty);
#else
  inkLevel = duty;                 // software brightness: more or less intense palette
  buildPalette();
  gDirty = true;
#endif
}

void updateLed() {
#if LED_ENABLED
  static int last = -1;
  bool blink = (millis() / 500) & 1;
  int st;
  if (coach.active)                 st = 20;                       // solid green
  else if (buildFail())               st = blink ? 2 : 0;            // v3: blinking red = build broken
  else if (buildBusy())               st = blink ? 40 : 41;          // v3: blinking amber = AI working
  else if (vitalAlertActive())      st = blink ? 30 : 31;          // blinking cyan = phase change
  else if (pomoRunning())           st = pomoPaused ? (blink ? 40 : 41) : 40;  // solid amber = pomo, blink = paused
  else if (nudge.active)            st = blink ? 30 : 31;          // blinking cyan
  else if (gNet == NET_CONNECTING)  st = blink ? 10 : 11;          // blinking blue
  else if (gNet == NET_FAIL)        st = 2;                        // red
  else if (vitalPhase == VIT_RELAX && !vitalPaused) st = blink ? 60 : 61;  // orange = relax
  else if (vitalPhase == VIT_SIT && !vitalPaused)   st = 70;       // blue = sitting
  else if (vitalPhase == VIT_STAND && !vitalPaused) st = 71;       // green = standing
  else                              st = 0;                        // off
  if (st == last) return;
  last = st;
  switch (st) {
    case 20: led.setLedColorData(0, 0, 40, 0);  break;
    case 60: led.setLedColorData(0, 30, 12, 0); break;   // relax orange (61 = off -> blinks)
    case 70: led.setLedColorData(0, 0, 0, 40);  break;   // sitting blue
    case 71: led.setLedColorData(0, 0, 35, 0);  break;   // standing green
    case 40: led.setLedColorData(0, 30, 15, 0); break;
    case 30: led.setLedColorData(0, 0, 30, 30); break;
    case 10: led.setLedColorData(0, 0, 0, 40);  break;
    case 2:  led.setLedColorData(0, 40, 0, 0);  break;
    default: led.setLedColorData(0, 0, 0, 0);   break;
  }
  led.show();
#endif
}

// ===========================================================================
//  SETUP / LOOP
// ===========================================================================
void setup() {
  Serial.begin(115200);
  // USB-CDC: wait (max 3 s) for the Serial Monitor to connect so the boot log is not lost
  for (uint32_t t0 = millis(); !Serial && millis() - t0 < 3000; ) delay(10);
  delay(300);
  Serial.println("\n=== KT1 DeskPet v" FW_VERSION " (ESP32-S3) ===");
  randomSeed(esp_random());
  dataMtx = xSemaphoreCreateMutex();

  prefs.begin("deskbuddy", false);   // NVS namespace kept for compatibility with saved settings
  gLang    = constrain((int)prefs.getUChar("lang", DEFAULT_LANG), 0, LANG_COUNT - 1);
  gBright  = constrain((int)prefs.getUChar("bri", 3), 0, BRIGHT_COUNT - 1);
  gInk     = constrain((int)prefs.getUChar("ink", 0), 0, INK_COUNT - 1);
  gSoundOn = prefs.getBool("snd", true);
  gVolume  = constrain((int)prefs.getUChar("vol", SOUND_VOLUME), 0, 100);
  pomoFocusMin = constrain((int)prefs.getUChar("pFoc", POMO_FOCUS_DEFAULT), 5, 90);
  pomoBreakMin = constrain((int)prefs.getUChar("pBrk", POMO_BREAK_DEFAULT), 1, 30);
  pomoLongMin = constrain((int)prefs.getUChar("pLng", POMO_LONG_DEFAULT), 5, 60);
  pomoQuiet = prefs.getBool("pQuiet", false);
  // Per-mode durations (Trabajo 25/5, Escritura 50/10, Ocio 15/15); legacy migrates mode 0
  pomoMode = constrain((int)prefs.getUChar("pMode", POMO_TRABAJO), 0, POMO_MODES - 1);
  {
    char k[8];
    static const int DF[POMO_MODES] = { 25, 50, 15 };
    static const int DB[POMO_MODES] = { 5, 10, 15 };
    for (int i = 0; i < POMO_MODES; i++) {
      snprintf(k, sizeof(k), "pF%d", i);
      int fb = (i == 0) ? pomoFocusMin : DF[i];
      pomoFocusPerMode[i] = constrain((int)prefs.getUChar(k, fb), 5, 90);
      snprintf(k, sizeof(k), "pB%d", i);
      int bb = (i == 0) ? pomoBreakMin : DB[i];
      pomoBreakPerMode[i] = constrain((int)prefs.getUChar(k, bb), 1, 30);
      snprintf(k, sizeof(k), "pT%d", i);
      pomoTodayPerMode[i] = prefs.getUShort(k, 0);
    }
    pomoTodayCount = pomoTodayPerMode[0] + pomoTodayPerMode[1] + pomoTodayPerMode[2];
    pomoSyncModeDur();
  }
  pomoDayKey = prefs.getUInt("pDay", 0);
  sedEveryMin = prefs.getUChar("sedEvery", SED_EVERY_DEFAULT);
  if (sedEveryMin != 0 && sedEveryMin != 15 && sedEveryMin != 30 && sedEveryMin != 45 && sedEveryMin != 60)
    sedEveryMin = SED_EVERY_DEFAULT;
  sedEnabled = prefs.getBool("sedOn", true);
  statsLoad();

  // Screen + canvas
  tft.init();
  tft.setRotation(SCREEN_ROTATION);
  tft.fillScreen(TFT_BLACK);
#ifdef TFT_BL
#if BL_PWM
  if (ledcAttach(TFT_BL, 12000, 8)) ledcWrite(TFT_BL, BRIGHT_LEVELS[gBright]);
  else { Serial.println("[TFT] ledcAttach failed -> fixed backlight"); pinMode(TFT_BL, OUTPUT); digitalWrite(TFT_BL, TFT_BACKLIGHT_ON); }
#else
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);    // same as TaskManager / Ente
#endif
#endif
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("KT1", 10, 10, 4);    // boot screen (audio takes ~2.5 s)
  tft.drawString(TR("Starting...", "Arrancando..."), 10, 44, 2);
  // Canvas: 16 bits in PSRAM. Without PSRAM, 8 bits (RGB332, 75 KB) in internal RAM: a
  // 16-bit one (150 KB) there would starve TLS and weather/GitHub would fail.
  bool psram = psramFound();
  int  depth = 16;
  if (!psram || !spr.createInPsram(SCR_W, SCR_H)) {
    Serial.println(psram ? "[TFT] PSRAM full: 8-bit canvas in internal RAM"
                         : "[TFT] PSRAM NOT detected -> enable Tools > PSRAM > \"OPI PSRAM\". Using 8-bit canvas");
    depth = 8;
    spr.setColorDepth(8);
    if (!spr.createSprite(SCR_W, SCR_H)) Serial.println("[TFT] ERROR creating sprite (out of memory)");
  }
  Serial.printf("[TFT] Canvas %d bits in %s. Free internal RAM %u B, free PSRAM %u B\n",
                depth, depth == 16 ? "PSRAM" : "internal RAM",
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                (unsigned)ESP.getFreePsram());
  buildPalette();

#if LED_ENABLED
  led.begin();
  led.setBrightness(LED_BRIGHT);
#endif

  // Internal bus as in ESP32S3_TaskManager (which works): Wire.begin(16,15) and touch.
  // Then the codec, which shares the same bus.
  i2cBusRecover(TOUCH_SDA, TOUCH_SCL);      // frees the bus if something left it stuck
  Wire.begin(TOUCH_SDA, TOUCH_SCL);
  Wire.setClock(400000);                    // like the Ente (same touch+codec+IMU bus)
  touchInit();
#if SOUND_ENABLED
  soundInit();
  soundSetVolume(gVolume);
#else
  pinMode(AMP_PIN, OUTPUT);
  digitalWrite(AMP_PIN, AMP_ON_LEVEL);
#endif
#if IMU_ENABLED
  imuInit();
  if (imuOk) xTaskCreatePinnedToCore(imuTask, "imu", 4096, nullptr, 2, nullptr, 0);
#endif
#if PET_ENABLED
  petInit();
#endif
#if PLANT_ENABLED
  plantInit();
#endif
  pomoInit();
  vitalInit();
  chatInit();   // v3: voice buffer in PSRAM

  setenv("TZ", TZ_INFO, 1); tzset();
  xTaskCreatePinnedToCore(netTask,  "net",  16384, nullptr, 1, nullptr, 0);
  xTaskCreatePinnedToCore(httpTask, "http", 6144,  nullptr, 1, nullptr, 0);

  lastInteraction = millis();
  sitSince = millis();
  sedLastMoveMs = millis();
  faceInit();
  if (strcmp(WIFI_SSID, "YOUR_WIFI") == 0 || strcmp(WIFI_SSID, "TU_WIFI") == 0) toast(TR("Edit secrets.h (Wi-Fi)", "Edita secrets.h (Wi-Fi)"), 8000);
  else                                   toast(TR("Connecting Wi-Fi...", "Conectando Wi-Fi..."), 2500);
  sound(SND_HELLO);   // audible self-test: if you do not hear it, check [SOUND] in the serial monitor
}

// OTA: asks the bridge for the version; if it differs, downloads and flashes itself.
static void otaCheck() {
  String out;
  if (!bridgeGet("/firmware/version", out)) return;
  JsonDocument d;
  if (deserializeJson(d, out)) return;
  const char* v = d["version"] | "";
  const char* u = d["url"] | "";
  if (!v[0] || !u[0] || strcmp(v, FW_VERSION) == 0) return;
  Serial.printf("[OTA] %s -> %s\n", FW_VERSION, v);
  toast(TR("Updating...", "Actualizando..."), 2000);
  render();
  WiFiClient c;
  c.setTimeout(15000);
  if (httpUpdate.update(c, String(u)) == HTTP_UPDATE_OK) return;  // reboots by itself
  toast(TR("OTA failed", "OTA fallo"), 1500);
}

void loop() {
  static uint32_t lastFrame = 0, lastTick = 0;

  Gesture g = pollTouch();
  if (g != G_NONE) handleGesture(g);
#if PET_ENABLED
  petTick();
#endif

  // NOTE: 'now' is taken AFTER the gesture. handleGesture() sets lastInteraction = millis(),
  // and if 'now' were earlier, "now - lastInteraction" (uint32_t) would wrap to a huge
  // value and auto-return would send the page back to the FACE in the same loop.
  uint32_t now = millis();

  if (now - lastTick >= 250) {          // exercise, pomodoro, plants, vital and stats logic
    lastTick = now;
    cycleTick();
    pomoTick();
    plantTick();
    vitalTick();
    sedTick();
    statsTick();
  }
  coachTick();                          // every loop: rep timing must be precise

  // Daily OTA against the bridge (pc-agent/firmware). Only with Wi-Fi and with
  // nothing active (does not interrupt exercise/pomo/vital/coach).
  static uint32_t nextOta = 30000;
  if (gNet == NET_OK && !coach.active && !pomoRunning() && vitalPhase == VIT_OFF
      && (int32_t)(now - nextOta) >= 0) {
    nextOta = now + 24UL * 3600UL * 1000UL;
    otaCheck();
  }

  // v2: timers (exercise, pomodoro, vital) keep running when you change page.
  // No auto-return during pomodoro or a posture nudge (the vital turn
  // alerts on the FACE with a pill + LED, so you can navigate freely).
  if (!coach.active && !pomoRunning() && !nudge.active && gPage != PAGE_FACE && now - lastInteraction > AUTO_RETURN_MS) {
    gPage = PAGE_FACE; gDirty = true;
  }
  // "Dizzy" NO longer changes page: it only shows spiral eyes when already on the face.

  updateBacklight();
  updateLed();

  bool animated = coach.active || vitalPhase != VIT_OFF || gPage == PAGE_FACE || gPage == PAGE_VITAL || gPage == PAGE_CHAT || ts.down;
  uint32_t frameMs = animated ? 33 : 200;
  if (gDirty || now - lastFrame >= frameMs) {
    lastFrame = now;
    gDirty = false;
    render();
  }
  delay(2);
}
