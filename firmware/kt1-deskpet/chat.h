// ===========================================================================
//  chat.h — V3: voice chatbot (Gemini STT+LLM, Google TTS). Adapted from
//  ESP32S3_Chatbot.ino (push-to-talk, streamed base64 WAV, parses
//  USER:/BOT:/EMO:). V3 differences:
//   - Trigger = TALK button on the CHAT page (records 4 s) or TTP223 held
//     on that page (until release/5 s). Tap / Listen button = replay.
//   - The system prompt also asks for PAGE: (switch screen) and CMD with the
//     PC tools: MUTE, PLAY, VOL:<n>, OPEN:<app>, LOCK/SLEEP (with CONFIRM),
//     SCREEN (+automatic photo for vision), PLAY:<video>, TODO:<t>,
//     FOCUS:<mode>:<task> (bind a pomodoro task, switch mode, start focus),
//     REMIND:<sec>:<t>, REMINDLIST, DND, PAIR, PROMPT:<t>, RESUME.
//     -> remote.h/bridge.py. These are the voice chatbot tools (no Remote page).
//   - soundTask is suspended before recording/speaking (soundAudioSuspend) and
//     restored afterwards (soundAudioResume). Requires the ESP32-audioI2S lib.
// ===========================================================================
#pragma once

#include "Audio.h"
#include "mbedtls/base64.h"
#include "remote.h"   // MUTE/PLAY/LOCK/TODO/PROMPT/RESUME (the .ino includes it earlier; this is a safeguard)

#define CHAT_REC_HZ      16000
#define CHAT_REC_MAX_S   5
#define CHAT_REC_BYTES   (CHAT_REC_HZ * 2 * CHAT_REC_MAX_S)
#define CHAT_PLAY_HZ     44100
#define CHAT_MIN_MS      600
#define CHAT_TTS_CHARS   190
#define CHAT_FIX_MS      4000   // recording with the TALK button (no sensor)

// TALK button geometry (full row, thick). Used by pages.h drawing and chatTap.
#define CHAT_BTN_X   8
#define CHAT_BTN_Y   154
#define CHAT_BTN_W   (SCR_W - 16)
#define CHAT_BTN_H   32

// System prompt, one per UI language. Protocol tokens (USER/BOT/EMO/PAGE/CMD and the
// command names) are the same in both: the parser below does not depend on the language.
static const char* CHAT_SYS_EN =
  "You are KT1, a friendly and direct desk companion voice assistant. ALWAYS answer in "
  "English, natural and SHORT (max 2 sentences, under 170 characters, no lists "
  "or markdown; it will be read aloud). Pick an EMOTION among happy, neutral, angry, bored. "
  "Each message comes with a 320x240 PHOTO of the PC screen (it may be missing): use it "
  "to answer what is on screen, without saying it is a photo. "
  "You may request a SCREEN CHANGE on the device among: Face, Exercise, Pomodoro, "
  "Plants, Vital, Weather, Clock, Settings, Chat (or - if no change). "
  "You may request ONE PC ACTION among: - (none), MUTE (mute), PLAY (play/pause), "
  "VOL:<0-100> (volume), OPEN:<app: notepad, calculator, terminal, code, chrome, spotify> (open), "
  "LOCK (lock), SLEEP (suspend), SCREEN (say what is on screen), "
  "PLAY:<text or URL of a video to play on the PC>, "
  "TODO:<text to note down>, REMIND:<seconds>:<text to remind> (minimum 60 s), "
  "FOCUS:<work|writing|leisure>:<short task, max 40 chars> (the user starts working on "
  "something: bind it as the pomodoro task and start its timer), "
  "REMINDLIST (read pending reminders), DND (toggle do not disturb), "
  "MEMORY:<fact to remember about the user: tastes, projects, schedules>, "
  "MEMLIST (read remembered facts), BRIEF (briefing: reminders, TODO, memories, screen), "
  "PAIR (pair with the PC), "
  "PROMPT:<text to send to the coding session>, RESUME (summarize the session). "
  "LOCK and SLEEP are dangerous: first answer CMD:LOCK or CMD:SLEEP with BOT asking "
  "\"are you sure?\"; ONLY if the user says yes/ok in the next message answer CMD:CONFIRM. "
  "If the user confirms something you asked, CMD:CONFIRM. "
  "If the user says they are going to work on something (starting or resuming a task), "
  "answer CMD:FOCUS:<mode>:<task> with PAGE:Pomodoro, classifying the mode as work "
  "(default: tasks, study, code), writing (write, draft, thesis, essay) or leisure "
  "(hobby, drawing, casual reading), and your BOT reply must contain exactly one short "
  "practical tip tailored to that task and block. "
  "Return EXACTLY these five lines, nothing else:\n"
  "USER: <literal transcription>\nBOT: <your answer>\nEMO: <happy|neutral|angry|bored>\n"
  "PAGE: <screen or ->\nCMD: <action or ->";

