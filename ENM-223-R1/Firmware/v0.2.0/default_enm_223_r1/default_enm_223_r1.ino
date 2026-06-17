#include <Arduino.h>
#include <ModbusSerial.h>
#include "hm_common.h"
#define HM_MODEL_ID   2
#define HM_FW_MAJOR   0
#define HM_FW_MINOR   2
#define HM_FW_PATCH   0
#define HM_FW         "0.2.0"
#define HM_MAP        1
#define HM_MAP_VERSION 1
#include <SimpleWebSerial.h>
#include <Arduino_JSON.h>
#include <LittleFS.h>
#include <cstring>
#include <math.h>

#include "atm90e32.h"

// LittleFS blob — must be before any function using EnmPersistCfg (Arduino inserts prototypes at top).
struct EnmPersistCfg {
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
static const uint16_t CFG_VERSION     = 0x0001;
static const char*    CFG_PATH        = "/enm_cfg.bin";
static volatile bool  cfgDirty        = false;
static unsigned long  lastCfgTouchMs  = 0;
static const uint32_t CFG_AUTOSAVE_MS = 1500;

// SimpleWebSerial: MaximumNumberOfEvents = 8 (library default). Never register more than 8 handlers.
static SimpleWebSerial WebSerial;
static inline void wsLog(const char* msg) { WebSerial.send("log", msg); }
static inline void wsLog(const String& msg) { WebSerial.send("log", msg); }

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

struct RlyCfg { bool enabled; bool inverted; };
struct LedCfg { uint8_t mode; uint8_t source; };
struct BtnCfg { uint8_t action; };

static RlyCfg rlyCfg[NUM_RLY];
static LedCfg ledCfg[NUM_LED];
static BtnCfg btnCfg[NUM_BTN];

static bool buttonState[NUM_BTN]  = {false,false,false,false};
static bool buttonPrev[NUM_BTN]   = {false,false,false,false};
static bool desiredRelay[NUM_RLY] = {false,false};

static unsigned long lastSend = 0;
static const unsigned long sendInterval = 1000;
static bool webHostWasConnected = false;

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
  uint16_t ucal;
  M90PhaseCal cal[3];
};
static AtmCfg g_atm_cfg;

static volatile bool atmApplyPending = false;
static unsigned long atmLastApplyMs = 0;
static const unsigned long atmApplyMinIntervalMs = 300;
static bool atmBusy = false;


// Modbus V2 map (matches default_enm_223_r1_plc_full.yaml / old firmware)
enum : uint16_t {
  COIL_RELAY1 = 0,
  COIL_RELAY2 = 1,
  DI_LED_BASE   = 0,
  DI_BTN_BASE   = 4,
  DI_RELAY_BASE = 8,
  IR_URMS_BASE  = 0,
  IR_IRMS_BASE  = 3,
  IR_FREQ       = 6,
  IR_TEMP       = 7,
  IR_PF_BASE    = 8,
  IR_P_BASE     = 20,
  IR_Q_BASE     = 28,
  IR_S_BASE     = 36,
  IR_ANG_BASE   = 44,
  IR_E_BASE     = 60,
  HR_MB_ADDR    = 0,
  HR_MB_BAUD_L  = 1
};

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
  mb.Hreg(4, g_atm_cfg.lineHz);
  mb.Hreg(5, g_atm_cfg.sumAbs);
  mb.Hreg(7, rlyCfg[0].enabled ? 1 : 0);
  mb.Hreg(8, rlyCfg[1].enabled ? 1 : 0);
}

static void serviceModbusRelays() {
  bool c0 = mb.Coil(COIL_RELAY1);
  bool c1 = mb.Coil(COIL_RELAY2);
  if (c0 != desiredRelay[0]) desiredRelay[0] = c0;
  if (c1 != desiredRelay[1]) desiredRelay[1] = c1;
  if (mb.Coil(COIL_RELAY1) != desiredRelay[0]) mb.Coil(COIL_RELAY1, desiredRelay[0]);
  if (mb.Coil(COIL_RELAY2) != desiredRelay[1]) mb.Coil(COIL_RELAY2, desiredRelay[1]);
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
  g_atm_cfg.ucal   = 36000;  // sag detector reference
  for (int i = 0; i < 3; i++) {
    g_atm_cfg.cal[i].Ugain   = 39500;  // divider 6×220k+1k, calibrated @ 230V
    g_atm_cfg.cal[i].Igain   = 49000;  // ZEMCTK05 + PGA=2×, 50A full-scale
    g_atm_cfg.cal[i].Uoffset = 0;
    g_atm_cfg.cal[i].Ioffset = 0;
  }
}

