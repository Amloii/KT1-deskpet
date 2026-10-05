# -*- coding: utf-8 -*-
"""
setup.py - Sets up a new PC for KT1 with a single command.

    python setup.py [--yes] [--dry-run] [--no-shortcuts] [--no-firewall]

Steps, in order:
  1. Checks Windows + Python 3.9+ and installs requirements.txt
  2. Locates opencode-cli and checks `session list`
  3. Asks for (or reuses) the config: ESP, port, TODO, token -> setup.json
  4. Patches the firmware's secrets.h (PC_HOST/BRIDGE_PORT/BRIDGE_TOKEN).
     If secrets.h does not exist it is created from secrets.example.h;
     if it exists, a secrets.h.bak backup is written next to it
     (both are git-ignored by the `secrets.h*` rule).
  5. Creates a shell:startup shortcut (watchdog -> bridge + pet_agent, pythonw)
  6. Opens the firewall for the bridge port (asks for admin if missing)
  7. Registers the newest OTA firmware found in pc-agent/firmware/
  8. Tests: resolves the ESP, pings it over HTTP, starts a test bridge,
     checks /health, correct auth (200) and missing token (401), then stops it.

Idempotent: it can be re-run without breaking anything.
"""
import argparse
import ctypes
import glob
import json
import os
import secrets
import shutil
import socket
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
CONF_FILE = os.path.join(HERE, "setup.json")
FW_DIR = os.path.normpath(os.path.join(HERE, "..", "firmware", "kt1-deskpet"))
SECRETS = os.path.join(FW_DIR, "secrets.h")
SECRETS_EXAMPLE = os.path.join(FW_DIR, "secrets.example.h")

PY = sys.executable


def log(msg):
    print(msg, flush=True)


def fail(msg):
    log(f"[X] {msg}")
    sys.exit(1)


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


def is_admin():
    try:
        return bool(ctypes.windll.shell32.IsUserAnAdmin())
    except Exception:
        return False


def local_ip():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return ""


def find_occli():
    base = os.path.expandvars(r"%APPDATA%\ai.opencode.desktop\cli")
    cands = glob.glob(os.path.join(base, "*", "opencode-cli.exe"))
    return sorted(cands)[-1] if cands else ""


def ask(prompt, default, auto):
    if auto:
        log(f"    {prompt} [{default}]")
        return default
    try:
        v = input(f"    {prompt} [{default}]: ").strip()
    except EOFError:
        v = ""
    return v or default


# --- steps ---------------------------------------------------------------
def step_env(args):
    log("[1/8] Environment")
    if os.name != "nt":
        fail("Windows only")
    if sys.version_info < (3, 9):
        fail(f"Python {sys.version} < 3.9")
    log(f"    Python {sys.version.split()[0]} OK")
    if args.dry_run:
        log("    (dry-run: not installing dependencies)")
        return
    r = run([PY, "-m", "pip", "install", "-r", os.path.join(HERE, "requirements.txt")])
    if r.returncode != 0:
        fail(f"pip install failed:\n{r.stdout[-1500:]}")
    log("    deps OK (flask, requests, pywin32, psutil, ...)")
    for mod in ("flask", "requests", "win32gui", "psutil"):
        r = run([PY, "-c", f"import {mod}"])
        if r.returncode != 0:
            fail(f"missing module {mod}")
    log("    imports OK")


def step_cli(args):
    log("[2/8] OpenCode CLI")
    cli = find_occli()
    if not cli:
        fail("opencode-cli.exe not found (install OpenCode Desktop)")
    log(f"    CLI: {cli}")
    try:
        p = subprocess.run([cli, "session", "list"], capture_output=True, timeout=25)
    except Exception as e:
        fail(f"session list: {type(e).__name__}")
    if p.returncode != 0:
        fail("session list rc!=0 (is the OpenCode service down?)")
    log("    session list OK")
    return cli


def step_config(args):
    log("[3/8] Configuration")
    cfg = {}
    if os.path.isfile(CONF_FILE):
        cfg = json.load(open(CONF_FILE, encoding="utf-8"))
        log(f"    reusing {CONF_FILE}")
    cfg.setdefault("esp", "http://kt1.local")
    cfg.setdefault("port", 8750)
    cfg.setdefault("todo", "")
    if not cfg.get("token"):
        cfg["token"] = secrets.token_hex(16)
        log("    new token generated")
    if not args.yes:
        cfg["esp"] = ask("ESP (host/IP)", cfg["esp"], False)
        cfg["port"] = int(ask("Bridge port", str(cfg["port"]), False))
        cfg["todo"] = ask("TODO.md (empty = no TODO)", cfg["todo"], False)
        regen = ask("Regenerate token? (y/N)", "N", False).lower() in ("y", "s")
        if regen:
            cfg["token"] = secrets.token_hex(16)
    if not cfg.get("token"):
        log("    no token: the bridge starts OPEN; tell KT1 to 'pair the PC'")
    cfg["pc_ip"] = local_ip()
    cfg["opencode_cli"] = find_occli()
    if not args.dry_run:
        json.dump(cfg, open(CONF_FILE, "w", encoding="utf-8"), indent=2)
        log(f"    saved {CONF_FILE}")
    else:
        log("    (dry-run: not saving setup.json)")
    return cfg


