// ===========================================================================
//  pomodoro.h — Pomodoro v3: 1-tap start, inline times, real 4-cycle,
//  Flowtime modes, quiet mode, ambient progress, per-mode identity.
//  320x240 layout: max 2 buttons/row, h>=22, single hero datum per screen.
//  NOTE: TFT font lacks accents/arrows/· — keep strings plain ASCII.
// ===========================================================================
#pragma once
#ifndef POMO_LONG_DEFAULT
#define POMO_LONG_DEFAULT 20   // min
#endif

enum PomoState { POMO_IDLE, POMO_READY, POMO_FOCUS, POMO_BREAK, POMO_DONE };
enum PomoMode { POMO_TRABAJO = 0, POMO_ESCRITURA = 1, POMO_OCIO = 2, POMO_MODES = 3 };

int pomoFocusMin = POMO_FOCUS_DEFAULT;
int pomoBreakMin = POMO_BREAK_DEFAULT;
int pomoLongMin = POMO_LONG_DEFAULT;
int pomoMode = POMO_TRABAJO;
// Per-mode durations (mirror in pomoFocusMin/BreakMin = active mode)
int pomoFocusPerMode[POMO_MODES] = { 25, 50, 15 };
int pomoBreakPerMode[POMO_MODES] = { 5, 10, 15 };
int pomoTodayPerMode[POMO_MODES] = { 0, 0, 0 };
PomoState pomoState = POMO_IDLE;
uint32_t pomoT0 = 0, pomoPausedAcc = 0, pomoPauseStart = 0;
bool pomoPaused = false;
bool pomoQuiet = false;                 // tranqui: no cheer sounds, LED+toast only
uint32_t pomoLastCheer = 0;
uint32_t pomoEndArmMs = 0;              // confirm window for End
int pomoDoneCount = 0;                  // focus blocks completed (cycle position = %4)
int pomoTodayCount = 0;                 // derived daily total (sum of per-mode)
uint32_t pomoDayKey = 0;
uint32_t pomoCurMs = 0;                 // frozen total for the running block
int pomoFlowBreakMin = 5;               // computed proportional break (flow)
bool pomoLastBreakLong = false;
int pomoFlowNudgeIdx = 0;               // flow stop-cues fired (25/50/90')
// Voice task binding (RAM only: cleared on manual stop / fresh IDLE start)
char pomoTask[48] = "";
char pomoTip[96] = "";
bool pomoHasTask = false;
void pomoClearTask() { pomoTask[0] = 0; pomoTip[0] = 0; pomoHasTask = false; }
// Per-mode start/end jingles
Sound pomoGoSound() { return pomoMode == POMO_ESCRITURA ? SND_PE_GO : (pomoMode == POMO_OCIO ? SND_PO_GO : SND_PW_GO); }
Sound pomoEndSound() { return pomoMode == POMO_ESCRITURA ? SND_PE_END : (pomoMode == POMO_OCIO ? SND_PO_END : SND_PW_END); }

const char* const POMO_MODE_NAMES_L[LANG_COUNT][POMO_MODES] = {
  { "Work",    "Writing",   "Leisure" },
  { "Trabajo", "Escritura", "Ocio"    },
};
#define POMO_MODE_NAMES (POMO_MODE_NAMES_L[gLang])

void pomoInit() {}
bool pomoRunning() { return pomoState == POMO_FOCUS || pomoState == POMO_BREAK; }
bool pomoIsFlow() { return pomoMode != POMO_TRABAJO; }
// Identity color per mode: Trabajo=accent, Escritura=info blue, Ocio=ok green
uint16_t pomoModeColor() { return pomoMode == POMO_ESCRITURA ? P.info : (pomoMode == POMO_OCIO ? P.ok : P.accent); }
float pomoEyeScale() { return pomoMode == POMO_ESCRITURA ? 0.34f : (pomoMode == POMO_OCIO ? 0.30f : 0.32f); }
int pomoTodayTotal() { return pomoTodayPerMode[0] + pomoTodayPerMode[1] + pomoTodayPerMode[2]; }

