#include <Arduino.h>
#include <ModbusSerial.h>
#include "hm_common.h"
#define HM_MODEL_ID   5
#define HM_FW_MAJOR   0
#define HM_FW_MINOR   2
#define HM_FW_PATCH   0
#define HM_FW         "0.2.0"
#define HM_MAP        1
#define HM_MAP_VERSION 1
#include <SimpleWebSerial.h>
#include <Arduino_JSON.h>
#include <LittleFS.h>
#include <utility>
#include "hardware/watchdog.h"

// Arduino IDE inserts function prototypes before struct definitions — forward-declare persist types.
struct PersistConfig;
struct PersistConfigV9;
struct PersistConfigV7;
struct OutputStateSnapshot;

// Legacy config layouts (must be declared before any function — Arduino auto-prototypes run early).
struct InCfgV9 { bool enabled; bool inverted; uint8_t action; uint8_t target; };
struct LedCfgV9 { uint8_t mode; uint8_t source; };
struct BtnCfgV9 { uint8_t action; };
struct RlyCfgV9 { bool enabled; bool inverted; uint8_t powerOn; };
struct RlyCfgV7 { bool enabled; bool inverted; };

// ================== UART2 (RS-485 / Modbus) ==================
#define TX2 4
#define RX2 5
const int TxenPin = -1;
int SlaveId = 1;
ModbusSerial mb(Serial2, SlaveId, TxenPin);

// ================== GPIO MAP (direct) ==================
static const uint8_t DI_PINS[4]    = {6, 11, 12, 7};
static const uint8_t RELAY_PINS[3] = {10, 9, 8};
static const uint8_t LED_PINS[3]   = {13, 14, 15};
static const uint8_t BTN_PINS[2]   = {2, 3};

// ================== Sizes ==================
static const uint8_t NUM_DI  = 4;
static const uint8_t NUM_RLY = 3;
static const uint8_t NUM_LED = 3;
static const uint8_t NUM_BTN = 2;
static const uint8_t NUM_EVT_SRC = 6; // DI0..3, Btn4..5

// ================== Enums & config structs ==================
enum InputType : uint8_t { IN_MAINTAINED = 0, IN_MOMENTARY = 1 };
enum MaintMode : uint8_t { MAINT_TOGGLE = 0, MAINT_FOLLOW = 1 };
enum InAction  : uint8_t { ACT_NONE = 0, ACT_TOGGLE = 1, ACT_ON = 2, ACT_OFF = 3, ACT_ALLOFF = 4 };
// target: 0=All, 1..3=R1..R3, 4=None
enum LedSource : uint8_t { LED_OFF = 0, LED_HA = 1, LED_LINK = 2, LED_LOCAL = 3, LED_CHILDLOCK = 4, LED_SAFEMODE = 5, LED_IDENTIFY = 6, LED_RELAY = 7 };
enum EvtType   : uint8_t { EVT_SINGLE = 0, EVT_DOUBLE = 1, EVT_TRIPLE = 2, EVT_LONG = 3 };

struct InCfg {
  bool enabled, inverted;
  uint8_t type;
  uint8_t followTarget;
  uint8_t shortAction, shortTarget;
  uint8_t longAction, longTarget;
  bool lockLocal;
  uint8_t maintMode;
};
struct RlyCfg { bool enabled, inverted; uint8_t powerOn, safeState; uint16_t autoOffSec; };
struct LedCfg { uint8_t source, mode; bool inverted; uint8_t arg; };
struct BtnCfg { uint8_t shortAction, shortTarget, longAction, longTarget; };
struct InterlockCfg { bool enabled; uint8_t relayA, relayB; uint16_t pauseMs; };

InCfg          diCfg[NUM_DI];
RlyCfg         rlyCfg[NUM_RLY];
LedCfg         ledCfg[NUM_LED];
BtnCfg         btnCfg[NUM_BTN];
InterlockCfg   g_interlock;

uint16_t g_longPressMs = 700;
uint16_t g_multiClickGapMs = 300;
uint16_t g_debounceMs = 30;
uint16_t g_linkTimeoutMs = 5000;
uint32_t g_commsTimeoutMs = 0;       // Phase B — unused
bool     g_inputsInSafeMode = false; // Phase B — unused

// ================== Runtime state ==================
bool desiredRelay[NUM_RLY] = {false, false, false};
bool prevDesiredRelay[NUM_RLY] = {false, false, false};
uint32_t rlyAutoOffUntil[NUM_RLY] = {0, 0, 0};
uint32_t rlyLastOffMs[NUM_RLY] = {0, 0, 0};

struct DebounceState {
  bool raw;
  bool stable;
  bool prevStable;
  uint32_t lastChangeMs;
};
struct ClickState {
  bool pressed;
  uint32_t pressStartMs;
  bool longFired;
  uint8_t pendingClicks;
  uint32_t lastReleaseMs;
  bool gapPending;
};

DebounceState diDeb[NUM_DI];
DebounceState btnDeb[NUM_BTN];
ClickState    diClick[NUM_DI];
ClickState    btnClick[NUM_BTN];

uint16_t evtCount[NUM_EVT_SRC][4] = {{0}};

bool     coilSnapBefore[15] = {false};
uint32_t g_lastLinkSeenMs = 0;
uint32_t g_identifyUntilMs = 0;
const uint32_t IDENTIFY_MS = 5000;

bool ledHaState[NUM_LED] = {false, false, false};
bool ledPhys[NUM_LED] = {false, false, false};

// ================== Web Serial ==================
SimpleWebSerial WebSerial;

static inline void wsLog(const char* msg) { WebSerial.send("log", msg); }
static inline void wsLog(const String& msg) { WebSerial.send("log", msg); }

// ================== Timing ==================
unsigned long lastSend = 0;
const unsigned long sendInterval = 250;
unsigned long lastBlinkToggle = 0;
const unsigned long blinkPeriodMs = 400;
bool blinkPhase = false;

// ================== Persisted Modbus settings ==================
uint8_t  g_mb_address = 3;
uint32_t g_mb_baud    = 19200;

// Legacy InCfg without maintMode (CFG 0x000A).
struct InCfgV10 { bool enabled, inverted; uint8_t type; uint8_t followTarget; uint8_t shortAction, shortTarget; uint8_t longAction, longTarget; bool lockLocal; };

// ================== Persistence (LittleFS) ==================
struct PersistConfig {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  InCfg          diCfg[NUM_DI];
  RlyCfg         rlyCfg[NUM_RLY];
  LedCfg         ledCfg[NUM_LED];
  BtnCfg         btnCfg[NUM_BTN];
  InterlockCfg   interlock;
  uint16_t       longPressMs;
  uint16_t       multiClickGapMs;
  uint16_t       debounceMs;
  uint16_t       linkTimeoutMs;
  uint8_t        mb_address;
  uint32_t       mb_baud;
  uint32_t       crc32;
} __attribute__((packed));

struct PersistConfigV10 {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  InCfgV10       diCfg[NUM_DI];
  RlyCfg         rlyCfg[NUM_RLY];
  LedCfg         ledCfg[NUM_LED];
  BtnCfg         btnCfg[NUM_BTN];
  InterlockCfg   interlock;
  bool           localLogicEnabled;
  uint16_t       longPressMs;
  uint16_t       multiClickGapMs;
  uint16_t       debounceMs;
  uint16_t       linkTimeoutMs;
  uint8_t        mb_address;
  uint32_t       mb_baud;
  uint32_t       crc32;
} __attribute__((packed));

struct PersistConfigV9 {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  InCfgV9   diCfg[NUM_DI];
  RlyCfgV9  rlyCfg[NUM_RLY];
  LedCfgV9  ledCfg[NUM_LED];
  BtnCfgV9  btnCfg[NUM_BTN];
  uint8_t   mb_address;
  uint32_t  mb_baud;
  uint32_t  crc32;
} __attribute__((packed));

struct PersistConfigV7 {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  InCfgV9    diCfg[NUM_DI];
  RlyCfgV7   rlyCfg[NUM_RLY];
  LedCfgV9   ledCfg[NUM_LED];
  BtnCfgV9   btnCfg[NUM_BTN];
  bool       desiredRelay[NUM_RLY];
  uint8_t    mb_address;
  uint32_t   mb_baud;
  uint32_t   crc32;
} __attribute__((packed));

struct OutputStateSnapshot {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  bool desiredRelay[NUM_RLY];
  uint32_t crc32;
} __attribute__((packed));

static const uint32_t CFG_MAGIC = 0x314D4C41UL;
static const uint16_t CFG_VERSION = 0x000B;
static const uint16_t CFG_VERSION_V10 = 0x000A;
static const uint16_t CFG_VERSION_V9 = 0x0009;
static const uint16_t CFG_VERSION_V7 = 0x0007;
static const char*    CFG_PATH = "/cfg.bin";
static const char*    OUT_STATE_PATH = "/cfg_out.bin";
static const uint32_t OUT_STATE_MAGIC = 0x484D4F53UL;
static const uint16_t OUT_STATE_VERSION = 0x0001;

volatile bool   cfgDirty = false;
uint32_t        lastCfgTouchMs = 0;
const uint32_t  CFG_AUTOSAVE_MS = 1500;
uint32_t        lastOutChangeMs = 0;
uint32_t        lastOutSaveMs = 0;
const uint32_t  OUT_AUTOSAVE_MS = 10000;
bool            outTrackInit = false;

// ================== Modbus map ==================
enum : uint16_t {
  IREG_DI_MASK = 0,
  IREG_RLY_MASK = 1,
  IREG_BTN_MASK = 2,
  IREG_LED_MASK = 3,
  IREG_STATUS_FLAGS = 4,
  IREG_LOCK_MASK = 5,
  EVT_BASE = 6,

