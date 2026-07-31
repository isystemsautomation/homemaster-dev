// AIO-422-R1 (RP2350) – Analog I/O module firmware
// Build / flash instructions: see README.md in this folder (../README.md)
// ------------------------------------------------------------
// Hardware:
//  - ADS1115 (4x AI) on Wire1 (SDA=6, SCL=7)
//  - 2x MCP4725 DAC (AO1=0x60, AO2=0x61) on Wire1
//  - 2x MAX31865 RTD on soft-SPI (CS=13/14, CLK=10, DO=12, DI=11)
//  - 4x Buttons on GPIO 22..25
//  - 4x LEDs on GPIO 18..21
//  - Modbus RTU on Serial2 (TX=4, RX=5)
//
// Key behaviors:
//  - Modbus: AI mV, RTD temp, DAC raw, button/LED discrete inputs
//  - WebSerial: Modbus address/baud, DAC, RTD config, LED/button mapping
//  - LED sources and button actions are Web-only and persisted (LittleFS)
//  - RTD configuration + diagnostics are Web-only
//  - Web traffic is throttled so Modbus stays responsive
// ------------------------------------------------------------

#include <Arduino.h>
// Modbus before Adafruit/SPI headers (they pull in std::byte and break Modbus.h).
#include <ModbusSerial.h>
#include "hm_common.h"
#define HM_MODEL_ID   4
#define HM_FW_MAJOR   0
#define HM_FW_MINOR   2
#define HM_FW_PATCH   0
#define HM_FW         "0.2.0"
#define HM_MAP        1
#define HM_MAP_VERSION 1

#include <Wire.h>
#include <ADS1X15.h>
#include <Adafruit_MCP4725.h>
#include <Adafruit_MAX31865.h>

#include <SimpleWebSerial.h>
#include <Arduino_JSON.h>
#include <LittleFS.h>
#include <math.h>
#include "hardware/watchdog.h"

// ================== Persistence (LittleFS) — must be before any function using it
// (Arduino IDE inserts function prototypes at the top of the generated .cpp). =====
struct PersistConfigV8 {
  uint32_t magic;
  uint16_t version;
  uint16_t size;

  uint16_t dacRaw[2];
  uint8_t  mb_address;
  uint32_t mb_baud;

  uint8_t  led_src[4];
  uint8_t  btn_action[4];

  uint8_t  rtd_wires[2];
  uint16_t rtd_rnominal[2];
  uint16_t rtd_rref[2];  // written but ignored — Rref derived from rnominal (sanitizeRtdCfg)

  uint32_t crc32;
} __attribute__((packed));

struct PersistConfig {
  uint32_t magic;
  uint16_t version;
  uint16_t size;

  uint8_t  aoPowerOn[2];
  uint8_t  mb_address;
  uint32_t mb_baud;

  uint8_t  led_src[4];
  uint8_t  btn_action[4];

  uint8_t  rtd_wires[2];
  uint16_t rtd_rnominal[2];
  uint16_t rtd_rref[2];  // written but ignored — Rref derived from rnominal (sanitizeRtdCfg)

  uint32_t crc32;
} __attribute__((packed));

struct OutputStateSnapshot {
  uint32_t magic; uint16_t version; uint16_t size;
  uint16_t dacRaw[2];
  uint32_t crc32;
} __attribute__((packed));

static const uint32_t CFG_MAGIC   = 0x314F4941UL;
static const uint16_t CFG_VERSION_V8 = 0x0008;
static const uint16_t CFG_VERSION = 0x0009;  // Phase B: aoPowerOn, DAC state decoupled
static const char*    CFG_PATH    = "/cfg.bin";
static const char*    OUT_STATE_PATH = "/cfg_out.bin";
static const uint32_t OUT_STATE_MAGIC = 0x484D4F53UL; // 'HMOS'
static const uint16_t OUT_STATE_VERSION = 0x0001;

volatile bool  cfgDirty        = false;
uint32_t       lastCfgTouchMs  = 0;
const uint32_t CFG_AUTOSAVE_MS = 1500;
uint32_t       lastOutChangeMs = 0;
uint32_t       lastOutSaveMs   = 0;
const uint32_t OUT_AUTOSAVE_MS = 10000;
uint16_t       prevDacRaw[2]   = {0, 0};
bool           outTrackInit    = false;

uint8_t aoPowerOn[2] = {HM_PWR_OFF, HM_PWR_OFF};

// ================== FW decls ==================
// Explicit prototypes above all call sites — do not rely on Arduino auto-prototype
// generation (breaks under PlatformIO / when a late manual decl suppresses the generator).
void handleValues(JSONVar values);
void handleCommand(JSONVar obj);
void handleUnifiedConfig(JSONVar obj);
void handleDac(JSONVar obj);      // whole Config object { t, list, powerOn? }
void handleLedCfg(JSONVar list);  // Config list array (bare ints or {src|source})
void handleBtnCfg(JSONVar list);  // Config list array (bare ints or {action})
void handleRtdCfg(JSONVar list);  // Config list object { wires, rnominal }

void performReset();
void sendWebStatus();
void sendWebCfg();
void sendWebBootstrap();
void sendWebExt();
void writeDac(int idx, uint16_t value);
void adsTick();
void readSensors();
static void rtdServiceChannel(uint8_t i);
void applyRtdHardwareCfg();
void rtdRecoveryTick();
void updateRtdDiagnostics();

// ================== UART2 (RS-485 / Modbus) ==================
#define TX2   4
#define RX2   5
const int TxenPin = -1;
int SlaveId = 1;
ModbusSerial mb(Serial2, SlaveId, TxenPin);

// ================== GPIO MAP ==================
static const uint8_t LED_PINS[4] = {18, 19, 20, 21};
static const uint8_t BTN_PINS[4] = {22, 23, 24, 25};

static const uint8_t NUM_LED = 4;
static const uint8_t NUM_BTN = 4;

// ================== I2C / SPI PINS ==================
#define SDA1 6
#define SCL1 7

#define RTD1_CS   13
#define RTD2_CS   14
#define RTD_DI    11
#define RTD_DO    12
#define RTD_CLK   10

// ================== Sensors / IO devices ==================
ADS1115 ads(0x48, &Wire1);

Adafruit_MCP4725 dac0;
Adafruit_MCP4725 dac1;

Adafruit_MAX31865 rtd1(RTD1_CS, RTD_DI, RTD_DO, RTD_CLK);
Adafruit_MAX31865 rtd2(RTD2_CS, RTD_DI, RTD_DO, RTD_CLK);

bool ads_ok     = false;
bool dac_ok[2]  = {false, false};
bool rtd_ok[2]  = {false, false};

// ================== ADC field scaling ==================
#define ADC_FIELD_SCALE_NUM 30303
#define ADC_FIELD_SCALE_DEN 10000
#define ADC_FIELD_SCALE ((float)ADC_FIELD_SCALE_NUM / (float)ADC_FIELD_SCALE_DEN)

// ================== AI channel map ==================
// Field terminal AI1..AI4 -> ADS1115 single-ended input index.
// AI3 and AI4 are crossed on the board; corrected here so that Modbus
// (HREG 140..143) and WebConfig (ext.ai[0..3]) both report the terminal
// the user actually wired to.
static const uint8_t AI_ADS_CH[4] = {0, 1, 3, 2};

// ================== Runtime state ==================
bool buttonState[NUM_BTN] = {false,false,false,false};
bool buttonPrev[NUM_BTN]  = {false,false,false,false};
bool ledState[NUM_LED]    = {false,false,false,false};
bool ledPhys[NUM_LED]     = {false,false,false,false};
uint32_t g_identifyUntilMs = 0;
const uint32_t IDENTIFY_MS = 5000;
unsigned long lastBlinkToggle = 0;
const unsigned long blinkPeriodMs = 400;
bool identifyBlinkPhase = false;

int16_t  aiRaw[4]   = {0,0,0,0};
uint16_t aiMv[4]    = {0,0,0,0};
int16_t  rtdTemp_x10[2] = {0,0};

// ===== ADS1115 async scan =====
static uint8_t  adsCh          = 0;  // logical field-terminal index 0..3 (AI1..AI4), not ADS input
static bool     adsPending     = false;
static uint32_t adsRequestMs   = 0;
static const uint32_t ADS_TIMEOUT_MS = 50;   // stuck-conversion guard

