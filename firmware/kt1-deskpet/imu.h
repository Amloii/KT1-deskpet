// ===========================================================================
//  imu.h — MPU6050 (GY-521): eye drift and shakes (dizzy).
//  Posture (sitting/standing) is MANUAL only: buttons, face, sensor and
//  pages. The old desk vibration detector was removed.
//  Runs in its own task at 100 Hz (core 0) and only publishes variables.
// ===========================================================================
#pragma once

#if IMU_ENABLED
float imuF[3]    = { 0, 0, 1 };     // filtered acceleration (g)
float imuBase[3] = { 0, 0, 1 };     // slowly adapting rest position

uint8_t  imuAddr = IMU_ADDR;
TwoWire* imuBus  = &Wire1;          // Wire (shared with the touch) or Wire1 (own bus)

bool imuWrite(uint8_t reg, uint8_t v) {
  imuBus->beginTransmission(imuAddr); imuBus->write(reg); imuBus->write(v);
  return imuBus->endTransmission() == 0;
}

// Read method for the 6 accelerometer bytes (chosen in imuInit based on what works):
//  0 = burst with repeated start, 1 = burst with STOP, 2 = byte by byte (clones without burst reads)
int imuMethod = 0;
const char* IMU_METHOD_NAMES[] = { "burst (repeated start)", "burst (with STOP)", "byte by byte" };

bool imuReadBytes(uint8_t reg, uint8_t* d, uint8_t n, int method) {
  if (method == 0) return i2cReadReg(*imuBus, imuAddr, reg, d, n);
  if (method == 1) return i2cReadRegStop(*imuBus, imuAddr, reg, d, n);
  for (uint8_t i = 0; i < n; i++)
    if (!i2cReadReg(*imuBus, imuAddr, reg + i, &d[i], 1)) return false;
  return true;
}
bool imuReadAccel(float a[3]) {
  uint8_t d[6];
  if (!imuReadBytes(0x3B, d, 6, imuMethod)) return false;
  for (int i = 0; i < 3; i++) a[i] = (int16_t)((d[2 * i] << 8) | d[2 * i + 1]) / 16384.0f;   // +-2 g
  return true;
}

const char* imuChipName(uint8_t who) {
  switch (who) {
    case 0x68: return "MPU6050";
    case 0x70: return "MPU6500";
    case 0x71: return "MPU9250";
    case 0x73: return "MPU9255";
    case 0x98: return "ICM-20689";
    case 0x12: return "ICM-20602";
    default:   return "unknown/clone";
  }
}

// Lists everything that answers on the bus (touch 0x38, codec 0x18, IMU 0x68/0x69...)
void i2cScan(TwoWire& w, const char* name) {
  Serial.printf("[I2C] Scan %s:", name);
  int found = 0;
  for (uint8_t a = 1; a < 127; a++) {
    w.beginTransmission(a);
    if (w.endTransmission() == 0) { Serial.printf(" 0x%02X", a); found++; }
  }
  Serial.println(found ? "" : " (nothing)");
}

// Reads WHO_AM_I (0x75) at 0x68 / 0x69 on the given bus
bool imuProbe(TwoWire& w, uint8_t& who) {
  for (uint8_t addr : { (uint8_t)IMU_ADDR, (uint8_t)0x68, (uint8_t)0x69 }) {
    uint8_t v = 0;
    if (i2cReadReg(w, addr, 0x75, &v, 1) && v != 0x00 && v != 0xFF) { imuAddr = addr; who = v; return true; }
  }
  return false;
}

