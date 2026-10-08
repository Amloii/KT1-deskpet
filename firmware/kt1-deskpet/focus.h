// ===========================================================================
//  focus.h — "Active focus" cycle (20-8-2), active-break trainer with
//            dumbbells and daily/weekly stats (stored in NVS).
//
//  Cycle:  SIT 20' -> STAND 8' -> MOVE 2'  (x4 -> LONG BREAK 10')
//  - Changes wait for a good moment: not during calls or mid-sentence.
//  - If you leave for 5 min (pc-agent), the cycle restarts on return: you already moved.
// ===========================================================================
#pragma once

const char* phaseName(Phase p) {
  switch (p) {
    case PH_SIT:   return TR("Sitting", "Sentado");
    case PH_STAND: return TR("Standing", "De pie");
    case PH_MOVE:  return TR("Active break", "Pausa activa");
    case PH_LONG:  return TR("Long break", "Pausa larga");
    case PH_AWAY:  return TR("Away", "Fuera");
    default:       return TR("Off", "Apagado");
  }
}
uint32_t phaseMs(Phase p) {
  switch (p) {
    case PH_SIT:   return SIT_MIN   * 60000UL;
    case PH_STAND: return STAND_MIN * 60000UL;
    case PH_MOVE:  return MOVE_MIN  * 60000UL;
    case PH_LONG:  return LONGBREAK_MIN  * 60000UL;
    default:       return 0;
  }
}
uint32_t phaseElapsed() {
  if (gPhase == PH_OFF) return 0;
  return (gPaused ? pauseStart : millis()) - phaseStart;
}
uint32_t phaseRemainingS() {
  uint32_t d = phaseMs(gPhase), e = phaseElapsed();
  return e >= d ? 0 : (d - e) / 1000;
}
bool isPresent() {
  if (agentConnected()) return !agentAway();
  return gPhase != PH_OFF && gPhase != PH_AWAY;   // no agent: present if the cycle is running
}

// ---------------------------------------------------------------------------
//  Stats
// ---------------------------------------------------------------------------
void statsSave() {
  prefs.putBytes("day", &stats, sizeof(stats));
  prefs.putBytes("week", &week, sizeof(week));
}
void statsLoad() {
  if (prefs.getBytesLength("day") == sizeof(stats))  prefs.getBytes("day", &stats, sizeof(stats));
  if (prefs.getBytesLength("week") == sizeof(week))  prefs.getBytes("week", &week, sizeof(week));
}
int strengthDaysThisWeek() {              // days with legs + back + arms
  int n = 0;
  for (int i = 0; i < 7; i++) if ((week.mask[i] & 0x07) == 0x07) n++;
  return n;
}
void logExercise(const Exercise& e) {
  stats.sets[e.group]++;
  week.mask[todayIdx] |= (1 << e.group);
  coach.anyDone = true;
}
void statsTick() {
  static uint32_t last = 0, lastSave = 0;
  uint32_t now = millis();
  if (last == 0) { last = now; lastSave = now; return; }
  if (now - last < 1000) return;
  uint32_t secs = (now - last) / 1000;
  last += secs * 1000;

  struct tm t;
  if (getLocal(t)) {
    uint32_t key = (t.tm_year + 1900) * 10000UL + (t.tm_mon + 1) * 100UL + t.tm_mday;
    if (stats.day != key) {                                   // new day
      if (stats.day != 0) Serial.println("[TODAY] New day: stats reset");
      stats = DayStats(); stats.day = key;
      autoStartDay = 0;
    }
    uint32_t wk = (t.tm_year + 1900) * 100UL + isoWeek(t);
    if (week.week != wk) { week = WeekStats(); week.week = wk; }
    todayIdx = (t.tm_wday + 6) % 7;
  }
  bool exercising = coach.active && coach.st != CO_READY && coach.st != CO_DONE;
  if (exercising)                          stats.moveS  += secs;
  else if (gPhase == PH_AWAY || agentAway()) stats.awayS += secs;
  else if (isPresent()) {
    if (gPosture == POS_STAND) stats.standS += secs;
    else                       stats.sitS   += secs;
  }
  if (now - lastSave > 300000UL) { lastSave = now; statsSave(); }   // every 5 min (spares the flash)
}

// ---------------------------------------------------------------------------
//  Nudges and posture
// ---------------------------------------------------------------------------
void startNudge(const char* text, Posture target, Sound snd) {
  nudge.active = true;
  strlcpy(nudge.text, text, sizeof(nudge.text));
  nudge.target = target;
  nudge.since = nudge.lastRing = millis();
  nudge.rings = 1;
  sound(snd);
  setPage(PAGE_FACE, -1, false);   // silent: the nudge banner is the message
  lastInteraction = millis();
  gDirty = true;
  Serial.printf("[FOCUS] Nudge: %s\n", text);
}

