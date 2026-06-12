// ================== WLD-521-R1 — Flow + Heat (+ LEDs + Buttons like DIM) ==================
// NOTE: This variant mirrors the LED + Button system used on the DIM-420-R1 module.
//       - 4 LEDs (pins 18..21) with per-LED {mode, source}
//       - 4 Buttons (pins 22..25) with per-button {action}
//       - Blink engine identical (500 ms phase), LED sources adapted to WLD (Relays/DI)
//       - Button actions mapped to WLD (toggle/pulse relays)

#include <Arduino.h>
#include <ModbusSerial.h>
#include <SimpleWebSerial.h>
#include <Arduino_JSON.h>
#include <LittleFS.h>
#include <OneWire.h>
#include <utility>
#include "hardware/watchdog.h"   // NOTE: keep it if your board supports it

// ================== UART2 (RS-485 / Modbus) ==================
#define TX2 4
#define RX2 5
const int TxenPin = -1;
int SlaveId = 1;
ModbusSerial mb(Serial2, SlaveId, TxenPin);

// ================== GPIO MAP (WLD-521-R1) ==================
static const uint8_t DI_PINS[5]    = {3, 2, 8, 9, 15}; // DI1..DI5
static const uint8_t RELAY_PINS[2] = {0, 1};           // R1..R2
static const uint8_t ONEWIRE_PIN   = 16;
OneWire oneWire(ONEWIRE_PIN);

// ---------- LEDs + Buttons ----------
static const uint8_t LED_PINS[4]   = {18, 19, 20, 21}; // LED1..LED4
static const uint8_t BTN_PINS[4]   = {22, 23, 24, 25}; // BTN1..BTN4
static const uint8_t NUM_LED = 4;
static const uint8_t NUM_BTN = 4;

// ================== Sizes & types ==================
static const uint8_t NUM_DI  = 5;
static const uint8_t NUM_RLY = 2;
enum : uint8_t { IT_WATER=0, IT_HUMIDITY=1, IT_WCOUNTER=2 };

struct InCfg  { bool enabled; bool inverted; uint8_t action; uint8_t target; uint8_t type; };
struct RlyCfg { bool enabled; bool inverted; };

InCfg  diCfg[NUM_DI];
RlyCfg rlyCfg[NUM_RLY];

bool diState[NUM_DI] = {0};
bool diPrev[NUM_DI]  = {0};
uint32_t diCounter[NUM_DI]   = {0};
uint32_t diLastEdgeMs[NUM_DI]= {0};
const uint32_t CNT_DEBOUNCE_MS = 20;

// ===== NEW: Relay control source & desired states =====
enum RlyCtrl : uint8_t { RCTRL_LOCAL=0, RCTRL_MODBUS=1 };
RlyCtrl rlyCtrlMode[NUM_RLY] = {RCTRL_LOCAL, RCTRL_LOCAL};

bool localDesiredRelay[NUM_RLY]  = {false,false};  // driven by UI/buttons/local logic
bool modbusDesiredRelay[NUM_RLY] = {false,false};  // driven only by Modbus coils
bool physRelayState[NUM_RLY]     = {false,false};  // last physical output (post invert/enable)

uint32_t rlyPulseUntil[NUM_RLY] = {0,0};           // local pulse timing only
const uint32_t PULSE_MS = 500;

// ===== Flow meter per-DI =====
uint32_t flowPulsesPerL[NUM_DI];
float    flowCalibRate[NUM_DI];
float    flowCalibAccum[NUM_DI];
uint32_t flowCounterBase[NUM_DI];

float    flowRateLmin[NUM_DI];
uint32_t lastPulseSnapshot[NUM_DI];
uint32_t nextRateTickMs = 0;

// ===== Heat energy per-DI =====
bool     heatEnabled[NUM_DI];
uint64_t heatAddrA[NUM_DI], heatAddrB[NUM_DI];
float    heatCp[NUM_DI], heatRho[NUM_DI], heatCalib[NUM_DI];
double   heatTA[NUM_DI], heatTB[NUM_DI], heatDT[NUM_DI];
double   heatPowerW[NUM_DI];
double   heatEnergyJ[NUM_DI];

uint32_t nextOneWireConvertMs = 0;
bool     oneWireBusy = false;

// ================== Web Serial ==================
SimpleWebSerial WebSerial;
JSONVar modbusStatus;

// ================== Timing ==================
unsigned long lastSend = 0;
const unsigned long sendInterval = 250;


// ================== Modbus persisted ==================
uint8_t  g_mb_address = 3;
uint32_t g_mb_baud    = 19200;

// ================== LEDs + Buttons config ==================
// LED mode: 0=solid, 1=blink
// LED source codes (adapted to WLD; mirrors DIM style single-field):
enum LedSource : uint8_t {
  LEDSRC_NONE   = 0,
  LEDSRC_R1     = 1,
  LEDSRC_R2     = 2,
  LEDSRC_DI1    = 3,
  LEDSRC_DI2    = 4,
  LEDSRC_DI3    = 5,
  LEDSRC_DI4    = 6,
  LEDSRC_DI5    = 7
};
struct LedCfg { uint8_t mode; uint8_t source; };

// Button actions (DIM-like mapping but for WLD)
enum BtnAction : uint8_t {
  BTN_NONE = 0,
  BTN_TOGGLE_R1 = 1,
  BTN_TOGGLE_R2 = 2,
  BTN_PULSE_R1  = 3,
  BTN_PULSE_R2  = 4
};
struct BtnCfg { uint8_t action; };

LedCfg ledCfg[NUM_LED];
BtnCfg btnCfg[NUM_BTN];

bool  buttonState[NUM_BTN] = {false,false,false,false};
bool  buttonPrev[NUM_BTN]  = {false,false,false,false};

// Blink engine
bool blinkPhase=false;
uint32_t lastBlinkToggle=0;
const uint32_t blinkPeriodMs=500;

// ================== Persistence (LittleFS) ==================
// Legacy V5/V6 kept so we can migrate forward to V7 (current)

struct PersistConfigV5 {
  uint32_t magic;  uint16_t version;  uint16_t size;

  InCfg   diCfg[NUM_DI];
  RlyCfg  rlyCfg[NUM_RLY];
  bool    desiredRelay[NUM_RLY]; // legacy -> maps to localDesiredRelay

  uint8_t  mb_address;
  uint32_t mb_baud;

  uint32_t flowPPL[NUM_DI];
  float    flowCalibRate[NUM_DI];
  float    flowCalibAccum[NUM_DI];
  uint32_t flowCounterBase[NUM_DI];

  bool     heatEnabled[NUM_DI];
  uint64_t heatAddrA[NUM_DI];
  uint64_t heatAddrB[NUM_DI];
  float    heatCp[NUM_DI];
  float    heatRho[NUM_DI];
  float    heatCalib[NUM_DI];
  double   heatEnergyJ[NUM_DI];

  uint32_t crc32;
} __attribute__((packed));

// V6: add LED/BTN config
struct PersistConfigV6 {
  uint32_t magic;  uint16_t version;  uint16_t size;

  InCfg   diCfg[NUM_DI];
  RlyCfg  rlyCfg[NUM_RLY];
  bool    desiredRelay[NUM_RLY]; // legacy -> maps to localDesiredRelay

  uint8_t  mb_address;
  uint32_t mb_baud;

  uint32_t flowPPL[NUM_DI];
  float    flowCalibRate[NUM_DI];
  float    flowCalibAccum[NUM_DI];
  uint32_t flowCounterBase[NUM_DI];

  bool     heatEnabled[NUM_DI];
  uint64_t heatAddrA[NUM_DI];
  uint64_t heatAddrB[NUM_DI];
  float    heatCp[NUM_DI];
  float    heatRho[NUM_DI];
  float    heatCalib[NUM_DI];
  double   heatEnergyJ[NUM_DI];

  // LEDs + Buttons
  LedCfg   ledCfg[NUM_LED];
  BtnCfg   btnCfg[NUM_BTN];

  uint32_t crc32;
} __attribute__((packed));

// V7: add relay control mode (Local/Modbus) and store localDesiredRelay
struct PersistConfigV7 {
  uint32_t magic;  uint16_t version;  uint16_t size;

  InCfg   diCfg[NUM_DI];
  RlyCfg  rlyCfg[NUM_RLY];
  bool    localDesiredRelay[NUM_RLY];

  uint8_t  mb_address;
  uint32_t mb_baud;

  uint32_t flowPPL[NUM_DI];
  float    flowCalibRate[NUM_DI];
  float    flowCalibAccum[NUM_DI];
  uint32_t flowCounterBase[NUM_DI];

  bool     heatEnabled[NUM_DI];
  uint64_t heatAddrA[NUM_DI];
  uint64_t heatAddrB[NUM_DI];
  float    heatCp[NUM_DI];
  float    heatRho[NUM_DI];
  float    heatCalib[NUM_DI];
  double   heatEnergyJ[NUM_DI];

  // LEDs + Buttons
  LedCfg   ledCfg[NUM_LED];
  BtnCfg   btnCfg[NUM_BTN];

  // NEW: per-relay control selection
  uint8_t  relayCtrlMode[NUM_RLY]; // 0=Local,1=Modbus

  uint32_t crc32;
} __attribute__((packed));

using PersistConfig = PersistConfigV7;

static const uint32_t CFG_MAGIC       = 0x31524C57UL; // 'WLR1'
static const uint16_t CFG_VERSION_V5  = 0x0005;
static const uint16_t CFG_VERSION_V6  = 0x0006;
static const uint16_t CFG_VERSION     = 0x0007;  // <— bumped to V7
static const char*    CFG_PATH        = "/cfg.bin";

// ---- 1-Wire DB ----
static const char* ONEWIRE_DB_PATH = "/ow_sensors.json";
static const size_t MAX_OW_SENSORS = 32;
struct OwRec { uint64_t addr; String name; };
OwRec  g_owDb[MAX_OW_SENSORS];
size_t g_owCount = 0;

// === 1-Wire last-good cache & error tracking ===
double   owLastGoodTemp[MAX_OW_SENSORS];
uint32_t owLastGoodMs[MAX_OW_SENSORS];
uint32_t owErrCount[MAX_OW_SENSORS];
const uint32_t OW_FAIL_HIDE_MS = 15000;

volatile bool   cfgDirty       = false;
uint32_t        lastCfgTouchMs = 0;
const uint32_t  CFG_AUTOSAVE_MS = 1500;

// ================== Utils ==================
uint32_t crc32_update(uint32_t crc, const uint8_t* data, size_t len) {
  crc = ~crc;
  while (len--) {
    crc ^= *data++;
    for (uint8_t k=0;k<8;k++) crc = (crc >> 1) ^ (0xEDB88320UL & (-(int32_t)(crc & 1)));
  }
  return ~crc;
}
inline bool timeAfter32(uint32_t a, uint32_t b) { return (int32_t)(a - b) >= 0; }

