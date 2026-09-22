#pragma once
// STR-3221-R1 v0.2.0 — underfloor heating zone loop (step 2g).
// Slow PWM writes the SOURCE (0 or 100) for profile=heat channels; the 2e
// pipeline then runs. NC/NO (ChCfg.flags bit0) is applied LAST, in the TLC
// write path, so it covers PWM, exercise, frost and DI-close alike.
//
// HeatCfg.minPulsePct 0..50, 0 reads as 10. Counters live in a separate
// LittleFS file so PersistConfigV5 / CFG_VERSION do not move.

enum : uint8_t {
  HEAT_DI_OFF = 0, HEAT_DI_ENABLE = 1, HEAT_DI_INHIBIT = 2, HEAT_DI_ECHO = 3
};

static const char* HEAT_STATS_PATH = "/heat_stats.bin";
static const uint32_t HEAT_STATS_MAGIC = 0x48545331UL; // '1STH'
static const uint32_t HEAT_STATS_PERIOD_MS = 3600000UL; // 1 h
static const uint32_t HEAT_FROST_ON_MS  = 15UL * 60UL * 1000UL;
static const uint32_t HEAT_FROST_OFF_MS = 45UL * 60UL * 1000UL;

struct HeatStats {
  uint32_t magic;
  uint16_t hours[NUM_PWM];
  uint16_t cycles[NUM_PWM];
  uint16_t closedH[NUM_PWM];
  uint32_t crc32;
} __attribute__((packed));

static HeatStats g_heatStats{};
static bool      g_heatStatsDirty = false;
static uint32_t  g_heatStatsLastMs = 0;
static uint32_t  g_heatOnAccumMs[NUM_PWM];
static uint32_t  g_heatClosedAccumMs[NUM_PWM];
static bool      g_heatWasOpen[NUM_PWM];
static uint8_t   g_heatDuty[NUM_PWM];          // last HR 0..100
static uint8_t   g_heatSource[NUM_PWM];        // 0 or 100, before NC/NO
static int32_t   g_heatDebtMs[NUM_PWM];
static bool      g_heatExercise[NUM_PWM];
static uint32_t  g_heatExerciseEndMs[NUM_PWM];
static bool      g_heatFrost = false;
static bool      g_heatDemandRaw = false;      // at least one zone wants open
static bool      g_heatDemandOut = false;      // after first-open / overrun
static uint32_t  g_heatFirstOpenAtMs = 0;
static uint32_t  g_heatLastCloseMs = 0;
static bool      g_heatHystOpen[NUM_PWM];
static uint8_t   g_heatOpenCount = 0;
static uint32_t  g_heatOpenMask = 0;
static uint32_t  g_heatPwmEpochMs = 0;

static inline uint8_t heatMinPulsePct() {
  uint8_t p = g_heatCfg.minPulsePct;
  if (p == 0) p = 10;
  if (p > 50) p = 50;
  return p;
}

static inline bool heatIsChannel(uint8_t ch) {
  return ch < NUM_PWM && chCfg[ch].profile == CH_PROF_HEAT;
}

static inline bool heatDiClosed() {
  if (g_heatCfg.diRole == HEAT_DI_OFF) return false;
  return diReportedLive(0);
}

static inline bool heatDiForcesClosed() {
  const uint8_t role = g_heatCfg.diRole;
  if (role == HEAT_DI_ENABLE) return !heatDiClosed();
  if (role == HEAT_DI_INHIBIT) return heatDiClosed();
  return false;
}

static inline bool heatSummer() {
  return g_heatCfg.summerMode != 0;
}

static uint32_t heatPeriodMs() {
  uint16_t s = g_heatCfg.slowPwmPeriodS;
  if (s == 0) return 0;
  if (s < 300) s = 300;          // 5 min
  if (s > 3600) s = 3600;        // 60 min
  return (uint32_t)s * 1000UL;
}

static uint8_t heatZoneCount() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < NUM_PWM; i++) if (heatIsChannel(i)) n++;
  return n;
}

