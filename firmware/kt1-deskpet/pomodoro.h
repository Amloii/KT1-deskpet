// ===========================================================================
//  pomodoro.h — Pomodoro v2 with creature face.
//  Focus (def 25') + break (def 10'), configurable in Settings (NVS).
//  Modes: work / writing / leisure -> tailored messages.
//  The timer runs in the background (pomoTick from loop) even on other pages.
//  Visual: the eyes are always shown small on top (the screen is the face).
// ===========================================================================
#pragma once

enum PomoState { POMO_IDLE, POMO_READY, POMO_FOCUS, POMO_BREAK, POMO_DONE };
enum PomoMode { POMO_TRABAJO = 0, POMO_ESCRITURA = 1, POMO_OCIO = 2, POMO_MODES = 3 };

int pomoFocusMin = POMO_FOCUS_DEFAULT;
int pomoBreakMin = POMO_BREAK_DEFAULT;
int pomoMode = POMO_TRABAJO;
PomoState pomoState = POMO_IDLE;
uint32_t pomoT0 = 0, pomoPausedAcc = 0, pomoPauseStart = 0;
bool pomoPaused = false;
uint32_t pomoLastCheer = 0;
int pomoDoneCount = 0;

const char* const POMO_MODE_NAMES_L[LANG_COUNT][POMO_MODES] = {
  { "Work",    "Writing",   "Leisure" },
  { "Trabajo", "Escritura", "Ocio"    },
};
#define POMO_MODE_NAMES (POMO_MODE_NAMES_L[gLang])

void pomoInit() {}

bool pomoRunning() { return pomoState == POMO_FOCUS || pomoState == POMO_BREAK; }

uint32_t pomoTotalMs() {
  if (pomoState == POMO_FOCUS) return (uint32_t)pomoFocusMin * 60000UL;
  if (pomoState == POMO_BREAK) return (uint32_t)pomoBreakMin * 60000UL;
  return 0;
}
uint32_t pomoElapsed() {
  if (pomoState != POMO_FOCUS && pomoState != POMO_BREAK) return 0;
  uint32_t now = millis();
  return pomoPausedAcc + (pomoPaused ? 0 : (now - pomoT0));
}
uint32_t pomoRemainS() {
  uint32_t tot = pomoTotalMs(), el = pomoElapsed();
  return el >= tot ? 0 : (tot - el) / 1000;
}

const char* pomoRelaxLine() {
  switch (pomoMode) {
    case POMO_ESCRITURA: return TR("Drop your shoulders. 3 breaths.", "Suelta hombros. 3 respiraciones.");
    case POMO_OCIO:      return TR("Get comfy. Breathe slowly.", "Ponte comodo. Respira lento.");
    default:             return TR("Sit up straight. Look far 20 s.", "Espalda recta. Mira lejos 20 s.");
  }
}
const char* pomoCheerLine() {
  // Short, non-distracting encouragement. Rotated.
  static int k = 0; k = (k + 1) % 9;
  switch (pomoMode) {
    case POMO_ESCRITURA:
      switch (k) { case 0: return TR("Good pace, go on.", "Buen ritmo, sigue."); case 1: return TR("One more line.", "Una frase mas."); case 2: return TR("Loose hands.", "Manos sueltas.");
        case 3: return TR("Doing great.", "Vas bien."); case 4: return TR("Keep weaving.", "Sigue hilando."); case 5: return TR("Breathe, carry on.", "Respira y continua.");
        case 6: return TR("Almost there.", "Casi lo tienes."); case 7: return TR("Easy, let it flow.", "Tranquilo, fluye."); default: return TR("Nice line.", "Buena linea."); }
    case POMO_OCIO:
      switch (k) { case 0: return TR("Enjoy.", "Disfruta."); case 1: return TR("No rush.", "Sin prisa."); case 2: return TR("So cozy.", "Que a gusto.");
        case 3: return TR("Keep it up.", "Sigue asi."); case 4: return "Relax..."; case 5: return TR("Nice.", "Bien.");
        case 6: return TR("Your pace.", "A tu ritmo."); case 7: return TR("Love to see it.", "Mola verte asi."); default: return ":)"; }
    default:
      switch (k) { case 0: return TR("Doing well, go on.", "Vas bien, sigue."); case 1: return TR("Focus, you got it.", "Foco, tu puedes."); case 2: return TR("One more step.", "Un paso mas.");
        case 3: return TR("Keep the pace.", "Aguanta el ritmo."); case 4: return TR("Well done.", "Bien hecho."); case 5: return TR("Keep it up.", "Sigue asi.");
        case 6: return TR("Almost done.", "Casi terminas."); case 7: return TR("Breathe, keep on.", "Respira y sigue."); default: return TR("Come on!", "Animo."); }
  }
}