// ===== RTD scan stagger =====
static uint8_t rtdScanCh = 0;

// Absolute suspicion window (was 600 / -200 — that capped the usable PT100 range).
static const float RTD_SUSPECT_HI = 850.0f;
static const float RTD_SUSPECT_LO = -250.0f;

// A Pt100 reading far below the product's usable range almost always means
// the on-board PT100/PT1000 jumper does not match the sensor type selected
// in WebConfig (jumper on 4000 ohm while firmware assumes Rref = 400).
static const float RTD_JUMPER_SUSPECT_C = -150.0f;
static const uint32_t RTD_JUMPER_WARN_MS = 30000;

// ===== Button debounce =====
struct BtnDebounce {
  bool     raw;
  bool     stable;
  bool     prevStable;
  uint32_t lastChangeMs;
};
BtnDebounce btnDeb[NUM_BTN] = {};
static const uint16_t BTN_DEBOUNCE_MS = 30;
static void serviceBtnDebounce(BtnDebounce &st, bool raw, uint32_t now);

// ===== Bus link indicator (WebConfig "Bus" pill) =====
uint32_t       g_lastLinkSeenMs = 0;
const uint32_t LINK_TIMEOUT_MS  = 5000;

// ===== RTD fast recovery state (per-channel) =====
// Used to recover quickly after ESD / latched MAX31865 upset without
// repeatedly reinitializing the chip on every loop iteration.
uint32_t rtdLastRecoverMs[2]     = {0, 0};
uint8_t  rtdBadCount[2]          = {0, 0};
float     rtdLastGoodTempC[2]   = {0, 0};
int16_t   rtdLastGoodTempX10[2] = {0, 0};
bool      rtdHasGoodValue[2]    = {false, false};
uint32_t  rtdJumperWarnMs[2]    = {0, 0};

// ===== RTD async recovery (non-blocking, no delay()) =====
static const uint32_t RTD_REC_WAIT_MS    = 2;
static const uint32_t RTD_REC_SETTLE_MS  = 5;

enum : uint8_t {
  RTD_REC_IDLE = 0,
  RTD_REC_RUNNING,
  RTD_REC_DONE_OK,
  RTD_REC_DONE_FAIL
};

struct RtdRecoverFsm {
  uint8_t  state;
  uint8_t  triggerCh;
  uint8_t  chip;
  uint8_t  step;
  uint32_t stepMs;
  float    resultTempC;
} rtdRec = { RTD_REC_IDLE, 0, 0, 0, 0, 0.0f };

static inline bool rtdRecoveryIdle() {
  return rtdRec.state == RTD_REC_IDLE;
}

static inline bool rtdRecoveryBusy() {
  return rtdRec.state != RTD_REC_IDLE;
}

static void rtdRecoveryReset();
static void rtdRecoveryRequest(uint8_t ch);
static bool rtdRecoveryTakeResult(uint8_t ch, float &tempOut, bool &ok);

// ===== RTD diagnostics (Web-only) =====
uint8_t  rtdFault[2]     = {0, 0};
String   rtdError[2]     = {"", ""};
uint16_t rtdRawCode[2]   = {0, 0};     // raw RTD ADC code (15-bit)
float    rtdRatio[2]     = {0, 0};     // ratio rtd/rref (approx)
float    rtdOhms[2]      = {0, 0};     // computed RTD resistance
float    rtdTempC[2]     = {0, 0};     // computed temperature in °C

// ===== RTD configuration (Web-only, persisted) =====
uint8_t  rtdWiresCfg[2]    = {2, 2};
uint16_t rtdRnominalCfg[2] = {100, 100};
uint16_t rtdRrefCfg[2]     = {400, 400};

uint16_t dacRaw[2] = {0,0};

// ================== Web Serial ==================
SimpleWebSerial WebSerial;

static inline void wsLog(const char* msg) { WebSerial.send("log", msg); }
static inline void wsLog(const String& msg) { WebSerial.send("log", msg); }

// ================== Timing ==================
unsigned long lastSend       = 0;
// FIX A: slow down general WebSerial traffic
const unsigned long sendInterval   = 1000;

unsigned long lastSensorRead = 0;
const unsigned long sensorInterval = 200;

// FIX B: RTD full info only every 2 seconds

// ================== Persisted Modbus settings ==================
uint8_t  g_mb_address = 3;
uint32_t g_mb_baud    = 19200;

// ================== Modbus map ==================
enum : uint16_t {
  ISTS_BTN_BASE   = 1,
  ISTS_LED_BASE   = 20,

  HREG_TEMP_BASE   = 120,
  HREG_AI_MV_BASE  = 140,
  HREG_DAC_BASE    = 200,
};

// ================== LED source selection (Web-only, persisted) ==================
enum : uint8_t {
  LEDSRC_MANUAL    = 0,
  LEDSRC_AO1_AT0   = 1,
  LEDSRC_AO2_AT0   = 2,
  LEDSRC_AO1_AT100 = 3,
  LEDSRC_AO2_AT100 = 4,
};

uint8_t ledSrc[4] = { LEDSRC_MANUAL, LEDSRC_MANUAL, LEDSRC_MANUAL, LEDSRC_MANUAL };

static const uint16_t AO_ZERO_TH = 5;
static const uint16_t AO_FULL_TH = 4090;

// ================== Button actions (Web-only, persisted) ==================
// Values 3/4 used to mean "AO=max toggle" (0↔4095). They now mean
// "on/off, restore last level" (0↔aoLastLevel, default 4095). Behaviour is
// identical until a mid-scale level is stored, so stored configs stay valid
// without a CFG_VERSION bump or remap.
enum : uint8_t {
  BTNACT_LED_MANUAL_TOGGLE = 0,   // unchanged
  BTNACT_AO1_ONOFF         = 1,   // unchanged behaviour
  BTNACT_AO2_ONOFF         = 2,   // unchanged behaviour
  BTNACT_AO1_ONOFF_LEVEL   = 3,   // NEW meaning, compatible with old value 3
  BTNACT_AO2_ONOFF_LEVEL   = 4,   // NEW meaning, compatible with old value 4
  BTNACT_AO1_UP            = 5,
  BTNACT_AO1_DOWN          = 6,
  BTNACT_AO2_UP            = 7,
  BTNACT_AO2_DOWN          = 8,
  BTNACT_ALL_AO_OFF        = 9,
};

uint8_t btnAction[4] = { BTNACT_LED_MANUAL_TOGGLE, BTNACT_LED_MANUAL_TOGGLE,
                         BTNACT_LED_MANUAL_TOGGLE, BTNACT_LED_MANUAL_TOGGLE };

// Last non-zero AO level, used by the "on/off, restore last level" actions.
// RAM only — after reboot the power-on policy (aoPowerOn) governs the output.
uint16_t aoLastLevel[2] = {4095, 4095};

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

static inline uint8_t clamp_u8(int v, int lo, int hi) {
  if (v < lo) v = lo;
  if (v > hi) v = hi;
  return (uint8_t)v;
}

static inline uint16_t clamp_u16(int v, int lo, int hi) {
  if (v < lo) v = lo;
  if (v > hi) v = hi;
  return (uint16_t)v;
}

static String decodeMax31865Fault(uint8_t f) {
  if (f == 0) return "";
  if (f == 0xFF) return "RTD module not detected";

  String s = "";
  if (f & 0x80) s += "RTD High Threshold; ";
  if (f & 0x40) s += "RTD Low Threshold; ";
  if (f & 0x20) s += "REFIN- > 0.85×Bias (open); ";
  if (f & 0x10) s += "REFIN- < 0.85×Bias; ";
  if (f & 0x08) s += "RTDIN- < 0.85×Bias (short); ";
  if (f & 0x04) s += "Over/Under voltage; ";
  if (s.length() >= 2) s.remove(s.length() - 2);
  return s;
}

static max31865_numwires_t wiresToEnum(uint8_t w) {
  if (w == 3) return MAX31865_3WIRE;
  if (w == 4) return MAX31865_4WIRE;
  return MAX31865_2WIRE;
}

// Rref is a hardware property of the board, tied to the sensor type.
// PT100 -> 400 ohm, PT1000 -> 4000 ohm. Never user-selectable.
static inline uint16_t rtdRrefForNominal(uint16_t rnom) {
  return (rnom == 1000) ? 4000 : 400;
}