void pomoSyncModeDur() {
  pomoMode = constrain(pomoMode, 0, POMO_MODES - 1);
  pomoFocusMin = pomoFocusPerMode[pomoMode];
  pomoBreakMin = pomoBreakPerMode[pomoMode];
}
void pomoWriteSlot() {
  char k[8];
  snprintf(k, sizeof(k), "pF%d", pomoMode); prefs.putUChar(k, (uint8_t)pomoFocusPerMode[pomoMode]);
  snprintf(k, sizeof(k), "pB%d", pomoMode); prefs.putUChar(k, (uint8_t)pomoBreakPerMode[pomoMode]);
  prefs.putUChar("pFoc", (uint8_t)pomoFocusMin);   // legacy mirror
  prefs.putUChar("pBrk", (uint8_t)pomoBreakMin);
  prefs.putUChar("pMode", (uint8_t)pomoMode);
}
// Inline feedback (never covers the buttons): transient line drawn inside the
// pomodoro layout. pomoFeedback also toasts when the user is on another page
// (e.g. blind pause via sensor while away).
char pomoMsg[80] = "";
uint32_t pomoMsgUntil = 0;
bool pomoMsgFresh() { return pomoMsg[0] && (int32_t)(millis() - pomoMsgUntil) < 0; }
void pomoSay(const char* s, uint32_t ms = 1500) {
  strlcpy(pomoMsg, s, sizeof(pomoMsg));
  pomoMsgUntil = millis() + ms;
  gDirty = true;
}
void pomoFeedback(const char* s, uint32_t ms = 1500) {
  pomoSay(s, ms);
  if (gPage != PAGE_POMO) toast(s, ms);
}

uint32_t pomoTotalMs() { return pomoCurMs; }
uint32_t pomoElapsed() {
  if (pomoState != POMO_FOCUS && pomoState != POMO_BREAK) return 0;
  uint32_t now = millis();
  return pomoPausedAcc + (pomoPaused ? 0 : (now - pomoT0));
}
uint32_t pomoRemainS() {
  if (pomoState != POMO_FOCUS && pomoState != POMO_BREAK) return 0;
  if (pomoState == POMO_FOCUS && pomoIsFlow()) return 0;  // count-up: no remain
  uint32_t tot = pomoCurMs, el = pomoElapsed();
  return el >= tot ? 0 : (tot - el) / 1000;
}
// Suggested break for flow: worked/5, clamped 2'..long
int pomoFlowSuggested() {
  uint32_t eMin = pomoElapsed() / 60000UL;
  return constrain((int)((eMin + 4) / 5), 2, pomoLongMin);
}

