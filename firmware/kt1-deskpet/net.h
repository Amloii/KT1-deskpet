// ===========================================================================
//  net.h — Wi-Fi, NTP, APIs (Open-Meteo, GitHub) and HTTP server
//          for pc-agent (POST /pet). Everything runs on core 0.
// ===========================================================================
#pragma once

// ---------------------------------------------------------------------------
//  HTTPS + JSON requests
// ---------------------------------------------------------------------------
bool httpGetJson(const String& url, JsonDocument& doc, JsonDocument* filter,
                 char* err, size_t errLen, bool github = false) {
  WiFiClientSecure client;
  client.setInsecure();              // public read-only data (see README, security)
  HTTPClient http;
  http.useHTTP10(true);              // no "chunked" -> the stream can be parsed directly
  http.setConnectTimeout(10000);
  http.setTimeout(15000);
  if (!http.begin(client, url)) { snprintf(err, errLen, "%s", TR("Invalid URL", "URL invalida")); return false; }
  http.addHeader("User-Agent", "ESP32-KT1");
  if (github) http.addHeader("Accept", "application/vnd.github+json");
  int code = http.GET();
  if (code < 0) {                    // diagnostics: DNS, internal memory and mbedTLS error
    char host[64] = "";
    const char* p = strstr(url.c_str(), "://");
    p = p ? p + 3 : url.c_str();
    size_t n = strcspn(p, "/:");
    if (n >= sizeof(host)) n = sizeof(host) - 1;
    memcpy(host, p, n); host[n] = 0;
    IPAddress ip;
    bool dnsOk = WiFi.hostByName(host, ip) == 1;
    char tls[80] = "";
    int tlsCode = client.lastError(tls, sizeof(tls));
    Serial.printf("[NET] %s -> %s | DNS %s (%s) | heap int %u, max block %u | TLS %d: %s\n",
                  host, http.errorToString(code).c_str(), dnsOk ? "OK" : "FAIL",
                  dnsOk ? ip.toString().c_str() : "-",
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                  (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                  tlsCode, tls[0] ? tls : "-");
  }
  if (code != 200) {
    if (code < 0) snprintf(err, errLen, "%s", http.errorToString(code).c_str());
    else if (code == 401) snprintf(err, errLen, "%s", TR("HTTP 401 (key?)", "HTTP 401 (clave?)"));
    else if (code == 403 || code == 429) snprintf(err, errLen, TR("HTTP %d (API limit)", "HTTP %d (limite API)"), code);
    else snprintf(err, errLen, "HTTP %d", code);
    http.end();
    return false;
  }
  DeserializationError e = filter
      ? deserializeJson(doc, http.getStream(), DeserializationOption::Filter(*filter))
      : deserializeJson(doc, http.getStream());
  http.end();
  if (e) { snprintf(err, errLen, "JSON: %s", e.c_str()); return false; }
  return true;
}

bool fetchWeather() {
  String url = String("https://api.open-meteo.com/v1/forecast?latitude=") + LATITUDE +
               "&longitude=" + LONGITUDE +
               "&current=temperature_2m,relative_humidity_2m,apparent_temperature,is_day,weather_code,wind_speed_10m"
               "&daily=temperature_2m_max,temperature_2m_min,precipitation_probability_max,sunrise,sunset"
               "&timezone=auto&forecast_days=1";
  WeatherData w;
  JsonDocument doc;
  bool ok = httpGetJson(url, doc, nullptr, w.err, sizeof(w.err));
  if (ok) {
    JsonObject cur = doc["current"];
    JsonObject day = doc["daily"];
    w.temp  = cur["temperature_2m"]       | NAN;
    w.feels = cur["apparent_temperature"] | NAN;
    w.hum   = cur["relative_humidity_2m"] | -1;
    w.code  = cur["weather_code"]         | -1;
    w.isDay = cur["is_day"]               | 1;
    w.wind  = cur["wind_speed_10m"]       | NAN;
    w.tmax  = day["temperature_2m_max"][0] | NAN;
    w.tmin  = day["temperature_2m_min"][0] | NAN;
    w.pop   = day["precipitation_probability_max"][0] | -1;
    const char* sr = day["sunrise"][0] | "";
    const char* ss = day["sunset"][0]  | "";
    const char* p;
    if ((p = strchr(sr, 'T'))) strlcpy(w.sunrise, p + 1, sizeof(w.sunrise));
    if ((p = strchr(ss, 'T'))) strlcpy(w.sunset,  p + 1, sizeof(w.sunset));
    w.ok = !isnan(w.temp);
    if (!w.ok) strlcpy(w.err, TR("Empty response", "Respuesta vacia"), sizeof(w.err));
    w.updated = time(nullptr);
  }
  xSemaphoreTake(dataMtx, portMAX_DELAY);
  if (w.ok) gWeather = w;
  else strlcpy(gWeather.err, w.err, sizeof(gWeather.err));
  xSemaphoreGive(dataMtx);
  Serial.printf("[WEATHER] %s %s\n", w.ok ? "OK" : "ERROR", w.err);
  return w.ok;
}

bool fetchGithub() {
  GithubData g;
  bool ok = false;
  if (strlen(GITHUB_USER) == 0 || strcmp(GITHUB_USER, "tu-usuario") == 0) {
    strlcpy(g.err, TR("Set GITHUB_USER", "Pon GITHUB_USER"), sizeof(g.err));
  } else {
    JsonDocument filter;
    filter["login"] = true; filter["name"] = true; filter["public_repos"] = true;
    filter["followers"] = true; filter["following"] = true;
    JsonDocument doc;
    ok = httpGetJson(String("https://api.github.com/users/") + GITHUB_USER, doc, &filter, g.err, sizeof(g.err), true);
    if (ok) {
      strlcpy(g.login, doc["login"] | GITHUB_USER, sizeof(g.login));
      strlcpy(g.name,  doc["name"]  | "",          sizeof(g.name));
      g.repos     = doc["public_repos"] | 0;
      g.followers = doc["followers"]    | 0;
      g.following = doc["following"]    | 0;
      int pages = min(3, (g.repos + 99) / 100);
      int stars = 0; bool starsOk = true;
      JsonDocument rf; rf[0]["stargazers_count"] = true;
      for (int p = 1; p <= pages && starsOk; p++) {
        JsonDocument rd; char e2[40];
        String u = String("https://api.github.com/users/") + GITHUB_USER +
                   "/repos?per_page=100&type=owner&page=" + p;
        starsOk = httpGetJson(u, rd, &rf, e2, sizeof(e2), true);
        if (starsOk) for (JsonObject r : rd.as<JsonArray>()) stars += r["stargazers_count"] | 0;
      }
      g.stars = starsOk ? stars : -1;
      g.ok = true;
      g.updated = time(nullptr);
    }
  }
  xSemaphoreTake(dataMtx, portMAX_DELAY);
  if (ok) gGithub = g;
  else strlcpy(gGithub.err, g.err, sizeof(gGithub.err));
  xSemaphoreGive(dataMtx);
  Serial.printf("[GITHUB] %s %s\n", ok ? "OK" : "ERROR", g.err);
  return ok;
}

// ---------------------------------------------------------------------------
//  v3: pc-agent restored (V1) + build/CI mirror via POST /pet {build,summary}
// ---------------------------------------------------------------------------
AgentData agentSnap() {
  xSemaphoreTake(dataMtx, portMAX_DELAY);
  AgentData a = gAgent;
  xSemaphoreGive(dataMtx);
  return a;
}
bool agentConnected() {
  AgentData a = agentSnap();
  return a.lastMs != 0 && millis() - a.lastMs < AGENT_TIMEOUT_MS;
}
// "Away": no keyboard/mouse for 5 min, no call and not watching a video
bool agentAway() {
  if (!agentConnected()) return false;
  AgentData a = agentSnap();
  return a.idle >= (int)AWAY_IDLE_S && !a.call && strcmp(a.fgCategory, "media") != 0;
}
bool callActive() {
  if (!agentConnected()) return false;
  return agentSnap().call;
}
// Writing code right now (last key press < 8 s ago)
bool focusTyping() {
  if (!agentConnected()) return false;
  AgentData a = agentSnap();
  return a.idle < 8 && strcmp(a.fgCategory, "coding") == 0;
}
int agentIdleSec() {
  if (!agentConnected()) return 9999;
  return agentSnap().idle;
}

// Build/CI mirror (pushed by bridge.py with POST /pet {"build","summary"})
// build: "" = no data, "busy" = working, "ok" = green, "fail" = red.
char gBuild[8] = "";
char gBuildSummary[120] = "";
uint32_t gBuildMs = 0;
bool buildFail() { return strcmp(gBuild, "fail") == 0 && millis() - gBuildMs < 15UL * 60UL * 1000UL; }
bool buildBusy() { return strcmp(gBuild, "busy") == 0 && millis() - gBuildMs < 15UL * 60UL * 1000UL; }

const char* phaseName(Phase p);   // focus.h (defined later)
uint32_t    phaseRemainingS();    // focus.h (defined later)

void handlePet() {
  if (!server.hasArg("plain")) { server.send(400, "application/json", "{\"ok\":false}"); return; }
  JsonDocument d;
  if (deserializeJson(d, server.arg("plain"))) { server.send(400, "application/json", "{\"ok\":false}"); return; }
  xSemaphoreTake(dataMtx, portMAX_DELAY);
  gAgent.lastMs = millis();
  strlcpy(gAgent.category,   d["category"]     | "",   sizeof(gAgent.category));
  strlcpy(gAgent.fgCategory, d["fg_category"]  | (const char*)gAgent.category, sizeof(gAgent.fgCategory));
  strlcpy(gAgent.emotion,    d["emotion"]      | "",   sizeof(gAgent.emotion));
  strlcpy(gAgent.app,        d["app"]          | "",   sizeof(gAgent.app));
  gAgent.idle = d["idle"] | 0;
  gAgent.call = d["call"] | false;
  if (d["talk"] | false) gAgent.greet = true;
  if (d["calm"] | false) reqCalmPage = true;   // SOS: open the Calm page
  const char* b = d["build"] | "";
  if (b[0]) {
    strlcpy(gBuild, b, sizeof(gBuild));
    strlcpy(gBuildSummary, d["summary"] | "", sizeof(gBuildSummary));
    gBuildMs = millis();
    gDirty = true;
  }
  xSemaphoreGive(dataMtx);

  JsonDocument r;
  r["ok"] = true;
  r["phase"] = phaseName(gPhase);
  r["remaining_s"] = phaseRemainingS();
  r["posture"] = gPosture == POS_STAND ? "stand" : "sit";
  r["coach"] = coach.active;
  r["paused"] = gPaused;
  String out; serializeJson(r, out);
  server.send(200, "application/json", out);
}

// ---------------------------------------------------------------------------
//  Local HTTP server (status page only, no pc-agent)
// ---------------------------------------------------------------------------
const char* phaseName(Phase p);   // focus.h
uint32_t    phaseRemainingS();    // focus.h

void httpTask(void*) {
  for (;;) {
    if (serverUp) server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

// ---------------------------------------------------------------------------
//  Status web page (GET /)
// ---------------------------------------------------------------------------
void handleRoot() {
  char b[600], sit[16], stand[16];
  fmtDur(stats.sitS, sit, sizeof(sit));
  fmtDur(stats.standS, stand, sizeof(stand));
  snprintf(b, sizeof(b),
    TR("<!doctype html><meta charset=utf-8><meta name=viewport content='width=device-width'>"
       "<meta http-equiv=refresh content=10><title>KT1 desk pet</title>"
       "<body style='font-family:monospace;background:#000;color:#eee;padding:1em'>"
       "<h2>KT1 desk pet</h2>"
       "<p>Exercise: <b>%s</b>%s &middot; %lu s left &middot; cycle %d</p>"
       "<p>Posture: %s &middot; Coach: %s</p>"
       "<p>Today: sitting %s &middot; standing %s</p>",
       "<!doctype html><meta charset=utf-8><meta name=viewport content='width=device-width'>"
       "<meta http-equiv=refresh content=10><title>KT1 mascota de escritorio</title>"
       "<body style='font-family:monospace;background:#000;color:#eee;padding:1em'>"
       "<h2>KT1 mascota de escritorio</h2>"
       "<p>Ejercicio: <b>%s</b>%s &middot; quedan %lu s &middot; ciclo %d</p>"
       "<p>Postura: %s &middot; Entrenador: %s</p>"
       "<p>Hoy: sentado %s &middot; de pie %s</p>"),
    phaseName(gPhase), gPaused ? TR(" (paused)", " (pausa)") : "", (unsigned long)phaseRemainingS(), cycleNo,
    gPosture == POS_STAND ? TR("standing", "de pie") : TR("sitting", "sentado"), coach.active ? TR("active", "activo") : TR("off", "no"),
    sit, stand);
  server.send(200, "text/html", b);
}

// ---------------------------------------------------------------------------
//  Network task: Wi-Fi, reconnection, NTP, mDNS and weather (v2: no GitHub)
// ---------------------------------------------------------------------------
// Fixed BSSID/channel (optional, for APs with band-steering that kick the ESP32
// off 2.4 GHz). In secrets.h: #define WIFI_BSSID "AA:BB:CC:DD:EE:FF" and
// #define WIFI_CHANNEL 1. Default ("" / 0) = automatic like V2.
#ifndef WIFI_BSSID
#define WIFI_BSSID ""
#endif
#ifndef WIFI_CHANNEL
#define WIFI_CHANNEL 0
#endif

static const char* wifiStatusName(wl_status_t s) {
  switch (s) {
    case WL_NO_SSID_AVAIL: return "NO_SSID (network not seen: 5 GHz, typo or channel)";
    case WL_CONNECT_FAILED: return "CONNECT_FAILED (wrong password?)";
    case WL_CONNECTION_LOST: return "LOST";
    case WL_DISCONNECTED: return "DISCONNECTED";
    case WL_IDLE_STATUS: return "IDLE";
    case WL_SCAN_COMPLETED: return "SCAN_DONE";
    default: return "?";
  }
}

static void wifiBegin() {
  if (WIFI_BSSID[0] && WIFI_CHANNEL > 0) {
    uint8_t b[6];
    if (sscanf(WIFI_BSSID, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
               &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) == 6) {
      Serial.printf("[WIFI] Fixed BSSID %s channel %d\n", WIFI_BSSID, WIFI_CHANNEL);
      WiFi.begin(WIFI_SSID, WIFI_PASS, WIFI_CHANNEL, b);
      return;
    }
    Serial.println("[WIFI] Malformed WIFI_BSSID, using automatic");
  }
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

// One scan to tell whether the network is visible (and its channel/RSSI/auth).
static void wifiScanReport() {
  Serial.println("[WIFI] Scanning networks...");
  int n = WiFi.scanNetworks();
  bool seen = false;
  for (int i = 0; i < n; i++) {
    if (WiFi.SSID(i) == WIFI_SSID) {
      seen = true;
      Serial.printf("[WIFI] Found \"%s\": channel %d RSSI %d dBm %s BSSID %s\n",
                    WIFI_SSID, WiFi.channel(i), WiFi.RSSI(i),
                    WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "OPEN" : "secured",
                    WiFi.BSSIDstr(i).c_str());
    }
  }
  if (!seen) Serial.printf("[WIFI] \"%s\" NOT found (%d networks seen). Note: the S3 only sees 2.4 GHz.\n", WIFI_SSID, n);
  WiFi.scanDelete();
}

void netTask(void*) {
  Serial.printf("[WIFI] heap int %u B, PSRAM %s (%u B free)\n",
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                psramFound() ? "YES" : "NO", (unsigned)ESP.getFreePsram());
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(MDNS_NAME);
  WiFi.setAutoReconnect(true);
  wifiBegin();
  Serial.printf("[WIFI] Connecting to \"%s\"...\n", WIFI_SSID);

  uint32_t lastOk = millis();
  bool ntpStarted = false, wasConnected = false, mdnsStarted = false;
  uint32_t nextW = 0;
  int wifiFails = 0;

  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      if (wasConnected) { Serial.println("[WIFI] Connection lost"); wasConnected = false; }
      gNet = (millis() - lastOk > 20000) ? NET_FAIL : NET_CONNECTING;
      if (millis() - lastOk > 30000) {
        Serial.printf("[WIFI] Retrying... (status %d %s)\n",
                      (int)WiFi.status(), wifiStatusName(WiFi.status()));
        if (!wasConnected) wifiScanReport();   // only before the first connection
        WiFi.disconnect();
        wifiBegin();
        lastOk = millis() - 20000;
        if (++wifiFails >= 20) {   // ~10 min without network: reboot and retry clean
          Serial.println("[WIFI] Down for 10 min: rebooting");
          delay(500);
          ESP.restart();
        }
      }
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }
    if (!wasConnected) {
      Serial.printf("[WIFI] Connected. IP %s  RSSI %d dBm  DNS %s\n", WiFi.localIP().toString().c_str(), WiFi.RSSI(),
                    WiFi.dnsIP().toString().c_str());
      wasConnected = true;
      wifiFails = 0;
      nextW = millis() + 3000;       // let DNS/network stack settle before the first request
    }
    if (!mdnsStarted) {
      if (MDNS.begin(MDNS_NAME)) { MDNS.addService("http", "tcp", 80); Serial.println("[MDNS] http://" MDNS_NAME ".local"); }
      server.on("/", HTTP_GET, handleRoot);
      server.on("/pet", HTTP_POST, handlePet);   // v3: pc-agent + build mirror
      server.begin();
      serverUp = true;
      mdnsStarted = true;
    }
    lastOk = millis();
    gNet = NET_OK;
    if (!ntpStarted) { configTzTime(TZ_INFO, NTP_SERVER_1, NTP_SERVER_2); ntpStarted = true; }

    uint32_t now = millis();
    if (reqWeather || nextW == 0 || (int32_t)(now - nextW) >= 0) {
      reqWeather = false;
      nextW = millis() + (fetchWeather() ? WEATHER_EVERY_MS : RETRY_MS);
    }
    reqGithub = false;
    vTaskDelay(pdMS_TO_TICKS(250));
  }
}