// Looks for the GY-521:
//  1) on the internal bus 16/15 (board I2C connector), WITHOUT restarting it: the touch uses it
//  2) on its own bus (Wire1) on the configured pins, swapped, 2/14 and 9/21
bool imuFind(uint8_t& who) {
  if (imuProbe(Wire, who)) {
    imuBus = &Wire;
    Serial.printf("[IMU] On the board I2C connector (IO16/IO15), addr 0x%02X\n", imuAddr);
    return true;
  }
  const int cand[][2] = { { IMU_SDA, IMU_SCL }, { IMU_SCL, IMU_SDA }, { 2, 14 }, { 9, 21 } };
  for (auto& c : cand) {
    if ((c[0] == TOUCH_SDA || c[0] == TOUCH_SCL) && (c[1] == TOUCH_SDA || c[1] == TOUCH_SCL)) continue;  // already tried
#if PET_ENABLED
    if (c[0] == PET_PIN || c[1] == PET_PIN) continue;   // used by the TTP223: do not create Wire1 there
#endif
    Wire1.end();
    delay(5);
    if (!Wire1.begin(c[0], c[1], 100000)) continue;
    if (imuProbe(Wire1, who)) {
      imuBus = &Wire1;
      Serial.printf("[IMU] Found on SDA=%d SCL=%d addr 0x%02X (own bus)\n", c[0], c[1], imuAddr);
      Wire1.setClock(400000);
      return true;
    }
  }
  Wire1.end();
  return false;
}

void imuInit() {
  uint8_t who = 0;
  if (!imuFind(who)) {
    Serial.println("[IMU] Not responding (tried I2C connector 16/15, 2/14 and 9/21). Eyes with random drift.");
    Serial.println("[IMU] Check: VCC -> 3.3V, GND -> GND, SDA -> IO16, SCL -> IO15, GY-521 pins soldered");
    return;
  }
  Serial.printf("[IMU] WHO_AM_I=0x%02X (%s)\n", who, imuChipName(who));
  i2cScan(*imuBus, imuBus == &Wire ? "internal bus 16/15" : "own bus");

  // Wake up: full reset and then PLL clock (some clones do not start without the reset)
  bool wOk = imuWrite(0x6B, 0x80);   // DEVICE_RESET
  delay(100);
  wOk &= imuWrite(0x6B, 0x01);       // wake up, PLL clock
  delay(50);
  uint8_t pm = 0xEE;
  bool rOk = i2cReadReg(*imuBus, imuAddr, 0x6B, &pm, 1);
  Serial.printf("[IMU] PWR_MGMT_1: write %s, read %s = 0x%02X %s\n", wOk ? "OK" : "FAIL",
                rOk ? "OK" : "FAIL", pm, (rOk && (pm & 0x40)) ? "(still ASLEEP)" : "");

  // Internal 44 Hz filter (gaze + shakes; no desk detector)
  imuWrite(0x1A, 0x03);
  imuWrite(0x1C, 0x00);              // +-2 g

  // Try the 3 read methods and keep the first one that works
  imuMethod = -1;
  for (int m = 0; m < 3; m++) {
    int ok = 0; uint8_t d[6];
    for (int i = 0; i < 5; i++) { if (imuReadBytes(0x3B, d, 6, m)) ok++; delay(5); }
    Serial.printf("[IMU] Read %s: %d/5\n", IMU_METHOD_NAMES[m], ok);
    if (ok >= 4 && imuMethod < 0) imuMethod = m;
  }
  if (imuMethod < 0) {
    Serial.println("[IMU] Error reading data: no method works.");
    // Is the chip still alive? If WHO_AM_I also fails now, it dropped out when woken up (power)
    uint8_t who2 = 0;
    bool alive = i2cReadReg(*imuBus, imuAddr, 0x75, &who2, 1);
    Serial.printf("[IMU] After the failure: WHO_AM_I %s (0x%02X)\n", alive ? "responds" : "does NOT respond", who2);
    i2cScan(*imuBus, "after the failure");
    if (!alive) Serial.println("[IMU] The chip stops responding when woken up -> almost certainly power: "
                               "connect the GY-521 VCC to 5V (its regulator needs >3.3V) or check GND");
    Serial.println("[IMU] If WHO_AM_I responds but this fails: long/loose wire or missing pull-ups; "
                   "try shorter wires or power the GY-521 VCC from 5V (it has its own regulator)");
    return;
  }
  Serial.printf("[IMU] Using read %s\n", IMU_METHOD_NAMES[imuMethod]);

  // --- Calibration at boot: the device is powered on UPRIGHT (vertical) ---
  // The fixed 400 ms average caught the wobble of plugging/placing it and the
  // eyes ended up tilted. Now: show a notice, wait for real stillness
  // (axes stable for 1.5 s in a row, max 8 s) and take THAT rest as reference.
  {
    spr.fillSprite(PAPER);
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(P.ink, PAPER);
    spr.drawString(TR("Hold still...", "Quieto..."), SCR_W / 2, 100, 4);
    spr.setTextColor(P.inkDim, PAPER);
    spr.drawString(TR("calibrating gaze", "calibrando mirada"), SCR_W / 2, 130, 2);
    spr.pushSprite(0, 0);

    float a[3], prev[3] = { 0, 0, 0 };
    bool havePrev = false;
    int still = 0;
    uint32_t tCal = millis();
    while (millis() - tCal < 8000) {
      if (!imuReadAccel(a)) { delay(10); continue; }
      if (havePrev) {
        float d = fabsf(a[0] - prev[0]) + fabsf(a[1] - prev[1]) + fabsf(a[2] - prev[2]);
        if (d < 0.03f) still++;
        else still = 0;
      }
      for (int k = 0; k < 3; k++) prev[k] = a[k];
      havePrev = true;
      if (still >= 150) break;   // 1.5 s still
      delay(10);
    }
    if (still < 150) Serial.println("[IMU] Not fully still: calibrating with the latest data (keep it still when plugging in)");

    float acc[3] = { 0, 0, 0 };
    int n = 0;
    for (int i = 0; i < 100; i++) {
      if (imuReadAccel(a)) { for (int k = 0; k < 3; k++) acc[k] += a[k]; n++; }
      delay(10);
    }
    if (n == 0) { Serial.println("[IMU] Error reading data"); return; }
    for (int k = 0; k < 3; k++) imuF[k] = imuBase[k] = acc[k] / n;
  }
  float mag = sqrtf(imuBase[0] * imuBase[0] + imuBase[1] * imuBase[1] + imuBase[2] * imuBase[2]);
  if (mag < 0.3f || mag > 2.0f)
    Serial.printf("[IMU] Warning: |a|=%.2f g at rest (should be ~1.0). Suspicious data.\n", mag);
  imuOk = true;
  {
    const char* pos = fabsf(imuBase[2]) > 0.7f ? "lying flat" : "upright";
    Serial.printf("[IMU] OK. WHO_AM_I=0x%02X  rest=(%.2f, %.2f, %.2f) g (%s)\n",
                  who, imuBase[0], imuBase[1], imuBase[2], pos);
  }
}

