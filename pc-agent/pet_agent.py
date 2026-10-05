# -*- coding: utf-8 -*-
"""
pet_agent.py - Desktop agent for the ESP32-S3 desk pets (KT1).

Detects which program is in the foreground on Windows, classifies it into a
category (see mapping.py) and sends it over Wi-Fi to one or more ESP32s:
  - Older DesktopPet / DesktopPetGif firmwares  ->  http://pet.local/pet
  - KT1                                         ->  http://kt1.local/pet

It also MONITORS the PC state:
  - Sustained high CPU        -> emotion "angry" (stressed)
  - Low battery, not charging -> emotion "bored" + speaks a warning (once)
  - Coming back from idle     -> "talks" to greet you
  - Microphone in use         -> "call": true  (KT1 does not interrupt or beep)
  - (optional) typing         -> moves the mouth while you type

POST /pet fields (firmwares ignore the ones they do not know):
    app, title        foreground program
    category          "classic" category (idle after >2 min without input)
    emotion           direct emotion (takes priority over category)
    talk              true -> talk / greet
    fg_category       REAL category of the app even while idle   [KT1]
    idle              seconds since the last key/mouse input     [KT1]
    call              true if some app is using the mic (meeting) [KT1]
    call_app          which app is using the mic                 [KT1]

Usage:
    python pet_agent.py                                  # -> pet.local
    python pet_agent.py --host kt1.local           # KT1 only
    python pet_agent.py --host kt1.local --host pet.local   # both
    python pet_agent.py --host 192.168.1.50 --verbose
    python pet_agent.py --no-call                        # do not watch the mic

Requirements: see requirements.txt  (pywin32, psutil, requests)
"""

import argparse
import ctypes
import socket
import sys
import time
import winreg

import requests

try:
    import win32gui
    import win32process
    import psutil
except ImportError:
    print("Missing dependencies. Install them with:  pip install -r requirements.txt")
    sys.exit(1)

from mapping import categorize, MIC_IGNORE

# ---------------------------------------------------------------------------
#  Default configuration
# ---------------------------------------------------------------------------
DEFAULT_HOST = "kt1.local"        # KT1 DeskPet mDNS name (legacy: deskbuddy.local, pet.local)
POLL_SECONDS = 1.0                # sampling interval
HEARTBEAT_SECONDS = 5.0           # periodic resend even if nothing changed
IDLE_THRESHOLD_SECONDS = 120      # no activity -> "idle"
REQUEST_TIMEOUT = 2.0             # timeout of each HTTP request
RESOLVE_RETRY_SECONDS = 30.0      # retry delay for a .local host that failed to resolve

# --- CPU monitor ---
CPU_HIGH_PCT = 85.0
CPU_SUSTAIN_SECONDS = 5.0

# --- Battery monitor ---
BATTERY_LOW_PCT = 20

# --- Talk while typing (optional) ---
TYPING_ACTIVE_SECONDS = 1.5


# ---------------------------------------------------------------------------
#  User idle detection (keyboard/mouse)
# ---------------------------------------------------------------------------
class LASTINPUTINFO(ctypes.Structure):
    _fields_ = [("cbSize", ctypes.c_uint), ("dwTime", ctypes.c_uint)]


def get_idle_seconds() -> float:
    """Seconds since the last keyboard/mouse activity."""
    info = LASTINPUTINFO()
    info.cbSize = ctypes.sizeof(info)
    if not ctypes.windll.user32.GetLastInputInfo(ctypes.byref(info)):
        return 0.0
    millis = (ctypes.windll.kernel32.GetTickCount() - info.dwTime) & 0xFFFFFFFF
    return millis / 1000.0


# ---------------------------------------------------------------------------
#  Foreground window detection
# ---------------------------------------------------------------------------
def get_foreground_app():
    """Return (exe_name, window_title) of the foreground window."""
    try:
        hwnd = win32gui.GetForegroundWindow()
        if not hwnd:
            return ("", "")
        title = win32gui.GetWindowText(hwnd) or ""
        _, pid = win32process.GetWindowThreadProcessId(hwnd)
        if not pid:
            return ("", title)
        exe = psutil.Process(pid).name()
        return (exe, title)
    except (psutil.NoSuchProcess, psutil.AccessDenied, Exception):
        return ("", "")


