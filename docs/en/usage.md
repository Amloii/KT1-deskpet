# Using KT1

**English** | [Español](../es/usage.md) · [← Back to README](../../README.md)

## Navigation

| Gesture | Action |
|---|---|
| Horizontal swipe | Previous / next page (the dots below show the position) |
| Vertical swipe | Brightness up / down |
| Long press (most pages) | Change accent colour |
| Long press on **Face** | Open **Calm** (SOS breathing) |
| Long press on **Calm** | End the session |
| Finger resting on the face | The eyes follow your finger |
| Tilt KT1 (with GY-521) | The eyes look to that side |
| Shake it | Dizzy spiral eyes for a few seconds |

After 90 s without touching it returns to the **Face**, unless a pomodoro, a Calm session,
a nudge or an active break is running.

### Touch sensor (TTP223)

Its action depends on what is happening (in this priority order):
Chat → active break → posture nudge → Vital alert → pomodoro → Vital phase →
plant measurement → otherwise **petting**.

- **Short touch** (< 0.4 s): main action (confirm, start, pause…) or a greeting.
- **Hold** (≥ 0.9 s): strong action (finish, exit…) or cuddles + purring.
- On the **Chat** page: hold to talk (push-to-talk), touch to replay the answer.
- On the **Calm** page: touch to pause/advance, hold to finish.

## Pages

| # | Page | What it does |
|---|---|---|
| 1 | **Face** | Animated eyes with 10 moods, blinking, idle gestures. Top bar: time, cycle phase and minutes left, outside temperature, Wi-Fi. Tap = next mood. Shows posture nudges and Vital alerts |
| 2 | **Exercise** | Trainer library: today's legs / back / arms plan + guided coach (3-2-1, beep per rep, side change, rest). The day cycle is run from Vital |
| — | *Coach* (overlay) | Guides each active break: 3-2-1 countdown, one beep per repetition, side change, rest. Buttons *Other* / *No weights* / *Skip* |
| 3 | **Pomodoro** | *Work* fixed + *Writing/Leisure* flow count-up, per-mode times (25/5, 50/10, 15/15) editable inline, per-mode start/end tones, voice task binding (`FOCUS:` sets task + mode, starts focus, speaks one tailored tip shown on the break card), auto 4-cycle + long break, dots + per-mode today count, quiet mode, Pause/End (End asks twice), inline notices on their own line that never cover the buttons |
| 4 | **Plants** | Pixel-art catalog. Live scout mode: light every ~0.4 s and temperature/humidity (DHT11) every 2 s with ideal-range bars turning green + advice (`Move nearer the window`...). Tap above or the sensor for the official ~8 s averaged measurement with a 0-100 score |
| 5 | **Vital** | Day director: SIT / STAND / RELAX with 3 one-tap presets (Gentle no-desk 45/3, Classic Cornell 20/8/2, Focus 50/10), big countdown, Buckley 2h day bar, next-up line. Opens the Exercise coach on breaks |
| 6 | **Calm** | SOS meditation/relaxation (see below): **Breathe 60 s** (cyclic sighing), **Ride the urge 3 min** (watch craving as a wave) and **Ground 5-4-3-2-1**. Today/total counter on the menu, no streaks |
| 7 | **Weather** | Open-Meteo (no key): icon, temperature, feels like, humidity, wind, max/min, rain chance, sunrise/sunset. Every 15 min; tap = refresh |
| 8 | **Clock** | Large time, date, week strip, day of year and ISO week |
| 9 | **Chat** | Voice assistant (see below) |
| 10 | **Sound** | Sound on/off, volume (tap a segment to test it) |
| 11 | **Screen** | Brightness (5 levels), accent colour (Cyan / Amber / Green / Pink) and **Language** (English / Espanol) |
| 12 | **Timers** | Pomodoro focus (15/25/35/45), short break (5/10/15), long break (15/20/30, auto every 4th) and anti-sedentary reminder (OFF/15/30/45/60 min) |

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
| "I need to calm down / craving" | `PAGE:Calm` | Opens the Calm window from the start (SOS menu) |
| "starting the report" | `FOCUS:<mode>:<task>` | Binds the task to pomodoro (classifies work/writing/leisure), starts focus and speaks one tailored tip |

## Calm window (Calm page + PC)

For moments of tension, stress or urge/craving. SOS access: long-press on **Face**,
say "I need to calm down", or open `Calm` on the PC (`python calm_window.py`).

| Mode | What it does | Basis |
|---|---|---|
| **Breathe 60 s** | Guided cyclic sighing: double inhale + long exhale, 6 cycles | Balban et al. 2023 (*Cell Reports Medicine*): 5 min/day improves mood and lowers arousal more than mindfulness |
| **Ride the urge 3 min** | Rate 0-10, watch the "wave" without acting, rate again | Bowen & Marlatt 2009: urge surfing does not remove craving on the spot, but changes the response (less use at 7 days) |
| **Ground 5-4-3-2-1** | See 5 → touch 4 → hear 3 → feel 2 → breathe 1 | Clinical-consensus sensory grounding for panic (limited direct evidence) |

First run shows a notice (not therapy) accepted with OK. Only counters are stored
(today/total, last rating) in flash; nothing sensitive leaves the device. On the PC,
`calm_window.py` adds big-screen breathing, a **DND 5 min** button (mutes notifications
and restores them alone) and remote Calm opening on KT1.

> ⚠️ **This is not medical or addiction treatment.** If the urge is strong or distress
> persists, seek professional help.

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