void pomoStartFocus() {
  pomoState = POMO_FOCUS; pomoT0 = millis(); pomoPausedAcc = 0; pomoPaused = false;
  pomoLastCheer = millis();
  setMoodFor(M_HAPPY, 2500); squash = 1.0f;
  sound(SND_GO);
  gDirty = true;
}
void pomoStartBreak() {
  pomoState = POMO_BREAK; pomoT0 = millis(); pomoPausedAcc = 0; pomoPaused = false;
  setMoodFor(M_LOVE, 3000); squash = 1.0f;
  sound(SND_DONE);
  gDirty = true;
}
void pomoStop() {
  pomoState = POMO_IDLE; pomoPaused = false; pomoPausedAcc = 0;
  gDirty = true;
}

// Pause/resume shared by the screen and the TTP223 sensor (same text).
void pomoTogglePause() {
  uint32_t now = millis();
  if (!pomoPaused) { pomoPaused = true; pomoPauseStart = now; toast(TR("Pomo paused", "Pomo en pausa"), 1000); }
  else { pomoPaused = false; pomoT0 = now; toast(TR("Resuming", "Seguimos"), 1000); }
  gDirty = true;
}

void pomoTick() {
  if (pomoState != POMO_FOCUS && pomoState != POMO_BREAK) return;
  if (pomoPaused) return;
  uint32_t now = millis();
  if (pomoElapsed() >= pomoTotalMs()) {
    if (pomoState == POMO_FOCUS) {
      pomoDoneCount++;
      pomoStartBreak();
      toast(TR("Focus done! Rest", "Foco hecho! Descansa"), 2000);
    } else {
      pomoState = POMO_DONE; pomoT0 = now;
      setMoodFor(M_HAPPY, 5000); squash = 1.0f;
      sound(SND_DONE);
      toast(TR("Break over", "Descanso listo"), 2000);
    }
    gDirty = true;
    return;
  }
  // Encouragement every ~5 min during focus, without changing page or distracting
  if (pomoState == POMO_FOCUS && now - pomoLastCheer > 5 * 60000UL) {
    pomoLastCheer = now;
    toast(pomoCheerLine(), 1500);
    setMoodFor(M_HAPPY, 2000);
    sound(SND_TICK);
  }
}

void pomoTap(int x, int y) {
  // Zones = drawn buttons (left x<160 / right x>=160). Left half = action,
  // right half = mode (IDLE) or back (READY).
  if (pomoState == POMO_IDLE || pomoState == POMO_DONE) {
    if (y >= 96 && y < 124) {   // mode pills
      int m = x < 111 ? 0 : (x < 214 ? 1 : 2);
      if (m != pomoMode) { pomoMode = m; toast(POMO_MODE_NAMES[pomoMode], 900); }
      else { pomoState = POMO_READY; toast(pomoRelaxLine(), 2200); }
      return;
    }
    if (y >= 168) {   // "Start" | "Mode"
      if (x < 160) { pomoState = POMO_READY; toast(pomoRelaxLine(), 2200); }
      else { pomoMode = (pomoMode + 1) % POMO_MODES; toast(POMO_MODE_NAMES[pomoMode], 900); }
      return;
    }
    pomoState = POMO_READY;
    toast(pomoRelaxLine(), 2200);
    return;
  }
  if (pomoState == POMO_READY) {
    if (y >= 168) {   // "Start focus" | "Back"
      if (x < 160) pomoStartFocus();
      else { pomoState = POMO_IDLE; toast("Pomodoro", 800); }
    } else pomoStartFocus();
    return;
  }
  // FOCUS / BREAK running
  if (y >= FOCUS_BTN_Y - 6 && y < NAV_TOUCH_Y) {
    if (x < 107) pomoTogglePause();   // pause / resume
    else if (x < 214) {  // manual cheer
      toast(pomoCheerLine(), 1500);
      setMoodFor(M_HAPPY, 2000);
    } else pomoStop();  // finish
  }
}