String hex64(uint64_t v) {
  char buf[19]; buf[0]='0'; buf[1]='x';
  for (int i=0;i<16;i++) { uint8_t nib=(v >> ((15-i)*4)) & 0xF; buf[2+i]=(nib<10)?('0'+nib):('A'+(nib-10)); }
  buf[18]=0; return String(buf);
}
uint64_t romBytesToU64(const uint8_t *addr) { uint64_t v=0; for (int i=7;i>=0;i--) { v = (v<<8) | addr[i]; } return v; }
bool parseHex64(const char* s, uint64_t &out) {
  if (!s) return false;
  String str(s); str.trim();
  if (str.startsWith("0x") || str.startsWith("0X")) str.remove(0,2);
  if (str.length()==0 || str.length()>16) return false;
  uint64_t v=0;
  for (uint16_t i=0;i<str.length();i++){
    char c=str[i]; uint8_t x;
    if (c>='0'&&c<='9') x=c-'0';
    else if (c>='a'&&c<='f') x=10+(c-'a');
    else if (c>='A'&&c<='F') x=10+(c-'A');
    else return false;
    v = (v<<4) | x;
  }
  out=v; return true;
}
bool parseDec64(const char* s, uint64_t &out) {
  if (!s) return false;
  while (*s==' '||*s=='\t') s++;
  if (!*s) return false;
  uint64_t v=0; bool any=false;
  for (const char* p=s; *p; ++p) {
    if (*p<'0'||*p>'9') return false;
    v = v*10 + uint64_t(*p-'0'); any=true;
  }
  if (!any) return false;
  out=v; return true;
}
static bool jsonGetStr(JSONVar obj, const char* key, String &out) {
  if (!obj.hasOwnProperty(key)) return false;
  JSONVar v = obj[key];
  String t = JSON.typeof(v);
  if (t=="string") {
    const char* s = (const char*)v;
    if (!s) return false;
    out = String(s); out.trim();
    return out.length()>0;
  }
  return false;
}
static bool jsonGetDouble(JSONVar obj, const char* key, double &out) {
  if (!obj.hasOwnProperty(key)) return false;
  JSONVar v=obj[key];
  String t=JSON.typeof(v);
  if (t=="number") { out = (double)v; return true; }
  if (t=="string") { const char* s=(const char*)v; if(!s) return false; char* e=nullptr; out = strtod(s,&e); return e && e!=s; }
  return false;
}
static bool jsonGetAddr64(JSONVar obj, const char* baseKey, uint64_t &out) {
  String s;
  if (jsonGetStr(obj,"rom_hex", s) || jsonGetStr(obj,"addr", s) ||
      jsonGetStr(obj,"address", s) || jsonGetStr(obj,"rom", s)) {
    if (parseHex64(s.c_str(), out)) return true;
  }
  if (obj.hasOwnProperty("addr_hi") && obj.hasOwnProperty("addr_lo")) {
    JSONVar vhi = obj["addr_hi"], vlo = obj["addr_lo"];
    uint32_t hi=0, lo=0;
    String th = JSON.typeof(vhi), tl = JSON.typeof(vlo);
    if (th=="number") hi = (uint32_t)(double)vhi; else if (th=="string") { uint64_t tmp; if (parseDec64((const char*)vhi, tmp)) hi=(uint32_t)tmp; }
    if (tl=="number") lo = (uint32_t)(double)vlo; else if (tl=="string") { uint64_t tmp; if (parseDec64((const char*)vlo, tmp)) lo=(uint32_t)tmp; }
    out = ( (uint64_t)hi << 32 ) | (uint64_t)lo;
    if (hi!=0 || lo!=0) return true;
  }
  if (jsonGetStr(obj,"addr_u64_str", s) || jsonGetStr(obj,"rom_u64_str", s)) {
    if (parseDec64(s.c_str(), out)) return true;
  }
  if (obj.hasOwnProperty("addr_u64")) {
    JSONVar v = obj["addr_u64"];
    if (JSON.typeof(v)=="number") {
      double d = (double)v;
      const double MAX_SAFE = 9007199254740991.0;
      if (d >= 0.0 && d <= MAX_SAFE && d == floor(d)) { out = (uint64_t)d; return true; }
      else { WebSerial.send("message","WARNING: addr_u64 not safely representable; ignoring numeric path."); }
    } else if (JSON.typeof(v)=="string") {
      if (parseDec64((const char*)v, out)) return true;
    }
  }
  if (baseKey && jsonGetStr(obj, baseKey, s)) {
    if (parseDec64(s.c_str(), out)) return true;
  }
  return false;
}

// ======= NEW: position helpers =======
inline uint64_t owdbAddrAtPos(uint32_t pos){
  if (pos == 0) return 0;
  size_t i = (pos - 1);
  if (i < g_owCount) return g_owDb[i].addr;
  return 0;
}
static bool jsonGetPos(JSONVar obj, const char* key, uint32_t &out){
  if (!obj.hasOwnProperty(key)) return false;
  JSONVar v = obj[key];
  String t = JSON.typeof(v);
  if (t=="number") {
    double d = (double)v;
    if (d >= 0.0 && d == floor(d)) { out = (uint32_t)d; return true; }
  } else if (t=="string") {
    const char* s = (const char*)v;
    if (!s) return false;
    uint64_t tmp;
    if (parseDec64(s, tmp)) { out = (uint32_t)tmp; return true; }
  }
  return false;
}

// ================== Defaults / persist ==================
void setDefaults() {
  for (int i=0;i<NUM_DI;i++)  diCfg[i]  = (InCfg){ true, false, 0, 0, IT_WATER };
  for (int i=0;i<NUM_RLY;i++) rlyCfg[i] = (RlyCfg){ true, false };

  for (int i=0;i<NUM_RLY;i++){
    localDesiredRelay[i]=false; modbusDesiredRelay[i]=false; rlyPulseUntil[i]=0;
    rlyCtrlMode[i]=RCTRL_MODBUS; // Default to Modbus control for ESPHome integration
  }

  for (int i=0;i<NUM_DI;i++){
    diCounter[i]=0; diLastEdgeMs[i]=0;
    flowPulsesPerL[i] = 450;
    flowCalibRate[i]  = 1.0f;
    flowCalibAccum[i] = 1.0f;
    flowCounterBase[i]= 0;
    flowRateLmin[i]   = 0.0f;
    lastPulseSnapshot[i] = 0;

    heatEnabled[i]=false;
    heatAddrA[i]=0; heatAddrB[i]=0;
    heatCp[i]=4186.0f; heatRho[i]=1.0f; heatCalib[i]=1.0f;
    heatTA[i]=NAN; heatTB[i]=NAN; heatDT[i]=0.0;
    heatPowerW[i]=0.0; heatEnergyJ[i]=0.0;
  }
  for (size_t i=0;i<MAX_OW_SENSORS;i++){ owLastGoodTemp[i]=NAN; owLastGoodMs[i]=0; owErrCount[i]=0; }

  // LEDs default: mirror relays solid
  for (int i=0;i<NUM_LED;i++){
    ledCfg[i].mode = 0; // solid
    ledCfg[i].source = (i==0)?LEDSRC_R1 : (i==1)?LEDSRC_R2 : LEDSRC_NONE;
  }
  // Buttons default: toggle relays 1/2, rest none
  btnCfg[0].action = BTN_TOGGLE_R1;
  btnCfg[1].action = BTN_TOGGLE_R2;
  btnCfg[2].action = BTN_NONE;
  btnCfg[3].action = BTN_NONE;

  oneWireBusy=false; nextOneWireConvertMs=millis();
  nextRateTickMs = millis() + 1000;
  g_mb_address=3; g_mb_baud=19200;

}

void captureToPersist(PersistConfig &pc){
  pc.magic=CFG_MAGIC; pc.version=CFG_VERSION; pc.size=sizeof(PersistConfig);
  memcpy(pc.diCfg,diCfg,sizeof(diCfg)); memcpy(pc.rlyCfg,rlyCfg,sizeof(rlyCfg));
  memcpy(pc.localDesiredRelay,localDesiredRelay,sizeof(localDesiredRelay));
  pc.mb_address=g_mb_address; pc.mb_baud=g_mb_baud;
  for (int i=0;i<NUM_DI;i++){
    pc.flowPPL[i]        = flowPulsesPerL[i];
    pc.flowCalibRate[i]  = flowCalibRate[i];
    pc.flowCalibAccum[i] = flowCalibAccum[i];
    pc.flowCounterBase[i]= flowCounterBase[i];

    pc.heatEnabled[i] = heatEnabled[i];
    pc.heatAddrA[i]   = heatAddrA[i];
    pc.heatAddrB[i]   = heatAddrB[i];
    pc.heatCp[i]      = heatCp[i];
    pc.heatRho[i]     = heatRho[i];
    pc.heatCalib[i]   = heatCalib[i];
    pc.heatEnergyJ[i] = heatEnergyJ[i];
  }
  // leds + buttons
  memcpy(pc.ledCfg, ledCfg, sizeof(ledCfg));
  memcpy(pc.btnCfg, btnCfg, sizeof(btnCfg));

  // relay control mode
  for (int i=0;i<NUM_RLY;i++) pc.relayCtrlMode[i] = (uint8_t)rlyCtrlMode[i];

  pc.crc32=0;
  pc.crc32=crc32_update(0,(const uint8_t*)&pc,sizeof(PersistConfig));
}

// ----- migrate V5 -> RAM -----
bool applyFromPersistV5(const PersistConfigV5 &pc){
  if (pc.magic!=CFG_MAGIC || pc.size!=sizeof(PersistConfigV5)) return false;
  PersistConfigV5 tmp=pc; uint32_t crc=tmp.crc32; tmp.crc32=0;
  if (crc32_update(0,(const uint8_t*)&tmp,sizeof(tmp))!=crc) return false;
  if (pc.version!=CFG_VERSION_V5) return false;

  memcpy(diCfg, pc.diCfg, sizeof(diCfg));
  memcpy(rlyCfg, pc.rlyCfg, sizeof(rlyCfg));
  // legacy desired -> localDesired
  memcpy(localDesiredRelay, pc.desiredRelay, sizeof(localDesiredRelay));
  modbusDesiredRelay[0]=modbusDesiredRelay[1]=false;
  rlyCtrlMode[0]=rlyCtrlMode[1]=RCTRL_LOCAL;

  g_mb_address=pc.mb_address; g_mb_baud=pc.mb_baud;

  for (int i=0;i<NUM_DI;i++){
    flowPulsesPerL[i] = pc.flowPPL[i] ? pc.flowPPL[i] : 1;
    flowCalibRate[i]  = (isnan(pc.flowCalibRate[i]) || pc.flowCalibRate[i]<=0) ? 1.0f : pc.flowCalibRate[i];
    flowCalibAccum[i] = (isnan(pc.flowCalibAccum[i])|| pc.flowCalibAccum[i]<=0)? 1.0f : pc.flowCalibAccum[i];
    flowCounterBase[i]= pc.flowCounterBase[i];
    flowRateLmin[i]   = 0.0f;
    lastPulseSnapshot[i] = 0;

    heatEnabled[i] = pc.heatEnabled[i];
    heatAddrA[i]   = pc.heatAddrA[i];
    heatAddrB[i]   = pc.heatAddrB[i];
    heatCp[i]      = (isnan(pc.heatCp[i]) || pc.heatCp[i]<=0) ? 4186.0f : pc.heatCp[i];
    heatRho[i]     = (isnan(pc.heatRho[i])|| pc.heatRho[i]<=0)? 1.0f    : pc.heatRho[i];
    heatCalib[i]   = (isnan(pc.heatCalib[i])||pc.heatCalib[i]<=0)?1.0f  : pc.heatCalib[i];
    heatEnergyJ[i] = isfinite(pc.heatEnergyJ[i]) ? pc.heatEnergyJ[i] : 0.0;
  }

  // Seed LED/BTN defaults for migration
  for (int i=0;i<NUM_LED;i++){ ledCfg[i].mode=0; ledCfg[i].source=(i==0)?LEDSRC_R1:(i==1)?LEDSRC_R2:LEDSRC_NONE; }
  btnCfg[0].action=BTN_TOGGLE_R1; btnCfg[1].action=BTN_TOGGLE_R2; btnCfg[2].action=BTN_NONE; btnCfg[3].action=BTN_NONE;

  nextRateTickMs = millis() + 1000;
  return true;
}

