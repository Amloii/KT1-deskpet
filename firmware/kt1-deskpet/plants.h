// ===========================================================================
//  plants.h — Plant check v3 (DHT11 + LDR): visual pixel-art catalog +
//  live light meter + ideal-range bars + score + actionable advice.
//  Icons: 32x32 nibble pixel-art negro+verde (plants_icons.h, generated
//  by tools/gen_plant_pixel.py, previews in docs/media/plants_pixel/).
// ===========================================================================
#pragma once
#include "plants_icons.h"

struct PlantSpec {
  const char* nameL[LANG_COUNT];   // on-screen name { en, es }
  int luxMin, luxMax;      // ideal light range
  int tMin, tMax;          // ideal temperature C
  int hMin, hMax;          // ideal relative humidity %
  const char* name() const { return nameL[gLang]; }
};

static const PlantSpec PLANTS[] = {
  { { "Pothos",      "Poto"        },   800,  8000, 18, 30, 40, 70 },
  { { "Monstera",    "Monstera"    },  2500, 15000, 18, 30, 50, 70 },
  { { "Snake plant", "Sansevieria" },   500,  5000, 15, 30, 30, 50 },
  { { "Ficus",       "Ficus"       },  5000, 20000, 18, 27, 40, 65 },
  { { "Calathea",    "Calathea"    },   800,  4000, 18, 26, 60, 80 },
  { { "Cactus",      "Cactus"      }, 15000, 60000, 18, 32, 20, 40 },
};
static const int PLANT_N = sizeof(PLANTS) / sizeof(PLANTS[0]);

enum PlantState { PL_IDLE, PL_MEASURING, PL_RESULT };
int plantIdx = 0;
PlantState plantState = PL_IDLE;
uint32_t plantT0 = 0;
float plantLux = 0, plantTemp = 0, plantHum = 0;
bool plantDhtOk = false;
int plantSamples = 0;
float plantLuxAcc = 0, plantTempAcc = 0, plantHumAcc = 0;
long plantAdcAcc = 0;   // raw ADC sum (to calibrate lux)
int plantDhtN = 0;   // good DHT11 reads (own counter, not derived from the LDR)
uint32_t plantLastSample = 0, plantLastDht = 0;
char plantErr[32] = "";
// Scout mode (IDLE on the Plants page): LDR every ~0.4 s + single-shot
// DHT11 every 2 s (no retries: fail keeps last value). Feeds the live
// gauges + advice so you can walk around seeking a good spot.
float plantLiveLux = 0, plantLiveTemp = NAN, plantLiveHum = -1;
bool plantHasLive = false, plantLiveDht = false;
uint32_t plantLiveT = 0, plantLiveDhtT = 0;

void plantInit() {
#if PLANT_ENABLED
  pinMode(LDR_PIN, INPUT);
  pinMode(DHT_PIN, INPUT_PULLUP);
  analogReadResolution(12);
  Serial.printf("[PLANTS] DHT11 on GPIO %d, LDR on GPIO %d\n", DHT_PIN, LDR_PIN);
#endif
}

