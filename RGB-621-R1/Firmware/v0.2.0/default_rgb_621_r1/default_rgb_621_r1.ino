#include <Arduino.h>
#include <ModbusSerial.h>
#include "hm_common.h"
#define HM_MODEL_ID   7
#define HM_FW_MAJOR   0
#define HM_FW_MINOR   2
#define HM_FW_PATCH   0
#define HM_FW         "0.2.0"
#define HM_MAP        2
#include <SimpleWebSerial.h>
#include <Arduino_JSON.h>
#include <LittleFS.h>
#include <utility>
#include "hardware/watchdog.h"

// ================== UART2 (RS-485 / Modbus) ==================
#define TX2 4
#define RX2 5
const int TxenPin = -1;          // -1 if RS-485 TXEN not used
int SlaveId = 1;
ModbusSerial mb(Serial2, SlaveId, TxenPin);

// ================== GPIO MAP (RGB module) ==================
// Macros for preprocessor-safe conflict checks:
#define PIN_BTN1   0
#define PIN_BTN2   1
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
static const uint8_t BTN_PINS[2]   = {PIN_BTN1, PIN_BTN2};      // Button1..2

static const uint8_t PWM_PINS[5] = {
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
static const uint8_t NUM_BTN  = 2;
static const uint8_t NUM_PWM  = 5; // R,G,B,WW,CW

// ================== Config & runtime ==================
struct InCfg  { bool enabled; bool inverted; uint8_t action; /*0=None,1=Toggle,2=Pulse*/ uint8_t target; /*4=None,0=All,1..N*/ };
struct RlyCfg { bool enabled; bool inverted; uint8_t powerOn; /*0=OFF,1=ON,2=RESTORE_LAST*/ };
struct LedCfg { uint8_t mode;   /*0=steady,1=blink*/
                uint8_t source; /*0=None, 5..(5+NUM_RLY-1)=Overridden relay index+5*/ };
struct BtnCfg { uint8_t action; /*0=None, 5..(5+NUM_RLY-1)=Relay override toggle*/ };

InCfg   diCfg[NUM_DI];
RlyCfg  rlyCfg[NUM_RLY];
LedCfg  ledCfg[NUM_LED];
BtnCfg  btnCfg[NUM_BTN];

bool buttonState[NUM_BTN]   = {false,false};
bool buttonPrev[NUM_BTN]    = {false,false};
bool diState[NUM_DI]        = {false,false};
bool diPrev[NUM_DI]         = {false,false};

// Desired relay state
bool desiredRelay[NUM_RLY] = {false};

// Pulse handling for relays (when DI action=Pulse)
uint32_t rlyPulseUntil[NUM_RLY] = {0};
const uint32_t PULSE_MS = 500; // default pulse width

// PWM levels 0..255 (R,G,B,WW,CW)
uint16_t pwmLevel[NUM_PWM] = {0,0,0,0,0}; // store as 16-bit for Modbus HR, but clamp to 0..255
uint8_t  pwmPowerOn[NUM_PWM];             // per-channel power-on behavior (HM_PWR_*)

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
uint32_t g_identifyUntilMs = 0;
const uint32_t IDENTIFY_MS = 5000;

// ================== Persisted Modbus settings ==================
uint8_t  g_mb_address = 3;
uint32_t g_mb_baud    = 19200;

// ================== Persistence (LittleFS) ==================
struct PersistConfig {
  uint32_t magic;  uint16_t version;  uint16_t size;
  InCfg   diCfg[NUM_DI];
  RlyCfg  rlyCfg[NUM_RLY];
  LedCfg  ledCfg[NUM_LED];
  BtnCfg  btnCfg[NUM_BTN];
  uint8_t pwmPowerOn[NUM_PWM];
  uint8_t mb_address;
  uint32_t mb_baud;
  uint32_t crc32;
} __attribute__((packed));

struct RlyCfgV2 { bool enabled; bool inverted; };

struct PersistConfigV2 {
  uint32_t magic;  uint16_t version;  uint16_t size;
  InCfg    diCfg[NUM_DI];
  RlyCfgV2 rlyCfg[NUM_RLY];
  LedCfg   ledCfg[NUM_LED];
  BtnCfg   btnCfg[NUM_BTN];
  bool     desiredRelay[NUM_RLY];
  uint16_t pwmLevel[NUM_PWM];
  uint8_t  mb_address;
  uint32_t mb_baud;
  uint32_t crc32;
} __attribute__((packed));

struct OutputStateSnapshot {
  uint32_t magic; uint16_t version; uint16_t size;
  bool     desiredRelay[NUM_RLY];
  uint16_t pwmLevel[NUM_PWM];
  uint32_t crc32;
} __attribute__((packed));

static const uint32_t CFG_MAGIC   = 0x52474231UL; // '1BGR'
static const uint16_t CFG_VERSION = 0x0003;       // Phase B: powerOn, output state decoupled
static const uint16_t CFG_VERSION_V2 = 0x0002;
static const char*    CFG_PATH    = "/cfg_rgb.bin";
static const char*    OUT_STATE_PATH = "/cfg_out.bin";
static const uint32_t OUT_STATE_MAGIC = 0x484D4F53UL; // 'HMOS'
static const uint16_t OUT_STATE_VERSION = 0x0001;

volatile bool   cfgDirty        = false;
uint32_t        lastCfgTouchMs  = 0;
const uint32_t  CFG_AUTOSAVE_MS = 1500;
uint32_t        lastOutChangeMs = 0;
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
  for (int i = 0; i < NUM_DI;  i++) diCfg[i]  = { true, false, 0 /*None*/, 0 /*All*/ };
  for (int i = 0; i < NUM_RLY; i++) rlyCfg[i] = { true, false, HM_PWR_OFF };
  for (int i = 0; i < NUM_LED; i++) ledCfg[i] = { 0 /*steady*/, 0 /*source: None*/ };
  for (int i = 0; i < NUM_BTN; i++) btnCfg[i] = { 0 };
  for (int i = 0; i < NUM_RLY; i++) { desiredRelay[i] = false; rlyPulseUntil[i] = 0; }
  for (int i = 0; i < NUM_PWM; i++) { pwmLevel[i] = 0; pwmPowerOn[i] = HM_PWR_OFF; }
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
  memcpy(outPwm, snap.pwmLevel, sizeof(snap.pwmLevel));
  return true;
}