// Manual posture signal (buttons, face, sensor, pages). fromDesk is kept
// for compatibility (the automatic desk detector was removed).
void confirmPosture(Posture p, bool fromDesk) {
  (void)fromDesk;
  if (p != gPosture) {
    gPosture = p;
    stats.changes++;
    sitSince = millis();
  }
  if (p == POS_STAND) sedLastMoveMs = millis();
  if (nudge.active && nudge.target == p) {
    nudge.active = false;
    squash = 1.0f;
    setMoodFor(M_HAPPY, 3000);
  }
  if (fromDesk) toast(p == POS_STAND ? TR("Desk up: stand!", "Mesa arriba: de pie!") : TR("Desk down: sit", "Mesa abajo: sentado"), 1800);
  else          toast(p == POS_STAND ? TR("On your feet!", "De pie. Bien!") : TR("Sitting", "Sentado"), 1200);
  gDirty = true;
}

// ---------------------------------------------------------------------------
//  Cycle
// ---------------------------------------------------------------------------
void setPhase(Phase p) {
  gPhase = p;
  phaseStart = millis();
  deferSince = 0;
  waitReason[0] = 0;
  gDirty = true;
  Serial.printf("[FOCUS] Phase: %s (cycle %d)\n", phaseName(p), cycleNo);
}
void cycleStart() {
  // Vital Dia es el unico director: Ejercicio delega en el (preset Cornell).
  vitalSetPreset(1);
  vitalStart();
  sitSince = millis();
}
void cycleStop() {
  vitalStop();
}
void cyclePauseToggle() {
  if (gPhase == PH_OFF) { cycleStart(); return; }
  vitalTogglePause();
}

// v2: no pc-agent. It's always a good moment (local nudges with sound).
bool goodMoment() {
  waitReason[0] = 0;
  return true;
}

void cycleTick() {
  uint32_t now = millis();

  if (gPhase == PH_OFF) return;   // Vital Dia manda; arranque manual desde Vital
  if (gPaused) return;

  // --- Repeated / expired reminders ---
  if (nudge.active) {
    if (now - nudge.lastRing > RENUDGE_MIN * 60000UL && nudge.rings < 3) {
      nudge.lastRing = now; nudge.rings++;
      sound(nudge.target == POS_STAND ? SND_STAND : SND_SIT);
    }
    if (now - nudge.since > 10 * 60000UL) nudge.active = false;
  }

  // Vital Dia es el unico temporizador: si el dirige, no avanzar aqui
  // (evita doble transicion con vitalTick). Solo quedan los nudges legacy.
  if (gVitalDrives) return;

  uint32_t el = phaseElapsed();
  switch (gPhase) {
    case PH_SIT:
      if (el >= phaseMs(PH_SIT) && goodMoment()) {
        setPhase(PH_STAND);
        if (gPosture != POS_STAND) startNudge(TR("Raise desk: stand 8 min", "Sube la mesa: 8 min de pie"), POS_STAND, SND_STAND);
      }
      break;
    case PH_STAND:
      if (el >= phaseMs(PH_STAND) && goodMoment()) {
        bool isLong = (cycleNo % LONG_EVERY) == 0;
        nudge.active = false;
        setPhase(isLong ? PH_LONG : PH_MOVE);
        coachOpen(isLong);
      }
      break;
    case PH_MOVE:
    case PH_LONG:
      if (!coach.active || el > phaseMs(gPhase) + 5 * 60000UL) {
        if (coach.active) coachClose();
        cycleNo++;
        setPhase(PH_SIT);
        if (gPosture != POS_SIT) startNudge(TR("Back to work: lower desk", "A trabajar: baja la mesa"), POS_SIT, SND_SIT);
      }
      break;
    default: break;
  }
}

// ---------------------------------------------------------------------------
//  Anti-sedentary: safety net if you stay seated for more than X min.
//  Complements the 20-8-2 cycle (doesn't replace it): if you follow the
//  stand-up nudges this timer almost never fires because confirmPosture()
//  and the trainer reset it. If you ignore them, it launches 1 mixed exercise
//  (same LEGS->BACK->ARMS rotation as coachOpen) with Done/Skip buttons.
//  Respects calls/typing via goodMoment(), night and pauses.
// ---------------------------------------------------------------------------
void sedMarkMoved() { sedLastMoveMs = millis(); }

int sedRemainingS() {
  if (!sedEnabled || sedEveryMin == 0) return -1;
  uint32_t now = millis();
  uint32_t target = (uint32_t)sedEveryMin * 60000UL;
  uint32_t el = now - sedLastMoveMs;
  if (el >= target) return 0;
  return (target - el) / 1000;
}

void sedTick() {
  uint32_t now = millis();
  if (!sedEnabled || sedEveryMin == 0) return;
  if (coach.active || gPaused) return;
  if (isNight()) { sedLastMoveMs = now; return; }

  // Presence for anti-sedentary: with pc-agent uses idle/call; without agent
  // assume present (except AWAY phase) so it also works with the cycle OFF.
  bool present;
  if (agentConnected()) present = !agentAway();
  else                  present = (gPhase != PH_AWAY);
  if (!present) { sedLastMoveMs = now; return; }

  // Any non-sedentary state resets the counter
  if (gPosture == POS_STAND) { sedLastMoveMs = now; return; }
  if (gPhase == PH_MOVE || gPhase == PH_LONG || gPhase == PH_STAND || gPhase == PH_AWAY) {
    sedLastMoveMs = now;
    return;
  }

  if (now - sedLastMoveMs < (uint32_t)sedEveryMin * 60000UL) return;

  // Don't step on the cycle: if SIT will ask to stand in <2 min, let the cycle do it
  if (gPhase == PH_SIT && phaseRemainingS() < 120) return;
  if (nudge.active) return;   // a posture nudge is already on screen
  if (!goodMoment()) return;  // on a call or typing: wait (sets waitReason)

  coachOpen(false);           // 1 mixed exercise; on a call it picks silent mobility
  sedLastMoveMs = now;
  Serial.printf("[SED] %d min seated in a row -> exercise %s\n", sedEveryMin, EXERCISES[coach.ex].name());
}