static uint8_t heatZoneIndex(uint8_t ch) {
  uint8_t k = 0;
  for (uint8_t i = 0; i < ch; i++) if (heatIsChannel(i)) k++;
  return k;
}

static void heatLoadStats() {
  memset(&g_heatStats, 0, sizeof(g_heatStats));
  File f = LittleFS.open(HEAT_STATS_PATH, "r");
  if (!f) return;
  HeatStats tmp{};
  const size_t n = f.read((uint8_t*)&tmp, sizeof(tmp));
  f.close();
  if (n != sizeof(tmp) || tmp.magic != HEAT_STATS_MAGIC) return;
  HeatStats v = tmp;
  const uint32_t crc = v.crc32;
  v.crc32 = 0;
  if (crc32_update(0, (const uint8_t*)&v, sizeof(v)) != crc) return;
  g_heatStats = tmp;
}

static bool heatSaveStats() {
  g_heatStats.magic = HEAT_STATS_MAGIC;
  g_heatStats.crc32 = 0;
  g_heatStats.crc32 = crc32_update(0, (const uint8_t*)&g_heatStats, sizeof(g_heatStats));
  File f = LittleFS.open(HEAT_STATS_PATH, "w");
  if (!f) return false;
  const size_t n = f.write((const uint8_t*)&g_heatStats, sizeof(g_heatStats));
  f.flush();
  f.close();
  return n == sizeof(g_heatStats);
}

static void heatNoteStats(uint32_t now) {
  if (!g_heatStatsDirty) return;
  if ((uint32_t)(now - g_heatStatsLastMs) < HEAT_STATS_PERIOD_MS && g_heatStatsLastMs != 0) return;
  if (heatSaveStats()) {
    g_heatStatsDirty = false;
    g_heatStatsLastMs = now;
  }
}

static void heatBumpHours(uint8_t ch, uint32_t dtMs) {
  g_heatOnAccumMs[ch] += dtMs;
  while (g_heatOnAccumMs[ch] >= 3600000UL) {
    g_heatOnAccumMs[ch] -= 3600000UL;
    if (g_heatStats.hours[ch] < 65535) g_heatStats.hours[ch]++;
    g_heatStatsDirty = true;
  }
}

static void heatBumpClosed(uint8_t ch, uint32_t dtMs) {
  g_heatClosedAccumMs[ch] += dtMs;
  while (g_heatClosedAccumMs[ch] >= 3600000UL) {
    g_heatClosedAccumMs[ch] -= 3600000UL;
    if (g_heatStats.closedH[ch] < 65535) g_heatStats.closedH[ch]++;
    g_heatStatsDirty = true;
  }
}

static void heatMarkCycle(uint8_t ch, bool open) {
  if (open && !g_heatWasOpen[ch]) {
    if (g_heatStats.cycles[ch] < 65535) g_heatStats.cycles[ch]++;
    g_heatStatsDirty = true;
    g_heatStats.closedH[ch] = 0;
    g_heatClosedAccumMs[ch] = 0;
  }
  g_heatWasOpen[ch] = open;
}

static void heatQueueExerciseAll() {
  for (uint8_t i = 0; i < NUM_PWM; i++) {
    if (!heatIsChannel(i)) continue;
    g_heatExercise[i] = true;
    g_heatExerciseEndMs[i] = 0;         // start when a slot is free
  }
  wsLog("Valve exercise queued for all heat channels");
}

static void heatApplySource(uint8_t ch, uint8_t src100) {
  g_heatSource[ch] = src100;
  // SOURCE only. Auto-off is suppressed for heat inside setChannelRequestOne.
  setChannelRequestOne(ch, src100);
}

