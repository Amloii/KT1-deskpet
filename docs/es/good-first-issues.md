# Good first issues (listos para copiar-pegar)

[English](../en/good-first-issues.md) | **Español**

> Mantenedor: crea estos como issues en GitHub y añade la etiqueta `good first issue`
> (un clic por issue). Copia el título y el cuerpo de cada sección.

---

## 1. Portar `pc-agent` a Linux (luego macOS)

**Etiquetas:** `good first issue`, `enhancement`, `pc-agent`
**Por qué:** `pc-agent` hoy solo funciona en Windows (`pywin32`, `ctypes.windll`, PowerShell). Soportar Linux/macOS triplica la cantera de contribuidores.

**Alcance (sugerido en 2 PRs):**
- `pet_agent.py`: abstraer app en primer plano / tiempo idle / micro en uso tras una capa `platform_`. Linux: `xdotool`/`libinput`/`/proc`, o portales XDG en Wayland. Sin tocar la ruta Windows.
- `bridge.py`: abstraer `/media/*`, `/os/lock`, `/os/dnd`, `/screen/shot`. Linux: `playerctl`, `loginctl`, `mss` ya es multiplataforma.
- `setup.py` + `requirements.txt`: `pywin32`/`pycaw`/`comtypes` como extras solo-Windows; documentar `requirements-linux.txt`.

**Archivos:** `pc-agent/pet_agent.py`, `pc-agent/bridge.py`, `pc-agent/setup.py`, `pc-agent/requirements.txt`, `docs/es/pc-agent.md`
**Aceptación:**
- `python -m compileall -q pc-agent` pasa en Windows + Linux.
- En Linux: presencia (`POST /pet`), volumen/media y captura funcionan; rutas no soportadas devuelven `501` claro, nunca cuelgue.
- Docs actualizadas (EN+ES) con pasos por SO.

---

## 2. Ampliar la librería de ejercicios (`exercises.h`)

**Etiquetas:** `good first issue`, `firmware`
**Por qué:** Sin hardware, pura edición de datos, visible en el dispositivo en minutos.

**Alcance:** añadir 4–6 entradas a `EXERCISES[]` en `firmware/kt1-deskpet/exercises.h` (grupos `GR_MOB` / `GR_LEG` sin pesas).
**Reglas:** nombre ≤ ~21 car., indicación ≤ ~34 car., sin tildes, ambos `{ "English", "Spanish" }`, `pio run -e kt1` compila.
**Aceptación:** los nuevos ejercicios rotan bien en las pausas; el PR incluye foto o nota de Wokwi.

---

## 3. Calibrar umbrales de plantas (DHT11 + LDR)

**Etiquetas:** `good first issue`, `firmware`, `documentation`
**Por qué:** Los umbrales actuales son estimaciones; datos reales de habitación hacen útil la página Plantas.

**Alcance:** registrar un día de lecturas DHT11 + KY-018 (sol/sombra, día/noche), proponer bandas `TEMP_OK`/`HUM_OK`/lux, actualizar constantes + `docs/es|en/hardware.md`.
**Aceptación:** tabla con lecturas + bandas elegidas; el firmware compila; docs EN+ES actualizadas.

---

## 4. Prueba de humo en Wokwi + fotos de cableado

**Etiquetas:** `good first issue`, `documentation`
**Por qué:** Cada foto verificada y cada prueba en Wokwi ahorra una hora a un nuevo montador.

**Alcance:** importar `diagram.json` en Wokwi (ESP32-S3 + ILI9341 + MPU6050 + DHT + botón), informar log de arranque + capturas; opcionalmente una foto real (sin Wi-Fi, IPs ni tokens — ver `docs/media/README.md`).
**Aceptación:** checklist de qué arranca / qué difiere del cableado real FNK0104B; fotos o enlace Wokwi en el PR.
