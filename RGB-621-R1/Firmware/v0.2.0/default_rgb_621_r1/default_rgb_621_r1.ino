#include <Arduino.h>
#include <ModbusSerial.h>
#include "hm_common.h"
#define HM_MODEL_ID   7
#define HM_FW_MAJOR   0
#define HM_FW_MINOR   2
#define HM_FW_PATCH   0
#define HM_FW         "0.2.0"
#define HM_MAP        3
#define HM_MAP_VERSION 3
#include <SimpleWebSerial.h>
#include <Arduino_JSON.h>
#include <LittleFS.h>
#include <utility>
#include <string.h>
#include "hardware/watchdog.h"
#include "hm_input_engine.h"
#include "hm_pwm_output.h"

// Arduino IDE inserts function prototypes before struct definitions — forward-declare persist types.
struct PersistConfig;
struct OutputStateSnapshot;

// ================== UART2 (RS-485 / Modbus) ==================
#define TX2 4
#define RX2 5
const int TxenPin = -1;          // -1 if RS-485 TXEN not used
int SlaveId = 1;
ModbusSerial mb(Serial2, SlaveId, TxenPin);

// ================== GPIO MAP (RGB module) ==================
// Macros for preprocessor-safe conflict checks:
// SW1/GPIO0 = BOOT strap only — never polled. SW2/GPIO1 = onboard button (active-HIGH).
// Field DI1/DI2: opto/dry-contact — idle LOW (INPUT_PULLDOWN), switch close drives HIGH.
#define PIN_BTN_SW2  1
#define PIN_DI1    14
#define PIN_DI2    13
#define PIN_RELAY1 15
#define PIN_LED1   2
#define PIN_LED2   3
#define PIN_PWM_R  9
#define PIN_PWM_G  12
#define PIN_PWM_B  10
#define PIN_PWM_WW 8
#define PIN_PWM_CW 11

// Arrays built from the macros (runtime use is unchanged):
static const uint8_t DI_PINS[2]    = {PIN_DI1, PIN_DI2};        // DI1..DI2
static const uint8_t RELAY_PINS[1] = {PIN_RELAY1};              // Relay1
static const uint8_t LED_PINS[2]   = {PIN_LED1, PIN_LED2};      // LED1..LED2
static const uint8_t BTN_PINS[1]   = { PIN_BTN_SW2 };

const uint8_t PWM_PINS[NUM_PWM] = {
  PIN_PWM_R,  // 0 -> R
  PIN_PWM_G,  // 1 -> G
  PIN_PWM_B,  // 2 -> B
  PIN_PWM_WW, // 3 -> WarmWhite
  PIN_PWM_CW  // 4 -> ColdWhite
};

// ================== Sizes ==================
static const uint8_t NUM_DI   = 2;
static const uint8_t NUM_RLY  = 1;
static const uint8_t NUM_LED  = 2;
static const uint8_t NUM_BTN  = 1;
static const uint8_t NUM_IN_CH = 2;
static const uint8_t NUM_PHYS  = 3;       // DI1, DI2, SW2
static const uint8_t NUM_SCENES = 4;
static const uint8_t NUM_EVT_SRC = 3;

enum RlyMode : uint8_t { RLY_MANUAL = 0, RLY_FOLLOW = 1 };

static inline uint16_t clampDimFullRange(int v) { return (uint16_t)constrain(v, 800, 8000); }

struct RlyCfg {
  bool enabled;
  bool inverted;
  uint8_t powerOn;
  uint8_t mode;
  uint8_t watchMask;
  uint32_t offDelayMs;
};

struct SafeModeCfg {
  bool allowLocalWhenOffline;
};

struct LedCfg { uint8_t mode; uint8_t source; };

HmInputChannelCfg inChCfg[NUM_IN_CH];
HmInputChannelCfg btnChCfg[NUM_BTN];
HmInputEngineTimings inpTimings;
PwmChCfg pwmChCfg[NUM_PWM];
DimCfg dimCfg;
uint8_t scenes[NUM_SCENES][NUM_PWM];

static inline void applyDimParams(uint16_t fullRangeMs) {
  dimCfg.dimFullRangeMs = clampDimFullRange(fullRangeMs);
}

SafeModeCfg safeCfg;
RlyCfg  rlyCfg[NUM_RLY];
LedCfg  ledCfg[NUM_LED];

HmInputRuntime inpRt[NUM_PHYS];
uint16_t evtCount[NUM_EVT_SRC][HM_EVT_COUNT] = {{0}};
bool physState[NUM_PHYS] = {false};
bool diState[NUM_DI] = {false, false};
bool buttonState[NUM_BTN] = {false};
bool g_dimToggleDir[NUM_EVT_SRC] = {false};
uint32_t rlyFollowOffAt[NUM_RLY] = {0};
// Desired relay state
bool desiredRelay[NUM_RLY] = {false};

// Pulse handling for relays (when DI action=Pulse)
uint32_t rlyPulseUntil[NUM_RLY] = {0};
const uint32_t PULSE_MS = 500; // default pulse width

// PWM setpoints 0..4095 (12-bit internal target); Modbus/WebConfig use 0..255
uint16_t pwmTarget[NUM_PWM] = {0,0,0,0,0};
uint16_t pwmCurrent[NUM_PWM] = {0,0,0,0,0};
uint16_t pwmLevel[NUM_PWM] = {0,0,0,0,0}; // alias: internal hi level (= pwmTarget)
uint16_t pwmLastNonZero[NUM_PWM] = {2048,2048,2048,2048,2048};
uint32_t slewLastMs[NUM_PWM] = {0};
uint16_t pwmHoldTraverseMs[NUM_PWM] = {0};
uint8_t  holdDimChMask = 0;
uint16_t g_gammaLut[PWM_HI + 1];
OutputQualityCfg outQuality = { true, 22 };
bool     rgbGroupStored = false;
uint16_t rgbGroupStore[3] = {0,0,0};
bool     cctGroupStored = false;
uint16_t cctGroupStore[2] = {0,0};

// ================== Web Serial ==================
SimpleWebSerial WebSerial;

static inline void wsLog(const char* msg) { if (hmUsbCanSend()) WebSerial.send("log", msg); }
static inline void wsLog(const String& msg) { if (hmUsbCanSend()) WebSerial.send("log", msg); }

// ================== Timing ==================
unsigned long lastSend = 0;
const unsigned long sendInterval = 250;
unsigned long lastBlinkToggle = 0;
const unsigned long blinkPeriodMs = 400;
bool blinkPhase = false;
uint32_t g_identifyUntilMs = 0;
const uint32_t IDENTIFY_MS = 5000;

// ================== Modbus linkOk detector (DIO-compatible) ==================
static uint32_t g_lastLinkSeenMs = 0;
static const uint16_t g_linkTimeoutMs = 5000;

// ================== Persisted Modbus settings ==================
uint8_t  g_mb_address = 3;
uint32_t g_mb_baud    = 19200;

// ================== Persistence (LittleFS) ==================
struct PersistConfig {
  uint32_t magic;  uint16_t version;  uint16_t size;
  HmInputChannelCfg inCh[NUM_IN_CH];
  HmInputChannelCfg btnCh[NUM_BTN];
  HmInputEngineTimings inpTimings;
  PwmChCfg pwmCh[NUM_PWM];
  DimCfg dimCfg;
  uint8_t scenes[NUM_SCENES][NUM_PWM];
  SafeModeCfg safe;
  OutputQualityCfg outQuality;
  RlyCfg  rlyCfg[NUM_RLY];
  LedCfg  ledCfg[NUM_LED];
  uint8_t mb_address;
  uint32_t mb_baud;
  uint32_t crc32;
} __attribute__((packed));

struct OutputStateSnapshot {
  uint32_t magic; uint16_t version; uint16_t size;
  bool     desiredRelay[NUM_RLY];
  uint16_t pwmTarget[NUM_PWM];
  uint32_t crc32;
} __attribute__((packed));

static const uint32_t CFG_MAGIC   = 0x52474231UL; // '1BGR'
// Config version tied to firmware: any FW update invalidates stored config.
static const uint16_t CFG_VERSION = (uint16_t)((HM_FW_MAJOR << 12) | (HM_FW_MINOR << 4) | HM_FW_PATCH);
static const char*    CFG_PATH    = "/cfg_rgb.bin";
static const char*    OUT_STATE_PATH = "/cfg_out.bin";
static const uint32_t OUT_STATE_MAGIC = 0x484D4F53UL; // 'HMOS'
static const uint16_t OUT_STATE_VERSION = 0x0002;

volatile bool   cfgDirty        = false;
uint32_t        lastCfgTouchMs  = 0;
const uint32_t  CFG_AUTOSAVE_MS = 1500;
volatile uint32_t lastOutChangeMs = 0;
uint32_t        lastOutSaveMs   = 0;
const uint32_t  OUT_AUTOSAVE_MS = 10000;
bool            prevDesiredRelay[NUM_RLY] = {false};
uint16_t        prevPwmLevel[NUM_PWM] = {0, 0, 0, 0, 0};
bool            outTrackInit    = false;

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

// ================== Defaults / persist ==================
void setDefaults() {
  dimCfg.dimFullRangeMs = 3000;
  applyDimParams(3000);
  hmInputEngineSetDiDefaults(inChCfg, NUM_IN_CH);
  hmInputEngineSetBtnDefaults(btnChCfg[0]);
  hmInputEngineSetTimingDefaults(inpTimings);
  for (int i = 0; i < NUM_PWM; i++) pwmChCfg[i] = { 1, 255, 400, HM_PWR_OFF };
  outQuality = { true, 22 };
  for (int s = 0; s < NUM_SCENES; s++)
    for (int c = 0; c < NUM_PWM; c++) scenes[s][c] = 0;
  scenes[0][0] = 255; scenes[0][1] = 128; scenes[0][2] = 64;
  scenes[0][3] = 128; scenes[0][4] = 64;
  safeCfg.allowLocalWhenOffline = true;
  for (int i = 0; i < NUM_RLY; i++) {
    rlyCfg[i] = { true, false, HM_PWR_OFF, RLY_FOLLOW, 0x03, 45000 };
    desiredRelay[i] = false;
    rlyPulseUntil[i] = 0;
    rlyFollowOffAt[i] = 0;
  }
  for (int i = 0; i < NUM_LED; i++) ledCfg[i] = { 0, 0 };
  for (int i = 0; i < NUM_PWM; i++) {
    pwmTarget[i] = 0;
    pwmCurrent[i] = 0;
    pwmLevel[i] = 0;
    slewLastMs[i] = 0;
  }
  pwmBuildGammaLut();
  g_mb_address = 3; g_mb_baud = 19200;
}

