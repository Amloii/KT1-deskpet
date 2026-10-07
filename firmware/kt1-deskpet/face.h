// ===========================================================================
//  face.h — Drawing utilities (1 bit) + the face: moods, eyes, blinking,
//           gaze, sport headband and posture nudge.
// ===========================================================================
#pragma once

// Pomo ambient chip (defined in pomodoro.h, called at runtime only)
bool pomoRunning();
bool pomoIsFlow();
uint32_t pomoRemainS();
uint32_t pomoElapsed();
extern bool pomoPaused;
extern int pomoMode;

// ---------------------------------------------------------------------------
//  Common drawing utilities
// ---------------------------------------------------------------------------
void txt(const char* s, int x, int y, int font, uint8_t datum, uint16_t col) {
  spr.setTextDatum(datum);
  spr.setTextColor(col);
  spr.drawString(s, x, y, font);
}
// Like txt(), but if the text does not fit in maxW it falls back to the next smaller valid font.
// NOTE: classic TFT_eSPI only has 1/2/4/6/7/8. The old `font-1` fell from 4 to 3
// (missing -> invisible title: only short names such as "Zancada atras" showed up).
void txtFit(const char* s, int x, int y, int maxW, int font, uint8_t datum, uint16_t col) {
  static const uint8_t CHAIN[] = { 8, 7, 6, 4, 2, 1 };
  int i = 0;
  while (i < 5 && CHAIN[i] > font) i++;   // valid font equal to or below the requested one (3/5 -> 2/4)
  while (i < 5 && spr.textWidth(s, CHAIN[i]) > maxW) i++;
  txt(s, x, y, CHAIN[i], datum, col);
}
// Approximate height of each valid font (line metrics only; the width is
// actually measured with textWidth). Font 7 is not used for text (digits only).
static int fontH(uint8_t f) {
  switch (f) {
    case 8: return 75; case 6: return 48; case 4: return 26; case 2: return 16;
    default: return 8;
  }
}
// Multi-line text centered inside (cx, yTop, maxW, maxH).
// Tries from fontStart and goes down (8/6/4/2) until it fits; words longer
// than the box are split. If it does not fit even in the smallest, truncates with "..."
// instead of falling to font 1 (unreadable at 320x240). Returns the font used.
static int wrapLines(const char* s, uint8_t f, int maxW, char lines[][64], int maxLines) {
  if (!s || !s[0] || maxLines <= 0) return 0;
  int nl = 0;
  lines[0][0] = 0;
  int w = 0;
  const char* p = s;
  char word[48];
  while (*p) {
    while (*p == ' ') p++;
    if (!*p) break;
    int n = 0;
    while (*p && *p != ' ' && n < 47) word[n++] = *p++;
    word[n] = 0;
    int a = 0;
    while (word[a]) {   // the word, in pieces that fit
      char piece[48];
      int m = 0;
      while (word[a + m]) {
        piece[m] = word[a + m]; piece[m + 1] = 0;
        if (spr.textWidth(piece, f) > maxW) { piece[m] = 0; break; }
        m++;
      }
      if (m == 0) m = 1;   // not even one letter fits: cut anyway (minimal overflow)
      piece[m] = 0;
      int pw = spr.textWidth(piece, f);
      int need = (w == 0) ? pw : w + spr.textWidth(" ", f) + pw;
      if (need > maxW) {
        if (++nl >= maxLines) return maxLines + 1;   // does not fit: signal to truncate
        lines[nl][0] = 0; w = 0;
        need = pw;
      }
      if (w) strlcat(lines[nl], " ", sizeof(lines[nl]));
      strlcat(lines[nl], piece, sizeof(lines[nl]));
      w = need;
      a += m;
    }
  }
  return nl + 1;
}
static int txtWrap(const char* s, int cx, int yTop, int maxW, int maxH, int fontStart, uint16_t col) {
  static const uint8_t CH[] = { 8, 6, 4, 2 };
  int si = 0;
  while (si < 3 && CH[si] > fontStart) si++;
  char lines[8][64];
  int use = 3, nl = 1;
  bool fits = false;
  for (; si < 4; si++) {
    uint8_t f = CH[si];
    int got = wrapLines(s, f, maxW, lines, 8);
    if (got <= 8) {
      nl = got;
      use = si;
      if (nl * (fontH(f) + 2) - 2 <= maxH) { fits = true; break; }   // fits: done
    }
  }
  uint8_t f = CH[use];
  if (!fits) {
    // Does not fit even in the smallest: cut to the visible lines and mark "...".
    // (lines[] holds the last wrap; if it overflowed, it holds the first 8.)
    if (nl < 8) nl = 8;
    int per = maxH / (fontH(f) + 2);
    if (per < 1) per = 1;
    if (per > 8) per = 8;
    nl = per;
    {
      size_t L = strlen(lines[nl - 1]);
      if (L >= 3) strcpy(lines[nl - 1] + L - 3, "...");
      else strlcpy(lines[nl - 1], "...", sizeof(lines[nl - 1]));
    }
  }
  if (nl > 8) nl = 8;
  int lh = fontH(f) + 2;
  int totalH = nl * fontH(f) + (nl - 1) * 2;
  int y = yTop + (maxH - totalH) / 2 + fontH(f) / 2;
  for (int i = 0; i < nl; i++, y += lh) txt(lines[i], cx, y, f, MC_DATUM, col);
  return f;
}
// Like txtWrap but with automatic paging for long replies: always wraps in
// `font` (readable, no tiny fonts) and shows one page that rotates every
// `periodMs`. Draws "1/3" if there are several. Returns the number of pages.
static int txtWrapScroll(const char* s, int cx, int yTop, int maxW, int maxH,
                         uint8_t font, uint16_t col, uint32_t periodMs = 2800) {
  char lines[24][64];
  int nl = wrapLines(s, font, maxW, lines, 24);
  bool cut = false;
  if (nl > 24) { nl = 24; cut = true; }
  if (nl <= 0) return 1;
  int lh = fontH(font) + 2;
  int per = maxH / lh;
  if (per < 1) per = 1;
  int pages = (nl + per - 1) / per;
  if (pages < 1) pages = 1;
  int pg = 0;
  if (pages > 1 && periodMs) pg = (millis() / periodMs) % pages;
  if (cut && pg == pages - 1) {
    size_t L = strlen(lines[nl - 1]);
    if (L >= 3) strcpy(lines[nl - 1] + L - 3, "...");
  }
  int n0 = pg * per;
  int n1 = n0 + per;
  if (n1 > nl) n1 = nl;
  int n = n1 - n0;
  int totalH = n * fontH(font) + (n - 1) * 2;
  int y = yTop + (maxH - totalH) / 2 + fontH(font) / 2;
  for (int i = n0; i < n1; i++, y += lh) txt(lines[i], cx, y, font, MC_DATUM, col);
  if (pages > 1) {
    char b[10];
    snprintf(b, sizeof(b), "%d/%d", pg + 1, pages);
    txt(b, cx + maxW / 2 - 2, yTop + maxH - 4, 1, TR_DATUM, P.inkDim);
  }
  return pages;
}
void thickLine(float x0, float y0, float x1, float y1, int r, uint16_t col) {
  float len = hypotf(x1 - x0, y1 - y0);
  int steps = max(1, (int)(len / 2));
  for (int i = 0; i <= steps; i++) {
    float t = (float)i / steps;
    spr.fillCircle(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, r, col);
  }
}
void drawDegree(int x, int y, int r, uint16_t col = INK) { spr.drawCircle(x, y, r, col); spr.drawCircle(x, y, r - 1, col); }

