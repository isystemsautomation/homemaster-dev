#include <Arduino.h>
#include <ModbusSerial.h>
#include "hm_common.h"
#define HM_MODEL_ID   2
#define HM_FW_MAJOR   0
#define HM_FW_MINOR   2
#define HM_FW_PATCH   0
#define HM_FW         "0.2.0"
#define HM_MAP        2
#define HM_MAP_VERSION 2
#include <SimpleWebSerial.h>
#include <Arduino_JSON.h>
#include <LittleFS.h>
#include <cstring>
#include <math.h>

#include "atm90e32.h"

// LittleFS blobs — must be before any function using them (Arduino inserts prototypes at top).
struct EnmSettingsCfgV20 {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  uint8_t  mb_address;
  uint32_t mb_baud;
  uint16_t lineHz;
  uint8_t  sumAbs;
  struct { bool enabled; bool inverted; } rlyCfg[2];
  struct { uint8_t mode; uint8_t source; } ledCfg[4];
  struct { uint8_t action; } btnCfg[4];
  uint32_t crc32;
} __attribute__((packed));

struct AlarmRuleCfg {
  uint8_t enabled;
  uint8_t metric;
  int32_t minVal;
  int32_t maxVal;
} __attribute__((packed));

struct AlarmChCfg {
  uint8_t ackRequired;
  uint8_t pad;
  AlarmRuleCfg rules[3];
} __attribute__((packed));

struct EnmSettingsCfgV21 {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  uint8_t  mb_address;
  uint32_t mb_baud;
  uint16_t lineHz;
  uint8_t  sumAbs;
  struct {
    bool    enabled;
    bool    inverted;
    uint8_t mode;
    uint8_t alarmCh;
    uint8_t alarmMask;
    uint8_t pad;
  } rlyCfg[2];
  struct { uint8_t mode; uint8_t source; } ledCfg[4];
  struct { uint8_t action; } btnCfg[4];
  AlarmChCfg alarm[4];
  uint32_t crc32;
} __attribute__((packed));

struct EnmSettingsCfg {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  uint8_t  mb_address;
  uint32_t mb_baud;
  uint16_t lineHz;
  uint8_t  sumAbs;
  uint8_t  wireMode;     // 0=3P4W, 1=3P3W
  uint8_t  phaseMap[3];  // logical L1..L3 -> physical phase 0..2 (A/B/C)
  struct {
    bool    enabled;
    bool    inverted;
    uint8_t mode;
    uint8_t alarmCh;
    uint8_t alarmMask;
    uint8_t pad;
  } rlyCfg[2];
  struct { uint8_t mode; uint8_t source; } ledCfg[4];
  struct { uint8_t action; } btnCfg[4];
  AlarmChCfg alarm[4];
  uint32_t crc32;
} __attribute__((packed));

struct EnmMeterCfg {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  uint16_t ucal;
  uint16_t Ugain[3];
  uint16_t Igain[3];
  int16_t  Uoffset[3];
  int16_t  Ioffset[3];
  uint64_t ap_cnt[4];
  uint64_t an_cnt[4];
  uint64_t rp_cnt[4];
  uint64_t rn_cnt[4];
  uint64_t s_cnt[4];
  uint64_t aph_cnt[4];
  uint32_t crc32;
} __attribute__((packed));

struct EnmMeterCfgV1 {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  uint16_t ucal;
  uint16_t Ugain[3];
  uint16_t Igain[3];
  int16_t  Uoffset[3];
  int16_t  Ioffset[3];
  uint64_t ap_cnt[4];
  uint64_t an_cnt[4];
  uint64_t rp_cnt[4];
  uint64_t rn_cnt[4];
  uint64_t s_cnt[4];
  uint32_t crc32;
} __attribute__((packed));

// Legacy monolithic blob (v0.2.0 beta / v0.1.0) — migrated on first boot.
struct EnmPersistCfgLegacy {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  uint8_t  mb_address;
  uint32_t mb_baud;
  uint16_t lineHz;
  uint8_t  sumAbs;
  uint16_t ucal;
  uint16_t Ugain[3];
  uint16_t Igain[3];
  int16_t  Uoffset[3];
  int16_t  Ioffset[3];
  uint64_t ap_cnt[4];
  uint64_t an_cnt[4];
  uint64_t rp_cnt[4];
  uint64_t rn_cnt[4];
  uint64_t s_cnt[4];
  uint32_t crc32;
} __attribute__((packed));

static const uint32_t CFG_MAGIC       = 0x334D4E45UL;  // 'ENM3'
static const uint16_t CFG_VERSION_V20 = 0x0020;
static const uint16_t CFG_VERSION_V21 = 0x0021;
static const uint16_t CFG_VERSION_V22 = 0x0022;
static const uint16_t CFG_VERSION     = 0x0023;
static const char*    CFG_PATH        = "/enm_cfg.bin";
static const uint32_t METER_MAGIC     = 0x4D4D4E45UL;  // 'ENMM'
static const uint16_t METER_VERSION   = 0x0002;
static const char*    METER_PATH      = "/enm_meter.bin";
static const uint16_t LEGACY_CFG_VER  = 0x0001;
static volatile bool  cfgDirty        = false;
static volatile bool  meterDirty      = false;
static unsigned long  lastCfgTouchMs  = 0;
static unsigned long  lastMeterTouchMs = 0;
static const uint32_t CFG_AUTOSAVE_MS = 1500;
// Persist energy/cal to LittleFS at most every 5 min (poll stays at energySampleMs).
static const uint32_t METER_SAVE_INTERVAL_MS = 300000;
static uint32_t       meterSavedEnergyCrc = 0;

// Types/globals used in function signatures — must be before any function (Arduino auto-prototypes).
struct DebounceState {
  bool     raw = false;
  bool     stable = false;
  bool     prevStable = false;
  uint32_t lastChangeMs = 0;
};

enum : uint8_t {
  ALARM_KIND_ALARM   = 0,
  ALARM_KIND_WARNING = 1,
  ALARM_KIND_EVENT   = 2,
  ALARM_MET_URMS     = 0,
  ALARM_MET_IRMS     = 1,
  ALARM_MET_P        = 2,
  ALARM_MET_Q        = 3,
  ALARM_MET_S        = 4,
  ALARM_MET_FREQ     = 5,
  RLY_MODE_NONE      = 0,
  RLY_MODE_MODBUS    = 1,
  RLY_MODE_ALARM     = 2,
  CHIP_EV_SAG        = 1,
  CHIP_EV_OV         = 2,
  CHIP_EV_PHASE_LOSS = 4,
  CHIP_EV_OVER_I     = 8,
  CHIP_EV_FREQ       = 16,
  CHIP_EV_REV_PHASE  = 32
};

struct AlarmRuleRun {
  bool active;
  bool hiSide;
};

struct AlarmChRun {
  AlarmRuleRun rules[3];
  bool acked;
  bool chipActive;
};

static bool meter_job = false;

// SimpleWebSerial: MaximumNumberOfEvents = 8 (library default). Never register more than 8 handlers.
static SimpleWebSerial WebSerial;
static inline void wsLog(const char* msg) { WebSerial.send("log", msg); }
static inline void wsLog(const String& msg) { WebSerial.send("log", msg); }

// ================== Modbus linkOk detector (DIO-compatible) ==================
static uint32_t g_lastLinkSeenMs = 0;
static const uint16_t g_linkTimeoutMs = 5000;
static bool coilSnapBefore[32] = {false};

// Arduino_JSON: (int)obj["key"] returns ASCII of first key char, not the numeric value.
static inline String jsonBlob(const JSONVar& v) {
  return JSON.stringify((JSONVar&)v);
}
static int parseKeyFromBlob(const String& blob, const char* key, int fallback) {
  if (blob.length() == 0 || blob == "null" || blob == "undefined") return fallback;
  String pat = String("\"") + key + "\":";
  int pos = blob.indexOf(pat);
  if (pos < 0) {
    pat = String(key) + ":";
    pos = blob.indexOf(pat);
    if (pos < 0) return fallback;
    pos += pat.length();
  } else {
    pos += pat.length();
  }
  while (pos < (int)blob.length() && (blob[pos] == ' ' || blob[pos] == '\t')) pos++;
  if (pos >= (int)blob.length()) return fallback;
  int end = pos;
  if (blob[end] == '"') {
    end++;
    while (end < (int)blob.length() && blob[end] != '"') end++;
  } else {
    while (end < (int)blob.length() && blob[end] != ',' && blob[end] != '}' && blob[end] != ']') end++;
  }
  return blob.substring(pos, end).toInt();
}

static int jvGetInt(const JSONVar& obj, const char* key, int fallback) {
  if (!obj.hasOwnProperty(key)) return fallback;
  return parseKeyFromBlob(jsonBlob(obj), key, fallback);
}

static double jvGetDouble(const JSONVar& obj, const char* key, double fallback) {
  if (!obj.hasOwnProperty(key)) return fallback;
  const String s = JSON.stringify(obj[key]);
  if (s.length() == 0 || s == "null" || s == "undefined" || s == "\"\"") return fallback;
  return s.toDouble();
}

// JSON.stringify(value) for scalars only; avoids (int)obj["key"] bug
static int jsonVarToInt(const JSONVar& v, int fallback) {
  if (JSON.typeof(v) == "undefined") return fallback;
  const String s = JSON.stringify(v);
  if (s.length() == 0 || s == "null" || s == "undefined") return fallback;
  return s.toInt();
}

#define TX2 4
#define RX2 5
static const int TxenPin = -1;
static int SlaveId = 30;
static ModbusSerial mb(Serial2, SlaveId, TxenPin);

static inline void setSlaveIdIfAvailable(ModbusSerial& m, uint8_t id) {
  m.setSlaveId(id);
}

static const uint8_t RELAY_PINS[2] = {0, 1};
static const uint8_t LED_PINS[4]   = {18, 19, 20, 21};
static const uint8_t BTN_PINS[4]   = {22, 23, 24, 25};

static const uint8_t NUM_RLY = 2;
static const uint8_t NUM_LED = 4;
static const uint8_t NUM_BTN = 4;

struct RlyCfg {
  bool    enabled;
  bool    inverted;
  uint8_t mode;
  uint8_t alarmCh;
  uint8_t alarmMask;
};
struct LedCfg { uint8_t mode; uint8_t source; };
struct BtnCfg { uint8_t action; };

static RlyCfg rlyCfg[NUM_RLY];
static LedCfg ledCfg[NUM_LED];
static BtnCfg btnCfg[NUM_BTN];

static bool buttonState[NUM_BTN]  = {false,false,false,false};

static DebounceState btnDeb[NUM_BTN];
static const uint32_t g_debounceMs = 25;

static bool desiredRelay[NUM_RLY] = {false,false};

static unsigned long lastWebFrameMs = 0;
static const unsigned long webFrameIntervalMs = 200;
static uint8_t webFramePhase = 0;
static unsigned long lastMeterWebMs = 0;
static const unsigned long meterWebIntervalMs = 1000;
static unsigned long lastBootstrap = 0;
static const unsigned long bootstrapInterval = 10000;
static bool webHostConnected = false;

static void sendWebCfgCore();
static void sendWebExt();
static void resetEnergyCounters();
static void sendWebIo(const bool* relayLogical, const bool* buttonState, const bool* ledPhysState);
static void serviceWebTelemetry(unsigned long now, const bool* relayLogical, const bool* buttonState, const bool* ledPhysState);
static void serviceMeterWeb(unsigned long now);

// Web Serial does not assert USB-CDC DTR. Do not gate sends on (bool)Serial or
// availableForWrite thresholds — v0.1.0 sends unconditionally; blocking here
// prevented all telemetry with passive WebConfig hosts.
void sendWebBootstrap();
static void markWebHostRx() {
  const bool fresh = !webHostConnected;
  webHostConnected = true;
  if (fresh) sendWebBootstrap();
}

static unsigned long lastBlinkToggle = 0;
static const unsigned long blinkPeriodMs = 400;
static bool blinkPhase = false;
static uint32_t g_identifyUntilMs = 0;
static const uint32_t IDENTIFY_MS = 5000;

static uint8_t  g_mb_address = 30;
static uint32_t g_mb_baud    = 19200;
static bool     g_mbSerialReady = false;

static bool isAllowedBaud(uint32_t b) {
  return b == 9600 || b == 19200 || b == 38400 || b == 57600 || b == 115200;
}

static const uint8_t ATM_SCK  = 10;
static const uint8_t ATM_MOSI = 11;
static const uint8_t ATM_MISO = 12;
static const uint8_t ATM_CS   = 13;
static const uint8_t ATM_PM1  = 2;
static const uint8_t ATM_PM0  = 3;

static ATM90E32 g_atm(SPI1, ATM_CS, ATM_PM0, ATM_PM1, 200000, SPI_MODE0, false);

struct AtmCfg {
  uint16_t lineHz;
  uint8_t  sumAbs;
  uint8_t  wireMode;
  uint8_t  phaseMap[3];
  uint16_t ucal;
  M90PhaseCal cal[3];
};
static AtmCfg g_atm_cfg;

static bool validatePhaseMap(const uint8_t map[3]) {
  bool used[3] = {false, false, false};
  for (int i = 0; i < 3; i++) {
    if (map[i] > 2) return false;
    if (used[map[i]]) return false;
    used[map[i]] = true;
  }
  return true;
}

static void normalizePhaseMap(uint8_t map[3]) {
  if (!validatePhaseMap(map)) {
    map[0] = 0;
    map[1] = 1;
    map[2] = 2;
  }
}

static volatile bool atmApplyPending = false;
static volatile bool atmCalOnlyPending = false;
static unsigned long atmLastApplyMs = 0;
static const unsigned long atmApplyMinIntervalMs = 300;
static bool atmBusy = false;


