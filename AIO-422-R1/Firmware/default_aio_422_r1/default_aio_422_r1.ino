// AIO-422-R1 (RP2350) – Golden Firmware (FINAL)
// ------------------------------------------------------------
// This is the production “golden build” firmware.
// Code behavior is frozen; only comments were rewritten.
//
// Hardware:
//  - ADS1115 (4x AI) on Wire1 (SDA=6, SCL=7)
//  - 2x MCP4725 DAC (AO1=0x60, AO2=0x61) on Wire1
//  - 2x MAX31865 RTD on soft-SPI (CS=13/14, CLK=10, DO=12, DI=11)
//  - 4x Buttons on GPIO 22..25
//  - 4x LEDs on GPIO 18..21
//  - Modbus RTU on Serial2 (TX=4, RX=5)
//
// Key behaviors:
//  - Web “manual setpoint” is separate from Modbus SP registers
//  - PID parameters EN/KP/KI/KD mirror Modbus writes into runtime config
//  - PID mode (direct/reverse), LED sources, button actions are Web-only + persisted
//  - Cascade: PID outputs can feed other PID setpoints
//  - RTD configuration + diagnostics are Web-only (no Modbus map changes)
//  - Web traffic is throttled so Modbus stays responsive
// ------------------------------------------------------------

#include <Arduino.h>
#include <Wire.h>
#include <ADS1X15.h>
#include <Adafruit_MCP4725.h>
#include <Adafruit_MAX31865.h>

#include <ModbusSerial.h>
#include <SimpleWebSerial.h>
#include <Arduino_JSON.h>
#include <LittleFS.h>
#include <utility>
#include <math.h>
#include "hardware/watchdog.h"

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

// ================== Runtime state ==================
bool buttonState[NUM_BTN] = {false,false,false,false};
bool buttonPrev[NUM_BTN]  = {false,false,false,false};
bool ledState[NUM_LED]    = {false,false,false,false};

int16_t  aiRaw[4]   = {0,0,0,0};
uint16_t aiMv[4]    = {0,0,0,0};
int16_t  rtdTemp_x10[2] = {0,0};

// ===== RTD fast recovery state (per-channel) =====
// Used to recover quickly after ESD / latched MAX31865 upset without
// repeatedly reinitializing the chip on every loop iteration.
uint32_t rtdLastRecoverMs[2]     = {0, 0};
uint8_t  rtdBadCount[2]          = {0, 0};
float     rtdLastGoodTempC[2]   = {0, 0};
int16_t   rtdLastGoodTempX10[2] = {0, 0};
bool      rtdHasGoodValue[2]    = {false, false};

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
uint16_t rtdRrefCfg[2]     = {200, 200};

uint16_t dacRaw[2] = {0,0};

// ================== Web Serial ==================
SimpleWebSerial WebSerial;
JSONVar modbusStatus;

// ================== Timing ==================
unsigned long lastSend       = 0;
// FIX A: slow down general WebSerial traffic
const unsigned long sendInterval   = 1000;

unsigned long lastSensorRead = 0;
const unsigned long sensorInterval = 200;

unsigned long lastPidUpdateMs = 0;
const unsigned long pidIntervalMs = 200;

// FIX B: RTD full info only every 2 seconds
unsigned long lastRtdInfoSend = 0;
const unsigned long rtdInfoInterval = 2000;

// ================== Persisted Modbus settings ==================
uint8_t  g_mb_address = 3;
uint32_t g_mb_baud    = 19200;

// ================== Modbus map ==================
enum : uint16_t {
  ISTS_BTN_BASE   = 1,
  ISTS_LED_BASE   = 20,

  HREG_AI_BASE     = 100,
  HREG_TEMP_BASE   = 120,
  HREG_AI_MV_BASE  = 140,
  HREG_DAC_BASE    = 200,

  HREG_SP_BASE        = 300, // 300..303 = SP1..SP4 (MODBUS)
  HREG_PID_EN_BASE    = 310,
  HREG_PID_PVSEL_BASE = 320,  // declared but not used (kept for compatibility)
  HREG_PID_SPSEL_BASE = 330,  // declared but not used
  HREG_PID_OUTSEL_BASE= 340,  // declared but not used
  HREG_PID_KP_BASE    = 350,
  HREG_PID_KI_BASE    = 360,
  HREG_PID_KD_BASE    = 370,
  HREG_PID_OUT_BASE   = 380,
  HREG_PID_PVVAL_BASE = 390,
  HREG_PID_ERR_BASE   = 400,

  HREG_MBPV_BASE      = 410   // 410..413 = MBPV1..MBPV4
};

// ================== PID state ==================
struct PIDState {
  bool    enabled;
  uint8_t pvSource;     // 0 none, 1..4 AI1..4(mV), 5 RTD1, 6 RTD2, 7..10 MBPV1..MBPV4
  uint8_t spSource;     // 0 manual (Web), 1..4 SP1..SP4(Modbus), 5..8 PID1..PID4 OUT
  uint8_t outTarget;    // 0 none, 1 AO1, 2 AO2, 3 virtual-only
  uint8_t mode;         // 0 direct, 1 reverse (Web-only persisted)

  float   Kp;
  float   Ki;
  float   Kd;

  float   integral;
  float   prevError;
  float   output;       // RAW output (0..4095)

  float   pvMin;
  float   pvMax;
  float   outMin;
  float   outMax;

  float   pvPct;
  float   spPct;
  float   outPct;
};

PIDState pid[4];

float pidVirtRaw[4] = {0,0,0,0};
float pidVirtPct[4] = {0,0,0,0};

// ===== Manual setpoints (Web only), per PID (NOT Modbus) =====
int16_t pidManualSp[4] = {0,0,0,0};

// ================== LED source selection (Web-only, persisted) ==================
enum : uint8_t {
  LEDSRC_MANUAL = 0,

  LEDSRC_PID1_EN = 1,
  LEDSRC_PID2_EN = 2,
  LEDSRC_PID3_EN = 3,
  LEDSRC_PID4_EN = 4,

  LEDSRC_PID1_AT0   = 5,
  LEDSRC_PID2_AT0   = 6,
  LEDSRC_PID3_AT0   = 7,
  LEDSRC_PID4_AT0   = 8,
  LEDSRC_PID1_AT100 = 9,
  LEDSRC_PID2_AT100 = 10,
  LEDSRC_PID3_AT100 = 11,
  LEDSRC_PID4_AT100 = 12,

  LEDSRC_AO1_AT0    = 13,
  LEDSRC_AO2_AT0    = 14,
  LEDSRC_AO1_AT100  = 15,
  LEDSRC_AO2_AT100  = 16,
};

uint8_t ledSrc[4] = { LEDSRC_MANUAL, LEDSRC_MANUAL, LEDSRC_MANUAL, LEDSRC_MANUAL };

static const uint16_t AO_ZERO_TH = 5;
static const uint16_t AO_FULL_TH = 4090;

// ================== Button actions (Web-only, persisted) ==================
enum : uint8_t {
  BTNACT_LED_MANUAL_TOGGLE = 0,
  BTNACT_TOGGLE_PID1       = 1,
  BTNACT_TOGGLE_PID2       = 2,
  BTNACT_TOGGLE_PID3       = 3,
  BTNACT_TOGGLE_PID4       = 4,

  BTNACT_TOGGLE_AO1_0      = 5,
  BTNACT_TOGGLE_AO2_0      = 6,
  BTNACT_TOGGLE_AO1_MAX    = 7,
  BTNACT_TOGGLE_AO2_MAX    = 8
};

uint8_t btnAction[4] = { BTNACT_LED_MANUAL_TOGGLE, BTNACT_LED_MANUAL_TOGGLE,
                         BTNACT_LED_MANUAL_TOGGLE, BTNACT_LED_MANUAL_TOGGLE };

// ================== Persistence (LittleFS) ==================
struct PersistConfig {
  uint32_t magic;
  uint16_t version;
  uint16_t size;

  uint16_t dacRaw[2];
  uint8_t  mb_address;
  uint32_t mb_baud;

  uint8_t  pid_mode[4];
  int16_t  pid_manual_sp[4];

  uint8_t  led_src[4];
  uint8_t  btn_action[4];

  uint8_t  rtd_wires[2];
  uint16_t rtd_rnominal[2];
  uint16_t rtd_rref[2];

  uint32_t crc32;
} __attribute__((packed));