bool saveOutputStateSnapshot() {
  OutputStateSnapshot snap{};
  snap.magic = OUT_STATE_MAGIC; snap.version = OUT_STATE_VERSION; snap.size = sizeof(OutputStateSnapshot);
  memcpy(snap.desiredRelay, desiredRelay, sizeof(desiredRelay));
  memcpy(snap.pwmLevel, pwmLevel, sizeof(pwmLevel));
  snap.crc32 = 0; snap.crc32 = crc32_update(0, (const uint8_t*)&snap, sizeof(snap));
  File f = LittleFS.open(OUT_STATE_PATH, "w");
  if (!f) return false;
  size_t n = f.write((const uint8_t*)&snap, sizeof(snap));
  f.flush(); f.close();
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
    if (pwmPowerOn[i] == HM_PWR_ON) pwmLevel[i] = 255;
    else if (pwmPowerOn[i] == HM_PWR_RESTORE && haveSnap) pwmLevel[i] = restoredPwm[i];
    else pwmLevel[i] = 0;
  }
  memcpy(prevDesiredRelay, desiredRelay, sizeof(prevDesiredRelay));
  memcpy(prevPwmLevel, pwmLevel, sizeof(prevPwmLevel));
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
      if (pwmPowerOn[i] == HM_PWR_RESTORE) { needRestore = true; break; }
    }
  }
  if (!needRestore) return;
  if ((uint32_t)(now - lastOutChangeMs) < OUT_AUTOSAVE_MS) return;
  if (lastOutSaveMs && (uint32_t)(now - lastOutSaveMs) < OUT_AUTOSAVE_MS) return;
  if (saveOutputStateSnapshot()) lastOutSaveMs = now;
}