// Modbus V2 map (matches default_enm_223_r1_plc.yaml)
// IR 105–128: fundamental/harmonic power + harmonic active energy (map v2)
enum : uint16_t {
  COIL_RELAY1 = 0,
  COIL_RELAY2 = 1,
  // Service coils (DIO-style)
  COIL_IDENTIFY     = 5,
  COIL_SAVE_CFG     = 6,
  COIL_REBOOT       = 7,
  COIL_RESET_ENERGY = 8,
  COIL_ACK_BASE = 16,
  DI_LED_BASE   = 0,
  DI_BTN_BASE   = 4,
  DI_RELAY_BASE = 8,
  DI_ALARM_BASE = 16,
  IR_URMS_BASE  = 0,
  IR_IRMS_BASE  = 3,
  IR_FREQ       = 6,
  IR_TEMP       = 7,
  IR_PF_BASE    = 8,
  IR_UPEAK_BASE = 12,  // U16 ×0.01 V, L1..L3
  IR_IPEAK_BASE = 15,  // U16 ×0.001 A, L1..L3
  IR_IRMSN      = 18,  // U16 ×0.001 A
  IR_P_BASE     = 20,
  IR_Q_BASE     = 28,
  IR_S_BASE     = 36,
  IR_ANG_BASE   = 44,
  IR_THD_BASE   = 47,  // U16 ×0.01 %, L1..L3 (active-power THD)
  IR_E_BASE     = 60,
  IR_PFUND_BASE = 105, // S32 W, L1/L2/L3/Total (ATM D0/D1 + E0/E1…)
  IR_PHARM_BASE = 113, // S32 W, L1/L2/L3/Total (ATM D4/D5 + E4/E5…)
  IR_EHARM_AP_BASE = 121, // U32 Wh, harmonic active import L1/L2/L3/Total (ATM A8–AB)
  // New: status flags (bit1=linkOk, bit3=cfgDirty) — placed after energy block, does not shift existing IR map.
  IR_STATUS_FLAGS = 100,
  // Chip PQ event masks (U16 bitfield): 101=L1, 102=L2, 103=L3, 104=Total
  IR_CHIP_EV_L1   = 101,
  IR_CHIP_EV_L2   = 102,
  IR_CHIP_EV_L3   = 103,
  IR_CHIP_EV_TOT  = 104,
  HR_MB_ADDR    = 0,
  HR_MB_BAUD_L  = 1,
  HR_LINE_HZ    = 4,
  HR_SUM_ABS    = 5,
  HR_RLY_EN_BASE = 7,
  // HR 2,3,6 reserved — read-only, not applied from Modbus
};

static uint32_t g_e_ap_Wh[4] = {0}, g_e_an_Wh[4] = {0}, g_e_rp_varh[4] = {0};
static uint32_t g_e_rn_varh[4] = {0}, g_e_s_VAh[4] = {0};
static uint32_t g_e_aph_Wh[4] = {0};
static uint64_t g_ap_cnt[4] = {0}, g_an_cnt[4] = {0}, g_rp_cnt[4] = {0};
static uint64_t g_rn_cnt[4] = {0}, g_s_cnt[4]  = {0};
static uint64_t g_aph_cnt[4] = {0};
static uint32_t g_MC_imp_per_kWh = 3200;

static bool mbMapBuilt = false;

static void mbBuildRegisterMap() {
  if (mbMapBuilt) return;
  mbMapBuilt = true;
  for (uint16_t a = 0; a < 32; ++a) {
    mb.addCoil(a);
    mb.Coil(a, false);
  }
  for (uint16_t a = 0; a < 128; ++a) mb.addIsts(a);
  for (uint16_t r = 0; r < 160; ++r) mb.addIreg(r);
  for (uint16_t r = 0; r < 200; ++r) mb.addHreg(r);
}

static inline void mbPutU32Ir(uint16_t reg, uint32_t v) {
  mb.Ireg(reg + 0, (uint16_t)((v >> 16) & 0xFFFF));
  mb.Ireg(reg + 1, (uint16_t)(v & 0xFFFF));
}
static inline void mbPutS32Ir(uint16_t reg, int32_t v) {
  mbPutU32Ir(reg, (uint32_t)v);
}
static inline void mbPutU32Hr(uint16_t reg, uint32_t v) {
  mb.Hreg(reg + 0, (uint16_t)((v >> 16) & 0xFFFF));
  mb.Hreg(reg + 1, (uint16_t)(v & 0xFFFF));
}

static void mbSyncHolding() {
  mb.Hreg(HR_MB_ADDR, g_mb_address);
  mbPutU32Hr(HR_MB_BAUD_L, g_mb_baud);
  mb.Hreg(HR_LINE_HZ, g_atm_cfg.lineHz);
  mb.Hreg(HR_SUM_ABS, g_atm_cfg.sumAbs);
  mb.Hreg(HR_RLY_EN_BASE + 0, rlyCfg[0].enabled ? 1 : 0);
  mb.Hreg(HR_RLY_EN_BASE + 1, rlyCfg[1].enabled ? 1 : 0);
}

static void serviceDebounce(DebounceState& st, bool raw, uint32_t now) {
  if (raw != st.raw) {
    st.raw = raw;
    st.lastChangeMs = now;
  }
  if ((uint32_t)(now - st.lastChangeMs) >= g_debounceMs) {
    st.prevStable = st.stable;
    st.stable = st.raw;
  }
}

static void serviceModbusRelays() {
  for (int i = 0; i < NUM_RLY; i++) {
    if (rlyCfg[i].mode != RLY_MODE_MODBUS) continue;
    const bool c = mb.Coil((uint16_t)i);
    if (c != desiredRelay[i]) desiredRelay[i] = c;
  }
  if (mb.Coil(COIL_RELAY1) != desiredRelay[0]) mb.Coil(COIL_RELAY1, desiredRelay[0]);
  if (mb.Coil(COIL_RELAY2) != desiredRelay[1]) mb.Coil(COIL_RELAY2, desiredRelay[1]);
}

static void serviceModbusServiceCoils(uint32_t now) {
  if (mb.Coil(COIL_IDENTIFY)) {
    mb.Coil(COIL_IDENTIFY, false);
    g_identifyUntilMs = now + IDENTIFY_MS;
    wsLog("Identify: LEDs active for 5 s");
  }
  if (mb.Coil(COIL_SAVE_CFG)) {
    mb.Coil(COIL_SAVE_CFG, false);
    if (saveConfigFS()) {
      cfgDirty = false;
      meterDirty = false;
      meterSavedEnergyCrc = meterEnergyCrc();
      wsLog("Configuration saved");
    } else {
      wsLog("ERROR: Save failed");
    }
  }
  if (mb.Coil(COIL_REBOOT)) {
    mb.Coil(COIL_REBOOT, false);
    wsLog("Rebooting…");
    (void)saveMeterFS();
    delay(50);
    rp2040.reboot();
  }
  if (mb.Coil(COIL_RESET_ENERGY)) {
    mb.Coil(COIL_RESET_ENERGY, false);
    resetEnergyCounters();
  }
}

static void updateLinkOkDetector(uint32_t now) {
  if (Serial2.available() > 0) g_lastLinkSeenMs = now;
  for (int i = 0; i < 32; i++) {
    const bool c = mb.Coil((uint16_t)i);
    if (c != coilSnapBefore[i]) {
      coilSnapBefore[i] = c;
      g_lastLinkSeenMs = now;
    }
  }
}

static inline bool linkOkNow(uint32_t now) {
  return ((uint32_t)(now - g_lastLinkSeenMs) < (uint32_t)g_linkTimeoutMs);
}

static void mbUpdateStatusFlags(uint32_t now) {
  uint16_t status = 0;
  if (linkOkNow(now)) status |= (1 << 1);
  if (cfgDirty)       status |= (1 << 3);
  mb.Ireg(IR_STATUS_FLAGS, status);
}

static inline uint16_t clamp_u16(int v) {
  if (v < 0) v = 0;
  if (v > 65535) v = 65535;
  return (uint16_t)v;
}
static inline int16_t clamp_i16(int v) {
  if (v < -32768) v = -32768;
  if (v >  32767) v =  32767;
  return (int16_t)v;
}

static void setAtmDefaults() {
  g_atm_cfg.lineHz = 50;
  g_atm_cfg.sumAbs = 1;
  g_atm_cfg.wireMode = 0;
  g_atm_cfg.phaseMap[0] = 0;
  g_atm_cfg.phaseMap[1] = 1;
  g_atm_cfg.phaseMap[2] = 2;
  g_atm_cfg.ucal   = 36000;  // sag detector reference
  for (int i = 0; i < 3; i++) {
    g_atm_cfg.cal[i].Ugain   = 39500;  // divider 6×220k+1k, calibrated @ 230V
    g_atm_cfg.cal[i].Igain   = 49000;  // ZEMCTK05 + PGA=2×, 50A full-scale
    g_atm_cfg.cal[i].Uoffset = 0;
    g_atm_cfg.cal[i].Ioffset = 0;
  }
}

static void clearEnergyCounters() {
  memset(g_ap_cnt, 0, sizeof(g_ap_cnt));
  memset(g_an_cnt, 0, sizeof(g_an_cnt));
  memset(g_rp_cnt, 0, sizeof(g_rp_cnt));
  memset(g_rn_cnt, 0, sizeof(g_rn_cnt));
  memset(g_s_cnt,  0, sizeof(g_s_cnt));
  memset(g_aph_cnt, 0, sizeof(g_aph_cnt));
  for (int i = 0; i < 4; i++) {
    g_e_ap_Wh[i] = g_e_an_Wh[i] = g_e_rp_varh[i] = g_e_rn_varh[i] = g_e_s_VAh[i] = 0;
    g_e_aph_Wh[i] = 0;
  }
}

static void drainChipEnergyRegs() {
  (void)g_atm.rdAP_A(); (void)g_atm.rdAP_B(); (void)g_atm.rdAP_C(); (void)g_atm.rdAP_T();
  (void)g_atm.rdAN_A(); (void)g_atm.rdAN_B(); (void)g_atm.rdAN_C(); (void)g_atm.rdAN_T();
  (void)g_atm.rdRP_A(); (void)g_atm.rdRP_B(); (void)g_atm.rdRP_C(); (void)g_atm.rdRP_T();
  (void)g_atm.rdRN_A(); (void)g_atm.rdRN_B(); (void)g_atm.rdRN_C(); (void)g_atm.rdRN_T();
  (void)g_atm.rdSA_A(); (void)g_atm.rdSA_B(); (void)g_atm.rdSA_C(); (void)g_atm.rdSA_T();
  (void)g_atm.rdAPH_A(); (void)g_atm.rdAPH_B(); (void)g_atm.rdAPH_C(); (void)g_atm.rdAPH_T();
}

static void setMeterDefaults() {
  g_atm_cfg.ucal = 36000;
  for (int i = 0; i < 3; i++) {
    g_atm_cfg.cal[i].Ugain   = 39500;
    g_atm_cfg.cal[i].Igain   = 49000;
    g_atm_cfg.cal[i].Uoffset = 0;
    g_atm_cfg.cal[i].Ioffset = 0;
  }
  clearEnergyCounters();
}

// ---- Meter cache + chunked SPI sampling (one step per loop) ----
static double   g_urms[3] = {0, 0, 0};
static double   g_irms[3] = {0, 0, 0};
static int32_t  g_p_W[4]  = {0, 0, 0, 0};
static int32_t  g_pfund_W[4] = {0, 0, 0, 0};
static int32_t  g_pharm_W[4] = {0, 0, 0, 0};
static int32_t  g_q_var[4]= {0, 0, 0, 0};
static int32_t  g_s_VA[4] = {0, 0, 0, 0};
static int16_t  g_pf_raw[4] = {0, 0, 0, 0};
static int16_t  g_ang_raw[3]= {0, 0, 0};
static uint16_t g_f_x100 = 0;
static int16_t  g_tempC = 0;
static double   g_upeak[3] = {0, 0, 0};
static double   g_ipeak[3] = {0, 0, 0};
static double   g_irmsN = 0.0;
static uint16_t g_thd_x100[3] = {0, 0, 0};
static bool     g_haveMeter = false;

static AlarmChCfg  alarmCfg[4];
static AlarmChRun  alarmRun[4];
static M90ChipEv   g_chipEv = {};
static uint8_t     g_chipEvMask[4] = {0, 0, 0, 0};
static uint8_t     g_chipEvMaskLatched[4] = {0, 0, 0, 0};
static uint32_t    g_chipEvLastMs[4] = {0, 0, 0, 0};
static unsigned long lastDiagSample = 0;
static const unsigned long diagSampleMs = 500;
static unsigned long lastWsDiagPrint = 0;
static const unsigned long wsDiagPrintMs = 2000;
static unsigned long lastPeakResetMs = 0;
static const unsigned long peakResetMs = 10000;
static unsigned long lastAlarmEval = 0;
static const unsigned long alarmEvalMs = 200;

static inline uint32_t ticks0p01CF_to_Wh(uint64_t ticks);

static inline uint16_t diAlarmAddr(uint8_t ch, uint8_t kind) {
  return (uint16_t)(DI_ALARM_BASE + ch * 3 + kind);
}

static uint32_t crc32_update(uint32_t crc, const uint8_t* data, size_t len) {
  crc = ~crc;
  while (len--) {
    crc ^= *data++;
    for (uint8_t k = 0; k < 8; k++)
      crc = (crc >> 1) ^ (0xEDB88320UL & (-(int32_t)(crc & 1)));
  }
  return ~crc;
}

static void markCfgDirty() {
  cfgDirty = true;
  lastCfgTouchMs = millis();
}

