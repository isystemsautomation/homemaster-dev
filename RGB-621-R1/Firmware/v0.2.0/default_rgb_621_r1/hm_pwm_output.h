#pragma once
// RGB-621-R1 12-bit PWM output: gamma LUT + non-blocking slew engine.

#include <Arduino.h>
#include <math.h>

static const uint16_t PWM_HI = 4095;
static const uint8_t  PWM_API_MAX = 255;

struct OutputQualityCfg {
  bool gammaEnable;
  uint8_t gammaTenths; // 22 = gamma 2.2
};

struct PwmChCfg;
extern PwmChCfg pwmChCfg[];
extern const uint8_t NUM_PWM;
extern const uint8_t PWM_PINS[];
extern uint16_t pwmTarget[];
extern uint16_t pwmCurrent[];
extern uint16_t pwmLastNonZero[];
extern uint32_t slewLastMs[];
extern uint16_t g_gammaLut[PWM_HI + 1];
extern OutputQualityCfg outQuality;
extern volatile uint32_t lastOutChangeMs;

inline uint16_t pwmApiToHi(uint16_t api) {
  return (uint16_t)((uint32_t)constrain((int)api, 0, 255) * PWM_HI / 255u);
}
inline uint8_t pwmHiToApi(uint16_t hi) {
  return (uint8_t)((uint32_t)constrain((int)hi, 0, PWM_HI) * 255u / PWM_HI);
}

inline uint16_t pwmTrimMinHi(uint8_t ch) {
  return pwmApiToHi(ch < NUM_PWM ? pwmChCfg[ch].minTrim : 1);
}
inline uint16_t pwmTrimMaxHi(uint8_t ch) {
  return pwmApiToHi(ch < NUM_PWM ? pwmChCfg[ch].maxTrim : 255);
}

inline void pwmBuildGammaLut() {
  const float g = outQuality.gammaEnable ? (outQuality.gammaTenths / 10.0f) : 1.0f;
  for (uint16_t i = 0; i <= PWM_HI; i++) {
    const float norm = (float)i / (float)PWM_HI;
    const float corrected = powf(norm, g);
    g_gammaLut[i] = (uint16_t)(corrected * (float)PWM_HI + 0.5f);
  }
}

inline uint16_t pwmApplyTrim(uint8_t ch, uint16_t perceived) {
  if (perceived == 0) return 0;
  const uint16_t tMin = pwmTrimMinHi(ch);
  const uint16_t tMax = pwmTrimMaxHi(ch);
  return (uint16_t)constrain((int)perceived, (int)tMin, (int)tMax);
}

inline void pwmWriteHardware(uint8_t ch, uint16_t perceived) {
  if (ch >= NUM_PWM) return;
  const uint16_t trimmed = pwmApplyTrim(ch, perceived);
  const uint16_t out = outQuality.gammaEnable ? g_gammaLut[trimmed] : trimmed;
  analogWrite(PWM_PINS[ch], out);
}

inline void pwmSetTargetHi(uint8_t ch, uint16_t perceivedHi) {
  if (ch >= NUM_PWM) return;
  perceivedHi = (uint16_t)constrain((int)perceivedHi, 0, (int)PWM_HI);
  if (perceivedHi > 0) {
    perceivedHi = pwmApplyTrim(ch, perceivedHi);
    pwmLastNonZero[ch] = perceivedHi;
  }
  pwmTarget[ch] = perceivedHi;
  lastOutChangeMs = millis();
}

inline void pwmSetTargetApi(uint8_t ch, uint16_t api) {
  pwmSetTargetHi(ch, pwmApiToHi(api));
}

inline void pwmSnapCurrentToTarget(uint8_t ch) {
  pwmCurrent[ch] = pwmTarget[ch];
  pwmWriteHardware(ch, pwmCurrent[ch]);
  slewLastMs[ch] = millis();
}

inline void pwmServiceSlew(uint32_t now) {
  for (uint8_t ch = 0; ch < NUM_PWM; ch++) {
    if (pwmCurrent[ch] == pwmTarget[ch]) continue;
    const uint16_t fadeMs = pwmChCfg[ch].fadeMs;
    if (fadeMs == 0) {
      pwmCurrent[ch] = pwmTarget[ch];
      pwmWriteHardware(ch, pwmCurrent[ch]);
      slewLastMs[ch] = now;
      continue;
    }
    const int32_t err = (int32_t)pwmTarget[ch] - (int32_t)pwmCurrent[ch];
    uint32_t elapsed = now - slewLastMs[ch];
    if (elapsed == 0) continue;
    slewLastMs[ch] = now;
    int32_t maxStep = (int32_t)((int64_t)PWM_HI * (int64_t)elapsed / (int64_t)fadeMs);
    if (maxStep < 1) maxStep = 1;
    const int32_t absErr = err > 0 ? err : -err;
    if (absErr <= maxStep) {
      pwmCurrent[ch] = pwmTarget[ch];
    } else {
      pwmCurrent[ch] = (uint16_t)((int32_t)pwmCurrent[ch] + (err > 0 ? maxStep : -maxStep));
    }
    pwmWriteHardware(ch, pwmCurrent[ch]);
  }
}

inline uint16_t pwmDimStepHi(uint8_t dimStepPct) {
  return (uint16_t)((uint32_t)PWM_HI * (uint32_t)dimStepPct / 100u);
  // dimStepPct is percent of full scale per hold tick (default 8 = 8%)
}
