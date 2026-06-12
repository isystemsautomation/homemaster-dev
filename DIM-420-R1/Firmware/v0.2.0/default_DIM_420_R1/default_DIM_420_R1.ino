/********** Arduino preprocessor fix: forward declare before includes **********/
struct PersistConfig;

/**************************************************************
 * DIM-420-R1 — RP2350A (Pico 2) firmware
 * QUIET WEB CONFIG + LOGS + LIVE DI STATE
 *
 * - Unified WebConfig: status / io / cfg / ext / log (once per second)
 * - Accepts "values" (Modbus addr/baud) and "Config" updates from Web UI
 * - Minimal "log" messages on user/Modbus actions
 * - RS-485/Modbus on Serial2, WebSerial on USB Serial
 **************************************************************/

#include <Arduino.h>
#include <ModbusSerial.h>
#include "hm_common.h"
#define HM_MODEL_ID   3
#define HM_FW_MAJOR   0
#define HM_FW_MINOR   2
#define HM_FW_PATCH   0
#define HM_FW         "0.2.0"
#define HM_MAP        1
#define HM_MAP_VERSION 1
#include <SimpleWebSerial.h>
#include <Arduino_JSON.h>
#include <LittleFS.h>
#include <utility>
#include <math.h>
#include "pico/time.h"          // time_us_64(), add_alarm_in_us(), alarm_id_t

// ================== UART2 (RS-485 / Modbus) ==================
#define TX2 4
#define RX2 5
const int TxenPin = -1; // -1 if RS-485 TXEN not used
int SlaveId = 1;
ModbusSerial mb(Serial2, SlaveId, TxenPin);

// ================== GPIO MAP (RP2350A) ==================
static const uint8_t DI_PINS[4] = {8, 9, 15, 16};  // IN1..IN4
static const uint8_t ZC_PINS[2]   = {0, 2};        // CH1/CH2 zero-cross
static const uint8_t GATE_PINS[2] = {1, 3};        // CH1/CH2 gate
static const uint8_t LED_PINS[4]  = {18, 19, 20, 21};
static const uint8_t BTN_PINS[4]  = {22, 23, 24, 25};

// ================== Sizes ==================
static const uint8_t NUM_DI=4, NUM_CH=2, NUM_LED=4, NUM_BTN=4;

// ================== AC timing / MOSFET gating ==================
constexpr uint32_t HALF_US_DEFAULT=10000u, ZC_BLANK_US=100u, MOS_OFF_GUARD_US=150u, GATE_PULSE_US=120u;

// ---- State (volatile: accessed in ISR) ----
volatile uint8_t  chLevel[NUM_CH] = {0,0};
volatile uint8_t  chLastNonZero[NUM_CH] = {200,200};

// ================== Digital Input switch model ==================
enum DiSwitchType : uint8_t { DI_SW_MOMENTARY=0, DI_SW_LATCHING=1 };
enum DiPressType  : uint8_t { PRESS_SHORT=0, PRESS_LONG=1, PRESS_DOUBLE_SHORT=2, PRESS_SHORT_THEN_LONG=3, PRESS_COUNT=4 };

// --------- DI actions aligned with UI (1 = Turn on) ----------
enum DiAction : uint8_t {
  DI_ACT_NONE=0,
  DI_ACT_TURN_ON=1,            // only turns on (to preset) if currently off
  DI_ACT_TURN_OFF=2,
  DI_ACT_TOGGLE_OUTPUT=3,      // toggle using last non-zero level
  DI_ACT_INC=4,
  DI_ACT_DEC=5,
  DI_ACT_INC_THEN_DEC=6,       // <-- short press: 1 step ping-pong; long: continuous ping-pong
  // internal (not sent by UI)
  DI_ACT_TOGGLE_TO_PRESET=7,
  // go straight to channel upper threshold (MAX)
  DI_ACT_GO_MAX=8
};

enum DiTarget     : uint8_t { DI_TGT_NONE=0, DI_TGT_CH1=1, DI_TGT_CH2=2, DI_TGT_ALL=4 };
enum DiLatchMode  : uint8_t { LATCH_TOGGLE_TO_PRESET_OR_0=0, LATCH_PINGPONG_UNTIL_TOGGLE=1 };

// ================== Config & runtime ==================
struct InCfg { bool enabled; bool inverted; uint8_t switchType; uint8_t pressAction[PRESS_COUNT]; uint8_t pressTarget[PRESS_COUNT]; uint8_t latchMode; uint8_t latchTarget; };
struct ChCfg { bool enabled; uint8_t powerOn; };
struct LedCfg { uint8_t mode; uint8_t source; };
struct BtnCfg { uint8_t action; };   // see mapping below

InCfg  diCfg[NUM_DI];
ChCfg  chCfg[NUM_CH];
LedCfg ledCfg[NUM_LED];
BtnCfg btnCfg[NUM_BTN];

// ---------- LED source dropdown codes (simple, single-field) ----------
enum LedSource : uint8_t {
  LEDSRC_NONE = 0,
  LEDSRC_CH1  = 1,
  LEDSRC_CH2  = 2,
  LEDSRC_AC1  = 3,
  LEDSRC_AC2  = 4,
  LEDSRC_DI1  = 5,
  LEDSRC_DI2  = 6,
  LEDSRC_DI3  = 7,
  LEDSRC_DI4  = 8
};

bool buttonState[NUM_BTN] = {false,false,false,false};
bool buttonPrev[NUM_BTN]  = {false,false,false,false};

// --- Button hold-to-ramp runtime ----
const uint8_t  STEP_DELTA=10;
const uint16_t BTN_RAMP_TICK_MS=80;     // stepping cadence during long press
const uint32_t SHORT_MAX_MS=350, LONG_MIN_MS=700, DOUBLE_GAP_MS=300;
struct BtnRuntime { bool pressed=false; uint32_t pressStart=0; bool rampActive=false; uint32_t lastRampTick=0; };
BtnRuntime btnRt[NUM_BTN];

uint32_t chPulseUntil[NUM_CH] = {0,0};
const uint32_t PULSE_MS = 500;

// ================== ZC monitor (per-channel) ==================
volatile uint32_t zcLastEdgeMs[NUM_CH] = {0,0};
bool zcOk[NUM_CH]      = {false,false};
bool zcPrevOk[NUM_CH]  = {false,false};
const uint32_t ZC_FAULT_TIMEOUT_MS = 1000;
const uint8_t  ZC_OK_STREAK_N=6, ZC_FAULT_STREAK_N=6;
uint8_t zcOkStreak[NUM_CH]={0,0}, zcFaultStreak[NUM_CH]={0,0};

// ================== Frequency measurement (robust) ==================
volatile uint64_t zcPrevEdgeUs64[NUM_CH] = {0,0};
volatile uint32_t zcHalfUsLatest[NUM_CH] = {HALF_US_DEFAULT,HALF_US_DEFAULT};
volatile uint32_t zcSampleSeq[NUM_CH]    = {0,0};
constexpr uint32_t HALF_MIN_US=7500, HALF_MAX_US=12000;
constexpr int MED_W=3, AVG_N=32;
uint32_t med3[NUM_CH][MED_W]={{HALF_US_DEFAULT,HALF_US_DEFAULT,HALF_US_DEFAULT},{HALF_US_DEFAULT,HALF_US_DEFAULT,HALF_US_DEFAULT}};
uint8_t  medIdx[NUM_CH]={0,0};
uint32_t avgBuf[NUM_CH][AVG_N]={{HALF_US_DEFAULT}}, avgIdx[NUM_CH]={0,0};
uint64_t avgSum[NUM_CH]={(uint64_t)HALF_US_DEFAULT*AVG_N,(uint64_t)HALF_US_DEFAULT*AVG_N};
uint32_t lastSeqConsumed[NUM_CH]={0,0};
constexpr int CLOCK_PPM_CORR=0;
inline double corr_half_us(double half_us){ return half_us * (1.0 + (CLOCK_PPM_CORR / 1e6)); }
uint16_t freq_x100[NUM_CH] = {5000,5000};
volatile uint32_t gateHalfUs[NUM_CH] = {HALF_US_DEFAULT,HALF_US_DEFAULT};

// ================== Dimming thresholds (per-channel) ==================
uint8_t chLower[NUM_CH]={20,20}, chUpper[NUM_CH]={255,255};

// ================== Load type + percent setpoint ==================
enum LoadType : uint8_t { LOAD_LAMP=0, LOAD_HEATER=1, LOAD_KEY=2 };
uint8_t  chLoadType[NUM_CH]={LOAD_LAMP,LOAD_LAMP};
uint16_t chPctX10[NUM_CH]={0,0};

// ================== MOSFET cutoff (waveform) mode ==================
enum CutMode : uint8_t { CUT_LEADING=0, CUT_TRAILING=1 };
uint8_t chCutMode[NUM_CH]={CUT_LEADING,CUT_LEADING};

// ================== Per-channel preset level ==================
uint8_t chPreset[NUM_CH]={200,200};

// ================== Web Serial ==================
SimpleWebSerial WebSerial;

// ------------------ LOGGING ------------------
static inline void wsLog(const char* msg) { WebSerial.send("log", msg); }
static inline void wsLog(const String& msg) { WebSerial.send("log", msg); }

// ================== Timing ==================
unsigned long lastSend=0; const unsigned long sendInterval=1000;
unsigned long lastBlinkToggle=0; const unsigned long blinkPeriodMs=400; bool blinkPhase=false;