void captureToPersist(PersistConfig &pc) {
  pc.magic   = CFG_MAGIC; pc.version = CFG_VERSION; pc.size = sizeof(PersistConfig);
  memcpy(pc.diCfg,        diCfg,        sizeof(diCfg));
  memcpy(pc.rlyCfg,       rlyCfg,       sizeof(rlyCfg));
  memcpy(pc.ledCfg,       ledCfg,       sizeof(ledCfg));
  memcpy(pc.btnCfg,       btnCfg,       sizeof(btnCfg));
  memcpy(pc.pwmPowerOn,   pwmPowerOn,   sizeof(pwmPowerOn));
  pc.mb_address = g_mb_address; pc.mb_baud = g_mb_baud;
  pc.crc32 = 0; pc.crc32 = crc32_update(0, (const uint8_t*)&pc, sizeof(PersistConfig));
}

bool applyFromPersistV2(const PersistConfigV2 &pc) {
  if (pc.magic != CFG_MAGIC || pc.size != sizeof(PersistConfigV2)) return false;
  PersistConfigV2 tmp = pc; uint32_t crc = tmp.crc32; tmp.crc32 = 0;
  if (crc32_update(0, (const uint8_t*)&tmp, sizeof(PersistConfigV2)) != crc) return false;
  if (pc.version != CFG_VERSION_V2) return false;
  memcpy(diCfg, pc.diCfg, sizeof(diCfg));
  for (int i = 0; i < NUM_RLY; i++) rlyCfg[i] = { pc.rlyCfg[i].enabled, pc.rlyCfg[i].inverted, HM_PWR_OFF };
  memcpy(ledCfg, pc.ledCfg, sizeof(ledCfg));
  memcpy(btnCfg, pc.btnCfg, sizeof(btnCfg));
  for (int i = 0; i < NUM_PWM; i++) pwmPowerOn[i] = HM_PWR_OFF;
  g_mb_address = pc.mb_address; g_mb_baud = pc.mb_baud;
  OutputStateSnapshot snap{};
  snap.magic = OUT_STATE_MAGIC; snap.version = OUT_STATE_VERSION; snap.size = sizeof(OutputStateSnapshot);
  memcpy(snap.desiredRelay, pc.desiredRelay, sizeof(snap.desiredRelay));
  memcpy(snap.pwmLevel, pc.pwmLevel, sizeof(snap.pwmLevel));
  snap.crc32 = 0; snap.crc32 = crc32_update(0, (const uint8_t*)&snap, sizeof(snap));
  File f = LittleFS.open(OUT_STATE_PATH, "w");
  if (f) { f.write((const uint8_t*)&snap, sizeof(snap)); f.flush(); f.close(); }
  return true;
}

bool applyFromPersist(const PersistConfig &pc) {
  if (pc.magic != CFG_MAGIC || pc.size != sizeof(PersistConfig)) return false;
  PersistConfig tmp = pc; uint32_t crc = tmp.crc32; tmp.crc32 = 0;
  if (crc32_update(0, (const uint8_t*)&tmp, sizeof(PersistConfig)) != crc) return false;
  if (pc.version != CFG_VERSION) return false;

  memcpy(diCfg,        pc.diCfg,        sizeof(diCfg));
  memcpy(rlyCfg,       pc.rlyCfg,       sizeof(rlyCfg));
  memcpy(ledCfg,       pc.ledCfg,       sizeof(ledCfg));
  memcpy(btnCfg,       pc.btnCfg,       sizeof(btnCfg));
  memcpy(pwmPowerOn,   pc.pwmPowerOn,   sizeof(pwmPowerOn));
  g_mb_address = pc.mb_address; g_mb_baud = pc.mb_baud;
  return true;
}

