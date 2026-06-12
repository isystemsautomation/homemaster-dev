#include <Arduino.h>
#include <ModbusSerial.h>
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
struct RlyCfg { bool enabled; bool inverted; };
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

// ================== Web Serial ==================
SimpleWebSerial WebSerial;
JSONVar modbusStatus;

// ================== Timing ==================
unsigned long lastSend = 0;
const unsigned long sendInterval = 250;
unsigned long lastBlinkToggle = 0;
const unsigned long blinkPeriodMs = 400;
bool blinkPhase = false;

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
  bool    desiredRelay[NUM_RLY];
  uint16_t pwmLevel[NUM_PWM]; // store 0..255
  uint8_t mb_address;
  uint32_t mb_baud;
  uint32_t crc32;
} __attribute__((packed));

static const uint32_t CFG_MAGIC   = 0x52474231UL; // '1BGR'
static const uint16_t CFG_VERSION = 0x0002;
static const char*    CFG_PATH    = "/cfg_rgb.bin";

volatile bool   cfgDirty        = false;
uint32_t        lastCfgTouchMs  = 0;
const uint32_t  CFG_AUTOSAVE_MS = 1500;

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
  for (int i = 0; i < NUM_RLY; i++) rlyCfg[i] = { true, false };
  for (int i = 0; i < NUM_LED; i++) ledCfg[i] = { 0 /*steady*/, 0 /*source: None*/ };
  for (int i = 0; i < NUM_BTN; i++) btnCfg[i] = { 0 };
  for (int i = 0; i < NUM_RLY; i++) { desiredRelay[i] = false; rlyPulseUntil[i] = 0; }
  for (int i = 0; i < NUM_PWM; i++) pwmLevel[i] = 0;
  g_mb_address = 3; g_mb_baud = 19200;
}

void captureToPersist(PersistConfig &pc) {
  pc.magic   = CFG_MAGIC; pc.version = CFG_VERSION; pc.size = sizeof(PersistConfig);
  memcpy(pc.diCfg,        diCfg,        sizeof(diCfg));
  memcpy(pc.rlyCfg,       rlyCfg,       sizeof(rlyCfg));
  memcpy(pc.ledCfg,       ledCfg,       sizeof(ledCfg));
  memcpy(pc.btnCfg,       btnCfg,       sizeof(btnCfg));
  memcpy(pc.desiredRelay, desiredRelay, sizeof(desiredRelay));
  memcpy(pc.pwmLevel,     pwmLevel,     sizeof(pwmLevel));
  pc.mb_address = g_mb_address; pc.mb_baud = g_mb_baud;
  pc.crc32 = 0; pc.crc32 = crc32_update(0, (const uint8_t*)&pc, sizeof(PersistConfig));
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
  memcpy(desiredRelay, pc.desiredRelay, sizeof(desiredRelay));
  memcpy(pwmLevel,     pc.pwmLevel,     sizeof(pwmLevel));
  g_mb_address = pc.mb_address; g_mb_baud = pc.mb_baud;
  return true;
}

bool saveConfigFS() {
  PersistConfig pc{}; captureToPersist(pc);
  File f = LittleFS.open(CFG_PATH, "w");
  if (!f) { WebSerial.send("message", "save: open failed"); return false; }
  size_t n = f.write((const uint8_t*)&pc, sizeof(pc));
  f.flush();
  f.close();
  if (n != sizeof(pc)) { WebSerial.send("message", String("save: short write ")+n); return false; }
  // quick read-back verify
  File r = LittleFS.open(CFG_PATH, "r");
  if (!r) { WebSerial.send("message", "save: reopen failed"); return false; }
  if ((size_t)r.size() != sizeof(PersistConfig)) { WebSerial.send("message", "save: size mismatch after write"); r.close(); return false; }
  PersistConfig back{}; size_t nr = r.read((uint8_t*)&back, sizeof(back)); r.close();
  if (nr != sizeof(back)) { WebSerial.send("message", "save: short readback"); return false; }
  PersistConfig tmp = back; uint32_t crc = tmp.crc32; tmp.crc32 = 0;
  if (crc32_update(0, (const uint8_t*)&tmp, sizeof(tmp)) != crc) { WebSerial.send("message", "save: CRC verify failed"); return false; }
  return true;
}
bool loadConfigFS() {
  File f = LittleFS.open(CFG_PATH, "r"); if (!f) { WebSerial.send("message", "load: open failed"); return false; }
  if (f.size() != sizeof(PersistConfig)) { WebSerial.send("message", String("load: size ")+f.size()+" != "+sizeof(PersistConfig)); f.close(); return false; }
  PersistConfig pc{}; size_t n = f.read((uint8_t*)&pc, sizeof(pc)); f.close();
  if (n != sizeof(pc)) { WebSerial.send("message", "load: short read"); return false; }
  if (!applyFromPersist(pc)) { WebSerial.send("message", "load: magic/version/crc mismatch"); return false; }
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
void sendAllEchoesOnce();
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
    WebSerial.send("message", "FATAL: Filesystem/config init failed");
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

  // Status for UI
  modbusStatus["address"] = g_mb_address;
  modbusStatus["baud"]    = g_mb_baud;
  modbusStatus["state"]   = 0;

  WebSerial.on("values",  handleValues);
  WebSerial.on("Config",  handleUnifiedConfig);
  WebSerial.on("command", handleCommand);

  WebSerial.send("message", "Boot OK (RGB+CCT via Modbus HR 400..404; DI actions None/Toggle/Pulse; LED source: None/Overridden R1)");
  sendAllEchoesOnce();

  // Apply restored PWM levels to outputs
  applyPwmFromHoldingRegs();
}

