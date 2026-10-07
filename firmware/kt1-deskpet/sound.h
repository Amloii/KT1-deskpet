// ===========================================================================
//  sound.h — Beeps and short tunes on the board speaker (ES8311)
//
//  Tones are generated in their own task (core 0) that receives "Sound" values
//  through a queue: writing to I2S (blocking) never slows down the animation.
//  Auto mute: during calls, at night or with sound disabled.
// ===========================================================================
#pragma once

#define SND_RATE 16000

#if SOUND_ENABLED
I2SClass      i2sOut;
QueueHandle_t sndQ = nullptr;
TaskHandle_t  sndTaskH = nullptr;   // v3: chat.h suspends it during voice/TTS
bool          soundOk = false;

static void playTone(float freq, int ms, float vol = 1.0f) {
  const int n = SND_RATE * ms / 1000;
  const int att = SND_RATE / 200;                   // 5 ms ramp (no "clicks")
  int16_t buf[512];                                 // 256 stereo frames (L+R with the same sample:
                                                    // the Ente plays in stereo; in left-only mono the
                                                    // speaker could stay silent depending on the wired channel)
  float ph = 0, dp = 2.0f * PI * freq / SND_RATE;
  for (int i = 0; i < n;) {
    int k = min(256, n - i);
    for (int j = 0; j < k; j++, i++) {
      float env = 1.0f;
      if (i < att) env = (float)i / att;
      else if (n - i < att) env = (float)(n - i) / att;
      int16_t s = (freq <= 0) ? 0 : (int16_t)(sinf(ph) * SOUND_AMP * vol * env);
      ph += dp; if (ph > 2.0f * PI) ph -= 2.0f * PI;
      buf[2 * j] = s; buf[2 * j + 1] = s;
    }
    i2sOut.write((uint8_t*)buf, k * 2 * sizeof(int16_t));
  }
}
static void playSilence(int ms) { playTone(0, ms); }

static void soundTask(void*) {
  uint8_t s;
  for (;;) {
    if (xQueueReceive(sndQ, &s, portMAX_DELAY) != pdTRUE) continue;
    playSilence(25);
    switch (s) {
      case SND_TICK:  playTone(1400, 35, 0.6f); break;                       // every rep
      case SND_GO:    playTone(880, 90); playSilence(40); playTone(1320, 160); break;
      case SND_SIDE:  playTone(988, 90); playSilence(60); playTone(988, 90); break;
      case SND_STAND: playTone(523, 120); playTone(659, 120); playTone(784, 220); break;   // rising
      case SND_SIT:   playTone(784, 120); playTone(659, 120); playTone(523, 220); break;   // falling
      case SND_MOVE:  for (int i = 0; i < 3; i++) { playTone(1047, 110); playSilence(70); } break;
      case SND_DONE:  playTone(523, 110); playTone(659, 110); playTone(784, 110); playTone(1047, 260); break;
      case SND_HELLO: playTone(659, 90); playTone(988, 150); break;
      case SND_PURR:  for (int i = 0; i < 7; i++) { playTone(150, 70, 0.8f); playSilence(55); } break;
      // Pomodoro per-mode jingles: Trabajo bright, Escritura soft mid, Ocio low and gentle
      case SND_PW_GO: playTone(880, 90); playSilence(40); playTone(1175, 90); playSilence(40); playTone(1320, 180); break;
      case SND_PE_GO: playTone(659, 120); playTone(880, 120); playTone(988, 200); break;
      case SND_PO_GO: playTone(523, 140, 0.8f); playTone(659, 140, 0.8f); playTone(784, 240, 0.8f); break;
      case SND_PW_END: playTone(784, 100); playTone(1047, 220); break;
      case SND_PE_END: playTone(659, 100); playTone(988, 220); break;
      case SND_PO_END: playTone(523, 120, 0.8f); playTone(784, 240, 0.8f); break;
    }
    playSilence(60);
  }
}

// Detects the codec by READING its ID register (0xFD = 0x83 on the ES8311), like its
// driver does. The "address only" probe is not used: it was unreliable on this board.
static bool probeCodec(int tries, int gapMs) {
  for (int i = 0; i < tries; i++) {
    uint8_t id = 0;
    if (i2cReadReg(Wire, ES8311_ADDRESS_0, 0xFD, &id, 1)) {
      Serial.printf("[SOUND] ES8311 detected (ID 0x%02X)\n", id);
      return true;
    }
    delay(gapMs);
  }
  return false;
}

