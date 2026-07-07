#pragma once
// RGB-621-R1 local input engine (per-module; not shared repo-wide).

#include <Arduino.h>

enum HmTgt : uint8_t {
  HM_TGT_NONE    = 0,
  HM_TGT_RLY1    = 1,
  HM_TGT_PWM_R   = 2,
  HM_TGT_PWM_G   = 3,
  HM_TGT_PWM_B   = 4,
  HM_TGT_PWM_WW  = 5,
  HM_TGT_PWM_CW  = 6,
  HM_TGT_GRP_RGB = 7,
  HM_TGT_GRP_CCT = 8,
  HM_TGT_ALL     = 9,
  HM_TGT_GRP_RGBCCT = 10,
};

enum HmAct : uint8_t {
  HM_ACT_NONE           = 0,
  HM_ACT_TOGGLE         = 1,
  HM_ACT_ON             = 2,
  HM_ACT_OFF            = 3,
  HM_ACT_ALLOFF         = 4,  // legacy; not UI-selectable
  HM_ACT_DIM_UP         = 5,
  HM_ACT_DIM_DOWN       = 6,
  HM_ACT_DIM_TOGGLE_DIR = 7,
  HM_ACT_SET_100        = 8,
  HM_ACT_RELAY_PULSE    = 9,
  HM_ACT_SCENE1         = 10,
  HM_ACT_IDENTIFY       = 11,
  HM_ACT_SCENE2         = 12,
  HM_ACT_SCENE3         = 13,
  HM_ACT_SCENE4         = 14,
};

#define HM_ACT_SCENE HM_ACT_SCENE1

static inline bool hmIsSceneAction(uint8_t act) {
  return act == HM_ACT_SCENE1 || act == HM_ACT_SCENE2 ||
         act == HM_ACT_SCENE3 || act == HM_ACT_SCENE4;
}

static inline uint8_t hmSceneIndex(uint8_t act) {
  if (act == HM_ACT_SCENE1) return 0;
  if (act == HM_ACT_SCENE2) return 1;
  if (act == HM_ACT_SCENE3) return 2;
  if (act == HM_ACT_SCENE4) return 3;
  return 0;
}

enum HmInMode : uint8_t {
  HM_IN_MOMENTARY  = 0,
  HM_IN_MAINTAINED = 1,
};

static inline uint8_t hmNormalizeInMode(uint8_t m) {
  return (m == HM_IN_MOMENTARY) ? HM_IN_MOMENTARY : HM_IN_MAINTAINED;
}

enum HmEvt : uint8_t {
  HM_EVT_SINGLE = 0,
  HM_EVT_DOUBLE = 1,
  HM_EVT_TRIPLE = 2,
  HM_EVT_LONG   = 3,
  HM_EVT_HOLD   = 4,
  HM_EVT_COUNT  = 5,
};

struct HmGestureBind {
  uint8_t action;
  uint8_t target;
};

static inline void hmNormalizeGestureBind(HmGestureBind& g) {
  if (g.action == HM_ACT_ALLOFF) {
    g.action = HM_ACT_OFF;
    g.target = HM_TGT_ALL;
  }
  if (g.action == HM_ACT_SET_100) {
    g.action = HM_ACT_NONE;
    g.target = HM_TGT_NONE;
  }
  if (g.action == HM_ACT_SCENE1 && g.target > 0 && g.target < 4) {
    static const uint8_t sceneActs[] = { HM_ACT_SCENE1, HM_ACT_SCENE2, HM_ACT_SCENE3, HM_ACT_SCENE4 };
    g.action = sceneActs[g.target];
    g.target = 0;
  }
  if (hmIsSceneAction(g.action)) g.target = 0;
  if (g.action == HM_ACT_RELAY_PULSE) g.target = HM_TGT_RLY1;
}

struct HmInputChannelCfg {
  bool enabled;
  bool inverted;
  bool lockLocal;
  uint8_t mode;
  HmGestureBind single;
  HmGestureBind dbl;
  HmGestureBind tpl;
  HmGestureBind lng;
  HmGestureBind hold;
};

struct HmInputEngineTimings {
  uint16_t debounceMs;
  uint16_t longPressMs;
  uint16_t multiClickGapMs;
  uint16_t holdDelayMs;
  uint16_t holdRepeatMs;
};

struct HmDebounceState {
  bool raw;
  bool stable;
  bool prevStable;
  uint32_t lastChangeMs;
};

struct HmClickState {
  bool pressed;
  uint32_t pressStartMs;
  bool longFired;
  bool holdFired;
  uint8_t pendingClicks;
  uint32_t lastReleaseMs;
  bool gapPending;
};

struct HmHoldRampState {
  bool active;
  bool dimDown;
  uint8_t target;
};

struct HmInputRuntime {
  HmDebounceState db;
  HmClickState cs;
  HmHoldRampState ramp;
};

