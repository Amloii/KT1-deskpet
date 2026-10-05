# -*- coding: utf-8 -*-
"""
bridge.py - KT1 <-> Windows <-> OpenCode bridge.

Listens on 0.0.0.0:8750 (LAN, for the ESP32) and translates:
  POST /media/mute-toggle, /media/playpause  -> system media keys (no focus steal)
  POST /media/play {"url"|"query"}            -> opens a video/URL in the browser
  GET  /screen                                -> {app, title, idle} (active window)
  POST /os/lock, /os/sleep, /os/dnd          -> Win+L / suspend / DND*
  POST /os/open {"app": ...}                  -> launches an app from a closed allowlist
  POST /todo {"text": ...}                    -> appends to a TODO.md file
  GET  /opencode/sessions                     -> lists sessions (via opencode-cli)
  POST /opencode/prompt {"session":..,"text":..} -> queues a prompt in the session (202)
  GET  /opencode/messages?session=..&limit=.. -> latest (trimmed) texts
  GET  /opencode/resume?session=..            -> extractive summary (<400 chars, display)
  POST /build {"build":"busy|ok|fail","summary":..} -> POST /pet to the ESP
  GET  /health -> {ok, bridge, opencode, esp, todo_exists}

OpenCode is driven through the local `opencode-cli` (Windows service), without
depending on `serve --port 4096` (v2 API with password). Designed as tools for
the ESP voice chatbot (chat.h): SCREEN, PLAY:<url>, PROMPT, RESUME.

* Language: every request may carry `?lang=en|es` (default en); human-readable
  sentences returned to the device are produced in that language.
* DND Win11: real Do-Not-Disturb via ToastEnabled (GET|POST /os/dnd).
* Vision: GET /screen/shot returns a 320x240 JPEG for Gemini.
* Auth: --token (X-Bridge-Token on everything except /health). Audit in audit.log.

Usage:
  python bridge.py --port 8750 ^
    --todo "C:\\path\\to\\TODO.md" ^
    --esp http://kt1.local
"""
import argparse
import base64
import ctypes
import datetime
import glob
import hmac
import json
import os
import re
import secrets
import subprocess
import sys
import threading
import time
import webbrowser

import requests
from flask import Flask, has_request_context, jsonify, request

app = Flask(__name__)
CFG = {"todo": "", "esp": "http://kt1.local", "opencode_cli": "", "token": "",
       "config": ""}
BRIDGE_VERSION = "3.5"
HERE = os.path.dirname(os.path.abspath(__file__))
AUDIT_FILE = os.path.join(HERE, "audit.log")
MEM_FILE = os.path.join(HERE, "memory.json")
BOOT_TS = time.time()


def _audit(action, detail=""):
    try:
        with open(AUDIT_FILE, "a", encoding="utf-8") as f:
            f.write(json.dumps({"ts": datetime.datetime.now().isoformat(timespec="seconds"),
                                "action": action,
                                "detail": str(detail)[:200]}) + "\n")
    except Exception:
        pass


def _lang():
    """Language requested by the device (?lang=en|es). Default: en."""
    if not has_request_context():
        return "en"
    return "es" if request.args.get("lang", "en").lower().startswith("es") else "en"


def T(en, es, lang=None):
    """Pick the English or Spanish variant of a sentence."""
    return es if (lang or _lang()) == "es" else en


def need_auth():
    """401 if a token is configured and does not match. /health is always open."""
    if request.path == "/health" or not CFG["token"]:
        return None
    got = request.headers.get("X-Bridge-Token", "").encode("utf-8")
    if not hmac.compare_digest(got, CFG["token"].encode("utf-8")):
        return jsonify({"ok": False, "error": "unauthorized"}), 401
    return None


@app.before_request
def _auth():
    r = need_auth()
    if r is not None:
        return r


from collections import deque
_RL = {}   # ip -> deque[timestamps] (mutations only: POST/DELETE)


@app.before_request
def _ratelimit():
    if request.method not in ("POST", "DELETE"):
        return None
    now = time.time()
    q = _RL.setdefault(request.remote_addr or "?", deque())
    while q and now - q[0] > 60:
        q.popleft()
    if len(q) >= 20:
        return jsonify({"ok": False, "error": "rate-limited"}), 429
    q.append(now)
    return None