// One owner of a heat channel's source. Priorities (also in the .ino header):
//   1 DI enable-open  2 panic  3 summer (exercise still runs)
//   5 frost  8 slow PWM
static void heatService(uint32_t now) {
  static uint32_t lastMs = 0;
  if (lastMs == 0) lastMs = now;
  uint32_t dt = now - lastMs;
  if (dt > 5000) dt = 5000;             // clamp after a long stall
  lastMs = now;
  if (g_heatPwmEpochMs == 0) g_heatPwmEpochMs = now;

  const bool diClose = heatDiForcesClosed();
  const bool summer = heatSummer();
  const uint32_t period = heatPeriodMs();
  const uint8_t minPct = heatMinPulsePct();
  const uint8_t maxOpen = g_heatCfg.maxOpenZones;
  const uint8_t nZones = heatZoneCount();
  const bool spread = g_heatCfg.phaseSpreadPct > 0 && nZones > 1;

  // Frost: T hours of silence on THIS slave, then 15/45 cycle. Heat replaces
  // the per-channel failsafe of step 2b. First addressed frame clears it.
  if (g_heatCfg.antifreezeHours && !summer && !diClose) {
    const uint32_t silentMs = now - g_lastLinkSeenMs;
    const uint32_t needMs = (uint32_t)g_heatCfg.antifreezeHours * 3600000UL;
    const bool silent = silentMs >= needMs;
    if (silent && !g_heatFrost) {
      g_heatFrost = true;
      wsLog("Frost protection active — bus silent");
    } else if (!silent && g_heatFrost) {
      g_heatFrost = false;
      wsLog("Frost protection cleared — bus alive");
    }
  } else {
    g_heatFrost = false;
  }

  // Exercise: N hours fully closed (HeatCfg.exerciseIntervalH; 14 d = 336 h).
  if (g_heatCfg.exerciseIntervalH) {
    for (uint8_t i = 0; i < NUM_PWM; i++) {
      if (!heatIsChannel(i) || g_heatExercise[i]) continue;
      if (g_heatStats.closedH[i] >= g_heatCfg.exerciseIntervalH) {
        g_heatExercise[i] = true;
        g_heatExerciseEndMs[i] = 0;
      }
    }
  }

  bool want[NUM_PWM];
  memset(want, 0, sizeof(want));

  for (uint8_t ch = 0; ch < NUM_PWM; ch++) {
    if (!heatIsChannel(ch)) continue;

    if (diClose || g_panicActive) {
      want[ch] = false;
      continue;
    }
    if (g_heatExercise[ch]) {
      want[ch] = true;                  // summer still exercises
      continue;
    }
    if (summer) {
      want[ch] = false;
      continue;
    }
    if (g_heatFrost) {
      const uint32_t cycle = HEAT_FROST_ON_MS + HEAT_FROST_OFF_MS;
      uint32_t phase = (now + (spread ? (uint32_t)heatZoneIndex(ch) * (cycle / nZones) : 0)) % cycle;
      want[ch] = (phase < HEAT_FROST_ON_MS);
      continue;
    }
    if (period == 0) {
      want[ch] = false;
      continue;
    }

    uint8_t duty = g_heatDuty[ch];
    if (duty > 100) duty = 100;
    // Hysteresis around min pulse: below min → 0, above (100-min) → 100.
    if (g_heatHystOpen[ch]) {
      if (duty < minPct) { duty = 0; g_heatHystOpen[ch] = false; }
      else if (duty > (uint8_t)(100 - minPct)) duty = 100;
    } else {
      if (duty < minPct) duty = 0;
      else {
        g_heatHystOpen[ch] = true;
        if (duty > (uint8_t)(100 - minPct)) duty = 100;
      }
    }
    if (duty == 0) { want[ch] = false; continue; }
    if (duty == 100) { want[ch] = true; continue; }

    uint32_t phase = (now - g_heatPwmEpochMs);
    if (spread) phase += (uint32_t)heatZoneIndex(ch) * (period / nZones);
    phase %= period;
    uint32_t onMs = (uint32_t)duty * period / 100u;
    if (g_heatDebtMs[ch] > 0) {
      const uint32_t extra = (uint32_t)g_heatDebtMs[ch];
      onMs = (onMs + extra > period) ? period : onMs + extra;
    }
    want[ch] = (phase < onMs);
  }

  // Simultaneous-open cap: defer, don't cancel. Missed on-time becomes debt.
  if (maxOpen) {
    uint8_t granted = 0;
    for (uint8_t ch = 0; ch < NUM_PWM; ch++) {
      if (!heatIsChannel(ch) || !want[ch]) continue;
      if (g_heatWasOpen[ch]) {
        if (granted < maxOpen) granted++;
        else {
          want[ch] = false;
          g_heatDebtMs[ch] += (int32_t)dt;
        }
      }
    }
    for (uint8_t ch = 0; ch < NUM_PWM; ch++) {
      if (!heatIsChannel(ch) || !want[ch] || g_heatWasOpen[ch]) continue;
      if (granted < maxOpen) granted++;
      else {
        want[ch] = false;
        g_heatDebtMs[ch] += (int32_t)dt;
      }
    }
  }

  // Exercise duration: start when the slot is actually granted.
  const uint32_t exMs = g_heatCfg.exerciseDurationS
    ? (uint32_t)g_heatCfg.exerciseDurationS * 1000UL : 300000UL;
  for (uint8_t ch = 0; ch < NUM_PWM; ch++) {
    if (!g_heatExercise[ch]) continue;
    if (want[ch] && g_heatExerciseEndMs[ch] == 0) g_heatExerciseEndMs[ch] = now + exMs;
    if (g_heatExerciseEndMs[ch] && (int32_t)(now - g_heatExerciseEndMs[ch]) >= 0) {
      g_heatExercise[ch] = false;
      g_heatExerciseEndMs[ch] = 0;
      want[ch] = false;
    }
  }

  bool anyWant = false;
  bool anyEx = false;
  uint8_t openN = 0;
  uint32_t mask = 0;
  for (uint8_t ch = 0; ch < NUM_PWM; ch++) {
    if (!heatIsChannel(ch)) continue;
    const bool open = want[ch];
    if (open) {
      anyWant = true;
      openN++;
      mask |= (1UL << ch);
      heatBumpHours(ch, dt);
      if (g_heatDebtMs[ch] > 0) g_heatDebtMs[ch] -= (int32_t)dt;
      if (g_heatDebtMs[ch] < 0) g_heatDebtMs[ch] = 0;
    } else {
      heatBumpClosed(ch, dt);
    }
    if (g_heatExercise[ch]) anyEx = true;
    heatMarkCycle(ch, open);
    heatApplySource(ch, open ? 100 : 0);
  }

  g_heatOpenCount = openN;
  g_heatOpenMask = mask;
  g_heatDemandRaw = anyWant;

  if (g_heatCfg.diRole == HEAT_DI_ECHO) {
    g_heatDemandOut = heatDiClosed();
  } else if (diClose) {
    g_heatDemandOut = false;
    g_heatFirstOpenAtMs = 0;
  } else {
    if (anyWant) {
      if (!g_heatDemandOut) {
        if (g_heatFirstOpenAtMs == 0) g_heatFirstOpenAtMs = now;
        const uint32_t delayMs = (uint32_t)g_heatCfg.firstOpenDelayS * 1000UL;
        if ((uint32_t)(now - g_heatFirstOpenAtMs) >= delayMs) g_heatDemandOut = true;
      }
      g_heatLastCloseMs = 0;
    } else {
      g_heatFirstOpenAtMs = 0;
      if (g_heatDemandOut) {
        if (g_heatLastCloseMs == 0) g_heatLastCloseMs = now;
        const uint32_t overMs = (uint32_t)g_heatCfg.overrunS * 1000UL;
        if ((uint32_t)(now - g_heatLastCloseMs) >= overMs) g_heatDemandOut = false;
      }
    }
    if (summer && !anyEx) g_heatDemandOut = false;
  }

  heatNoteStats(now);
}

static inline uint16_t heatIregFlags() {
  uint16_t f = 0;
  if (g_heatDemandOut) f |= (1u << 0);
  if (heatSummer())    f |= (1u << 1);
  if (heatDiClosed())  f |= (1u << 2);
  if (g_heatFrost)     f |= (1u << 3);
  bool ex = false;
  for (uint8_t i = 0; i < NUM_PWM; i++) if (g_heatExercise[i]) { ex = true; break; }
  if (ex) f |= (1u << 4);
  return f;
}