# ---------------------------------------------------------------------------
#  Microphone in use (= probably in a call / meeting)
#
#  Windows 10/11 records which apps use the mic under:
#    HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\
#        ConsentStore\\microphone\\<app>            (Store apps)
#        ConsentStore\\microphone\\NonPackaged\\<path#to#exe>   (regular apps)
#  If LastUsedTimeStop == 0 and LastUsedTimeStart != 0, the mic is in use NOW.
#  Nothing is recorded or listened to: only that flag is read.
# ---------------------------------------------------------------------------
MIC_KEY = r"Software\Microsoft\Windows\CurrentVersion\CapabilityAccessManager\ConsentStore\microphone"


def _pretty_app(key_name: str) -> str:
    if "#" in key_name:                          # C:#Program Files#...#Teams.exe
        return key_name.split("#")[-1]
    return key_name.split("_")[0]                # MicrosoftTeams_8wekyb3d8bbwe


def _mic_users(path: str):
    users = []
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, path) as key:
            i = 0
            while True:
                try:
                    sub = winreg.EnumKey(key, i)
                except OSError:
                    break
                i += 1
                if sub == "NonPackaged":
                    users += _mic_users(path + "\\NonPackaged")
                    continue
                try:
                    with winreg.OpenKey(key, sub) as sk:
                        start, _ = winreg.QueryValueEx(sk, "LastUsedTimeStart")
                        stop, _ = winreg.QueryValueEx(sk, "LastUsedTimeStop")
                        if start and stop == 0:
                            users.append(_pretty_app(sub))
                except OSError:
                    pass
    except OSError:
        pass
    return users


def mic_in_use():
    """Return (in_use, app_name)."""
    users = [u for u in _mic_users(MIC_KEY)
             if not any(ign in u.lower() for ign in MIC_IGNORE)]
    return (bool(users), users[0] if users else "")


# ---------------------------------------------------------------------------
#  Sending to the ESP32s (with a .local -> IP resolution cache)
# ---------------------------------------------------------------------------
class Target:
    """A target ESP32. Resolves the mDNS name once and reuses the IP
    (on Windows each .local resolution can take several seconds)."""

    def __init__(self, host: str):
        self.host = host
        self.ip = None
        self.next_resolve = 0.0

    def url(self):
        now = time.time()
        if self.ip is None and now >= self.next_resolve:
            try:
                self.ip = socket.gethostbyname(self.host)
            except OSError:
                self.next_resolve = now + RESOLVE_RETRY_SECONDS
        return f"http://{self.ip or self.host}/pet"

    def invalidate(self):
        if self.ip and self.ip != self.host:
            self.ip = None


def send_state(target: Target, payload: dict, verbose: bool) -> bool:
    url = target.url()
    try:
        r = requests.post(url, json=payload, timeout=REQUEST_TIMEOUT)
        if verbose:
            tag = payload.get("emotion") or payload.get("category") or "?"
            talk = " talk" if payload.get("talk") else ""
            call = " CALL" if payload.get("call") else ""
            app = payload.get("app", "")
            extra = ""
            try:                                  # KT1 replies with its state
                j = r.json()
                if "phase" in j:
                    extra = f" | {j['phase']} {j.get('remaining_s', 0)//60}m"
            except ValueError:
                pass
            print(f"  -> {target.host:16s} {tag:8s}{talk:5s}{call:5s} | {app:24s} "
                  f"| idle {payload.get('idle', 0):4d}s | HTTP {r.status_code}{extra}")
        return r.ok
    except requests.RequestException as e:
        target.invalidate()
        if verbose:
            print(f"  -> network error sending to {target.host}: {e.__class__.__name__}")
        return False


# ---------------------------------------------------------------------------
#  System monitors
# ---------------------------------------------------------------------------
def battery_status():
    """Return (percent, plugged_in) or (None, None) if there is no battery."""
    try:
        b = psutil.sensors_battery()
        if b is None:
            return (None, None)
        return (b.percent, b.power_plugged)
    except Exception:
        return (None, None)


