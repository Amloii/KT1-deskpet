# Hardware: componentes y conexiones

[English](../en/hardware.md) | **Español** · [← Volver al README](../../README.es.md)

## 1. Lista de materiales

| # | Componente | Cant. | ¿Obligatorio? | Para qué se usa |
|---|---|:-:|:-:|---|
| 1 | **Freenove ESP32-S3 Display FNK0104B** (TFT ILI9341 de 2,8" y 240×320, táctil capacitivo FT6336U, códec de audio ES8311 con micrófono MEMS + amplificador de altavoz, LED RGB WS2812, 16 MB de flash, 8 MB de PSRAM OPI) | 1 | ✅ | Cerebro, pantalla, táctil, sonido, voz, Wi-Fi |
| 2 | Cable USB-C **de datos** | 1 | ✅ | Alimentación y programación |
| 3 | Cargador USB de 5 V, **≥ 1 A** (o un puerto USB del PC) | 1 | ✅ | Alimentación |
| 4 | **GY-521** (acelerómetro/giroscopio MPU6050, I2C) | 1 | Opcional | Los ojos siguen la inclinación; ojos mareados al agitarlo |
| 5 | **Módulo sensor táctil** TTP223 (o KY-036 «metal touch» con salida digital) | 1 | Opcional | Caricias, acciones contextuales, pulsar para hablar en el Chat |
| 6 | Módulo **DHT11** (temperatura + humedad) | 1 | Opcional | Página **Plantas** (*Plants*) |
| 7 | Módulo **KY-018** (fotorresistencia LDR + 10 kΩ) | 1 | Opcional | Página *Plantas* (luz) |
| 8 | Cables Dupont (hembra-hembra / hembra-macho) | ~14 | Con sensores | Conexiones |
| 9 | Mancuernas | 1–2 | Opcional | Pausas activas del ciclo de **Ejercicio** (*Exercise*) (los ejercicios de movilidad no las necesitan) |
| 10 | Carcasa (impresa en 3D, se recomienda PETG) | 1 | Opcional | Ver §6 |
| 11 | Módulo **LED 2 colores** (kit Elegoo, pines GRY) | 1 | Opcional | Luz ambiental de un color por IO2: fijo en actividad, strobe en avisos (ver §2) |

Software/servicios (todos opcionales salvo el primero):

| Qué | ¿Obligatorio? | Para |
|---|:-:|---|
| Arduino IDE 2.x | ✅ | Compilar y subir el firmware |
| PC con Windows 10/11 y Python ≥ 3.9 | Opcional | `pc-agent` (presencia/llamadas, control del PC por voz, OTA) |
| Clave de API de Google AI Studio (Gemini) | Opcional | Página **Chat** (asistente de voz) |
| OpenCode de escritorio/CLI | Opcional | Enviar prompts por voz a una sesión de programación |

> Cada módulo opcional se detecta al arrancar. Si falta, KT1 sigue funcionando: los
> ojos se mueven al azar sin la IMU, la página *Plantas* muestra «sin lectura» sin
> DHT11/LDR, etc.

## 2. Pines que usa la placa (internos, ya conectados)

| Función | GPIO | Notas |
|---|---|---|
| TFT ILI9341 (SPI) | MOSI 11 · SCLK 12 · MISO 13 · CS 10 · DC 46 · BL 45 | Los configura el setup de TFT_eSPI de Freenove, no el sketch |
| Bus I2C interno | **SDA 16 · SCL 15** (400 kHz) | Compartido por el táctil (0x38), el códec (0x18) y el GY-521 (0x68) |
| Táctil FT6336U | RST 18 · INT 17 | Se lee directamente por I2C (`touch.h`) |
| Códec ES8311 (I2S) | MCLK 4 · BCLK 5 · WS 7 · DOUT 8 (altavoz) · DIN 6 (micro) | |
| Activación del amplificador del altavoz | 1 | Activo a nivel BAJO en esta placa (el firmware prueba la otra polaridad si el códec no responde) |
| LED RGB WS2812 | 42 | |
| microSD | 38, 39, 40, 41, 47, 48 | No se usa |

**Pines libres que usa KT1:** GPIO **2** (sensor táctil), GPIO **14** (DHT11), GPIO **3** (LDR, ADC1).
Evita el GPIO 21 (conectado a GND en esta placa) y los pines de la tabla anterior.

> **Módulo LED 2 colores externo (opcional, kit Elegoo, GRY):** con solo IO2 libre se usa
> un color. G es masa: cablea G → **GND**, el color elegido (Y amarillo o R rojo) →
> **IO2** y deja el otro sin conectar. Pon `PET_ENABLED 0` y `TWO_LED_ENABLED 1`. Fijo =
> actividad (fase, pomo), strobe 4 Hz = avisos, apagado = reposo. Las plantas se quedan
> como están.

## 3. Mapa de conexiones

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

> La disposición del conector lateral depende de la revisión de la placa: guíate por las
> serigrafías (`3V3`, `GND`, `IO2`, `IO14`, `IO3`), no por la posición en este dibujo.

### Tablas de cables

**GY-521 → conector I2C de la placa**

| GY-521 | Placa | Color sugerido | Nota |
|---|---|---|---|
| VCC | **3V3** | rojo | Nunca 5 V |
| GND | **GND** | negro | Masa común |
| SDA | **IO16 (SDA)** | azul | Bus compartido |
| SCL | **IO15 (SCL)** | amarillo | Bus compartido |
| AD0 | — | — | Al aire o a GND → dirección 0x68 (a 3V3 → 0x69) |
| INT, XDA, XCL | — | — | Sin conectar |

**Sensor táctil (TTP223 / KY-036)**

| Módulo | Placa | Nota |
|---|---|---|
| + / VCC | **3V3** | **Nunca 5 V**: su salida sería de 5 V y dañaría el GPIO |
| G / GND | **GND** | |
| DO / S | **IO2** | En ALTO mientras se toca (`PET_PIN 2`) |
| AO | — | Sin conectar (solo KY-036) |

**DHT11**

| Módulo | Placa | Nota |
|---|---|---|
| + / VCC | **3V3** | |
| − / GND | **GND** | |
| S / DATA | **IO14** | Se lee por bit-banging (sin librería); pull-up interno activado |

**KY-018 (LDR)**

| Módulo | Placa | Nota |
|---|---|---|
| pin central (+) | **3V3** | La resistencia de 10 kΩ del módulo va de + a S |
| − | **GND** | |
| S (AO) | **IO3** (ADC1) | Divisor resultante: 3V3 → 10 kΩ → AO → LDR → GND. **AO baja con más luz** |

> Si tu KY-018 lee al revés (el valor **sube** con la luz), intercambia `+` y `−`.
> El valor en lux es aproximado: sirve para comparar sitios, no es un luxómetro.

### Direcciones I2C

| Dispositivo | Dirección |
|---|---|
| Táctil FT6336U | 0x38 |
| Códec ES8311 | 0x18 |
| MPU6050 (GY-521) | 0x68 (0x69 con AD0 en alto) |

Los tres comparten SDA 16 / SCL 15 sin conflicto. Al arrancar, el firmware busca primero la
IMU en ese bus; si no está, prueba también los GPIO 9/21 en un segundo bus.

## 4. Notas de seguridad eléctrica

- Todo funciona a **3,3 V**. No alimentes los módulos a 5 V.
- Desconecta el USB antes de cambiar cables.
- Usa cables Dupont cortos enchufados directamente a los pines: el I2C a 400 kHz no se lleva
  bien con los contactos flojos de una protoboard (la IMU puede responder una vez y luego desaparecer).
- Consumo típico: 150–250 mA (picos de ~400 mA cuando transmite la Wi-Fi). Los cables finos o
  largos provocan reinicios por caída de tensión (brownout): cambia el cable/cargador antes de sospechar del código.

## 5. Batería (opcional, avanzado)

La placa tiene un conector de batería. Usa solo una **LiPo/Li-ion de 1 celda (3,7 V nominales) con
circuito de protección**, comprueba la polaridad del conector con un multímetro frente a la
serigrafía (los cables JST de distintos fabricantes vienen cableados de forma diferente) y consulta la
corriente de carga de la placa en la documentación de Freenove. El firmware **no** mide el
nivel de batería.

## 6. Consejos para la carcasa

- Ranuras de ventilación cerca del ESP32; el PLA se deforma a ~55 °C, el PETG es mejor.
- No presiones el cristal táctil (presión constante = toques fantasma); nada de metal encima.
- Monta el GY-521 **plano y rígido**, a ≥ 1 cm del ESP32 (calor): si se tambalea,
  los ojos tiemblan.
- Deja libre la zona de la antena del ESP32-S3; nada de carcasa metálica ni pintura metalizada.
- Deja huecos para el USB (con alivio de tensión) y los botones BOOT/RESET.