// ---- Meter cache + chunked SPI sampling (one step per loop) ----
static double   g_urms[3] = {0, 0, 0};
static double   g_irms[3] = {0, 0, 0};
static int32_t  g_p_W[4]  = {0, 0, 0, 0};
static int32_t  g_q_var[4]= {0, 0, 0, 0};
static int32_t  g_s_VA[4] = {0, 0, 0, 0};
static int16_t  g_pf_raw[4] = {0, 0, 0, 0};
static int16_t  g_ang_raw[3]= {0, 0, 0};
static uint16_t g_f_x100 = 0;
static int16_t  g_tempC = 0;
static bool     g_haveMeter = false;

static uint32_t g_e_ap_Wh[4] = {0}, g_e_an_Wh[4] = {0}, g_e_rp_varh[4] = {0};
static uint32_t g_e_rn_varh[4] = {0}, g_e_s_VAh[4] = {0};
static uint64_t g_ap_cnt[4] = {0}, g_an_cnt[4] = {0}, g_rp_cnt[4] = {0};
static uint64_t g_rn_cnt[4] = {0}, g_s_cnt[4]  = {0};
static uint32_t g_MC_imp_per_kWh = 3200;

static inline uint32_t ticks0p01CF_to_Wh(uint64_t ticks);

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

static void captureToPersist(EnmPersistCfg& pc) {
  memset(&pc, 0, sizeof(pc));
  pc.magic      = CFG_MAGIC;
  pc.version    = CFG_VERSION;
  pc.size       = sizeof(EnmPersistCfg);
  pc.mb_address = g_mb_address;
  pc.mb_baud    = g_mb_baud;
  pc.lineHz     = g_atm_cfg.lineHz;
  pc.sumAbs     = g_atm_cfg.sumAbs;
  pc.ucal       = g_atm_cfg.ucal;
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
  pc.crc32 = 0;
  pc.crc32 = crc32_update(0, reinterpret_cast<const uint8_t*>(&pc), sizeof(EnmPersistCfg));
}

static bool applyFromPersist(const EnmPersistCfg& pc) {
  if (pc.magic != CFG_MAGIC || pc.version != CFG_VERSION || pc.size != sizeof(EnmPersistCfg))
    return false;
  EnmPersistCfg tmp = pc;
  const uint32_t crc = tmp.crc32;
  tmp.crc32 = 0;
  if (crc32_update(0, reinterpret_cast<const uint8_t*>(&tmp), sizeof(EnmPersistCfg)) != crc)
    return false;

  g_mb_address = pc.mb_address;
  if (g_mb_address < 1 || g_mb_address > 247) g_mb_address = 30;
  g_mb_baud = pc.mb_baud;
  if (!isAllowedBaud(g_mb_baud)) g_mb_baud = 19200;
  SlaveId = (int)g_mb_address;

  g_atm_cfg.lineHz = (pc.lineHz == 60) ? 60 : 50;
  g_atm_cfg.sumAbs = pc.sumAbs ? 1 : 0;
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
  return true;
}

static bool saveConfigFS() {
  EnmPersistCfg pc;
  captureToPersist(pc);
  File f = LittleFS.open(CFG_PATH, "w");
  if (!f) return false;
  const size_t n = f.write(reinterpret_cast<const uint8_t*>(&pc), sizeof(pc));
  f.close();
  return n == sizeof(pc);
}

static bool loadConfigFS() {
  File f = LittleFS.open(CFG_PATH, "r");
  if (!f) return false;
  if (f.size() != sizeof(EnmPersistCfg)) {
    f.close();
    return false;
  }
  EnmPersistCfg pc;
  const size_t n = f.read(reinterpret_cast<uint8_t*>(&pc), sizeof(pc));
  f.close();
  if (n != sizeof(pc)) return false;
  return applyFromPersist(pc);
}

static bool     meter_job = false;
static uint8_t  meter_step = 0;
static double   urms_tmp[3], irms_tmp[3];