static void sanitizeRtdCfg() {
  for (int i=0;i<2;i++) {
    uint8_t w = rtdWiresCfg[i];
    if (w != 2 && w != 3 && w != 4) w = 2;
    rtdWiresCfg[i] = w;

    uint16_t rn = rtdRnominalCfg[i];
    if (rn != 100 && rn != 1000) rn = 100;
    rtdRnominalCfg[i] = rn;

    // Derived, not configured. Overrides whatever was stored or received.
    rtdRrefCfg[i] = rtdRrefForNominal(rn);
  }
}

// ===== RTD async recovery implementation (after RTD cfg + helpers) =====
static void rtdRecoveryReset() {
  rtdRec.state       = RTD_REC_IDLE;
  rtdRec.triggerCh   = 0;
  rtdRec.chip        = 0;
  rtdRec.step        = 0;
  rtdRec.stepMs      = 0;
  rtdRec.resultTempC = 0.0f;
}

static void rtdRecoveryFinishFail() {
  rtdRec.state = RTD_REC_DONE_FAIL;
  rtdRec.step  = 0;
}

static void rtdRecoveryRequest(uint8_t ch) {
  if (!rtdRecoveryIdle()) return;
  rtdRec.state       = RTD_REC_RUNNING;
  rtdRec.triggerCh   = ch;
  rtdRec.chip        = 0;
  rtdRec.step        = 0;
  rtdRec.stepMs      = millis();
  rtdRec.resultTempC = 0.0f;
}

static bool rtdRecoveryTakeResult(uint8_t ch, float &tempOut, bool &ok) {
  if (rtdRec.state != RTD_REC_DONE_OK && rtdRec.state != RTD_REC_DONE_FAIL) return false;
  if (rtdRec.triggerCh != ch) return false;

  ok = (rtdRec.state == RTD_REC_DONE_OK);
  tempOut = rtdRec.resultTempC;
  rtdRecoveryReset();
  return true;
}

static void rtdRecoveryVerifyTrigger() {
  uint8_t i = rtdRec.triggerCh;
  Adafruit_MAX31865* rtds[2] = { &rtd1, &rtd2 };
  if (i > 1 || !rtd_ok[i]) {
    rtdRecoveryFinishFail();
    return;
  }

  float rnom = (float)rtdRnominalCfg[i];
  float rref = (float)rtdRrefCfg[i];
  uint8_t f2 = rtds[i]->readFault();
  uint16_t raw2 = rtds[i]->readRTD();
  rtdRawCode[i] = raw2;
  float temp2 = rtds[i]->calculateTemperature(raw2, rnom, rref);

  rtdFault[i] = f2;
  rtdError[i] = decodeMax31865Fault(f2);

  bool tempFinite2 = (!isnan(temp2) && !isinf(temp2));
  bool tempInRange2 = (temp2 >= -250.0f && temp2 <= 850.0f);
  bool retryValid = (f2 == 0) && tempFinite2 && tempInRange2;

  if (retryValid) {
    rtdRec.resultTempC = temp2;
    rtdRec.state = RTD_REC_DONE_OK;
  } else {
    rtdRecoveryFinishFail();
  }
}

static void rtdRecoveryAdvanceChipStep(Adafruit_MAX31865 &dev, uint8_t chipIdx) {
  uint32_t now = millis();

  switch (rtdRec.step) {
    case 0:
      dev.clearFault();
      rtdRec.step = 1;
      rtdRec.stepMs = now;
      break;

    case 1:
      if (now - rtdRec.stepMs < RTD_REC_WAIT_MS) return;
      rtdRec.step = 2;
      break;

    case 2: {
      bool ok = dev.begin(wiresToEnum(rtdWiresCfg[chipIdx]));
      rtd_ok[chipIdx] = ok;
      if (!ok) {
        rtdRecoveryFinishFail();
        return;
      }
      rtdRec.step = 3;
      rtdRec.stepMs = now;
      break;
    }

    case 3:
      dev.clearFault();
      rtdRec.step = 4;
      rtdRec.stepMs = now;
      break;

    case 4:
      if (now - rtdRec.stepMs < RTD_REC_WAIT_MS) return;
      rtdRec.step = 5;
      break;

    case 5:
      // No settle readRTD() — next readSensors() scan performs a real conversion.
      rtdRec.step = 6;
      rtdRec.stepMs = now;
      break;

    case 6:
      if (now - rtdRec.stepMs < RTD_REC_SETTLE_MS) return;
      if (chipIdx == 0) {
        rtdRec.chip = 1;
        rtdRec.step = 0;
        rtdRec.stepMs = now;
      } else {
        rtdRecoveryVerifyTrigger();
      }
      break;

    default:
      rtdRecoveryFinishFail();
      break;
  }
}

void rtdRecoveryTick() {
  if (!rtdRecoveryBusy()) return;

  Adafruit_MAX31865* rtds[2] = { &rtd1, &rtd2 };
  rtdRecoveryAdvanceChipStep(*rtds[rtdRec.chip], rtdRec.chip);
  mb.task();
}

bool getLedAutoState(uint8_t src) {
  if (src == LEDSRC_AO1_AT0)   return (dacRaw[0] <= AO_ZERO_TH);
  if (src == LEDSRC_AO2_AT0)   return (dacRaw[1] <= AO_ZERO_TH);
  if (src == LEDSRC_AO1_AT100) return (dacRaw[0] >= AO_FULL_TH);
  if (src == LEDSRC_AO2_AT100) return (dacRaw[1] >= AO_FULL_TH);
  return false;
}

// ================== Defaults / persist ==================
void setDefaults() {
  for (int i=0;i<NUM_LED;i++) { ledState[i] = false; }
  for (int i=0;i<NUM_BTN;i++) { buttonState[i] = buttonPrev[i] = false; }

  dacRaw[0] = 0;
  dacRaw[1] = 0;
  aoPowerOn[0] = HM_PWR_OFF;
  aoPowerOn[1] = HM_PWR_OFF;

  g_mb_address = 3;
  g_mb_baud    = 19200;

  for (int i=0;i<4;i++) {
    ledSrc[i] = LEDSRC_MANUAL;
    btnAction[i] = BTNACT_LED_MANUAL_TOGGLE;
  }

  rtdWiresCfg[0]    = 2;
  rtdWiresCfg[1]    = 2;
  rtdRnominalCfg[0] = 100;
  rtdRnominalCfg[1] = 100;
  rtdRrefCfg[0]     = 400;   // derived from PT100
  rtdRrefCfg[1]     = 400;
  sanitizeRtdCfg();

  rtdFault[0] = rtdFault[1] = 0;
  rtdError[0] = rtdError[1] = "";
  rtdRawCode[0] = rtdRawCode[1] = 0;
  rtdRatio[0] = rtdRatio[1] = 0;
  rtdOhms[0] = rtdOhms[1] = 0;
  rtdTempC[0] = rtdTempC[1] = 0;
}

bool readOutputStateSnapshot(uint16_t out[2]) {
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
  out[0] = snap.dacRaw[0];
  out[1] = snap.dacRaw[1];
  return true;
}

bool saveOutputStateSnapshot() {
  OutputStateSnapshot snap{};
  snap.magic = OUT_STATE_MAGIC; snap.version = OUT_STATE_VERSION; snap.size = sizeof(OutputStateSnapshot);
  snap.dacRaw[0] = dacRaw[0];
  snap.dacRaw[1] = dacRaw[1];
  snap.crc32 = 0; snap.crc32 = crc32_update(0, (const uint8_t*)&snap, sizeof(snap));
  File f = LittleFS.open(OUT_STATE_PATH, "w");
  if (!f) return false;
  size_t n = f.write((const uint8_t*)&snap, sizeof(snap));
  f.flush(); f.close();
  return n == sizeof(snap);
}

void applyPowerOnOutputs() {
  uint16_t restored[2] = {0, 0};
  bool haveSnap = readOutputStateSnapshot(restored);
  for (int i = 0; i < 2; i++) {
    if (aoPowerOn[i] == HM_PWR_ON) dacRaw[i] = 4095;
    else if (aoPowerOn[i] == HM_PWR_RESTORE && haveSnap) dacRaw[i] = restored[i];
    else dacRaw[i] = 0;
    writeDac(i, dacRaw[i]);
    mb.Hreg(HREG_DAC_BASE + i, dacRaw[i]);
  }
  memcpy(prevDacRaw, dacRaw, sizeof(prevDacRaw));
  outTrackInit = true;
  lastOutChangeMs = millis();
}

