# Tutorial de instalación

[English](../en/installation.md) | **Español** · [← Volver al README](../../README.es.md)

Tiempo estimado: 30–60 min la primera vez. Sigue los pasos en orden: cada uno termina con una
comprobación, así que si algo falla sabrás exactamente dónde.

- [1. Arduino IDE y núcleo ESP32](#1-arduino-ide-y-núcleo-esp32)
- [2. Librerías](#2-librerías)
- [3. Seleccionar la pantalla en TFT_eSPI](#3-seleccionar-la-pantalla-en-tft_espi)
- [4. Crear `secrets.h`](#4-crear-secretsh)
- [5. Ajustes de placa y primera subida](#5-ajustes-de-placa-y-primera-subida)
- [6. Comprobar la pantalla táctil](#6-comprobar-la-pantalla-táctil)
- [7. Conectar los sensores](#7-conectar-los-sensores)
- [8. Calibrar los ejes de la IMU](#8-calibrar-los-ejes-de-la-imu)
- [9. Chat de voz (Gemini)](#9-chat-de-voz-gemini)
- [10. Compañero de PC (`pc-agent`)](#10-compañero-de-pc-pc-agent)
- [11. Actualizaciones inalámbricas (OTA)](#11-actualizaciones-inalámbricas-ota)

---

## 1. Arduino IDE y núcleo ESP32

1. Instala **Arduino IDE 2.x**.
2. *File → Preferences → Additional boards manager URLs*:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
3. *Tools → Board → Boards Manager* → instala **esp32 by Espressif Systems**.
   Probado con la **3.1.1** (cualquier 3.x debería funcionar; la 2.x **no**).

## 2. Librerías

| Librería | Versión probada | Cómo instalarla |
|---|---|---|
| **TFT_eSPI** + **TFT_eSPI_Setups** | 2.5.43 / Freenove | ZIP del [repositorio de Freenove](https://github.com/Freenove/Freenove_ESP32_S3_Display) (*Sketch → Include Library → Add .ZIP Library*) |
| **Freenove WS2812 Lib for ESP32** | 2.0.1 | Gestor de librerías o ZIP de Freenove |
| **ArduinoJson** (Benoit Blanchon) | 7.4.3 | Gestor de librerías (**se requiere la v7**) |
| **ESP32-audioI2S** (schreibfaul1) | 2.0.0 | ZIP de GitHub. Solo para la voz. Las versiones mayores más nuevas cambiaron la API y puede que no compilen |

No hacen falta: librerías de MPU6050, DHT ni FT6336U (el sketch habla con ellos directamente).

✅ **Comprobación:** *Sketch → Include Library* muestra las cuatro librerías.

## 3. Seleccionar la pantalla en TFT_eSPI

Edita `Documents/Arduino/libraries/TFT_eSPI/User_Setup_Select.h` y deja **solo** esta
línea sin comentar:

```cpp
#define FNK0104AB_2P8_240x320_ILI9341
```

## 4. Crear `secrets.h`

El sketch no compila sin él (a propósito):

1. Ve a `firmware/kt1-deskpet/`.
2. Copia `secrets.example.h` → **`secrets.h`**.
3. Rellena como mínimo:

```cpp
#define WIFI_SSID     "YOUR_WIFI"     // 2.4 GHz network
#define WIFI_PASS     "YOUR_PASSWORD"
#define CITY_NAME     "YOUR_CITY"     // shown on the Weather page
#define LATITUDE      "0.0000"        // approximate city coordinates
#define LONGITUDE     "0.0000"
```

El resto (clave de Gemini, IP del PC, token del bridge) puede quedarse de momento como en la plantilla.

> 🔒 `secrets.h` está en `.gitignore`. No lo subas nunca al repositorio, no lo pegues en issues ni lo
> enseñes en capturas. Usa las coordenadas de la ciudad, no la dirección de tu casa.

## 5. Ajustes de placa y primera subida

1. Conecta la placa por USB-C (todavía sin sensores).
2. Abre `firmware/kt1-deskpet/kt1-deskpet.ino`.
3. Menú *Tools* (*Herramientas*):

   | Opción | Valor |
   |---|---|
   | Board | **ESP32S3 Dev Module** |
   | USB CDC On Boot | **Enabled** |
   | PSRAM | **OPI PSRAM** (obligatorio: ahí viven el búfer de pantalla y la voz) |
   | Flash Size | **16MB (128Mb)** |
   | Partition Scheme | **16M Flash (3MB APP/9.9MB FATFS)** (permite OTA) — o *Huge APP (3MB No OTA/1MB SPIFFS)* si nunca vas a usar OTA |
   | Upload Speed | 921600 (usa 115200 si falla) |

4. Sube el sketch. Si no encuentra el puerto: mantén pulsado **BOOT**, pulsa **RESET**, suelta BOOT,
   vuelve a subir y pulsa RESET al terminar.
5. Abre el *Serial Monitor* (*Monitor serie*) a **115200** baudios. Deberías ver algo así:

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

✅ **Comprobación:** la pantalla muestra los ojos, el reloj aparece en la barra superior y la
página **Tiempo** (*Weather*) tiene datos. La página de estado del dispositivo está en `http://kt1.local/`.

> La interfaz arranca en **inglés**. Para ponerla en español: desliza en horizontal hasta la
> página **Pantalla** (*Screen*) → fila **Idioma** (*Language*) → *Espanol*. La elección se guarda.

## 6. Comprobar la pantalla táctil

- Desliza el dedo en horizontal: el nombre de la página siguiente aparece en un aviso emergente.
- Cada toque imprime `[TOUCH] tap at (x, y)` en el monitor serie.
- ¿X invertida? Pon `#define TOUCH_FLIP_X 1` en el `.ino`. ¿Y invertida? `TOUCH_FLIP_Y 1`.
- ¿El conector USB queda en el otro lado? `#define SCREEN_ROTATION 3` (el táctil se adapta).

## 7. Conectar los sensores

**Desenchufa antes el USB.** Sigue [hardware.md](hardware.md#3-mapa-de-conexiones) y añade **los módulos
de uno en uno**, revisando el monitor serie después de cada uno:

| Módulo | Línea serie esperada |
|---|---|
| GY-521 | `[IMU] On the board I2C connector (IO16/IO15), addr 0x68` y `[IMU] OK. WHO_AM_I=0x68 rest=(…) g` (algunos clones devuelven 0x70/0x72/0x98: también es válido). Mantenlo quieto mientras se muestra «Quieto… calibrando mirada» (*Hold still… calibrating gaze*) |
| Sensor táctil | `[PET] TTP223 on GPIO 2 ready (DO at 3.3V)` y `[PET] sensor tap` cada vez que lo tocas |
| DHT11 + LDR | `[PLANTS] DHT11 on GPIO 14, LDR on GPIO 3`; en la página **Plantas** (*Plants*) toca **Medir** (*Measure*) → valores tras ~8 s |

## 8. Calibrar los ejes de la IMU

Depende de cómo hayas montado el GY-521.

1. Pon `#define IMU_DEBUG 1`, sube el sketch y abre el monitor serie.
2. Inclina KT1 hacia la **derecha**: el valor de `a=(x y z)` que más cambia es
   `IMU_EYE_X_AXIS` (0 = x, 1 = y, 2 = z). Si los ojos van a la izquierda, `IMU_EYE_X_SIGN -1`.
3. Inclínalo **hacia delante/atrás** → `IMU_EYE_Y_AXIS` / `IMU_EYE_Y_SIGN`.
4. `IMU_GAIN_X/Y` = cuánto se mueven los ojos; `IMU_SHAKE_G` = sensibilidad al agitado.
5. Vuelve a `IMU_DEBUG 0`.

## 9. Chat de voz (Gemini)

1. Crea una clave de API en <https://aistudio.google.com/apikey>.
2. En `secrets.h`: `GEMINI_API_KEY` (y, opcionalmente, `GEMINI_MODEL`).
3. Vuelve a subir el sketch, ve a la página **Chat** y toca **HABLAR** (*TALK*): graba 4 s, piensa y
   responde por texto y por voz, en el idioma seleccionado en el dispositivo.

> Privacidad: cada turno envía el audio grabado —y, si `pc-agent` está en marcha, una
> captura de 320×240 de tu PC— a Google Gemini; la respuesta se lee en voz alta con Google
> Translate TTS. No lo uses si eso no te parece aceptable.

Sin `pc-agent` el chat sigue respondiendo, pero los comandos al PC (volumen, abrir apps…) fallan.

## 10. Compañero de PC (`pc-agent`)

En el PC con Windows (en la misma red Wi-Fi que KT1):

```powershell
cd pc-agent
python -m venv .venv
.\.venv\Scripts\Activate.ps1          # if blocked: Set-ExecutionPolicy -Scope CurrentUser RemoteSigned
pip install -r requirements.txt
python setup.py                       # guided setup: config, token, secrets.h, firewall, autostart
```

`setup.py` escribe `PC_HOST`, `BRIDGE_PORT` y `BRIDGE_TOKEN` en el `secrets.h` del
firmware. **Vuelve a subir el firmware** después para que KT1 conozca el PC.

Arranque manual (en lugar del inicio automático que crea `setup.py`):

```powershell
python bridge.py --config setup.json            # voice commands, screenshot, OTA
python pet_agent.py --host kt1.local --verbose   # presence, calls, CPU…
```

✅ **Comprobación:** `pet_agent.py --verbose` imprime `HTTP 200` y la fase actual; en la
página Chat, di *«¿qué hay en mi pantalla?»*.

Referencia completa: [pc-agent.md](pc-agent.md).

## 11. Actualizaciones inalámbricas (OTA)

Requiere el esquema de particiones **16M Flash (3MB APP/9.9MB FATFS)** (el esquema *Huge APP*
no tiene hueco para OTA).

1. En el IDE: *Sketch → Export Compiled Binary*.
2. Copia el `.bin` a `pc-agent/firmware/` con el nombre `kt1_vX.Y.Z.bin`, donde `X.Y.Z`
   coincide con `FW_VERSION` del `.ino` (súbela en cada versión).
3. Vuelve a ejecutar `python setup.py` (registra el `.bin` más reciente) y reinicia el bridge.
4. KT1 comprueba si hay actualización 30 s después de arrancar y luego cada 24 h, solo cuando está inactivo.

> El `.bin` del firmware contiene tu contraseña de la Wi-Fi y tus claves: git lo ignora,
> no lo publiques nunca.