// ----- migrate V6 -> RAM (adds ctrl mode; map desired->local) -----
bool applyFromPersistV6(const PersistConfigV6 &pc){
  if (pc.magic!=CFG_MAGIC || pc.size!=sizeof(PersistConfigV6)) return false;
  PersistConfigV6 tmp=pc; uint32_t crc=tmp.crc32; tmp.crc32=0;
  if (crc32_update(0,(const uint8_t*)&tmp,sizeof(tmp))!=crc) return false;
  if (pc.version!=CFG_VERSION_V6) return false;

  memcpy(diCfg, pc.diCfg, sizeof(diCfg));
  memcpy(rlyCfg, pc.rlyCfg, sizeof(rlyCfg));
  memcpy(localDesiredRelay, pc.desiredRelay, sizeof(localDesiredRelay));
  modbusDesiredRelay[0]=modbusDesiredRelay[1]=false;
  rlyCtrlMode[0]=rlyCtrlMode[1]=RCTRL_LOCAL;

  g_mb_address=pc.mb_address; g_mb_baud=pc.mb_baud;

  for (int i=0;i<NUM_DI;i++){
    flowPulsesPerL[i] = pc.flowPPL[i] ? pc.flowPPL[i] : 1;
    flowCalibRate[i]  = (isnan(pc.flowCalibRate[i]) || pc.flowCalibRate[i]<=0) ? 1.0f : pc.flowCalibRate[i];
    flowCalibAccum[i] = (isnan(pc.flowCalibAccum[i])|| pc.flowCalibAccum[i]<=0)? 1.0f : pc.flowCalibAccum[i];
    flowCounterBase[i]= pc.flowCounterBase[i];
    flowRateLmin[i]   = 0.0f;
    lastPulseSnapshot[i] = 0;

    heatEnabled[i] = pc.heatEnabled[i];
    heatAddrA[i]   = pc.heatAddrA[i];
    heatAddrB[i]   = pc.heatAddrB[i];
    heatCp[i]      = (isnan(pc.heatCp[i]) || pc.heatCp[i]<=0) ? 4186.0f : pc.heatCp[i];
    heatRho[i]     = (isnan(pc.heatRho[i])|| pc.heatRho[i]<=0)? 1.0f    : pc.heatRho[i];
    heatCalib[i]   = (isnan(pc.heatCalib[i])||pc.heatCalib[i]<=0)?1.0f  : pc.heatCalib[i];
    heatEnergyJ[i] = isfinite(pc.heatEnergyJ[i]) ? pc.heatEnergyJ[i] : 0.0;
  }
  memcpy(ledCfg, pc.ledCfg, sizeof(ledCfg));
  memcpy(btnCfg, pc.btnCfg, sizeof(btnCfg));

  nextRateTickMs = millis() + 1000;
  return true;
}

bool applyFromPersist(const PersistConfig &pc){
  if (pc.magic!=CFG_MAGIC || pc.size!=sizeof(PersistConfig)) return false;
  PersistConfig tmp=pc; uint32_t crc=tmp.crc32; tmp.crc32=0;
  if (crc32_update(0,(const uint8_t*)&tmp,sizeof(tmp))!=crc) return false;
  if (pc.version!=CFG_VERSION) return false;

  memcpy(diCfg, pc.diCfg, sizeof(diCfg));
  memcpy(rlyCfg, pc.rlyCfg, sizeof(rlyCfg));
  memcpy(localDesiredRelay, pc.localDesiredRelay, sizeof(localDesiredRelay));
  modbusDesiredRelay[0]=modbusDesiredRelay[1]=false;

  g_mb_address=pc.mb_address; g_mb_baud=pc.mb_baud;

  for (int i=0;i<NUM_DI;i++){
    flowPulsesPerL[i] = pc.flowPPL[i] ? pc.flowPPL[i] : 1;
    flowCalibRate[i]  = (isnan(pc.flowCalibRate[i]) || pc.flowCalibRate[i]<=0) ? 1.0f : pc.flowCalibRate[i];
    flowCalibAccum[i] = (isnan(pc.flowCalibAccum[i])|| pc.flowCalibAccum[i]<=0)? 1.0f : pc.flowCalibAccum[i];
    flowCounterBase[i]= pc.flowCounterBase[i];
    flowRateLmin[i]   = 0.0f;
    lastPulseSnapshot[i] = 0;

    heatEnabled[i] = pc.heatEnabled[i];
    heatAddrA[i]   = pc.heatAddrA[i];
    heatAddrB[i]   = pc.heatAddrB[i];
    heatCp[i]      = (isnan(pc.heatCp[i]) || pc.heatCp[i]<=0) ? 4186.0f : pc.heatCp[i];
    heatRho[i]     = (isnan(pc.heatRho[i])|| pc.heatRho[i]<=0)? 1.0f    : pc.heatRho[i];
    heatCalib[i]   = (isnan(pc.heatCalib[i])||pc.heatCalib[i]<=0)?1.0f  : pc.heatCalib[i];
    heatEnergyJ[i] = isfinite(pc.heatEnergyJ[i]) ? pc.heatEnergyJ[i] : 0.0;
  }
  memcpy(ledCfg, pc.ledCfg, sizeof(ledCfg));
  memcpy(btnCfg, pc.btnCfg, sizeof(btnCfg));
  for (int i=0;i<NUM_RLY;i++) rlyCtrlMode[i] = (pc.relayCtrlMode[i]==1)?RCTRL_MODBUS:RCTRL_LOCAL;

  nextRateTickMs = millis() + 1000;
  return true;
}
bool saveConfigFS(){
  PersistConfig pc{}; captureToPersist(pc);
  File f=LittleFS.open(CFG_PATH,"w");
  if(!f){ WebSerial.send("message","save: open failed"); return false; }
  size_t n=f.write((const uint8_t*)&pc,sizeof(pc)); f.flush(); f.close();
  if(n!=sizeof(pc)){ WebSerial.send("message","save: short write "); return false; }
  File r=LittleFS.open(CFG_PATH,"r"); if(!r){ WebSerial.send("message","save: reopen failed"); return false; }
  if((size_t)r.size()!=sizeof(PersistConfig)){ WebSerial.send("message","save: size mismatch after write"); r.close(); return false; }
  PersistConfig back{}; size_t nr=r.read((uint8_t*)&back,sizeof(back)); r.close();
  if(nr!=sizeof(back)){ WebSerial.send("message","save: short readback"); return false; }
  PersistConfig tmp=back; uint32_t crc=tmp.crc32; tmp.crc32=0;
  if (crc32_update(0,(const uint8_t*)&tmp,sizeof(tmp))!=crc){ WebSerial.send("message","save: CRC verify failed"); return false; }
  return true;
}
bool loadConfigFS(){
  File f=LittleFS.open(CFG_PATH,"r"); if(!f){ WebSerial.send("message","load: open failed"); return false; }
  size_t sz=f.size();
  if (sz==sizeof(PersistConfigV5)){
    PersistConfigV5 pc{}; size_t n=f.read((uint8_t*)&pc,sizeof(pc)); f.close();
    if(n!=sizeof(pc)){ WebSerial.send("message","load: short read (v5)"); return false; }
    if(!applyFromPersistV5(pc)){ WebSerial.send("message","load: v5 magic/version/crc mismatch"); return false; }
    WebSerial.send("message","Loaded legacy config v5 → migrated to v7 (LED/BTN defaults + Local control).");
    return true;
  } else if (sz==sizeof(PersistConfigV6)){
    PersistConfigV6 pc{}; size_t n=f.read((uint8_t*)&pc,sizeof(pc)); f.close();
    if(n!=sizeof(pc)){ WebSerial.send("message","load: short read (v6)"); return false; }
    if(!applyFromPersistV6(pc)){ WebSerial.send("message","load: v6 magic/version/crc mismatch"); return false; }
    WebSerial.send("message","Loaded legacy config v6 → migrated to v7 (relay control mode defaults to Local).");
    return true;
  } else if (sz==sizeof(PersistConfig)){
    PersistConfig pc{}; size_t n=f.read((uint8_t*)&pc,sizeof(pc)); f.close();
    if(n!=sizeof(pc)){ WebSerial.send("message","load: short read (v7)"); return false; }
    if(!applyFromPersist(pc)){ WebSerial.send("message","load: v7 magic/version/crc mismatch"); return false; }
    return true;
  } else {
    WebSerial.send("message",String("load: unexpected size ")+sz); f.close(); return false;
  }
}
bool initFilesystemAndConfig(){
  if(!LittleFS.begin()){
    WebSerial.send("message","LittleFS mount failed. Formatting…");
    if(!LittleFS.format() || !LittleFS.begin()){ WebSerial.send("message","FATAL: FS mount/format failed"); return false; }
  }
  if (loadConfigFS()){ WebSerial.send("message","Config loaded from flash"); return true; }
  WebSerial.send("message","No valid config. Using defaults."); setDefaults();
  if (saveConfigFS()){ WebSerial.send("message","Defaults saved"); return true; }
  WebSerial.send("message","First save failed. Formatting FS…");
  if(!LittleFS.format() || !LittleFS.begin()){ WebSerial.send("message","FATAL: FS format failed"); return false; }
  setDefaults();
  if (saveConfigFS()){ WebSerial.send("message","FS formatted and config saved"); return true; }
  WebSerial.send("message","FATAL: save still failing after format"); return false;
}

// ================== SFINAE helper ==================
template <class M>
inline auto setSlaveIdIfAvailable(M& m, uint8_t id)
  -> decltype(std::declval<M&>().setSlaveId(uint8_t{}), void()) { m.setSlaveId(id); }
inline void setSlaveIdIfAvailable(...){}

// ================== Modbus maps ==================
// ISTS (Input Status) - kept for backward compatibility (FC02)
enum : uint16_t { ISTS_DI_BASE=1, ISTS_RLY_BASE=60 };
enum : uint16_t { ISTS_LED_BASE=90, ISTS_BTN_BASE=100 };

// Command Coils (FC05/FC01) - Maintained (switched) coils from ESPHome
enum : uint16_t {
  CMD_RLY_STATE_BASE=200,    // 2 coils: RLY1, RLY2 state (maintained, not pulses)
  CMD_DI_ENABLE_BASE=220,    // 5 coils: DI1..DI5 enable state (maintained, not pulses)
  CMD_CNT_RST_BASE=340       // 5 coils: Counter reset (pulse, cleared after use)
};

