// ===========================================================================
//  calm.h - Ventana Calma: meditacion y relajacion para tension, estres
//  e impulso/craving. SOS en 1 toque, sin cuentas ni rachas en crisis.
//
//  Modos (todos offline, bilingue con TR, sin tildes: TFT sin glifos):
//    BREATHE 60 s : suspiro ciclico (doble inhalacion + exhalacion larga,
//                   Balban et al. 2023) + box 4-4-4. Guia visual + pitidos.
//    SURF 3 min   : surfear el impulso (Bowen & Marlatt 2009): observar la
//                   ola sin actuar. No promete quitar el craving.
//    GROUND ~2 min: anclaje sensorial 5-4-3-2-1 (consenso clinico).
//
//  Esto NO es terapia ni tratamiento medico. Primera apertura: aviso + OK.
//  Solo guarda contadores (NVS cSeen/cCount/cPre/cPost), nada sensible.
// ===========================================================================
#pragma once

enum CalmState : uint8_t { CALM_MENU, CALM_BREATHE, CALM_SURF, CALM_GROUND, CALM_DONE };

#define CALM_BREATHE_MS 60000UL
#define CALM_SURF_MS    180000UL
#define CALM_CYCLE_MS   10000UL   // 2.5 in + 1 sip + 5.5 out + 1 hold
#define CALM_GROUND_AUTO_MS 40000UL
#define CALM_ROW_H      26
#define CALM_MENU_Y0    78
#define CALM_BTN_Y      164
#define CALM_BTN_H      28    // ends at 192: above the nav touch zone (196)

CalmState  calmState = CALM_MENU;
uint32_t   calmT0 = 0, calmPauseStart = 0, calmStepT0 = 0;
bool       calmPaused = false;
bool       calmAskConsent = false;
int        calmPendingMode = 1;   // 1 breathe, 2 surf, 3 ground
int        calmStep = 0;
int        calmPre = -1, calmPost = -1;   // 0..10, -1 = unset
int        calmLastTick = -1;             // phase/cycle tracker for soft beeps
bool       calmSeen = false;
int        calmCount = 0;
int        calmToday = 0;
uint32_t   calmDay = 0;   // YYYYMMDD of calmToday (NVS cDay/cToday)

void calmInit() {
  calmSeen = prefs.getBool("cSeen", false);
  calmCount = prefs.getUShort("cCount", 0);
  calmDay = prefs.getUInt("cDay", 0);
  calmToday = prefs.getUShort("cToday", 0);
  calmState = CALM_MENU;
  calmPaused = false;
  calmAskConsent = false;
  Serial.printf("[CALM] ready (seen=%d count=%d)\n", calmSeen ? 1 : 0, calmCount);
}

bool calmRunning() {
  return calmState == CALM_BREATHE || calmState == CALM_SURF || calmState == CALM_GROUND;
}

uint32_t calmElapsed() {
  return (calmPaused ? calmPauseStart : millis()) - calmT0;
}

// SOS entry: from FACE long-press, /pet {"calm":true} or voice PAGE:Calm.
void calmOpen() {
  calmState = CALM_MENU;
  calmPaused = false;
  calmAskConsent = false;
  calmStep = 0;
  calmPre = calmPost = -1;
  setPage(PAGE_CALM);
}

void calmStart(int mode) {
  calmPendingMode = constrain(mode, 1, 3);
  if (!calmSeen) { calmAskConsent = true; gDirty = true; return; }
  calmState = (mode == 1) ? CALM_BREATHE : (mode == 2 ? CALM_SURF : CALM_GROUND);
  calmT0 = millis();
  calmStepT0 = calmT0;
  calmStep = 0;
  calmPaused = false;
  calmPre = calmPost = -1;
  calmLastTick = -1;
  lastInteraction = millis();
  sound(SND_GO);
  gDirty = true;
}

void calmEndToMenu() {
  calmState = CALM_MENU;
  calmPaused = false;
  calmAskConsent = false;
  toast(TR("Calm off", "Calma off"), 1000);
  sound(SND_TICK);
  gDirty = true;
}

void calmTogglePause() {
  if (!calmRunning()) return;
  uint32_t now = millis();
  if (!calmPaused) { calmPaused = true; calmPauseStart = now; }
  else { calmPaused = false; calmT0 += now - calmPauseStart; calmStepT0 += now - calmPauseStart; }
  sound(SND_TICK);
  gDirty = true;
}