void maybePersistOutputState(uint32_t now) {
  bool needRestore = false;
  for (int i = 0; i < 2; i++) {
    if (aoPowerOn[i] == HM_PWR_RESTORE) { needRestore = true; break; }
  }
  if (!needRestore) return;
  if ((uint32_t)(now - lastOutChangeMs) < OUT_AUTOSAVE_MS) return;
  if (lastOutSaveMs && (uint32_t)(now - lastOutSaveMs) < OUT_AUTOSAVE_MS) return;
  if (saveOutputStateSnapshot()) lastOutSaveMs = now;
}

void captureToPersist(PersistConfig &pc) {
  pc.magic   = CFG_MAGIC;
  pc.version = CFG_VERSION;
  pc.size    = sizeof(PersistConfig);

  pc.aoPowerOn[0] = aoPowerOn[0];
  pc.aoPowerOn[1] = aoPowerOn[1];

  pc.mb_address = g_mb_address;
  pc.mb_baud    = g_mb_baud;

  for (int i=0;i<4;i++) {
    pc.led_src[i] = ledSrc[i];
    pc.btn_action[i] = btnAction[i];
  }

  pc.rtd_wires[0]    = rtdWiresCfg[0];
  pc.rtd_wires[1]    = rtdWiresCfg[1];
  pc.rtd_rnominal[0] = rtdRnominalCfg[0];
  pc.rtd_rnominal[1] = rtdRnominalCfg[1];
  pc.rtd_rref[0]     = rtdRrefCfg[0];
  pc.rtd_rref[1]     = rtdRrefCfg[1];

  pc.crc32 = 0;
  pc.crc32 = crc32_update(0, (const uint8_t*)&pc, sizeof(PersistConfig));
}

bool applyFromPersistV8(const PersistConfigV8 &pc) {
  if (pc.magic != CFG_MAGIC || pc.size != sizeof(PersistConfigV8)) return false;

  PersistConfigV8 tmp = pc;
  uint32_t crc = tmp.crc32; tmp.crc32 = 0;
  if (crc32_update(0, (const uint8_t*)&tmp, sizeof(PersistConfigV8)) != crc) return false;
  if (pc.version != CFG_VERSION_V8) return false;

  aoPowerOn[0] = HM_PWR_OFF;
  aoPowerOn[1] = HM_PWR_OFF;
  g_mb_address = pc.mb_address;
  g_mb_baud    = pc.mb_baud;

  for (int i=0;i<4;i++) {
    ledSrc[i] = clamp_u8((int)pc.led_src[i], 0, 4);
    btnAction[i] = clamp_u8((int)pc.btn_action[i], 0, 9);
  }

  rtdWiresCfg[0]    = pc.rtd_wires[0];
  rtdWiresCfg[1]    = pc.rtd_wires[1];
  rtdRnominalCfg[0] = pc.rtd_rnominal[0];
  rtdRnominalCfg[1] = pc.rtd_rnominal[1];
  rtdRrefCfg[0]     = pc.rtd_rref[0];
  rtdRrefCfg[1]     = pc.rtd_rref[1];
  sanitizeRtdCfg();

  return true;
}

bool applyFromPersist(const PersistConfig &pc) {
  if (pc.magic != CFG_MAGIC || pc.size != sizeof(PersistConfig)) return false;

  PersistConfig tmp = pc;
  uint32_t crc = tmp.crc32; tmp.crc32 = 0;
  if (crc32_update(0, (const uint8_t*)&tmp, sizeof(PersistConfig)) != crc) return false;
  if (pc.version != CFG_VERSION) return false;

  aoPowerOn[0] = clamp_u8((int)pc.aoPowerOn[0], 0, 2);
  aoPowerOn[1] = clamp_u8((int)pc.aoPowerOn[1], 0, 2);
  g_mb_address = pc.mb_address;
  g_mb_baud    = pc.mb_baud;

  for (int i=0;i<4;i++) {
    ledSrc[i] = clamp_u8((int)pc.led_src[i], 0, 4);
    btnAction[i] = clamp_u8((int)pc.btn_action[i], 0, 9);
  }

  rtdWiresCfg[0]    = pc.rtd_wires[0];
  rtdWiresCfg[1]    = pc.rtd_wires[1];
  rtdRnominalCfg[0] = pc.rtd_rnominal[0];
  rtdRnominalCfg[1] = pc.rtd_rnominal[1];
  rtdRrefCfg[0]     = pc.rtd_rref[0];
  rtdRrefCfg[1]     = pc.rtd_rref[1];
  sanitizeRtdCfg();

  return true;
}

bool saveConfigFS() {
  PersistConfig pc{};
  captureToPersist(pc);

  File f = LittleFS.open(CFG_PATH, "w");
  if (!f) { wsLog( "save: open failed"); return false; }
  size_t n = f.write((const uint8_t*)&pc, sizeof(pc));
  f.flush();
  f.close();
  if (n != sizeof(pc)) {
    wsLog( String("save: short write ")+n);
    return false;
  }

  File r = LittleFS.open(CFG_PATH, "r");
  if (!r) { wsLog( "save: reopen failed"); return false; }
  if ((size_t)r.size() != sizeof(PersistConfig)) {
    wsLog( "save: size mismatch after write");
    r.close();
    return false;
  }
  PersistConfig back{};
  size_t nr = r.read((uint8_t*)&back, sizeof(back));
  r.close();
  if (nr != sizeof(back)) {
    wsLog( "save: short readback");
    return false;
  }
  PersistConfig tmp = back;
  uint32_t crc = tmp.crc32;
  tmp.crc32 = 0;
  if (crc32_update(0, (const uint8_t*)&tmp, sizeof(tmp)) != crc) {
    wsLog( "save: CRC verify failed");
    return false;
  }
  return true;
}

bool loadConfigFS() {
  File f = LittleFS.open(CFG_PATH, "r");
  if (!f) { wsLog( "load: open failed"); return false; }

  size_t sz = (size_t)f.size();

  if (sz == sizeof(PersistConfigV8)) {
    PersistConfigV8 pc{};
    size_t n = f.read((uint8_t*)&pc, sizeof(pc));
    f.close();
    if (n != sizeof(pc)) { wsLog( "load: short read (v8)"); return false; }
    if (!applyFromPersistV8(pc)) { wsLog( "load: v8 magic/version/crc mismatch"); return false; }
    wsLog( "Loaded legacy config v8 → migrated to v9 (aoPowerOn defaults OFF).");
    cfgDirty = true; lastCfgTouchMs = millis();
    return true;
  }
  if (sz != sizeof(PersistConfig)) {
    wsLog( String("load: size ")+sz+" unsupported");
    f.close();
    return false;
  }

  PersistConfig pc{};
  size_t n = f.read((uint8_t*)&pc, sizeof(pc));
  f.close();
  if (n != sizeof(pc)) { wsLog( "load: short read"); return false; }
  if (!applyFromPersist(pc)) { wsLog( "load: magic/version/crc mismatch"); return false; }
  return true;
}

bool initFilesystemAndConfig() {
  if (!LittleFS.begin()) {
    wsLog( "LittleFS mount failed. Formatting…");
    if (!LittleFS.format() || !LittleFS.begin()) {
      wsLog( "FATAL: FS mount/format failed");
      return false;
    }
  }

  if (loadConfigFS()) {
    wsLog( "Config loaded from flash");
    return true;
  }

  wsLog( "No valid config. Using defaults.");
  setDefaults();
  if (saveConfigFS()) {
    wsLog( "Defaults saved");
    return true;
  }

  wsLog( "FATAL: first save failed");
  return false;
}

static inline void setSlaveIdIfAvailable(ModbusSerial& m, uint8_t id) {
  m.setSlaveId(id);
}

void applyModbusSettings(uint8_t addr, uint32_t baud) {
  if (g_mb_baud != baud) {
    Serial2.end();
    Serial2.begin(baud);
    mb.config(baud);
  }
  setSlaveIdIfAvailable(mb, addr);
  g_mb_address = addr;
  g_mb_baud    = baud;
}