// Bit-banged DHT11. Reading is sensitive to interrupts (WiFi lives on the
// other core, but core 1 also gets interrupts): read with interrupts paused
// for ~5 ms and with retries from plantTick.
bool dht11ReadOnce(float& temp, float& hum) {
  pinMode(DHT_PIN, OUTPUT);
  digitalWrite(DHT_PIN, LOW);
  delay(20);                       // start >= 18 ms
  digitalWrite(DHT_PIN, HIGH);
  delayMicroseconds(40);
  pinMode(DHT_PIN, INPUT_PULLUP);
  noInterrupts();                  // critical section: 26 us vs 70 us pulses
  unsigned long t0 = micros();
  while (digitalRead(DHT_PIN) == HIGH) { if (micros() - t0 > 120) { interrupts(); return false; } }
  t0 = micros();
  while (digitalRead(DHT_PIN) == LOW)  { if (micros() - t0 > 200) { interrupts(); return false; } }
  t0 = micros();
  while (digitalRead(DHT_PIN) == HIGH) { if (micros() - t0 > 200) { interrupts(); return false; } }
  uint8_t d[5] = {0,0,0,0,0};
  for (int i = 0; i < 40; i++) {
    t0 = micros();
    while (digitalRead(DHT_PIN) == LOW)  { if (micros() - t0 > 120) { interrupts(); return false; } }
    t0 = micros();
    while (digitalRead(DHT_PIN) == HIGH) { if (micros() - t0 > 150) { interrupts(); return false; } }
    if (micros() - t0 > 45) d[i / 8] |= (1 << (7 - (i % 8)));
  }
  interrupts();
  if ((uint8_t)(d[0] + d[1] + d[2] + d[3]) != d[4]) return false;
  hum = d[0] + d[1] / 10.0f;
  temp = d[2] + d[3] / 10.0f;
  if (hum < 1 || hum > 100 || temp < -10 || temp > 60) return false;
  return true;
}
// 3 attempts 200 ms apart (the DHT11 can't really do more than 1 Hz)
bool dht11Read(float& temp, float& hum) {
  for (int k = 0; k < 3; k++) {
    if (dht11ReadOnce(temp, hum)) return true;
    delay(200);
  }
  return false;
}

// LDR divider 3.3V -> 10k -> AO -> LDR -> GND: AO DROPS with more light.
// Calibrated with 2 real points (2852->8 lux, 1688->50 lux):
//   R_LDR = 10k * V/(3.3-V)  =>  22.9k @8 lux, 7.0k @50 lux
//   => R10 = 20k, gamma = 0.65  (lux = 10 * (R10/R)^(1/gamma))
float ldrToLux(int adc) {
  float v = constrain(adc, 1, 4094) / 4095.0f * 3.3f;
  float r = 10000.0f * v / max(0.05f, 3.3f - v);
  float lux = 10.0f * powf(20000.0f / max(100.0f, r), 1.0f / 0.65f);
  return constrain(lux, 0, 60000);
}

void plantStart() {
  plantState = PL_MEASURING;
  plantT0 = millis();
  plantLuxAcc = plantTempAcc = plantHumAcc = 0;
  plantAdcAcc = 0;
  plantSamples = 0;
  plantDhtN = 0;
  plantDhtOk = false;
  plantErr[0] = 0;
  plantLastSample = plantLastDht = 0;
  sound(SND_TICK);
  gDirty = true;
  Serial.printf("[PLANTS] Measuring for %s...\n", PLANTS[plantIdx].nameL[LANG_EN]);
}