void calmFinish() {
  calmCount++;
  prefs.putUShort("cCount", (uint16_t)calmCount);
  // Day rollover (same YYYYMMDD key as the stats module; no NTP = keep counting).
  struct tm t;
  uint32_t day = calmDay;
  if (getLocal(t)) day = (t.tm_year + 1900) * 10000UL + (t.tm_mon + 1) * 100UL + t.tm_mday;
  if (day != calmDay) { calmDay = day; calmToday = 0; }
  calmToday++;
  prefs.putUInt("cDay", calmDay);
  prefs.putUShort("cToday", (uint16_t)calmToday);
  if (calmPre >= 0) prefs.putUChar("cPre", (uint8_t)calmPre);
  if (calmPost >= 0) prefs.putUChar("cPost", (uint8_t)calmPost);
  calmState = CALM_DONE;
  calmPaused = false;
  setMoodFor(M_HAPPY, 3000);
  squash = 1.0f;
  sound(SND_DONE);
  gDirty = true;
  Serial.printf("[CALM] done pre=%d post=%d count=%d\n", calmPre, calmPost, calmCount);
}

// Called every 250 ms from loop(): timers + soft beeps + repaint while animating.
void calmTick() {
  if (!calmRunning() || calmPaused) return;
  uint32_t el = calmElapsed();
  if (calmState == CALM_BREATHE) {
    int cyc = (int)(el / CALM_CYCLE_MS);
    if (cyc != calmLastTick) { calmLastTick = cyc; if (el < CALM_BREATHE_MS) sound(SND_TICK); }
    if (el >= CALM_BREATHE_MS) { calmFinish(); return; }
  } else if (calmState == CALM_SURF) {
    int step = constrain((int)(el / (CALM_SURF_MS / 4)), 0, 3);
    if (calmPre < 0) step = 0;   // wait for the pre rating on step 0
    if (step != calmStep && calmPre >= 0) { calmStep = step; calmStepT0 = millis(); sound(SND_TICK); }
    if (el >= CALM_SURF_MS && calmPost >= 0) { calmFinish(); return; }
    if (el >= CALM_SURF_MS + 60000UL) { calmFinish(); return; }   // no rating: close anyway
  } else if (calmState == CALM_GROUND) {
    if (millis() - calmStepT0 >= CALM_GROUND_AUTO_MS && calmStep < 4) {
      calmStep++; calmStepT0 = millis(); sound(SND_TICK);
    }
  }
  if (gPage == PAGE_CALM) gDirty = true;
}

// --- tap helpers -------------------------------------------------------------
static bool calmInRow(int y, int row) {
  int ry = CALM_MENU_Y0 + row * (CALM_ROW_H + 4);
  return y >= ry && y < ry + CALM_ROW_H;
}

void calmTap(int x, int y) {
  lastInteraction = millis();
  // Consent gate (first run only).
  if (calmAskConsent) {
    if (y >= 130 && y < 158) {
      calmSeen = true;
      prefs.putBool("cSeen", true);
      calmAskConsent = false;
      calmStart(calmPendingMode);
      return;
    }
    if (y >= CALM_BTN_Y) { calmAskConsent = false; gDirty = true; return; }
    return;
  }
  if (calmState == CALM_MENU) {
    for (int i = 0; i < 3; i++)
      if (calmInRow(y, i)) { calmStart(i + 1); return; }
    return;
  }
  if (calmState == CALM_DONE) {
    if (y >= CALM_BTN_Y) {
      if (x < 160) { calmState = CALM_MENU; gDirty = true; }
      else setPage(PAGE_FACE, -1, false);
    }
    return;
  }
  // Sessions: bottom row = Pause | End (x split at 160).
  if (y >= CALM_BTN_Y && y < NAV_TOUCH_Y) {
    if (x < 160) calmTogglePause();
    else calmEndToMenu();
    return;
  }
  if (calmPaused) return;
  if (calmState == CALM_BREATHE) {
    if (y < CALM_BTN_Y) calmTogglePause();   // big tap target: the circle pauses
    return;
  }
  if (calmState == CALM_SURF) {
    // Rating screens: 3 options; observe screens: tap card = next step.
    if ((calmStep == 0 && calmPre < 0) || (calmStep == 3 && calmPost < 0)) {
      if (y >= 96 && y < 124) {
        int v = (x < 112) ? 2 : (x < 216 ? 5 : 8);
        if (calmStep == 0 && calmPre < 0) {
          calmPre = v; calmStep = 1; calmT0 = millis(); calmStepT0 = calmT0;
          sound(SND_TICK);
        } else if (calmStep == 3 && calmPost < 0) {
          calmPost = v; calmFinish();
        }
        gDirty = true;
      }
      return;
    }
    if (y < CALM_BTN_Y) {   // next step (or post rating on the last one)
      if (calmStep < 3) { calmStep++; calmStepT0 = millis(); sound(SND_TICK); gDirty = true; }
    }
    return;
  }
  if (calmState == CALM_GROUND) {
    if (y < CALM_BTN_Y) {
      if (calmStep < 4) { calmStep++; calmStepT0 = millis(); sound(SND_TICK); }
      else calmFinish();
      gDirty = true;
    }
    return;
  }
}