// ================== Command handler / reset ==================
void handleCommand(JSONVar obj) {
  const char* actC = (const char*)obj["action"];
  if (!actC) { wsLog( "command: missing 'action'"); return; }
  String act = String(actC);
  act.toLowerCase();

  if (act == "reset" || act == "reboot") {
    bool ok = saveConfigFS();
    wsLog( ok ? "Saved. Rebooting…" : "WARNING: Save verify FAILED. Rebooting anyway…");
    delay(400);
    performReset();
  } else if (act == "save") {
    if (saveConfigFS()) wsLog( "Configuration saved");
    else               wsLog( "ERROR: Save failed");
  } else if (act == "load") {
    if (loadConfigFS()) {
      applyPowerOnOutputs();
      wsLog( "Configuration loaded");
      applyModbusSettings(g_mb_address, g_mb_baud);
      applyRtdHardwareCfg();
      sendWebBootstrap();
    } else {
      wsLog( "ERROR: Load failed/invalid");
    }
  } else if (act == "factory") {
    LittleFS.remove(OUT_STATE_PATH);
    setDefaults();
    applyPowerOnOutputs();
    if (saveConfigFS()) {
      wsLog( "Factory defaults restored & saved");
      applyModbusSettings(g_mb_address, g_mb_baud);
      applyRtdHardwareCfg();
      sendWebBootstrap();
    } else {
      wsLog( "ERROR: Save after factory reset failed");
    }
  } else if (act == "hello" || act == "getconfig") {
    sendWebBootstrap();
  } else if (act == "identify") {
    g_identifyUntilMs = millis() + IDENTIFY_MS;
    wsLog("Identify: LEDs active for 5 s");
  } else {
    wsLog( String("Unknown command: ") + actC);
  }
}

void performReset() {
  if (Serial) Serial.flush();
  delay(50);
  watchdog_reboot(0, 0, 0);
  while (true) { __asm__("wfi"); }
}

// ================== WebSerial handlers ==================
void handleValues(JSONVar values) {
  int addr = (int)values["mb_address"];
  int baud = (int)values["mb_baud"];
  addr = hmValidAddress(addr);
  baud = hmValidBaud(baud);

  applyModbusSettings((uint8_t)addr, (uint32_t)baud);
  wsLog("Modbus configuration updated");
  sendWebStatus();
  cfgDirty = true;
  lastCfgTouchMs = millis();
}

void handleDac(JSONVar obj) {
  JSONVar list = obj["list"];
  bool changed = false;

  if (JSON.typeof(list) == "array") {
    for (int i = 0; i < 2 && i < (int)list.length(); i++) {
      long v = (long)list[i];
      v = constrain(v, 0L, 4095L);
      dacRaw[i] = (uint16_t)v;
      writeDac(i, dacRaw[i]);
      mb.Hreg(HREG_DAC_BASE + i, dacRaw[i]);
    }
    wsLog("DAC values updated");
  }

  JSONVar powerOn = obj["powerOn"];
  if (JSON.typeof(powerOn) == "array") {
    for (int i = 0; i < 2 && i < (int)powerOn.length(); i++) {
      aoPowerOn[i] = clamp_u8((int)powerOn[i], 0, 2);
    }
    wsLog("AO power-on policy updated");
    changed = true;
  }

  if (changed) {
    lastCfgTouchMs = millis();
    cfgDirty = true;              // TODO (intake rule 4)
    sendWebCfg();                 // TODO (intake rule 4)
  }
}

// ===== RTD configuration handler (Web-only, persisted) =====
// Receives the raw 'list' object from the Config router (intake rule 2).
void handleRtdCfg(JSONVar list) {
  JSONVar wires = list["wires"];
  JSONVar rn    = list["rnominal"];

  if (JSON.typeof(wires) == "array") {
    for (int i=0;i<2;i++) {
      if (i >= (int)wires.length()) break;
      int v = (int)wires[i];
      if (v != 2 && v != 3 && v != 4) v = 2;
      rtdWiresCfg[i] = (uint8_t)v;
    }
  }
  if (JSON.typeof(rn) == "array") {
    for (int i=0;i<2;i++) {
      if (i >= (int)rn.length()) break;
      int v = (int)rn[i];
      if (v != 100 && v != 1000) v = 100;
      rtdRnominalCfg[i] = (uint16_t)v;
    }
  }

  sanitizeRtdCfg();
  applyRtdHardwareCfg();
  wsLog("RTD configuration updated (Web-only)");

  cfgDirty = true;                // TODO (intake rule 4)
  lastCfgTouchMs = millis();
  sendWebCfg();                   // TODO (intake rule 4)
}

// Receives the raw 'list' array from the Config router (intake rule 2).
// Each element may be a bare number or an object: {src: N} or {source: N}.
void handleLedCfg(JSONVar list) {
  if (JSON.typeof(list) != "array") {
    wsLog(String("led: expected array, got ") + JSON.typeof(list));
    return;
  }
  for (int i = 0; i < NUM_LED && i < (int)list.length(); i++) {
    JSONVar e = list[i];
    int v;
    if      (e.hasOwnProperty("src"))    v = (int)e["src"];
    else if (e.hasOwnProperty("source")) v = (int)e["source"];
    else                                 v = (int)e;
    ledSrc[i] = clamp_u8(v, 0, 4);
  }
  wsLog("LED source configuration updated");
  cfgDirty = true;                // TODO (intake rule 4)
  lastCfgTouchMs = millis();
  sendWebCfg();                   // TODO (intake rule 4)
}

// Receives the raw 'list' array from the Config router (intake rule 2).
// Each element may be a bare number or an object of the form {action: N}.
void handleBtnCfg(JSONVar list) {
  if (JSON.typeof(list) != "array") {
    wsLog(String("btn: expected array, got ") + JSON.typeof(list));
    return;
  }
  for (int i = 0; i < NUM_BTN && i < (int)list.length(); i++) {
    JSONVar e = list[i];
    int v = e.hasOwnProperty("action") ? (int)e["action"] : (int)e;
    btnAction[i] = clamp_u8(v, 0, 9);
  }
  wsLog("Button actions updated");
  cfgDirty = true;                // TODO (intake rule 4)
  lastCfgTouchMs = millis();
  sendWebCfg();                   // TODO (intake rule 4)
}

// ================== Config intake rules (HomeMaster line-wide) ==================
// 1. Config arrives on exactly one channel: "Config", shaped { t, list }.
//    Only "values", "Config" and "command" may be registered with WebSerial.on().
//    A module may add a channel ONLY for a payload that genuinely cannot be
//    expressed as { t, list } (see WLD "onewire", ENM "AlarmsCfg"), and must
//    document why. SimpleWebSerial caps the total at 8 events.
// 2. The router NEVER constructs an intermediate JSONVar. No payload[key][i] = ...
//    Pass "list" (or the whole "obj" when top-level siblings are needed) through
//    unchanged. Building a JSONVar by integer index yields an object keyed "0",
//    "1", "2"..., not an array, and downstream JSON.typeof() checks then fail.
// 3. Tolerate element shape where the value is read, not in an adapter layer:
//    an element may be a bare number or an object ({action:N}, {source:N}).
// 4. One exit point. Set a single "changed" flag, and at the end of the router
//    do the persistence and echo once. Leaf helpers must not each call
//    sendWebCfg() / touch cfgDirty on their own.
// 5. Never return silently. A malformed or unrecognised payload must wsLog what
//    was actually received, including JSON.typeof() of the offending value.
// ==============================================================================
// Config router: contract t + legacy inbound type aliases (dac, rtdCfg, btnCfg, ledCfg).
void handleUnifiedConfig(JSONVar obj) {
  const char* t = (const char*)obj["t"];
  JSONVar list = obj["list"];
  if (!t) { wsLog("Config: missing 't'"); return; }

  String type = String(t);

  if (type == "ext.dac" || type == "dac") {
    handleDac(obj);
    return;

  } else if (type == "ext.rtd" || type == "rtdCfg") {
    handleRtdCfg(list);
    return;

  } else if (type == "btn" || type == "btnCfg" || type == "buttons") {
    handleBtnCfg(list);
    return;

  } else if (type == "led" || type == "ledCfg" || type == "leds") {
    handleLedCfg(list);
    return;

  } else {
    wsLog(String("Unknown Config type: ") + t + " (list is " + JSON.typeof(list) + ")");
  }
}

