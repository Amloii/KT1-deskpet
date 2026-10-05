// ===========================================================================
//  plants.h — Plant check v2 (DHT11 + LDR).
//  When started: measures ~8 s (averages light + temp + humidity) and compares
//  with the chosen plant's table: Low / Ideal / High with its range.
//  Rough indoor ranges (light in approx. lux, uncalibrated LDR).
//  Visual: rounded creature card + small eyes on top.
// ===========================================================================
#pragma once

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
  if (plantState != PL_MEASURING) return;
  uint32_t now = millis();
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
    if (y >= 172) {
      if (x < 160) plantStart();       // measure again
      else { plantState = PL_IDLE; gDirty = true; }
      return;
    }
    // tapping the top also changes plant on the result screen
  }
  // PL_IDLE: selector + measure
  if (y >= 60 && y < 92) {
    if (x < 60) { plantIdx = (plantIdx + PLANT_N - 1) % PLANT_N; toast(PLANTS[plantIdx].name(), 800); return; }
    if (x > SCR_W - 60) { plantIdx = (plantIdx + 1) % PLANT_N; toast(PLANTS[plantIdx].name(), 800); return; }
  }
  if (y >= 92 && y < 118) {
    // plant dots (6): same coordinates as the drawing (y=104)
    int sp = 26, x0 = SCR_W / 2 - (PLANT_N - 1) * sp / 2;
    for (int i = 0; i < PLANT_N; i++) {
      if (abs(x - (x0 + i * sp)) < 14) { plantIdx = i; gDirty = true; return; }
    }
  }
  if (y >= 168) {
    if (x < 210) plantStart();   // "Measure" drawn at x 8-208
    else { plantIdx = (plantIdx + 1) % PLANT_N; gDirty = true; }
  }
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

void drawPlantPage() {
  drawHeader(TR("PLANTS", "PLANTAS"));
  const PlantSpec& p = PLANTS[plantIdx];
  drawEyesAt(SCR_W / 2, 52, 0.32f);   // the creature looks at the plants
  char b[48];

  // plant selector (rounded creature arrows)
  drawArrowButton(4, 66, 44, 28, true);
  drawArrowButton(SCR_W - 48, 66, 44, 28, false);
  txt(p.name(), SCR_W / 2, 80, 4, MC_DATUM, P.accent);
  int sp = 26, x0 = SCR_W / 2 - (PLANT_N - 1) * sp / 2;
  for (int i = 0; i < PLANT_N; i++) {
    if (i == plantIdx) spr.fillCircle(x0 + i * sp, 104, 4, P.accent);
    else spr.drawCircle(x0 + i * sp, 104, 3, P.inkDim);
  }

  if (plantState == PL_IDLE) {
    snprintf(b, sizeof(b), TR("Light %d-%d lux", "Luz %d-%d lux"), p.luxMin, p.luxMax);
    txt(b, SCR_W / 2, 122, 2, MC_DATUM, P.ink);
    snprintf(b, sizeof(b), "Temp %d-%d C   Hum %d-%d %%", p.tMin, p.tMax, p.hMin, p.hMax);
    txt(b, SCR_W / 2, 140, 1, MC_DATUM, P.inkDim);
    txt(TR("Place buddy next to the plant", "Pon el buddy junto a la planta"), SCR_W / 2, 156, 1, MC_DATUM, P.inkDim);
    drawButton(8, 168, 200, 26, TR("Measure (~8 s)", "Medir (~8 s)"), true, P.ok);
    drawButton(214, 168, 98, 26, TR("Plant", "Planta"), false);
    return;
  }
  if (plantState == PL_MEASURING) {
    float f = constrain((float)(millis() - plantT0) / PLANT_MEAS_MS, 0, 1);
    uint32_t left = (PLANT_MEAS_MS - (millis() - plantT0) + 999) / 1000;
    snprintf(b, sizeof(b), TR("Measuring... %lu s", "Midiendo... %lu s"), (unsigned long)left);
    txt(b, SCR_W / 2, 122, 2, MC_DATUM, P.accent);
    spr.fillRoundRect(60, 142, 200, 10, 5, P.card);
    spr.fillRoundRect(62, 144, 196 * f, 6, 3, P.accent);
    txt(TR(" hold still, sensors clear ", " quieto, sin tapar sensores "), SCR_W / 2, 160, 1, MC_DATUM, P.inkDim);
    return;
  }
  // RESULT
  if (plantErr[0] && isnan(plantTemp)) txt(TR("DHT11 no reading", "DHT11 sin lectura"), SCR_W / 2, 118, 1, MC_DATUM, P.danger);
  else {
    snprintf(b, sizeof(b), "%.0f lux  %.0f C  %.0f %%", plantLux, plantTemp, plantHum);
    txt(b, SCR_W / 2, 118, 2, MC_DATUM, P.ink);
  }
  int rl = plantRate(plantLux, p.luxMin, p.luxMax);
  int rt = plantRate(plantTemp, p.tMin, p.tMax);
  int rh = plantHum < 0 ? -2 : plantRate(plantHum, p.hMin, p.hMax);
  plantChip(14, 134, 92, TR("Light", "Luz"), rl);
  plantChip(114, 134, 92, "Temp", rt);
  plantChip(214, 134, 92, "Hum", rh);
  // ideal range below
  snprintf(b, sizeof(b), "ideal: %d-%d lx %d-%dC %d-%d%%", p.luxMin, p.luxMax, p.tMin, p.tMax, p.hMin, p.hMax);
  txt(b, SCR_W / 2, 162, 1, MC_DATUM, P.inkDim);
  drawButton(8, 168, 150, 26, TR("Repeat", "Repetir"), true, P.ok);
  drawButton(162, 168, 150, 26, TR("Plants", "Plantas"), false);
}
