// ===========================================================================
//  vital.h — Posture shifts: SIT / STAND + mobility RELAX.
//
//  VITAL DIA: director del dia con 3 presets (NVS vPreset). SND_SIT al sentarse,
//  SND_STAND al levantarse y SND_MOVE para relax (1 movilidad suave GR_MOB).
//  Espeja gPhase para que cara/stats/sed sigan funcionando: Vital es el unico
//  temporizador, Exercise es biblioteca + entrenador. Sin tildes (TFT sin glifos).
// ===========================================================================
#pragma once

#define VIT_SIT_DEF   20
#define VIT_STAND_DEF  8
#define VIT_RELAX_DEF  2
#define VIT_EVERY_DEF  2

// ---- Vital Dia: 3 presets de 1 toque (base cientifica en README §Health) ----
//  0 Suave sin mesa: 45/3/2 cada 3 (WHO 2020: ligera vale, evita parado largo)
//  1 Cornell clasico: 20/8/2 cada 2 (Hedge Cornell 20-8-2, default)
//  2 Foco: 50/10/5 cada 3 (Buckley: acumular 2h->4h sin rigidez)
struct VitalPreset { int sit, stand, relax, every; };
static const VitalPreset VIT_PRESETS[3] = { {45,3,2,3}, {20,8,2,2}, {50,10,5,3} };
static const char* const VIT_PRESET_NAMES_L[LANG_COUNT][3] = {
  { "Gentle no-desk 45/3", "Classic Cornell 20/8/2", "Focus 50/10" },
  { "Suave sin mesa 45/3", "Clasico Cornell 20/8/2", "Foco 50/10" },
};
#define VIT_PRESET_NAMES (VIT_PRESET_NAMES_L[gLang])

enum VitalPhase { VIT_OFF, VIT_SIT, VIT_STAND, VIT_RELAX };

VitalPhase vitalPhase = VIT_OFF;
bool     vitalPaused = false;
bool     vitalAlert = false;          // phase change not yet confirmed
uint32_t vitalPhaseStart = 0, vitalPauseStart = 0;
int      vitalSitMin = VIT_SIT_DEF, vitalStandMin = VIT_STAND_DEF;
int      vitalRelaxMin = VIT_RELAX_DEF, vitalEvery = VIT_EVERY_DEF;
int      vitalPreset = 1;   // 0 suave, 1 cornell (default), 2 foco (NVS "vPreset")
int      vitalStandsDone = 0, vitalMobIdx = 0;
uint32_t vitalLastSec = 0xFFFFFFFF;

// Espejo en el ciclo clasico (gPhase) para que cara/stats/sed sigan funcionando:
// Vital es el unico temporizador, Exercise es el entrenador.
void vitalSyncCycle() {
  if (vitalPhase == VIT_SIT)        gPhase = PH_SIT;
  else if (vitalPhase == VIT_STAND) gPhase = PH_STAND;
  else if (vitalPhase == VIT_RELAX) gPhase = PH_MOVE;
  else                              gPhase = PH_OFF;
  SIT_MIN = vitalSitMin; STAND_MIN = vitalStandMin; MOVE_MIN = vitalRelaxMin;
  phaseStart = vitalPhaseStart; pauseStart = vitalPauseStart; gPaused = vitalPaused;
  if (vitalPhase == VIT_OFF) { nudge.active = false; }
  gDirty = true;
}

static int vitalStep(int cur, const int* vs, int n) {
  for (int i = 0; i < n; i++) if (vs[i] == cur) return vs[(i + 1) % n];
  return vs[0];
}

uint32_t vitalPhaseMs() {
  if (vitalPhase == VIT_SIT)   return (uint32_t)vitalSitMin * 60000UL;
  if (vitalPhase == VIT_STAND) return (uint32_t)vitalStandMin * 60000UL;
  if (vitalPhase == VIT_RELAX) return (uint32_t)vitalRelaxMin * 60000UL;
  return 0;
}
uint32_t vitalElapsed() { return (vitalPaused ? vitalPauseStart : millis()) - vitalPhaseStart; }
int vitalRemainS() {
  uint32_t d = vitalPhaseMs(), e = vitalElapsed();
  return e >= d ? 0 : (d - e) / 1000;
}
bool vitalAlertActive() { return vitalAlert && vitalPhase != VIT_OFF; }