// ================== DAC write helper ==================
void writeDac(int idx, uint16_t value) {
  if (idx == 0 && dac_ok[0]) dac0.setVoltage(value, false);
  else if (idx == 1 && dac_ok[1]) dac1.setVoltage(value, false);
}

// ================== Apply RTD hardware config (wire mode) ==================
void applyRtdHardwareCfg() {
  rtdRecoveryReset();
  Adafruit_MAX31865* rtds[2] = { &rtd1, &rtd2 };
  for (int i=0;i<2;i++) {
    bool ok = rtds[i]->begin(wiresToEnum(rtdWiresCfg[i]));
    rtd_ok[i] = ok;
    if (ok) {
      rtds[i]->clearFault();
      wsLog( String("MAX31865 RTD") + (i+1) + " configured: " +
        String(rtdWiresCfg[i]) + "wire, " + String(rtdRnominalCfg[i]) + "ohm, Rref " + String(rtdRrefCfg[i]) + "ohm");
    } else {
      wsLog( String("ERROR: MAX31865 RTD") + (i+1) + " init failed");
    }
  }
}

void updateRtdDiagnostics() {
  // No SPI I/O — consume values cached by rtdServiceChannel() / recovery.
  for (int i=0;i<2;i++) {
    if (!rtd_ok[i]) {
      rtdFault[i]    = 0xFF;
      rtdError[i]    = decodeMax31865Fault(rtdFault[i]);
      rtdRawCode[i]  = 0;
      rtdRatio[i]    = 0;
      rtdOhms[i]     = 0;
      continue;
    }

    float rref = (float)rtdRrefCfg[i];
    float ratio = (rtdRawCode[i] / 32768.0f);
    rtdRatio[i] = ratio;
    rtdOhms[i]  = ratio * rref;
    rtdError[i] = decodeMax31865Fault(rtdFault[i]);
  }
}

// ================== ADS1115 async scan ==================
// Non-blocking: at most one I2C register access per call.
void adsTick() {
  if (!ads_ok) return;
  uint32_t now = millis();
  if (!adsPending) {
    ads.requestADC(AI_ADS_CH[adsCh]);
    adsPending   = true;
    adsRequestMs = now;
    return;
  }
  if (!ads.isReady()) {
    if ((uint32_t)(now - adsRequestMs) >= ADS_TIMEOUT_MS) {
      adsPending = false;          // retry the same channel
    }
    return;
  }
  int16_t raw = ads.getValue();
  aiRaw[adsCh] = raw;
  float v_field = ads.toVoltage(raw) * ADC_FIELD_SCALE;
  long  mv      = lroundf(v_field * 1000.0f);
  if (mv < 0)     mv = 0;
  if (mv > 65535) mv = 65535;
  aiMv[adsCh] = (uint16_t)mv;
  mb.Hreg(HREG_AI_MV_BASE + adsCh, aiMv[adsCh]);
  adsPending = false;
  adsCh = (uint8_t)((adsCh + 1) & 0x03);
}

// ================== Sensor read helper ==================
static void rtdServiceChannel(uint8_t i) {
  const uint32_t RTD_RECOVER_COOLDOWN_MS = 500;
  const uint8_t  RTD_ZERO_AFTER_BAD_COUNT = 3;
  Adafruit_MAX31865* rtds[2] = { &rtd1, &rtd2 };

  float rnom = (float)rtdRnominalCfg[i];
  float rref = (float)rtdRrefCfg[i];

  // ---- Fast, stable RTD recovery with anti-flapping ----
  // Do not trust temperature when faulted or out-of-range.
  // Recovery is rate-limited (cooldown) so we don't reinitialize
  // the MAX31865 on every loop and cause output flapping.

  Adafruit_MAX31865& dev = *rtds[i];
  uint32_t nowMs = millis();

  // Read fault first, then RTD + pure-math temperature (single conversion).
  // MAX31865 can sometimes latch an invalid internal state after ESD where
  // fault bits are not set but the computed temperature/resistance jumps.
  uint8_t f = dev.readFault();
  rtdFault[i] = f;
  rtdError[i] = decodeMax31865Fault(f);

  uint16_t rawCode = dev.readRTD();
  rtdRawCode[i] = rawCode;
  float ratio = (rawCode / 32768.0f);
  float ohmsNow = ratio * rref;

  float temp = dev.calculateTemperature(rawCode, rnom, rref);
  bool tempFinite =
    (!isnan(temp) && !isinf(temp));
  bool tempInRange =
    (temp >= -250.0f && temp <= 850.0f);
  bool readingValid = (f == 0) && tempFinite && tempInRange;

  // Jump detection: if the reading differs too much from the last known-good
  // value, treat it as invalid and force a quick MAX31865 re-init.
  bool jumpDetected = false;

  // Extra guard: absolute suspicion window. Previously 600 / -200 which
  // silently capped the usable PT100 range below the documented 850 C.
  // Keep the diagnostic path; jump detection (relative to last-good) catches ESD.
  if (readingValid && (temp > RTD_SUSPECT_HI || temp < RTD_SUSPECT_LO)) {
    jumpDetected = true;
    readingValid = false;
    WebSerial.send(
      "message",
      String("RTD suspicious value -> forcing reinit: ch=") + String(i+1) +
      " temp=" + String(temp, 2) + "C" +
      " fault=0x" + String(f, HEX) +
      " raw=" + String((int)rawCode)
    );
  }
  if (rtdHasGoodValue[i]) {
    float oldTempC = rtdLastGoodTempC[i];
    float deltaT = fabs(temp - oldTempC);

    // Convert last-good temperature back to resistance (Pt100/Pt1000 model)
    // so we can compare resistance jumps even when fault bits are not set.
    // Callendar–Van Dusen coefficients for platinum RTDs.
    const float A = 3.9083e-3f;
    const float B = -5.775e-7f;
    const float C = -4.183e-12f;
    float tt = oldTempC;
    float ohmsOld =
      (tt >= 0.0f)
        ? (rnom * (1.0f + A*tt + B*tt*tt))
        : (rnom * (1.0f + A*tt + B*tt*tt + C*(tt - 100.0f)*tt*tt*tt));

    float deltaR = fabs(ohmsNow - ohmsOld);
    const float deltaRLimit = rnom * 0.5f;   // ~130 C equivalent on both Pt100 and Pt1000
    if (deltaT > 100.0f || deltaR > deltaRLimit) {
      jumpDetected = true;
      readingValid = false; // do not overwrite last-good with a bad reading
      WebSerial.send(
        "message",
        String("RTD jump detected -> forcing reinit: ch=") + String(i+1) +
        " oldT=" + String(oldTempC, 2) + "C newT=" + String(temp, 2) + "C" +
        " raw=" + String((int)rawCode)
      );
    }
  }

  if (readingValid) {
    // Advisory only: PT100 + implausibly cold often means jumper on 4000 Ω.
    // Do not invalidate, recover, or zero — cryogenic use must still work.
    const bool jumperSuspect =
      (rtdRnominalCfg[i] == 100) && (temp < RTD_JUMPER_SUSPECT_C);
    if (jumperSuspect) {
      // 0 = cleared / never warned → emit immediately; else throttle 30 s.
      if (rtdJumperWarnMs[i] == 0 ||
          (uint32_t)(nowMs - rtdJumperWarnMs[i]) >= RTD_JUMPER_WARN_MS) {
        rtdJumperWarnMs[i] = nowMs ? nowMs : 1;
        WebSerial.send(
          "message",
          String("RTD") + String(i+1) +
          ": reading " + String(temp, 1) + "C is implausible for PT100 - check the "
          "on-board PT100/PT1000 jumper against the sensor type selected here."
        );
      }
    } else {
      rtdJumperWarnMs[i] = 0;  // reset so a recurrence is reported promptly
    }

    // Valid reading: store as last-good and publish immediately
    rtdBadCount[i] = 0;
    rtdHasGoodValue[i] = true;
    rtdLastGoodTempC[i] = temp;
    rtdLastGoodTempX10[i] = (int16_t)lroundf(temp * 10.0f);

    rtdTempC[i] = temp;
    rtdTemp_x10[i] = rtdLastGoodTempX10[i];
    mb.Hreg(HREG_TEMP_BASE + i, (uint16_t)rtdTemp_x10[i]);
    return;
  }

  // Invalid reading: increment bad counter (anti-flapping), saturate uint8_t
  if (rtdBadCount[i] < 255) rtdBadCount[i]++;

  // Start async recovery if cooldown allows (non-blocking; see rtdRecoveryTick()).
  if (rtdRecoveryIdle() &&
      (jumpDetected || (nowMs - rtdLastRecoverMs[i] >= RTD_RECOVER_COOLDOWN_MS))) {
    rtdLastRecoverMs[i] = nowMs;
    rtdRecoveryRequest((uint8_t)i);
  }

  // Still invalid while recovery is pending or not yet started:
  // Avoid immediate 0 output on first/second bad reads. Hold last-good briefly.
  if (rtdHasGoodValue[i] && rtdBadCount[i] < RTD_ZERO_AFTER_BAD_COUNT) {
    rtdTempC[i]    = rtdLastGoodTempC[i];
    rtdTemp_x10[i] = rtdLastGoodTempX10[i];
    mb.Hreg(HREG_TEMP_BASE + i, (uint16_t)rtdTemp_x10[i]);
  } else {
    rtdTemp_x10[i] = 0;
    rtdTempC[i]    = 0;
    mb.Hreg(HREG_TEMP_BASE + i, 0);
  }
}