  COIL_RLY_BASE = 0,
  COIL_ALL_OFF = 3,
  COIL_LOCAL_LOGIC = 4,
  COIL_IDENTIFY = 5,
  COIL_SAVE_CFG = 6,
  COIL_REBOOT = 7,
  COIL_LED_HA_BASE = 8,
  COIL_DI_LOCK_BASE = 11,

  HREG_MB_ADDR = 3,
  HREG_MB_BAUD = 4,
  HREG_DI_EN_MASK = 8,
  HREG_DI_INV_MASK = 9,
  HREG_DI_TYPE_MASK = 10,
  HREG_DI_LOCK_MASK = 11,
  HREG_DI_FOLLOW_BASE = 12,
  HREG_DI_SHORT_BASE = 16,
  HREG_DI_LONG_BASE = 20,
  HREG_RLY_EN_MASK = 24,
  HREG_RLY_INV_MASK = 25,
  HREG_RLY_POWERON = 26,
  HREG_RLY_AUTOOFF_BASE = 27,
  HREG_BTN1_SHORT = 30,
  HREG_BTN1_LONG = 31,
  HREG_BTN2_SHORT = 32,
  HREG_BTN2_LONG = 33,
  HREG_LED_BASE = 34,
  HREG_INTERLOCK = 40,
  HREG_INTERLOCK_PAUSE = 41,
  HREG_DI_MAINT_MODE_MASK = 42,
  HREG_LONGPRESS_MS = 43,
  HREG_MULTICLICK_MS = 44,
  HREG_DEBOUNCE_MS = 45,
  HREG_LINKTIMEOUT_MS = 46
};

// ================== Utils ==================
uint32_t crc32_update(uint32_t crc, const uint8_t* data, size_t len) {
  crc = ~crc;
  while (len--) {
    crc ^= *data++;
    for (uint8_t k = 0; k < 8; k++)
      crc = (crc >> 1) ^ (0xEDB88320UL & (-(int32_t)(crc & 1)));
  }
  return ~crc;
}
inline bool timeAfter32(uint32_t a, uint32_t b) { return (int32_t)(a - b) >= 0; }

static inline uint8_t packActTarget(uint8_t action, uint8_t target) {
  return (uint8_t)(((action & 0x07) << 3) | (target & 0x07));
}
static inline uint8_t unpackAction(uint8_t v) { return (v >> 3) & 0x07; }
static inline uint8_t unpackTarget(uint8_t v) { return v & 0x07; }

static inline uint16_t packLedHreg(const LedCfg& lc) {
  return (uint16_t)(((lc.source & 0x0F) << 8) | ((lc.arg & 0x0F) << 4) | ((lc.mode & 0x01) << 1) | (lc.inverted ? 1 : 0));
}
static inline void unpackLedHreg(uint16_t v, LedCfg& lc) {
  lc.inverted = (v & 1) != 0;
  lc.mode = (v >> 1) & 0x01;
  lc.arg = (v >> 4) & 0x0F;
  lc.source = (v >> 8) & 0x0F;
}

static inline bool localInputAllowed(bool lockLocal) {
  return !lockLocal;
}

static inline uint8_t evtSourceForDi(uint8_t i) { return i; }
static inline uint8_t evtSourceForBtn(uint8_t i) { return (uint8_t)(4 + i); }

void touchCfgDirty() {
  cfgDirty = true;
  lastCfgTouchMs = millis();
}

// ================== Defaults / persist ==================
void setDefaults() {
  for (int i = 0; i < NUM_DI; i++) {
    diCfg[i] = {true, false, IN_MAINTAINED, (uint8_t)(i + 1), ACT_NONE, 0, ACT_NONE, 0, false, MAINT_TOGGLE};
  }
  diCfg[3] = {true, false, IN_MOMENTARY, 0, ACT_ALLOFF, 0, ACT_NONE, 0, false, MAINT_TOGGLE};

  for (int i = 0; i < NUM_RLY; i++) {
    rlyCfg[i] = {true, false, HM_PWR_OFF, 0, 0};
  }

  ledCfg[0] = {LED_LINK, 0, false, 0};
  ledCfg[1] = {LED_OFF, 0, false, 0};
  ledCfg[2] = {LED_HA, 0, false, 0};

  btnCfg[0] = {ACT_TOGGLE, 1, ACT_ALLOFF, 0};
  btnCfg[1] = {ACT_TOGGLE, 2, ACT_NONE, 0};

  g_interlock = {false, 0, 1, 500};
  g_longPressMs = 700;
  g_multiClickGapMs = 300;
  g_debounceMs = 30;
  g_linkTimeoutMs = 5000;

  for (int i = 0; i < NUM_RLY; i++) {
    desiredRelay[i] = false;
    rlyAutoOffUntil[i] = 0;
    rlyLastOffMs[i] = 0;
  }
  memset(evtCount, 0, sizeof(evtCount));
  g_mb_address = 3;
  g_mb_baud = 19200;
}

static void migrateLegacyDi(const InCfgV9& old, InCfg& neu) {
  neu.enabled = old.enabled;
  neu.inverted = old.inverted;
  neu.lockLocal = false;
  neu.longAction = ACT_NONE;
  neu.longTarget = 0;
  neu.maintMode = MAINT_TOGGLE;
  if (old.action == 0) {
    neu.type = IN_MAINTAINED;
    neu.followTarget = old.target;
    neu.shortAction = ACT_NONE;
    neu.shortTarget = 0;
  } else {
    neu.type = IN_MOMENTARY;
    neu.followTarget = 0;
    neu.shortAction = ACT_TOGGLE;
    neu.shortTarget = old.target;
  }
}

static void migrateDiV10(const InCfgV10& old, InCfg& neu) {
  neu.enabled = old.enabled;
  neu.inverted = old.inverted;
  neu.type = old.type;
  neu.followTarget = old.followTarget;
  neu.shortAction = old.shortAction;
  neu.shortTarget = old.shortTarget;
  neu.longAction = old.longAction;
  neu.longTarget = old.longTarget;
  neu.lockLocal = old.lockLocal;
  neu.maintMode = (old.type == IN_MAINTAINED) ? MAINT_FOLLOW : MAINT_TOGGLE;
}

static void sanitizeLedCfg(LedCfg& lc) {
  if (lc.source == LED_LOCAL) lc.source = LED_OFF;
}

static void migrateLegacyLed(const LedCfgV9& old, LedCfg& neu) {
  neu.mode = old.mode;
  neu.inverted = false;
  neu.arg = 0;
  if (old.source >= 5 && old.source <= 7) {
    neu.source = LED_RELAY;
    neu.arg = (uint8_t)(old.source - 5);
  } else {
    neu.source = LED_OFF;
  }
}

static void migrateLegacyBtn(const BtnCfgV9& old, BtnCfg& neu) {
  if (old.action >= 5 && old.action <= 7) {
    neu.shortAction = ACT_TOGGLE;
    neu.shortTarget = (uint8_t)(old.action - 4);
    neu.longAction = ACT_NONE;
    neu.longTarget = 0;
  } else {
    neu.shortAction = ACT_NONE;
    neu.shortTarget = 0;
    neu.longAction = ACT_NONE;
    neu.longTarget = 0;
  }
}

void applyMigratedBasics(uint8_t mbAddr, uint32_t mbBaud) {
  g_mb_address = mbAddr;
  g_mb_baud = mbBaud;
}

bool readOutputStateSnapshot(bool out[NUM_RLY]) {
  File f = LittleFS.open(OUT_STATE_PATH, "r");
  if (!f) return false;
  if ((size_t)f.size() != sizeof(OutputStateSnapshot)) { f.close(); return false; }
  OutputStateSnapshot snap{};
  size_t n = f.read((uint8_t*)&snap, sizeof(snap));
  f.close();
  if (n != sizeof(snap)) return false;
  if (snap.magic != OUT_STATE_MAGIC || snap.version != OUT_STATE_VERSION || snap.size != sizeof(OutputStateSnapshot)) return false;
  OutputStateSnapshot tmp = snap;
  uint32_t crc = tmp.crc32;
  tmp.crc32 = 0;
  if (crc32_update(0, (const uint8_t*)&tmp, sizeof(tmp)) != crc) return false;
  memcpy(out, snap.desiredRelay, sizeof(snap.desiredRelay));
  return true;
}

bool saveOutputStateSnapshot() {
  OutputStateSnapshot snap{};
  snap.magic = OUT_STATE_MAGIC;
  snap.version = OUT_STATE_VERSION;
  snap.size = sizeof(OutputStateSnapshot);
  memcpy(snap.desiredRelay, desiredRelay, sizeof(desiredRelay));
  snap.crc32 = 0;
  snap.crc32 = crc32_update(0, (const uint8_t*)&snap, sizeof(snap));
  File f = LittleFS.open(OUT_STATE_PATH, "w");
  if (!f) return false;
  size_t n = f.write((const uint8_t*)&snap, sizeof(snap));
  f.flush();
  f.close();
  return n == sizeof(snap);
}

void armAutoOffTimer(uint8_t r, uint32_t now) {
  if (r >= NUM_RLY) return;
  if (rlyCfg[r].autoOffSec > 0) {
    rlyAutoOffUntil[r] = now + (uint32_t)rlyCfg[r].autoOffSec * 1000UL;
  } else {
    rlyAutoOffUntil[r] = 0;
  }
}