// ================== Filesystem init ==================
bool initFilesystemAndConfig() {
  if (!LittleFS.begin()) {
    WebSerial.send("message", "LittleFS mount failed. Formatting…");
    if (!LittleFS.format() || !LittleFS.begin()) {
      WebSerial.send("message", "FATAL: FS mount/format failed");
      return false;
    }
  }

  if (loadConfigFS()) {
    WebSerial.send("message", "Config loaded from flash");
    return true;
  }

  WebSerial.send("message", "No valid config. Using defaults.");
  setDefaults();
  if (saveConfigFS()) {
    WebSerial.send("message", "Defaults saved");
    return true;
  }

  WebSerial.send("message", "First save failed. Formatting FS…");
  if (!LittleFS.format() || !LittleFS.begin()) {
    WebSerial.send("message", "FATAL: FS format failed");
    return false;
  }

  setDefaults();
  if (saveConfigFS()) {
    WebSerial.send("message", "FS formatted and config saved");
    return true;
  }

  WebSerial.send("message", "FATAL: save still failing after format");
  return false;
}

// ================== Command handler / reset ==================
void handleCommand(JSONVar obj) {
  const char* actC = (const char*)obj["action"];
  if (!actC) { WebSerial.send("message", "command: missing 'action'"); return; }
  String act = String(actC); act.toLowerCase();

  if (act == "reset" || act == "reboot") {
    bool ok = saveConfigFS();
    WebSerial.send("message", ok ? "Saved. Rebooting…" : "WARNING: Save verify FAILED. Rebooting anyway…");
    delay(400); performReset();
  } else if (act == "save") {
    if (saveConfigFS()) WebSerial.send("message", "Configuration saved"); else WebSerial.send("message", "ERROR: Save failed");
  } else if (act == "load") {
    if (loadConfigFS()) { WebSerial.send("message", "Configuration loaded"); sendAllEchoesOnce(); applyModbusSettings(g_mb_address, g_mb_baud); applyPwmFromHoldingRegs(); }
    else WebSerial.send("message", "ERROR: Load failed/invalid");
  } else if (act == "factory") {
    setDefaults(); if (saveConfigFS()) { WebSerial.send("message", "Factory defaults restored & saved"); sendAllEchoesOnce(); applyModbusSettings(g_mb_address, g_mb_baud); applyPwmFromHoldingRegs(); }
    else WebSerial.send("message", "ERROR: Save after factory reset failed");
  } else if (act == "off") {
    for (int i=0;i<NUM_PWM;i++){ pwmLevel[i]=0; mb.Hreg(HR_PWM_BASE+i,0); }
    applyPwmFromHoldingRegs();
    cfgDirty = true; lastCfgTouchMs = millis();
    WebSerial.send("message", "All PWM channels set to 0");
  } else {
    WebSerial.send("message", String("Unknown command: ") + actC);
  }
}

void performReset() {
  if (Serial) Serial.flush(); delay(50);
  watchdog_reboot(0, 0, 0);
  while (true) { __asm__("wfi"); }
}

void applyModbusSettings(uint8_t addr, uint32_t baud) {
  if ((uint32_t)modbusStatus["baud"] != baud) { Serial2.end(); Serial2.begin(baud); mb.config(baud); }
  setSlaveIdIfAvailable(mb, addr);
  g_mb_address = addr; g_mb_baud = baud;
  modbusStatus["address"] = g_mb_address; modbusStatus["baud"] = g_mb_baud;
  mb.Hreg(HR_MB_ADDR, g_mb_address);
  mb.Hreg(HR_MB_BAUD, (uint16_t)g_mb_baud);
}