// ===== Unified Holding Register Map (FC03) - Single Address Space =====
// All data accessible via FC03 Read Holding Registers
enum : uint16_t {
  // ISTS mirrors (as UINT16: 0 or 1)
  HREG_DI_BASE    = 1,   // 5 regs: DI1-DI5 states
  HREG_RLY_BASE   = 60,  // 2 regs: RLY1-RLY2 states
  HREG_LED_BASE   = 90,  // 4 regs: LED1-LED4 states
  HREG_BTN_BASE   = 100, // 4 regs: BTN1-BTN4 states
  
  // Flow/Heat/1-Wire (continuous from 104)
  HREG_FLOW_RATE_BASE   = 104, // 5×(U32)  L/min ×1000  (2 regs each) = 10 regs
  HREG_FLOW_ACCUM_BASE  = 114, // 5×(U32)  L ×1000      (2 regs each) = 10 regs
  HREG_HEAT_POWER_BASE  = 124, // 5×(S32)  W            (2 regs each) = 10 regs
  HREG_HEAT_EN_WH_BASE  = 134, // 5×(U32)  Wh ×1000     (2 regs each) = 10 regs
  HREG_HEAT_DT_BASE     = 144, // 5×(S32)  °C ×1000     (2 regs each) = 10 regs
  HREG_OW_TEMP_BASE     = 154  // 10×(S32) °C ×1000     (2 regs each) = 20 regs
  // Total: 1-173 (continuous)
};

// ================== 1-Wire DB helpers ==================
int owdbIndexOf(uint64_t addr){ for(size_t i=0;i<g_owCount;i++) if(g_owDb[i].addr==addr) return (int)i; return -1; }
inline uint32_t owdbPosOf(uint64_t addr){
  int idx = owdbIndexOf(addr);
  return (idx >= 0) ? (uint32_t)(idx + 1) : 0;
}
JSONVar owdbBuildArray(){
  JSONVar arr;
  for(size_t i=0;i<g_owCount;i++){
    JSONVar o;
    o["pos"]  = (double)(i + 1);
    o["addr"] = hex64(g_owDb[i].addr);
    o["name"] = g_owDb[i].name.c_str();
    arr[i] = o;
  }
  return arr;
}
JSONVar owdbBuildIndexMap(){
  JSONVar map;
  for(size_t i=0;i<g_owCount;i++){
    map[ hex64(g_owDb[i].addr) ] = (double)(i + 1);
  }
  return map;
}
bool owdbSave(){
  String json=JSON.stringify(owdbBuildArray());
  File f=LittleFS.open(ONEWIRE_DB_PATH,"w");
  if(!f){ WebSerial.send("message","owdb: save open failed"); return false; }
  size_t n=f.print(json); f.flush(); f.close();
  if(n!=json.length()){ WebSerial.send("message","owdb: short write"); return false; }
  return true;
}
bool owdbLoad(){
  g_owCount=0;
  File f=LittleFS.open(ONEWIRE_DB_PATH,"r"); if(!f){ return false; }
  String s; while(f.available()) s+=(char)f.read(); f.close();
  JSONVar arr=JSON.parse(s); if(JSON.typeof(arr)!="array"){ WebSerial.send("message","owdb: invalid JSON, ignoring"); return false; }
  for (unsigned i=0; i<arr.length() && g_owCount<MAX_OW_SENSORS; i++){
    const char* a=(const char*)arr[i]["addr"]; const char* n=(const char*)arr[i]["name"];
    if(!a||!n) continue; uint64_t v; if(!parseHex64(a,v)) continue;
    g_owDb[g_owCount].addr=v; g_owDb[g_owCount].name=String(n); g_owCount++;
  }
  for (size_t i=0;i<MAX_OW_SENSORS;i++){ owLastGoodTemp[i]=NAN; owLastGoodMs[i]=0; owErrCount[i]=0; }
  return true;
}
bool owdbAddOrUpdate(uint64_t addr, const char* name){
  if (!name) return false;
  int idx = owdbIndexOf(addr);
  if (idx >= 0) {
    g_owDb[idx].name = String(name);
    bool ok = owdbSave();
    if (ok) owdbSendList();
    return ok;
  }
  if (g_owCount >= MAX_OW_SENSORS) return false;
  g_owDb[g_owCount].addr = addr;
  g_owDb[g_owCount].name = String(name);
  g_owCount++;
  bool ok = owdbSave();
  if (ok) owdbSendList();
  return ok;
}
bool owdbRemove(uint64_t addr){
  int idx = owdbIndexOf(addr);
  if (idx < 0) return false;
  for (size_t i = idx + 1; i < g_owCount; i++) g_owDb[i - 1] = g_owDb[i];
  g_owCount--;
  bool ok = owdbSave();
  if (ok) owdbSendList();
  return ok;
}
void owdbSendList(){
  WebSerial.send("onewireDb",    owdbBuildArray());
  WebSerial.send("onewireIndex", owdbBuildIndexMap());
}


// ================== Decls ==================
void applyModbusSettings(uint8_t addr, uint32_t baud);
void handleValues(JSONVar values);
void handleUnifiedConfig(JSONVar obj);
void handleCommand(JSONVar obj);
void handleOneWire(JSONVar obj);
void sendAllEchoesOnce();
void processModbusCommands();
void applyActionToTarget(uint8_t target, uint8_t action, uint32_t now);
void doOneWireScan();
bool applyHeatCfgObjectToIndex(int idx, JSONVar o);

// ---- DS18B20 helpers ----
bool ds18b20StartConvertAll(){
  oneWire.reset();
  oneWire.skip();
  oneWire.write(0x44, 1);
  return true;
}
bool ds18b20ReadTempC(uint64_t rom, double &outC){
  if (!rom) return false;
  uint8_t addr[8]; uint64_t v=rom;
  for (int i=0;i<8;i++){ addr[i]=(uint8_t)(v & 0xFF); v >>= 8; }
  auto try_once = [&](double &tOut)->bool{
    if (!oneWire.reset()) return false;
    oneWire.select(addr);
    oneWire.write(0xBE);
    uint8_t data[9];
    for (int i=0;i<9;i++) data[i]=oneWire.read();
    if (OneWire::crc8(data,8) != data[8]) return false;
    int16_t raw = ((int16_t)data[1] << 8) | data[0];
    if (addr[0] == 0x10) {
      raw <<= 3;
      if (data[7] == 0x10) raw = (raw & 0xFFF0) + 12 - data[6];
    } else {
      uint8_t cfg = (data[4] & 0x60);
      if (cfg == 0x00)      raw &= ~0x7;
      else if (cfg == 0x20) raw &= ~0x3;
      else if (cfg == 0x40) raw &= ~0x1;
    }
    double t = (double)raw / 16.0;
    if (t == 85.0 || t < -55.0 || t > 125.0) return false;
    tOut = t;
    return true;
  };
  double t;
  if (try_once(t)) { outC=t; return true; }
  delayMicroseconds(200);
  if (try_once(t)) { outC=t; return true; }
  return false;
}
void publishOneWireTemps(){
  JSONVar owTemps, owErrs, owTempsList;
  uint32_t now = millis();
  for (size_t i=0; i<g_owCount; i++){
    bool fresh = (owLastGoodMs[i] != 0) && (now - owLastGoodMs[i] <= OW_FAIL_HIDE_MS);
    double t = fresh ? owLastGoodTemp[i] : NAN;
    String addrHex = hex64(g_owDb[i].addr);

    owTemps[addrHex] = isfinite(t) ? t : NAN;
    owErrs[addrHex]  = (double)owErrCount[i];

    JSONVar row;
    row["pos"]  = (double)(i+1);
    row["addr"] = addrHex;
    row["name"] = g_owDb[i].name.c_str();
    row["temp"] = isfinite(t) ? t : NAN;
    row["errs"] = (double)owErrCount[i];
    owTempsList[i] = row;
  }
  WebSerial.send("onewireTemps", owTemps);
  WebSerial.send("onewireErrs",  owErrs);
  WebSerial.send("onewireTempsList", owTempsList);
}
void buildCachedOwTemps(JSONVar &owTemps, JSONVar &owErrs){
  uint32_t now = millis();
  for (size_t i=0;i<g_owCount; i++){
    bool fresh = (owLastGoodMs[i] != 0) && (now - owLastGoodMs[i] <= OW_FAIL_HIDE_MS);
    double t = fresh ? owLastGoodTemp[i] : NAN;
    owTemps[hex64(g_owDb[i].addr)] = isfinite(t) ? t : NAN;
    owErrs[hex64(g_owDb[i].addr)]  = (double)owErrCount[i];
  }
}

// ================== Setup ==================
void setup(){
  Serial.begin(57600);

  for(uint8_t i=0;i<NUM_DI;i++)   pinMode(DI_PINS[i], INPUT);
  for(uint8_t i=0;i<NUM_RLY;i++){ pinMode(RELAY_PINS[i], OUTPUT); digitalWrite(RELAY_PINS[i], LOW); }
  pinMode(ONEWIRE_PIN, INPUT_PULLUP);

  for(uint8_t i=0;i<NUM_LED;i++){ pinMode(LED_PINS[i], OUTPUT); digitalWrite(LED_PINS[i], LOW); }
  for(uint8_t i=0;i<NUM_BTN;i++) pinMode(BTN_PINS[i], INPUT); // HIGH=pressed

  setDefaults();

  if(!initFilesystemAndConfig()){ WebSerial.send("message","FATAL: Filesystem/config init failed"); }
  if(owdbLoad()) WebSerial.send("message","1-Wire DB loaded from flash");
  else           WebSerial.send("message","1-Wire DB missing/invalid (will create on first save)");


  publishOneWireTemps();

  Serial2.setTX(TX2); Serial2.setRX(RX2);
  Serial2.begin(g_mb_baud); mb.config(g_mb_baud); setSlaveIdIfAvailable(mb, g_mb_address);
  mb.setAdditionalServerData("WLD-521-R1");

  // ISTS (Input Status) - kept for backward compatibility (FC02)
  for(uint16_t i=0;i<NUM_DI;i++)  mb.addIsts(ISTS_DI_BASE + i);
  for(uint16_t i=0;i<NUM_RLY;i++) mb.addIsts(ISTS_RLY_BASE + i);
  for(uint16_t i=0;i<NUM_LED;i++) mb.addIsts(ISTS_LED_BASE + i);
  for(uint16_t i=0;i<NUM_BTN;i++) mb.addIsts(ISTS_BTN_BASE + i);

  // Command Coils (FC05/FC01) - Maintained (switched) coils from ESPHome
  // Relay state coils (maintained - ESPHome can set ON/OFF directly)
  for(uint16_t i=0;i<NUM_RLY;i++){
    mb.addCoil(CMD_RLY_STATE_BASE + i);
    mb.setCoil(CMD_RLY_STATE_BASE + i, false); // Initialize to OFF
  }
  // DI enable coils (maintained - ESPHome can enable/disable inputs directly)
  for(uint16_t i=0;i<NUM_DI;i++){
    mb.addCoil(CMD_DI_ENABLE_BASE + i);
    mb.setCoil(CMD_DI_ENABLE_BASE + i, true); // Initialize to enabled (default)
  }
  // Counter reset coils (pulse - cleared after use)
  for(uint16_t i=0;i<NUM_DI;i++){
    mb.addCoil(CMD_CNT_RST_BASE + i);
    mb.setCoil(CMD_CNT_RST_BASE + i, false);
  }

  // ===== Unified Holding Register Map (FC03) - Single Continuous Address Space =====
  // ISTS mirrors (as UINT16: 0 or 1)
  for(uint16_t i=0;i<NUM_DI;i++)  mb.addHreg(HREG_DI_BASE + i, 0);
  for(uint16_t i=0;i<NUM_RLY;i++) mb.addHreg(HREG_RLY_BASE + i, 0);
  for(uint16_t i=0;i<NUM_LED;i++) mb.addHreg(HREG_LED_BASE + i, 0);
  for(uint16_t i=0;i<NUM_BTN;i++) mb.addHreg(HREG_BTN_BASE + i, 0);

  // Flow/Heat/1-Wire (continuous from 104)
  for (uint8_t i=0;i<NUM_DI;i++){
    uint16_t b1 = HREG_FLOW_RATE_BASE  + (i*2);
    uint16_t b2 = HREG_FLOW_ACCUM_BASE + (i*2);
    mb.addHreg(b1+0,0); mb.addHreg(b1+1,0);
    mb.addHreg(b2+0,0); mb.addHreg(b2+1,0);
  }
  for (uint8_t i=0;i<NUM_DI;i++){
    uint16_t p = HREG_HEAT_POWER_BASE + (i*2);
    uint16_t e = HREG_HEAT_EN_WH_BASE + (i*2);
    uint16_t d = HREG_HEAT_DT_BASE    + (i*2);
    mb.addHreg(p+0,0); mb.addHreg(p+1,0);
    mb.addHreg(e+0,0); mb.addHreg(e+1,0);
    mb.addHreg(d+0,0); mb.addHreg(d+1,0);
  }

  // 1-Wire quick telemetry (first 10 sensors)
  for (uint8_t i=0;i<10;i++){
    uint16_t b = HREG_OW_TEMP_BASE + (i*2);
    mb.addHreg(b+0,0); mb.addHreg(b+1,0);
  }

  modbusStatus["address"]=g_mb_address; modbusStatus["baud"]=g_mb_baud; modbusStatus["state"]=0;

  WebSerial.on("values",  handleValues);
  WebSerial.on("Config",  handleUnifiedConfig);
  WebSerial.on("command", handleCommand);
  WebSerial.on("onewire", handleOneWire);

  WebSerial.send("message","Boot OK (Flow + Heat + LEDs+Buttons + Local/Modbus relay control).");
  sendAllEchoesOnce();
}