void applyPowerOnOutputs() {
  bool restored[NUM_RLY] = {false, false, false};
  bool haveSnap = readOutputStateSnapshot(restored);
  uint32_t now = millis();
  for (int i = 0; i < NUM_RLY; i++) {
    rlyAutoOffUntil[i] = 0;
    if (rlyCfg[i].powerOn == HM_PWR_ON) {
      desiredRelay[i] = true;
      armAutoOffTimer((uint8_t)i, now);
    } else if (rlyCfg[i].powerOn == HM_PWR_RESTORE && haveSnap) {
      desiredRelay[i] = restored[i];
      if (desiredRelay[i]) armAutoOffTimer((uint8_t)i, now);
    } else {
      desiredRelay[i] = false;
    }
  }
  memcpy(prevDesiredRelay, desiredRelay, sizeof(prevDesiredRelay));
  outTrackInit = true;
  lastOutChangeMs = now;
}

void maybePersistOutputState(uint32_t now) {
  bool needRestore = false;
  for (int i = 0; i < NUM_RLY; i++) {
    if (rlyCfg[i].powerOn == HM_PWR_RESTORE) { needRestore = true; break; }
  }
  if (!needRestore) return;
  if ((uint32_t)(now - lastOutChangeMs) < OUT_AUTOSAVE_MS) return;
  if (lastOutSaveMs && (uint32_t)(now - lastOutSaveMs) < OUT_AUTOSAVE_MS) return;
  if (saveOutputStateSnapshot()) lastOutSaveMs = now;
}

void captureToPersist(PersistConfig& pc) {
  pc.magic = CFG_MAGIC;
  pc.version = CFG_VERSION;
  pc.size = sizeof(PersistConfig);
  memcpy(pc.diCfg, diCfg, sizeof(diCfg));
  memcpy(pc.rlyCfg, rlyCfg, sizeof(rlyCfg));
  memcpy(pc.ledCfg, ledCfg, sizeof(ledCfg));
  memcpy(pc.btnCfg, btnCfg, sizeof(btnCfg));
  pc.interlock = g_interlock;
  pc.longPressMs = g_longPressMs;
  pc.multiClickGapMs = g_multiClickGapMs;
  pc.debounceMs = g_debounceMs;
  pc.linkTimeoutMs = g_linkTimeoutMs;
  pc.mb_address = g_mb_address;
  pc.mb_baud = g_mb_baud;
  pc.crc32 = 0;
  pc.crc32 = crc32_update(0, (const uint8_t*)&pc, sizeof(PersistConfig));
}

bool applyFromPersist(const PersistConfig& pc) {
  if (pc.magic != CFG_MAGIC || pc.size != sizeof(PersistConfig)) return false;
  PersistConfig tmp = pc;
  uint32_t crc = tmp.crc32;
  tmp.crc32 = 0;
  if (crc32_update(0, (const uint8_t*)&tmp, sizeof(PersistConfig)) != crc) return false;
  if (pc.version != CFG_VERSION) return false;

  memcpy(diCfg, pc.diCfg, sizeof(diCfg));
  memcpy(rlyCfg, pc.rlyCfg, sizeof(rlyCfg));
  memcpy(ledCfg, pc.ledCfg, sizeof(ledCfg));
  for (int i = 0; i < NUM_LED; i++) sanitizeLedCfg(ledCfg[i]);
  memcpy(btnCfg, pc.btnCfg, sizeof(btnCfg));
  g_interlock = pc.interlock;
  g_longPressMs = pc.longPressMs ? pc.longPressMs : 700;
  g_multiClickGapMs = pc.multiClickGapMs ? pc.multiClickGapMs : 300;
  g_debounceMs = pc.debounceMs ? pc.debounceMs : 30;
  g_linkTimeoutMs = pc.linkTimeoutMs ? pc.linkTimeoutMs : 5000;
  g_mb_address = pc.mb_address;
  g_mb_baud = pc.mb_baud;
  return true;
}

bool applyFromPersistV10(const PersistConfigV10& pc) {
  if (pc.magic != CFG_MAGIC || pc.size != sizeof(PersistConfigV10)) return false;
  PersistConfigV10 tmp = pc;
  uint32_t crc = tmp.crc32;
  tmp.crc32 = 0;
  if (crc32_update(0, (const uint8_t*)&tmp, sizeof(PersistConfigV10)) != crc) return false;
  if (pc.version != CFG_VERSION_V10) return false;

  for (int i = 0; i < NUM_DI; i++) migrateDiV10(pc.diCfg[i], diCfg[i]);
  memcpy(rlyCfg, pc.rlyCfg, sizeof(rlyCfg));
  memcpy(ledCfg, pc.ledCfg, sizeof(ledCfg));
  for (int i = 0; i < NUM_LED; i++) sanitizeLedCfg(ledCfg[i]);
  memcpy(btnCfg, pc.btnCfg, sizeof(btnCfg));
  g_interlock = pc.interlock;
  g_longPressMs = pc.longPressMs ? pc.longPressMs : 700;
  g_multiClickGapMs = pc.multiClickGapMs ? pc.multiClickGapMs : 300;
  g_debounceMs = pc.debounceMs ? pc.debounceMs : 30;
  g_linkTimeoutMs = pc.linkTimeoutMs ? pc.linkTimeoutMs : 5000;
  g_mb_address = pc.mb_address;
  g_mb_baud = pc.mb_baud;
  return true;
}

bool applyFromPersistV9(const PersistConfigV9& pc) {
  if (pc.magic != CFG_MAGIC || pc.size != sizeof(PersistConfigV9)) return false;
  PersistConfigV9 tmp = pc;
  uint32_t crc = tmp.crc32;
  tmp.crc32 = 0;
  if (crc32_update(0, (const uint8_t*)&tmp, sizeof(PersistConfigV9)) != crc) return false;
  if (pc.version != CFG_VERSION_V9) return false;

  setDefaults();
  for (int i = 0; i < NUM_DI; i++) migrateLegacyDi(pc.diCfg[i], diCfg[i]);
  for (int i = 0; i < NUM_RLY; i++) {
    rlyCfg[i].enabled = pc.rlyCfg[i].enabled;
    rlyCfg[i].inverted = pc.rlyCfg[i].inverted;
    rlyCfg[i].powerOn = pc.rlyCfg[i].powerOn;
  }
  for (int i = 0; i < NUM_LED; i++) migrateLegacyLed(pc.ledCfg[i], ledCfg[i]);
  for (int i = 0; i < NUM_BTN; i++) migrateLegacyBtn(pc.btnCfg[i], btnCfg[i]);
  applyMigratedBasics(pc.mb_address, pc.mb_baud);
  return true;
}

bool applyFromPersistV7(const PersistConfigV7& pc) {
  if (pc.magic != CFG_MAGIC || pc.size != sizeof(PersistConfigV7)) return false;
  PersistConfigV7 tmp = pc;
  uint32_t crc = tmp.crc32;
  tmp.crc32 = 0;
  if (crc32_update(0, (const uint8_t*)&tmp, sizeof(PersistConfigV7)) != crc) return false;
  if (pc.version != CFG_VERSION_V7) return false;

  setDefaults();
  for (int i = 0; i < NUM_DI; i++) migrateLegacyDi(pc.diCfg[i], diCfg[i]);
  for (int i = 0; i < NUM_RLY; i++) {
    rlyCfg[i].enabled = pc.rlyCfg[i].enabled;
    rlyCfg[i].inverted = pc.rlyCfg[i].inverted;
    rlyCfg[i].powerOn = HM_PWR_OFF;
  }
  for (int i = 0; i < NUM_LED; i++) migrateLegacyLed(pc.ledCfg[i], ledCfg[i]);
  for (int i = 0; i < NUM_BTN; i++) migrateLegacyBtn(pc.btnCfg[i], btnCfg[i]);
  applyMigratedBasics(pc.mb_address, pc.mb_baud);
  return true;
}

bool saveConfigFS() {
  PersistConfig pc{};
  captureToPersist(pc);
  File f = LittleFS.open(CFG_PATH, "w");
  if (!f) { wsLog("save: open failed"); return false; }
  size_t n = f.write((const uint8_t*)&pc, sizeof(pc));
  f.flush();
  f.close();
  if (n != sizeof(pc)) { wsLog(String("save: short write ") + n); return false; }
  File r = LittleFS.open(CFG_PATH, "r");
  if (!r) { wsLog("save: reopen failed"); return false; }
  if ((size_t)r.size() != sizeof(PersistConfig)) { wsLog("save: size mismatch after write"); r.close(); return false; }
  PersistConfig back{};
  size_t nr = r.read((uint8_t*)&back, sizeof(back));
  r.close();
  if (nr != sizeof(back)) { wsLog("save: short readback"); return false; }
  PersistConfig tmp = back;
  uint32_t crc = tmp.crc32;
  tmp.crc32 = 0;
  if (crc32_update(0, (const uint8_t*)&tmp, sizeof(tmp)) != crc) { wsLog("save: CRC verify failed"); return false; }
  return true;
}