void plantTick() {
#if !PLANT_ENABLED
  return;
#endif
  uint32_t now = millis();
  // Sensors only live on this page: no ADC/DHT activity elsewhere.
  // Leaving mid-measurement aborts it (no silent sampling in background).
  if (gPage != PAGE_PLANTS) {
    if (plantState == PL_MEASURING) {
      plantState = PL_IDLE;
      Serial.println("[PLANTS] aborted (page left)");
    }
    return;
  }
  if (plantState == PL_IDLE) {
    bool upd = false;
    if (now - plantLiveT >= 400) {
      plantLiveT = now;
      plantLiveLux = ldrToLux(analogRead(LDR_PIN));
      plantHasLive = true; upd = true;
    }
    // DHT11: single shot every 2 s (dht11Read() retries with delays and
    // would block; here a miss just keeps the previous value)
    if (now - plantLiveDhtT >= 2000) {
      plantLiveDhtT = now;
      float t, h;
      if (dht11ReadOnce(t, h)) { plantLiveTemp = t; plantLiveHum = h; plantLiveDht = true; upd = true; }
    }
    if (upd) gDirty = true;
    return;
  }
  if (plantState != PL_MEASURING) return;
  if (now - plantLastSample >= 400) {
    plantLastSample = now;
    int adc = analogRead(LDR_PIN);
    plantLuxAcc += ldrToLux(adc);
    plantAdcAcc += adc;
    plantSamples++;
    gDirty = true;
  }
  if (now - plantLastDht >= 2000) {   // DHT11: max 1 real read/s
    plantLastDht = now;
    float t, h;
    if (dht11Read(t, h)) {
      plantTempAcc += t; plantHumAcc += h; plantDhtN++;
      plantDhtOk = true;
      Serial.printf("[PLANTS] DHT ok: %.0f C %.0f %%\n", t, h);
    } else {
      Serial.println("[PLANTS] DHT failed (check VCC 3.3V, DATA->GPIO14, GND)");
    }
  }
  if (now - plantT0 >= PLANT_MEAS_MS) {
    if (plantSamples > 0) plantLux = plantLuxAcc / plantSamples;
    if (plantDhtOk && plantDhtN > 0) {      plantTemp = plantTempAcc / plantDhtN;
      plantHum = plantHumAcc / plantDhtN;
    } else {
      strlcpy(plantErr, "DHT11 no reading", sizeof(plantErr));
      plantTemp = NAN; plantHum = -1;
    }
    plantState = PL_RESULT;
    sound(SND_DONE);
    long adcAvg = plantSamples > 0 ? plantAdcAcc / plantSamples : 0;
    Serial.printf("[PLANTS] %s: ADC=%ld -> %.0f lux T=%.1f H=%.0f %s\n", PLANTS[plantIdx].nameL[LANG_EN],
                  adcAvg, plantLux, plantTemp, plantHum, plantErr);
    gDirty = true;
  }
}

void plantTap(int x, int y) {
  if (plantState == PL_MEASURING) return;  // ignore touches while measuring
  if (plantState == PL_RESULT) {
    if (y >= 164) {
      if (x < 160) plantStart();       // measure again
      else { plantState = PL_IDLE; gDirty = true; }
      return;
    }
    plantState = PL_IDLE; gDirty = true;  // tap top = back to catalog
    return;
  }
  // PL_IDLE: bottom strip = catalog (6 slots of 48 px from x=16, y>=158),
  // tap anywhere above = measure here (scout page doubles as finder)
  if (y >= 158) {
    if (x >= 16 && x < 304) {
      int i = constrain((x - 16) / 48, 0, PLANT_N - 1);
      if (i != plantIdx) { plantIdx = i; toast(PLANTS[plantIdx].name(), 800); gDirty = true; }
    }
    return;
  }
  plantStart();
}

// Low/Ideal/High chip: -1 low, 0 ideal, 1 high
int plantRate(float v, float lo, float hi) {
  if (isnan(v) || v < 0) return -2;  // no data
  if (v < lo) return -1;
  if (v > hi) return 1;
  return 0;
}
void plantChip(int x, int y, int w, const char* label, int rate) {
  uint16_t c = rate == 0 ? P.ok : (rate == -2 ? P.inkDim : P.warn);
  const char* s = rate == 0 ? "Ideal" : (rate == -1 ? TR("Low", "Bajo") : (rate == 1 ? TR("High", "Alto") : "--"));
  spr.fillRoundRect(x, y, w, 22, 7, P.card);
  spr.drawRoundRect(x, y, w, 22, 7, c);
  char b[24]; snprintf(b, sizeof(b), "%s %s", label, s);
  txt(b, x + w / 2, y + 11, 1, MC_DATUM, c);
}