// FOCUS page buttons (above the page dots)
#define FOCUS_BTN_Y 168
#define FOCUS_BTN_H 26   // ends at 194: above the nav touch zone (196)

void focusTap(int x, int y) {
  if (y >= FOCUS_BTN_Y - 6 && y < NAV_TOUCH_Y) {   // row of 3 buttons: Posture | Move | Pause
    if (x < 111)       confirmPosture(gPosture == POS_SIT ? POS_STAND : POS_SIT, false);
    else if (x < 214) { if (!coach.active) { coachOpen(false); toast(TR("Manual break", "Pausa manual"), 1200); Serial.println("[COACH] manual from FOCUS"); } }
    else               cyclePauseToggle();
  } else if (gPhase == PH_OFF) {
    cycleStart();
  } else {
    toast(TR("Use the buttons below", "Usa los botones de abajo"), 1200);
  }
}

// TODAY page settings buttons: Sound | Brightness | Ink
#define STATS_BTN_Y 170
#define STATS_BTN_H 30

void statsTap(int x, int y) {
  // Settings moved to the Sound/Screen/Timers pages — no buttons here
  (void)x; (void)y;
}

// ---------------------------------------------------------------------------
//  Trainer (active breaks)
// ---------------------------------------------------------------------------
int lastUsed[GR_COUNT] = { -1, -1, -1, -1 };
int groupRot = 0;

int pickNext(uint8_t g) {
  for (int k = 1; k <= EX_COUNT; k++) {
    int i = (lastUsed[g] + k + EX_COUNT) % EX_COUNT;
    if (EXERCISES[i].group == g) { lastUsed[g] = i; return i; }
  }
  return 0;
}

void coachOpen(bool circuit) {
  coach = Coach();
  coach.active = true;
  coach.circuit = circuit;
  if (callActive()) {                           // on a call: silent mobility
    coach.list[coach.n++] = pickNext(GR_MOB);
  } else if (circuit) {
    for (int r = 0; r < CIRCUIT_ROUNDS; r++) {
      coach.list[coach.n++] = pickNext(GR_LEG);
      coach.list[coach.n++] = pickNext(GR_BACK);
      coach.list[coach.n++] = pickNext(GR_ARM);
    }
    coach.list[coach.n++] = pickNext(GR_MOB);
  } else {
    coach.list[coach.n++] = pickNext(groupRot % 3);   // LEGS -> BACK -> ARMS
    groupRot++;
  }
  coach.ex = coach.list[0];
  coach.st = CO_READY;
  coach.t0 = millis();
  gPosture = POS_STAND;                          // exercises are done standing
  sound(SND_MOVE);
  lastInteraction = millis();
  gDirty = true;
  Serial.printf("[COACH] %s: %d exercise(s), first %s\n", circuit ? "Long break" : "Active break", coach.n, EXERCISES[coach.ex].name());
}

void coachClose() {
  if (!coach.active) return;
  if (coach.anyDone) { stats.breaksDone++; sedLastMoveMs = millis(); }
  else stats.breaksSkip++;
  coach.active = false;
  coach.st = CO_OFF;
  statsSave();
  gDirty = true;
}

void coachBegin() {                              // 3-2-1 countdown
  coach.st = CO_GO; coach.t0 = millis();
  coach.side = 0; coach.rep = 0; coach.lastCount = -1;
}

void exerciseSideDone() {
  const Exercise& e = EXERCISES[coach.ex];
  uint32_t now = millis();
  if (e.perSide && coach.side == 0) {
    coach.side = 1; coach.rep = 0;
    coach.st = CO_SWITCH; coach.t0 = now;
    sound(SND_SIDE);
    return;
  }
  logExercise(e);
  coach.pos++;
  if (coach.pos < coach.n) {
    coach.ex = coach.list[coach.pos];
    coach.st = CO_REST; coach.t0 = now;
  } else {
    coach.st = CO_DONE; coach.t0 = now;
    sound(SND_DONE);
    squash = 1.0f;
  }
}

