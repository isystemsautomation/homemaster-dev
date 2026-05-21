#include <Arduino.h>
#include <ModbusSerial.h>
#include <SimpleWebSerial.h>
#include <Arduino_JSON.h>
#include <math.h>

#include "atm90e32.h"

// SimpleWebSerial: MaximumNumberOfEvents = 8 (library default). Never register more than 8 handlers.
static const char* FW_TAG = "ENM-v18";

// Arduino_JSON: (int)obj["key"] → ASCII первой буквы ключа, не число.
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

// stringify(JSONVar value) — только число, без бага (int)obj["key"]
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

static SimpleWebSerial WebSerial;
static JSONVar modbusStatus;

static unsigned long lastSend = 0;
static const unsigned long sendInterval = 1000;

static unsigned long lastBlinkToggle = 0;
static const unsigned long blinkPeriodMs = 400;
static bool blinkPhase = false;

static uint8_t  g_mb_address = 30;
static uint32_t g_mb_baud    = 19200;

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

static volatile bool dirtyRelayCfg = false;
static volatile bool dirtyBtnCfg   = false;
static volatile bool dirtyLedCfg   = false;
static volatile bool dirtyAtmCfg   = false;

static volatile bool pendingEchoAll = false;
static volatile bool pendingStatus  = false;
static volatile bool pendingAtmCfg  = false;
static volatile bool pendingMsg      = false;
static const char*   pendingMsgText  = nullptr;
static uint8_t       echoStep        = 0;

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
}

static void sendMeterEcho() {
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

  auto to_k = [](uint32_t Wh) -> double { return Wh / 1000.0; };
  JSONVar Ephase;
  for (int i = 0; i < 3; i++) {
    JSONVar ph;
    ph["AP_kWh"] = to_k(g_e_ap_Wh[i]);
    ph["AN_kWh"] = to_k(g_e_an_Wh[i]);
    ph["RP_kvarh"] = to_k(g_e_rp_varh[i]);
    ph["RN_kvarh"] = to_k(g_e_rn_varh[i]);
    ph["S_kVAh"] = to_k(g_e_s_VAh[i]);
    Ephase[i] = ph;
  }
  JSONVar Etot;
  Etot["AP_kWh"] = to_k(g_e_ap_Wh[3]);
  Etot["AN_kWh"] = to_k(g_e_an_Wh[3]);
  Etot["RP_kvarh"] = to_k(g_e_rp_varh[3]);
  Etot["RN_kvarh"] = to_k(g_e_rn_varh[3]);
  Etot["S_kVAh"] = to_k(g_e_s_VAh[3]);
  m["E_phase"] = Ephase;
  m["E_tot"] = Etot;

  WebSerial.send("ENM_Meter", m);
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
  dirtyAtmCfg = true;
  pendingAtmCfg = true;
}

static JSONVar relayEnableListToJson() {
  JSONVar a;
  for (int i = 0; i < NUM_RLY; i++) a[i] = (bool)rlyCfg[i].enabled;
  return a;
}
static JSONVar relayInvertListToJson() {
  JSONVar a;
  for (int i = 0; i < NUM_RLY; i++) a[i] = (bool)rlyCfg[i].inverted;
  return a;
}
static JSONVar buttonGroupListToJson() {
  JSONVar a;
  for (int i = 0; i < NUM_BTN; i++) a[i] = (int)btnCfg[i].action;
  return a;
}
static JSONVar ledCfgListToJson() {
  JSONVar a;
  for (int i = 0; i < NUM_LED; i++) {
    JSONVar o;
    o["mode"]   = (int)ledCfg[i].mode;
    o["source"] = (int)ledCfg[i].source;
    a[i] = o;
  }
  return a;
}
static JSONVar calPhasesArrayFromCfg() {
  JSONVar cal;
  static const char* names[] = {"A", "B", "C"};
  for (int i = 0; i < 3; i++) {
    JSONVar p;
    p["Ugain"]   = (int)g_atm_cfg.cal[i].Ugain;
    p["Igain"]   = (int)g_atm_cfg.cal[i].Igain;
    p["Uoffset"] = (int)g_atm_cfg.cal[i].Uoffset;
    p["Ioffset"] = (int)g_atm_cfg.cal[i].Ioffset;
    cal[names[i]] = p;
  }
  return cal;
}

static JSONVar calibCfgToJson() {
  JSONVar root;
  root["cal"] = calPhasesArrayFromCfg();
  return root;
}

static JSONVar atmCfgToJson() {
  JSONVar o;
  o["lineHz"] = (int)g_atm_cfg.lineHz;
  o["sumAbs"] = (int)g_atm_cfg.sumAbs;
  o["ucal"]   = (int)g_atm_cfg.ucal;
  o["cal"]    = calPhasesArrayFromCfg();
  return o;
}

