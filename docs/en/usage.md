# Using KT1

**English** | [Español](../es/usage.md) · [← Back to README](../../README.md)

## Navigation

| Gesture | Action |
|---|---|
| Horizontal swipe | Previous / next page (the dots below show the position) |
| Vertical swipe | Brightness up / down |
| Long press (most pages) | Change accent colour |
| Finger resting on the face | The eyes follow your finger |
| Tilt KT1 (with GY-521) | The eyes look to that side |
| Shake it | Dizzy spiral eyes for a few seconds |

After 90 s without touching it returns to the **Face**, unless a pomodoro, a nudge or an
active break is running.

### Touch sensor (TTP223)

Its action depends on what is happening (in this priority order):
Chat → active break → posture nudge → Vital alert → pomodoro → Vital phase →
plant measurement → otherwise **petting**.

- **Short touch** (< 0.4 s): main action (confirm, start, pause…) or a greeting.
- **Hold** (≥ 0.9 s): strong action (finish, exit…) or cuddles + purring.
- On the **Chat** page: hold to talk (push-to-talk), touch to replay the answer.

## Pages

| # | Page | What it does |
|---|---|---|
| 1 | **Face** | Animated eyes with 10 moods, blinking, idle gestures. Top bar: time, cycle phase and minutes left, outside temperature, Wi-Fi. Tap = next mood. Shows posture nudges and Vital alerts |
| 2 | **Exercise** | Sit/stand cycle **20 min sitting → 8 min standing → 2 min moving** (Cornell). Every 4 cycles, a longer circuit break. Today's muscle group (legs / back / arms) |
| — | *Coach* (overlay) | Guides each active break: 3-2-1 countdown, one beep per repetition, side change, rest. Buttons *Other* / *No weights* / *Skip* |
| 3 | **Pomodoro** | *Work* fixed + *Writing/Leisure* flow count-up, per-mode times (25/5, 50/10, 15/15) editable inline, per-mode start/end tones, voice task binding (`FOCUS:` sets task + mode, starts focus, speaks one tailored tip shown on the break card), auto 4-cycle + long break, dots + per-mode today count, quiet mode, Pause/End (End asks twice), inline notices on their own line that never cover the buttons |
| 4 | **Plants** | Pick a plant and tap **Measure**: ~8 s of light (LDR), temperature and humidity (DHT11) compared with its ideal ranges → *Low / Ideal / High* |
| 5 | **Vital** | Alternative posture shifts SITTING / STANDING / RELAX (mobility only). Configurable intervals. Mutually exclusive with *Exercise* |
| 6 | **Weather** | Open-Meteo (no key): icon, temperature, feels like, humidity, wind, max/min, rain chance, sunrise/sunset. Every 15 min; tap = refresh |
| 7 | **Clock** | Large time, date, week strip, day of year and ISO week |
| 8 | **Chat** | Voice assistant (see below) |
| 9 | **Sound** | Sound on/off, volume (tap a segment to test it) |
| 10 | **Screen** | Brightness (5 levels), accent colour (Cyan / Amber / Green / Pink) and **Language** (English / Espanol) |
| 11 | **Timers** | Pomodoro focus (15/25/35/45), short break (5/10/15), long break (15/20/30, auto every 4th) and anti-sedentary reminder (OFF/15/30/45/60 min) |

All settings are stored in flash (NVS) and survive reboots.

## Language

**Screen** page → **Language** row → tap *English* or *Espanol*. It changes immediately:
texts, page names, exercises, dates, the voice assistant's language and the language of
the texts returned by the PC bridge. The first boot uses `DEFAULT_LANG` (English).

## Onboard RGB LED (diagnostics: inside the case, not visible)

| Colour | Meaning |
|---|---|
| Blinking blue | Connecting to Wi-Fi |
| Steady red | No Wi-Fi |
| Blinking amber | OpenCode build in progress |
| Blinking red | OpenCode build failed |
| Steady purple | Firmware update (OTA) |
| Off | All good |

## External LED module (optional, see hardware §2)

| Behaviour | Meaning |
|---|---|
| Steady on | Activity: cycle phase, pomodoro, active break |
| Fast strobe | Alert: posture, Vital, broken build, no Wi-Fi |
| Off | Idle |

## Voice assistant (Chat page)

Tap **TALK** (records 4 s) or hold the touch sensor. Eyes show the state: wide open =
listening, amber = thinking, happy = speaking. Long press = replay the last answer.

With `pc-agent` running on your PC you can also ask for actions:

| You say (examples) | Command | Effect on the PC |
|---|---|---|
| "mute the PC" | `MUTE` | Mute toggle |
| "volume to 40" | `VOL:40` | Master volume |
| "play / pause the music" | `PLAY` | Play/pause media key |
| "open notepad" | `OPEN:<app>` | Opens an app from the **allowlist** only |
| "lock the PC" | `LOCK` → `CONFIRM` | Win+L (asks "are you sure?" first) |
| "suspend the PC" | `SLEEP` → `CONFIRM` | Sleep (asks first) |
| "what's on my screen?" | `SCREEN` / screenshot | Describes the active window / screenshot |
| "play the video of X" | `PLAY:<text or URL>` | Opens the URL or a YouTube search |
| "note down: buy bread" | `TODO:<text>` | Appends to your TODO file |
| "remind me in 5 minutes to …" | `REMIND:<s>:<text>` | Windows notification + KT1 alert |
| "what reminders do I have?" | `REMINDLIST` | Reads pending reminders |
| "remember that I like …" | `MEMORY:<fact>` | Stores a fact in local memory |
| "what do you remember?" | `MEMLIST` | Reads stored facts |
| "give me the briefing" | `BRIEF` | Reminders + TODO + memory + screen |
| "do not disturb" | `DND` | Toggles Windows notifications |
| "pair the PC" | `PAIR` | Gets the bridge token without editing `secrets.h` (first 15 min after the bridge starts) |
| "tell the session to review X" | `PROMPT:<text>` | Sends a prompt to an OpenCode session |
| "summarize the session" | `RESUME` | Reads the latest OpenCode messages |
| "go to pomodoro" | `PAGE:` | Changes the device page |
| "starting the report" | `FOCUS:<mode>:<task>` | Binds the task to pomodoro (classifies work/writing/leisure), starts focus and speaks one tailored tip |

## Health notes

The 20-8-2 cycle follows Cornell University Ergonomics (*Sitting and Standing at Work*);
the daily goal of 2 h standing/light activity follows Buckley et al. 2015 (*Br J Sports
Med*); strength for all major muscle groups ≥ 2 days/week follows the WHO 2020
guidelines. Exercises are listed in `firmware/kt1-deskpet/exercises.h`.

> ⚠️ **This is not a medical program.** Use a weight that leaves 2–3 reps in reserve, warm
> up with the *no weights* version, keep a neutral back, never lean on a raised standing
> desk or a wheeled chair, keep dumbbells on the floor, and stop if you feel sharp pain,
> dizziness or shortness of breath. If you have injuries or health conditions, ask your
> doctor first.