// Vertical gradient fill between two RGB565 colors
void fillVGradient(int x, int y, int w, int h, uint16_t top, uint16_t bot) {
  int tr = (top >> 11) & 0x1F, tg = (top >> 5) & 0x3F, tb = top & 0x1F;
  int br = (bot >> 11) & 0x1F, bg = (bot >> 5) & 0x3F, bb = bot & 0x1F;
  for (int i = 0; i < h; i++) {
    int r = tr + (br - tr) * i / (h - 1);
    int g = tg + (bg - tg) * i / (h - 1);
    int b = tb + (bb - tb) * i / (h - 1);
    spr.drawFastHLine(x, y + i, w, (uint16_t)((r << 11) | (g << 5) | b));
  }
}

void drawWifiIcon(int x, int y) {
  NetState n = gNet;
  uint16_t col = (n == NET_OK) ? P.ok : (n == NET_FAIL ? P.danger : P.inkDim);
  for (int i = 0; i < 3; i++) {
    int bh = 4 + i * 4;
    if (n == NET_OK) spr.fillRect(x + i * 5, y - bh, 3, bh, col);
    else             spr.drawRect(x + i * 5, y - bh, 3, bh, col);
  }
  if (n == NET_FAIL) spr.drawLine(x - 1, y - 13, x + 14, y + 1, col);
}
void drawHeader(const char* title) {
  txt(title, 8, 4, 2, TL_DATUM, P.accent);
  struct tm t; char b[8];
  if (getLocal(t)) { snprintf(b, sizeof(b), "%02d:%02d", t.tm_hour, t.tm_min); txt(b, SCR_W - 30, 4, 2, TR_DATUM, P.inkDim); }
  drawWifiIcon(SCR_W - 20, 18);
  spr.drawFastHLine(8, 23, SCR_W - 16, P.line);
}
// Navigation buttons (bottom, on both sides). The touch area is a bit larger than the drawing.
#define NAV_Y      210
#define NAV_H      28
#define NAV_W      56
#define NAV_TOUCH_Y 196        // touches below this line count for the buttons
#define NAV_TOUCH_W 84

