#pragma once
// STR-3221-R1 v0.2.0 — on-board stair sequencer (step 2f).
// Sets the SOURCE value for channels in the step table; the 2e pipeline
// (profile → min/max → curve → master → ramp → block I2C) does the rest.
// Headers never write the driver.
//
// StairCfg.mode    — launch: 0 one-shot · 1 hold while presence
// StairCfg.pattern — drawing: 0 sequential · 1 all · 2 wave · 3 comet
//                    · 4 center · 5 random
// IN1 = DI2 (index 1), IN2 = DI3 (index 2). inputEndMap bit0 swaps which
// end is bottom. bothWindowMs / bothEnds govern a dual-edge window.

enum : uint8_t { STAIR_MODE_ONESHOT = 0, STAIR_MODE_HOLD = 1, STAIR_MODE_MAX = 1 };
enum : uint8_t {
  STAIR_PAT_SEQ = 0, STAIR_PAT_ALL = 1, STAIR_PAT_WAVE = 2,
  STAIR_PAT_COMET = 3, STAIR_PAT_CENTER = 4, STAIR_PAT_RANDOM = 5, STAIR_PAT_MAX = 5
};
enum : uint8_t {
  SEQ_IDLE = 0, SEQ_UP = 1, SEQ_DOWN = 2, SEQ_HOLD = 3, SEQ_FADE = 4
};
enum : uint8_t { SEQ_DIR_NONE = 0, SEQ_DIR_UP = 1, SEQ_DIR_DOWN = 2 };

static bool     g_seqInhibit = false;
static bool     g_seqNightWindow = false;
static uint8_t  g_seqPhase = SEQ_IDLE;
static uint8_t  g_seqDir = SEQ_DIR_NONE;
static uint8_t  g_seqStep = 0;          // 1..n, 0 = none
static uint8_t  g_seqOrder[NUM_PWM];
static uint8_t  g_seqN = 0;
static uint32_t g_seqStartMs = 0;
static uint32_t g_seqHoldEndMs = 0;
static uint32_t g_seqEndedMs = 0;
static uint32_t g_seqDbAt[2] = {0, 0};
static bool     g_seqDb[2] = {false, false};
static uint32_t g_seqFirstEdgeMs = 0;
static uint8_t  g_seqFirstDir = SEQ_DIR_NONE;
static uint32_t g_stairChMask = 0;
static bool     g_seqForceAll = false;

static inline bool stairEnabled() {
  return g_stairCfg.stepCount >= 2 && g_stairCfg.stepCount <= NUM_PWM;
}

static inline uint8_t stairCh(uint8_t step) {
  return (step < NUM_PWM) ? g_stairCfg.stepChannel[step] : 0;
}

static inline uint8_t stairPattern() {
  const uint8_t p = g_seqForceAll ? STAIR_PAT_ALL : g_stairCfg.pattern;
  return (p > STAIR_PAT_MAX) ? STAIR_PAT_SEQ : p;
}

static inline uint8_t stairLaunchMode() {
  return (g_stairCfg.mode > STAIR_MODE_MAX) ? STAIR_MODE_ONESHOT : g_stairCfg.mode;
}

static inline uint16_t stairBothWindowMs() {
  return g_stairCfg.bothWindowMs ? g_stairCfg.bothWindowMs : 300;
}

static inline uint8_t stairWaveWidth(uint8_t n) {
  uint8_t w = g_stairCfg.waveWidth ? g_stairCfg.waveWidth : 3;
  if (w < 1) w = 1;
  if (w > n) w = n;
  return w;
}

static void stairRebuildMask() {
  g_stairChMask = 0;
  if (!stairEnabled()) return;
  for (uint8_t s = 0; s < g_stairCfg.stepCount; s++) {
    const uint8_t ch = stairCh(s);
    if (ch < NUM_PWM) g_stairChMask |= (1UL << ch);
  }
}

static inline bool stairOwnsChannel(uint8_t ch) {
  return ch < NUM_PWM && (g_stairChMask & (1UL << ch));
}

static inline bool stairRunning() {
  return g_seqPhase != SEQ_IDLE;
}

