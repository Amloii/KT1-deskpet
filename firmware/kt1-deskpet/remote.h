// ===========================================================================
//  remote.h — V3: HTTP client for the PC's bridge.py (mute, lock, OpenCode...).
//
//  The bridge lives at http://PC_HOST:BRIDGE_PORT (see secrets.h). Short
//  blocking calls (2-4 s): only from gestures/voice, never every frame.
//  Requires Wi-Fi (gNet == NET_OK); otherwise, toast and done.
//  Every request carries ?lang=en|es (UI language) so the bridge answers its
//  human-readable texts (briefing, reminders, memory, screen, resume) in it.
// ===========================================================================
#pragma once

#include <HTTPClient.h>

#ifndef BRIDGE_TOKEN
#define BRIDGE_TOKEN ""
#endif

// Bridge token: the one in secrets.h or the one paired by voice (NVS "btok").
// So a new PC works without editing secrets: say "pair the PC".
static String gBridgeToken;
static bool bridgeTokenLoaded = false;
static const char* bridgeToken() {
  if (!bridgeTokenLoaded) {
    bridgeTokenLoaded = true;
    String t = prefs.getString("btok", "");
    if (!t.length() && BRIDGE_TOKEN[0]) t = BRIDGE_TOKEN;
    gBridgeToken = t;
  }
  return gBridgeToken.c_str();
}
static void bridgeSaveToken(const String& t) {
  gBridgeToken = t;
  bridgeTokenLoaded = true;
  prefs.putString("btok", t);
}

static String bridgeBase() {
  String b = "http://";
  b += PC_HOST;
  b += ":";
  b += BRIDGE_PORT;
  return b;
}

// Full bridge URL: base + path + lang=<en|es> (with '?' or '&' as needed).
static String bridgeUrl(const String& path) {
  String u = bridgeBase() + path;
  u += (path.indexOf('?') >= 0) ? "&lang=" : "?lang=";
  u += LANG_CODES[gLang];
  return u;
}

static bool bridgePost(const String& path, const String& json) {
  if (gNet != NET_OK) { toast(TR("No Wi-Fi", "Sin Wi-Fi"), 1000); return false; }
  HTTPClient http;
  http.setConnectTimeout(4000);
  http.setTimeout(8000);
  String url = bridgeUrl(path);
  if (!http.begin(url)) return false;
  http.addHeader("Content-Type", "application/json");
  const char* tok = bridgeToken();
  if (tok[0]) http.addHeader("X-Bridge-Token", tok);
  int code = http.POST(json);
  http.end();
  Serial.printf("[BRIDGE] POST %s -> %d\n", path.c_str(), code);
  // 2xx = ok. 501 = known bridge fallback (DND on Win11): not a "failure".
  if (code == 501) { toast(TR("DND not supported", "DND no soportado"), 1200); return true; }
  return code >= 200 && code < 300;
}

static bool bridgeGet(const String& path, String& out) {
  if (gNet != NET_OK) { toast(TR("No Wi-Fi", "Sin Wi-Fi"), 1000); return false; }
  HTTPClient http;
  http.setConnectTimeout(4000);
  http.setTimeout(10000);
  String url = bridgeUrl(path);
  if (!http.begin(url)) return false;
  const char* tok = bridgeToken();
  if (tok[0]) http.addHeader("X-Bridge-Token", tok);
  int code = http.GET();
  bool ok = (code >= 200 && code < 300);
  if (ok) out = http.getString();
  http.end();
  Serial.printf("[BRIDGE] GET %s -> %d (%u B)\n", path.c_str(), code, (unsigned)out.length());
  return ok;
}

// --- 1. Mute / play-pause button (without stealing focus) ---
static void remoteMuteToggle()   { bridgePost("/media/mute-toggle", "{}") ? toast(TR("PC muted", "Mute PC"), 1000) : toast(TR("Mute failed", "Fallo mute"), 1200); }
static void remotePlayPause()    { bridgePost("/media/playpause", "{}")    ? toast(TR("Play/Pause", "Play/Pausa"), 1000) : toast(TR("Media failed", "Fallo media"), 1200); }

// --- 2. Lock / sleep (DND = local fallback: the bridge returns 501) ---
static void remoteLock()  { bridgePost("/os/lock", "{}")  ? toast(TR("PC locked", "PC bloqueado"), 1200) : toast(TR("Lock failed", "Fallo lock"), 1200); }
static void remoteSleep() { toast(TR("PC sleeping...", "Durmiendo PC..."), 1200); bridgePost("/os/sleep", "{}"); }

// --- 4. Voice TODO -> Drive ---
static void remoteTodo(const char* text) {
  if (!text || !text[0]) return;
  JsonDocument d;
  d["text"] = text;
  String j; serializeJson(d, j);
  bridgePost("/todo", j) ? toast(TR("Noted!", "Apuntado!"), 1200) : toast(TR("TODO failed", "Fallo TODO"), 1200);
}