bool loadConfigFS() {
  File f = LittleFS.open(CFG_PATH, "r");
  if (!f) { wsLog("load: open failed"); return false; }
  size_t sz = f.size();

  if (sz == sizeof(PersistConfigV7)) {
    PersistConfigV7 pc{};
    size_t n = f.read((uint8_t*)&pc, sizeof(pc));
    f.close();
    if (n != sizeof(pc)) { wsLog("load: short read (v7)"); return false; }
    if (!applyFromPersistV7(pc)) { wsLog("load: v7 magic/version/crc mismatch"); return false; }
    touchCfgDirty();
    wsLog("load: migrated from v7");
    return true;
  }
  if (sz == sizeof(PersistConfigV10)) {
    PersistConfigV10 pc{};
    size_t n = f.read((uint8_t*)&pc, sizeof(pc));
    f.close();
    if (n != sizeof(pc)) { wsLog("load: short read (v10)"); return false; }
    if (!applyFromPersistV10(pc)) { wsLog("load: v10 magic/version/crc mismatch"); return false; }
    touchCfgDirty();
    wsLog("load: migrated from v10");
    return true;
  }
  if (sz == sizeof(PersistConfigV9)) {
    PersistConfigV9 pc{};
    size_t n = f.read((uint8_t*)&pc, sizeof(pc));
    f.close();
    if (n != sizeof(pc)) { wsLog("load: short read (v9)"); return false; }
    if (!applyFromPersistV9(pc)) { wsLog("load: v9 magic/version/crc mismatch"); return false; }
    touchCfgDirty();
    wsLog("load: migrated from v9");
    return true;
  }
  if (sz != sizeof(PersistConfig)) { wsLog(String("load: size ") + sz + " unsupported"); f.close(); return false; }
  PersistConfig pc{};
  size_t n = f.read((uint8_t*)&pc, sizeof(pc));
  f.close();
  if (n != sizeof(pc)) { wsLog("load: short read"); return false; }
  if (!applyFromPersist(pc)) { wsLog("load: magic/version/crc mismatch"); return false; }
  return true;
}

bool initFilesystemAndConfig() {
  if (!LittleFS.begin()) {
    wsLog("LittleFS mount failed. Formatting…");
    if (!LittleFS.format() || !LittleFS.begin()) {
      wsLog("FATAL: FS mount/format failed");
      return false;
    }
  }

  if (loadConfigFS()) {
    wsLog("Config loaded from flash");
    applyPowerOnOutputs();
    return true;
  }

  wsLog("No valid config. Using defaults.");
  setDefaults();
  applyPowerOnOutputs();
  if (saveConfigFS()) {
    wsLog("Defaults saved");
    return true;
  }

  wsLog("First save failed. Formatting FS…");
  if (!LittleFS.format() || !LittleFS.begin()) {
    wsLog("FATAL: FS format failed");
    return false;
  }

  setDefaults();
  applyPowerOnOutputs();
  if (saveConfigFS()) {
    wsLog("FS formatted and config saved");
    return true;
  }

  wsLog("FATAL: save still failing after format");
  return false;
}

// ================== SFINAE helper ==================
template <class M>
inline auto setSlaveIdIfAvailable(M& m, uint8_t id)
  -> decltype(std::declval<M&>().setSlaveId(uint8_t{}), void()) { m.setSlaveId(id); }
inline void setSlaveIdIfAvailable(...) {}

// ================== Fw decls ==================
void applyModbusSettings(uint8_t addr, uint32_t baud);
void handleValues(JSONVar values);
void handleUnifiedConfig(JSONVar obj);
void handleCommand(JSONVar obj);
void sendWebStatus();
void sendWebCfg();
void sendWebBootstrap();
void buildModbusMap();
void syncHoldingFromCfg();
void applyHoldingToCfg();
void updateInputRegisters(uint32_t now);
void syncCoilsFromState();
void processModbusCoils(uint32_t now);
void applyAction(uint8_t target, uint8_t action, uint32_t now);
void applyMaintainedInput(uint8_t diIdx, bool level);
void serviceDebounce(DebounceState& st, bool raw, uint32_t now);
void serviceMomentaryChannel(uint8_t src, bool lockLocal, uint8_t shortAction, uint8_t shortTarget,
                             uint8_t longAction, uint8_t longTarget, DebounceState& db, ClickState& cs, uint32_t now);
void finalizeClickGaps(uint32_t now);
void serviceAutoOff(uint32_t now);
void applyInterlock(bool logicalIn[NUM_RLY], bool logicalOut[NUM_RLY], uint32_t now);
bool computeLedActive(uint8_t ledIdx, uint32_t now);
void updateLinkOkDetector(uint32_t now);
void trackDesiredRelayChanges(uint32_t now);

// ================== Action engine ==================
void applyAction(uint8_t target, uint8_t action, uint32_t now) {
  if (action == ACT_NONE) return;
  if (action == ACT_ALLOFF) {
    for (int r = 0; r < NUM_RLY; r++) {
      if (desiredRelay[r]) desiredRelay[r] = false;
      rlyAutoOffUntil[r] = 0;
    }
    return;
  }
  if (target == 4) return;

  auto doRelay = [&](int rIdx) {
    if (rIdx < 0 || rIdx >= NUM_RLY) return;
    bool was = desiredRelay[rIdx];
    if (action == ACT_TOGGLE) desiredRelay[rIdx] = !desiredRelay[rIdx];
    else if (action == ACT_ON) desiredRelay[rIdx] = true;
    else if (action == ACT_OFF) desiredRelay[rIdx] = false;
    if (!was && desiredRelay[rIdx]) armAutoOffTimer((uint8_t)rIdx, now);
    if (!desiredRelay[rIdx]) rlyAutoOffUntil[rIdx] = 0;
  };

  if (target == 0) {
    for (int r = 0; r < NUM_RLY; r++) doRelay(r);
  } else if (target >= 1 && target <= 3) {
    doRelay(target - 1);
  }
}

void applyMaintainedInput(uint8_t diIdx, bool level) {
  if (diIdx >= NUM_DI) return;
  const InCfg& c = diCfg[diIdx];
  if (!c.enabled || c.type != IN_MAINTAINED) return;
  if (c.lockLocal) return;
  if (c.followTarget == 4) return;

  uint32_t now = millis();
  if (c.maintMode == MAINT_TOGGLE) {
    applyAction(c.followTarget, ACT_TOGGLE, now);
    return;
  }

  if (c.followTarget == 0) {
    for (int r = 0; r < NUM_RLY; r++) {
      bool was = desiredRelay[r];
      desiredRelay[r] = level;
      if (!was && level) armAutoOffTimer((uint8_t)r, now);
      if (!level) rlyAutoOffUntil[r] = 0;
    }
  } else if (c.followTarget >= 1 && c.followTarget <= 3) {
    int r = c.followTarget - 1;
    bool was = desiredRelay[r];
    desiredRelay[r] = level;
    if (!was && level) armAutoOffTimer((uint8_t)r, now);
    if (!level) rlyAutoOffUntil[r] = 0;
  }
}

static void incEvt(uint8_t src, EvtType type) {
  if (src >= NUM_EVT_SRC || type > EVT_LONG) return;
  if (evtCount[src][type] < 0xFFFF) evtCount[src][type]++;
}

void serviceDebounce(DebounceState& st, bool raw, uint32_t now) {
  if (raw != st.raw) {
    st.raw = raw;
    st.lastChangeMs = now;
  }
  if ((uint32_t)(now - st.lastChangeMs) >= g_debounceMs) {
    st.prevStable = st.stable;
    st.stable = st.raw;
  }
}

void serviceMomentaryChannel(uint8_t src, bool lockLocal, uint8_t shortAction, uint8_t shortTarget,
                             uint8_t longAction, uint8_t longTarget, DebounceState& db, ClickState& cs, uint32_t now) {
  bool rising = (!db.prevStable && db.stable);
  bool falling = (db.prevStable && !db.stable);
  bool localOk = localInputAllowed(lockLocal);

  if (rising) {
    cs.pressed = true;
    cs.pressStartMs = now;
    cs.longFired = false;
    if (longAction == ACT_NONE && localOk) {
      applyAction(shortTarget, shortAction, now);
    }
  }

  if (cs.pressed && db.stable && longAction != ACT_NONE && !cs.longFired) {
    if ((uint32_t)(now - cs.pressStartMs) >= g_longPressMs) {
      if (localOk) applyAction(longTarget, longAction, now);
      incEvt(src, EVT_LONG);
      cs.longFired = true;
    }
  }

  if (falling && cs.pressed) {
    cs.pressed = false;
    if (longAction != ACT_NONE) {
      if (!cs.longFired && localOk) {
        applyAction(shortTarget, shortAction, now);
      }
      if (!cs.longFired) {
        cs.pendingClicks++;
        cs.lastReleaseMs = now;
        cs.gapPending = true;
      }
    } else {
      cs.pendingClicks++;
      cs.lastReleaseMs = now;
      cs.gapPending = true;
    }
  }
}

void finalizeClickGaps(uint32_t now) {
  for (uint8_t s = 0; s < NUM_EVT_SRC; s++) {
    ClickState* cs = (s < NUM_DI) ? &diClick[s] : &btnClick[s - NUM_DI];
    if (!cs->gapPending) continue;
    if ((uint32_t)(now - cs->lastReleaseMs) < g_multiClickGapMs) continue;
    if (cs->pendingClicks >= 2) incEvt(s, EVT_DOUBLE);
    else if (cs->pendingClicks == 1) incEvt(s, EVT_SINGLE);
    cs->pendingClicks = 0;
    cs->gapPending = false;
  }
}

void serviceAutoOff(uint32_t now) {
  for (int r = 0; r < NUM_RLY; r++) {
    if (rlyAutoOffUntil[r] && timeAfter32(now, rlyAutoOffUntil[r])) {
      desiredRelay[r] = false;
      rlyAutoOffUntil[r] = 0;
    }
  }
}

void applyInterlock(bool logicalIn[NUM_RLY], bool logicalOut[NUM_RLY], uint32_t now) {
  memcpy(logicalOut, logicalIn, sizeof(bool) * NUM_RLY);
  if (!g_interlock.enabled) return;

  uint8_t a = g_interlock.relayA;
  uint8_t b = g_interlock.relayB;
  if (a >= NUM_RLY || b >= NUM_RLY || a == b) return;

  bool wantA = logicalOut[a];
  bool wantB = logicalOut[b];
  if (wantA && wantB) wantA = wantB = false;
  if (wantA && (uint32_t)(now - rlyLastOffMs[b]) < g_interlock.pauseMs) wantA = false;
  if (wantB && (uint32_t)(now - rlyLastOffMs[a]) < g_interlock.pauseMs) wantB = false;
  logicalOut[a] = wantA;
  logicalOut[b] = wantB;
}