// ================== Persisted Modbus settings ==================
uint8_t  g_mb_address=3; uint32_t g_mb_baud=19200;

// ================== Persistence (LittleFS) ==================
struct ChCfgV6 { bool enabled; };

struct PersistConfigV6 {
  uint32_t magic; uint16_t version; uint16_t size;
  InCfg  diCfg[NUM_DI]; ChCfgV6 chCfg[NUM_CH]; LedCfg ledCfg[NUM_LED]; BtnCfg btnCfg[NUM_BTN];
  uint8_t chLevel[NUM_CH]; uint8_t chLastNonZero[NUM_CH]; uint8_t chLower[NUM_CH]; uint8_t chUpper[NUM_CH];
  uint8_t chLoadType[NUM_CH]; uint16_t chPctX10[NUM_CH]; uint8_t chCutMode[NUM_CH]; uint8_t chPreset[NUM_CH];
  uint8_t mb_address; uint32_t mb_baud; uint32_t crc32;
} __attribute__((packed));

struct PersistConfig {
  uint32_t magic; uint16_t version; uint16_t size;
  InCfg  diCfg[NUM_DI]; ChCfg chCfg[NUM_CH]; LedCfg ledCfg[NUM_LED]; BtnCfg btnCfg[NUM_BTN];
  uint8_t chLower[NUM_CH]; uint8_t chUpper[NUM_CH];
  uint8_t chLoadType[NUM_CH]; uint16_t chPctX10[NUM_CH]; uint8_t chCutMode[NUM_CH]; uint8_t chPreset[NUM_CH];
  uint8_t mb_address; uint32_t mb_baud; uint32_t crc32;
} __attribute__((packed));

struct OutputStateSnapshot {
  uint32_t magic; uint16_t version; uint16_t size;
  uint8_t chLevel[NUM_CH]; uint8_t chLastNonZero[NUM_CH];
  uint32_t crc32;
} __attribute__((packed));

static const uint32_t CFG_MAGIC=0x314D4449UL;
static const uint16_t CFG_VERSION=0x0007;
static const uint16_t CFG_VERSION_V6=0x0006;
static const char* CFG_PATH="/cfg.bin";
static const char* OUT_STATE_PATH="/cfg_out.bin";
static const uint32_t OUT_STATE_MAGIC=0x484D4F53UL;
static const uint16_t OUT_STATE_VERSION=0x0001;

volatile bool cfgDirty=false; uint32_t lastCfgTouchMs=0; const uint32_t CFG_AUTOSAVE_MS=1500;
uint32_t lastOutChangeMs=0; uint32_t lastOutSaveMs=0; const uint32_t OUT_AUTOSAVE_MS=10000;
uint8_t prevChLevel[NUM_CH]={0,0}; bool outTrackInit=false;

// ================== Utils ==================
uint32_t crc32_update(uint32_t crc, const uint8_t* data, size_t len){
  crc = ~crc; while(len--){ crc ^= *data++; for(uint8_t k=0;k<8;k++) crc = (crc>>1) ^ (0xEDB88320UL & (-(int32_t)(crc & 1))); } return ~crc;
}
inline bool timeAfter32(uint32_t a, uint32_t b){ return (int32_t)(a-b) >= 0; }

// ---- MOSFET gate set callbacks ----
// Important: ON callbacks may already be scheduled by a prior zero-cross edge.
// If a channel gets disabled in the meantime, we must not re-enable the gate.
static int64_t mos_on_cb_ch0 (alarm_id_t, void*){
  if(!chCfg[0].enabled || chLevel[0]==0){ digitalWrite(GATE_PINS[0], LOW); return 0; }
  digitalWrite(GATE_PINS[0], HIGH);
  return 0;
}
static int64_t mos_on_cb_ch1 (alarm_id_t, void*){
  if(!chCfg[1].enabled || chLevel[1]==0){ digitalWrite(GATE_PINS[1], LOW); return 0; }
  digitalWrite(GATE_PINS[1], HIGH);
  return 0;
}
static int64_t mos_off_cb_ch0(alarm_id_t, void*){ digitalWrite(GATE_PINS[0], LOW ); return 0; }
static int64_t mos_off_cb_ch1(alarm_id_t, void*){ digitalWrite(GATE_PINS[1], LOW ); return 0; }

// ---- Zero-cross ISR ----
void zc_isr_common(uint8_t ch){
  uint64_t now = time_us_64(); uint64_t prev = zcPrevEdgeUs64[ch]; zcPrevEdgeUs64[ch]=now;
  if (prev){ uint32_t delta=(uint32_t)(now-prev); if (delta>=7500 && delta<=12000){ zcHalfUsLatest[ch]=delta; zcSampleSeq[ch]++; } }
  zcLastEdgeMs[ch]=millis();
  digitalWrite(GATE_PINS[ch], LOW);

  const uint8_t lvl=chLevel[ch]; if (lvl==0 || !chCfg[ch].enabled) return;

  uint32_t half_us=gateHalfUs[ch]; if (half_us<HALF_MIN_US || half_us>HALF_MAX_US) half_us=HALF_US_DEFAULT;
  const uint32_t usable=(half_us>(ZC_BLANK_US+MOS_OFF_GUARD_US+20))?(half_us-ZC_BLANK_US-MOS_OFF_GUARD_US):(half_us/2);
  auto on_cb  = (ch==0)?mos_on_cb_ch0:mos_on_cb_ch1;
  auto off_cb = (ch==0)?mos_off_cb_ch0:mos_off_cb_ch1;

  if (chCutMode[ch]==CUT_TRAILING){
    uint32_t on_time=(uint32_t)((uint64_t)lvl*usable/255u); if (on_time==0) return;
    uint32_t t_on=ZC_BLANK_US, t_off=ZC_BLANK_US+on_time, latest_off=half_us-MOS_OFF_GUARD_US;
    if (t_off>latest_off) t_off=latest_off; if (t_on>=t_off) return;
    add_alarm_in_us((int64_t)t_on, on_cb, nullptr, true);
    add_alarm_in_us((int64_t)t_off, off_cb, nullptr, true);
  } else {
    uint32_t delay=ZC_BLANK_US + (uint32_t)((uint64_t)(255-lvl)*usable/255u);
    uint32_t t_off=half_us-MOS_OFF_GUARD_US; if (delay>=t_off) return;
    add_alarm_in_us((int64_t)delay, on_cb, nullptr, true);
    add_alarm_in_us((int64_t)t_off, off_cb, nullptr, true);
  }
}
void zc_isr_ch0(){ zc_isr_common(0); }
void zc_isr_ch1(){ zc_isr_common(1); }

// ================== Mapping helpers ==================
constexpr double LAMP_LOG_MAX=6.0;
inline uint8_t clamp8(int v){ return (uint8_t)constrain(v,0,255); }
uint8_t mapLampPercentToLevel(uint8_t ch,double pct){ if(pct<=0.0) return 0; if(pct>100.0) pct=100.0; if(pct<=1.0) return chLower[ch];
  const double L=1.0+(LAMP_LOG_MAX-1.0)*((pct-1.0)/99.0); const double t=(L-1.0)/(LAMP_LOG_MAX-1.0);
  const double span=(double)(chUpper[ch]-chLower[ch]); int lvl=(int)lround(chLower[ch]+t*span);
  if(lvl>0 && lvl<chLower[ch]) lvl=chLower[ch]; if(lvl>chUpper[ch]) lvl=chUpper[ch]; return clamp8(lvl);}
uint8_t mapHeaterPercentToLevel(uint8_t ch,double pct){ if(pct<=0.0) return 0; if(pct<1.0) return chLower[ch]; if(pct>100.0) pct=100.0;
  double span=(double)(chUpper[ch]-chLower[ch]); int lvl=(int)lround(chLower[ch]+(span*(pct/100.0)));
  if(lvl<chLower[ch]) lvl=chLower[ch]; if(lvl>chUpper[ch]) lvl=chUpper[ch]; return clamp8(lvl);}
uint8_t mapKeyPercentToLevel(uint8_t ch,double pct){ return (pct<=0.0)?0:chUpper[ch]; }
uint8_t mapPercentToLevel(uint8_t ch,double pct){ switch((LoadType)chLoadType[ch]){case LOAD_LAMP: return mapLampPercentToLevel(ch,pct); case LOAD_HEATER: return mapHeaterPercentToLevel(ch,pct); case LOAD_KEY: return mapKeyPercentToLevel(ch,pct);} return mapLampPercentToLevel(ch,pct); }
void setLevelDirect(uint8_t ch,uint8_t lvl){ chLevel[ch]=lvl; if(lvl>0) chLastNonZero[ch]=lvl; mb.setHreg(400+ch,chLevel[ch]); }