bool readOutputStateSnapshot(bool outRelay[NUM_RLY], uint16_t outPwm[NUM_PWM]) {
  File f = LittleFS.open(OUT_STATE_PATH, "r");
  if (!f) return false;
  if ((size_t)f.size() != sizeof(OutputStateSnapshot)) { f.close(); return false; }
  OutputStateSnapshot snap{};
  size_t n = f.read((uint8_t*)&snap, sizeof(snap));
  f.close();
  if (n != sizeof(snap)) return false;
  if (snap.magic != OUT_STATE_MAGIC || snap.version != OUT_STATE_VERSION || snap.size != sizeof(OutputStateSnapshot)) return false;
  OutputStateSnapshot tmp = snap; uint32_t crc = tmp.crc32; tmp.crc32 = 0;
  if (crc32_update(0, (const uint8_t*)&tmp, sizeof(tmp)) != crc) return false;
  memcpy(outRelay, snap.desiredRelay, sizeof(snap.desiredRelay));
  memcpy(outPwm, snap.pwmTarget, sizeof(snap.pwmTarget));
  return true;
}

bool saveOutputStateSnapshot() {
  mb.task();
  OutputStateSnapshot snap{};
  snap.magic = OUT_STATE_MAGIC; snap.version = OUT_STATE_VERSION; snap.size = sizeof(OutputStateSnapshot);
  memcpy(snap.desiredRelay, desiredRelay, sizeof(desiredRelay));
  memcpy(snap.pwmTarget, pwmTarget, sizeof(pwmTarget));
  snap.crc32 = 0; snap.crc32 = crc32_update(0, (const uint8_t*)&snap, sizeof(snap));
  File f = LittleFS.open(OUT_STATE_PATH, "w");
  if (!f) return false;
  size_t n = f.write((const uint8_t*)&snap, sizeof(snap));
  f.flush(); f.close();
  mb.task();
  return n == sizeof(snap);
}

void applyPowerOnOutputs() {
  bool restoredRelay[NUM_RLY] = {false};
  uint16_t restoredPwm[NUM_PWM] = {0, 0, 0, 0, 0};
  bool haveSnap = readOutputStateSnapshot(restoredRelay, restoredPwm);
  for (int i = 0; i < NUM_RLY; i++) {
    rlyPulseUntil[i] = 0;
    if (rlyCfg[i].powerOn == HM_PWR_ON) desiredRelay[i] = true;
    else if (rlyCfg[i].powerOn == HM_PWR_RESTORE && haveSnap) desiredRelay[i] = restoredRelay[i];
    else desiredRelay[i] = false;
  }
  for (int i = 0; i < NUM_PWM; i++) {
    if (pwmChCfg[i].powerOn == HM_PWR_ON) pwmSetTargetHi((uint8_t)i, PWM_HI);
    else if (pwmChCfg[i].powerOn == HM_PWR_RESTORE && haveSnap) {
      uint16_t v = restoredPwm[i];
      if (v <= 255) v = pwmApiToHi(v); // migrate old 8-bit snapshots
      pwmSetTargetHi((uint8_t)i, v);
    } else pwmSetTargetHi((uint8_t)i, 0);
    pwmCurrent[i] = pwmTarget[i];
    pwmLevel[i] = pwmTarget[i];
    pwmWriteHardware((uint8_t)i, pwmCurrent[i]);
    slewLastMs[i] = millis();
  }
  memcpy(prevDesiredRelay, desiredRelay, sizeof(prevDesiredRelay));
  memcpy(prevPwmLevel, pwmTarget, sizeof(prevPwmLevel));
  outTrackInit = true;
  lastOutChangeMs = millis();
}

void maybePersistOutputState(uint32_t now) {
  bool needRestore = false;
  for (int i = 0; i < NUM_RLY; i++) {
    if (rlyCfg[i].powerOn == HM_PWR_RESTORE) { needRestore = true; break; }
  }
  if (!needRestore) {
    for (int i = 0; i < NUM_PWM; i++) {
      if (pwmChCfg[i].powerOn == HM_PWR_RESTORE) { needRestore = true; break; }
    }
  }
  if (!needRestore) return;
  if ((uint32_t)(now - lastOutChangeMs) < OUT_AUTOSAVE_MS) return;
  if (lastOutSaveMs && (uint32_t)(now - lastOutSaveMs) < OUT_AUTOSAVE_MS) return;
  if (saveOutputStateSnapshot()) lastOutSaveMs = now;
}

void captureToPersist(PersistConfig &pc) {
  pc.magic   = CFG_MAGIC; pc.version = CFG_VERSION; pc.size = sizeof(PersistConfig);
  memcpy(pc.inCh, inChCfg, sizeof(inChCfg));
  memcpy(pc.btnCh, btnChCfg, sizeof(btnChCfg));
  pc.inpTimings = inpTimings;
  memcpy(pc.pwmCh, pwmChCfg, sizeof(pwmChCfg));
  pc.dimCfg = dimCfg;
  memcpy(pc.scenes, scenes, sizeof(scenes));
  pc.safe = safeCfg;
  pc.outQuality = outQuality;
  memcpy(pc.rlyCfg, rlyCfg, sizeof(rlyCfg));
  memcpy(pc.ledCfg, ledCfg, sizeof(ledCfg));
  pc.mb_address = g_mb_address; pc.mb_baud = g_mb_baud;
  pc.crc32 = 0; pc.crc32 = crc32_update(0, (const uint8_t*)&pc, sizeof(PersistConfig));
}

bool applyFromPersist(const PersistConfig &pc) {
  if (pc.magic != CFG_MAGIC || pc.size != sizeof(PersistConfig)) return false;
  PersistConfig tmp = pc; uint32_t crc = tmp.crc32; tmp.crc32 = 0;
  if (crc32_update(0, (const uint8_t*)&tmp, sizeof(PersistConfig)) != crc) return false;
  if (pc.version != CFG_VERSION) return false;

  memcpy(inChCfg, pc.inCh, sizeof(inChCfg));
  memcpy(btnChCfg, pc.btnCh, sizeof(btnChCfg));
  inpTimings = pc.inpTimings;
  memcpy(pwmChCfg, pc.pwmCh, sizeof(pwmChCfg));
  dimCfg = pc.dimCfg;
  applyDimParams(dimCfg.dimFullRangeMs);
  for (int i = 0; i < NUM_IN_CH; i++) {
    hmNormalizeGestureBind(inChCfg[i].single);
    hmNormalizeGestureBind(inChCfg[i].dbl);
    hmNormalizeGestureBind(inChCfg[i].tpl);
    hmNormalizeGestureBind(inChCfg[i].hold);
    hmMigrateLegacyLongGesture(inChCfg[i]);
    if (inChCfg[i].maintTarget == 0) {
      inChCfg[i].maintTarget = (i == 0) ? HM_TGT_GRP_RGB : HM_TGT_GRP_CCT;
    }
  }
  for (int i = 0; i < NUM_BTN; i++) {
    hmNormalizeBtnChannel(btnChCfg[i]);
  }
  memcpy(scenes, pc.scenes, sizeof(scenes));
  safeCfg = pc.safe;
  outQuality = pc.outQuality;
  memcpy(rlyCfg, pc.rlyCfg, sizeof(rlyCfg));
  memcpy(ledCfg, pc.ledCfg, sizeof(ledCfg));
  g_mb_address = pc.mb_address; g_mb_baud = pc.mb_baud;
  for (int i = 0; i < NUM_IN_CH; i++) inChCfg[i].mode = hmNormalizeInMode(inChCfg[i].mode);
  for (int i = 0; i < NUM_BTN; i++) btnChCfg[i].mode = HM_IN_MOMENTARY;
  pwmBuildGammaLut();
  return true;
}

bool saveConfigFS() {
  mb.task();
  PersistConfig pc{}; captureToPersist(pc);
  File f = LittleFS.open(CFG_PATH, "w");
  if (!f) { wsLog("save: open failed"); return false; }
  size_t n = f.write((const uint8_t*)&pc, sizeof(pc));
  f.flush();
  f.close();
  if (n != sizeof(pc)) { wsLog(String("save: short write ")+n); return false; }
  // quick read-back verify
  File r = LittleFS.open(CFG_PATH, "r");
  if (!r) { wsLog("save: reopen failed"); return false; }
  if ((size_t)r.size() != sizeof(PersistConfig)) { wsLog("save: size mismatch after write"); r.close(); return false; }
  PersistConfig back{}; size_t nr = r.read((uint8_t*)&back, sizeof(back)); r.close();
  if (nr != sizeof(back)) { wsLog("save: short readback"); return false; }
  PersistConfig tmp = back; uint32_t crc = tmp.crc32; tmp.crc32 = 0;
  if (crc32_update(0, (const uint8_t*)&tmp, sizeof(tmp)) != crc) { wsLog("save: CRC verify failed"); return false; }
  mb.task();
  return true;
}
bool loadConfigFS() {
  File f = LittleFS.open(CFG_PATH, "r"); if (!f) { wsLog("load: open failed"); return false; }
  if ((size_t)f.size() != sizeof(PersistConfig)) { wsLog(String("load: size ")+f.size()+" unsupported"); f.close(); return false; }
  PersistConfig pc{}; size_t n = f.read((uint8_t*)&pc, sizeof(pc)); f.close();
  if (n != sizeof(pc)) { wsLog("load: short read"); return false; }
  if (!applyFromPersist(pc)) { wsLog("load: magic/version/crc mismatch"); return false; }
  return true;
}

