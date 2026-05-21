#include <Arduino.h>
#include <ModbusSerial.h>
#include <SimpleWebSerial.h>
#include <Arduino_JSON.h>
#include <utility>

#include "atm90e32.h"   // your external driver

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
// Slower push = MUCH more stable on USB/WebSerial stacks
static const unsigned long sendInterval = 500;

static unsigned long lastBlinkToggle = 0;
static const unsigned long blinkPeriodMs = 400;
static bool blinkPhase = false;

// ================== Modbus settings (runtime) ==================
static uint8_t  g_mb_address = 30;
static uint32_t g_mb_baud    = 19200;

// ===== SAFE queued Modbus apply (NO UART CHANGES IN CALLBACKS) =====
static volatile bool mbApplyPending = false;
static uint8_t  mb_req_address = 30;
static uint32_t mb_req_baud    = 19200;
static unsigned long mbLastApplyMs = 0;
static const unsigned long mbApplyMinIntervalMs = 250;
static bool mbBusy = false;

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
    g_atm_cfg.cal[i].Ugain   = 8000;
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

  mb_req_address = g_mb_address;
  mb_req_baud    = g_mb_baud;

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

static void sendAllConfigEcho_NOW() {
  updateModbusStatusJson();
  WebSerial.send("status", modbusStatus);
  WebSerial.send("relayEnableList", relayEnableListToJson());
  WebSerial.send("relayInvertList", relayInvertListToJson());
  WebSerial.send("ButtonGroupList", buttonGroupListToJson());
  WebSerial.send("LedConfigList",   ledCfgListToJson());
  WebSerial.send("atmCfg",          atmCfgToJson());
}

// ================== Update-from-JSON (NO send, no hardware) ==================
static void atmUpdateBaseFromJson(const JSONVar& obj) {
  if (obj.hasOwnProperty("lineHz")) {
    int hz = (int)obj["lineHz"];
    g_atm_cfg.lineHz = (hz == 60) ? 60 : 50;
  }
  if (obj.hasOwnProperty("sumAbs")) {
    g_atm_cfg.sumAbs = ((int)obj["sumAbs"]) ? 1 : 0;
  }
  if (obj.hasOwnProperty("ucal")) {
    int u = (int)obj["ucal"];
    if (u < 1) u = 1;
    if (u > 65535) u = 65535;
    g_atm_cfg.ucal = (uint16_t)u;
  }
}
static void atmUpdatePhaseFromJson(int phase, const JSONVar& obj) {
  if (phase < 0 || phase > 2) return;
  if (obj.hasOwnProperty("Ugain"))   g_atm_cfg.cal[phase].Ugain   = clamp_u16((int)obj["Ugain"]);
  if (obj.hasOwnProperty("Igain"))   g_atm_cfg.cal[phase].Igain   = clamp_u16((int)obj["Igain"]);
  if (obj.hasOwnProperty("Uoffset")) g_atm_cfg.cal[phase].Uoffset = clamp_i16((int)obj["Uoffset"]);
  if (obj.hasOwnProperty("Ioffset")) g_atm_cfg.cal[phase].Ioffset = clamp_i16((int)obj["Ioffset"]);
}

// ================== Safe queued Modbus apply (ONLY in loop) ==================
static void queueModbusApply(uint8_t addr, uint32_t baud) {
  addr = constrain(addr, 1, 247);
  baud = constrain((int)baud, 9600, 115200);
  mb_req_address = addr;
  mb_req_baud    = baud;
  mbApplyPending = true;
}

static void applyModbusPendingIfNeeded() {
  const unsigned long now = millis();
  if (!mbApplyPending) return;
  if (mbBusy) return;
  if (now - mbLastApplyMs < mbApplyMinIntervalMs) return;

  mbApplyPending = false;
  mbBusy = true;

  if (g_mb_baud != mb_req_baud) {
    Serial2.end();
    Serial2.begin(mb_req_baud);
    mb.config(mb_req_baud);
    g_mb_baud = mb_req_baud;
  }
  if (g_mb_address != mb_req_address) {
    setSlaveIdIfAvailable(mb, mb_req_address);
    g_mb_address = mb_req_address;
  }

  mbLastApplyMs = now;
  mbBusy = false;

  pendingStatus = true;
  pendingMsgText = "OK: Modbus applied";
  pendingMsg = true;
}