const char* vitalPhaseName() {
  switch (vitalPhase) {
    case VIT_SIT:   return TR("SIT", "SENTADO");
    case VIT_STAND: return TR("STAND", "DE PIE");
    case VIT_RELAX: return TR("RELAX", "RELAJATE");
    default:        return "OFF";
  }
}
uint16_t vitalPhaseColor() {
  switch (vitalPhase) {
    case VIT_SIT:   return P.info;
    case VIT_STAND: return P.ok;
    case VIT_RELAX: return P.warn;
    default:        return P.inkDim;
  }
}

// Gentle mobility for the shift (GR_MOB only, rotating)
const Exercise* vitalMobCur() {
  static int idx[10]; static int n = -1;
  if (n < 0) {
    n = 0;
    for (int i = 0; i < EX_COUNT && n < 10; i++) if (EXERCISES[i].group == GR_MOB) idx[n++] = i;
    if (n == 0) idx[n++] = 0;
  }
  return &EXERCISES[idx[vitalMobIdx % n]];
}

void vitalSave() {
  prefs.putUChar("vSit", (uint8_t)vitalSitMin);
  prefs.putUChar("vStand", (uint8_t)vitalStandMin);
  prefs.putUChar("vRelax", (uint8_t)vitalRelaxMin);
  prefs.putUChar("vEvery", (uint8_t)vitalEvery);
  prefs.putUChar("vPreset", (uint8_t)vitalPreset);
}

void vitalSetPreset(int i) {
  i = constrain(i, 0, 2);
  vitalPreset = i;
  vitalSitMin = VIT_PRESETS[i].sit;
  vitalStandMin = VIT_PRESETS[i].stand;
  vitalRelaxMin = VIT_PRESETS[i].relax;
  vitalEvery = VIT_PRESETS[i].every;
  SIT_MIN = vitalSitMin; STAND_MIN = vitalStandMin; MOVE_MIN = vitalRelaxMin;
  vitalSave();
}

void vitalInit() {
  vitalPreset   = constrain((int)prefs.getUChar("vPreset", 1), 0, 2);
  vitalSitMin   = VIT_PRESETS[vitalPreset].sit;
  vitalStandMin = VIT_PRESETS[vitalPreset].stand;
  vitalRelaxMin = VIT_PRESETS[vitalPreset].relax;
  vitalEvery    = VIT_PRESETS[vitalPreset].every;
  // Migracion: si habia valores manuales antiguos distintos de un preset,
  // se respetan una vez y se marcan como preset personalizado (= mantener valores).
  int oldSit = prefs.getUChar("vSit", 0);
  if (oldSit >= 5 && oldSit != vitalSitMin) {
    vitalSitMin   = constrain(oldSit, 5, 90);
    vitalStandMin = constrain((int)prefs.getUChar("vStand", vitalStandMin), 2, 45);
    vitalRelaxMin = constrain((int)prefs.getUChar("vRelax", vitalRelaxMin), 1, 10);
    vitalEvery    = constrain((int)prefs.getUChar("vEvery", vitalEvery), 1, 6);
  }
  vitalPhase = VIT_OFF; vitalPaused = false; vitalAlert = false;
  vitalSyncCycle();
  Serial.printf("[VITAL] dia preset %d: %d/%d/%d cada %d\n", vitalPreset, vitalSitMin, vitalStandMin, vitalRelaxMin, vitalEvery);
}

void vitalSetPhase(VitalPhase p) {
  vitalPhase = p;
  vitalPhaseStart = millis();
  vitalAlert = true;
  if (p == VIT_SIT) sound(SND_SIT);
  else if (p == VIT_STAND) sound(SND_STAND);
  else if (p == VIT_RELAX) sound(SND_MOVE);
  vitalSyncCycle();
  gDirty = true;
  Serial.printf("[VITAL] phase: %s\n", vitalPhaseName());
}