static unsigned long lastMeterSample = 0;
static const unsigned long meterSampleMs = 1000;
static unsigned long lastEnergySample = 0;
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
  }

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

      for (int i = 0; i < 3; i++) {
        double pf = ((double)g_pf_raw[i]) / 1000.0;
        if (pf > 1.0) pf = 1.0;
        if (pf < -1.0) pf = -1.0;
        double S = g_urms[i] * g_irms[i];
        double P = S * pf;
        double q2 = (S * S) - (P * P);
        if (q2 < 0) q2 = 0;
        double Q = sqrt(q2);
        g_s_VA[i]  = (int32_t)lround(S);
        g_p_W[i]   = (int32_t)lround(P);
        g_q_var[i] = (int32_t)lround(Q);
      }
      g_s_VA[3]  = g_s_VA[0] + g_s_VA[1] + g_s_VA[2];
      g_p_W[3]   = g_p_W[0] + g_p_W[1] + g_p_W[2];
      g_q_var[3] = g_q_var[0] + g_q_var[1] + g_q_var[2];
      meter_job = false;
      g_haveMeter = true;
      mbPublishMeter();
      break;
    }
    default:
      meter_job = false;
      break;
  }
}

static void sampleEnergyCounters() {
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

  for (int i = 0; i < 4; i++) {
    g_e_ap_Wh[i]   = ticks0p01CF_to_Wh(g_ap_cnt[i]);
    g_e_an_Wh[i]   = ticks0p01CF_to_Wh(g_an_cnt[i]);
    g_e_rp_varh[i] = ticks0p01CF_to_Wh(g_rp_cnt[i]);
    g_e_rn_varh[i] = ticks0p01CF_to_Wh(g_rn_cnt[i]);
    g_e_s_VAh[i]   = ticks0p01CF_to_Wh(g_s_cnt[i]);
  }
  markCfgDirty();
}

static void energiesToJson(JSONVar& Ephase, JSONVar& Etot) {
  auto to_k = [](uint32_t Wh) -> double { return Wh / 1000.0; };
  for (int i = 0; i < 3; i++) {
    JSONVar ph;
    ph["AP_kWh"]   = to_k(g_e_ap_Wh[i]);
    ph["AN_kWh"]   = to_k(g_e_an_Wh[i]);
    ph["RP_kvarh"] = to_k(g_e_rp_varh[i]);
    ph["RN_kvarh"] = to_k(g_e_rn_varh[i]);
    ph["S_kVAh"]   = to_k(g_e_s_VAh[i]);
    Ephase[i] = ph;
  }
  Etot["AP_kWh"]   = to_k(g_e_ap_Wh[3]);
  Etot["AN_kWh"]   = to_k(g_e_an_Wh[3]);
  Etot["RP_kvarh"] = to_k(g_e_rp_varh[3]);
  Etot["RN_kvarh"] = to_k(g_e_rn_varh[3]);
  Etot["S_kVAh"]   = to_k(g_e_s_VAh[3]);
}

static void setDefaults() {
  for (int i = 0; i < NUM_RLY; i++) rlyCfg[i] = { true, false };
  for (int i = 0; i < NUM_LED; i++) ledCfg[i] = { 0, 0 };
  for (int i = 0; i < NUM_BTN; i++) btnCfg[i] = { 0 };
  for (int i = 0; i < NUM_RLY; i++) desiredRelay[i] = false;
  g_mb_address = 30;
  g_mb_baud    = 19200;
  SlaveId      = (int)g_mb_address;
  setAtmDefaults();
}

static void atmApplyFromCfg_NOW() {
  M90PhaseCal tmp[3];
  for (int i = 0; i < 3; i++) tmp[i] = g_atm_cfg.cal[i];
  g_atm.begin(g_atm_cfg.lineHz, g_atm_cfg.sumAbs, g_atm_cfg.ucal, tmp);
}