// --- v3 visuals: nibble icon -> screen (tinted by brightness) ---
uint16_t plantIconCol(uint8_t n) {
  switch (n) {
    case 1: return tint(rgb(11, 61, 46));    // shadow
    case 2: return P.ok;                      // main green (C_OK)
    case 3: return tint(rgb(165, 243, 207));  // highlight
    case 4: return tint(rgb(16, 26, 22));     // pot (greenish black)
    default: return 0;
  }
}
void drawPlantIcon(int idx, int x, int y, int scale) {
  if (idx < 0 || idx >= PLANT_N) return;
  const uint8_t* d = PLANT_ICONS[idx];
  for (int py = 0; py < 32; py++) {
    for (int px = 0; px < 32; px++) {
      uint8_t b = d[(py * 32 + px) >> 1];
      uint8_t n = (px & 1) ? (b & 0x0F) : (b >> 4);
      if (!n) continue;
      if (scale <= 1) spr.drawPixel(x + px, y + py, plantIconCol(n));
      else spr.fillRect(x + px * scale, y + py * scale, scale, scale, plantIconCol(n));
    }
  }
}
// Ideal-range bar with live marker. Lux is log-scale (100..60000).
// rate colors the zone + marker: 0 = inside (green), +-1 = outside (amber),
// -2 = no data yet (dim, marker parked at the left edge).
void plantBar(int x, int y, int w, float v, float lo, float hi,
              float dmin, float dmax, bool isLog, int rate) {
  auto mapv = [&](float t) -> float {
    t = constrain(t, dmin, dmax);
    if (isLog) return (log10f(t) - log10f(dmin)) / (log10f(dmax) - log10f(dmin));
    return (t - dmin) / (dmax - dmin);
  };
  spr.fillRoundRect(x, y, w, 6, 3, P.line);
  int x0 = x + (int)(w * mapv(lo)), x1 = x + (int)(w * mapv(hi));
  uint16_t zc = rate == 0 ? P.ok : (rate == -2 ? P.line : P.inkDim);
  if (x1 > x0) spr.fillRoundRect(x0, y, x1 - x0, 6, 2, zc);
  float f = (rate == -2) ? 0 : mapv(v);
  int mx = x + (int)(w * constrain(f, 0, 1));
  uint16_t mc = rate == 0 ? P.ok : (rate == -2 ? P.inkDim : P.warn);
  spr.drawFastVLine(mx, y - 3, 12, P.ink);
  spr.fillCircle(mx, y + 3, 3, mc);
}
int plantScore(float lux, float t, float h, const PlantSpec& p) {
  bool noDht = (isnan(t) || h < 0);
  float pl = 0, pt = 0, ph = 0;
  if (lux < p.luxMin) pl = constrain((p.luxMin - lux) / p.luxMin, 0, 1);
  else if (lux > p.luxMax) pl = constrain((lux - p.luxMax) / p.luxMax, 0, 1);
  if (!noDht) {
    if (t < p.tMin) pt = constrain((p.tMin - t) / 10.0f, 0, 1);
    else if (t > p.tMax) pt = constrain((t - p.tMax) / 10.0f, 0, 1);
    if (h < p.hMin) ph = constrain((p.hMin - h) / 40.0f, 0, 1);
    else if (h > p.hMax) ph = constrain((h - p.hMax) / 40.0f, 0, 1);
    return constrain((int)(100 - 50 * pl - 25 * pt - 25 * ph), 0, 100);
  }
  return constrain((int)(100 - 100 * pl), 0, 100);
}
// One actionable line: worst offender first (light > temp > humidity)
const char* plantAdvice(int rl, int rt, int rh) {
  if (rl < 0) return TR("Move nearer the window", "Acerca a la ventana");
  if (rl > 0) return TR("Shade from direct sun", "Quita del sol directo");
  if (rt < 0) return TR("Warmer spot, no draught", "Sitio calido, sin frio");
  if (rt > 0) return TR("Ventilate a little", "Ventila un poco");
  if (rh < 0) return TR("Mist the leaves", "Pulveriza las hojas");
  if (rh > 0) return TR("Air out, less water", "Ventila, menos agua");
  return TR("Happy here!", "Feliz aqui!");
}

