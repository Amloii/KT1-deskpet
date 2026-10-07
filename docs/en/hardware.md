# Hardware: components and wiring

**English** | [Español](../es/hardware.md) · [← Back to README](../../README.md)

## 1. Bill of materials

| # | Component | Qty | Required? | What it is used for |
|---|---|:-:|:-:|---|
| 1 | **Freenove ESP32-S3 Display FNK0104B** (2.8" 240×320 ILI9341 TFT, FT6336U capacitive touch, ES8311 audio codec with MEMS mic + speaker amplifier, WS2812 RGB LED, 16 MB flash, 8 MB OPI PSRAM) | 1 | ✅ | Brain, screen, touch, sound, voice, Wi-Fi |
| 2 | USB-C **data** cable | 1 | ✅ | Power and programming |
| 3 | 5 V USB charger, **≥ 1 A** (or a PC USB port) | 1 | ✅ | Power |
| 4 | **GY-521** (MPU6050 accelerometer/gyroscope, I2C) | 1 | Optional | Eyes follow the tilt; dizzy eyes when shaken |
| 5 | **Touch sensor module** TTP223 (or KY-036 "metal touch" with digital output) | 1 | Optional | Petting, contextual actions, push-to-talk in Chat |
| 6 | **DHT11** module (temperature + humidity) | 1 | Optional | *Plants* page |
| 7 | **KY-018** module (LDR photoresistor + 10 kΩ) | 1 | Optional | *Plants* page (light) |
| 8 | Dupont jumper wires (female-female / female-male) | ~14 | With sensors | Connections |
| 9 | Dumbbells | 1–2 | Optional | Active breaks of the *Exercise* cycle (mobility exercises need none) |
| 10 | Case (3D printed, PETG recommended) | 1 | Optional | See §6 |
| 11 | **2-color LED** module (Elegoo kit, GRY pins) | 1 | Optional | Single-color ambient light on IO2: steady on activity, strobe on alerts (see §2) |

Software/services (all optional except the first):

| What | Required? | For |
|---|:-:|---|
| Arduino IDE 2.x | ✅ | Building and uploading the firmware |
| Windows 10/11 PC with Python ≥ 3.9 | Optional | `pc-agent` (presence/calls, PC control by voice, OTA) |
| Google AI Studio API key (Gemini) | Optional | *Chat* page (voice assistant) |
| OpenCode desktop/CLI | Optional | Sending prompts to a coding session by voice |

> Every optional module is detected at boot. If it is missing, KT1 still works: the
> eyes move at random without the IMU, the *Plants* page shows "no reading" without
> DHT11/LDR, and so on.

### Where to buy (no links, to avoid dead stores)

- **FNK0104B:** search "Freenove ESP32-S3 Display FNK0104B" on the Freenove store or your usual electronics shop. Verify it is the 2.8" ILI9341 + FT6336U + ES8311 version (board support: [Freenove_ESP32_S3_Display](https://github.com/Freenove/Freenove_ESP32_S3_Display)).
- **GY-521 / TTP223 / DHT11 / KY-018:** generic modules, any vendor works. Buy the 3.3 V-compatible versions and short Dupont wires (female-female / female-male, ~14 pcs).
- Typical prices (Oct 2026, varies by region): board ~25–40 €, sensors ~1–3 € each.

## 2. Pins used by the board (internal, already wired)

| Function | GPIO | Notes |
|---|---|---|
| TFT ILI9341 (SPI) | MOSI 11 · SCLK 12 · MISO 13 · CS 10 · DC 46 · BL 45 | Configured by the Freenove TFT_eSPI setup, not in the sketch |
| Internal I2C bus | **SDA 16 · SCL 15** (400 kHz) | Shared by touch (0x38), codec (0x18) and the GY-521 (0x68) |
| Touch FT6336U | RST 18 · INT 17 | Read directly over I2C (`touch.h`) |
| ES8311 codec (I2S) | MCLK 4 · BCLK 5 · WS 7 · DOUT 8 (speaker) · DIN 6 (mic) | |
| Speaker amplifier enable | 1 | Active LOW on this board (the firmware tries the other polarity if the codec does not answer) |
| WS2812 RGB LED | 42 | |
| microSD | 38, 39, 40, 41, 47, 48 | Not used |

**Free pins used by KT1:** GPIO **2** (touch sensor), GPIO **14** (DHT11), GPIO **3** (LDR, ADC1).
Avoid GPIO 21 (pulled to GND on this board) and the pins in the table above.

> **External 2-color LED module (optional, Elegoo kit, GRY):** with only IO2 free, one
> color is used. G is ground: wire G → **GND**, the chosen color (Y yellow or R red) →
> **IO2**, leave the other floating. Set `PET_ENABLED 0` and `TWO_LED_ENABLED 1`.
> Steady = activity (phase, pomo), 4 Hz strobe = alerts, off = idle. Plants stay as they are.

## 3. Wiring map

```
                      FREENOVE ESP32-S3 DISPLAY (FNK0104B)
   ┌──────────────────────────────────────────────────────────────────┐
   │  I2C CONNECTOR (4 pins)            SIDE PIN HEADER               │
   │   3V3  SDA(16)  SCL(15)  GND        3V3   GND   IO2   IO14   IO3  │
   └────┬──────┬───────┬───────┬──────────┬─────┬─────┬─────┬──────┬───┘
        │      │       │       │          │     │     │     │      │
        │      │       │       │          │     │     │     │      │
   ┌────┴──────┴───────┴───────┴──┐       │     │     │     │      │
   │ GY-521 (MPU6050)  addr 0x68  │       │     │     │     │      │
   │ VCC   SDA    SCL    GND      │       │     │     │     │      │
   │ AD0 → open/GND · INT, XDA,   │       │     │     │     │      │
   │ XCL → not connected          │       │     │     │     │      │
   └──────────────────────────────┘       │     │     │     │      │
                                          │     │     │     │      │
             3V3 rail ════════════════════╪═════╪═════╪═════╪══════╪═══ to every "+"/VCC
             GND rail ════════════════════╪═════╪═════╪═════╪══════╪═══ to every "−"/GND
                                                │     │     │      │
                         TTP223 / KY-036  DO ───┘─────┘     │      │
                         DHT11            DATA ─────────────┘      │
                         KY-018 (LDR)     S (AO) ──────────────────┘
```

> The side header layout depends on the board revision: follow the silkscreen labels
> (`3V3`, `GND`, `IO2`, `IO14`, `IO3`), not the position in this drawing.

### Wire tables

**GY-521 → board I2C connector**

| GY-521 | Board | Suggested colour | Note |
|---|---|---|---|
| VCC | **3V3** | red | Never 5 V |
| GND | **GND** | black | Common ground |
| SDA | **IO16 (SDA)** | blue | Shared bus |
| SCL | **IO15 (SCL)** | yellow | Shared bus |
| AD0 | — | — | Open or GND → address 0x68 (to 3V3 → 0x69) |
| INT, XDA, XCL | — | — | Not connected |

**Touch sensor (TTP223 / KY-036)**

| Module | Board | Note |
|---|---|---|
| + / VCC | **3V3** | **Never 5 V**: its output would be 5 V and damage the GPIO |
| G / GND | **GND** | |
| DO / S | **IO2** | HIGH while touched (`PET_PIN 2`) |
| AO | — | Not connected (KY-036 only) |

**DHT11**

| Module | Board | Note |
|---|---|---|
| + / VCC | **3V3** | |
| − / GND | **GND** | |
| S / DATA | **IO14** | Read by bit-banging (no library); internal pull-up enabled |

**KY-018 (LDR)**

| Module | Board | Note |
|---|---|---|
| middle pin (+) | **3V3** | The module's 10 kΩ goes from + to S |
| − | **GND** | |
| S (AO) | **IO3** (ADC1) | Resulting divider: 3V3 → 10 kΩ → AO → LDR → GND. **AO drops with more light** |

> If your KY-018 reads the opposite way (value goes **up** with light), swap `+` and `−`.
> The lux value is approximate: it is meant to compare spots, it is not a light meter.

### I2C addresses

| Device | Address |
|---|---|
| FT6336U touch | 0x38 |
| ES8311 codec | 0x18 |
| MPU6050 (GY-521) | 0x68 (0x69 with AD0 high) |

All three share SDA 16 / SCL 15 without conflict. At boot the firmware looks for the
IMU on that bus first; if it is not there it also tries GPIO 9/21 on a second bus.

## 4. Electrical safety notes

- Everything runs at **3.3 V**. Do not power the modules from 5 V.
- Disconnect USB before changing wires.
- Use short Dupont wires plugged directly into the pins: 400 kHz I2C does not like
  loose breadboard contacts (the IMU may answer once and then drop out).
- Typical consumption: 150–250 mA (peaks ~400 mA when Wi-Fi transmits). Thin or long
  cables cause brownout resets: change cable/charger before suspecting the code.

## 5. Battery (optional, advanced)

The board has a battery connector. Only use a **1-cell LiPo/Li-ion (3.7 V nominal) with a
protection circuit**, check the connector polarity with a multimeter against the
silkscreen (JST cables from different vendors are wired differently) and check the
board's charge current in the Freenove documentation. The firmware does **not** measure
battery level.

## 6. Case tips

- Ventilation slots near the ESP32; PLA warps at ~55 °C, PETG is better.
- Do not press the touch glass (constant pressure = ghost touches); no metal on top.
- Mount the GY-521 **flat and rigid**, ≥ 1 cm away from the ESP32 (heat) — if it wobbles,
  the eyes tremble.
- Keep the ESP32-S3 antenna area free; no metal case or metallic paint.
- Leave holes for USB (with strain relief) and the BOOT/RESET buttons.
