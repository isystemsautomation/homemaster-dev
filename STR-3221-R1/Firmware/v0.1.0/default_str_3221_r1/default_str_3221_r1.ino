/*
 * STR-3221-R1 v0.1.0 — 32-channel stair LED controller (4× TLC59208F over I2C)
 *
 * Modbus RTU map (register numbers = Modbus address in this library)
 * -------------------------------------------------------------------
 * Discrete inputs (FC=02):
 *   1..3     IO1..IO3 logical state (after enable + invert)
 *   20..23   BUTTON1..BUTTON4 pressed (1=pressed)
 *   90..91   LED1..LED2 logical state
 *
 * Command coils (FC=05/15, auto-clear pulse):
 *   300..302  pulse ENABLE  IO1..IO3
 *   320..322  pulse DISABLE IO1..IO3
 *
 * Holding registers (FC=03/06/16):
 *   400..431  O1..O32 brightness 0..255 (TLC59208F PWM)
 *   480       Modbus slave address (R/W)
 *   481       Modbus baud rate (R/W, whitelist 9600..115200; хранится сырым значением,
 *             115200 не представим в uint16 -> читается как 0, ставится только через WebConfig)
 *
 * Input registers (FC=04, identity block base 0x00C8 = 200):
 *   200..204  MODEL_ID, FW_MAJOR, FW_MINOR, FW_PATCH, MAP_VERSION
 *
 * GPIO (STR MCU board schematic — not README §8.3):
 *   UART TX/RX  GPIO4/5   RS-485 via MAX485 (DE/RE auto, TxenPin=-1)
 *   I2C SDA/SCL GPIO6/7   4× TLC59208F @ 0x40..0x43 (U9..U12)
 *   LED2/LED1   GPIO8/9   status LEDs
 *   IO2/IO1/IO3 GPIO10/11/12  isolated digital inputs
 *   BUTTON1..4  GPIO16..19  active-LOW, INPUT_PULLUP
 *
 * TLC59208F channel map (strap A2:A1:A0):
 *   U9  0x40  O1..O8
 *   U10 0x41  O9..O16
 *   U11 0x42  O17..O24
 *   U12 0x43  O25..O32
 */

#include <Arduino.h>
#include <Wire.h>
#include <ModbusSerial.h>
#include "hm_common.h"
#define HM_MODEL_ID   8
#define HM_FW_MAJOR   0
#define HM_FW_MINOR   1
#define HM_FW_PATCH   0
#define HM_FW         "0.1.0"
#define HM_MAP_VERSION 1
#include <SimpleWebSerial.h>
#include <Arduino_JSON.h>
#include <LittleFS.h>
#include <utility>
#include "hardware/watchdog.h"

struct PersistConfig;  // forward decl for Arduino auto-prototypes

// ================== UART2 (RS-485 / Modbus) ==================
#define TX2 4
#define RX2 5
const int TxenPin = -1;
int SlaveId = 1;
ModbusSerial mb(Serial2, SlaveId, TxenPin);

// ================== GPIO MAP (STR MCU board) ==================
#define PIN_IO1   11
#define PIN_IO2   10
#define PIN_IO3   12
#define PIN_LED1  9
#define PIN_LED2  8
#define PIN_BTN1  16
#define PIN_BTN2  17
#define PIN_BTN3  18
#define PIN_BTN4  19
#define PIN_I2C_SDA 6
#define PIN_I2C_SCL 7

static const uint8_t DI_PINS[3]  = {PIN_IO1, PIN_IO2, PIN_IO3};
static const uint8_t LED_PINS[2] = {PIN_LED1, PIN_LED2};
static const uint8_t BTN_PINS[4] = {PIN_BTN1, PIN_BTN2, PIN_BTN3, PIN_BTN4};

static const uint8_t NUM_DI   = 3;
static const uint8_t NUM_LED  = 2;
static const uint8_t NUM_BTN  = 4;
static const uint8_t NUM_PWM  = 32;

// ================== TLC59208F ==================
static const uint8_t TLC_ADDR[4] = {0x40, 0x41, 0x42, 0x43};
static const uint8_t TLC_REG_MODE1  = 0x00;
static const uint8_t TLC_REG_MODE2  = 0x01;
static const uint8_t TLC_REG_PWM0   = 0x02;
static const uint8_t TLC_REG_LEDOUT = 0x0C;
static const uint8_t TLC_LEDOUT_INDIVIDUAL_PWM = 0xAA;

