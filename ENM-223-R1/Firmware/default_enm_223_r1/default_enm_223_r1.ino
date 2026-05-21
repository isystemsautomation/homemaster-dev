#include <Arduino.h>
#include <ModbusSerial.h>
#include <SimpleWebSerial.h>
#include <Arduino_JSON.h>
#include <math.h>
#include <utility>

#include "atm90e32.h"   // your external driver

// ================== Safe JSON value extraction ==================
// Баг Arduino_JSON на Linux: (int)obj["key"] и obj["key"] дают ASCII буквы ключа.
// Надёжно: парсим число из JSON.stringify(всего объекта) по имени ключа.
static inline String jsonBlob(const JSONVar& v) {
  return JSON.stringify((JSONVar&)v);
}

static int jsonParseKey(const JSONVar& obj, const char* key, int fallback) {
  if (!obj.hasOwnProperty(key)) return fallback;
  const String blob = jsonBlob(obj);
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

static bool jsonParseKeyBool(const JSONVar& obj, const char* key, bool fallback) {
  if (!obj.hasOwnProperty(key)) return fallback;
  const String blob = jsonBlob(obj);
  String pat = String("\"") + key + "\":";
  int pos = blob.indexOf(pat);
  if (pos < 0) return fallback;
  pos += pat.length();
  while (pos < (int)blob.length() && blob[pos] == ' ') pos++;
  if (blob.startsWith("true", pos))  return true;
  if (blob.startsWith("false", pos)) return false;
  return jsonParseKey(obj, key, fallback ? 1 : 0) != 0;
}

static int jsonParseArrayInt(const JSONVar& arr, int idx, int fallback) {
  const String blob = jsonBlob(arr);
  if (blob.length() < 2 || blob[0] != '[') return fallback;
  int pos = 1;
  for (int i = 0; i < idx; i++) {
    while (pos < (int)blob.length() && blob[pos] != ',' && blob[pos] != ']') pos++;
    if (pos >= (int)blob.length() || blob[pos] == ']') return fallback;
    pos++;
    while (pos < (int)blob.length() && (blob[pos] == ' ' || blob[pos] == ',')) pos++;
  }
  while (pos < (int)blob.length() && (blob[pos] == ' ')) pos++;
  int end = pos;
  while (end < (int)blob.length() && blob[end] != ',' && blob[end] != ']') end++;
  return blob.substring(pos, end).toInt();
}

static bool jsonParseArrayBool(const JSONVar& arr, int idx, bool fallback) {
  const String blob = jsonBlob(arr);
  if (blob.length() < 2 || blob[0] != '[') return fallback;
  int pos = 1;
  for (int i = 0; i < idx; i++) {
    while (pos < (int)blob.length() && blob[pos] != ',' && blob[pos] != ']') pos++;
    if (pos >= (int)blob.length() || blob[pos] == ']') return fallback;
    pos++;
    while (pos < (int)blob.length() && (blob[pos] == ' ' || blob[pos] == ',')) pos++;
  }
  while (pos < (int)blob.length() && (blob[pos] == ' ')) pos++;
  if (blob.startsWith("true", pos))  return true;
  if (blob.startsWith("false", pos)) return false;
  return jsonParseArrayInt(arr, idx, fallback ? 1 : 0) != 0;
}

// ================== UART2 (RS-485 / Modbus) ==================
#define TX2 4
#define RX2 5
static const int TxenPin = -1;
static int SlaveId = 1;
static ModbusSerial mb(Serial2, SlaveId, TxenPin);

// ================== GPIO MAP (ENM) ==================
static const uint8_t RELAY_PINS[2] = {0, 1};
static const uint8_t LED_PINS[4]   = {18, 19, 20, 21};
static const uint8_t BTN_PINS[4]   = {22, 23, 24, 25};

static const uint8_t NUM_RLY = 2;
static const uint8_t NUM_LED = 4;
static const uint8_t NUM_BTN = 4;

// ================== Config & runtime ==================
struct RlyCfg { bool enabled; bool inverted; };
struct LedCfg { uint8_t mode; uint8_t source; }; // mode:0 steady,1 blink; source:0 none,5 relay1,6 relay2
struct BtnCfg { uint8_t action; };               // 0 none, 5 relay1 toggle, 6 relay2 toggle

static RlyCfg rlyCfg[NUM_RLY];
static LedCfg ledCfg[NUM_LED];
static BtnCfg btnCfg[NUM_BTN];

static bool buttonState[NUM_BTN]  = {false,false,false,false};
static bool buttonPrev[NUM_BTN]   = {false,false,false,false};
static bool desiredRelay[NUM_RLY] = {false,false};

// ================== Web Serial ==================
static SimpleWebSerial WebSerial;
static JSONVar modbusStatus;

// ================== Timing ==================
static unsigned long lastSend = 0;
static unsigned long lastStatusSend = 0;
static unsigned long lastEnergyAccumMs = 0;
// Slower push = MUCH more stable on USB/WebSerial stacks
static const unsigned long sendInterval = 2000;
static const unsigned long statusIntervalMs = 2000;
static const unsigned long energyIntervalMs = 5000;

static unsigned long lastBlinkToggle = 0;
static const unsigned long blinkPeriodMs = 400;
static bool blinkPhase = false;

// ================== Modbus settings (runtime) ==================
static uint8_t  g_mb_address = 30;
static uint32_t g_mb_baud    = 19200;

static volatile bool pendingModbusApply = false;
static volatile bool pendingPhaseCalApply = false;
static uint8_t  pending_mb_addr = 30;
static uint32_t pending_mb_baud = 19200;
static unsigned long mbTaskHoldUntil = 0;
static bool deferredSlaveIdApply = false;
static uint8_t deferredSlaveId = 30;

// During ATM SPI sampling: only Modbus/yield — never WebSerial.check() (reentrancy).
static inline void yieldMbOnly() {
  mb.task();
  yield();
}

// ================== ATM90E32 (RP2350 / ENM hardware) ==================
static const uint8_t ATM_SCK  = 10;
static const uint8_t ATM_MOSI = 11;
static const uint8_t ATM_MISO = 12;
static const uint8_t ATM_CS   = 13;
static const uint8_t ATM_PM1  = 2;
static const uint8_t ATM_PM0  = 3;

static ATM90E32 g_atm(SPI1, ATM_CS, ATM_PM0, ATM_PM1, 200000, SPI_MODE0, false);

struct AtmCfg {
  uint16_t lineHz;   // 50/60
  uint8_t  sumAbs;   // 0/1
  uint16_t ucal;     // sag math reference
  M90PhaseCal cal[3];
};
static AtmCfg g_atm_cfg;

// ===== SAFE queued ATM apply (NO begin() IN CALLBACKS) =====
static volatile bool atmApplyPending = false;
static unsigned long atmLastApplyMs = 0;
static const unsigned long atmApplyMinIntervalMs = 300;
static bool atmBusy = false;

// ================== Dirty flags (echo when needed) ==================
static volatile bool dirtyRelayCfg = false;
static volatile bool dirtyBtnCfg   = false;
static volatile bool dirtyLedCfg   = false;
static volatile bool dirtyAtmCfg   = false;

// ================== Outbound-send deferral (CRITICAL) ==================
// Never call WebSerial.send() from handlers; only set these flags.
static volatile bool pendingEchoAll   = false;
static volatile bool pendingStatus    = false;
static volatile bool pendingAtmCfg    = false;
static volatile bool pendingMsg       = false;
static const char*   pendingMsgText   = nullptr;
static uint8_t       echoStep         = 0;

// ================== SFINAE helper ==================
template <class M>
inline auto setSlaveIdIfAvailable(M& m, uint8_t id)
  -> decltype(std::declval<M&>().setSlaveId(uint8_t{}), void()) { m.setSlaveId(id); }
inline void setSlaveIdIfAvailable(...) {}

// ================== Modbus mapping ==================
enum : uint16_t {
  ISTS_BTN_BASE = 1,
  ISTS_RLY_BASE = 60,
  ISTS_LED_BASE = 90
};

enum : uint16_t {
  CMD_RLY_ON_BASE  = 200,
  CMD_RLY_OFF_BASE = 210
};

// ================== clamps ==================
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

// ================== Defaults ==================
static void setAtmDefaults() {
  g_atm_cfg.lineHz = 50;
  g_atm_cfg.sumAbs = 1;
  g_atm_cfg.ucal   = 117; // your UI screenshot expectation

  for (int i = 0; i < 3; i++) {
    g_atm_cfg.cal[i].Ugain   = 43000; // 6×220k+1k divider @ 230 V (calibrate on-site)
    g_atm_cfg.cal[i].Igain   = 2000;
    g_atm_cfg.cal[i].Uoffset = 0;
    g_atm_cfg.cal[i].Ioffset = 0;
  }
}

static void setDefaults() {
  for (int i = 0; i < NUM_RLY; i++) rlyCfg[i] = { true, false };
  for (int i = 0; i < NUM_LED; i++) ledCfg[i] = { 0, 0 };
  for (int i = 0; i < NUM_BTN; i++) btnCfg[i] = { 0 };
  for (int i = 0; i < NUM_RLY; i++) desiredRelay[i] = false;

  g_mb_address = 30;
  g_mb_baud    = 19200;

  setAtmDefaults();
}

// ================== ATM apply (safe) ==================
static void atmApplyFromCfg_NOW() {
  // protect against drivers that overwrite the passed array
  M90PhaseCal tmp[3];
  for (int i = 0; i < 3; i++) tmp[i] = g_atm_cfg.cal[i];
  g_atm.begin(g_atm_cfg.lineHz, g_atm_cfg.sumAbs, g_atm_cfg.ucal, tmp);
}

static void queueAtmApply() {
  atmApplyPending = true;
  dirtyAtmCfg = true;
  pendingAtmCfg = true;
}

static void queuePhaseCalApply() {
  pendingPhaseCalApply = true;
  dirtyAtmCfg = true;
  pendingAtmCfg = true;
}

static void atmApplyPhaseCalOnly() {
  M90PhaseCal tmp[3];
  for (int i = 0; i < 3; i++) tmp[i] = g_atm_cfg.cal[i];
  g_atm.applyCalibration(tmp);
}

// ================== JSON builders (built ONLY in loop) ==================
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
static JSONVar atmCfgToJson() {
  JSONVar o;
  o["lineHz"] = (int)g_atm_cfg.lineHz;
  o["sumAbs"] = (int)g_atm_cfg.sumAbs;
  o["ucal"]   = (int)g_atm_cfg.ucal;

  JSONVar cal;
  for (int i = 0; i < 3; i++) {
    JSONVar p;
    p["Ugain"]   = (int)g_atm_cfg.cal[i].Ugain;
    p["Igain"]   = (int)g_atm_cfg.cal[i].Igain;
    p["Uoffset"] = (int)g_atm_cfg.cal[i].Uoffset;
    p["Ioffset"] = (int)g_atm_cfg.cal[i].Ioffset;
    cal[i] = p;
  }
  o["cal"] = cal;
  return o;
}

static void updateModbusStatusJson() {
  modbusStatus["address"] = (int)g_mb_address;
  modbusStatus["baud"]    = (int)g_mb_baud;
  modbusStatus["state"]   = 0;
}

static JSONVar calibCfgToJson() {
  JSONVar a;
  for (int i = 0; i < 3; i++) {
    JSONVar p;
    p["Ugain"]   = (int)g_atm_cfg.cal[i].Ugain;
    p["Igain"]   = (int)g_atm_cfg.cal[i].Igain;
    p["Uoffset"] = (int)g_atm_cfg.cal[i].Uoffset;
    p["Ioffset"] = (int)g_atm_cfg.cal[i].Ioffset;
    a[i] = p;
  }
  return a;
}

// One USB frame per loop — avoids host/WebSerial stall on connect
static void echoStepSend() {
  switch (echoStep) {
    case 0:
      updateModbusStatusJson();
      WebSerial.send("status", modbusStatus);
      break;
    case 1:
      WebSerial.send("CalibCfg", calibCfgToJson());
      break;
    case 2:
      WebSerial.send("atmCfg", atmCfgToJson());
      break;
    case 3:
      WebSerial.send("relayEnableList", relayEnableListToJson());
      break;
    case 4:
      WebSerial.send("relayInvertList", relayInvertListToJson());
      break;
    case 5:
      WebSerial.send("ButtonGroupList", buttonGroupListToJson());
      break;
    case 6:
      WebSerial.send("LedConfigList", ledCfgListToJson());
      pendingEchoAll = false;
      echoStep = 0;
      return;
    default:
      pendingEchoAll = false;
      echoStep = 0;
      return;
  }
  echoStep++;
}

// ================== Update-from-JSON (NO send, no hardware) ==================
static void atmUpdateBaseFromJson(const JSONVar& obj) {
  if (obj.hasOwnProperty("lineHz")) {
    int hz = jsonParseKey(obj, "lineHz", (int)g_atm_cfg.lineHz);
    g_atm_cfg.lineHz = (hz == 60) ? 60 : 50;
  }
  if (obj.hasOwnProperty("sumAbs")) {
    g_atm_cfg.sumAbs = jsonParseKey(obj, "sumAbs", (int)g_atm_cfg.sumAbs) ? 1 : 0;
  }
  if (obj.hasOwnProperty("ucal")) {
    int u = jsonParseKey(obj, "ucal", (int)g_atm_cfg.ucal);
    if (u < 1) u = 1;
    if (u > 65535) u = 65535;
    g_atm_cfg.ucal = (uint16_t)u;
  }
}
static void atmUpdatePhaseFromJson(int phase, const JSONVar& obj) {
  if (phase < 0 || phase > 2) return;
  if (obj.hasOwnProperty("Ugain"))   g_atm_cfg.cal[phase].Ugain   = clamp_u16(jsonParseKey(obj, "Ugain",   (int)g_atm_cfg.cal[phase].Ugain));
  if (obj.hasOwnProperty("Igain"))   g_atm_cfg.cal[phase].Igain   = clamp_u16(jsonParseKey(obj, "Igain",   (int)g_atm_cfg.cal[phase].Igain));
  if (obj.hasOwnProperty("Uoffset")) g_atm_cfg.cal[phase].Uoffset = clamp_i16(jsonParseKey(obj, "Uoffset", (int)g_atm_cfg.cal[phase].Uoffset));
  if (obj.hasOwnProperty("Ioffset")) g_atm_cfg.cal[phase].Ioffset = clamp_i16(jsonParseKey(obj, "Ioffset", (int)g_atm_cfg.cal[phase].Ioffset));
}

// ================== ATM live (chunked — keeps USB/Modbus alive) ==================
struct AtmLiveCache {
  double Ua_V, Ub_V, Uc_V;
  double Ia_A, Ib_A, Ic_A;
  int16_t PF_A_raw, PF_B_raw, PF_C_raw, PF_T_raw;
  int16_t AngA_raw, AngB_raw, AngC_raw;
  uint16_t Freq_x100;
  int16_t Temp_C;
  double P_W[4], Q_var[4], S_VA[4];
  bool valid = false;
};

static uint32_t g_e_ap_Wh[4], g_e_an_Wh[4], g_e_rp_varh[4], g_e_rn_varh[4], g_e_s_VAh[4];
static AtmLiveCache g_atm_live;
static bool atmLiveJob = false;
static uint8_t atmLiveStep = 0;

static void atmLiveJobBegin() {
  if (atmBusy) return;
  atmLiveJob = true;
  atmLiveStep = 0;
}

static void atmLiveComputePQS() {
  const double u[3] = { g_atm_live.Ua_V, g_atm_live.Ub_V, g_atm_live.Uc_V };
  const double i[3] = { g_atm_live.Ia_A, g_atm_live.Ib_A, g_atm_live.Ic_A };
  const int16_t pf_raw[4] = {
    g_atm_live.PF_A_raw, g_atm_live.PF_B_raw, g_atm_live.PF_C_raw, g_atm_live.PF_T_raw
  };

  for (int ph = 0; ph < 3; ph++) {
    double pf = pf_raw[ph] / 1000.0;
    if (pf > 1.0) pf = 1.0;
    if (pf < -1.0) pf = -1.0;
    const double s = u[ph] * i[ph];
    const double p = s * pf;
    double q2 = (s * s) - (p * p);
    if (q2 < 0) q2 = 0;
    g_atm_live.S_VA[ph] = s;
    g_atm_live.P_W[ph]  = p;
    g_atm_live.Q_var[ph] = sqrt(q2);
  }
  g_atm_live.S_VA[3] = g_atm_live.S_VA[0] + g_atm_live.S_VA[1] + g_atm_live.S_VA[2];
  g_atm_live.P_W[3]  = g_atm_live.P_W[0]  + g_atm_live.P_W[1]  + g_atm_live.P_W[2];
  g_atm_live.Q_var[3] = g_atm_live.Q_var[0] + g_atm_live.Q_var[1] + g_atm_live.Q_var[2];
}

static void atmLiveAccumulateEnergyStep(uint8_t sub) {
  switch (sub) {
    case 0:
      g_e_ap_Wh[0] += g_atm.rdAP_A(); g_e_ap_Wh[1] += g_atm.rdAP_B(); g_e_ap_Wh[2] += g_atm.rdAP_C();
      break;
    case 1:
      g_e_ap_Wh[3] += g_atm.rdAP_T();
      g_e_an_Wh[0] += g_atm.rdAN_A(); g_e_an_Wh[1] += g_atm.rdAN_B(); g_e_an_Wh[2] += g_atm.rdAN_C();
      break;
    case 2:
      g_e_an_Wh[3] += g_atm.rdAN_T();
      g_e_rp_varh[0] += g_atm.rdRP_A(); g_e_rp_varh[1] += g_atm.rdRP_B(); g_e_rp_varh[2] += g_atm.rdRP_C();
      break;
    case 3:
      g_e_rp_varh[3] += g_atm.rdRP_T();
      g_e_rn_varh[0] += g_atm.rdRN_A(); g_e_rn_varh[1] += g_atm.rdRN_B(); g_e_rn_varh[2] += g_atm.rdRN_C();
      g_e_rn_varh[3] += g_atm.rdRN_T();
      g_e_s_VAh[0]  += g_atm.rdSA_A();  g_e_s_VAh[1]  += g_atm.rdSA_B();  g_e_s_VAh[2]  += g_atm.rdSA_C();
      g_e_s_VAh[3]  += g_atm.rdSA_T();
      break;
    default: break;
  }
}

static void atmLiveJobStep() {
  if (!atmLiveJob || atmBusy) return;
  switch (atmLiveStep) {
    case 0: g_atm_live.Ua_V = g_atm.readUrmsA_V(); g_atm_live.Ub_V = g_atm.readUrmsB_V(); yieldMbOnly(); atmLiveStep++; break;
    case 1: g_atm_live.Uc_V = g_atm.readUrmsC_V(); g_atm_live.Ia_A = g_atm.readIrmsA_A(); yieldMbOnly(); atmLiveStep++; break;
    case 2: g_atm_live.Ib_A = g_atm.readIrmsB_A(); g_atm_live.Ic_A = g_atm.readIrmsC_A(); yieldMbOnly(); atmLiveStep++; break;
    case 3:
      g_atm_live.PF_A_raw = g_atm.readPFmeanA(); g_atm_live.PF_B_raw = g_atm.readPFmeanB();
      g_atm_live.PF_C_raw = g_atm.readPFmeanC(); g_atm_live.PF_T_raw = g_atm.readPFmeanT();
      yieldMbOnly(); atmLiveStep++; break;
    case 4:
      g_atm_live.AngA_raw = g_atm.readPAngleA(); g_atm_live.AngB_raw = g_atm.readPAngleB(); g_atm_live.AngC_raw = g_atm.readPAngleC();
      yieldMbOnly(); atmLiveStep++; break;
    case 5: g_atm_live.Freq_x100 = g_atm.readFreq_x100(); g_atm_live.Temp_C = g_atm.readTempC(); yieldMbOnly(); atmLiveStep++; break;
    case 6: atmLiveComputePQS(); g_atm_live.valid = true; atmLiveStep++; break;
    case 7: case 8: case 9: case 10:
      if (millis() - lastEnergyAccumMs >= energyIntervalMs) {
        atmLiveAccumulateEnergyStep((uint8_t)(atmLiveStep - 7));
        yieldMbOnly();
        atmLiveStep++;
      } else {
        atmLiveJob = false;
      }
      break;
    default: atmLiveJob = false; break;
  }
  if (atmLiveStep > 10) {
    lastEnergyAccumMs = millis();
    atmLiveJob = false;
  }
}

static inline double roundTo(double v, int dec) {
  long scale = 1;
  for (int i = 0; i < dec; i++) scale *= 10;
  return ((double)lround(v * (double)scale)) / (double)scale;
}

static JSONVar enmMeterToJson() {
  JSONVar m;
  m["Urms"][0] = roundTo(g_atm_live.Ua_V, 2);
  m["Urms"][1] = roundTo(g_atm_live.Ub_V, 2);
  m["Urms"][2] = roundTo(g_atm_live.Uc_V, 2);
  m["Irms"][0] = roundTo(g_atm_live.Ia_A, 3);
  m["Irms"][1] = roundTo(g_atm_live.Ib_A, 3);
  m["Irms"][2] = roundTo(g_atm_live.Ic_A, 3);

  JSONVar pW, qV, sVA, pfR, ang;
  for (int i = 0; i < 4; i++) {
    pW[i]  = roundTo(g_atm_live.P_W[i], 1);
    qV[i]  = roundTo(g_atm_live.Q_var[i], 1);
    sVA[i] = roundTo(g_atm_live.S_VA[i], 1);
  }
  pfR[0] = roundTo(g_atm_live.PF_A_raw / 1000.0, 3);
  pfR[1] = roundTo(g_atm_live.PF_B_raw / 1000.0, 3);
  pfR[2] = roundTo(g_atm_live.PF_C_raw / 1000.0, 3);
  pfR[3] = roundTo(g_atm_live.PF_T_raw / 1000.0, 3);
  m["P_W"] = pW;
  m["Q_var"] = qV;
  m["S_VA"] = sVA;
  m["PF"] = pfR;

  ang[0] = roundTo(g_atm_live.AngA_raw / 10.0, 1);
  ang[1] = roundTo(g_atm_live.AngB_raw / 10.0, 1);
  ang[2] = roundTo(g_atm_live.AngC_raw / 10.0, 1);
  m["Angle_deg"] = ang;
  m["FreqHz"] = roundTo(g_atm_live.Freq_x100 / 100.0, 2);
  m["TempC"]  = (int)g_atm_live.Temp_C;

  auto toKwh = [](uint32_t w)->double { return roundTo(w / 1000.0, 3); };
  JSONVar ePhase;
  for (int i = 0; i < 3; i++) {
    JSONVar ph;
    ph["AP_kWh"]   = toKwh(g_e_ap_Wh[i]);
    ph["AN_kWh"]   = toKwh(g_e_an_Wh[i]);
    ph["RP_kvarh"] = toKwh(g_e_rp_varh[i]);
    ph["RN_kvarh"] = toKwh(g_e_rn_varh[i]);
    ph["S_kVAh"]   = toKwh(g_e_s_VAh[i]);
    ePhase[i] = ph;
  }
  JSONVar eTot;
  eTot["AP_kWh"]   = toKwh(g_e_ap_Wh[3]);
  eTot["AN_kWh"]   = toKwh(g_e_an_Wh[3]);
  eTot["RP_kvarh"] = toKwh(g_e_rp_varh[3]);
  eTot["RN_kvarh"] = toKwh(g_e_rn_varh[3]);
  eTot["S_kVAh"]   = toKwh(g_e_s_VAh[3]);
  m["E_phase"] = ePhase;
  m["E_tot"]   = eTot;
  return m;
}

// ================== Modbus command pulses ==================
static void processModbusCommandPulses() {
  for (int r = 0; r < NUM_RLY; r++) {
    if (mb.Coil(CMD_RLY_ON_BASE + r)) {
      mb.setCoil(CMD_RLY_ON_BASE + r, false);
      desiredRelay[r] = true;
    }
    if (mb.Coil(CMD_RLY_OFF_BASE + r)) {
      mb.setCoil(CMD_RLY_OFF_BASE + r, false);
      desiredRelay[r] = false;
    }
  }
}

// ================== WebSerial handlers (ABSOLUTELY NO send/hardware) ==================
static void handleHello(JSONVar) {
  echoStep = 0;
  pendingEchoAll = true;
  pendingMsgText = "OK: Hello";
  pendingMsg = true;
}
static void handleGetAll(JSONVar) {
  echoStep = 0;
  pendingEchoAll = true;
  pendingMsgText = "OK: Refreshed from device";
  pendingMsg = true;
}

static void applyModbusSettingsDirect(uint8_t addr, uint32_t baud) {
  addr = (uint8_t)constrain((int)addr, 1, 247);
  baud = (uint32_t)constrain((int)baud, 9600, 115200);

  if (g_mb_baud != baud) {
    Serial2.end();
    delay(20);
    Serial2.setTX(TX2);
    Serial2.setRX(RX2);
    Serial2.begin(baud);
    while (Serial2.available()) (void)Serial2.read();
    mb.config(baud);
    g_mb_baud = baud;
    mbTaskHoldUntil = millis() + 500;
  }
  if (g_mb_address != addr) {
    g_mb_address = addr;
    SlaveId = (int)addr;
    deferredSlaveId = addr;
    deferredSlaveIdApply = true;
    mbTaskHoldUntil = millis() + 500;
  }
}

static void handleValues(JSONVar values) {
  const int addr = jsonParseKey(values, "mb_address", (int)g_mb_address);
  const int baud = jsonParseKey(values, "mb_baud",    (int)g_mb_baud);
  pending_mb_addr = (uint8_t)constrain(addr, 1, 247);
  pending_mb_baud = (uint32_t)constrain(baud, 9600, 115200);
  pendingModbusApply = true;
}

static void handleRelayCfg(JSONVar obj) {
  JSONVar en  = obj["enabled"];
  JSONVar inv = obj["inverted"];
  for (int i = 0; i < NUM_RLY; i++) {
    if (JSON.typeof(en[i])  != "undefined")  rlyCfg[i].enabled  = jsonParseArrayBool(en, i, rlyCfg[i].enabled);
    if (JSON.typeof(inv[i]) != "undefined")  rlyCfg[i].inverted = jsonParseArrayBool(inv, i, rlyCfg[i].inverted);
  }
  dirtyRelayCfg = true;
  pendingMsgText = "OK: Relays updated";
  pendingMsg = true;
}

static void handleBtnCfg(JSONVar obj) {
  JSONVar a = obj["action"];
  for (int i = 0; i < NUM_BTN; i++) {
    if (JSON.typeof(a[i]) == "undefined") continue;
    int v = jsonParseArrayInt(a, i, (int)btnCfg[i].action);
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
    if (JSON.typeof(m[i]) != "undefined") ledCfg[i].mode   = (uint8_t)constrain(jsonParseArrayInt(m, i, (int)ledCfg[i].mode), 0, 1);
    if (JSON.typeof(s[i]) != "undefined") {
      int sv = jsonParseArrayInt(s, i, (int)ledCfg[i].source);
      ledCfg[i].source = (uint8_t)((sv==0 || sv==5 || sv==6) ? sv : 0);
    }
  }
  dirtyLedCfg = true;
  pendingMsgText = "OK: LEDs updated";
  pendingMsg = true;
}

// ATM grouped handlers: update cfg + queue apply (no begin here)
static void handleAtmBase(JSONVar obj) {
  atmUpdateBaseFromJson(obj);
  queueAtmApply();
  pendingMsgText = "OK: ATM base queued";
  pendingMsg = true;
}
static void handleAtmA(JSONVar obj) {
  atmUpdatePhaseFromJson(0, obj);
  queuePhaseCalApply();
  pendingMsgText = "OK: Phase A cal queued";
  pendingMsg = true;
}
static void handleAtmB(JSONVar obj) {
  atmUpdatePhaseFromJson(1, obj);
  queuePhaseCalApply();
  pendingMsgText = "OK: Phase B cal queued";
  pendingMsg = true;
}
static void handleAtmC(JSONVar obj) {
  atmUpdatePhaseFromJson(2, obj);
  queuePhaseCalApply();
  pendingMsgText = "OK: Phase C cal queued";
  pendingMsg = true;
}

// Legacy support (optional)
static void handleAtmCfgLegacy(JSONVar obj) {
  atmUpdateBaseFromJson(obj);
  if (obj.hasOwnProperty("cal")) {
    JSONVar cal = obj["cal"];
    for (int i = 0; i < 3; i++) {
      JSONVar p = cal[i];
      if (JSON.typeof(p) == "undefined") continue;
      atmUpdatePhaseFromJson(i, p);
    }
  }
  queueAtmApply();
  pendingMsgText = "OK: ATM queued (legacy)";
  pendingMsg = true;
}

// ================== Setup ==================
void setup() {
  Serial.begin(57600);

  for (uint8_t i=0;i<NUM_RLY;i++) { pinMode(RELAY_PINS[i], OUTPUT); digitalWrite(RELAY_PINS[i], LOW); }
  for (uint8_t i=0;i<NUM_LED;i++) { pinMode(LED_PINS[i],   OUTPUT); digitalWrite(LED_PINS[i],   LOW); }
  for (uint8_t i=0;i<NUM_BTN;i++) pinMode(BTN_PINS[i], INPUT_PULLUP);

  setDefaults();

  // Serial2 / Modbus
  Serial2.setTX(TX2);
  Serial2.setRX(RX2);
  Serial2.begin(g_mb_baud);
  mb.config(g_mb_baud);
  setSlaveIdIfAvailable(mb, g_mb_address);
  mb.setAdditionalServerData("ENM223-ENM");

  // Discrete inputs
  for (uint16_t i=0;i<NUM_BTN;i++) mb.addIsts(ISTS_BTN_BASE + i);
  for (uint16_t i=0;i<NUM_RLY;i++) mb.addIsts(ISTS_RLY_BASE + i);
  for (uint16_t i=0;i<NUM_LED;i++) mb.addIsts(ISTS_LED_BASE + i);

  // Coils (pulse commands)
  for (uint16_t i=0;i<NUM_RLY;i++){ mb.addCoil(CMD_RLY_ON_BASE  + i); mb.setCoil(CMD_RLY_ON_BASE  + i, false); }
  for (uint16_t i=0;i<NUM_RLY;i++){ mb.addCoil(CMD_RLY_OFF_BASE + i); mb.setCoil(CMD_RLY_OFF_BASE + i, false); }

  updateModbusStatusJson();

  // WebSerial handlers
  WebSerial.on("values",   handleValues);
  WebSerial.on("getAll",   handleGetAll);
  WebSerial.on("hello",    handleHello);

  WebSerial.on("relayCfg", handleRelayCfg);
  WebSerial.on("btnCfg",   handleBtnCfg);
  WebSerial.on("ledCfg",   handleLedCfg);

  WebSerial.on("atm",      handleAtmBase);
  WebSerial.on("atmA",     handleAtmA);
  WebSerial.on("atmB",     handleAtmB);
  WebSerial.on("atmC",     handleAtmC);
  WebSerial.on("atmCfg",   handleAtmCfgLegacy);

  // ---- SPI1 + ATM init ----
  SPI1.setSCK(ATM_SCK);
  SPI1.setTX(ATM_MOSI);
  SPI1.setRX(ATM_MISO);
  SPI1.begin();

  // apply defaults once
  atmBusy = true;
  atmApplyFromCfg_NOW();
  atmBusy = false;
  atmLiveJobBegin();

  // Defer this message to loop (safe)
  pendingMsgText = "Boot OK v4: Modbus USB-first, mb.task paused 500ms";
  pendingMsg = true;
  echoStep = 0;
  pendingEchoAll = true;
}

// ================== Main loop ==================
void loop() {
  const unsigned long now = millis();

  // 0) USB in first
  WebSerial.check();

  // 1) Modbus — immediate ack in same iteration as values
  if (pendingModbusApply) {
    pendingModbusApply = false;
    atmLiveJob = false;
    if (pending_mb_addr != g_mb_address || pending_mb_baud != g_mb_baud) {
      applyModbusSettingsDirect(pending_mb_addr, pending_mb_baud);
    }
    updateModbusStatusJson();
    WebSerial.send("status", modbusStatus);
    static char mbAck[56];
    snprintf(mbAck, sizeof(mbAck), "OK: Modbus addr=%u baud=%lu",
             (unsigned)g_mb_address, (unsigned long)g_mb_baud);
    WebSerial.send("message", mbAck);
    yield();
  }

  atmLiveJobStep();

  // 2b) Phase cal only (no chip soft-reset)
  if (pendingPhaseCalApply && !atmBusy) {
    pendingPhaseCalApply = false;
    atmLiveJob = false;
    atmBusy = true;
    atmApplyPhaseCalOnly();
    atmBusy = false;
    atmLiveJobBegin();
    pendingMsgText = "OK: Phase cal applied";
    pendingMsg = true;
  }

  // 3) Apply queued ATM safely (rate-limited, no live reads while busy)
  if (atmApplyPending && !atmBusy && (now - atmLastApplyMs >= atmApplyMinIntervalMs)) {
    atmApplyPending = false;
    atmBusy = true;
    atmApplyFromCfg_NOW();
    atmBusy = false;
    atmLastApplyMs = now;
    atmLiveJobBegin();

    pendingAtmCfg = true;
    pendingMsgText = "OK: ATM applied";
    pendingMsg = true;
  }

  // 4) Modbus tasking — pause after address/baud change (mb.task can block on RS485)
  if (!atmBusy && now >= mbTaskHoldUntil) {
    if (deferredSlaveIdApply) {
      deferredSlaveIdApply = false;
      setSlaveIdIfAvailable(mb, deferredSlaveId);
    }
    mb.task();
    processModbusCommandPulses();
  }

  // 5) Blink scheduler
  if (now - lastBlinkToggle >= blinkPeriodMs) {
    lastBlinkToggle = now;
    blinkPhase = !blinkPhase;
  }

  // 6) Buttons
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
    mb.setIsts(ISTS_BTN_BASE + i, pressed);
  }

  // 7) Relays
  bool relayLogical[NUM_RLY];
  for (int i = 0; i < NUM_RLY; i++) {
    bool logical = desiredRelay[i];
    if (!rlyCfg[i].enabled) logical = false;

    bool phys = logical;
    if (rlyCfg[i].inverted) phys = !phys;
    digitalWrite(RELAY_PINS[i], phys ? HIGH : LOW);

    relayLogical[i] = logical;
    mb.setIsts(ISTS_RLY_BASE + i, logical);
  }

  // 8) LEDs
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
    mb.setIsts(ISTS_LED_BASE + i, physLed);
  }

  // 9) SAFE outbound sends (deferred from handlers; one config chunk per loop)
  if (pendingEchoAll) {
    echoStepSend();
  }
  if (pendingStatus) {
    pendingStatus = false;
    updateModbusStatusJson();
    WebSerial.send("status", modbusStatus);
  }
  if (pendingAtmCfg) {
    pendingAtmCfg = false;
    WebSerial.send("CalibCfg", calibCfgToJson());
    WebSerial.send("atmCfg", atmCfgToJson());
  }
  if (pendingMsg && pendingMsgText) {
    pendingMsg = false;
    WebSerial.send("message", pendingMsgText);
    pendingMsgText = nullptr;
  }

  // 10) Periodic live updates
  if (now - lastSend >= sendInterval) {
    lastSend = now;
    if (!atmLiveJob) atmLiveJobBegin();

    if (pendingStatus || (now - lastStatusSend >= statusIntervalMs)) {
      pendingStatus = false;
      updateModbusStatusJson();
      WebSerial.send("status", modbusStatus);
      lastStatusSend = now;
    }

    if (!pendingEchoAll) {
      JSONVar relayStateList;
      for (int i = 0; i < NUM_RLY; i++) relayStateList[i] = relayLogical[i];
      WebSerial.send("relayStateList", relayStateList);

      if (!atmBusy && g_atm_live.valid) {
        WebSerial.send("ENM_Meter", enmMeterToJson());
      }
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
  }

}