void coachTick() {
  if (!coach.active) return;
  uint32_t now = millis();
  const Exercise& e = EXERCISES[coach.ex];
  switch (coach.st) {
    case CO_READY:
      if (now - coach.t0 > 3 * 60000UL) { toast(TR("Break skipped", "Pausa saltada")); coachClose(); }
      break;
    case CO_GO: {
      int left = 3 - (int)((now - coach.t0) / 1000);
      if (left != coach.lastCount && left > 0) { coach.lastCount = left; sound(SND_TICK); gDirty = true; }
      if (now - coach.t0 >= 3000) {
        coach.st = CO_WORK; coach.t0 = now; coach.lastBeat = now; coach.rep = 0;
        sound(SND_GO);
      }
    } break;
    case CO_WORK:
      if (e.reps > 0) {
        if (now - coach.lastBeat >= e.tempoMs) {
          coach.lastBeat += e.tempoMs;
          coach.rep++;
          squash = 1.0f;                           // the eyes "bounce" with each rep
          if (coach.rep >= e.reps) exerciseSideDone();
          else sound(SND_TICK);
          gDirty = true;
        }
      } else if (now - coach.t0 >= e.holdSec * 1000UL) {
        exerciseSideDone();
      }
      break;
    case CO_SWITCH:
      if (now - coach.t0 >= 4000) { coach.st = CO_WORK; coach.t0 = now; coach.lastBeat = now; coach.rep = 0; sound(SND_GO); }
      break;
    case CO_REST:
      if (now - coach.t0 >= REST_SEC * 1000UL) coachBegin();
      break;
    case CO_DONE:
      if (now - coach.t0 >= 3500) coachClose();
      break;
    default: break;
  }
}

#define COACH_BTN_Y 206
#define COACH_BTN_H 30

void coachSwap(bool noWeights) {
  const Exercise& e = EXERCISES[coach.ex];
  coach.ex = coach.list[coach.pos] = pickNext(noWeights ? (uint8_t)GR_MOB : e.group);
  toast(noWeights ? TR("No weights", "Sin pesas") : EXERCISES[coach.ex].name(), 800);
}

void coachGesture(Gesture g) {
  const Exercise& e = EXERCISES[coach.ex];
  // Bottom buttons (only when shown: ready or rest)
  if (g == G_TAP && (coach.st == CO_READY || coach.st == CO_REST) && ts.y >= COACH_BTN_Y - 8) {
    if (ts.x < 108)      coachSwap(false);                        // Other
    else if (ts.x < 212) { if (e.weights) coachSwap(true); }      // No weights (hidden if it no longer applies)
    else { toast(coach.anyDone ? TR("Break finished", "Pausa terminada") : TR("Break skipped", "Pausa saltada"), 1000); coachClose(); }
    return;
  }
  switch (g) {
    case G_TAP:
      if      (coach.st == CO_READY)  coachBegin();
      else if (coach.st == CO_WORK)   exerciseSideDone();          // "I'm done"
      else if (coach.st == CO_SWITCH) { coach.t0 = millis() - 4000; }
      else if (coach.st == CO_REST)   coachBegin();
      else if (coach.st == CO_DONE)   coachClose();
      break;
    case G_SWIPE_L:
    case G_SWIPE_R:
      if (coach.st == CO_READY || coach.st == CO_REST) coachSwap(false);
      break;
    case G_SWIPE_D:
      if (coach.st == CO_READY || coach.st == CO_REST) coachSwap(true);
      break;
    case G_SWIPE_U:
    case G_LONG:
      toast(coach.anyDone ? TR("Break finished", "Pausa terminada") : TR("Break skipped", "Pausa saltada"), 1000);
      coachClose();
      break;
    default: break;
  }
}

// v2: no pc-agent. No PC greeting.
void handleAgentGreet() {}

// ---------------------------------------------------------------------------
//  Screens: EXERCISE, TODAY and TRAINER
// ---------------------------------------------------------------------------
uint16_t groupColor(uint8_t g);
void drawButton(int x, int y, int w, int h, const char* label, bool filled, uint16_t col = 0) {
  uint16_t c = col ? col : P.accent;
  if (filled) { spr.fillRoundRect(x, y, w, h, 8, c); txt(label, x + w / 2, y + h / 2, 2, MC_DATUM, P.bg); }
  else        { spr.fillRoundRect(x, y, w, h, 8, P.card); spr.drawRoundRect(x, y, w, h, 8, P.line); txt(label, x + w / 2, y + h / 2, 2, MC_DATUM, P.ink); }
}