static uint8_t tlcApplied[NUM_PWM];
static bool    tlcReady = false;

// ================== Config & runtime ==================
struct InCfg  { bool enabled; bool inverted; uint8_t action; uint8_t target; };
struct LedCfg { uint8_t mode; uint8_t source; };
struct BtnCfg { uint8_t action; };

InCfg  diCfg[NUM_DI];
LedCfg ledCfg[NUM_LED];
BtnCfg btnCfg[NUM_BTN];

bool buttonState[NUM_BTN] = {false, false, false, false};
bool buttonPrev[NUM_BTN]  = {false, false, false, false};
bool diState[NUM_DI]      = {false, false, false};
bool diPrev[NUM_DI]       = {false, false, false};

uint16_t pwmLevel[NUM_PWM];

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
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  InCfg    diCfg[NUM_DI];
  LedCfg   ledCfg[NUM_LED];
  BtnCfg   btnCfg[NUM_BTN];
  uint16_t pwmLevel[NUM_PWM];
  uint8_t  mb_address;
  uint32_t mb_baud;
  uint32_t crc32;
} __attribute__((packed));

static const uint32_t CFG_MAGIC   = 0x53545231UL; // '1RTS'
static const uint16_t CFG_VERSION = 0x0001;
static const char*    CFG_PATH    = "/cfg_str.bin";

volatile bool   cfgDirty        = false;
uint32_t        lastCfgTouchMs  = 0;
const uint32_t  CFG_AUTOSAVE_MS = 1500;