// ================== Defaults / persist ==================
static inline void setThresholds(uint8_t ch,int lower,int upper);
static inline void clampAndApplyPreset(uint8_t ch){ if(ch>=NUM_CH) return; if(chPreset[ch]>0){ if(chPreset[ch]<chLower[ch]) chPreset[ch]=chLower[ch]; if(chPreset[ch]>chUpper[ch]) chPreset[ch]=chUpper[ch]; } }
void initFreqEstimator(){ for(int i=0;i<NUM_CH;i++){ zcPrevEdgeUs64[i]=0; zcHalfUsLatest[i]=HALF_US_DEFAULT; zcSampleSeq[i]=0; med3[i][0]=med3[i][1]=med3[i][2]=HALF_US_DEFAULT; medIdx[i]=0; for(int k=0;k<AVG_N;k++) avgBuf[i][k]=HALF_US_DEFAULT; avgIdx[i]=0; avgSum[i]=(uint64_t)HALF_US_DEFAULT*AVG_N; lastSeqConsumed[i]=0; freq_x100[i]=5000; gateHalfUs[i]=HALF_US_DEFAULT; } }
void setDefaults(){
  for(int i=0;i<NUM_DI;i++){ diCfg[i].enabled=true; diCfg[i].inverted=false; diCfg[i].switchType=DI_SW_MOMENTARY;
    for(int p=0;p<PRESS_COUNT;p++){ diCfg[i].pressAction[p]=DI_ACT_NONE; diCfg[i].pressTarget[p]=DI_TGT_NONE; }
    diCfg[i].latchMode=LATCH_TOGGLE_TO_PRESET_OR_0; diCfg[i].latchTarget=DI_TGT_NONE; }
  for(int i=0;i<NUM_CH;i++) chCfg[i]={true,HM_PWR_OFF}; for(int i=0;i<NUM_LED;i++) ledCfg[i]={0,0}; for(int i=0;i<NUM_BTN;i++) btnCfg[i]={0};
  for(int i=0;i<NUM_CH;i++){ chLevel[i]=0; chLastNonZero[i]=200; chPulseUntil[i]=0; zcLastEdgeMs[i]=0; zcOk[i]=zcPrevOk[i]=false; zcOkStreak[i]=zcFaultStreak[i]=0; chLower[i]=20; chUpper[i]=255; chLoadType[i]=LOAD_LAMP; chPctX10[i]=0; chCutMode[i]=CUT_LEADING; chPreset[i]=200; }
  g_mb_address=3; g_mb_baud=19200; initFreqEstimator();
}

// ===== Persist helpers =====
bool readOutputStateSnapshot(uint8_t levelOut[NUM_CH], uint8_t lnzOut[NUM_CH]){
  File f=LittleFS.open(OUT_STATE_PATH,"r"); if(!f) return false;
  if((size_t)f.size()!=sizeof(OutputStateSnapshot)){ f.close(); return false; }
  OutputStateSnapshot snap{}; size_t n=f.read((uint8_t*)&snap,sizeof(snap)); f.close();
  if(n!=sizeof(snap)) return false;
  if(snap.magic!=OUT_STATE_MAGIC || snap.version!=OUT_STATE_VERSION || snap.size!=sizeof(OutputStateSnapshot)) return false;
  OutputStateSnapshot tmp=snap; uint32_t crc=tmp.crc32; tmp.crc32=0;
  if(crc32_update(0,(const uint8_t*)&tmp,sizeof(tmp))!=crc) return false;
  memcpy(levelOut,snap.chLevel,sizeof(snap.chLevel)); memcpy(lnzOut,snap.chLastNonZero,sizeof(snap.chLastNonZero));
  return true;
}
bool saveOutputStateSnapshot(){
  OutputStateSnapshot snap{};
  snap.magic=OUT_STATE_MAGIC; snap.version=OUT_STATE_VERSION; snap.size=sizeof(OutputStateSnapshot);
  for(int i=0;i<NUM_CH;i++){ snap.chLevel[i]=chLevel[i]; snap.chLastNonZero[i]=chLastNonZero[i]; }
  snap.crc32=0; snap.crc32=crc32_update(0,(const uint8_t*)&snap,sizeof(snap));
  File f=LittleFS.open(OUT_STATE_PATH,"w"); if(!f) return false;
  size_t n=f.write((const uint8_t*)&snap,sizeof(snap)); f.flush(); f.close();
  return n==sizeof(snap);
}
void applyPowerOnOutputs(){
  uint8_t restoredLevel[NUM_CH]={0,0}, restoredLNZ[NUM_CH]={200,200};
  bool haveSnap=readOutputStateSnapshot(restoredLevel,restoredLNZ);
  for(int i=0;i<NUM_CH;i++){
    chPulseUntil[i]=0;
    uint8_t lvl=0;
    if(chCfg[i].powerOn==HM_PWR_ON){
      lvl=chPreset[i]?chPreset[i]:chLower[i];
    } else if(chCfg[i].powerOn==HM_PWR_RESTORE && haveSnap){
      chLastNonZero[i]=constrain(restoredLNZ[i],chLower[i],chUpper[i]);
      lvl=restoredLevel[i];
      if(lvl>0 && lvl<chLower[i]) lvl=0; else if(lvl>chUpper[i]) lvl=chUpper[i];
    }
    setLevelDirect(i,lvl);
  }
  for(int i=0;i<NUM_CH;i++) prevChLevel[i]=chLevel[i];
  outTrackInit=true; lastOutChangeMs=millis();
}
void maybePersistOutputState(uint32_t now){
  bool needRestore=false;
  for(int i=0;i<NUM_CH;i++){ if(chCfg[i].powerOn==HM_PWR_RESTORE){ needRestore=true; break; } }
  if(!needRestore) return;
  if((uint32_t)(now-lastOutChangeMs)<OUT_AUTOSAVE_MS) return;
  if(lastOutSaveMs && (uint32_t)(now-lastOutSaveMs)<OUT_AUTOSAVE_MS) return;
  if(saveOutputStateSnapshot()) lastOutSaveMs=now;
}
void captureToPersist(PersistConfig &pc){
  pc.magic=CFG_MAGIC; pc.version=CFG_VERSION; pc.size=sizeof(PersistConfig);
  memcpy(pc.diCfg,diCfg,sizeof(diCfg)); memcpy(pc.chCfg,chCfg,sizeof(chCfg)); memcpy(pc.ledCfg,ledCfg,sizeof(ledCfg)); memcpy(pc.btnCfg,btnCfg,sizeof(btnCfg));
  for(int i=0;i<NUM_CH;i++){ pc.chLower[i]=chLower[i]; pc.chUpper[i]=chUpper[i]; pc.chLoadType[i]=chLoadType[i]; pc.chPctX10[i]=chPctX10[i]; pc.chCutMode[i]=chCutMode[i]; pc.chPreset[i]=chPreset[i]; }
  pc.mb_address=g_mb_address; pc.mb_baud=g_mb_baud; pc.crc32=0; pc.crc32=crc32_update(0,(const uint8_t*)&pc,sizeof(PersistConfig));
}
bool applyFromPersistV6(const PersistConfigV6 &pc){
  if(pc.magic!=CFG_MAGIC || pc.size!=sizeof(PersistConfigV6)) return false; PersistConfigV6 tmp=pc; uint32_t crc=tmp.crc32; tmp.crc32=0;
  if(crc32_update(0,(const uint8_t*)&tmp,sizeof(PersistConfigV6))!=crc) return false; if(pc.version!=CFG_VERSION_V6) return false;
  memcpy(diCfg,pc.diCfg,sizeof(diCfg));
  for(int i=0;i<NUM_CH;i++) chCfg[i]={pc.chCfg[i].enabled,HM_PWR_OFF};
  memcpy(ledCfg,pc.ledCfg,sizeof(ledCfg)); memcpy(btnCfg,pc.btnCfg,sizeof(btnCfg));
  for(int i=0;i<NUM_CH;i++){ chLower[i]=pc.chLower[i]; chUpper[i]=pc.chUpper[i];
    chLoadType[i]=(pc.chLoadType[i]<=LOAD_KEY)?pc.chLoadType[i]:LOAD_LAMP; chPctX10[i]=(pc.chPctX10[i]>1000)?1000:pc.chPctX10[i];
    chCutMode[i]=(pc.chCutMode[i]<=CUT_TRAILING)?pc.chCutMode[i]:CUT_LEADING; chPreset[i]=pc.chPreset[i]; clampAndApplyPreset(i); }
  g_mb_address=pc.mb_address; g_mb_baud=pc.mb_baud; return true;
}
bool applyFromPersist(const PersistConfig &pc){
  if(pc.magic!=CFG_MAGIC || pc.size!=sizeof(PersistConfig)) return false; PersistConfig tmp=pc; uint32_t crc=tmp.crc32; tmp.crc32=0;
  if(crc32_update(0,(const uint8_t*)&tmp,sizeof(PersistConfig))!=crc) return false; if(pc.version!=CFG_VERSION) return false;
  memcpy(diCfg,pc.diCfg,sizeof(diCfg)); memcpy(chCfg,pc.chCfg,sizeof(chCfg)); memcpy(ledCfg,pc.ledCfg,sizeof(ledCfg)); memcpy(btnCfg,pc.btnCfg,sizeof(btnCfg));
  for(int i=0;i<NUM_CH;i++){ chLower[i]=pc.chLower[i]; chUpper[i]=pc.chUpper[i];
    chLoadType[i]=(pc.chLoadType[i]<=LOAD_KEY)?pc.chLoadType[i]:LOAD_LAMP; chPctX10[i]=(pc.chPctX10[i]>1000)?1000:pc.chPctX10[i];
    chCutMode[i]=(pc.chCutMode[i]<=CUT_TRAILING)?pc.chCutMode[i]:CUT_LEADING; chPreset[i]=pc.chPreset[i]; clampAndApplyPreset(i); }
  g_mb_address=pc.mb_address; g_mb_baud=pc.mb_baud; return true;
}
bool saveConfigFS(){ PersistConfig pc{}; captureToPersist(pc); File f=LittleFS.open(CFG_PATH,"w"); if(!f) return false; size_t n=f.write((const uint8_t*)&pc,sizeof(pc)); f.flush(); f.close(); if(n!=sizeof(pc)) return false;
  File r=LittleFS.open(CFG_PATH,"r"); if(!r) return false; if((size_t)r.size()!=sizeof(PersistConfig)){ r.close(); return false; } PersistConfig back{}; size_t nr=r.read((uint8_t*)&back,sizeof(back)); r.close(); if(n!=sizeof(back)) return false;
  PersistConfig tmp2=back; uint32_t crc=tmp2.crc32; tmp2.crc32=0; if(crc32_update(0,(const uint8_t*)&tmp2,sizeof(tmp2))!=crc) return false; wsLog("config: saved to FS"); return true; }