// TTP223 sensor: tap = main action, hold = finish (page-level, see .ino).
void calmSensorTap() {
  if (calmState == CALM_MENU) { calmStart(1); return; }   // fastest relief
  if (calmState == CALM_DONE) { calmState = CALM_MENU; gDirty = true; return; }
  if (calmState == CALM_BREATHE) { calmTogglePause(); return; }
  calmTap(SCR_W / 2, 100);   // surf/ground: advance one step
}
void calmSensorHold() {
  if (calmRunning()) calmEndToMenu();
}

// --- drawing -----------------------------------------------------------------
static void calmMenuRow(int idx, const char* label, bool filled, uint16_t col) {
  int x = 8, w = SCR_W - 16, y = CALM_MENU_Y0 + idx * (CALM_ROW_H + 4);
  if (filled) { spr.fillRoundRect(x, y, w, CALM_ROW_H, 8, col); txt(label, x + w / 2, y + CALM_ROW_H / 2, 2, MC_DATUM, P.bg); }
  else { spr.fillRoundRect(x, y, w, CALM_ROW_H, 8, P.card); spr.drawRoundRect(x, y, w, CALM_ROW_H, 8, P.line); txt(label, x + w / 2, y + CALM_ROW_H / 2, 2, MC_DATUM, P.ink); }
}

static void calmBottomBar(bool paused) {
  drawButton(8, CALM_BTN_Y, 148, CALM_BTN_H,
             paused ? TR("Resume", "Seguir") : TR("Pause", "Pausa"), paused, paused ? P.ok : 0);
  drawButton(164, CALM_BTN_Y, 148, CALM_BTN_H, TR("End", "Fin"), false, P.danger);
}

static void calmProgress(float f, uint16_t col) {
  spr.fillRoundRect(24, 156, SCR_W - 48, 6, 3, P.card);
  spr.fillRoundRect(25, 157, (int)((SCR_W - 50) * constrain(f, 0, 1)), 4, 2, col);
}

void drawCalmConsent() {
  drawHeader(TR("CALM", "CALMA"));
  txt(TR("Short breathing exercise.", "Ejercicio corto de respiracion."), SCR_W / 2, 44, 2, MC_DATUM, P.ink);
  txt(TR("Not medical therapy.", "No es terapia medica."), SCR_W / 2, 66, 2, MC_DATUM, P.warn);
  txt(TR("Strong craving? Ask a", "Impulso fuerte? Pide"), SCR_W / 2, 92, 1, MC_DATUM, P.inkDim);
  txt(TR("professional for help.", "ayuda profesional."), SCR_W / 2, 104, 1, MC_DATUM, P.inkDim);
  drawButton(8, 130, SCR_W - 16, 28, TR("OK, start", "OK, empezar"), true, P.ok);
  drawButton(8, CALM_BTN_Y, SCR_W - 16, CALM_BTN_H, TR("Back", "Atras"), false, 0);
}