# --- media keys (without stealing focus) ---
VK_VOLUME_MUTE = 0xAD
VK_MEDIA_PLAY_PAUSE = 0xB1
KEYEVENTF_KEYUP = 0x0002


def tap_vk(vk):
    ctypes.windll.user32.keybd_event(vk, 0, 0, 0)
    ctypes.windll.user32.keybd_event(vk, 0, KEYEVENTF_KEYUP, 0)


@app.get("/health")
def health():
    oc = oc_status()
    try:
        re_ = requests.get(f"{CFG['esp']}/", timeout=2)
        esp = {"reachable": re_.ok}
    except Exception as e:
        esp = {"reachable": False, "error": type(e).__name__}
    return jsonify({"ok": True, "bridge": BRIDGE_VERSION, "opencode": oc,
                    "esp": esp, "todo_exists": os.path.isfile(CFG["todo"])})


# --- OpenCode via the local CLI (Windows service, no password) ---
def find_occli():
    """Locate the Desktop app's opencode-cli.exe (the version folder changes with updates)."""
    base = os.path.expandvars(r"%APPDATA%\ai.opencode.desktop\cli")
    cands = glob.glob(os.path.join(base, "*", "opencode-cli.exe"))
    if not cands:
        return ""
    return sorted(cands)[-1]


def oc_cli(*args, timeout=20):
    """Run opencode-cli and return (rc, stdout). Raises TimeoutExpired."""
    cli = CFG["opencode_cli"] or find_occli()
    if not cli or not os.path.isfile(cli):
        raise FileNotFoundError("opencode-cli not found")
    p = subprocess.run([cli, *args], capture_output=True, timeout=timeout)
    raw = p.stdout
    try:
        # list = utf-8, export = utf-16 with BOM (depends on the subcommand)
        text = raw.decode("utf-8-sig")
        if "\x00" in text:
            raise UnicodeDecodeError("utf-8", raw, 0, 1, "null bytes")
    except UnicodeDecodeError:
        text = raw.decode("utf-16", errors="replace")
    return p.returncode, text


def oc_status():
    try:
        rc, out = oc_cli("session", "list", timeout=15)
        if rc != 0:
            return {"reachable": False, "error": f"rc={rc}"}
        n = len([l for l in out.splitlines() if l.startswith("ses_")])
        return {"reachable": True, "sessions": n}
    except Exception as e:
        return {"reachable": False, "error": type(e).__name__}


def oc_list_sessions():
    """[(session, title)] most recent first (the CLI already sorts that way)."""
    rc, out = oc_cli("session", "list", timeout=15)
    if rc != 0:
        raise RuntimeError(f"list rc={rc}")
    items = []
    for line in out.splitlines():
        parts = line.split("\t")
        if parts and parts[0].startswith("ses_"):
            items.append((parts[0], parts[1] if len(parts) > 1 else ""))
    return items


SESSION_PREFER = ("kt1", "deskbuddy", "puente")   # title of the ESP's dedicated session


def oc_resolve(sid):
    """Resolve the real ID in a stable way, so the ESP's secrets.h never goes stale:
      1) if 'sid' exists and is not 'default' -> it is used as is
      2) otherwise, the most recent session whose TITLE contains kt1/deskbuddy/puente
         (dedicated session; avoids hijacking active work sessions)
      3) if there is no dedicated one -> the most recent session"""
    sessions = oc_list_sessions()          # (id, title) most recent first
    if not sessions:
        raise RuntimeError("no sessions")
    ids = [s for s, _ in sessions]
    if sid and sid != "default" and sid in ids:
        return sid
    for s, title in sessions:
        tl = (title or "").lower()
        if any(k in tl for k in SESSION_PREFER):
            return s
    return ids[0]


def oc_export_texts(sid):
    """Export the session and return [(role, text)] (role=user|assistant)."""
    sid = oc_resolve(sid)
    rc, out = oc_cli("session", "export", sid, timeout=25)
    if rc != 0 or not out.strip().startswith("{"):
        raise RuntimeError(f"export rc={rc}")
    d = json.loads(out)
    res = []
    for m in d.get("messages", []):
        t = (m.get("text") or "").replace("\n", " ").strip()
        if m.get("type") in ("user", "assistant") and t:
            res.append((m["type"], t))
    return res