void drawArrowButton(int x, int y, int w, int h, bool left) {
  spr.fillRoundRect(x, y, w, h, 8, P.card);
  spr.drawRoundRect(x, y, w, h, 8, P.line);
  int cx = x + w / 2, cy = y + h / 2;
  if (left) spr.fillTriangle(cx + 6, cy - 8, cx + 6, cy + 8, cx - 8, cy, P.accent);
  else      spr.fillTriangle(cx - 6, cy - 8, cx - 6, cy + 8, cx + 8, cy, P.accent);
}

void drawPageDots() {
  drawArrowButton(4, NAV_Y, NAV_W, NAV_H, true);
  drawArrowButton(SCR_W - 4 - NAV_W, NAV_Y, NAV_W, NAV_H, false);
  int sp = 12, x0 = SCR_W / 2 - (PAGE_COUNT - 1) * sp / 2;
  for (int i = 0; i < PAGE_COUNT; i++) {
    if (i == gPage) spr.fillCircle(x0 + i * sp, NAV_Y + NAV_H / 2, 3, P.accent);
    else            spr.drawCircle(x0 + i * sp, NAV_Y + NAV_H / 2, 2, P.inkDim);
  }
}

// -1 = left button, +1 = right, 0 = none
int navHit(int x, int y) {
  if (y < NAV_TOUCH_Y) return 0;
  if (x < NAV_TOUCH_W) return -1;
  if (x > SCR_W - NAV_TOUCH_W) return 1;
  return 0;
}
void drawToast() {
  if (millis() > toastUntil || !toastText[0]) return;
  int w = spr.textWidth(toastText, 2) + 26;
  // On the pomodoro page the buttons live at y>=168: show global toasts on top
  // instead of covering them (inline pomoMsg already handles local feedback).
  int x = (SCR_W - w) / 2, y = (gPage == PAGE_POMO ? 30 : 168);
  spr.fillRoundRect(x - 1, y - 1, w + 2, 30, 10, P.bg);      // halo to separate it from the background
  spr.fillRoundRect(x, y, w, 28, 9, P.card);
  spr.drawRoundRect(x, y, w, 28, 9, P.accent);
  txt(toastText, SCR_W / 2, y + 14, 2, MC_DATUM, P.ink);
}
void drawCentered2(const char* a, const char* b) {
  txt(a, SCR_W / 2, SCR_H / 2 - 12, 4, MC_DATUM, P.ink);
  if (b && b[0]) txt(b, SCR_W / 2, SCR_H / 2 + 18, 2, MC_DATUM, P.inkDim);
}
void drawNoData(const char* err) {
  if (gNet == NET_FAIL)       drawCentered2(TR("No Wi-Fi", "Sin Wi-Fi"), TR("Check WIFI_SSID / WIFI_PASS in secrets.h", "Revisa WIFI_SSID / WIFI_PASS en secrets.h"));
  else if (err && err[0])     drawCentered2(TR("No data", "Sin datos"), err);
  else                        drawCentered2(TR("Loading...", "Cargando..."), "");
}

