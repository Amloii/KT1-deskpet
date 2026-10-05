# Auto-reminder: TFT_eSPI needs the Freenove display selected.
# Runs on every PlatformIO build; never fails the build, only prints a warning.
Import("env")
import os

setup_name = "User_Setup_Select.h"
found = []
libdeps = env.get("PROJECT_LIBDEPS_DIR", "")
search_roots = [libdeps, env.get("PROJECT_DIR", "")]
for root in search_roots:
    if not root or not os.path.isdir(root):
        continue
    for dirpath, _, filenames in os.walk(root):
        if "TFT_eSPI" in dirpath and setup_name in filenames:
            found.append(os.path.join(dirpath, setup_name))

if not found:
    print("[KT1] TFT_eSPI not installed yet — after first install, enable ONLY "
          "FNK0104AB_2P8_240x320_ILI9341 in TFT_eSPI/User_Setup_Select.h "
          "(docs/en/installation.md section 3).")
else:
    for path in found:
        try:
            with open(path, encoding="utf-8", errors="ignore") as f:
                text = f.read()
            if "FNK0104AB_2P8_240x320_ILI9341" in text:
                print(f"[KT1] TFT_eSPI setup found: {path} (check that ONLY "
                      "FNK0104AB_2P8_240x320_ILI9341 is enabled).")
        except OSError:
            pass