static const char* CHAT_SYS_ES =
  "Eres KT1, un asistente de voz en espanol, simpatico y directo. Responde SIEMPRE en "
  "espanol, natural y BREVE (maximo 2 frases, menos de 170 caracteres, sin listas "
  "ni markdown; se leera en voz alta). Eliges EMOCION entre happy, neutral, angry, bored. "
  "Cada mensaje lleva una FOTO 320x240 de la pantalla del PC (puede faltar): usala "
  "para responder que se ve, sin decir que es una foto. "
  "Puedes pedir CAMBIO DE PANTALLA del dispositivo entre: Cara, Ejercicio, Pomodoro, "
  "Plantas, Vital, Tiempo, Reloj, Ajustes, Chat (o - si no cambia). "
  "Puedes pedir UNA ACCION del PC entre: - (ninguna), MUTE (silenciar), PLAY (pausa o play), "
  "VOL:<0-100> (volumen), OPEN:<app: bloc, calculadora, terminal, codigo, chrome, spotify> (abrir), "
  "LOCK (bloquear), SLEEP (suspender), SCREEN (decir que se ve en pantalla), "
  "PLAY:<texto o URL de video para reproducir en el PC>, "
  "TODO:<texto para apuntar>, REMIND:<segundos>:<texto para avisar> (minimo 60 s), "
  "FOCUS:<work|writing|leisure>:<tarea corta, maximo 40 caracteres> (el usuario se pone "
  "con algo: atalo como tarea del pomodoro y arranca su timer), "
  "REMINDLIST (leer avisos pendientes), DND (alternar no molestar), "
  "MEMORY:<dato para recordar de ti: gustos, proyectos, horarios>, "
  "MEMLIST (leer lo recordado), BRIEF (el parte: avisos, TODO, recuerdos, pantalla), "
  "PAIR (emparejar con el PC), "
  "PROMPT:<texto para enviar a la sesion de codigo>, RESUME (resumir la sesion). "
  "LOCK y SLEEP son peligrosos: primero responde CMD:LOCK o CMD:SLEEP con BOT preguntando "
  "\"seguro?\"; SOLO si el usuario dice si/vale en el mensaje siguiente respondes CMD:CONFIRM. "
  "Si el usuario confirma algo que preguntaste, CMD:CONFIRM. "
  "Si el usuario dice que se pone a trabajar en algo (empieza o retoma una tarea), "
  "responde CMD:FOCUS:<modo>:<tarea> con PAGE:Pomodoro, clasificando el modo como work "
  "(defecto: tareas, estudio, codigo), writing (escribir, redactar, tesis, ensayo) o leisure "
  "(hobby, dibujo, lectura tranquila), y tu respuesta BOT debe contener exactamente un consejo "
  "practico y corto adaptado a esa tarea y ese bloque. "
  "Devuelve EXACTAMENTE estas cinco lineas, sin nada mas:\n"
  "USER: <transcripcion literal>\nBOT: <tu respuesta>\nEMO: <happy|neutral|angry|bored>\n"
  "PAGE: <pantalla o ->\nCMD: <accion o ->";

#define CHAT_SYS (gLang == LANG_ES ? CHAT_SYS_ES : CHAT_SYS_EN)

char chatUser[200] = "";
// Initial placeholder: refreshed in the current language (chatHint) while there
// is no real answer yet, since a boot-time buffer would not follow gLang.
char chatBot[420]  = "Tap TALK and speak";
char chatSub[240]  = "";   // subtitle: chunk being spoken (advances with the voice)
bool chatBusy = false;
bool chatHasAnswer = false;   // there is a real answer (avoids replaying the initial text)
static const char* chatHint() { return TR("Tap TALK and speak", "Toca HABLAR y habla"); }
// Recording in progress (for the progress bar and level meter in pages.h)
uint32_t chatRecT0 = 0, chatRecSpan = 4000;
int chatLevel = 0;            // mic level 0..100 (smoothed peak)

// Visible turn state (drawn by pages.h): recording / thinking / speaking.
// The turn is blocking, so it is repainted by hand with render() + chatAlive().
enum ChatState { CHAT_IDLE, CHAT_REC, CHAT_THINK, CHAT_TALK };
ChatState chatState = CHAT_IDLE;
void render();   // .ino: repaint now (immediate feedback, does not wait for loop)
static uint32_t chatLastRender = 0;
static void chatAlive(uint32_t every = 400) {
  uint32_t now = millis();
  if (now - chatLastRender >= every) { chatLastRender = now; render(); }
}

static uint8_t* chatBuf = nullptr;
static size_t   chatLen = 0;
// PC photo for vision (JPEG 320x240 ~5-20 KB). Best-effort: if it fails,
// the turn continues with voice only.
#define CHAT_SHOT_MAX  (48 * 1024)
static uint8_t* chatShotBuf = nullptr;
static size_t   chatShotLen = 0;
static I2SClass chatMic;
// Deferred Audio: the ESP32-audioI2S constructor allocates large buffers.
// As a global it ate the internal heap BEFORE Wi-Fi started and V3 could not
// connect (V2 has no such object). Created on demand.
static Audio*   chatAudioPtr = nullptr;
static Audio& chatDev() {
  if (!chatAudioPtr) chatAudioPtr = new Audio(false, 3, I2S_NUM_1);
  return *chatAudioPtr;
}

// Eyes per turn state (used by currentMood on the CHAT page).
// Listening = surprised (wide open), thinking = amber half-closed,
// speaking = happy, idle = neutral.
Mood chatEyeMood() {
  switch (chatState) {
    case CHAT_REC:   return M_SURPRISED;
    case CHAT_THINK: return M_THINK;
    case CHAT_TALK:  return M_HAPPY;
    default:         return M_NEUTRAL;
  }
}

void chatInit() {
  // PSRAM ONLY for the voice buffer (160 KB). The internal RAM fallback
  // (64 KB) left the Wi-Fi/TLS driver without contiguous heap and V3 could
  // not connect with the same credentials as V2. No PSRAM: no recording
  // (TTS is still available, it uses little RAM on demand).
  if (psramFound()) chatBuf = (uint8_t*)ps_malloc(CHAT_REC_BYTES);
  if (chatBuf) Serial.printf("[CHAT] ready (%u B in PSRAM)\n", (unsigned)CHAT_REC_BYTES);
  else Serial.println("[CHAT] no PSRAM -> voice OFF (enable Tools > PSRAM > OPI PSRAM). TTS available.");
  if (psramFound()) chatShotBuf = (uint8_t*)ps_malloc(CHAT_SHOT_MAX);
}

