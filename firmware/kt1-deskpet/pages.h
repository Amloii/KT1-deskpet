// ===========================================================================
//  pages.h — Dashboard pages: CLOCK/DATE, WEATHER, MOON, GITHUB,
//  CHAT and SETTINGS (Sound/Screen/Timers, at the end).
//  (v3.2: no REMOTE page; the PC is voice-controlled via the chatbot tools.)
// ===========================================================================
#pragma once

// ---------------------------------------------------------------------------
//  CLOCK
// ---------------------------------------------------------------------------
void drawClockPage() {
  drawHeader(TR("CLOCK", "RELOJ"));
  struct tm t;
  if (!getLocal(t)) { drawCentered2(TR("Syncing", "Sincronizando"), gNet == NET_OK ? TR("NTP time...", "hora NTP...") : TR("waiting for Wi-Fi...", "esperando Wi-Fi...")); return; }
  char hh[3], mm[3], b[32];
  snprintf(hh, sizeof(hh), "%02d", t.tm_hour);
  snprintf(mm, sizeof(mm), "%02d", t.tm_min);
  txt(hh, SCR_W / 2 - 12, 98, 8, MR_DATUM, P.ink);
  if (millis() % 1000 < 500) txt(":", SCR_W / 2, 94, 8, MC_DATUM, P.accent);
  txt(mm, SCR_W / 2 + 12, 98, 8, ML_DATUM, P.accent);
  spr.fillRoundRect(30, 150, 220, 10, 4, P.card);
  spr.fillRoundRect(32, 152, 216 * min(t.tm_sec, 59) / 59, 6, 3, P.accent);
  snprintf(b, sizeof(b), "%02d", t.tm_sec);
  txt(b, 290, 155, 4, MC_DATUM, P.inkDim);
  snprintf(b, sizeof(b), "%s %d %s", DAYS[t.tm_wday], t.tm_mday, MONTHS_SHORT[t.tm_mon]);
  txt(b, SCR_W / 2, 196, 4, MC_DATUM, P.ink);
}

// ---------------------------------------------------------------------------
//  DATE (with mini calendar)
// ---------------------------------------------------------------------------
void drawDatePage() {
  drawHeader(TR("DATE", "FECHA"));
  struct tm t;
  if (!getLocal(t)) { drawCentered2(TR("Syncing", "Sincronizando"), TR("NTP time...", "hora NTP...")); return; }
  char b[32];
  txt(DAYS[t.tm_wday], 82, 46, 4, MC_DATUM, P.accent);
  snprintf(b, sizeof(b), "%d", t.tm_mday);
  txt(b, 82, 110, 8, MC_DATUM, P.ink);
  txt(MONTHS[t.tm_mon], 82, 164, 4, MC_DATUM, P.ink);
  snprintf(b, sizeof(b), "%d", t.tm_year + 1900);
  txt(b, 82, 188, 2, MC_DATUM, P.inkDim);

  const char* const* hdr = WEEK_INITIALS;
  int x0 = 172, y0 = 38, cw = 20, ch = 18;
  for (int i = 0; i < 7; i++) txt(hdr[i], x0 + i * cw + cw / 2, y0, 1, TC_DATUM, i >= 5 ? P.accent : P.inkDim);
  spr.drawFastHLine(x0, y0 + 10, 7 * cw, P.line);
  int wdayMon = (t.tm_wday + 6) % 7;
  int firstCol = ((wdayMon - (t.tm_mday - 1) % 7) + 7) % 7;
  int dim = daysInMonth(t.tm_year + 1900, t.tm_mon);
  for (int d = 1; d <= dim; d++) {
    int idx = firstCol + d - 1, col = idx % 7, row = idx / 7;
    int cx = x0 + col * cw + cw / 2, cy = y0 + 22 + row * ch;
    snprintf(b, sizeof(b), "%d", d);
    if (d == t.tm_mday) { spr.fillRoundRect(cx - 9, cy - 8, 18, 16, 4, P.accent); txt(b, cx, cy, 1, MC_DATUM, P.bg); }
    else txt(b, cx, cy, 1, MC_DATUM, col >= 5 ? P.inkDim : P.ink);
  }
  snprintf(b, sizeof(b), TR("Day %d  -  Week %d", "Dia %d  -  Semana %d"), t.tm_yday + 1, isoWeek(t));
  txt(b, x0 + 7 * cw / 2, 200, 2, MC_DATUM, P.inkDim);
}