static JSONVar atmChipCfgToJson() {
  M90PhaseCal chip[3];
  g_atm.readPhaseCal(chip);
  JSONVar ph;
  static const char* names[] = {"A", "B", "C"};
  for (int i = 0; i < 3; i++) {
    JSONVar p;
    p["Ugain"]   = (int)chip[i].Ugain;
    p["Igain"]   = (int)chip[i].Igain;
    p["Uoffset"] = (int)chip[i].Uoffset;
    p["Ioffset"] = (int)chip[i].Ioffset;
    ph[names[i]] = p;
  }
  JSONVar o;
  o["phase"] = ph;
  return o;
}

static void sendCalibEcho() {
  updateModbusStatusJson();
  WebSerial.send("status", modbusStatus);
  yield();
  WebSerial.send("CalibCfg", calibCfgToJson());
  yield();
  WebSerial.send("atmCfg", atmCfgToJson());
  yield();
  if (!atmBusy) {
    WebSerial.send("ATM_ChipCfg", atmChipCfgToJson());
    yield();
  }
}

static void sendFullSyncToWeb() {
  sendCalibEcho();
  WebSerial.send("relayEnableList", relayEnableListToJson());
  yield();
  WebSerial.send("relayInvertList", relayInvertListToJson());
  yield();
  WebSerial.send("ButtonGroupList", buttonGroupListToJson());
  yield();
  WebSerial.send("LedConfigList", ledCfgListToJson());
  yield();
}

static void updateModbusStatusJson() {
  modbusStatus["address"] = (int)g_mb_address;
  modbusStatus["baud"]    = (int)g_mb_baud;
  modbusStatus["state"]   = 0;
  modbusStatus["fw"]      = FW_TAG;
}

static void applyModbusSettings(uint8_t addr, uint32_t baud) {
  addr = (uint8_t)constrain((int)addr, 1, 247);
  if (!isAllowedBaud(baud)) baud = g_mb_baud;

  if (g_mb_baud != baud) {
    Serial2.end();
    Serial2.setTX(TX2);
    Serial2.setRX(RX2);
    Serial2.begin(baud);
    while (Serial2.available()) (void)Serial2.read();
    mb.config(baud);
    g_mb_baud = baud;
  }
  setSlaveIdIfAvailable(mb, addr);
  g_mb_address = addr;
  SlaveId = (int)addr;
  updateModbusStatusJson();
  mbSyncHolding();
}