// ---------------------------------------------------------------------------
//  Moods
// ---------------------------------------------------------------------------
const char* const MOOD_NAMES_L[LANG_COUNT][M_COUNT] = {
  { "Normal", "Happy", "Love", "Surprise", "Angry", "Sad", "Sleepy", "Dizzy", "Sport", "Think" },
  { "Normal", "Feliz", "Amor", "Sorpresa", "Enfado", "Triste", "Sueno", "Mareo", "Deporte", "Piensa" },
};
#define MOOD_NAMES (MOOD_NAMES_L[gLang])
const Mood  TAP_CYCLE[] = { M_HAPPY, M_LOVE, M_SURPRISED, M_ANGRY, M_SAD, M_SLEEPY, M_NEUTRAL };
const int   TAP_CYCLE_N = sizeof(TAP_CYCLE) / sizeof(TAP_CYCLE[0]);

//                                  width height radius offsetY
const EyeShape SHAPES[M_COUNT] = { { 70,  88,  18,   0 },   // normal
                                   { 76,  84,  24,  -4 },   // happy  (cut at the bottom -> ^ ^)
                                   { 70,  70,  20,   0 },   // love   (hearts)
                                   { 84, 104,  38,  -6 },   // surprised
                                   { 74,  76,  14,   6 },   // angry
                                   { 66,  80,  18,  12 },   // sad
                                   { 76,  18,   8,  18 },   // sleepy
                                   { 70,  70,  20,   0 },   // dizzy (spirals)
                                   { 76,  66,  16,   8 },   // sport (determined + headband)
                                   { 70,  54,  16, -12 } }; // thinking (squinting upwards, amber)

EyeShape cur = SHAPES[M_NEUTRAL];
float    gazeX = 0, gazeY = 0, randGX = 0, randGY = 0;
uint32_t nextSaccade = 0, nextBlink = 0, blinkStart = 0;
bool     blinking = false;
uint32_t lastAnim = 0;

// Idle gestures (life at rest): only in M_NEUTRAL, FACE page,
// during the day and without coach/nudge/manual mood. Scheduled every 9-16 s
// after >12 s without touching. Kind: 0 none, 1 double blink,
// 2 curious glance, 3 droopy eyes, 4 little hop.
uint32_t idleNext = 0;
uint8_t  idleKind = 0;
uint32_t idleT0 = 0;
float    idleGX = 0, idleGY = 0;

static uint32_t idleDur(uint8_t k) {
  switch (k) {
    case 1: return 700;
    case 2: return 950;
    case 3: return 1300;
    case 4: return 700;
    default: return 0;
  }
}

void faceInit() { nextBlink = millis() + 1500; idleNext = millis() + 8000; }

Mood chatEyeMood();   // chat.h: eyes follow the turn state (recording/thinking/speaking)

Mood autoMood() {
  if (isNight()) return M_SLEEPY;
  if (gPhase == PH_AWAY) return M_SLEEPY;
  if (millis() - lastInteraction > SLEEPY_IDLE_MS) return M_SLEEPY;
  if (gPosture == POS_SIT && gPhase != PH_OFF && millis() - sitSince > SIT_TOO_LONG_MIN * 60000UL) return M_SAD;
  return M_NEUTRAL;
}
Mood currentMood() {
  uint32_t now = millis();
  if (now < dizzyUntil)  return M_DIZZY;
  if (coach.active)      return coach.st == CO_DONE ? M_HAPPY : M_SPORT;
  if (gPage == PAGE_CHAT && !coach.active) return chatEyeMood();   // eyes = chat state
  if (now < manualUntil) return manualMood;
  if (nudge.active)      return nudge.target == POS_STAND ? M_SURPRISED : M_NEUTRAL;
  return autoMood();
}
void setMoodFor(Mood m, uint32_t ms) { manualMood = m; manualUntil = millis() + ms; }
void nextMoodByTap() {
  Mood m = currentMood();
  int idx = -1;
  for (int i = 0; i < TAP_CYCLE_N; i++) if (TAP_CYCLE[i] == m) idx = i;
  setMoodFor(TAP_CYCLE[(idx + 1) % TAP_CYCLE_N], MANUAL_MOOD_MS);
  dizzyUntil = 0;
  squash = 1.0f;
  char buf[32]; snprintf(buf, sizeof(buf), TR("Mood: %s", "Animo: %s"), MOOD_NAMES[manualMood]);
  toast(buf, 900);
}