// ---------------------------------------------------------------------------
//  WEATHER (Open-Meteo)
// ---------------------------------------------------------------------------
// WMO code descriptions, one row per language (index from wmoIdx)
#define WMO_N 20
const char* const WMO_TEXT_L[LANG_COUNT][WMO_N] = {
  { "Clear", "Mostly clear", "Partly cloudy", "Cloudy", "Fog", "Drizzle", "Icy drizzle",
    "Light rain", "Rain", "Heavy rain", "Freezing rain", "Light snow", "Snow", "Heavy snow",
    "Snow grains", "Showers", "Snow showers", "Thunderstorm", "Hailstorm", "---" },
  { "Despejado", "Casi despejado", "Algo nublado", "Nublado", "Niebla", "Llovizna", "Llovizna helada",
    "Lluvia debil", "Lluvia", "Lluvia fuerte", "Lluvia helada", "Nieve debil", "Nieve", "Nieve fuerte",
    "Granizo fino", "Chubascos", "Chubascos nieve", "Tormenta", "Tormenta granizo", "---" },
};
int wmoIdx(int c) {
  switch (c) {
    case 0: return 0;                    case 1: return 1;
    case 2: return 2;                    case 3: return 3;
    case 45: case 48: return 4;
    case 51: case 53: case 55: return 5;
    case 56: case 57: return 6;
    case 61: return 7;                   case 63: return 8;
    case 65: return 9;
    case 66: case 67: return 10;
    case 71: return 11;                  case 73: return 12;
    case 75: return 13;                  case 77: return 14;
    case 80: case 81: case 82: return 15;
    case 85: case 86: return 16;
    case 95: return 17;
    case 96: case 99: return 18;
    default: return 19;
  }
}
const char* wmoText(int c) { return WMO_TEXT_L[gLang][wmoIdx(c)]; }
// Weather colors (dimmed by brightness)
#define WC_SUN    tint(rgb(250, 204,  21))
#define WC_MOON   tint(rgb(226, 232, 240))
#define WC_CLOUD  tint(rgb(148, 163, 184))
#define WC_RAIN   tint(rgb( 96, 165, 250))
#define WC_SNOW   tint(rgb(224, 242, 254))
#define WC_BOLT   tint(rgb(250, 204,  21))

void iconSun(int x, int y, int r) {
  uint16_t c = WC_SUN;
  spr.fillCircle(x, y, r * 0.52f, c);
  for (int i = 0; i < 8; i++) {
    float a = i * PI / 4;
    thickLine(x + cosf(a) * r * 0.72f, y + sinf(a) * r * 0.72f, x + cosf(a) * r, y + sinf(a) * r, 2, c);
  }
}
void iconMoon(int x, int y, int r) {
  spr.fillCircle(x, y, r * 0.62f, WC_MOON);
  spr.fillCircle(x + r * 0.32f, y - r * 0.22f, r * 0.55f, PAPER);
}
void cloudShape(int x, int y, float r, uint16_t col) {
  spr.fillCircle(x - r * 0.45f, y + r * 0.10f, r * 0.38f, col);
  spr.fillCircle(x + r * 0.05f, y - r * 0.15f, r * 0.50f, col);
  spr.fillCircle(x + r * 0.50f, y + r * 0.12f, r * 0.36f, col);
  spr.fillRoundRect(x - r * 0.85f, y + r * 0.05f, r * 1.72f, r * 0.45f, r * 0.2f, col);
}
void iconCloud(int x, int y, int r) { cloudShape(x, y, r * 1.12f, tint(rgb(71, 85, 105))); cloudShape(x, y, r, WC_CLOUD); }
void drawWeatherIcon(int code, bool day, int x, int y, int r) {
  if (code == 0) { day ? iconSun(x, y, r) : iconMoon(x, y, r); return; }
  if (code == 1 || code == 2) {
    day ? iconSun(x + r * 0.35f, y - r * 0.35f, r * 0.7f) : iconMoon(x + r * 0.35f, y - r * 0.35f, r * 0.7f);
    iconCloud(x - r * 0.1f, y + r * 0.2f, r * 0.85f);
    return;
  }
  if (code == 45 || code == 48) {
    for (int i = 0; i < 4; i++) spr.fillRoundRect(x - r + (i % 2) * 8, y - r * 0.5f + i * r * 0.35f, 2 * r - 8, 5, 2, WC_CLOUD);
    return;
  }
  iconCloud(x, y - r * 0.25f, r);
  int by = y + r * 0.45f;
  if (code >= 95) {
    spr.fillTriangle(x + 4, by - 4, x - 10, by + 14, x + 1, by + 12, WC_BOLT);
    spr.fillTriangle(x - 2, by + 10, x + 10, by + 10, x - 6, by + 30, WC_BOLT);
  } else if ((code >= 71 && code <= 77) || code == 85 || code == 86) {
    for (int i = -1; i <= 1; i++) { spr.fillCircle(x + i * r * 0.5f, by + 8 + (i & 1) * 8, 3, WC_SNOW); spr.fillCircle(x + i * r * 0.5f + 8, by + 22, 3, WC_SNOW); }
  } else if (code != 3) {
    int n = (code >= 51 && code <= 57) ? 2 : 3;
    for (int i = 0; i < n; i++) { float px = x - r * 0.45f + i * r * 0.45f; thickLine(px + 4, by + 4, px - 4, by + 22, 1, WC_RAIN); }
  }
}