void drawPlantIdle(const PlantSpec& p) {
  char b[40];
  // Live rates from scout values (unknown until first scout sample)
  int rl = !plantHasLive ? -2 : plantRate(plantLiveLux, p.luxMin, p.luxMax);
  int rt = !plantLiveDht ? -2 : plantRate(plantLiveTemp, p.tMin, p.tMax);
  int rh = !plantLiveDht ? -2 : plantRate(plantLiveHum, p.hMin, p.hMax);
  bool allOk = plantHasLive && plantLiveDht && rl == 0 && rt == 0 && rh == 0;
  uint16_t cl = rl == 0 ? P.ok : (rl == -2 ? P.inkDim : P.warn);
  uint16_t ct = rt == 0 ? P.ok : (rt == -2 ? P.inkDim : P.warn);
  uint16_t ch = rh == 0 ? P.ok : (rh == -2 ? P.inkDim : P.warn);
  // left creature card (8,46,116,100 -> 46..146): big icon + name
  spr.fillRoundRect(8, 46, 116, 100, 10, P.card);
  spr.drawRoundRect(8, 46, 116, 100, 10, allOk ? P.ok : P.line);
  drawPlantIcon(plantIdx, 34, 52, 2);   // 52..116
  txtFit(p.name(), 66, 130, 104, 2, MC_DATUM, P.accent);   // ~122..138
  // right: 3 live gauges (value colored by status)
  if (plantHasLive) snprintf(b, sizeof(b), "%s %.0f lx", TR("Light", "Luz"), plantLiveLux);
  else snprintf(b, sizeof(b), "%s -- lx", TR("Light", "Luz"));
  txt(b, 132, 50, 2, TL_DATUM, cl);
  plantBar(132, 68, 180, plantLiveLux, p.luxMin, p.luxMax, 100, 60000, true, rl);
  if (plantLiveDht) snprintf(b, sizeof(b), "Temp %.0f C", plantLiveTemp);
  else snprintf(b, sizeof(b), "Temp -- C");
  txt(b, 132, 80, 2, TL_DATUM, ct);
  plantBar(132, 98, 180, plantLiveTemp, p.tMin, p.tMax, 10, 35, false, rt);
  if (plantLiveDht) snprintf(b, sizeof(b), "Hum %.0f %%", plantLiveHum);
  else snprintf(b, sizeof(b), "Hum -- %%");
  txt(b, 132, 110, 2, TL_DATUM, ch);
  plantBar(132, 128, 180, plantLiveHum, p.hMin, p.hMax, 0, 100, false, rh);   // ends 134
  // live advice: 2-line box in the right column (136..156), clear of card+strip
  const char* adv; uint16_t advC;
  if (rl == -2 && rt == -2) { adv = TR("Seeking...", "Buscando..."); advC = P.inkDim; }
  else {
    int ql = rl == -2 ? 0 : rl, qt = rt == -2 ? 0 : rt, qh = rh == -2 ? 0 : rh;
    adv = plantAdvice(ql, qt, qh);
    advC = (ql == 0 && qt == 0 && qh == 0) ? P.ok : P.warn;
  }
  txtWrap(adv, 222, 136, 172, 20, 1, advC);
  // bottom: pixel-art catalog strip (6 x 48 px slots, 160..194)
  for (int i = 0; i < PLANT_N; i++) {
    int sx = 16 + i * 48;
    spr.fillRoundRect(sx, 160, 44, 34, 8, P.card);
    if (i == plantIdx) spr.drawRoundRect(sx, 160, 44, 34, 8, P.accent);
    else spr.drawRoundRect(sx, 160, 44, 34, 8, P.line);
    drawPlantIcon(i, sx + 6, 161, 1);   // 161..193
  }
}