// ================== Command handlers ==================
void applyModbusSettings(uint8_t addr, uint32_t baud){
  if ((uint32_t)modbusStatus["baud"]!=baud){ Serial2.end(); Serial2.begin(baud); mb.config(baud); }
  setSlaveIdIfAvailable(mb, addr); g_mb_address=addr; g_mb_baud=baud;
  modbusStatus["address"]=g_mb_address; modbusStatus["baud"]=g_mb_baud;
}
void handleValues(JSONVar values){
  int addr=(int)values["mb_address"]; int baud=(int)values["mb_baud"];
  addr=constrain(addr,1,255); baud=constrain(baud,9600,115200);
  applyModbusSettings((uint8_t)addr,(uint32_t)baud);
  WebSerial.send("message","Modbus configuration updated");
  cfgDirty=true; lastCfgTouchMs=millis();
}

// ===== apply heat config (posA/posB or explicit addresses) =====
bool applyHeatCfgObjectToIndex(int idx, JSONVar o){
  if (idx < 0 || idx >= (int)NUM_DI) return false;
  if (o.hasOwnProperty("enabled"))
    heatEnabled[idx] = (bool)o["enabled"];

  bool setA = false, setB = false;
  uint32_t posA = 0, posB = 0;

  if (jsonGetPos(o, "posA", posA) || jsonGetPos(o, "addrA_pos", posA)) {
    uint64_t a = owdbAddrAtPos(posA);
    heatAddrA[idx] = a; setA = true;
    WebSerial.send("message", String("Heat: DI")+String(idx+1)+" A set by posA="+String(posA)+" -> "+(a?hex64(a):String("''")));
  }
  if (jsonGetPos(o, "posB", posB) || jsonGetPos(o, "addrB_pos", posB)) {
    uint64_t b = owdbAddrAtPos(posB);
    heatAddrB[idx] = b; setB = true;
    WebSerial.send("message", String("Heat: DI")+String(idx+1)+" B set by posB="+String(posB)+" -> "+(b?hex64(b):String("''")));
  }
  if (!setA) {
    uint64_t a=0;
    if (jsonGetAddr64(o,"addrA_u64_str", a)) heatAddrA[idx]=a;
    else if (jsonGetAddr64(o,"addrA", a))    heatAddrA[idx]=a;
  }
  if (!setB) {
    uint64_t b=0;
    if (jsonGetAddr64(o,"addrB_u64_str", b)) heatAddrB[idx]=b;
    else if (jsonGetAddr64(o,"addrB", b))    heatAddrB[idx]=b;
  }

  double dv;
  if (jsonGetDouble(o,"cp",dv)  && dv>0) heatCp[idx]=(float)dv;
  if (jsonGetDouble(o,"rho",dv) && dv>0) heatRho[idx]=(float)dv;
  if (jsonGetDouble(o,"calib",dv) && dv>0) heatCalib[idx]=(float)dv;

  return true;
}

void handleUnifiedConfig(JSONVar obj){
  const char* t = (const char*)obj["t"];
  JSONVar list = obj["list"];
  if (!t) return;

  String type = String(t);
  bool changed = false;

  if (type=="inputEnable"){
    for (int i=0;i<NUM_DI && i<list.length();i++) diCfg[i].enabled = (bool)list[i];
    WebSerial.send("message","Input Enabled list updated");
    changed = true;
  }
  else if (type=="inputInvert"){
    for (int i=0;i<NUM_DI && i<list.length();i++) diCfg[i].inverted = (bool)list[i];
    WebSerial.send("message","Input Invert list updated");
    changed = true;
  }
  else if (type=="inputAction"){
    for (int i=0;i<NUM_DI && i<list.length();i++){
      int a=(int)list[i];
      diCfg[i].action = (uint8_t)constrain(a,0,2);
    }
    WebSerial.send("message","Input Action list updated");
    changed = true;
  }
  else if (type=="inputTarget"){
    for (int i=0;i<NUM_DI && i<list.length();i++){
      int tgt=(int)list[i];
      diCfg[i].target = (uint8_t)((tgt==4||tgt==0||(tgt>=1&&tgt<=2))?tgt:0);
    }
    WebSerial.send("message","Input Control Target list updated");
    changed = true;
  }
  else if (type=="inputType"){
    for (int i=0;i<NUM_DI && i<list.length();i++){
      int tp=(int)list[i];
      diCfg[i].type = (uint8_t)constrain(tp,IT_WATER,IT_WCOUNTER);
    }
    WebSerial.send("message","Input Type list updated");
    changed = true;
  }
  else if (type=="counterResetList"){
    uint32_t now = millis();
    for (int i=0;i<NUM_DI && i<list.length();i++){
      if ((bool)list[i]){
        diCounter[i]=0;
        diLastEdgeMs[i]=now;
        lastPulseSnapshot[i]=0;
        flowCounterBase[i]=0;
      }
    }
    WebSerial.send("message","Counters reset");
  }
  else if (type=="relays"){
    for (int i=0;i<NUM_RLY && i<list.length();i++){
      rlyCfg[i].enabled = (bool)list[i]["enabled"];
      rlyCfg[i].inverted= (bool)list[i]["inverted"];
    }
    WebSerial.send("message","Relay Configuration updated");
    changed = true;
  }
  else if (type=="relayCtrlMode"){
    for (int i=0;i<NUM_RLY && i<list.length(); i++){
      int v = (int)list[i];
      rlyCtrlMode[i] = (v==1)?RCTRL_MODBUS:RCTRL_LOCAL;
    }
    WebSerial.send("message","Relay Control Mode updated (0=Local,1=Modbus)");
    changed = true;
  }
  else if (type=="flowPPL"){
    for (int i=0;i<NUM_DI && i<list.length(); i++){
      long v = (long)(double)list[i];
      if (v <= 0) v = 1;
      flowPulsesPerL[i] = (uint32_t)v;
    }
    WebSerial.send("message","Flow: pulsesPerLiter updated");
    changed = true;
  }
  else if (type=="flowCalib"){
    for (int i=0;i<NUM_DI && i<list.length(); i++){
      double v = (double)list[i];
      if (v <= 0.0) v = 1.0;
      flowCalibRate[i]  = (float)v;
      flowCalibAccum[i] = (float)v;
    }
    WebSerial.send("message","Flow: calibration updated (applied to rate & accum)");
    changed = true;
  }
  else if (type=="flowCalibRate"){
    for (int i=0;i<NUM_DI && i<list.length(); i++){
      double v = (double)list[i];
      if (v <= 0.0) v = 1.0;
      flowCalibRate[i] = (float)v;
    }
    WebSerial.send("message","Flow: rate calibration updated");
    changed = true;
  }
  else if (type=="flowCalibAccum"){
    for (int i=0;i<NUM_DI && i<list.length(); i++){
      double v = (double)list[i];
      if (v <= 0.0) v = 1.0;
      flowCalibAccum[i] = (float)v;
    }
    WebSerial.send("message","Flow: accumulation calibration updated");
    changed = true;
  }
  else if (type=="flowResetAccumList"){
    for (int i=0;i<NUM_DI && i<list.length(); i++){
      if ((bool)list[i]) flowCounterBase[i] = diCounter[i];
    }
    WebSerial.send("message","Flow: accumulation windows reset (baseline moved)");
    changed = true;
  }
  else if (type=="heatConfig"){
    for (int i=0;i<NUM_DI && i<list.length(); i++){
      JSONVar o = list[i];
      if (JSON.typeof(o)!="object") continue;
      bool ok = applyHeatCfgObjectToIndex(i, o);
      if (ok){
        String line; line.reserve(200);
        line += "Heat: DI"; line += String(i+1); line += " updated — ";
        line += "enabled="; line += (heatEnabled[i] ? "true" : "false");
        line += " A=";      line += (heatAddrA[i] ? hex64(heatAddrA[i]) : "''");
        line += " B=";      line += (heatAddrB[i] ? hex64(heatAddrB[i]) : "''");
        line += " posA=";   line += String(owdbPosOf(heatAddrA[i]));
        line += " posB=";   line += String(owdbPosOf(heatAddrB[i]));
        line += " cp=";     line += String(heatCp[i], 0);
        line += " rho=";    line += String(heatRho[i], 3);
        line += " calib=";  line += String(heatCalib[i], 4);
        WebSerial.send("message", line);
        changed = true;
      }
    }
  }
  else if (type=="heatResetEnergyList"){
    for (int i=0;i<NUM_DI && i<list.length(); i++){
      if ((bool)list[i]) heatEnergyJ[i]=0.0;
    }
    WebSerial.send("message","Heat: energy counters reset");
    changed = true;
  }
  else if (type=="heatConfigSingle"){
    int di = -1;
    if (obj.hasOwnProperty("di")) di = (int)obj["di"];
    if (di >= 1 && di <= (int)NUM_DI) di -= 1;
    if (di < 0 || di >= (int)NUM_DI) { WebSerial.send("message","heatConfigSingle: invalid 'di'"); return; }
    if (!obj.hasOwnProperty("cfg")) { WebSerial.send("message","heatConfigSingle: missing 'cfg'");  return; }
    JSONVar cfg = obj["cfg"];
    if (JSON.typeof(cfg)!="object"){ WebSerial.send("message","heatConfigSingle: 'cfg' must be an object"); return; }
    if (!applyHeatCfgObjectToIndex(di, cfg)){
      WebSerial.send("message","heatConfigSingle: failed to apply");
      return;
    }
    String line; line.reserve(200);
    line += "Heat: DI"; line += String(di+1); line += " updated — ";
    line += "enabled="; line += (heatEnabled[di] ? "true" : "false");
    line += " A=";      line += (heatAddrA[di] ? hex64(heatAddrA[di]) : "''");
    line += " B=";      line += (heatAddrB[di] ? hex64(heatAddrB[di]) : "''");
    line += " posA=";   line += String(owdbPosOf(heatAddrA[di]));
    line += " posB=";   line += String(owdbPosOf(heatAddrB[di]));
    line += " cp=";     line += String(heatCp[di], 0);
    line += " rho=";    line += String(heatRho[di], 3);
    line += " calib=";  line += String(heatCalib[di], 4);
    WebSerial.send("message", line);
    changed = true;
  }
  // -------- LED/Buttons CONFIG --------
  else if (type=="led"){
    for(int i=0;i<NUM_LED && i<list.length(); i++){
      JSONVar o = list[i];
      if (JSON.typeof(o)!="object") continue;
      if (o.hasOwnProperty("mode"))   ledCfg[i].mode   = (uint8_t)constrain((int)o["mode"],0,1);
      if (o.hasOwnProperty("source")) ledCfg[i].source = (uint8_t)constrain((int)o["source"],0,(int)LEDSRC_DI5);
    }
    WebSerial.send("message","LED config updated"); changed=true;
  }
  else if (type=="buttons"){
    for(int i=0;i<NUM_BTN && i<list.length(); i++){
      int act=(int)list[i];
      btnCfg[i].action = (uint8_t)constrain(act,0,(int)BTN_PULSE_R2);
    }
    WebSerial.send("message","Buttons config updated"); changed=true;
  }
  else {
    WebSerial.send("message","Unknown Config type");
  }

  if (changed){
    cfgDirty = true;
    lastCfgTouchMs = millis();
  }
}