// Best-effort download of the bridge JPEG (GET /screen/shot). Silent:
// a failure just leaves the turn without vision.
static bool chatShot();

// Bridge binary into RAM (for the photo). Returns bytes or 0.
static size_t bridgeGetBin(const String& path, uint8_t* buf, size_t cap);

// --- recording: sensor mode (while held) or button mode (fixed duration) ---
static size_t chatRecord(uint32_t fixedMs = 0) {
  es8311_codec_set_fs(CHAT_REC_HZ);
  es8311_codec_set_mic_gain(ES8311_MIC_GAIN_30DB);
  chatMic.setPins(I2S_BCK, I2S_WS, I2S_DOUT, I2S_DIN, I2S_MCK);
  if (!chatMic.begin(I2S_MODE_STD, CHAT_REC_HZ, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT)) {
    Serial.println("[CHAT] i2s mic failed"); return 0;
  }
  size_t cap = chatBuf ? CHAT_REC_BYTES : 0;
  if (!cap) return 0;
  chatLen = 0;
  chatRecT0 = millis();
  chatRecSpan = fixedMs ? fixedMs : (uint32_t)CHAT_REC_MAX_S * 1000;
  chatLevel = 0;
  int16_t chunk[256];
  uint32_t t0 = millis(), lastHeld = t0;
  while (chatLen + sizeof(chunk) <= cap) {
    uint32_t now = millis();
    if (fixedMs) {
      if (now - t0 >= fixedMs) break;   // button: record N ms and done
    } else {
#if PET_ENABLED
      if (digitalRead(PET_PIN) == HIGH) lastHeld = now;
#else
      lastHeld = now;   // no touch sensor (pin reused, e.g. flash LED): record to max
#endif
      if (now - t0 > CHAT_MIN_MS && now - lastHeld > 150) break;
      if (now - t0 > (uint32_t)CHAT_REC_MAX_S * 1000) break;
    }
    size_t n = chatMic.readBytes((char*)chunk, sizeof(chunk));
    if (!n) { delay(1); continue; }
    int peak = 0;   // level for the on-screen meter
    for (size_t i = 0; i < n / 2; i++) { int v = abs(chunk[i]); if (v > peak) peak = v; }
    chatLevel = (chatLevel * 2 + peak * 100 / 32767) / 3;
    memcpy(chatBuf + chatLen, chunk, n);
    chatLen += n;
    chatAlive();   // live eyes + "Listening" while recording
  }
  chatMic.end();
  Serial.printf("[CHAT] recorded %u B\n", (unsigned)chatLen);
  return chatLen;
}

static void chatNormalize() {
  size_t ns = chatLen / 2;
  if (!ns) return;
  int16_t* s = (int16_t*)chatBuf;
  long sum = 0;
  for (size_t i = 0; i < ns; i++) sum += s[i];
  int dc = (int)(sum / (long)ns), peak = 0;
  for (size_t i = 0; i < ns; i++) { int v = abs(s[i] - dc); if (v > peak) peak = v; }
  if (peak < 250) { Serial.println("[CHAT] silence, not amplifying"); return; }
  float g = (float)29000 / peak;
  if (g > 12.0f) g = 12.0f;
  if (g < 1.0f) g = 1.0f;
  for (size_t i = 0; i < ns; i++) {
    int v = (int)lroundf((s[i] - dc) * g);
    if (v > 32767) v = 32767; else if (v < -32768) v = -32768;
    s[i] = (int16_t)v;
  }
}

static void chatWavHdr(uint8_t* h, uint32_t dataLen) {
  uint32_t br = CHAT_REC_HZ * 2, cs = 36 + dataLen;
  memcpy(h, "RIFF", 4);
  h[4]=cs; h[5]=cs>>8; h[6]=cs>>16; h[7]=cs>>24;
  memcpy(h + 8, "WAVEfmt ", 8);
  h[16]=16; h[17]=0; h[18]=0; h[19]=0; h[20]=1; h[21]=0; h[22]=1; h[23]=0;
  h[24]=CHAT_REC_HZ; h[25]=CHAT_REC_HZ>>8; h[26]=0; h[27]=0;
  h[28]=br; h[29]=br>>8; h[30]=br>>16; h[31]=br>>24;
  h[32]=2; h[33]=0; h[34]=16; h[35]=0;
  memcpy(h + 36, "data", 4);
  h[40]=dataLen; h[41]=dataLen>>8; h[42]=dataLen>>16; h[43]=dataLen>>24;
}

static void chatEsc(String& out, const char* s) {
  for (const char* p = s; *p; p++) {
    char c = *p;
    if (c == '"' || c == '\\') { out += '\\'; out += c; }
    else if (c == '\n') out += "\\n";
    else if (c != '\r') out += c;
  }
}