void drawFocusPage() {
  // VITAL DIA: Exercise es biblioteca + entrenador. El ciclo lo lleva Vital.
  drawHeader(TR("EXERCISE", "EJERCICIO"));
  char b[56];
  // today's plan: 3 exercises (legs/back/arms) rotating by day of year
  struct tm t; int doy = 0;
  if (getLocal(t)) doy = t.tm_yday;
  int ids[3] = {0, 0, 0}, found[3] = {0, 0, 0};
  for (int k = 0; k < EX_COUNT; k++) {
    uint8_t g = EXERCISES[k].group;
    if (g < 3 && !found[g]) {
      // rotate within each group by day
      int cnt = 0; for (int j = 0; j < EX_COUNT; j++) if (EXERCISES[j].group == g) cnt++;
      int want = (doy + g) % max(1, cnt);
      int seen = 0;
      for (int j = 0; j < EX_COUNT; j++) if (EXERCISES[j].group == g) {
        if (seen == want) { ids[g] = j; found[g] = 1; break; }
        seen++;
      }
    }
  }
  drawEyesAt(SCR_W / 2, 50, 0.34f);
  txt(TR("Today:", "Hoy toca:"), 12, 72, 1, TL_DATUM, P.inkDim);
  for (int g = 0; g < 3; g++) {
    const Exercise& e = EXERCISES[ids[g]];
    uint16_t gc = groupColor(e.group);
    spr.fillCircle(20, 88 + g * 18, 3, gc);
    if (e.reps) snprintf(b, sizeof(b), "%s - %d reps", e.name(), e.reps);
    else snprintf(b, sizeof(b), "%s - %d s", e.name(), e.holdSec);
    txt(b, 30, 88 + g * 18, 1, TL_DATUM, P.ink);
  }
  // Vital Dia dirige (gPhase es espejo de Vital). Aqui solo estado + entrenador.
  if (gPhase == PH_OFF) {
    txt(TR("Run from Vital Day", "Se lleva desde Vital"), SCR_W / 2, 140, 2, MC_DATUM, P.ink);
    txt(TR("(Cornell 20-8-2, sound alerts)", "(Cornell 20-8-2, avisos con sonido)"), SCR_W / 2, 156, 1, MC_DATUM, P.inkDim);
  } else {
    uint32_t rem = phaseRemainingS();
    uint16_t pc = phaseColor(gPhase);
    snprintf(b, sizeof(b), "%s %02lu:%02lu%s", phaseName(gPhase),
             (unsigned long)(rem / 60), (unsigned long)(rem % 60), gPaused ? " II" : "");
    txt(b, SCR_W / 2, 140, 2, MC_DATUM, pc);
    // compact 20-8-2 bar
    int x0 = 30, bw = SCR_W - 60, y = 156, h = 8;
    float tot = SIT_MIN + STAND_MIN + MOVE_MIN;
    int w1 = bw * SIT_MIN / tot, w2 = bw * STAND_MIN / tot;
    float frac = phaseMs(gPhase) ? min(1.0f, (float)phaseElapsed() / phaseMs(gPhase)) : 1.0f;
    spr.fillRoundRect(x0, y, bw, h, 3, P.card);
    spr.fillRect(x0 + 1, y + 2, (w1 - 2) * (gPhase == PH_SIT ? frac : 1.0f), h - 4, phaseColor(PH_SIT));
    spr.fillRect(x0 + w1, y + 2, w2 * (gPhase == PH_STAND ? frac : ((gPhase == PH_MOVE || gPhase == PH_LONG) ? 1.0f : 0.0f)), h - 4, phaseColor(PH_STAND));
  }

  drawButton(8, FOCUS_BTN_Y, 97, FOCUS_BTN_H, gPosture == POS_STAND ? TR("Standing", "De pie") : TR("Sitting", "Sentado"), gPosture == POS_STAND, gPosture == POS_STAND ? P.ok : phaseColor(PH_SIT));
  drawButton(111, FOCUS_BTN_Y, 97, FOCUS_BTN_H, TR("Move", "Mover"), true, phaseColor(PH_MOVE));
  drawButton(214, FOCUS_BTN_Y, 98, FOCUS_BTN_H, gPhase == PH_OFF ? TR("Start", "Iniciar") : (gPaused ? TR("Resume", "Seguir") : TR("Pause", "Pausar")), gPhase == PH_OFF, gPhase == PH_OFF ? P.ok : 0);
}

