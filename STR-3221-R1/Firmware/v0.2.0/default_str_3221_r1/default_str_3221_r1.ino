/*
 * STR-3221-R1 v0.2.0 (beta) — 32-channel stair LED controller (4× TLC59208F over I2C)
 *
 * Modbus RTU map (register numbers = Modbus address in this library)
 * -------------------------------------------------------------------
 * Input registers (FC=04) — contiguous runtime block 0..31 (one merged poll):
 *   0  IREG_DI_MASK       bit0..2 IO1..IO3 (logical, after enable + invert)
 *   1  IREG_BTN_MASK      bit0..3 BUTTON1..4 pressed
 *   2  IREG_LED_MASK      bit0..1 status LED1..2 physical state
 *   3  IREG_STATUS_FLAGS  bit0=tlcReady, bit1=linkOk, bit2=busFailsafe,
 *                         bit3=cfgDirty, bit4=localOverride
 *   4  IREG_TLC_MASK      bit0..3 TLC59208F chip OK U9..U12
 *   5  IREG_I2C_ERRORS    failed TLC transaction count (saturating)
 *   6  IREG_RESET_REASON  0=unknown/POR, 1=watchdog stall, 2=commanded reboot
 *   7  IREG_LINK_AGE_S    seconds since last frame to this slave (saturating)
 *   8  IREG_UPTIME_MIN    minutes since boot (saturating)
 *   9  reserved (0)
 *  10..25 IREG_OUT_BASE   O1..O32 level readback — low byte odd, high byte even
 *  26..31 reserved (0) — sequencer / heating (later steps)
 *
 * Command coils (FC=05/15, auto-clear pulse):
 *   300..302  pulse ENABLE  IO1..IO3
 *   320..322  pulse DISABLE IO1..IO3
 *   330       pulse SAVE output levels to flash (rate-limited 10 s)
 *   331       pulse RELEASE local override
 *
 * Holding registers (FC=03/06/16) — config / write surface; not polled:
 *   400..431  O1..O32 brightness 0..255 (TLC59208F PWM)
 *   480       Modbus slave address (R/W)
 *   481       Modbus baud rate (R/W, whitelist 9600..115200; stored raw,
 *             115200 not representable in uint16 → reads as 0, set via WebConfig)
 *
 * Input registers (FC=04, identity block base 0x00C8 = 200):
 *   200..204  MODEL_ID, FW_MAJOR, FW_MINOR, FW_PATCH, MAP_VERSION
 *
 * GPIO (STR MCU board schematic — not README §8.3):
 *   UART TX/RX  GPIO4/5   RS-485 via MAX485 (DE/RE auto, TxenPin=-1)
 *   I2C SDA/SCL GPIO6/7 (Wire1 / I2C1)   4× TLC59208F (U9..U12)
 *   LED2/LED1   GPIO8/9   onboard indicators (not front-panel O.1..O32 — those follow TLC PWM)
 *   Field inputs (Modbus DI1..3):
 *     DI1 / DI     GPIO11  ISO1212 24 V — idle 0 V, active 3 V (HIGH)
 *     DI2 / SENS.B GPIO12  SFH6156 U17 — idle 3 V, active 0 V (LOW)
 *     DI3 / SENS.A GPIO10  SFH6156 U18 — idle 3 V, active 0 V (LOW)
 *   BUTTON1..4  GPIO16..19  active-HIGH via CD4069 (INPUT, pressed=HIGH)
 *
 * TLC59208F address map (TI datasheet Table 1):
 *   Strap GND/VCC on A2:A1:A0 → 0x40, 0x42, 0x44, 0x46 (U9..U12 typical)
 *   Strap GND/SCL/SDA on A2:A1:A0 → 0x20..0x3E range (A1=SCL on bus)
 *   Runtime auto-bind from I2C scan (prefers block 0x20..0x23 if present)
 */

#include <Arduino.h>
#include <Wire.h>
#include <ModbusSerial.h>
#include "hm_common.h"
#define HM_MODEL_ID   8
#define HM_FW_MAJOR   0
#define HM_FW_MINOR   2
#define HM_FW_PATCH   0
#define HM_FW         "0.2.0"
// Rule 13: the map version IS the module version. Derived, never hand-kept.
#define HM_MAP_VERSION ((HM_FW_MAJOR << 8) | (HM_FW_MINOR << 4) | HM_FW_PATCH)  // 0x0020 = v0.2.0
#include <SimpleWebSerial.h>
#include <Arduino_JSON.h>
#include <LittleFS.h>
#include <utility>
#include "hardware/watchdog.h"

// Arduino IDE inserts function prototypes before struct definitions — forward-declare persist types.
struct PersistConfigV3;
struct PersistConfigV4;
typedef PersistConfigV4 PersistConfig;

// ================== UART2 (RS-485 / Modbus) ==================
#define TX2 4
#define RX2 5
const int TxenPin = -1;
int SlaveId = 3;   // constructor seed only — setup() overrides from g_mb_address (canon default 3)
ModbusSerial mb(Serial2, SlaveId, TxenPin);

// ================== GPIO MAP (STR MCU board) ==================
// Modbus DI1=GPIO11 ISO1212, DI2=GPIO12 SENS.B, DI3=GPIO10 SENS.A.
#define PIN_DI      11
#define PIN_SENS_B  12
#define PIN_SENS_A  10
#define PIN_LED1  9
#define PIN_LED2  8
#define PIN_BTN1  16
#define PIN_BTN2  17
#define PIN_BTN3  18
#define PIN_BTN4  19
#define PIN_I2C_SDA 6
#define PIN_I2C_SCL 7

static const uint8_t DI_PINS[3]  = { PIN_DI, PIN_SENS_B, PIN_SENS_A };
static const bool    DI_ACTIVE_HIGH[3] = { true, false, false };
static const uint8_t LED_PINS[2] = {PIN_LED1, PIN_LED2};
static const uint8_t BTN_PINS[4] = {PIN_BTN1, PIN_BTN2, PIN_BTN3, PIN_BTN4};

static const uint8_t NUM_DI   = 3;
static const uint8_t NUM_LED  = 2;
static const uint8_t NUM_BTN  = 4;
static const uint8_t NUM_PWM  = 32;

// ================== TLC59208F ==================
static const uint8_t TLC_ADDR_DEFAULT[4] = {0x40, 0x42, 0x44, 0x46};
static const uint8_t TLC_ADDR_SCL_BLOCK[4] = {0x20, 0x21, 0x22, 0x23};
static uint8_t tlcAddrActive[4] = {0x40, 0x42, 0x44, 0x46};
static const uint8_t TLC_ADDR_BIND_MIN = 0x20;
static const uint8_t TLC_ADDR_BIND_MAX = 0x5E;
static const uint8_t I2C_SCAN_MIN = 0x08;
static const uint8_t I2C_SCAN_MAX = 0x77;
static const uint8_t I2C_FOUND_MAX = 24;