# --- screen: what is visible on the PC (for the chatbot's SCREEN tool) ---
def fg_info():
    try:
        import win32gui
        import win32process
        import psutil
    except ImportError:
        return {"app": "", "title": "", "idle": -1}
    try:
        h = win32gui.GetForegroundWindow()
        title = (win32gui.GetWindowText(h) or "")[:80] if h else ""
        try:
            _, pid = win32process.GetWindowThreadProcessId(h)
            exe = psutil.Process(pid).name() if pid else ""
        except Exception:
            exe = ""
    except Exception:
        title, exe = "", ""
    return {"app": exe, "title": title, "idle": idle_seconds()}


class _LASTINPUTINFO(ctypes.Structure):
    _fields_ = [("cbSize", ctypes.c_uint), ("dwTime", ctypes.c_uint)]


def idle_seconds():
    try:
        i = _LASTINPUTINFO()
        i.cbSize = ctypes.sizeof(i)
        if not ctypes.windll.user32.GetLastInputInfo(ctypes.byref(i)):
            return -1
        return int(((ctypes.windll.kernel32.GetTickCount() - i.dwTime) & 0xFFFFFFFF) / 1000)
    except Exception:
        return -1


@app.get("/screen")
def screen():
    """SCREEN tool: active window + idle time, ready to be summarized by voice."""
    d = fg_info()
    return jsonify({"ok": True, **d})


_SHOT = {"data": b"", "ts": 0.0}


@app.get("/screen/shot")
def shot():
    """Real vision: 320x240 JPEG of the screen for Gemini (inline_data)."""
    import time as _t
    if _t.time() - _SHOT["ts"] < 3 and _SHOT["data"]:
        return (_SHOT["data"], 200, {"Content-Type": "image/jpeg"})
    try:
        import io
        import mss
        from PIL import Image
        with mss.mss() as s:
            raw = s.grab(s.monitors[1])
        im = Image.frombytes("RGB", raw.size, raw.bgra, "raw", "BGRX")
        im.thumbnail((320, 240))
        buf = io.BytesIO()
        im.save(buf, "JPEG", quality=60)
        _SHOT["data"], _SHOT["ts"] = buf.getvalue(), _t.time()
        return (_SHOT["data"], 200, {"Content-Type": "image/jpeg"})
    except Exception as e:
        return jsonify({"ok": False, "error": type(e).__name__}), 501


@app.post("/media/mute-toggle")
def mute_toggle():
    tap_vk(VK_VOLUME_MUTE)
    _audit("media.mute-toggle")
    return jsonify({"ok": True})


@app.post("/media/playpause")
def playpause():
    tap_vk(VK_MEDIA_PLAY_PAUSE)
    _audit("media.playpause")
    return jsonify({"ok": True})


@app.post("/media/play")
def play_url():
    """PLAY tool: plays a video/URL in the PC's browser."""
    body = request.get_json(force=True, silent=True) or {}
    url = str(body.get("url", "") or body.get("query", "")).strip()
    if not url:
        return jsonify({"ok": False, "error": "url|query required"}), 400
    if not url.startswith(("http://", "https://")):
        # free text -> YouTube search
        from urllib.parse import quote_plus
        url = f"https://www.youtube.com/results?search_query={quote_plus(url)}"
    try:
        webbrowser.open(url)
    except Exception as e:
        return jsonify({"ok": False, "error": type(e).__name__}), 500
    _audit("media.play", url[:120])
    return jsonify({"ok": True, "opened": url[:120]})


def _speaker():
    try:  # COM per thread (Flask threaded): without this, intermittent OSError
        import comtypes
        comtypes.CoInitialize()
    except Exception:
        pass
    from pycaw.pycaw import AudioUtilities
    return AudioUtilities.GetSpeakers()


@app.get("/media/volume")
def vol_get():
    try:
        d = _speaker()
        return jsonify({"ok": True, "level": round(d.volume_percent),
                        "muted": bool(d.EndpointVolume.GetMute())})
    except Exception as e:
        return jsonify({"ok": False, "error": type(e).__name__}), 501


@app.post("/media/volume")
def vol_set():
    body = request.get_json(force=True, silent=True) or {}
    try:
        level = int(body.get("level", -1))
    except (TypeError, ValueError):
        return jsonify({"ok": False, "error": "level 0-100 required"}), 400
    if not 0 <= level <= 100:
        return jsonify({"ok": False, "error": "level 0-100 required"}), 400
    try:
        _speaker().volume_percent = level
    except Exception as e:
        return jsonify({"ok": False, "error": type(e).__name__}), 501
    _audit("media.volume", level)
    return jsonify({"ok": True, "level": level})