static inline uint16_t stairRampMs(uint8_t ch) {
  if (chCfg[ch].rampMs) return chCfg[ch].rampMs;
  if (stairRunning() && stairOwnsChannel(ch) && g_stairCfg.stepFadeMs) return g_stairCfg.stepFadeMs;
  return chCfg[ch].rampMs;
}

static inline uint8_t stairRunLevel() {
  const uint8_t v = g_seqNightWindow ? g_stairCfg.nightLevel : g_stairCfg.dayLevel;
  return v ? v : 255;
}

static uint8_t stairAccent(uint8_t step) {
  if (g_stairCfg.nightLightLevel) return g_stairCfg.nightLightLevel;
  if (step == 0) return g_stairCfg.standbyFirst;
  if (stairEnabled() && step == (uint8_t)(g_stairCfg.stepCount - 1)) return g_stairCfg.standbyLast;
  return 0;
}

// Sequencer writes the source only — never the driver, never HR.
static void stairSetSource(uint8_t ch, uint8_t val) {
  if (ch >= NUM_PWM) return;
  setChannelRequestOne(ch, val);
}

static void stairApplyIdleBase() {
  if (!stairEnabled()) return;
  for (uint8_t s = 0; s < g_stairCfg.stepCount; s++) {
    stairSetSource(stairCh(s), stairAccent(s));
  }
}

static void stairBuildOrder(uint8_t dir) {
  const uint8_t n = g_stairCfg.stepCount;
  g_seqN = n;
  const uint8_t pat = stairPattern();

  if (pat == STAIR_PAT_CENTER) {
    const uint8_t mid = n / 2;
    uint8_t w = 0;
    g_seqOrder[w++] = mid;
    for (uint8_t d = 1; w < n; d++) {
      if ((int)mid - (int)d >= 0) g_seqOrder[w++] = (uint8_t)(mid - d);
      if (w < n && (uint8_t)(mid + d) < n) g_seqOrder[w++] = (uint8_t)(mid + d);
    }
    return;
  }
  if (pat == STAIR_PAT_RANDOM) {
    for (uint8_t i = 0; i < n; i++) g_seqOrder[i] = i;
    uint32_t rng = millis() ^ (n * 0x9E3779B9UL);
    for (uint8_t i = n; i > 1; i--) {
      rng = rng * 1664525UL + 1013904223UL;
      const uint8_t j = (uint8_t)(rng % i);
      const uint8_t t = g_seqOrder[i - 1];
      g_seqOrder[i - 1] = g_seqOrder[j];
      g_seqOrder[j] = t;
    }
    return;
  }
  if (dir == SEQ_DIR_DOWN) {
    for (uint8_t i = 0; i < n; i++) g_seqOrder[i] = (uint8_t)(n - 1 - i);
  } else {
    for (uint8_t i = 0; i < n; i++) g_seqOrder[i] = i;
  }
}

static void stairAbortToBase() {
  g_seqPhase = SEQ_IDLE;
  g_seqDir = SEQ_DIR_NONE;
  g_seqStep = 0;
  g_seqForceAll = false;
  g_seqEndedMs = millis();
  stairApplyIdleBase();
  wsLog("Stair sequence aborted");
}

static void stairStart(uint8_t dir) {
  if (!stairEnabled() || g_panicActive) return;
  if (g_seqInhibit && !stairRunning()) return;
  stairRebuildMask();
  stairBuildOrder(dir);
  g_seqDir = dir;
  g_seqPhase = (dir == SEQ_DIR_DOWN) ? SEQ_DOWN : SEQ_UP;
  g_seqStartMs = millis();
  g_seqHoldEndMs = 0;
  g_seqStep = 1;
  wsLog(String("Stair sequence ") + ((dir == SEQ_DIR_DOWN) ? "DOWN" : "UP"));
}