// ================== Modbus addresses ==================
enum : uint16_t {
  ISTS_DI_BASE  = 1,
  ISTS_BTN_BASE = 20,
  ISTS_LED_BASE = 90,
  CMD_DI_EN_BASE  = 300,
  CMD_DI_DIS_BASE = 320,
  HR_PWM_BASE = 400,
  HR_MB_ADDR  = 480,
  HR_MB_BAUD  = 481
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

template <class M>
inline auto setSlaveIdIfAvailable(M& m, uint8_t id)
  -> decltype(std::declval<M&>().setSlaveId(uint8_t{}), void()) { m.setSlaveId(id); }
inline void setSlaveIdIfAvailable(...) {}

// ================== TLC59208F driver ==================
static bool tlcWriteReg(uint8_t addr, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

static bool tlcWritePwm(uint8_t chipIdx, uint8_t ch, uint8_t val) {
  if (chipIdx >= 4 || ch >= 8) return false;
  return tlcWriteReg(TLC_ADDR[chipIdx], TLC_REG_PWM0 + ch, val);
}

static bool tlcInitChip(uint8_t addr) {
  if (!tlcWriteReg(addr, TLC_REG_MODE1, 0x00)) return false;
  if (!tlcWriteReg(addr, TLC_REG_MODE2, 0x00)) return false;
  if (!tlcWriteReg(addr, TLC_REG_LEDOUT, TLC_LEDOUT_INDIVIDUAL_PWM)) return false;
  for (uint8_t ch = 0; ch < 8; ch++) {
    if (!tlcWriteReg(addr, TLC_REG_PWM0 + ch, 0)) return false;
  }
  return true;
}

static bool tlcInitAll() {
  Wire.setSDA(PIN_I2C_SDA);
  Wire.setSCL(PIN_I2C_SCL);
  Wire.begin();
  Wire.setClock(400000);
  for (uint8_t i = 0; i < 4; i++) {
    if (!tlcInitChip(TLC_ADDR[i])) return false;
  }
  for (uint8_t i = 0; i < NUM_PWM; i++) tlcApplied[i] = 0xFF;
  tlcReady = true;
  return true;
}

static void applyPwmChannel(uint8_t idx, uint16_t val) {
  if (idx >= NUM_PWM) return;
  if (val > 255) val = 255;
  const uint8_t v = (uint8_t)val;
  pwmLevel[idx] = val;
  if (!tlcReady) return;
  if (tlcApplied[idx] == v) return;
  tlcApplied[idx] = v;
  tlcWritePwm(idx / 8, idx % 8, v);
}

static void applyPwmFromHoldingRegs() {
  for (uint8_t i = 0; i < NUM_PWM; i++) {
    uint16_t val = (uint16_t)mb.Hreg(HR_PWM_BASE + i);
    applyPwmChannel(i, val);
  }
}

static void applyAllPwmLevels() {
  for (uint8_t i = 0; i < NUM_PWM; i++) applyPwmChannel(i, pwmLevel[i]);
}

// ================== Defaults / persist ==================
void setDefaults() {
  for (int i = 0; i < NUM_DI; i++) diCfg[i] = {true, false, 0, 4};
  for (int i = 0; i < NUM_LED; i++) ledCfg[i] = {0, 0};
  for (int i = 0; i < NUM_BTN; i++) btnCfg[i] = {0};
  for (int i = 0; i < NUM_PWM; i++) pwmLevel[i] = 0;
  g_mb_address = 3;
  g_mb_baud    = 19200;
}

void captureToPersist(PersistConfig &pc) {
  pc.magic = CFG_MAGIC;
  pc.version = CFG_VERSION;
  pc.size = sizeof(PersistConfig);
  memcpy(pc.diCfg, diCfg, sizeof(diCfg));
  memcpy(pc.ledCfg, ledCfg, sizeof(ledCfg));
  memcpy(pc.btnCfg, btnCfg, sizeof(btnCfg));
  memcpy(pc.pwmLevel, pwmLevel, sizeof(pwmLevel));
  pc.mb_address = g_mb_address;
  pc.mb_baud = g_mb_baud;
  pc.crc32 = 0;
  pc.crc32 = crc32_update(0, (const uint8_t*)&pc, sizeof(PersistConfig));
}

bool applyFromPersist(const PersistConfig &pc) {
  if (pc.magic != CFG_MAGIC || pc.size != sizeof(PersistConfig)) return false;
  PersistConfig tmp = pc;
  uint32_t crc = tmp.crc32;
  tmp.crc32 = 0;
  if (crc32_update(0, (const uint8_t*)&tmp, sizeof(PersistConfig)) != crc) return false;
  if (pc.version != CFG_VERSION) return false;

  memcpy(diCfg, pc.diCfg, sizeof(diCfg));
  memcpy(ledCfg, pc.ledCfg, sizeof(ledCfg));
  memcpy(btnCfg, pc.btnCfg, sizeof(btnCfg));
  memcpy(pwmLevel, pc.pwmLevel, sizeof(pwmLevel));
  g_mb_address = pc.mb_address;
  g_mb_baud = pc.mb_baud;
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
  PersistConfig verify = back;
  uint32_t crc = verify.crc32;
  verify.crc32 = 0;
  if (crc32_update(0, (const uint8_t*)&verify, sizeof(verify)) != crc) { wsLog("save: CRC verify failed"); return false; }
  return true;
}

bool loadConfigFS() {
  File f = LittleFS.open(CFG_PATH, "r");
  if (!f) { wsLog("load: open failed"); return false; }
  if (f.size() != sizeof(PersistConfig)) {
    wsLog(String("load: size ") + f.size() + " != " + sizeof(PersistConfig));
    f.close();
    return false;
  }
  PersistConfig pc{};
  size_t n = f.read((uint8_t*)&pc, sizeof(pc));
  f.close();
  if (n != sizeof(pc)) { wsLog("load: short read"); return false; }
  if (!applyFromPersist(pc)) { wsLog("load: magic/version/crc mismatch"); return false; }
  return true;
}

// ================== Fw decls ==================
bool initFilesystemAndConfig();
void applyModbusSettings(uint8_t addr, uint32_t baud);
void handleValues(JSONVar values);
void handleUnifiedConfig(JSONVar obj);
void handleCommand(JSONVar obj);
void performReset();
void processModbusCommandPulses();
void sendWebStatus();
void sendWebCfg();
void sendWebBootstrap();
bool ledSourceActive(uint8_t source);
void markCfgDirty();

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
    return true;
  }

  wsLog("No valid config. Using defaults.");
  setDefaults();
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
  if (saveConfigFS()) {
    wsLog("FS formatted and config saved");
    return true;
  }

  wsLog("FATAL: save still failing after format");
  return false;
}

// ================== Modbus / Web handlers ==================
void applyModbusSettings(uint8_t addr, uint32_t baud) {
  addr = hmValidAddress(addr);
  baud = hmValidBaud(baud);
  if (g_mb_baud != baud) {
    Serial2.end();
    Serial2.begin(baud);
    mb.config(baud);
  }
  setSlaveIdIfAvailable(mb, addr);
  g_mb_address = addr;
  g_mb_baud = baud;
  mb.Hreg(HR_MB_ADDR, g_mb_address);
  mb.Hreg(HR_MB_BAUD, (g_mb_baud > 65535UL) ? (uint16_t)0 : (uint16_t)g_mb_baud);
}