static void queueAtmApply() {
  atmApplyPending = true;
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

static JSONVar energyToJsonObj() {
  JSONVar o;
  JSONVar Ephase, Etot;
  energiesToJson(Ephase, Etot);
  o["E_phase"] = Ephase;
  o["E_tot"]   = Etot;
  o["MC_imp_per_kWh"] = (int)g_MC_imp_per_kWh;
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

  JSONVar Ephase, Etot;
  energiesToJson(Ephase, Etot);
  m["E_phase"] = Ephase;
  m["E_tot"] = Etot;
  return m;
}

// ================== WebConfig (unified) ==================
static void atmUpdateBaseFromJson(const JSONVar& obj);
static void atmUpdatePhaseFromJson(int phase, const JSONVar& obj);
static void atmApplyFromJson(const JSONVar& obj);

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
  const int addr = jvGetInt(values, "mb_address", (int)g_mb_address);
  const int baud = jvGetInt(values, "mb_baud",    (int)g_mb_baud);
  applyModbusSettings((uint8_t)addr, (uint32_t)baud);
  wsLog("Modbus configuration updated");
  sendWebStatus();
}

void handleCommand(JSONVar obj) {
  const char* actC = (const char*)obj["action"];
  if (!actC) { wsLog("command: missing 'action'"); return; }
  String act = String(actC); act.toLowerCase();
  if (act == "reboot" || act == "reset") {
    wsLog("Rebooting…");
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
  } else if (act == "identify") {
    g_identifyUntilMs = millis() + IDENTIFY_MS;
    wsLog("Identify: LEDs active for 5 s");
  } else {
    wsLog(String("Unknown command: ") + actC);
  }
}

// Contract t: relay, btn, led, ext.atm (+ legacy relayCfg/btnCfg/ledCfg/atm)
void handleUnifiedConfig(JSONVar obj) {
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
        rlyCfg[i].enabled  = (bool)list[i]["enabled"];
        rlyCfg[i].inverted = (bool)list[i]["inverted"];
      }
    }
    wsLog("Relay Configuration updated");
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
    atmApplyFromJson(atmObj);
    queueAtmApply();
    wsLog("OK: ATM queued");
    changed = true;

  } else {
    wsLog(String("Unknown Config type: ") + t);
  }

  if (changed) {
    markCfgDirty();
    sendWebCfg();
  }
}

void sendWebStatus() {
  JSONVar st;
  st["model"] = HM_MODEL_ID;
  st["fw"]    = HM_FW;
  st["map"]   = HM_MAP;
  st["addr"]  = g_mb_address;
  st["baud"]  = g_mb_baud;
  WebSerial.send("status", st);
}

