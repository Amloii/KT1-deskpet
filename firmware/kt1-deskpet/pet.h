// ===========================================================================
//  pet.h — Capacitive TTP223: single-channel physical button (DO high while touched).
//
//  Detection (non-blocking, in petTick): short tap (< PET_TAP_MS),
//  hold (>= PET_CARESS_MS) and a purr repeated every 3 s while the finger
//  stays on it (only if the hold ended up as "love").
//
//  The ACTION is contextual and decided by onSensorTap()/onSensorHold(),
//  defined in the .ino after all modules (coach, pomo and vital are not
//  visible here because pet.h is included earlier). Priority:
//  coach > posture nudge > vital alert > pomo > vital phase > plants > greeting.
//  Every action answers with toast + sound + mood (S6: the button is hidden).
//
//  The module RUNS AT 3.3V (at 5V its DO would output 5V and burn the GPIO).
// ===========================================================================
#pragma once

#if PET_ENABLED
bool     petDown = false;
uint32_t petT0 = 0, petLastEdge = 0, petLastPurr = 0;
bool     petLoved = false, petRepeat = false;

void petInit() {
  pinMode(PET_PIN, INPUT);
  Serial.printf("[PET] TTP223 on GPIO %d ready (DO at 3.3V)\n", PET_PIN);
}

void petTick() {
  uint32_t now = millis();
  bool v = digitalRead(PET_PIN) == HIGH;
  if (v != petDown && now - petLastEdge < 30) return;   // debounce
  if (v != petDown) {
    petLastEdge = now;
    petDown = v;
    if (v) { petT0 = now; petLoved = false; petRepeat = false; }
    else {
      if (!petLoved && now - petT0 < PET_TAP_MS) onSensorTap();
      lastInteraction = now;
      gDirty = true;
    }
  } else if (petDown && !petLoved && now - petT0 >= PET_CARESS_MS) {
    petLoved = true;
    petRepeat = onSensorHold();   // true = love: keep purring while held
    petLastPurr = now;
    Serial.println("[PET] held 900 ms");
  } else if (petDown && petLoved && petRepeat && now - petLastPurr > 3000) {
    setMoodFor(M_LOVE, MANUAL_MOOD_MS);   // still petting: does not expire
    sound(SND_PURR);
    petLastPurr = now;
  }
  if (petDown) lastInteraction = now;   // touching counts as activity
}
#endif