def step_secrets(cfg, args):
    log("[4/8] Firmware secrets.h")
    created = False
    if not os.path.isfile(SECRETS):
        if not os.path.isfile(SECRETS_EXAMPLE):
            log(f"    neither secrets.h nor secrets.example.h found in {FW_DIR}")
            log("    create secrets.h by hand; PC_HOST/BRIDGE_PORT/BRIDGE_TOKEN "
                "are stored in setup.json (keys pc_ip, port, token)")
            return
        if args.dry_run:
            log("    (dry-run: would create secrets.h from secrets.example.h)")
            return
        shutil.copy2(SECRETS_EXAMPLE, SECRETS)
        created = True
        log("    secrets.h created from secrets.example.h")
    elif args.dry_run:
        log("    (dry-run: not touching secrets.h)")
        return
    else:
        # The backup stays git-ignored thanks to the `secrets.h*` rule.
        shutil.copy2(SECRETS, SECRETS + ".bak")
    lines = open(SECRETS, encoding="utf-8").read().splitlines()
    want = {"BRIDGE_PORT": str(cfg["port"]),
            "BRIDGE_TOKEN": f'"{cfg["token"]}"'}
    if cfg.get("pc_ip"):
        want["PC_HOST"] = f'"{cfg["pc_ip"]}"'
    else:
        log("    warning: could not detect this PC's LAN IP; set PC_HOST by hand")
    seen = set()
    out = []
    for ln in lines:
        done = False
        for k, v in want.items():
            parts = ln.split()
            if len(parts) >= 2 and parts[0] == "#define" and parts[1] == k:
                indent = ln[:len(ln) - len(ln.lstrip())]
                cpos = ln.find("//")
                comment = f"  {ln[cpos:]}" if cpos >= 0 else ""
                out.append(f"{indent}#define {k:<17} {v}{comment}")
                seen.add(k)
                done = True
                break
        if not done:
            out.append(ln)
    missing = [k for k in want if k not in seen]
    if missing:
        out.append("")
        for k in missing:
            out.append(f"#define {k:<17} {want[k]}")
    open(SECRETS, "w", encoding="utf-8").write("\n".join(out) + "\n")
    note = "new file" if created else "backup in secrets.h.bak"
    log(f"    patched PC_HOST/BRIDGE_PORT/BRIDGE_TOKEN ({note}). "
        f"Added: {missing or 'none'}")
    todo = [ln.split()[1] for ln in out
            if ln.startswith("#define") and len(ln.split()) >= 3
            and ("YOUR_" in ln or '"0.0000"' in ln)]
    if todo:
        log(f"    fill in the remaining fields in {SECRETS}:")
        log(f"      {', '.join(todo)}  (Wi-Fi, location, Gemini key)")


def _ps_quote(s):
    return str(s).replace("'", "''")


def shortcut(target, args_str, link_path):
    ps = (f"$s=(New-Object -ComObject WScript.Shell).CreateShortcut('{_ps_quote(link_path)}');"
          f"$s.TargetPath='{_ps_quote(target)}';$s.Arguments='{_ps_quote(args_str)}';"
          f"$s.WorkingDirectory='{_ps_quote(HERE)}';$s.Save()")
    r = run(["powershell", "-NoProfile", "-Command", ps])
    return r.returncode == 0


def step_shortcuts(cfg, args):
    log("[5/8] Autostart (shell:startup)")
    if args.no_shortcuts or args.dry_run:
        log("    skipped")
        return
    start = os.path.join(os.path.expandvars("%APPDATA%"),
                         r"Microsoft\Windows\Start Menu\Programs\Startup")
    for old in ("DeskBuddy bridge.lnk", "DeskBuddy agent.lnk", "DeskBuddy watchdog.lnk"):
        try:
            os.remove(os.path.join(start, old))
        except OSError:
            pass
    pyw = os.path.join(os.path.dirname(PY), "pythonw.exe")
    if not os.path.isfile(pyw):
        pyw = PY
    w_args = (f'"{os.path.join(HERE, "watchdog.py")}" '
              f'--config "{os.path.join(HERE, "setup.json")}"')
    ok = shortcut(pyw, w_args, os.path.join(start, "KT1 watchdog.lnk"))
    log(f"    watchdog.lnk: {'OK' if ok else 'FAILED'} (starts + watches bridge and agent)")
    if not ok:
        fail("could not create the shortcut")


def step_firewall(cfg, args):
    log("[6/8] Firewall")
    if args.no_firewall or args.dry_run:
        log("    skipped")
        return
    cmd = ["netsh", "advfirewall", "firewall", "add", "rule",
           f"name=KT1 bridge {cfg['port']}", "dir=in", "action=allow",
           "protocol=TCP", f"localport={cfg['port']}"]
    if not is_admin():
        log("    no admin rights: run this elevated and re-run setup:")
        log("    " + " ".join(cmd))
        return
    r = run(cmd)
    log("    firewall rule OK" if r.returncode == 0 else f"    warning: {r.stdout.strip()[-200:]}")