// --- Readable TFT text: the screen only has ASCII glyphs. Any accented
// UTF-8 (2 bytes 0xC3 0x..) or \uXXXX escapes from Gemini showed up as
// "A?", "Ã¡" or "?" during THINKING/SPEAKING. Everything is converted to ASCII
// here: accents stripped, ¿?/¡! simplified, markdown (*#_`>) removed, spaces
// collapsed. The TTS still understands the ASCII fine.
static char chatDeaccent(uint16_t u) {
  switch (u) {
    case 0x00C0: case 0x00C1: case 0x00C2: case 0x00C3: case 0x00C4: case 0x00C5:
    case 0x00E0: case 0x00E1: case 0x00E2: case 0x00E3: case 0x00E4: case 0x00E5:
      return 'a';
    case 0x00C8: case 0x00C9: case 0x00CA: case 0x00CB:
    case 0x00E8: case 0x00E9: case 0x00EA: case 0x00EB:
      return 'e';
    case 0x00CC: case 0x00CD: case 0x00CE: case 0x00CF:
    case 0x00EC: case 0x00ED: case 0x00EE: case 0x00EF:
      return 'i';
    case 0x00D2: case 0x00D3: case 0x00D4: case 0x00D5: case 0x00D6: case 0x00D8:
    case 0x00F2: case 0x00F3: case 0x00F4: case 0x00F5: case 0x00F6: case 0x00F8:
      return 'o';
    case 0x00D9: case 0x00DA: case 0x00DB: case 0x00DC:
    case 0x00F9: case 0x00FA: case 0x00FB: case 0x00FC:
      return 'u';
    case 0x00D1: case 0x00F1: return 'n';
    case 0x00C7: case 0x00E7: return 'c';
    case 0x00BF: return '?';   // ¿ -> ?
    case 0x00A1: return '!';   // ¡ -> !
    case 0x00BA: case 0x00AA: return 'o';   // º ª
    case 0x00AB: case 0x00BB: return '"';   // « »
    case 0x2018: case 0x2019: case 0x201A: return '\'';
    case 0x201C: case 0x201D: case 0x201E: return '"';
    case 0x2013: case 0x2014: return '-';
    case 0x2026: return '.';
    case 0x20AC: return 'E';   // €
    default: break;
  }
  if (u < 0x80) return (char)u;
  return 0;   // unknown: skipped (used to be '?')
}
static void chatAppendUnicode(String& out, uint16_t u) {
  char c = chatDeaccent(u);
  if (c) out += c;
}
// Converts a \uXXXX (4 hex digits at p) to a code point. Returns false if not hex.
static bool chatParseU(const char* p, uint16_t& u) {
  u = 0;
  for (int k = 0; k < 4; k++) {
    char c = p[k];
    u <<= 4;
    if (c >= '0' && c <= '9') u |= (c - '0');
    else if (c >= 'a' && c <= 'f') u |= (c - 'a' + 10);
    else if (c >= 'A' && c <= 'F') u |= (c - 'A' + 10);
    else return false;
  }
  return true;
}
// Cleans text for screen+voice: UTF-8 -> ASCII, no markdown, no line
// breaks, collapsed spaces, trimmed to cap-1 with "..." if needed.
static void chatToAscii(char* dst, size_t cap, const char* src) {
  if (!cap) return;
  if (!src) { dst[0] = 0; return; }
  size_t o = 0;
  bool space = true;   // collapses spaces (starts by eating leading ones)
  for (const uint8_t* p = (const uint8_t*)src; *p && o + 1 < cap; ) {
    uint8_t c = *p;
    char base = 0;
    size_t adv = 1;
    if (c < 0x80) {
      if (c == '*' || c == '#' || c == '_' || c == '`' || c == '>' || c == '~' ||
          c == '|' || c == '\t' || c == '\r' || c == '\n') { p++; space = (o > 0); continue; }
      if (c < 0x20) { p++; continue; }   // drop other control chars
      base = (char)c;
    } else if ((c & 0xE0) == 0xC0 && p[1]) {
      uint16_t u = ((uint16_t)(c & 0x1F) << 6) | (p[1] & 0x3F);
      base = chatDeaccent(u);
      adv = 2;
    } else if ((c & 0xF0) == 0xE0 && p[1] && p[2]) {
      if (c == 0xE2 && p[1] == 0x80 && p[2] >= 0x93 && p[2] <= 0x9D) {
        // common typographic quotes/dashes
        if (p[2] == 0x93 || p[2] == 0x94) base = '-';
        else if (p[2] == 0x98 || p[2] == 0x99) base = '\'';
        else base = '"';
      } else if (c == 0xE2 && p[1] == 0x82 && p[2] == 0xAC) base = 'E';
      else if (c == 0xE2 && p[1] == 0x80 && p[2] == 0xA6) base = '.';
      adv = 3;
    } else {
      p++; continue;   // stray byte: skipped without drawing '?'
    }
    if (!base) { p += adv; continue; }
    if (base == ' ') {
      if (!space && o + 1 < cap) { dst[o++] = ' '; space = true; }
      p += adv;
    } else {
      if (o + 1 < cap) { dst[o++] = base; space = false; }
      p += adv;
    }
  }
  while (o && dst[o - 1] == ' ') o--;   // no trailing space
  dst[o] = 0;
  // If cut by cap, mark "..." (makes clear it continues in voice/scroll).
  if (src[0] && o + 1 >= cap && o >= 3) {
    // only if the source was longer than what fit
    size_t sl = strlen(src);
    if (sl >= cap) strcpy(dst + o - 3, "...");
  }
}
// Stores into chatBot normalized to readable ASCII (bridge tools may bring
// accents/markdown just like Gemini). Single entry point.
static void chatSetBot(const char* src) {
  chatToAscii(chatBot, sizeof(chatBot), src ? src : "");
}

static bool chatTlsWrite(WiFiClientSecure& c, const uint8_t* b, size_t n) {
  size_t sent = 0;
  uint32_t t0 = millis();
  while (sent < n) {
    int w = c.write(b + sent, n - sent);
    if (w > 0) { sent += w; t0 = millis(); }
    else if (millis() - t0 > 15000) return false;
    else delay(1);
  }
  return true;
}