bool anyChildLockActive(uint8_t arg) {
  if (arg >= 1 && arg <= NUM_DI) return diCfg[arg - 1].lockLocal;
  for (int i = 0; i < NUM_DI; i++) {
    if (diCfg[i].lockLocal) return true;
  }
  return false;
}

bool computeLedActive(uint8_t ledIdx, uint32_t now) {
  if (ledIdx >= NUM_LED) return false;
  const LedCfg& lc = ledCfg[ledIdx];
  bool linkOk = ((uint32_t)(now - g_lastLinkSeenMs) < g_linkTimeoutMs);
  bool active = false;

  switch ((LedSource)lc.source) {
    case LED_OFF: active = false; break;
    case LED_HA: active = ledHaState[ledIdx]; break;
    case LED_LINK: active = linkOk; break;
    case LED_CHILDLOCK: active = anyChildLockActive(lc.arg); break;
    case LED_SAFEMODE: active = g_inputsInSafeMode; break;
    case LED_IDENTIFY: active = g_identifyUntilMs && !timeAfter32(now, g_identifyUntilMs); break;
    case LED_RELAY:
      if (lc.arg >= 1 && lc.arg <= NUM_RLY) active = desiredRelay[lc.arg - 1];
      break;
    default: active = false; break;
  }

  if (lc.inverted) active = !active;
  if (lc.mode == 1) active = active && blinkPhase;
  return active;
}

// ================== Modbus map build ==================
void buildModbusMap() {
  for (uint16_t i = 0; i < 30; i++) mb.addIreg(i);
  for (uint16_t i = 0; i < 15; i++) {
    mb.addCoil(i);
    mb.setCoil(i, false);
  }
  for (uint16_t i = 0; i <= HREG_LINKTIMEOUT_MS; i++) mb.addHreg(i, 0);
  hmRegisterIdentityHolding(mb, HM_MODEL_ID, HM_FW_MAJOR, HM_FW_MINOR, HM_FW_PATCH, HM_MAP_VERSION);
}

void syncHoldingFromCfg() {
  mb.setHreg(0, HM_MODEL_ID);
  mb.setHreg(1, (uint16_t)((HM_FW_MAJOR << 8) | HM_FW_MINOR));
  mb.setHreg(2, HM_MAP_VERSION);
  mb.setHreg(HREG_MB_ADDR, g_mb_address);
  mb.setHreg(HREG_MB_BAUD, hmBaudCode(g_mb_baud));

  uint16_t diEn = 0, diInv = 0, diType = 0, diLock = 0, diMaint = 0;
  for (int i = 0; i < NUM_DI; i++) {
    if (diCfg[i].enabled) diEn |= (1 << i);
    if (diCfg[i].inverted) diInv |= (1 << i);
    if (diCfg[i].type == IN_MOMENTARY) diType |= (1 << i);
    if (diCfg[i].lockLocal) diLock |= (1 << i);
    if (diCfg[i].maintMode == MAINT_FOLLOW) diMaint |= (1 << i);
    mb.setHreg(HREG_DI_FOLLOW_BASE + i, diCfg[i].followTarget);
    mb.setHreg(HREG_DI_SHORT_BASE + i, packActTarget(diCfg[i].shortAction, diCfg[i].shortTarget));
    mb.setHreg(HREG_DI_LONG_BASE + i, packActTarget(diCfg[i].longAction, diCfg[i].longTarget));
  }
  mb.setHreg(HREG_DI_EN_MASK, diEn);
  mb.setHreg(HREG_DI_INV_MASK, diInv);
  mb.setHreg(HREG_DI_TYPE_MASK, diType);
  mb.setHreg(HREG_DI_LOCK_MASK, diLock);

  uint16_t rlyEn = 0, rlyInv = 0, rlyPwr = 0;
  for (int i = 0; i < NUM_RLY; i++) {
    if (rlyCfg[i].enabled) rlyEn |= (1 << i);
    if (rlyCfg[i].inverted) rlyInv |= (1 << i);
    rlyPwr |= ((rlyCfg[i].powerOn & 0x03) << (i * 2));
    mb.setHreg(HREG_RLY_AUTOOFF_BASE + i, rlyCfg[i].autoOffSec);
  }
  mb.setHreg(HREG_RLY_EN_MASK, rlyEn);
  mb.setHreg(HREG_RLY_INV_MASK, rlyInv);
  mb.setHreg(HREG_RLY_POWERON, rlyPwr);

  mb.setHreg(HREG_BTN1_SHORT, packActTarget(btnCfg[0].shortAction, btnCfg[0].shortTarget));
  mb.setHreg(HREG_BTN1_LONG, packActTarget(btnCfg[0].longAction, btnCfg[0].longTarget));
  mb.setHreg(HREG_BTN2_SHORT, packActTarget(btnCfg[1].shortAction, btnCfg[1].shortTarget));
  mb.setHreg(HREG_BTN2_LONG, packActTarget(btnCfg[1].longAction, btnCfg[1].longTarget));

  for (int i = 0; i < NUM_LED; i++) mb.setHreg(HREG_LED_BASE + i, packLedHreg(ledCfg[i]));

  uint16_t ilk = (uint16_t)((g_interlock.enabled ? 1 : 0) << 8) | ((g_interlock.relayA & 0x0F) << 4) | (g_interlock.relayB & 0x0F);
  mb.setHreg(HREG_INTERLOCK, ilk);
  mb.setHreg(HREG_INTERLOCK_PAUSE, g_interlock.pauseMs);
  mb.setHreg(HREG_DI_MAINT_MODE_MASK, diMaint);
  mb.setHreg(HREG_LONGPRESS_MS, g_longPressMs);
  mb.setHreg(HREG_MULTICLICK_MS, g_multiClickGapMs);
  mb.setHreg(HREG_DEBOUNCE_MS, g_debounceMs);
  mb.setHreg(HREG_LINKTIMEOUT_MS, g_linkTimeoutMs);
}

static bool hregChanged(uint16_t addr, uint16_t expected) {
  return mb.Hreg(addr) != expected;
}