// ---------------------------------------------------------------------------
//  Eye drawing
// ---------------------------------------------------------------------------
// Color of each cycle phase (used on the face and on the FOCUS page)
uint16_t phaseColor(Phase p) {
  switch (p) {
    case PH_SIT:   return tint(rgb( 96, 165, 250));   // blue
    case PH_STAND: return P.ok;                        // green
    case PH_MOVE:  return tint(rgb(251, 146,  60));    // orange
    case PH_LONG:  return tint(rgb(167, 139, 250));    // purple
    case PH_AWAY:  return P.inkDim;                     // gray
    default:       return P.inkDim;
  }
}

// Eye / decoration color by mood (dimmed by brightness)
uint16_t moodColor(Mood m) {
  switch (m) {
    case M_HAPPY:     return tint(rgb(250, 204, 21));    // yellow
    case M_LOVE:      return tint(rgb(244, 114, 182));   // pink
    case M_SURPRISED: return tint(rgb(56, 189, 248));    // light cyan
    case M_ANGRY:     return P.danger;                   // red
    case M_SAD:       return tint(rgb(96, 165, 250));    // blue
    case M_SLEEPY:    return tint(rgb(120, 133, 150));   // dim gray
    case M_DIZZY:     return P.accent;
    case M_SPORT:     return tint(rgb(251, 146, 60));    // orange
    case M_THINK:     return P.warn;                     // thinking amber
    default:          return P.accent;                   // neutral: accent color
  }
}

void drawHeart(int cx, int cy, float s, uint16_t col = INK) {
  int r = s * 0.5f;
  spr.fillCircle(cx - r, cy - r / 2, r, col);
  spr.fillCircle(cx + r, cy - r / 2, r, col);
  spr.fillTriangle(cx - 2 * r + 1, cy - r / 2 + r * 0.35f, cx + 2 * r - 1, cy - r / 2 + r * 0.35f, cx, cy + 1.7f * r, col);
}
void drawSpiral(int cx, int cy, float maxR, float rot, uint16_t col = INK) {
  const float turns = 4.0f * PI;
  for (float a = 0; a < turns; a += 0.18f) {
    float rr = a / turns * maxR;
    spr.fillCircle(cx + rr * cosf(a + rot), cy + rr * sinf(a + rot), 3, col);
  }
}
void drawEye(int cx, int cy, float w, float h, float r, bool left, Mood m, uint16_t col = INK) {
  int iw = max(6, (int)w), ih = max(4, (int)h);
  int x = cx - iw / 2, y = cy - ih / 2;
  int ir = min((int)r, min(iw, ih) / 2);
  spr.fillRoundRect(x, y, iw, ih, ir, col);
  int inner = left ? x + iw : x;
  int outer = left ? x : x + iw;
  int pad = left ? 2 : -2;
  switch (m) {
    case M_HAPPY:
      spr.fillEllipse(cx, y + ih + ih * 0.15f, iw * 0.75f, ih * 0.62f, PAPER);
      break;
    case M_ANGRY:
      spr.fillTriangle(outer, y - 2, inner + pad, y - 2, inner + pad, y + ih * 0.50f, PAPER);
      break;
    case M_SPORT:     // slightly slanted eyelid: determined look
      spr.fillTriangle(outer, y - 2, inner + pad, y - 2, inner + pad, y + ih * 0.28f, PAPER);
      break;
    case M_SAD:
      spr.fillTriangle(inner, y - 2, outer - pad, y - 2, outer - pad, y + ih * 0.45f, PAPER);
      break;
    case M_SURPRISED:
      if (ih > 30) spr.fillCircle(x + iw * 0.30f, y + ih * 0.28f, max(3, iw / 10), PAPER);
      break;
    default: break;
  }
}