static void stairOnTrigger(uint8_t dir) {
  if (g_panicActive) return;
  if (g_localOverride) return;          // 2f §3.2 — override deafens PIR
  if (!stairEnabled()) return;

  const uint32_t now = millis();
  if (!stairRunning()) {
    if (g_seqInhibit) return;
    if (g_stairCfg.minRepeatMs && g_seqEndedMs &&
        (uint32_t)(now - g_seqEndedMs) < g_stairCfg.minRepeatMs) return;
    stairStart(dir);
    return;
  }
  const bool same = (dir == g_seqDir);
  if (same) {
    if (g_stairCfg.retrigger == 0) return;
    if (g_stairCfg.retrigger == 2 && g_seqPhase == SEQ_HOLD) {
      g_seqHoldEndMs = now + (uint32_t)g_stairCfg.holdS * 1000UL;
      return;
    }
    stairStart(dir);                    // restart
    return;
  }
  if (g_stairCfg.oppose == 0) return;
  stairStart(dir);                      // reverse / opposing front
}

static void stairForce(uint8_t dir) {
  if (g_panicActive) return;
  if (dir == SEQ_DIR_NONE) { stairAbortToBase(); return; }
  stairStart(dir);
}

static uint16_t stairDelay() {
  return g_stairCfg.stepDelayMs ? g_stairCfg.stepDelayMs : 1;
}

static inline bool stairAnyPresence() {
  return g_seqDb[0] || g_seqDb[1];
}

static void stairRender(uint32_t now) {
  const uint8_t n = g_seqN;
  if (!n) { stairAbortToBase(); return; }
  const uint8_t pat = stairPattern();
  const uint16_t delay = stairDelay();
  const uint32_t tLight = (pat == STAIR_PAT_ALL) ? 0 : (uint32_t)(n - 1) * delay;
  const uint32_t tHold  = (uint32_t)g_stairCfg.holdS * 1000UL;
  uint32_t tFade = g_stairCfg.fadeOutMs;
  if (tFade == 0) tFade = tLight ? tLight : delay;

  uint32_t fadeStart;
  if (g_stairCfg.overlapPct == 0) fadeStart = tLight + tHold;
  else fadeStart = (tLight * (uint32_t)(100 - g_stairCfg.overlapPct)) / 100u;

  const uint32_t elapsed = now - g_seqStartMs;
  if (stairLaunchMode() == STAIR_MODE_HOLD && stairAnyPresence()) {
    fadeStart = elapsed + 1000UL;       // stay lit while presence is held
    g_seqHoldEndMs = now + tHold;
  }

  const uint8_t run = stairRunLevel();
  uint8_t width = 1;
  if (pat == STAIR_PAT_WAVE || pat == STAIR_PAT_COMET) {
    width = stairWaveWidth(n);
    if (width < 2 && n >= 2) width = 2;
  }

  uint8_t lastOn = 0;
  bool anyAboveAccent = false;
  bool lightingDone = true;
  bool fadeDone = true;

  for (uint8_t p = 0; p < n; p++) {
    const uint8_t step = g_seqOrder[p];
    const uint8_t ch = stairCh(step);
    const uint8_t accent = stairAccent(step);
    uint32_t lightAt = (pat == STAIR_PAT_ALL) ? 0 : (uint32_t)p * delay;
    uint32_t fadeAt  = fadeStart + ((pat == STAIR_PAT_ALL || n < 2) ? 0 : ((uint32_t)p * tFade) / (n - 1));
    if (fadeAt <= lightAt) fadeAt = lightAt + 1;

    uint8_t lvl = accent;
    if (elapsed >= lightAt) {
      lightingDone = lightingDone && (elapsed >= tLight);
      if (elapsed < fadeAt) {
        fadeDone = false;
        if (pat == STAIR_PAT_WAVE || pat == STAIR_PAT_COMET) {
          const int front = tLight ? (int)((elapsed * (n - 1)) / tLight) : (int)(n - 1);
          const int dist = front - (int)p;
          if (dist >= 0 && dist < (int)width) {
            if (pat == STAIR_PAT_COMET) {
              lvl = (uint8_t)((uint32_t)run * (uint32_t)(width - dist) / width);
            } else {
              lvl = run;
            }
          }
        } else {
          lvl = run;
          lastOn = (uint8_t)(p + 1);
        }
      } else {
        lvl = accent;
      }
    } else {
      lightingDone = false;
      fadeDone = false;
    }
    if (lvl > accent) anyAboveAccent = true;
    stairSetSource(ch, lvl);
  }

  if (pat == STAIR_PAT_WAVE || pat == STAIR_PAT_COMET) {
    const int front = tLight ? (int)((elapsed * (n - 1)) / tLight) : (int)(n - 1);
    g_seqStep = (uint8_t)constrain(front + 1, 1, (int)n);
  } else {
    g_seqStep = lastOn ? lastOn : (lightingDone ? n : 1);
  }

  if (!lightingDone) {
    g_seqPhase = (g_seqDir == SEQ_DIR_DOWN) ? SEQ_DOWN : SEQ_UP;
  } else if (elapsed < fadeStart) {
    g_seqPhase = SEQ_HOLD;
    if (!g_seqHoldEndMs) g_seqHoldEndMs = now + tHold;
    if (g_stairCfg.overlapPct == 0 && now < g_seqHoldEndMs) return;
    if (g_stairCfg.overlapPct == 0) fadeStart = 0;
  } else if (!fadeDone || anyAboveAccent) {
    g_seqPhase = SEQ_FADE;
  } else {
    g_seqPhase = SEQ_IDLE;
    g_seqDir = SEQ_DIR_NONE;
    g_seqStep = 0;
    g_seqForceAll = false;
    g_seqEndedMs = now;
    wsLog("Stair sequence finished");
  }
}