void imuTask(void*) {
  uint32_t lastShakeHit = 0; int shakeHits = 0;
#if IMU_DEBUG
  uint32_t lastDbg = 0;
#endif
  for (;;) {
    float a[3];
    if (imuReadAccel(a)) {
      uint32_t now = millis();
      for (int k = 0; k < 3; k++) {
        imuF[k]    += 0.10f   * (a[k] - imuF[k]);
        imuBase[k] += 0.0015f * (imuF[k] - imuBase[k]);    // ~7 s
      }
      float dx = imuF[IMU_EYE_X_AXIS] - imuBase[IMU_EYE_X_AXIS];
      float dy = imuF[IMU_EYE_Y_AXIS] - imuBase[IMU_EYE_Y_AXIS];
      imuGazeX = constrain(IMU_EYE_X_SIGN * dx * IMU_GAIN_X, -45.0f, 45.0f);
      imuGazeY = constrain(IMU_EYE_Y_SIGN * dy * IMU_GAIN_Y, -28.0f, 28.0f);

      float mag = sqrtf(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);

      // --- Shake -> dizzy (only a REAL shake, not the small bump of tapping the screen) ---
      // Ignored while a finger is on the screen and for 300 ms after release.
      bool touching = ts.down || (now - ts.lastSeen < 300);
      if (!touching && fabsf(mag - 1.0f) > IMU_SHAKE_G) {
        shakeHits = (now - lastShakeHit < 500) ? shakeHits + 1 : 1;
        lastShakeHit = now;
        if (shakeHits >= 10) { dizzyUntil = now + 3000; shakeHits = 0; }   // ~1 s of real shaking
      }
      if (now - lastShakeHit > 800) shakeHits = 0;

#if IMU_DEBUG
      if (now - lastDbg > 250) {
        lastDbg = now;
        Serial.printf("[IMU] a=(%.2f %.2f %.2f) gaze=(%.0f %.0f)\n", imuF[0], imuF[1], imuF[2],
                      (float)imuGazeX, (float)imuGazeY);
      }
#endif
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
#endif