static void markMeterDirty() {
  meterDirty = true;
  lastMeterTouchMs = millis();
}

static uint32_t meterEnergyCrc();  // defined with sampleEnergyCounters

static void setAlarmDefaults() {
  memset(alarmCfg, 0, sizeof(alarmCfg));
  memset(alarmRun, 0, sizeof(alarmRun));
  memset(g_chipEvMask, 0, sizeof(g_chipEvMask));
  memset(&g_chipEv, 0, sizeof(g_chipEv));
}

static double alarmMetricScale(uint8_t metric) {
  switch (metric) {
    case ALARM_MET_URMS: return 100.0;
    case ALARM_MET_IRMS: return 1000.0;
    case ALARM_MET_FREQ: return 100.0;
    default: return 1.0;
  }
}

static double alarmRuleToDouble(int32_t raw, uint8_t metric) {
  return (double)raw / alarmMetricScale(metric);
}

static int32_t alarmDoubleToRule(double v, uint8_t metric) {
  const double scaled = v * alarmMetricScale(metric);
  if (scaled > 2147483647.0) return 2147483647;
  if (scaled < -2147483648.0) return -2147483648;
  return (int32_t)lround(scaled);
}

static double alarmMetricValue(uint8_t ch, uint8_t metric) {
  if (ch < 3) {
    switch (metric) {
      case ALARM_MET_URMS: return g_urms[ch];
      case ALARM_MET_IRMS: return g_irms[ch];
      case ALARM_MET_P:    return (double)g_p_W[ch];
      case ALARM_MET_Q:    return (double)g_q_var[ch];
      case ALARM_MET_S:    return (double)g_s_VA[ch];
      case ALARM_MET_FREQ: return ((double)g_f_x100) / 100.0;
      default: return 0.0;
    }
  }
  switch (metric) {
    case ALARM_MET_URMS:
      return fmax(fmax(g_urms[0], g_urms[1]), g_urms[2]);
    case ALARM_MET_IRMS:
      return g_irms[0] + g_irms[1] + g_irms[2];
    case ALARM_MET_P:    return (double)g_p_W[3];
    case ALARM_MET_Q:    return (double)g_q_var[3];
    case ALARM_MET_S:    return (double)g_s_VA[3];
    case ALARM_MET_FREQ: return ((double)g_f_x100) / 100.0;
    default: return 0.0;
  }
}

static double alarmHystBand(const AlarmRuleCfg& rule) {
  const double mn = alarmRuleToDouble(rule.minVal, rule.metric);
  const double mx = alarmRuleToDouble(rule.maxVal, rule.metric);
  double span = mx - mn;
  if (span <= 0.0) span = fmax(fabs(mn), fabs(mx));
  if (span <= 0.0) span = 1.0;
  double h = span * 0.02;
  if (rule.metric == ALARM_MET_FREQ) h = fmax(h, 0.10);
  if (rule.metric == ALARM_MET_URMS) h = fmax(h, 2.0);
  if (rule.metric == ALARM_MET_IRMS) h = fmax(h, 0.05);
  return h;
}

static void alarmEvalRule(const AlarmRuleCfg& cfg, AlarmRuleRun& run, double value) {
  if (!cfg.enabled) {
    run.active = false;
    run.hiSide = false;
    return;
  }
  const double mn = alarmRuleToDouble(cfg.minVal, cfg.metric);
  const double mx = alarmRuleToDouble(cfg.maxVal, cfg.metric);
  const bool hasMin = cfg.minVal != 0;
  const bool hasMax = cfg.maxVal != 0;
  const double h = alarmHystBand(cfg);

  if (!run.active) {
    if (hasMin && value < mn) {
      run.active = true;
      run.hiSide = false;
    } else if (hasMax && value > mx) {
      run.active = true;
      run.hiSide = true;
    }
    return;
  }

  bool clear = false;
  if (!run.hiSide) {
    clear = !hasMin || value >= (mn + h);
  } else {
    clear = !hasMax || value <= (mx - h);
  }
  if (clear) {
    run.active = false;
    run.hiSide = false;
  }
}

static void alarmUpdateChipMasks() {
  for (int i = 0; i < 4; i++) g_chipEvMask[i] = 0;
  for (int ph = 0; ph < 3; ph++) {
    uint8_t m = 0;
    if (g_chipEv.sag[ph])        m |= CHIP_EV_SAG;
    if (g_chipEv.ov[ph])         m |= CHIP_EV_OV;
    if (g_chipEv.phaseLoss[ph])  m |= CHIP_EV_PHASE_LOSS;
    if (g_chipEv.overI[ph])      m |= CHIP_EV_OVER_I;
    g_chipEvMask[ph] = m;
    g_chipEvMask[3] |= m;
  }
  if (g_chipEv.freqHi || g_chipEv.freqLo) g_chipEvMask[3] |= CHIP_EV_FREQ;
  if (g_chipEv.revPhase) g_chipEvMask[3] |= CHIP_EV_REV_PHASE;
}

static void chipEvPublishModbus() {
  mb.Ireg(IR_CHIP_EV_L1,  g_chipEvMask[0]);
  mb.Ireg(IR_CHIP_EV_L2,  g_chipEvMask[1]);
  mb.Ireg(IR_CHIP_EV_L3,  g_chipEvMask[2]);
  mb.Ireg(IR_CHIP_EV_TOT, g_chipEvMask[3]);
}

static void chipEvLatchFromMask(uint32_t now) {
  for (int ch = 0; ch < 4; ch++) {
    if (g_chipEvMask[ch]) {
      g_chipEvMaskLatched[ch] |= g_chipEvMask[ch];
      g_chipEvLastMs[ch] = now;
    }
    // Keep events visible for a short window even if chip clears state quickly.
    if (g_chipEvMaskLatched[ch] && (uint32_t)(now - g_chipEvLastMs[ch]) > 5000u) {
      g_chipEvMaskLatched[ch] = 0;
    }
  }
  // Use latched masks for publication.
  for (int ch = 0; ch < 4; ch++) g_chipEvMask[ch] = g_chipEvMaskLatched[ch];
}

static JSONVar chipEvMasksToJson() {
  JSONVar a;
  for (int i = 0; i < 4; i++) a[i] = (int)g_chipEvMask[i];
  return a;
}

static void alarmSampleChipDiag(uint32_t now) {
  if (atmBusy || meter_job) return;
  const M90DiagRegs d = g_atm.readDiag();
  decodeM90ChipEv(d, g_chipEv);
  alarmUpdateChipMasks();
  chipEvLatchFromMask(now);
  g_atm.clearDiagInterrupts();
  chipEvPublishModbus();
  for (int ch = 0; ch < 4; ch++)
    alarmRun[ch].chipActive = g_chipEvMask[ch] != 0;
}

static bool alarmRulePublished(uint8_t ch, uint8_t kind) {
  if (ch > 3 || kind > 2) return false;
  bool active = alarmRun[ch].rules[kind].active;
  if (kind == ALARM_KIND_EVENT && alarmRun[ch].chipActive) active = true;
  if (!active) return false;
  if (alarmCfg[ch].ackRequired && alarmRun[ch].acked) return false;
  return true;
}

static bool alarmChannelActive(uint8_t ch, uint8_t mask) {
  if (ch > 3) return false;
  for (uint8_t k = 0; k < 3; k++) {
    if ((mask & (1u << k)) && alarmRulePublished(ch, k)) return true;
  }
  return false;
}

static bool relayAlarmDemand(uint8_t rly) {
  if (rly >= NUM_RLY || rlyCfg[rly].mode != RLY_MODE_ALARM) return false;
  const uint8_t ch = (uint8_t)constrain((int)rlyCfg[rly].alarmCh, 0, 3);
  const uint8_t mask = rlyCfg[rly].alarmMask ? rlyCfg[rly].alarmMask : 1;
  return alarmChannelActive(ch, mask);
}

static void alarmEvalThresholds() {
  if (!g_haveMeter) return;
  for (uint8_t ch = 0; ch < 4; ch++) {
    for (uint8_t kind = 0; kind < 3; kind++) {
      const AlarmRuleCfg& cfg = alarmCfg[ch].rules[kind];
      if (!cfg.enabled) {
        alarmRun[ch].rules[kind].active = false;
        alarmRun[ch].rules[kind].hiSide = false;
        continue;
      }
      const double v = alarmMetricValue(ch, cfg.metric);
      alarmEvalRule(cfg, alarmRun[ch].rules[kind], v);
    }
    bool any = false;
    for (uint8_t kind = 0; kind < 3; kind++) {
      if (alarmRun[ch].rules[kind].active) any = true;
    }
    if (alarmRun[ch].chipActive) any = true;
    if (!any) alarmRun[ch].acked = false;
  }
}

static void alarmPublishModbus() {
  for (uint8_t ch = 0; ch < 4; ch++) {
    for (uint8_t kind = 0; kind < 3; kind++) {
      mb.setIsts(diAlarmAddr(ch, kind), alarmRulePublished(ch, kind));
    }
  }
}

static void alarmAckChannel(uint8_t ch) {
  if (ch > 3) return;
  alarmRun[ch].acked = true;
}

static void alarmAckAll() {
  for (uint8_t ch = 0; ch < 4; ch++) alarmAckChannel(ch);
}

static void alarmServiceTick(unsigned long now) {
  if (!atmBusy && !meter_job && (now - lastDiagSample >= diagSampleMs)) {
    lastDiagSample = now;
    alarmSampleChipDiag((uint32_t)now);
  }
  if (now - lastAlarmEval >= alarmEvalMs) {
    lastAlarmEval = now;
    alarmEvalThresholds();
    alarmPublishModbus();
  }
}

static void serviceModbusAck() {
  for (uint8_t ch = 0; ch < 4; ch++) {
    if (mb.Coil(COIL_ACK_BASE + ch)) {
      alarmAckChannel(ch);
      mb.Coil(COIL_ACK_BASE + ch, false);
    }
  }
}

static JSONVar alarmsStateToJson() {
  JSONVar st;
  for (uint8_t ch = 0; ch < 4; ch++) {
    JSONVar kinds;
    for (uint8_t kind = 0; kind < 3; kind++) {
      JSONVar e;
      e["active"] = alarmRulePublished(ch, kind) ? 1 : 0;
      kinds[kind] = e;
    }
    st[ch] = kinds;
  }
  return st;
}

static JSONVar alarmCfgToJson() {
  JSONVar arr;
  for (uint8_t ch = 0; ch < 4; ch++) {
    JSONVar chO;
    chO["ack"] = alarmCfg[ch].ackRequired ? 1 : 0;
    for (uint8_t kind = 0; kind < 3; kind++) {
      JSONVar r;
      const AlarmRuleCfg& cfg = alarmCfg[ch].rules[kind];
      r["enabled"] = cfg.enabled ? 1 : 0;
      r["metric"]  = (int)cfg.metric;
      r["min"]     = alarmRuleToDouble(cfg.minVal, cfg.metric);
      r["max"]     = alarmRuleToDouble(cfg.maxVal, cfg.metric);
      chO[kind] = r;
    }
    arr[ch] = chO;
  }
  return arr;
}

static void alarmApplyChannelFromJson(uint8_t ch, const JSONVar& obj) {
  if (ch > 3 || JSON.typeof(obj) == "undefined") return;
  if (JSON.typeof(obj["ack"]) != "undefined") alarmCfg[ch].ackRequired = jvGetInt(obj, "ack", 0) ? 1 : 0;
  for (uint8_t kind = 0; kind < 3; kind++) {
    JSONVar r = obj[kind];
    if (JSON.typeof(r) == "undefined") continue;
    AlarmRuleCfg& cfg = alarmCfg[ch].rules[kind];
    if (JSON.typeof(r["enabled"]) != "undefined") cfg.enabled = jvGetInt(r, "enabled", 0) ? 1 : 0;
    if (JSON.typeof(r["metric"]) != "undefined")  cfg.metric  = (uint8_t)constrain(jvGetInt(r, "metric", cfg.metric), 0, 5);
    if (JSON.typeof(r["min"]) != "undefined") {
      // Engineering units (float). Do not use jvGetInt — it truncates before scale.
      const double eng = jvGetDouble(r, "min", 0.0);
      cfg.minVal = (eng != 0.0) ? alarmDoubleToRule(eng, cfg.metric) : 0;
    }
    if (JSON.typeof(r["max"]) != "undefined") {
      const double eng = jvGetDouble(r, "max", 0.0);
      cfg.maxVal = (eng != 0.0) ? alarmDoubleToRule(eng, cfg.metric) : 0;
    }
  }
}

static void alarmApplyFromJson(const JSONVar& list) {
  if (JSON.typeof(list) == "array") {
    for (uint8_t ch = 0; ch < 4 && ch < list.length(); ch++) alarmApplyChannelFromJson(ch, list[ch]);
    return;
  }
  if (list.hasOwnProperty("ch")) {
    const int ch = jvGetInt(list, "ch", -1);
    if (ch >= 0 && ch < 4) alarmApplyChannelFromJson((uint8_t)ch, list["cfg"]);
    return;
  }
  for (uint8_t ch = 0; ch < 4; ch++) {
    if (list.hasOwnProperty(String(ch).c_str())) alarmApplyChannelFromJson(ch, list[ch]);
  }
}

static void handleAlarmsCfg(JSONVar obj) {
  markWebHostRx();
  if (obj.hasOwnProperty("ch")) {
    const int ch = jvGetInt(obj, "ch", -1);
    if (ch >= 0 && ch < 4) alarmApplyChannelFromJson((uint8_t)ch, obj["cfg"]);
  } else {
    alarmApplyFromJson(obj);
  }
  markCfgDirty();
  sendWebCfg();
  sendWebExt();
  wsLog("Alarm configuration updated");
}