void drawPlantPage() {
  drawHeader(TR("PLANTS", "PLANTAS"));
  const PlantSpec& p = PLANTS[plantIdx];
  char b[64];
  drawEyesAt(SCR_W / 2, 38, 0.22f);   // the creature looks at the plants

  if (plantState == PL_IDLE) { drawPlantIdle(p); return; }
  if (plantState == PL_MEASURING) {
    float f = constrain((float)(millis() - plantT0) / PLANT_MEAS_MS, 0, 1);
    uint32_t left = (PLANT_MEAS_MS - (millis() - plantT0) + 999) / 1000;
    int yo = (int)(sinf(millis() / 300.0f) * 3);   // breathing plant
    drawPlantIcon(plantIdx, 128, 56 + yo, 2);
    snprintf(b, sizeof(b), TR("Measuring... %lu s", "Midiendo... %lu s"), (unsigned long)left);
    txt(b, SCR_W / 2, 134, 2, MC_DATUM, P.accent);
    spr.fillRoundRect(60, 148, 200, 8, 4, P.card);
    spr.fillRoundRect(62, 150, (int)(196 * f), 4, 2, P.accent);
    float avg = plantSamples > 0 ? plantLuxAcc / plantSamples : 0;
    if (plantSamples > 0 && plantLiveDht)
      snprintf(b, sizeof(b), "%.0f lx  %.0f C  %.0f %%", avg, plantLiveTemp, plantLiveHum);
    else if (plantSamples > 0)
      snprintf(b, sizeof(b), "%.0f lux  %d/8 s", avg, (int)((millis() - plantT0) / 1000));
    else snprintf(b, sizeof(b), TR(" hold still, sensors clear ", " quieto, sin tapar sensores "));
    txt(b, SCR_W / 2, 164, 1, MC_DATUM, P.inkDim);
    return;
  }
  // RESULT: icon + score | values + chips | advice + buttons
  int rl = plantRate(plantLux, p.luxMin, p.luxMax);
  int rt = plantRate(plantTemp, p.tMin, p.tMax);
  int rh = plantHum < 0 ? -2 : plantRate(plantHum, p.hMin, p.hMax);
  int sc = plantScore(plantLux, plantTemp, plantHum, p);
  uint16_t scc = sc >= 80 ? P.ok : (sc >= 50 ? P.warn : P.danger);
  spr.fillRoundRect(8, 46, 116, 108, 10, P.card);   // 46..154
  spr.drawRoundRect(8, 46, 116, 108, 10, scc);
  txtFit(p.name(), 66, 54, 104, 1, MC_DATUM, P.inkDim);
  drawPlantIcon(plantIdx, 34, 60, 2);   // 60..124
  snprintf(b, sizeof(b), "%d", sc);
  txt(b, 66, 140, 4, MC_DATUM, scc);   // ~127..153
  if (plantErr[0] && isnan(plantTemp))
    txt(TR("DHT11 no reading", "DHT11 sin lectura"), 222, 58, 1, MC_DATUM, P.danger);
  else {
    snprintf(b, sizeof(b), "%.0f lx  %.0f C  %.0f %%", plantLux, plantTemp, plantHum);
    txt(b, 222, 58, 2, MC_DATUM, P.ink);
  }
  plantChip(132, 72, 180, TR("Light", "Luz"), rl);
  plantChip(132, 98, 180, "Temp", rt);
  plantChip(132, 124, 180, "Hum", rh);   // ends 146
  txtFit(plantAdvice(rl == -2 ? 0 : rl, rt == -2 ? 0 : rt, rh == -2 ? 0 : rh),
         SCR_W / 2, 158, 300, 1, MC_DATUM, (rl == 0 && rt == 0 && (rh == 0 || rh == -2)) ? P.ok : P.warn);
  drawButton(8, 168, 150, 24, TR("Repeat", "Repetir"), true, P.ok);
  drawButton(162, 168, 150, 24, TR("Plants", "Plantas"), false);
}
