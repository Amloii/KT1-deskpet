# Troubleshooting

**English** | [Español](../es/troubleshooting.md) · [← Back to README](../../README.md)

Always start by opening the **Serial Monitor at 115200**: almost every problem prints a
`[TAG] …` line explaining it.

| Symptom | Cause / fix |
|---|---|
| `Missing secrets.h` / `Define CITY_NAME…` when compiling | Copy `secrets.example.h` to `secrets.h` and fill it in ([installation §4](installation.md#4-create-secretsh)) |
| `Sketch too big` | Partition Scheme with a 3 MB app (*16M Flash (3MB APP/9.9MB FATFS)* or *Huge APP*) |
| `TOUCH_CS pin not defined` warning | Harmless: touch is read through the FT6336U, not TFT_eSPI |
| White/black screen | Another display selected in `User_Setup_Select.h`; leave only `FNK0104AB_2P8_240x320_ILI9341` |
| Errors in `Audio.h` / `connecttospeech` | Incompatible ESP32-audioI2S version: use the tested 2.0.0 |
| `[TFT] PSRAM NOT detected` / `[CHAT] no PSRAM -> voice OFF` | *Tools → PSRAM → OPI PSRAM* |
| Wi-Fi does not connect | 5 GHz network (only 2.4 GHz works), wrong SSID/password, weak signal (check RSSI). If 2.4 and 5 GHz share the name and the AP kicks you out, set `WIFI_BSSID` / `WIFI_CHANNEL` in `secrets.h` |
| Clock shows "Syncing" | No internet access or NTP blocked on your network |
| Taps land in the wrong place | `TOUCH_FLIP_X` / `TOUCH_FLIP_Y`, or `SCREEN_ROTATION 3` |
| `[IMU]` not found | SDA/SCL swapped, VCC not on 3V3, loose Dupont, AD0 high (→ `IMU_ADDR 0x69`) |
| Eyes tremble / dizzy for no reason | IMU not rigidly mounted; lower `IMU_GAIN_X/Y`; raise `IMU_SHAKE_G` |
| No sound | Check the *Sound* page (on, volume ≥ 50). Serial: `[SOUND] ES8311 not responding…` → I2C problem; it is muted at night and during calls |
| Boot loop with `ES8311` errors | Keep `AMP_ON_LEVEL LOW` (the codec does not answer if GPIO 1 is HIGH before init) |
| Plants: "no reading" | DHT11 DATA not on GPIO 14, or powered from 5 V instead of 3V3; the first reading takes ~1 s |
| Light value goes up with more light | KY-018 `+` and `−` swapped ([hardware](hardware.md)) |
| Chat: "Gemini HTTP 400/403" | Wrong or missing `GEMINI_API_KEY`, or the model in `GEMINI_MODEL` is not available to your key |
| Chat answers but PC commands fail | `bridge.py` not running, wrong `PC_HOST`, Windows firewall blocking the port, or token mismatch (`401`) → say "pair the PC" or rerun `setup.py` and upload again |
| `kt1.local` does not resolve | Use the IP printed at boot (`--host 192.168.x.x`); PC and KT1 must be on the same network |
| "On a call" all the time | An app keeps the microphone open: run `pet_agent.py --verbose`, look at `call_app` and add it to `MIC_IGNORE` in `pc-agent/mapping.py` |
| `OTA failed` | *Huge APP* partition (no OTA slot): use *16M Flash (3MB APP/9.9MB FATFS)* and flash once over USB |
| Random resets (`Brownout detector`) | Bad cable/charger; use a short data cable and a ≥ 1 A charger |

If you open an issue, paste the serial log **after removing** your SSID, IP addresses,
BSSID and any token.