// ================== SFINAE helper ==================
template <class M>
inline auto setSlaveIdIfAvailable(M& m, uint8_t id)
  -> decltype(std::declval<M&>().setSlaveId(uint8_t{}), void()) { m.setSlaveId(id); }
inline void setSlaveIdIfAvailable(...) {}

// ================== Modbus addresses ==================
// Input Registers (FC=04) — contiguous 0..4 + event counters
// IREG 6..20: 3 sources (DI1, DI2, SW2) × 5 gestures (single/double/triple/reserved/hold)
//
// Holding Registers (FC=03/06/16)
// 400-404 pwm 8-bit API (0..255), 410-414 pwm 12-bit (0..4095, same targets)
// 480 MB_ADDR, 481 MB_BAUD
// Engine configuration (inputs, gestures, dimming, trim, scenes, relay mode, gamma, etc.)
// is stored in flash and edited via USB WebConfig only; it is intentionally not exposed on Modbus.
// IREG 21..25: PWM 12-bit current (diagnostic)
// IREG 26..28: STATE readback — applied levels (pwmCurrent after slew, API 0..255) + flags
//   26: (R<<8)|G   27: (B<<8)|WW   28: (CW<<8)|flags
//   flags low byte: bit0 anyOn, bit1 rgbGroupOn, bit2 cctGroupOn, bit3 relay1
enum : uint16_t {
  IREG_DI_MASK      = 0,
  IREG_RLY_MASK     = 1,
  IREG_BTN_MASK     = 2,
  IREG_LED_MASK     = 3,
  IREG_STATUS_FLAGS = 4,
  EVT_BASE = 6,
  IREG_PWM_RAW_BASE = 21,
  IREG_STATE_BASE   = 26,
  IREG_STATE_COUNT  = 3
};

// Discrete Inputs (FC=02)
enum : uint16_t {
  ISTS_DI_BASE   = 1,   // 1..2 : IN1..IN2 (after enable+invert)
  ISTS_RLY_BASE  = 60,  // 60..60 : RELAY1 logical state
  ISTS_LED_BASE  = 90   // 90..91 : LED1..LED2 logical state
};

// Command Coils (FC=05/15; pulses)
enum : uint16_t {
  COIL_RLY_BASE   = 0,    // 0..0 : Relay1 toggle coil (DIO-compatible)
  COIL_IDENTIFY   = 5,
  COIL_SAVE_CFG   = 6,
  COIL_REBOOT     = 7,
  CMD_RLY_ON_BASE   = 200,  // 200..200 : pulse turn Relay1 ON
  CMD_RLY_OFF_BASE  = 210,  // 210..210 : pulse turn Relay1 OFF
  CMD_DI_EN_BASE    = 300,  // 300..301 : pulse ENABLE  IN1..IN2
  CMD_DI_DIS_BASE   = 320   // 320..321 : pulse DISABLE IN1..IN2
};

enum : uint16_t {
  HR_PWM_BASE = 400,
  HR_PWM_HI_BASE = 410, // 12-bit level 0..4095 per channel (alongside 8-bit HR 400–404)
  HR_MB_ADDR  = 480,
  HR_MB_BAUD  = 481
};

static inline uint16_t packGesture(const HmGestureBind& g) {
  return (uint16_t)((g.action << 8) | g.target);
}
static inline void unpackGesture(HmGestureBind& g, uint16_t v) {
  g.action = (uint8_t)(v >> 8);
  g.target = (uint8_t)(v & 0xFF);
}
static inline uint16_t packInFlags(const HmInputChannelCfg& c) {
  return (uint16_t)((c.enabled ? 1 : 0) | (c.inverted ? 2 : 0) | (c.lockLocal ? 4 : 0) | ((c.mode & 3) << 3));
}
static inline void unpackInFlags(HmInputChannelCfg& c, uint16_t f) {
  c.enabled = (f & 1) != 0;
  c.inverted = (f & 2) != 0;
  c.lockLocal = (f & 4) != 0;
  c.mode = hmNormalizeInMode((uint8_t)((f >> 3) & 3));
}


// ================== Fw decls ==================
void applyModbusSettings(uint8_t addr, uint32_t baud);
void handleValues(JSONVar values);
void handleUnifiedConfig(JSONVar obj);
void handleCommand(JSONVar obj);
void performReset();
JSONVar LedConfigListFromCfg();
void sendWebStatus();
void sendWebCfg();
void sendWebBootstrap();
void processModbusCoils(uint32_t now);
void updateLinkOkDetector(uint32_t now);
void updateInputRegisters(uint32_t now);
void syncCoilsFromState();
void hmApplyGesture(uint8_t physIdx, HmEvt evt, uint8_t action, uint8_t target, uint32_t now);
void hmApplyMaintainedEdge(uint8_t physIdx, bool level, uint32_t now);
bool hmLocalInputAllowed(bool lockLocal, bool allowOffline);
void serviceRelayFollow(uint32_t now);
void applyPwmFromHoldingRegs();
void syncPwmHregsFromTargets();
void writePwmCh(uint8_t ch, uint16_t lvlHi);

// ================== Setup ==================
void setup() {
  Serial.begin(57600);

  // GPIO directions
  for (uint8_t i=0;i<NUM_DI;i++)   pinMode(DI_PINS[i],   INPUT_PULLDOWN);
  for (uint8_t i=0;i<NUM_RLY;i++)  { pinMode(RELAY_PINS[i], OUTPUT); digitalWrite(RELAY_PINS[i], LOW); } // OFF
  for (uint8_t i=0;i<NUM_LED;i++)  { pinMode(LED_PINS[i],   OUTPUT);  digitalWrite(LED_PINS[i],   LOW); } // OFF
  for (uint8_t i=0;i<NUM_BTN;i++)  pinMode(BTN_PINS[i], INPUT);
  // PWM pins init
  for (uint8_t i=0;i<NUM_PWM;i++) {
    pinMode(PWM_PINS[i], OUTPUT);
    analogWrite(PWM_PINS[i], 0);
  }
  analogWriteResolution(12);
  pwmBuildGammaLut();

  setDefaults();

  // Guarded FS init
  if (!initFilesystemAndConfig()) {
    wsLog("FATAL: Filesystem/config init failed");
  }

  // Serial2 / Modbus
  Serial2.setTX(TX2); Serial2.setRX(RX2);
  Serial2.begin(g_mb_baud); mb.config(g_mb_baud); setSlaveIdIfAvailable(mb, g_mb_address);
  mb.setAdditionalServerData("RGB621-RGBW-CCT");

  // ==== Modbus states (discrete inputs) ====
  for (uint16_t i=0;i<NUM_DI;i++)  mb.addIsts(ISTS_DI_BASE + i);
  for (uint16_t i=0;i<NUM_RLY;i++) mb.addIsts(ISTS_RLY_BASE + i);
  for (uint16_t i=0;i<NUM_LED;i++) mb.addIsts(ISTS_LED_BASE + i);

  for (uint16_t i = 0; i < (EVT_BASE + NUM_EVT_SRC * HM_EVT_COUNT); i++) mb.addIreg(i);
  for (uint16_t i = 0; i < NUM_PWM; i++) mb.addIreg(IREG_PWM_RAW_BASE + i);
  for (uint16_t i = 0; i < IREG_STATE_COUNT; i++) mb.addIreg(IREG_STATE_BASE + i);

  // ==== Modbus relay + service + command pulses (coils) ====
  for (uint16_t i = 0; i < NUM_RLY; i++) { mb.addCoil(COIL_RLY_BASE + i); mb.setCoil(COIL_RLY_BASE + i, false); }
  mb.addCoil(COIL_IDENTIFY); mb.setCoil(COIL_IDENTIFY, false);
  mb.addCoil(COIL_SAVE_CFG); mb.setCoil(COIL_SAVE_CFG, false);
  mb.addCoil(COIL_REBOOT);   mb.setCoil(COIL_REBOOT, false);
  for (uint16_t i=0;i<NUM_RLY;i++){ mb.addCoil(CMD_RLY_ON_BASE  + i);  mb.setCoil(CMD_RLY_ON_BASE  + i, false); }
  for (uint16_t i=0;i<NUM_RLY;i++){ mb.addCoil(CMD_RLY_OFF_BASE + i);  mb.setCoil(CMD_RLY_OFF_BASE + i, false); }
  for (uint16_t i=0;i<NUM_DI;i++)  { mb.addCoil(CMD_DI_EN_BASE   + i);  mb.setCoil(CMD_DI_EN_BASE   + i, false); }
  for (uint16_t i=0;i<NUM_DI;i++)  { mb.addCoil(CMD_DI_DIS_BASE  + i);  mb.setCoil(CMD_DI_DIS_BASE  + i, false); }

  // ==== Modbus holding registers for PWM + MB settings ====
  for (uint16_t i=0;i<NUM_PWM;i++) {
    mb.addHreg(HR_PWM_BASE + i);
    mb.Hreg(HR_PWM_BASE + i, pwmHiToApi(pwmTarget[i]));
    mb.addHreg(HR_PWM_HI_BASE + i);
    mb.Hreg(HR_PWM_HI_BASE + i, pwmTarget[i]);
  }
  mb.addHreg(HR_MB_ADDR); mb.Hreg(HR_MB_ADDR, g_mb_address);
  mb.addHreg(HR_MB_BAUD); mb.Hreg(HR_MB_BAUD, (uint16_t)g_mb_baud);

  hmRegisterIdentity(mb, HM_MODEL_ID, HM_FW_MAJOR, HM_FW_MINOR, HM_FW_PATCH, HM_MAP_VERSION);

  g_lastLinkSeenMs = millis();

  WebSerial.on("values",  handleValues);
  WebSerial.on("Config",  handleUnifiedConfig);
  WebSerial.on("command", handleCommand);

  wsLog("Boot OK (RGB v0.2.0; 12-bit gamma+slew; USB-configured engine)");
  sendWebBootstrap();

  // Seed debounce state from current pin levels (no spurious edge on first loop).
  {
    uint32_t now = millis();
    for (uint8_t p = 0; p < NUM_PHYS; p++) {
      bool raw = readPhysRaw(p);
      inpRt[p].db.raw = raw;
      inpRt[p].db.stable = raw;
      inpRt[p].db.prevStable = raw;
      inpRt[p].db.lastChangeMs = now;
      physState[p] = raw;
      if (p < NUM_DI) diState[p] = raw;
      else buttonState[p - NUM_DI] = raw;
    }
  }

  // Apply restored PWM levels to outputs
  applyPwmFromHoldingRegs();
  hmWatchdogArm(4000);
}