@app.post("/os/lock")
def lock():
    ctypes.windll.user32.LockWorkStation()
    _audit("os.lock")
    return jsonify({"ok": True})


@app.post("/os/sleep")
def sleep():
    ctypes.windll.powrprof.SetSuspendState(False, True, False)
    _audit("os.sleep")
    return jsonify({"ok": True})


# Closed allowlist: spoken name (English + Spanish aliases) -> fixed launch target.
# Anything not listed here is rejected; arbitrary paths/URLs are never launched.
APP_MAP = {
    # English
    "notepad": "notepad.exe", "calculator": "calc.exe", "calc": "calc.exe",
    "paint": "mspaint.exe", "terminal": "wt.exe", "cmd": "cmd.exe",
    "command prompt": "cmd.exe", "console": "cmd.exe", "explorer": "explorer.exe",
    "file explorer": "explorer.exe", "files": "explorer.exe", "code": "code",
    "vscode": "code", "vs code": "code", "visual studio code": "code",
    "chrome": "chrome.exe", "google chrome": "chrome.exe", "edge": "msedge.exe",
    "microsoft edge": "msedge.exe", "firefox": "firefox.exe", "discord": "discord",
    "teams": "msteams", "telegram": "telegram", "whatsapp": "whatsapp",
    "spotify": "spotify", "word": "winword.exe", "excel": "excel.exe",
    "snipping tool": "snippingtool.exe", "snip": "snippingtool.exe",
    # Spanish
    "bloc": "notepad.exe", "bloc de notas": "notepad.exe", "calculadora": "calc.exe",
    "consola": "cmd.exe", "explorador": "explorer.exe", "archivos": "explorer.exe",
    "codigo": "code", "código": "code", "tijeras": "snippingtool.exe",
    "recortes": "snippingtool.exe",
}


@app.post("/os/open")
def os_open():
    """OPEN tool: launches an app by name from the closed APP_MAP allowlist."""
    body = request.get_json(force=True, silent=True) or {}
    name = str(body.get("app", "")).strip()
    if not name:
        return jsonify({"ok": False, "error": "app required"}), 400
    low = " ".join(name.lower().strip(" .,;:!?\"'").split())
    target = APP_MAP.get(low)
    if not target:
        _audit("os.open.denied", name[:60])
        return jsonify({"ok": False, "error": "app not allowed",
                        "message": T(f"I'm not allowed to open '{name[:40]}'",
                                     f"no tengo permiso para abrir '{name[:40]}'")}), 400
    try:
        os.startfile(target)  # noqa: S606 (fixed allowlist target)
    except Exception:
        _audit("os.open.fail", name[:60])
        return jsonify({"ok": False, "error": T(f"I can't open '{name[:40]}'",
                                                f"no puedo abrir '{name[:40]}'")}), 400
    _audit("os.open", name[:60])
    return jsonify({"ok": True, "opened": name[:60]})


def dnd_state():
    import winreg
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER,
                            r"Software\Microsoft\Windows\CurrentVersion\PushNotifications") as k:
            v, _ = winreg.QueryValueEx(k, "ToastEnabled")
            return v == 0
    except OSError:
        return False


def dnd_apply(on):
    import winreg
    with winreg.CreateKey(winreg.HKEY_CURRENT_USER,
                          r"Software\Microsoft\Windows\CurrentVersion\PushNotifications") as k:
        winreg.SetValueEx(k, "ToastEnabled", 0, winreg.REG_DWORD, 0 if on else 1)
    try:  # notify apps of the change without a restart
        ctypes.windll.user32.SendMessageTimeoutW(0xFFFF, 0x001A, 0, "Policy", 0x0002, 200, None)
    except Exception:
        pass
    return on


@app.get("/os/dnd")
def dnd_get():
    return jsonify({"ok": True, "dnd": dnd_state()})


@app.post("/os/dnd")
def dnd():
    """Real Do-Not-Disturb: no body toggles; {"on":bool} sets the state."""
    body = request.get_json(force=True, silent=True) or {}
    want = body.get("on", None)
    try:
        on = dnd_apply(not dnd_state() if want is None else bool(want))
    except Exception as e:
        return jsonify({"ok": False, "error": type(e).__name__}), 500
    _audit("os.dnd", on)
    return jsonify({"ok": True, "dnd": on})