bool loadConfigFS(){
  File f=LittleFS.open(CFG_PATH,"r"); if(!f) return false;
  size_t sz=f.size();
  if(sz==sizeof(PersistConfigV6)){
    PersistConfigV6 pc{}; size_t n=f.read((uint8_t*)&pc,sizeof(pc)); f.close();
    if(n!=sizeof(pc)) return false;
    if(!applyFromPersistV6(pc)) return false;
    cfgDirty=true; lastCfgTouchMs=millis();
    wsLog("config: loaded v6, migrated");
    return true;
  }
  if(sz!=sizeof(PersistConfig)){ f.close(); return false; }
  PersistConfig pc{}; size_t n=f.read((uint8_t*)&pc,sizeof(pc)); f.close();
  if(n!=sizeof(pc)) return false;
  if(!applyFromPersist(pc)) return false;
  wsLog("config: loaded from FS");
  return true;
}
bool initFilesystemAndConfig(){
  if(!LittleFS.begin()){ if(!LittleFS.format()||!LittleFS.begin()){ wsLog("fs: init failed"); return false; } }
  if(loadConfigFS()){ applyPowerOnOutputs(); return true; }
  setDefaults(); applyPowerOnOutputs();
  if(saveConfigFS()) return true;
  if(!LittleFS.format()||!LittleFS.begin()) return false;
  setDefaults(); applyPowerOnOutputs();
  if(saveConfigFS()) return true;
  return false;
}

// ================== SFINAE helper ==================
template <class M> inline auto setSlaveIdIfAvailable(M& m,uint8_t id)->decltype(std::declval<M&>().setSlaveId(uint8_t{}),void()){ m.setSlaveId(id); }
inline void setSlaveIdIfAvailable(...){}

// ================== Modbus map ==================
enum : uint16_t { ISTS_DI_BASE=1, ISTS_CH_BASE=50, ISTS_LED_BASE=90, ISTS_ZC_OK_BASE=120 };
enum : uint16_t { CMD_CH_ON_BASE=200, CMD_CH_OFF_BASE=210, CMD_DI_EN_BASE=300, CMD_DI_DIS_BASE=320 };
enum : uint16_t { HREG_DIM_LEVEL_BASE=400, HREG_DIM_LO_BASE=410, HREG_DIM_HI_BASE=420, HREG_FREQ_X100_BASE=430, HREG_PCT_X10_BASE=440, HREG_LOADTYPE_BASE=460, HREG_CUTMODE_BASE=470, HREG_PRESET_BASE=480 };

// ================== Fw decls (helpers) ==================
void applyModbusSettings(uint8_t addr,uint32_t baud);
void handleValues(JSONVar values);
void handleUnifiedConfig(JSONVar obj);
void handleCommand(JSONVar obj);
JSONVar LedConfigListFromCfg();
void sendWebStatus();
void sendWebCfg();
void sendWebBootstrap();
void processModbusCommandPulses();
void applyActionToTarget(uint8_t target,uint8_t action,uint32_t now);
void clampAndSetLevel(uint8_t ch,int value);

// ================== DI press detection runtime ==================
struct DiRuntime{
  bool cur=false, prev=false;
  uint32_t lastChange=0, pressStart=0;
  bool waitingSecond=false;
  uint32_t firstShortAt=0;
  bool shortThenLongArmed=false;

  // Per-DI, per-channel short-press ping-pong direction (+1 up, -1 down)
  int8_t shortDir[NUM_CH] = {+1, +1};

  // Latching ping-pong
  bool rampActive=false; int8_t rampDir=+1; uint32_t lastRampTick=0;

  // Momentary LONG press continuous ramp
  bool longHold=false;           // true while auto-stepping for LONG
  bool longUsed=false;           // set if at least one LONG step happened
  int8_t longDir=+1;
  uint32_t longLastTick=0;
};
DiRuntime diRt[NUM_DI];

// Cadence for DI LONG press ramping (≈ 1 step per second)
const uint16_t DI_LONG_RAMP_TICK_MS = 1000;
// Dedicated cadence for LATCH ping-pong (1 step/sec)
const uint16_t LATCH_RAMP_TICK_MS = 1000;

const uint16_t RAMP_TICK_MS=80;  // retained for legacy fast ramps

// ---------- LED source evaluation helper ----------
static inline bool ledSrcActive(uint8_t src){
  switch(src){
    case LEDSRC_CH1:  return (chCfg[0].enabled && chLevel[0] > 0);
    case LEDSRC_CH2:  return (chCfg[1].enabled && chLevel[1] > 0);
    case LEDSRC_AC1:  return zcOk[0];
    case LEDSRC_AC2:  return zcOk[1];
    case LEDSRC_DI1:  return diRt[0].cur;
    case LEDSRC_DI2:  return diRt[1].cur;
    case LEDSRC_DI3:  return diRt[2].cur;
    case LEDSRC_DI4:  return diRt[3].cur;
    default:          return false; // NONE/unknown
  }
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
    cfg["in"][i]["switchType"] = diCfg[i].switchType;
    cfg["in"][i]["pressShort"]["action"]       = diCfg[i].pressAction[PRESS_SHORT];
    cfg["in"][i]["pressShort"]["target"]       = diCfg[i].pressTarget[PRESS_SHORT];
    cfg["in"][i]["pressLong"]["action"]        = diCfg[i].pressAction[PRESS_LONG];
    cfg["in"][i]["pressLong"]["target"]        = diCfg[i].pressTarget[PRESS_LONG];
    cfg["in"][i]["pressDoubleShort"]["action"] = diCfg[i].pressAction[PRESS_DOUBLE_SHORT];
    cfg["in"][i]["pressDoubleShort"]["target"] = diCfg[i].pressTarget[PRESS_DOUBLE_SHORT];
    cfg["in"][i]["pressShortThenLong"]["action"] = diCfg[i].pressAction[PRESS_SHORT_THEN_LONG];
    cfg["in"][i]["pressShortThenLong"]["target"] = diCfg[i].pressTarget[PRESS_SHORT_THEN_LONG];
    cfg["in"][i]["latchMode"]   = diCfg[i].latchMode;
    cfg["in"][i]["latchTarget"] = diCfg[i].latchTarget;
  }
  for (int i = 0; i < NUM_CH; i++) {
    cfg["ext"]["ch"][i]["enabled"]  = chCfg[i].enabled ? 1 : 0;
    cfg["ext"]["ch"][i]["powerOn"]  = (int)chCfg[i].powerOn;
    cfg["ext"]["ch"][i]["lower"]    = (int)chLower[i];
    cfg["ext"]["ch"][i]["upper"]    = (int)chUpper[i];
    cfg["ext"]["ch"][i]["loadType"] = (int)chLoadType[i];
    cfg["ext"]["ch"][i]["cutMode"]  = (int)chCutMode[i];
    cfg["ext"]["ch"][i]["preset"]   = (int)chPreset[i];
  }
  for (int i = 0; i < NUM_BTN; i++) {
    cfg["btn"][i]["action"] = btnCfg[i].action;
  }
  JSONVar ledList = LedConfigListFromCfg();
  for (int i = 0; i < NUM_LED; i++) {
    cfg["led"][i]["mode"]   = ledList[i]["mode"];
    cfg["led"][i]["source"] = ledList[i]["source"];
  }
  WebSerial.send("cfg", cfg);
}

void sendWebBootstrap() {
  sendWebStatus();
  sendWebCfg();
}