// ================== ATM live ==================
static JSONVar atmLiveToJson() {
  JSONVar o;

  o["Ua_V"] = g_atm.readUrmsA_V();
  o["Ub_V"] = g_atm.readUrmsB_V();
  o["Uc_V"] = g_atm.readUrmsC_V();

  o["Ia_A"] = g_atm.readIrmsA_A();
  o["Ib_A"] = g_atm.readIrmsB_A();
  o["Ic_A"] = g_atm.readIrmsC_A();

  o["PF_A_raw"] = (int)g_atm.readPFmeanA();
  o["PF_B_raw"] = (int)g_atm.readPFmeanB();
  o["PF_C_raw"] = (int)g_atm.readPFmeanC();
  o["PF_T_raw"] = (int)g_atm.readPFmeanT();

  o["AngA_raw"] = (int)g_atm.readPAngleA();
  o["AngB_raw"] = (int)g_atm.readPAngleB();
  o["AngC_raw"] = (int)g_atm.readPAngleC();

  o["Freq_x100"] = (int)g_atm.readFreq_x100();
  o["Temp_C"]    = (int)g_atm.readTempC();

  M90DiagRegs d = g_atm.readDiag();
  JSONVar diag;
  diag["EMMState0"]     = (int)d.EMMState0;
  diag["EMMState1"]     = (int)d.EMMState1;
  diag["EMMIntState0"]  = (int)d.EMMIntState0;
  diag["EMMIntState1"]  = (int)d.EMMIntState1;
  diag["CRCErrStatus"]  = (int)d.CRCErrStatus;
  diag["LastSPIData"]   = (int)d.LastSPIData;
  o["diag"] = diag;

  return o;
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
  pendingEchoAll = true;
  pendingMsgText = "OK: Hello";
  pendingMsg = true;
}
static void handleGetAll(JSONVar) {
  pendingEchoAll = true;
  pendingMsgText = "OK: Refreshed from device";
  pendingMsg = true;
}

static void handleValues(JSONVar values) {
  int addr = values.hasOwnProperty("mb_address") ? (int)values["mb_address"] : (int)g_mb_address;
  int baud = values.hasOwnProperty("mb_baud")    ? (int)values["mb_baud"]    : (int)g_mb_baud;
  addr = constrain(addr, 1, 247);
  baud = constrain(baud, 9600, 115200);

  queueModbusApply((uint8_t)addr, (uint32_t)baud);

  pendingMsgText = "OK: Modbus queued";
  pendingMsg = true;
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

// ATM grouped handlers: update cfg + queue apply (no begin here)
static void handleAtmBase(JSONVar obj) {
  atmUpdateBaseFromJson(obj);
  queueAtmApply();
  pendingMsgText = "OK: ATM base queued";
  pendingMsg = true;
}
static void handleAtmA(JSONVar obj) {
  atmUpdatePhaseFromJson(0, obj);
  queueAtmApply();
  pendingMsgText = "OK: Phase A queued";
  pendingMsg = true;
}
static void handleAtmB(JSONVar obj) {
  atmUpdatePhaseFromJson(1, obj);
  queueAtmApply();
  pendingMsgText = "OK: Phase B queued";
  pendingMsg = true;
}
static void handleAtmC(JSONVar obj) {
  atmUpdatePhaseFromJson(2, obj);
  queueAtmApply();
  pendingMsgText = "OK: Phase C queued";
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

  // Defer this message to loop (safe)
  pendingMsgText = "Boot OK (stable): callbacks do not send; all sends happen in loop";
  pendingMsg = true;
  pendingEchoAll = true;
}

// ================== Main loop ==================
void loop() {
  const unsigned long now = millis();

  // 1) Process inbound WebSerial
  WebSerial.check();

  // 2) Apply queued Modbus safely (before mb.task)
  applyModbusPendingIfNeeded();

  // 3) Apply queued ATM safely (rate-limited, no live reads while busy)
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

  // 4) Modbus tasking
  if (!mbBusy) {
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

  // 9) SAFE outbound sends (deferred from handlers)
  if (pendingEchoAll) {
    pendingEchoAll = false;
    sendAllConfigEcho_NOW();
  }
  if (pendingStatus) {
    pendingStatus = false;
    updateModbusStatusJson();
    WebSerial.send("status", modbusStatus);
  }
  if (pendingAtmCfg) {
    pendingAtmCfg = false;
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

    updateModbusStatusJson();
    WebSerial.send("status", modbusStatus);

    JSONVar relayStateList;
    for (int i = 0; i < NUM_RLY; i++) relayStateList[i] = relayLogical[i];
    WebSerial.send("relayStateList", relayStateList);

    JSONVar buttonStateList;
    for (int i = 0; i < NUM_BTN; i++) buttonStateList[i] = buttonState[i];
    WebSerial.send("ButtonStateList", buttonStateList);

    JSONVar ledStateList;
    for (int i = 0; i < NUM_LED; i++) ledStateList[i] = ledPhysState[i];
    WebSerial.send("LedStateList", ledStateList);

    if (!atmBusy) {
      WebSerial.send("atmLive", atmLiveToJson());
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