static uint8_t i2cFound[I2C_FOUND_MAX];
static uint8_t i2cFoundCount = 0;
static const uint8_t TLC_REG_MODE1   = 0x00;
static const uint8_t TLC_REG_MODE2   = 0x01;
static const uint8_t TLC_REG_PWM0    = 0x02;
static const uint8_t TLC_REG_LEDOUT0 = 0x0C;
static const uint8_t TLC_REG_LEDOUT1 = 0x0D;
static const uint8_t TLC_LEDOUT_INDIVIDUAL_PWM = 0xAA;

static uint8_t tlcApplied[NUM_PWM];
static bool    tlcChipOk[4] = {false, false, false, false};
static bool    tlcReady = false;
static uint32_t tlcNextRetryMs = 0;
static const uint32_t TLC_RETRY_MS = 5000;
static uint16_t g_i2cErrorCount = 0;
static uint16_t g_bootResetReason = 0;
static uint32_t g_bootMs = 0;
static uint16_t iregCache[32];
static const uint16_t IREG_BLOCK_COUNT = 32;

// ================== Config & runtime ==================
struct InCfgV3 { bool enabled; bool inverted; uint8_t action; uint8_t target; };
struct InCfg  { bool enabled; bool inverted; uint8_t action; uint8_t target; uint8_t level; };
struct LedCfg { uint8_t mode; uint8_t source; };
struct BtnCfg { uint8_t action; };

enum : uint8_t { FS_HOLD = 0, FS_OFF = 1, FS_LEVEL = 2 };

InCfg  diCfg[NUM_DI];
LedCfg ledCfg[NUM_LED];
BtnCfg btnCfg[NUM_BTN];

uint16_t g_busTimeoutS = 0;
uint16_t g_overrideTimeoutS = 0;
uint8_t  g_failsafeAction[NUM_PWM];
uint8_t  g_failsafeLevel[NUM_PWM];
bool     g_localOverride = false;
uint32_t g_overrideSinceMs = 0;
bool     g_linkFrameSeen = false;
bool     g_busFailsafeActive = false;
bool     g_inputToggleState[NUM_DI] = {false, false, false};
uint32_t g_lastPwmSaveMs = 0;

bool buttonState[NUM_BTN] = {false, false, false, false};
bool buttonPrev[NUM_BTN]  = {false, false, false, false};
bool diState[NUM_DI]      = {false, false, false};
bool diPrev[NUM_DI]       = {false, false, false};

uint16_t pwmLevel[NUM_PWM];

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
struct PersistConfigV3 {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  InCfgV3  diCfg[NUM_DI];
  LedCfg   ledCfg[NUM_LED];
  BtnCfg   btnCfg[NUM_BTN];
  uint16_t pwmLevel[NUM_PWM];
  uint8_t  mb_address;
  uint32_t mb_baud;
  uint32_t crc32;
} __attribute__((packed));

struct PersistConfigV4 {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  InCfg    diCfg[NUM_DI];
  LedCfg   ledCfg[NUM_LED];
  BtnCfg   btnCfg[NUM_BTN];
  uint16_t pwmLevel[NUM_PWM];
  uint8_t  mb_address;
  uint32_t mb_baud;
  uint16_t busTimeoutS;
  uint16_t overrideTimeoutS;
  uint8_t  failsafeAction[NUM_PWM];
  uint8_t  failsafeLevel[NUM_PWM];
  uint32_t crc32;
} __attribute__((packed));

static const uint32_t CFG_MAGIC      = 0x53545231UL; // '1RTS'
static const uint16_t CFG_VERSION_V3 = 0x0003;
static const uint16_t CFG_VERSION    = 0x0004;

// MCU board: CD4069 inverts button signals — pressed reads HIGH (same as ENM/WLD/DIM).
static constexpr bool BUTTON_PRESSED_LOW = false;
static const char*    CFG_PATH    = "/cfg_str.bin";

volatile bool   cfgDirty        = false;
uint32_t        lastCfgTouchMs  = 0;
const uint32_t  CFG_AUTOSAVE_MS = 1500;

// ================== Modbus map ==================
enum : uint16_t {
  IREG_DI_MASK       = 0,
  IREG_BTN_MASK      = 1,
  IREG_LED_MASK      = 2,
  IREG_STATUS_FLAGS  = 3,
  IREG_TLC_MASK      = 4,
  IREG_I2C_ERRORS    = 5,
  IREG_RESET_REASON  = 6,
  IREG_LINK_AGE_S    = 7,
  IREG_UPTIME_MIN    = 8,
  IREG_RESERVED_9    = 9,
  IREG_OUT_BASE      = 10,
  IREG_RESERVED_26   = 26,