void handleCommand(JSONVar obj){
  const char* actC=(const char*)obj["action"]; if(!actC){ WebSerial.send("message","command: missing 'action'"); return; }
  String act=String(actC); act.toLowerCase();

  if (act=="save"){ if (saveConfigFS()) WebSerial.send("message","Configuration saved"); else WebSerial.send("message","ERROR: Save failed"); return; }
  if (act=="load"){ if (loadConfigFS()){ WebSerial.send("message","Configuration loaded"); sendAllEchoesOnce(); applyModbusSettings(g_mb_address,g_mb_baud); }
                    else WebSerial.send("message","ERROR: Load failed/invalid"); return; }
  if (act=="factory"){
    setDefaults(); saveConfigFS();
    WebSerial.send("message","Factory defaults restored & saved");
    sendAllEchoesOnce(); applyModbusSettings(g_mb_address,g_mb_baud);
    return;
  }
  if (act=="scan"||act=="scan1wire"||act=="scan_1wire"||act=="scan1w"){ doOneWireScan(); return; }


  // flow/heat helpers kept
  if (act=="flow_calculate"){
    int di = -1;
    if (obj.hasOwnProperty("di")) di = (int)obj["di"];
    else if (obj.hasOwnProperty("input")) di = (int)obj["input"];
    else if (obj.hasOwnProperty("index")) di = (int)obj["index"];
    if (di >= 1 && di <= (int)NUM_DI) di -= 1;
    if (di < 0 || di >= (int)NUM_DI) { WebSerial.send("message","flow_calculate: invalid 'di'"); return; }
    double extLit = 0.0;
    if (!jsonGetDouble(obj,"external", extLit) &&
        !jsonGetDouble(obj,"external_liters", extLit) &&
        !jsonGetDouble(obj,"externalAccum", extLit)) {
      WebSerial.send("message","flow_calculate: missing 'external' liters"); return;
    }
    if (extLit <= 0.0) { WebSerial.send("message","flow_calculate: external must be > 0"); return; }
    uint32_t ppl = flowPulsesPerL[di] ? flowPulsesPerL[di] : 1;
    uint32_t pulses_since = (diCounter[di] >= flowCounterBase[di]) ? (diCounter[di] - flowCounterBase[di]) : 0;
    if (pulses_since == 0) { WebSerial.send("message","flow_calculate: no pulses in current window (reset total, run flow)"); return; }
    double liters_no_cal = (double)pulses_since / (double)ppl;
    double newCal = extLit / liters_no_cal;
    flowCalibAccum[di] = (float)((newCal>0.0)?newCal:1.0);
    cfgDirty = true; lastCfgTouchMs = millis();
    String msg; msg.reserve(128);
    msg += "flow_calculate: DI"; msg += String(di+1);
    msg += " pulses_since="; msg += String(pulses_since);
    msg += " ppl=";          msg += String((uint32_t)ppl);
    msg += " external=";     msg += String(extLit, 6);
    msg += " => calibAccum=";msg += String(newCal, 6);
    WebSerial.send("message", msg);
    return;
  }
  if (act=="flow_reset"){
    int di = -1;
    if (obj.hasOwnProperty("di")) di = (int)obj["di"];
    if (di >= 1 && di <= (int)NUM_DI) di -= 1;
    if (di >= 0 && di < (int)NUM_DI) {
      flowCounterBase[di] = diCounter[di];
      cfgDirty = true; lastCfgTouchMs = millis();
      String msg; msg.reserve(32);
      msg += "flow_reset: DI"; msg += String(di+1);
      WebSerial.send("message", msg);
    } else {
      WebSerial.send("message","flow_reset: invalid 'di'");
    }
    return;
  }
  if (act=="heat_reset"){
    int di=-1; if (obj.hasOwnProperty("di")) di=(int)obj["di"];
    if (di>=1 && di<=NUM_DI) di-=1;
    if (di>=0 && di<NUM_DI){ heatEnergyJ[di]=0.0; WebSerial.send("message", String("heat_reset: DI")+String(di+1)); }
    else WebSerial.send("message","heat_reset: invalid 'di'");
    return;
  }


  WebSerial.send("message", String("Unknown command: ")+actC);
}

void handleOneWire(JSONVar obj){
  const char* actC=(const char*)obj["action"]; if(!actC){ WebSerial.send("message","onewire: missing action"); return; }
  String act=String(actC); act.toLowerCase();

  if (act=="list"){ owdbSendList(); return; }
  if (act=="clear"){ g_owCount=0; if(owdbSave()) WebSerial.send("message","onewire: cleared"); else WebSerial.send("message","onewire: clear save failed"); owdbSendList(); return; }

  uint64_t addr=0;
  if (!jsonGetAddr64(obj, "addr_u64_str", addr)){ WebSerial.send("message","onewire: invalid or missing address (expect rom_hex or addr_hi/addr_lo)"); return; }

  if (act=="add"||act=="save"||act=="rename"){
    String nm; if (!jsonGetStr(obj,"name",nm) || nm.length()==0){ WebSerial.send("message","onewire: missing 'name'"); return; }
    if (owdbAddOrUpdate(addr, nm.c_str())) {
      String msg; msg.reserve(64); msg += "onewire: saved "; msg += hex64(addr);
      WebSerial.send("message", msg);
    } else {
      WebSerial.send("message","onewire: save failed (maybe full?)"); owdbSendList();
    }
  } else if (act=="remove"||act=="delete"){
    if (owdbRemove(addr)) {
      String msg; msg.reserve(64); msg += "onewire: removed "; msg += hex64(addr);
      WebSerial.send("message", msg);
    } else {
      WebSerial.send("message","onewire: address not found"); owdbSendList();
    }
  } else {
    WebSerial.send("message", String("onewire: unknown action '")+act+"'"); 
  }
}

// ================== Modbus helpers ==================
inline void setHreg32(uint16_t base, uint32_t v){
  mb.setHreg(base+0,(uint16_t)(v & 0xFFFF));
  mb.setHreg(base+1,(uint16_t)((v>>16)&0xFFFF));
}
inline void setHreg32s(uint16_t base, int32_t v){
  setHreg32(base, (uint32_t)v);
}

// ================== Modbus maintained coils (switched from ESPHome) ==================
void processModbusCommands(){
  // Relay controls - read maintained coil states (affect ONLY modbusDesiredRelay[])
  // Disabled relays ignore all commands and force Modbus coil to false
  for(int r=0;r<NUM_RLY;r++){
    if(rlyCfg[r].enabled) {
      modbusDesiredRelay[r] = mb.Coil(CMD_RLY_STATE_BASE + r);
    } else {
      // Relay is disabled - clear Modbus desired state and force coil to false
      modbusDesiredRelay[r] = false;
      mb.setCoil(CMD_RLY_STATE_BASE + r, false);
    }
  }
  
  // DI enables/disables - read maintained coil states
  for(int i=0;i<NUM_DI;i++){
    bool newEnabled = mb.Coil(CMD_DI_ENABLE_BASE + i);
    if (diCfg[i].enabled != newEnabled) {
      diCfg[i].enabled = newEnabled;
      cfgDirty = true;
      lastCfgTouchMs = millis();
    }
  }
  
  // Counter resets (pulse - cleared after use)
  for(int i=0;i<NUM_DI;i++){
    if (mb.Coil(CMD_CNT_RST_BASE+i)) {
      mb.setCoil(CMD_CNT_RST_BASE+i, false);
      diCounter[i]=0;
      diLastEdgeMs[i]=millis();
      lastPulseSnapshot[i]=0;
      flowCounterBase[i]=0;
    }
  }
}

// Apply local action to target (buttons/DI)
void applyActionToTarget(uint8_t tgt, uint8_t action, uint32_t now){
  auto doRelay = [&](int rIdx){
    if(rIdx<0||rIdx>=NUM_RLY) return;
    // Skip processing if relay is disabled (disabled relays ignore all commands)
    if(!rlyCfg[rIdx].enabled) return;
    if(action==1){ localDesiredRelay[rIdx]=!localDesiredRelay[rIdx]; rlyPulseUntil[rIdx]=0; }
    else if(action==2){ localDesiredRelay[rIdx]=true; rlyPulseUntil[rIdx]=now+PULSE_MS; }
  };
  if(action==0 || tgt==4) return;
  if(tgt==0){ for(int r=0;r<NUM_RLY;r++) doRelay(r); }
  else if(tgt>=1 && tgt<=2) doRelay(tgt-1);
  cfgDirty=true; lastCfgTouchMs=now;
}