void sendWebCfg() {
  JSONVar cfg;
  for (int i = 0; i < NUM_RLY; i++) {
    cfg["relay"][i]["enabled"] = rlyCfg[i].enabled ? 1 : 0;
    cfg["relay"][i]["invert"]  = rlyCfg[i].inverted ? 1 : 0;
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
  cfg["ext"]["atm"]["ucal"]   = (int)g_atm_cfg.ucal;
  cfg["ext"]["atm"]["cal"]    = calPhasesArrayFromCfg();
  JSONVar energy = energyToJsonObj();
  cfg["ext"]["energy"]["E_phase"] = energy["E_phase"];
  cfg["ext"]["energy"]["E_tot"]   = energy["E_tot"];
  cfg["ext"]["energy"]["MC_imp_per_kWh"] = energy["MC_imp_per_kWh"];
  WebSerial.send("cfg", cfg);
}

void sendWebBootstrap() {
  sendWebStatus();
  sendWebCfg();
}

void sendWebExt() {
  JSONVar ext;
  ext["energy"] = energyToJsonObj();
  if (g_haveMeter && !meter_job && !atmBusy) {
    ext["meter"] = meterLiveToJson();
  }
  WebSerial.send("ext", ext);
}

static void atmUpdateBaseFromJson(const JSONVar& obj) {
  if (obj.hasOwnProperty("lineHz")) {
    int hz = jvGetInt(obj, "lineHz", (int)g_atm_cfg.lineHz);
    g_atm_cfg.lineHz = (hz == 60) ? 60 : 50;
  }
  if (obj.hasOwnProperty("sumAbs")) {
    g_atm_cfg.sumAbs = jvGetInt(obj, "sumAbs", (int)g_atm_cfg.sumAbs) ? 1 : 0;
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

static void atmApplyFromJson(const JSONVar& obj) {
  if (obj.hasOwnProperty("lineHz") || obj.hasOwnProperty("sumAbs") || obj.hasOwnProperty("ucal")) {
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

void setup() {
  Serial.begin(57600);

  if (!LittleFS.begin()) {
    LittleFS.format();
    LittleFS.begin();
  }
  if (!loadConfigFS()) setDefaults();

  for (uint8_t i=0;i<NUM_RLY;i++) { pinMode(RELAY_PINS[i], OUTPUT); digitalWrite(RELAY_PINS[i], LOW); }
  for (uint8_t i=0;i<NUM_LED;i++) { pinMode(LED_PINS[i],   OUTPUT); digitalWrite(LED_PINS[i],   LOW); }
  for (uint8_t i=0;i<NUM_BTN;i++) pinMode(BTN_PINS[i], INPUT_PULLUP);

  Serial2.setTX(TX2);
  Serial2.setRX(RX2);
  applyModbusSettings(g_mb_address, g_mb_baud);
  mb.setAdditionalServerData("ENM223-ENM");
  mbBuildRegisterMap();
  hmRegisterIdentity(mb, HM_MODEL_ID, HM_FW_MAJOR, HM_FW_MINOR, HM_FW_PATCH, HM_MAP_VERSION);
  mbSyncHolding();

  // SimpleWebSerial allows max 8 events — do not add more without changing the library.
  WebSerial.on("values",  handleValues);
  WebSerial.on("Config",  handleUnifiedConfig);
  WebSerial.on("command", handleCommand);

  SPI1.setSCK(ATM_SCK);
  SPI1.setTX(ATM_MOSI);
  SPI1.setRX(ATM_MISO);
  SPI1.begin();

  atmBusy = true;
  atmApplyFromCfg_NOW();
  atmBusy = false;
  sampleEnergyCounters();
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

  serviceModbusRelays();

  if (atmApplyPending && !atmBusy && (now - atmLastApplyMs >= atmApplyMinIntervalMs)) {
    atmApplyPending = false;
    atmBusy = true;
    atmApplyFromCfg_NOW();
    atmBusy = false;
    atmLastApplyMs = now;
    markCfgDirty();
    wsLog("OK: ATM applied");
    sendWebCfg();
  }

  if (cfgDirty && (now - lastCfgTouchMs >= CFG_AUTOSAVE_MS)) {
    cfgDirty = false;
    saveConfigFS();
  }

  if (now - lastBlinkToggle >= blinkPeriodMs) {
    lastBlinkToggle = now;
    blinkPhase = !blinkPhase;
  }

  bool relayLogical[NUM_RLY];
  for (int i = 0; i < NUM_BTN; i++) {
    bool pressed = (digitalRead(BTN_PINS[i]) == LOW);
    buttonPrev[i]  = buttonState[i];
    buttonState[i] = pressed;
    if (!buttonPrev[i] && buttonState[i]) {
      uint8_t act = btnCfg[i].action;
      if (act == 5 || act == 6) {
        int r = act - 5;
        if (r >= 0 && r < NUM_RLY) desiredRelay[r] = !desiredRelay[r];
      }
    }
    mb.setIsts(DI_BTN_BASE + i, pressed);
  }

  for (int i = 0; i < NUM_RLY; i++) {
    bool logical = desiredRelay[i];
    if (!rlyCfg[i].enabled) logical = false;
    bool phys = logical;
    if (rlyCfg[i].inverted) phys = !phys;
    digitalWrite(RELAY_PINS[i], phys ? HIGH : LOW);
    relayLogical[i] = logical;
    mb.setIsts(DI_RELAY_BASE + i, logical);
  }

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

  const bool hostUp = Serial && Serial.dtr();
  if (hostUp && !webHostWasConnected) {
    webHostWasConnected = true;
    sendWebBootstrap();
  } else if (!hostUp) {
    webHostWasConnected = false;
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

  if (now - lastSend >= sendInterval) {
    lastSend = now;

    if (hmUsbCanSend()) {
      sendWebStatus();

      JSONVar io;
      for (int i = 0; i < NUM_RLY; i++) io["relay"][i] = relayLogical[i] ? 1 : 0;
      for (int i = 0; i < NUM_BTN; i++) io["btn"][i] = buttonState[i] ? 1 : 0;
      for (int i = 0; i < NUM_LED; i++) io["led"][i] = ledPhysState[i] ? 1 : 0;
      WebSerial.send("io", io);

      sendWebExt();
    }
  }
}