  CMD_DI_EN_BASE  = 300,
  CMD_DI_DIS_BASE = 320,
  COIL_SAVE_PWM   = 330,
  COIL_RELEASE_OVERRIDE = 331,
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

static const uint8_t TLC_SWRST_ADDR = 0x96;
static const uint32_t I2C_TIMEOUT_MS = 25;

static TwoWire* tlcWire = &Wire1;
static bool tlcWireStarted = false;

// ================== TLC59208F driver ==================
static uint8_t i2cProbeErr(uint8_t addr) {
  tlcWire->beginTransmission(addr);
  return tlcWire->endTransmission();
}

static bool i2cProbe(uint8_t addr) {
  return i2cProbeErr(addr) == 0;
}

static void tlcWireBegin() {
  if (!tlcWireStarted) {
    pinMode(PIN_I2C_SDA, INPUT_PULLUP);
    pinMode(PIN_I2C_SCL, INPUT_PULLUP);
    Wire1.setSDA(PIN_I2C_SDA);
    Wire1.setSCL(PIN_I2C_SCL);
    Wire1.begin();
    Wire1.setTimeout(I2C_TIMEOUT_MS);
    Wire1.setClock(100000);
    tlcWire = &Wire1;
    tlcWireStarted = true;
    delay(2);
  }
}

static void tlcSoftwareReset() {
  tlcWire->beginTransmission(TLC_SWRST_ADDR);
  tlcWire->write(0xA5);
  tlcWire->write(0x5A);
  tlcWire->endTransmission();
  delay(2);
}

static void tlcNoteI2cFailure() {
  if (g_i2cErrorCount < 65535) g_i2cErrorCount++;
}

static bool tlcWriteReg(uint8_t addr, uint8_t reg, uint8_t val) {
  tlcWire->beginTransmission(addr);
  tlcWire->write(reg);
  tlcWire->write(val);
  const bool ok = (tlcWire->endTransmission() == 0);
  if (!ok) tlcNoteI2cFailure();
  return ok;
}

static bool tlcReadReg(uint8_t addr, uint8_t reg, uint8_t* val) {
  tlcWire->beginTransmission(addr);
  tlcWire->write(reg);
  if (tlcWire->endTransmission(false) != 0) return false;
  if (tlcWire->requestFrom(addr, (uint8_t)1) != 1) return false;
  *val = tlcWire->read();
  return true;
}

static void tlcLoadDefaultAddrs() {
  for (uint8_t i = 0; i < 4; i++) tlcAddrActive[i] = TLC_ADDR_DEFAULT[i];
}

static bool i2cFoundHas(uint8_t addr) {
  for (uint8_t i = 0; i < i2cFoundCount; i++) {
    if (i2cFound[i] == addr) return true;
  }
  return false;
}

static bool i2cFoundHasAll(const uint8_t* addrs, uint8_t count) {
  for (uint8_t i = 0; i < count; i++) {
    if (!i2cFoundHas(addrs[i])) return false;
  }
  return true;
}

static void tlcSetAddrs(const uint8_t* addrs, uint8_t count) {
  tlcLoadDefaultAddrs();
  const uint8_t use = (count > 4) ? 4 : count;
  for (uint8_t i = 0; i < use; i++) tlcAddrActive[i] = addrs[i];
}

static bool tlcVerifyChip(uint8_t addr) {
  if (!i2cProbe(addr)) return false;
  if (!tlcWriteReg(addr, TLC_REG_MODE1, 0x01)) return false; // clear SLEEP
  delayMicroseconds(500);
  uint8_t mode1 = 0;
  if (!tlcReadReg(addr, TLC_REG_MODE1, &mode1)) return false;
  return (mode1 & 0x10) == 0;
}

static uint8_t i2cBusScan(uint8_t start, uint8_t end) {
  i2cFoundCount = 0;
  String found = "I2C bus scan:";
  uint8_t n = 0;
  for (uint8_t a = start; a <= end; a++) {
    if (i2cProbe(a)) {
      if (i2cFoundCount < I2C_FOUND_MAX) i2cFound[i2cFoundCount++] = a;
      if (n == 0) found += " found";
      found += " 0x";
      if (a < 0x10) found += "0";
      found += String(a, HEX);
      n++;
    }
    if ((a & 0x0F) == 0x0F) {
      yield();
      hmWatchdogFeed();
    }
  }
  if (n == 0) {
    found += " (none; SDA=";
    found += digitalRead(PIN_I2C_SDA) ? "H" : "L";
    found += " SCL=";
    found += digitalRead(PIN_I2C_SCL) ? "H" : "L";
    found += ")";
  }
  wsLog(found);
  return n;
}

static uint8_t tlcBindFromScan() {
  if (i2cFoundHasAll(TLC_ADDR_SCL_BLOCK, 4)) {
    tlcSetAddrs(TLC_ADDR_SCL_BLOCK, 4);
    wsLog("TLC bind: block 0x20 0x21 0x22 0x23 (A1=SCL strap range)");
    return 4;
  }
  if (i2cFoundHasAll(TLC_ADDR_DEFAULT, 4)) {
    tlcSetAddrs(TLC_ADDR_DEFAULT, 4);
    wsLog("TLC bind: block 0x40 0x42 0x44 0x46 (GND/VCC strap range)");
    return 4;
  }

  uint8_t candidates[8];
  uint8_t n = 0;
  for (uint8_t i = 0; i < i2cFoundCount && n < 8; i++) {
    const uint8_t a = i2cFound[i];
    if (a < TLC_ADDR_BIND_MIN || a > TLC_ADDR_BIND_MAX) continue;
    if (a == TLC_SWRST_ADDR) continue;
    candidates[n++] = a;
  }
  for (uint8_t i = 0; i + 1 < n; i++) {
    for (uint8_t j = i + 1; j < n; j++) {
      if (candidates[j] < candidates[i]) {
        const uint8_t t = candidates[i];
        candidates[i] = candidates[j];
        candidates[j] = t;
      }
    }
  }

  uint8_t verified[4];
  uint8_t v = 0;
  for (uint8_t i = 0; i < n && v < 4; i++) {
    if (tlcVerifyChip(candidates[i])) verified[v++] = candidates[i];
    yield();
    hmWatchdogFeed();
  }
  if (v >= 1) {
    tlcSetAddrs(verified, v);
    String msg = "TLC bind (verified):";
    for (uint8_t i = 0; i < v; i++) {
      msg += " 0x";
      if (tlcAddrActive[i] < 0x10) msg += "0";
      msg += String(tlcAddrActive[i], HEX);
    }
    wsLog(msg);
    return v;
  }

  if (n >= 4) {
    tlcSetAddrs(candidates, 4);
    String msg = "TLC bind (unverified, first 4 in 0x20..0x5E):";
    for (uint8_t i = 0; i < 4; i++) {
      msg += " 0x";
      if (tlcAddrActive[i] < 0x10) msg += "0";
      msg += String(tlcAddrActive[i], HEX);
    }
    wsLog(msg);
    return 4;
  }

  tlcLoadDefaultAddrs();
  wsLog("TLC bind: no 4-chip block, using defaults 0x40 0x42 0x44 0x46");
  return 0;
}

static void i2cScanLog() {
  i2cBusScan(I2C_SCAN_MIN, I2C_SCAN_MAX);
  tlcBindFromScan();
}

static bool tlcWritePwm(uint8_t chipIdx, uint8_t ch, uint8_t val) {
  if (chipIdx >= 4 || ch >= 8 || !tlcChipOk[chipIdx]) return false;
  return tlcWriteReg(tlcAddrActive[chipIdx], TLC_REG_PWM0 + ch, val);
}

static bool tlcFlushChip(uint8_t chipIdx) {
  if (chipIdx >= 4 || !tlcChipOk[chipIdx]) return false;
  const uint8_t addr = tlcAddrActive[chipIdx];
  tlcWire->beginTransmission(addr);
  uint8_t wrote = 0;
  wrote += (uint8_t)tlcWire->write(0xA2);  // AI=101, start at PWM0 — eight bytes auto-increment
  for (uint8_t ch = 0; ch < 8; ch++) {
    const uint8_t idx = (uint8_t)(chipIdx * 8 + ch);
    const uint8_t v = (uint8_t)constrain((int)pwmLevel[idx], 0, 255);
    wrote += (uint8_t)tlcWire->write(v);
  }
  if (wrote != 9 || tlcWire->endTransmission() != 0) {
    tlcNoteI2cFailure();
    return false;
  }
  for (uint8_t ch = 0; ch < 8; ch++) {
    const uint8_t idx = (uint8_t)(chipIdx * 8 + ch);
    tlcApplied[idx] = (uint8_t)constrain((int)pwmLevel[idx], 0, 255);
  }
  return true;
}

static bool tlcInitChip(uint8_t chipIdx, uint8_t addr) {
  tlcChipOk[chipIdx] = false;
  if (!i2cProbe(addr)) return false;
  if (!tlcWriteReg(addr, TLC_REG_MODE1, 0x00)) return false;
  if (!tlcWriteReg(addr, TLC_REG_MODE2, 0x00)) return false;
  if (!tlcWriteReg(addr, TLC_REG_LEDOUT0, TLC_LEDOUT_INDIVIDUAL_PWM)) return false;
  if (!tlcWriteReg(addr, TLC_REG_LEDOUT1, TLC_LEDOUT_INDIVIDUAL_PWM)) return false;
  delayMicroseconds(500);
  for (uint8_t ch = 0; ch < 8; ch++) {
    if (!tlcWriteReg(addr, TLC_REG_PWM0 + ch, 0)) return false;
    yield();
  }
  tlcChipOk[chipIdx] = true;
  return true;
}

static bool tlcInitAll(bool fullScan) {
  tlcWireBegin();
  if (fullScan) {
    i2cScanLog();
  } else {
    String found = "I2C quick:";
    uint8_t n = 0;
    for (uint8_t i = 0; i < 4; i++) {
      const uint8_t addr = tlcAddrActive[i];
      if (i2cProbe(addr)) {
        if (n == 0) found += " ";
        found += "0x";
        if (addr < 0x10) found += "0";
        found += String(addr, HEX);
        n++;
      }
      yield();
      hmWatchdogFeed();
    }
    if (n == 0) {
      found += " (none @ bound addrs)";
      wsLog(found);
    }
  }

  bool anyTlc = false;
  for (uint8_t i = 0; i < 4; i++) {
    if (i2cProbe(tlcAddrActive[i])) { anyTlc = true; break; }
    yield();
    hmWatchdogFeed();
  }
  if (anyTlc && i2cFoundHasAll(TLC_ADDR_SCL_BLOCK, 4)) tlcSoftwareReset();

  uint8_t chipsOk = 0;
  for (uint8_t i = 0; i < 4; i++) {
    if (tlcInitChip(i, tlcAddrActive[i])) chipsOk++;
    yield();
    hmWatchdogFeed();
  }
  tlcReady = (chipsOk > 0);
  if (!tlcReady) return false;
  Wire1.setClock(400000);
  for (uint8_t i = 0; i < NUM_PWM; i++) tlcApplied[i] = 0xFF;
  if (chipsOk < 4) {
    wsLog(String("TLC59208F: ") + chipsOk + "/4 chips OK");
  } else {
    wsLog("TLC59208F: 4/4 chips OK");
  }
  applyAllPwmLevels();
  return true;
}

static bool readDiHardware(uint8_t idx) {
  if (idx >= NUM_DI) return false;
  const bool pinHigh = (digitalRead(DI_PINS[idx]) == HIGH);
  return DI_ACTIVE_HIGH[idx] ? pinHigh : !pinHigh;
}

static bool diLiveState(uint8_t mbIdx) {
  bool val = readDiHardware(mbIdx);
  if (diCfg[mbIdx].inverted) val = !val;
  return val;
}

static void applyPwmChannel(uint8_t idx, uint16_t val) {
  if (idx >= NUM_PWM) return;
  if (val > 255) val = 255;
  const uint8_t v = (uint8_t)val;
  pwmLevel[idx] = val;
  const uint8_t chipIdx = idx / 8;
  if (!tlcReady || !tlcChipOk[chipIdx]) return;
  if (tlcApplied[idx] == v) return;
  tlcApplied[idx] = v;
  tlcWritePwm(chipIdx, idx % 8, v);
}

static void applyPwmFromHoldingRegs() {
  for (uint8_t i = 0; i < NUM_PWM; i++) {
    uint16_t val = (uint16_t)mb.Hreg(HR_PWM_BASE + i);
    if (val > 255) val = 255;
    pwmLevel[i] = val;
  }
  applyAllPwmLevels();
}

static void applyAllPwmLevels() {
  for (uint8_t chipIdx = 0; chipIdx < 4; chipIdx++) {
    if (!tlcReady || !tlcChipOk[chipIdx]) continue;
    uint8_t diffCount = 0;
    uint8_t loneIdx = 0;
    for (uint8_t ch = 0; ch < 8; ch++) {
      const uint8_t idx = (uint8_t)(chipIdx * 8 + ch);
      const uint8_t v = (uint8_t)constrain((int)pwmLevel[idx], 0, 255);
      if (tlcApplied[idx] != v) {
        diffCount++;
        loneIdx = idx;
      }
    }
    if (diffCount >= 2) tlcFlushChip(chipIdx);
    else if (diffCount == 1) applyPwmChannel(loneIdx, pwmLevel[loneIdx]);
  }
}

static inline bool outputsModbusLocked() {
  return g_localOverride || g_busFailsafeActive;
}

static void drivePwmOutputs() {
  applyAllPwmLevels();
}

static void setAllPwmLocal(uint16_t val) {
  if (val > 255) val = 255;
  for (int i = 0; i < NUM_PWM; i++) pwmLevel[i] = val;
  drivePwmOutputs();
}

static void applyRampLevelsLocal() {
  for (int i = 0; i < NUM_PWM; i++) {
    pwmLevel[i] = (NUM_PWM > 1) ? (uint16_t)((i * 255) / (NUM_PWM - 1)) : 255;
  }
  drivePwmOutputs();
}

static void restoreOutputsFromHoldingRegs() {
  for (uint8_t i = 0; i < NUM_PWM; i++) {
    uint16_t v = (uint16_t)mb.Hreg(HR_PWM_BASE + i);
    if (v > 255) v = 255;
    pwmLevel[i] = v;
  }
  drivePwmOutputs();
}

static void enterLocalOverride() {
  if (g_busFailsafeActive) return;
  if (!g_localOverride) g_overrideSinceMs = millis();
  g_localOverride = true;
}

static void releaseLocalOverride() {
  if (!g_localOverride) return;
  g_localOverride = false;
  for (int i = 0; i < NUM_DI; i++) g_inputToggleState[i] = false;
  if (!g_busFailsafeActive) {
    restoreOutputsFromHoldingRegs();
    wsLog("Local override released — outputs match holding registers");
  }
}

static void releaseLocalOverrideForWebConfig() {
  if (!g_localOverride) return;
  g_localOverride = false;
  for (int i = 0; i < NUM_DI; i++) g_inputToggleState[i] = false;
  wsLog("WebConfig: local override released");
}

static uint16_t detectBootResetReason() {
  // pico-sdk scratch[4]: WATCHDOG_NON_REBOOT_MAGIC set by watchdog_enable (stall path);
  // cleared to 0 by watchdog_reboot (commanded reset path). See watchdog_enable_caused_reboot().
  if (!watchdog_caused_reboot()) return 0;
  if (watchdog_enable_caused_reboot()) return 1;
  return 2;
}

static void applyFailsafeOnce() {
  for (int i = 0; i < NUM_PWM; i++) {
    switch (g_failsafeAction[i]) {
      case FS_OFF:   pwmLevel[i] = 0; break;
      case FS_LEVEL: pwmLevel[i] = g_failsafeLevel[i]; break;
      default: break; // HOLD — leave channel unchanged
    }
  }
  drivePwmOutputs();
}

static void processBusFailsafe(uint32_t now) {
  bool linkLost = false;
  if (g_linkFrameSeen && g_busTimeoutS > 0) {
    linkLost = ((uint32_t)(now - g_lastLinkSeenMs) >= (uint32_t)g_busTimeoutS * 1000UL);
  }

  if (!g_busFailsafeActive && linkLost) {
    g_busFailsafeActive = true;
    g_localOverride = false;
    applyFailsafeOnce();
    wsLog(String("Bus failsafe active (no poll for ") + g_busTimeoutS + " s)");
  } else if (g_busFailsafeActive && !linkLost) {
    g_busFailsafeActive = false;
    restoreOutputsFromHoldingRegs();
    wsLog("Bus link restored — outputs match holding registers");
  }
}

static bool anyPulseInputActive() {
  for (int i = 0; i < NUM_DI; i++) {
    if (!diCfg[i].enabled || diCfg[i].target != 0 || diCfg[i].action != 2) continue;
    if (diState[i]) return true;
  }
  return false;
}

static void processInputActions() {
  if (g_busFailsafeActive) return;

  for (int i = 0; i < NUM_DI; i++) {
    if (!diCfg[i].enabled || diCfg[i].target != 0) continue;

    if (diCfg[i].action == 1) {
      if (diState[i] && !diPrev[i]) {
        g_inputToggleState[i] = !g_inputToggleState[i];
        if (g_inputToggleState[i]) setAllPwmLocal(diCfg[i].level);
        else setAllPwmLocal(0);
        enterLocalOverride();
      }
    } else if (diCfg[i].action == 2) {
      if (diState[i] != diPrev[i]) {
        if (diState[i]) {
          setAllPwmLocal(diCfg[i].level);
          enterLocalOverride();
        } else if (!anyPulseInputActive()) {
          releaseLocalOverride();
        }
      }
    }
  }
}

static void processButtonActions() {
  const bool comboBoot  = buttonState[0] && buttonState[1];
  const bool comboReset = buttonState[2] && buttonState[3];
  if (comboReset) return;

  for (int i = 0; i < NUM_BTN; i++) {
    if (!buttonState[i] || buttonPrev[i]) continue; // rising edge; loop is fast enough (~ms)
    if (i <= 1 && comboBoot) continue;
    if (i >= 2 && comboReset) continue;

    switch (btnCfg[i].action) {
      case 1:
        if (!g_busFailsafeActive) { setAllPwmLocal(255); enterLocalOverride(); }
        break;
      case 2:
        if (!g_busFailsafeActive) { setAllPwmLocal(0); enterLocalOverride(); }
        break;
      case 3:
        if (!g_busFailsafeActive) { applyRampLevelsLocal(); enterLocalOverride(); }
        break;
      case 4:
        releaseLocalOverride();
        break;
      default:
        break;
    }
  }
}

static void processOverrideTimeout(uint32_t now) {
  if (!g_localOverride || g_overrideTimeoutS == 0 || g_busFailsafeActive) return;
  if ((uint32_t)(now - g_overrideSinceMs) >= (uint32_t)g_overrideTimeoutS * 1000UL) {
    wsLog("Local override timed out");
    releaseLocalOverride();
  }
}

static bool saveConfigFS(); // defined below — used by savePwmLevelsToFlash

static void savePwmLevelsToFlash() {
  const uint32_t now = millis();
  if ((uint32_t)(now - g_lastPwmSaveMs) < 10000UL) {
    wsLog("PWM save rate-limited (10 s)");
    return;
  }
  if (saveConfigFS()) {
    g_lastPwmSaveMs = now;
    wsLog("Output levels saved to flash");
  } else {
    wsLog("ERROR: Output level save failed");
  }
}

// ================== Defaults / persist ==================
static void applyFailsafeDefaults() {
  g_busTimeoutS = 0;
  g_overrideTimeoutS = 0;
  for (int i = 0; i < NUM_PWM; i++) {
    g_failsafeAction[i] = FS_HOLD;
    g_failsafeLevel[i] = 0;
  }
}

void setDefaults() {
  for (int i = 0; i < NUM_DI; i++) diCfg[i] = {true, false, 0, 4, 255};
  for (int i = 0; i < NUM_LED; i++) ledCfg[i] = {0, 0};
  btnCfg[0].action = 1; // SW1 = All ON (README promise)
  btnCfg[1].action = 2; // SW2 = All OFF
  btnCfg[2].action = 0;
  btnCfg[3].action = 0;
  for (int i = 0; i < NUM_PWM; i++) pwmLevel[i] = 0;
  g_mb_address = 3;
  g_mb_baud    = 19200;
  applyFailsafeDefaults();
}

static bool verifyPersistCrcV3(const PersistConfigV3 &pc) {
  PersistConfigV3 tmp = pc;
  const uint32_t crc = tmp.crc32;
  tmp.crc32 = 0;
  return crc32_update(0, (const uint8_t*)&tmp, sizeof(PersistConfigV3)) == crc;
}

static bool verifyPersistCrcV4(const PersistConfigV4 &pc) {
  PersistConfigV4 tmp = pc;
  const uint32_t crc = tmp.crc32;
  tmp.crc32 = 0;
  return crc32_update(0, (const uint8_t*)&tmp, sizeof(PersistConfigV4)) == crc;
}

static void fillV4Defaults(PersistConfigV4 &pc) {
  pc.busTimeoutS = 0;
  pc.overrideTimeoutS = 0;
  for (int i = 0; i < NUM_PWM; i++) {
    pc.failsafeAction[i] = FS_HOLD;
    pc.failsafeLevel[i] = 0;
  }
}

static bool applyFromPersistV4(const PersistConfigV4 &pc) {
  if (pc.magic != CFG_MAGIC || pc.size != sizeof(PersistConfigV4)) return false;
  if (!verifyPersistCrcV4(pc)) return false;
  if (pc.version != CFG_VERSION) return false;

  memcpy(diCfg, pc.diCfg, sizeof(diCfg));
  memcpy(ledCfg, pc.ledCfg, sizeof(ledCfg));
  memcpy(btnCfg, pc.btnCfg, sizeof(btnCfg));
  memcpy(pwmLevel, pc.pwmLevel, sizeof(pwmLevel));
  g_mb_address = pc.mb_address;
  g_mb_baud = pc.mb_baud;
  g_busTimeoutS = pc.busTimeoutS;
  g_overrideTimeoutS = pc.overrideTimeoutS;
  for (int i = 0; i < NUM_PWM; i++) {
    const uint8_t act = pc.failsafeAction[i];
    g_failsafeAction[i] = (act <= FS_LEVEL) ? act : FS_HOLD;
    g_failsafeLevel[i] = pc.failsafeLevel[i];
  }
  return true;
}

static bool migrateV3ToV4(const PersistConfigV3 &pc3, PersistConfigV4 &pc4) {
  if (pc3.magic != CFG_MAGIC || pc3.size != sizeof(PersistConfigV3)) return false;
  if (!verifyPersistCrcV3(pc3)) return false;
  if (pc3.version != CFG_VERSION_V3) return false;

  memset(&pc4, 0, sizeof(pc4));
  pc4.magic = CFG_MAGIC;
  pc4.version = CFG_VERSION;
  pc4.size = sizeof(PersistConfigV4);
  for (int i = 0; i < NUM_DI; i++) {
    pc4.diCfg[i].enabled = pc3.diCfg[i].enabled;
    pc4.diCfg[i].inverted = pc3.diCfg[i].inverted;
    pc4.diCfg[i].action = pc3.diCfg[i].action;
    pc4.diCfg[i].target = pc3.diCfg[i].target;
    pc4.diCfg[i].level = 255;
  }
  memcpy(pc4.ledCfg, pc3.ledCfg, sizeof(pc4.ledCfg));
  memcpy(pc4.btnCfg, pc3.btnCfg, sizeof(pc4.btnCfg));
  memcpy(pc4.pwmLevel, pc3.pwmLevel, sizeof(pc4.pwmLevel));
  pc4.mb_address = pc3.mb_address;
  pc4.mb_baud = pc3.mb_baud;
  fillV4Defaults(pc4);
  return true;
}

void captureToPersist(PersistConfigV4 &pc) {
  pc.magic = CFG_MAGIC;
  pc.version = CFG_VERSION;
  pc.size = sizeof(PersistConfig);
  memcpy(pc.diCfg, diCfg, sizeof(diCfg));
  memcpy(pc.ledCfg, ledCfg, sizeof(ledCfg));
  memcpy(pc.btnCfg, btnCfg, sizeof(btnCfg));
  memcpy(pc.pwmLevel, pwmLevel, sizeof(pwmLevel));
  pc.mb_address = g_mb_address;
  pc.mb_baud = g_mb_baud;
  pc.busTimeoutS = g_busTimeoutS;
  pc.overrideTimeoutS = g_overrideTimeoutS;
  memcpy(pc.failsafeAction, g_failsafeAction, sizeof(g_failsafeAction));
  memcpy(pc.failsafeLevel, g_failsafeLevel, sizeof(g_failsafeLevel));
  pc.crc32 = 0;
  pc.crc32 = crc32_update(0, (const uint8_t*)&pc, sizeof(PersistConfigV4));
}

static bool saveConfigFS() {
  PersistConfigV4 pc{};
  captureToPersist(pc);
  File f = LittleFS.open(CFG_PATH, "w");
  if (!f) { wsLog("save: open failed"); return false; }
  size_t n = f.write((const uint8_t*)&pc, sizeof(pc));
  f.flush();
  f.close();
  if (n != sizeof(pc)) { wsLog(String("save: short write ") + n); return false; }
  File r = LittleFS.open(CFG_PATH, "r");
  if (!r) { wsLog("save: reopen failed"); return false; }
  if ((size_t)r.size() != sizeof(PersistConfigV4)) { wsLog("save: size mismatch after write"); r.close(); return false; }
  PersistConfigV4 back{};
  size_t nr = r.read((uint8_t*)&back, sizeof(back));
  r.close();
  if (nr != sizeof(back)) { wsLog("save: short readback"); return false; }
  PersistConfigV4 verify = back;
  uint32_t crc = verify.crc32;
  verify.crc32 = 0;
  if (crc32_update(0, (const uint8_t*)&verify, sizeof(verify)) != crc) { wsLog("save: CRC verify failed"); return false; }
  return true;
}

bool loadConfigFS() {
  File f = LittleFS.open(CFG_PATH, "r");
  if (!f) { wsLog("load: open failed"); return false; }
  const size_t fsz = f.size();

  if (fsz == sizeof(PersistConfigV4)) {
    PersistConfigV4 pc4{};
    const size_t n = f.read((uint8_t*)&pc4, sizeof(pc4));
    f.close();
    if (n != sizeof(pc4)) { wsLog("load: short read"); return false; }
    if (!applyFromPersistV4(pc4)) { wsLog("load: v4 magic/version/crc mismatch"); return false; }
    return true;
  }

  if (fsz == sizeof(PersistConfigV3)) {
    PersistConfigV3 pc3{};
    const size_t n = f.read((uint8_t*)&pc3, sizeof(pc3));
    f.close();
    if (n != sizeof(pc3)) { wsLog("load: short read"); return false; }
    PersistConfigV4 pc4{};
    if (!migrateV3ToV4(pc3, pc4)) { wsLog("load: v3 migrate failed"); return false; }
    if (!applyFromPersistV4(pc4)) { wsLog("load: v4 apply after migrate failed"); return false; }
    wsLog("Config migrated 0x0003 -> 0x0004");
    if (!saveConfigFS()) {
      wsLog("ERROR: migrate re-save failed");
      return false;
    }
    return true;
  }

  f.close();
  wsLog(String("load: unknown size ") + fsz);
  return false;
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
    yield();
    if (!LittleFS.format() || !LittleFS.begin()) {
      wsLog("FATAL: FS mount/format failed");
      return false;
    }
    yield();
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
  yield();
  if (!LittleFS.format() || !LittleFS.begin()) {
    wsLog("FATAL: FS format failed");
    return false;
  }
  yield();

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
  if (addr || baud) markCfgDirty();   // regression lost in the 2026-08-04 rollback

  if (values.hasOwnProperty("pwm")) {
    releaseLocalOverrideForWebConfig();
    JSONVar arr = values["pwm"];
    for (int i = 0; i < NUM_PWM && i < arr.length(); i++) {
      uint16_t v = (uint16_t)constrain((int)arr[i], 0, 255);
      mb.Hreg(HR_PWM_BASE + i, v);
      pwmLevel[i] = v;
    }
    drivePwmOutputs();
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
    g_localOverride = false;
    g_busFailsafeActive = false;
    for (int i = 0; i < NUM_DI; i++) g_inputToggleState[i] = false;
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
  } else if (act == "i2c_scan" || act == "i2cscan") {
    tlcWireBegin();
    i2cScanLog();
    if (!tlcInitAll(true)) wsLog("TLC init still failed after I2C scan");
  } else if (act == "off") {
    releaseLocalOverrideForWebConfig();
    for (int i = 0; i < NUM_PWM; i++) mb.Hreg(HR_PWM_BASE + i, 0);
    setAllPwmLocal(0);
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
    for (int i = 0; i < NUM_DI && i < list.length(); i++)
      diCfg[i].enabled = (bool)list[i];
    wsLog("Input Enabled list updated");
    changed = true;
  } else if (type == "in.invert" || type == "inputInvert") {
    for (int i = 0; i < NUM_DI && i < list.length(); i++)
      diCfg[i].inverted = (bool)list[i];
    wsLog("Input Invert list updated");
    changed = true;
  } else if (type == "in.action" || type == "inputAction") {
    for (int i = 0; i < NUM_DI && i < list.length(); i++)
      diCfg[i].action = (uint8_t)constrain((int)list[i], 0, 2);
    wsLog("Input Action list updated");
    changed = true;
  } else if (type == "in.target" || type == "inputTarget") {
    for (int i = 0; i < NUM_DI && i < list.length(); i++) {
      int tgt = (int)list[i];
      diCfg[i].target = (uint8_t)((tgt == 4 || tgt == 0) ? tgt : 4);
    }
    wsLog("Input Control Target list updated");
    changed = true;
  } else if (type == "in.level" || type == "inputLevel") {
    for (int i = 0; i < NUM_DI && i < list.length(); i++)
      diCfg[i].level = (uint8_t)constrain((int)list[i], 0, 255);
    wsLog("Input Level list updated");
    changed = true;
  } else if (type == "btn" || type == "buttons") {
    for (int i = 0; i < NUM_BTN && i < list.length(); i++) {
      if (list[i].hasOwnProperty("action")) {
        btnCfg[i].action = (uint8_t)constrain((int)list[i]["action"], 0, 4);
      } else {
        btnCfg[i].action = (uint8_t)constrain((int)list[i], 0, 4);
      }
    }
    wsLog("Buttons Configuration updated");
    changed = true;
  } else if (type == "bus.timeout") {
    g_busTimeoutS = (uint16_t)constrain((int)list, 0, 65535);
    wsLog("Bus failsafe timeout updated");
    changed = true;
  } else if (type == "override.timeout") {
    g_overrideTimeoutS = (uint16_t)constrain((int)list, 0, 65535);
    wsLog("Override timeout updated");
    changed = true;
  } else if (type == "bus.failsafe") {
    JSONVar actions = list["actions"];
    JSONVar levels = list["levels"];
    for (int i = 0; i < NUM_PWM && i < actions.length() && i < levels.length(); i++) {
      const uint8_t act = (uint8_t)constrain((int)actions[i], 0, 2);
      g_failsafeAction[i] = act;
      g_failsafeLevel[i] = (uint8_t)constrain((int)levels[i], 0, 255);
    }
    wsLog("Bus failsafe per-channel config updated");
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
    releaseLocalOverrideForWebConfig();
    for (int i = 0; i < NUM_PWM && i < list.length(); i++) {
      uint16_t v = (uint16_t)constrain((int)list[i], 0, 255);
      pwmLevel[i] = v;
      mb.Hreg(HR_PWM_BASE + i, v);
    }
    applyAllPwmLevels();
    wsLog("Output levels updated");
    sendWebCfg();  // brightness not auto-persisted
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
  if (mb.Coil(COIL_SAVE_PWM)) {
    mb.setCoil(COIL_SAVE_PWM, false);
    savePwmLevelsToFlash();
  }
  if (mb.Coil(COIL_RELEASE_OVERRIDE)) {
    mb.setCoil(COIL_RELEASE_OVERRIDE, false);
    releaseLocalOverride();
  }
}

inline void markCfgDirty() {
  cfgDirty = true;
  lastCfgTouchMs = millis();
}

bool ledSourceActive(uint8_t source) {
  if (source == 0) return false;
  for (uint8_t i = 0; i < NUM_DI; i++) {
    if (DI_PINS[i] == source) return diState[i];
  }
  return false;
}

static void updateLinkOkDetector(uint32_t now) {
  // Peek BEFORE mb.task() drains RX. Only frames addressed to us (or broadcast)
  // count as link — shared RS-485 traffic to other slaves must not set linkOk.
  if (Serial2.available() < 1) return;
  const int addr = Serial2.peek();
  if (addr < 0) return;
  if (addr == 0 || addr == (int)g_mb_address) {
    g_lastLinkSeenMs = now;
    g_linkFrameSeen = true;
  }
}

static inline bool linkOkNow(uint32_t now) {
  return ((uint32_t)(now - g_lastLinkSeenMs) < (uint32_t)g_linkTimeoutMs);
}

static inline void setIregIfChanged(uint16_t addr, uint16_t val) {
  if (addr >= IREG_BLOCK_COUNT) return;
  if (iregCache[addr] == val) return;
  iregCache[addr] = val;
  mb.setIreg(addr, val);
}

static void updateInputRegisters(uint32_t now) {
  uint16_t diMask = 0, btnMask = 0, ledMask = 0, tlcMask = 0;

  for (int i = 0; i < NUM_DI; i++) {
    const bool logical = diCfg[i].enabled ? diLiveState(i) : false;
    if (logical) diMask |= (uint16_t)(1u << i);
  }
  for (int i = 0; i < NUM_BTN; i++) {
    if (buttonState[i]) btnMask |= (uint16_t)(1u << i);
  }
  for (int i = 0; i < NUM_LED; i++) {
    const bool on = (digitalRead(LED_PINS[i]) == HIGH);
    if (on) ledMask |= (uint16_t)(1u << i);
  }
  for (int i = 0; i < 4; i++) {
    if (tlcChipOk[i]) tlcMask |= (uint16_t)(1u << i);
  }

  uint16_t status = 0;
  if (tlcReady)              status |= (1u << 0);
  if (linkOkNow(now))        status |= (1u << 1);
  if (g_busFailsafeActive)   status |= (1u << 2);
  if (cfgDirty)              status |= (1u << 3);
  if (g_localOverride)       status |= (1u << 4);

  const uint32_t linkAgeMs = now - g_lastLinkSeenMs;
  uint16_t linkAgeS = (linkAgeMs >= 65535000UL) ? 65535 : (uint16_t)(linkAgeMs / 1000UL);
  const uint32_t upMs = now - g_bootMs;
  uint16_t upMin = (upMs >= 3932100000UL) ? 65535 : (uint16_t)(upMs / 60000UL);

  setIregIfChanged(IREG_DI_MASK, diMask);
  setIregIfChanged(IREG_BTN_MASK, btnMask);
  setIregIfChanged(IREG_LED_MASK, ledMask);
  setIregIfChanged(IREG_STATUS_FLAGS, status);
  setIregIfChanged(IREG_TLC_MASK, tlcMask);
  setIregIfChanged(IREG_I2C_ERRORS, g_i2cErrorCount);
  setIregIfChanged(IREG_RESET_REASON, g_bootResetReason);
  setIregIfChanged(IREG_LINK_AGE_S, linkAgeS);
  setIregIfChanged(IREG_UPTIME_MIN, upMin);
  setIregIfChanged(IREG_RESERVED_9, 0);

  for (uint16_t reg = IREG_OUT_BASE; reg < IREG_OUT_BASE + 16; reg++) {
    const uint8_t base = (uint8_t)((reg - IREG_OUT_BASE) * 2);
    const uint8_t lo = (uint8_t)constrain((int)pwmLevel[base], 0, 255);
    const uint8_t hi = (base + 1 < NUM_PWM)
      ? (uint8_t)constrain((int)pwmLevel[base + 1], 0, 255) : 0;
    setIregIfChanged(reg, (uint16_t)((hi << 8) | lo));
  }

  for (uint16_t reg = IREG_RESERVED_26; reg < IREG_BLOCK_COUNT; reg++) {
    setIregIfChanged(reg, 0);
  }
}

static void buildModbusMap() {
  for (uint16_t i = 0; i < IREG_BLOCK_COUNT; i++) {
    mb.addIreg(i);
    mb.setIreg(i, 0);
    iregCache[i] = 0xFFFF;
  }
  for (uint16_t i = 0; i < NUM_DI; i++) {
    mb.addCoil(CMD_DI_EN_BASE + i);
    mb.setCoil(CMD_DI_EN_BASE + i, false);
    mb.addCoil(CMD_DI_DIS_BASE + i);
    mb.setCoil(CMD_DI_DIS_BASE + i, false);
  }
  mb.addCoil(COIL_SAVE_PWM);
  mb.setCoil(COIL_SAVE_PWM, false);
  mb.addCoil(COIL_RELEASE_OVERRIDE);
  mb.setCoil(COIL_RELEASE_OVERRIDE, false);
  for (uint16_t i = 0; i < NUM_PWM; i++) {
    mb.addHreg(HR_PWM_BASE + i);
    mb.Hreg(HR_PWM_BASE + i, pwmLevel[i]);
  }
  mb.addHreg(HR_MB_ADDR);
  mb.Hreg(HR_MB_ADDR, g_mb_address);
  mb.addHreg(HR_MB_BAUD);
  mb.Hreg(HR_MB_BAUD, (g_mb_baud > 65535UL) ? (uint16_t)0 : (uint16_t)g_mb_baud);
  hmRegisterIdentity(mb, HM_MODEL_ID, HM_FW_MAJOR, HM_FW_MINOR, HM_FW_PATCH, HM_MAP_VERSION);
}

void sendWebStatus() {
  JSONVar st;
  st["model"] = HM_MODEL_ID;
  st["fw"]    = HM_FW;
  st["map"]   = HM_FW;
  st["addr"]  = g_mb_address;
  st["baud"]  = g_mb_baud;
  st["linkOk"] = linkOkNow(millis()) ? 1 : 0;
  st["busFailsafe"] = g_busFailsafeActive ? 1 : 0;
  st["localOverride"] = g_localOverride ? 1 : 0;
  WebSerial.send("status", st);
}

void sendWebCfg() {
  JSONVar cfg;
  for (int i = 0; i < NUM_DI; i++) {
    cfg["in"][i]["enabled"] = diCfg[i].enabled ? 1 : 0;
    cfg["in"][i]["invert"]  = diCfg[i].inverted ? 1 : 0;
    cfg["in"][i]["action"]  = diCfg[i].action;
    cfg["in"][i]["target"]  = diCfg[i].target;
    cfg["in"][i]["level"]   = (int)diCfg[i].level;
  }
  for (int i = 0; i < NUM_BTN; i++) cfg["btn"][i]["action"] = btnCfg[i].action;
  for (int i = 0; i < NUM_LED; i++) {
    cfg["led"][i]["mode"]   = ledCfg[i].mode;
    cfg["led"][i]["source"] = ledCfg[i].source;
  }
  cfg["bus"]["timeout"] = (int)g_busTimeoutS;
  cfg["override"]["timeout"] = (int)g_overrideTimeoutS;
  for (int i = 0; i < NUM_PWM; i++) {
    cfg["bus"]["failsafe"]["actions"][i] = (int)g_failsafeAction[i];
    cfg["bus"]["failsafe"]["levels"][i]  = (int)g_failsafeLevel[i];
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
  for (uint8_t i = 0; i < NUM_BTN; i++) pinMode(BTN_PINS[i], INPUT); // HIGH=pressed (CD4069)

  WebSerial.on("values", handleValues);
  WebSerial.on("Config", handleUnifiedConfig);
  WebSerial.on("command", handleCommand);
  for (uint8_t i = 0; i < 20; i++) {
    yield();
    delay(5);
  }

  setDefaults();
  if (!initFilesystemAndConfig()) wsLog("FATAL: Filesystem/config init failed");

  g_bootMs = millis();
  g_bootResetReason = detectBootResetReason();

  Serial2.setTX(TX2);
  Serial2.setRX(RX2);
  Serial2.begin(g_mb_baud);
  mb.config(g_mb_baud);
  setSlaveIdIfAvailable(mb, g_mb_address);
  mb.setAdditionalServerData("STR3221-32CH");

  buildModbusMap();
  g_lastLinkSeenMs = millis();
  g_linkFrameSeen = false;
  updateInputRegisters(g_lastLinkSeenMs);

  wsLog("Boot OK");
  sendWebBootstrap();
  hmWatchdogArm(4000);

  tlcNextRetryMs = millis() + 300;
}

// ================== Main loop ==================
void loop() {
  hmWatchdogFeed();
  WebSerial.check();
  yield();

  if (!tlcReady && (int32_t)(millis() - tlcNextRetryMs) >= 0) {
    tlcNextRetryMs = millis() + TLC_RETRY_MS;
    static uint8_t tlcRetryLog = 0;
    const bool fullScan = ((tlcRetryLog % 6) == 0);
    if (!tlcInitAll(fullScan)) {
      if (fullScan) {
        wsLog("WARNING: TLC59208F still offline (retry every 5s, full I2C scan every 30s)");
      }
      tlcRetryLog++;
    } else {
      tlcRetryLog = 0;
    }
  }

  const uint32_t now = millis();

  updateLinkOkDetector(now);
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
  if (pwmChanged && !outputsModbusLocked()) {
    applyPwmFromHoldingRegs();  // brightness not auto-persisted
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
    const bool pressed = BUTTON_PRESSED_LOW
      ? (digitalRead(BTN_PINS[i]) == LOW)
      : (digitalRead(BTN_PINS[i]) == HIGH);
    buttonPrev[i] = buttonState[i];
    buttonState[i] = pressed;
  }

  static int8_t prevIoIn[NUM_DI];
  static bool prevIoInit = false;
  if (!prevIoInit) {
    for (int i = 0; i < NUM_DI; i++) prevIoIn[i] = -1;
    prevIoInit = true;
  }

  for (int m = 0; m < NUM_DI; m++) {
    const bool val = diLiveState(m);
    const bool logical = diCfg[m].enabled ? val : false;
    diPrev[m] = diState[m];
    diState[m] = logical;
  }

  processBusFailsafe(now);
  processInputActions();
  processButtonActions();
  processOverrideTimeout(now);

  bool diUiChanged = false;
  for (int i = 0; i < NUM_DI; i++) {
    const int iv = diLiveState(i) ? 1 : 0;
    if (prevIoIn[i] != iv) {
      prevIoIn[i] = iv;
      diUiChanged = true;
    }
  }

  const bool identifying = (int32_t)(g_identifyUntilMs - now) > 0;
  static bool prevIdentifying = false;
  if (identifying && tlcReady && tlcChipOk[0]) {
    const uint8_t pulse = blinkPhase ? 200 : 0;
    for (uint8_t i = 0; i < 8; i++) {
      tlcWritePwm(0, i, pulse);
      tlcApplied[i] = pulse;
    }
  } else if (prevIdentifying && tlcReady && tlcChipOk[0]) {
    for (uint8_t i = 0; i < 8; i++) tlcApplied[i] = 0xFF;
    for (uint8_t i = 0; i < 8; i++) applyPwmChannel(i, pwmLevel[i]);
  }
  prevIdentifying = identifying;

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
  }

  updateInputRegisters(now);

  if (diUiChanged || (millis() - lastSend >= sendInterval)) {
    lastSend = millis();
    WebSerial.check();

    sendWebStatus();

    JSONVar inArr;
    for (int i = 0; i < NUM_DI; i++) inArr[i] = diLiveState(i) ? 1 : 0;
    JSONVar io;
    io["in"] = inArr;
    JSONVar btnArr;
    for (int i = 0; i < NUM_BTN; i++) btnArr[i] = buttonState[i] ? 1 : 0;
    io["btn"] = btnArr;
    JSONVar ledArr;
    for (int i = 0; i < NUM_LED; i++) ledArr[i] = ledStates[i];
    io["led"] = ledArr;
    WebSerial.send("io", io);

    if (hmUsbCanSend()) {
      JSONVar ext;
      for (int i = 0; i < NUM_PWM; i++) ext["pwm"][i] = (int)pwmLevel[i];
      ext["tlc_ready"] = tlcReady ? 1 : 0;
      for (int i = 0; i < 4; i++) ext["tlc_chip"][i] = tlcChipOk[i] ? 1 : 0;
      WebSerial.send("ext", ext);
    }
  }
}