@app.post("/todo")
def todo():
    text = (request.get_json(force=True, silent=True) or {}).get("text", "").strip()
    if not text:
        return jsonify({"ok": False, "error": "empty text"}), 400
    if not CFG["todo"]:
        return jsonify({"ok": False, "error": "todo path not configured"}), 500
    os.makedirs(os.path.dirname(CFG["todo"]), exist_ok=True)
    ts = datetime.datetime.now().strftime("%Y-%m-%d %H:%M")
    with open(CFG["todo"], "a", encoding="utf-8") as f:
        f.write(f"- [{ts}] {text}\n")
    _audit("todo", text[:100])
    return jsonify({"ok": True})


@app.get("/opencode/sessions")
def oc_sessions():
    try:
        rc, out = oc_cli("session", "list", timeout=15)
    except Exception as e:
        return jsonify({"ok": False, "error": type(e).__name__}), 502
    if rc != 0:
        return jsonify({"ok": False, "error": f"rc={rc}"}), 502
    items = []
    for line in out.splitlines():
        parts = line.split("\t")
        if parts and parts[0].startswith("ses_"):
            items.append({"session": parts[0],
                          "title": parts[1] if len(parts) > 1 else ""})
    return jsonify(items)


def _prompt_bg(sid, text):
    try:
        oc_cli("run", "-s", sid, text, timeout=600)
    except Exception as e:
        print(f"[bridge] background prompt failed: {type(e).__name__}", flush=True)


@app.post("/opencode/prompt")
def oc_prompt():
    body = request.get_json(force=True, silent=True) or {}
    sid, text = body.get("session", ""), body.get("text", "")
    if not text:
        return jsonify({"ok": False, "error": "text required"}), 400
    try:
        sid = oc_resolve(sid)   # 'default'/dead -> most recent
    except Exception as e:
        return jsonify({"ok": False, "error": type(e).__name__}), 502
    # `run` blocks until the agent finishes: launch it in the background and answer 202.
    threading.Thread(target=_prompt_bg, args=(sid, text), daemon=True).start()
    _audit("opencode.prompt", f"{sid} {text[:80]}")
    return jsonify({"ok": True, "status": 202, "queued": True, "session": sid})


@app.get("/opencode/messages")
def oc_messages():
    sid = request.args.get("session", "")   # empty/'default' -> most recent
    try:
        limit = max(1, min(int(request.args.get("limit", "5")), 10))
    except ValueError:
        limit = 5
    try:
        msgs = oc_export_texts(sid)
    except Exception as e:
        return jsonify({"ok": False, "error": type(e).__name__}), 502
    return jsonify([{"role": r, "text": t[-400:]} for r, t in msgs[-limit:]])


@app.get("/opencode/resume")
def oc_resume():
    # Extractive (<400 chars for the 320x240 display).
    sid = request.args.get("session", "")   # empty/'default' -> most recent
    try:
        msgs = oc_export_texts(sid)
    except Exception as e:
        return jsonify({"ok": False, "error": type(e).__name__}), 502
    tail = (" | ".join(t for _, t in msgs[-4:]) if msgs
            else T("(no messages)", "(sin mensajes)"))
    return jsonify({"resume": tail[-400:]})


# --- memory + briefing: what the chatbot remembers between turns ---
def _mem_load():
    try:
        d = json.load(open(MEM_FILE, encoding="utf-8"))
        return d if isinstance(d, list) else []
    except Exception:
        return []


def _mem_save(facts):
    try:
        json.dump(facts[-100:], open(MEM_FILE, "w", encoding="utf-8"),
                  ensure_ascii=False, indent=1)
    except Exception:
        pass


@app.get("/memory")
def mem_list():
    return jsonify([{"i": i, "text": t} for i, t in enumerate(_mem_load())])


@app.post("/memory")
def mem_add():
    body = request.get_json(force=True, silent=True) or {}
    text = str(body.get("text", "")).strip()[:200]
    if not text:
        return jsonify({"ok": False, "error": "text required"}), 400
    facts = _mem_load()
    if text not in facts:
        facts.append(text)
        _mem_save(facts)
    _audit("memory.add", text[:100])
    return jsonify({"ok": True, "facts": len(facts)})