static void handleAlarmsAck(JSONVar obj) {
  markWebHostRx();
  if (obj.hasOwnProperty("list")) {
    JSONVar list = obj["list"];
    for (uint8_t ch = 0; ch < 4; ch++) {
      if (list[ch] || list[(int)ch]) alarmAckChannel(ch);
    }
  } else if (obj.hasOwnProperty("ch")) {
    alarmAckChannel((uint8_t)constrain(jvGetInt(obj, "ch", 0), 0, 3));
  } else {
    alarmAckAll();
  }
  alarmPublishModbus();
  sendWebExt();
  wsLog("Alarms acknowledged");
}

static void captureSettings(EnmSettingsCfg& pc) {
  memset(&pc, 0, sizeof(pc));
  pc.magic      = CFG_MAGIC;
  pc.version    = CFG_VERSION;
  pc.size       = sizeof(EnmSettingsCfg);
  pc.mb_address = g_mb_address;
  pc.mb_baud    = g_mb_baud;
  pc.lineHz     = g_atm_cfg.lineHz;
  pc.sumAbs     = g_atm_cfg.sumAbs;
  pc.wireMode   = g_atm_cfg.wireMode;
  for (int i = 0; i < 3; i++) pc.phaseMap[i] = g_atm_cfg.phaseMap[i];
  for (int i = 0; i < NUM_RLY; i++) {
    pc.rlyCfg[i].enabled   = rlyCfg[i].enabled;
    pc.rlyCfg[i].inverted  = rlyCfg[i].inverted;
    pc.rlyCfg[i].mode      = rlyCfg[i].mode;
    pc.rlyCfg[i].alarmCh   = rlyCfg[i].alarmCh;
    pc.rlyCfg[i].alarmMask = rlyCfg[i].alarmMask;
    pc.rlyCfg[i].pad       = 0;
  }
  for (int i = 0; i < NUM_LED; i++) {
    pc.ledCfg[i].mode   = ledCfg[i].mode;
    pc.ledCfg[i].source = ledCfg[i].source;
  }
  for (int i = 0; i < NUM_BTN; i++) {
    pc.btnCfg[i].action = btnCfg[i].action;
  }
  for (int i = 0; i < 4; i++) {
    pc.alarm[i] = alarmCfg[i];
  }
  pc.crc32 = 0;
  pc.crc32 = crc32_update(0, reinterpret_cast<const uint8_t*>(&pc), sizeof(EnmSettingsCfg));
}

static bool applySettings(const EnmSettingsCfg& pc) {
  if (pc.magic != CFG_MAGIC || pc.size != sizeof(EnmSettingsCfg))
    return false;
  if (pc.version != CFG_VERSION && pc.version != CFG_VERSION_V22)
    return false;
  EnmSettingsCfg tmp = pc;
  const uint32_t crc = tmp.crc32;
  tmp.crc32 = 0;
  if (crc32_update(0, reinterpret_cast<const uint8_t*>(&tmp), sizeof(EnmSettingsCfg)) != crc)
    return false;

  g_mb_address = pc.mb_address;
  if (g_mb_address < 1 || g_mb_address > 247) g_mb_address = 30;
  g_mb_baud = pc.mb_baud;
  if (!isAllowedBaud(g_mb_baud)) g_mb_baud = 19200;
  SlaveId = (int)g_mb_address;

  g_atm_cfg.lineHz = (pc.lineHz == 60) ? 60 : 50;
  g_atm_cfg.sumAbs = pc.sumAbs ? 1 : 0;
  g_atm_cfg.wireMode = pc.wireMode ? 1 : 0;
  for (int i = 0; i < 3; i++) g_atm_cfg.phaseMap[i] = pc.phaseMap[i];
  normalizePhaseMap(g_atm_cfg.phaseMap);

  for (int i = 0; i < NUM_RLY; i++) {
    rlyCfg[i].enabled   = pc.rlyCfg[i].enabled;
    rlyCfg[i].inverted  = pc.rlyCfg[i].inverted;
    rlyCfg[i].mode      = pc.rlyCfg[i].mode;
    rlyCfg[i].alarmCh   = (uint8_t)constrain((int)pc.rlyCfg[i].alarmCh, 0, 3);
    rlyCfg[i].alarmMask = pc.rlyCfg[i].alarmMask ? pc.rlyCfg[i].alarmMask : 1;
  }
  for (int i = 0; i < NUM_LED; i++) {
    ledCfg[i].mode   = pc.ledCfg[i].mode;
    ledCfg[i].source = pc.ledCfg[i].source;
  }
  for (int i = 0; i < NUM_BTN; i++) {
    btnCfg[i].action = pc.btnCfg[i].action;
  }
  for (int i = 0; i < 4; i++) {
    alarmCfg[i] = pc.alarm[i];
  }
  return true;
}

static bool applySettingsV21(const EnmSettingsCfgV21& pc) {
  if (pc.magic != CFG_MAGIC || pc.version != CFG_VERSION_V21 || pc.size != sizeof(EnmSettingsCfgV21))
    return false;
  EnmSettingsCfgV21 tmp = pc;
  const uint32_t crc = tmp.crc32;
  tmp.crc32 = 0;
  if (crc32_update(0, reinterpret_cast<const uint8_t*>(&tmp), sizeof(EnmSettingsCfgV21)) != crc)
    return false;

  g_mb_address = pc.mb_address;
  if (g_mb_address < 1 || g_mb_address > 247) g_mb_address = 30;
  g_mb_baud = pc.mb_baud;
  if (!isAllowedBaud(g_mb_baud)) g_mb_baud = 19200;
  SlaveId = (int)g_mb_address;

  g_atm_cfg.lineHz = (pc.lineHz == 60) ? 60 : 50;
  g_atm_cfg.sumAbs = pc.sumAbs ? 1 : 0;
  g_atm_cfg.wireMode = 0;
  g_atm_cfg.phaseMap[0] = 0;
  g_atm_cfg.phaseMap[1] = 1;
  g_atm_cfg.phaseMap[2] = 2;

  for (int i = 0; i < NUM_RLY; i++) {
    rlyCfg[i].enabled   = pc.rlyCfg[i].enabled;
    rlyCfg[i].inverted  = pc.rlyCfg[i].inverted;
    rlyCfg[i].mode      = pc.rlyCfg[i].mode;
    rlyCfg[i].alarmCh   = (uint8_t)constrain((int)pc.rlyCfg[i].alarmCh, 0, 3);
    rlyCfg[i].alarmMask = pc.rlyCfg[i].alarmMask ? pc.rlyCfg[i].alarmMask : 1;
  }
  for (int i = 0; i < NUM_LED; i++) {
    ledCfg[i].mode   = pc.ledCfg[i].mode;
    ledCfg[i].source = pc.ledCfg[i].source;
  }
  for (int i = 0; i < NUM_BTN; i++) {
    btnCfg[i].action = pc.btnCfg[i].action;
  }
  for (int i = 0; i < 4; i++) {
    alarmCfg[i] = pc.alarm[i];
  }
  return true;
}

static bool applySettingsV20(const EnmSettingsCfgV20& pc) {
  if (pc.magic != CFG_MAGIC || pc.version != CFG_VERSION_V20 || pc.size != sizeof(EnmSettingsCfgV20))
    return false;
  EnmSettingsCfgV20 tmp = pc;
  const uint32_t crc = tmp.crc32;
  tmp.crc32 = 0;
  if (crc32_update(0, reinterpret_cast<const uint8_t*>(&tmp), sizeof(EnmSettingsCfgV20)) != crc)
    return false;

  g_mb_address = pc.mb_address;
  if (g_mb_address < 1 || g_mb_address > 247) g_mb_address = 30;
  g_mb_baud = pc.mb_baud;
  if (!isAllowedBaud(g_mb_baud)) g_mb_baud = 19200;
  SlaveId = (int)g_mb_address;

  g_atm_cfg.lineHz = (pc.lineHz == 60) ? 60 : 50;
  g_atm_cfg.sumAbs = pc.sumAbs ? 1 : 0;
  g_atm_cfg.wireMode = 0;
  g_atm_cfg.phaseMap[0] = 0;
  g_atm_cfg.phaseMap[1] = 1;
  g_atm_cfg.phaseMap[2] = 2;

  for (int i = 0; i < NUM_RLY; i++) {
    rlyCfg[i].enabled   = pc.rlyCfg[i].enabled;
    rlyCfg[i].inverted  = pc.rlyCfg[i].inverted;
    rlyCfg[i].mode      = RLY_MODE_MODBUS;
    rlyCfg[i].alarmCh   = 3;
    rlyCfg[i].alarmMask = 1;
  }
  for (int i = 0; i < NUM_LED; i++) {
    ledCfg[i].mode   = pc.ledCfg[i].mode;
    ledCfg[i].source = pc.ledCfg[i].source;
  }
  for (int i = 0; i < NUM_BTN; i++) {
    btnCfg[i].action = pc.btnCfg[i].action;
  }
  setAlarmDefaults();
  return true;
}

static void captureMeter(EnmMeterCfg& pc) {
  memset(&pc, 0, sizeof(pc));
  pc.magic   = METER_MAGIC;
  pc.version = METER_VERSION;
  pc.size    = sizeof(EnmMeterCfg);
  pc.ucal    = g_atm_cfg.ucal;
  for (int i = 0; i < 3; i++) {
    pc.Ugain[i]   = g_atm_cfg.cal[i].Ugain;
    pc.Igain[i]   = g_atm_cfg.cal[i].Igain;
    pc.Uoffset[i] = g_atm_cfg.cal[i].Uoffset;
    pc.Ioffset[i] = g_atm_cfg.cal[i].Ioffset;
  }
  memcpy(pc.ap_cnt, g_ap_cnt, sizeof(g_ap_cnt));
  memcpy(pc.an_cnt, g_an_cnt, sizeof(g_an_cnt));
  memcpy(pc.rp_cnt, g_rp_cnt, sizeof(g_rp_cnt));
  memcpy(pc.rn_cnt, g_rn_cnt, sizeof(g_rn_cnt));
  memcpy(pc.s_cnt,  g_s_cnt,  sizeof(g_s_cnt));
  memcpy(pc.aph_cnt, g_aph_cnt, sizeof(g_aph_cnt));
  pc.crc32 = 0;
  pc.crc32 = crc32_update(0, reinterpret_cast<const uint8_t*>(&pc), sizeof(EnmMeterCfg));
}

static bool applyMeter(const EnmMeterCfg& pc) {
  if (pc.magic != METER_MAGIC || pc.version != METER_VERSION || pc.size != sizeof(EnmMeterCfg))
    return false;
  EnmMeterCfg tmp = pc;
  const uint32_t crc = tmp.crc32;
  tmp.crc32 = 0;
  if (crc32_update(0, reinterpret_cast<const uint8_t*>(&tmp), sizeof(EnmMeterCfg)) != crc)
    return false;

  g_atm_cfg.ucal = pc.ucal ? pc.ucal : 36000;
  for (int i = 0; i < 3; i++) {
    g_atm_cfg.cal[i].Ugain   = pc.Ugain[i];
    g_atm_cfg.cal[i].Igain   = pc.Igain[i];
    g_atm_cfg.cal[i].Uoffset = pc.Uoffset[i];
    g_atm_cfg.cal[i].Ioffset = pc.Ioffset[i];
  }

  memcpy(g_ap_cnt, pc.ap_cnt, sizeof(g_ap_cnt));
  memcpy(g_an_cnt, pc.an_cnt, sizeof(g_an_cnt));
  memcpy(g_rp_cnt, pc.rp_cnt, sizeof(g_rp_cnt));
  memcpy(g_rn_cnt, pc.rn_cnt, sizeof(g_rn_cnt));
  memcpy(g_s_cnt,  pc.s_cnt,  sizeof(g_s_cnt));
  memcpy(g_aph_cnt, pc.aph_cnt, sizeof(g_aph_cnt));

  for (int i = 0; i < 4; i++) {
    g_e_ap_Wh[i]   = ticks0p01CF_to_Wh(g_ap_cnt[i]);
    g_e_an_Wh[i]   = ticks0p01CF_to_Wh(g_an_cnt[i]);
    g_e_rp_varh[i] = ticks0p01CF_to_Wh(g_rp_cnt[i]);
    g_e_rn_varh[i] = ticks0p01CF_to_Wh(g_rn_cnt[i]);
    g_e_s_VAh[i]   = ticks0p01CF_to_Wh(g_s_cnt[i]);
    g_e_aph_Wh[i]  = ticks0p01CF_to_Wh(g_aph_cnt[i]);
  }
  return true;
}

static bool applyMeterV1(const EnmMeterCfgV1& pc) {
  if (pc.magic != METER_MAGIC || pc.version != 0x0001 || pc.size != sizeof(EnmMeterCfgV1))
    return false;
  EnmMeterCfgV1 tmp = pc;
  const uint32_t crc = tmp.crc32;
  tmp.crc32 = 0;
  if (crc32_update(0, reinterpret_cast<const uint8_t*>(&tmp), sizeof(EnmMeterCfgV1)) != crc)
    return false;

  g_atm_cfg.ucal = pc.ucal ? pc.ucal : 36000;
  for (int i = 0; i < 3; i++) {
    g_atm_cfg.cal[i].Ugain   = pc.Ugain[i];
    g_atm_cfg.cal[i].Igain   = pc.Igain[i];
    g_atm_cfg.cal[i].Uoffset = pc.Uoffset[i];
    g_atm_cfg.cal[i].Ioffset = pc.Ioffset[i];
  }

  memcpy(g_ap_cnt, pc.ap_cnt, sizeof(g_ap_cnt));
  memcpy(g_an_cnt, pc.an_cnt, sizeof(g_an_cnt));
  memcpy(g_rp_cnt, pc.rp_cnt, sizeof(g_rp_cnt));
  memcpy(g_rn_cnt, pc.rn_cnt, sizeof(g_rn_cnt));
  memcpy(g_s_cnt,  pc.s_cnt,  sizeof(g_s_cnt));
  memset(g_aph_cnt, 0, sizeof(g_aph_cnt));

  for (int i = 0; i < 4; i++) {
    g_e_ap_Wh[i]   = ticks0p01CF_to_Wh(g_ap_cnt[i]);
    g_e_an_Wh[i]   = ticks0p01CF_to_Wh(g_an_cnt[i]);
    g_e_rp_varh[i] = ticks0p01CF_to_Wh(g_rp_cnt[i]);
    g_e_rn_varh[i] = ticks0p01CF_to_Wh(g_rn_cnt[i]);
    g_e_s_VAh[i]   = ticks0p01CF_to_Wh(g_s_cnt[i]);
    g_e_aph_Wh[i]  = 0;
  }
  return true;
}

