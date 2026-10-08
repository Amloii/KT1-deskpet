# PC companion (`pc-agent`)

**English** | [Español](../es/pc-agent.md) · [← Back to README](../../README.md)

Optional Python tools for **Windows 10/11** (Python ≥ 3.9) that run on the PC sharing the
Wi-Fi network with KT1.

| Script | What it does |
|---|---|
| `pet_agent.py` | Every second reads the foreground app, idle time, CPU, battery and whether the **microphone is in use** (call) and sends it to KT1 (`POST /pet`). KT1 uses it to never interrupt you during a call or mid-sentence |
| `bridge.py` | Local HTTP server (port 8750) that executes the voice commands: media, volume, apps, lock/sleep, TODO, reminders, memory, do-not-disturb, PC screenshot, OpenCode, OTA firmware |
| `mapping.py` | App → category table (`coding`, `web`, `gaming`, `media`, `chat`) and `MIC_IGNORE` |
| `setup.py` | Guided setup: dependencies, OpenCode CLI, `setup.json`, token, patches the firmware `secrets.h`, Windows firewall rule, autostart shortcut, OTA firmware registration and self-tests |
| `watchdog.py` | Starts `bridge.py` and `pet_agent.py` and restarts them if they die (used by the autostart shortcut) |
| `calm_window.py` | **Calm window** on the PC: big 60 s guided breathing, **DND 5 min** button (via bridge, self-restoring) and remote Calm opening on KT1 |

## Install

```powershell
cd pc-agent
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
python setup.py            # --yes (no questions) · --dry-run · --no-shortcuts · --no-firewall
```

`setup.py` creates `setup.json` (local, git-ignored; template: `setup.example.json`) and a
single `KT1 watchdog.lnk` in `shell:startup`. Then **re-upload the firmware** so it picks
up `PC_HOST` / `BRIDGE_TOKEN` from `secrets.h`.

## Command line

```text
python pet_agent.py --host kt1.local [--host other.local] [--verbose]
                    [--no-cpu] [--no-battery] [--no-greet] [--no-call] [--talk-typing]

python bridge.py [--port 8750] [--config setup.json] [--todo C:\path\to\TODO.md]
                 [--esp http://kt1.local] [--token ...] [--opencode-cli path]

python calm_window.py [--config setup.json] [--lang es|en] [--seconds 60]
```

If `kt1.local` does not resolve, use the IP printed by KT1 at boot.

## `POST /pet` protocol (agent → KT1)

| Field | Meaning |
|---|---|
| `app`, `title` | Foreground app and window title (empty when idle) |
| `category` / `emotion` | App category, or an emotion that overrides it (`angry` with high CPU, `bored` with low battery) |
| `fg_category` | Real category of the app even when idle (e.g. watching a video ≠ away) |
| `idle` | Seconds without keyboard/mouse (≥ 300 → "away") |
| `call`, `call_app` | An app is using the microphone. Read from the Windows privacy indicator (registry `CapabilityAccessManager`): **nothing is recorded or listened to** |
| `talk` | Greet / speak once |
| `calm` | Open the Calm page on KT1 (SOS; used by `calm_window.py`) |
| `build`, `summary` | OpenCode build state (`busy`/`ok`/`fail`) forwarded by the bridge |

KT1 answers `{"ok":true,"phase":"…","remaining_s":754,"posture":"sit","coach":false,"paused":false}`.

## Bridge API (KT1 → PC)

All routes except `/health` need the header `X-Bridge-Token` when a token is set.
KT1 adds `?lang=en|es` to every request; human-readable answers (briefing, spoken
errors, reminder texts) come back in that language.

| Method | Route | Body / query | Action |
|---|---|---|---|
| GET | `/health` | – | Status of bridge, ESP and OpenCode (no token) |
| GET | `/screen` · `/screen/shot` | – | Active window (app, title, idle) · 320×240 JPEG screenshot |
| POST | `/media/mute-toggle` · `/media/playpause` | – | Media keys |
| POST | `/media/play` | `{url}` or `{query}` | Opens a URL or a YouTube search |
| GET/POST | `/media/volume` | `{level: 0-100}` | Read / set master volume |
| POST | `/os/lock` · `/os/sleep` | – | Lock session · suspend |
| POST | `/os/open` | `{app}` | Opens an app **only from the allowlist** (unknown → `400 app not allowed`) |
| GET/POST | `/os/dnd` | `{on}` optional | Windows notifications on/off |
| POST | `/todo` | `{text}` | Appends `- [date] text` to the TODO file |
| GET/POST/DELETE | `/memory` · `/memory/<i>` | `{text}` | Local facts (`memory.json`, max 100) |
| GET | `/briefing` | – | Reminders + TODO + memory + screen, as one sentence |
| POST/GET/DELETE | `/remind` · `/reminders` · `/reminders/<id>` | `{text, seconds 10-86400}` | Reminders: Windows notification + alert on KT1 |
| GET | `/opencode/sessions` · `/opencode/messages` · `/opencode/resume` | `?session=` | Read OpenCode sessions |
| POST | `/opencode/prompt` | `{session, text}` | Runs `opencode-cli run -s <session> <text>` in the background |
| POST | `/pair` | – | Returns a new token if none is set (only during the first 15 min) |
| POST | `/build` | `{build, summary}` | Forwards a build status to KT1 |
| GET | `/firmware/version` · `/firmware/kt1.bin` | – | OTA |

### Allowed apps (`/os/open`)

English: notepad, calculator, calc, paint, terminal, cmd, command prompt, console, explorer,
file explorer, files, code, vscode, vs code, visual studio code, chrome, google chrome, edge,
microsoft edge, firefox, discord, teams, telegram, whatsapp, spotify, word, excel,
snipping tool, snip. Spanish: bloc, bloc de notas, calculadora, consola, explorador,
archivos, codigo, tijeras, recortes. Edit `APP_MAP` in `bridge.py` to change it.

## Calm window (`calm_window.py`)

SOS window on the PC for tension, stress or craving: big guided breathing (same 10 s
cycle as KT1), a button to open Calm on the device (`POST /pet {"calm":true}`) and a
**DND 5 min** button (turns Windows Do-Not-Disturb on via the bridge and restores it
alone). Reads `setup.json` for the ESP, port and token. Records and uploads nothing.

## Security

- Use a token (`setup.py` generates one). Without a token the bridge accepts any client on
  your LAN.
- Rate limit: 20 POST/DELETE per minute per IP (`429`). Every action is logged to
  `audit.log` (local, git-ignored).
- Reminder texts are passed to PowerShell through environment variables and XML-escaped
  (no command injection).
- Plain HTTP on the LAN: do not forward the port to the internet.
- Local files with personal data, never committed: `setup.json` (token, IP, paths),
  `memory.json`, `audit.log`, `firmware/*.bin`.
