// ===========================================================================
//  touch.h — Direct, NON-blocking read of the FT6336U touch controller (I2C 0x38)
//
//  Why the Freenove FT6336U library is not used:
//   - readByte() does  do { ... } while (rdDataCount == 0);  -> if a read fails it
//     retries FOREVER and KT1 hung on the "Starting..." screen.
//   - It adds delay(10) per register (~60 ms per read): bad idea at 30 fps.
//  Here: 5-byte read (registers 0x02..0x06) with 2 methods and a short retry.
//  The point's "event flag" is also checked: some panels keep TD_STATUS = 1 after
//  the finger is lifted, with the "lift up" event -> that is NOT a touch.
// ===========================================================================
#pragma once

#define FT_ADDR        0x38
#define FT_REG_STATUS  0x02      // TD_STATUS: fingers (bits 3..0); then P1_XH, P1_XL, P1_YH, P1_YL
#define FT_REG_CHIPID  0xA3      // 0x64 = FT6336U (other FT6x06 give 0x06, 0x36...)

bool     touchOk = false;
uint32_t touchNextRetry = 0;
int      touchFails = 0;

struct TouchDbg { uint32_t ok = 0, fail = 0; int n = 0, ev = 0, rx = 0, ry = 0; } tdbg;   // diagnostics

// Writes the register and reads n bytes with a REPEATED START (like the Freenove library)
bool i2cReadReg(TwoWire& w, uint8_t addr, uint8_t reg, uint8_t* buf, uint8_t n) {
  w.beginTransmission(addr);
  w.write(reg);
  if (w.endTransmission(false) != 0) return false;
  if (w.requestFrom(addr, n) != n) { while (w.available()) w.read(); return false; }
  for (uint8_t i = 0; i < n; i++) buf[i] = w.read();
  return true;
}
// Same but with a STOP between write and read (some chips prefer it)
bool i2cReadRegStop(TwoWire& w, uint8_t addr, uint8_t reg, uint8_t* buf, uint8_t n) {
  w.beginTransmission(addr);
  w.write(reg);
  if (w.endTransmission(true) != 0) return false;
  if (w.requestFrom(addr, n) != n) { while (w.available()) w.read(); return false; }
  for (uint8_t i = 0; i < n; i++) buf[i] = w.read();
  return true;
}
bool ftRead(uint8_t reg, uint8_t* buf, uint8_t n) {
  for (int a = 0; a < 2; a++) {
    if (i2cReadReg(Wire, FT_ADDR, reg, buf, n))     return true;
    if (i2cReadRegStop(Wire, FT_ADDR, reg, buf, n)) return true;
  }
  return false;
}

// Checks the bus before Wire.begin(). If a device hung in the middle of a read
// (e.g. after a RESET without power cycling) it holds SDA low and blocks the WHOLE
// bus (touch, codec, IMU). It is released with up to 9 clock pulses + a STOP.
bool i2cBusRecover(int sda, int scl) {
  pinMode(sda, INPUT_PULLUP); pinMode(scl, INPUT_PULLUP);
  delayMicroseconds(20);
  bool sdaLow = digitalRead(sda) == LOW, sclLow = digitalRead(scl) == LOW;
  if (!sdaLow && !sclLow) { Serial.println("[I2C] Bus free (SDA and SCL high)"); return true; }
  Serial.printf("[I2C] Bus STUCK at boot: SDA %s, SCL %s\n", sdaLow ? "LOW" : "high", sclLow ? "LOW" : "high");
  if (sclLow) {
    Serial.println("[I2C] SCL shorted to GND: short circuit or badly wired device. Disconnect the GY-521 and check the wires");
    return false;
  }
  pinMode(scl, OUTPUT_OPEN_DRAIN); digitalWrite(scl, HIGH);
  for (int i = 0; i < 9 && digitalRead(sda) == LOW; i++) {
    digitalWrite(scl, LOW);  delayMicroseconds(10);
    digitalWrite(scl, HIGH); delayMicroseconds(10);
  }
  pinMode(sda, OUTPUT_OPEN_DRAIN);                    // STOP condition: SDA rises while SCL is high
  digitalWrite(sda, LOW);  delayMicroseconds(10);
  digitalWrite(scl, HIGH); delayMicroseconds(10);
  digitalWrite(sda, HIGH); delayMicroseconds(10);
  pinMode(sda, INPUT_PULLUP); pinMode(scl, INPUT_PULLUP);
  delayMicroseconds(20);
  bool ok = digitalRead(sda) == HIGH;
  Serial.println(ok ? "[I2C] Bus released" : "[I2C] SDA still low: hung device or short circuit on SDA");
  return ok;
}

bool touchDetect() {
  uint8_t id = 0;
  bool ok = ftRead(FT_REG_CHIPID, &id, 1);
  if (ok) Serial.printf("[TOUCH] FT6336U detected (chip ID 0x%02X)\n", id);
  return ok;
}

void touchInit() {
  pinMode(TOUCH_INT, INPUT);
  pinMode(TOUCH_RST, OUTPUT);
  digitalWrite(TOUCH_RST, LOW);  delay(10);           // same reset as the Freenove library
  digitalWrite(TOUCH_RST, HIGH); delay(500);
  for (int i = 0; i < 8 && !touchOk; i++) { touchOk = touchDetect(); if (!touchOk) delay(100); }
  if (!touchOk) Serial.println("[TOUCH] Not responding (retrying every 3 s; everything else works)");
}

// Returns true if a finger is down; x,y in native panel coordinates (portrait 240x320)
bool touchRead(int& x, int& y) {
  uint32_t now = millis();
  if (!touchOk) {
    if (now >= touchNextRetry) { touchNextRetry = now + 3000; touchOk = touchDetect(); }
    return false;
  }
  uint8_t d[5];
  if (!ftRead(FT_REG_STATUS, d, 5)) {
    tdbg.fail++;
    if (++touchFails > 60) { touchOk = false; touchFails = 0; Serial.println("[TOUCH] Lost, retrying..."); }
    return false;
  }
  tdbg.ok++;
  touchFails = 0;
  uint8_t n  = d[0] & 0x0F;
  uint8_t ev = d[1] >> 6;               // 0 = press down, 1 = lift up, 2 = contact, 3 = no event
  x = ((d[1] & 0x0F) << 8) | d[2];
  y = ((d[3] & 0x0F) << 8) | d[4];
  tdbg.n = n; tdbg.ev = ev; tdbg.rx = x; tdbg.ry = y;
  if (n == 0 || n > 2) return false;
  if (ev == 1 || ev == 3) return false; // finger already lifted
  return true;
}