// ================== Filesystem init ==================
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

// ================== Command handler / reset ==================
void handleCommand(JSONVar obj) {
  const char* actC = (const char*)obj["action"];
  if (!actC) { wsLog("command: missing 'action'"); return; }
  String act = String(actC); act.toLowerCase();

  if (act == "reset" || act == "reboot") {
    bool ok = saveConfigFS();
    wsLog(ok ? "Saved. Rebooting…" : "WARNING: Save verify FAILED. Rebooting anyway…");
    delay(400); performReset();
  } else if (act == "save") {
    if (saveConfigFS()) wsLog("Configuration saved"); else wsLog("ERROR: Save failed");
  } else if (act == "load") {
    if (loadConfigFS()) {
      applyPowerOnOutputs();
      syncPwmHregsFromTargets();
      wsLog("Configuration loaded");
      sendWebBootstrap();
      applyModbusSettings(g_mb_address, g_mb_baud);
    }
    else wsLog("ERROR: Load failed/invalid");
  } else if (act == "factory") {
    LittleFS.remove(OUT_STATE_PATH);
    setDefaults(); applyPowerOnOutputs();
    syncPwmHregsFromTargets();
    if (saveConfigFS()) { wsLog("Factory defaults restored & saved"); sendWebBootstrap(); applyModbusSettings(g_mb_address, g_mb_baud); }
    else wsLog("ERROR: Save after factory reset failed");
  } else if (act == "hello" || act == "getconfig") {
    sendWebBootstrap();
  } else if (act == "identify") {
    g_identifyUntilMs = millis() + IDENTIFY_MS;
    wsLog("Identify: LEDs active for 5 s");
  } else if (act == "off") {
    for (int i = 0; i < NUM_PWM; i++) writePwmCh((uint8_t)i, 0);
    wsLog("All PWM channels set to 0");
  } else {
    wsLog(String("Unknown command: ") + actC);
  }
}

void performReset() {
  if (Serial) Serial.flush(); delay(50);
  watchdog_reboot(0, 0, 0);
  while (true) { __asm__("wfi"); }
}

void applyModbusSettings(uint8_t addr, uint32_t baud) {
  if (g_mb_baud != baud) { Serial2.end(); Serial2.begin(baud); mb.config(baud); }
  setSlaveIdIfAvailable(mb, addr);
  g_mb_address = addr; g_mb_baud = baud;
  mb.Hreg(HR_MB_ADDR, g_mb_address);
  mb.Hreg(HR_MB_BAUD, (uint16_t)g_mb_baud);
}

// ================== WebSerial config handlers ==================
void handleValues(JSONVar values) {
  bool changed = false;

  if (values.hasOwnProperty("mb_address") || values.hasOwnProperty("mb_baud")) {
    int addr = values.hasOwnProperty("mb_address") ? (int)values["mb_address"] : (int)g_mb_address;
    int baud = values.hasOwnProperty("mb_baud")    ? (int)values["mb_baud"]    : (int)g_mb_baud;
    applyModbusSettings(hmValidAddress(addr), hmValidBaud(baud));
    changed = true;
  }

  // Optionally accept direct PWM payloads: {"rgb":[r,g,b],"cct":[ww,cw]} (0..255)
  if (values.hasOwnProperty("rgb")) {
    JSONVar arr = values["rgb"];
    if (arr.length() >= 3) {
      for (int i = 0; i < 3; i++) {
        uint16_t api = (uint16_t)constrain((int)arr[i], 0, 255);
        pwmSetTargetApi((uint8_t)i, api);
        pwmLevel[i] = pwmTarget[i];
        mb.Hreg(HR_PWM_BASE + i, api);
        mb.Hreg(HR_PWM_HI_BASE + i, pwmTarget[i]);
      }
      changed = true;
    }
  }
  if (values.hasOwnProperty("cct")) {
    JSONVar arr = values["cct"];
    if (arr.length() >= 2) {
      for (int j = 0; j < 2; j++) {
        uint16_t api = (uint16_t)constrain((int)arr[j], 0, 255);
        pwmSetTargetApi((uint8_t)(3 + j), api);
        pwmLevel[3 + j] = pwmTarget[3 + j];
        mb.Hreg(HR_PWM_BASE + 3 + j, api);
        mb.Hreg(HR_PWM_HI_BASE + 3 + j, pwmTarget[3 + j]);
      }
      changed = true;
    }
  }
  wsLog("Values updated");
  if (changed) { cfgDirty = true; lastCfgTouchMs = millis(); }
  sendWebStatus();
}

// Contract t: in.*, relay, led, ext.pwmPowerOn, global (+ legacy aliases)
static void applyGestureObj(HmGestureBind& g, JSONVar o) {
  if (o.hasOwnProperty("action")) {
    g.action = (uint8_t)constrain((int)o["action"], 0, 14);
    g.target = (uint8_t)constrain((int)o["target"], 0, 10);
    hmNormalizeGestureBind(g);
  }
}

void handleUnifiedConfig(JSONVar obj) {
  const char* t = (const char*)obj["t"]; JSONVar list = obj["list"]; if (!t) return;
  String type = String(t); bool changed = false;

  if (type == "in.enabled" || type == "inputEnable") {
    for (int i = 0; i < NUM_IN_CH && i < list.length(); i++) inChCfg[i].enabled = (bool)list[i];
    wsLog("Input Enabled list updated"); changed = true;

  } else if (type == "in.invert" || type == "inputInvert") {
    for (int i = 0; i < NUM_IN_CH && i < list.length(); i++) inChCfg[i].inverted = (bool)list[i];
    wsLog("Input Invert list updated"); changed = true;

  } else if (type == "in.mode") {
    for (int i = 0; i < NUM_IN_CH && i < list.length(); i++) inChCfg[i].mode = hmNormalizeInMode((uint8_t)constrain((int)list[i], 0, 2));
    wsLog("Input mode updated"); changed = true;

  } else if (type == "in.lock") {
    for (int i = 0; i < NUM_IN_CH && i < list.length(); i++) inChCfg[i].lockLocal = (bool)list[i];
    wsLog("Input child-lock updated"); changed = true;

  } else if (type == "in.mainttarget") {
    for (int i = 0; i < NUM_IN_CH && i < list.length(); i++)
      inChCfg[i].maintTarget = (uint8_t)constrain((int)list[i], 0, 10);
    wsLog("Input maintained target updated"); changed = true;

  } else if (type == "in.single") {
    for (int i = 0; i < NUM_IN_CH && i < list.length(); i++) applyGestureObj(inChCfg[i].single, list[i]);
    changed = true;
  } else if (type == "in.double") {
    for (int i = 0; i < NUM_IN_CH && i < list.length(); i++) applyGestureObj(inChCfg[i].dbl, list[i]);
    changed = true;
  } else if (type == "in.hold") {
    for (int i = 0; i < NUM_IN_CH && i < list.length(); i++) applyGestureObj(inChCfg[i].hold, list[i]);
    changed = true;

  } else if (type == "btn.enabled") {
    for (int i = 0; i < NUM_BTN && i < list.length(); i++) btnChCfg[i].enabled = (bool)list[i];
    changed = true;
  } else if (type == "btn.invert" || type == "btn.mode" || type == "btn.lock" || type == "btn.hold") {
    // SW2 is fixed momentary single+double; ignore legacy keys
  } else if (type == "btn.single") {
    for (int i = 0; i < NUM_BTN && i < list.length(); i++) applyGestureObj(btnChCfg[i].single, list[i]);
    for (int i = 0; i < NUM_BTN; i++) hmNormalizeBtnChannel(btnChCfg[i]);
    changed = true;
  } else if (type == "btn.double") {
    for (int i = 0; i < NUM_BTN && i < list.length(); i++) applyGestureObj(btnChCfg[i].dbl, list[i]);
    for (int i = 0; i < NUM_BTN; i++) hmNormalizeBtnChannel(btnChCfg[i]);
    changed = true;

  } else if (type == "global" || type == "engine") {
    JSONVar g = list;
    if (g.hasOwnProperty("debounceMs")) inpTimings.debounceMs = (uint16_t)constrain((int)g["debounceMs"], 1, 500);
    if (g.hasOwnProperty("multiClickGapMs")) inpTimings.multiClickGapMs = (uint16_t)constrain((int)g["multiClickGapMs"], 50, 2000);
    if (g.hasOwnProperty("holdDelayMs")) inpTimings.holdDelayMs = (uint16_t)constrain((int)g["holdDelayMs"], 100, 3000);
    if (g.hasOwnProperty("holdRepeatMs")) inpTimings.holdRepeatMs = (uint16_t)constrain((int)g["holdRepeatMs"], 20, 500);
    if (g.hasOwnProperty("allowLocalWhenOffline")) safeCfg.allowLocalWhenOffline = (bool)g["allowLocalWhenOffline"];
    wsLog("Engine timings updated"); changed = true;

  } else if (type == "pwm") {
    for (int i = 0; i < NUM_PWM && i < list.length(); i++) {
      if (list[i].hasOwnProperty("minTrim")) pwmChCfg[i].minTrim = (uint8_t)constrain((int)list[i]["minTrim"], 0, 254);
      if (list[i].hasOwnProperty("maxTrim")) pwmChCfg[i].maxTrim = (uint8_t)constrain((int)list[i]["maxTrim"], 1, 255);
      if (list[i].hasOwnProperty("fadeMs")) pwmChCfg[i].fadeMs = (uint16_t)constrain((int)list[i]["fadeMs"], 0, 10000);
      if (list[i].hasOwnProperty("transitionMs")) pwmChCfg[i].fadeMs = (uint16_t)constrain((int)list[i]["transitionMs"], 0, 10000);
      if (list[i].hasOwnProperty("powerOn")) pwmChCfg[i].powerOn = (uint8_t)constrain((int)list[i]["powerOn"], 0, 2);
    }
    changed = true;
  } else if (type == "dim") {
    JSONVar g = list;
    if (g.hasOwnProperty("dimFullRangeMs"))
      applyDimParams(clampDimFullRange((int)g["dimFullRangeMs"]));
    changed = true;
  } else if (type == "scenes") {
    for (int s = 0; s < NUM_SCENES && s < list.length(); s++) {
      JSONVar row = list[s];
      for (int c = 0; c < NUM_PWM && c < row.length(); c++)
        scenes[s][c] = (uint8_t)constrain((int)row[c], 0, 255);
    }
    changed = true;
  } else if (type == "safe") {
    JSONVar g = list;
    if (g.hasOwnProperty("allowLocalWhenOffline")) safeCfg.allowLocalWhenOffline = (bool)g["allowLocalWhenOffline"];
    changed = true;

  } else if (type == "output" || type == "output.gamma") {
    JSONVar g = list;
    if (g.hasOwnProperty("gammaEnable")) outQuality.gammaEnable = (bool)g["gammaEnable"];
    if (g.hasOwnProperty("gammaTenths")) outQuality.gammaTenths = (uint8_t)constrain((int)g["gammaTenths"], 10, 40);
    pwmBuildGammaLut();
    changed = true;

  } else if (type == "relay" || type == "relays") {
    for (int i = 0; i < NUM_RLY && i < list.length(); i++) {
      rlyCfg[i].enabled  = (bool)list[i]["enabled"];
      rlyCfg[i].inverted = (bool)list[i]["inverted"];
      if (list[i].hasOwnProperty("powerOn")) rlyCfg[i].powerOn = (uint8_t)constrain((int)list[i]["powerOn"], 0, 2);
      if (list[i].hasOwnProperty("mode")) rlyCfg[i].mode = (uint8_t)constrain((int)list[i]["mode"], 0, 1);
      if (list[i].hasOwnProperty("watchMask")) rlyCfg[i].watchMask = (uint8_t)constrain((int)list[i]["watchMask"], 0, 3);
      if (list[i].hasOwnProperty("offDelayMs")) rlyCfg[i].offDelayMs = (uint32_t)constrain((int)list[i]["offDelayMs"], 0, 600000);
    }
    wsLog("Relay Configuration updated"); changed = true;

  } else if (type == "ext.pwmPowerOn" || type == "pwmPowerOn") {
    for (int i = 0; i < NUM_PWM && i < list.length(); i++) pwmChCfg[i].powerOn = (uint8_t)constrain((int)list[i], 0, 2);
    wsLog("PWM Power-On list updated"); changed = true;

  } else if (type == "led" || type == "leds") {
    for (int i = 0; i < NUM_LED && i < list.length(); i++) {
      ledCfg[i].mode   = (uint8_t)constrain((int)list[i]["mode"], 0, 1);
      int src          = (int)list[i]["source"];
      ledCfg[i].source = (uint8_t)((src == 0 || src == 5) ? src : 0);
    }
    wsLog("LEDs Configuration updated"); changed = true;

  } else {
    wsLog("Unknown Config type");
  }

  if (changed) {
    cfgDirty = true; lastCfgTouchMs = millis();
    sendWebCfg();
  }
}