void drawCalmMenu() {
  drawHeader(TR("CALM", "CALMA"));
  drawEyesAt(SCR_W / 2, 52, 0.30f);
  txt(TR("1 tap = SOS breathing", "1 toque = SOS respirar"), SCR_W / 2, 70, 1, MC_DATUM, P.inkDim);
  calmMenuRow(0, TR("Breathe 60 s", "Respirar 60 s"), true, P.accent);
  calmMenuRow(1, TR("Ride the urge 3 min", "Surfear impulso 3 min"), false, 0);
  calmMenuRow(2, TR("Ground 5-4-3-2-1", "Anclar 5-4-3-2-1"), false, 0);
  char cc[40];
  snprintf(cc, sizeof(cc), TR("Today %d - total %d", "Hoy %d - total %d"), calmToday, calmCount);
  txt(cc, SCR_W / 2, 174, 1, MC_DATUM, P.inkDim);
  txt(TR("Hold FACE = SOS here", "Manten CARA = SOS aqui"), SCR_W / 2, 186, 1, MC_DATUM, P.inkDim);
}

void drawCalmBreathe() {
  drawHeader(TR("CALM - BREATHE", "CALMA - RESPIRA"));
  uint32_t el = calmElapsed();
  if (el > CALM_BREATHE_MS) el = CALM_BREATHE_MS;
  int cyc = (int)(el / CALM_CYCLE_MS);
  uint32_t w = el % CALM_CYCLE_MS;
  const char* ph;
  float r;
  uint16_t col;
  if (w < 2500) { ph = TR("Breathe in...", "Inhala..."); r = 20 + 32 * (w / 2500.0f); col = P.accent; }
  else if (w < 3500) { ph = TR("a sip more", "un poco mas"); r = 52 + 8 * ((w - 2500) / 1000.0f); col = P.accent; }
  else if (w < 9000) { ph = TR("Let go...", "Suelta..."); r = 60 - 38 * ((w - 3500) / 5500.0f); col = P.ok; }
  else { ph = "..."; r = 22; col = P.inkDim; }
  drawEyesAt(SCR_W / 2, 44, 0.24f);
  spr.drawCircle(SCR_W / 2, 96, (int)r + 5, col);
  spr.fillCircle(SCR_W / 2, 96, (int)r, col);
  spr.fillCircle(SCR_W / 2, 96, (int)(r * 0.55f), P.bg);
  char b[32];
  snprintf(b, sizeof(b), TR("cycle %d/6", "ciclo %d/6"), min(cyc + 1, 6));
  txt(ph, SCR_W / 2, 138, 2, MC_DATUM, P.ink);
  txt(b, SCR_W / 2, 154, 1, MC_DATUM, P.inkDim);
  if (calmPaused) txt(TR("paused", "en pausa"), SCR_W / 2, 120, 1, MC_DATUM, P.warn);
  calmBottomBar(calmPaused);
}

static void calmRateRow() {
  drawButton(8, 96, 96, 28, TR("Low", "Bajo"), false, 0);
  drawButton(112, 96, 96, 28, TR("Mid", "Medio"), false, 0);
  drawButton(216, 96, 96, 28, TR("High", "Fuerte"), false, 0);
}

void drawCalmSurf() {
  drawHeader(TR("CALM - URGE", "CALMA - IMPULSO"));
  if (calmStep == 0 && calmPre < 0) {
    txt(TR("0-10: how strong now?", "0-10: como pega ahora?"), SCR_W / 2, 60, 2, MC_DATUM, P.ink);
    calmRateRow();
    txt(TR("step 1/4 - rate it", "paso 1/4 - puntua"), SCR_W / 2, 140, 1, MC_DATUM, P.inkDim);
    calmBottomBar(calmPaused);
    return;
  }
  if (calmStep == 3 && calmPost < 0) {
    txt(TR("0-10: how strong now?", "0-10: como pega ahora?"), SCR_W / 2, 60, 2, MC_DATUM, P.ink);
    calmRateRow();
    txt(TR("step 4/4 - rate it", "paso 4/4 - puntua"), SCR_W / 2, 140, 1, MC_DATUM, P.inkDim);
    calmBottomBar(calmPaused);
    return;
  }
  static const char* const MSG_L[LANG_COUNT][3] = {
    { "It is a wave: it rises", "Feel the body, no fight", "Long out-breath..." },
    { "Es una ola: sube y baja", "Nota el cuerpo, sin luchar", "Suelta el aire largo..." },
  };
  int s = constrain(calmStep - 1, 0, 2);
  // Animated wave: craving rises, peaks and falls - you only watch it.
  uint32_t t = millis();
  for (int x = 20; x < SCR_W - 20; x += 3) {
    float env = sinf(3.14159f * (x - 20) / (SCR_W - 40));
    int y = 96 + (int)(sinf(x * 0.09f + t / 450.0f) * 22 * env);
    spr.drawPixel(x, y, P.accent);
    spr.drawPixel(x, y + 1, P.accent);
  }
  txt(MSG_L[gLang][s], SCR_W / 2, 138, 2, MC_DATUM, P.ink);
  char b[24];
  snprintf(b, sizeof(b), TR("step %d/4 - tap = next", "paso %d/4 - toca = sigue"), calmStep + 1);
  txt(b, SCR_W / 2, 154, 1, MC_DATUM, P.inkDim);
  calmProgress((float)calmElapsed() / CALM_SURF_MS, P.accent);
  calmBottomBar(calmPaused);
}