static const uint32_t CFG_MAGIC   = 0x314F4941UL;
static const uint16_t CFG_VERSION = 0x0007;
static const char*    CFG_PATH    = "/cfg.bin";

volatile bool  cfgDirty        = false;
uint32_t       lastCfgTouchMs  = 0;
const uint32_t CFG_AUTOSAVE_MS = 1500;

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

static void sanitizeRtdCfg() {
  for (int i=0;i<2;i++) {
    uint8_t w = rtdWiresCfg[i];
    if (w != 2 && w != 3 && w != 4) w = 2;
    rtdWiresCfg[i] = w;

    uint16_t rn = rtdRnominalCfg[i];
    if (rn != 100 && rn != 1000) rn = 100;
    rtdRnominalCfg[i] = rn;

    uint16_t rr = rtdRrefCfg[i];
    if (rr != 200 && rr != 400 && rr != 2000 && rr != 4000) rr = 200;
    rtdRrefCfg[i] = rr;
  }
}

bool getLedAutoState(uint8_t src) {
  if (src >= LEDSRC_PID1_EN && src <= LEDSRC_PID4_EN) {
    uint8_t i = src - LEDSRC_PID1_EN;
    return pid[i].enabled;
  }
  if (src >= LEDSRC_PID1_AT0 && src <= LEDSRC_PID4_AT0) {
    uint8_t i = src - LEDSRC_PID1_AT0;
    return (pidVirtPct[i] <= 0.01f);
  }
  if (src >= LEDSRC_PID1_AT100 && src <= LEDSRC_PID4_AT100) {
    uint8_t i = src - LEDSRC_PID1_AT100;
    return (pidVirtPct[i] >= 99.99f);
  }
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

  g_mb_address = 3;
  g_mb_baud    = 19200;

  for (int i=0;i<4;i++) {
    pid[i].mode = 0;
    pidManualSp[i] = 0;
    ledSrc[i] = LEDSRC_MANUAL;
    btnAction[i] = BTNACT_LED_MANUAL_TOGGLE;
  }

  rtdWiresCfg[0]    = 2;
  rtdWiresCfg[1]    = 2;
  rtdRnominalCfg[0] = 100;
  rtdRnominalCfg[1] = 100;
  rtdRrefCfg[0]     = 200;
  rtdRrefCfg[1]     = 200;
  sanitizeRtdCfg();

  rtdFault[0] = rtdFault[1] = 0;
  rtdError[0] = rtdError[1] = "";
  rtdRawCode[0] = rtdRawCode[1] = 0;
  rtdRatio[0] = rtdRatio[1] = 0;
  rtdOhms[0] = rtdOhms[1] = 0;
  rtdTempC[0] = rtdTempC[1] = 0;
}