// ================== Modbus link / status ==================
static const uint16_t kLinkWatchCoils[] = {
  COIL_RLY_BASE,
  COIL_IDENTIFY, COIL_SAVE_CFG, COIL_REBOOT,
  CMD_RLY_ON_BASE, CMD_RLY_OFF_BASE,
  (uint16_t)(CMD_DI_EN_BASE + 0), (uint16_t)(CMD_DI_EN_BASE + 1),
  (uint16_t)(CMD_DI_DIS_BASE + 0), (uint16_t)(CMD_DI_DIS_BASE + 1),
};
static bool coilSnapBefore[sizeof(kLinkWatchCoils) / sizeof(kLinkWatchCoils[0])] = {false};

static inline bool linkOkNow(uint32_t now) {
  return ((uint32_t)(now - g_lastLinkSeenMs) < (uint32_t)g_linkTimeoutMs);
}

void updateLinkOkDetector(uint32_t now) {
  if (Serial2.available() > 0) g_lastLinkSeenMs = now;
  for (uint8_t i = 0; i < (uint8_t)(sizeof(kLinkWatchCoils) / sizeof(kLinkWatchCoils[0])); i++) {
    const uint16_t addr = kLinkWatchCoils[i];
    const bool c = mb.Coil(addr);
    if (c != coilSnapBefore[i]) {
      coilSnapBefore[i] = c;
      g_lastLinkSeenMs = now;
    }
  }
}

void updateInputRegisters(uint32_t now) {
  uint16_t diMask = 0, rlyMask = 0, btnMask = 0, ledMask = 0;
  for (int i = 0; i < NUM_DI; i++)
    if (mb.Ists(ISTS_DI_BASE + i)) diMask |= (uint16_t)(1u << i);
  for (int i = 0; i < NUM_RLY; i++)
    if (mb.Ists(ISTS_RLY_BASE + i)) rlyMask |= (uint16_t)(1u << i);
  for (int i = 0; i < NUM_BTN; i++)
    if (buttonState[i]) btnMask |= (uint16_t)(1u << i);
  for (int i = 0; i < NUM_LED; i++)
    if (mb.Ists(ISTS_LED_BASE + i)) ledMask |= (uint16_t)(1u << i);

  mb.setIreg(IREG_DI_MASK, diMask);
  mb.setIreg(IREG_RLY_MASK, rlyMask);
  mb.setIreg(IREG_BTN_MASK, btnMask);
  mb.setIreg(IREG_LED_MASK, ledMask);

  uint16_t status = 0;
  if (linkOkNow(now)) status |= (1 << 1);
  if (cfgDirty)       status |= (1 << 3);
  mb.setIreg(IREG_STATUS_FLAGS, status);

  for (uint8_t s = 0; s < NUM_EVT_SRC; s++) {
    for (uint8_t e = 0; e < HM_EVT_COUNT; e++) {
      mb.setIreg((uint16_t)(EVT_BASE + s * HM_EVT_COUNT + e), evtCount[s][e]);
    }
  }
  for (int i = 0; i < NUM_PWM; i++) {
    mb.setIreg(IREG_PWM_RAW_BASE + i, pwmCurrent[i]);
  }

  const uint8_t rApi  = pwmHiToApi(pwmCurrent[0]);
  const uint8_t gApi  = pwmHiToApi(pwmCurrent[1]);
  const uint8_t bApi  = pwmHiToApi(pwmCurrent[2]);
  const uint8_t wwApi = pwmHiToApi(pwmCurrent[3]);
  const uint8_t cwApi = pwmHiToApi(pwmCurrent[4]);
  mb.setIreg(IREG_STATE_BASE + 0, (uint16_t)((rApi << 8) | gApi));
  mb.setIreg(IREG_STATE_BASE + 1, (uint16_t)((bApi << 8) | wwApi));

  uint8_t stFlags = 0;
  for (int i = 0; i < NUM_PWM; i++) {
    if (pwmCurrent[i] > 0) { stFlags |= 0x01; break; }
  }
  if (pwmCurrent[0] > 0 || pwmCurrent[1] > 0 || pwmCurrent[2] > 0) stFlags |= 0x02;
  if (pwmCurrent[3] > 0 || pwmCurrent[4] > 0) stFlags |= 0x04;
  if (mb.Ists(ISTS_RLY_BASE)) stFlags |= 0x08;
  mb.setIreg(IREG_STATE_BASE + 2, (uint16_t)((cwApi << 8) | stFlags));
}

void syncCoilsFromState() {
  for (int i = 0; i < NUM_RLY; i++) {
    bool logical = desiredRelay[i] && rlyCfg[i].enabled;
    mb.setCoil(COIL_RLY_BASE + i, logical);
  }
}

void syncPwmHregsFromTargets() {
  for (int i = 0; i < NUM_PWM; i++) {
    mb.Hreg(HR_PWM_BASE + i, pwmHiToApi(pwmTarget[i]));
    mb.Hreg(HR_PWM_HI_BASE + i, pwmTarget[i]);
  }
}

void writePwmCh(uint8_t ch, uint16_t lvlHi) {
  if (ch >= NUM_PWM) return;
  pwmSetTargetHi(ch, lvlHi);
  pwmLevel[ch] = pwmTarget[ch];
  mb.Hreg(HR_PWM_BASE + ch, pwmHiToApi(pwmTarget[ch]));
  mb.Hreg(HR_PWM_HI_BASE + ch, pwmTarget[ch]);
}

static bool groupRgbAnyOn() {
  for (int i = 0; i < 3; i++) if (pwmTarget[i] > 0) return true;
  return false;
}

static bool groupCctAnyOn() {
  for (int i = 3; i < 5; i++) if (pwmTarget[i] > 0) return true;
  return false;
}

void toggleGroupRgb() {
  if (groupRgbAnyOn()) {
    rgbGroupStore[0] = pwmTarget[0]; rgbGroupStore[1] = pwmTarget[1]; rgbGroupStore[2] = pwmTarget[2];
    rgbGroupStored = true;
    writePwmCh(0, 0); writePwmCh(1, 0); writePwmCh(2, 0);
  } else if (rgbGroupStored) {
    writePwmCh(0, rgbGroupStore[0]); writePwmCh(1, rgbGroupStore[1]); writePwmCh(2, rgbGroupStore[2]);
  } else {
    writePwmCh(0, pwmLastNonZero[0]); writePwmCh(1, pwmLastNonZero[1]); writePwmCh(2, pwmLastNonZero[2]);
  }
}