def step_firmware(cfg, args):
    log("[7/8] Firmware OTA (pc-agent/firmware/kt1_vX.Y.Z.bin)")
    fwdir = os.path.join(HERE, "firmware")
    os.makedirs(fwdir, exist_ok=True)
    import re
    best = None
    for f in glob.glob(os.path.join(fwdir, "*.bin")):
        m = re.search(r"_v(\d+\.\d+\.\d+)\.bin$", os.path.basename(f))
        if m and (best is None or m.group(1) > best[0]):
            best = (m.group(1), f)
    if best and not args.dry_run:
        cfg["firmware"] = {"version": best[0], "file": best[1]}
        json.dump(cfg, open(CONF_FILE, "w", encoding="utf-8"), indent=2)
    log(f"    registered version: {best[0] if best else '(none: copy the .bin exported by the IDE)'}")


def step_tests(cfg, args):
    log("[8/8] Tests")
    host = cfg["esp"].replace("http://", "").rstrip("/")
    try:
        log(f"    ESP resolves: {socket.gethostbyname(host)}")
    except OSError:
        log(f"    warning: {host} does not resolve (ESP off?)")
    try:
        import requests
    except ImportError:
        log("    warning: requests not installed (step 1 skipped with dry-run)")
        return
    try:
        r = requests.get(f"http://{host}/", timeout=4)
        log(f"    ESP HTTP: {r.status_code}")
    except Exception as e:
        log(f"    warning: ESP not responding: {type(e).__name__}")
    # temporary bridge with token
    log("    starting a test bridge with token...")
    bargs = [PY, "-u", os.path.join(HERE, "bridge.py"), "--port", str(cfg["port"] + 1),
             "--todo", cfg["todo"], "--esp", cfg["esp"], "--token", cfg.get("token", "")]
    if os.path.isfile(CONF_FILE):
        bargs += ["--config", CONF_FILE]
    proc = subprocess.Popen(bargs,
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        base = f"http://127.0.0.1:{cfg['port'] + 1}"
        for _ in range(30):
            time.sleep(1)
            try:
                h = requests.get(base + "/health", timeout=3)
                break
            except Exception:
                h = None
        if h is None:
            fail("the test bridge did not start")
        log(f"    /health: bridge {h.json().get('bridge')}")
        if cfg.get("token"):
            r = requests.get(base + "/screen", timeout=6)
            log(f"    /screen without token: {r.status_code} (expected 401)")
            if r.status_code != 401:
                fail("auth is not blocking (check --token)")
            hdr = {"X-Bridge-Token": cfg["token"]}
        else:
            hdr = {}
            log("    no token: pair by voice after flashing (CMD PAIR)")
        r = requests.get(base + "/screen", headers=hdr, timeout=8)
        log(f"    /screen: {r.status_code} {str(r.text)[:100]}")
        if r.status_code != 200:
            fail("/screen failed")
        r = requests.get(base + "/opencode/sessions", headers=hdr, timeout=25)
        log(f"    /opencode/sessions: {r.status_code}")
        r = requests.get(base + "/screen/shot", headers=hdr, timeout=15)
        log(f"    /screen/shot: {r.status_code} {r.headers.get('Content-Type')} "
            f"{len(r.content)} B")
        r = requests.post(base + "/memory", json={"text": "setup test"}, headers=hdr, timeout=8)
        log(f"    /memory: {r.status_code}")
        r = requests.get(base + "/briefing", headers=hdr, timeout=15)
        log(f"    /briefing: {r.status_code} {str(r.text)[:120]}")
        for m in requests.get(base + "/memory", headers=hdr, timeout=8).json():
            if m.get("text") == "setup test":
                requests.delete(base + f"/memory/{m['i']}", headers=hdr, timeout=8)
        log("    ALL OK: bridge working")
    finally:
        proc.terminate()
        # Short grace period so no orphan is left if terminate is slow
        try:
            proc.wait(timeout=8)
        except Exception:
            proc.kill()


def main():
    ap = argparse.ArgumentParser(description="Set up KT1 on a new PC")
    ap.add_argument("--yes", action="store_true",
                    help="no questions (uses setup.json or defaults)")
    ap.add_argument("--dry-run", action="store_true", help="writes nothing, only checks")
    ap.add_argument("--no-shortcuts", action="store_true", help="do not create the autostart shortcut")
    ap.add_argument("--no-firewall", action="store_true", help="do not add the firewall rule")
    args = ap.parse_args()
    log("== KT1 setup ==")
    step_env(args)
    step_cli(args)
    cfg = step_config(args)
    step_secrets(cfg, args)
    step_shortcuts(cfg, args)
    step_firewall(cfg, args)
    step_firmware(cfg, args)
    step_tests(cfg, args)
    log("")
    log("Done. Next:")
    log("  1. Fill in any remaining secrets.h fields, then build/flash the ESP.")
    log("  2. On the ESP: Chat -> speak -> 'what's on screen' / 'play the video of X'.")
    log("  3. Restart the PC: bridge + agent start by themselves.")


if __name__ == "__main__":
    sys.exit(main())