void readSensors() {
  // AI channels are scanned asynchronously by adsTick() every loop iteration.

  // FAST RTD temperature — cheap paths every tick; hardware read one channel/tick.
  const uint8_t  RTD_ZERO_AFTER_BAD_COUNT = 3;
  for (int i=0;i<2;i++) {
    float recoveredTemp = 0.0f;
    bool  recoveredOk   = false;
    if (rtdRecoveryTakeResult((uint8_t)i, recoveredTemp, recoveredOk)) {
      if (recoveredOk) {
        rtdBadCount[i] = 0;
        rtdHasGoodValue[i] = true;
        rtdLastGoodTempC[i] = recoveredTemp;
        rtdLastGoodTempX10[i] = (int16_t)lroundf(recoveredTemp * 10.0f);
        rtdTempC[i] = recoveredTemp;
        rtdTemp_x10[i] = rtdLastGoodTempX10[i];
        mb.Hreg(HREG_TEMP_BASE + i, (uint16_t)rtdTemp_x10[i]);
      } else if (rtdHasGoodValue[i] && rtdBadCount[i] < RTD_ZERO_AFTER_BAD_COUNT) {
        rtdTempC[i]    = rtdLastGoodTempC[i];
        rtdTemp_x10[i] = rtdLastGoodTempX10[i];
        mb.Hreg(HREG_TEMP_BASE + i, (uint16_t)rtdTemp_x10[i]);
      } else {
        rtdTemp_x10[i] = 0;
        rtdTempC[i]    = 0;
        mb.Hreg(HREG_TEMP_BASE + i, 0);
      }
      continue;
    }

    if (rtdRecoveryBusy()) {
      if (rtdHasGoodValue[i] && rtdBadCount[i] < RTD_ZERO_AFTER_BAD_COUNT) {
        rtdTempC[i]    = rtdLastGoodTempC[i];
        rtdTemp_x10[i] = rtdLastGoodTempX10[i];
        mb.Hreg(HREG_TEMP_BASE + i, (uint16_t)rtdTemp_x10[i]);
      } else if (!rtdHasGoodValue[i]) {
        rtdTemp_x10[i] = 0;
        rtdTempC[i]    = 0;
        mb.Hreg(HREG_TEMP_BASE + i, 0);
      }
      continue;
    }

    if (!rtd_ok[i]) {
      // If channel is not initialized, suppress bad values.
      // Publish last known good value if we have one, otherwise publish 0.
      if (rtdHasGoodValue[i]) {
        rtdTempC[i]    = rtdLastGoodTempC[i];
        rtdTemp_x10[i] = rtdLastGoodTempX10[i];
        mb.Hreg(HREG_TEMP_BASE + i, (uint16_t)rtdTemp_x10[i]);
      } else {
        rtdTemp_x10[i] = 0;
        rtdTempC[i]    = 0;
        mb.Hreg(HREG_TEMP_BASE + i, 0);
      }
      continue;
    }

    if ((uint8_t)i != rtdScanCh) {
      // Non-scanned channel: republish current value only (no SPI).
      mb.Hreg(HREG_TEMP_BASE + i, (uint16_t)rtdTemp_x10[i]);
      continue;
    }

    rtdServiceChannel((uint8_t)i);
  }

  rtdScanCh ^= 1;
}

// ================== Setup ==================
void setup() {
  Serial.begin(115200);

  for (uint8_t i=0;i<NUM_LED;i++) {
    pinMode(LED_PINS[i], OUTPUT);
    digitalWrite(LED_PINS[i], LOW);
    ledState[i] = false;
  }
  for (uint8_t i=0;i<NUM_BTN;i++) {
    pinMode(BTN_PINS[i], INPUT);
    buttonState[i] = buttonPrev[i] = false;
  }

  setDefaults();

  WebSerial.on("values",  handleValues);
  WebSerial.on("Config",  handleUnifiedConfig);
  WebSerial.on("command", handleCommand);

  if (!initFilesystemAndConfig()) {
    wsLog( "FATAL: Filesystem/config init failed");
  }

  Wire1.setSDA(SDA1);
  Wire1.setSCL(SCL1);
  Wire1.begin();
  Wire1.setClock(400000);

  ads_ok = ads.begin();
  if (ads_ok) {
    ads.setGain(1);
    ads.setDataRate(4);
    ads.setMode(1);   // 1 = single shot
    wsLog( "ADS1115 OK @0x48 (Wire1)");
  } else {
    wsLog( "ERROR: ADS1115 not found @0x48");
    for (int ch=0; ch<4; ch++) {
      aiRaw[ch] = 0;
      aiMv[ch]  = 0;
    }
  }

  dac_ok[0] = dac0.begin(0x60, &Wire1);
  dac_ok[1] = dac1.begin(0x61, &Wire1);
  wsLog( dac_ok[0] ? "MCP4725 #0 OK @0x60 (Wire1)" : "ERROR: MCP4725 #0 not found");
  wsLog( dac_ok[1] ? "MCP4725 #1 OK @0x61 (Wire1)" : "ERROR: MCP4725 #1 not found");

  applyRtdHardwareCfg();

  Serial2.setTX(TX2);
  Serial2.setRX(RX2);
  Serial2.setFIFOSize(256);
  Serial2.begin(g_mb_baud);
  mb.config(g_mb_baud);
  setSlaveIdIfAvailable(mb, g_mb_address);
  mb.setAdditionalServerData("AIO422-AIO");

  for (uint16_t i=0;i<NUM_BTN;i++) mb.addIsts(ISTS_BTN_BASE + i);
  for (uint16_t i=0;i<NUM_LED;i++) mb.addIsts(ISTS_LED_BASE + i);

  for (uint16_t i=0;i<2;i++) mb.addHreg(HREG_TEMP_BASE  + i);
  for (uint16_t i=0;i<4;i++) mb.addHreg(HREG_AI_MV_BASE + i);
  for (uint16_t i=0;i<2;i++) mb.addHreg(HREG_DAC_BASE   + i, 0);

  if (!ads_ok) {
    for (int ch=0; ch<4; ch++) mb.Hreg(HREG_AI_MV_BASE + ch, 0);
  }

  hmRegisterIdentity(mb, HM_MODEL_ID, HM_FW_MAJOR, HM_FW_MINOR, HM_FW_PATCH, HM_MAP_VERSION);

  applyPowerOnOutputs();

  wsLog("Boot OK (AIO-422-R1 RP2350: ADS1115@Wire1, 2xMCP4725@Wire1, 2xMAX31865 softSPI, 4 BTN, 4 LED + Web-only RTD config/diagnostics)");

  updateRtdDiagnostics();
  sendWebBootstrap();
  g_lastLinkSeenMs = millis();
  hmWatchdogArm(4000);
}

// ================== Unified WebConfig outbound ==================
void sendWebStatus() {
  JSONVar st;
  st["model"] = HM_MODEL_ID;
  st["fw"]    = HM_FW;
  st["map"]   = HM_MAP;
  st["addr"]  = g_mb_address;
  st["baud"]  = g_mb_baud;
  st["linkOk"] = ((uint32_t)(millis() - g_lastLinkSeenMs) < LINK_TIMEOUT_MS) ? 1 : 0;
  WebSerial.send("status", st);
}