void handleValues(JSONVar values) {
  int addr = (int)values["mb_address"];
  int baud = (int)values["mb_baud"];
  if (addr) g_mb_address = hmValidAddress(addr);
  if (baud) g_mb_baud = hmValidBaud(baud);
  applyModbusSettings(g_mb_address, g_mb_baud);

  if (values.hasOwnProperty("pwm")) {
    JSONVar arr = values["pwm"];
    for (int i = 0; i < NUM_PWM && i < arr.length(); i++) {
      uint16_t v = (uint16_t)constrain((int)arr[i], 0, 255);
      pwmLevel[i] = v;
      mb.Hreg(HR_PWM_BASE + i, v);
      applyPwmChannel(i, v);
    }
  }

  wsLog("Modbus configuration updated");
  sendWebStatus();
}

void handleCommand(JSONVar obj) {
  const char* actC = (const char*)obj["action"];
  if (!actC) { wsLog("command: missing 'action'"); return; }
  String act = String(actC);
  act.toLowerCase();

  if (act == "reset" || act == "reboot") {
    bool ok = saveConfigFS();
    wsLog(ok ? "Saved. Rebooting…" : "WARNING: Save verify FAILED. Rebooting anyway…");
    delay(400);
    performReset();
  } else if (act == "save") {
    if (saveConfigFS()) wsLog("Configuration saved");
    else wsLog("ERROR: Save failed");
  } else if (act == "load") {
    if (loadConfigFS()) {
      for (int i = 0; i < NUM_PWM; i++) mb.Hreg(HR_PWM_BASE + i, pwmLevel[i]);
      applyAllPwmLevels();
      wsLog("Configuration loaded");
      sendWebBootstrap();
      applyModbusSettings(g_mb_address, g_mb_baud);
    } else {
      wsLog("ERROR: Load failed/invalid");
    }
  } else if (act == "factory") {
    setDefaults();
    for (int i = 0; i < NUM_PWM; i++) mb.Hreg(HR_PWM_BASE + i, pwmLevel[i]);
    applyAllPwmLevels();
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
    wsLog("Identify: status LEDs active for 5 s");
  } else if (act == "off") {
    for (int i = 0; i < NUM_PWM; i++) {
      pwmLevel[i] = 0;
      mb.Hreg(HR_PWM_BASE + i, 0);
      applyPwmChannel(i, 0);
    }
    wsLog("All output channels set to 0");
  } else {
    wsLog(String("Unknown command: ") + actC);
  }
}

void handleUnifiedConfig(JSONVar obj) {
  const char* t = (const char*)obj["t"];
  JSONVar list = obj["list"];
  if (!t) { wsLog("Config: missing 't'"); return; }

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
  } else if (type == "in.action" || type == "inputAction") {
    for (int i = 0; i < NUM_DI && i < list.length(); i++) {
      diCfg[i].action = (uint8_t)constrain((int)list[i], 0, 2);
    }
    wsLog("Input Action list updated");
    changed = true;
  } else if (type == "in.target" || type == "inputTarget") {
    for (int i = 0; i < NUM_DI && i < list.length(); i++) {
      int tgt = (int)list[i];
      diCfg[i].target = (uint8_t)((tgt == 4 || tgt == 0) ? tgt : 4);
    }
    wsLog("Input Control Target list updated");
    changed = true;
  } else if (type == "btn" || type == "buttons") {
    for (int i = 0; i < NUM_BTN && i < list.length(); i++) {
      if (list[i].hasOwnProperty("action")) {
        btnCfg[i].action = (uint8_t)constrain((int)list[i]["action"], 0, 0);
      } else {
        btnCfg[i].action = (uint8_t)constrain((int)list[i], 0, 0);
      }
    }
    wsLog("Buttons Configuration updated");
    changed = true;
  } else if (type == "led" || type == "leds") {
    for (int i = 0; i < NUM_LED && i < list.length(); i++) {
      ledCfg[i].mode = (uint8_t)constrain((int)list[i]["mode"], 0, 1);
      int src = (int)list[i]["source"];
      ledCfg[i].source = (uint8_t)((src == 0 || (src >= 10 && src <= 12)) ? src : 0);
    }
    wsLog("LEDs Configuration updated");
    changed = true;
  } else if (type == "ext.pwm") {
    for (int i = 0; i < NUM_PWM && i < list.length(); i++) {
      uint16_t v = (uint16_t)constrain((int)list[i], 0, 255);
      pwmLevel[i] = v;
      mb.Hreg(HR_PWM_BASE + i, v);
      applyPwmChannel(i, v);
    }
    wsLog("Output levels updated");
    sendWebCfg();               // без markCfgDirty(): яркости не персистим автоматически
  } else {
    wsLog(String("Unknown Config type: ") + t);
  }

  if (changed) {
    markCfgDirty();
    sendWebCfg();
  }
}