// Sport headband on the "forehead", with both knot ends flapping in the wind
void drawHeadband(int cxL, int cxR, int topY, float s, uint32_t now, uint16_t col = INK) {
  int x0 = cxL - 50 * s, x1 = cxR + 50 * s, h = max(6, (int)(11 * s));
  spr.fillRect(x0, topY, x1 - x0, h, col);
  for (int i = 0; i < 4; i++) spr.drawFastVLine(x0 + 6 + i * (x1 - x0 - 12) / 3, topY + 2, h - 4, PAPER);   // seams
  float wave = sinf(now / 120.0f) * 5 * s;
  spr.fillTriangle(x1, topY, x1, topY + h, x1 + 34 * s, topY + 10 * s + wave, col);
  spr.fillTriangle(x1, topY + 2, x1, topY + h, x1 + 26 * s, topY + 22 * s - wave, col);
}

void updateFaceAnim(Mood m) {
  uint32_t now = millis();
  if (now - lastAnim < 15) return;          // once per frame
  lastAnim = now;
  const EyeShape& tg = SHAPES[m];
  cur.w += (tg.w - cur.w) * 0.22f;  cur.h += (tg.h - cur.h) * 0.22f;
  cur.r += (tg.r - cur.r) * 0.22f;  cur.dy += (tg.dy - cur.dy) * 0.22f;

  // --- Idle gestures: schedule / expire ---
  bool canIdle = (m == M_NEUTRAL && gPage == PAGE_FACE && !coach.active &&
                  !nudge.active && (int32_t)(now - manualUntil) >= 0 &&
                  (int32_t)(now - dizzyUntil) >= 0 && !isNight());
  if (idleKind != 0) {
    if (!canIdle || now - lastInteraction < 2000 || now - idleT0 >= idleDur(idleKind)) idleKind = 0;
  } else if (canIdle && (int32_t)(now - idleNext) >= 0 && now - lastInteraction > 12000) {
    idleKind = random(1, 5);
    idleT0 = now;
    idleNext = now + random(9000, 16000);
    if (idleKind == 2) {   // curious glance: to one side and slightly up/down
      idleGX = random(0, 2) ? random(22, 38) : -random(22, 38);
      idleGY = random(-12, 13);
    }
    if (idleKind == 4) squash = 1.0f;   // the hop starts with a bounce
  }

  if ((int32_t)(now - nextSaccade) >= 0) {
    bool center = random(100) < 30;
    float amp = (fabsf(imuGazeX) + fabsf(imuGazeY) > 10) ? 0.3f : 1.0f;
    if (canIdle && now - lastInteraction > 30000) amp = 1.3f;   // after a long idle, looks around more widely
    randGX = center ? 0 : random(-30, 31) * amp;
    randGY = center ? 0 : random(-16, 17) * amp;
    nextSaccade = now + random(1200, 4200);
    if (canIdle && now - lastInteraction > 30000) nextSaccade = now + random(700, 2000);
    if (m == M_SLEEPY) nextSaccade += 3000;
  }
  float tx = randGX + imuGazeX, ty = randGY + imuGazeY;
  if (idleKind == 2) { tx = idleGX + imuGazeX * 0.3f; ty = idleGY + imuGazeY * 0.3f; }
  if (nudge.active && !coach.active) { tx = 0; ty = nudge.target == POS_STAND ? -26 : 24; }  // look up/down
  if (coach.active) { tx *= 0.3f; ty = 0; }
  if (ts.down && gPage == PAGE_FACE && !coach.active) {
    tx = (ts.x - SCR_W / 2) * 45.0f / (SCR_W / 2);
    ty = (ts.y - SCR_H / 2) * 28.0f / (SCR_H / 2);
  }
  // Content-oriented gaze: outside the FACE page the eyes look at the
  // content (below) with a short wander, instead of roaming randomly.
  // The FACE looks freely (saccades + IMU + finger); coach and nudge take priority.
  if (!coach.active && !(nudge.active && !coach.active) && gPage != PAGE_FACE && idleKind == 0) {
    float ax = 0, ay = 14;
    switch (gPage) {
      case PAGE_EXERCISE: ax = 0; ay = 16; break;   // "Today" list below
      case PAGE_POMO:     ax = 0; ay = 14; break;   // time card below
      case PAGE_PLANTS:   ax = 0; ay = 14; break;   // selector + data below
      case PAGE_CHAT:     ax = 0; ay = -22; break;  // looking up: thinking/speaking
      case PAGE_VITAL:    ax = 0; ay = 12; break;   // countdown and buttons below
      default:            ax = 0; ay = 10; break;
    }
    tx = ax + randGX * 0.35f + imuGazeX * 0.4f;
    ty = ay + randGY * 0.35f + imuGazeY * 0.4f;
  }
  tx = constrain(tx, -45.0f, 45.0f);  ty = constrain(ty, -28.0f, 28.0f);
  float k = (m == M_SLEEPY) ? 0.06f : 0.28f;
  gazeX += (tx - gazeX) * k;  gazeY += (ty - gazeY) * k;

  if (!blinking && (int32_t)(now - nextBlink) >= 0) { blinking = true; blinkStart = now; }
  if (blinking && now - blinkStart > (m == M_SLEEPY ? 450u : 160u)) {
    blinking = false;
    nextBlink = now + random(1800, 6000);
    if (random(100) < 15) nextBlink = now + 220;
  }
  squash *= 0.82f;
}