void drawCalmGround() {
  drawHeader(TR("CALM - GROUND", "CALMA - ANCLAR"));
  static const char* const N_L[LANG_COUNT][5] = {
    { "See", "Touch", "Hear", "Feel", "Breathe" },
    { "Ver", "Tocar", "Oir", "Sentir", "Respirar" },
  };
  static const int CNT[5] = { 5, 4, 3, 2, 1 };
  static const char* const I_L[LANG_COUNT][5] = {
    { "things you see", "things you touch", "sounds near you", "smells or feelings", "slow breath" },
    { "cosas que ves", "cosas que tocas", "sonidos cerca de ti", "olores o sensaciones", "respiracion lenta" },
  };
  int s = constrain(calmStep, 0, 4);
  char b[16];
  snprintf(b, sizeof(b), "%d", CNT[s]);
  txt(b, SCR_W / 2, 78, 8, MC_DATUM, P.accent);
  txt(N_L[gLang][s], SCR_W / 2, 122, 4, MC_DATUM, P.ink);
  txt(I_L[gLang][s], SCR_W / 2, 146, 1, MC_DATUM, P.inkDim);
  for (int i = 0; i < 5; i++) {
    int x = SCR_W / 2 - 4 * 12 + i * 12;
    if (i <= s) spr.fillCircle(x, 158, 3, P.accent);
    else spr.drawCircle(x, 158, 2, P.inkDim);
  }
  calmBottomBar(calmPaused);
}

void drawCalmDone() {
  drawHeader(TR("CALM", "CALMA"));
  drawEyesAt(SCR_W / 2, 56, 0.34f);
  if (calmPre >= 0 && calmPost >= 0) {
    char b[48];
    snprintf(b, sizeof(b), TR("From %d to %d. Well done.", "De %d a %d. Bien."), calmPre, calmPost);
    txt(b, SCR_W / 2, 96, 2, MC_DATUM, P.ok);
  } else {
    txt(TR("Done. Well done.", "Hecho. Bien."), SCR_W / 2, 96, 2, MC_DATUM, P.ok);
  }
  char c[40];
  snprintf(c, sizeof(c), TR("Calm moments: %d", "Momentos calma: %d"), calmCount);
  txt(c, SCR_W / 2, 118, 1, MC_DATUM, P.inkDim);
  txt(TR("Not therapy. Ask for help", "No es terapia. Pide ayuda"), SCR_W / 2, 132, 1, MC_DATUM, P.inkDim);
  txt(TR("if the urge is strong.", "si el impulso es fuerte."), SCR_W / 2, 144, 1, MC_DATUM, P.inkDim);
  drawButton(8, CALM_BTN_Y, 148, CALM_BTN_H, TR("Again", "Otra"), true, P.accent);
  drawButton(164, CALM_BTN_Y, 148, CALM_BTN_H, TR("Face", "Cara"), false, 0);
}

void drawCalmPage() {
  if (calmAskConsent) { drawCalmConsent(); return; }
  switch (calmState) {
    case CALM_BREATHE: drawCalmBreathe(); break;
    case CALM_SURF:    drawCalmSurf(); break;
    case CALM_GROUND:  drawCalmGround(); break;
    case CALM_DONE:    drawCalmDone(); break;
    default:           drawCalmMenu(); break;
  }
}