// Fail-safe codec startup. History on this board:
//  - With the codec started ~1 s after boot, BEFORE I2S and with GPIO 1 HIGH, the ES8311
//    did not respond (I2C read/write error -> abort -> reboot loop).
//  - In ESP32S3_Ente / Chatbot (which work) it starts after Wi-Fi connects (several s later)
//    with GPIO 1 LOW. Espressif's official example (i2s_es8311) starts I2S (MCLK) first.
// Hence: GPIO 1 on -> I2S/MCLK -> wait until AUDIO_BOOT_WAIT_MS -> probe the codec with
// retries -> if missing, try the other GPIO 1 polarity and keep whichever works.
#define AUDIO_BOOT_WAIT_MS 2500
uint8_t ampLevel = AMP_ON_LEVEL;

void soundInit() {
  pinMode(AMP_PIN, OUTPUT);
  digitalWrite(AMP_PIN, ampLevel);

  // 1) I2S first: MCLK running on GPIO 4 before talking to the codec.
  //    Stereo with duplicated L+R (like the Ente, which works): in left-only mono
  //    the speaker stayed silent if the board mixes the other channel.
  i2sOut.setPins(I2S_BCK, I2S_WS, I2S_DOUT, I2S_DIN, I2S_MCK);
  bool i2sOk = i2sOut.begin(I2S_MODE_STD, SND_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH);
  if (!i2sOk) Serial.println("[SOUND] i2s.begin failed");

  // 2) Give the audio block time (in the working sketches several seconds pass)
  while (millis() < AUDIO_BOOT_WAIT_MS) delay(10);     // Wire is already started in setup()

  // 3) Probe the codec (1 s of retries)
  bool found = probeCodec(10, 100);
  if (!found) {
    Serial.printf("[SOUND] ES8311 not responding with AP_ENABLE=%s\n", ampLevel == LOW ? "LOW" : "HIGH");
    // 4) Try the other GPIO 1 polarity
    ampLevel = !ampLevel;
    digitalWrite(AMP_PIN, ampLevel);
    delay(400);
    found = probeCodec(10, 100);
    Serial.printf("[SOUND] With AP_ENABLE=%s: %s\n", ampLevel == LOW ? "LOW" : "HIGH",
                  found ? "RESPONDS -> using this polarity (set AMP_ON_LEVEL to it)" : "no response either");
    if (!found) {
      ampLevel = AMP_ON_LEVEL;
      digitalWrite(AMP_PIN, ampLevel);
      Serial.println("[SOUND] No sound. The rest of KT1 works normally.");
      return;
    }
  }
  if (!i2sOk) { Serial.println("[SOUND] Codec OK but I2S failed -> no sound"); return; }
  if (es8311_codec_init() != ESP_OK) { Serial.println("[SOUND] ES8311 init failed -> no sound"); return; }
  es8311_codec_set_fs(SND_RATE);
  es8311_codec_set_volume(SOUND_VOLUME);
  sndQ = xQueueCreate(8, sizeof(uint8_t));
  xTaskCreatePinnedToCore(soundTask, "snd", 4096, nullptr, 2, &sndTaskH, 0);
  soundOk = true;
  Serial.printf("[SOUND] ES8311 ready (AP_ENABLE=%s)\n", ampLevel == LOW ? "LOW" : "HIGH");
}
#endif

// Queues a sound (non-blocking). Respects mute and night (v2: no call detection).
void sound(Sound s) {
#if SOUND_ENABLED
  if (!soundOk || !gSoundOn) return;
  if (isNight()) return;
  uint8_t v = (uint8_t)s;
  xQueueSend(sndQ, &v, 0);
#else
  (void)s;
#endif
}
void soundSetVolume(int v) {
#if SOUND_ENABLED
  v = constrain(v, 0, 100);
  if (soundOk) es8311_codec_set_volume(v);
#endif
}

// v3: hand the codec+I2S over to the voice module (Gemini recording + TTS).
// Called from chat.h: suspends beeps, frees NUM_0; reopens it on return.
void soundAudioSuspend() {
#if SOUND_ENABLED
  if (sndTaskH) vTaskSuspend(sndTaskH);
  soundOk = false;
  i2sOut.end();
#endif
}
void soundAudioResume() {
#if SOUND_ENABLED
  es8311_codec_set_fs(SND_RATE);
  i2sOut.setPins(I2S_BCK, I2S_WS, I2S_DOUT, I2S_DIN, I2S_MCK);
  if (i2sOut.begin(I2S_MODE_STD, SND_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) {
    soundOk = true;
    if (sndTaskH) vTaskResume(sndTaskH);
  } else Serial.println("[SOUND] I2S reopen failed");
#endif
}