void performReset() {
  if (Serial) Serial.flush();
  delay(50);
  watchdog_reboot(0, 0, 0);
  while (true) { __asm__("wfi"); }
}

void processModbusCommandPulses() {
  for (int i = 0; i < NUM_DI; i++) {
    if (mb.Coil(CMD_DI_EN_BASE + i)) {
      mb.setCoil(CMD_DI_EN_BASE + i, false);
      if (!diCfg[i].enabled) { diCfg[i].enabled = true; markCfgDirty(); }
    }
    if (mb.Coil(CMD_DI_DIS_BASE + i)) {
      mb.setCoil(CMD_DI_DIS_BASE + i, false);
      if (diCfg[i].enabled) { diCfg[i].enabled = false; markCfgDirty(); }
    }
  }
}

inline void markCfgDirty() {
  cfgDirty = true;
  lastCfgTouchMs = millis();
}

bool ledSourceActive(uint8_t source) {
  if (source >= 10 && source <= 12) {
    const int idx = source - 10;
    if (idx >= 0 && idx < NUM_DI) return diState[idx];
  }
  return false;
}

void sendWebStatus() {
  JSONVar st;
  st["model"] = HM_MODEL_ID;
  st["fw"]    = HM_FW;
  st["map"]   = HM_MAP_VERSION;
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
  for (int i = 0; i < NUM_BTN; i++) cfg["btn"][i]["action"] = btnCfg[i].action;
  for (int i = 0; i < NUM_LED; i++) {
    cfg["led"][i]["mode"]   = ledCfg[i].mode;
    cfg["led"][i]["source"] = ledCfg[i].source;
  }
  for (int i = 0; i < NUM_PWM; i++) cfg["ext"]["pwm"][i] = (int)pwmLevel[i];
  WebSerial.send("cfg", cfg);
}

void sendWebBootstrap() {
  sendWebStatus();
  sendWebCfg();
}

// ================== Setup ==================
void setup() {
  Serial.begin(57600);

  for (uint8_t i = 0; i < NUM_DI; i++) pinMode(DI_PINS[i], INPUT);
  for (uint8_t i = 0; i < NUM_LED; i++) {
    pinMode(LED_PINS[i], OUTPUT);
    digitalWrite(LED_PINS[i], LOW);
  }
  for (uint8_t i = 0; i < NUM_BTN; i++) pinMode(BTN_PINS[i], INPUT_PULLUP);

  setDefaults();
  if (!initFilesystemAndConfig()) wsLog("FATAL: Filesystem/config init failed");
  if (!tlcInitAll()) wsLog("FATAL: TLC59208F init failed");

  Serial2.setTX(TX2);
  Serial2.setRX(RX2);
  Serial2.begin(g_mb_baud);
  mb.config(g_mb_baud);
  setSlaveIdIfAvailable(mb, g_mb_address);
  mb.setAdditionalServerData("STR3221-32CH");

  for (uint16_t i = 0; i < NUM_DI; i++) mb.addIsts(ISTS_DI_BASE + i);
  for (uint16_t i = 0; i < NUM_BTN; i++) mb.addIsts(ISTS_BTN_BASE + i);
  for (uint16_t i = 0; i < NUM_LED; i++) mb.addIsts(ISTS_LED_BASE + i);
  for (uint16_t i = 0; i < NUM_DI; i++) {
    mb.addCoil(CMD_DI_EN_BASE + i);
    mb.setCoil(CMD_DI_EN_BASE + i, false);
    mb.addCoil(CMD_DI_DIS_BASE + i);
    mb.setCoil(CMD_DI_DIS_BASE + i, false);
  }
  for (uint16_t i = 0; i < NUM_PWM; i++) {
    mb.addHreg(HR_PWM_BASE + i);
    mb.Hreg(HR_PWM_BASE + i, pwmLevel[i]);
  }
  mb.addHreg(HR_MB_ADDR);
  mb.Hreg(HR_MB_ADDR, g_mb_address);
  mb.addHreg(HR_MB_BAUD);
  mb.Hreg(HR_MB_BAUD, (g_mb_baud > 65535UL) ? (uint16_t)0 : (uint16_t)g_mb_baud);

  hmRegisterIdentity(mb, HM_MODEL_ID, HM_FW_MAJOR, HM_FW_MINOR, HM_FW_PATCH, HM_MAP_VERSION);

  WebSerial.on("values", handleValues);
  WebSerial.on("Config", handleUnifiedConfig);
  WebSerial.on("command", handleCommand);

  wsLog("Boot OK (STR-3221-R1: 32ch TLC59208F @ HR400..431; IO1..IO3 @ DI1..3; buttons @ DI20..23)");
  sendWebBootstrap();
  applyAllPwmLevels();
  hmWatchdogArm(4000);
}