// Streams the WAV as base64, returns raw text (with USER/BOT/EMO/PAGE/CMD)
static bool chatAskGemini(String& answer) {
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(20000);
  if (!client.connect("generativelanguage.googleapis.com", 443)) return false;
  String sysEsc; chatEsc(sysEsc, CHAT_SYS);
  String head = "{\"system_instruction\":{\"parts\":[{\"text\":\"" + sysEsc + "\"}]},"
    "\"contents\":[{\"role\":\"user\",\"parts\":[{\"text\":\"" +
    TR("Listen to this audio and reply.", "Escucha este audio y responde.") + "\"},"
    "{\"inline_data\":{\"mime_type\":\"audio/wav\",\"data\":\"";
  String imgMid = "\"}},{\"inline_data\":{\"mime_type\":\"image/jpeg\",\"data\":\"";
  String tail = "\"}}]}],\"generationConfig\":{\"temperature\":0.7,\"maxOutputTokens\":220}}";
  size_t rawLen = 44 + chatLen, b64Len = ((rawLen + 2) / 3) * 4;
  bool hasImg = chatShotLen > 0;
  size_t imgB64 = hasImg ? ((chatShotLen + 2) / 3) * 4 : 0;
  size_t contentLen = head.length() + b64Len + (hasImg ? imgMid.length() + imgB64 : 0) + tail.length();
  String req = String("POST /v1beta/models/") + GEMINI_MODEL + ":generateContent?key=" + GEMINI_API_KEY +
    " HTTP/1.1\r\nHost: generativelanguage.googleapis.com\r\nContent-Type: application/json\r\nContent-Length: " +
    String((unsigned)contentLen) + "\r\nConnection: close\r\n\r\n" + head;
  if (!chatTlsWrite(client, (const uint8_t*)req.c_str(), req.length())) { client.stop(); return false; }
  const size_t INB = 1536;
  static uint8_t inb[INB], o64[2100];
  uint8_t carry[3]; size_t carryN = 0;
  auto feed = [&](const uint8_t* d, size_t n) -> bool {
    size_t i = 0;
    while (carryN && carryN < 3 && i < n) carry[carryN++] = d[i++];
    if (carryN == 3) { size_t ol; mbedtls_base64_encode(o64, sizeof(o64), &ol, carry, 3);
      if (!chatTlsWrite(client, o64, ol)) return false; carryN = 0; }
    while (n - i >= INB) { memcpy(inb, d + i, INB); size_t ol;
      mbedtls_base64_encode(o64, sizeof(o64), &ol, inb, INB);
      if (!chatTlsWrite(client, o64, ol)) return false; i += INB; }
    size_t rem = n - i, full = (rem / 3) * 3;
    if (full) { size_t ol; mbedtls_base64_encode(o64, sizeof(o64), &ol, d + i, full);
      if (!chatTlsWrite(client, o64, ol)) return false; i += full; }
    while (i < n) carry[carryN++] = d[i++];
    return true;
  };
  uint8_t hdr[44]; chatWavHdr(hdr, chatLen);
  if (!feed(hdr, 44) || !feed(chatBuf, chatLen)) { client.stop(); return false; }
  auto flushCarry = [&]() -> bool {
    if (!carryN) return true;
    size_t ol; mbedtls_base64_encode(o64, sizeof(o64), &ol, carry, carryN);
    carryN = 0;
    return chatTlsWrite(client, o64, ol);
  };
  if (!flushCarry()) { client.stop(); return false; }
  if (hasImg) {   // best-effort vision: 320x240 PC photo along with the audio
    if (!chatTlsWrite(client, (const uint8_t*)imgMid.c_str(), imgMid.length())) { client.stop(); return false; }
    if (!feed(chatShotBuf, chatShotLen)) { client.stop(); return false; }
    if (!flushCarry()) { client.stop(); return false; }
  }
  if (!chatTlsWrite(client, (const uint8_t*)tail.c_str(), tail.length())) { client.stop(); return false; }
  String sl = client.readStringUntil('\n');
  int code = 0, sp = sl.indexOf(' ');
  if (sp > 0) code = sl.substring(sp + 1, sp + 4).toInt();
  bool chunked = false; long clen = -1;
  while (client.connected() || client.available()) {
    String ln = client.readStringUntil('\n');
    if (ln == "\r" || !ln.length()) break;
    String lw = ln; lw.toLowerCase();
    if (lw.startsWith("transfer-encoding:") && lw.indexOf("chunked") >= 0) chunked = true;
    if (lw.startsWith("content-length:")) clen = lw.substring(15).toInt();
  }
  String body; body.reserve(2048);
  uint32_t t0 = millis();
  if (chunked) {
    while (millis() - t0 < 20000) {
      String sz = client.readStringUntil('\n'); sz.trim();
      if (!sz.length()) { if (!client.connected() && !client.available()) break; continue; }
      long cs = strtol(sz.c_str(), NULL, 16);
      if (cs <= 0) break;
      long got = 0;
      while (got < cs && millis() - t0 < 20000) {
        if (client.available()) { int c = client.read(); if (c < 0) break; body += (char)c; got++; t0 = millis(); }
        else delay(2);
        chatAlive();   // animated "thinking" face while the answer arrives
      }
      client.readStringUntil('\n');
    }
  } else {
    while ((client.connected() || client.available()) && millis() - t0 < 20000) {
      while (client.available()) { body += (char)client.read(); t0 = millis(); }
      if (clen >= 0 && (long)body.length() >= clen) break;
      delay(3);
      chatAlive();   // animated "thinking" face while the answer arrives
    }
  }
  client.stop();
  if (code != 200) { Serial.printf("[CHAT] Gemini HTTP %d\n", code); return false; }
  int idx = body.indexOf("\"text\"");
  if (idx < 0) return false;
  int colon = body.indexOf(':', idx + 6), open = body.indexOf('"', colon + 1);
  answer = "";
  answer.reserve(512);
  for (int i = open + 1; i < (int)body.length(); i++) {
    char c = body[i];
    if (c == '\\') {
      char n2 = body[i + 1];
      if (n2 == 'n' || n2 == 'r') answer += ' ';
      else if (n2 == 't') answer += ' ';
      else if (n2 == 'b' || n2 == 'f') answer += ' ';
      else if (n2 == '"') answer += '"';
      else if (n2 == '\'') answer += '\'';
      else if (n2 == '\\' || n2 == '/') answer += n2 == '/' ? '/' : '\\';
      else if (n2 == 'u' && i + 5 < (int)body.length()) {
        uint16_t u = 0;
        if (chatParseU(body.c_str() + i + 2, u)) chatAppendUnicode(answer, u);
        // if not valid hex it is skipped without drawing '?'
        i += 5;
        continue;
      } else if (n2) answer += n2;
      i++;
    } else if (c == '"') break;
    else answer += c;
  }
  answer.trim();
  // Gemini sometimes returns accented UTF-8 or markdown: normalized to ASCII
  // readable on the TFT (it used to show "Ã¡", "?" or "*"). The voice says it the same.
  {
    char tmp[512];
    chatToAscii(tmp, sizeof(tmp), answer.c_str());
    answer = tmp;
  }
  return answer.length() > 0;
}