void sendWebCfg() {
  JSONVar cfg;
  for (int i = 0; i < NUM_BTN; i++) {
    cfg["btn"][i]["action"] = btnAction[i];
    cfg["ext"]["btnAction"][i] = btnAction[i];
  }
  for (int i = 0; i < NUM_LED; i++) {
    cfg["led"][i]["source"] = ledSrc[i];
    cfg["ext"]["ledSrc"][i] = ledSrc[i];
  }
  for (int i = 0; i < 2; i++) {
    cfg["ext"]["dac"]["raw"][i] = dacRaw[i];
    cfg["ext"]["dac"]["powerOn"][i] = (int)aoPowerOn[i];
  }
  for (int i = 0; i < 2; i++) {
    cfg["ext"]["rtd"]["wires"][i]    = (int)rtdWiresCfg[i];
    cfg["ext"]["rtd"]["rnominal"][i] = (int)rtdRnominalCfg[i];
    cfg["ext"]["rtd"]["rref"][i]     = (int)rtdRrefCfg[i];
  }
  WebSerial.send("cfg", cfg);
}

void sendWebExt() {
  JSONVar ext;
  for (int i = 0; i < 4; i++) ext["ai"][i] = aiMv[i];
  for (int i = 0; i < 2; i++) ext["rtd"]["temp_x10"][i] = rtdTemp_x10[i];
  JSONVar info;
  for (int i = 0; i < 2; i++) {
    info["temp_x10"][i] = rtdTemp_x10[i];
    info["temp_c"][i]   = rtdTempC[i];
    info["fault"][i]    = (int)rtdFault[i];
    info["error"][i]    = rtdError[i];
    info["raw"][i]      = (int)rtdRawCode[i];
    info["ratio"][i]    = rtdRatio[i];
    info["ohms"][i]     = rtdOhms[i];
  }
  ext["rtd"]["info"] = info;
  WebSerial.send("ext", ext);
}

void sendWebBootstrap() {
  sendWebStatus();
  sendWebCfg();
  sendWebExt();
}

static void serviceBtnDebounce(BtnDebounce &st, bool raw, uint32_t now) {
  if (raw != st.raw) {
    st.raw = raw;
    st.lastChangeMs = now;
  }
  if ((uint32_t)(now - st.lastChangeMs) >= BTN_DEBOUNCE_MS) {
    st.prevStable = st.stable;
    st.stable = st.raw;
  }
}

// ================== Button action executor ==================
static inline void setAO(int ch, uint16_t v) {
  dacRaw[ch] = v;
  mb.Hreg(HREG_DAC_BASE + ch, dacRaw[ch]);
  writeDac(ch, dacRaw[ch]);
}

static const int AO_STEP_PCT = 10;
static void stepAO(int ch, int deltaPct) {
  int pct = (int)lroundf((float)dacRaw[ch] * 100.0f / 4095.0f);
  pct += deltaPct;
  if (pct < 0)   pct = 0;
  if (pct > 100) pct = 100;
  uint16_t v = (uint16_t)lroundf((float)pct * 4095.0f / 100.0f);
  setAO(ch, v);
}

void runButtonAction(uint8_t btnIndex) {
  uint8_t act = btnAction[btnIndex];

  switch (act) {
    case BTNACT_LED_MANUAL_TOGGLE:
      if (ledSrc[btnIndex] == LEDSRC_MANUAL) ledState[btnIndex] = !ledState[btnIndex];
      break;

    case BTNACT_AO1_ONOFF:
      setAO(0, dacRaw[0] != 0 ? 0 : 4095);
      break;
    case BTNACT_AO2_ONOFF:
      setAO(1, dacRaw[1] != 0 ? 0 : 4095);
      break;

    case BTNACT_AO1_ONOFF_LEVEL:
      if (dacRaw[0] != 0) setAO(0, 0);
      else                setAO(0, aoLastLevel[0] ? aoLastLevel[0] : 4095);
      break;
    case BTNACT_AO2_ONOFF_LEVEL:
      if (dacRaw[1] != 0) setAO(1, 0);
      else                setAO(1, aoLastLevel[1] ? aoLastLevel[1] : 4095);
      break;

    case BTNACT_AO1_UP:   stepAO(0, +AO_STEP_PCT); break;
    case BTNACT_AO1_DOWN: stepAO(0, -AO_STEP_PCT); break;
    case BTNACT_AO2_UP:   stepAO(1, +AO_STEP_PCT); break;
    case BTNACT_AO2_DOWN: stepAO(1, -AO_STEP_PCT); break;

    case BTNACT_ALL_AO_OFF:
      setAO(0, 0);
      setAO(1, 0);
      break;

    default:
      break;
  }
}

// ================== Main loop ==================
void loop() {
  hmWatchdogFeed();
  unsigned long now = millis();

  WebSerial.check();
  if (Serial2.available() > 0) g_lastLinkSeenMs = now;

  mb.task();
  rtdRecoveryTick();
  adsTick();

  if (cfgDirty && (now - lastCfgTouchMs >= CFG_AUTOSAVE_MS)) {
    if (saveConfigFS()) wsLog( "Configuration saved");
    else               wsLog( "ERROR: Save failed");
    cfgDirty = false;
  }

  // Buttons (debounced)
  for (int i=0;i<NUM_BTN;i++) {
    bool raw = (digitalRead(BTN_PINS[i]) == HIGH);
    serviceBtnDebounce(btnDeb[i], raw, now);
    buttonPrev[i]  = btnDeb[i].prevStable;
    buttonState[i] = btnDeb[i].stable;
    if (!btnDeb[i].prevStable && btnDeb[i].stable) {
      runButtonAction((uint8_t)i);
    }
    mb.setIsts(ISTS_BTN_BASE + i, btnDeb[i].stable);
  }

  if (!outTrackInit) {
    memcpy(prevDacRaw, dacRaw, sizeof(prevDacRaw));
    outTrackInit = true;
  } else {
    for (int i = 0; i < 2; i++) {
      if (dacRaw[i] != prevDacRaw[i]) {
        if (dacRaw[i] != 0) aoLastLevel[i] = dacRaw[i];
        prevDacRaw[i] = dacRaw[i];
        lastOutChangeMs = now;
      }
    }
  }
  maybePersistOutputState(now);

  // DAC from Modbus (clamp to 12-bit, echo clamped value)
  for (int i=0;i<2;i++) {
    uint16_t regVal = mb.Hreg(HREG_DAC_BASE + i);
    if (regVal > 4095) {
      regVal = 4095;
      mb.Hreg(HREG_DAC_BASE + i, regVal);   // echo the clamped value back to the master
    }
    if (regVal != dacRaw[i]) {
      dacRaw[i] = regVal;
      writeDac(i, dacRaw[i]);
      lastOutChangeMs = now;
    }
  }

  if (now - lastSensorRead >= sensorInterval) {
    lastSensorRead = now;
    readSensors();
  }

  if (now - lastBlinkToggle >= blinkPeriodMs) {
    lastBlinkToggle = now;
    identifyBlinkPhase = !identifyBlinkPhase;
  }

  // LEDs — ledState is logical/manual; ledPhys is what is driven
  const bool identifying = g_identifyUntilMs && ((int32_t)(now - g_identifyUntilMs) < 0);
  for (int i=0;i<NUM_LED;i++) {
    bool on;
    if (identifying) {
      on = identifyBlinkPhase;
    } else if (ledSrc[i] == LEDSRC_MANUAL) {
      on = ledState[i];
    } else {
      on = getLedAutoState(ledSrc[i]);
    }
    ledPhys[i] = on;
    digitalWrite(LED_PINS[i], on ? HIGH : LOW);
    mb.setIsts(ISTS_LED_BASE + i, on);
  }

  // WebSerial UI updates (unified status/io/ext) — outbound only throttled
  if (now - lastSend >= sendInterval) {
    lastSend = now;

    if (hmUsbCanSend()) {
      sendWebStatus();

      JSONVar io;
      for (int i = 0; i < NUM_BTN; i++) io["btn"][i] = buttonState[i] ? 1 : 0;
      for (int i = 0; i < NUM_LED; i++) io["led"][i] = ledPhys[i] ? 1 : 0;
      WebSerial.send("io", io);

      updateRtdDiagnostics();
      sendWebExt();
    }
  }

  mb.task();
}