// ================== Setup ==================
void setup(){
  Serial.begin(57600);

  // GPIO
  for(uint8_t i=0;i<NUM_DI;i++) pinMode(DI_PINS[i], INPUT);
  for(uint8_t i=0;i<NUM_LED;i++){ pinMode(LED_PINS[i], OUTPUT); digitalWrite(LED_PINS[i], LOW); }
  for(uint8_t i=0;i<NUM_BTN;i++) pinMode(BTN_PINS[i], INPUT); // HIGH=pressed

  for(uint8_t i=0;i<NUM_CH;i++){ pinMode(GATE_PINS[i], OUTPUT); digitalWrite(GATE_PINS[i], LOW); }
  pinMode(ZC_PINS[0], INPUT_PULLUP); pinMode(ZC_PINS[1], INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ZC_PINS[0]), zc_isr_ch0, FALLING);
  attachInterrupt(digitalPinToInterrupt(ZC_PINS[1]), zc_isr_ch1, FALLING);

  setDefaults(); initFreqEstimator(); initFilesystemAndConfig();

  uint32_t now=millis(); for(int i=0;i<NUM_CH;i++){ zcLastEdgeMs[i]=now; zcOk[i]=zcPrevOk[i]=false; zcOkStreak[i]=zcFaultStreak[i]=0; }

  // Serial2 / Modbus
  Serial2.setTX(TX2); Serial2.setRX(RX2); Serial2.begin(g_mb_baud); mb.config(g_mb_baud);
  setSlaveIdIfAvailable(mb, g_mb_address); mb.setAdditionalServerData("DIM-420-R1 RP2350A");

  // Modbus maps
  for(uint16_t i=0;i<NUM_DI;i++)  mb.addIsts(ISTS_DI_BASE + i);
  for(uint16_t i=0;i<NUM_CH;i++)  mb.addIsts(ISTS_CH_BASE + i);
  for(uint16_t i=0;i<NUM_LED;i++) mb.addIsts(ISTS_LED_BASE + i);
  for(uint16_t i=0;i<NUM_CH;i++)  mb.addIsts(ISTS_ZC_OK_BASE + i);
  for(uint16_t i=0;i<NUM_CH;i++){ mb.addHreg(HREG_DIM_LEVEL_BASE + i, chLevel[i]); mb.addHreg(HREG_DIM_LO_BASE + i, chLower[i]); mb.addHreg(HREG_DIM_HI_BASE + i, chUpper[i]); mb.addHreg(HREG_FREQ_X100_BASE + i, freq_x100[i]); mb.addHreg(HREG_PCT_X10_BASE + i, chPctX10[i]); mb.addHreg(HREG_LOADTYPE_BASE + i, chLoadType[i]); mb.addHreg(HREG_CUTMODE_BASE + i, chCutMode[i]); mb.addHreg(HREG_PRESET_BASE + i, chPreset[i]); }
  for(uint16_t i=0;i<NUM_CH;i++){ mb.addCoil(CMD_CH_ON_BASE + i);  mb.setCoil(CMD_CH_ON_BASE + i, false); }
  for(uint16_t i=0;i<NUM_CH;i++){ mb.addCoil(CMD_CH_OFF_BASE + i); mb.setCoil(CMD_CH_OFF_BASE + i, false); }
  for(uint16_t i=0;i<NUM_DI;i++){ mb.addCoil(CMD_DI_EN_BASE + i);  mb.setCoil(CMD_DI_EN_BASE + i, false); }
  for(uint16_t i=0;i<NUM_DI;i++){ mb.addCoil(CMD_DI_DIS_BASE + i); mb.setCoil(CMD_DI_DIS_BASE + i, false); }

  hmRegisterIdentity(mb, HM_MODEL_ID, HM_FW_MAJOR, HM_FW_MINOR, HM_FW_PATCH, HM_MAP_VERSION);

  // WebSerial handlers
  WebSerial.on("values",  handleValues);
  WebSerial.on("Config",  handleUnifiedConfig);
  WebSerial.on("command", handleCommand);

  wsLog("Boot OK");
  sendWebBootstrap();
  hmWatchdogArm(4000);
}

// ================== Command handler ==================
void handleCommand(JSONVar obj){
  const char* actC=(const char*)obj["action"]; if(!actC) return; String act=String(actC); act.toLowerCase();
  if(act=="save"){ if(saveConfigFS()) wsLog("Configuration saved"); else wsLog("ERROR: Save failed"); }
  else if(act=="load"){
    if(loadConfigFS()){ applyPowerOnOutputs(); wsLog("Configuration loaded"); sendWebBootstrap(); applyModbusSettings(g_mb_address,g_mb_baud); }
    else wsLog("ERROR: Load failed/invalid");
  }
  else if(act=="factory"){
    LittleFS.remove(OUT_STATE_PATH); setDefaults(); applyPowerOnOutputs();
    if(saveConfigFS()){ wsLog("Factory defaults restored & saved"); sendWebBootstrap(); applyModbusSettings(g_mb_address,g_mb_baud); }
    else wsLog("ERROR: Save after factory reset failed");
  }
}
void applyModbusSettings(uint8_t addr,uint32_t baud){
  if(g_mb_baud!=baud){ Serial2.end(); Serial2.begin(baud); mb.config(baud); }
  setSlaveIdIfAvailable(mb, addr);
  g_mb_address=addr; g_mb_baud=baud;
}

// ================== WebSerial config handlers ==================
void handleValues(JSONVar values){
  int addr=(int)values["mb_address"], baud=(int)values["mb_baud"]; addr=hmValidAddress(addr); baud=hmValidBaud(baud);
  applyModbusSettings((uint8_t)addr,(uint32_t)baud);
  wsLog("Modbus configuration updated");
  sendWebStatus();
  cfgDirty=true; lastCfgTouchMs=millis();
}

// Contract t: in.*, ext.ch, btn, led — legacy aliases accepted.
void handleUnifiedConfig(JSONVar obj){
  const char* t=(const char*)obj["t"]; JSONVar list=obj["list"]; if(!t) return;
  String type=String(t); bool changed=false;

  if(type=="in.enabled" || type=="inputEnable"){
    for(int i=0;i<NUM_DI && i<list.length();i++) diCfg[i].enabled=(bool)list[i];
    wsLog("Input Enabled list updated"); changed=true;
  }
  else if(type=="in.invert" || type=="inputInvert"){
    for(int i=0;i<NUM_DI && i<list.length();i++) diCfg[i].inverted=(bool)list[i];
    wsLog("Input Invert list updated"); changed=true;
  }
  else if(type=="in.switchType" || type=="inputSwitchType"){
    for(int i=0;i<NUM_DI && i<list.length();i++) diCfg[i].switchType=(uint8_t)constrain((int)list[i],0,1);
    wsLog("Input Switch Type list updated"); changed=true;
  }
  else if(type=="in.pressShort" || type=="inputPressActionShort" || type=="inputPressTargetShort"){
    for(int i=0;i<NUM_DI && i<list.length();i++){
      if(type=="inputPressActionShort") diCfg[i].pressAction[PRESS_SHORT]=(uint8_t)constrain((int)list[i],0,8);
      else if(type=="inputPressTargetShort") diCfg[i].pressTarget[PRESS_SHORT]=(uint8_t)list[i];
      else {
        if(list[i].hasOwnProperty("action")) diCfg[i].pressAction[PRESS_SHORT]=(uint8_t)constrain((int)list[i]["action"],0,8);
        if(list[i].hasOwnProperty("target")) diCfg[i].pressTarget[PRESS_SHORT]=(uint8_t)list[i]["target"];
      }
    }
    wsLog("Input Short press mapping updated"); changed=true;
  }
  else if(type=="in.pressLong" || type=="inputPressActionLong" || type=="inputPressTargetLong"){
    for(int i=0;i<NUM_DI && i<list.length();i++){
      if(type=="inputPressActionLong") diCfg[i].pressAction[PRESS_LONG]=(uint8_t)constrain((int)list[i],0,8);
      else if(type=="inputPressTargetLong") diCfg[i].pressTarget[PRESS_LONG]=(uint8_t)list[i];
      else {
        if(list[i].hasOwnProperty("action")) diCfg[i].pressAction[PRESS_LONG]=(uint8_t)constrain((int)list[i]["action"],0,8);
        if(list[i].hasOwnProperty("target")) diCfg[i].pressTarget[PRESS_LONG]=(uint8_t)list[i]["target"];
      }
    }
    wsLog("Input Long press mapping updated"); changed=true;
  }
  else if(type=="in.pressDoubleShort" || type=="inputPressActionDoubleShort" || type=="inputPressTargetDoubleShort"){
    for(int i=0;i<NUM_DI && i<list.length();i++){
      if(type=="inputPressActionDoubleShort") diCfg[i].pressAction[PRESS_DOUBLE_SHORT]=(uint8_t)constrain((int)list[i],0,8);
      else if(type=="inputPressTargetDoubleShort") diCfg[i].pressTarget[PRESS_DOUBLE_SHORT]=(uint8_t)list[i];
      else {
        if(list[i].hasOwnProperty("action")) diCfg[i].pressAction[PRESS_DOUBLE_SHORT]=(uint8_t)constrain((int)list[i]["action"],0,8);
        if(list[i].hasOwnProperty("target")) diCfg[i].pressTarget[PRESS_DOUBLE_SHORT]=(uint8_t)list[i]["target"];
      }
    }
    wsLog("Input Double-short press mapping updated"); changed=true;
  }
  else if(type=="in.pressShortThenLong" || type=="inputPressActionShortThenLong" || type=="inputPressTargetShortThenLong"){
    for(int i=0;i<NUM_DI && i<list.length();i++){
      if(type=="inputPressActionShortThenLong") diCfg[i].pressAction[PRESS_SHORT_THEN_LONG]=(uint8_t)constrain((int)list[i],0,8);
      else if(type=="inputPressTargetShortThenLong") diCfg[i].pressTarget[PRESS_SHORT_THEN_LONG]=(uint8_t)list[i];
      else {
        if(list[i].hasOwnProperty("action")) diCfg[i].pressAction[PRESS_SHORT_THEN_LONG]=(uint8_t)constrain((int)list[i]["action"],0,8);
        if(list[i].hasOwnProperty("target")) diCfg[i].pressTarget[PRESS_SHORT_THEN_LONG]=(uint8_t)list[i]["target"];
      }
    }
    wsLog("Input Short-then-long press mapping updated"); changed=true;
  }
  else if(type=="in.latchMode" || type=="inputLatchMode"){
    for(int i=0;i<NUM_DI && i<list.length();i++) diCfg[i].latchMode=(uint8_t)constrain((int)list[i],0,1);
    wsLog("Input Latch Mode list updated"); changed=true;
  }
  else if(type=="in.latchTarget" || type=="inputLatchTarget"){
    for(int i=0;i<NUM_DI && i<list.length();i++) diCfg[i].latchTarget=(uint8_t)list[i];
    wsLog("Input Latch Target list updated"); changed=true;
  }
  else if(type=="ext.ch" || type=="ext.channels" || type=="channels"){
    bool settingsChanged=false;
    for(int i=0;i<NUM_CH && i<list.length();i++){
      if(list[i].hasOwnProperty("enabled")){
        bool en=(bool)list[i]["enabled"];
        if(en!=chCfg[i].enabled){ chCfg[i].enabled=en; settingsChanged=true; }
      }
      if(list[i].hasOwnProperty("powerOn")){
        uint8_t po=(uint8_t)constrain((int)list[i]["powerOn"],0,2);
        if(po!=chCfg[i].powerOn){ chCfg[i].powerOn=po; settingsChanged=true; }
      }
      if(list[i].hasOwnProperty("lower") || list[i].hasOwnProperty("upper")){
        int lo=list[i].hasOwnProperty("lower")?(int)list[i]["lower"]:(int)chLower[i];
        int hi=list[i].hasOwnProperty("upper")?(int)list[i]["upper"]:(int)chUpper[i];
        if(lo!=(int)chLower[i] || hi!=(int)chUpper[i]){ setThresholds(i,lo,hi); settingsChanged=true; }
      }
      if(list[i].hasOwnProperty("loadType")){
        int lt=(int)list[i]["loadType"];
        uint8_t nlt=(uint8_t)constrain(lt,0,2);
        if(nlt!=chLoadType[i]){ chLoadType[i]=nlt; mb.setHreg(HREG_LOADTYPE_BASE + i, chLoadType[i]); settingsChanged=true; }
      }
      if(list[i].hasOwnProperty("cutMode")){
        int cm=(int)list[i]["cutMode"];
        uint8_t ncm=(uint8_t)constrain(cm,0,1);
        if(ncm!=chCutMode[i]){ chCutMode[i]=ncm; mb.setHreg(HREG_CUTMODE_BASE + i, chCutMode[i]); settingsChanged=true; }
      }
      if(list[i].hasOwnProperty("preset")){
        int pv=constrain((int)list[i]["preset"],0,255);
        if(pv!=(int)chPreset[i]){ chPreset[i]=(uint8_t)pv; clampAndApplyPreset(i); mb.setHreg(HREG_PRESET_BASE + i, chPreset[i]); settingsChanged=true; }
      }
      if(list[i].hasOwnProperty("percent")){
        double pct=(double)list[i]["percent"];
        if((LoadType)chLoadType[i]!=LOAD_KEY) pct=constrain(pct,0.0,100.0);
        chPctX10[i]=(uint16_t)constrain((int)lround(pct*10.0),0,1000);
        mb.setHreg(HREG_PCT_X10_BASE + i, chPctX10[i]);
        uint8_t lvl=mapPercentToLevel(i,pct);
        setLevelDirect(i,lvl);
        wsLog("channel["+String(i)+"]: percent="+String((int)pct)+" -> level="+String(lvl));
      } else if(list[i].hasOwnProperty("level")){
        int lvl=(int)list[i]["level"];
        clampAndSetLevel(i,lvl);
      }
      if(!chCfg[i].enabled) clampAndSetLevel(i, 0);
    }
    if(settingsChanged){ changed=true; wsLog("Dimmer channel settings updated"); }
  }
  else if(type=="btn" || type=="buttons"){
    for(int i=0;i<NUM_BTN && i<list.length();i++){
      int a=list[i].hasOwnProperty("action")?(int)list[i]["action"]:(int)list[i];
      btnCfg[i].action=(uint8_t)constrain(a,0,8);
    }
    wsLog("Buttons Configuration updated"); changed=true;
  }
  else if(type=="led" || type=="leds"){
    for(int i=0;i<NUM_LED && i<list.length();i++){
      ledCfg[i].mode   =(uint8_t)constrain((int)list[i]["mode"],0,1);
      ledCfg[i].source =(uint8_t)constrain((int)list[i]["source"], 0, 8);
    }
    wsLog("LEDs Configuration updated"); changed=true;
  }
  else {
    wsLog("Unknown Config type");
  }

  if(changed){ cfgDirty=true; lastCfgTouchMs=millis(); sendWebCfg(); }
}