void drawStatsPage() {
  drawHeader(TR("TODAY", "HOY"));
  char b[72], d1[16], d2[16];
  // standing + movement goal
  uint32_t active = stats.standS + stats.moveS;
  fmtDur(active, d1, sizeof(d1));
  snprintf(b, sizeof(b), TR("Standing + moving: %s / %dh", "De pie + movimiento: %s / %dh"), d1, STAND_GOAL_MIN / 60);
  txt(b, 12, 28, 2, TL_DATUM, P.ink);
  int bw = SCR_W - 24;
  float goalFrac = min(1.0f, active / (STAND_GOAL_MIN * 60.0f));
  spr.fillRoundRect(12, 45, bw, 9, 4, P.card);
  spr.fillRoundRect(14, 47, (bw - 4) * goalFrac, 5, 2, goalFrac >= 1.0f ? P.ok : P.accent);

  fmtDur(stats.sitS, d1, sizeof(d1)); fmtDur(stats.awayS, d2, sizeof(d2));
  int y = 58;
  snprintf(b, sizeof(b), TR("Sitting %s   Away %s", "Sentado %s   Fuera %s"), d1, d2);                  txt(b, 12, y, 2, TL_DATUM, P.inkDim); y += 16;
  if (gPosture == POS_SIT && isPresent()) {
    fmtDur((millis() - sitSince) / 1000, d1, sizeof(d1));
    snprintf(b, sizeof(b), TR("Sitting streak: %s", "Sentado seguido: %s"), d1);                     txt(b, 12, y, 2, TL_DATUM, P.inkDim);
  }
  y += 16;
  snprintf(b, sizeof(b), TR("Breaks %u ok / %u skipped - changes %u", "Pausas %u ok / %u saltadas - cambios %u"), stats.breaksDone, stats.breaksSkip, stats.changes);
  txt(b, 12, y, 2, TL_DATUM, P.inkDim); y += 16;
  snprintf(b, sizeof(b), TR("Sets: legs %u back %u arms %u mob %u", "Series: pierna %u espalda %u brazo %u mov %u"), stats.sets[GR_LEG], stats.sets[GR_BACK], stats.sets[GR_ARM], stats.sets[GR_MOB]);
  txt(b, 12, y, 1, TL_DATUM, P.inkDim); y += 14;

  // week: days with full strength work (WHO/NHS: >= 2 days)
  int sd = strengthDaysThisWeek();
  snprintf(b, sizeof(b), TR("Week strength: %d/%d days%s", "Fuerza semana: %d/%d dias%s"), sd, STRENGTH_DAYS_GOAL, sd >= STRENGTH_DAYS_GOAL ? "  OK" : "");
  txt(b, 12, y, 2, TL_DATUM, sd >= STRENGTH_DAYS_GOAL ? P.ok : P.ink);
  const char* const* L = WEEK_INITIALS;
  for (int i = 0; i < 7; i++) {
    int x = 20 + i * 42, by = 142;
    uint8_t m = week.mask[i] & 0x07;
    if (m == 0x07)      spr.fillRoundRect(x, by, 30, 18, 4, P.ok);
    else if (m)         { spr.fillRoundRect(x, by, 30, 18, 4, P.card); spr.drawRoundRect(x, by, 30, 18, 4, P.line); spr.fillRect(x + 3, by + 11, 24, 4, P.accent); }
    else                { spr.fillRoundRect(x, by, 30, 18, 4, P.card); spr.drawRoundRect(x, by, 30, 18, 4, P.line); }
    txt(L[i], x + 15, by + 9, 2, MC_DATUM, m == 0x07 ? P.bg : P.inkDim);
    if (i == todayIdx) spr.drawFastHLine(x + 5, by + 21, 20, P.accent);
  }
  // Settings available on the SETTINGS page (swipe to the end)
  txt(TR("See the Settings page ->", "Ajustes en pagina Ajustes ->"), SCR_W / 2, STATS_BTN_Y + STATS_BTN_H / 2, 2, MC_DATUM, P.inkDim);
}

// Color per muscle group
uint16_t groupColor(uint8_t g) {
  switch (g) {
    case GR_LEG:  return tint(rgb( 96, 165, 250));   // legs blue
    case GR_BACK: return tint(rgb(167, 139, 250));   // back purple
    case GR_ARM:  return tint(rgb(251, 146,  60));   // arms orange
    default:      return P.ok;                        // mobility green
  }
}

// Trainer bottom row: Other | No weights | Skip/Finish
// (the "No weights" button is hidden if the current exercise doesn't use dumbbells: tapping
// it would do exactly the same as "Other", so it confused more than it helped)
// Touch zones (see coachGesture): x<108 | 108..212 | >212, y>=COACH_BTN_Y-8
void drawCoachButtons(const char* skipLabel, bool showNoWeights) {
  drawButton(6,   COACH_BTN_Y, 98, COACH_BTN_H, TR("Other", "Otro"), false);
  if (showNoWeights) drawButton(111, COACH_BTN_Y, 98, COACH_BTN_H, TR("No weights", "Sin pesas"), false);
  drawButton(216, COACH_BTN_Y, 98, COACH_BTN_H, skipLabel, false, P.danger);
}

// Centered pill (short, very readable label)
void coachPill(int cx, int y, int w, int h, const char* s, uint16_t bg, uint16_t fg, int font = 2) {
  spr.fillRoundRect(cx - w / 2, y, w, h, h / 2, bg);
  txt(s, cx, y + h / 2, font, MC_DATUM, fg);
}

// Cue on 1-2 lines (font 2). If it fits on one, one; otherwise split at the
// most central space into two lines. Always centered on (cx, y).
void coachCue2(const char* s, int cx, int y, int maxW) {
  if (spr.textWidth(s, 2) <= maxW) { txt(s, cx, y, 2, MC_DATUM, P.inkDim); return; }
  int n = strlen(s), best = -1, bestD = 99;
  for (int i = 0; i < n; i++) if (s[i] == ' ') { int d = abs(i - n / 2); if (d < bestD) { bestD = d; best = i; } }
  if (best < 0) { txtFit(s, cx, y, maxW, 2, MC_DATUM, P.inkDim); return; }
  char a[40], b2[40];
  strncpy(a, s, best); a[best] = 0;
  strlcpy(b2, s + best + 1, sizeof(b2));
  txt(a, cx, y - 9, 2, MC_DATUM, P.inkDim);
  txt(b2, cx, y + 9, 2, MC_DATUM, P.inkDim);
}

// Session progress dots (only if there's more than 1 exercise, e.g. long break).
// They sit in the header's central gap: no screen height lost in an already tight area.
void drawCoachProgress() {
  if (coach.n <= 1) return;
  int sp = 11, x0 = SCR_W / 2 - (coach.n - 1) * sp / 2, y = 12;
  for (int i = 0; i < coach.n; i++) {
    if (i < coach.pos)      spr.fillCircle(x0 + i * sp, y, 3, P.ok);       // done
    else if (i == coach.pos) spr.fillCircle(x0 + i * sp, y, 3, P.accent);  // in progress
    else                     spr.drawCircle(x0 + i * sp, y, 2, P.inkDim); // pending
  }
}