void vitalNextPhase() {
  if (vitalPhase == VIT_SIT) vitalSetPhase(VIT_STAND);
  else if (vitalPhase == VIT_STAND) {
    vitalStandsDone++;
    cycleNo++;
    vitalSetPhase((vitalStandsDone % vitalEvery == 0) ? VIT_RELAX : VIT_SIT);
  }
  else if (vitalPhase == VIT_RELAX) vitalSetPhase(VIT_SIT);
}

void vitalStart() {
  // Para el entrenador/ciclo clasico en linea (sin llamar a cycleStop: recursion).
  if (coach.active) coachClose();
  nudge.active = false;
  gPhase = PH_OFF; gPaused = false;
  gVitalDrives = true;
  cycleNo = 1;
  vitalStandsDone = 0;
  vitalPaused = false;
  vitalSetPhase(VIT_SIT);
  toast(TR("Vital ON: shifts", "Vital ON: turnos"), 1500);
}

void vitalStop() {
  if (vitalPhase == VIT_OFF && gPhase == PH_OFF) return;
  if (coach.active) coachClose();
  vitalPhase = VIT_OFF; vitalPaused = false; vitalAlert = false;
  nudge.active = false;
  gPhase = PH_OFF; gPaused = false;
  gVitalDrives = false;
  struct tm t;
  if (getLocal(t)) autoStartDay = (t.tm_year + 1900) * 10000UL + (t.tm_mon + 1) * 100UL + t.tm_mday;
  gDirty = true;
}

void vitalTogglePause() {
  if (vitalPhase == VIT_OFF) return;
  uint32_t now = millis();
  if (!vitalPaused) { vitalPaused = true; vitalPauseStart = now; toast(TR("Vital paused", "Vital en pausa"), 1200); }
  else { vitalPaused = false; vitalPhaseStart += now - vitalPauseStart; toast(TR("Vital resumed", "Vital sigue"), 1200); }
  vitalSyncCycle();
  gDirty = true;
}

// Confirm = "done it" (posture) or finish the relax
void vitalConfirm() {
  if (vitalPhase == VIT_OFF) return;
  if (vitalPhase == VIT_RELAX) {
    const Exercise* e = vitalMobCur();
    logExercise(*e);
    statsSave();
    sedMarkMoved();
    vitalMobIdx++;
    vitalAlert = false;
    vitalSetPhase(VIT_SIT);
    setMoodFor(M_HAPPY, 3000); squash = 1.0f;
    toast(TR("Relax done!", "Relax hecho!"), 1500);
    Serial.printf("[VITAL] relax done: %s\n", e->name());
    return;
  }
  vitalAlert = false;
  confirmPosture(vitalPhase == VIT_STAND ? POS_STAND : POS_SIT, false);
  gDirty = true;
}

// Advance = confirm if there's an alert, finish relax, or skip the phase
void vitalAdvance() {
  if (vitalPhase == VIT_OFF) return;
  if (vitalAlert || vitalPhase == VIT_RELAX) { vitalConfirm(); return; }
  vitalAlert = false;
  vitalSetPhase(vitalPhase == VIT_SIT ? VIT_STAND : VIT_SIT);  // manual skip: alternates without touching the relax count
  toast(TR("Next phase", "Siguiente fase"), 1000);
}

void vitalTick() {
  if (vitalPhase == VIT_OFF || vitalPaused) return;
  uint32_t now = millis();
  uint32_t sec = vitalElapsed() / 1000;
  if (sec != vitalLastSec) {
    vitalLastSec = sec;
    if (gPage == PAGE_VITAL || gPage == PAGE_FACE) gDirty = true;
  }
  if (vitalElapsed() >= vitalPhaseMs()) {
    if (vitalPhase == VIT_RELAX) vitalConfirm();   // ends by itself: logs it and back to sitting
    else vitalNextPhase();
  }
}