void toggleGroupCct() {
  if (groupCctAnyOn()) {
    cctGroupStore[0] = pwmTarget[3]; cctGroupStore[1] = pwmTarget[4];
    cctGroupStored = true;
    writePwmCh(3, 0); writePwmCh(4, 0);
  } else if (cctGroupStored) {
    writePwmCh(3, cctGroupStore[0]); writePwmCh(4, cctGroupStore[1]);
  } else {
    writePwmCh(3, pwmLastNonZero[3]); writePwmCh(4, pwmLastNonZero[4]);
  }
}

static void holdDimArmChannel(uint8_t ch, uint16_t targetHi) {
  if (ch >= NUM_PWM) return;
  holdDimChMask |= (uint8_t)(1u << ch);
  pwmHoldTraverseMs[ch] = dimCfg.dimFullRangeMs;
  pwmSetTargetHi(ch, targetHi);
  slewLastMs[ch] = millis();
  pwmLevel[ch] = pwmTarget[ch];
}

static void beginGroupHoldDim(uint8_t ch0, uint8_t count, bool dimDown) {
  uint16_t mx = 0;
  for (uint8_t i = 0; i < count; i++) {
    uint8_t ch = (uint8_t)(ch0 + i);
    if (pwmTarget[ch] > mx) mx = pwmTarget[ch];
  }
  if (mx == 0 && !dimDown) {
    uint16_t defMx = 0;
    for (uint8_t i = 0; i < count; i++) {
      uint8_t ch = (uint8_t)(ch0 + i);
      if (pwmLastNonZero[ch] > defMx) defMx = pwmLastNonZero[ch];
    }
    if (defMx == 0) defMx = pwmApiToHi(128);
    for (uint8_t i = 0; i < count; i++) {
      uint8_t ch = (uint8_t)(ch0 + i);
      uint16_t v = pwmLastNonZero[ch] ? pwmLastNonZero[ch] : defMx;
      holdDimArmChannel(ch, v);
    }
    mx = 0;
    for (uint8_t i = 0; i < count; i++) {
      uint8_t ch = (uint8_t)(ch0 + i);
      if (pwmTarget[ch] > mx) mx = pwmTarget[ch];
    }
  }
  if (mx == 0 && dimDown) return;
  if (dimDown) {
    for (uint8_t i = 0; i < count; i++) holdDimArmChannel((uint8_t)(ch0 + i), 0);
    return;
  }
  const uint16_t goalMx = pwmTrimMaxHi(ch0);
  for (uint8_t i = 0; i < count; i++) {
    uint8_t ch = (uint8_t)(ch0 + i);
    uint16_t tgt = 0;
    if (pwmTarget[ch] > 0 && mx > 0)
      tgt = (uint16_t)((uint32_t)pwmTarget[ch] * (uint32_t)goalMx / (uint32_t)mx);
    holdDimArmChannel(ch, tgt);
  }
}

void hmHoldDimBegin(uint8_t physIdx, uint8_t target, bool dimDown, uint32_t now) {
  (void)physIdx;
  if (target == HM_TGT_GRP_RGB) {
    beginGroupHoldDim(HM_GRP_RGB_CH_FIRST, HM_GRP_RGB_CH_COUNT, dimDown);
  } else if (target == HM_TGT_GRP_CCT) {
    beginGroupHoldDim(HM_GRP_CCT_CH_FIRST, HM_GRP_CCT_CH_COUNT, dimDown);
  } else if (target == HM_TGT_GRP_RGBCCT) {
    beginGroupHoldDim(HM_GRP_RGB_CH_FIRST, HM_GRP_RGB_CH_COUNT, dimDown);
    beginGroupHoldDim(HM_GRP_CCT_CH_FIRST, HM_GRP_CCT_CH_COUNT, dimDown);
  } else if (target >= HM_TGT_PWM_R && target <= HM_TGT_PWM_CW) {
    uint8_t ch = (uint8_t)(target - HM_TGT_PWM_R);
    uint16_t tgt = dimDown ? 0 : pwmTrimMaxHi(ch);
    if (!dimDown && pwmTarget[ch] == 0) tgt = pwmLastNonZero[ch] ? pwmLastNonZero[ch] : pwmApiToHi(128);
    holdDimArmChannel(ch, tgt);
  }
  syncPwmHregsFromTargets();
  lastOutChangeMs = now;
  serviceRelayFollow(now);
}

void hmHoldDimEnd(uint8_t physIdx, uint32_t now) {
  (void)physIdx;
  for (uint8_t ch = 0; ch < NUM_PWM; ch++) {
    if (!(holdDimChMask & (1u << ch))) continue;
    pwmTarget[ch] = pwmCurrent[ch];
    pwmLevel[ch] = pwmTarget[ch];
    if (pwmTarget[ch] > 0) pwmLastNonZero[ch] = pwmTarget[ch];
    pwmHoldTraverseMs[ch] = 0;
  }
  holdDimChMask = 0;
  syncPwmHregsFromTargets();
  lastOutChangeMs = now;
  serviceRelayFollow(now);
}

void applyScene(uint8_t sceneIdx) {
  if (sceneIdx >= NUM_SCENES) return;
  for (int i = 0; i < NUM_PWM; i++) writePwmCh((uint8_t)i, pwmApiToHi(scenes[sceneIdx][i]));
}

void applyRelayTarget(uint8_t target, uint8_t action, uint32_t now) {
  auto doR = [&](int r) {
    if (r < 0 || r >= NUM_RLY) return;
    if (action == HM_ACT_TOGGLE) { desiredRelay[r] = !desiredRelay[r]; rlyPulseUntil[r] = 0; }
    else if (action == HM_ACT_ON) { desiredRelay[r] = true; rlyPulseUntil[r] = 0; }
    else if (action == HM_ACT_OFF) { desiredRelay[r] = false; rlyPulseUntil[r] = 0; }
    else if (action == HM_ACT_RELAY_PULSE) { desiredRelay[r] = true; rlyPulseUntil[r] = now + PULSE_MS; }
  };
  if (target == HM_TGT_RLY1) doR(0);
}

static bool groupRgbcctAnyOn() {
  return groupRgbAnyOn() || groupCctAnyOn();
}

void toggleGroupRgbcct() {
  if (groupRgbcctAnyOn()) {
    if (groupRgbAnyOn()) {
      rgbGroupStore[0] = pwmTarget[0]; rgbGroupStore[1] = pwmTarget[1]; rgbGroupStore[2] = pwmTarget[2];
      rgbGroupStored = true;
      writePwmCh(0, 0); writePwmCh(1, 0); writePwmCh(2, 0);
    }
    if (groupCctAnyOn()) {
      cctGroupStore[0] = pwmTarget[3]; cctGroupStore[1] = pwmTarget[4];
      cctGroupStored = true;
      writePwmCh(3, 0); writePwmCh(4, 0);
    }
  } else {
    if (rgbGroupStored) {
      writePwmCh(0, rgbGroupStore[0]); writePwmCh(1, rgbGroupStore[1]); writePwmCh(2, rgbGroupStore[2]);
    } else {
      writePwmCh(0, pwmLastNonZero[0]); writePwmCh(1, pwmLastNonZero[1]); writePwmCh(2, pwmLastNonZero[2]);
    }
    if (cctGroupStored) {
      writePwmCh(3, cctGroupStore[0]); writePwmCh(4, cctGroupStore[1]);
    } else {
      writePwmCh(3, pwmLastNonZero[3]); writePwmCh(4, pwmLastNonZero[4]);
    }
  }
}

void applyPwmTargetAction(uint8_t target, uint8_t action) {
  if (target == HM_TGT_GRP_RGB) {
    if (action == HM_ACT_TOGGLE) toggleGroupRgb();
    else if (action == HM_ACT_ON) { if (!groupRgbAnyOn()) toggleGroupRgb(); }
    else if (action == HM_ACT_OFF) { if (groupRgbAnyOn()) toggleGroupRgb(); }
  } else if (target == HM_TGT_GRP_CCT) {
    if (action == HM_ACT_TOGGLE) toggleGroupCct();
    else if (action == HM_ACT_ON) { if (!groupCctAnyOn()) toggleGroupCct(); }
    else if (action == HM_ACT_OFF) { if (groupCctAnyOn()) toggleGroupCct(); }
  } else if (target == HM_TGT_GRP_RGBCCT) {
    if (action == HM_ACT_TOGGLE) toggleGroupRgbcct();
    else if (action == HM_ACT_ON) { if (!groupRgbcctAnyOn()) toggleGroupRgbcct(); }
    else if (action == HM_ACT_OFF) { if (groupRgbcctAnyOn()) toggleGroupRgbcct(); }
  } else if (target >= HM_TGT_PWM_R && target <= HM_TGT_PWM_CW) {
    uint8_t ch = (uint8_t)(target - HM_TGT_PWM_R);
    if (action == HM_ACT_TOGGLE) writePwmCh(ch, pwmTarget[ch] ? 0 : pwmLastNonZero[ch]);
    else if (action == HM_ACT_ON) writePwmCh(ch, pwmLastNonZero[ch]);
    else if (action == HM_ACT_OFF) writePwmCh(ch, 0);
  }
}

bool hmLocalInputAllowed(bool lockLocal, bool allowOffline) {
  const bool linkOk = linkOkNow(millis());
  if (!linkOk) return allowOffline;  // offline: allow wall switches even if child-locked
  if (lockLocal) return false;       // online: honour child-lock
  return true;
}

void hmApplyMaintainedEdge(uint8_t physIdx, bool level, uint32_t now) {
  if (physIdx >= NUM_PHYS) return;
  const HmInputChannelCfg& cfg = physCfg(physIdx);
  hmApplyGesture(physIdx, HM_EVT_SINGLE, level ? HM_ACT_ON : HM_ACT_OFF, cfg.maintTarget, now);
}