// drawCoach — layout v4 (split screen)
//  Left (x 0..120): gaze (animated eyes) + side + step. Always visible.
//  Right (x 128..319): WHICH exercise + HOW MANY reps/sec in big type + cue.
//  The dose number uses font 7 with its unit ("REPS", "REPS / SIDE",
//  "SEC", "SEC / SIDE") so it reads at a glance while standing.
//  MANUAL start: CO_READY shows everything and waits for a tap on START.
void drawCoach() {
  const Exercise& e = EXERCISES[coach.ex];
  uint32_t now = millis();
  char b[48], b2[48];
  uint16_t gc = groupColor(e.group);

  // --- Header (short in circuit mode so it doesn't overlap the progress dots) ---
  if (coach.circuit) snprintf(b, sizeof(b), TR("LONG %d/%d", "LARGA %d/%d"), min(coach.pos + 1, coach.n), coach.n);
  else               strlcpy(b, TR("ACTIVE BREAK", "PAUSA ACTIVA"), sizeof(b));
  txt(b, 8, 4, 2, TL_DATUM, P.accent);
  txt(GROUP_NAMES[e.group], SCR_W - 8, 4, 2, TR_DATUM, gc);
  drawCoachProgress();
  spr.drawFastHLine(8, 23, SCR_W - 16, P.line);

  // Left/right vertical divider (leaves room for the bottom buttons)
  spr.drawFastVLine(120, 26, 174, P.line);

  // --- Left zone: gaze always visible + ONE single status pill ---
  float eyeScale = 0.50f;
  if (coach.st == CO_WORK) {
    float beat = (e.reps > 0) ? (float)(now - coach.lastBeat) / e.tempoMs
                               : (float)(now - coach.t0) / (e.holdSec * 1000UL);
    float pulse = constrain(1.0f - beat * 2.0f, 0.0f, 1.0f);
    eyeScale = 0.50f + 0.08f * pulse;
  }
  drawEyesAt(60, 88, eyeScale);

  // The pill says ONE thing: side (bilateral) > step (circuit) > equipment
  if (e.perSide && (coach.st == CO_GO || coach.st == CO_WORK || coach.st == CO_SWITCH || coach.st == CO_READY)) {
    snprintf(b, sizeof(b), TR("SIDE %d/2", "LADO %d/2"), coach.side + 1);
    coachPill(60, 146, 94, 24, b, gc, P.bg);
  } else if (coach.n > 1) {
    snprintf(b, sizeof(b), TR("STEP %d/%d", "PASO %d/%d"), min(coach.pos + 1, coach.n), coach.n);
    coachPill(60, 146, 94, 24, b, P.card, P.inkDim);
  } else {
    coachPill(60, 146, 94, 24, e.weights ? TR("WEIGHTS", "CON PESAS") : TR("NO WEIGHTS", "SIN PESAS"), P.card, e.weights ? P.warn : P.ok, 2);
  }

  // --- CO_DONE: end screen (split: eyes + summary) ---
  if (coach.st == CO_DONE) {
    uint8_t doneMask = 0;
    for (int i = 0; i < coach.n; i++) doneMask |= (1 << EXERCISES[coach.list[i]].group);
    char groups[64] = "";
    for (int g2 = 0; g2 < GR_COUNT; g2++) if (doneMask & (1 << g2)) {
      if (groups[0]) strlcat(groups, " + ", sizeof(groups));
      strlcat(groups, GROUP_NAMES[g2], sizeof(groups));
    }
    txt(TR("Well done!", "Bien hecho!"), 224, 60, 4, MC_DATUM, P.ok);
    txt(groups, 224, 92, 2, MC_DATUM, P.inkDim);
    snprintf(b, sizeof(b), TR("Sets today: %u", "Series hoy: %u"),
      stats.sets[GR_LEG] + stats.sets[GR_BACK] + stats.sets[GR_ARM] + stats.sets[GR_MOB]);
    txt(b, 224, 118, 2, MC_DATUM, P.inkDim);
    drawButton(140, 140, 168, 30, TR("Close", "Cerrar"), true, P.ok);
    txt(TR("tap to close", "toca para cerrar"), 224, 180, 1, MC_DATUM, P.inkDim);
    return;
  }

  // --- Right zone (shared): name + cue + equipment label (fixed anchor) ---
  const int RX  = 128;
  const int RCX = 224;
  const int RW  = SCR_W - RX - 8;   // 184 px

  txtFit(e.name(), RCX, 36, RW - 4, 4, MC_DATUM, P.ink);
  coachCue2(e.cue(), RCX, 64, RW - 4);
  // Equipment label only where it refers to the current exercise (in REST the
  // next one's is shown and in DONE there's no exercise: avoids overlaps)
  if (coach.st == CO_READY || coach.st == CO_WORK || coach.st == CO_GO || coach.st == CO_SWITCH)
    txt(e.weights ? TR("WEIGHTS", "CON PESAS") : TR("BODYWEIGHT", "PESO CORPORAL"), RCX, 92, 1, MC_DATUM, e.weights ? P.warn : P.ok);

  switch (coach.st) {

    case CO_READY: {
      // TOTAL dose in big type + short unit (equipment is already on the y92 label)
      if (e.reps) snprintf(b, sizeof(b), "%d", e.reps);
      else        snprintf(b, sizeof(b), "%d", e.holdSec);
      txt(b, RCX, 128, 7, MC_DATUM, gc);
      if (e.reps) strlcpy(b2, e.perSide ? TR("REPS / SIDE", "REPS X LADO") : "REPS", sizeof(b2));
      else        strlcpy(b2, TR("SEC", "SEG"), sizeof(b2));
      txt(b2, RCX, 162, 2, MC_DATUM, P.ink);
      // Main CTA: solid full-width button, no blinking
      const char* ctaLong = TR("TAP TO START", "TOCA PARA EMPEZAR");
      const char* cta = (spr.textWidth(ctaLong, 2) <= RW - 10) ? ctaLong : TR("START", "EMPEZAR");
      drawButton(RX, 176, RW, 24, cta, true, gc);
      drawCoachButtons(TR("Skip", "Saltar"), e.weights);
    } break;

    case CO_GO: {
      int left = max(1, 3 - (int)((now - coach.t0) / 1000));
      snprintf(b, sizeof(b), "%d", left);
      txt(b, RCX, 130, 7, MC_DATUM, P.warn);
      txt(TR("Get ready...", "Preparate..."), RCX, 168, 2, MC_DATUM, P.inkDim);
      txt(TR("Don't tap: auto start", "No toques: empieza solo"), RCX, 188, 1, MC_DATUM, P.inkDim);
    } break;

    case CO_WORK: {
      float f;
      if (e.reps) {
        float intra = constrain((float)(now - coach.lastBeat) / e.tempoMs, 0.0f, 1.0f);
        snprintf(b, sizeof(b), "%d", coach.rep);
        txt(b, RCX, 122, 7, MC_DATUM, gc);
        snprintf(b2, sizeof(b2), TR("of %d", "de %d"), e.reps);   // the side is already on the left pill
        txt(b2, RCX, 158, 2, MC_DATUM, P.ink);
        f = (coach.rep + intra) / e.reps;
      } else {
        uint32_t total = e.holdSec * 1000UL;
        uint32_t elapsed = min(total, now - coach.t0);
        uint32_t left = (total - elapsed + 999) / 1000;
        snprintf(b, sizeof(b), "%lu", (unsigned long)left);
        txt(b, RCX, 122, 7, MC_DATUM, gc);
        strlcpy(b2, TR("SEC", "SEG"), sizeof(b2));
        txt(b2, RCX, 158, 2, MC_DATUM, P.ink);
        f = (float)elapsed / total;
      }
      // Overall progress bar (not per rep)
      spr.fillRoundRect(RX, 176, RW, 10, 4, P.card);
      spr.fillRoundRect(RX + 2, 178, (RW - 4) * constrain(f, 0.0f, 1.0f), 6, 3, gc);
      drawButton(6, COACH_BTN_Y, SCR_W - 12, COACH_BTN_H, TR("DONE - next", "HECHO - siguiente"), true, gc);
    } break;

    case CO_SWITCH: {
      // Countdown only: the target side is already on the left pill
      uint32_t left = (4000 - min(4000UL, now - coach.t0) + 999) / 1000;
      snprintf(b, sizeof(b), "%lu", (unsigned long)left);
      txt(TR("SWITCH SIDES", "CAMBIA DE LADO"), RCX, 108, 2, MC_DATUM, P.warn);
      txt(b, RCX, 158, 7, MC_DATUM, P.warn);
      txt(TR("tap to continue", "toca para seguir"), RCX, 192, 1, MC_DATUM, P.inkDim);
    } break;

    case CO_REST: {
      // Next + countdown only: no bar (the number is already the progress)
      uint32_t left = (REST_SEC * 1000UL - min((uint32_t)(REST_SEC * 1000UL), now - coach.t0)) / 1000;
      const Exercise& nx = EXERCISES[coach.list[coach.pos]];
      if (coach.pos < coach.n) {
        txt(TR("Rest. Next up:", "Descansa. Sigue:"), RCX, 100, 2, MC_DATUM, P.inkDim);
        snprintf(b, sizeof(b), "%s", nx.name());
        txtFit(b, RCX, 120, RW - 4, 4, MC_DATUM, P.ink);
        txt(nx.weights ? TR("with weights", "con pesas") : TR("no weights", "sin pesas"), RCX, 140, 1, MC_DATUM, nx.weights ? P.warn : P.ok);
      }
      snprintf(b, sizeof(b), "%lu", (unsigned long)left);
      txt(b, RCX, 170, 7, MC_DATUM, P.info);
      drawCoachButtons(TR("Finish", "Terminar"), EXERCISES[coach.list[coach.pos]].weights);
    } break;

    default: break;
  }
}