void hmApplyGesture(uint8_t physIdx, HmEvt evt, uint8_t action, uint8_t target, uint32_t now);
void hmApplyMaintainedEdge(uint8_t physIdx, bool level, uint32_t now);
void hmHoldDimBegin(uint8_t physIdx, uint8_t target, bool dimDown, uint32_t now);
void hmHoldDimEnd(uint8_t physIdx, uint32_t now);
bool hmLocalInputAllowed(bool lockLocal, bool allowOffline);

static inline bool hmIsDimHoldAction(uint8_t act) {
  return act == HM_ACT_DIM_UP || act == HM_ACT_DIM_DOWN || act == HM_ACT_DIM_TOGGLE_DIR;
}

static inline void hmGestureClear(HmInputChannelCfg& c) {
  c.enabled = true;
  c.inverted = false;
  c.lockLocal = false;
  c.mode = HM_IN_MOMENTARY;
  c.single = { HM_ACT_NONE, HM_TGT_NONE };
  c.dbl = { HM_ACT_NONE, HM_TGT_NONE };
  c.tpl = { HM_ACT_NONE, HM_TGT_NONE };
  c.lng = { HM_ACT_NONE, HM_TGT_NONE };
  c.hold = { HM_ACT_NONE, HM_TGT_NONE };
}

static inline bool hmMultiClickConfigured(const HmInputChannelCfg& cfg) {
  return cfg.dbl.action != HM_ACT_NONE || cfg.tpl.action != HM_ACT_NONE;
}

static inline void hmInputEngineSetDiDefaults(HmInputChannelCfg* di, uint8_t n, bool singleButtonDim) {
  for (uint8_t i = 0; i < n; i++) {
    hmGestureClear(di[i]);
    di[i].inverted = false; // INPUT_PULLDOWN: idle LOW, switch close = HIGH
  }
  if (n >= 1) {
    di[0].single = { HM_ACT_TOGGLE, HM_TGT_GRP_RGB };
    di[0].dbl    = { HM_ACT_NONE, HM_TGT_NONE };
    di[0].hold   = singleButtonDim
      ? HmGestureBind{ HM_ACT_DIM_TOGGLE_DIR, HM_TGT_GRP_RGB }
      : HmGestureBind{ HM_ACT_DIM_UP, HM_TGT_GRP_RGB };
  }
  if (n >= 2) {
    di[1].single = { HM_ACT_TOGGLE, HM_TGT_GRP_CCT };
    di[1].dbl    = { HM_ACT_NONE, HM_TGT_NONE };
    di[1].hold   = singleButtonDim
      ? HmGestureBind{ HM_ACT_NONE, HM_TGT_NONE }
      : HmGestureBind{ HM_ACT_DIM_DOWN, HM_TGT_GRP_CCT };
  }
}

static inline void hmInputEngineSetBtnDefaults(HmInputChannelCfg& btn) {
  hmGestureClear(btn);
  btn.single = { HM_ACT_TOGGLE, HM_TGT_ALL };
  btn.lng    = { HM_ACT_IDENTIFY, HM_TGT_NONE };
}

static inline void hmInputEngineSetTimingDefaults(HmInputEngineTimings& t) {
  t.debounceMs = 25;
  t.longPressMs = 700;
  t.multiClickGapMs = 350;
  t.holdDelayMs = 650;
  t.holdRepeatMs = 60;
}

static inline void hmServiceDebounce(HmDebounceState& st, bool raw, uint32_t now, uint16_t debounceMs) {
  if (raw != st.raw) {
    st.raw = raw;
    st.lastChangeMs = now;
  }
  if (st.stable != st.raw && (uint32_t)(now - st.lastChangeMs) >= debounceMs) {
    st.prevStable = st.stable;
    st.stable = st.raw;
  }
}

static inline void hmIncEvt(uint16_t evtCount[][HM_EVT_COUNT], uint8_t src, uint8_t numSrc, HmEvt type) {
  if (src >= numSrc || type >= HM_EVT_COUNT) return;
  if (evtCount[src][type] < 0xFFFF) evtCount[src][type]++;
}

static inline void hmFireGesture(uint8_t physIdx, HmEvt evt, const HmGestureBind& bind, uint32_t now,
                                uint16_t evtCount[][HM_EVT_COUNT], uint8_t numPhys) {
  hmIncEvt(evtCount, physIdx, numPhys, evt);
  if (bind.action != HM_ACT_NONE) hmApplyGesture(physIdx, evt, bind.action, bind.target, now);
}