void applyHoldingToCfg() {
  bool changed = false;

  uint16_t addr = mb.Hreg(HREG_MB_ADDR);
  if (addr >= 1 && addr <= 247 && addr != g_mb_address) {
    g_mb_address = (uint8_t)addr;
    changed = true;
  }
  uint32_t baud = hmBaudFromCode((uint8_t)mb.Hreg(HREG_MB_BAUD));
  if (baud != g_mb_baud) { g_mb_baud = baud; changed = true; }
  static uint8_t lastAddr = 0;
  static uint32_t lastBaud = 0;
  if (g_mb_address != lastAddr || g_mb_baud != lastBaud) {
    applyModbusSettings(g_mb_address, g_mb_baud);
    lastAddr = g_mb_address;
    lastBaud = g_mb_baud;
  }

  uint16_t diEn = mb.Hreg(HREG_DI_EN_MASK);
  uint16_t diInv = mb.Hreg(HREG_DI_INV_MASK);
  uint16_t diType = mb.Hreg(HREG_DI_TYPE_MASK);
  uint16_t diLock = mb.Hreg(HREG_DI_LOCK_MASK);
  uint16_t diMaint = mb.Hreg(HREG_DI_MAINT_MODE_MASK);
  for (int i = 0; i < NUM_DI; i++) {
    bool en = (diEn >> i) & 1;
    bool inv = (diInv >> i) & 1;
    bool mom = (diType >> i) & 1;
    bool lk = (diLock >> i) & 1;
    uint8_t maint = ((diMaint >> i) & 1) ? MAINT_FOLLOW : MAINT_TOGGLE;
    uint8_t follow = (uint8_t)mb.Hreg(HREG_DI_FOLLOW_BASE + i);
    uint8_t sh = (uint8_t)mb.Hreg(HREG_DI_SHORT_BASE + i);
    uint8_t lg = (uint8_t)mb.Hreg(HREG_DI_LONG_BASE + i);
    if (diCfg[i].enabled != en || diCfg[i].inverted != inv || diCfg[i].type != (mom ? IN_MOMENTARY : IN_MAINTAINED) ||
        diCfg[i].lockLocal != lk || diCfg[i].followTarget != follow || diCfg[i].maintMode != maint ||
        diCfg[i].shortAction != unpackAction(sh) || diCfg[i].shortTarget != unpackTarget(sh) ||
        diCfg[i].longAction != unpackAction(lg) || diCfg[i].longTarget != unpackTarget(lg)) {
      diCfg[i].enabled = en;
      diCfg[i].inverted = inv;
      diCfg[i].type = mom ? IN_MOMENTARY : IN_MAINTAINED;
      diCfg[i].lockLocal = lk;
      diCfg[i].followTarget = follow;
      diCfg[i].maintMode = maint;
      diCfg[i].shortAction = unpackAction(sh);
      diCfg[i].shortTarget = unpackTarget(sh);
      diCfg[i].longAction = unpackAction(lg);
      diCfg[i].longTarget = unpackTarget(lg);
      changed = true;
    }
  }

  uint16_t rlyEn = mb.Hreg(HREG_RLY_EN_MASK);
  uint16_t rlyInv = mb.Hreg(HREG_RLY_INV_MASK);
  uint16_t rlyPwr = mb.Hreg(HREG_RLY_POWERON);
  for (int i = 0; i < NUM_RLY; i++) {
    bool en = (rlyEn >> i) & 1;
    bool inv = (rlyInv >> i) & 1;
    uint8_t pwr = (rlyPwr >> (i * 2)) & 0x03;
    uint16_t autoOff = mb.Hreg(HREG_RLY_AUTOOFF_BASE + i);
    if (rlyCfg[i].enabled != en || rlyCfg[i].inverted != inv || rlyCfg[i].powerOn != pwr || rlyCfg[i].autoOffSec != autoOff) {
      rlyCfg[i].enabled = en;
      rlyCfg[i].inverted = inv;
      rlyCfg[i].powerOn = pwr;
      rlyCfg[i].autoOffSec = autoOff;
      changed = true;
    }
  }

  static const uint16_t btnShortReg[NUM_BTN] = {HREG_BTN1_SHORT, HREG_BTN2_SHORT};
  static const uint16_t btnLongReg[NUM_BTN] = {HREG_BTN1_LONG, HREG_BTN2_LONG};
  for (int i = 0; i < NUM_BTN; i++) {
    uint8_t sh = (uint8_t)mb.Hreg(btnShortReg[i]);
    uint8_t lg = (uint8_t)mb.Hreg(btnLongReg[i]);
    if (btnCfg[i].shortAction != unpackAction(sh) || btnCfg[i].shortTarget != unpackTarget(sh) ||
        btnCfg[i].longAction != unpackAction(lg) || btnCfg[i].longTarget != unpackTarget(lg)) {
      btnCfg[i].shortAction = unpackAction(sh);
      btnCfg[i].shortTarget = unpackTarget(sh);
      btnCfg[i].longAction = unpackAction(lg);
      btnCfg[i].longTarget = unpackTarget(lg);
      changed = true;
    }
  }

  for (int i = 0; i < NUM_LED; i++) {
    LedCfg lc = ledCfg[i];
    unpackLedHreg(mb.Hreg(HREG_LED_BASE + i), lc);
    if (ledCfg[i].source != lc.source || ledCfg[i].mode != lc.mode || ledCfg[i].inverted != lc.inverted || ledCfg[i].arg != lc.arg) {
      ledCfg[i] = lc;
      changed = true;
    }
  }

  uint16_t ilk = mb.Hreg(HREG_INTERLOCK);
  InterlockCfg ilkNew = g_interlock;
  ilkNew.enabled = ((ilk >> 8) & 1) != 0;
  ilkNew.relayA = (ilk >> 4) & 0x0F;
  ilkNew.relayB = ilk & 0x0F;
  ilkNew.pauseMs = mb.Hreg(HREG_INTERLOCK_PAUSE);
  if (ilkNew.enabled != g_interlock.enabled || ilkNew.relayA != g_interlock.relayA ||
      ilkNew.relayB != g_interlock.relayB || ilkNew.pauseMs != g_interlock.pauseMs) {
    g_interlock = ilkNew;
    changed = true;
  }

  if (mb.Hreg(HREG_LONGPRESS_MS) && mb.Hreg(HREG_LONGPRESS_MS) != g_longPressMs) {
    g_longPressMs = mb.Hreg(HREG_LONGPRESS_MS); changed = true;
  }
  if (mb.Hreg(HREG_MULTICLICK_MS) && mb.Hreg(HREG_MULTICLICK_MS) != g_multiClickGapMs) {
    g_multiClickGapMs = mb.Hreg(HREG_MULTICLICK_MS); changed = true;
  }
  if (mb.Hreg(HREG_DEBOUNCE_MS) && mb.Hreg(HREG_DEBOUNCE_MS) != g_debounceMs) {
    g_debounceMs = mb.Hreg(HREG_DEBOUNCE_MS); changed = true;
  }
  if (mb.Hreg(HREG_LINKTIMEOUT_MS) && mb.Hreg(HREG_LINKTIMEOUT_MS) != g_linkTimeoutMs) {
    g_linkTimeoutMs = mb.Hreg(HREG_LINKTIMEOUT_MS); changed = true;
  }

  if (changed) touchCfgDirty();
  (void)hregChanged; // reserved for future selective apply
}

void updateInputRegisters(uint32_t now) {
  uint16_t diMask = 0, btnMask = 0, ledMask = 0, lockMask = 0;
  bool linkOk = ((uint32_t)(now - g_lastLinkSeenMs) < g_linkTimeoutMs);

  for (int i = 0; i < NUM_DI; i++) {
    if (diCfg[i].enabled && diDeb[i].stable) diMask |= (1 << i);
    if (diCfg[i].lockLocal) lockMask |= (1 << i);
  }
  for (int i = 0; i < NUM_BTN; i++) {
    if (btnDeb[i].stable) btnMask |= (1 << i);
  }

  bool logicalRelay[NUM_RLY];
  bool interlocked[NUM_RLY];
  for (int i = 0; i < NUM_RLY; i++) {
    logicalRelay[i] = desiredRelay[i];
    if (!rlyCfg[i].enabled) logicalRelay[i] = false;
  }
  applyInterlock(logicalRelay, interlocked, now);

  uint16_t rlyMask = 0;
  for (int i = 0; i < NUM_RLY; i++) {
    if (interlocked[i]) rlyMask |= (1 << i);
  }

  for (int i = 0; i < NUM_LED; i++) {
    ledPhys[i] = computeLedActive((uint8_t)i, now);
    if (ledPhys[i]) ledMask |= (1 << i);
  }

  uint16_t status = 0;
  if (linkOk) status |= (1 << 1);
  if (g_inputsInSafeMode) status |= (1 << 2);
  if (cfgDirty) status |= (1 << 3);

  mb.setIreg(IREG_DI_MASK, diMask);
  mb.setIreg(IREG_RLY_MASK, rlyMask);
  mb.setIreg(IREG_BTN_MASK, btnMask);
  mb.setIreg(IREG_LED_MASK, ledMask);
  mb.setIreg(IREG_STATUS_FLAGS, status);
  mb.setIreg(IREG_LOCK_MASK, lockMask);

  for (uint8_t s = 0; s < NUM_EVT_SRC; s++) {
    for (uint8_t t = 0; t < 4; t++) {
      mb.setIreg((uint16_t)(EVT_BASE + s * 4 + t), evtCount[s][t]);
    }
  }
}

void syncCoilsFromState() {
  bool logicalRelay[NUM_RLY];
  bool interlocked[NUM_RLY];
  for (int i = 0; i < NUM_RLY; i++) {
    logicalRelay[i] = desiredRelay[i];
    if (!rlyCfg[i].enabled) logicalRelay[i] = false;
  }
  applyInterlock(logicalRelay, interlocked, millis());

  for (int i = 0; i < NUM_RLY; i++) mb.setCoil(COIL_RLY_BASE + i, interlocked[i]);
  mb.setCoil(COIL_LOCAL_LOGIC, false);
  for (int i = 0; i < NUM_LED; i++) mb.setCoil(COIL_LED_HA_BASE + i, ledHaState[i]);
  for (int i = 0; i < NUM_DI; i++) mb.setCoil(COIL_DI_LOCK_BASE + i, diCfg[i].lockLocal);
}

void processModbusCoils(uint32_t now) {
  for (int r = 0; r < NUM_RLY; r++) {
    bool c = mb.Coil(COIL_RLY_BASE + r);
    if (!rlyCfg[r].enabled) {
      desiredRelay[r] = false;
      mb.setCoil(COIL_RLY_BASE + r, false);
      continue;
    }
    if (c != desiredRelay[r]) {
      bool was = desiredRelay[r];
      desiredRelay[r] = c;
      if (!was && c) armAutoOffTimer((uint8_t)r, now);
      if (!c) rlyAutoOffUntil[r] = 0;
    }
  }

  if (mb.Coil(COIL_ALL_OFF)) {
    mb.setCoil(COIL_ALL_OFF, false);
    applyAction(0, ACT_ALLOFF, now);
  }

  if (mb.Coil(COIL_IDENTIFY)) {
    mb.setCoil(COIL_IDENTIFY, false);
    g_identifyUntilMs = now + IDENTIFY_MS;
  }

  if (mb.Coil(COIL_SAVE_CFG)) {
    mb.setCoil(COIL_SAVE_CFG, false);
    if (saveConfigFS()) { wsLog("Configuration saved"); cfgDirty = false; }
    else wsLog("ERROR: Save failed");
  }

  if (mb.Coil(COIL_REBOOT)) {
    mb.setCoil(COIL_REBOOT, false);
    wsLog("Rebooting…");
    delay(50);
    rp2040.reboot();
  }

  for (int i = 0; i < NUM_LED; i++) {
    ledHaState[i] = mb.Coil(COIL_LED_HA_BASE + i);
  }

  for (int i = 0; i < NUM_DI; i++) {
    bool lk = mb.Coil(COIL_DI_LOCK_BASE + i);
    if (lk != diCfg[i].lockLocal) {
      diCfg[i].lockLocal = lk;
      touchCfgDirty();
    }
  }
}

void updateLinkOkDetector(uint32_t now) {
  if (Serial2.available() > 0) g_lastLinkSeenMs = now;
  for (int i = 0; i < 15; i++) {
    if (mb.Coil(i) != coilSnapBefore[i]) g_lastLinkSeenMs = now;
  }
  (void)now;
}

void trackDesiredRelayChanges(uint32_t now) {
  if (!outTrackInit) {
    memcpy(prevDesiredRelay, desiredRelay, sizeof(prevDesiredRelay));
    outTrackInit = true;
    return;
  }
  for (int i = 0; i < NUM_RLY; i++) {
    if (desiredRelay[i] != prevDesiredRelay[i]) {
      if (prevDesiredRelay[i] && !desiredRelay[i]) rlyLastOffMs[i] = now;
      if (!prevDesiredRelay[i] && desiredRelay[i]) armAutoOffTimer((uint8_t)i, now);
      prevDesiredRelay[i] = desiredRelay[i];
      lastOutChangeMs = now;
    }
  }
}