static bool applyLegacyPersist(const EnmPersistCfgLegacy& pc) {
  if (pc.magic != CFG_MAGIC || pc.version != LEGACY_CFG_VER || pc.size != sizeof(EnmPersistCfgLegacy))
    return false;
  EnmPersistCfgLegacy tmp = pc;
  const uint32_t crc = tmp.crc32;
  tmp.crc32 = 0;
  if (crc32_update(0, reinterpret_cast<const uint8_t*>(&tmp), sizeof(EnmPersistCfgLegacy)) != crc)
    return false;

  g_mb_address = pc.mb_address;
  if (g_mb_address < 1 || g_mb_address > 247) g_mb_address = 30;
  g_mb_baud = pc.mb_baud;
  if (!isAllowedBaud(g_mb_baud)) g_mb_baud = 19200;
  SlaveId = (int)g_mb_address;
  g_atm_cfg.lineHz = (pc.lineHz == 60) ? 60 : 50;
  g_atm_cfg.sumAbs = pc.sumAbs ? 1 : 0;
  g_atm_cfg.wireMode = 0;
  g_atm_cfg.phaseMap[0] = 0;
  g_atm_cfg.phaseMap[1] = 1;
  g_atm_cfg.phaseMap[2] = 2;
  g_atm_cfg.ucal   = pc.ucal ? pc.ucal : 36000;
  for (int i = 0; i < 3; i++) {
    g_atm_cfg.cal[i].Ugain   = pc.Ugain[i];
    g_atm_cfg.cal[i].Igain   = pc.Igain[i];
    g_atm_cfg.cal[i].Uoffset = pc.Uoffset[i];
    g_atm_cfg.cal[i].Ioffset = pc.Ioffset[i];
  }
  memcpy(g_ap_cnt, pc.ap_cnt, sizeof(g_ap_cnt));
  memcpy(g_an_cnt, pc.an_cnt, sizeof(g_an_cnt));
  memcpy(g_rp_cnt, pc.rp_cnt, sizeof(g_rp_cnt));
  memcpy(g_rn_cnt, pc.rn_cnt, sizeof(g_rn_cnt));
  memcpy(g_s_cnt,  pc.s_cnt,  sizeof(g_s_cnt));
  for (int i = 0; i < 4; i++) {
    g_e_ap_Wh[i]   = ticks0p01CF_to_Wh(g_ap_cnt[i]);
    g_e_an_Wh[i]   = ticks0p01CF_to_Wh(g_an_cnt[i]);
    g_e_rp_varh[i] = ticks0p01CF_to_Wh(g_rp_cnt[i]);
    g_e_rn_varh[i] = ticks0p01CF_to_Wh(g_rn_cnt[i]);
    g_e_s_VAh[i]   = ticks0p01CF_to_Wh(g_s_cnt[i]);
  }
  for (int i = 0; i < NUM_RLY; i++) {
    rlyCfg[i].enabled   = true;
    rlyCfg[i].inverted  = false;
    rlyCfg[i].mode      = RLY_MODE_MODBUS;
    rlyCfg[i].alarmCh   = 3;
    rlyCfg[i].alarmMask = 1;
  }
  setAlarmDefaults();
  return true;
}

static bool saveSettingsFS() {
  EnmSettingsCfg pc;
  captureSettings(pc);
  File f = LittleFS.open(CFG_PATH, "w");
  if (!f) return false;
  const size_t n = f.write(reinterpret_cast<const uint8_t*>(&pc), sizeof(pc));
  f.close();
  return n == sizeof(pc);
}

static bool saveMeterFS() {
  EnmMeterCfg pc;
  captureMeter(pc);
  File f = LittleFS.open(METER_PATH, "w");
  if (!f) return false;
  const size_t n = f.write(reinterpret_cast<const uint8_t*>(&pc), sizeof(pc));
  f.close();
  return n == sizeof(pc);
}

static bool saveConfigFS() {
  return saveSettingsFS() && saveMeterFS();
}

static bool loadSettingsFS() {
  File f = LittleFS.open(CFG_PATH, "r");
  if (!f) return false;
  const size_t sz = f.size();
  if (sz == sizeof(EnmSettingsCfg)) {
    EnmSettingsCfg pc;
    const size_t n = f.read(reinterpret_cast<uint8_t*>(&pc), sizeof(pc));
    f.close();
    if (n != sizeof(pc)) return false;
    const bool ok = applySettings(pc);
    if (ok && pc.version == CFG_VERSION_V22) markCfgDirty();
    return ok;
  }
  if (sz == sizeof(EnmSettingsCfgV21)) {
    EnmSettingsCfgV21 pc;
    const size_t n = f.read(reinterpret_cast<uint8_t*>(&pc), sizeof(pc));
    f.close();
    if (n != sizeof(pc)) return false;
    if (!applySettingsV21(pc)) return false;
    markCfgDirty();
    return true;
  }
  if (sz == sizeof(EnmSettingsCfgV20)) {
    EnmSettingsCfgV20 pc;
    const size_t n = f.read(reinterpret_cast<uint8_t*>(&pc), sizeof(pc));
    f.close();
    if (n != sizeof(pc)) return false;
    if (!applySettingsV20(pc)) return false;
    markCfgDirty();
    return true;
  }
  f.close();
  return false;
}

static bool loadMeterFS() {
  File f = LittleFS.open(METER_PATH, "r");
  if (!f) return false;
  const size_t sz = f.size();
  if (sz == sizeof(EnmMeterCfg)) {
    EnmMeterCfg pc;
    const size_t n = f.read(reinterpret_cast<uint8_t*>(&pc), sizeof(pc));
    f.close();
    if (n != sizeof(pc)) return false;
    return applyMeter(pc);
  }
  if (sz == sizeof(EnmMeterCfgV1)) {
    EnmMeterCfgV1 pc;
    const size_t n = f.read(reinterpret_cast<uint8_t*>(&pc), sizeof(pc));
    f.close();
    if (n != sizeof(pc)) return false;
    if (!applyMeterV1(pc)) return false;
    markMeterDirty();
    return true;
  }
  f.close();
  return false;
}

static bool tryMigrateLegacyCfg() {
  File f = LittleFS.open(CFG_PATH, "r");
  if (!f) return false;
  if (f.size() != sizeof(EnmPersistCfgLegacy)) {
    f.close();
    return false;
  }
  EnmPersistCfgLegacy pc;
  const size_t n = f.read(reinterpret_cast<uint8_t*>(&pc), sizeof(pc));
  f.close();
  if (n != sizeof(pc) || !applyLegacyPersist(pc)) return false;
  saveSettingsFS();
  saveMeterFS();
  return true;
}

static bool loadConfigFS() {
  const bool settingsOk = loadSettingsFS();
  const bool meterOk = loadMeterFS();
  if (settingsOk && meterOk) return true;
  if (!settingsOk && !meterOk && tryMigrateLegacyCfg()) return true;
  if (!settingsOk) return false;
  if (!meterOk) {
    setMeterDefaults();
    markMeterDirty();
  }
  return true;
}

static uint8_t  meter_step = 0;
static double   urms_tmp[3], irms_tmp[3];

static unsigned long lastMeterSample = 0;
static const unsigned long meterSampleMs = 1000;
static unsigned long lastEnergySample = 0;
// ATM90E32 energy regs are 16-bit read-to-clear; Total overflows ~204.8 Wh
// (~21 s at full scale). Keep poll ≤10 s so ticks are not lost under load.
static const unsigned long energySampleMs = 5000;

static inline uint32_t ticks0p01CF_to_Wh(uint64_t ticks) {
  if (g_MC_imp_per_kWh == 0) return 0;
  return (uint32_t)lround((double)ticks * (10.0 / (double)g_MC_imp_per_kWh));
}

static void mbPublishMeter() {
  if (!g_haveMeter) return;

  auto clampU = [](double v) -> uint16_t {
    long x = lround(v * 100.0);
    return (uint16_t)constrain(x, 0L, 65535L);
  };
  auto clampI = [](double a) -> uint16_t {
    long x = lround(a * 1000.0);
    return (uint16_t)constrain(x, 0L, 65535L);
  };

  mb.Ireg(IR_URMS_BASE + 0, clampU(g_urms[0]));
  mb.Ireg(IR_URMS_BASE + 1, clampU(g_urms[1]));
  mb.Ireg(IR_URMS_BASE + 2, clampU(g_urms[2]));
  mb.Ireg(IR_IRMS_BASE + 0, clampI(g_irms[0]));
  mb.Ireg(IR_IRMS_BASE + 1, clampI(g_irms[1]));
  mb.Ireg(IR_IRMS_BASE + 2, clampI(g_irms[2]));
  mb.Ireg(IR_FREQ, g_f_x100);
  mb.Ireg(IR_TEMP, (uint16_t)g_tempC);

  for (int i = 0; i < 4; i++) {
    mb.Ireg(IR_PF_BASE + i, (uint16_t)g_pf_raw[i]);
    if (i < 3) mb.Ireg(IR_ANG_BASE + i, (uint16_t)g_ang_raw[i]);
    mbPutS32Ir(IR_P_BASE + i * 2, g_p_W[i]);
    mbPutS32Ir(IR_Q_BASE + i * 2, g_q_var[i]);
    mbPutS32Ir(IR_S_BASE + i * 2, g_s_VA[i]);
    mbPutS32Ir(IR_PFUND_BASE + i * 2, g_pfund_W[i]);
    mbPutS32Ir(IR_PHARM_BASE + i * 2, g_pharm_W[i]);
  }

  for (int i = 0; i < 3; i++) {
    mb.Ireg(IR_UPEAK_BASE + i, clampU(g_upeak[i]));
    mb.Ireg(IR_IPEAK_BASE + i, clampI(g_ipeak[i]));
    mb.Ireg(IR_THD_BASE + i, g_thd_x100[i]);
  }
  mb.Ireg(IR_IRMSN, clampI(g_irmsN));

  uint16_t o = IR_E_BASE;
  for (int i = 0; i < 4; i++) mbPutU32Ir(o + i * 2, g_e_ap_Wh[i]);
  o += 8;
  for (int i = 0; i < 4; i++) mbPutU32Ir(o + i * 2, g_e_an_Wh[i]);
  o += 8;
  for (int i = 0; i < 4; i++) mbPutU32Ir(o + i * 2, g_e_rp_varh[i]);
  o += 8;
  for (int i = 0; i < 4; i++) mbPutU32Ir(o + i * 2, g_e_rn_varh[i]);
  o += 8;
  for (int i = 0; i < 4; i++) mbPutU32Ir(o + i * 2, g_e_s_VAh[i]);
  o = IR_EHARM_AP_BASE;
  for (int i = 0; i < 4; i++) mbPutU32Ir(o + i * 2, g_e_aph_Wh[i]);
}

static void meter_job_begin() {
  meter_job = true;
  meter_step = 0;
}

static void meter_job_step() {
  if (!meter_job || atmBusy) return;

  switch (meter_step) {
    case 0:
      urms_tmp[0] = g_atm.readUrmsA_V();
      urms_tmp[1] = g_atm.readUrmsB_V();
      meter_step++;
      break;
    case 1:
      urms_tmp[2] = g_atm.readUrmsC_V();
      irms_tmp[0] = g_atm.readIrmsA_A();
      meter_step++;
      break;
    case 2:
      irms_tmp[1] = g_atm.readIrmsB_A();
      irms_tmp[2] = g_atm.readIrmsC_A();
      for (int i = 0; i < 3; i++) {
        g_urms[i] = urms_tmp[i];
        g_irms[i] = irms_tmp[i];
      }
      g_haveMeter = true;
      meter_step++;
      break;
    case 3:
      g_pf_raw[0] = g_atm.readPFmeanA();
      g_pf_raw[1] = g_atm.readPFmeanB();
      meter_step++;
      break;
    case 4:
      g_pf_raw[2] = g_atm.readPFmeanC();
      g_pf_raw[3] = g_atm.readPFmeanT();
      g_ang_raw[0] = g_atm.readPAngleA();
      g_ang_raw[1] = g_atm.readPAngleB();
      meter_step++;
      break;
    case 5: {
      g_ang_raw[2] = g_atm.readPAngleC();
      g_f_x100 = g_atm.readFreq_x100();
      g_tempC  = g_atm.readTempC();

      for (int i = 0; i < 4; i++) {
        g_p_W[i]    = g_atm.readPmeanW((uint8_t)i);
        g_q_var[i]  = g_atm.readQmean_var((uint8_t)i);
        g_s_VA[i]   = g_atm.readSmean_VA((uint8_t)i);
        g_pfund_W[i] = g_atm.readPmeanFundW((uint8_t)i);
        g_pharm_W[i] = g_atm.readPmeanHarmW((uint8_t)i);
      }
      meter_step++;
      break;
    }
    case 6:
      for (int i = 0; i < 3; i++) {
        g_upeak[i] = g_atm.readUPeak_V(i);
        g_ipeak[i] = g_atm.readIPeak_A(i);
      }
      g_irmsN = g_atm.readIrmsN_A();
      meter_step++;
      break;
    case 7:
      for (int i = 0; i < 3; i++)
        g_thd_x100[i] = g_atm.readThdPct_x100((uint8_t)i);
      meter_job = false;
      g_haveMeter = true;
      mbPublishMeter();
      break;
    default:
      meter_job = false;
      break;
  }
}