// --- Linea del dia (Buckley 2h->4h): pie+mov acumulado + racha + siguiente ---
void vitalDiaLine(char* out, size_t n) {
  uint32_t active = stats.standS + stats.moveS;
  char d1[16];
  fmtDur(active, d1, sizeof(d1));
  const char* next = "?";
  if (vitalPhase == VIT_SIT) next = TR("stand", "pie");
  else if (vitalPhase == VIT_STAND) next = ((vitalStandsDone + 1) % vitalEvery == 0)
      ? TR("relax", "relax") : TR("sit", "sentado");
  else if (vitalPhase == VIT_RELAX) next = TR("sit", "sentado");
  else next = TR("start", "empezar");
  snprintf(out, n, TR("Day %s/%dh - next %s", "Hoy %s/%dh - prox %s"),
           d1, STAND_GOAL_MIN / 60, next);
}

const char* vitalNextExerciseName() {
  const Exercise* e = vitalMobCur();
  return e ? e->name() : "";
}

// --- Tappable presets (solo OFF). 3 filas h26: y 74/104/134, boton 164 ---
#define VIT_ROW_H   26
#define VIT_ROW0_Y  74
#define VIT_BTN_Y   164
#define VIT_BTN_H   28    // ends at 192: above the nav touch zone (196)

void vitalTap(int x, int y) {
  if (vitalPhase == VIT_OFF) {
    for (int i = 0; i < 3; i++) {
      int ry = VIT_ROW0_Y + i * (VIT_ROW_H + 4);
      if (y >= ry && y < ry + VIT_ROW_H) {
        vitalSetPreset(i);
        char b[32];
        snprintf(b, sizeof(b), "%s", VIT_PRESET_NAMES[i]);
        toast(b, 900);
        gDirty = true;
        return;
      }
    }
    if (y >= VIT_BTN_Y - 6) { vitalStart(); return; }   // full-width Start
    return;
  }
  if (y >= VIT_BTN_Y - 6 && y < NAV_TOUCH_Y) {          // row: Pause | OK | End
    if (x < 112) vitalTogglePause();
    else if (x < 216) vitalAdvance();   // OK / skip
    else { vitalStop(); toast("Vital off", 1200); }
    return;
  }
  if (y >= 56 && y < VIT_BTN_Y - 6) { vitalAdvance(); return; }   // card = confirm/skip
}

// --- Drawing: everything ends at y=192 (nav touch zone starts at 196) ---
void vitalPresetRow(int idx, int y, bool sel) {
  int x = 8, w = SCR_W - 16;
  spr.fillRoundRect(x, y, w, VIT_ROW_H, 8, sel ? P.accent : P.card);
  if (!sel) spr.drawRoundRect(x, y, w, VIT_ROW_H, 8, P.line);
  uint16_t fg = sel ? P.bg : P.ink;
  txtFit(VIT_PRESET_NAMES[idx], x + w / 2, y + VIT_ROW_H / 2, w - 16, 2, MC_DATUM, fg);
}