bool saveConfigFS() {
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
  return true;
}
bool loadConfigFS() {
  File f = LittleFS.open(CFG_PATH, "r"); if (!f) { wsLog("load: open failed"); return false; }
  size_t sz = f.size();
  if (sz == sizeof(PersistConfigV2)) {
    PersistConfigV2 pc{}; size_t n = f.read((uint8_t*)&pc, sizeof(pc)); f.close();
    if (n != sizeof(pc)) { wsLog("load: short read (v2)"); return false; }
    if (!applyFromPersistV2(pc)) { wsLog("load: v2 magic/version/crc mismatch"); return false; }
    cfgDirty = true; lastCfgTouchMs = millis();
    return true;
  }
  if (sz != sizeof(PersistConfig)) { wsLog(String("load: size ")+sz+" unsupported"); f.close(); return false; }
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
// Discrete Inputs (FC=02)
enum : uint16_t {
  ISTS_DI_BASE   = 1,   // 1..2 : IN1..IN2 (after enable+invert)
  ISTS_RLY_BASE  = 60,  // 60..60 : RELAY1 logical state
  ISTS_LED_BASE  = 90   // 90..91 : LED1..LED2 logical state
};

// Command Coils (FC=05/15; pulses)
enum : uint16_t {
  CMD_RLY_ON_BASE   = 200,  // 200..200 : pulse turn Relay1 ON
  CMD_RLY_OFF_BASE  = 210,  // 210..210 : pulse turn Relay1 OFF
  CMD_DI_EN_BASE    = 300,  // 300..301 : pulse ENABLE  IN1..IN2
  CMD_DI_DIS_BASE   = 320   // 320..321 : pulse DISABLE IN1..IN2
};

// Holding Registers (FC=03/06/16) for PWM levels (0..255)
enum : uint16_t {
  HR_PWM_BASE = 400, // 400..404 : R,G,B,WW,CW
  HR_MB_ADDR  = 480, // Modbus address
  HR_MB_BAUD  = 481  // Modbus baud
};

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
void processModbusCommandPulses();
void applyActionToTarget(uint8_t target, uint8_t action, uint32_t now);
void applyPwmFromHoldingRegs();
void analogWriteClamp(uint8_t pin, uint16_t level);

// ================== Setup ==================
void setup() {
  Serial.begin(57600);

  // GPIO directions
  for (uint8_t i=0;i<NUM_DI;i++)   pinMode(DI_PINS[i],   INPUT);           // change to INPUT_PULLUP if needed
  for (uint8_t i=0;i<NUM_RLY;i++)  { pinMode(RELAY_PINS[i], OUTPUT); digitalWrite(RELAY_PINS[i], LOW); } // OFF
  for (uint8_t i=0;i<NUM_LED;i++)  { pinMode(LED_PINS[i],   OUTPUT);  digitalWrite(LED_PINS[i],   LOW); } // OFF
  for (uint8_t i=0;i<NUM_BTN;i++)  pinMode(BTN_PINS[i],   INPUT_PULLUP);   // active-LOW
  // PWM pins init
  for (uint8_t i=0;i<NUM_PWM;i++) analogWrite(PWM_PINS[i], 0);

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

  // ==== Modbus command pulses (coils) ====
  for (uint16_t i=0;i<NUM_RLY;i++){ mb.addCoil(CMD_RLY_ON_BASE  + i);  mb.setCoil(CMD_RLY_ON_BASE  + i, false); }
  for (uint16_t i=0;i<NUM_RLY;i++){ mb.addCoil(CMD_RLY_OFF_BASE + i);  mb.setCoil(CMD_RLY_OFF_BASE + i, false); }
  for (uint16_t i=0;i<NUM_DI;i++)  { mb.addCoil(CMD_DI_EN_BASE   + i);  mb.setCoil(CMD_DI_EN_BASE   + i, false); }
  for (uint16_t i=0;i<NUM_DI;i++)  { mb.addCoil(CMD_DI_DIS_BASE  + i);  mb.setCoil(CMD_DI_DIS_BASE  + i, false); }

  // ==== Modbus holding registers for PWM + MB settings ====
  for (uint16_t i=0;i<NUM_PWM;i++) { mb.addHreg(HR_PWM_BASE + i); mb.Hreg(HR_PWM_BASE + i, pwmLevel[i]); }
  mb.addHreg(HR_MB_ADDR); mb.Hreg(HR_MB_ADDR, g_mb_address);
  mb.addHreg(HR_MB_BAUD); mb.Hreg(HR_MB_BAUD, (uint16_t)g_mb_baud);

  hmRegisterIdentity(mb, HM_MODEL_ID, HM_FW_MAJOR, HM_FW_MINOR, HM_FW_PATCH, HM_MAP);

  WebSerial.on("values",  handleValues);
  WebSerial.on("Config",  handleUnifiedConfig);
  WebSerial.on("command", handleCommand);

  wsLog("Boot OK (RGB+CCT via Modbus HR 400..404; DI actions None/Toggle/Pulse; LED source: None/Overridden R1)");
  sendWebBootstrap();

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
      for (int i = 0; i < NUM_PWM; i++) mb.Hreg(HR_PWM_BASE + i, pwmLevel[i]);
      applyPwmFromHoldingRegs();
      wsLog("Configuration loaded");
      sendWebBootstrap();
      applyModbusSettings(g_mb_address, g_mb_baud);
    }
    else wsLog("ERROR: Load failed/invalid");
  } else if (act == "factory") {
    LittleFS.remove(OUT_STATE_PATH);
    setDefaults(); applyPowerOnOutputs();
    for (int i = 0; i < NUM_PWM; i++) mb.Hreg(HR_PWM_BASE + i, pwmLevel[i]);
    applyPwmFromHoldingRegs();
    if (saveConfigFS()) { wsLog("Factory defaults restored & saved"); sendWebBootstrap(); applyModbusSettings(g_mb_address, g_mb_baud); }
    else wsLog("ERROR: Save after factory reset failed");
  } else if (act == "identify") {
    g_identifyUntilMs = millis() + IDENTIFY_MS;
    wsLog("Identify: LEDs active for 5 s");
  } else if (act == "off") {
    for (int i=0;i<NUM_PWM;i++){ pwmLevel[i]=0; mb.Hreg(HR_PWM_BASE+i,0); }
    applyPwmFromHoldingRegs();
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
  int addr = (int)values["mb_address"];
  int baud = (int)values["mb_baud"];
  addr = hmValidAddress(addr);
  baud = hmValidBaud(baud);
  applyModbusSettings((uint8_t)addr, (uint32_t)baud);

  // Optionally accept direct PWM payloads: {"rgb":[r,g,b],"cct":[ww,cw]} (0..255)
  if (values.hasOwnProperty("rgb")) {
    JSONVar arr = values["rgb"];
    if (arr.length() >= 3) {
      pwmLevel[0] = (uint16_t)constrain((int)arr[0], 0, 255);
      pwmLevel[1] = (uint16_t)constrain((int)arr[1], 0, 255);
      pwmLevel[2] = (uint16_t)constrain((int)arr[2], 0, 255);
      mb.Hreg(HR_PWM_BASE+0, pwmLevel[0]);
      mb.Hreg(HR_PWM_BASE+1, pwmLevel[1]);
      mb.Hreg(HR_PWM_BASE+2, pwmLevel[2]);
    }
  }
  if (values.hasOwnProperty("cct")) {
    JSONVar arr = values["cct"];
    if (arr.length() >= 2) {
      pwmLevel[3] = (uint16_t)constrain((int)arr[0], 0, 255);
      pwmLevel[4] = (uint16_t)constrain((int)arr[1], 0, 255);
      mb.Hreg(HR_PWM_BASE+3, pwmLevel[3]);
      mb.Hreg(HR_PWM_BASE+4, pwmLevel[4]);
    }
  }

  applyPwmFromHoldingRegs();
  wsLog("Values updated");
  cfgDirty = true; lastCfgTouchMs = millis();
  sendWebStatus();
}

