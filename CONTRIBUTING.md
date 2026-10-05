# Contributing to KT1

**EN:** Keep it small and visual. Docs are bilingual (EN/ES): if you change one language, update the other or open the PR as draft and ask for help.

1. Fork → branch → PR against `main`. One feature/fix per PR.
2. Firmware: match the existing `.h` module style, no new libraries unless justified. Test compile with `pio run -e kt1` (CI does the same).
3. `pc-agent`: `python -m compileall -q pc-agent`. Never commit `setup.json`, `memory.json`, `audit.log`, `*.bin` or `secrets.h`.
4. Photos/videos: never show Wi-Fi names, IPs, tokens, keys or your PC screen content (see `docs/media/README.md`).
5. Good first issues: wiring photos, Wokwi test, translations, plant thresholds, exercise library in `firmware/.../exercises.h`.

**ES:** Pequeño y visual. La documentación es bilingüe (EN/ES): si cambias un idioma, actualiza el otro o abre el PR como borrador y pide ayuda.

1. Fork → rama → PR contra `main`. Una función/corrección por PR.
2. Firmware: sigue el estilo de módulos `.h`, sin librerías nuevas salvo justificación. Compila con `pio run -e kt1` (el CI hace lo mismo).
3. `pc-agent`: `python -m compileall -q pc-agent`. Nunca subas `setup.json`, `memory.json`, `audit.log`, `*.bin` ni `secrets.h`.
4. Fotos/vídeos: nunca muestres Wi-Fi, IP, tokens, claves ni contenido de tu PC (ver `docs/media/README.md`).
5. Buenas primeras tareas: fotos de cableado, prueba en Wokwi, traducciones, umbrales de plantas, librería de ejercicios en `firmware/.../exercises.h`.
