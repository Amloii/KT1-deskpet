# Installation tutorial

**English** | [Español](../es/installation.md) · [← Back to README](../../README.md)

Estimated time: 30–60 min the first time. Do the steps in order: each one ends with a
check, so if something fails you know exactly where.

- [1. Arduino IDE and ESP32 core](#1-arduino-ide-and-esp32-core)
- [2. Libraries](#2-libraries)
- [3. Select the display in TFT_eSPI](#3-select-the-display-in-tft_espi)
- [4. Create `secrets.h`](#4-create-secretsh)
- [5. Board settings and first upload](#5-board-settings-and-first-upload)
- [6. Check the touch screen](#6-check-the-touch-screen)
- [7. Connect the sensors](#7-connect-the-sensors)
- [8. Calibrate the IMU axes](#8-calibrate-the-imu-axes)
- [9. Voice chat (Gemini)](#9-voice-chat-gemini)
- [10. PC companion (`pc-agent`)](#10-pc-companion-pc-agent)
- [11. Wireless updates (OTA)](#11-wireless-updates-ota)

---

## 1. Arduino IDE and ESP32 core

1. Install **Arduino IDE 2.x**.
2. *File → Preferences → Additional boards manager URLs*:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
3. *Tools → Board → Boards Manager* → install **esp32 by Espressif Systems**.
   Tested with **3.1.1** (any 3.x should work; 2.x does **not**).

## 2. Libraries

| Library | Tested version | How to install |
|---|---|---|
| **TFT_eSPI** + **TFT_eSPI_Setups** | 2.5.43 / Freenove | ZIPs from the [Freenove repository](https://github.com/Freenove/Freenove_ESP32_S3_Display) (*Sketch → Include Library → Add .ZIP Library*) |
| **Freenove WS2812 Lib for ESP32** | 2.0.1 | Library Manager or Freenove ZIP |
| **ArduinoJson** (Benoit Blanchon) | 7.4.3 | Library Manager (**v7 required**) |
| **ESP32-audioI2S** (schreibfaul1) | 3.0.12 | ZIP from GitHub (3.0.x line). Only for voice. >=3.1 changed the constructor and may not compile |

Not needed: any MPU6050, DHT or FT6336U library (the sketch talks to them directly).

✅ **Check:** *Sketch → Include Library* lists the four libraries.

## 3. Select the display in TFT_eSPI

Edit `Documents/Arduino/libraries/TFT_eSPI/User_Setup_Select.h` and leave **only** this
line uncommented:

```cpp
#define FNK0104AB_2P8_240x320_ILI9341
```

## 4. Create `secrets.h`

The sketch does not compile without it (on purpose):

1. Go to `firmware/kt1-deskpet/`.
2. Copy `secrets.example.h` → **`secrets.h`**.
3. Fill in at least:

```cpp
#define WIFI_SSID     "YOUR_WIFI"     // 2.4 GHz network
#define WIFI_PASS     "YOUR_PASSWORD"
#define CITY_NAME     "YOUR_CITY"     // shown on the Weather page
#define LATITUDE      "0.0000"        // approximate city coordinates
#define LONGITUDE     "0.0000"
```

The rest (Gemini key, PC IP, bridge token) can stay as in the template for now.

> 🔒 `secrets.h` is in `.gitignore`. Never commit it, paste it in issues or show it in
> screenshots. Use city coordinates, not your home address.

## 5. Board settings and first upload

1. Connect the board over USB-C (no sensors yet).
2. Open `firmware/kt1-deskpet/kt1-deskpet.ino`.
3. *Tools* menu:

   | Option | Value |
   |---|---|
   | Board | **ESP32S3 Dev Module** |
   | USB CDC On Boot | **Enabled** |
   | PSRAM | **OPI PSRAM** (required: screen buffer and voice live there) |
   | Flash Size | **16MB (128Mb)** |
   | Partition Scheme | **16M Flash (3MB APP/9.9MB FATFS)** (allows OTA) — or *Huge APP (3MB No OTA/1MB SPIFFS)* if you will never use OTA |
   | Upload Speed | 921600 (use 115200 if it fails) |

4. Upload. If the port is not found: hold **BOOT**, press **RESET**, release BOOT,
   upload again and press RESET at the end.
5. Open the *Serial Monitor* at **115200** baud. You should see something like:

   ```
   === kt1-deskpet v3.5.0 (ESP32-S3) ===
   [TFT] Canvas 16 bits in PSRAM. Free internal RAM ... B, free PSRAM ... B
   [TOUCH] FT6336U detected (chip ID 0x..)
   [SOUND] ES8311 detected (ID 0x..)
   [SOUND] ES8311 ready (AP_ENABLE=LOW)
   [IMU] Not responding (...). Eyes with random drift.     <- normal without the GY-521
   [CHAT] ready (160000 B in PSRAM)
   [WIFI] Connecting to "..."...
   [WIFI] Connected. IP 192.168.x.x  RSSI -55 dBm  DNS ...
   [MDNS] http://kt1.local
   [WEATHER] OK
   ```

✅ **Check:** the screen shows the eyes, the clock appears in the top bar and the
*Weather* page has data. The device status page is at `http://kt1.local/`.

> The UI starts in **English**. To switch to Spanish: swipe horizontally to the **Screen**
> page → **Language** row → *Espanol*. The choice is saved.

## 6. Check the touch screen

- Swipe horizontally: the next page name appears as a toast.
- Every tap prints `[TOUCH] tap at (x, y)` in the serial monitor.
- X mirrored? Set `#define TOUCH_FLIP_X 1` in the `.ino`. Y mirrored? `TOUCH_FLIP_Y 1`.
- USB connector on the other side? `#define SCREEN_ROTATION 3` (touch adapts).

## 7. Connect the sensors

**Unplug USB first.** Follow [hardware.md](hardware.md#3-wiring-map) and add **one module
at a time**, checking the serial monitor after each one:

| Module | Expected serial line |
|---|---|
| GY-521 | `[IMU] On the board I2C connector (IO16/IO15), addr 0x68` and `[IMU] OK. WHO_AM_I=0x68 rest=(…) g` (some clones report 0x70/0x72/0x98: also valid). Keep it still while "Hold still… calibrating gaze" is shown |
| Touch sensor | `[PET] TTP223 on GPIO 2 ready (DO at 3.3V)` and `[PET] sensor tap` each time you touch it |
| DHT11 + LDR | `[PLANTS] DHT11 on GPIO 14, LDR on GPIO 3`; on the *Plants* page tap **Measure** → values after ~8 s |

## 8. Calibrate the IMU axes

Depends on how you mounted the GY-521.

1. Set `#define IMU_DEBUG 1`, upload, open the serial monitor.
2. Tilt KT1 to the **right**: the value in `a=(x y z)` that changes most is
   `IMU_EYE_X_AXIS` (0 = x, 1 = y, 2 = z). If the eyes go left, `IMU_EYE_X_SIGN -1`.
3. Tilt it **forward/back** → `IMU_EYE_Y_AXIS` / `IMU_EYE_Y_SIGN`.
4. `IMU_GAIN_X/Y` = how much the eyes move; `IMU_SHAKE_G` = shake sensitivity.
5. Back to `IMU_DEBUG 0`.

## 9. Voice chat (Gemini)

1. Create an API key at <https://aistudio.google.com/apikey>.
2. In `secrets.h`: `GEMINI_API_KEY` (and optionally `GEMINI_MODEL`).
3. Upload again, go to the **Chat** page and tap **TALK**: it records 4 s, thinks and
   answers in text and voice, in the language selected on the device.

> Privacy: each turn sends your recorded audio — and, if `pc-agent` is running, a
> 320×240 screenshot of your PC — to Google Gemini; the answer is spoken with Google
> Translate TTS. Do not use it if that is not acceptable for you.

Without `pc-agent` the chat still answers, but PC commands (volume, open apps…) fail.

## 10. PC companion (`pc-agent`)

On the Windows PC (same Wi-Fi network as KT1):

```powershell
cd pc-agent
python -m venv .venv
.\.venv\Scripts\Activate.ps1          # if blocked: Set-ExecutionPolicy -Scope CurrentUser RemoteSigned
pip install -r requirements.txt
python setup.py                       # guided setup: config, token, secrets.h, firewall, autostart
```

`setup.py` writes `PC_HOST`, `BRIDGE_PORT` and `BRIDGE_TOKEN` into the firmware's
`secrets.h`. **Upload the firmware again** afterwards so KT1 knows the PC.

Manual start (instead of the autostart created by `setup.py`):

```powershell
python bridge.py --config setup.json            # voice commands, screenshot, OTA
python pet_agent.py --host kt1.local --verbose   # presence, calls, CPU…
```

✅ **Check:** `pet_agent.py --verbose` prints `HTTP 200` and the current phase; in the
Chat page, say *"what's on my screen?"*.

Full reference: [pc-agent.md](pc-agent.md).

## 11. Wireless updates (OTA)

Requires the **16M Flash (3MB APP/9.9MB FATFS)** partition scheme (the *Huge APP*
scheme has no OTA slot).

1. In the IDE: *Sketch → Export Compiled Binary*.
2. Copy the `.bin` to `pc-agent/firmware/` named `kt1_vX.Y.Z.bin`, where `X.Y.Z`
   matches `FW_VERSION` in the `.ino` (bump it on every release).
3. Run `python setup.py` again (it registers the newest `.bin`) and restart the bridge.
4. KT1 checks 30 s after boot and then every 24 h, only when idle.

> The firmware `.bin` contains your Wi-Fi password and keys: it is ignored by git,
> never publish it.