static uint32_t meterEnergyCrc() {
  uint32_t c = 0;
  c = crc32_update(c, reinterpret_cast<const uint8_t*>(g_ap_cnt),  sizeof(g_ap_cnt));
  c = crc32_update(c, reinterpret_cast<const uint8_t*>(g_an_cnt),  sizeof(g_an_cnt));
  c = crc32_update(c, reinterpret_cast<const uint8_t*>(g_rp_cnt),  sizeof(g_rp_cnt));
  c = crc32_update(c, reinterpret_cast<const uint8_t*>(g_rn_cnt),  sizeof(g_rn_cnt));
  c = crc32_update(c, reinterpret_cast<const uint8_t*>(g_s_cnt),   sizeof(g_s_cnt));
  c = crc32_update(c, reinterpret_cast<const uint8_t*>(g_aph_cnt), sizeof(g_aph_cnt));
  return c;
}

static void sampleEnergyCounters() {
  // Poll only — do not mark LittleFS dirty here (flash wear + Modbus FIFO overruns).
  g_ap_cnt[0] += g_atm.rdAP_A(); g_ap_cnt[1] += g_atm.rdAP_B();
  g_ap_cnt[2] += g_atm.rdAP_C(); g_ap_cnt[3] += g_atm.rdAP_T();
  g_an_cnt[0] += g_atm.rdAN_A(); g_an_cnt[1] += g_atm.rdAN_B();
  g_an_cnt[2] += g_atm.rdAN_C(); g_an_cnt[3] += g_atm.rdAN_T();
  g_rp_cnt[0] += g_atm.rdRP_A(); g_rp_cnt[1] += g_atm.rdRP_B();
  g_rp_cnt[2] += g_atm.rdRP_C(); g_rp_cnt[3] += g_atm.rdRP_T();
  g_rn_cnt[0] += g_atm.rdRN_A(); g_rn_cnt[1] += g_atm.rdRN_B();
  g_rn_cnt[2] += g_atm.rdRN_C(); g_rn_cnt[3] += g_atm.rdRN_T();
  g_s_cnt[0]  += g_atm.rdSA_A(); g_s_cnt[1]  += g_atm.rdSA_B();
  g_s_cnt[2]  += g_atm.rdSA_C(); g_s_cnt[3]  += g_atm.rdSA_T();
  g_aph_cnt[0] += g_atm.rdAPH_A(); g_aph_cnt[1] += g_atm.rdAPH_B();
  g_aph_cnt[2] += g_atm.rdAPH_C(); g_aph_cnt[3] += g_atm.rdAPH_T();

  for (int i = 0; i < 4; i++) {
    g_e_ap_Wh[i]   = ticks0p01CF_to_Wh(g_ap_cnt[i]);
    g_e_an_Wh[i]   = ticks0p01CF_to_Wh(g_an_cnt[i]);
    g_e_rp_varh[i] = ticks0p01CF_to_Wh(g_rp_cnt[i]);
    g_e_rn_varh[i] = ticks0p01CF_to_Wh(g_rn_cnt[i]);
    g_e_s_VAh[i]   = ticks0p01CF_to_Wh(g_s_cnt[i]);
    g_e_aph_Wh[i]  = ticks0p01CF_to_Wh(g_aph_cnt[i]);
  }
}

// Periodic LittleFS meter flush: at most every METER_SAVE_INTERVAL_MS, and only
// when energy counters changed (CRC) or meterDirty was set (cal / explicit).
static void serviceMeterAutosave(unsigned long now) {
  if (now - lastMeterTouchMs < METER_SAVE_INTERVAL_MS) return;
  lastMeterTouchMs = now;
  const uint32_t crc = meterEnergyCrc();
  if (!meterDirty && crc == meterSavedEnergyCrc) return;
  if (saveMeterFS()) {
    meterDirty = false;
    meterSavedEnergyCrc = crc;
  }
}

static void energiesToJson(JSONVar& Ephase, JSONVar& Etot) {
  auto to_k = [](uint32_t Wh) -> double { return Wh / 1000.0; };
  auto fill = [&](JSONVar& ph, int i) {
    const double ap = to_k(g_e_ap_Wh[i]);
    const double an = to_k(g_e_an_Wh[i]);
    const double rp = to_k(g_e_rp_varh[i]);
    const double rn = to_k(g_e_rn_varh[i]);
    ph["AP_kWh"]   = ap;
    ph["AN_kWh"]   = an;
    ph["RP_kvarh"] = rp;
    ph["RN_kvarh"] = rn;
    ph["S_kVAh"]   = to_k(g_e_s_VAh[i]);
    ph["AP_harm_kWh"] = to_k(g_e_aph_Wh[i]);
    ph["import_kWh"]    = ap;
    ph["export_kWh"]    = an;
    ph["net_kWh"]       = ap - an;
    ph["import_kvarh"]  = rp;
    ph["export_kvarh"]  = rn;
    ph["net_kvarh"]     = rp - rn;
  };
  for (int i = 0; i < 3; i++) {
    JSONVar ph;
    fill(ph, i);
    Ephase[i] = ph;
  }
  fill(Etot, 3);
}

static void setDefaults() {
  for (int i = 0; i < NUM_RLY; i++) {
    rlyCfg[i].enabled   = true;
    rlyCfg[i].inverted  = false;
    rlyCfg[i].mode      = RLY_MODE_MODBUS;
    rlyCfg[i].alarmCh   = 3;
    rlyCfg[i].alarmMask = 1;
  }
  for (int i = 0; i < NUM_LED; i++) ledCfg[i] = { 0, 0 };
  for (int i = 0; i < NUM_BTN; i++) btnCfg[i] = { 0 };
  for (int i = 0; i < NUM_RLY; i++) desiredRelay[i] = false;
  setAlarmDefaults();
  g_mb_address = 30;
  g_mb_baud    = 19200;
  SlaveId      = (int)g_mb_address;
  setAtmDefaults();
  setMeterDefaults();
}

// Settings-only defaults: never clear energy/cal (those live in enm_meter.bin).
static void setSettingsDefaults() {
  for (int i = 0; i < NUM_RLY; i++) {
    rlyCfg[i].enabled   = true;
    rlyCfg[i].inverted  = false;
    rlyCfg[i].mode      = RLY_MODE_MODBUS;
    rlyCfg[i].alarmCh   = 3;
    rlyCfg[i].alarmMask = 1;
  }
  for (int i = 0; i < NUM_LED; i++) ledCfg[i] = { 0, 0 };
  for (int i = 0; i < NUM_BTN; i++) btnCfg[i] = { 0 };
  for (int i = 0; i < NUM_RLY; i++) desiredRelay[i] = false;
  setAlarmDefaults();
  g_mb_address = 30;
  g_mb_baud    = 19200;
  SlaveId      = (int)g_mb_address;
  g_atm_cfg.lineHz = 50;
  g_atm_cfg.sumAbs = 1;
  g_atm_cfg.wireMode = 0;
  g_atm_cfg.phaseMap[0] = 0;
  g_atm_cfg.phaseMap[1] = 1;
  g_atm_cfg.phaseMap[2] = 2;
}

static void atmApplyFromCfg_NOW() {
  // Soft-reset path — drain unread energy ticks first.
  sampleEnergyCounters();
  M90PhaseCal tmp[3];
  for (int i = 0; i < 3; i++) tmp[i] = g_atm_cfg.cal[i];
  g_atm.begin(g_atm_cfg.lineHz, g_atm_cfg.sumAbs, g_atm_cfg.wireMode, g_atm_cfg.phaseMap,
              g_atm_cfg.ucal, tmp);
}

static void atmApplyCalOnly_NOW() {
  M90PhaseCal tmp[3];
  for (int i = 0; i < 3; i++) tmp[i] = g_atm_cfg.cal[i];
  g_atm.applyCalibration(tmp);
}

static void queueAtmFullApply() {
  atmApplyPending = true;
  atmCalOnlyPending = false;
}

static void queueAtmCalOnlyApply() {
  if (atmApplyPending) return;  // full apply supersedes
  atmCalOnlyPending = true;
}

static void queueAtmApply() {
  queueAtmFullApply();
}

static void applyHoldingFromModbus() {
  bool changed = false;

  const uint16_t addr = mb.Hreg(HR_MB_ADDR);
  if (addr >= 1 && addr <= 247 && addr != g_mb_address) {
    g_mb_address = (uint8_t)addr;
    changed = true;
  }

  const uint32_t baud = ((uint32_t)mb.Hreg(HR_MB_BAUD_L) << 16) | (uint32_t)mb.Hreg(HR_MB_BAUD_L + 1);
  const uint32_t vBaud = hmValidBaud(baud);
  if (vBaud != g_mb_baud) {
    g_mb_baud = vBaud;
    changed = true;
  }

  static uint8_t  lastAddr = 0;
  static uint32_t lastBaud = 0;
  if (g_mb_address != lastAddr || g_mb_baud != lastBaud) {
    applyModbusSettings(g_mb_address, g_mb_baud);
    lastAddr = g_mb_address;
    lastBaud = g_mb_baud;
  }

  const uint16_t hz = mb.Hreg(HR_LINE_HZ);
  if (hz == 50 || hz == 60) {
    const uint16_t newHz = (uint16_t)hz;
    if (g_atm_cfg.lineHz != newHz) {
      g_atm_cfg.lineHz = newHz;
      changed = true;
      queueAtmApply();
    }
  }

  const uint8_t sumAbs = mb.Hreg(HR_SUM_ABS) ? 1 : 0;
  if (g_atm_cfg.sumAbs != sumAbs) {
    g_atm_cfg.sumAbs = sumAbs;
    changed = true;
    queueAtmApply();
  }

  for (int i = 0; i < NUM_RLY; i++) {
    const bool en = mb.Hreg(HR_RLY_EN_BASE + i) != 0;
    if (rlyCfg[i].enabled != en) {
      rlyCfg[i].enabled = en;
      changed = true;
    }
  }

  if (changed) {
    markCfgDirty();
    mbSyncHolding();
  }
}

static JSONVar calPhasesArrayFromCfg() {
  JSONVar cal;
  static const char* phName[] = { "A", "B", "C" };
  for (int i = 0; i < 3; i++) {
    JSONVar p;
    p["Ugain"]   = (int)g_atm_cfg.cal[i].Ugain;
    p["Igain"]   = (int)g_atm_cfg.cal[i].Igain;
    p["Uoffset"] = (int)g_atm_cfg.cal[i].Uoffset;
    p["Ioffset"] = (int)g_atm_cfg.cal[i].Ioffset;
    cal[(int)i] = p;
    cal[phName[i]] = p;
  }
  return cal;
}

// Web-only: numeric indices 0..2 (no duplicate A/B/C keys) for smaller JSON frames.
static JSONVar calPhasesArraySlim() {
  JSONVar cal;
  for (int i = 0; i < 3; i++) {
    JSONVar p;
    p["Ugain"]   = (int)g_atm_cfg.cal[i].Ugain;
    p["Igain"]   = (int)g_atm_cfg.cal[i].Igain;
    p["Uoffset"] = (int)g_atm_cfg.cal[i].Uoffset;
    p["Ioffset"] = (int)g_atm_cfg.cal[i].Ioffset;
    cal[(int)i] = p;
  }
  return cal;
}

static void sendWebCalib() {
  const JSONVar slim = calPhasesArraySlim();
  JSONVar root;
  root["cal"] = slim;
  WebSerial.send("CalibCfg", root);
  yield();
  WebSerial.send("cal", slim);
  yield();
}

static JSONVar energyToJsonObj() {
  JSONVar o;
  JSONVar Ephase, Etot;
  energiesToJson(Ephase, Etot);
  o["E_phase"] = Ephase;
  o["E_tot"]   = Etot;
  o["MC_imp_per_kWh"] = (int)g_MC_imp_per_kWh;
  return o;
}

// v0.1.0 ENM_Sync carried modbus identity, atm options, cal and energies together.
static JSONVar enmSyncToJson() {
  JSONVar o = energyToJsonObj();
  o["address"] = (int)g_mb_address;
  o["baud"]    = (int)g_mb_baud;
  o["fw"]      = HM_FW;
  o["lineHz"]  = (int)g_atm_cfg.lineHz;
  o["sumAbs"]  = (int)g_atm_cfg.sumAbs;
  o["ucal"]    = (int)g_atm_cfg.ucal;
  o["cal"]     = calPhasesArraySlim();
  return o;
}