// Contract t: in.*, relay, btn, led, ext.pwmPowerOn (+ legacy aliases)
void handleUnifiedConfig(JSONVar obj) {
  const char* t = (const char*)obj["t"]; JSONVar list = obj["list"]; if (!t) return;
  String type = String(t); bool changed = false;

  if (type == "in.enabled" || type == "inputEnable") {
    for (int i=0;i<NUM_DI && i<list.length();i++) diCfg[i].enabled = (bool)list[i];
    wsLog("Input Enabled list updated"); changed = true;

  } else if (type == "in.invert" || type == "inputInvert") {
    for (int i=0;i<NUM_DI && i<list.length();i++) diCfg[i].inverted = (bool)list[i];
    wsLog("Input Invert list updated"); changed = true;

  } else if (type == "in.action" || type == "inputAction") {
    for (int i=0;i<NUM_DI && i<list.length();i++) {
      int a = (int)list[i];
      diCfg[i].action = (uint8_t)constrain(a, 0, 2);
    }
    wsLog("Input Action list updated"); changed = true;

  } else if (type == "in.target" || type == "inputTarget") {
    for (int i=0;i<NUM_DI && i<list.length();i++) {
      int tgt = (int)list[i];
      diCfg[i].target = (uint8_t)((tgt==4 || tgt==0 || (tgt>=1 && tgt<=NUM_RLY)) ? tgt : 0);
    }
    wsLog("Input Control Target list updated"); changed = true;

  } else if (type == "relay" || type == "relays") {
    for (int i = 0; i < NUM_RLY && i < list.length(); i++) {
      rlyCfg[i].enabled  = (bool)list[i]["enabled"];
      rlyCfg[i].inverted = (bool)list[i]["inverted"];
      if (list[i].hasOwnProperty("powerOn")) {
        rlyCfg[i].powerOn = (uint8_t)constrain((int)list[i]["powerOn"], 0, 2);
      }
    }
    wsLog("Relay Configuration updated"); changed = true;

  } else if (type == "ext.pwmPowerOn" || type == "pwmPowerOn") {
    for (int i = 0; i < NUM_PWM && i < list.length(); i++) {
      pwmPowerOn[i] = (uint8_t)constrain((int)list[i], 0, 2);
    }
    wsLog("PWM Power-On list updated"); changed = true;

  } else if (type == "btn" || type == "buttons") {
    for (int i = 0; i < NUM_BTN && i < list.length(); i++) {
      int a = list[i].hasOwnProperty("action") ? (int)list[i]["action"] : (int)list[i];
      btnCfg[i].action = (uint8_t)constrain(a, 0, 5);
    }
    wsLog("Buttons Configuration updated"); changed = true;

  } else if (type == "led" || type == "leds") {
    for (int i = 0; i < NUM_LED && i < list.length(); i++) {
      ledCfg[i].mode   = (uint8_t)constrain((int)list[i]["mode"],   0, 1);
      int src          = (int)list[i]["source"];
      ledCfg[i].source = (uint8_t)((src==0 || src==5) ? src : 0);
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

// ================== Modbus command pulses ==================
void processModbusCommandPulses() {
  // Relay ON/OFF
  for (int r=0; r<NUM_RLY; r++) {
    if (mb.Coil(CMD_RLY_ON_BASE + r))  { mb.setCoil(CMD_RLY_ON_BASE + r,  false); desiredRelay[r] = true;  rlyPulseUntil[r] = 0; }
    if (mb.Coil(CMD_RLY_OFF_BASE + r)) { mb.setCoil(CMD_RLY_OFF_BASE + r, false); desiredRelay[r] = false; rlyPulseUntil[r] = 0; }
  }
  // DI enable/disable
  for (int i=0; i<NUM_DI; i++) {
    if (mb.Coil(CMD_DI_EN_BASE + i))  { mb.setCoil(CMD_DI_EN_BASE + i,  false); if (!diCfg[i].enabled)  { diCfg[i].enabled  = true;  cfgDirty = true; lastCfgTouchMs = millis(); } }
    if (mb.Coil(CMD_DI_DIS_BASE + i)) { mb.setCoil(CMD_DI_DIS_BASE + i, false); if ( diCfg[i].enabled)  { diCfg[i].enabled  = false; cfgDirty = true; lastCfgTouchMs = millis(); } }
  }
}

// ================== Apply DI action to a target ==================
void applyActionToTarget(uint8_t target, uint8_t action, uint32_t now) {
  auto doRelay = [&](int rIdx) {
    if (rIdx < 0 || rIdx >= NUM_RLY) return;
    if (action == 1) { // Toggle
      desiredRelay[rIdx] = !desiredRelay[rIdx];
      rlyPulseUntil[rIdx] = 0;
    } else if (action == 2) { // Pulse
      desiredRelay[rIdx] = true;
      rlyPulseUntil[rIdx] = now + PULSE_MS;
    }
  };

  if (action == 0) return;           // None action
  if (target == 4) return;           // None target

  if (target == 0) {                 // All relays
    for (int r=0; r<NUM_RLY; r++) doRelay(r);
  } else if (target >= 1 && target <= NUM_RLY) {
    doRelay(target - 1);
  }
}

// ================== PWM helpers ==================
void analogWriteClamp(uint8_t pin, uint16_t level) {
  if (level > 255) level = 255;
  analogWrite(pin, (uint8_t)level);
}

void applyPwmFromHoldingRegs() {
  for (int i=0;i<NUM_PWM;i++) {
    uint16_t val = (uint16_t)mb.Hreg(HR_PWM_BASE + i);
    if (val > 255) val = 255;
    pwmLevel[i] = val;
    analogWriteClamp(PWM_PINS[i], val);
  }
}

// ================== Main loop ==================
void loop() {
  hmWatchdogFeed();
  unsigned long now = millis();

  mb.task();                     // Modbus polling
  processModbusCommandPulses();  // consume pulses

  // Monitor for external Modbus writes to PWM registers
  static uint16_t prevPwm[NUM_PWM] = {0,0,0,0,0};
  bool pwmChanged = false;
  for (int i=0;i<NUM_PWM;i++) {
    uint16_t v = (uint16_t)mb.Hreg(HR_PWM_BASE+i);
    if (v != prevPwm[i]) { prevPwm[i] = v; pwmChanged = true; }
  }
  if (pwmChanged) { applyPwmFromHoldingRegs(); }

  // Blink phase (for LED blink mode)
  if (now - lastBlinkToggle >= blinkPeriodMs) { lastBlinkToggle = now; blinkPhase = !blinkPhase; }

  // Auto-save settings after quiet period
  if (cfgDirty && (now - lastCfgTouchMs >= CFG_AUTOSAVE_MS)) {
    if (saveConfigFS()) wsLog("Configuration saved");
    else                wsLog("ERROR: Save failed");
    cfgDirty = false;
  }
  maybePersistOutputState(now);

  // -------- Buttons: read (ACTIVE-LOW), rising edge ----------
  for (int i = 0; i < NUM_BTN; i++) {
    bool pressed = (digitalRead(BTN_PINS[i]) == LOW);
    buttonPrev[i] = buttonState[i];
    buttonState[i] = pressed;

    if (!buttonPrev[i] && buttonState[i]) {
      // Only override toggles supported: 5.. map to Relay1..
      uint8_t act = btnCfg[i].action;
      if (act >= 5 && act < (5+NUM_RLY)) {
        int r = act - 5; if (r >= 0 && r < NUM_RLY) {
          desiredRelay[r] = !desiredRelay[r];
          rlyPulseUntil[r] = 0; // cancel any pending pulse
        }
      }
    }
  }

  // -------- Inputs with Actions & Targets ----------
  JSONVar inputs;
  for (int i = 0; i < NUM_DI; i++) {
    bool val = false;
    if (diCfg[i].enabled) {
      val = (digitalRead(DI_PINS[i]) == HIGH);
      if (diCfg[i].inverted) val = !val;
    }

    bool prev = diState[i];
    diPrev[i]  = prev;
    diState[i] = val;
    inputs[i]  = val;
    mb.setIsts(ISTS_DI_BASE + i, val);

    // Edge detection
    bool rising  = (!prev && val);
    bool falling = (prev && !val);

    // Actions:
    // 1 = Toggle -> toggle on ANY edge (rising or falling)
    // 2 = Pulse  -> toggle on RISING edge only
    uint8_t act = diCfg[i].action;
    if (act == 1) {
      if (rising || falling) {
        applyActionToTarget(diCfg[i].target, 1 /*toggle*/, now);
      }
    } else if (act == 2) {
      if (rising) {
        applyActionToTarget(diCfg[i].target, 1 /*toggle*/, now);
      }
    }
  }

  // -------- Relays: drive outputs from desiredRelay + relay config ----------
  if (!outTrackInit) {
    memcpy(prevDesiredRelay, desiredRelay, sizeof(prevDesiredRelay));
    memcpy(prevPwmLevel, pwmLevel, sizeof(prevPwmLevel));
    outTrackInit = true;
  } else {
    for (int i = 0; i < NUM_RLY; i++) {
      if (desiredRelay[i] != prevDesiredRelay[i]) {
        prevDesiredRelay[i] = desiredRelay[i];
        lastOutChangeMs = now;
      }
    }
    for (int i = 0; i < NUM_PWM; i++) {
      if (pwmLevel[i] != prevPwmLevel[i]) {
        prevPwmLevel[i] = pwmLevel[i];
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
      for (int i = 0; i < NUM_PWM; i++) ext["pwm"][i] = (int)pwmLevel[i];
      WebSerial.send("ext", ext);
    }
  }
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
  for (int i = 0; i < NUM_DI; i++) {
    cfg["in"][i]["enabled"] = diCfg[i].enabled ? 1 : 0;
    cfg["in"][i]["invert"]  = diCfg[i].inverted ? 1 : 0;
    cfg["in"][i]["action"]  = diCfg[i].action;
    cfg["in"][i]["target"]  = diCfg[i].target;
  }
  for (int i = 0; i < NUM_RLY; i++) {
    cfg["relay"][i]["enabled"] = rlyCfg[i].enabled ? 1 : 0;
    cfg["relay"][i]["invert"]  = rlyCfg[i].inverted ? 1 : 0;
    cfg["relay"][i]["powerOn"] = rlyCfg[i].powerOn;
  }
  for (int i = 0; i < NUM_BTN; i++) {
    cfg["btn"][i]["action"] = btnCfg[i].action;
  }
  JSONVar ledList = LedConfigListFromCfg();
  for (int i = 0; i < NUM_LED; i++) {
    cfg["led"][i]["mode"]   = ledList[i]["mode"];
    cfg["led"][i]["source"] = ledList[i]["source"];
  }
  for (int i = 0; i < NUM_PWM; i++) {
    cfg["ext"]["pwmPowerOn"][i] = pwmPowerOn[i];
  }
  WebSerial.send("cfg", cfg);
}

void sendWebBootstrap() {
  sendWebStatus();
  sendWebCfg();
}