// ================== Helpers ==================
static inline void setThresholds(uint8_t ch,int lower,int upper){
  if(ch>=NUM_CH) return; lower=constrain(lower,0,255); upper=constrain(upper,0,255); if(upper<lower) upper=lower;
  if((uint8_t)lower==chLower[ch] && (uint8_t)upper==chUpper[ch]) return;
  chLower[ch]=(uint8_t)lower; chUpper[ch]=(uint8_t)upper; chLastNonZero[ch]=constrain(chLastNonZero[ch],chLower[ch],chUpper[ch]); clampAndApplyPreset(ch);
  if(chLevel[ch]>0){ if(chLevel[ch]<chLower[ch]) chLevel[ch]=chLower[ch]; else if(chLevel[ch]>chUpper[ch]) chLevel[ch]=chUpper[ch]; }
  mb.setHreg(HREG_DIM_LO_BASE + ch, chLower[ch]); mb.setHreg(HREG_DIM_HI_BASE + ch, chUpper[ch]); mb.setHreg(HREG_DIM_LEVEL_BASE + ch, chLevel[ch]);
  wsLog("channel["+String(ch)+"]: range=["+String(chLower[ch])+".."+String(chUpper[ch])+"]");
}
void clampAndSetLevel(uint8_t ch,int value){
  if(ch>=NUM_CH) return; value=constrain(value,0,255); uint8_t prev=chLevel[ch]; uint8_t out;
  if(value==0) out=0; else if(value>chUpper[ch]) out=chUpper[ch]; else if(value>=chLower[ch]) out=(uint8_t)value; else out=(prev==0)?chLower[ch]:0;
  setLevelDirect(ch,out); if(out!=prev){ wsLog("channel["+String(ch)+"]: level "+String(prev)+" -> "+String(out)); }
}

// Old helper for mapped button target actions (toggle/pulse demo)
void applyActionToTarget(uint8_t target,uint8_t action,uint32_t now){
  auto doCh=[&](int idx){
    if(idx<0||idx>=NUM_CH) return;
    if(action==1){ // toggle to preset
      if(chLevel[idx]==0) clampAndSetLevel(idx,chPreset[idx]); else clampAndSetLevel(idx,0);
      chPulseUntil[idx]=0;
    } else if(action==2){ // pulse full
      chPulseUntil[idx]=now+PULSE_MS; clampAndSetLevel(idx,255);
    }
  };
  if(action==0) return;
  if(target==4) return;
  if(target==0){ for(int i=0;i<NUM_CH;i++) doCh(i); }
  else if(target>=1 && target<=2){ doCh(target-1); }
  wsLog("button: target="+String(target)+" action="+String(action));
}

// ===== DI actions (generic, keep burst for non-short contexts) =====
void applyDiAction(uint8_t target,uint8_t action,uint32_t now){
  if(action==DI_ACT_NONE || target==DI_TGT_NONE) return;
  auto doCh=[&](int idx){ if(idx<0||idx>=NUM_CH) return; switch(action){
    case DI_ACT_TURN_ON:           if(chLevel[idx]==0) clampAndSetLevel(idx, chPreset[idx]); break;
    case DI_ACT_TURN_OFF:          clampAndSetLevel(idx,0); break;
    case DI_ACT_TOGGLE_OUTPUT:     clampAndSetLevel(idx,(chLevel[idx]==0)?chLastNonZero[idx]:0); break;
    case DI_ACT_INC:               clampAndSetLevel(idx,chLevel[idx]+STEP_DELTA); break;
    case DI_ACT_DEC:               clampAndSetLevel(idx,chLevel[idx]-STEP_DELTA); break;
    case DI_ACT_INC_THEN_DEC: {    // NOTE: kept as burst for non-short uses (e.g., Modbus)
      static const uint8_t burst=8;
      for(uint8_t k=0;k<burst;k++){ if(chLevel[idx]<chUpper[idx]) clampAndSetLevel(idx,chLevel[idx]+STEP_DELTA); }
      if(chLevel[idx]>=chUpper[idx]){ for(uint8_t k=0;k<burst;k++){ if(chLevel[idx]>0) clampAndSetLevel(idx,chLevel[idx]-STEP_DELTA); } }
      break;
    }
    case DI_ACT_TOGGLE_TO_PRESET:  if(chLevel[idx]==0) clampAndSetLevel(idx, chPreset[idx]); else clampAndSetLevel(idx,0); break;
    case DI_ACT_GO_MAX:            clampAndSetLevel(idx, chUpper[idx]); break;
  } };
  if(target==DI_TGT_ALL){ doCh(0); doCh(1); } else if(target==DI_TGT_CH1){ doCh(0); } else if(target==DI_TGT_CH2){ doCh(1); }
  wsLog("DI: target="+String(target)+" action="+String(action));
}

JSONVar LedConfigListFromCfg(){ JSONVar arr; for(int i=0;i<NUM_LED;i++){ JSONVar o; o["mode"]=ledCfg[i].mode; o["source"]=ledCfg[i].source; arr[i]=o; } return arr; }

// ---- helper: one ping-pong step for a channel with external direction ref
static inline void onePingPongStep(int ch, int8_t &dir){
  if(ch<0 || ch>=NUM_CH) return;
  int next = (int)chLevel[ch] + (int)dir * (int)STEP_DELTA;
  if(next >= (int)chUpper[ch]){ next = chUpper[ch]; dir = -1; }
  else if(next <= 0){ next = 0; dir = +1; }
  clampAndSetLevel(ch, next);
}

