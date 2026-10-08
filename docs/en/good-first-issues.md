# Good first issues (copy-paste ready)

**English** | [Español](../es/good-first-issues.md)

> Maintainer: create these as GitHub issues and add the `good first issue` label
> (one click per issue). Copy the title + body from each section below.

---

## 1. Port `pc-agent` to Linux (then macOS)

**Labels:** `good first issue`, `enhancement`, `pc-agent`
**Why:** `pc-agent` is Windows-only today (`pywin32`, `ctypes.windll`, PowerShell). Linux/macOS support triples the contributor pool.

**Scope (suggested split into 2 PRs):**
- `pet_agent.py`: abstract foreground-app / idle-time / mic-in-use behind a `platform_` layer. Linux: `xdotool`/`libinput`/`/proc`, or XDG portals on Wayland. Keep Windows path untouched.
- `bridge.py`: abstract `/media/*`, `/os/lock`, `/os/dnd`, `/screen/shot`. Linux: `playerctl`, `loginctl`, `mss` already cross-platform.
- `setup.py` + `requirements.txt`: make `pywin32`/`pycaw`/`comtypes` Windows-only extras; document `pip install -r requirements-linux.txt`.

**Files:** `pc-agent/pet_agent.py`, `pc-agent/bridge.py`, `pc-agent/setup.py`, `pc-agent/requirements.txt`, `docs/en/pc-agent.md`
**Acceptance:**
- `python -m compileall -q pc-agent` passes on Windows + Linux.
- On Linux: presence (`POST /pet`), volume/media and screenshot work; unsupported routes return `501` with a clear message, never crash.
- Docs updated (EN+ES) with per-OS install steps.

---

## 2. Grow the exercise library (`exercises.h`)

**Labels:** `good first issue`, `firmware`
**Why:** Zero hardware needed, pure data edit, visible on device in minutes.

**Scope:** add 4–6 entries to `EXERCISES[]` in `firmware/kt1-deskpet/exercises.h` (groups `GR_MOB` / `GR_LEG` without weights).
**Rules:** name ≤ ~21 chars, cue ≤ ~34 chars, no accents, both `{ "English", "Spanish" }`, `pio run -e kt1` compiles.
**Acceptance:** new moves rotate correctly in Exercise breaks; PR includes a photo or Wokwi note.

---

## 3. Calibrate plant thresholds (DHT11 + LDR)

**Labels:** `good first issue`, `firmware`, `documentation`
**Why:** Current thresholds are guesses; real-room data makes the Plants page useful.

**Scope:** log a day of DHT11 + KY-018 readings (sun/shade, day/night), propose `TEMP_OK`/`HUM_OK`/lux bands, update firmware constants + `docs/en|es/hardware.md § Plants`.
**Acceptance:** table with sample readings + chosen bands; firmware still compiles; docs EN+ES updated.

---

## 4. Wokwi smoke test + wiring photos

**Labels:** `good first issue`, `documentation`
**Why:** Every verified wiring photo and Wokwi run saves a new builder an hour.

**Scope:** import `diagram.json` into Wokwi (ESP32-S3 + ILI9341 + MPU6050 + DHT + button), report boot log + screenshots; optionally add one real wiring photo (no Wi-Fi names, IPs, tokens — see `docs/media/README.md`).
**Acceptance:** checklist of what boots / what differs from real FNK0104B wiring; photos or Wokwi link attached to the PR.