static JSONVar meterLiveToJson() {
  JSONVar m;
  for (int i = 0; i < 3; i++) {
    m["Urms"][i] = g_urms[i];
    m["Irms"][i] = g_irms[i];
  }

  JSONVar pW, qVar, sVA, pfR, ang;
  for (int i = 0; i < 4; i++) {
    pW[i] = g_p_W[i];
    qVar[i] = g_q_var[i];
    sVA[i] = g_s_VA[i];
    pfR[i] = ((double)g_pf_raw[i]) / 1000.0;
  }
  for (int i = 0; i < 3; i++) ang[i] = ((double)g_ang_raw[i]) / 10.0;

  m["P_W"] = pW;
  m["Q_var"] = qVar;
  m["S_VA"] = sVA;
  m["PF"] = pfR;
  m["Angle_deg"] = ang;
  m["FreqHz"] = ((double)g_f_x100) / 100.0;
  m["TempC"] = (int)g_tempC;
  m["MC_imp_per_kWh"] = (int)g_MC_imp_per_kWh;

  JSONVar uPk, iPk, thd;
  for (int i = 0; i < 3; i++) {
    uPk[i] = g_upeak[i];
    iPk[i] = g_ipeak[i];
    thd[i] = ((double)g_thd_x100[i]) / 100.0;
  }
  m["Upeak_V"] = uPk;
  m["Ipeak"] = iPk;
  m["Ipeak_A"] = iPk;
  m["THD_pct"] = thd;
  m["IrmsN_A"] = g_irmsN;

  JSONVar pfund, pharm;
  for (int i = 0; i < 4; i++) {
    pfund[i] = g_pfund_W[i];
    pharm[i] = g_pharm_W[i];
  }
  m["Pfund"] = pfund;
  m["Pharm"] = pharm;
  m["PfundT"] = g_pfund_W[3];
  m["PharmT"] = g_pharm_W[3];

  // v0.1.0 ENM_Meter carried energies in the same frame; WebConfig reads them here.
  JSONVar Ephase, Etot;
  energiesToJson(Ephase, Etot);
  m["E_phase"] = Ephase;
  m["E_tot"]   = Etot;
  return m;
}

// ================== WebConfig (unified) ==================
static void atmUpdateBaseFromJson(const JSONVar& obj);
static void atmUpdatePhaseFromJson(int phase, const JSONVar& obj);
static void atmApplyFromJson(const JSONVar& obj);
static void queueAtmFullApply();
static void queueAtmCalOnlyApply();
static bool atmJsonNeedsFullApply(const JSONVar& obj);

void applyModbusSettings(uint8_t addr, uint32_t baud) {
  addr = hmValidAddress(addr);
  baud = hmValidBaud(baud);

  // Must run on first boot too — v18 called Serial2.begin() in setup(); only
  // re-initing when baud changed left UART off when loaded baud == 19200.
  if (!g_mbSerialReady || g_mb_baud != baud) {
    Serial2.end();
    Serial2.setTX(TX2);
    Serial2.setRX(RX2);
    Serial2.begin(baud);
    while (Serial2.available()) (void)Serial2.read();
    mb.config(baud);
    g_mb_baud = baud;
    g_mbSerialReady = true;
  }
  setSlaveIdIfAvailable(mb, addr);
  g_mb_address = addr;
  SlaveId = (int)addr;
  mbSyncHolding();
  markCfgDirty();
}

void handleValues(JSONVar values) {
  markWebHostRx();
  const int addr = jvGetInt(values, "mb_address", (int)g_mb_address);
  const int baud = jvGetInt(values, "mb_baud",    (int)g_mb_baud);
  applyModbusSettings((uint8_t)addr, (uint32_t)baud);
  wsLog("Modbus configuration updated");
  sendWebStatus();
}

void handleCommand(JSONVar obj) {
  markWebHostRx();
  const char* actC = (const char*)obj["action"];
  if (!actC) { wsLog("command: missing 'action'"); return; }
  String act = String(actC); act.toLowerCase();
  if (act == "reboot" || act == "reset") {
    wsLog("Rebooting…");
    (void)saveMeterFS();
    delay(50);
    rp2040.reboot();
  } else if (act == "factory") {
    setDefaults();
    if (saveConfigFS()) {
      wsLog("Factory defaults restored & saved");
      sendWebBootstrap();
      applyModbusSettings(g_mb_address, g_mb_baud);
    } else {
      wsLog("ERROR: Save after factory reset failed");
    }
  } else if (act == "hello" || act == "getconfig") {
    sendWebBootstrap();
  } else if (act == "identify") {
    g_identifyUntilMs = millis() + IDENTIFY_MS;
    wsLog("Identify: LEDs active for 5 s");
  } else if (act == "reset_energy") {
    resetEnergyCounters();
  } else {
    wsLog(String("Unknown command: ") + actC);
  }
}

// Contract t: relay, btn, led, ext.atm (+ legacy relayCfg/btnCfg/ledCfg/atm)
void handleUnifiedConfig(JSONVar obj) {
  markWebHostRx();
  const char* t = (const char*)obj["t"];
  JSONVar list = obj["list"];
  if (!t) { wsLog("Config: missing 't'"); return; }

  String type = String(t);
  bool changed = false;

  if (type == "relay" || type == "relays" || type == "relayCfg") {
    if (type == "relayCfg") {
      JSONVar en  = list["enabled"];
      JSONVar inv = list["inverted"];
      for (int i = 0; i < NUM_RLY; i++) {
        if (JSON.typeof(en[i])  != "undefined") rlyCfg[i].enabled  = (bool)en[i];
        if (JSON.typeof(inv[i]) != "undefined") rlyCfg[i].inverted = (bool)inv[i];
      }
    } else {
      for (int i = 0; i < NUM_RLY && i < list.length(); i++) {
        JSONVar item = list[i];
        rlyCfg[i].enabled  = (bool)item["enabled"];
        rlyCfg[i].inverted = (bool)(item.hasOwnProperty("inverted") ? item["inverted"] : item["invert"]);
        if (JSON.typeof(item["mode"]) != "undefined") rlyCfg[i].mode = (uint8_t)constrain(jvGetInt(item, "mode", rlyCfg[i].mode), 0, 2);
        if (JSON.typeof(item["alarmCh"]) != "undefined") rlyCfg[i].alarmCh = (uint8_t)constrain(jvGetInt(item, "alarmCh", rlyCfg[i].alarmCh), 0, 3);
        if (JSON.typeof(item["alarmMask"]) != "undefined") rlyCfg[i].alarmMask = (uint8_t)jvGetInt(item, "alarmMask", rlyCfg[i].alarmMask);
      }
    }
    wsLog("Relay Configuration updated");
    changed = true;

  } else if (type == "alarm" || type == "alarms" || type == "AlarmsCfg") {
    alarmApplyFromJson(list);
    wsLog("Alarm configuration updated");
    changed = true;

  } else if (type == "btn" || type == "buttons" || type == "btnCfg") {
    JSONVar a = (type == "btnCfg") ? list["action"] : list;
    for (int i = 0; i < NUM_BTN; i++) {
      if (JSON.typeof(a[i]) == "undefined") continue;
      int v = a[i].hasOwnProperty("action") ? (int)a[i]["action"] : (int)a[i];
      btnCfg[i].action = (uint8_t)((v==0 || v==5 || v==6) ? v : 0);
    }
    wsLog("Buttons Configuration updated");
    changed = true;

  } else if (type == "led" || type == "leds" || type == "ledCfg") {
    JSONVar m = (type == "ledCfg") ? list["mode"] : JSONVar();
    JSONVar s = (type == "ledCfg") ? list["source"] : JSONVar();
    for (int i = 0; i < NUM_LED; i++) {
      if (type == "ledCfg") {
        if (JSON.typeof(m[i]) != "undefined") ledCfg[i].mode = (uint8_t)constrain((int)m[i], 0, 1);
        if (JSON.typeof(s[i]) != "undefined") {
          int sv = (int)s[i];
          ledCfg[i].source = (uint8_t)((sv==0 || sv==5 || sv==6) ? sv : 0);
        }
      } else if (i < list.length()) {
        ledCfg[i].mode   = (uint8_t)constrain((int)list[i]["mode"],   0, 1);
        int src          = (int)list[i]["source"];
        ledCfg[i].source = (uint8_t)((src==0 || src==5 || src==6) ? src : 0);
      }
    }
    wsLog("LEDs Configuration updated");
    changed = true;

  } else if (type == "ext.atm" || type == "atm") {
    JSONVar atmObj = list;
    if (JSON.typeof(list) == "array" && list.length() > 0) atmObj = list[0];
    const bool full = atmJsonNeedsFullApply(atmObj);
    atmApplyFromJson(atmObj);
    if (full) {
      queueAtmFullApply();
      wsLog("OK: ATM full apply queued");
    } else {
      queueAtmCalOnlyApply();
      wsLog("OK: ATM calibration queued");
    }
    changed = true;
    markMeterDirty();

  } else {
    wsLog(String("Unknown Config type: ") + t);
  }

  if (changed) {
    markCfgDirty();
    sendWebCfg();
    if (type == "alarm" || type == "alarms" || type == "AlarmsCfg")
      sendWebExt();
  }
}

void sendWebStatus() {
  const bool linkOk = linkOkNow((uint32_t)millis());
  JSONVar st;
  st["model"] = HM_MODEL_ID;
  st["fw"]    = HM_FW;
  st["map"]   = HM_MAP;
  st["addr"]  = g_mb_address;
  st["baud"]  = g_mb_baud;
  st["linkOk"] = linkOk ? 1 : 0;
  WebSerial.send("status", st);
  yield();
}

void sendWebCfg() {
  sendWebCfgCore();
  sendWebCalib();
}

static void sendWebCfgCore() {
  JSONVar cfg;
  for (int i = 0; i < NUM_RLY; i++) {
    cfg["relay"][i]["enabled"]   = rlyCfg[i].enabled ? 1 : 0;
    cfg["relay"][i]["invert"]    = rlyCfg[i].inverted ? 1 : 0;
    cfg["relay"][i]["inverted"]  = rlyCfg[i].inverted ? 1 : 0;
    cfg["relay"][i]["mode"]      = (int)rlyCfg[i].mode;
    cfg["relay"][i]["alarmCh"]   = (int)rlyCfg[i].alarmCh;
    cfg["relay"][i]["alarmMask"] = (int)rlyCfg[i].alarmMask;
  }
  for (int i = 0; i < NUM_BTN; i++) {
    cfg["btn"][i]["action"] = btnCfg[i].action;
  }
  for (int i = 0; i < NUM_LED; i++) {
    cfg["led"][i]["mode"]   = ledCfg[i].mode;
    cfg["led"][i]["source"] = ledCfg[i].source;
  }
  cfg["ext"]["atm"]["lineHz"] = (int)g_atm_cfg.lineHz;
  cfg["ext"]["atm"]["sumAbs"] = (int)g_atm_cfg.sumAbs;
  cfg["ext"]["atm"]["wireMode"] = (int)g_atm_cfg.wireMode;
  JSONVar pmap;
  for (int i = 0; i < 3; i++) pmap[i] = (int)g_atm_cfg.phaseMap[i];
  cfg["ext"]["atm"]["phaseMap"] = pmap;
  cfg["ext"]["atm"]["ucal"]   = (int)g_atm_cfg.ucal;
  // cal via CalibCfg / cal / ENM_Sync / ext — keep cfg lean (alarm block is large).
  cfg["alarm"] = alarmCfgToJson();
  WebSerial.send("cfg", cfg);
  yield();
}

static void sendWebExt() {
  // Separate frames: one fat "ext" JSON often fails over USB WebSerial; ENM_Meter/ENM_Sync are proven.
  WebSerial.send("ENM_Sync", enmSyncToJson());
  yield();
  if (g_haveMeter) {
    WebSerial.send("ENM_Meter", meterLiveToJson());
    yield();
  }
  JSONVar ext;
  // NOTE: Arduino_JSON JSONVar temporaries can serialize as null when assigned
  // directly into nested objects; keep locals to ensure stable values.
  JSONVar atm;
  JSONVar cal = calPhasesArraySlim();
  atm["cal"] = cal;
  ext["atm"] = atm;

  JSONVar alarms = alarmsStateToJson();
  ext["alarms"] = alarms;

  JSONVar chipEv;
  for (int i = 0; i < 4; i++) chipEv[i] = (int)g_chipEvMask[i];
  ext["chipEv"] = chipEv;
  WebSerial.send("ext", ext);
  yield();
}

static void resetEnergyCounters() {
  if (!atmBusy) drainChipEnergyRegs();
  clearEnergyCounters();
  markMeterDirty();
  if (saveMeterFS()) {
    meterDirty = false;
    meterSavedEnergyCrc = meterEnergyCrc();
    wsLog("Energy counters reset");
  } else {
    wsLog("ERROR: Energy reset save failed");
  }
  mbPublishMeter();
  sendWebExt();
}

static void sendWebIo(const bool* relayLogical, const bool* buttonState, const bool* ledPhysState) {
  JSONVar io;
  for (int i = 0; i < NUM_RLY; i++) io["relay"][i] = relayLogical[i] ? 1 : 0;
  for (int i = 0; i < NUM_BTN; i++) io["btn"][i] = buttonState[i] ? 1 : 0;
  for (int i = 0; i < NUM_LED; i++) io["led"][i] = ledPhysState[i] ? 1 : 0;
  WebSerial.send("io", io);
  yield();
}

// Small frames: status / io rotate; meter+energy via ENM_* on 1 Hz tick; alarms via ext.
static void serviceWebTelemetry(unsigned long now, const bool* relayLogical, const bool* buttonState, const bool* ledPhysState) {
  if (now - lastWebFrameMs < webFrameIntervalMs) return;
  lastWebFrameMs = now;

  if (webFramePhase == 0) sendWebStatus();
  else sendWebIo(relayLogical, buttonState, ledPhysState);
  webFramePhase = (uint8_t)(!webFramePhase);
}

static void serviceMeterWeb(unsigned long now) {
  if (now - lastMeterWebMs < meterWebIntervalMs) return;
  lastMeterWebMs = now;
  sendWebExt();
}

// v0.1.0 pushWebConfigPeriodic: status + cfg in small steps with yield between frames.
static void pushWebConfigPeriodic() {
  sendWebStatus();
  yield();
  sendWebCfg();
}