@app.delete("/memory/<int:i>")
def mem_del(i):
    facts = _mem_load()
    if not 0 <= i < len(facts):
        return jsonify({"ok": False, "error": "not found"}), 404
    facts.pop(i)
    _mem_save(facts)
    _audit("memory.del", i)
    return jsonify({"ok": True})


@app.get("/briefing")
def briefing():
    """Spoken briefing (<=380 chars, ready for TTS)."""
    parts = []
    pend = sorted(REMINDERS.values(), key=lambda v: v["due"])
    if pend:
        nxt = max(0, int(pend[0]["due"] - time.time()))
        txt = pend[0]["text"][:60]
        parts.append(T(f"{len(pend)} reminder(s), next '{txt}' in {nxt // 60} min",
                       f"{len(pend)} aviso(s), proximo '{txt}' en {nxt // 60} min"))
    else:
        parts.append(T("no reminders", "sin avisos"))
    if CFG["todo"] and os.path.isfile(CFG["todo"]):
        try:
            lines = open(CFG["todo"], encoding="utf-8").read().splitlines()
            if lines:
                parts.append("TODO: " + lines[-1][-100:])
        except Exception:
            pass
    facts = _mem_load()
    if facts:
        parts.append(T(f"you remember {len(facts)} thing(s): ",
                       f"recuerdas {len(facts)} cosa(s): ") + facts[-1][-100:])
    fg = fg_info()
    if fg.get("app"):
        parts.append(T(f"on screen {fg['app'][:30]}", f"en pantalla {fg['app'][:30]}"))
    out = T("Briefing: ", "Parte: ") + ". ".join(parts)
    return jsonify({"brief": out[:380]})


# --- reminders: Windows toast + notice to the ESP (greet) + audit ---
REMINDERS = {}   # id -> {"text", "due", "lang", "timer"}


def _rem_save():
    if not CFG["config"]:
        return
    try:
        p = CFG["config"]
        d = json.load(open(p, encoding="utf-8")) if os.path.isfile(p) else {}
        d["reminders"] = [{"id": i, "text": v["text"], "due": v["due"],
                           "lang": v.get("lang", "en")}
                          for i, v in REMINDERS.items()]
        json.dump(d, open(p, "w", encoding="utf-8"), indent=2)
    except Exception:
        pass


_CTRL_CHARS = re.compile(r"[\x00-\x1f\x7f-\x9f]")

# The script never contains user text: title/body arrive via environment
# variables and are XML-escaped inside PowerShell before building the toast.
_TOAST_PS = (
    "$ErrorActionPreference='Stop';"
    "[void][Windows.UI.Notifications.ToastNotificationManager,"
    "Windows.UI.Notifications,ContentType=WindowsRuntime];"
    "[void][Windows.Data.Xml.Dom.XmlDocument,"
    "Windows.Data.Xml.Dom.XmlDocument,ContentType=WindowsRuntime];"
    "$t=[System.Security.SecurityElement]::Escape($env:KT1_TOAST_TITLE);"
    "$m=[System.Security.SecurityElement]::Escape($env:KT1_TOAST_TEXT);"
    "$x=New-Object Windows.Data.Xml.Dom.XmlDocument;"
    "$x.LoadXml(\"<toast><visual><binding template='ToastGeneric'>"
    "<text>$t</text><text>$m</text></binding></visual></toast>\");"
    "$id='{1AC14E77-02E7-4E5D-B744-2EB1AE5198B7}\\WindowsPowerShell\\v1.0\\powershell.exe';"
    "[Windows.UI.Notifications.ToastNotificationManager]::CreateToastNotifier($id)"
    ".Show([Windows.UI.Notifications.ToastNotification]::new($x))"
)


def _clean(s, n):
    return _CTRL_CHARS.sub(" ", str(s or "")).strip()[:n]