// --- 3. OpenCode launcher ---
static void ocPrompt(const char* text) {
  if (!text || !text[0]) return;
  JsonDocument d;
  d["session"] = OPENCODE_SESSION;
  d["text"] = text;
  String j; serializeJson(d, j);
  bridgePost("/opencode/prompt", j) ? toast(TR("Prompt sent", "Prompt enviado"), 1200) : toast(TR("Prompt failed", "Fallo prompt"), 1200);
}

// Short resume (<400 chars, display-ready) -> left in chatBot, which ONLY reads it.
extern char ocResumeBuf[420];
static bool ocResume() {
  String out;
  String path = String("/opencode/resume?session=") + OPENCODE_SESSION;
  if (!bridgeGet(path, out)) { toast(TR("Resume failed", "Fallo resume"), 1200); return false; }
  JsonDocument d;
  if (deserializeJson(d, out)) { toast(TR("Bad resume", "Resume roto"), 1200); return false; }
  strlcpy(ocResumeBuf, d["resume"] | TR("(empty)", "(vacio)"), sizeof(ocResumeBuf));
  return true;
}
char ocResumeBuf[420] = "";

// --- 4. PC screen (SCREEN tool: which app/title is shown + idle time) ---
extern char screenBuf[200];
static bool bridgeScreen() {
  String out;
  if (!bridgeGet("/screen", out)) { toast(TR("Screen failed", "Fallo pantalla"), 1200); return false; }
  JsonDocument d;
  if (deserializeJson(d, out)) { toast(TR("Bad screen data", "Pantalla rota"), 1200); return false; }
  const char* app   = d["app"]   | "";
  const char* title = d["title"] | "";
  int idle = d["idle"] | -1;
  if (!app[0] && !title[0]) { strlcpy(screenBuf, TR("I see nothing on the PC", "No veo nada en el PC"), sizeof(screenBuf)); return true; }
  if (idle < 0) idle = 0;
  snprintf(screenBuf, sizeof(screenBuf), TR("On the PC: %s%s%s, idle for %ds", "En el PC: %s%s%s, quieto hace %ds"),
           app, title[0] ? " (" : "", title[0] ? title : "", idle);
  if (title[0]) strlcat(screenBuf, ")", sizeof(screenBuf));
  return true;
}
char screenBuf[200] = "";

// --- 5. Play video/URL on the PC (PLAY:<text or URL> tool) ---
static void remotePlayUrl(const char* q) {
  if (!q || !q[0]) return;
  JsonDocument d;
  d["query"] = q;
  String j; serializeJson(d, j);
  bridgePost("/media/play", j) ? toast(TR("Playing video", "Poniendo video"), 1200) : toast(TR("Video failed", "Fallo video"), 1200);
}

// --- 6. Bridge binary (JPEG photo for Gemini vision) ---
static size_t bridgeGetBin(const String& path, uint8_t* buf, size_t cap) {
  if (gNet != NET_OK || !buf || !cap) return 0;
  HTTPClient http;
  http.setConnectTimeout(4000);
  http.setTimeout(15000);
  String url = bridgeUrl(path);
  if (!http.begin(url)) return 0;
  const char* tok = bridgeToken();
  if (tok[0]) http.addHeader("X-Bridge-Token", tok);
  size_t n = 0;
  if (http.GET() == 200) {
    WiFiClient* s = http.getStreamPtr();
    int total = http.getSize();
    size_t want = total > 0 ? min((size_t)total, cap) : cap;
    uint32_t t0 = millis();
    while (n < want && millis() - t0 < 12000) {
      if (s->available()) { int c = s->read(); if (c < 0) break; buf[n++] = (uint8_t)c; t0 = millis(); }
      else delay(2);
    }
  }
  http.end();
  return n;
}

// --- 7. Open app by name (OPEN:<app> tool) ---
static void remoteOpen(const char* app) {
  if (!app || !app[0]) return;
  JsonDocument d;
  d["app"] = app;
  String j; serializeJson(d, j);
  bridgePost("/os/open", j) ? toast(TR("Opening...", "Abriendo..."), 1200) : toast(TR("Open failed", "Fallo abrir"), 1200);
}

// --- 8. Volume 0-100 (VOL:<n> tool) ---
static void remoteVolume(int level) {
  if (level < 0) level = 0; else if (level > 100) level = 100;
  JsonDocument d;
  d["level"] = level;
  String j; serializeJson(d, j);
  bridgePost("/media/volume", j) ? toast(TR("Volume set", "Volumen listo"), 1000) : toast(TR("Volume failed", "Fallo volumen"), 1200);
}