# ---------------------------------------------------------------------------
#  Main loop
# ---------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(description="Desktop agent for the ESP32-S3 desk pets (KT1)")
    ap.add_argument("--host", action="append",
                    help=f"ESP32 host/IP (repeatable). Default {DEFAULT_HOST}")
    ap.add_argument("--verbose", action="store_true", help="Detailed log")
    ap.add_argument("--no-cpu", action="store_true", help="Disable the CPU monitor")
    ap.add_argument("--no-battery", action="store_true", help="Disable the battery monitor")
    ap.add_argument("--no-greet", action="store_true", help="Do not greet when coming back from idle")
    ap.add_argument("--no-call", action="store_true", help="Do not detect calls via the microphone")
    ap.add_argument("--talk-typing", action="store_true",
                    help="Move the mouth while you type/use the PC")
    args = ap.parse_args()

    targets = [Target(h) for h in (args.host or [DEFAULT_HOST])]
    print("[pet_agent] Sending to: " + ", ".join(f"http://{t.host}/pet" for t in targets))
    monitors = []
    if not args.no_cpu:     monitors.append("CPU")
    if not args.no_battery: monitors.append("battery")
    if not args.no_greet:   monitors.append("greeting")
    if not args.no_call:    monitors.append("calls (microphone)")
    if args.talk_typing:    monitors.append("talk-typing")
    print(f"[pet_agent] Monitors: {', '.join(monitors) or 'none'}")
    print("[pet_agent] Ctrl+C to quit.\n")

    psutil.cpu_percent(interval=None)   # first sample (returns 0.0)

    last_sent_key = {t.host: None for t in targets}
    last_heartbeat = {t.host: 0.0 for t in targets}
    cpu_high_since = None
    battery_warned = False
    prev_active = True
    prev_call = False

    while True:
        try:
            now = time.time()
            idle = get_idle_seconds()
            active = idle < IDLE_THRESHOLD_SECONDS
            cpu = psutil.cpu_percent(interval=None)
            batt_pct, plugged = battery_status()
            in_call, call_app = (False, "") if args.no_call else mic_in_use()
            if in_call != prev_call and args.verbose:
                print(f"  [call] {'STARTS (' + call_app + ')' if in_call else 'ends'}")
            prev_call = in_call

            # --- Foreground app (always, for fg_category) ---
            exe, title = get_foreground_app()
            fg_category = categorize(exe)
            if in_call and fg_category == "idle":
                fg_category = "chat"
            # Classic category: "idle" after a while without input (except during a call)
            category = fg_category if (active or in_call) else "idle"

            emotion = None
            talk = False

            # --- Override: low battery without charger ---
            if not args.no_battery and batt_pct is not None:
                if (not plugged) and batt_pct <= BATTERY_LOW_PCT:
                    emotion = "bored"
                    if not battery_warned:
                        talk = True
                        battery_warned = True
                        if args.verbose:
                            print(f"  [battery] {batt_pct}% without charger -> warning")
                else:
                    battery_warned = False

            # --- Override: sustained high CPU ---
            if not args.no_cpu:
                if cpu >= CPU_HIGH_PCT:
                    if cpu_high_since is None:
                        cpu_high_since = now
                else:
                    cpu_high_since = None
                sustained = (cpu_high_since is not None and
                             (now - cpu_high_since) >= CPU_SUSTAIN_SECONDS)
                if sustained and emotion is None:
                    emotion = "angry"

            # --- Greeting when coming back from idle ---
            if active and not prev_active and not args.no_greet:
                talk = True
            prev_active = active

            if args.talk_typing and active and idle < TYPING_ACTIVE_SECONDS:
                talk = True

            # --- Payload ---
            payload = {
                "app": exe if (active or in_call) else "",
                "title": title[:60] if (active or in_call) else "",
                "fg_category": fg_category,
                "idle": int(idle),
                "call": in_call,
            }
            if call_app:
                payload["call_app"] = call_app
            if emotion is not None:
                payload["emotion"] = emotion
            else:
                payload["category"] = category
            if talk:
                payload["talk"] = True

            tag = emotion if emotion is not None else category
            key = (tag, payload["app"], in_call)

            for t in targets:
                changed = key != last_sent_key[t.host]
                heartbeat_due = (now - last_heartbeat[t.host]) >= HEARTBEAT_SECONDS
                if changed or heartbeat_due or talk:
                    if send_state(t, payload, args.verbose):
                        last_sent_key[t.host] = key
                        last_heartbeat[t.host] = now
                    elif changed and args.verbose:
                        print("  (will retry on the next cycle)")

            time.sleep(POLL_SECONDS)

        except KeyboardInterrupt:
            print("\n[pet_agent] Shutting down. Bye!")
            break


if __name__ == "__main__":
    main()