// --- Drawing: creature frame (small eyes on top + rounded card) ---
void drawPomoPage() {
  drawHeader("POMODORO");
  char b[48];
  // Creature eyes always visible on top (small, not full screen)
  drawEyesAt(SCR_W / 2, 62, 0.42f);

  if (pomoState == POMO_IDLE || pomoState == POMO_DONE) {
    if (pomoState == POMO_DONE) txt(TR("Cycle complete!", "Ciclo completo!"), SCR_W / 2, 92, 2, MC_DATUM, P.ok);
    else txt(TR("Pick a mode (tap 2x)", "Elige modo (toca 2x)"), SCR_W / 2, 92, 1, MC_DATUM, P.inkDim);
    for (int i = 0; i < 3; i++) {
      int bx = 8 + i * 103;
      bool sel = (i == pomoMode);
      spr.fillRoundRect(bx, 100, 97, 24, 8, sel ? P.accent : P.card);
      if (!sel) spr.drawRoundRect(bx, 100, 97, 24, 8, P.line);
      txt(POMO_MODE_NAMES[i], bx + 48, 112, 2, MC_DATUM, sel ? P.bg : P.ink);
    }
    char t[32]; snprintf(t, sizeof(t), "%d' + %d'", pomoFocusMin, pomoBreakMin);
    txt(t, SCR_W / 2, 138, 4, MC_DATUM, P.ink);
    txt(TR("(times in Settings)", "(tiempos en Ajustes)"), SCR_W / 2, 158, 1, MC_DATUM, P.inkDim);
    drawButton(8, 168, 150, 26, TR("Start", "Empezar"), true, P.ok);
    drawButton(162, 168, 150, 26, TR("Mode", "Modo"), false);
    return;
  }
  if (pomoState == POMO_READY) {
    txt(POMO_MODE_NAMES[pomoMode], SCR_W / 2, 92, 2, MC_DATUM, P.accent);
    txt(TR("Get ready:", "Preparate:"), SCR_W / 2, 112, 2, MC_DATUM, P.ink);
    txtFit(pomoRelaxLine(), SCR_W / 2, 132, SCR_W - 20, 2, MC_DATUM, P.ink);
    txt(TR("Sit well, water nearby,", "Sientate bien, agua cerca,"), SCR_W / 2, 150, 1, MC_DATUM, P.inkDim);
    txt(TR("phone away.", "movil lejos."), SCR_W / 2, 162, 1, MC_DATUM, P.inkDim);
    drawButton(8, 168, 150, 26, TR("Start focus", "Empezar foco"), true, P.ok);
    drawButton(162, 168, 150, 26, TR("Back", "Atras"), false);
    return;
  }
  // Running: rounded card with big time (a face, not a hard panel)
  bool focus = (pomoState == POMO_FOCUS);
  uint32_t r = pomoRemainS();
  spr.fillRoundRect(60, 88, 200, 76, 14, P.card);
  spr.drawRoundRect(60, 88, 200, 76, 14, focus ? P.accent : P.ok);
  snprintf(b, sizeof(b), "%s%s", focus ? TR("FOCUS ", "FOCO ") : TR("BREAK ", "DESCANSO "), POMO_MODE_NAMES[pomoMode]);
  txt(b, SCR_W / 2, 98, 1, MC_DATUM, focus ? P.accent : P.ok);
  snprintf(b, sizeof(b), "%02lu:%02lu", (unsigned long)(r / 60), (unsigned long)(r % 60));
  txt(b, SCR_W / 2, 132, 7, MC_DATUM, P.ink);
  // progress
  uint32_t tot = pomoTotalMs(); uint32_t el = pomoElapsed();
  float f = tot ? constrain((float)el / tot, 0, 1) : 0;
  spr.fillRoundRect(76, 158, 168, 6, 3, P.line);
  spr.fillRoundRect(77, 159, (168 - 2) * f, 4, 2, focus ? P.accent : P.ok);
  // (paused state is shown by the "Resume" button)
  drawButton(8, 168, 98, 26, pomoPaused ? TR("Resume", "Seguir") : TR("Pause", "Pausa"), false);
  drawButton(111, 168, 98, 26, TR("Cheer", "Animo"), true, P.accent);
  drawButton(214, 168, 98, 26, TR("End", "Fin"), false, P.danger);
}