void drawWeatherPage() {
  WeatherData w;
  xSemaphoreTake(dataMtx, portMAX_DELAY); w = gWeather; xSemaphoreGive(dataMtx);
  // "Sky" panel on the left (stays above the navigation bar)
  uint16_t skyTop = w.isDay ? tint(rgb(56, 189, 248)) : tint(rgb(30, 41, 59));
  uint16_t skyBot = w.isDay ? tint(rgb(14, 116, 176)) : tint(rgb(15, 23, 42));
  spr.fillRoundRect(6, 28, 118, 176, 12, skyBot);
  fillVGradient(8, 30, 114, 172, skyTop, skyBot);
  drawHeader(TR("WEATHER - " CITY_NAME, "TIEMPO - " CITY_NAME));
  if (!w.ok) { drawNoData(w.err); return; }
  char b[40];
  drawWeatherIcon(w.code, w.isDay, 65, 80, 34);
  txt(wmoText(w.code), 65, 150, 2, MC_DATUM, P.ink);
  fmtHM(w.updated, b, sizeof(b));
  char u[48]; snprintf(u, sizeof(u), TR("upd. %s", "act. %s"), b);
  txt(u, 65, 188, 1, MC_DATUM, w.isDay ? tint(rgb(220, 240, 255)) : P.inkDim);

  snprintf(b, sizeof(b), "%.0f", w.temp);
  txt(b, 134, 32, 6, TL_DATUM, P.ink);
  int tw = spr.textWidth(b, 6);
  drawDegree(134 + tw + 8, 40, 5, P.accent);
  txt("C", 134 + tw + 16, 34, 4, TL_DATUM, P.inkDim);

  int x = 134, y = 96, dy = 18;
  snprintf(b, sizeof(b), TR("Feels %.0f C", "Sens. %.0f C"), w.feels);        txt(b, x, y, 2, TL_DATUM, P.inkDim); y += dy;
  snprintf(b, sizeof(b), TR("Humidity %d%%", "Humedad %d%%"), w.hum);         txt(b, x, y, 2, TL_DATUM, P.inkDim); y += dy;
  snprintf(b, sizeof(b), TR("Wind %.0f km/h", "Viento %.0f km/h"), w.wind);   txt(b, x, y, 2, TL_DATUM, P.inkDim); y += dy;
  snprintf(b, sizeof(b), "Max %.0f  Min %.0f C", w.tmax, w.tmin);     txt(b, x, y, 2, TL_DATUM, P.ink); y += dy;
  if (w.pop >= 0) { snprintf(b, sizeof(b), TR("Rain %d%%", "Lluvia %d%%"), w.pop); txt(b, x, y, 2, TL_DATUM, P.info); y += dy; }
  snprintf(b, sizeof(b), TR("Sun %s-%s", "Sol %s-%s"), w.sunrise, w.sunset);  txt(b, x, y, 2, TL_DATUM, P.warn);
}

// ---------------------------------------------------------------------------
//  MOON (computed locally)
// ---------------------------------------------------------------------------
const double SYNODIC = 29.530588853;
double moonAgeDays(time_t utc) {
  const double NEW_MOON_REF = 947182440.0;          // 2000-01-06 18:14 UTC
  double days = (utc - NEW_MOON_REF) / 86400.0;
  double age = fmod(days, SYNODIC);
  if (age < 0) age += SYNODIC;
  return age;
}
void drawMoonDisc(int cx, int cy, int r, double age) {
  uint16_t lit = tint(rgb(248, 245, 214));      // lit surface (warm white)
  uint16_t crater = tint(rgb(198, 196, 170));
  double th = 2.0 * PI * age / SYNODIC;
  bool waxing = age < SYNODIC / 2;
  double c = cos(th);
  spr.fillCircle(cx, cy, r, tint(rgb(40, 44, 58)));   // faint dark side (not fully black)
  for (int dy = -r; dy <= r; dy++) {
    double hw = sqrt((double)r * r - (double)dy * dy);
    int xa, xb;
    if (waxing) { xa = cx + (int)lround(hw * c);  xb = cx + (int)lround(hw); }
    else        { xa = cx - (int)lround(hw);      xb = cx - (int)lround(hw * c); }
    if (xb > xa) spr.drawFastHLine(xa, cy + dy, xb - xa + 1, lit);
  }
  spr.drawCircle(cx, cy, r, tint(rgb(120, 125, 140)));
  spr.drawCircle(cx - r * 0.35f, cy - r * 0.30f, r * 0.16f, crater);
  spr.drawCircle(cx + r * 0.30f, cy + r * 0.25f, r * 0.22f, crater);
  spr.drawCircle(cx + r * 0.10f, cy - r * 0.55f, r * 0.10f, crater);
  spr.drawCircle(cx - r * 0.20f, cy + r * 0.50f, r * 0.12f, crater);
}
void drawMoonPage() {
  // Night sky with stars (deterministic so they don't flicker)
  fillVGradient(0, 0, SCR_W, SCR_H, tint(rgb(13, 18, 43)), tint(rgb(6, 8, 20)));
  for (int i = 0; i < 46; i++) {
    int sx = (i * 71 + 13) % SCR_W, sy = (i * 39 + 7) % (SCR_H - 30);
    uint16_t s = (i % 4 == 0) ? tint(rgb(200, 210, 235)) : tint(rgb(120, 130, 160));
    spr.drawPixel(sx, sy, s);
    if (i % 7 == 0) spr.drawPixel(sx + 1, sy, s);
  }
  drawHeader(TR("MOON", "LUNA"));
  time_t now = time(nullptr);
  if (now < 1700000000) { drawCentered2(TR("Syncing", "Sincronizando"), TR("NTP time...", "hora NTP...")); return; }
  double age = moonAgeDays(now);
  double illum = (1.0 - cos(2.0 * PI * age / SYNODIC)) / 2.0;
  // Phase name in 2 lines, one row per language
  static const char* const N1_L[LANG_COUNT][8] = {
    { "New", "Waxing", "First", "Waxing", "Full", "Waning", "Last", "Waning" },
    { "Luna", "Luna", "Cuarto", "Gibosa", "Luna", "Gibosa", "Cuarto", "Luna" },
  };
  static const char* const N2_L[LANG_COUNT][8] = {
    { "moon", "crescent", "quarter", "gibbous", "moon", "gibbous", "quarter", "crescent" },
    { "nueva", "creciente", "creciente", "creciente", "llena", "menguante", "menguante", "menguante" },
  };
  int idx = ((int)floor(age / SYNODIC * 8.0 + 0.5)) % 8;
  drawMoonDisc(88, 122, 72, age);
  txt(N1_L[gLang][idx], 178, 46, 4, TL_DATUM, P.accent);
  txt(N2_L[gLang][idx], 178, 72, 4, TL_DATUM, P.accent);
  char b[32];
  double toFull = fmod(SYNODIC / 2 - age + SYNODIC, SYNODIC);
  double toNew  = SYNODIC - age;
  snprintf(b, sizeof(b), TR("Illum. %d %%", "Iluminada %d %%"), (int)lround(illum * 100)); txt(b, 178, 116, 2, TL_DATUM, P.ink);
  snprintf(b, sizeof(b), TR("Age %.1f days", "Edad %.1f dias"), age);                     txt(b, 178, 136, 2, TL_DATUM, P.inkDim);
  snprintf(b, sizeof(b), TR("Full in %.1f d", "Llena en %.1f d"), toFull);                txt(b, 178, 156, 2, TL_DATUM, P.inkDim);
  snprintf(b, sizeof(b), TR("New in %.1f d", "Nueva en %.1f d"), toNew);                  txt(b, 178, 176, 2, TL_DATUM, P.inkDim);
  txt(TR("(N. hemisphere)", "(hemisferio norte)"), 178, 200, 1, TL_DATUM, P.inkDim);
}