// --- 9. Reminders (REMIND:<sec>:<text> and REMINDLIST tools) ---
extern char remBuf[300];
static void remoteRemind(int secs, const char* text) {
  if (!text || !text[0] || secs < 10 || secs > 86400) { toast(TR("Invalid reminder", "Aviso no valido"), 1200); return; }
  JsonDocument d;
  d["text"] = text;
  d["seconds"] = secs;
  String j; serializeJson(d, j);
  bridgePost("/remind", j) ? toast(TR("Reminder set", "Aviso puesto"), 1200) : toast(TR("Reminder failed", "Fallo aviso"), 1200);
}
static bool remoteRemindList() {
  String out;
  if (!bridgeGet("/reminders", out)) { toast(TR("Reminders failed", "Fallo avisos"), 1200); return false; }
  JsonDocument d;
  if (deserializeJson(d, out)) { toast(TR("Bad reminders", "Avisos rotos"), 1200); return false; }
  JsonArray a = d.as<JsonArray>();
  if (!a.size()) { strlcpy(remBuf, TR("You have no pending reminders", "No tienes avisos pendientes"), sizeof(remBuf)); return true; }
  String s = String((int)a.size()) + (a.size() == 1 ? TR(" reminder: ", " aviso: ") : TR(" reminders: ", " avisos: "));
  for (JsonObject r : a) {
    String one = String(r["text"] | "") + TR(" (in ", " (en ") + String((int)(r["in"] | 0) / 60) + " min). ";
    if (s.length() + one.length() > sizeof(remBuf) - 1) break;
    s += one;
  }
  strlcpy(remBuf, s.c_str(), sizeof(remBuf));
  return true;
}
char remBuf[300] = "";

// --- 10. Do not disturb (DND tool: toggles; says the state for voice) ---
static bool remoteDndToggle(String& spoken) {
  String out;
  if (!bridgePost("/os/dnd", "{}")) { toast(TR("DND failed", "Fallo DND"), 1200); return false; }
  // bridgePost returns no body: the state is read with GET.
  if (!bridgeGet("/os/dnd", out)) { toast(TR("DND unclear", "DND dudoso"), 1200); return false; }
  JsonDocument d;
  bool on = false;
  if (!deserializeJson(d, out)) on = d["dnd"] | false;
  spoken = on ? TR("Do not disturb on", "No molestar activado") : TR("Do not disturb off", "No molestar desactivado");
  toast(spoken.c_str(), 1200);
  return true;
}

// --- 11. Pairing (PAIR tool: token without editing secrets.h) ---
static bool bridgePair(String& spoken) {
  if (gNet != NET_OK) { toast(TR("No Wi-Fi", "Sin Wi-Fi"), 1000); return false; }
  HTTPClient http;
  http.setConnectTimeout(4000);
  http.setTimeout(8000);
  String url = bridgeUrl("/pair");
  if (!http.begin(url)) return false;
  http.addHeader("Content-Type", "application/json");
  int code = http.POST("{}");
  String out = http.getString();
  http.end();
  if (code == 403) { spoken = TR("The PC is already paired with another", "El PC ya esta emparejado con otro"); toast(TR("Already paired", "Ya emparejado"), 1200); return false; }
  if (code < 200 || code >= 300) { spoken = TR("Couldn't pair, check the PC", "No pude emparejar, revisa el PC"); return false; }
  JsonDocument d;
  if (deserializeJson(d, out)) return false;
  const char* t = d["token"] | "";
  if (!t[0]) return false;
  bridgeSaveToken(String(t));
  spoken = TR("Paired with the PC", "Emparejado con el PC");
  toast(TR("Paired!", "Emparejado!"), 1500);
  return true;
}

// --- 12. Memory (MEMORY:<fact>, MEMLIST tools) and briefing (BRIEF) ---
static void remoteRemember(const char* text) {
  if (!text || !text[0]) return;
  JsonDocument d;
  d["text"] = text;
  String j; serializeJson(d, j);
  bridgePost("/memory", j) ? toast(TR("I'll remember", "Lo recuerdo"), 1200) : toast(TR("Memory failed", "Fallo memoria"), 1200);
}
extern char memBuf[300];
static bool remoteMemList() {
  String out;
  if (!bridgeGet("/memory", out)) { toast(TR("Memory failed", "Fallo memoria"), 1200); return false; }
  JsonDocument d;
  if (deserializeJson(d, out)) { toast(TR("Bad memory", "Memoria rota"), 1200); return false; }
  JsonArray a = d.as<JsonArray>();
  if (!a.size()) { strlcpy(memBuf, TR("I don't remember anything yet", "No recuerdo nada aun"), sizeof(memBuf)); return true; }
  String s = String(TR("I remember ", "Recuerdo ")) + String((int)a.size()) + ": ";
  for (JsonObject m : a) {
    String one = String(m["text"] | "") + ". ";
    if (s.length() + one.length() > sizeof(memBuf) - 1) break;
    s += one;
  }
  strlcpy(memBuf, s.c_str(), sizeof(memBuf));
  return true;
}
char memBuf[300] = "";
static bool remoteBrief() {
  String out;
  if (!bridgeGet("/briefing", out)) { toast(TR("Briefing failed", "Fallo parte"), 1200); return false; }
  JsonDocument d;
  if (deserializeJson(d, out)) { toast(TR("Bad briefing", "Parte roto"), 1200); return false; }
  strlcpy(memBuf, d["brief"] | TR("(empty)", "(vacio)"), sizeof(memBuf));
  return true;
}