static String chatField(const String& full, const char* tag) {
  int i = full.indexOf(tag);
  if (i < 0) return "";
  String v = full.substring(i + strlen(tag));
  int nl = v.indexOf('\n');
  if (nl >= 0) v = v.substring(0, nl);
  v.trim();
  return v;
}

static uint32_t chatHash(const String& s) {
  uint32_t h = 5381;
  for (size_t i = 0; i < s.length(); i++) h = h * 33 + (uint8_t)s[i];
  return h;
}
static uint32_t chatLastHash = 0, chatLastSpoke = 0;

static void chatSpeak(const String& text, bool allowDup) {
  uint32_t now = millis(), h = chatHash(text);
  if (!allowDup && h == chatLastHash && now - chatLastSpoke < 5000) {
    Serial.println("[CHAT] duplicate echo suppressed");   // same text <5 s: do not replay
    return;
  }
  chatLastHash = h; chatLastSpoke = now;
  if (!gSoundOn) return;   // voice only with sound on (the text stays on screen)
  es8311_codec_set_fs(CHAT_PLAY_HZ);
  Audio& dev = chatDev();
  dev.setPinout(I2S_BCK, I2S_WS, I2S_DOUT, I2S_MCK);
  dev.forceMono(true);
  dev.setVolume(15, 1);
  String rest = text;
  while (rest.length()) {
    String piece;
    if (rest.length() <= CHAT_TTS_CHARS) { piece = rest; rest = ""; }
    else {
      int cut = -1;
      for (int i = CHAT_TTS_CHARS; i > 20; i--) {
        char c = rest[i];
        if (c == '.' || c == ',' || c == ';' || c == ' ') { cut = i; break; }
      }
      if (cut < 0) cut = CHAT_TTS_CHARS;
      piece = rest.substring(0, cut);
      rest = rest.substring(cut); rest.trim();
    }
    piece.trim();
    if (!piece.length()) continue;
    strlcpy(chatSub, piece.c_str(), sizeof(chatSub));   // update subtitle before speaking it
    gDirty = true; render();
    dev.connecttospeech(piece.c_str(), LANG_CODES[gLang]);
    uint32_t guard = millis();
    while ((dev.isRunning() || millis() - guard < 700) && millis() - guard < 20000) {
      dev.loop();
      if (dev.isRunning()) guard = millis();
      delay(2);
      chatAlive(1200);   // live eyes without choppy audio
    }
  }
  dev.stopSong();
}

static bool chatShot() {
  chatShotLen = 0;
  if (!chatShotBuf) return false;
  size_t n = bridgeGetBin("/screen/shot", chatShotBuf, CHAT_SHOT_MAX);
  if (n < 1024) { chatShotLen = 0; return false; }   // broken JPEG or 501
  chatShotLen = n;
  Serial.printf("[CHAT] photo %u B\n", (unsigned)n);
  return true;
}

static void chatReplay() {
  if (chatBusy || !chatBot[0]) return;
  if (!chatHasAnswer) { toast(TR("Speak first", "Habla primero"), 1200); return; }   // do not replay the initial text
  if (!gSoundOn) { toast(TR("Voice off: text only", "Voz off: solo texto"), 1200); return; }
  chatBusy = true;
  chatState = CHAT_TALK; chatLastRender = millis();
  soundAudioSuspend();
  gDirty = true; render();
  chatSpeak(chatBot, true);   // explicit replay: always plays
  soundAudioResume();
  chatState = CHAT_IDLE;
  gDirty = true; render();
  chatBusy = false;
}

static void chatApplyPage(const String& page) {
  // Accepts the page names of both languages (the prompt uses the current one)
  struct { const char* name; int idx; } map[] = {
    {"Face", PAGE_FACE}, {"Exercise", PAGE_EXERCISE}, {"Pomodoro", PAGE_POMO},
    {"Plants", PAGE_PLANTS}, {"Vital", PAGE_VITAL}, {"Weather", PAGE_WEATHER},
    {"Clock", PAGE_CLOCK}, {"Settings", PAGE_SET_SOUND},
    {"Sound", PAGE_SET_SOUND}, {"Screen", PAGE_SET_SCREEN}, {"Timers", PAGE_SET_TIME},
    {"Cara", PAGE_FACE}, {"Ejercicio", PAGE_EXERCISE},
    {"Plantas", PAGE_PLANTS}, {"Tiempo", PAGE_WEATHER},
    {"Reloj", PAGE_CLOCK}, {"Ajustes", PAGE_SET_SOUND},
    {"Sonido", PAGE_SET_SOUND}, {"Pantalla", PAGE_SET_SCREEN}, {"Tiempos", PAGE_SET_TIME},
    {"Chat", PAGE_CHAT},
  };
  for (auto& m : map)
    if (page.equalsIgnoreCase(m.name)) { setPage(m.idx); return; }
}