// ---------------------------------------------------------------------------
//  GITHUB
// ---------------------------------------------------------------------------
void statBox(int x, int y, int w, int h, const char* label, int value, uint16_t accent) {
  spr.fillRoundRect(x, y, w, h, 8, P.card);
  spr.drawRoundRect(x, y, w, h, 8, P.line);
  spr.fillRoundRect(x, y, 4, h, 2, accent);            // color stripe on the left
  txt(label, x + w / 2, y + 12, 2, MC_DATUM, P.inkDim);
  char b[16];
  if (value < 0) strlcpy(b, "-", sizeof(b)); else snprintf(b, sizeof(b), "%d", value);
  txt(b, x + w / 2, y + h / 2 + 10, 4, MC_DATUM, accent);
}
void drawGithubPage() {
  GithubData g;
  xSemaphoreTake(dataMtx, portMAX_DELAY); g = gGithub; xSemaphoreGive(dataMtx);
  drawHeader("GITHUB");
  if (!g.ok) { drawNoData(g.err); return; }
  char b[48];
  uint16_t gh = tint(rgb(163, 113, 247));              // GitHub purple
  snprintf(b, sizeof(b), "@%s", g.login);
  txt(b, SCR_W / 2, 40, 4, MC_DATUM, gh);
  if (g.name[0]) txt(g.name, SCR_W / 2, 62, 2, MC_DATUM, P.inkDim);
  statBox(14, 76, 140, 56, "Repos", g.repos, P.info);
  statBox(166, 76, 140, 56, TR("Stars", "Estrellas"), g.stars, P.warn);
  statBox(14, 140, 140, 56, TR("Followers", "Seguidores"), g.followers, gh);
  statBox(166, 140, 140, 56, TR("Following", "Siguiendo"), g.following, P.ok);
  char tu[8]; fmtHM(g.updated, tu, sizeof(tu));
  snprintf(b, sizeof(b), TR("upd. %s", "act. %s"), tu);
  txt(b, SCR_W / 2, 206, 1, MC_DATUM, P.inkDim);
}


// ---------------------------------------------------------------------------
//  SETTINGS in 3 pages (at the end): SOUND · SCREEN · TIMERS.
//  Each page has 2-3 wide rows: everything ends at y<=192, above the
//  navigation touch zone (196).
// ---------------------------------------------------------------------------
#define SET_X0 12
#define SET_W  (SCR_W - 24)
#define SET_BOX_H 26
#define SET_LANG_Y 152

// Touchable segment bar (brightness, volume). Returns the segment width.
static int setSegBar(int y, int n, int sel) {
  const int X0 = SET_X0, W = SET_W;
  spr.fillRoundRect(X0, y, W, SET_BOX_H, 8, P.card);
  spr.drawRoundRect(X0, y, W, SET_BOX_H, 8, P.line);
  int sw = (W - 8) / n;
  for (int i = 0; i < n; i++) {
    int sx = X0 + 4 + i * sw;
    if (i <= sel) spr.fillRoundRect(sx, y + 3, sw - 3, SET_BOX_H - 6, 4, P.accent);
    else          spr.fillRoundRect(sx, y + 3, sw - 3, SET_BOX_H - 6, 4, P.line);
  }
  return sw;
}

