// ===========================================================================
//  secrets.example.h  —  TEMPLATE / PLANTILLA (safe to share / se puede compartir)
//
//  1. Copy this file as secrets.h in the same folder as the .ino
//     Copia este archivo como secrets.h en la misma carpeta del .ino
//  2. Fill in your data in secrets.h / Rellena tus datos en secrets.h
//  3. NEVER share or commit secrets.h (it is in .gitignore)
//     NUNCA compartas ni subas secrets.h (esta en .gitignore)
// ===========================================================================
#pragma once

// --- Wi-Fi (2.4 GHz only; the ESP32-S3 can't see 5 GHz networks) ---
#define WIFI_SSID     "YOUR_WIFI"
#define WIFI_PASS     "YOUR_PASSWORD"
// Optional (only if the AP keeps kicking you out): fixed BSSID and channel of your 2.4 GHz AP.
//#define WIFI_BSSID    "AA:BB:CC:DD:EE:FF"
//#define WIFI_CHANNEL  1

// --- Location for the weather page (Open-Meteo, no key needed) ---
// Use your city's approximate coordinates, not your home address.
#define CITY_NAME     "YOUR_CITY"
#define LATITUDE      "0.0000"
#define LONGITUDE     "0.0000"
// Optional: POSIX time zone (default: peninsular Spain).
//#define TZ_INFO       "CET-1CEST,M3.5.0,M10.5.0/3"

// --- GitHub: public user whose stats would be shown (no token; currently unused) ---
#define GITHUB_USER   "your-user"

// --- Voice (Google AI Studio -> https://aistudio.google.com/apikey) ---
#define GEMINI_API_KEY  "YOUR_GEMINI_KEY"
#define GEMINI_MODEL    "gemini-flash-lite-latest"

// --- Windows bridge (pc-agent/bridge.py) + OpenCode ---
#define PC_HOST           "192.168.1.100"    // LAN IP of the PC running bridge.py
#define BRIDGE_PORT       8750               // bridge.py port
#define BRIDGE_TOKEN      ""                 // bridge.py token (empty = pair by voice: "pair the PC")
#define OPENCODE_SESSION  "default"          // OpenCode session id (default = auto-pick)