// Draws both eyes centered at (cx0, cy0) with scale s
void drawEyesAt(int cx0, int cy0, float s) {
  Mood m = currentMood();
  updateFaceAnim(m);
  uint32_t now = millis();
  float open = 1.0f;
  if (blinking) {
    float dur = (m == M_SLEEPY) ? 450.0f : 160.0f;
    float p = (now - blinkStart) / dur;
    open = fabsf(1.0f - 2.0f * constrain(p, 0.0f, 1.0f));
  }
  float w = cur.w * s * (1.0f + 0.14f * squash);
  float h = cur.h * s * (1.0f - 0.22f * squash) * max(open, 0.06f);
  float persp = gazeX / 45.0f * 0.10f;
  int cy  = cy0 + (cur.dy + gazeY) * s;
  int cxL = cx0 + (-58 + gazeX) * s, cxR = cx0 + (58 + gazeX) * s;
  // Breathing at rest: always a slight bob (except in coach, which already pulses)
  if (!coach.active) {
    float br = sinf(now / 900.0f);
    cy += (int)(br * 2.0f * s);
    h *= (1.0f + 0.02f * br);
  }
  // Idle gestures (FACE page only, see updateFaceAnim)
  if (idleKind != 0 && gPage == PAGE_FACE && !coach.active) {
    float ph = (now - idleT0) / (float)idleDur(idleKind);
    ph = constrain(ph, 0.0f, 1.0f);
    if (idleKind == 1) {
      open = min(open, 0.10f + 0.90f * fabsf(sinf(ph * 2.0f * PI)));  // double blink
      h = cur.h * s * (1.0f - 0.22f * squash) * max(open, 0.06f);
    } else if (idleKind == 3) {
      h *= (1.0f - 0.45f * sinf(ph * PI));   // brief droopy eyes (bored)
    } else if (idleKind == 4) {
      cy -= (int)(sinf(ph * PI) * 10.0f * s);  // little hop inviting a touch
    }
  }
  uint16_t ec = moodColor(m);

  if (m == M_LOVE) {
    float hs = (30 + 4 * sinf(now / 140.0f)) * s;
    if (open < 0.3f) { spr.fillRoundRect(cxL - 30 * s, cy - 3, 60 * s, 6, 3, ec); spr.fillRoundRect(cxR - 30 * s, cy - 3, 60 * s, 6, 3, ec); }
    else { drawHeart(cxL, cy - 4 * s, hs, ec); drawHeart(cxR, cy - 4 * s, hs, ec); }
  } else if (m == M_DIZZY) {
    float rot = now / 90.0f;
    drawSpiral(cxL, cy, 34 * s, rot,  tint(rgb(56, 189, 248)));
    drawSpiral(cxR, cy, 34 * s, -rot, tint(rgb(244, 114, 182)));
  } else {
    drawEye(cxL, cy, w, h * (1.0f - persp), cur.r * s, true,  m, ec);
    drawEye(cxR, cy, w, h * (1.0f + persp), cur.r * s, false, m, ec);
  }
  if (m == M_SPORT) drawHeadband(cxL, cxR, cy - (SHAPES[M_SPORT].h / 2 + 28) * s, s, now, P.danger);
  if (m == M_SLEEPY) {
    for (int i = 0; i < 3; i++) {
      float ph = fmodf(now / 1400.0f + i / 3.0f, 1.0f);
      txt("z", cx0 + (90 + i * 14 + ph * 10) * s, cy0 - (54 + ph * 40) * s, i == 2 ? 4 : 2, MC_DATUM, P.inkDim);
    }
  }
}