// --- SOUND: ON/OFF + segmented volume (plays a test on tap) ---
// Scale 50..100 (below 50 the speaker is inaudible on this board).
static const int VOL_VS[6] = { 50, 60, 70, 80, 90, 100 };
static int volSeg() {
  int best = 0;
  for (int i = 0; i < 6; i++) if (abs(gVolume - VOL_VS[i]) < abs(gVolume - VOL_VS[best])) best = i;
  return best;
}

void drawSetSoundPage() {
  drawHeader(TR("SOUND", "SONIDO"));
  const int X0 = SET_X0, W = SET_W;
  txt(TR("Sound", "Sonido"), X0, 26, 2, TL_DATUM, P.inkDim);
  drawButton(X0, 44, W, SET_BOX_H, gSoundOn ? "ON" : "OFF", gSoundOn, gSoundOn ? P.ok : 0);
  char vb[24]; snprintf(vb, sizeof(vb), TR("Volume %d", "Volumen %d"), gVolume);
  txt(vb, X0, 80, 2, TL_DATUM, P.inkDim);
  setSegBar(98, 6, volSeg());
  txt("50", X0, 128, 1, TL_DATUM, P.inkDim);
  txt("100", X0 + W, 128, 1, TR_DATUM, P.inkDim);
  txt(TR("Tap a segment to test", "Toca un segmento para probar"), SCR_W / 2, 142, 1, MC_DATUM, P.inkDim);
}

void setSoundTap(int x, int y) {
  const int X0 = SET_X0, W = SET_W;
  if (y >= 44 && y < 44 + SET_BOX_H && x >= X0 && x <= X0 + W) { toggleSound(); return; }
  if (y >= 98 && y < 98 + SET_BOX_H && x >= X0 && x <= X0 + W) {
    int s = constrain((x - X0) * 6 / W, 0, 5);
    gVolume = VOL_VS[s];
    prefs.putUChar("vol", gVolume);
    soundSetVolume(gVolume);
    if (!gSoundOn) { gSoundOn = true; prefs.putBool("snd", true); }
    char b[24]; snprintf(b, sizeof(b), TR("Volume %d", "Volumen %d"), gVolume);
    toast(b, 900);
    sound(SND_TICK);
  }
}

// --- SCREEN: brightness + accent color + language ---
void drawSetScreenPage() {
  drawHeader(TR("SCREEN", "PANTALLA"));
  const int X0 = SET_X0, W = SET_W;
  char b[24];
  snprintf(b, sizeof(b), TR("Brightness %d/%d", "Brillo %d/%d"), gBright + 1, BRIGHT_COUNT);
  txt(b, X0, 26, 2, TL_DATUM, P.inkDim);
  setSegBar(44, BRIGHT_COUNT, gBright);
  txt(TR("Accent color", "Color de acento"), X0, 80, 2, TL_DATUM, P.inkDim);
  int cw = (W - (INK_COUNT - 1) * 6) / INK_COUNT;
  for (int i = 0; i < INK_COUNT; i++) {
    int cx = X0 + i * (cw + 6);
    uint16_t col = dim565(ACCENTS[i], inkLevel);
    if (i == gInk) {
      spr.fillRoundRect(cx, 98, cw, SET_BOX_H, 8, col);
      txt(INK_NAMES[i], cx + cw / 2, 98 + SET_BOX_H / 2, 2, MC_DATUM, P.bg);
    } else {
      spr.fillRoundRect(cx, 98, cw, SET_BOX_H, 8, P.card);
      spr.drawRoundRect(cx, 98, cw, SET_BOX_H, 8, col);
      txt(INK_NAMES[i], cx + cw / 2, 98 + SET_BOX_H / 2, 2, MC_DATUM, col);
    }
  }
  txt(TR("Language", "Idioma"), X0, 134, 2, TL_DATUM, P.inkDim);
  int lw = (W - (LANG_COUNT - 1) * 6) / LANG_COUNT;
  for (int i = 0; i < LANG_COUNT; i++)
    drawButton(X0 + i * (lw + 6), SET_LANG_Y, lw, SET_BOX_H, LANG_NAMES[i], i == gLang, i == gLang ? P.accent : 0);
}

// Switch UI language and save it in NVS
void setLanguage(int lang) {
  lang = constrain(lang, 0, LANG_COUNT - 1);
  if (lang == gLang) return;
  gLang = lang;
  prefs.putUChar("lang", gLang);
  gDirty = true;
  toast(LANG_NAMES[gLang], 900);
}

