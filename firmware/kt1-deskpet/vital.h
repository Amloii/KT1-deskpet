// ===========================================================================
//  vital.h — Posture shifts: SIT / STAND + mobility RELAX.
//
//  VITAL screen with a Start button. Once started it alternates shifts with
//  the configured intervals (NVS): SND_SIT pulse when it's time to sit,
//  SND_STAND when it's time to stand and a distinct SND_MOVE for relax (1 gentle
//  GR_MOB exercise, no weights: the dumbbell coach is left untouched).
//  Everything runs in the background (vitalTick from loop) and also alerts on
//  the FACE with a big pill + LED. The TTP223 sensor confirms/advances the
//  phase and a long hold pauses/resumes. Pausing freezes the timers.
//  Mutually exclusive with the Exercise cycle: each one stops the other.
//  Texts WITHOUT accents (built-in TFT_eSPI fonts don't have them).
// ===========================================================================
#pragma once

#define VIT_SIT_DEF   20
#define VIT_STAND_DEF  8
#define VIT_RELAX_DEF  2
#define VIT_EVERY_DEF  2

enum VitalPhase { VIT_OFF, VIT_SIT, VIT_STAND, VIT_RELAX };

VitalPhase vitalPhase = VIT_OFF;
bool     vitalPaused = false;
bool     vitalAlert = false;          // phase change not yet confirmed
uint32_t vitalPhaseStart = 0, vitalPauseStart = 0;
int      vitalSitMin = VIT_SIT_DEF, vitalStandMin = VIT_STAND_DEF;
int      vitalRelaxMin = VIT_RELAX_DEF, vitalEvery = VIT_EVERY_DEF;
int      vitalStandsDone = 0, vitalMobIdx = 0;
uint32_t vitalLastSec = 0xFFFFFFFF;

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
}

void vitalInit() {
  vitalSitMin   = constrain((int)prefs.getUChar("vSit", VIT_SIT_DEF), 5, 90);
  vitalStandMin = constrain((int)prefs.getUChar("vStand", VIT_STAND_DEF), 2, 45);
  vitalRelaxMin = constrain((int)prefs.getUChar("vRelax", VIT_RELAX_DEF), 1, 10);
  vitalEvery    = constrain((int)prefs.getUChar("vEvery", VIT_EVERY_DEF), 1, 6);
  vitalPhase = VIT_OFF; vitalPaused = false; vitalAlert = false;
  Serial.printf("[VITAL] shifts %d/%d/%d every %d\n", vitalSitMin, vitalStandMin, vitalRelaxMin, vitalEvery);
}

void vitalSetPhase(VitalPhase p) {
  vitalPhase = p;
  vitalPhaseStart = millis();
  vitalAlert = true;
  if (p == VIT_SIT) sound(SND_SIT);
  else if (p == VIT_STAND) sound(SND_STAND);
  else if (p == VIT_RELAX) sound(SND_MOVE);
  gDirty = true;
  Serial.printf("[VITAL] phase: %s\n", vitalPhaseName());
}

void vitalNextPhase() {
  if (vitalPhase == VIT_SIT) vitalSetPhase(VIT_STAND);
  else if (vitalPhase == VIT_STAND) {
    vitalStandsDone++;
    vitalSetPhase((vitalStandsDone % vitalEvery == 0) ? VIT_RELAX : VIT_SIT);
  }
  else if (vitalPhase == VIT_RELAX) vitalSetPhase(VIT_SIT);
}

void vitalStart() {
  cycleStop();   // mutual exclusion: stops the Exercise cycle
  vitalStandsDone = 0;
  vitalPaused = false;
  vitalSetPhase(VIT_SIT);
  toast(TR("Vital ON: shifts", "Vital ON: turnos"), 1500);
}

void vitalStop() {
  if (vitalPhase == VIT_OFF) return;
  vitalPhase = VIT_OFF; vitalPaused = false; vitalAlert = false;
  gDirty = true;
}