// ================== Main loop ==================
void loop(){
  hmWatchdogFeed();
  unsigned long now=millis();
  mb.task(); processModbusCommandPulses();
  if(now-lastBlinkToggle>=blinkPeriodMs){ lastBlinkToggle=now; blinkPhase=!blinkPhase; }

  // ZC presence/fault
  for(int c=0;c<NUM_CH;c++){
    bool okNow=((uint32_t)(now-zcLastEdgeMs[c])<=ZC_FAULT_TIMEOUT_MS);
    if(okNow){ if(zcOkStreak[c]<255) zcOkStreak[c]++; zcFaultStreak[c]=0; if(!zcOk[c] && zcOkStreak[c]>=ZC_OK_STREAK_N){ zcOk[c]=true; mb.setIsts(ISTS_ZC_OK_BASE+c,true); wsLog("zc["+String(c)+"]: OK"); } }
    else { if(zcFaultStreak[c]<255) zcFaultStreak[c]++; zcOkStreak[c]=0; if(zcOk[c] && zcFaultStreak[c]>=ZC_FAULT_STREAK_N){ zcOk[c]=false; mb.setIsts(ISTS_ZC_OK_BASE+c,false); wsLog("zc["+String(c)+"]: FAULT"); } }
  }

  // Frequency estimator
  for(uint8_t ch=0; ch<NUM_CH; ++ch){
    uint32_t seq=zcSampleSeq[ch]; if(seq!=lastSeqConsumed[ch]){
      noInterrupts(); uint32_t half=zcHalfUsLatest[ch]; interrupts();
      // Simple estimator: use the latest measured half-period directly.
      // This removes the median/rolling filtering used previously.
      if(half<HALF_MIN_US || half>HALF_MAX_US) half=HALF_US_DEFAULT;
      gateHalfUs[ch]=half;

      double fx100=50000000.0/(double)half; // half_us -> Hz*100
      if(fx100<0.0) fx100=0.0;
      if(fx100>65535.0) fx100=65535.0;
      uint16_t prevF=freq_x100[ch];
      freq_x100[ch]=(uint16_t)lround(fx100);
      if(freq_x100[ch]!=prevF) mb.setHreg(HREG_FREQ_X100_BASE + ch, freq_x100[ch]);
      lastSeqConsumed[ch]=seq;
    }
  }

  // Auto-save
  if(cfgDirty && (now-lastCfgTouchMs>=CFG_AUTOSAVE_MS)){ if(saveConfigFS()) wsLog("config: autosaved"); cfgDirty=false; }
  maybePersistOutputState(now);

  // Track output level changes for /cfg_out.bin debounced save
  if(!outTrackInit){
    for(int i=0;i<NUM_CH;i++) prevChLevel[i]=chLevel[i];
    outTrackInit=true;
  } else {
    for(int i=0;i<NUM_CH;i++){
      if(chLevel[i]!=prevChLevel[i]){ prevChLevel[i]=chLevel[i]; lastOutChangeMs=now; }
    }
  }

  // ================== Buttons (inverted: HIGH = pressed) ==================
  for(int i=0;i<NUM_BTN;i++){
    bool pressed=(digitalRead(BTN_PINS[i])==HIGH);
    buttonPrev[i]=buttonState[i];
    buttonState[i]=pressed;

    // rising edge: apply old single action immediately
    if(!buttonPrev[i] && buttonState[i]){
      btnRt[i].pressed = true;
      btnRt[i].pressStart = now;
      btnRt[i].rampActive = false;
      btnRt[i].lastRampTick = now;

      switch(btnCfg[i].action){
        case 1: /* Toggle CH1 */ if(chLevel[0]==0) clampAndSetLevel(0,chPreset[0]); else clampAndSetLevel(0,0); wsLog("button: toggle CH1"); break;
        case 2: /* Toggle CH2 */ if(chLevel[1]==0) clampAndSetLevel(1,chPreset[1]); else clampAndSetLevel(1,0); wsLog("button: toggle CH2"); break;

        // Old logic single-step immediately (hold will start auto-ramp later if long)
        case 3: clampAndSetLevel(0, chLevel[0]+STEP_DELTA); wsLog("button: stepUp CH1"); break;
        case 4: clampAndSetLevel(0, chLevel[0]-STEP_DELTA); wsLog("button: stepDown CH1"); break;
        case 5: clampAndSetLevel(1, chLevel[1]+STEP_DELTA); wsLog("button: stepUp CH2"); break;
        case 6: clampAndSetLevel(1, chLevel[1]-STEP_DELTA); wsLog("button: stepDown CH2"); break;

        // New: go to MAX threshold
        case 7: clampAndSetLevel(0, chUpper[0]); wsLog("button: MAX CH1"); break;
        case 8: clampAndSetLevel(1, chUpper[1]); wsLog("button: MAX CH2"); break;

        default: break;
      }
    }

    // while held: after LONG_MIN_MS, if action is step up/down -> enable ramp
    if(buttonState[i]){
      if(!btnRt[i].rampActive && (now - btnRt[i].pressStart) >= LONG_MIN_MS){
        if(btnCfg[i].action==3 || btnCfg[i].action==4 || btnCfg[i].action==5 || btnCfg[i].action==6){
          btnRt[i].rampActive = true;
          btnRt[i].lastRampTick = now;
          wsLog(String("button: ramp start #")+String(i+1));
        }
      }
      if(btnRt[i].rampActive && (now - btnRt[i].lastRampTick) >= BTN_RAMP_TICK_MS){
        btnRt[i].lastRampTick = now;
        // perform repeated steps
        switch(btnCfg[i].action){
          case 3: clampAndSetLevel(0, chLevel[0]+STEP_DELTA); break;
          case 4: clampAndSetLevel(0, chLevel[0]-STEP_DELTA); break;
          case 5: clampAndSetLevel(1, chLevel[1]+STEP_DELTA); break;
          case 6: clampAndSetLevel(1, chLevel[1]-STEP_DELTA); break;
          default: break;
        }
      }
    }

    // falling edge: stop ramp
    if(buttonPrev[i] && !buttonState[i]){
      if(btnRt[i].rampActive){
        btnRt[i].rampActive=false;
        wsLog(String("button: ramp stop #")+String(i+1));
      }
      btnRt[i].pressed=false;
    }
  }

  // ================== Digital Inputs ==================
  for(int i=0;i<NUM_DI;i++){
    bool val=false; if(diCfg[i].enabled){ val=(digitalRead(DI_PINS[i])==HIGH); if(diCfg[i].inverted) val=!val; }
    DiRuntime &rt=diRt[i]; rt.prev=rt.cur; rt.cur=val; mb.setIsts(ISTS_DI_BASE + i, val);
    uint32_t nowMs=millis(); bool rising=(!rt.prev && rt.cur), falling=(rt.prev && !rt.cur);

    const uint8_t shortAct = diCfg[i].pressAction[PRESS_SHORT];
    const uint8_t shortTgt = diCfg[i].pressTarget[PRESS_SHORT];

    const uint8_t longAct   = diCfg[i].pressAction[PRESS_LONG];
    const uint8_t longTgt   = diCfg[i].pressTarget[PRESS_LONG];
    const bool longIsRamp   = (longAct==DI_ACT_INC || longAct==DI_ACT_DEC || longAct==DI_ACT_INC_THEN_DEC);

    // For LATCHING switches, act on ANY state change (both edges)
    if(diCfg[i].switchType==DI_SW_LATCHING && (rising || falling)){
      if(diCfg[i].latchMode==LATCH_TOGGLE_TO_PRESET_OR_0){
        // Toggle to preset/0 every time the DI toggles
        applyDiAction(diCfg[i].latchTarget, DI_ACT_TOGGLE_TO_PRESET, nowMs);
        wsLog(String("DI")+String(i+1)+": latch toggle preset/0 on edge");
      } else { // LATCH_PINGPONG_UNTIL_TOGGLE
        // Toggle the continuous 1Hz ping-pong on each DI edge
        rt.rampActive = !rt.rampActive;
        if(rt.rampActive){
          rt.rampDir = +1;                 // start by increasing
          rt.lastRampTick = nowMs;
          wsLog(String("DI")+String(i+1)+": latch ping-pong START");
        } else {
          wsLog(String("DI")+String(i+1)+": latch ping-pong STOP");
        }
      }
      rt.lastChange=nowMs;
    }

    if(rising){
      rt.pressStart=nowMs;
      rt.longHold=false; rt.longUsed=false; rt.longDir=+1; rt.longLastTick=nowMs;
      rt.lastChange=nowMs;
    } else if(falling){
      uint32_t dur=nowMs-rt.pressStart;

      if(diCfg[i].switchType==DI_SW_MOMENTARY){
        if(rt.longUsed){
          rt.longHold=false; rt.longUsed=false;
          { String msg="DI"; msg+=String(i+1); msg+=": long-ramp stop"; wsLog(msg); }
        }else{
          if(dur>=LONG_MIN_MS){
            applyDiAction(longTgt, longAct, nowMs);
          } else if(dur<=SHORT_MAX_MS){
            // stage for double-short; will resolve below after timeout
            if(!rt.waitingSecond){ rt.waitingSecond=true; rt.firstShortAt=nowMs; rt.shortThenLongArmed=true; }
            else { applyDiAction(diCfg[i].pressTarget[PRESS_DOUBLE_SHORT], diCfg[i].pressAction[PRESS_DOUBLE_SHORT], nowMs); rt.waitingSecond=false; rt.shortThenLongArmed=false; }
          }
        }
      }
      rt.lastChange=nowMs;
    }

    // Resolve pending single SHORT after double-gap window
    if(diCfg[i].switchType==DI_SW_MOMENTARY && rt.waitingSecond){
      if((uint32_t)(nowMs-rt.firstShortAt)>DOUBLE_GAP_MS){
        if(shortAct==DI_ACT_INC_THEN_DEC){
          // One ping-pong step per channel in target, with per-DI direction
          if(shortTgt==DI_TGT_CH1) onePingPongStep(0, rt.shortDir[0]);
          else if(shortTgt==DI_TGT_CH2) onePingPongStep(1, rt.shortDir[1]);
          else if(shortTgt==DI_TGT_ALL){ onePingPongStep(0, rt.shortDir[0]); onePingPongStep(1, rt.shortDir[1]); }
          { String msg="DI"; msg+=String(i+1); msg+=": short ping-pong step"; wsLog(msg); }
        }else{
          // Legacy single-shot actions
          applyDiAction(shortTgt, shortAct, nowMs);
        }
        rt.waitingSecond=false; rt.shortThenLongArmed=false;
      }
    }

    // ---------- Momentary LONG press continuous ramp ----------
    if(diCfg[i].switchType==DI_SW_MOMENTARY && rt.cur && longIsRamp){
      if(!rt.longHold && (uint32_t)(nowMs - rt.pressStart) >= LONG_MIN_MS){
        // start ramp
        rt.longHold=true; rt.longLastTick=nowMs; rt.longUsed=false;
        rt.longDir = (longAct==DI_ACT_DEC) ? -1 : +1;
        { String msg="DI"; msg+=String(i+1); msg+=": long-ramp start (act="; msg+=String(longAct); msg+=")"; wsLog(msg); }
      }
      if(rt.longHold && (uint32_t)(nowMs - rt.longLastTick) >= DI_LONG_RAMP_TICK_MS){
        rt.longLastTick=nowMs; rt.longUsed=true;

        auto stepOne=[&](int ch){
          if(ch<0||ch>=NUM_CH) return;
          int next = (int)chLevel[ch] + (int)rt.longDir * (int)STEP_DELTA;

          if(longAct==DI_ACT_INC){
            if(next >= (int)chUpper[ch]){ next = chUpper[ch]; }
          } else if(longAct==DI_ACT_DEC){
            if(next <= 0){ next = 0; }
          } else { // INC_THEN_DEC ping-pong
            if(next >= (int)chUpper[ch]){ next = chUpper[ch]; rt.longDir = -1; }
            if(next <= 0){ next = 0; rt.longDir = +1; }
          }

          clampAndSetLevel(ch, next);
        };

        if(longTgt==DI_TGT_CH1) stepOne(0);
        else if(longTgt==DI_TGT_CH2) stepOne(1);
        else if(longTgt==DI_TGT_ALL){ stepOne(0); stepOne(1); }
      }
    }

    // Latching ping-pong at 1 Hz until next DI toggle
    if(diCfg[i].switchType==DI_SW_LATCHING && diCfg[i].latchMode==LATCH_PINGPONG_UNTIL_TOGGLE && rt.rampActive){
      if((uint32_t)(nowMs-rt.lastRampTick)>=LATCH_RAMP_TICK_MS){
        rt.lastRampTick=nowMs;
        auto rampOne=[&](int ch){
          if(ch<0||ch>=NUM_CH) return;
          int next = (int)chLevel[ch] + (rt.rampDir>0?STEP_DELTA:-STEP_DELTA);
          if(next>=(int)chUpper[ch]){ next=chUpper[ch]; rt.rampDir=-1; }
          if(next<=0){ next=0; rt.rampDir=+1; }
          clampAndSetLevel(ch,next);
        };
        if(diCfg[i].latchTarget==DI_TGT_CH1) rampOne(0);
        else if(diCfg[i].latchTarget==DI_TGT_CH2) rampOne(1);
        else if(diCfg[i].latchTarget==DI_TGT_ALL){ rampOne(0); rampOne(1); }
      }
    }
  }

  // Pulse timeout restore
  for(int c=0;c<NUM_CH;c++){ if(chPulseUntil[c]!=0 && timeAfter32(now,chPulseUntil[c])){ clampAndSetLevel(c,chLastNonZero[c]); chPulseUntil[c]=0; wsLog("channel["+String(c)+"]: pulse end -> restore"); } }

  // LEDs drive + Modbus mirror
  JSONVar ledStateList;
  for(int i=0;i<NUM_LED;i++){
    bool srcActive = ledSrcActive(ledCfg[i].source);
    bool phys = (ledCfg[i].mode==0) ? srcActive : (srcActive && blinkPhase);
    ledStateList[i] = phys ? 1 : 0;
    digitalWrite(LED_PINS[i], phys ? HIGH : LOW);
    mb.setIsts(ISTS_LED_BASE + i, phys);
  }

  // Channel "on" mirror
  for(int c=0;c<NUM_CH;c++){ bool onb=(chCfg[c].enabled && chLevel[c]>0); mb.setIsts(ISTS_CH_BASE + c, onb); }

  // Periodic WebConfig telemetry
  if(millis()-lastSend>=sendInterval){
    lastSend=millis();
    WebSerial.check();
    if(hmUsbCanSend()){
      sendWebStatus();

      JSONVar io;
      for(int i=0;i<NUM_DI;i++) io["in"][i]=diRt[i].cur?1:0;
      for(int i=0;i<NUM_BTN;i++) io["btn"][i]=buttonState[i]?1:0;
      for(int i=0;i<NUM_LED;i++) io["led"][i]=ledStateList[i];
      WebSerial.send("io", io);

      JSONVar ext;
      for(int i=0;i<NUM_CH;i++){
        ext["ch"][i]["level"]=(int)chLevel[i];
        ext["ch"][i]["percent"]=(int)min((int)(chPctX10[i]/10),100);
        ext["ch"][i]["zc_ok"]=zcOk[i]?1:0;
        ext["ch"][i]["freq_x100"]=(int)freq_x100[i];
      }
      WebSerial.send("ext", ext);
    }
  }
}

