# -*- coding: utf-8 -*-
"""
calm_window.py - KT1 "Ventana Calma" for Windows.

A small SOS/meditation window for moments of tension, stress or craving:
  - 60 s guided breathing on a big canvas (cyclic sigh: double inhale +
    long exhale, Balban et al. 2023) with phase text and progress bar.
  - "KT1" button: opens the Calm page on the device (POST /pet {"calm":true}).
  - "DND 5 min" button: Windows Do-Not-Disturb on via the bridge, auto off
    after 5 minutes (POST /os/dnd).

Needs bridge.py running (for DND) and the ESP32 reachable (for KT1).
Nothing is recorded or uploaded; errors are shown in the status line.

Usage:
    python calm_window.py [--config setup.json] [--bridge http://127.0.0.1:8750]
                          [--esp http://kt1.local] [--token ...]
                          [--lang es|en] [--seconds 60]

Requires: requests (see requirements.txt). tkinter ships with Python.
"""

import argparse
import json
import os
import sys

try:
    import tkinter as tk
except ImportError:
    print("tkinter is not available in this Python. Reinstall with Tcl/Tk support.")
    sys.exit(1)

try:
    import requests
except ImportError:
    print("Missing dependency: pip install -r requirements.txt")
    sys.exit(1)

DEFAULT_SECONDS = 60
DND_MINUTES = 5
REQ_TIMEOUT = 4.0

T = {
    "en": {
        "title": "KT1 Calm window",
        "start": "Start", "pause": "Pause", "resume": "Resume", "end": "End",
        "kt1": "Open Calm on KT1", "dnd": "DND 5 min",
        "in1": "Breathe in...", "in2": "a sip more", "out": "Let go...",
        "hold": "...", "ready": "SOS breathing. 1 tap = start.",
        "done": "Done. Well done.",
        "paused": "paused",
        "kt1_ok": "Calm opened on KT1", "kt1_fail": "KT1 unreachable",
        "dnd_on": "DND on for 5 min", "dnd_off": "DND off",
        "dnd_fail": "Bridge unreachable",
    },
    "es": {
        "title": "KT1 Ventana Calma",
        "start": "Empezar", "pause": "Pausa", "resume": "Seguir", "end": "Fin",
        "kt1": "Abrir Calma en KT1", "dnd": "DND 5 min",
        "in1": "Inhala...", "in2": "un poco más", "out": "Suelta...",
        "hold": "...", "ready": "SOS respirar. 1 toque = empezar.",
        "done": "Hecho. Bien.",
        "paused": "en pausa",
        "kt1_ok": "Calma abierta en KT1", "kt1_fail": "KT1 inalcanzable",
        "dnd_on": "DND 5 min activado", "dnd_off": "DND desactivado",
        "dnd_fail": "Bridge inalcanzable",
    },
}

BG, CARD, INK, DIM, ACCENT, OK = "#090c14", "#1a2131", "#e8eef4", "#8c99a8", "#22d3ee", "#34d399"


def load_config(path):
    """Defaults from setup.json (same file bridge.py uses). CLI flags win."""
    cfg = {}
    if path and os.path.isfile(path):
        try:
            cfg = json.load(open(path, encoding="utf-8"))
        except (OSError, ValueError):
            pass
    return cfg