static void chatApplyCmd(String cmd, const char* botText = nullptr) {
  cmd.trim();
  if (!cmd.length() || cmd == "-") return;
  if (cmd.equalsIgnoreCase("MUTE")) { remoteMuteToggle(); return; }
  if (cmd.equalsIgnoreCase("PLAY")) { remotePlayPause(); return; }
  if (cmd.equalsIgnoreCase("LOCK")) { remoteLock(); return; }    // only via CONFIRM
  if (cmd.equalsIgnoreCase("SLEEP")) { remoteSleep(); return; }  // only via CONFIRM
  if (cmd.startsWith("OPEN:")) { String t = cmd.substring(5); t.trim(); remoteOpen(t.c_str()); return; }
  if (cmd.startsWith("VOL:")) { String v = cmd.substring(4); v.trim(); if (v.length() && isdigit(v[0])) remoteVolume(v.toInt()); return; }
  if (cmd.startsWith("TODO:")) { String t = cmd.substring(5); t.trim(); remoteTodo(t.c_str()); return; }
  // FOCUS:<mode>:<task> — bind a pomodoro task, switch to its mode and start focus.
  // The spoken BOT answer carries the tailored tip: keep it for the break card.
  if (cmd.startsWith("FOCUS:")) {
    String r = cmd.substring(6);
    int sep = r.indexOf(':');
    String m = (sep > 0) ? r.substring(0, sep) : r;
    String t = (sep > 0) ? r.substring(sep + 1) : "";
    m.trim(); m.toLowerCase(); t.trim();
    int mode = POMO_TRABAJO;
    if (m.startsWith("writ") || m.startsWith("escr")) mode = POMO_ESCRITURA;
    else if (m.startsWith("leis") || m.startsWith("oci")) mode = POMO_OCIO;
    pomoMode = mode; pomoSyncModeDur(); pomoWriteSlot();
    char tmp[64];
    chatToAscii(tmp, sizeof(tmp), t.c_str());
    String ts = tmp; ts.trim();
    if (ts.length()) { strlcpy(pomoTask, ts.c_str(), sizeof(pomoTask)); pomoHasTask = true; }
    else pomoHasTask = false;
    if (botText && botText[0]) {
      char tip[128];
      chatToAscii(tip, sizeof(tip), botText);
      strlcpy(pomoTip, tip, sizeof(pomoTip));
    } else pomoTip[0] = 0;
    setPage(PAGE_POMO);
    pomoStartFocus();
    if (pomoHasTask) pomoSay(pomoTask, 1600);
    else pomoSay(pomoRelaxLine(), 1800);
    gDirty = true;
    return;
  }
  if (cmd.startsWith("MEMORY:")) { String t = cmd.substring(7); t.trim(); remoteRemember(t.c_str()); return; }
  if (cmd.startsWith("PROMPT:")) { String t = cmd.substring(7); t.trim(); ocPrompt(t.c_str()); return; }
  if (cmd.startsWith("REMIND:")) {
    String r = cmd.substring(7);
    int sep = r.indexOf(':');
    if (sep > 0) remoteRemind(r.substring(0, sep).toInt(), r.substring(sep + 1).c_str());
    return;
  }
}