// ================== Modbus helpers ==================
void processModbusCommandPulses(){
  for(int c=0;c<NUM_CH;c++){
    if(mb.Coil(CMD_CH_ON_BASE + c)){ mb.setCoil(CMD_CH_ON_BASE + c,false); clampAndSetLevel(c,chPreset[c]); wsLog("modbus: CH"+String(c+1)+" ON"); }
    if(mb.Coil(CMD_CH_OFF_BASE + c)){ mb.setCoil(CMD_CH_OFF_BASE + c,false); clampAndSetLevel(c,0); wsLog("modbus: CH"+String(c+1)+" OFF"); }
  }
  for(int i=0;i<NUM_DI;i++){
    if(mb.Coil(CMD_DI_EN_BASE + i)){  mb.setCoil(CMD_DI_EN_BASE + i,false); if(!diCfg[i].enabled){ diCfg[i].enabled=true; cfgDirty=true; lastCfgTouchMs=millis(); wsLog("modbus: DI"+String(i+1)+" enable"); } }
    if(mb.Coil(CMD_DI_DIS_BASE + i)){ mb.setCoil(CMD_DI_DIS_BASE + i,false); if(diCfg[i].enabled){ diCfg[i].enabled=false; cfgDirty=true; lastCfgTouchMs=millis(); wsLog("modbus: DI"+String(i+1)+" disable"); } }
  }
  for(int c=0;c<NUM_CH;c++){
    uint16_t lo=mb.Hreg(HREG_DIM_LO_BASE + c), hi=mb.Hreg(HREG_DIM_HI_BASE + c); if(lo!=chLower[c] || hi!=chUpper[c]){ setThresholds(c,(int)lo,(int)hi); cfgDirty=true; lastCfgTouchMs=millis(); }
    uint16_t lt=constrain((int)mb.Hreg(HREG_LOADTYPE_BASE + c),0,2); if(lt!=chLoadType[c]){ chLoadType[c]=(uint8_t)lt; cfgDirty=true; lastCfgTouchMs=millis(); wsLog("modbus: CH"+String(c+1)+" loadType="+String(lt)); }
    uint16_t cm=constrain((int)mb.Hreg(HREG_CUTMODE_BASE + c),0,1); if(cm!=chCutMode[c]){ chCutMode[c]=(uint8_t)cm; cfgDirty=true; lastCfgTouchMs=millis(); wsLog("modbus: CH"+String(c+1)+" cutMode="+String(cm)); }
    uint16_t pv=constrain((int)mb.Hreg(HREG_PRESET_BASE + c),0,255); if(pv!=chPreset[c]){ chPreset[c]=(uint8_t)pv; clampAndApplyPreset(c); cfgDirty=true; lastCfgTouchMs=millis(); wsLog("modbus: CH"+String(c+1)+" preset="+String((int)chPreset[c])); }
    uint16_t p10=mb.Hreg(HREG_PCT_X10_BASE + c); if(p10>1000 && chLoadType[c]!=LOAD_KEY) p10=1000; if(p10!=chPctX10[c]){ chPctX10[c]=p10; double pct=chPctX10[c]/10.0; uint8_t lvl=mapPercentToLevel(c,pct); setLevelDirect(c,lvl); wsLog("modbus: CH"+String(c+1)+" percent="+String(pct,1)+" -> level="+String(lvl)); }
    uint16_t lvl=mb.Hreg(HREG_DIM_LEVEL_BASE + c); clampAndSetLevel(c,(int)lvl);
  }
}