// ================== WebSerial config handlers ==================
void handleValues(JSONVar values) {
  // Optional: Update Modbus settings
  int addr = (int)values["mb_address"];
  int baud = (int)values["mb_baud"];
  if (addr) { addr = constrain(addr, 1, 255); g_mb_address = (uint8_t)addr; }
  if (baud) { baud = constrain(baud, 9600, 115200); g_mb_baud = (uint32_t)baud; }
  applyModbusSettings(g_mb_address, g_mb_baud);

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
  WebSerial.send("message", "Values updated");
  cfgDirty = true; lastCfgTouchMs = millis();
}

// Supported types: inputEnable, inputInvert, inputAction, inputTarget, relays, buttons, leds
void handleUnifiedConfig(JSONVar obj) {
  const char* t = (const char*)obj["t"]; JSONVar list = obj["list"]; if (!t) return;
  String type = String(t); bool changed = false;

  if (type == "inputEnable") {
    for (int i=0;i<NUM_DI && i<list.length();i++) diCfg[i].enabled = (bool)list[i];
    WebSerial.send("message", "Input Enabled list updated"); changed = true;

  } else if (type == "inputInvert") {
    for (int i=0;i<NUM_DI && i<list.length();i++) diCfg[i].inverted = (bool)list[i];
    WebSerial.send("message", "Input Invert list updated"); changed = true;

  } else if (type == "inputAction") {
    for (int i=0;i<NUM_DI && i<list.length();i++) {
      int a = (int)list[i];
      diCfg[i].action = (uint8_t)constrain(a, 0, 2); // 0=None,1=Toggle,2=Pulse
    }
    WebSerial.send("message", "Input Action list updated"); changed = true;

  } else if (type == "inputTarget") {
    for (int i=0;i<NUM_DI && i<list.length();i++) {
      int tgt = (int)list[i];
      // 4=None, 0=All, 1..NUM_RLY
      diCfg[i].target = (uint8_t)((tgt==4 || tgt==0 || (tgt>=1 && tgt<=NUM_RLY)) ? tgt : 0);
    }
    WebSerial.send("message", "Input Control Target list updated"); changed = true;

  } else if (type == "relays") {
    for (int i = 0; i < NUM_RLY && i < list.length(); i++) {
      rlyCfg[i].enabled  = (bool)list[i]["enabled"];
      rlyCfg[i].inverted = (bool)list[i]["inverted"];
    }
    WebSerial.send("message", "Relay Configuration updated"); changed = true;

  } else if (type == "buttons") {
    for (int i = 0; i < NUM_BTN && i < list.length(); i++) {
      int a = (int)list[i]["action"]; btnCfg[i].action = (uint8_t)constrain(a, 0, 5); // 5 -> Relay1 toggle
    }
    WebSerial.send("message", "Buttons Configuration updated"); changed = true;

  } else if (type == "leds") {
    for (int i = 0; i < NUM_LED && i < list.length(); i++) {
      ledCfg[i].mode   = (uint8_t)constrain((int)list[i]["mode"],   0, 1);        // 0 steady, 1 blink
      int src          = (int)list[i]["source"];
      // 0=None, 5..(5+NUM_RLY-1)=Overridden relay n
      ledCfg[i].source = (uint8_t)((src==0 || src==5) ? src : 0);
    }
    WebSerial.send("message", "LEDs Configuration updated"); changed = true;

  } else {
    WebSerial.send("message", "Unknown Config type");
  }

  if (changed) { cfgDirty = true; lastCfgTouchMs = millis(); }
}