static inline void hmServiceMomentaryPhys(uint8_t physIdx, const HmInputChannelCfg& cfg, HmInputRuntime& rt,
                                          const HmInputEngineTimings& timings, bool allowOffline,
                                          bool dimToggleDir[], uint16_t evtCount[][HM_EVT_COUNT], uint8_t numPhys,
                                          uint32_t now) {
  if (!cfg.enabled) return;
  if (!hmLocalInputAllowed(cfg.lockLocal, allowOffline)) return;
  if (cfg.mode != HM_IN_MOMENTARY) return;

  HmDebounceState& db = rt.db;
  HmClickState& cs = rt.cs;
  HmHoldRampState& ramp = rt.ramp;
  bool rising = (!db.prevStable && db.stable);
  bool falling = (db.prevStable && !db.stable);

  if (rising) {
    cs.pressed = true;
    cs.pressStartMs = now;
    cs.longFired = false;
    cs.holdFired = false;
    ramp.active = false;
    if (cfg.hold.action == HM_ACT_DIM_TOGGLE_DIR) dimToggleDir[physIdx] = !dimToggleDir[physIdx];
  }

  if (cs.pressed && db.stable) {
    uint32_t held = now - cs.pressStartMs;
    if (!cs.longFired && !cs.holdFired && cfg.lng.action != HM_ACT_NONE &&
        !hmIsDimHoldAction(cfg.hold.action) && held >= timings.longPressMs) {
      hmFireGesture(physIdx, HM_EVT_LONG, cfg.lng, now, evtCount, numPhys);
      cs.longFired = true;
    }
    if (!ramp.active && hmIsDimHoldAction(cfg.hold.action) && held >= timings.holdDelayMs) {
      ramp.active = true;
      ramp.target = cfg.hold.target;
      ramp.dimDown = (cfg.hold.action == HM_ACT_DIM_DOWN) ||
        (cfg.hold.action == HM_ACT_DIM_TOGGLE_DIR && dimToggleDir[physIdx]);
      cs.holdFired = true;
      hmIncEvt(evtCount, physIdx, numPhys, HM_EVT_HOLD);
      hmHoldDimBegin(physIdx, ramp.target, ramp.dimDown, now);
    }
  }

  if (falling && cs.pressed) {
    cs.pressed = false;
    uint32_t held = now - cs.pressStartMs;
    if (ramp.active) hmHoldDimEnd(physIdx, now);
    ramp.active = false;
    if (!cs.longFired && !cs.holdFired && held < timings.holdDelayMs) {
      if (!hmMultiClickConfigured(cfg)) {
        hmFireGesture(physIdx, HM_EVT_SINGLE, cfg.single, now, evtCount, numPhys);
      } else {
        cs.pendingClicks++;
        cs.lastReleaseMs = now;
        cs.gapPending = true;
      }
    }
  }
}

static inline void hmFinalizeOneGap(HmInputRuntime& rt, const HmInputChannelCfg& cfg,
                                    uint8_t physIdx, const HmInputEngineTimings& timings,
                                    uint16_t evtCount[][HM_EVT_COUNT], uint8_t numPhys, uint32_t now) {
  HmClickState& cs = rt.cs;
  if (!cs.gapPending) return;
  if ((uint32_t)(now - cs.lastReleaseMs) < timings.multiClickGapMs) return;
  if (cs.pendingClicks >= 3) {
    if (cfg.tpl.action != HM_ACT_NONE) hmFireGesture(physIdx, HM_EVT_TRIPLE, cfg.tpl, now, evtCount, numPhys);
    else hmFireGesture(physIdx, HM_EVT_SINGLE, cfg.single, now, evtCount, numPhys);
  } else if (cs.pendingClicks == 2) {
    hmFireGesture(physIdx, HM_EVT_DOUBLE, cfg.dbl, now, evtCount, numPhys);
  } else if (cs.pendingClicks == 1) {
    hmFireGesture(physIdx, HM_EVT_SINGLE, cfg.single, now, evtCount, numPhys);
  }
  cs.pendingClicks = 0;
  cs.gapPending = false;
}

static inline const HmInputChannelCfg& hmPhysCfg(uint8_t physIdx,
    const HmInputChannelCfg* diCfg, uint8_t numDi,
    const HmInputChannelCfg* btnCfg) {
  if (physIdx < numDi) return diCfg[physIdx];
  return btnCfg[physIdx - numDi];
}

static inline void hmFinalizeClickGaps(HmInputRuntime* rt, uint8_t numPhys,
    const HmInputChannelCfg* diCfg, uint8_t numDi,
    const HmInputChannelCfg* btnCfg, uint8_t /*numBtn*/,
    const HmInputEngineTimings& timings,
    uint16_t evtCount[][HM_EVT_COUNT], uint32_t now) {
  for (uint8_t p = 0; p < numPhys; p++) {
    const HmInputChannelCfg& cfg = hmPhysCfg(p, diCfg, numDi, btnCfg);
    hmFinalizeOneGap(rt[p], cfg, p, timings, evtCount, numPhys, now);
  }
}

static inline void hmInputEngineSetDefaults(HmInputChannelCfg* di, uint8_t n, bool singleButtonDim) {
  hmInputEngineSetDiDefaults(di, n, singleButtonDim);
}

static inline void hmServiceMaintainedPhys(uint8_t physIdx, uint8_t chIdx, const HmInputChannelCfg& cfg,
                                           HmInputRuntime& rt, uint32_t now) {
  (void)chIdx;
  if (!cfg.enabled) return;
  if (!hmLocalInputAllowed(cfg.lockLocal, true)) return;
  if (cfg.mode != HM_IN_MAINTAINED) return;
  HmDebounceState& db = rt.db;
  if (db.stable == db.prevStable) return;
  hmApplyMaintainedEdge(physIdx, db.stable, now);
}