void setScreenTap(int x, int y) {
  const int X0 = SET_X0, W = SET_W;
  if (y >= 44 && y < 44 + SET_BOX_H && x >= X0 && x <= X0 + W) {
    int seg = constrain((x - X0) * BRIGHT_COUNT / W, 0, BRIGHT_COUNT - 1);
    if (seg != gBright) {
      gBright = seg;
      prefs.putUChar("bri", gBright);
      char b[24]; snprintf(b, sizeof(b), TR("Brightness %d/%d", "Brillo %d/%d"), gBright + 1, BRIGHT_COUNT);
      toast(b, 900);
    }
    return;
  }
  if (y >= 98 && y < 98 + SET_BOX_H && x >= X0 && x <= X0 + W) {
    int cw = (W - (INK_COUNT - 1) * 6) / INK_COUNT;
    int i = constrain((x - X0) / (cw + 6), 0, INK_COUNT - 1);
    if (i != gInk) {
      gInk = i;
      buildPalette();
      prefs.putUChar("ink", gInk);
      char b[24]; snprintf(b, sizeof(b), TR("Theme: %s", "Tema: %s"), INK_NAMES[gInk]);
      toast(b, 1000);
    }
    return;
  }
  if (y >= SET_LANG_Y && y < SET_LANG_Y + SET_BOX_H && x >= X0 && x <= X0 + W) {
    int lw = (W - (LANG_COUNT - 1) * 6) / LANG_COUNT;
    setLanguage((x - X0) / (lw + 6));
  }
}

// --- TIMERS: pomodoro focus/short/long + anti-sedentary (4 big rows, h=30) ---
#define SET_TB_H 30
void drawSetTimePage() {
  drawHeader(TR("TIMERS", "TIEMPOS"));
  const int X0 = SET_X0, W = SET_W;
  char b[40];
  snprintf(b, sizeof(b), TR("Focus %d min %s", "Foco %d min %s"), pomoFocusMin, POMO_MODE_NAMES[pomoMode]);
  drawButton(X0, 32, W, SET_TB_H, b, true, P.accent);
  snprintf(b, sizeof(b), TR("Break %d min %s", "Descanso %d min %s"), pomoBreakMin, POMO_MODE_NAMES[pomoMode]);
  drawButton(X0, 70, W, SET_TB_H, b, true, P.ok);
  snprintf(b, sizeof(b), TR("Long %d' auto x4", "Largo %d' auto x4"), pomoLongMin);
  drawButton(X0, 108, W, SET_TB_H, b, true, P.warn);
  if (sedEveryMin == 0) drawButton(X0, 146, W, SET_TB_H, TR("Sedentary OFF", "Sedentario OFF"), false, 0);
  else { snprintf(b, sizeof(b), TR("Sedentary %d min", "Sedentario %d min"), sedEveryMin); drawButton(X0, 146, W, SET_TB_H, b, true, P.info); }
}

void setTimeTap(int x, int y) {
  const int X0 = SET_X0, W = SET_W;
  char b[24];
  if (x < X0 || x > X0 + W) return;
  if (y >= 32 && y < 32 + SET_TB_H) {
    static const int FS[4] = { 15, 25, 35, 45 };
    int i = 0; while (i < 3 && FS[i] != pomoFocusMin) i++;
    pomoFocusMin = FS[(i + 1) % 4];
    pomoFocusPerMode[pomoMode] = pomoFocusMin;
    pomoWriteSlot();
    snprintf(b, sizeof(b), TR("Focus %d min", "Foco %d min"), pomoFocusMin); toast(b, 900);
  } else if (y >= 70 && y < 70 + SET_TB_H) {
    static const int BS[3] = { 5, 10, 15 };
    int i = 0; while (i < 2 && BS[i] != pomoBreakMin) i++;
    pomoBreakMin = BS[(i + 1) % 3];
    pomoBreakPerMode[pomoMode] = pomoBreakMin;
    pomoWriteSlot();
    snprintf(b, sizeof(b), TR("Break %d min", "Descanso %d min"), pomoBreakMin); toast(b, 900);
  } else if (y >= 108 && y < 108 + SET_TB_H) {
    static const int LS[3] = { 15, 20, 30 };
    int i = 0; while (i < 2 && LS[i] != pomoLongMin) i++;
    pomoLongMin = LS[(i + 1) % 3];
    prefs.putUChar("pLng", pomoLongMin);
    snprintf(b, sizeof(b), TR("Long %d min", "Largo %d min"), pomoLongMin); toast(b, 900);
  } else if (y >= 146 && y < 146 + SET_TB_H) {
    static const int SS[5] = { 0, 15, 30, 45, 60 };
    int i = 0; while (i < 4 && SS[i] != sedEveryMin) i++;
    sedEveryMin = SS[(i + 1) % 5];
    sedEnabled = (sedEveryMin != 0);
    prefs.putUChar("sedEvery", sedEveryMin);
    prefs.putBool("sedOn", sedEnabled);
    sedLastMoveMs = millis();
    if (sedEveryMin == 0) { snprintf(b, sizeof(b), "%s", TR("Sedentary OFF", "Sedentario OFF")); }
    else snprintf(b, sizeof(b), TR("Sedentary %d min", "Sedentario %d min"), sedEveryMin);
    toast(b, 900);
  }
}

