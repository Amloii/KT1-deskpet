# -*- coding: utf-8 -*-
"""watchdog.py - Starts and watches bridge + pet_agent, relaunching them if they die.

    pythonw watchdog.py [--config setup.json]

Every 60 s it checks the bridge's /health and the agent process.
The shell:startup shortcut points here (a single .lnk).
"""
import json
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
PY = os.path.join(os.path.dirname(sys.executable), "pythonw.exe")
if not os.path.isfile(PY):
    PY = sys.executable
DEVNULL = subprocess.DEVNULL


def load_cfg():
    i = sys.argv.index("--config") + 1 if "--config" in sys.argv else -1
    p = sys.argv[i] if 0 <= i < len(sys.argv) else os.path.join(HERE, "setup.json")
    try:
        return json.load(open(p, encoding="utf-8"))
    except Exception:
        return {}


def bridge_ok(port):
    try:
        import requests
        return requests.get(f"http://127.0.0.1:{port}/health", timeout=5).ok
    except Exception:
        return False


def start_bridge(cfg):
    return subprocess.Popen(
        [PY, "-u", os.path.join(HERE, "bridge.py"),
         "--port", str(cfg.get("port", 8750)),
         "--config", os.path.join(HERE, "setup.json")],
        stdout=DEVNULL, stderr=DEVNULL)


def start_agent(cfg):
    esp = str(cfg.get("esp", "http://kt1.local")).replace("http://", "").rstrip("/")
    return subprocess.Popen([PY, "-u", os.path.join(HERE, "pet_agent.py"), "--host", esp],
                            stdout=DEVNULL, stderr=DEVNULL)


def main():
    cfg = load_cfg()
    bridge, agent = start_bridge(cfg), start_agent(cfg)
    bad = 0
    while True:
        time.sleep(60)
        if bridge_ok(cfg.get("port", 8750)):
            bad = 0
        elif bridge.poll() is not None or (bad := bad + 1) >= 2:
            bad = 0
            try:
                bridge.kill()
            except Exception:
                pass
            bridge = start_bridge(cfg)
        if agent.poll() is not None:
            agent = start_agent(cfg)


if __name__ == "__main__":
    sys.exit(main())