// ---------------------------------------------------------------------------
//  FACE page: top bar + eyes + posture nudge
// ---------------------------------------------------------------------------
void drawStatusBar() {
  struct tm t; char b[24];
  if (getLocal(t)) { snprintf(b, sizeof(b), "%02d:%02d", t.tm_hour, t.tm_min); txt(b, 8, 4, 2, TL_DATUM, P.inkDim); }
  // center: exercise phase and/or pomo remain (ambient chip when away from pomo page)
  if (gPhase != PH_OFF || pomoRunning()) {
    static const char* const SHORT_L[LANG_COUNT][6] = {
      { "", "SIT", "STAND", "MOVE", "BREAK", "AWAY" },
      { "", "SENTADO", "DE PIE", "MOVER", "PAUSA", "FUERA" },
    };
    char c[48]; c[0] = 0;
    if (gPhase != PH_OFF) {
      uint32_t rem = phaseRemainingS();
      snprintf(c, sizeof(c), "%s %lum%s", SHORT_L[gLang][gPhase], (unsigned long)((rem + 59) / 60), gPaused ? " II" : "");
    }
    if (pomoRunning()) {
      char pb[24];
      const char* ini = (pomoMode == 1) ? "E" : (pomoMode == 2 ? "O" : "T");
      if (pomoIsFlow()) {
        uint32_t e = pomoElapsed() / 60000UL;
        snprintf(pb, sizeof(pb), "%s+%lum%s", ini, (unsigned long)e, pomoPaused ? " II" : "");
      } else {
        uint32_t r = pomoRemainS();
        snprintf(pb, sizeof(pb), "%s%lum%s", ini, (unsigned long)((r + 59) / 60), pomoPaused ? " II" : "");
      }
      if (c[0]) { strlcat(c, " ", sizeof(c)); strlcat(c, pb, sizeof(c)); }
      else strlcpy(c, pb, sizeof(c));
    }
    txt(c, SCR_W / 2, 4, 2, TC_DATUM, (gPhase == PH_OFF) ? P.warn : phaseColor(gPhase));
  }
  float temp; bool ok;
  xSemaphoreTake(dataMtx, portMAX_DELAY); ok = gWeather.ok; temp = gWeather.temp; xSemaphoreGive(dataMtx);
  int xr = SCR_W - 30;
  if (ok) {
    snprintf(b, sizeof(b), "%.0f", temp);
    txt("C", xr - 2, 4, 2, TR_DATUM, P.inkDim);
    int cw = spr.textWidth("C", 2);
    spr.drawCircle(xr - cw - 5, 7, 2, P.inkDim);
    txt(b, xr - cw - 9, 4, 2, TR_DATUM, P.warn);
  }
  drawWifiIcon(SCR_W - 20, 18);
}

void drawNudgeBanner() {
  if (!nudge.active) return;
  int y = 168, h = 34;
  uint16_t bg = nudge.target == POS_STAND ? P.ok : tint(rgb(96, 165, 250));
  spr.fillRoundRect(14, y, SCR_W - 28, h, 10, bg);
  int ax = 36, ay = y + h / 2;
  if (nudge.target == POS_STAND) spr.fillTriangle(ax - 10, ay + 7, ax + 10, ay + 7, ax, ay - 9, P.bg);
  else                           spr.fillTriangle(ax - 10, ay - 7, ax + 10, ay - 7, ax, ay + 9, P.bg);
  // Text derived from the target (not nudge.text) so it follows a language change
  const char* nudgeMsg = nudge.target == POS_STAND
    ? TR("Raise desk: stand 8 min", "Sube la mesa: 8 min de pie")
    : TR("Back to work: lower desk", "A trabajar: baja la mesa");
  txt(nudgeMsg, SCR_W / 2 + 12, ay - 5, 2, MC_DATUM, P.bg);
  txt(TR("tap when done", "toca cuando lo hagas"), SCR_W / 2 + 12, ay + 10, 1, MC_DATUM, P.bg);
}

void drawFacePage() {
  drawStatusBar();
  drawEyesAt(SCR_W / 2, nudge.active ? 96 : 116, nudge.active ? 0.8f : 1.0f);
  drawNudgeBanner();
}