static void echoStepSend() {
  switch (echoStep) {
    case 0: updateModbusStatusJson(); WebSerial.send("status", modbusStatus); break;
    case 1: WebSerial.send("CalibCfg", calibCfgToJson()); break;
    case 2: WebSerial.send("atmCfg", atmCfgToJson()); break;
    case 3: WebSerial.send("relayEnableList", relayEnableListToJson()); break;
    case 4: WebSerial.send("relayInvertList", relayInvertListToJson()); break;
    case 5: WebSerial.send("ButtonGroupList", buttonGroupListToJson()); break;
    case 6: WebSerial.send("LedConfigList", ledCfgListToJson()); break;
    case 7:
      if (!atmBusy) WebSerial.send("ATM_ChipCfg", atmChipCfgToJson());
      break;
    default: echoStep = 0; pendingEchoAll = false; return;
  }
  echoStep++;
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

static void handleHello(JSONVar) {
  echoStep = 0;
  pendingEchoAll = true;
  pendingMsgText = "OK: Hello";
  pendingMsg = true;
}
static void handleGetAll(JSONVar) {
  pendingEchoAll = false;
  echoStep = 0;
  sendFullSyncToWeb();
  pendingMsgText = "OK: Refreshed";
  pendingMsg = true;
}

static void handleValues(JSONVar values) {
  const int addr = jvGetInt(values, "mb_address", (int)g_mb_address);
  const int baud = jvGetInt(values, "mb_baud",    (int)g_mb_baud);
  applyModbusSettings((uint8_t)addr, (uint32_t)baud);
  sendCalibEcho();
  WebSerial.send("message", "Modbus configuration updated");
}

static void handleRelayCfg(JSONVar obj) {
  JSONVar en  = obj["enabled"];
  JSONVar inv = obj["inverted"];
  for (int i = 0; i < NUM_RLY; i++) {
    if (JSON.typeof(en[i])  != "undefined")  rlyCfg[i].enabled  = (bool)en[i];
    if (JSON.typeof(inv[i]) != "undefined")  rlyCfg[i].inverted = (bool)inv[i];
  }
  dirtyRelayCfg = true;
  pendingMsgText = "OK: Relays updated";
  pendingMsg = true;
}

static void handleBtnCfg(JSONVar obj) {
  JSONVar a = obj["action"];
  for (int i = 0; i < NUM_BTN; i++) {
    if (JSON.typeof(a[i]) == "undefined") continue;
    int v = (int)a[i];
    btnCfg[i].action = (uint8_t)((v==0 || v==5 || v==6) ? v : 0);
  }
  dirtyBtnCfg = true;
  pendingMsgText = "OK: Buttons updated";
  pendingMsg = true;
}

static void handleLedCfg(JSONVar obj) {
  JSONVar m = obj["mode"];
  JSONVar s = obj["source"];
  for (int i = 0; i < NUM_LED; i++) {
    if (JSON.typeof(m[i]) != "undefined") ledCfg[i].mode   = (uint8_t)constrain((int)m[i], 0, 1);
    if (JSON.typeof(s[i]) != "undefined") {
      int sv = (int)s[i];
      ledCfg[i].source = (uint8_t)((sv==0 || sv==5 || sv==6) ? sv : 0);
    }
  }
  dirtyLedCfg = true;
  pendingMsgText = "OK: LEDs updated";
  pendingMsg = true;
}

static void handleAtm(JSONVar obj) {
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
  queueAtmApply();
  pendingMsgText = "OK: ATM queued";
  pendingMsg = true;
  sendCalibEcho();
}

void setup() {
  Serial.begin(57600);

  for (uint8_t i=0;i<NUM_RLY;i++) { pinMode(RELAY_PINS[i], OUTPUT); digitalWrite(RELAY_PINS[i], LOW); }
  for (uint8_t i=0;i<NUM_LED;i++) { pinMode(LED_PINS[i],   OUTPUT); digitalWrite(LED_PINS[i],   LOW); }
  for (uint8_t i=0;i<NUM_BTN;i++) pinMode(BTN_PINS[i], INPUT_PULLUP);

  setDefaults();

  Serial2.setTX(TX2);
  Serial2.setRX(RX2);
  Serial2.begin(g_mb_baud);
  mb.config(g_mb_baud);
  setSlaveIdIfAvailable(mb, (uint8_t)g_mb_address);
  mb.setAdditionalServerData("ENM223-ENM");
  mbBuildRegisterMap();
  mbSyncHolding();

  updateModbusStatusJson();

  // SimpleWebSerial allows max 8 events — do not add more without changing the library.
  WebSerial.on("values",   handleValues);
  WebSerial.on("getAll",   handleGetAll);
  WebSerial.on("relayCfg", handleRelayCfg);
  WebSerial.on("btnCfg",   handleBtnCfg);
  WebSerial.on("ledCfg",   handleLedCfg);
  WebSerial.on("atm",      handleAtm);

  SPI1.setSCK(ATM_SCK);
  SPI1.setTX(ATM_MOSI);
  SPI1.setRX(ATM_MISO);
  SPI1.begin();

  atmBusy = true;
  atmApplyFromCfg_NOW();
  atmBusy = false;

  pendingMsgText = "Boot OK ENM-v18";
  pendingMsg = true;
  lastMeterSample = millis();
  lastEnergySample = millis();
}

void loop() {
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
    pendingAtmCfg = true;
    pendingMsgText = "OK: ATM applied";
    pendingMsg = true;
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
    bool srcActive = false;
    uint8_t src = ledCfg[i].source;
    if (src == 5 || src == 6) {
      int r = src - 5;
      srcActive = (r >= 0 && r < NUM_RLY) ? relayLogical[r] : false;
    }
    bool physLed = (ledCfg[i].mode == 0) ? srcActive : (srcActive && blinkPhase);
    ledPhysState[i] = physLed;
    digitalWrite(LED_PINS[i], physLed ? HIGH : LOW);
    mb.setIsts(DI_LED_BASE + i, physLed);
  }

  if (pendingEchoAll) echoStepSend();
  if (pendingStatus) {
    pendingStatus = false;
    updateModbusStatusJson();
    WebSerial.send("status", modbusStatus);
  }
  if (pendingAtmCfg) {
    pendingAtmCfg = false;
    sendCalibEcho();
  }
  if (pendingMsg && pendingMsgText) {
    pendingMsg = false;
    WebSerial.send("message", pendingMsgText);
    pendingMsgText = nullptr;
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

    if (!pendingEchoAll && !atmBusy) {
      JSONVar relayStateList;
      for (int i = 0; i < NUM_RLY; i++) relayStateList[i] = relayLogical[i];
      WebSerial.send("relayStateList", relayStateList);
    }

    if (dirtyRelayCfg) {
      WebSerial.send("relayEnableList", relayEnableListToJson());
      WebSerial.send("relayInvertList", relayInvertListToJson());
      dirtyRelayCfg = false;
    }
    if (dirtyBtnCfg) {
      WebSerial.send("ButtonGroupList", buttonGroupListToJson());
      dirtyBtnCfg = false;
    }
    if (dirtyLedCfg) {
      WebSerial.send("LedConfigList", ledCfgListToJson());
      dirtyLedCfg = false;
    }
    if (dirtyAtmCfg) {
      WebSerial.send("atmCfg", atmCfgToJson());
      dirtyAtmCfg = false;
    }

    if (g_haveMeter && !meter_job && !atmBusy) {
      sendMeterEcho();
    }
  }
}