def _win_toast(title, msg):
    env = {**os.environ,
           "KT1_TOAST_TITLE": _clean(title, 60),
           "KT1_TOAST_TEXT": _clean(msg, 200)}
    enc = base64.b64encode(_TOAST_PS.encode("utf-16-le")).decode("ascii")
    try:
        subprocess.run(["powershell", "-NoProfile", "-NonInteractive",
                        "-EncodedCommand", enc],
                       env=env, capture_output=True, timeout=20,
                       creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    except Exception:
        pass


def _rem_fire(rid):
    item = REMINDERS.pop(rid, None)
    if not item:
        return
    _rem_save()
    text = item["text"]
    lang = item.get("lang", "en")
    _audit("remind.fire", text[:100])
    _win_toast(T("KT1 reminder", "Recordatorio de KT1", lang), text[:120])
    try:  # the ESP greets; the text also goes to the audit log + toast
        summary = T("Reminder: ", "Aviso: ", lang) + text
        requests.post(f"{CFG['esp']}/pet",
                      json={"talk": True, "summary": summary[:110]}, timeout=4)
    except Exception:
        pass


def _rem_arm(rid, text, due, lang="en"):
    delay = max(1.0, due - time.time())
    t = threading.Timer(delay, _rem_fire, args=(rid,))
    t.daemon = True
    t.start()
    REMINDERS[rid] = {"text": text, "due": due,
                      "lang": "es" if str(lang).lower().startswith("es") else "en",
                      "timer": t}
    _rem_save()


def _rem_boot():
    if not CFG["config"] or not os.path.isfile(CFG["config"]):
        return
    try:
        saved = json.load(open(CFG["config"], encoding="utf-8")).get("reminders", [])
    except Exception:
        return
    now = time.time()
    for r in saved:
        if r.get("due", 0) <= now - 3600:
            continue  # expired more than 1 h ago: forget it
        _rem_arm(r.get("id") or secrets.token_hex(4)[:8],
                 r.get("text", ""), min(r["due"], now + 1) if r["due"] <= now else r["due"],
                 r.get("lang") or "en")


@app.post("/remind")
def remind():
    """REMIND tool: {"text","seconds" 10..86400} -> toast + notice to the ESP."""
    body = request.get_json(force=True, silent=True) or {}
    text = str(body.get("text", "")).strip()[:140]
    try:
        secs = int(body.get("seconds", 0))
    except (TypeError, ValueError):
        secs = 0
    if not text or not 10 <= secs <= 86400:
        return jsonify({"ok": False, "error": "text + seconds(10-86400) required"}), 400
    rid = secrets.token_hex(4)[:8]
    _rem_arm(rid, text, time.time() + secs, _lang())
    _audit("remind.set", f"{secs}s {text[:80]}")
    return jsonify({"ok": True, "id": rid, "in": secs})


@app.get("/reminders")
def reminders():
    items = [{"id": i, "text": v["text"], "in": max(0, int(v["due"] - time.time()))}
             for i, v in sorted(REMINDERS.items(), key=lambda kv: kv[1]["due"])]
    return jsonify(items)


@app.delete("/reminders/<rid>")
def reminder_del(rid):
    item = REMINDERS.pop(rid, None)
    if not item:
        return jsonify({"ok": False, "error": "not found"}), 404
    try:
        item["timer"].cancel()
    except Exception:
        pass
    _rem_save()
    _audit("remind.del", rid)
    return jsonify({"ok": True})


# --- OTA: the ESP downloads the .bin from here (see FW_VERSION in the .ino) ---
def _fw_info():
    if CFG["config"] and os.path.isfile(CFG["config"]):
        try:
            return json.load(open(CFG["config"], encoding="utf-8")).get("firmware", {})
        except Exception:
            pass
    return {}


@app.get("/firmware/version")
def fw_version():
    fw = _fw_info()
    if not fw.get("version") or not fw.get("file"):
        return jsonify({"ok": False, "error": T("no firmware registered",
                                                "sin firmware registrado")}), 404
    return jsonify({"ok": True, "version": fw["version"],
                    "url": f"http://{request.host}/firmware/kt1.bin"})


@app.get("/firmware/kt1.bin")
def fw_bin():
  return _serve_fw_bin()


@app.get("/firmware/deskbuddy.bin")
def fw_bin_legacy():
  return _serve_fw_bin()  # legacy alias (pre-rename firmware name)


def _serve_fw_bin():
    fw = _fw_info()
    path = fw.get("file", "")
    if not path or not os.path.isfile(path):
        return jsonify({"ok": False, "error": T("bin not found", "bin no encontrado")}), 404
    _audit("firmware.download", os.path.basename(path))
    with open(path, "rb") as f:
        data = f.read()
    return (data, 200, {"Content-Type": "application/octet-stream"})


# --- pairing: if there is no token, the first caller sets it ---
PAIR_WINDOW_S = 900   # 15 min since startup; after that the bridge must be restarted
@app.post("/pair")
def pair():
    if CFG["token"]:
        return jsonify({"ok": False, "error": T("already paired", "ya emparejado")}), 403
    if time.time() - BOOT_TS > PAIR_WINDOW_S:
        return jsonify({"ok": False,
                        "error": T("pairing window closed: restart the bridge",
                                   "ventana cerrada: reinicia el bridge")}), 403
    tok = secrets.token_hex(16)
    CFG["token"] = tok
    if CFG["config"]:
        try:
            p = CFG["config"]
            d = json.load(open(p, encoding="utf-8")) if os.path.isfile(p) else {}
            d["token"] = tok
            json.dump(d, open(p, "w", encoding="utf-8"), indent=2)
        except Exception:
            pass
    _audit("pair", "new pairing")
    return jsonify({"ok": True, "token": tok})


@app.post("/build")
def build_mirror():
    """Build/CI relay -> POST /pet to the ESP.
    Usage: POST /build {"build":"busy|ok|fail","summary":"short text"}"""
    body = request.get_json(force=True, silent=True) or {}
    build = str(body.get("build", "")).strip().lower()
    summary = str(body.get("summary", ""))[:110]
    if build not in ("busy", "ok", "fail"):
        return jsonify({"ok": False, "error": "build must be busy|ok|fail"}), 400
    try:
        r = requests.post(f"{CFG['esp']}/pet",
                          json={"build": build, "summary": summary}, timeout=4)
        _audit("build", f"{build} {summary[:80]}")
        return jsonify({"ok": r.ok, "esp_status": r.status_code})
    except Exception as e:
        return jsonify({"ok": False, "error": type(e).__name__}), 502


def main():
    ap = argparse.ArgumentParser(description="KT1 <-> Windows <-> OpenCode bridge")
    ap.add_argument("--port", type=int, default=8750, help="listening port (default 8750)")
    ap.add_argument("--opencode", default="",
                    help="v3.1 compatibility (ignored: the local opencode-cli is used)")
    ap.add_argument("--opencode-cli", default="",
                    help="path to opencode-cli.exe (auto-detected if omitted)")
    ap.add_argument("--todo", default="", help="TODO.md file to append notes to")
    ap.add_argument("--esp", default="http://kt1.local", help="base URL of the ESP")
    ap.add_argument("--token", default="",
                    help="token shared with the ESP (BRIDGE_TOKEN). Empty = no auth")
    ap.add_argument("--config", default="",
                    help="setup.json (persists token and reminders)")
    args = ap.parse_args()
    if args.config:
        CFG["config"] = os.path.abspath(args.config)
        try:
            saved = json.load(open(CFG["config"], encoding="utf-8"))
            if not args.todo:
                CFG["todo"] = saved.get("todo", "")
            if not args.token:
                CFG["token"] = saved.get("token", "")
            if args.esp == "http://kt1.local":
                CFG["esp"] = saved.get("esp", CFG["esp"])
        except Exception:
            pass
    CFG["todo"] = args.todo or CFG["todo"]
    if args.esp != "http://kt1.local" or not CFG["esp"]:
        CFG["esp"] = args.esp.rstrip("/")
    CFG["opencode_cli"] = args.opencode_cli or find_occli()
    CFG["token"] = args.token or CFG["token"]
    _rem_boot()
    print(f"[bridge v{BRIDGE_VERSION}] Auth -> {'token ON' if CFG['token'] else 'OFF (open LAN)'}", flush=True)
    print(f"[bridge v{BRIDGE_VERSION}] OpenCode CLI -> {CFG['opencode_cli'] or '(not found)'}", flush=True)
    print(f"[bridge v{BRIDGE_VERSION}] TODO     -> {CFG['todo'] or '(not configured)'}", flush=True)
    print(f"[bridge v{BRIDGE_VERSION}] ESP      -> {CFG['esp']}", flush=True)
    print(f"[bridge v{BRIDGE_VERSION}] Listening on http://0.0.0.0:{args.port} (threaded)", flush=True)
    # threaded=True: the ESP chains health/resume/prompt without blocking.
    app.run(host="0.0.0.0", port=args.port, threaded=True)


if __name__ == "__main__":
    sys.exit(main())
