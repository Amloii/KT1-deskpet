# -*- coding: utf-8 -*-
"""
Windows process -> pet category mapping.

Categories understood by the ESP32:
    coding | web | gaming | media | chat | idle | offline

How it works:
    - The lowercase executable name is compared (e.g. "code.exe").
    - First an EXACT match is looked up in PROCESS_MAP.
    - Otherwise a SUBSTRING match is tried in KEYWORD_MAP (useful for games,
      which have countless names, or browsers with variants).
    - If nothing matches -> DEFAULT_CATEGORY.

Feel free to edit these tables to fit the programs you use.
"""

DEFAULT_CATEGORY = "idle"

# EXACT match by executable name (lowercase).
PROCESS_MAP = {
    # --- coding ---
    "code.exe": "coding",
    "code - insiders.exe": "coding",
    "cursor.exe": "coding",
    "devenv.exe": "coding",          # Visual Studio
    "pycharm64.exe": "coding",
    "idea64.exe": "coding",
    "clion64.exe": "coding",
    "sublime_text.exe": "coding",
    "notepad++.exe": "coding",
    "windowsterminal.exe": "coding",
    "powershell.exe": "coding",
    "pwsh.exe": "coding",
    "cmd.exe": "coding",
    "arduino ide.exe": "coding",
    "arduino.exe": "coding",

    # --- web ---
    "chrome.exe": "web",
    "firefox.exe": "web",
    "msedge.exe": "web",
    "brave.exe": "web",
    "opera.exe": "web",
    "vivaldi.exe": "web",

    # --- media ---
    "spotify.exe": "media",
    "vlc.exe": "media",
    "wmplayer.exe": "media",
    "mpc-hc64.exe": "media",
    "mpc-hc.exe": "media",
    "foobar2000.exe": "media",

    # --- chat ---
    "discord.exe": "chat",
    "teams.exe": "chat",
    "ms-teams.exe": "chat",
    "slack.exe": "chat",
    "telegram.exe": "chat",
    "whatsapp.exe": "chat",
    "zoom.exe": "chat",
    "webex.exe": "chat",
    "skype.exe": "chat",

    # --- gaming (well-known launchers) ---
    "steam.exe": "gaming",
    "steamwebhelper.exe": "gaming",
    "epicgameslauncher.exe": "gaming",
    "battle.net.exe": "gaming",
    "leagueclient.exe": "gaming",
}

# SUBSTRING match (if the exe name contains this word).
# Evaluated in order; the first match wins.
KEYWORD_MAP = [
    ("chrome", "web"),
    ("firefox", "web"),
    ("edge", "web"),
    ("browser", "web"),
    ("spotify", "media"),
    ("vlc", "media"),
    ("discord", "chat"),
    ("teams", "chat"),
    ("slack", "chat"),
    # games: common patterns in game executables
    ("game", "gaming"),
    ("launcher", "gaming"),
    ("unity", "gaming"),
    ("unreal", "gaming"),
]


# Apps that use the microphone but do NOT mean "I'm in a call"
# (lowercase substring match against the name Windows reports).
# Examples: recorders, voice assistants, the chatbot's own test script...
MIC_IGNORE = [
    "soundrecorder",
    "voicerecorder",
    "python",          # Python-based chatbot tests record with Python
    "steam",           # Steam keeps the mic open for voice chat even when you are silent
]


def categorize(exe_name: str) -> str:
    """Return the category for an executable name."""
    if not exe_name:
        return DEFAULT_CATEGORY
    name = exe_name.lower()

    if name in PROCESS_MAP:
        return PROCESS_MAP[name]

    for keyword, category in KEYWORD_MAP:
        if keyword in name:
            return category

    return DEFAULT_CATEGORY