// ---------------------------------------------------------------------------
//  CLOCK + DATE together (v2): big HH:MM, date and compact mini calendar.
//  Rounded creature frame.
// ---------------------------------------------------------------------------
void drawClockDatePage() {
  // Full-width layout (like the v1 clock, which already fit):
  // big time on top, date in the middle, week strip below. Nothing goes past y=192.
  drawHeader(TR("CLOCK", "RELOJ"));
  struct tm t;
  if (!getLocal(t)) { drawCentered2(TR("Syncing", "Sincronizando"), TR("NTP time...", "hora NTP...")); return; }
  char b[48];
  char hh[3], mm[3];
  snprintf(hh, sizeof(hh), "%02d", t.tm_hour);
  snprintf(mm, sizeof(mm), "%02d", t.tm_min);
  txt(hh, SCR_W / 2 - 12, 84, 8, MR_DATUM, P.ink);
  if (millis() % 1000 < 500) txt(":", SCR_W / 2, 80, 8, MC_DATUM, P.accent);
  txt(mm, SCR_W / 2 + 12, 84, 8, ML_DATUM, P.accent);
  snprintf(b, sizeof(b), "%s %d %s %d", DAYS[t.tm_wday], t.tm_mday,
           MONTHS_SHORT[t.tm_mon], t.tm_year + 1900);
  txtFit(b, SCR_W / 2, 140, SCR_W - 20, 4, MC_DATUM, P.ink);
  // Mon-Sun strip with today highlighted
  const char* const* hdr = WEEK_INITIALS;
  int today = (t.tm_wday + 6) % 7;
  int cw = 37, gap = 6, x0 = (SCR_W - (7 * cw + 6 * gap)) / 2, y = 158;
  for (int i = 0; i < 7; i++) {
    int cx = x0 + i * (cw + gap);
    if (i == today) { spr.fillRoundRect(cx, y, cw, 20, 6, P.accent); txt(hdr[i], cx + cw / 2, y + 10, 2, MC_DATUM, P.bg); }
    else { spr.fillRoundRect(cx, y, cw, 20, 6, P.card); txt(hdr[i], cx + cw / 2, y + 10, 2, MC_DATUM, P.inkDim); }
  }
  snprintf(b, sizeof(b), TR("Day %d - Week %d", "Dia %d - Semana %d"), t.tm_yday + 1, isoWeek(t));
  txt(b, SCR_W / 2, 188, 1, MC_DATUM, P.inkDim);
}