// ================== Setup ==================
void setup() {
  Serial.begin(57600);

  for (uint8_t i = 0; i < NUM_DI; i++) pinMode(DI_PINS[i], INPUT);
  for (uint8_t i = 0; i < NUM_RLY; i++) { pinMode(RELAY_PINS[i], OUTPUT); digitalWrite(RELAY_PINS[i], LOW); }
  for (uint8_t i = 0; i < NUM_LED; i++) { pinMode(LED_PINS[i], OUTPUT); digitalWrite(LED_PINS[i], LOW); }
  for (uint8_t i = 0; i < NUM_BTN; i++) pinMode(BTN_PINS[i], INPUT_PULLUP);

  setDefaults();

  if (!initFilesystemAndConfig()) {
    wsLog("FATAL: Filesystem/config init failed");
  }

  Serial2.setTX(TX2);
  Serial2.setRX(RX2);
  Serial2.begin(g_mb_baud);
  mb.config(g_mb_baud);
  setSlaveIdIfAvailable(mb, g_mb_address);
  mb.setAdditionalServerData("DIO430-DIDO");

  buildModbusMap();
  syncHoldingFromCfg();
  g_lastLinkSeenMs = millis();

  WebSerial.on("values", handleValues);
  WebSerial.on("Config", handleUnifiedConfig);
  WebSerial.on("command", handleCommand);

  wsLog("Boot OK (DIO-430-R1 Phase A — block Modbus map)");
  sendWebBootstrap();
  hmWatchdogArm(4000);
}

// ================== Command handler ==================
void handleCommand(JSONVar obj) {
  const char* actC = (const char*)obj["action"];
  if (!actC) { wsLog("command: missing 'action'"); return; }
  String act = String(actC);
  act.toLowerCase();

  if (act == "save") {
    if (saveConfigFS()) wsLog("Configuration saved");
    else wsLog("ERROR: Save failed");
  } else if (act == "load") {
    if (loadConfigFS()) {
      applyPowerOnOutputs();
      wsLog("Configuration loaded");
      sendWebBootstrap();
      applyModbusSettings(g_mb_address, g_mb_baud);
      syncHoldingFromCfg();
    } else wsLog("ERROR: Load failed/invalid");
  } else if (act == "factory") {
    LittleFS.remove(OUT_STATE_PATH);
    setDefaults();
    applyPowerOnOutputs();
    if (saveConfigFS()) {
      wsLog("Factory defaults restored & saved");
      sendWebBootstrap();
      applyModbusSettings(g_mb_address, g_mb_baud);
      syncHoldingFromCfg();
    } else wsLog("ERROR: Save after factory reset failed");
  } else if (act == "identify") {
    g_identifyUntilMs = millis() + IDENTIFY_MS;
    wsLog("Identify started");
  } else if (act == "reboot") {
    wsLog("Rebooting…");
    delay(50);
    rp2040.reboot();
  } else {
    wsLog(String("Unknown command: ") + actC);
  }
}

void applyModbusSettings(uint8_t addr, uint32_t baud) {
  if (g_mb_baud != baud) { Serial2.end(); Serial2.begin(baud); mb.config(baud); }
  setSlaveIdIfAvailable(mb, addr);
  g_mb_address = addr;
  g_mb_baud = baud;
}

// ================== WebSerial config handlers ==================
void handleValues(JSONVar values) {
  int addr = (int)values["mb_address"];
  int baud = (int)values["mb_baud"];
  addr = hmValidAddress(addr);
  baud = (int)hmValidBaud((uint32_t)baud);
  applyModbusSettings((uint8_t)addr, (uint32_t)baud);
  syncHoldingFromCfg();
  wsLog("Modbus configuration updated");
  sendWebStatus();
  touchCfgDirty();
}

void handleUnifiedConfig(JSONVar obj) {
  const char* t = (const char*)obj["t"];
  JSONVar list = obj["list"];
  if (!t) return;
  String type = String(t);
  bool changed = false;

  if (type == "in.enabled" || type == "inputEnable") {
    for (int i = 0; i < NUM_DI && i < list.length(); i++) diCfg[i].enabled = (bool)list[i];
    wsLog("Input Enabled list updated");
    changed = true;
  } else if (type == "in.invert" || type == "inputInvert") {
    for (int i = 0; i < NUM_DI && i < list.length(); i++) diCfg[i].inverted = (bool)list[i];
    wsLog("Input Invert list updated");
    changed = true;
  } else if (type == "in.type") {
    for (int i = 0; i < NUM_DI && i < list.length(); i++) diCfg[i].type = (uint8_t)constrain((int)list[i], 0, 1);
    wsLog("Input type list updated");
    changed = true;
  } else if (type == "in.follow") {
    for (int i = 0; i < NUM_DI && i < list.length(); i++) {
      int tgt = (int)list[i];
      diCfg[i].followTarget = (uint8_t)((tgt == 4 || tgt == 0 || (tgt >= 1 && tgt <= 3)) ? tgt : 0);
    }
    wsLog("Input follow target list updated");
    changed = true;
  } else if (type == "in.short") {
    for (int i = 0; i < NUM_DI && i < list.length(); i++) {
      JSONVar o = list[i];
      diCfg[i].shortAction = (uint8_t)constrain((int)o["action"], 0, 4);
      int tgt = (int)o["target"];
      diCfg[i].shortTarget = (uint8_t)((tgt == 4 || tgt == 0 || (tgt >= 1 && tgt <= 3)) ? tgt : 0);
    }
    wsLog("Input short action list updated");
    changed = true;
  } else if (type == "in.long") {
    for (int i = 0; i < NUM_DI && i < list.length(); i++) {
      JSONVar o = list[i];
      diCfg[i].longAction = (uint8_t)constrain((int)o["action"], 0, 4);
      int tgt = (int)o["target"];
      diCfg[i].longTarget = (uint8_t)((tgt == 4 || tgt == 0 || (tgt >= 1 && tgt <= 3)) ? tgt : 0);
    }
    wsLog("Input long action list updated");
    changed = true;
  } else if (type == "in.lock") {
    for (int i = 0; i < NUM_DI && i < list.length(); i++) diCfg[i].lockLocal = (bool)list[i];
    wsLog("Input child-lock list updated");
    changed = true;
  } else if (type == "in.maint") {
    for (int i = 0; i < NUM_DI && i < list.length(); i++) {
      int m = (int)list[i];
      diCfg[i].maintMode = (uint8_t)((m == 1) ? MAINT_FOLLOW : MAINT_TOGGLE);
    }
    wsLog("Input maintained mode list updated");
    changed = true;
  } else if (type == "in.action" || type == "inputAction") {
    for (int i = 0; i < NUM_DI && i < list.length(); i++) {
      int a = (int)list[i];
      diCfg[i].shortAction = (uint8_t)constrain(a, 0, 4);
      diCfg[i].type = IN_MOMENTARY;
    }
    wsLog("Input Action list updated (legacy)");
    changed = true;
  } else if (type == "in.target" || type == "inputTarget") {
    for (int i = 0; i < NUM_DI && i < list.length(); i++) {
      int tgt = (int)list[i];
      diCfg[i].shortTarget = (uint8_t)((tgt == 4 || tgt == 0 || (tgt >= 1 && tgt <= 3)) ? tgt : 0);
    }
    wsLog("Input Control Target list updated (legacy)");
    changed = true;
  } else if (type == "relay" || type == "relays") {
    for (int i = 0; i < NUM_RLY && i < list.length(); i++) {
      rlyCfg[i].enabled = (bool)list[i]["enabled"];
      rlyCfg[i].inverted = (bool)list[i]["inverted"];
      if (list[i].hasOwnProperty("powerOn")) rlyCfg[i].powerOn = (uint8_t)constrain((int)list[i]["powerOn"], 0, 2);
      if (list[i].hasOwnProperty("autoOffSec")) rlyCfg[i].autoOffSec = (uint16_t)constrain((int)list[i]["autoOffSec"], 0, 65535);
    }
    wsLog("Relay Configuration updated");
    changed = true;
  } else if (type == "btn" || type == "buttons") {
    for (int i = 0; i < NUM_BTN && i < list.length(); i++) {
      JSONVar o = list[i];
      if (o.hasOwnProperty("shortAction")) {
        btnCfg[i].shortAction = (uint8_t)constrain((int)o["shortAction"], 0, 4);
        btnCfg[i].shortTarget = (uint8_t)constrain((int)o["shortTarget"], 0, 4);
        btnCfg[i].longAction = (uint8_t)constrain((int)o["longAction"], 0, 4);
        btnCfg[i].longTarget = (uint8_t)constrain((int)o["longTarget"], 0, 4);
      } else {
        int a = o.hasOwnProperty("action") ? (int)o["action"] : (int)o;
        if (a >= 5 && a <= 7) {
          btnCfg[i].shortAction = ACT_TOGGLE;
          btnCfg[i].shortTarget = (uint8_t)(a - 4);
        }
      }
    }
    wsLog("Buttons Configuration updated");
    changed = true;
  } else if (type == "led" || type == "leds") {
    for (int i = 0; i < NUM_LED && i < list.length(); i++) {
      ledCfg[i].mode = (uint8_t)constrain((int)list[i]["mode"], 0, 1);
      ledCfg[i].source = (uint8_t)constrain((int)list[i]["source"], 0, 7);
      if (list[i].hasOwnProperty("invert")) ledCfg[i].inverted = (bool)list[i]["invert"];
      if (list[i].hasOwnProperty("arg")) ledCfg[i].arg = (uint8_t)constrain((int)list[i]["arg"], 0, 15);
    }
    wsLog("LEDs Configuration updated");
    changed = true;
  } else if (type == "interlock") {
    JSONVar ilk = list;
    g_interlock.enabled = ilk.hasOwnProperty("enabled") ? (bool)ilk["enabled"] : (bool)obj["enabled"];
    g_interlock.relayA = (uint8_t)constrain(ilk.hasOwnProperty("relayA") ? (int)ilk["relayA"] : (int)obj["relayA"], 0, 2);
    g_interlock.relayB = (uint8_t)constrain(ilk.hasOwnProperty("relayB") ? (int)ilk["relayB"] : (int)obj["relayB"], 0, 2);
    g_interlock.pauseMs = (uint16_t)constrain(ilk.hasOwnProperty("pauseMs") ? (int)ilk["pauseMs"] : (int)obj["pauseMs"], 0, 65535);
    wsLog("Interlock configuration updated");
    changed = true;
  } else if (type == "global") {
    JSONVar g = list;
    if (g.hasOwnProperty("longPressMs")) g_longPressMs = (uint16_t)constrain((int)g["longPressMs"], 50, 5000);
    if (g.hasOwnProperty("multiClickGapMs")) g_multiClickGapMs = (uint16_t)constrain((int)g["multiClickGapMs"], 50, 2000);
    if (g.hasOwnProperty("debounceMs")) g_debounceMs = (uint16_t)constrain((int)g["debounceMs"], 1, 500);
    if (g.hasOwnProperty("linkTimeoutMs")) g_linkTimeoutMs = (uint16_t)constrain((int)g["linkTimeoutMs"], 500, 60000);
    wsLog("Global settings updated");
    changed = true;
  } else {
    wsLog("Unknown Config type");
  }

  if (changed) {
    touchCfgDirty();
    syncHoldingFromCfg();
    sendWebCfg();
  }
}