void vitalTogglePause() {
  if (vitalPhase == VIT_OFF) return;
  uint32_t now = millis();
  if (!vitalPaused) { vitalPaused = true; vitalPauseStart = now; toast(TR("Vital paused", "Vital en pausa"), 1200); }
  else { vitalPaused = false; vitalPhaseStart += now - vitalPauseStart; toast(TR("Vital resumed", "Vital sigue"), 1200); }
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

// --- Tappable intervals (only when OFF). Rows: y 78/104/130, boxes h 22 ---
#define VIT_ROW_H   22
#define VIT_ROW0_Y  78
#define VIT_ROW1_Y  104
#define VIT_ROW2_Y  130
#define VIT_BTN_Y   164
#define VIT_BTN_H   28    // ends at 192: above the nav touch zone (196)

void vitalTapIntervals(int x, int y) {
  static const int SIT_V[] = { 15, 20, 30, 45, 60 };
  static const int STAND_V[] = { 5, 8, 10, 15 };
  static const int RELAX_V[] = { 1, 2, 3, 5 };
  static const int EVERY_V[] = { 1, 2, 3, 4 };
  char b[24];
  if (y >= VIT_ROW0_Y && y < VIT_ROW0_Y + VIT_ROW_H) {
    vitalSitMin = vitalStep(vitalSitMin, SIT_V, 5); vitalSave();
    snprintf(b, sizeof(b), TR("Sit %d'", "Sentado %d'"), vitalSitMin); toast(b, 900);
  } else if (y >= VIT_ROW1_Y && y < VIT_ROW1_Y + VIT_ROW_H) {
    vitalStandMin = vitalStep(vitalStandMin, STAND_V, 4); vitalSave();
    snprintf(b, sizeof(b), TR("Stand %d'", "De pie %d'"), vitalStandMin); toast(b, 900);
  } else if (y >= VIT_ROW2_Y && y < VIT_ROW2_Y + VIT_ROW_H) {
    if (x < 160) {
      vitalRelaxMin = vitalStep(vitalRelaxMin, RELAX_V, 4); vitalSave();
      snprintf(b, sizeof(b), "Relax %d'", vitalRelaxMin); toast(b, 900);
    } else {
      vitalEvery = vitalStep(vitalEvery, EVERY_V, 4); vitalSave();
      snprintf(b, sizeof(b), TR("Relax every %d", "Relax cada %d"), vitalEvery); toast(b, 900);
    }
  }
}

void vitalTap(int x, int y) {
  if (vitalPhase == VIT_OFF) {
    if (y >= VIT_BTN_Y - 6) { vitalStart(); return; }   // full-width Start
    vitalTapIntervals(x, y);
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
void vitalIntervalRow(int y, const char* label, const char* val) {
  txt(label, 12, y + 11, 2, TL_DATUM, P.inkDim);
  spr.fillRoundRect(SCR_W - 112, y, 100, VIT_ROW_H, 8, P.card);
  spr.drawRoundRect(SCR_W - 112, y, 100, VIT_ROW_H, 8, P.accent);
  txt(val, SCR_W - 62, y + 11, 2, MC_DATUM, P.ink);
}

void drawVitalPage() {
  drawHeader("VITAL");
  char b[64];

  if (vitalPhase == VIT_OFF) {
    drawEyesAt(SCR_W / 2, 46, 0.28f);
    txt(TR("Gentle posture shifts", "Turnos suaves de postura"), SCR_W / 2, 64, 1, MC_DATUM, P.inkDim);
    snprintf(b, sizeof(b), "%d'", vitalSitMin);
    vitalIntervalRow(VIT_ROW0_Y, TR("Sitting", "Sentado"), b);
    snprintf(b, sizeof(b), "%d'", vitalStandMin);
    vitalIntervalRow(VIT_ROW1_Y, TR("Standing", "De pie"), b);
    txt("Relax", 12, VIT_ROW2_Y + 11, 2, TL_DATUM, P.inkDim);
    snprintf(b, sizeof(b), "%d'", vitalRelaxMin);
    spr.fillRoundRect(96, VIT_ROW2_Y, 80, VIT_ROW_H, 8, P.card);
    spr.drawRoundRect(96, VIT_ROW2_Y, 80, VIT_ROW_H, 8, P.accent);
    txt(b, 136, VIT_ROW2_Y + 11, 2, MC_DATUM, P.ink);
    snprintf(b, sizeof(b), TR("every %d", "c/%d"), vitalEvery);
    spr.fillRoundRect(SCR_W - 112, VIT_ROW2_Y, 100, VIT_ROW_H, 8, P.card);
    spr.drawRoundRect(SCR_W - 112, VIT_ROW2_Y, 100, VIT_ROW_H, 8, P.accent);
    txt(b, SCR_W - 62, VIT_ROW2_Y + 11, 2, MC_DATUM, P.ink);
    drawButton(8, VIT_BTN_Y, SCR_W - 16, VIT_BTN_H, TR("Start", "Empezar"), true, P.ok);
    return;
  }

  // Running: phase + big countdown + bar + instruction + 3 buttons
  drawEyesAt(SCR_W / 2, 44, 0.26f);
  uint16_t pc = vitalPhaseColor();
  int r = vitalRemainS();
  txt(vitalPhaseName(), SCR_W / 2, 70, 4, MC_DATUM, pc);
  if (vitalPaused) txt(TR("paused", "en pausa"), SCR_W / 2, 90, 1, MC_DATUM, P.warn);
  else if (vitalAlert) txt(TR("tap the sensor when done!", "toca el sensor al hacerlo!"), SCR_W / 2, 90, 1, MC_DATUM, pc);
  snprintf(b, sizeof(b), "%02d:%02d", r / 60, r % 60);
  txt(b, SCR_W / 2, 116, 6, MC_DATUM, P.ink);
  uint32_t tot = vitalPhaseMs(), el = vitalElapsed();
  float f = tot ? constrain((float)el / tot, 0, 1) : 0;
  spr.fillRoundRect(24, 140, SCR_W - 48, 7, 3, P.card);
  spr.fillRoundRect(25, 141, (SCR_W - 50) * f, 5, 2, pc);
  if (vitalPhase == VIT_RELAX) {
    const Exercise* e = vitalMobCur();
    txtFit(e->name(), SCR_W / 2, 156, SCR_W - 20, 2, MC_DATUM, P.ok);
  } else if (vitalPhase == VIT_SIT) {
    txt(TR("Back straight, feet flat", "Espalda recta, pies apoyados"), SCR_W / 2, 156, 1, MC_DATUM, P.inkDim);
  } else {
    txt(TR("Desk up, shift your weight", "Mesa arriba, cambia el peso"), SCR_W / 2, 156, 1, MC_DATUM, P.inkDim);
  }

  drawButton(8, VIT_BTN_Y, 96, VIT_BTN_H, vitalPaused ? TR("Resume", "Seguir") : TR("Pause", "Pausa"), vitalPaused, vitalPaused ? P.ok : 0);
  drawButton(112, VIT_BTN_Y, 96, VIT_BTN_H, vitalAlert ? "OK!" : TR("Next >>", "Fase >>"), vitalAlert, vitalAlert ? P.warn : 0);
  drawButton(216, VIT_BTN_Y, 96, VIT_BTN_H, TR("End", "Fin"), false, P.danger);
}

// Big pill on the FACE: what's due now + countdown (LED + sound separately)
void drawVitalBanner() {
  if (vitalPhase == VIT_OFF) return;
  int y = 168, h = 34;
  uint16_t bg = vitalPhaseColor();
  spr.fillRoundRect(14, y, SCR_W - 28, h, 10, bg);
  int ax = 36, ay = y + h / 2;
  if (vitalPhase == VIT_STAND) spr.fillTriangle(ax - 10, ay + 7, ax + 10, ay + 7, ax, ay - 9, P.bg);
  else if (vitalPhase == VIT_SIT) spr.fillTriangle(ax - 10, ay - 7, ax + 10, ay - 7, ax, ay + 9, P.bg);
  else spr.fillCircle(ax, ay, 8, P.bg);
  char b[32];
  int r = vitalRemainS();
  const char* what = vitalPhase == VIT_STAND ? TR("Stand up", "Ponte de pie") : (vitalPhase == VIT_SIT ? TR("Sit down", "Sientate") : TR("Relax", "Relajate"));
  snprintf(b, sizeof(b), "%s %02d:%02d", what, r / 60, r % 60);
  txt(b, SCR_W / 2 + 12, ay - 5, 2, MC_DATUM, P.bg);
  txt(vitalPaused ? TR("paused", "en pausa") : TR("tap the sensor when done", "toca el sensor al hacerlo"), SCR_W / 2 + 12, ay + 10, 1, MC_DATUM, P.bg);
}