// Full turn (blocking). fixedMs=0: sensor held; >0: TALK button.
// While it runs, the screen repaints itself (chatAlive) and no other turn
// is accepted: chatBusy + chatTap block it (the button shows "...").
// LOCK/SLEEP wait for CONFIRM ("yes") in the next turn (60 s max).
static char pendingCmd[12] = "";
static uint32_t pendingMs = 0;
static void chatTurn(uint32_t fixedMs = 0) {
  if (chatBusy) return;
  if (!chatBuf) { toast(TR("Voice off: no PSRAM", "Voz off: sin PSRAM"), 1500); return; }
  chatBusy = true;
  if (!chatHasAnswer) strlcpy(chatBot, chatHint(), sizeof(chatBot));   // placeholder in the current language
  chatState = CHAT_REC; chatLastRender = millis();
  soundAudioSuspend();
  toast(fixedMs ? TR("Recording... speak!", "Grabando... habla!") : TR("Listening...", "Escuchando..."), 1500);
  gDirty = true; render();   // feedback now, without waiting for the loop
  size_t n = chatRecord(fixedMs);
  if (n < CHAT_REC_HZ) {  // <0.5 s: nothing useful
    toast(TR("Didn't hear you", "No te oi"), 1200);
    soundAudioResume();
    chatState = CHAT_IDLE;
    gDirty = true; render();
    chatBusy = false;
    return;
  }
  chatNormalize();
  chatState = CHAT_THINK;
  toast(TR("Thinking...", "Pensando..."), 2000);
  gDirty = true; render();   // "thinking" face from the very first moment
  chatShot();   // best-effort vision: the photo travels with the audio to Gemini
  String raw;
  String bot = TR("Sorry, I couldn't answer.", "Lo siento, no pude responder.");
  String emo = "bored", page = "-", cmd = "-";
  bool ok = chatAskGemini(raw);
  if (ok) {
    String user = chatField(raw, "USER:");
    bot  = chatField(raw, "BOT:");  if (!bot.length()) bot = raw;
    String e = chatField(raw, "EMO:"); e.toLowerCase();
    if (e.startsWith("happy")) emo = "happy";
    else if (e.startsWith("angry")) emo = "angry";
    else if (e.startsWith("bored")) emo = "bored";
    else emo = "neutral";
    page = chatField(raw, "PAGE:");
    cmd  = chatField(raw, "CMD:");
    // Everything drawn (and spoken) goes through safe ASCII: the TFT has no
    // accents and broke them ("Ã¡"); it also collapses Gemini's \n and markdown.
    {
      char tmp[512];
      chatToAscii(tmp, sizeof(tmp), user.c_str());
      user = tmp;
      user.trim();
      snprintf(chatUser, sizeof(chatUser), "%s", user.c_str());
    }
    chatHasAnswer = true;
  }
  {
    char tmp[512];
    // bot is already ASCII from chatAskGemini, but the raw fallback and tool
    // texts (bridge) may bring UTF-8: normalized anyway.
    bot.replace("\n", " "); bot.replace("\r", " "); bot.trim();
    chatToAscii(tmp, sizeof(tmp), bot.c_str());
    // bot already trimmed to 512 by the parse; chatBot (420) keeps the full
    // version for voice+scroll (it used to be cut to 200 and looked tiny).
    snprintf(chatBot, sizeof(chatBot), "%s", tmp);
    bot = chatBot;
  }
  Serial.printf("[CHAT] BOT=%s EMO=%s PAGE=%s CMD=%s\n", bot.c_str(), emo.c_str(), page.c_str(), cmd.c_str());
  if (emo == "happy") setMoodFor(M_HAPPY, MANUAL_MOOD_MS);
  else if (emo == "angry") setMoodFor(M_ANGRY, MANUAL_MOOD_MS);
  else setMoodFor(M_NEUTRAL, MANUAL_MOOD_MS);
  chatApplyPage(page);
  chatState = CHAT_TALK;
  gDirty = true; render();
  // LOCK/SLEEP are dangerous: they ask for "yes" in the next turn (60 s).
  if (pendingMs && millis() - pendingMs > 60000) { pendingCmd[0] = 0; pendingMs = 0; }
  if (cmd.equalsIgnoreCase("CONFIRM")) {
    if (pendingCmd[0] && pendingMs) {
      String done = pendingCmd;
      pendingCmd[0] = 0; pendingMs = 0;
      chatSpeak(bot, false);
      chatApplyCmd(done);   // runs the pending command (LOCK/SLEEP)
    } else {
      strlcpy(chatBot, TR("There was nothing to confirm", "No habia nada que confirmar"), sizeof(chatBot));
      chatSpeak(chatBot, false);
    }
  } else if (cmd.equalsIgnoreCase("LOCK") || cmd.equalsIgnoreCase("SLEEP")) {
    strlcpy(pendingCmd, cmd.c_str(), sizeof(pendingCmd));
    pendingMs = millis();
    snprintf(chatBot, sizeof(chatBot), TR("%s? Say yes to confirm", "%s? Di si para confirmar"),
             cmd.equalsIgnoreCase("LOCK") ? TR("Lock the PC", "Bloqueo el PC") : TR("Suspend the PC", "Suspendo el PC"));
    chatSpeak(chatBot, false);
  } else if (cmd.equalsIgnoreCase("RESUME")) {
    // The summary IS the answer: spoken only once (the bot acknowledgement +
    // the summary used to play and it sounded repeated).
    if (ocResume()) chatSetBot(ocResumeBuf);
    chatSpeak(chatBot, false);
  } else if (cmd.equalsIgnoreCase("SCREEN")) {
    // See-screen tool: the result IS spoken (it is new information).
    if (bridgeScreen()) chatSetBot(screenBuf);
    chatSpeak(chatBot, false);
  } else if (cmd.equalsIgnoreCase("REMINDLIST")) {
    if (remoteRemindList()) chatSetBot(remBuf);
    chatSpeak(chatBot, false);
  } else if (cmd.equalsIgnoreCase("MEMLIST")) {
    if (remoteMemList()) chatSetBot(memBuf);
    chatSpeak(chatBot, false);
  } else if (cmd.equalsIgnoreCase("BRIEF")) {
    if (remoteBrief()) chatSetBot(memBuf);
    chatSpeak(chatBot, false);
  } else if (cmd.equalsIgnoreCase("DND")) {
    String spoken;
    if (remoteDndToggle(spoken)) chatSetBot(spoken.c_str());
    chatSpeak(chatBot, false);
  } else if (cmd.equalsIgnoreCase("PAIR")) {
    String spoken;
    if (bridgePair(spoken)) chatSetBot(spoken.c_str());
    else chatSetBot(spoken.length() ? spoken.c_str() : TR("Couldn't pair", "No pude emparejar"));
    chatSpeak(chatBot, false);
  } else if (cmd.startsWith("PLAY:")) {
    String q = cmd.substring(5); q.trim();
    if (q.length()) {
      remotePlayUrl(q.c_str());
      char tmp[512];
      chatToAscii(tmp, sizeof(tmp), q.c_str());
      snprintf(chatBot, sizeof(chatBot), TR("Playing %s on the PC", "Poniendo %s en el PC"), tmp);
      chatSpeak(chatBot, false);
    } else {
      chatSpeak(bot, false);
    }
  } else {
    chatSpeak(bot, false);
    chatApplyCmd(cmd, bot.c_str());   // after speaking: MUTE/PLAY/LOCK/TODO/FOCUS/PROMPT...
  }
  soundAudioResume();
  chatState = CHAT_IDLE;
  ts.down = false; ts.longFired = false;   // discard touches made during the turn
  gDirty = true; render();
  chatBusy = false;
}
