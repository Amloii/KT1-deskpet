# Solución de problemas

[English](../en/troubleshooting.md) | **Español** · [← Volver al README](../../README.es.md)

Empieza siempre abriendo el **Monitor serie a 115200**: casi todos los problemas imprimen una
línea `[TAG] …` que los explica.

| Síntoma | Causa / solución |
|---|---|
| `Missing secrets.h` / `Define CITY_NAME…` al compilar | Copia `secrets.example.h` a `secrets.h` y rellénalo ([instalación §4](installation.md#4-crear-secretsh)) |
| `Sketch too big` | Partition Scheme con una app de 3 MB (*16M Flash (3MB APP/9.9MB FATFS)* o *Huge APP*) |
| Aviso `TOUCH_CS pin not defined` | Inofensivo: el táctil se lee a través del FT6336U, no de TFT_eSPI |
| Pantalla en blanco/negro | Hay otra pantalla seleccionada en `User_Setup_Select.h`; deja solo `FNK0104AB_2P8_240x320_ILI9341` |
| Errores en `Audio.h` / `connecttospeech` | Versión incompatible de ESP32-audioI2S: usa la 3.0.12 probada (>=3.1 cambió el constructor) |
| `[TFT] PSRAM NOT detected` / `[CHAT] no PSRAM -> voice OFF` | *Tools → PSRAM → OPI PSRAM* |
| La Wi-Fi no conecta | Red de 5 GHz (solo funciona la de 2,4 GHz), SSID/contraseña incorrectos, señal débil (mira el RSSI). Si las redes de 2,4 y 5 GHz tienen el mismo nombre y el punto de acceso te expulsa, define `WIFI_BSSID` / `WIFI_CHANNEL` en `secrets.h` |
| El reloj muestra «Sincronizando» (*Syncing*) | Sin acceso a internet o NTP bloqueado en tu red |
| Los toques caen en el sitio equivocado | `TOUCH_FLIP_X` / `TOUCH_FLIP_Y`, o `SCREEN_ROTATION 3` |
| No se encuentra la `[IMU]` | SDA/SCL intercambiados, VCC no conectado a 3V3, Dupont flojo, AD0 en alto (→ `IMU_ADDR 0x69`) |
| Los ojos tiemblan / se marean sin motivo | La IMU no está montada de forma rígida; baja `IMU_GAIN_X/Y`; sube `IMU_SHAKE_G` |
| No hay sonido | Revisa la página **Sonido** (*Sound*) (activado, volumen ≥ 50). Serie: `[SOUND] ES8311 not responding…` → problema de I2C; se silencia por la noche y durante las llamadas |
| Bucle de reinicios con errores de `ES8311` | Mantén `AMP_ON_LEVEL LOW` (el códec no responde si el GPIO 1 está en ALTO antes de inicializarlo) |
| Plantas: «sin lectura» | El DATA del DHT11 no está en el GPIO 14, o está alimentado a 5 V en lugar de 3V3; la primera lectura tarda ~1 s |
| El valor de luz sube con más luz | `+` y `−` del KY-018 intercambiados ([hardware](hardware.md)) |
| Chat: «Gemini HTTP 400/403» | `GEMINI_API_KEY` incorrecta o ausente, o el modelo de `GEMINI_MODEL` no está disponible para tu clave |
| El chat responde pero los comandos al PC fallan | `bridge.py` no está en marcha, `PC_HOST` incorrecto, el firewall de Windows bloquea el puerto o el token no coincide (`401`) → di «empareja el PC» o vuelve a ejecutar `setup.py` y sube de nuevo el firmware |
| `kt1.local` no se resuelve | Usa la IP que se imprime al arrancar (`--host 192.168.x.x`); el PC y KT1 deben estar en la misma red |
| «En llamada» todo el rato | Una app mantiene el micrófono abierto: ejecuta `pet_agent.py --verbose`, mira `call_app` y añádela a `MIC_IGNORE` en `pc-agent/mapping.py` |
| `OTA failed` | Partición *Huge APP* (sin hueco para OTA): usa *16M Flash (3MB APP/9.9MB FATFS)* y flashea una vez por USB |
| Reinicios aleatorios (`Brownout detector`) | Cable/cargador defectuoso; usa un cable de datos corto y un cargador de ≥ 1 A |

Si abres un issue, pega el log serie **después de quitar** tu SSID, las direcciones IP,
el BSSID y cualquier token.