// ================== Modbus command pulses ==================
void processModbusCommandPulses() {
  // Relay ON/OFF
  for (int r=0; r<NUM_RLY; r++) {
    if (mb.Coil(CMD_RLY_ON_BASE + r))  { mb.setCoil(CMD_RLY_ON_BASE + r,  false); desiredRelay[r] = true;  rlyPulseUntil[r] = 0; cfgDirty = true; lastCfgTouchMs = millis(); }
    if (mb.Coil(CMD_RLY_OFF_BASE + r)) { mb.setCoil(CMD_RLY_OFF_BASE + r, false); desiredRelay[r] = false; rlyPulseUntil[r] = 0; cfgDirty = true; lastCfgTouchMs = millis(); }
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
  cfgDirty = true; lastCfgTouchMs = now;
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
  if (pwmChanged) { applyPwmFromHoldingRegs(); cfgDirty = true; lastCfgTouchMs = now; }

  // Blink phase (for LED blink mode)
  if (now - lastBlinkToggle >= blinkPeriodMs) { lastBlinkToggle = now; blinkPhase = !blinkPhase; }

  // Auto-save after quiet period
  if (cfgDirty && (now - lastCfgTouchMs >= CFG_AUTOSAVE_MS)) {
    if (saveConfigFS()) WebSerial.send("message", "Configuration saved");
    else                WebSerial.send("message", "ERROR: Save failed");
    cfgDirty = false;
  }

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
          cfgDirty = true; lastCfgTouchMs = millis();
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
    // Determine "source active"
    bool srcActive = false;
    uint8_t src = ledCfg[i].source;            // 0=None, 5.. -> relays
    if (src >= 5 && src < (5+NUM_RLY)) {
      int r = src - 5;                         // 0..
      bool relLogical = (r >=0 && r < NUM_RLY) ? (bool)relayStateList[r] : false; // logical relay (after cfg)
      srcActive = relLogical;
    }

    bool phys = (ledCfg[i].mode == 0) ? srcActive : (srcActive && blinkPhase);
    LedStateList[i] = phys;
    digitalWrite(LED_PINS[i], phys ? HIGH : LOW);
    mb.setIsts(ISTS_LED_BASE + i, phys);
  }

  // -------- WebSerial UI updates --------
  if (millis() - lastSend >= sendInterval) {
    lastSend = millis();
    WebSerial.check();
    WebSerial.send("status", modbusStatus);

    JSONVar invertList, enableList, actionList, targetList;
    for (int i = 0; i < NUM_DI; i++) {
      invertList[i] = diCfg[i].inverted;
      enableList[i] = diCfg[i].enabled;
      actionList[i] = diCfg[i].action;     // 0=None,1=Toggle,2=Pulse
      targetList[i] = diCfg[i].target;     // 4=None,0=All,1..NUM_RLY
    }

    JSONVar relayEnableList, relayInvertList;
    for (int i = 0; i < NUM_RLY; i++) { relayEnableList[i] = rlyCfg[i].enabled; relayInvertList[i] = rlyCfg[i].inverted; }

    JSONVar ButtonStateList, ButtonGroupList;
    for (int i = 0; i < NUM_BTN; i++) { ButtonStateList[i] = buttonState[i]; ButtonGroupList[i] = btnCfg[i].action; }

    JSONVar LedConfigList = LedConfigListFromCfg();

    JSONVar PwmLevels;
    for (int i=0;i<NUM_PWM;i++) PwmLevels[i] = (int)mb.Hreg(HR_PWM_BASE+i);

    WebSerial.send("inputs", inputs);
    WebSerial.send("invertList", invertList);
    WebSerial.send("enableList", enableList);
    WebSerial.send("inputActionList", actionList);
    WebSerial.send("inputTargetList", targetList);

    WebSerial.send("relayStateList", relayStateList);
    WebSerial.send("relayEnableList", relayEnableList);
    WebSerial.send("relayInvertList", relayInvertList);

    WebSerial.send("ButtonStateList", ButtonStateList);
    WebSerial.send("ButtonGroupList", ButtonGroupList);

    WebSerial.send("LedConfigList", LedConfigList);
    WebSerial.send("LedStateList", LedStateList);

    WebSerial.send("PwmLevels", PwmLevels); // R,G,B,WW,CW (0..255)
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

void sendAllEchoesOnce() {
  JSONVar enableList, invertList, actionList, targetList;
  for (int i = 0; i < NUM_DI; i++) {
    enableList[i] = diCfg[i].enabled;
    invertList[i] = diCfg[i].inverted;
    actionList[i] = diCfg[i].action;
    targetList[i] = diCfg[i].target; // 4=None,0=All,1..
  }
  WebSerial.send("enableList", enableList);
  WebSerial.send("invertList", invertList);
  WebSerial.send("inputActionList", actionList);
  WebSerial.send("inputTargetList", targetList);

  JSONVar relayEnableList, relayInvertList;
  for (int i = 0; i < NUM_RLY; i++) { relayEnableList[i] = rlyCfg[i].enabled; relayInvertList[i] = rlyCfg[i].inverted; }
  WebSerial.send("relayEnableList", relayEnableList);
  WebSerial.send("relayInvertList", relayInvertList);

  JSONVar ButtonGroupList; for (int i = 0; i < NUM_BTN; i++) ButtonGroupList[i] = btnCfg[i].action;
  WebSerial.send("ButtonGroupList", ButtonGroupList);

  WebSerial.send("LedConfigList", LedConfigListFromCfg());

  modbusStatus["address"] = g_mb_address; modbusStatus["baud"] = g_mb_baud;
  WebSerial.send("status", modbusStatus);

  JSONVar PwmLevels;
  for (int i=0;i<NUM_PWM;i++) PwmLevels[i] = (int)pwmLevel[i];
  WebSerial.send("PwmLevels", PwmLevels);
}