static bool anyOutputOn() {
  for (int i = 0; i < NUM_PWM; i++) if (pwmTarget[i]) return true;
  for (int r = 0; r < NUM_RLY; r++) if (desiredRelay[r]) return true;
  return false;
}

void hmApplyGesture(uint8_t physIdx, HmEvt evt, uint8_t action, uint8_t target, uint32_t now) {
  (void)evt;
  if (action == HM_ACT_IDENTIFY) { g_identifyUntilMs = now + IDENTIFY_MS; return; }
  if (hmIsSceneAction(action)) { applyScene(hmSceneIndex(action)); serviceRelayFollow(now); return; }
  if (action == HM_ACT_NONE) return;
  if (action == HM_ACT_ALLOFF || (action == HM_ACT_OFF && target == HM_TGT_ALL)) {
    for (int i = 0; i < NUM_PWM; i++) writePwmCh((uint8_t)i, 0);
    for (int r = 0; r < NUM_RLY; r++) { desiredRelay[r] = false; rlyPulseUntil[r] = 0; }
    serviceRelayFollow(now);
    return;
  }
  if (action == HM_ACT_RELAY_PULSE) {
    applyRelayTarget(HM_TGT_RLY1, action, now);
    return;
  }
  if (target == HM_TGT_ALL) {
    if (action == HM_ACT_TOGGLE) {
      if (anyOutputOn()) hmApplyGesture(physIdx, evt, HM_ACT_ALLOFF, HM_TGT_NONE, now);
      else toggleGroupRgbcct();
    } else if (action == HM_ACT_OFF) {
      hmApplyGesture(physIdx, evt, HM_ACT_ALLOFF, HM_TGT_NONE, now);
    } else if (action == HM_ACT_ON) {
      if (!groupRgbcctAnyOn()) toggleGroupRgbcct();
      serviceRelayFollow(now);
    }
    return;
  }
  if (target == HM_TGT_NONE && action != HM_ACT_ALLOFF) return;
  if (target == HM_TGT_RLY1) {
    applyRelayTarget(target, action, now);
    return;
  }
  if (target >= HM_TGT_GRP_RGB && target <= HM_TGT_GRP_RGBCCT) {
    applyPwmTargetAction(target, action);
    serviceRelayFollow(now);
  } else if (target >= HM_TGT_PWM_R && target <= HM_TGT_PWM_CW) {
    applyPwmTargetAction(target, action);
    serviceRelayFollow(now);
  }
  (void)physIdx;
}

static bool watchedGroupOn(uint8_t watchMask) {
  if ((watchMask & 1) && groupRgbAnyOn()) return true;
  if ((watchMask & 2) && groupCctAnyOn()) return true;
  return false;
}

void serviceRelayFollow(uint32_t now) {
  for (int r = 0; r < NUM_RLY; r++) {
    if (rlyCfg[r].mode != RLY_FOLLOW) continue;
    if (watchedGroupOn(rlyCfg[r].watchMask)) {
      desiredRelay[r] = true;
      rlyFollowOffAt[r] = 0;
      rlyPulseUntil[r] = 0;
    } else if (desiredRelay[r]) {
      if (!rlyFollowOffAt[r]) rlyFollowOffAt[r] = now + rlyCfg[r].offDelayMs;
      else if (timeAfter32(now, rlyFollowOffAt[r])) {
        desiredRelay[r] = false;
        rlyFollowOffAt[r] = 0;
        rlyPulseUntil[r] = 0;
      }
    }
  }
}

// Safe per-channel outputs on link loss were removed for RGB-621-R1.

// ================== Modbus coils (relay toggle + service + legacy pulses) ==================
void processModbusCoils(uint32_t now) {
  for (int r = 0; r < NUM_RLY; r++) {
    bool c = mb.Coil(COIL_RLY_BASE + r);
    if (!rlyCfg[r].enabled) {
      desiredRelay[r] = false;
      mb.setCoil(COIL_RLY_BASE + r, false);
      continue;
    }
    if (c != desiredRelay[r]) {
      desiredRelay[r] = c;
      rlyPulseUntil[r] = 0;
    }
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
    performReset();
  }

  // Relay ON/OFF
  for (int r=0; r<NUM_RLY; r++) {
    if (mb.Coil(CMD_RLY_ON_BASE + r))  { mb.setCoil(CMD_RLY_ON_BASE + r,  false); desiredRelay[r] = true;  rlyPulseUntil[r] = 0; }
    if (mb.Coil(CMD_RLY_OFF_BASE + r)) { mb.setCoil(CMD_RLY_OFF_BASE + r, false); desiredRelay[r] = false; rlyPulseUntil[r] = 0; }
  }
  // DI enable/disable
  for (int i=0; i<NUM_DI; i++) {
    if (mb.Coil(CMD_DI_EN_BASE + i))  { mb.setCoil(CMD_DI_EN_BASE + i,  false); if (!inChCfg[i].enabled)  { inChCfg[i].enabled  = true;  cfgDirty = true; lastCfgTouchMs = millis(); } }
    if (mb.Coil(CMD_DI_DIS_BASE + i)) { mb.setCoil(CMD_DI_DIS_BASE + i, false); if ( inChCfg[i].enabled)  { inChCfg[i].enabled  = false; cfgDirty = true; lastCfgTouchMs = millis(); } }
  }
}

// ================== PWM helpers ==================
void applyPwmFromHoldingRegs() {
  for (int i = 0; i < NUM_PWM; i++) {
    uint16_t hi = (uint16_t)mb.Hreg(HR_PWM_HI_BASE + i);
    if (hi > PWM_HI) hi = PWM_HI;
    uint16_t api = (uint16_t)mb.Hreg(HR_PWM_BASE + i);
    if (api > 255) api = 255;
    pwmSetTargetHi((uint8_t)i, hi);
    pwmLevel[i] = pwmTarget[i];
    mb.Hreg(HR_PWM_BASE + i, pwmHiToApi(pwmTarget[i]));
    mb.Hreg(HR_PWM_HI_BASE + i, pwmTarget[i]);
  }
}

static void applyPwmModbusChange(uint8_t ch, bool apiChanged, bool hiChanged) {
  if (ch >= NUM_PWM) return;
  if (hiChanged) {
    uint16_t hi = (uint16_t)mb.Hreg(HR_PWM_HI_BASE + ch);
    if (hi > PWM_HI) hi = PWM_HI;
    pwmSetTargetHi(ch, hi);
  } else if (apiChanged) {
    uint16_t api = (uint16_t)mb.Hreg(HR_PWM_BASE + ch);
    if (api > 255) api = 255;
    pwmSetTargetApi(ch, api);
  }
  pwmLevel[ch] = pwmTarget[ch];
  mb.Hreg(HR_PWM_BASE + ch, pwmHiToApi(pwmTarget[ch]));
  mb.Hreg(HR_PWM_HI_BASE + ch, pwmTarget[ch]);
}

static inline const HmInputChannelCfg& physCfg(uint8_t p) {
  return hmPhysCfg(p, inChCfg, NUM_IN_CH, btnChCfg);
}

static bool readPhysRaw(uint8_t p) {
  bool v;
  if (p < NUM_DI) v = (digitalRead(DI_PINS[p]) == HIGH);
  else v = (digitalRead(BTN_PINS[p - NUM_DI]) == HIGH);
  const HmInputChannelCfg& cfg = physCfg(p);
  if (cfg.inverted) v = !v;
  return v;
}