void captureToPersist(PersistConfig &pc) {
  pc.magic   = CFG_MAGIC;
  pc.version = CFG_VERSION;
  pc.size    = sizeof(PersistConfig);

  pc.dacRaw[0] = dacRaw[0];
  pc.dacRaw[1] = dacRaw[1];

  pc.mb_address = g_mb_address;
  pc.mb_baud    = g_mb_baud;

  for (int i=0;i<4;i++) {
    pc.pid_mode[i] = pid[i].mode;
    pc.pid_manual_sp[i] = pidManualSp[i];
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

// ---- older format apply helpers (same as your code) ----
bool applyFromPersist_v2(const uint8_t* buf, size_t len) {
  struct PersistConfigV2 {
    uint32_t magic; uint16_t version; uint16_t size;
    uint16_t dacRaw[2]; uint8_t mb_address; uint32_t mb_baud;
    uint8_t  pid_mode[4];
    uint32_t crc32;
  } __attribute__((packed));

  if (len != sizeof(PersistConfigV2)) return false;
  PersistConfigV2 pc{}; memcpy(&pc, buf, sizeof(pc));

  if (pc.magic != CFG_MAGIC || pc.size != sizeof(PersistConfigV2)) return false;
  uint32_t crc = pc.crc32; pc.crc32 = 0;
  if (crc32_update(0, (const uint8_t*)&pc, sizeof(PersistConfigV2)) != crc) return false;
  if (pc.version != 0x0002) return false;

  dacRaw[0] = pc.dacRaw[0]; dacRaw[1] = pc.dacRaw[1];
  g_mb_address = pc.mb_address; g_mb_baud = pc.mb_baud;

  for (int i=0;i<4;i++) {
    pid[i].mode = clamp_u8((int)pc.pid_mode[i], 0, 1);
    pidManualSp[i] = 0;
    ledSrc[i] = LEDSRC_MANUAL;
    btnAction[i] = BTNACT_LED_MANUAL_TOGGLE;
  }

  rtdWiresCfg[0]=2; rtdWiresCfg[1]=2;
  rtdRnominalCfg[0]=100; rtdRnominalCfg[1]=100;
  rtdRrefCfg[0]=200; rtdRrefCfg[1]=200;
  sanitizeRtdCfg();
  return true;
}

bool applyFromPersist_v3(const uint8_t* buf, size_t len) {
  struct PersistConfigV3 {
    uint32_t magic; uint16_t version; uint16_t size;
    uint16_t dacRaw[2]; uint8_t mb_address; uint32_t mb_baud;
    uint8_t  pid_mode[4]; int16_t  pid_manual_sp[4];
    uint32_t crc32;
  } __attribute__((packed));

  if (len != sizeof(PersistConfigV3)) return false;
  PersistConfigV3 pc{}; memcpy(&pc, buf, sizeof(pc));

  if (pc.magic != CFG_MAGIC || pc.size != sizeof(PersistConfigV3)) return false;
  uint32_t crc = pc.crc32; pc.crc32 = 0;
  if (crc32_update(0, (const uint8_t*)&pc, sizeof(PersistConfigV3)) != crc) return false;
  if (pc.version != 0x0003) return false;

  dacRaw[0] = pc.dacRaw[0]; dacRaw[1] = pc.dacRaw[1];
  g_mb_address = pc.mb_address; g_mb_baud = pc.mb_baud;

  for (int i=0;i<4;i++) {
    pid[i].mode = clamp_u8((int)pc.pid_mode[i], 0, 1);
    pidManualSp[i] = pc.pid_manual_sp[i];
    ledSrc[i] = LEDSRC_MANUAL;
    btnAction[i] = BTNACT_LED_MANUAL_TOGGLE;
  }

  rtdWiresCfg[0]=2; rtdWiresCfg[1]=2;
  rtdRnominalCfg[0]=100; rtdRnominalCfg[1]=100;
  rtdRrefCfg[0]=200; rtdRrefCfg[1]=200;
  sanitizeRtdCfg();
  return true;
}

bool applyFromPersist_v4(const uint8_t* buf, size_t len) {
  struct PersistConfigV4 {
    uint32_t magic; uint16_t version; uint16_t size;
    uint16_t dacRaw[2]; uint8_t mb_address; uint32_t mb_baud;
    uint8_t  pid_mode[4]; int16_t  pid_manual_sp[4];
    uint8_t  led_src[4];
    uint32_t crc32;
  } __attribute__((packed));

  if (len != sizeof(PersistConfigV4)) return false;
  PersistConfigV4 pc{}; memcpy(&pc, buf, sizeof(pc));

  if (pc.magic != CFG_MAGIC || pc.size != sizeof(PersistConfigV4)) return false;
  uint32_t crc = pc.crc32; pc.crc32 = 0;
  if (crc32_update(0, (const uint8_t*)&pc, sizeof(PersistConfigV4)) != crc) return false;
  if (pc.version != 0x0004) return false;

  dacRaw[0] = pc.dacRaw[0]; dacRaw[1] = pc.dacRaw[1];
  g_mb_address = pc.mb_address; g_mb_baud = pc.mb_baud;

  for (int i=0;i<4;i++) {
    pid[i].mode = clamp_u8((int)pc.pid_mode[i], 0, 1);
    pidManualSp[i] = pc.pid_manual_sp[i];
    ledSrc[i] = clamp_u8((int)pc.led_src[i], 0, 16);
    btnAction[i] = BTNACT_LED_MANUAL_TOGGLE;
  }

  rtdWiresCfg[0]=2; rtdWiresCfg[1]=2;
  rtdRnominalCfg[0]=100; rtdRnominalCfg[1]=100;
  rtdRrefCfg[0]=200; rtdRrefCfg[1]=200;
  sanitizeRtdCfg();
  return true;
}

bool applyFromPersist_v5(const uint8_t* buf, size_t len) {
  struct PersistConfigV5 {
    uint32_t magic; uint16_t version; uint16_t size;
    uint16_t dacRaw[2]; uint8_t mb_address; uint32_t mb_baud;
    uint8_t  pid_mode[4]; int16_t  pid_manual_sp[4];
    uint8_t  led_src[4]; uint8_t  btn_action[4];
    uint32_t crc32;
  } __attribute__((packed));

  if (len != sizeof(PersistConfigV5)) return false;
  PersistConfigV5 pc{}; memcpy(&pc, buf, sizeof(pc));

  if (pc.magic != CFG_MAGIC || pc.size != sizeof(PersistConfigV5)) return false;
  uint32_t crc = pc.crc32; pc.crc32 = 0;
  if (crc32_update(0, (const uint8_t*)&pc, sizeof(PersistConfigV5)) != crc) return false;
  if (pc.version != 0x0005) return false;

  dacRaw[0] = pc.dacRaw[0]; dacRaw[1] = pc.dacRaw[1];
  g_mb_address = pc.mb_address; g_mb_baud = pc.mb_baud;

  for (int i=0;i<4;i++) {
    pid[i].mode = clamp_u8((int)pc.pid_mode[i], 0, 1);
    pidManualSp[i] = pc.pid_manual_sp[i];
    ledSrc[i] = clamp_u8((int)pc.led_src[i], 0, 16);
    btnAction[i] = clamp_u8((int)pc.btn_action[i], 0, 8);
  }

  rtdWiresCfg[0]=2; rtdWiresCfg[1]=2;
  rtdRnominalCfg[0]=100; rtdRnominalCfg[1]=100;
  rtdRrefCfg[0]=200; rtdRrefCfg[1]=200;
  sanitizeRtdCfg();
  return true;
}

bool applyFromPersist(const PersistConfig &pc) {
  if (pc.magic != CFG_MAGIC || pc.size != sizeof(PersistConfig)) return false;

  PersistConfig tmp = pc;
  uint32_t crc = tmp.crc32; tmp.crc32 = 0;
  if (crc32_update(0, (const uint8_t*)&tmp, sizeof(PersistConfig)) != crc) return false;
  if (pc.version != CFG_VERSION) return false;

  dacRaw[0] = pc.dacRaw[0];
  dacRaw[1] = pc.dacRaw[1];
  g_mb_address = pc.mb_address;
  g_mb_baud    = pc.mb_baud;

  for (int i=0;i<4;i++) {
    pid[i].mode = clamp_u8((int)pc.pid_mode[i], 0, 1);
    pidManualSp[i] = pc.pid_manual_sp[i];
    ledSrc[i] = clamp_u8((int)pc.led_src[i], 0, 16);
    btnAction[i] = clamp_u8((int)pc.btn_action[i], 0, 8);
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
  if (!f) { WebSerial.send("message", "save: open failed"); return false; }
  size_t n = f.write((const uint8_t*)&pc, sizeof(pc));
  f.flush();
  f.close();
  if (n != sizeof(pc)) {
    WebSerial.send("message", String("save: short write ")+n);
    return false;
  }

  File r = LittleFS.open(CFG_PATH, "r");
  if (!r) { WebSerial.send("message", "save: reopen failed"); return false; }
  if ((size_t)r.size() != sizeof(PersistConfig)) {
    WebSerial.send("message", "save: size mismatch after write");
    r.close();
    return false;
  }
  PersistConfig back{};
  size_t nr = r.read((uint8_t*)&back, sizeof(back));
  r.close();
  if (nr != sizeof(back)) {
    WebSerial.send("message", "save: short readback");
    return false;
  }
  PersistConfig tmp = back;
  uint32_t crc = tmp.crc32;
  tmp.crc32 = 0;
  if (crc32_update(0, (const uint8_t*)&tmp, sizeof(tmp)) != crc) {
    WebSerial.send("message", "save: CRC verify failed");
    return false;
  }
  return true;
}

bool loadConfigFS() {
  File f = LittleFS.open(CFG_PATH, "r");
  if (!f) { WebSerial.send("message", "load: open failed"); return false; }

  size_t sz = (size_t)f.size();

  if (sz == sizeof(PersistConfig)) {
    PersistConfig pc{};
    size_t n = f.read((uint8_t*)&pc, sizeof(pc));
    f.close();
    if (n != sizeof(pc)) { WebSerial.send("message", "load: short read"); return false; }
    if (!applyFromPersist(pc)) { WebSerial.send("message", "load: magic/version/crc mismatch"); return false; }
    return true;
  }

  uint8_t buf[256];
  if (sz > sizeof(buf)) { WebSerial.send("message", "load: file too big"); f.close(); return false; }
  size_t n = f.read(buf, sz);
  f.close();
  if (n != sz) { WebSerial.send("message", "load: short read"); return false; }

  if (applyFromPersist_v5(buf, sz)) return true;
  if (applyFromPersist_v4(buf, sz)) return true;
  if (applyFromPersist_v3(buf, sz)) return true;
  if (applyFromPersist_v2(buf, sz)) return true;

  WebSerial.send("message", String("load: size ")+sz+" unsupported");
  return false;
}

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

  WebSerial.send("message", "FATAL: first save failed");
  return false;
}

// ================== SFINAE helper for ModbusSerial ==================
template <class M>
inline auto setSlaveIdIfAvailable(M& m, uint8_t id)
  -> decltype(std::declval<M&>().setSlaveId(uint8_t{}), void()) { m.setSlaveId(id); }
inline void setSlaveIdIfAvailable(...) {}

void applyModbusSettings(uint8_t addr, uint32_t baud) {
  if ((uint32_t)modbusStatus["baud"] != baud) {
    Serial2.end();
    Serial2.begin(baud);
    mb.config(baud);
  }
  setSlaveIdIfAvailable(mb, addr);
  g_mb_address = addr;
  g_mb_baud    = baud;
  modbusStatus["address"] = g_mb_address;
  modbusStatus["baud"]    = g_mb_baud;
}

// ================== FW decls ==================
void handleValues(JSONVar values);
void handleCommand(JSONVar obj);
void handleDac(JSONVar obj);
void handlePid(JSONVar obj);
void handlePidMode(JSONVar obj);
void handleLedCfg(JSONVar obj);
void handleBtnCfg(JSONVar obj);
void handleRtdCfg(JSONVar obj);

void performReset();
void sendAllEchoesOnce();
void sendPidSnapshot();
void writeDac(int idx, uint16_t value);
void readSensors();
void updatePids();
void applyRtdHardwareCfg();
void updateRtdDiagnostics();   // FIX C

float getPidPvValue(uint8_t src, bool &ok);
float getPidSpValue(uint8_t pidIndex, uint8_t src, bool &ok);

// ================== Command handler / reset ==================
void handleCommand(JSONVar obj) {
  const char* actC = (const char*)obj["action"];
  if (!actC) { WebSerial.send("message", "command: missing 'action'"); return; }
  String act = String(actC);
  act.toLowerCase();

  if (act == "reset" || act == "reboot") {
    bool ok = saveConfigFS();
    WebSerial.send("message", ok ? "Saved. Rebooting…" : "WARNING: Save verify FAILED. Rebooting anyway…");
    delay(400);
    performReset();
  } else if (act == "save") {
    if (saveConfigFS()) WebSerial.send("message", "Configuration saved");
    else               WebSerial.send("message", "ERROR: Save failed");
  } else if (act == "load") {
    if (loadConfigFS()) {
      WebSerial.send("message", "Configuration loaded");
      applyModbusSettings(g_mb_address, g_mb_baud);
      applyRtdHardwareCfg();
      writeDac(0, dacRaw[0]);
      writeDac(1, dacRaw[1]);
      sendAllEchoesOnce();
    } else {
      WebSerial.send("message", "ERROR: Load failed/invalid");
    }
  } else if (act == "factory") {
    setDefaults();
    if (saveConfigFS()) {
      WebSerial.send("message", "Factory defaults restored & saved");
      applyModbusSettings(g_mb_address, g_mb_baud);
      applyRtdHardwareCfg();
      writeDac(0, dacRaw[0]);
      writeDac(1, dacRaw[1]);
      sendAllEchoesOnce();
    } else {
      WebSerial.send("message", "ERROR: Save after factory reset failed");
    }
  } else {
    WebSerial.send("message", String("Unknown command: ") + actC);
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
  addr = constrain(addr, 1, 255);
  baud = constrain(baud, 9600, 115200);

  applyModbusSettings((uint8_t)addr, (uint32_t)baud);
  WebSerial.send("message", "Modbus configuration updated");

  cfgDirty = true;
  lastCfgTouchMs = millis();
}

void handleDac(JSONVar obj) {
  JSONVar list = obj["list"];
  if (JSON.typeof(list) != "array") return;

  uint32_t now = millis();

  for (int i = 0; i < 2 && i < (int)list.length(); i++) {
    long v = (long)list[i];
    v = constrain(v, 0L, 4095L);
    dacRaw[i] = (uint16_t)v;
    writeDac(i, dacRaw[i]);
    mb.Hreg(HREG_DAC_BASE + i, dacRaw[i]);
  }

  cfgDirty       = true;
  lastCfgTouchMs = now;
  WebSerial.send("message", "DAC values updated");
}

// ===== RTD configuration handler (Web-only, persisted) =====
void handleRtdCfg(JSONVar obj) {
  JSONVar wires = obj["wires"];
  JSONVar rn    = obj["rnominal"];
  JSONVar rr    = obj["rref"];

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
  if (JSON.typeof(rr) == "array") {
    for (int i=0;i<2;i++) {
      if (i >= (int)rr.length()) break;
      int v = (int)rr[i];
      if (v != 200 && v != 400 && v != 2000 && v != 4000) v = 200;
      rtdRrefCfg[i] = (uint16_t)v;
    }
  }

  sanitizeRtdCfg();
  applyRtdHardwareCfg();
  WebSerial.send("message", "RTD configuration updated (Web-only)");

  cfgDirty = true;
  lastCfgTouchMs = millis();
}

void handlePid(JSONVar obj) {
  JSONVar sp     = obj["sp"];
  JSONVar en     = obj["en"];
  JSONVar pv     = obj["pv"];
  JSONVar spSrc  = obj["sp_src"];
  JSONVar out    = obj["out"];
  JSONVar kp     = obj["kp"];
  JSONVar ki     = obj["ki"];
  JSONVar kd     = obj["kd"];

  JSONVar pvMin  = obj["pv_min"];
  JSONVar pvMax  = obj["pv_max"];
  JSONVar outMin = obj["out_min"];
  JSONVar outMax = obj["out_max"];

  for (int i = 0; i < 4; i++) {
    PIDState &p = pid[i];

    if (JSON.typeof(sp) == "array" && i < (int)sp.length()) {
      int16_t spv = (int16_t)((int)sp[i]);
      pidManualSp[i] = spv;
    }

    if (JSON.typeof(en) == "array" && i < (int)en.length()) {
      int v = (int)en[i];
      p.enabled = (v != 0);
      mb.Hreg(HREG_PID_EN_BASE + i, (uint16_t)(p.enabled ? 1 : 0));
    }

    if (JSON.typeof(pv) == "array" && i < (int)pv.length()) {
      int v = (int)pv[i];
      p.pvSource = clamp_u8(v, 0, 10);
    }

    if (JSON.typeof(spSrc) == "array" && i < (int)spSrc.length()) {
      int v = (int)spSrc[i];
      p.spSource = clamp_u8(v, 0, 8);
    }

    if (JSON.typeof(out) == "array" && i < (int)out.length()) {
      int v = (int)out[i];
      p.outTarget = clamp_u8(v, 0, 3);
    }

    if (JSON.typeof(kp) == "array" && i < (int)kp.length()) {
      int16_t raw = (int16_t)((int)kp[i]);
      p.Kp = (float)raw / 100.0f;
      mb.Hreg(HREG_PID_KP_BASE + i, (uint16_t)raw);
    }
    if (JSON.typeof(ki) == "array" && i < (int)ki.length()) {
      int16_t raw = (int16_t)((int)ki[i]);
      p.Ki = (float)raw / 100.0f;
      mb.Hreg(HREG_PID_KI_BASE + i, (uint16_t)raw);
    }
    if (JSON.typeof(kd) == "array" && i < (int)kd.length()) {
      int16_t raw = (int16_t)((int)kd[i]);
      p.Kd = (float)raw / 100.0f;
      mb.Hreg(HREG_PID_KD_BASE + i, (uint16_t)raw);
    }

    if (JSON.typeof(pvMin) == "array" && i < (int)pvMin.length()) p.pvMin = (float)((double)pvMin[i]);
    if (JSON.typeof(pvMax) == "array" && i < (int)pvMax.length()) p.pvMax = (float)((double)pvMax[i]);
    if (JSON.typeof(outMin)== "array" && i < (int)outMin.length()) p.outMin = (float)((double)outMin[i]);
    if (JSON.typeof(outMax)== "array" && i < (int)outMax.length()) p.outMax = (float)((double)outMax[i]);
  }

  WebSerial.send("message", "PID configuration updated via WebSerial");
  cfgDirty = true;
  lastCfgTouchMs = millis();
}

void handlePidMode(JSONVar obj) {
  JSONVar mode = obj["mode"];
  if (JSON.typeof(mode) != "array") {
    WebSerial.send("message", "pidMode: missing 'mode' array");
    return;
  }

  for (int i = 0; i < 4; i++) {
    if (i >= (int)mode.length()) break;
    int m = (int)mode[i];
    pid[i].mode = clamp_u8(m, 0, 1);
  }

  WebSerial.send("message", "PID mode updated via WebSerial (pidMode)");
  cfgDirty = true;
  lastCfgTouchMs = millis();
}

void handleLedCfg(JSONVar obj) {
  JSONVar src = obj["src"];
  if (JSON.typeof(src) != "array") {
    WebSerial.send("message", "ledCfg: missing 'src' array");
    return;
  }

  for (int i = 0; i < 4; i++) {
    if (i >= (int)src.length()) break;
    int v = (int)src[i];
    ledSrc[i] = clamp_u8(v, 0, 16);
  }

  WebSerial.send("message", "LED source configuration updated");
  cfgDirty = true;
  lastCfgTouchMs = millis();
}

void handleBtnCfg(JSONVar obj) {
  JSONVar act = obj["action"];
  if (JSON.typeof(act) != "array") {
    WebSerial.send("message", "btnCfg: missing 'action' array");
    return;
  }

  for (int i = 0; i < 4; i++) {
    if (i >= (int)act.length()) break;
    int v = (int)act[i];
    btnAction[i] = clamp_u8(v, 0, 8);
  }

  WebSerial.send("message", "Button actions updated");
  cfgDirty = true;
  lastCfgTouchMs = millis();
}

// ================== DAC write helper ==================
void writeDac(int idx, uint16_t value) {
  if (idx == 0 && dac_ok[0]) dac0.setVoltage(value, false);
  else if (idx == 1 && dac_ok[1]) dac1.setVoltage(value, false);
}

// ================== Apply RTD hardware config (wire mode) ==================
void applyRtdHardwareCfg() {
  Adafruit_MAX31865* rtds[2] = { &rtd1, &rtd2 };
  for (int i=0;i<2;i++) {
    bool ok = rtds[i]->begin(wiresToEnum(rtdWiresCfg[i]));
    rtd_ok[i] = ok;
    if (ok) {
      rtds[i]->clearFault();
      WebSerial.send("message", String("MAX31865 RTD") + (i+1) + " configured: " +
        String(rtdWiresCfg[i]) + "wire, " + String(rtdRnominalCfg[i]) + "ohm, Rref " + String(rtdRrefCfg[i]) + "ohm");
    } else {
      WebSerial.send("message", String("ERROR: MAX31865 RTD") + (i+1) + " init failed");
    }
  }
}

// ================== RTD recovery helper (ESD / latched MAX31865) ==================
// MAX31865 can become upset after an ESD event and sometimes latch an invalid
// internal state (e.g. stuck fault / wrong value). This helper forces a
// full clear + reconfiguration over SPI so the channel can recover
// automatically without manual reinit.
bool recoverRtd(Adafruit_MAX31865& dev, int idx) {
  // 1) Clear any latched fault state first
  dev.clearFault();
  delay(2);

  // 2) Reinitialize the chip with the currently selected wire type
  bool ok = dev.begin(wiresToEnum(rtdWiresCfg[idx]));
  if (!ok) return false;

  // 3) Clear fault again after reconfiguration
  delay(2);
  dev.clearFault();
  delay(2);

  // 4) Dummy read to settle the chip after begin() reconfiguration
  (void)dev.readRTD();
  delay(5);

  return true;
}

// ================== Fast RTD recovery helper ==================
// Keep delays short so recovery is fast after ESD-upset.
// Anti-flapping is handled in readSensors(); this helper only does a single
// clear + reinit + settle sequence for the affected RTD.
bool recoverRtdFast(Adafruit_MAX31865& dev, int idx) {
  dev.clearFault();
  delay(2);

  bool ok = dev.begin(wiresToEnum(rtdWiresCfg[idx]));
  if (!ok) return false;

  dev.clearFault();
  delay(2);

  // Dummy read to settle after reconfiguration
  (void)dev.readRTD();
  delay(5);

  return true;
}

// ================== Re-apply RTD hardware configuration (one channel) ==================
// This is the essential “same effect as applying RTD settings” recovery:
// it forces a full MAX31865 runtime re-init for the selected wire mode
// (rtdWiresCfg) and clears any latched fault state after ESD upset.
bool reapplyRtdHardwareCfgOne(Adafruit_MAX31865& dev, int idx) {
  // The Web UI "Apply RTD settings" triggers applyRtdHardwareCfg(),
  // which reinitializes BOTH MAX31865 chips sequentially.
  // In practice, after ESD one channel can be latched and only
  // clears reliably when the other channel is reinitialized too.
  Adafruit_MAX31865* rtds[2] = { &rtd1, &rtd2 };
  bool okAll = true;
  for (int i=0;i<2;i++) {
    bool ok = rtds[i]->begin(wiresToEnum(rtdWiresCfg[i]));
    rtd_ok[i] = ok;
    okAll = okAll && ok;
    if (ok) {
      rtds[i]->clearFault();
    }
  }
  return okAll;
}

// ================== FIX C: update RTD diagnostics only every rtdInfoInterval ==================
void updateRtdDiagnostics() {
  Adafruit_MAX31865* rtds[2] = { &rtd1, &rtd2 };

  for (int i=0;i<2;i++) {
    if (!rtd_ok[i]) {
      rtdFault[i]    = 0xFF;
      rtdError[i]    = decodeMax31865Fault(rtdFault[i]);
      rtdRawCode[i]  = 0;
      rtdRatio[i]    = 0;
      rtdOhms[i]     = 0;
      // temp already maintained in readSensors()
      continue;
    }

    uint16_t raw = rtds[i]->readRTD();
    rtdRawCode[i] = raw;

    float rref = (float)rtdRrefCfg[i];
    float ratio = (raw / 32768.0f);
    rtdRatio[i] = ratio;
    rtdOhms[i]  = ratio * rref;

    uint8_t f = rtds[i]->readFault();
    rtdFault[i] = f;
    rtdError[i] = decodeMax31865Fault(f);
  }
}

// ================== Sensor read helper ==================
void readSensors() {
  if (ads_ok) {
    for (int ch=0; ch<4; ch++) {
      int16_t raw = ads.readADC(ch);
      aiRaw[ch] = raw;

      float v_adc   = ads.toVoltage(raw);
      float v_field = v_adc * ADC_FIELD_SCALE;
      long  mv      = lroundf(v_field * 1000.0f);

      if (mv < 0)      mv = 0;
      if (mv > 65535)  mv = 65535;

      aiMv[ch] = (uint16_t)mv;
      mb.Hreg(HREG_AI_MV_BASE + ch, aiMv[ch]);
    }
  } else {
    for (int ch=0; ch<4; ch++) {
      aiRaw[ch] = 0;
      aiMv[ch]  = 0;
      mb.Hreg(HREG_AI_MV_BASE + ch, 0);
    }
  }

  // FAST RTD temperature only (avoid readRTD here)
  const uint32_t RTD_RECOVER_COOLDOWN_MS = 500;
  const uint8_t  RTD_ZERO_AFTER_BAD_COUNT = 3;
  Adafruit_MAX31865* rtds[2] = { &rtd1, &rtd2 };
  for (int i=0;i<2;i++) {
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

    float rnom = (float)rtdRnominalCfg[i];
    float rref = (float)rtdRrefCfg[i];

    // ---- Fast, stable RTD recovery with anti-flapping ----
    // Do not trust temperature() when faulted or out-of-range.
    // Recovery is rate-limited (cooldown) so we don't reinitialize
    // the MAX31865 on every loop and cause output flapping.

    Adafruit_MAX31865& dev = *rtds[i];
    uint32_t nowMs = millis();

    // Read fault first, then RTD/temperature.
    // MAX31865 can sometimes latch an invalid internal state after ESD where
    // fault bits are not set but the computed temperature/resistance jumps.
    uint8_t f = dev.readFault();
    rtdFault[i] = f;
    rtdError[i] = decodeMax31865Fault(f);

    uint16_t rawCode = dev.readRTD();
    float ratio = (rawCode / 32768.0f);
    float ohmsNow = ratio * rref;

    float temp = dev.temperature(rnom, rref);
    bool tempFinite =
      (!isnan(temp) && !isinf(temp));
    bool tempInRange =
      (temp >= -250.0f && temp <= 850.0f);
    bool readingValid = (f == 0) && tempFinite && tempInRange;

    // Jump detection: if the reading differs too much from the last known-good
    // value, treat it as invalid and force a quick MAX31865 re-init.
    bool jumpDetected = false;

    // Extra guard: if the value is extremely high/low, treat it as suspicious
    // even if the MAX31865 fault bit is not set (common after ESD).
    // This prevents accepting a stuck temperature like ~735C as "valid".
    if (readingValid && (temp > 600.0f || temp < -200.0f)) {
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
      float t = oldTempC;
      float ohmsOld =
        (t >= 0.0f)
          ? (rnom * (1.0f + A*t + B*t*t))
          : (rnom * (1.0f + A*t + B*t*t + C*(t - 100.0f)*t*t*t));

      float deltaR = fabs(ohmsNow - ohmsOld);
      if (deltaT > 100.0f || deltaR > 50.0f) {
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
      // Valid reading: store as last-good and publish immediately
      rtdBadCount[i] = 0;
      rtdHasGoodValue[i] = true;
      rtdLastGoodTempC[i] = temp;
      rtdLastGoodTempX10[i] = (int16_t)lroundf(temp * 10.0f);

      rtdTempC[i] = temp;
      rtdTemp_x10[i] = rtdLastGoodTempX10[i];
      mb.Hreg(HREG_TEMP_BASE + i, (uint16_t)rtdTemp_x10[i]);
      continue;
    }

    // Invalid reading: increment bad counter (anti-flapping)
    rtdBadCount[i]++;

    // Try one fast recovery if cooldown allows
    bool retryValid = false;
    float tempRetry = 0.0f;
    // Try one fast recovery if cooldown allows, or immediately if a jump was detected.
    if (jumpDetected || (nowMs - rtdLastRecoverMs[i] >= RTD_RECOVER_COOLDOWN_MS)) {
      rtdLastRecoverMs[i] = nowMs;

      uint16_t rawDbg = rawCode;
      if (reapplyRtdHardwareCfgOne(dev, i)) {
        uint8_t f2 = dev.readFault();
        float temp2 = dev.temperature(rnom, rref);

        bool tempFinite2 = (!isnan(temp2) && !isinf(temp2));
        bool tempInRange2 = (temp2 >= -250.0f && temp2 <= 850.0f);
        retryValid = (f2 == 0) && tempFinite2 && tempInRange2;

        // Update diagnostics after retry
        rtdFault[i] = f2;
        rtdError[i] = decodeMax31865Fault(f2);

        if (retryValid) tempRetry = temp2;
      }
    }

    if (retryValid) {
      // Publish recovered value immediately and reset bad counter
      rtdBadCount[i] = 0;
      rtdHasGoodValue[i] = true;
      rtdLastGoodTempC[i] = tempRetry;
      rtdLastGoodTempX10[i] = (int16_t)lroundf(tempRetry * 10.0f);

      rtdTempC[i] = tempRetry;
      rtdTemp_x10[i] = rtdLastGoodTempX10[i];
      mb.Hreg(HREG_TEMP_BASE + i, (uint16_t)rtdTemp_x10[i]);
      continue;
    }

    // Still invalid after recovery attempt (or no recovery attempted yet):
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
}

// ================== PID helpers ==================
float getPidPvValue(uint8_t src, bool &ok) {
  ok = false;
  if (src >= 1 && src <= 4) {
    uint8_t idx = src - 1;
    ok = true;
    return (float)((int32_t)aiMv[idx]);
  } else if (src == 5) {
    ok = true;
    return (float)rtdTemp_x10[0];
  } else if (src == 6) {
    ok = true;
    return (float)rtdTemp_x10[1];
  } else if (src >= 7 && src <= 10) {
    uint8_t idx = src - 7;
    ok = true;
    return (float)((int32_t)(uint16_t)mb.Hreg(HREG_MBPV_BASE + idx));
  }
  return 0.0f;
}

float getPidSpValue(uint8_t pidIndex, uint8_t src, bool &ok) {
  ok = false;

  if (src == 0) {
    ok = true;
    return (float)pidManualSp[pidIndex];
  } else if (src >= 1 && src <= 4) {
    uint8_t idx = src - 1;
    int16_t raw = (int16_t)mb.Hreg(HREG_SP_BASE + idx);
    ok = true;
    return (float)raw;
  } else if (src >= 5 && src <= 8) {
    uint8_t idx = src - 5;
    ok = true;
    return pidVirtRaw[idx];
  }
  return 0.0f;
}

// ================== PID update (2-pass for PID->PID SP stability) ==================
void updatePids() {
  unsigned long now = millis();
  if (now - lastPidUpdateMs < pidIntervalMs) return;

  float dt = (now - lastPidUpdateMs) / 1000.0f;
  if (dt <= 0.0f) dt = pidIntervalMs / 1000.0f;
  lastPidUpdateMs = now;

  float newOutRaw[4] = { pidVirtRaw[0], pidVirtRaw[1], pidVirtRaw[2], pidVirtRaw[3] };
  float newOutPct[4] = { pidVirtPct[0], pidVirtPct[1], pidVirtPct[2], pidVirtPct[3] };

  bool active[4] = { false, false, false, false };

  for (int i = 0; i < 4; i++) {
    PIDState &p = pid[i];

    bool  pvOk   = false;
    bool  spOk   = false;
    float pvRaw  = getPidPvValue(p.pvSource, pvOk);
    float spRaw  = getPidSpValue((uint8_t)i, p.spSource, spOk);

    if (p.pvMax <= p.pvMin) { p.pvMin = 0.0f; p.pvMax = 10000.0f; }
    if (p.outMax <= p.outMin) { p.outMin = 0.0f; p.outMax = 4095.0f; }

    mb.Hreg(HREG_PID_PVVAL_BASE + i, (uint16_t)lroundf(pvRaw));

    active[i] = (p.enabled && pvOk && spOk);

    if (!active[i]) {
      p.integral  = 0.0f;
      p.prevError = 0.0f;
      p.output    = 0.0f;
      p.pvPct     = 0.0f;
      p.spPct     = 0.0f;
      p.outPct    = 0.0f;

      newOutRaw[i] = 0.0f;
      newOutPct[i] = 0.0f;

      mb.Hreg(HREG_PID_OUT_BASE + i, 0);
      mb.Hreg(HREG_PID_ERR_BASE + i, 0);
      continue;
    }

    float pvPct = (pvRaw - p.pvMin) * 100.0f / (p.pvMax - p.pvMin);
    float spPct = (spRaw - p.pvMin) * 100.0f / (p.pvMax - p.pvMin);

    pvPct = constrain(pvPct, 0.0f, 100.0f);
    spPct = constrain(spPct, 0.0f, 100.0f);

    float errorPct = spPct - pvPct;
    if (p.mode == 1) errorPct = -errorPct;

    // publish error
    mb.Hreg(HREG_PID_ERR_BASE + i, (uint16_t)lroundf(errorPct));

    float derivPct = (dt > 0.0f) ? ((errorPct - p.prevError) / dt) : 0.0f;

    float pd = p.Kp * errorPct + p.Kd * derivPct;

    float uPct_unclamped = pd + p.integral;
    float uPct_clamped   = constrain(uPct_unclamped, 0.0f, 100.0f);

    bool saturated_low  = (uPct_unclamped <= 0.0f);
    bool saturated_high = (uPct_unclamped >= 100.0f);

    bool allow_integrate =
        (!saturated_low && !saturated_high) ||
        (saturated_low  && (errorPct > 0.0f)) ||
        (saturated_high && (errorPct < 0.0f));

    if (allow_integrate) {
      p.integral += errorPct * dt * p.Ki;
      p.integral = constrain(p.integral, -100.0f, 100.0f);
      uPct_unclamped = pd + p.integral;
      uPct_clamped   = constrain(uPct_unclamped, 0.0f, 100.0f);
    }

    p.prevError = errorPct;

    float uPct = uPct_clamped;

    float outSpan = (p.outMax - p.outMin);
    if (outSpan < 1.0f) outSpan = 1.0f;

    float outRawF = p.outMin + (uPct / 100.0f) * outSpan;
    outRawF = constrain(outRawF, 0.0f, 4095.0f);

    p.output = outRawF;
    p.pvPct  = pvPct;
    p.spPct  = spPct;
    p.outPct = uPct;

    newOutRaw[i] = outRawF;
    newOutPct[i] = uPct;

    mb.Hreg(HREG_PID_OUT_BASE + i, (uint16_t)lroundf(outRawF));
  }

  for (int i=0;i<4;i++) {
    pidVirtRaw[i] = newOutRaw[i];
    pidVirtPct[i] = newOutPct[i];
  }

  // write AO only for active PIDs
  for (int i = 0; i < 4; i++) {
    if (!active[i]) continue;
    PIDState &p = pid[i];
    if (p.outTarget == 1 || p.outTarget == 2) {
      int ch = p.outTarget - 1;
      uint16_t val = (uint16_t)lroundf(pidVirtRaw[i]);
      dacRaw[ch] = val;
      mb.Hreg(HREG_DAC_BASE + ch, dacRaw[ch]);
      writeDac(ch, dacRaw[ch]);
    }
  }
}

// ================== PID snapshot helper ==================
void sendPidSnapshot() {
  JSONVar pidObj;
  JSONVar spArr, enArr, pvArr, spSrcArr, outArr, kpArr, kiArr, kdArr;
  JSONVar pvMinArr, pvMaxArr, outMinArr, outMaxArr;
  JSONVar pvPctArr, spPctArr, outPctArr;
  JSONVar modeArr;
  JSONVar virtRawArr, virtPctArr;

  for (int i = 0; i < 4; i++) {
    int16_t spShow = 0;
    uint8_t src = pid[i].spSource;
    if (src == 0) {
      spShow = pidManualSp[i];
    } else if (src >= 1 && src <= 4) {
      spShow = (int16_t)mb.Hreg(HREG_SP_BASE + (src - 1));
    } else if (src >= 5 && src <= 8) {
      spShow = (int16_t)lroundf(pidVirtRaw[src - 5]);
    }
    spArr[i] = spShow;

    enArr[i]      = (int)(pid[i].enabled ? 1 : 0);
    pvArr[i]      = (int)pid[i].pvSource;
    spSrcArr[i]   = (int)pid[i].spSource;
    outArr[i]     = (int)pid[i].outTarget;
    modeArr[i]    = (int)pid[i].mode;

    kpArr[i]      = (int16_t)lroundf(pid[i].Kp * 100.0f);
    kiArr[i]      = (int16_t)lroundf(pid[i].Ki * 100.0f);
    kdArr[i]      = (int16_t)lroundf(pid[i].Kd * 100.0f);

    pvMinArr[i]   = pid[i].pvMin;
    pvMaxArr[i]   = pid[i].pvMax;
    outMinArr[i]  = pid[i].outMin;
    outMaxArr[i]  = pid[i].outMax;

    pvPctArr[i]   = pid[i].pvPct;
    spPctArr[i]   = pid[i].spPct;
    outPctArr[i]  = pid[i].outPct;

    virtRawArr[i] = pidVirtRaw[i];
    virtPctArr[i] = pidVirtPct[i];
  }

  pidObj["sp"]       = spArr;
  pidObj["en"]       = enArr;
  pidObj["pv"]       = pvArr;
  pidObj["sp_src"]   = spSrcArr;
  pidObj["out"]      = outArr;
  pidObj["kp"]       = kpArr;
  pidObj["ki"]       = kiArr;
  pidObj["kd"]       = kdArr;

  pidObj["pv_min"]   = pvMinArr;
  pidObj["pv_max"]   = pvMaxArr;
  pidObj["out_min"]  = outMinArr;
  pidObj["out_max"]  = outMaxArr;

  pidObj["pv_pct"]   = pvPctArr;
  pidObj["sp_pct"]   = spPctArr;
  pidObj["out_pct"]  = outPctArr;

  pidObj["mode"]     = modeArr;

  pidObj["virt_raw"] = virtRawArr;
  pidObj["virt_pct"] = virtPctArr;

  WebSerial.send("pidState", pidObj);
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

  for (int i=0;i<4;i++) {
    pid[i].enabled   = false;
    pid[i].pvSource  = 0;
    pid[i].spSource  = 0;
    pid[i].outTarget = 0;
    pid[i].mode      = 0;
    pid[i].Kp = pid[i].Ki = pid[i].Kd = 0.0f;
    pid[i].integral  = 0.0f;
    pid[i].prevError = 0.0f;
    pid[i].output    = 0.0f;

    pid[i].pvMin     = 0.0f;
    pid[i].pvMax     = 10000.0f;
    pid[i].outMin    = 0.0f;
    pid[i].outMax    = 4095.0f;

    pid[i].pvPct     = 0.0f;
    pid[i].spPct     = 0.0f;
    pid[i].outPct    = 0.0f;

    pidVirtRaw[i]    = 0.0f;
    pidVirtPct[i]    = 0.0f;

    pidManualSp[i]   = 0;
    ledSrc[i]        = LEDSRC_MANUAL;
    btnAction[i]     = BTNACT_LED_MANUAL_TOGGLE;
  }

  setDefaults();

  WebSerial.on("values",  handleValues);
  WebSerial.on("command", handleCommand);
  WebSerial.on("dac",     handleDac);
  WebSerial.on("pid",     handlePid);
  WebSerial.on("pidMode", handlePidMode);
  WebSerial.on("ledCfg",  handleLedCfg);
  WebSerial.on("btnCfg",  handleBtnCfg);
  WebSerial.on("rtdCfg",  handleRtdCfg);

  if (!initFilesystemAndConfig()) {
    WebSerial.send("message", "FATAL: Filesystem/config init failed");
  }

  Wire1.setSDA(SDA1);
  Wire1.setSCL(SCL1);
  Wire1.begin();
  Wire1.setClock(400000);

  ads_ok = ads.begin();
  if (ads_ok) {
    ads.setGain(1);
    ads.setDataRate(4);
    WebSerial.send("message", "ADS1115 OK @0x48 (Wire1)");
  } else {
    WebSerial.send("message", "ERROR: ADS1115 not found @0x48");
  }

  dac_ok[0] = dac0.begin(0x60, &Wire1);
  dac_ok[1] = dac1.begin(0x61, &Wire1);
  WebSerial.send("message", dac_ok[0] ? "MCP4725 #0 OK @0x60 (Wire1)" : "ERROR: MCP4725 #0 not found");
  WebSerial.send("message", dac_ok[1] ? "MCP4725 #1 OK @0x61 (Wire1)" : "ERROR: MCP4725 #1 not found");

  applyRtdHardwareCfg();

  writeDac(0, dacRaw[0]);
  writeDac(1, dacRaw[1]);

  Serial2.setTX(TX2);
  Serial2.setRX(RX2);
  Serial2.begin(g_mb_baud);
  mb.config(g_mb_baud);
  setSlaveIdIfAvailable(mb, g_mb_address);
  mb.setAdditionalServerData("AIO422-AIO");

  modbusStatus["address"] = g_mb_address;
  modbusStatus["baud"]    = g_mb_baud;
  modbusStatus["state"]   = 0;

  for (uint16_t i=0;i<NUM_BTN;i++) mb.addIsts(ISTS_BTN_BASE + i);
  for (uint16_t i=0;i<NUM_LED;i++) mb.addIsts(ISTS_LED_BASE + i);

  for (uint16_t i=0;i<2;i++) mb.addHreg(HREG_TEMP_BASE  + i);
  for (uint16_t i=0;i<4;i++) mb.addHreg(HREG_AI_MV_BASE + i);
  for (uint16_t i=0;i<2;i++) mb.addHreg(HREG_DAC_BASE   + i, dacRaw[i]);

  for (uint16_t i=0;i<4;i++) mb.addHreg(HREG_SP_BASE + i, 0);
  for (uint16_t i=0;i<4;i++) mb.addHreg(HREG_MBPV_BASE + i, 0);

  for (uint16_t i=0;i<4;i++) {
    mb.addHreg(HREG_PID_EN_BASE     + i, 0);
    mb.addHreg(HREG_PID_KP_BASE     + i, 0);
    mb.addHreg(HREG_PID_KI_BASE     + i, 0);
    mb.addHreg(HREG_PID_KD_BASE     + i, 0);
    mb.addHreg(HREG_PID_OUT_BASE    + i, 0);
    mb.addHreg(HREG_PID_PVVAL_BASE  + i, 0);
    mb.addHreg(HREG_PID_ERR_BASE    + i, 0);
  }

  WebSerial.send("message",
    "Boot OK (AIO-422-R1 RP2350: ADS1115@Wire1, 2xMCP4725@Wire1, 2xMAX31865 softSPI, 4 BTN, 4 LED, 4xPID + Web-only RTD config/diagnostics)");

  sendAllEchoesOnce();
}

// ================== send initial state ==================
void sendAllEchoesOnce() {
  JSONVar dacList;
  dacList[0] = dacRaw[0];
  dacList[1] = dacRaw[1];
  WebSerial.send("dacValues", dacList);

  JSONVar ledList;
  for (int i=0;i<NUM_LED;i++) ledList[i] = ledState[i];
  WebSerial.send("LedStateList", ledList);

  JSONVar ledSrcList;
  for (int i=0;i<NUM_LED;i++) ledSrcList[i] = (int)ledSrc[i];
  WebSerial.send("LedSourceList", ledSrcList);

  JSONVar btnActList;
  for (int i=0;i<NUM_BTN;i++) btnActList[i] = (int)btnAction[i];
  WebSerial.send("ButtonActionList", btnActList);

  JSONVar btnList;
  for (int i=0;i<NUM_BTN;i++) btnList[i] = buttonState[i];
  WebSerial.send("ButtonStateList", btnList);

  JSONVar cfg;
  JSONVar wires, rn, rr;
  wires[0] = (int)rtdWiresCfg[0]; wires[1] = (int)rtdWiresCfg[1];
  rn[0]    = (int)rtdRnominalCfg[0]; rn[1] = (int)rtdRnominalCfg[1];
  rr[0]    = (int)rtdRrefCfg[0]; rr[1] = (int)rtdRrefCfg[1];
  cfg["wires"]    = wires;
  cfg["rnominal"] = rn;
  cfg["rref"]     = rr;
  WebSerial.send("rtdCfg", cfg);

  // Take one diagnostics snapshot at boot
  updateRtdDiagnostics();

  JSONVar info;
  JSONVar t10, tc, fault, err, raw, ratio, ohm;
  t10[0] = rtdTemp_x10[0]; t10[1] = rtdTemp_x10[1];
  tc[0]  = rtdTempC[0];    tc[1]  = rtdTempC[1];
  fault[0] = (int)rtdFault[0]; fault[1] = (int)rtdFault[1];
  err[0] = rtdError[0]; err[1] = rtdError[1];
  raw[0] = (int)rtdRawCode[0]; raw[1] = (int)rtdRawCode[1];
  ratio[0] = rtdRatio[0]; ratio[1] = rtdRatio[1];
  ohm[0] = rtdOhms[0]; ohm[1] = rtdOhms[1];
  info["temp_x10"] = t10;
  info["temp_c"]   = tc;
  info["fault"]    = fault;
  info["error"]    = err;
  info["raw"]      = raw;
  info["ratio"]    = ratio;
  info["ohms"]     = ohm;
  WebSerial.send("rtdInfo", info);

  modbusStatus["address"] = g_mb_address;
  modbusStatus["baud"]    = g_mb_baud;
  WebSerial.send("status", modbusStatus);

  sendPidSnapshot();
}

// ================== Button action executor ==================
static inline void setAO(int ch, uint16_t v) {
  dacRaw[ch] = v;
  mb.Hreg(HREG_DAC_BASE + ch, dacRaw[ch]);
  writeDac(ch, dacRaw[ch]);
}

void runButtonAction(uint8_t btnIndex) {
  uint8_t act = btnAction[btnIndex];

  if (act == BTNACT_LED_MANUAL_TOGGLE) {
    if (ledSrc[btnIndex] == LEDSRC_MANUAL) ledState[btnIndex] = !ledState[btnIndex];
    return;
  }

  if (act >= BTNACT_TOGGLE_PID1 && act <= BTNACT_TOGGLE_PID4) {
    uint8_t pidIdx = act - BTNACT_TOGGLE_PID1;
    pid[pidIdx].enabled = !pid[pidIdx].enabled;
    mb.Hreg(HREG_PID_EN_BASE + pidIdx, (uint16_t)(pid[pidIdx].enabled ? 1 : 0));
    cfgDirty = true;
    lastCfgTouchMs = millis();
    return;
  }

  if (act == BTNACT_TOGGLE_AO1_0) {
    uint16_t target = (dacRaw[0] != 0) ? 0 : 4095;
    setAO(0, target);
    cfgDirty = true;
    lastCfgTouchMs = millis();
    return;
  }
  if (act == BTNACT_TOGGLE_AO2_0) {
    uint16_t target = (dacRaw[1] != 0) ? 0 : 4095;
    setAO(1, target);
    cfgDirty = true;
    lastCfgTouchMs = millis();
    return;
  }
  if (act == BTNACT_TOGGLE_AO1_MAX) {
    uint16_t target = (dacRaw[0] != 4095) ? 4095 : 0;
    setAO(0, target);
    cfgDirty = true;
    lastCfgTouchMs = millis();
    return;
  }
  if (act == BTNACT_TOGGLE_AO2_MAX) {
    uint16_t target = (dacRaw[1] != 4095) ? 4095 : 0;
    setAO(1, target);
    cfgDirty = true;
    lastCfgTouchMs = millis();
    return;
  }
}

// ================== Main loop ==================
void loop() {
  unsigned long now = millis();

  mb.task();

  // Sync PID config FROM Modbus
  for (int i = 0; i < 4; i++) {
    bool en = (mb.Hreg(HREG_PID_EN_BASE + i) != 0);
    if (pid[i].enabled != en) { pid[i].enabled = en; cfgDirty = true; lastCfgTouchMs = now; }

    int16_t kpRaw = (int16_t)mb.Hreg(HREG_PID_KP_BASE + i);
    int16_t kiRaw = (int16_t)mb.Hreg(HREG_PID_KI_BASE + i);
    int16_t kdRaw = (int16_t)mb.Hreg(HREG_PID_KD_BASE + i);

    float kp = (float)kpRaw / 100.0f;
    float ki = (float)kiRaw / 100.0f;
    float kd = (float)kdRaw / 100.0f;

    if (pid[i].Kp != kp) { pid[i].Kp = kp; cfgDirty = true; lastCfgTouchMs = now; }
    if (pid[i].Ki != ki) { pid[i].Ki = ki; cfgDirty = true; lastCfgTouchMs = now; }
    if (pid[i].Kd != kd) { pid[i].Kd = kd; cfgDirty = true; lastCfgTouchMs = now; }
  }

  if (cfgDirty && (now - lastCfgTouchMs >= CFG_AUTOSAVE_MS)) {
    if (saveConfigFS()) WebSerial.send("message", "Configuration saved");
    else               WebSerial.send("message", "ERROR: Save failed");
    cfgDirty = false;
  }

  // Buttons
  for (int i=0;i<NUM_BTN;i++) {
    bool pressed = (digitalRead(BTN_PINS[i]) == HIGH);
    buttonPrev[i]  = buttonState[i];
    buttonState[i] = pressed;

    if (!buttonPrev[i] && buttonState[i]) {
      runButtonAction((uint8_t)i);
    }

    mb.setIsts(ISTS_BTN_BASE + i, pressed);
  }

  // DAC from Modbus (manual control stays available; PID overwrites only when active)
  for (int i=0;i<2;i++) {
    uint16_t regVal = mb.Hreg(HREG_DAC_BASE + i);
    if (regVal != dacRaw[i]) {
      dacRaw[i] = regVal;
      writeDac(i, dacRaw[i]);
      cfgDirty = true;
      lastCfgTouchMs = now;
    }
  }

  if (now - lastSensorRead >= sensorInterval) {
    lastSensorRead = now;
    readSensors();
  }

  updatePids();

  // LEDs
  for (int i=0;i<NUM_LED;i++) {
    bool on = (ledSrc[i] == LEDSRC_MANUAL) ? ledState[i] : getLedAutoState(ledSrc[i]);
    if (ledState[i] != on) ledState[i] = on;
    digitalWrite(LED_PINS[i], on ? HIGH : LOW);
    mb.setIsts(ISTS_LED_BASE + i, on);
  }

  // FIX A: general WebSerial every 1 second
  if (now - lastSend >= sendInterval) {
    lastSend = now;

    WebSerial.check();
    WebSerial.send("status", modbusStatus);

    JSONVar aiList;
    for (int i=0;i<4;i++) aiList[i] = aiMv[i];
    WebSerial.send("aiValues", aiList);

    JSONVar tempList;
    for (int i=0;i<2;i++) tempList[i] = rtdTemp_x10[i];
    WebSerial.send("rtdTemps_x10", tempList);

    JSONVar dacList;
    dacList[0] = dacRaw[0];
    dacList[1] = dacRaw[1];
    WebSerial.send("dacValues", dacList);

    JSONVar ledList;
    for (int i=0;i<NUM_LED;i++) ledList[i] = ledState[i];
    WebSerial.send("LedStateList", ledList);

    JSONVar ledSrcList;
    for (int i=0;i<NUM_LED;i++) ledSrcList[i] = (int)ledSrc[i];
    WebSerial.send("LedSourceList", ledSrcList);

    JSONVar btnActList;
    for (int i=0;i<NUM_BTN;i++) btnActList[i] = (int)btnAction[i];
    WebSerial.send("ButtonActionList", btnActList);

    JSONVar btnList;
    for (int i=0;i<NUM_BTN;i++) btnList[i] = buttonState[i];
    WebSerial.send("ButtonStateList", btnList);

    sendPidSnapshot();
  }

  // FIX B + FIX C: full RTD diagnostics only every 2 seconds
  if (now - lastRtdInfoSend >= rtdInfoInterval) {
    lastRtdInfoSend = now;

    updateRtdDiagnostics();

    JSONVar info;
    JSONVar t10, tc, fault, err, raw, ratio, ohm;
    t10[0] = rtdTemp_x10[0]; t10[1] = rtdTemp_x10[1];
    tc[0]  = rtdTempC[0];    tc[1]  = rtdTempC[1];
    fault[0] = (int)rtdFault[0]; fault[1] = (int)rtdFault[1];
    err[0] = rtdError[0]; err[1] = rtdError[1];
    raw[0] = (int)rtdRawCode[0]; raw[1] = (int)rtdRawCode[1];
    ratio[0] = rtdRatio[0]; ratio[1] = rtdRatio[1];
    ohm[0] = rtdOhms[0]; ohm[1] = rtdOhms[1];
    info["temp_x10"] = t10;
    info["temp_c"]   = tc;
    info["fault"]    = fault;
    info["error"]    = err;
    info["raw"]      = raw;
    info["ratio"]    = ratio;
    info["ohms"]     = ohm;
    WebSerial.send("rtdInfo", info);

    // Also periodically echo current RTD configuration so WebConfig
    // tabs opened after boot still see the correct wiring/sensor/Rref.
    JSONVar cfg;
    JSONVar wires, rn, rr;
    wires[0] = (int)rtdWiresCfg[0]; wires[1] = (int)rtdWiresCfg[1];
    rn[0]    = (int)rtdRnominalCfg[0]; rn[1] = (int)rtdRnominalCfg[1];
    rr[0]    = (int)rtdRrefCfg[0]; rr[1] = (int)rtdRrefCfg[1];
    cfg["wires"]    = wires;
    cfg["rnominal"] = rn;
    cfg["rref"]     = rr;
    WebSerial.send("rtdCfg", cfg);
  }
}