class CalmWindow:
    def __init__(self, root, esp, bridge, token, lang, seconds):
        self.esp = esp.rstrip("/")
        self.bridge = bridge.rstrip("/")
        self.token = token
        self.s = T[lang]
        self.total_ms = max(20, seconds) * 1000
        self.t0 = 0
        self.elapsed = 0
        self.running = False
        self.paused = False
        self._job = None

        root.title(self.s["title"])
        root.configure(bg=BG)
        root.resizable(False, False)

        self.canvas = tk.Canvas(root, width=340, height=240, bg=BG, highlightthickness=0)
        self.canvas.pack(padx=10, pady=(10, 0))
        self.ball = self.canvas.create_oval(0, 0, 0, 0, fill=ACCENT, outline=ACCENT)
        self.ring = self.canvas.create_oval(0, 0, 0, 0, outline=DIM)
        self.phase = self.canvas.create_text(170, 196, text=self.s["ready"],
                                             fill=INK, font=("Segoe UI", 13))
        self.sub = self.canvas.create_text(170, 220, text="",
                                            fill=DIM, font=("Segoe UI", 10))

        bar = tk.Frame(root, bg=BG)
        bar.pack(padx=10, pady=8, fill="x")
        self.b_start = tk.Button(bar, text=self.s["start"], width=10, command=self.toggle)
        self.b_start.pack(side="left", padx=2)
        self.b_end = tk.Button(bar, text=self.s["end"], width=10, command=self.stop)
        self.b_end.pack(side="left", padx=2)
        self.b_kt1 = tk.Button(bar, text=self.s["kt1"], command=self.open_kt1)
        self.b_kt1.pack(side="left", padx=2)
        self.b_dnd = tk.Button(bar, text=self.s["dnd"], command=self.dnd_5min)
        self.b_dnd.pack(side="left", padx=2)

        self.status = tk.Label(root, text="", bg=BG, fg=DIM, font=("Segoe UI", 9))
        self.status.pack(padx=10, pady=(0, 10), anchor="w")
        self._draw(0.0, self.s["ready"], "")

    # --- breathing engine (10 s cycle, mirrors firmware calm.h) ---
    def _cycle(self, ms):
        w = ms % 10000
        if w < 2500:
            return self.s["in1"], 20 + 32 * (w / 2500.0)
        if w < 3500:
            return self.s["in2"], 52 + 8 * ((w - 2500) / 1000.0)
        if w < 9000:
            return self.s["out"], 60 - 38 * ((w - 3500) / 5500.0)
        return self.s["hold"], 22.0

    def _draw(self, frac, phase, sub):
        r = max(8.0, getattr(self, "_r", 22.0))
        cx, cy = 170, 105
        self.canvas.coords(self.ball, cx - r, cy - r, cx + r, cy + r)
        self.canvas.coords(self.ring, cx - r - 6, cy - r - 6, cx + r + 6, cy + r + 6)
        self.canvas.itemconfig(self.phase, text=phase)
        self.canvas.itemconfig(self.sub, text=sub)
        # progress bar
        self.canvas.delete("prog")
        self.canvas.create_rectangle(40, 236, 300, 240, fill=CARD, outline="", tags="prog")
        self.canvas.create_rectangle(40, 236, 40 + 260 * min(1.0, frac), 240,
                                     fill=OK, outline="", tags="prog")

    def _tick(self):
        if not self.running or self.paused:
            return
        import time
        el = self.elapsed + (time.monotonic() * 1000 - self.t0)
        if el >= self.total_ms:
            self._finish()
            return
        import math
        phase, r = self._cycle(el)
        self._r = r
        cyc = int(el // 10000) + 1
        total_cyc = max(1, int(round(self.total_ms / 10000)))
        self._draw(el / self.total_ms, phase, f"{cyc}/{total_cyc}")
        self._job = self.canvas.after(50, self._tick)

    def toggle(self):
        import time
        if not self.running:
            self.running, self.paused = True, False
            self.elapsed = 0
            self.t0 = time.monotonic() * 1000
            self.b_start.config(text=self.s["pause"])
            self.status.config(text="")
            self._tick()
        elif self.paused:
            self.paused = False
            self.t0 = time.monotonic() * 1000
            self.b_start.config(text=self.s["pause"])
            self._tick()
        else:
            self.paused = True
            self.elapsed += time.monotonic() * 1000 - self.t0
            self.b_start.config(text=self.s["resume"])
            self.canvas.itemconfig(self.sub, text=self.s["paused"])

    def stop(self):
        self.running, self.paused, self.elapsed = False, False, 0
        if self._job:
            self.canvas.after_cancel(self._job)
            self._job = None
        self._r = 22.0
        self.b_start.config(text=self.s["start"])
        self._draw(0.0, self.s["ready"], "")

    def _finish(self):
        self.stop()
        self._draw(1.0, self.s["done"], "")

    # --- PC + device actions ---
    def open_kt1(self):
        try:
            r = requests.post(f"{self.esp}/pet", json={"calm": True}, timeout=REQ_TIMEOUT)
            self.status.config(text=self.s["kt1_ok"] if r.ok else self.s["kt1_fail"])
        except requests.RequestException:
            self.status.config(text=self.s["kt1_fail"])

    def dnd_5min(self):
        h = {"X-Bridge-Token": self.token} if self.token else {}
        try:
            r = requests.post(f"{self.bridge}/os/dnd", json={"on": True},
                              headers=h, timeout=REQ_TIMEOUT)
            if not r.ok:
                raise requests.RequestException()
            self.status.config(text=self.s["dnd_on"])
            self.canvas.after(DND_MINUTES * 60 * 1000, self._dnd_off)
        except requests.RequestException:
            self.status.config(text=self.s["dnd_fail"])

    def _dnd_off(self):
        h = {"X-Bridge-Token": self.token} if self.token else {}
        try:
            requests.post(f"{self.bridge}/os/dnd", json={"on": False},
                          headers=h, timeout=REQ_TIMEOUT)
            self.status.config(text=self.s["dnd_off"])
        except requests.RequestException:
            self.status.config(text=self.s["dnd_fail"])


def main():
    ap = argparse.ArgumentParser(description="KT1 Calm window (SOS breathing for the PC)")
    ap.add_argument("--config", default="setup.json", help="setup.json (esp, token, port)")
    ap.add_argument("--bridge", default="", help="bridge base URL (default from port)")
    ap.add_argument("--esp", default="", help="ESP base URL")
    ap.add_argument("--token", default="", help="X-Bridge-Token")
    ap.add_argument("--lang", default="es", choices=["es", "en"])
    ap.add_argument("--seconds", type=int, default=DEFAULT_SECONDS)
    args = ap.parse_args()

    cfg = load_config(args.config)
    port = cfg.get("port", 8750)
    bridge = args.bridge or f"http://127.0.0.1:{port}"
    esp = args.esp or cfg.get("esp", "http://kt1.local")
    token = args.token or cfg.get("token", "")

    root = tk.Tk()
    CalmWindow(root, esp, bridge, token, args.lang, args.seconds)
    root.mainloop()


if __name__ == "__main__":
    main()