const char* pomoRelaxLine() {
  switch (pomoMode) {
    case POMO_ESCRITURA: return TR("Drop your shoulders. 3 breaths.", "Suelta hombros. 3 respiraciones.");
    case POMO_OCIO:      return TR("Get comfy. Breathe slowly.", "Ponte comodo. Respira lento.");
    default:             return TR("Sit up straight. Look far 20 s.", "Espalda recta. Mira lejos 20 s.");
  }
}
const char* pomoCheerLine() {
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
// Flow stop-cues (closing-oriented, not pushing): 25 / 50 / 90 min
const char* pomoFlowNudge(int idx) {
  switch (idx) {
    case 0: return TR("Good point to stop.", "Buen punto para parar.");
    case 1: return TR("50 min in, wrap up.", "50 min, ve cerrando.");
    default: return TR("Close with dignity.", "Cierra con dignidad.");
  }
}

void pomoSaveToday() {
  pomoTodayCount = pomoTodayTotal();
  prefs.putUShort("pT0", (uint16_t)pomoTodayPerMode[0]);
  prefs.putUShort("pT1", (uint16_t)pomoTodayPerMode[1]);
  prefs.putUShort("pT2", (uint16_t)pomoTodayPerMode[2]);
  prefs.putUShort("pToday", (uint16_t)pomoTodayCount);
  prefs.putUInt("pDay", pomoDayKey);
}
void pomoTodayTick() {
  struct tm t;
  if (!getLocal(t)) return;
  uint32_t key = (uint32_t)(t.tm_year + 1900) * 10000UL + (uint32_t)(t.tm_mon + 1) * 100UL + (uint32_t)t.tm_mday;
  if (pomoDayKey != key) {
    pomoDayKey = key;
    pomoTodayPerMode[0] = pomoTodayPerMode[1] = pomoTodayPerMode[2] = 0;
    pomoSaveToday();
  }
}
void pomoCountDone() {
  pomoDoneCount++;
  pomoTodayPerMode[constrain(pomoMode, 0, POMO_MODES - 1)]++;
  pomoSaveToday();
}

void pomoStartFocus() {
  pomoState = POMO_FOCUS; pomoT0 = millis(); pomoPausedAcc = 0; pomoPaused = false;
  pomoCurMs = pomoIsFlow() ? 0 : (uint32_t)pomoFocusMin * 60000UL;
  pomoFlowBreakMin = pomoBreakMin;
  pomoLastBreakLong = false;
  pomoEndArmMs = 0;
  pomoFlowNudgeIdx = 0;
  pomoLastCheer = millis();
  setMoodFor(M_HAPPY, 2500); squash = 1.0f;
  sound(pomoGoSound());
  gDirty = true;
}
void pomoStartBreak(int breakMin, bool isLong) {
  pomoState = POMO_BREAK; pomoT0 = millis(); pomoPausedAcc = 0; pomoPaused = false;
  pomoCurMs = (uint32_t)constrain(breakMin, 1, 60) * 60000UL;
  pomoFlowBreakMin = breakMin;
  pomoLastBreakLong = isLong;
  pomoEndArmMs = 0;
  setMoodFor(M_LOVE, 3000); squash = 1.0f;
  sound(pomoEndSound());
  gDirty = true;
}
void pomoStop() {
  pomoState = POMO_IDLE; pomoPaused = false; pomoPausedAcc = 0; pomoEndArmMs = 0;
  pomoClearTask();
  gDirty = true;
}

// Fixed pause: accumulate worked time so pause never loses progress.
void pomoTogglePause() {
  uint32_t now = millis();
  if (!pomoPaused) { pomoPausedAcc += now - pomoT0; pomoPaused = true; pomoPauseStart = now; pomoFeedback(TR("Pomo paused", "Pomo en pausa"), 1000); }
  else { pomoPaused = false; pomoT0 = now; pomoFeedback(TR("Resuming", "Seguimos"), 1000); }
  gDirty = true;
}

bool pomoEndConfirm() {
  uint32_t now = millis();
  if (now - pomoEndArmMs > 3000UL) {
    pomoEndArmMs = now;
    pomoSay(TR("Tap End again to finish", "Toca Fin para terminar"), 2500);
    gDirty = true;
    return false;
  }
  pomoEndArmMs = 0;
  return true;
}

void pomoTick() {
  pomoTodayTick();
  if (pomoState != POMO_FOCUS && pomoState != POMO_BREAK) return;
  if (pomoPaused) return;
  uint32_t now = millis();
  if (pomoState == POMO_FOCUS && !pomoIsFlow() && pomoElapsed() >= pomoCurMs) {
    pomoCountDone();
    bool isLong = (pomoDoneCount % 4 == 0);
    pomoStartBreak(isLong ? pomoLongMin : pomoBreakMin, isLong);
    toast(isLong ? TR("4 done! Long rest", "4 hechos! Descanso largo")
                 : TR("Focus done! Rest", "Foco hecho! Descansa"), 2000);
    gDirty = true;
    return;
  }
  if (pomoState == POMO_FOCUS && pomoIsFlow()) {
    // Flow stop-cues at 25/50/90 min (toast always, sound unless quiet)
    static const uint32_t MARKS[3] = { 25 * 60000UL, 50 * 60000UL, 90 * 60000UL };
    if (pomoFlowNudgeIdx < 3 && pomoElapsed() >= MARKS[pomoFlowNudgeIdx]) {
      pomoSay(pomoFlowNudge(pomoFlowNudgeIdx), 2000);
      setMoodFor(M_HAPPY, 2000);
      if (!pomoQuiet) sound(SND_TICK);
      pomoFlowNudgeIdx++;
    }
  }
  if (pomoState == POMO_BREAK && pomoElapsed() >= pomoCurMs) {
    if (pomoLastBreakLong) {
      pomoState = POMO_DONE; pomoT0 = now;
      setMoodFor(M_HAPPY, 5000); squash = 1.0f;
      sound(SND_DONE);
      toast(TR("Set complete! Nice", "Serie completa! Bien"), 2000);
    } else {
      // Auto-chain short break -> next focus (true pomodoro cycle)
      pomoState = POMO_FOCUS; pomoT0 = now; pomoPausedAcc = 0; pomoPaused = false;
      pomoCurMs = pomoIsFlow() ? 0 : (uint32_t)pomoFocusMin * 60000UL;
      pomoFlowNudgeIdx = 0;
      pomoLastCheer = now;
      setMoodFor(M_HAPPY, 2500); squash = 1.0f;
      sound(pomoGoSound());
      toast(TR("Back to focus", "De vuelta al foco"), 1500);
    }
    gDirty = true;
    return;
  }
  // Encouragement every ~5 min during focus (skipped in quiet mode)
  if (pomoState == POMO_FOCUS && !pomoQuiet && now - pomoLastCheer > 5 * 60000UL) {
    pomoLastCheer = now;
    pomoSay(pomoCheerLine(), 1500);
    setMoodFor(M_HAPPY, 2000);
    sound(SND_TICK);
  }
}

static int pomoCycleStep(int cur, const int* arr, int n, int dir) {
  int i = 0;
  while (i < n - 1 && arr[i] != cur) i++;
  if (arr[i] != cur) return arr[0];
  return arr[(i + dir + n) % n];
}

void pomoTap(int x, int y) {
  // Legacy READY (kept for sensor compat): left=start, right=back, else start.
  if (pomoState == POMO_READY) {
    if (y >= 168) {
      if (x < 160) pomoStartFocus();
      else { pomoState = POMO_IDLE; toast("Pomodoro", 800); }
    } else pomoStartFocus();
    return;
  }
  if (pomoState == POMO_IDLE || pomoState == POMO_DONE) {
    // Mode pills y 90..114: tap = select + sync that mode's durations
    if (y >= 90 && y < 114) {
      int m = x < 111 ? 0 : (x < 214 ? 1 : 2);
      if (m != pomoMode) {
        pomoMode = m; pomoSyncModeDur(); pomoWriteSlot();
        pomoSay(POMO_MODE_NAMES[pomoMode], 900); gDirty = true;
      }
      return;
    }
    // Focus stepper y 116..140 (edits ACTIVE mode slot)
    if (y >= 116 && y < 140) {
      static const int FS[4] = { 15, 25, 35, 45 };
      int dir = (x < 64) ? -1 : +1;
      pomoFocusMin = pomoCycleStep(pomoFocusMin, FS, 4, dir);
      pomoFocusPerMode[pomoMode] = pomoFocusMin;
      pomoWriteSlot();
      char b[24]; snprintf(b, sizeof(b), TR("Focus %d min", "Foco %d min"), pomoFocusMin); pomoSay(b, 900);
      gDirty = true;
      return;
    }
    // Break stepper y 142..166 (edits ACTIVE mode slot)
    if (y >= 142 && y < 166) {
      static const int BS[3] = { 5, 10, 15 };
      int dir = (x < 64) ? -1 : +1;
      pomoBreakMin = pomoCycleStep(pomoBreakMin, BS, 3, dir);
      pomoBreakPerMode[pomoMode] = pomoBreakMin;
      pomoWriteSlot();
      char b[24]; snprintf(b, sizeof(b), TR("Break %d min", "Descanso %d min"), pomoBreakMin); pomoSay(b, 900);
      gDirty = true;
      return;
    }
    if (y >= 168) {
      if (x < 204) {
        bool repeat = (pomoState == POMO_DONE);
        if (!repeat) pomoClearTask();
        pomoStartFocus();
        if (pomoHasTask) pomoSay(pomoTask, 1500);
        else pomoSay(pomoRelaxLine(), 1800);
      }
      else {
        pomoQuiet = !pomoQuiet; prefs.putBool("pQuiet", pomoQuiet);
        pomoSay(pomoQuiet ? TR("Quiet: LED only", "Tranqui: solo LED") : TR("Sound on", "Sonido on"), 1000);
        gDirty = true;
      }
      return;
    }
    return;
  }
  // FOCUS / BREAK running: card tap = cheer (or relax hint in BREAK), bottom = Pause | End
  if (y < 168) {
    if (pomoState == POMO_BREAK) pomoSay(pomoRelaxLine(), 1800);
    else if (!pomoQuiet) { pomoSay(pomoCheerLine(), 1500); setMoodFor(M_HAPPY, 2000); }
    else pomoSay(pomoRelaxLine(), 1500);
    gDirty = true;
    return;
  }
  if (x < 160) { pomoTogglePause(); return; }
  // End / Descanso with 3 s confirm
  if (!pomoEndConfirm()) return;
  if (pomoState == POMO_FOCUS && pomoIsFlow()) {
    // Flow: finish focus -> proportional break (worked/5, capped by long)
    pomoCountDone();
    bool isLong = (pomoDoneCount % 4 == 0);
    int prop = pomoFlowSuggested();
    pomoStartBreak(isLong ? pomoLongMin : prop, isLong);
    pomoSay(pomoRelaxLine(), 2000);
  } else {
    pomoStop();
    pomoSay(TR("Pomo ended", "Pomo terminado"), 1200);
  }
}

// --- Drawing: 2 states, max 2 buttons/row, h>=22 ---
void pomoDots(int cx, int y) {
  int pos = pomoDoneCount % 4;
  uint16_t dc = pomoModeColor();
  for (int i = 0; i < 4; i++) {
    int dx = cx + (int)((i - 1.5f) * 18);
    if (i < pos) spr.fillCircle(dx, y, 4, dc);
    else spr.drawCircle(dx, y, 4, P.inkDim);
  }
}
void drawPomoPage() {
  drawHeader("POMODORO");
  char b[64];
  drawEyesAt(SCR_W / 2, 54, pomoEyeScale());

  if (pomoState == POMO_IDLE || pomoState == POMO_DONE) {
    if (pomoMsgFresh()) {
      txtFit(pomoMsg, SCR_W / 2, 78, SCR_W - 24, 2, MC_DATUM, P.ink);
    } else if (pomoState == POMO_DONE) {
      snprintf(b, sizeof(b), TR("Set complete! Today %d", "Serie completa! Hoy %d"), pomoTodayTotal());
      txt(b, SCR_W / 2, 78, 2, MC_DATUM, P.ok);
    } else {
      pomoDots(SCR_W / 2 - 40, 78);
      snprintf(b, sizeof(b), TR("Today %d", "Hoy %d"), pomoTodayTotal());
      txt(b, SCR_W / 2 + 52, 78, 2, MC_DATUM, P.inkDim);
    }
    // Mode pills in identity color + today count per mode
    for (int i = 0; i < 3; i++) {
      int bx = 8 + i * 103;
      bool sel = (i == pomoMode);
      uint16_t mc = (i == POMO_ESCRITURA) ? P.info : (i == POMO_OCIO ? P.ok : P.accent);
      spr.fillRoundRect(bx, 90, 97, 22, 8, sel ? mc : P.card);
      if (!sel) spr.drawRoundRect(bx, 90, 97, 22, 8, P.line);
      if (pomoTodayPerMode[i] > 0) {
        snprintf(b, sizeof(b), "%s %d", POMO_MODE_NAMES_L[gLang][i], pomoTodayPerMode[i]);
        txt(b, bx + 48, 101, 1, MC_DATUM, sel ? P.bg : P.ink);
      } else {
        txt(POMO_MODE_NAMES_L[gLang][i], bx + 48, 101, 2, MC_DATUM, sel ? P.bg : P.ink);
      }
    }
    // Steppers edit the ACTIVE mode slot
    drawButton(8, 116, 48, 24, "-", false);
    snprintf(b, sizeof(b), "%d' %s", pomoFocusMin, TR("FOCUS", "FOCO"));
    txt(b, SCR_W / 2, 128, 2, MC_DATUM, pomoModeColor());
    drawButton(264, 116, 48, 24, "+", false);
    drawButton(8, 142, 48, 22, "-", false);
    snprintf(b, sizeof(b), "%d' %s L%d'", pomoBreakMin, TR("REST", "DESC"), pomoLongMin);
    txt(b, SCR_W / 2, 153, 2, MC_DATUM, P.inkDim);
    drawButton(264, 142, 48, 22, "+", false);
    drawButton(8, 168, 196, 26, pomoState == POMO_DONE ? TR("Again", "Otra vez") : TR("Start focus", "Empezar foco"), true, P.ok);
    drawButton(208, 168, 104, 26, pomoQuiet ? TR("Quiet", "Tranqui") : TR("Sound", "Sonido"), false);
    return;
  }
  if (pomoState == POMO_READY) {
    txt(POMO_MODE_NAMES[pomoMode], SCR_W / 2, 92, 2, MC_DATUM, pomoModeColor());
    txt(TR("Get ready:", "Preparate:"), SCR_W / 2, 112, 2, MC_DATUM, P.ink);
    txtFit(pomoRelaxLine(), SCR_W / 2, 132, SCR_W - 20, 2, MC_DATUM, P.ink);
    drawButton(8, 168, 150, 26, TR("Start focus", "Empezar foco"), true, P.ok);
    drawButton(162, 168, 150, 26, TR("Back", "Atras"), false);
    return;
  }
  // Running: identity color border, warn glow last 5' of fixed focus
  bool focus = (pomoState == POMO_FOCUS);
  bool flow = pomoIsFlow() && focus;
  uint32_t r = pomoRemainS();
  uint32_t e = pomoElapsed() / 1000;
  uint16_t mc = pomoModeColor();
  uint16_t border = focus ? ((!flow && r > 0 && r < 300) ? P.warn : mc) : mc;
  if (pomoPaused) border = P.line;
  spr.fillRoundRect(60, 84, 200, 72, 14, P.card);
  spr.drawRoundRect(60, 84, 200, 72, 14, border);
  int cyc = (pomoDoneCount % 4) + 1;
  if (pomoHasTask && focus) {
    char tk[22]; snprintf(tk, sizeof(tk), "%.20s", pomoTask);
    if (flow) snprintf(b, sizeof(b), "FLOW %s", tk);
    else snprintf(b, sizeof(b), "%s %d/4 %s", TR("FOCUS", "FOCO"), cyc, tk);
  } else if (flow) {
    snprintf(b, sizeof(b), "FLOW %s desc %d'", POMO_MODE_NAMES[pomoMode], (int)constrain(((e / 60) + 4) / 5, 2UL, (uint32_t)pomoLongMin));
  } else if (focus) {
    char endS[8] = "";
    time_t nn = time(nullptr);
    if (nn >= 1700000000) {
      time_t fe = nn + (time_t)r;
      struct tm* lp = localtime(&fe);
      if (lp) snprintf(endS, sizeof(endS), " %02d:%02d", lp->tm_hour, lp->tm_min);
    }
    snprintf(b, sizeof(b), "%s %s %d/4%s%s", TR("FOCUS", "FOCO"), POMO_MODE_NAMES[pomoMode], cyc, pomoPaused ? " II" : "", endS);
  } else {
    snprintf(b, sizeof(b), "%s %s%s", TR("REST", "DESC"), POMO_MODE_NAMES[pomoMode], pomoPaused ? " II" : "");
  }
  txt(b, SCR_W / 2, 94, 1, MC_DATUM, mc);
  if (flow) snprintf(b, sizeof(b), "+%02lu:%02lu", (unsigned long)(e / 60), (unsigned long)(e % 60));
  else snprintf(b, sizeof(b), "%02lu:%02lu", (unsigned long)(r / 60), (unsigned long)(r % 60));
  txt(b, SCR_W / 2, 126, 7, MC_DATUM, pomoPaused ? P.inkDim : P.ink);
  if (focus) pomoDots(SCR_W / 2, 142);
  float f = 0;
  if (!flow && pomoCurMs) f = constrain((float)pomoElapsed() / pomoCurMs, 0, 1);
  else if (flow) f = constrain((float)e / 3000.0f, 0, 1);  // fills toward 50'
  spr.fillRoundRect(76, 151, 168, 5, 2, P.line);
  if (f > 0.01f) spr.fillRoundRect(77, 152, (168 - 2) * f, 3, 1, mc);
  if (flow) {  // suggested-break ticks at 25' and 50'
    spr.drawFastVLine(76 + 168 / 2, 151, 5, P.inkDim);
    spr.drawFastVLine(76 + 168 - 1, 151, 5, P.inkDim);
  }
  // Dedicated message line (transient feedback, else break tip): never covers buttons
  if (pomoMsgFresh()) txtFit(pomoMsg, SCR_W / 2, 162, SCR_W - 24, 1, MC_DATUM, P.ink);
  else if (pomoState == POMO_BREAK) {
    if (pomoHasTask && pomoTip[0]) txtFit(pomoTip, SCR_W / 2, 162, SCR_W - 24, 1, MC_DATUM, P.inkDim);
    else txtFit(pomoRelaxLine(), SCR_W / 2, 162, SCR_W - 24, 1, MC_DATUM, P.inkDim);
  }
  bool armed = (millis() - pomoEndArmMs < 3000UL);
  drawButton(8, 168, 148, 26, pomoPaused ? TR("Resume", "Seguir") : TR("Pause", "Pausa"), false);
  if (focus && flow) drawButton(164, 168, 148, 26, armed ? TR("Sure?", "Seguro?") : TR("Rest", "Descanso"), armed, armed ? P.danger : mc);
  else drawButton(164, 168, 148, 26, armed ? TR("Sure?", "Seguro?") : TR("End", "Fin"), armed, armed ? P.danger : 0);
}