// ---------------------------------------------------------------------------
//  VOICE CHAT (v3): eyes looking up + status pill + level meter with
//  progress (recording) or big Q+A with subtitle (answering).
//  Tap = talk 4 s; hold = listen again. No double voice: the summary is
//  spoken once, and text with no answer or immediate echoes is not replayed.
// ---------------------------------------------------------------------------
//  VOICE CHAT (v3): the screen depends on the moment (eyes = state, no LED).
//   INITIAL (nothing said yet): big eyes + hint (no answer exists yet).
//   IDLE with answer: compact eyes + "You:" + auto-scrolling answer
//     (readable font 2, rotating page 1/3; it used to fall back to tiny font 1).
//   REC: surprised eyes + red pill + level meter + 4 s progress.
//   THINK: amber eyes + "?" thought bubble + amber pill + previous text dimmed.
//   TALK: happy eyes + green pill + big advancing subtitle.
//  TALK button spans the whole row: tap = talk, hold = listen again.
//  All text goes through chatToAscii (no accents or markdown): the TFT lacks
//  those glyphs and showed "Ã¡"/"?" while thinking/talking.
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
//  VOICE CHAT v4 WOW (cinema): compact eyes, pulsing pill, level/waveform,
//  typewriter THINK, paginated text (txtWrapScroll 1/3), confetti on
//  TODO/REMIND/MEMORY/FOCUS, mic icon on TALK.
//  Tap = talk 4 s; hold = replay. Cinema hides page dots (see render()).
// ---------------------------------------------------------------------------
static void chatConfettiDraw() {
  if ((int32_t)(chatConfettiUntil - millis()) <= 0) return;
  uint32_t dt = chatConfettiUntil - millis();  // counts down
  float p = 1.0f - dt / 1600.0f;               // 0 -> 1
  uint16_t cols[4] = { P.ok, P.warn, P.accent, P.danger };
  for (int i = 0; i < 24; i++) {
    int sx = (i * 53 + 17) % SCR_W;
    int x = sx + (int)(sinf(i * 1.7f + p * 6.0f) * 12);
    int y = 30 + (int)(p * (150 + (i * 37) % 60)) % 170;
    if (y > 190) continue;
    spr.fillCircle(x, y, 2 + (i % 2), cols[i & 3]);
  }
}
void drawChatPage() {
  drawHeader(TR("VOICE CHAT", "CHAT VOZ"));
  if (chatState == CHAT_IDLE && !chatHasAnswer) {
    drawEyesAt(SCR_W / 2, 92, 0.80f);
    txt(TR("Tap TALK and speak", "Toca HABLAR y habla"), SCR_W / 2, 142, 2, MC_DATUM, P.ink);
  } else {
    // Compact eyes leave room for 3-line paginated text (no aura frame).
    float es = (chatState == CHAT_IDLE) ? 0.42f : 0.38f;
    int ey = (chatState == CHAT_IDLE) ? 50 : 46;
    drawEyesAt(SCR_W / 2, ey, es);
    if (chatState == CHAT_THINK) {
      spr.fillCircle(218, 52, 3, P.ink);
      spr.fillCircle(232, 44, 4, P.ink);
      spr.fillCircle(250, 34, 9, P.ink);
      txt("?", 250, 34, 1, MC_DATUM, P.bg);
    }
    if (chatState != CHAT_IDLE) {
      const char* st; uint16_t bg;
      if (chatState == CHAT_REC) { st = TR("LISTENING", "ESCUCHANDO"); bg = P.danger; }
      else if (chatState == CHAT_THINK) { st = TR("THINKING", "PENSANDO"); bg = P.warn; }
      else { st = gSoundOn ? TR("SPEAKING", "HABLANDO") : TR("TEXT", "TEXTO"); bg = P.ok; }
      // Typewriter dots while thinking.
      char pill[24];
      if (chatState == CHAT_THINK) {
        int dots = 1 + ((millis() - chatThinkT0) / 400) % 3;
        snprintf(pill, sizeof(pill), "%s", st);
        for (int i = 0; i < dots && strlen(pill) < sizeof(pill) - 2; i++) strlcat(pill, ".", sizeof(pill));
        st = pill;
      }
      int pw = spr.textWidth(st, 2) + 30;
      // Pulse: outer halo breathing.
      uint8_t pl = (uint8_t)(170 + 60 * sinf(millis() / 240.0f));
      spr.drawRoundRect(SCR_W / 2 - pw / 2 - 2, 76, pw + 4, 20, 10, dim565(bg, pl));
      spr.fillRoundRect(SCR_W / 2 - pw / 2, 78, pw, 16, 8, bg);
      txt(st, SCR_W / 2, 86, 2, MC_DATUM, P.bg);
    }
    if (chatState == CHAT_REC) {
      int nb = 7, bw = 16, gap = 8;
      int x0 = SCR_W / 2 - (nb * bw + (nb - 1) * gap) / 2, yb = 138;
      for (int i = 0; i < nb; i++) {
        float wob = 0.55f + 0.45f * sinf(millis() / 180.0f + i * 1.1f);
        int h = constrain((int)(chatLevel * wob * 42 / 100), 3, 42);
        spr.fillRoundRect(x0 + i * (bw + gap), yb - h, bw, h, 3,
                          chatLevel > 4 ? P.danger : P.line);
      }
      uint32_t el = millis() - chatRecT0;
      float f = constrain((float)el / (chatRecSpan ? chatRecSpan : 4000), 0, 1);
      spr.fillRoundRect(60, 144, 200, 6, 3, P.card);
      spr.fillRoundRect(62, 145, (int)(196 * f), 4, 2, P.danger);
    } else if (chatState == CHAT_IDLE) {
      char u[80];
      snprintf(u, sizeof(u), TR("You: %.60s", "Tu: %.60s"), chatUser[0] ? chatUser : "...");
      txt(u, SCR_W / 2, 98, 1, MC_DATUM, P.inkDim);
      // Paginated (was txtWrap: truncated to 2 lines). Box 106..150 = 44px -> 2 lines
      // per page rotating 1/3 automatically.
      txtWrapScroll(chatBot, SCR_W / 2, 106, SCR_W - 24, 44, 2, P.ink);
    } else if (chatState == CHAT_THINK) {
      // Paginated dimmed previous answer (was txtWrap: cut off).
      txtWrapScroll(chatBot, SCR_W / 2, 100, SCR_W - 24, 50, 2, P.inkDim);
    } else {  // TALK: live waveform behind karaoke subtitle, paginated
      int nb = 9, bw = 12, gap = 6;
      int x0 = SCR_W / 2 - (nb * bw + (nb - 1) * gap) / 2, yb = 108;
      for (int i = 0; i < nb; i++) {
        float wob = 0.5f + 0.5f * sinf(millis() / 160.0f + i * 0.9f);
        int h = 4 + (int)(wob * 10);
        spr.fillRoundRect(x0 + i * (bw + gap), yb - h / 2, bw, h, 3, P.ok);
      }
      bool sub = (chatSub[0] != 0);
      txtWrapScroll(sub ? chatSub : chatBot, SCR_W / 2, 114, SCR_W - 24, 36, 2, P.ink);
    }
  }
  bool busy = (chatState != CHAT_IDLE);
  drawButton(CHAT_BTN_X, CHAT_BTN_Y, CHAT_BTN_W, CHAT_BTN_H,
             busy ? "..." : TR("TALK", "HABLAR"), !busy, P.accent);
  if (!busy) {
    // Mic icon on the button (left side).
    int mx = CHAT_BTN_X + 26, my = CHAT_BTN_Y + CHAT_BTN_H / 2;
    spr.fillRoundRect(mx - 5, my - 9, 10, 14, 5, P.bg);
    spr.drawRoundRect(mx - 8, my - 4, 16, 10, 5, P.bg);
    spr.fillRect(mx - 1, my + 8, 2, 5, P.bg);
  }
  chatConfettiDraw();
}

void chatTap(int x, int y) {
  (void)x; (void)y;
  if (chatState != CHAT_IDLE) return;   // thinking/talking: no new turn accepted
  chatTurn(CHAT_FIX_MS);   // tapping the page = talk
}

// ---------------------------------------------------------------------------
//  REMOTE removed (v3.2): PC control is done via voice chatbot tools
//  (chat.h CMD: MUTE/PLAY/LOCK/SLEEP/SCREEN/PLAY:/TODO:/PROMPT:/RESUME
//  against bridge.py). The build mirror (gBuild/gBuildSummary) still
//  arrives via POST /pet and can be shown on the face.
// ---------------------------------------------------------------------------