void sendWebStatus() {
  uint32_t now = millis();
  bool linkOk = ((uint32_t)(now - g_lastLinkSeenMs) < g_linkTimeoutMs);
  JSONVar st;
  st["model"] = HM_MODEL_ID;
  st["fw"] = HM_FW;
  st["map"] = HM_MAP;
  st["addr"] = g_mb_address;
  st["baud"] = g_mb_baud;
  st["linkOk"] = linkOk ? 1 : 0;
  WebSerial.send("status", st);
}

void sendWebCfg() {
  JSONVar cfg;
  for (int i = 0; i < NUM_DI; i++) {
    cfg["in"][i]["enabled"] = diCfg[i].enabled ? 1 : 0;
    cfg["in"][i]["invert"] = diCfg[i].inverted ? 1 : 0;
    cfg["in"][i]["type"] = diCfg[i].type;
    cfg["in"][i]["followTarget"] = diCfg[i].followTarget;
    cfg["in"][i]["shortAction"] = diCfg[i].shortAction;
    cfg["in"][i]["shortTarget"] = diCfg[i].shortTarget;
    cfg["in"][i]["longAction"] = diCfg[i].longAction;
    cfg["in"][i]["longTarget"] = diCfg[i].longTarget;
    cfg["in"][i]["lockLocal"] = diCfg[i].lockLocal ? 1 : 0;
    cfg["in"][i]["maintMode"] = diCfg[i].maintMode;
  }
  for (int i = 0; i < NUM_RLY; i++) {
    cfg["relay"][i]["enabled"] = rlyCfg[i].enabled ? 1 : 0;
    cfg["relay"][i]["invert"] = rlyCfg[i].inverted ? 1 : 0;
    cfg["relay"][i]["powerOn"] = rlyCfg[i].powerOn;
    cfg["relay"][i]["autoOffSec"] = rlyCfg[i].autoOffSec;
  }
  for (int i = 0; i < NUM_BTN; i++) {
    cfg["btn"][i]["shortAction"] = btnCfg[i].shortAction;
    cfg["btn"][i]["shortTarget"] = btnCfg[i].shortTarget;
    cfg["btn"][i]["longAction"] = btnCfg[i].longAction;
    cfg["btn"][i]["longTarget"] = btnCfg[i].longTarget;
  }
  for (int i = 0; i < NUM_LED; i++) {
    cfg["led"][i]["source"] = ledCfg[i].source;
    cfg["led"][i]["mode"] = ledCfg[i].mode;
    cfg["led"][i]["invert"] = ledCfg[i].inverted ? 1 : 0;
    cfg["led"][i]["arg"] = ledCfg[i].arg;
  }
  cfg["interlock"]["enabled"] = g_interlock.enabled ? 1 : 0;
  cfg["interlock"]["relayA"] = g_interlock.relayA;
  cfg["interlock"]["relayB"] = g_interlock.relayB;
  cfg["interlock"]["pauseMs"] = g_interlock.pauseMs;
  cfg["global"]["longPressMs"] = g_longPressMs;
  cfg["global"]["multiClickGapMs"] = g_multiClickGapMs;
  cfg["global"]["debounceMs"] = g_debounceMs;
  cfg["global"]["linkTimeoutMs"] = g_linkTimeoutMs;
  WebSerial.send("cfg", cfg);
}

void sendWebBootstrap() {
  sendWebStatus();
  sendWebCfg();
}

// ================== Main loop ==================
void loop() {
  hmWatchdogFeed();
  uint32_t now = millis();

  if (now - lastBlinkToggle >= blinkPeriodMs) {
    lastBlinkToggle = now;
    blinkPhase = !blinkPhase;
  }

  for (int i = 0; i < 15; i++) coilSnapBefore[i] = mb.Coil(i);
  if (Serial2.available() > 0) g_lastLinkSeenMs = now;

  mb.task();
  updateLinkOkDetector(now);

  applyHoldingToCfg();
  processModbusCoils(now);

  // -------- Debounced DI --------
  for (int i = 0; i < NUM_DI; i++) {
    bool raw = false;
    if (diCfg[i].enabled) {
      raw = (digitalRead(DI_PINS[i]) == HIGH);
      if (diCfg[i].inverted) raw = !raw;
    }
    serviceDebounce(diDeb[i], raw, now);

    if (diCfg[i].enabled) {
      if (diCfg[i].type == IN_MAINTAINED) {
        if (diDeb[i].stable != diDeb[i].prevStable) applyMaintainedInput((uint8_t)i, diDeb[i].stable);
      } else {
        serviceMomentaryChannel(evtSourceForDi((uint8_t)i), diCfg[i].lockLocal,
                                diCfg[i].shortAction, diCfg[i].shortTarget,
                                diCfg[i].longAction, diCfg[i].longTarget,
                                diDeb[i], diClick[i], now);
      }
    }
  }

  // -------- Debounced buttons --------
  for (int i = 0; i < NUM_BTN; i++) {
    bool pressed = (digitalRead(BTN_PINS[i]) == LOW);
    serviceDebounce(btnDeb[i], pressed, now);
    serviceMomentaryChannel(evtSourceForBtn((uint8_t)i), false,
                            btnCfg[i].shortAction, btnCfg[i].shortTarget,
                            btnCfg[i].longAction, btnCfg[i].longTarget,
                            btnDeb[i], btnClick[i], now);
  }

  finalizeClickGaps(now);
  serviceAutoOff(now);
  trackDesiredRelayChanges(now);

  // -------- Relay outputs (with interlock) --------
  bool logicalRelay[NUM_RLY];
  bool interlocked[NUM_RLY];
  for (int i = 0; i < NUM_RLY; i++) {
    logicalRelay[i] = desiredRelay[i];
    if (!rlyCfg[i].enabled) logicalRelay[i] = false;
  }
  applyInterlock(logicalRelay, interlocked, now);

  for (int i = 0; i < NUM_RLY; i++) {
    bool phys = rlyCfg[i].inverted ? !interlocked[i] : interlocked[i];
    digitalWrite(RELAY_PINS[i], phys ? HIGH : LOW);
  }

  // -------- LEDs --------
  for (int i = 0; i < NUM_LED; i++) {
    bool on = computeLedActive((uint8_t)i, now);
    digitalWrite(LED_PINS[i], on ? HIGH : LOW);
    ledPhys[i] = on;
  }

  updateInputRegisters(now);
  syncCoilsFromState();
  syncHoldingFromCfg();

  if (cfgDirty && (now - lastCfgTouchMs >= CFG_AUTOSAVE_MS)) {
    if (saveConfigFS()) wsLog("Configuration saved");
    else wsLog("ERROR: Save failed");
    cfgDirty = false;
  }
  maybePersistOutputState(now);

  if (millis() - lastSend >= sendInterval) {
    lastSend = millis();
    WebSerial.check();
    if (hmUsbCanSend()) {
      sendWebStatus();

      bool logical[NUM_RLY];
      bool ilk[NUM_RLY];
      for (int i = 0; i < NUM_RLY; i++) {
        logical[i] = desiredRelay[i] && rlyCfg[i].enabled;
      }
      applyInterlock(logical, ilk, now);

      JSONVar io;
      for (int i = 0; i < NUM_DI; i++) io["in"][i] = (diCfg[i].enabled && diDeb[i].stable) ? 1 : 0;
      for (int i = 0; i < NUM_RLY; i++) io["relay"][i] = ilk[i] ? 1 : 0;
      for (int i = 0; i < NUM_BTN; i++) io["btn"][i] = btnDeb[i].stable ? 1 : 0;
      for (int i = 0; i < NUM_LED; i++) io["led"][i] = ledPhys[i] ? 1 : 0;
      WebSerial.send("io", io);
    }
  }
}