// ================== Loop ==================
void loop(){
  unsigned long now=millis();
  mb.task(); processModbusCommands();

  // blink phase
  if(now-lastBlinkToggle>=blinkPeriodMs){ lastBlinkToggle=now; blinkPhase=!blinkPhase; }

  // ===== module time tick handled later (1s section)

  JSONVar inputs; JSONVar counters;
  for (int i=0;i<NUM_DI;i++){
    bool raw=(digitalRead(DI_PINS[i])==HIGH); if (diCfg[i].inverted) raw=!raw;
    bool val=false;

    if(!diCfg[i].enabled){ val=false; }
    else {
      switch(diCfg[i].type){
        case IT_WATER: case IT_HUMIDITY: val=raw; break;
        case IT_WCOUNTER: val=raw; break;
        default: val=raw; break;
      }
    }

    bool prev=diState[i]; diPrev[i]=prev; diState[i]=val;
    inputs[i]=val; 
    mb.setIsts(ISTS_DI_BASE+i, val);
    mb.setHreg(HREG_DI_BASE + i, val ? 1 : 0); // Mirror to HREG

    bool rising=(!prev && val);

    if (diCfg[i].type==IT_WCOUNTER){
      if (rising && (now - diLastEdgeMs[i] >= CNT_DEBOUNCE_MS)){
        diCounter[i]++; diLastEdgeMs[i]=now;
        // NOTE: Modbus HREG counters removed; keep internal/WebSerial only
      }
    } else {
      uint8_t act=diCfg[i].action;
      if (act==1){ if (rising || (prev && !val)) applyActionToTarget(diCfg[i].target,1,now); }
      else if (act==2){ if (rising) applyActionToTarget(diCfg[i].target,2,now); }
    }

    counters[i]=(double)diCounter[i];
  }

  // Buttons (rising-edge actions)
  JSONVar btnStates;
  for(int i=0;i<NUM_BTN;i++){
    bool pressed=(digitalRead(BTN_PINS[i])==HIGH);
    buttonPrev[i]=buttonState[i];
    buttonState[i]=pressed;
    btnStates[i]=pressed;

    // rising edge => immediate action types
    if(!buttonPrev[i] && buttonState[i]){
      switch(btnCfg[i].action){
        case BTN_TOGGLE_R1: if(rlyCfg[0].enabled) { localDesiredRelay[0] = !localDesiredRelay[0]; rlyPulseUntil[0]=0; cfgDirty=true; lastCfgTouchMs=now; WebSerial.send("message","button: toggle R1"); } break;
        case BTN_TOGGLE_R2: if(rlyCfg[1].enabled) { localDesiredRelay[1] = !localDesiredRelay[1]; rlyPulseUntil[1]=0; cfgDirty=true; lastCfgTouchMs=now; WebSerial.send("message","button: toggle R2"); } break;
        case BTN_PULSE_R1:  if(rlyCfg[0].enabled) { localDesiredRelay[0] = true; rlyPulseUntil[0]= now + PULSE_MS; cfgDirty=true; lastCfgTouchMs=now; WebSerial.send("message","button: pulse R1"); } break;
        case BTN_PULSE_R2:  if(rlyCfg[1].enabled) { localDesiredRelay[1] = true; rlyPulseUntil[1]= now + PULSE_MS; cfgDirty=true; lastCfgTouchMs=now; WebSerial.send("message","button: pulse R2"); } break;
        default: break;
      }
    }


    mb.setIsts(ISTS_BTN_BASE + i, pressed);
    mb.setHreg(HREG_BTN_BASE + i, pressed ? 1 : 0); // Mirror to HREG
  }

  JSONVar relayStateList;
  // compute relay state from selected source (Modbus or Local)
  for (int i=0;i<NUM_RLY;i++){
    bool selectedCmd = false;

    if (rlyCtrlMode[i] == RCTRL_MODBUS) {
      selectedCmd = modbusDesiredRelay[i];
    } else {
      selectedCmd = localDesiredRelay[i];
    }

    bool outVal=selectedCmd; if(!rlyCfg[i].enabled) outVal=false; if(rlyCfg[i].inverted) outVal=!outVal;
    digitalWrite(RELAY_PINS[i], outVal?HIGH:LOW);
    physRelayState[i]=outVal;

    relayStateList[i]=outVal; 
    mb.setIsts(ISTS_RLY_BASE+i,outVal);
    mb.setHreg(HREG_RLY_BASE + i, outVal ? 1 : 0); // Mirror to HREG
    
    // Update Modbus coil to reflect actual relay state (for ESPHome switch state reading)
    mb.setCoil(CMD_RLY_STATE_BASE + i, outVal);

    // expire local pulse
    if (rlyPulseUntil[i] && timeAfter32(now, rlyPulseUntil[i])){ localDesiredRelay[i]=false; rlyPulseUntil[i]=0; cfgDirty=true; lastCfgTouchMs=now; }
  }

  // LEDs (srcActive + optional blink)
  JSONVar ledStates, ledConfigArray;
  auto ledSrcActive = [&](uint8_t src)->bool{
    switch(src){
      case LEDSRC_R1:     return physRelayState[0];
      case LEDSRC_R2:     return physRelayState[1];
      case LEDSRC_DI1:    return diState[0];
      case LEDSRC_DI2:    return diState[1];
      case LEDSRC_DI3:    return diState[2];
      case LEDSRC_DI4:    return diState[3];
      case LEDSRC_DI5:    return diState[4];
      default: return false;
    }
  };
  for(int i=0;i<NUM_LED;i++){
    bool srcActive = ledSrcActive(ledCfg[i].source);
    bool phys = (ledCfg[i].mode==0) ? srcActive : (srcActive && blinkPhase);
    digitalWrite(LED_PINS[i], phys ? HIGH : LOW);
    mb.setIsts(ISTS_LED_BASE + i, phys);
    mb.setHreg(HREG_LED_BASE + i, phys ? 1 : 0); // Mirror to HREG
    ledStates[i]=phys;

    JSONVar L; L["mode"]=(double)ledCfg[i].mode; L["source"]=(double)ledCfg[i].source; L["state"]=phys;
    ledConfigArray[i]=L;
  }

  // ===== 1s tick =====
  if (timeAfter32(now, nextRateTickMs)) {
    uint32_t dt_ms = 1000;
    nextRateTickMs = now + 1000;

    // OneWire convert / read / cache
    if (!oneWireBusy) {
      ds18b20StartConvertAll();
      oneWireBusy = true;
      nextOneWireConvertMs = now + 1500;
    } else if (timeAfter32(now, nextOneWireConvertMs)) {
      for (size_t i=0; i<g_owCount; i++){
        double t;
        if (ds18b20ReadTempC(g_owDb[i].addr, t)) {
          owLastGoodTemp[i] = t;
          owLastGoodMs[i]   = now;
          owErrCount[i]     = 0;
        } else {
          if (owErrCount[i] < 0xFFFFFFFF) owErrCount[i]++;
        }
      }
      for (int i=0;i<NUM_DI;i++){
        double ta=NAN, tb=NAN;
        int ia = owdbIndexOf(heatAddrA[i]);
        int ib = owdbIndexOf(heatAddrB[i]);
        if (ia>=0 && ia<(int)g_owCount && owLastGoodMs[ia] && (now-owLastGoodMs[ia] <= OW_FAIL_HIDE_MS)) ta = owLastGoodTemp[ia];
        if (ib>=0 && ib<(int)g_owCount && owLastGoodMs[ib] && (now-owLastGoodMs[ib] <= OW_FAIL_HIDE_MS)) tb = owLastGoodTemp[ib];
        heatTA[i]=ta; heatTB[i]=tb;
      }
      publishOneWireTemps();
      oneWireBusy=false;
    }

    // Flow: update rate from counters
    for (int i=0;i<NUM_DI;i++){
      uint32_t ppl   = flowPulsesPerL[i] ? flowPulsesPerL[i] : 1;
      uint32_t delta = diCounter[i] - lastPulseSnapshot[i];
      lastPulseSnapshot[i] = diCounter[i];
      float liters_last_sec = ((float)delta / (float)ppl) * flowCalibRate[i];
      flowRateLmin[i] = liters_last_sec * 60.0f;
    }

    // Heat power & energy integration
    for (int i=0;i<NUM_DI;i++){
      double P=0.0; double dT=0.0;
      if (diCfg[i].type==IT_WCOUNTER && heatEnabled[i] &&
          isfinite(heatTA[i]) && isfinite(heatTB[i])) {
        dT = (heatTA[i] - heatTB[i]);
        double m_dot = ((double)flowRateLmin[i] / 60.0) * (double)heatRho[i]; // kg/s
        P = m_dot * (double)heatCp[i] * dT;     // W
        P *= (double)heatCalib[i];
        heatEnergyJ[i] += P * (dt_ms/1000.0);   // J
      }
      heatDT[i]=dT; heatPowerW[i]=P;
    }

  }

  // ===== Update Modbus Holding Registers =====
  // Flow per DI
  for (int i=0;i<NUM_DI;i++){
    uint32_t rate_milli = (flowRateLmin[i] <= 0.0f) ? 0u : (uint32_t)llround((double)flowRateLmin[i]*1000.0);
    setHreg32(HREG_FLOW_RATE_BASE + (i*2), rate_milli);

    uint32_t ppl = flowPulsesPerL[i] ? flowPulsesPerL[i] : 1;
    uint32_t pulses_since = (diCounter[i] >= flowCounterBase[i]) ? (diCounter[i] - flowCounterBase[i]) : 0;
    double accumL = ((double)pulses_since / (double)ppl) * (double)flowCalibAccum[i];
    uint32_t accum_milli = (accumL <= 0.0) ? 0u : (uint32_t)llround(accumL*1000.0);
    setHreg32(HREG_FLOW_ACCUM_BASE + (i*2), accum_milli);

    int32_t P = (int32_t)llround(heatPowerW[i]);
    setHreg32s(HREG_HEAT_POWER_BASE + (i*2), P);

    double wh = heatEnergyJ[i] / 3600.0;
    uint32_t wh_milli = (wh <= 0.0) ? 0u : (uint32_t)llround(wh*1000.0);
    setHreg32(HREG_HEAT_EN_WH_BASE + (i*2), wh_milli);

    int32_t dt_milli = (int32_t)llround(heatDT[i]*1000.0);
    setHreg32s(HREG_HEAT_DT_BASE + (i*2), dt_milli);
  }


  // 1-Wire quick telemetry (first 10 sensors)
  for (int i=0;i<10;i++){
    int32_t temp_milli = 0;
    if (i < (int)g_owCount){
      uint32_t ms = owLastGoodMs[i];
      if (ms != 0 && timeAfter32(millis(), ms) && (millis() - ms) <= OW_FAIL_HIDE_MS){
        double t = owLastGoodTemp[i];
        if (isfinite(t)) temp_milli = (int32_t)llround(t*1000.0);
      }
    }
    setHreg32s(HREG_OW_TEMP_BASE + (i*2), temp_milli);
  }

  if (millis()-lastSend>=sendInterval){
    lastSend=millis();
    WebSerial.check();
    WebSerial.send("status", modbusStatus);

    JSONVar invertList, enableList, actionList, targetList, typeList;
    for (int i=0;i<NUM_DI;i++){
      invertList[i]=diCfg[i].inverted; enableList[i]=diCfg[i].enabled;
      actionList[i]=diCfg[i].action;   targetList[i]=diCfg[i].target; typeList[i]=diCfg[i].type;
    }
    JSONVar relayEnableList, relayInvertList;
    for (int i=0;i<NUM_RLY;i++){ relayEnableList[i]=rlyCfg[i].enabled; relayInvertList[i]=rlyCfg[i].inverted; }

    JSONVar flowPPLList, flowCalibList, flowAccumList, flowRateList, flowCalibRateList, flowCalibAccumList;
    for (int i=0;i<NUM_DI;i++){
      uint32_t ppl = flowPulsesPerL[i] ? flowPulsesPerL[i] : 1;
      uint32_t pulses_since = (diCounter[i] >= flowCounterBase[i]) ? (diCounter[i] - flowCounterBase[i]) : 0;
      double accumL = ((double)pulses_since / (double)ppl) * (double)flowCalibAccum[i];

      flowPPLList[i]         = (double)flowPulsesPerL[i];
      flowCalibList[i]       = (double)flowCalibAccum[i];
      flowCalibRateList[i]   = (double)flowCalibRate[i];
      flowCalibAccumList[i]  = (double)flowCalibAccum[i];
      flowAccumList[i]       = accumL;
      flowRateList[i]        = (double)flowRateLmin[i];
    }

    JSONVar heatEnabledList, heatAddrAList, heatAddrBList, heatAddrAPosList, heatAddrBPosList;
    JSONVar heatCpList, heatRhoList, heatCalibList;
    JSONVar heatTAList, heatTBList, heatDTList, heatPowerList, heatEnergyJList, heatEnergyKWhList;

    for (int i=0;i<NUM_DI;i++){
      heatEnabledList[i]=heatEnabled[i];
      heatAddrAList[i]= (heatAddrA[i] ? hex64(heatAddrA[i]) : "");
      heatAddrBList[i]= (heatAddrB[i] ? hex64(heatAddrB[i]) : "");

      heatAddrAPosList[i] = (double)owdbPosOf(heatAddrA[i]);
      heatAddrBPosList[i] = (double)owdbPosOf(heatAddrB[i]);

      heatCpList[i]=(double)heatCp[i];
      heatRhoList[i]=(double)heatRho[i];
      heatCalibList[i]=(double)heatCalib[i];

      heatTAList[i]=isfinite(heatTA[i])?heatTA[i]:NAN;
      heatTBList[i]=isfinite(heatTB[i])?heatTB[i]:NAN;
      heatDTList[i]=heatDT[i];
      heatPowerList[i]=heatPowerW[i];
      heatEnergyJList[i]=heatEnergyJ[i];
      heatEnergyKWhList[i]=heatEnergyJ[i]/3.6e6;
    }

    // cached temps/errors
    {
      JSONVar owTemps, owErrs;
      buildCachedOwTemps(owTemps, owErrs);
      WebSerial.send("onewireTemps", owTemps);
      WebSerial.send("onewireErrs",  owErrs);
    }

    WebSerial.send("inputs", inputs);
    WebSerial.send("invertList", invertList);
    WebSerial.send("enableList", enableList);
    WebSerial.send("inputActionList", actionList);
    WebSerial.send("inputTargetList", targetList);
    WebSerial.send("inputTypeList", typeList);
    WebSerial.send("counterList", counters);

    WebSerial.send("flowPPLList",   flowPPLList);
    WebSerial.send("flowCalibList", flowCalibList);
    WebSerial.send("flowCalibRateList",  flowCalibRateList);
    WebSerial.send("flowCalibAccumList", flowCalibAccumList);
    WebSerial.send("flowAccumList", flowAccumList);
    WebSerial.send("flowRateList",  flowRateList);

    WebSerial.send("heatEnabledList",   heatEnabledList);
    WebSerial.send("heatAddrAList",     heatAddrAList);
    WebSerial.send("heatAddrBList",     heatAddrBList);
    WebSerial.send("heatAddrAPosList",  heatAddrAPosList);
    WebSerial.send("heatAddrBPosList",  heatAddrBPosList);
    WebSerial.send("heatCpList",        heatCpList);
    WebSerial.send("heatRhoList",       heatRhoList);
    WebSerial.send("heatCalibList",     heatCalibList);
    WebSerial.send("heatTAList",        heatTAList);
    WebSerial.send("heatTBList",        heatTBList);
    WebSerial.send("heatDTList",        heatDTList);
    WebSerial.send("heatPowerList",     heatPowerList);
    WebSerial.send("heatEnergyJList",   heatEnergyJList);
    WebSerial.send("heatEnergyKWhList", heatEnergyKWhList);

    WebSerial.send("relayStateList", relayStateList);

    // LED/BTN echoes (names aligned with DIM UI)
    WebSerial.send("led", ledConfigArray);
    WebSerial.send("ledStateList", ledStates);
    WebSerial.send("buttonStateList", btnStates);



    // Relay control mode status
    JSONVar rcList;
    for (int i=0;i<NUM_RLY;i++){ rcList[i]=(double)((rlyCtrlMode[i]==RCTRL_MODBUS)?1:0); }
    WebSerial.send("relayCtrlMode", rcList);

    owdbSendList();
  }

  if (cfgDirty && (now-lastCfgTouchMs>=CFG_AUTOSAVE_MS)){
    if (saveConfigFS()) WebSerial.send("message","Configuration saved");
    else WebSerial.send("message","ERROR: Save failed");
    cfgDirty=false;
  }
}