// Second edge inside bothWindowMs. The first edge already started the run.
static void stairApplyBothEnds(uint8_t firstDir) {
  if (g_stairCfg.bothEnds == 2) return;          // ignore
  if (g_stairCfg.bothEnds == 1) {
    g_seqForceAll = true;
    if (stairRunning()) stairBuildOrder(g_seqDir ? g_seqDir : firstDir);
    else stairOnTrigger(firstDir);
    return;
  }
  // bothEnds == 0: keep the direction of the first edge — already running
  (void)firstDir;
}

static void stairServicePresence(uint32_t now) {
  if (!stairEnabled()) return;
  const uint16_t dbMs = g_stairCfg.debounceMs ? g_stairCfg.debounceMs : 50;
  const uint8_t idxBottom = (g_stairCfg.inputEndMap & 1) ? 2 : 1;
  const uint8_t idxTop    = (g_stairCfg.inputEndMap & 1) ? 1 : 2;
  const bool raw[2] = { diState[idxBottom], diState[idxTop] };
  const uint16_t win = stairBothWindowMs();
  for (uint8_t i = 0; i < 2; i++) {
    if (raw[i] != g_seqDb[i]) {
      if (g_seqDbAt[i] == 0) g_seqDbAt[i] = now;
      if ((uint32_t)(now - g_seqDbAt[i]) < dbMs) continue;
      g_seqDb[i] = raw[i];
      g_seqDbAt[i] = 0;
      if (!raw[i]) continue;            // rising edge only
      const uint8_t dir = (i == 0) ? SEQ_DIR_UP : SEQ_DIR_DOWN;
      if (g_seqFirstDir == SEQ_DIR_NONE) {
        g_seqFirstDir = dir;
        g_seqFirstEdgeMs = now;
        stairOnTrigger(dir);            // start on the first edge
      } else if ((uint32_t)(now - g_seqFirstEdgeMs) <= win) {
        stairApplyBothEnds(g_seqFirstDir);
        g_seqFirstDir = SEQ_DIR_NONE;
      } else {
        g_seqFirstDir = dir;
        g_seqFirstEdgeMs = now;
        stairOnTrigger(dir);
      }
    } else {
      g_seqDbAt[i] = 0;
    }
  }
  if (g_seqFirstDir != SEQ_DIR_NONE &&
      (uint32_t)(now - g_seqFirstEdgeMs) > win) {
    g_seqFirstDir = SEQ_DIR_NONE;       // window expired — do not start again
  }
}

static void stairService(uint32_t now) {
  stairRebuildMask();
  if (g_panicActive) {
    if (stairRunning()) {
      g_seqPhase = SEQ_IDLE;
      g_seqDir = SEQ_DIR_NONE;
      g_seqStep = 0;
      g_seqForceAll = false;
    }
    return;
  }
  stairServicePresence(now);
  if (stairRunning()) stairRender(now);
}

static inline uint16_t stairIregState() {
  return (uint16_t)((g_seqDir << 8) | g_seqPhase);
}