// ================== Main loop ==================
void loop() {
  hmWatchdogFeed();
  unsigned long now = millis();

  for (uint8_t i = 0; i < (uint8_t)(sizeof(kLinkWatchCoils) / sizeof(kLinkWatchCoils[0])); i++)
    coilSnapBefore[i] = mb.Coil(kLinkWatchCoils[i]);
  if (Serial2.available() > 0) g_lastLinkSeenMs = now;

  mb.task();                     // Modbus polling
  updateLinkOkDetector(now);
  processModbusCoils(now);

  // Monitor for external Modbus writes to PWM registers
  static uint16_t prevPwm[NUM_PWM] = {0,0,0,0,0};
  static uint16_t prevPwmHi[NUM_PWM] = {0,0,0,0,0};
  bool pwmChanged = false;
  for (int i=0;i<NUM_PWM;i++) {
    uint16_t v = (uint16_t)mb.Hreg(HR_PWM_BASE+i);
    uint16_t vhi = (uint16_t)mb.Hreg(HR_PWM_HI_BASE+i);
    bool apiChg = (v != prevPwm[i]);
    bool hiChg = (vhi != prevPwmHi[i]);
    if (apiChg || hiChg) {
      prevPwm[i] = v;
      prevPwmHi[i] = vhi;
      applyPwmModbusChange((uint8_t)i, apiChg, hiChg);
      pwmChanged = true;
    }
  }
  if (pwmChanged) { applyPwmFromHoldingRegs(); serviceRelayFollow(now); }

  // Blink phase (for LED blink mode)
  if (now - lastBlinkToggle >= blinkPeriodMs) { lastBlinkToggle = now; blinkPhase = !blinkPhase; }

  // Auto-save settings after quiet period
  if (cfgDirty && (now - lastCfgTouchMs >= CFG_AUTOSAVE_MS)) {
    if (saveConfigFS()) wsLog("Configuration saved");
    else                wsLog("ERROR: Save failed");
    cfgDirty = false;
  }
  maybePersistOutputState(now);

  pwmServiceSlew(now);

  // -------- Local input engine: DI1/2 + SW2 --------
  JSONVar inputs;
  const bool allowLocal = safeCfg.allowLocalWhenOffline;
  for (uint8_t p = 0; p < NUM_PHYS; p++) {
    const HmInputChannelCfg& cfg = physCfg(p);
    bool raw = readPhysRaw(p);
    hmServiceDebounce(inpRt[p].db, raw, now, inpTimings.debounceMs);
    physState[p] = inpRt[p].db.stable;
    if (p < NUM_DI) {
      diState[p] = physState[p];
      inputs[p] = physState[p] ? 1 : 0;
      mb.setIsts(ISTS_DI_BASE + p, physState[p]);
    } else {
      buttonState[p - NUM_DI] = physState[p];
    }
    uint8_t chIdx = (p < NUM_IN_CH) ? p : 0;
    if (p < NUM_DI) {
      hmServiceMaintainedPhys(p, chIdx, cfg, inpRt[p], now);
      hmServiceMomentaryPhys(p, cfg, inpRt[p], inpTimings, allowLocal, g_dimToggleDir, evtCount, NUM_EVT_SRC, now);
    } else {
      hmServiceOnboardBtnPhys(p, cfg, inpRt[p], inpTimings, evtCount, NUM_EVT_SRC, now);
    }
    inpRt[p].db.prevStable = inpRt[p].db.stable;
  }
  hmFinalizeClickGaps(inpRt, NUM_PHYS, inChCfg, NUM_IN_CH, btnChCfg, NUM_BTN, inpTimings, evtCount, now);
  serviceRelayFollow(now);

  // -------- Relays: drive outputs from desiredRelay + relay config ----------
  if (!outTrackInit) {
    memcpy(prevDesiredRelay, desiredRelay, sizeof(prevDesiredRelay));
    memcpy(prevPwmLevel, pwmTarget, sizeof(prevPwmLevel));
    outTrackInit = true;
  } else {
    for (int i = 0; i < NUM_RLY; i++) {
      if (desiredRelay[i] != prevDesiredRelay[i]) {
        prevDesiredRelay[i] = desiredRelay[i];
        lastOutChangeMs = now;
      }
    }
    for (int i = 0; i < NUM_PWM; i++) {
      if (pwmTarget[i] != prevPwmLevel[i]) {
        prevPwmLevel[i] = pwmTarget[i];
        lastOutChangeMs = now;
      }
    }
  }

  JSONVar relayStateList;
  for (int i = 0; i < NUM_RLY; i++) {
    bool outVal = desiredRelay[i];
    if (!rlyCfg[i].enabled) outVal = false;
    if (rlyCfg[i].inverted) outVal = !outVal;

    digitalWrite(RELAY_PINS[i], outVal ? HIGH : LOW);

    relayStateList[i] = outVal;
    mb.setIsts(ISTS_RLY_BASE + i, outVal);
  }

  // -------- LEDs: follow selected source; blink if mode=1 ----------
  JSONVar LedStateList;
  for (int i = 0; i < NUM_LED; i++) {
    bool phys;
    const bool identifying = g_identifyUntilMs && !timeAfter32(now, g_identifyUntilMs);
    if (identifying) {
      phys = blinkPhase;
    } else {
      bool srcActive = false;
      uint8_t src = ledCfg[i].source;            // 0=None, 5.. -> relays
      if (src >= 5 && src < (5+NUM_RLY)) {
        int r = src - 5;                         // 0..
        bool relLogical = (r >=0 && r < NUM_RLY) ? (bool)relayStateList[r] : false; // logical relay (after cfg)
        srcActive = relLogical;
      }
      phys = (ledCfg[i].mode == 0) ? srcActive : (srcActive && blinkPhase);
    }
    LedStateList[i] = phys;
    digitalWrite(LED_PINS[i], phys ? HIGH : LOW);
    mb.setIsts(ISTS_LED_BASE + i, phys);
  }

  syncCoilsFromState();
  updateInputRegisters(now);

  // -------- WebSerial UI updates --------
  if (millis() - lastSend >= sendInterval) {
    lastSend = millis();
    WebSerial.check();
    if (hmUsbCanSend()) {
      sendWebStatus();

      JSONVar io;
      for (int i = 0; i < NUM_DI; i++) io["in"][i] = inputs[i] ? 1 : 0;
      for (int i = 0; i < NUM_RLY; i++) io["relay"][i] = relayStateList[i] ? 1 : 0;
      for (int i = 0; i < NUM_BTN; i++) io["btn"][i] = buttonState[i] ? 1 : 0;
      for (int i = 0; i < NUM_LED; i++) io["led"][i] = LedStateList[i] ? 1 : 0;
      WebSerial.send("io", io);

      JSONVar ext;
      for (int i = 0; i < NUM_PWM; i++) ext["pwm"][i] = (int)pwmHiToApi(pwmTarget[i]);
      for (int i = 0; i < NUM_PWM; i++) ext["pwmRaw"][i] = (int)pwmCurrent[i];
      WebSerial.send("ext", ext);
    }
  }
  mb.task();
}

// ================== helpers ==================
JSONVar LedConfigListFromCfg() {
  JSONVar arr;
  for (int i = 0; i < NUM_LED; i++) {
    JSONVar o;
    o["mode"]   = ledCfg[i].mode;                     // 0 steady, 1 blink
    o["source"] = ledCfg[i].source;                   // 0=None, 5..=Overridden relay n
    arr[i] = o;
  }
  return arr;
}

void sendWebCfg() {
  if (!hmUsbCanSend()) return;
  JSONVar cfg;
  for (int i = 0; i < NUM_IN_CH; i++) {
    cfg["in"][i]["enabled"] = inChCfg[i].enabled ? 1 : 0;
    cfg["in"][i]["invert"]  = inChCfg[i].inverted ? 1 : 0;
    cfg["in"][i]["mode"]    = hmNormalizeInMode(inChCfg[i].mode);
    cfg["in"][i]["lockLocal"] = inChCfg[i].lockLocal ? 1 : 0;
    cfg["in"][i]["maintTarget"] = inChCfg[i].maintTarget;
    cfg["in"][i]["single"]["action"] = inChCfg[i].single.action;
    cfg["in"][i]["single"]["target"] = inChCfg[i].single.target;
    cfg["in"][i]["double"]["action"] = inChCfg[i].dbl.action;
    cfg["in"][i]["double"]["target"] = inChCfg[i].dbl.target;
    cfg["in"][i]["hold"]["action"] = inChCfg[i].hold.action;
    cfg["in"][i]["hold"]["target"] = inChCfg[i].hold.target;
  }
  for (int i = 0; i < NUM_BTN; i++) {
    cfg["btn"][i]["enabled"] = btnChCfg[i].enabled ? 1 : 0;
    cfg["btn"][i]["single"]["action"] = btnChCfg[i].single.action;
    cfg["btn"][i]["single"]["target"] = btnChCfg[i].single.target;
    cfg["btn"][i]["double"]["action"] = btnChCfg[i].dbl.action;
    cfg["btn"][i]["double"]["target"] = btnChCfg[i].dbl.target;
  }
  cfg["global"]["debounceMs"] = inpTimings.debounceMs;
  cfg["global"]["multiClickGapMs"] = inpTimings.multiClickGapMs;
  cfg["global"]["holdDelayMs"] = inpTimings.holdDelayMs;
  cfg["global"]["holdRepeatMs"] = inpTimings.holdRepeatMs;
  cfg["global"]["allowLocalWhenOffline"] = safeCfg.allowLocalWhenOffline ? 1 : 0;
  cfg["dim"]["dimFullRangeMs"] = dimCfg.dimFullRangeMs;
  cfg["safe"]["allowLocalWhenOffline"] = safeCfg.allowLocalWhenOffline ? 1 : 0;
  for (int i = 0; i < NUM_RLY; i++) {
    cfg["relay"][i]["enabled"] = rlyCfg[i].enabled ? 1 : 0;
    cfg["relay"][i]["invert"]  = rlyCfg[i].inverted ? 1 : 0;
    cfg["relay"][i]["powerOn"] = rlyCfg[i].powerOn;
    cfg["relay"][i]["mode"] = rlyCfg[i].mode;
    cfg["relay"][i]["watchMask"] = rlyCfg[i].watchMask;
    cfg["relay"][i]["offDelayMs"] = (int)rlyCfg[i].offDelayMs;
  }
  JSONVar ledList = LedConfigListFromCfg();
  for (int i = 0; i < NUM_LED; i++) {
    cfg["led"][i]["mode"]   = ledList[i]["mode"];
    cfg["led"][i]["source"] = ledList[i]["source"];
  }
  cfg["output"]["gammaEnable"] = outQuality.gammaEnable ? 1 : 0;
  cfg["output"]["gammaTenths"] = outQuality.gammaTenths;
  for (int i = 0; i < NUM_PWM; i++) {
    cfg["pwm"][i]["minTrim"] = pwmChCfg[i].minTrim;
    cfg["pwm"][i]["maxTrim"] = pwmChCfg[i].maxTrim;
    cfg["pwm"][i]["transitionMs"] = pwmChCfg[i].fadeMs;
    cfg["pwm"][i]["fadeMs"] = pwmChCfg[i].fadeMs;
    cfg["pwm"][i]["powerOn"] = pwmChCfg[i].powerOn;
    cfg["ext"]["pwmPowerOn"][i] = pwmChCfg[i].powerOn;
  }
  for (int s = 0; s < NUM_SCENES; s++)
    for (int c = 0; c < NUM_PWM; c++) cfg["scenes"][s][c] = scenes[s][c];
  for (uint8_t s = 0; s < NUM_EVT_SRC; s++) {
    for (uint8_t e = 0; e < HM_EVT_COUNT; e++) cfg["ext"]["evt"][s][e] = evtCount[s][e];
  }
  WebSerial.send("cfg", cfg);
}

void sendWebStatus() {
  if (!hmUsbCanSend()) return;

  const uint32_t now = millis();
  const bool linkOk = linkOkNow(now);
  JSONVar st;
  st["model"] = HM_MODEL_ID;
  st["fw"]    = HM_FW;
  st["map"]   = HM_MAP;
  st["addr"]  = g_mb_address;
  st["baud"]  = g_mb_baud;
  st["linkOk"] = linkOk ? 1 : 0;
  WebSerial.send("status", st);
}

void sendWebBootstrap() {
  sendWebStatus();
  sendWebCfg();
}