void sendAllEchoesOnce(){
  JSONVar enableList, invertList, actionList, targetList, typeList, counters;
  for (int i=0;i<NUM_DI;i++){
    enableList[i]=diCfg[i].enabled; invertList[i]=diCfg[i].inverted;
    actionList[i]=diCfg[i].action;  targetList[i]=diCfg[i].target;
    typeList[i]=diCfg[i].type;      counters[i]=(double)diCounter[i];
  }
  WebSerial.send("enableList", enableList);
  WebSerial.send("invertList", invertList);
  WebSerial.send("inputActionList", actionList);
  WebSerial.send("inputTargetList", targetList);
  WebSerial.send("inputTypeList", typeList);
  WebSerial.send("counterList", counters);

  JSONVar relayEnableList, relayInvertList;
  for (int i=0;i<NUM_RLY;i++){ relayEnableList[i]=rlyCfg[i].enabled; relayInvertList[i]=rlyCfg[i].inverted; }
  WebSerial.send("relayEnableList", relayEnableList);
  WebSerial.send("relayInvertList", relayInvertList);

  JSONVar flowPPLList, flowCalibList, flowAccumList, flowRateList, flowCalibRateList, flowCalibAccumList;
  for (int i=0;i<NUM_DI;i++){
    uint32_t ppl = flowPulsesPerL[i] ? flowPulsesPerL[i] : 1;
    uint32_t pulses_since = (diCounter[i] >= flowCounterBase[i]) ? (diCounter[i] - flowCounterBase[i]) : 0;
    double accumL = ((double)pulses_since / (double)ppl) * (double)flowCalibAccum[i];

    flowPPLList[i]         = (double)flowPulsesPerL[i];
    flowCalibList[i]       = (double)flowCalibAccum[i];
    flowCalibRateList[i]   = (double)flowCalibRate[i];
    flowCalibAccumList[i]  = (double)flowCalibAccum[i];
    flowAccumList[i]       = accumL;
    flowRateList[i]        = (double)flowRateLmin[i];
  }
  WebSerial.send("flowPPLList",   flowPPLList);
  WebSerial.send("flowCalibList", flowCalibList);
  WebSerial.send("flowCalibRateList",  flowCalibRateList);
  WebSerial.send("flowCalibAccumList", flowCalibAccumList);
  WebSerial.send("flowAccumList", flowAccumList);
  WebSerial.send("flowRateList",  flowRateList);

  JSONVar heatEnabledList, heatAddrAList, heatAddrBList, heatAddrAPosList, heatAddrBPosList;
  JSONVar heatCpList, heatRhoList, heatCalibList;
  JSONVar heatTAList, heatTBList, heatDTList, heatPowerList, heatEnergyJList, heatEnergyKWhList;
  for (int i=0;i<NUM_DI;i++){
    heatEnabledList[i]=heatEnabled[i];
    heatAddrAList[i]= (heatAddrA[i] ? hex64(heatAddrA[i]) : "");
    heatAddrBList[i]= (heatAddrB[i] ? hex64(heatAddrB[i]) : "");

    heatAddrAPosList[i] = (double)owdbPosOf(heatAddrA[i]);
    heatAddrBPosList[i] = (double)owdbPosOf(heatAddrB[i]);

    heatCpList[i]=(double)heatCp[i];
    heatRhoList[i]=(double)heatRho[i];
    heatCalibList[i]=(double)heatCalib[i];

    heatTAList[i]=isfinite(heatTA[i])?heatTA[i]:NAN;
    heatTBList[i]=isfinite(heatTB[i])?heatTB[i]:NAN;
    heatDTList[i]=heatDT[i];
    heatPowerList[i]=heatPowerW[i];
    heatEnergyJList[i]=heatEnergyJ[i];
    heatEnergyKWhList[i]=heatEnergyJ[i]/3.6e6;
  }
  WebSerial.send("heatEnabledList",   heatEnabledList);
  WebSerial.send("heatAddrAList",     heatAddrAList);
  WebSerial.send("heatAddrBList",     heatAddrBList);
  WebSerial.send("heatAddrAPosList",  heatAddrAPosList);
  WebSerial.send("heatAddrBPosList",  heatAddrBPosList);
  WebSerial.send("heatCpList", heatCpList);
  WebSerial.send("heatRhoList", heatRhoList);
  WebSerial.send("heatCalibList", heatCalibList);
  WebSerial.send("heatTAList", heatTAList);
  WebSerial.send("heatTBList", heatTBList);
  WebSerial.send("heatDTList", heatDTList);
  WebSerial.send("heatPowerList", heatPowerList);
  WebSerial.send("heatEnergyJList", heatEnergyJList);
  WebSerial.send("heatEnergyKWhList", heatEnergyKWhList);



  // LEDs + Buttons echo once
  JSONVar ledArr;
  for(int i=0;i<NUM_LED;i++){ JSONVar L; L["mode"]=(double)ledCfg[i].mode; L["source"]=(double)ledCfg[i].source; ledArr[i]=L; }
  WebSerial.send("led", ledArr);
  JSONVar btnArr;
  for(int i=0;i<NUM_BTN;i++){ btnArr[i]=(double)btnCfg[i].action; }
  WebSerial.send("buttons", btnArr);

  // relay control mode once
  JSONVar rcList;
  for (int i=0;i<NUM_RLY;i++){ rcList[i]=(double)((rlyCtrlMode[i]==RCTRL_MODBUS)?1:0); }
  WebSerial.send("relayCtrlMode", rcList);

  {
    JSONVar owTemps, owErrs;
    buildCachedOwTemps(owTemps, owErrs);
    WebSerial.send("onewireTemps", owTemps);
    WebSerial.send("onewireErrs",  owErrs);
  }

  owdbSendList();
  modbusStatus["address"]=g_mb_address; modbusStatus["baud"]=g_mb_baud;
  WebSerial.send("status", modbusStatus);
}

void doOneWireScan(){
  JSONVar romList; uint8_t addr[8]; int idx=0; int found=0;
  oneWire.reset_search();
  while(oneWire.search(addr)){
    if (OneWire::crc8(addr,7)!=addr[7]) continue;
    uint64_t v=romBytesToU64(addr);
    romList[idx++]=hex64(v); found++;
  }
  WebSerial.send("onewireScan", romList);
  WebSerial.send("message", String("1-Wire scan: ")+found+ " device(s)");
}
