# Compañero de PC (`pc-agent`)

[English](../en/pc-agent.md) | **Español** · [← Volver al README](../../README.es.md)

Herramientas opcionales en Python para **Windows 10/11** (Python ≥ 3.9) que se ejecutan en el PC que comparte
la red Wi-Fi con KT1.

| Script | Qué hace |
|---|---|
| `pet_agent.py` | Cada segundo lee la app en primer plano, el tiempo de inactividad, la CPU, la batería y si el **micrófono está en uso** (llamada), y lo envía a KT1 (`POST /pet`). KT1 lo usa para no interrumpirte nunca durante una llamada ni a mitad de frase |
| `bridge.py` | Servidor HTTP local (puerto 8750) que ejecuta los comandos de voz: multimedia, volumen, apps, bloquear/suspender, TODO, recordatorios, memoria, no molestar, captura de pantalla del PC, OpenCode, firmware OTA |
| `mapping.py` | Tabla app → categoría (`coding`, `web`, `gaming`, `media`, `chat`) y `MIC_IGNORE` |
| `setup.py` | Configuración guiada: dependencias, OpenCode CLI, `setup.json`, token, modifica el `secrets.h` del firmware, regla del firewall de Windows, acceso directo de inicio automático, registro del firmware OTA y autopruebas |
| `watchdog.py` | Arranca `bridge.py` y `pet_agent.py` y los reinicia si se caen (lo usa el acceso directo de inicio automático) |

## Instalación

```powershell
cd pc-agent
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
python setup.py            # --yes (no questions) · --dry-run · --no-shortcuts · --no-firewall
```

`setup.py` crea `setup.json` (local, ignorado por git; plantilla: `setup.example.json`) y un
único `KT1 watchdog.lnk` en `shell:startup`. Después **vuelve a subir el firmware** para que tome
`PC_HOST` / `BRIDGE_TOKEN` de `secrets.h`.

## Línea de comandos

```text
python pet_agent.py --host kt1.local [--host other.local] [--verbose]
                    [--no-cpu] [--no-battery] [--no-greet] [--no-call] [--talk-typing]

python bridge.py [--port 8750] [--config setup.json] [--todo C:\path\to\TODO.md]
                 [--esp http://kt1.local] [--token ...] [--opencode-cli path]
```

Si `kt1.local` no se resuelve, usa la IP que imprime KT1 al arrancar.

## Protocolo `POST /pet` (agente → KT1)

| Campo | Significado |
|---|---|
| `app`, `title` | App en primer plano y título de la ventana (vacíos si estás inactivo) |
| `category` / `emotion` | Categoría de la app, o una emoción que la sustituye (`angry` con la CPU alta, `bored` con poca batería) |
| `fg_category` | Categoría real de la app incluso estando inactivo (p. ej., ver un vídeo ≠ estar ausente) |
| `idle` | Segundos sin teclado/ratón (≥ 300 → «ausente») |
| `call`, `call_app` | Una app está usando el micrófono. Se lee del indicador de privacidad de Windows (registro `CapabilityAccessManager`): **no se graba ni se escucha nada** |
| `talk` | Saludar / hablar una vez |
| `build`, `summary` | Estado de la compilación de OpenCode (`busy`/`ok`/`fail`) que reenvía el bridge |

KT1 responde `{"ok":true,"phase":"…","remaining_s":754,"posture":"sit","coach":false,"paused":false}`.

## API del bridge (KT1 → PC)

Todas las rutas salvo `/health` necesitan la cabecera `X-Bridge-Token` cuando hay un token configurado.
KT1 añade `?lang=en|es` a cada petición; las respuestas legibles (parte, errores
hablados, textos de los recordatorios) vuelven en ese idioma.

| Método | Ruta | Cuerpo / query | Acción |
|---|---|---|---|
| GET | `/health` | – | Estado del bridge, el ESP y OpenCode (sin token) |
| GET | `/screen` · `/screen/shot` | – | Ventana activa (app, título, inactividad) · captura JPEG de 320×240 |
| POST | `/media/mute-toggle` · `/media/playpause` | – | Teclas multimedia |
| POST | `/media/play` | `{url}` o `{query}` | Abre una URL o una búsqueda en YouTube |
| GET/POST | `/media/volume` | `{level: 0-100}` | Leer / fijar el volumen general |
| POST | `/os/lock` · `/os/sleep` | – | Bloquear la sesión · suspender |
| POST | `/os/open` | `{app}` | Abre una app **solo de la lista de permitidas** (desconocida → `400 app not allowed`) |
| GET/POST | `/os/dnd` | `{on}` opcional | Notificaciones de Windows activadas/desactivadas |
| POST | `/todo` | `{text}` | Añade `- [date] text` al archivo TODO |
| GET/POST/DELETE | `/memory` · `/memory/<i>` | `{text}` | Datos locales (`memory.json`, máx. 100) |
| GET | `/briefing` | – | Recordatorios + TODO + memoria + pantalla, en una sola frase |
| POST/GET/DELETE | `/remind` · `/reminders` · `/reminders/<id>` | `{text, seconds 10-86400}` | Recordatorios: notificación de Windows + alerta en KT1 |
| GET | `/opencode/sessions` · `/opencode/messages` · `/opencode/resume` | `?session=` | Leer sesiones de OpenCode |
| POST | `/opencode/prompt` | `{session, text}` | Ejecuta `opencode-cli run -s <session> <text>` en segundo plano |
| POST | `/pair` | – | Devuelve un token nuevo si no hay ninguno configurado (solo durante los primeros 15 min) |
| POST | `/build` | `{build, summary}` | Reenvía un estado de compilación a KT1 |
| GET | `/firmware/version` · `/firmware/kt1.bin` | – | OTA |

### Apps permitidas (`/os/open`)

Inglés: notepad, calculator, calc, paint, terminal, cmd, command prompt, console, explorer,
file explorer, files, code, vscode, vs code, visual studio code, chrome, google chrome, edge,
microsoft edge, firefox, discord, teams, telegram, whatsapp, spotify, word, excel,
snipping tool, snip. Español: bloc, bloc de notas, calculadora, consola, explorador,
archivos, codigo, tijeras, recortes. Edita `APP_MAP` en `bridge.py` para cambiarla.

## Seguridad

- Usa un token (`setup.py` genera uno). Sin token, el bridge acepta cualquier cliente de
  tu red local.
- Límite de peticiones: 20 POST/DELETE por minuto e IP (`429`). Cada acción queda registrada en
  `audit.log` (local, ignorado por git).
- Los textos de los recordatorios se pasan a PowerShell mediante variables de entorno y con escape XML
  (sin inyección de comandos).
- HTTP sin cifrar en la red local: no redirijas el puerto a internet.
- Archivos locales con datos personales, que nunca se suben al repositorio: `setup.json` (token, IP, rutas),
  `memory.json`, `audit.log`, `firmware/*.bin`.