void drawVitalPage() {
  drawHeader("VITAL");
  char b[64];

  if (vitalPhase == VIT_OFF) {
    drawEyesAt(SCR_W / 2, 38, 0.28f);
    txt(TR("Choose your day", "Elige tu dia"), SCR_W / 2, 52, 1, MC_DATUM, P.inkDim);
    vitalDiaLine(b, sizeof(b));
    txt(b, SCR_W / 2, 62, 1, MC_DATUM, P.inkDim);
    for (int i = 0; i < 3; i++)
      vitalPresetRow(i, VIT_ROW0_Y + i * (VIT_ROW_H + 4), i == vitalPreset);
    drawButton(8, VIT_BTN_Y, SCR_W - 16, VIT_BTN_H, TR("Start", "Empezar"), true, P.ok);
    return;
  }

  // Running: phase + big countdown + bar + instruction + day line + 3 buttons
  drawEyesAt(SCR_W / 2, 42, 0.26f);
  uint16_t pc = vitalPhaseColor();
  int r = vitalRemainS();
  txt(vitalPhaseName(), SCR_W / 2, 66, 4, MC_DATUM, pc);
  if (vitalPaused) txt(TR("paused", "en pausa"), SCR_W / 2, 86, 1, MC_DATUM, P.warn);
  else if (vitalAlert) txt(TR("tap the sensor when done!", "toca el sensor al hacerlo!"), SCR_W / 2, 86, 1, MC_DATUM, pc);
  snprintf(b, sizeof(b), "%02d:%02d", r / 60, r % 60);
  txt(b, SCR_W / 2, 110, 6, MC_DATUM, P.ink);
  uint32_t tot = vitalPhaseMs(), el = vitalElapsed();
  float f = tot ? constrain((float)el / tot, 0, 1) : 0;
  spr.fillRoundRect(24, 134, SCR_W - 48, 7, 3, P.card);
  spr.fillRoundRect(25, 135, (SCR_W - 50) * f, 5, 2, pc);
  if (vitalPhase == VIT_RELAX) {
    const Exercise* e = vitalMobCur();
    txtFit(e->name(), SCR_W / 2, 146, SCR_W - 20, 2, MC_DATUM, P.ok);
  } else if (vitalPhase == VIT_SIT) {
    txt(TR("Back straight, feet flat", "Espalda recta, pies apoyados"), SCR_W / 2, 146, 1, MC_DATUM, P.inkDim);
  } else {
    txt(vitalPreset == 0
        ? TR("Stand, walk, drink water", "De pie, camina, bebe agua")
        : TR("Desk up, shift your weight", "Mesa arriba, cambia el peso"), SCR_W / 2, 146, 1, MC_DATUM, P.inkDim);
  }
  vitalDiaLine(b, sizeof(b));
  txt(b, SCR_W / 2, 157, 1, MC_DATUM, P.inkDim);

  drawButton(8, VIT_BTN_Y, 96, VIT_BTN_H, vitalPaused ? TR("Resume", "Seguir") : TR("Pause", "Pausa"), vitalPaused, vitalPaused ? P.ok : 0);
  drawButton(112, VIT_BTN_Y, 96, VIT_BTN_H, vitalAlert ? "OK!" : TR("Next >>", "Fase >>"), vitalAlert, vitalAlert ? P.warn : 0);
  drawButton(216, VIT_BTN_Y, 96, VIT_BTN_H, TR("End", "Fin"), false, P.danger);
}

// Big pill on the FACE: what's due now + countdown + next (LED + sound separately)
void drawVitalBanner() {
  if (vitalPhase == VIT_OFF) return;
  int y = 168, h = 34;
  uint16_t bg = vitalPhaseColor();
  spr.fillRoundRect(14, y, SCR_W - 28, h, 10, bg);
  int ax = 36, ay = y + h / 2;
  if (vitalPhase == VIT_STAND) spr.fillTriangle(ax - 10, ay + 7, ax + 10, ay + 7, ax, ay - 9, P.bg);
  else if (vitalPhase == VIT_SIT) spr.fillTriangle(ax - 10, ay - 7, ax + 10, ay - 7, ax, ay + 9, P.bg);
  else spr.fillCircle(ax, ay, 8, P.bg);
  char b[48];
  int r = vitalRemainS();
  const char* what = vitalPhase == VIT_STAND ? TR("Stand up", "Ponte de pie") : (vitalPhase == VIT_SIT ? TR("Sit down", "Sientate") : TR("Relax", "Relajate"));
  snprintf(b, sizeof(b), "%s %02d:%02d", what, r / 60, r % 60);
  txtFit(b, SCR_W / 2 + 12, ay - 5, SCR_W - 100, 2, MC_DATUM, P.bg);
  if (vitalPhase == VIT_RELAX) {
    txtFit(vitalNextExerciseName(), SCR_W / 2 + 12, ay + 10, SCR_W - 100, 1, MC_DATUM, P.bg);
  } else {
    char nb[48];
    vitalDiaLine(nb, sizeof(nb));
    txtFit(nb, SCR_W / 2 + 12, ay + 10, SCR_W - 100, 1, MC_DATUM, P.bg);
  }
}