// ================== Main loop ==================
void loop() {
  hmWatchdogFeed();
  const uint32_t now = millis();

  mb.task();
  processModbusCommandPulses();

  static uint16_t prevPwm[NUM_PWM];
  static bool prevInit = false;
  if (!prevInit) {
    for (int i = 0; i < NUM_PWM; i++) prevPwm[i] = (uint16_t)mb.Hreg(HR_PWM_BASE + i);
    prevInit = true;
  }
  bool pwmChanged = false;
  for (int i = 0; i < NUM_PWM; i++) {
    uint16_t v = (uint16_t)mb.Hreg(HR_PWM_BASE + i);
    if (v != prevPwm[i]) {
      prevPwm[i] = v;
      pwmChanged = true;
    }
  }
  if (pwmChanged) {
    applyPwmFromHoldingRegs();   // яркости не персистим: сохранение только по команде save/reset/factory
  }

  if (now - lastBlinkToggle >= blinkPeriodMs) {
    lastBlinkToggle = now;
    blinkPhase = !blinkPhase;
  }

  if (cfgDirty && (now - lastCfgTouchMs >= CFG_AUTOSAVE_MS)) {
    if (saveConfigFS()) wsLog("Configuration saved");
    else wsLog("ERROR: Save failed");
    cfgDirty = false;
  }

  for (int i = 0; i < NUM_BTN; i++) {
    const bool pressed = (digitalRead(BTN_PINS[i]) == LOW);
    buttonPrev[i] = buttonState[i];
    buttonState[i] = pressed;
    mb.setIsts(ISTS_BTN_BASE + i, pressed);
  }

  JSONVar inputs;
  for (int i = 0; i < NUM_DI; i++) {
    bool val = false;
    if (diCfg[i].enabled) {
      val = (digitalRead(DI_PINS[i]) == HIGH);
      if (diCfg[i].inverted) val = !val;
    }
    diPrev[i] = diState[i];
    diState[i] = val;
    inputs[i] = val ? 1 : 0;
    mb.setIsts(ISTS_DI_BASE + i, val);
  }

  const bool identifying = (int32_t)(g_identifyUntilMs - now) > 0;

  JSONVar ledStates;
  for (int i = 0; i < NUM_LED; i++) {
    bool phys = false;
    if (identifying) {
      phys = (ledCfg[i].mode == 0) ? true : blinkPhase;
    } else {
      const bool srcActive = ledSourceActive(ledCfg[i].source);
      phys = (ledCfg[i].mode == 0) ? srcActive : (srcActive && blinkPhase);
    }
    ledStates[i] = phys ? 1 : 0;
    digitalWrite(LED_PINS[i], phys ? HIGH : LOW);
    mb.setIsts(ISTS_LED_BASE + i, phys);
  }

  if (millis() - lastSend >= sendInterval) {
    lastSend = millis();
    WebSerial.check();
    if (hmUsbCanSend()) {
      sendWebStatus();

      JSONVar io;
      for (int i = 0; i < NUM_DI; i++) io["in"][i] = inputs[i];
      for (int i = 0; i < NUM_BTN; i++) io["btn"][i] = buttonState[i] ? 1 : 0;
      for (int i = 0; i < NUM_LED; i++) io["led"][i] = ledStates[i];
      WebSerial.send("io", io);

      JSONVar ext;
      for (int i = 0; i < NUM_PWM; i++) ext["pwm"][i] = (int)pwmLevel[i];
      WebSerial.send("ext", ext);
    }
  }
}