void sendWebBootstrap() {
  sendWebStatus();
  yield();
  sendWebExt();
  yield();
  sendWebCfg();
}

static void atmUpdateBaseFromJson(const JSONVar& obj) {
  if (obj.hasOwnProperty("lineHz")) {
    int hz = jvGetInt(obj, "lineHz", (int)g_atm_cfg.lineHz);
    g_atm_cfg.lineHz = (hz == 60) ? 60 : 50;
  }
  if (obj.hasOwnProperty("sumAbs")) {
    g_atm_cfg.sumAbs = jvGetInt(obj, "sumAbs", (int)g_atm_cfg.sumAbs) ? 1 : 0;
  }
  if (obj.hasOwnProperty("wireMode")) {
    g_atm_cfg.wireMode = jvGetInt(obj, "wireMode", (int)g_atm_cfg.wireMode) ? 1 : 0;
  }
  if (obj.hasOwnProperty("phaseMap")) {
    JSONVar pmap = obj["phaseMap"];
    uint8_t tmp[3];
    for (int i = 0; i < 3; i++) {
      JSONVar v = pmap[i];
      if (JSON.typeof(v) == "undefined") v = pmap[(int)i];
      tmp[i] = (uint8_t)constrain(jsonVarToInt(v, (int)g_atm_cfg.phaseMap[i]), 0, 2);
    }
    if (validatePhaseMap(tmp)) {
      for (int i = 0; i < 3; i++) g_atm_cfg.phaseMap[i] = tmp[i];
    }
  }
  if (obj.hasOwnProperty("ucal")) {
    int u = jvGetInt(obj, "ucal", (int)g_atm_cfg.ucal);
    if (u < 1) u = 1;
    if (u > 65535) u = 65535;
    g_atm_cfg.ucal = (uint16_t)u;
  }
}
static void atmUpdatePhaseFromJson(int phase, const JSONVar& obj) {
  if (phase < 0 || phase > 2) return;
  if (obj.hasOwnProperty("Ugain"))   g_atm_cfg.cal[phase].Ugain   = clamp_u16(jvGetInt(obj, "Ugain",   (int)g_atm_cfg.cal[phase].Ugain));
  if (obj.hasOwnProperty("Igain"))   g_atm_cfg.cal[phase].Igain   = clamp_u16(jvGetInt(obj, "Igain",   (int)g_atm_cfg.cal[phase].Igain));
  if (obj.hasOwnProperty("Uoffset")) g_atm_cfg.cal[phase].Uoffset = clamp_i16(jvGetInt(obj, "Uoffset", (int)g_atm_cfg.cal[phase].Uoffset));
  if (obj.hasOwnProperty("Ioffset")) g_atm_cfg.cal[phase].Ioffset = clamp_i16(jvGetInt(obj, "Ioffset", (int)g_atm_cfg.cal[phase].Ioffset));
}

static bool atmJsonNeedsFullApply(const JSONVar& obj) {
  // Structural options require SoftReset via begin(). Calibration/offset alone do not.
  // ucal feeds sag/OV thresholds computed inside begin() — treat as structural.
  return obj.hasOwnProperty("lineHz") || obj.hasOwnProperty("sumAbs")
      || obj.hasOwnProperty("wireMode") || obj.hasOwnProperty("phaseMap")
      || obj.hasOwnProperty("ucal");
}

static void atmApplyFromJson(const JSONVar& obj) {
  if (atmJsonNeedsFullApply(obj)) {
    atmUpdateBaseFromJson(obj);
  }
  if (obj.hasOwnProperty("ph")) {
    const int ph = jvGetInt(obj, "ph", 0);
    if (ph >= 0 && ph <= 2) atmUpdatePhaseFromJson(ph, obj);
  }
  if (obj.hasOwnProperty("cal")) {
    JSONVar cal = obj["cal"];
    for (int i = 0; i < 3; i++) {
      JSONVar p = cal[i];
      if (JSON.typeof(p) == "undefined") continue;
      atmUpdatePhaseFromJson(i, p);
    }
  }
}

static void wsDiagPrintRawAtm(uint32_t now) {
  if (now - lastWsDiagPrint < wsDiagPrintMs) return;
  lastWsDiagPrint = now;
  if (atmBusy || meter_job) return;

  // Raw diagnostics: read directly from chip registers to confirm detector state.
  const uint16_t emm0 = g_atm.debugRead16(0x71);
  const uint16_t emm1 = g_atm.debugRead16(0x72);
  const uint16_t int0 = g_atm.debugRead16(0x73);
  const uint16_t int1 = g_atm.debugRead16(0x74);

  const uint16_t uPkA = g_atm.debugRead16(0xF1);
  const uint16_t uPkB = g_atm.debugRead16(0xF2);
  const uint16_t uPkC = g_atm.debugRead16(0xF3);
  const uint16_t iPkA = g_atm.debugRead16(0xF5);
  const uint16_t iPkB = g_atm.debugRead16(0xF6);
  const uint16_t iPkC = g_atm.debugRead16(0xF7);

  const uint16_t sagTh = g_atm.debugRead16(0x08);
  const uint16_t ovTh  = g_atm.debugRead16(0x06);
  const uint16_t plTh  = g_atm.debugRead16(0x09);
  const uint16_t oiTh  = g_atm.debugRead16(0x0B);
  const uint16_t fHiTh = g_atm.debugRead16(0x0D);
  const uint16_t fLoTh = g_atm.debugRead16(0x0C);

  const uint8_t m0 = g_chipEvMask[0];
  const uint8_t m1 = g_chipEvMask[1];
  const uint8_t m2 = g_chipEvMask[2];
  const uint8_t m3 = g_chipEvMask[3];
  JSONVar ev = chipEvMasksToJson();
  String evSer = JSON.stringify(ev);

  char buf[320];
  snprintf(buf, sizeof(buf),
           "DIAG EMM0=%04X EMM1=%04X INT0=%04X INT1=%04X | "
           "UPk=%04X/%04X/%04X IPk=%04X/%04X/%04X | "
           "SagTh=%04X OVth=%04X PLth=%04X OIth=%04X FHi=%04X FLo=%04X | "
           "mask=%02X/%02X/%02X/%02X ev=%s",
           emm0, emm1, int0, int1,
           uPkA, uPkB, uPkC, iPkA, iPkB, iPkC,
           sagTh, ovTh, plTh, oiTh, fHiTh, fLoTh,
           m0, m1, m2, m3,
           evSer.c_str());
  wsLog(buf);
}

void setup() {
  Serial.begin(57600);

  if (!LittleFS.begin()) {
    LittleFS.format();
    LittleFS.begin();
  }
  if (!loadConfigFS()) setDefaults();

  for (uint8_t i=0;i<NUM_RLY;i++) { pinMode(RELAY_PINS[i], OUTPUT); digitalWrite(RELAY_PINS[i], LOW); }
  for (uint8_t i=0;i<NUM_LED;i++) { pinMode(LED_PINS[i],   OUTPUT); digitalWrite(LED_PINS[i],   LOW); }
  for (uint8_t i=0;i<NUM_BTN;i++) pinMode(BTN_PINS[i], INPUT); // HIGH=pressed (field board)

  Serial2.setTX(TX2);
  Serial2.setRX(RX2);
  applyModbusSettings(g_mb_address, g_mb_baud);
  mb.setAdditionalServerData("ENM223-ENM");
  mbBuildRegisterMap();
  hmRegisterIdentity(mb, HM_MODEL_ID, HM_FW_MAJOR, HM_FW_MINOR, HM_FW_PATCH, HM_MAP_VERSION);
  mbSyncHolding();
  g_lastLinkSeenMs = millis();

  // SimpleWebSerial allows max 8 events — do not add more without changing the library.
  WebSerial.on("values",  handleValues);
  WebSerial.on("Config",  handleUnifiedConfig);
  WebSerial.on("command", handleCommand);
  WebSerial.on("AlarmsCfg", handleAlarmsCfg);
  WebSerial.on("AlarmsAck", handleAlarmsAck);

  SPI1.setSCK(ATM_SCK);
  SPI1.setTX(ATM_MOSI);
  SPI1.setRX(ATM_MISO);
  SPI1.begin();

  atmBusy = true;
  atmApplyFromCfg_NOW();
  atmBusy = false;
  sampleEnergyCounters();
  meterSavedEnergyCrc = meterEnergyCrc();
  lastMeterTouchMs = millis();
  mbPublishMeter();

  wsLog("Boot OK " HM_FW " (config saved to flash)");
  sendWebBootstrap();
  lastMeterSample = millis();
  lastEnergySample = millis();
  hmWatchdogArm(4000);
}

void loop() {
  hmWatchdogFeed();
  const unsigned long now = millis();

  mb.task();
  WebSerial.check();
  yield();

  if (!atmBusy && !meter_job && (now - lastPeakResetMs >= peakResetMs)) {
    lastPeakResetMs = now;
    g_atm.resetPeakRegisters();
  }

  updateLinkOkDetector((uint32_t)now);
  mbUpdateStatusFlags((uint32_t)now);

  serviceModbusAck();
  serviceModbusRelays();
  serviceModbusServiceCoils((uint32_t)now);
  alarmServiceTick(now);
  wsDiagPrintRawAtm((uint32_t)now);
  applyHoldingFromModbus();

  if (atmApplyPending && !atmBusy && (now - atmLastApplyMs >= atmApplyMinIntervalMs)) {
    atmApplyPending = false;
    atmCalOnlyPending = false;
    atmBusy = true;
    atmApplyFromCfg_NOW();
    atmBusy = false;
    atmLastApplyMs = now;
    markCfgDirty();
    markMeterDirty();
    wsLog("OK: ATM applied (full begin)");
    sendWebCfg();
  } else if (atmCalOnlyPending && !atmBusy && (now - atmLastApplyMs >= atmApplyMinIntervalMs)) {
    atmCalOnlyPending = false;
    atmBusy = true;
    atmApplyCalOnly_NOW();
    atmBusy = false;
    atmLastApplyMs = now;
    markCfgDirty();
    markMeterDirty();
    wsLog("OK: ATM calibration applied");
    sendWebCfg();
  }

  if (cfgDirty && (now - lastCfgTouchMs >= CFG_AUTOSAVE_MS)) {
    cfgDirty = false;
    saveSettingsFS();
  }

  serviceMeterAutosave(now);

  if (now - lastBlinkToggle >= blinkPeriodMs) {
    lastBlinkToggle = now;
    blinkPhase = !blinkPhase;
  }

  bool relayLogical[NUM_RLY];
  for (int i = 0; i < NUM_BTN; i++) {
    const bool rawPressed = (digitalRead(BTN_PINS[i]) == HIGH);
    serviceDebounce(btnDeb[i], rawPressed, now);
    const bool pressed = btnDeb[i].stable;
    buttonState[i] = pressed;
    const bool rising = (!btnDeb[i].prevStable && btnDeb[i].stable);
    if (rising) {
      uint8_t act = btnCfg[i].action;
      if (act == 5 || act == 6) {
        int r = act - 5;
        if (r >= 0 && r < NUM_RLY) desiredRelay[r] = !desiredRelay[r];
      }
    }
    mb.setIsts(DI_BTN_BASE + i, pressed);
  }

  for (int i = 0; i < NUM_RLY; i++) {
    bool logical = false;
    if (!rlyCfg[i].enabled) {
      logical = false;
    } else if (rlyCfg[i].mode == RLY_MODE_NONE) {
      logical = false;
    } else if (rlyCfg[i].mode == RLY_MODE_ALARM) {
      logical = relayAlarmDemand((uint8_t)i);
    } else {
      logical = desiredRelay[i];
    }
    bool phys = logical;
    if (rlyCfg[i].inverted) phys = !phys;
    digitalWrite(RELAY_PINS[i], phys ? HIGH : LOW);
    relayLogical[i] = logical;
    mb.setIsts(DI_RELAY_BASE + i, logical);
  }
  if (rlyCfg[0].mode == RLY_MODE_MODBUS && mb.Coil(COIL_RELAY1) != desiredRelay[0]) mb.Coil(COIL_RELAY1, desiredRelay[0]);
  if (rlyCfg[1].mode == RLY_MODE_MODBUS && mb.Coil(COIL_RELAY2) != desiredRelay[1]) mb.Coil(COIL_RELAY2, desiredRelay[1]);

  bool ledPhysState[NUM_LED];
  for (int i = 0; i < NUM_LED; i++) {
    bool physLed;
    const bool identifying = g_identifyUntilMs && ((int32_t)(now - g_identifyUntilMs) < 0);
    if (identifying) {
      physLed = blinkPhase;
    } else {
      bool srcActive = false;
      uint8_t src = ledCfg[i].source;
      if (src == 5 || src == 6) {
        int r = src - 5;
        srcActive = (r >= 0 && r < NUM_RLY) ? relayLogical[r] : false;
      }
      physLed = (ledCfg[i].mode == 0) ? srcActive : (srcActive && blinkPhase);
    }
    ledPhysState[i] = physLed;
    digitalWrite(LED_PINS[i], physLed ? HIGH : LOW);
    mb.setIsts(DI_LED_BASE + i, physLed);
  }

  if (!meter_job && !atmBusy && (now - lastMeterSample >= meterSampleMs)) {
    lastMeterSample = now;
    meter_job_begin();
  }
  meter_job_step();

  if (!atmBusy && !meter_job && (now - lastEnergySample >= energySampleMs)) {
    lastEnergySample = now;
    sampleEnergyCounters();
    mbPublishMeter();
  }

  serviceWebTelemetry(now, relayLogical, buttonState, ledPhysState);
  serviceMeterWeb(now);

  if (now - lastBootstrap >= bootstrapInterval) {
    lastBootstrap = now;
    pushWebConfigPeriodic();
  }
}
