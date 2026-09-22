/*
 * STR-3221-R1 v0.2.0 (beta) — 32-channel stair LED controller (4× TLC59208F over I2C)
 *
 * Modbus RTU map (register numbers = Modbus address in this library)
 * -------------------------------------------------------------------
 * Input registers (FC=04) — contiguous runtime block 0..31 (one merged poll).
 * The whole block is fixed here: steps 2f (stair sequencer) and 2g (underfloor
 * heating) FILL registers 26..31, they do not move anything.
 *   0  IREG_DI_MASK       bit0 DI, bit1 IN1, bit2 IN2 (logical, after enable + invert)
 *   1  IREG_BTN_MASK      bit0..3 BUTTON1..4 pressed
 *   2  IREG_LED_MASK      bit0..1 status LED1..2 physical state
 *   3  IREG_STATUS_FLAGS  bit0=tlcReady, bit1=linkOk, bit2=busFailsafe,
 *                         bit3=cfgDirty, bit4=localOverride, bit5=panic,
 *                         bit6=seqRunning (2f), bit7=heatDemand (2g)
 *   4  IREG_TLC_MASK      bit0..3 TLC59208F chip OK U9..U12
 *   5  IREG_I2C_ERRORS    failed TLC transaction count (saturating)
 *   6  IREG_RESET_REASON  0=unknown/POR, 1=watchdog stall, 2=commanded reboot
 *   7  IREG_LINK_AGE_S    seconds since last frame to this slave (saturating)
 *   8  IREG_UPTIME_MIN    minutes since boot (saturating)
 *   9  IREG_ACTIVE_SCENE  0=none, 1..8=last recalled scene; cleared by any
 *                         level change that did not come from a scene
 *  10..25 IREG_OUT_BASE   O1..O32 level readback — low byte even channel,
 *                         high byte odd channel. Reports the COMMANDED level
 *                         (what HR 400..431 / a scene / failsafe / panic asked
 *                         for), not the driver duty after curve and master —
 *                         so a write to HR 400 reads back unchanged.
 *  26  IREG_SEQ_STATE     lo: 0 idle 1 up 2 down 3 hold 4 fade-out;
 *                         hi: direction 0 none · 1 up · 2 down
 *  27  IREG_SEQ_STEP      current step, 0 = none
 *  28  IREG_HEAT_FLAGS    bit0 heat demand, bit1 summer mode, bit2 enable input
 *                         closed, bit3 anti-freeze active, bit4 valve exercise
 *  29  IREG_ZONES_OPEN    number of open zones
 *  30  IREG_ZONE_MASK_LO  open-zone mask, channels 1..16
 *  31  IREG_ZONE_MASK_HI  open-zone mask, channels 17..32
 *
 * Command coils (FC=05/15, auto-clear pulse):
 *   300..302  pulse ENABLE  IO1..IO3
 *   320..322  pulse DISABLE IO1..IO3
 *   330       pulse SAVE output levels to flash (rate-limited 10 s)
 *   331       pulse RELEASE local override
 *   332       pulse ENTER panic — every channel to panicLevel and HELD there
 *   333       pulse ALL OFF — one shot, not latched
 *   334       pulse CLEAR panic
 *   340       pulse stair run UP
 *   341       pulse stair run DOWN
 *   342       pulse stair abort
 *   345       pulse exercise all heat valves now
 *
 * Holding registers (FC=03/06/16) — config / write surface; not polled:
 *   400..431  O1..O32 level 0..255; on a channel with profile=heat, duty 0..100
 *   432       master level 0..255 (multiplier on every channel, default 255)
 *   433       sequencer inhibit 0/1 (running sequence is allowed to finish)
 *   434       night level 0..255 (level only — does not select the window)
 *   435       day level 0..255 (level only — does not select the window)
 *   436       scene recall: write 1..8 applies that scene, reads back 0
 *   437       summer mode 0/1 — heat closed, valve exercise still runs
 *   438       night window 0=day · 1=night — the only register that selects it
 *   480       Modbus slave address (R/W)
 *   481       Modbus baud rate (R/W, whitelist 9600..115200; stored raw,
 *             115200 not representable in uint16 → reads as 0, set via WebConfig)
 *
 * The bus failsafe timeout is deliberately NOT on Modbus: it is configuration,
 * and configuration lives in WebConfig. The bus carries state and runtime
 * commands only.
 *
 * ───────────────────────────────────────────────────────────────────────────
 * THE OUTPUT PIPELINE — one order, one place: resolveChannelDriverValue()
 * ───────────────────────────────────────────────────────────────────────────
 *   source value (Modbus HR / scene / local input or button / auto-off)
 *     → profile      led | heat | raw | indicator
 *     → min/max      rescaled into [minLevel..maxLevel], not clipped at the ends
 *     → curve        linear | gamma 2.2 | CIE 1931, per channel
 *     → master level multiplier over everything, applied last
 *     → ramp         time interpolation towards the resolved value (rampMs)
 *     → block I2C    four auto-increment writes, eight channels each
 *
 * Panic and bus failsafe cut in BEFORE the profile: they replace the source
 * value, never the result, so a channel in panic is still shaped by its own
 * curve, its own min/max and the master level. Heat NC/NO is applied LAST,
 * in the TLC write, so it covers PWM, exercise, frost and DI-close.
 * profile=raw skips min/max and the curve only; the master level still applies
 * to it, otherwise a master dim would silently miss raw channels.
 *
 * SOURCE PRIORITY — one list for the whole module, highest first:
 *   1. DI in role `enable`, open — every heat channel closed. Unconditional.
 *   2. Panic (coil 332) — led/raw/indicator → panicLevel. Heat channels stay
 *      CLOSED: a fire alarm must not throw the valves open.
 *   3. Summer mode — heat closed; valve exercise still runs.
 *   4. Local override — holds outputs; PIR/buttons that start the sequencer
 *      are ignored. The sequencer, if already running, is allowed to finish.
 *   5. Frost protection — heat channels, instead of per-channel failsafe,
 *      after T hours of silence on this slave.
 *   6. Bus failsafe — led/raw/indicator only. Stair-table channels keep
 *      running: a dead bus is when the staircase most has to work.
 *   7. Sequencer — owns step-table channels while a run is in progress.
 *      HR 400..431 writes to those channels are accepted but not applied
 *      until the run ends (then the channel returns to accent / the bus).
 *   8. Slow PWM — owns profile=heat channels.
 *   9. Holding-register writes 400..431.
 *
 * ───────────────────────────────────────────────────────────────────────────
 * CONFIG CONTRACT FOR STEPS 2f AND 2g — declared now so CFG_VERSION never
 * moves again for them. Both blocks exist in PersistConfigV5, are migrated,
 * are zero by default, and are NOT read by any code in this step.
 * ───────────────────────────────────────────────────────────────────────────
 * StairCfg (2f — stair sequencer), all zero = disabled:
 *   uint8_t  stepCount            number of steps in use, 0..32 (0 = off)
 *   uint8_t  stepChannel[32]      step N → output channel index 0..31
 *   uint8_t  mode                 launch: 0 one-shot · 1 hold while presence
 *   uint16_t stepDelayMs          delay between consecutive steps
 *   uint16_t holdS                hold time at full before fade-out
 *   uint16_t fadeOutMs            fade-out phase length
 *   uint8_t  overlapPct           overlap between steps, 0..100 %
 *   uint8_t  retrigger            re-trigger while running: 0 ignore · 1 restart · 2 extend hold
 *   uint8_t  oppose               opposite end triggers: 0 ignore · 1 reverse · 2 hold both
 *   uint8_t  nightLevel           level used in the night window
 *   uint8_t  dayLevel             level used in the day window
 *   uint8_t  standbyFirst         standby backlight level, first step
 *   uint8_t  standbyLast          standby backlight level, last step
 *   uint8_t  nightLightLevel      standby level while the night window is active
 *   uint16_t stepFadeMs           per-step fade
 *   uint16_t debounceMs           sensor debounce
 *   uint16_t minRepeatMs          minimum interval between two runs
 *   uint8_t  pattern              drawing: 0 sequential · 1 all · 2 wave
 *                                 · 3 comet · 4 center · 5 random
 *                                 (not the same byte as mode)
 *   uint8_t  waveWidth            simultaneous steps for wave / comet
 *   uint8_t  inputEndMap          bit0: 0 = IN1 bottom, 1 = IN1 top
 *   uint8_t  bothEnds             both-in-window: 0 default dir · 1 all at once · 2 ignore
 *   uint16_t bothWindowMs         dual-trigger window
 *   uint8_t  _rsv[6]
 * HeatCfg (2g — underfloor heating), all zero = disabled:
 *   uint16_t slowPwmPeriodS       slow-PWM cycle length
 *   uint8_t  phaseSpreadPct       phase spread between zones, 0..100 %
 *   uint8_t  maxOpenZones         limit on simultaneously open zones, 0 = no limit
 *   uint8_t  diRole               discrete input role: 0 none · 1 enable · 2 inhibit · 3 demand echo
 *   uint16_t firstOpenDelayS      first-open delay before pump / boiler demand
 *   uint16_t overrunS             pump overrun after the last zone closes
 *   uint16_t exerciseIntervalH    valve exercise interval, hours
 *   uint16_t exerciseDurationS    valve exercise duration
 *   uint16_t antifreezeHours      anti-freeze threshold, hours without demand
 *   uint8_t  summerMode           1 = heating inhibited
 *   uint8_t  _rsv                 unused alignment byte from 2e
 *   uint8_t  minPulsePct          min duty; below → 0, above 100−min → 100
 *   uint8_t  _rsv2[5]
 * Per channel, ChCfg.flags bit0 is the NC/NO inversion 2g fills in.
 *
 * Packed sizes (static_assert in this file): ChCfg 10, StairCfg 66, HeatCfg 23,
 * PersistConfigV5 886. CFG_VERSION stays 0x0005.
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
#include <math.h>
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
struct PersistConfigV5;
typedef PersistConfigV5 PersistConfig;

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

static const uint8_t NUM_DI     = 3;
static const uint8_t NUM_LED    = 2;
static const uint8_t NUM_BTN    = 4;
static const uint8_t NUM_PWM    = 32;
static const uint8_t NUM_GROUPS = 8;
static const uint8_t NUM_SCENES = 8;

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
struct InCfgV4 { bool enabled; bool inverted; uint8_t action; uint8_t target; uint8_t level; };
// param carries the scene number for action = IN_ACT_SCENE; 0 otherwise.
struct InCfg  { bool enabled; bool inverted; uint8_t action; uint8_t target; uint8_t level; uint8_t param; };
struct LedCfg { uint8_t mode; uint8_t source; };
struct BtnCfgV4 { uint8_t action; };
struct BtnCfg { uint8_t action; uint8_t param; };

enum : uint8_t { FS_HOLD = 0, FS_OFF = 1, FS_LEVEL = 2 };

// Channel profile — decides how the value in HR 400..431 is read.
enum : uint8_t { CH_PROF_LED = 0, CH_PROF_HEAT = 1, CH_PROF_RAW = 2, CH_PROF_IND = 3 };
// Dimming curve applied after min/max.
enum : uint8_t { CH_CURVE_LINEAR = 0, CH_CURVE_GAMMA = 1, CH_CURVE_CIE = 2, CH_CURVE_COUNT = 3 };

// Input actions (diCfg.action). 0..2 are v0.1.0/2d behaviour and must not move.
enum : uint8_t { IN_ACT_NONE = 0, IN_ACT_TOGGLE = 1, IN_ACT_PULSE = 2, IN_ACT_SCENE = 3, IN_ACT_MAX = 3 };
// Button actions (btnCfg.action). 0..4 are 2d behaviour and must not move.
enum : uint8_t {
  BTN_ACT_NONE = 0, BTN_ACT_ALL_ON = 1, BTN_ACT_ALL_OFF = 2, BTN_ACT_RAMP = 3,
  BTN_ACT_CLEAR_OVERRIDE = 4, BTN_ACT_SCENE = 5, BTN_ACT_PANIC = 6,
  BTN_ACT_CLEAR_PANIC = 7, BTN_ACT_MAX = 7
};

struct ChCfg {
  uint8_t  profile;   // CH_PROF_*
  uint8_t  curve;     // CH_CURVE_*
  uint8_t  minLevel;  // bottom of the output window
  uint8_t  maxLevel;  // top of the output window
  uint16_t rampMs;    // 0 = step immediately
  uint16_t autoOffS;  // 0 = no auto-off
  uint8_t  flags;     // bit0 NC/NO inversion — filled in by 2g
  uint8_t  _rsv;
} __attribute__((packed));

// ---- Declared now, used by step 2f. Zero = disabled. See the header contract.
struct StairCfg {
  uint8_t  stepCount;
  uint8_t  stepChannel[NUM_PWM];
  uint8_t  mode;                  // 0 one-shot · 1 hold while presence
  uint16_t stepDelayMs;
  uint16_t holdS;
  uint16_t fadeOutMs;
  uint8_t  overlapPct;
  uint8_t  retrigger;
  uint8_t  oppose;
  uint8_t  nightLevel;
  uint8_t  dayLevel;
  uint8_t  standbyFirst;
  uint8_t  standbyLast;
  uint8_t  nightLightLevel;
  uint16_t stepFadeMs;
  uint16_t debounceMs;
  uint16_t minRepeatMs;
  uint8_t  pattern;               // 0 seq · 1 all · 2 wave · 3 comet · 4 center · 5 random
  uint8_t  waveWidth;
  uint8_t  inputEndMap;           // bit0: 0 IN1=bottom, 1 IN1=top
  uint8_t  bothEnds;              // 0 default dir · 1 all at once · 2 ignore
  uint16_t bothWindowMs;
  uint8_t  _rsv[6];
} __attribute__((packed));

// ---- Declared now, used by step 2g. Zero = disabled. See the header contract.
struct HeatCfg {
  uint16_t slowPwmPeriodS;
  uint8_t  phaseSpreadPct;
  uint8_t  maxOpenZones;
  uint8_t  diRole;
  uint16_t firstOpenDelayS;
  uint16_t overrunS;
  uint16_t exerciseIntervalH;
  uint16_t exerciseDurationS;
  uint16_t antifreezeHours;
  uint8_t  summerMode;
  uint8_t  _rsv;
  uint8_t  minPulsePct;
  uint8_t  _rsv2[5];
} __attribute__((packed));

InCfg  diCfg[NUM_DI];
LedCfg ledCfg[NUM_LED];
BtnCfg btnCfg[NUM_BTN];
ChCfg  chCfg[NUM_PWM];
uint32_t groupMask[NUM_GROUPS];          // bit N = channel N belongs to the group
uint8_t  sceneLevel[NUM_SCENES][NUM_PWM];
uint8_t  sceneUsed[NUM_SCENES];
uint8_t  g_masterLevel = 255;
uint8_t  g_panicLevel  = 255;
StairCfg g_stairCfg;
HeatCfg  g_heatCfg;

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

// ---- Output engine state ----
// chRequest   the COMMANDED level per channel — the source value of the pipeline
// chDriver    the resolved driver value after profile/min-max/curve/master
// pwmLevel    what the TLC is actually holding; the ramp walks it towards chDriver
uint8_t  chRequest[NUM_PWM];
uint8_t  chDriver[NUM_PWM];
uint32_t chAutoOffAtMs[NUM_PWM];
uint32_t chRampLastMs[NUM_PWM];
bool     g_panicActive = false;
uint8_t  g_activeScene = 0;              // 0 = none, else 1..8
static uint8_t g_curveLut[CH_CURVE_COUNT][256];
static const uint32_t RAMP_SERVICE_MS = 10;   // 100 Hz is smooth on an 8-bit PWM
static uint32_t g_lastRampServiceMs = 0;

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
  BtnCfgV4 btnCfg[NUM_BTN];
  uint16_t pwmLevel[NUM_PWM];
  uint8_t  mb_address;
  uint32_t mb_baud;
  uint32_t crc32;
} __attribute__((packed));

struct PersistConfigV4 {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  InCfgV4  diCfg[NUM_DI];
  LedCfg   ledCfg[NUM_LED];
  BtnCfgV4 btnCfg[NUM_BTN];
  uint16_t pwmLevel[NUM_PWM];
  uint8_t  mb_address;
  uint32_t mb_baud;
  uint16_t busTimeoutS;
  uint16_t overrideTimeoutS;
  uint8_t  failsafeAction[NUM_PWM];
  uint8_t  failsafeLevel[NUM_PWM];
  uint32_t crc32;
} __attribute__((packed));

struct PersistConfigV5 {
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
  ChCfg    chCfg[NUM_PWM];
  uint32_t groupMask[NUM_GROUPS];
  uint8_t  sceneLevel[NUM_SCENES][NUM_PWM];
  uint8_t  sceneUsed[NUM_SCENES];
  uint8_t  masterLevel;
  uint8_t  panicLevel;
  StairCfg stair;                 // 2f — carried, never read in this step
  HeatCfg  heat;                  // 2g — carried, never read in this step
  uint32_t crc32;
} __attribute__((packed));

static_assert(sizeof(ChCfg) == 10, "ChCfg is 10 bytes packed");
static_assert(sizeof(StairCfg) == 66, "StairCfg is 66 bytes — 2f contract");
static_assert(sizeof(HeatCfg) == 23, "HeatCfg is 23 bytes — 2g contract");
static_assert(sizeof(PersistConfigV5) == 886, "PersistConfigV5 is 886 bytes packed");

static const uint32_t CFG_MAGIC      = 0x53545231UL; // '1RTS'
static const uint16_t CFG_VERSION_V3 = 0x0003;
static const uint16_t CFG_VERSION_V4 = 0x0004;
static const uint16_t CFG_VERSION    = 0x0005;

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
  IREG_ACTIVE_SCENE  = 9,
  IREG_OUT_BASE      = 10,
  // 26..31 are declared and published as zero here; 2f and 2g fill them.
  IREG_SEQ_STATE     = 26,
  IREG_SEQ_STEP      = 27,
  IREG_HEAT_FLAGS    = 28,
  IREG_ZONES_OPEN    = 29,
  IREG_ZONE_MASK_LO  = 30,
  IREG_ZONE_MASK_HI  = 31,

  CMD_DI_EN_BASE  = 300,
  CMD_DI_DIS_BASE = 320,
  COIL_SAVE_PWM   = 330,
  COIL_RELEASE_OVERRIDE = 331,
  COIL_PANIC_ON   = 332,
  COIL_ALL_OFF    = 333,
  COIL_PANIC_OFF  = 334,
  COIL_STAIR_UP   = 340,
  COIL_STAIR_DOWN = 341,
  COIL_STAIR_STOP = 342,
  COIL_HEAT_EXERCISE = 345,
  HR_PWM_BASE = 400,
  HR_MASTER   = 432,
  HR_INHIBIT  = 433,
  HR_NIGHT    = 434,
  HR_DAY      = 435,
  HR_SCENE    = 436,
  HR_SUMMER   = 437,
  HR_NIGHT_WINDOW = 438,
  HR_MB_ADDR  = 480,
  HR_MB_BAUD  = 481
};

// WebConfig transfer sections — the config is delivered one named, chunked and
// acknowledged section at a time (see the WEBCONFIG block further down).
enum : uint8_t {
  SEC_BASE = 0, SEC_INPUTS, SEC_BUTTONS, SEC_LEDS,
  SEC_FAILSAFE, SEC_CHANNELS, SEC_GROUPS, SEC_SCENES,
  SEC_STAIRS, SEC_HEATING, SEC_COUNT
};
static const char* const CFG_SEC_NAME[SEC_COUNT] = {
  "base", "inputs", "buttons", "leds", "failsafe", "channels", "groups", "scenes",
  "stairs", "heating"
};
// inputs 3, failsafe 4, channels 11 (3/part), scenes 8 (1/part), stairs 3.
// Heating stays 1: hours/cycles live on the status line, not here.
static const uint8_t CFG_SEC_PARTS[SEC_COUNT] = { 1, 3, 1, 1, 4, 11, 1, 8, 3, 1 };

// STATUS_FLAGS bits — 6 and 7 are declared for 2f/2g and read 0 in this step.
enum : uint16_t {
  ST_TLC_READY  = 1u << 0,
  ST_LINK_OK    = 1u << 1,
  ST_BUS_FS     = 1u << 2,
  ST_CFG_DIRTY  = 1u << 3,
  ST_OVERRIDE   = 1u << 4,
  ST_PANIC      = 1u << 5,
  ST_SEQ_RUN    = 1u << 6,
  ST_HEAT_DEM   = 1u << 7
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

static inline uint8_t driverOut(uint8_t ch, uint8_t v) {
  // NC/NO last: immediately before the TLC. NO inverts every heat mode.
  if (chCfg[ch].profile == CH_PROF_HEAT && (chCfg[ch].flags & 0x01)) return (uint8_t)(255 - v);
  return v;
}

static uint16_t channelRampMs(uint8_t ch);

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
    const uint8_t v = driverOut(idx, (uint8_t)constrain((int)pwmLevel[idx], 0, 255));
    wrote += (uint8_t)tlcWire->write(v);
  }
  if (wrote != 9 || tlcWire->endTransmission() != 0) {
    tlcNoteI2cFailure();
    return false;
  }
  for (uint8_t ch = 0; ch < 8; ch++) {
    const uint8_t idx = (uint8_t)(chipIdx * 8 + ch);
    tlcApplied[idx] = driverOut(idx, (uint8_t)constrain((int)pwmLevel[idx], 0, 255));
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

// One predicate for DI_MASK and the heating DI flags. A heating role on
// IO1 treats that input as enabled even if the WebConfig checkbox is off.
static inline bool diConsideredEnabled(uint8_t i) {
  if (i >= NUM_DI) return false;
  if (i == 0 && g_heatCfg.diRole != 0) return true;
  return diCfg[i].enabled;
}

static inline bool diReportedLive(uint8_t i) {
  return diConsideredEnabled(i) ? diLiveState(i) : false;
}

static void applyPwmChannel(uint8_t idx, uint16_t val) {
  if (idx >= NUM_PWM) return;
  if (val > 255) val = 255;
  const uint8_t v = driverOut(idx, (uint8_t)val);
  pwmLevel[idx] = val;
  const uint8_t chipIdx = idx / 8;
  if (!tlcReady || !tlcChipOk[chipIdx]) return;
  if (tlcApplied[idx] == v) return;
  tlcApplied[idx] = v;
  tlcWritePwm(chipIdx, idx % 8, v);
}

// The single flush path to the drivers. Per chip: two or more changed channels
// go out as ONE auto-increment block write (tlcFlushChip), so a ramp moving all
// 32 channels costs four I2C transactions per service tick, not thirty-two.
static void applyAllPwmLevels() {
  for (uint8_t chipIdx = 0; chipIdx < 4; chipIdx++) {
    if (!tlcReady || !tlcChipOk[chipIdx]) continue;
    uint8_t diffCount = 0;
    uint8_t loneIdx = 0;
    for (uint8_t ch = 0; ch < 8; ch++) {
      const uint8_t idx = (uint8_t)(chipIdx * 8 + ch);
      const uint8_t v = driverOut(idx, (uint8_t)constrain((int)pwmLevel[idx], 0, 255));
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
  return g_panicActive || g_localOverride || g_busFailsafeActive;
}

// ============================================================================
//  OUTPUT PIPELINE — the one and only application order (see the file header)
//
//    source → profile → min/max → curve → master → ramp → block I2C write
//
//  Nothing else in this file may shape a channel value. Panic and bus failsafe
//  replace the SOURCE (stage 0) and then travel the same road as any other
//  command, so a channel keeps its own curve, window and the master level.
// ============================================================================

// Curve tables: 3 × 256 bytes, built once at boot. The gamma table is the
// RGB-621-R1 v0.2.0 formula (hm_pwm_output.h pwmBuildGammaLut, gamma 2.2)
// rebuilt at 8-bit width; CIE is the standard L* → luminance transfer.
static void buildCurveLuts() {
  for (uint16_t i = 0; i < 256; i++) {
    const float norm = (float)i / 255.0f;
    g_curveLut[CH_CURVE_LINEAR][i] = (uint8_t)i;
    g_curveLut[CH_CURVE_GAMMA][i]  = (uint8_t)(powf(norm, 2.2f) * 255.0f + 0.5f);
    const float L = norm * 100.0f;
    const float Y = (L <= 8.0f) ? (L / 903.3f)
                                : powf((L + 16.0f) / 116.0f, 3.0f);
    g_curveLut[CH_CURVE_CIE][i] = (uint8_t)(Y * 255.0f + 0.5f);
  }
}

// Stage 0 — who owns the channel value right now.
// Panic outranks everything; failsafe and local override work by having already
// written chRequest and by locking Modbus out (outputsModbusLocked).
static inline uint8_t chEffectiveSource(uint8_t ch) {
  if (g_panicActive) {
    // Heat stays closed in panic — a fire alarm must not throw valves open.
    if (chCfg[ch].profile == CH_PROF_HEAT) return 0;
    return g_panicLevel;
  }
  return chRequest[ch];
}

// Stage 2 — rescale into the channel window. 0 stays off; 1..255 spans
// [minLevel..maxLevel]. This is a rescale, not a clip: the top of the command
// range reaches maxLevel and the bottom reaches minLevel.
static inline uint8_t chApplyMinMax(uint8_t ch, uint8_t v) {
  if (v == 0) return 0;
  const uint8_t lo = chCfg[ch].minLevel;
  uint8_t hi = chCfg[ch].maxLevel;
  if (hi < lo) hi = lo;
  if (lo == 0 && hi == 255) return v;
  return (uint8_t)((uint32_t)lo + (((uint32_t)v * (uint32_t)(hi - lo) + 127u) / 255u));
}

// Stage 4 — master level, applied last, to every profile including raw.
static inline uint8_t chApplyMaster(uint8_t v) {
  if (g_masterLevel >= 255) return v;
  return (uint8_t)(((uint32_t)v * (uint32_t)g_masterLevel + 127u) / 255u);
}

static uint8_t resolveChannelDriverValue(uint8_t ch) {
  uint8_t v = chEffectiveSource(ch);                    // stage 0 — source

  const uint8_t prof = chCfg[ch].profile;               // stage 1 — profile
  if (prof == CH_PROF_HEAT) {
    v = (v >= 100) ? 255 : (uint8_t)(((uint32_t)v * 255u + 50u) / 100u);
  } else if (prof == CH_PROF_IND) {
    v = (v > 0) ? 255 : 0;
  } else if (prof == CH_PROF_RAW) {
    return chApplyMaster(v);                            // raw skips 2 and 3 only
  }

  v = chApplyMinMax(ch, v);                             // stage 2 — min/max
  const uint8_t curve = (chCfg[ch].curve < CH_CURVE_COUNT) ? chCfg[ch].curve : CH_CURVE_LINEAR;
  v = g_curveLut[curve][v];                             // stage 3 — curve
  return chApplyMaster(v);                              // stage 4 — master
}

static void recomputeChannelDriver(uint8_t ch) {
  if (ch >= NUM_PWM) return;
  chDriver[ch] = resolveChannelDriverValue(ch);
}

static void recomputeAllDrivers() {
  for (uint8_t ch = 0; ch < NUM_PWM; ch++) chDriver[ch] = resolveChannelDriverValue(ch);
}

// Stage 5 — ramp. Walks pwmLevel towards chDriver without blocking the loop.
// Step size is proportional to elapsed time so a full 0..255 traverse takes
// rampMs; the algorithm is RGB-621-R1 v0.2.0 pwmServiceSlew() retargeted from
// per-channel analogWrite onto the shared block-write flush below.
static void serviceRamp(uint32_t now) {
  bool changed = false;

  // rampMs = 0 lands at once: a plain level write keeps the latency it had
  // before ramps existed.
  for (uint8_t ch = 0; ch < NUM_PWM; ch++) {
    if (channelRampMs(ch) != 0) continue;
    chRampLastMs[ch] = now;
    if (pwmLevel[ch] == chDriver[ch]) continue;
    pwmLevel[ch] = chDriver[ch];
    changed = true;
  }

  // Ramping channels step at most every RAMP_SERVICE_MS, so a fast ramp cannot
  // turn the shared flush below into a continuous I2C load.
  if ((uint32_t)(now - g_lastRampServiceMs) >= RAMP_SERVICE_MS) {
    g_lastRampServiceMs = now;
    for (uint8_t ch = 0; ch < NUM_PWM; ch++) {
      const uint16_t rampMs = channelRampMs(ch);
      if (rampMs == 0) continue;
      const uint16_t target = chDriver[ch];
      if (pwmLevel[ch] == target) { chRampLastMs[ch] = now; continue; }

      const uint32_t elapsed = now - chRampLastMs[ch];
      const int32_t maxStep = (int32_t)((255UL * elapsed) / rampMs);
      if (maxStep < 1) continue;   // too little time yet — keep accumulating it
      chRampLastMs[ch] = now;

      const int32_t err = (int32_t)target - (int32_t)pwmLevel[ch];
      const int32_t absErr = (err > 0) ? err : -err;
      pwmLevel[ch] = (absErr <= maxStep)
        ? target
        : (uint16_t)((int32_t)pwmLevel[ch] + ((err > 0) ? maxStep : -maxStep));
      changed = true;
    }
  }

  if (changed) applyAllPwmLevels();   // stage 6 — four block writes at most
}

// ============================================================================
//  COMMAND LAYER — everything that can set a channel goes through here
// ============================================================================

// Internal write to the holding register: keeps the master's view in sync
// without the main-loop scan mistaking it for a fresh command from the bus.
static uint16_t g_prevHrPwm[NUM_PWM];
static bool     g_prevHrPwmInit = false;

static void hrPwmSet(uint8_t ch, uint8_t v) {
  if (ch >= NUM_PWM) return;
  mb.Hreg(HR_PWM_BASE + ch, v);
  g_prevHrPwm[ch] = v;
}

// One channel, no group propagation, no scene bookkeeping.
// armAutoOff: a user/bus/scene command restarts the timer; failsafe must not.
// Heat channels never arm — the slow-PWM cycle is not a user command.
static void setChannelRequestOne(uint8_t ch, uint8_t val, bool armAutoOff = true) {
  if (ch >= NUM_PWM) return;
  chRequest[ch] = val;
  if (chCfg[ch].profile == CH_PROF_HEAT || !armAutoOff) {
    chAutoOffAtMs[ch] = 0;
  } else {
    chAutoOffAtMs[ch] = (val > 0 && chCfg[ch].autoOffS > 0)
      ? (millis() + (uint32_t)chCfg[ch].autoOffS * 1000UL)
      : 0;
  }
  recomputeChannelDriver(ch);
}

// GROUPS — conflict rule.
// A write to channel C is mirrored ONCE to every channel sharing at least one
// group with C. Mirrored channels do not propagate further, so overlapping
// groups cannot loop and a channel in two groups is written exactly once.
// Every member takes the same commanded level: there is no priority between
// groups, and when two masters write different levels to two members in the
// same cycle the last write wins.
static uint32_t groupPeersOf(uint8_t ch) {
  const uint32_t bit = 1UL << ch;
  uint32_t peers = 0;
  for (uint8_t g = 0; g < NUM_GROUPS; g++) {
    if (groupMask[g] & bit) peers |= groupMask[g];
  }
  return peers & ~bit;
}

enum : uint8_t { SRC_MODBUS = 0, SRC_SCENE = 1, SRC_LOCAL = 2 };

static void setChannelRequest(uint8_t ch, uint8_t val, uint8_t src) {
  if (ch >= NUM_PWM) return;
  if (src != SRC_SCENE) g_activeScene = 0;   // any level change outside a scene
  setChannelRequestOne(ch, val);
  hrPwmSet(ch, val);
  const uint32_t peers = groupPeersOf(ch);
  if (!peers) return;
  for (uint8_t p = 0; p < NUM_PWM; p++) {
    if (!(peers & (1UL << p))) continue;
    setChannelRequestOne(p, val);
    hrPwmSet(p, val);
  }
}

#include "hm_stair.h"
#include "hm_heat.h"

static uint16_t channelRampMs(uint8_t ch) {
  return stairRampMs(ch);
}

static void setAllPwmLocal(uint16_t val) {
  if (val > 255) val = 255;
  g_activeScene = 0;
  for (uint8_t i = 0; i < NUM_PWM; i++) {
    if (heatIsChannel(i)) continue;     // All ON must not throw valves open
    setChannelRequestOne(i, (uint8_t)val);
    hrPwmSet(i, (uint8_t)val);
  }
}

static void applyRampLevelsLocal() {
  g_activeScene = 0;
  for (uint8_t i = 0; i < NUM_PWM; i++) {
    const uint8_t v = (NUM_PWM > 1) ? (uint8_t)((i * 255) / (NUM_PWM - 1)) : 255;
    setChannelRequestOne(i, v);
    hrPwmSet(i, v);
  }
}

static void restoreOutputsFromHoldingRegs() {
  g_activeScene = 0;
  for (uint8_t i = 0; i < NUM_PWM; i++) {
    if (heatIsChannel(i) || (stairRunning() && stairOwnsChannel(i))) continue;
    uint16_t v = (uint16_t)mb.Hreg(HR_PWM_BASE + i);
    if (v > 255) v = 255;
    setChannelRequestOne(i, (uint8_t)v);
    g_prevHrPwm[i] = v;
  }
}

static void serviceAutoOff(uint32_t now) {
  if (g_busFailsafeActive || g_panicActive) return;
  for (uint8_t ch = 0; ch < NUM_PWM; ch++) {
    if (chCfg[ch].profile == CH_PROF_HEAT) {
      chAutoOffAtMs[ch] = 0;
      continue;
    }
    if (chAutoOffAtMs[ch] == 0) continue;
    if ((int32_t)(now - chAutoOffAtMs[ch]) < 0) continue;
    setChannelRequestOne(ch, 0);     // clears chAutoOffAtMs[ch] on the way
    hrPwmSet(ch, 0);
    g_activeScene = 0;
    wsLog(String("Channel O") + (ch + 1) + " auto-off");
  }
}

// ---- Scenes ----
// A scene names all 32 channels, so recalling one does not propagate through
// groups: every member is already written by the scene itself.
static void sceneRecall(uint8_t n) {
  if (n < 1 || n > NUM_SCENES) return;
  const uint8_t idx = (uint8_t)(n - 1);
  for (uint8_t ch = 0; ch < NUM_PWM; ch++) {
    setChannelRequestOne(ch, sceneLevel[idx][ch]);
    hrPwmSet(ch, sceneLevel[idx][ch]);
  }
  g_activeScene = n;
  wsLog(String("Scene ") + n + " recalled");
}

static void sceneSaveCurrent(uint8_t n) {
  if (n < 1 || n > NUM_SCENES) return;
  const uint8_t idx = (uint8_t)(n - 1);
  for (uint8_t ch = 0; ch < NUM_PWM; ch++) sceneLevel[idx][ch] = chRequest[ch];
  sceneUsed[idx] = 1;
  g_activeScene = n;
  wsLog(String("Scene ") + n + " saved from current levels");
}

static void sceneClear(uint8_t n) {
  if (n < 1 || n > NUM_SCENES) return;
  const uint8_t idx = (uint8_t)(n - 1);
  memset(sceneLevel[idx], 0, NUM_PWM);
  sceneUsed[idx] = 0;
  if (g_activeScene == n) g_activeScene = 0;
  wsLog(String("Scene ") + n + " cleared");
}

// ---- Panic ----
// Coil 332 latches every channel at panicLevel and holds it there: writes to
// HR 400..431 are accepted into chRequest but cannot reach the drivers until
// coil 334. Panic outranks local override and bus failsafe.
static void panicEnter() {
  if (g_panicActive) return;
  g_panicActive = true;
  g_localOverride = false;
  for (uint8_t i = 0; i < NUM_DI; i++) g_inputToggleState[i] = false;
  recomputeAllDrivers();
  wsLog(String("PANIC: all channels held at ") + g_panicLevel);
}

static void panicExit() {
  if (!g_panicActive) return;
  g_panicActive = false;
  restoreOutputsFromHoldingRegs();
  wsLog("Panic cleared — outputs match holding registers");
}

static void allOutputsOffPulse() {
  g_activeScene = 0;
  for (uint8_t ch = 0; ch < NUM_PWM; ch++) {
    setChannelRequestOne(ch, 0);
    hrPwmSet(ch, 0);
  }
  wsLog("All outputs off (coil 333)");
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

// Failsafe replaces the SOURCE value, not the result: the channel still goes
// through its own profile, window, curve and the master level.
static void applyFailsafeOnce() {
  for (uint8_t i = 0; i < NUM_PWM; i++) {
    if (heatIsChannel(i)) continue;     // frost protection owns heat
    if (stairOwnsChannel(i)) continue;  // staircase must work on a dead bus
    switch (g_failsafeAction[i]) {
      case FS_OFF:   setChannelRequestOne(i, 0, false); break;
      case FS_LEVEL: setChannelRequestOne(i, g_failsafeLevel[i], false); break;
      default: break; // HOLD — leave channel unchanged
    }
  }
  g_activeScene = 0;
}

static void processBusFailsafe(uint32_t now) {
  if (g_panicActive) return;   // panic outranks failsafe — neither arm nor clear
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
  if (g_busFailsafeActive || g_panicActive) return;

  for (int i = 0; i < NUM_DI; i++) {
    if (!diCfg[i].enabled) continue;

    if (diCfg[i].action == IN_ACT_SCENE) {
      // A scene names every channel, so it needs no control target.
      if (diState[i] && !diPrev[i]) {
        sceneRecall(diCfg[i].param);
        enterLocalOverride();
      }
      continue;
    }

    if (diCfg[i].target != 0) continue;

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
      case BTN_ACT_ALL_ON:
        if (!g_busFailsafeActive && !g_panicActive) { setAllPwmLocal(255); enterLocalOverride(); }
        break;
      case BTN_ACT_ALL_OFF:
        if (!g_busFailsafeActive && !g_panicActive) { setAllPwmLocal(0); enterLocalOverride(); }
        break;
      case BTN_ACT_RAMP:
        if (!g_busFailsafeActive && !g_panicActive) { applyRampLevelsLocal(); enterLocalOverride(); }
        break;
      case BTN_ACT_CLEAR_OVERRIDE:
        releaseLocalOverride();
        break;
      case BTN_ACT_SCENE:
        if (!g_busFailsafeActive && !g_panicActive) { sceneRecall(btnCfg[i].param); enterLocalOverride(); }
        break;
      case BTN_ACT_PANIC:
        panicEnter();
        break;
      case BTN_ACT_CLEAR_PANIC:
        panicExit();
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

static void fillStairHeatFactory(StairCfg &st, HeatCfg &ht) {
  st.pattern = 0;
  st.waveWidth = 3;
  st.inputEndMap = 0;
  st.bothEnds = 0;
  st.bothWindowMs = 300;
  memset(st._rsv, 0, sizeof(st._rsv));
  ht.minPulsePct = 10;
  memset(ht._rsv2, 0, sizeof(ht._rsv2));
}

// Channels, groups, scenes and the two globals — factory state.
// Every new field is inert by default: profile led, linear curve, full window,
// no ramp, no auto-off, no groups, no scenes, master and panic wide open.
static void applyChannelDefaults() {
  for (uint8_t i = 0; i < NUM_PWM; i++) {
    chCfg[i].profile  = CH_PROF_LED;
    chCfg[i].curve    = CH_CURVE_LINEAR;
    chCfg[i].minLevel = 0;
    chCfg[i].maxLevel = 255;
    chCfg[i].rampMs   = 0;
    chCfg[i].autoOffS = 0;
    chCfg[i].flags    = 0;
    chCfg[i]._rsv     = 0;
  }
  for (uint8_t g = 0; g < NUM_GROUPS; g++) groupMask[g] = 0;
  for (uint8_t s = 0; s < NUM_SCENES; s++) {
    memset(sceneLevel[s], 0, NUM_PWM);
    sceneUsed[s] = 0;
  }
  g_masterLevel = 255;
  g_panicLevel  = 255;
  memset(&g_stairCfg, 0, sizeof(g_stairCfg));
  memset(&g_heatCfg,  0, sizeof(g_heatCfg));
  fillStairHeatFactory(g_stairCfg, g_heatCfg);
}

void setDefaults() {
  for (int i = 0; i < NUM_DI; i++) diCfg[i] = {true, false, 0, 4, 255, 0};
  for (int i = 0; i < NUM_LED; i++) ledCfg[i] = {0, 0};
  btnCfg[0] = {BTN_ACT_ALL_ON, 0};   // SW1 = All ON (README promise)
  btnCfg[1] = {BTN_ACT_ALL_OFF, 0};  // SW2 = All OFF
  btnCfg[2] = {BTN_ACT_NONE, 0};
  btnCfg[3] = {BTN_ACT_NONE, 0};
  for (int i = 0; i < NUM_PWM; i++) {
    pwmLevel[i] = 0;
    chRequest[i] = 0;
    chDriver[i] = 0;
    chAutoOffAtMs[i] = 0;
  }
  g_mb_address = 3;
  g_mb_baud    = 19200;
  g_panicActive = false;
  g_activeScene = 0;
  applyChannelDefaults();
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

static bool verifyPersistCrcV5(const PersistConfigV5 &pc) {
  PersistConfigV5 tmp = pc;
  const uint32_t crc = tmp.crc32;
  tmp.crc32 = 0;
  return crc32_update(0, (const uint8_t*)&tmp, sizeof(PersistConfigV5)) == crc;
}

static void fillV4Defaults(PersistConfigV4 &pc) {
  pc.busTimeoutS = 0;
  pc.overrideTimeoutS = 0;
  for (int i = 0; i < NUM_PWM; i++) {
    pc.failsafeAction[i] = FS_HOLD;
    pc.failsafeLevel[i] = 0;
  }
}

// Everything 0x0005 added, at its factory value. Used by the 0x0004 migrator.
static void fillV5Defaults(PersistConfigV5 &pc) {
  for (int i = 0; i < NUM_PWM; i++) {
    pc.chCfg[i].profile  = CH_PROF_LED;
    pc.chCfg[i].curve    = CH_CURVE_LINEAR;
    pc.chCfg[i].minLevel = 0;
    pc.chCfg[i].maxLevel = 255;
    pc.chCfg[i].rampMs   = 0;
    pc.chCfg[i].autoOffS = 0;
    pc.chCfg[i].flags    = 0;
    pc.chCfg[i]._rsv     = 0;
  }
  for (int g = 0; g < NUM_GROUPS; g++) pc.groupMask[g] = 0;
  for (int s = 0; s < NUM_SCENES; s++) {
    memset(pc.sceneLevel[s], 0, NUM_PWM);
    pc.sceneUsed[s] = 0;
  }
  pc.masterLevel = 255;
  pc.panicLevel  = 255;
  memset(&pc.stair, 0, sizeof(pc.stair));
  memset(&pc.heat,  0, sizeof(pc.heat));
  fillStairHeatFactory(pc.stair, pc.heat);
}

static bool applyFromPersistV5(const PersistConfigV5 &pc) {
  if (pc.magic != CFG_MAGIC || pc.size != sizeof(PersistConfigV5)) return false;
  if (!verifyPersistCrcV5(pc)) return false;
  if (pc.version != CFG_VERSION) return false;

  memcpy(diCfg, pc.diCfg, sizeof(diCfg));
  memcpy(ledCfg, pc.ledCfg, sizeof(ledCfg));
  memcpy(btnCfg, pc.btnCfg, sizeof(btnCfg));
  for (int i = 0; i < NUM_PWM; i++) {
    chRequest[i] = (uint8_t)((pc.pwmLevel[i] > 255) ? 255 : pc.pwmLevel[i]);
    chAutoOffAtMs[i] = 0;
  }
  g_mb_address = pc.mb_address;
  g_mb_baud = pc.mb_baud;
  g_busTimeoutS = pc.busTimeoutS;
  g_overrideTimeoutS = pc.overrideTimeoutS;
  for (int i = 0; i < NUM_PWM; i++) {
    const uint8_t act = pc.failsafeAction[i];
    g_failsafeAction[i] = (act <= FS_LEVEL) ? act : FS_HOLD;
    g_failsafeLevel[i] = pc.failsafeLevel[i];
  }
  memcpy(chCfg, pc.chCfg, sizeof(chCfg));
  for (int i = 0; i < NUM_PWM; i++) {
    if (chCfg[i].profile > CH_PROF_IND) chCfg[i].profile = CH_PROF_LED;
    if (chCfg[i].curve >= CH_CURVE_COUNT) chCfg[i].curve = CH_CURVE_LINEAR;
  }
  memcpy(groupMask, pc.groupMask, sizeof(groupMask));
  memcpy(sceneLevel, pc.sceneLevel, sizeof(sceneLevel));
  memcpy(sceneUsed, pc.sceneUsed, sizeof(sceneUsed));
  g_masterLevel = pc.masterLevel;
  g_panicLevel  = pc.panicLevel;
  memcpy(&g_stairCfg, &pc.stair, sizeof(g_stairCfg));
  memcpy(&g_heatCfg,  &pc.heat,  sizeof(g_heatCfg));
  for (int i = 0; i < NUM_PWM; i++) {
    if (pc.chCfg[i].profile == CH_PROF_HEAT)
      g_heatDuty[i] = (uint8_t)((chRequest[i] > 100) ? 100 : chRequest[i]);
  }
  return true;
}

static bool migrateV3ToV4(const PersistConfigV3 &pc3, PersistConfigV4 &pc4) {
  if (pc3.magic != CFG_MAGIC || pc3.size != sizeof(PersistConfigV3)) return false;
  if (!verifyPersistCrcV3(pc3)) return false;
  if (pc3.version != CFG_VERSION_V3) return false;

  memset(&pc4, 0, sizeof(pc4));
  pc4.magic = CFG_MAGIC;
  pc4.version = CFG_VERSION_V4;
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
  // The freshly built record must carry a valid CRC: the next stage verifies it
  // exactly like one read from flash. Without this the whole migration chain
  // fails its own check and a v0.1.0 device silently comes up on defaults.
  pc4.crc32 = 0;
  pc4.crc32 = crc32_update(0, (const uint8_t*)&pc4, sizeof(PersistConfigV4));
  return true;
}

// 0x0004 → 0x0005. Everything configured in steps 2a..2d survives: Modbus
// address and baud, the three inputs (enabled / inverted / action / target /
// on-level), the four buttons, both status LEDs, the bus and override timeouts,
// the per-channel failsafe action and level arrays, and the stored levels.
// Only the fields 0x0005 introduces are set, and they are set to factory.
static bool migrateV4ToV5(const PersistConfigV4 &pc4, PersistConfigV5 &pc5) {
  if (pc4.magic != CFG_MAGIC || pc4.size != sizeof(PersistConfigV4)) return false;
  if (!verifyPersistCrcV4(pc4)) return false;
  if (pc4.version != CFG_VERSION_V4) return false;

  memset(&pc5, 0, sizeof(pc5));
  pc5.magic = CFG_MAGIC;
  pc5.version = CFG_VERSION;
  pc5.size = sizeof(PersistConfigV5);
  for (int i = 0; i < NUM_DI; i++) {
    pc5.diCfg[i].enabled  = pc4.diCfg[i].enabled;
    pc5.diCfg[i].inverted = pc4.diCfg[i].inverted;
    pc5.diCfg[i].action   = pc4.diCfg[i].action;
    pc5.diCfg[i].target   = pc4.diCfg[i].target;
    pc5.diCfg[i].level    = pc4.diCfg[i].level;
    pc5.diCfg[i].param    = 0;
  }
  memcpy(pc5.ledCfg, pc4.ledCfg, sizeof(pc5.ledCfg));
  for (int i = 0; i < NUM_BTN; i++) {
    pc5.btnCfg[i].action = pc4.btnCfg[i].action;
    pc5.btnCfg[i].param  = 0;
  }
  memcpy(pc5.pwmLevel, pc4.pwmLevel, sizeof(pc5.pwmLevel));
  pc5.mb_address = pc4.mb_address;
  pc5.mb_baud = pc4.mb_baud;
  pc5.busTimeoutS = pc4.busTimeoutS;
  pc5.overrideTimeoutS = pc4.overrideTimeoutS;
  memcpy(pc5.failsafeAction, pc4.failsafeAction, sizeof(pc5.failsafeAction));
  memcpy(pc5.failsafeLevel, pc4.failsafeLevel, sizeof(pc5.failsafeLevel));
  fillV5Defaults(pc5);
  pc5.crc32 = 0;
  pc5.crc32 = crc32_update(0, (const uint8_t*)&pc5, sizeof(PersistConfigV5));
  return true;
}

void captureToPersist(PersistConfigV5 &pc) {
  pc.magic = CFG_MAGIC;
  pc.version = CFG_VERSION;
  pc.size = sizeof(PersistConfig);
  memcpy(pc.diCfg, diCfg, sizeof(diCfg));
  memcpy(pc.ledCfg, ledCfg, sizeof(ledCfg));
  memcpy(pc.btnCfg, btnCfg, sizeof(btnCfg));
  // The stored levels are the COMMANDED ones, not the driver duty: restoring
  // them re-enters the pipeline on boot instead of freezing a curved value.
  for (int i = 0; i < NUM_PWM; i++) pc.pwmLevel[i] = chRequest[i];
  pc.mb_address = g_mb_address;
  pc.mb_baud = g_mb_baud;
  pc.busTimeoutS = g_busTimeoutS;
  pc.overrideTimeoutS = g_overrideTimeoutS;
  memcpy(pc.failsafeAction, g_failsafeAction, sizeof(g_failsafeAction));
  memcpy(pc.failsafeLevel, g_failsafeLevel, sizeof(g_failsafeLevel));
  memcpy(pc.chCfg, chCfg, sizeof(chCfg));
  memcpy(pc.groupMask, groupMask, sizeof(groupMask));
  memcpy(pc.sceneLevel, sceneLevel, sizeof(sceneLevel));
  memcpy(pc.sceneUsed, sceneUsed, sizeof(sceneUsed));
  pc.masterLevel = g_masterLevel;
  pc.panicLevel  = g_panicLevel;
  memcpy(&pc.stair, &g_stairCfg, sizeof(pc.stair));
  memcpy(&pc.heat,  &g_heatCfg,  sizeof(pc.heat));
  pc.crc32 = 0;
  pc.crc32 = crc32_update(0, (const uint8_t*)&pc, sizeof(PersistConfigV5));
}

static bool saveConfigFS() {
  PersistConfigV5 pc{};
  captureToPersist(pc);
  File f = LittleFS.open(CFG_PATH, "w");
  if (!f) { wsLog("save: open failed"); return false; }
  size_t n = f.write((const uint8_t*)&pc, sizeof(pc));
  f.flush();
  f.close();
  if (n != sizeof(pc)) { wsLog(String("save: short write ") + n); return false; }
  File r = LittleFS.open(CFG_PATH, "r");
  if (!r) { wsLog("save: reopen failed"); return false; }
  if ((size_t)r.size() != sizeof(PersistConfigV5)) { wsLog("save: size mismatch after write"); r.close(); return false; }
  PersistConfigV5 back{};
  size_t nr = r.read((uint8_t*)&back, sizeof(back));
  r.close();
  if (nr != sizeof(back)) { wsLog("save: short readback"); return false; }
  PersistConfigV5 verify = back;
  uint32_t crc = verify.crc32;
  verify.crc32 = 0;
  if (crc32_update(0, (const uint8_t*)&verify, sizeof(verify)) != crc) { wsLog("save: CRC verify failed"); return false; }
  return true;
}

bool loadConfigFS() {
  File f = LittleFS.open(CFG_PATH, "r");
  if (!f) { wsLog("load: open failed"); return false; }
  const size_t fsz = f.size();

  if (fsz == sizeof(PersistConfigV5)) {
    PersistConfigV5 pc5{};
    const size_t n = f.read((uint8_t*)&pc5, sizeof(pc5));
    f.close();
    if (n != sizeof(pc5)) { wsLog("load: short read"); return false; }
    if (!applyFromPersistV5(pc5)) { wsLog("load: v5 magic/version/crc mismatch"); return false; }
    return true;
  }

  if (fsz == sizeof(PersistConfigV4)) {
    PersistConfigV4 pc4{};
    const size_t n = f.read((uint8_t*)&pc4, sizeof(pc4));
    f.close();
    if (n != sizeof(pc4)) { wsLog("load: short read"); return false; }
    PersistConfigV5 pc5{};
    if (!migrateV4ToV5(pc4, pc5)) { wsLog("load: v4 migrate failed"); return false; }
    if (!applyFromPersistV5(pc5)) { wsLog("load: v5 apply after migrate failed"); return false; }
    wsLog("Config migrated 0x0004 -> 0x0005");
    if (!saveConfigFS()) { wsLog("ERROR: migrate re-save failed"); return false; }
    return true;
  }

  if (fsz == sizeof(PersistConfigV3)) {
    PersistConfigV3 pc3{};
    const size_t n = f.read((uint8_t*)&pc3, sizeof(pc3));
    f.close();
    if (n != sizeof(pc3)) { wsLog("load: short read"); return false; }
    PersistConfigV4 pc4{};
    if (!migrateV3ToV4(pc3, pc4)) { wsLog("load: v3 migrate failed"); return false; }
    PersistConfigV5 pc5{};
    if (!migrateV4ToV5(pc4, pc5)) { wsLog("load: v3->v4->v5 migrate failed"); return false; }
    if (!applyFromPersistV5(pc5)) { wsLog("load: v5 apply after migrate failed"); return false; }
    wsLog("Config migrated 0x0003 -> 0x0004 -> 0x0005");
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
void sendWebIdentity();
void sendHeatStats();
void sendWebCfg();
void sendWebBootstrap();
void sendWebLevels();
void sendCfgSection(uint8_t sec);
void cfgTransferStart();
void handleCfgAck(const char* sec, int part);
void serviceCfgTransfer(uint32_t now);
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
  const bool changed = (g_mb_address != addr) || (g_mb_baud != baud);
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
  if (changed) sendWebIdentity();
}

void handleValues(JSONVar values) {
  int addr = (int)values["mb_address"];
  int baud = (int)values["mb_baud"];
  const uint8_t newAddr = addr ? hmValidAddress(addr) : g_mb_address;
  const uint32_t newBaud = baud ? hmValidBaud(baud) : g_mb_baud;
  applyModbusSettings(newAddr, newBaud);
  if (addr || baud) markCfgDirty();   // regression lost in the 2026-08-04 rollback

  if (values.hasOwnProperty("pwm")) {
    releaseLocalOverrideForWebConfig();
    JSONVar arr = values["pwm"];
    // The page names every channel, so this does not propagate through groups.
    g_activeScene = 0;
    for (int i = 0; i < NUM_PWM && i < arr.length(); i++) {
      const uint8_t v = (uint8_t)constrain((int)arr[i], 0, 255);
      if (heatIsChannel((uint8_t)i)) {
        g_heatDuty[i] = (v > 100) ? 100 : v;
        hrPwmSet((uint8_t)i, g_heatDuty[i]);
        continue;
      }
      setChannelRequestOne((uint8_t)i, v);
      hrPwmSet((uint8_t)i, v);
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
      for (int i = 0; i < NUM_PWM; i++) hrPwmSet((uint8_t)i, chRequest[i]);
      mb.Hreg(HR_MASTER, g_masterLevel);
      recomputeAllDrivers();
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
    for (int i = 0; i < NUM_PWM; i++) hrPwmSet((uint8_t)i, chRequest[i]);
    mb.Hreg(HR_MASTER, g_masterLevel);
    mb.Hreg(HR_SCENE, 0);
    recomputeAllDrivers();
    if (saveConfigFS()) {
      wsLog("Factory defaults restored & saved");
      sendWebBootstrap();
      applyModbusSettings(g_mb_address, g_mb_baud);
    } else {
      wsLog("ERROR: Save after factory reset failed");
    }
  } else if (act == "cfgack") {
    handleCfgAck((const char*)obj["sec"], (int)obj["part"]);
  } else if (act.startsWith("scene.")) {
    // scene.save.N / scene.recall.N / scene.clear.N — N travels in the action
    // string so the shared WebConfig compatibility gate still sees a write.
    const int dot = act.lastIndexOf('.');
    const uint8_t n = (uint8_t)act.substring(dot + 1).toInt();
    if (n < 1 || n > NUM_SCENES) {
      wsLog(String("scene: bad number in ") + actC);
    } else if (act.startsWith("scene.save")) {
      sceneSaveCurrent(n);
      markCfgDirty();
      sendCfgSection(SEC_SCENES);
    } else if (act.startsWith("scene.recall")) {
      releaseLocalOverrideForWebConfig();
      sceneRecall(n);
      sendWebLevels();
    } else if (act.startsWith("scene.clear")) {
      sceneClear(n);
      markCfgDirty();
      sendCfgSection(SEC_SCENES);
    }
  } else if (act == "panic") {
    panicEnter();
    sendWebStatus();
  } else if (act == "panic.clear") {
    panicExit();
    sendWebStatus();
    sendWebLevels();
  } else if (act == "stair.up") {
    stairForce(SEQ_DIR_UP);
    sendWebStatus();
  } else if (act == "stair.down") {
    stairForce(SEQ_DIR_DOWN);
    sendWebStatus();
  } else if (act == "stair.stop") {
    stairForce(SEQ_DIR_NONE);
    sendWebStatus();
  } else if (act == "heat.exercise") {
    heatQueueExerciseAll();
    sendWebStatus();
  } else if (act == "heat.stats") {
    sendHeatStats();
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
      diCfg[i].action = (uint8_t)constrain((int)list[i], 0, IN_ACT_MAX);
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
  } else if (type == "in.param") {
    for (int i = 0; i < NUM_DI && i < list.length(); i++)
      diCfg[i].param = (uint8_t)constrain((int)list[i], 0, NUM_SCENES);
    wsLog("Input scene binding updated");
    changed = true;
  } else if (type == "btn" || type == "buttons") {
    for (int i = 0; i < NUM_BTN && i < list.length(); i++) {
      if (list[i].hasOwnProperty("action")) {
        btnCfg[i].action = (uint8_t)constrain((int)list[i]["action"], 0, BTN_ACT_MAX);
        if (list[i].hasOwnProperty("param"))
          btnCfg[i].param = (uint8_t)constrain((int)list[i]["param"], 0, NUM_SCENES);
      } else {
        btnCfg[i].action = (uint8_t)constrain((int)list[i], 0, BTN_ACT_MAX);
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
    // Chunked by offset: 64 numbers in one message overran SimpleWebSerial's
    // 256-byte line buffer and the whole write was silently dropped.
    const int off = (int)list["o"];
    JSONVar actions = list["a"];
    JSONVar levels = list["l"];
    for (int k = 0; k < actions.length() && k < levels.length(); k++) {
      const int i = off + k;
      if (i < 0 || i >= NUM_PWM) continue;
      g_failsafeAction[i] = (uint8_t)constrain((int)actions[k], 0, 2);
      g_failsafeLevel[i] = (uint8_t)constrain((int)levels[k], 0, 255);
    }
    wsLog(String("Bus failsafe channels ") + (off + 1) + ".." + (off + actions.length()) + " updated");
    changed = true;
  } else if (type == "ch.set") {
    const int i = (int)list["i"];
    if (i < 0 || i >= NUM_PWM) {
      wsLog(String("ch.set: channel out of range ") + i);
    } else {
      chCfg[i].profile  = (uint8_t)constrain((int)list["p"], 0, CH_PROF_IND);
      chCfg[i].curve    = (uint8_t)constrain((int)list["c"], 0, CH_CURVE_COUNT - 1);
      chCfg[i].minLevel = (uint8_t)constrain((int)list["mn"], 0, 255);
      chCfg[i].maxLevel = (uint8_t)constrain((int)list["mx"], 0, 255);
      chCfg[i].rampMs   = (uint16_t)constrain((int)list["r"], 0, 60000);
      chCfg[i].autoOffS = (uint16_t)constrain((int)list["ao"], 0, 65535);
      if (list.hasOwnProperty("f")) chCfg[i].flags = (uint8_t)constrain((int)list["f"], 0, 255);
      recomputeChannelDriver((uint8_t)i);
      wsLog(String("Channel O") + (i + 1) + " profile updated");
      changed = true;
    }
  } else if (type == "ch.all") {
    for (int i = 0; i < NUM_PWM; i++) {
      if (list.hasOwnProperty("p"))  chCfg[i].profile  = (uint8_t)constrain((int)list["p"], 0, CH_PROF_IND);
      if (list.hasOwnProperty("c"))  chCfg[i].curve    = (uint8_t)constrain((int)list["c"], 0, CH_CURVE_COUNT - 1);
      if (list.hasOwnProperty("mn")) chCfg[i].minLevel = (uint8_t)constrain((int)list["mn"], 0, 255);
      if (list.hasOwnProperty("mx")) chCfg[i].maxLevel = (uint8_t)constrain((int)list["mx"], 0, 255);
      if (list.hasOwnProperty("r"))  chCfg[i].rampMs   = (uint16_t)constrain((int)list["r"], 0, 60000);
      if (list.hasOwnProperty("ao")) chCfg[i].autoOffS = (uint16_t)constrain((int)list["ao"], 0, 65535);
    }
    recomputeAllDrivers();
    wsLog("Channel profiles applied to all 32 channels");
    changed = true;
  } else if (type == "grp.set") {
    const int g = (int)list["i"];
    if (g < 0 || g >= NUM_GROUPS) {
      wsLog(String("grp.set: group out of range ") + g);
    } else {
      groupMask[g] = (uint32_t)(double)list["m"];
      wsLog(String("Group ") + (g + 1) + " membership updated");
      changed = true;
    }
  } else if (type == "global") {
    if (list.hasOwnProperty("master")) {
      g_masterLevel = (uint8_t)constrain((int)list["master"], 0, 255);
      mb.Hreg(HR_MASTER, g_masterLevel);
    }
    if (list.hasOwnProperty("panic")) g_panicLevel = (uint8_t)constrain((int)list["panic"], 0, 255);
    recomputeAllDrivers();
    wsLog(String("Master level ") + g_masterLevel + ", panic level " + g_panicLevel);
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
    g_activeScene = 0;
    for (int i = 0; i < NUM_PWM && i < list.length(); i++) {
      const uint8_t v = (uint8_t)constrain((int)list[i], 0, 255);
      setChannelRequestOne((uint8_t)i, v);
      hrPwmSet((uint8_t)i, v);
    }
    wsLog("Output levels updated");
    sendWebLevels();  // brightness not auto-persisted
  } else if (type == "stairs") {
    if (list.hasOwnProperty("n"))  g_stairCfg.stepCount = (uint8_t)constrain((int)list["n"], 0, 32);
    if (list.hasOwnProperty("md")) g_stairCfg.mode = (uint8_t)constrain((int)list["md"], 0, STAIR_MODE_MAX);
    if (list.hasOwnProperty("pt")) g_stairCfg.pattern = (uint8_t)constrain((int)list["pt"], 0, STAIR_PAT_MAX);
    if (list.hasOwnProperty("ww")) g_stairCfg.waveWidth = (uint8_t)constrain((int)list["ww"], 0, 32);
    if (list.hasOwnProperty("im")) g_stairCfg.inputEndMap = (uint8_t)constrain((int)list["im"], 0, 1);
    if (list.hasOwnProperty("be")) g_stairCfg.bothEnds = (uint8_t)constrain((int)list["be"], 0, 2);
    if (list.hasOwnProperty("bw")) g_stairCfg.bothWindowMs = (uint16_t)constrain((int)list["bw"], 0, 5000);
    if (list.hasOwnProperty("d"))  g_stairCfg.stepDelayMs = (uint16_t)constrain((int)list["d"], 0, 60000);
    if (list.hasOwnProperty("h"))  g_stairCfg.holdS = (uint16_t)constrain((int)list["h"], 0, 3600);
    if (list.hasOwnProperty("f"))  g_stairCfg.fadeOutMs = (uint16_t)constrain((int)list["f"], 0, 60000);
    if (list.hasOwnProperty("ov")) g_stairCfg.overlapPct = (uint8_t)constrain((int)list["ov"], 0, 100);
    if (list.hasOwnProperty("rt")) g_stairCfg.retrigger = (uint8_t)constrain((int)list["rt"], 0, 2);
    if (list.hasOwnProperty("op")) g_stairCfg.oppose = (uint8_t)constrain((int)list["op"], 0, 2);
    if (list.hasOwnProperty("nl")) { g_stairCfg.nightLevel = (uint8_t)constrain((int)list["nl"], 0, 255); mb.Hreg(HR_NIGHT, g_stairCfg.nightLevel); }
    if (list.hasOwnProperty("dl")) { g_stairCfg.dayLevel = (uint8_t)constrain((int)list["dl"], 0, 255); mb.Hreg(HR_DAY, g_stairCfg.dayLevel); }
    if (list.hasOwnProperty("sf")) g_stairCfg.standbyFirst = (uint8_t)constrain((int)list["sf"], 0, 255);
    if (list.hasOwnProperty("sl")) g_stairCfg.standbyLast = (uint8_t)constrain((int)list["sl"], 0, 255);
    if (list.hasOwnProperty("nn")) g_stairCfg.nightLightLevel = (uint8_t)constrain((int)list["nn"], 0, 255);
    if (list.hasOwnProperty("fd")) g_stairCfg.stepFadeMs = (uint16_t)constrain((int)list["fd"], 0, 60000);
    if (list.hasOwnProperty("db")) g_stairCfg.debounceMs = (uint16_t)constrain((int)list["db"], 0, 5000);
    if (list.hasOwnProperty("mr")) g_stairCfg.minRepeatMs = (uint16_t)constrain((int)list["mr"], 0, 60000);
    if (list.hasOwnProperty("map")) {
      const char* hex = (const char*)list["map"];
      if (hex) {
        for (uint8_t s = 0; s < NUM_PWM; s++) {
          const char a = hex[s * 2], b = hex[s * 2 + 1];
          if (!a || !b) break;
          auto nib = [](char c) -> uint8_t {
            if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
            if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
            if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
            return 0;
          };
          const uint8_t v = (uint8_t)((nib(a) << 4) | nib(b));
          g_stairCfg.stepChannel[s] = (v > 31) ? 31 : v;
        }
      }
    }
    stairRebuildMask();
    if (stairEnabled() && !stairRunning()) stairApplyIdleBase();
    wsLog("Stairs config updated");
    changed = true;
  } else if (type == "heating") {
    if (list.hasOwnProperty("p"))  g_heatCfg.slowPwmPeriodS = (uint16_t)constrain((int)list["p"], 0, 3600);
    if (list.hasOwnProperty("ph")) g_heatCfg.phaseSpreadPct = (uint8_t)constrain((int)list["ph"], 0, 100);
    if (list.hasOwnProperty("mx")) g_heatCfg.maxOpenZones = (uint8_t)constrain((int)list["mx"], 0, 32);
    if (list.hasOwnProperty("di")) {
      g_heatCfg.diRole = (uint8_t)constrain((int)list["di"], 0, 3);
      if (g_heatCfg.diRole != HEAT_DI_OFF) diCfg[0].enabled = true;
    }
    if (list.hasOwnProperty("fo")) g_heatCfg.firstOpenDelayS = (uint16_t)constrain((int)list["fo"], 0, 3600);
    if (list.hasOwnProperty("ov")) g_heatCfg.overrunS = (uint16_t)constrain((int)list["ov"], 0, 3600);
    if (list.hasOwnProperty("eh")) g_heatCfg.exerciseIntervalH = (uint16_t)constrain((int)list["eh"], 0, 8760);
    if (list.hasOwnProperty("ed")) g_heatCfg.exerciseDurationS = (uint16_t)constrain((int)list["ed"], 0, 3600);
    if (list.hasOwnProperty("af")) g_heatCfg.antifreezeHours = (uint16_t)constrain((int)list["af"], 0, 168);
    if (list.hasOwnProperty("sm")) {
      g_heatCfg.summerMode = (uint8_t)constrain((int)list["sm"], 0, 1);
      mb.Hreg(HR_SUMMER, g_heatCfg.summerMode);
    }
    if (list.hasOwnProperty("mp")) g_heatCfg.minPulsePct = (uint8_t)constrain((int)list["mp"], 0, 50);
    if (list.hasOwnProperty("nc")) {
      const char* hex = (const char*)list["nc"];
      if (hex) {
        auto nib = [](char c) -> uint8_t {
          if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
          if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
          if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
          return 0;
        };
        uint32_t mask = 0;
        for (uint8_t b = 0; b < 8 && hex[b]; b++) mask = (mask << 4) | nib(hex[b]);
        for (uint8_t i = 0; i < NUM_PWM; i++) {
          if (mask & (1UL << i)) chCfg[i].flags |= 0x01;
          else chCfg[i].flags &= (uint8_t)~0x01;
        }
      }
    }
    wsLog("Heating config updated");
    changed = true;
  } else {
    wsLog(String("Unknown Config type: ") + t);
  }

  if (changed) {
    markCfgDirty();
    // Echo back only the small sections. The bulk ones (failsafe, channels,
    // groups, scenes) are not re-sent on every keystroke: the page already
    // holds what it just wrote, and re-running their chunk sequence would
    // flood the link on each edit.
    if (type.startsWith("in.") || type == "inputEnable" || type == "inputInvert" ||
        type == "inputAction" || type == "inputTarget" || type == "inputLevel") {
      sendCfgSection(SEC_INPUTS);
    } else if (type == "btn" || type == "buttons") {
      sendCfgSection(SEC_BUTTONS);
    } else if (type == "led" || type == "leds") {
      sendCfgSection(SEC_LEDS);
    } else if (type == "global") {
      sendCfgSection(SEC_BASE);
    } else if (type == "heating" && g_heatCfg.diRole != HEAT_DI_OFF) {
      sendCfgSection(SEC_INPUTS);
    }
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
  if (mb.Coil(COIL_PANIC_ON)) {
    mb.setCoil(COIL_PANIC_ON, false);
    panicEnter();
  }
  if (mb.Coil(COIL_ALL_OFF)) {
    mb.setCoil(COIL_ALL_OFF, false);
    allOutputsOffPulse();
  }
  if (mb.Coil(COIL_PANIC_OFF)) {
    mb.setCoil(COIL_PANIC_OFF, false);
    panicExit();
  }
  if (mb.Coil(COIL_STAIR_UP)) {
    mb.setCoil(COIL_STAIR_UP, false);
    stairForce(SEQ_DIR_UP);
  }
  if (mb.Coil(COIL_STAIR_DOWN)) {
    mb.setCoil(COIL_STAIR_DOWN, false);
    stairForce(SEQ_DIR_DOWN);
  }
  if (mb.Coil(COIL_STAIR_STOP)) {
    mb.setCoil(COIL_STAIR_STOP, false);
    stairForce(SEQ_DIR_NONE);
  }
  if (mb.Coil(COIL_HEAT_EXERCISE)) {
    mb.setCoil(COIL_HEAT_EXERCISE, false);
    heatQueueExerciseAll();
  }
}

// HR 432 master level and HR 436 scene recall. Both are runtime commands, not
// configuration: 436 is self-clearing and reads back 0, 432 is persisted
// because it is a setting a customer expects to survive a power cut.
static void processModbusHoldingWrites() {
  const uint16_t master = (uint16_t)mb.Hreg(HR_MASTER);
  const uint8_t m = (uint8_t)((master > 255) ? 255 : master);
  if (m != g_masterLevel) {
    g_masterLevel = m;
    mb.Hreg(HR_MASTER, m);
    recomputeAllDrivers();
    markCfgDirty();
  }

  const uint16_t scene = (uint16_t)mb.Hreg(HR_SCENE);
  if (scene != 0) {
    mb.Hreg(HR_SCENE, 0);
    if (scene <= NUM_SCENES && !outputsModbusLocked()) sceneRecall((uint8_t)scene);
  }

  const uint16_t inh = (uint16_t)mb.Hreg(HR_INHIBIT);
  const bool inhibit = (inh != 0);
  if (inhibit != g_seqInhibit) {
    g_seqInhibit = inhibit;
    mb.Hreg(HR_INHIBIT, inhibit ? 1 : 0);
  }

  const uint16_t night = (uint16_t)mb.Hreg(HR_NIGHT);
  if (night != g_stairCfg.nightLevel) {
    g_stairCfg.nightLevel = (uint8_t)((night > 255) ? 255 : night);
    mb.Hreg(HR_NIGHT, g_stairCfg.nightLevel);
    markCfgDirty();
  }
  const uint16_t day = (uint16_t)mb.Hreg(HR_DAY);
  if (day != g_stairCfg.dayLevel) {
    g_stairCfg.dayLevel = (uint8_t)((day > 255) ? 255 : day);
    mb.Hreg(HR_DAY, g_stairCfg.dayLevel);
    markCfgDirty();
  }

  const bool nightWin = ((uint16_t)mb.Hreg(HR_NIGHT_WINDOW) != 0);
  if (nightWin != g_seqNightWindow) {
    g_seqNightWindow = nightWin;
    mb.Hreg(HR_NIGHT_WINDOW, nightWin ? 1 : 0);
  }

  const uint16_t sum = (uint16_t)mb.Hreg(HR_SUMMER);
  const uint8_t summer = sum ? 1 : 0;
  if (summer != g_heatCfg.summerMode) {
    g_heatCfg.summerMode = summer;
    mb.Hreg(HR_SUMMER, summer);
    markCfgDirty();
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
    const bool logical = diReportedLive(i);
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
  if (tlcReady)              status |= ST_TLC_READY;
  if (linkOkNow(now))        status |= ST_LINK_OK;
  if (g_busFailsafeActive)   status |= ST_BUS_FS;
  if (cfgDirty)              status |= ST_CFG_DIRTY;
  if (g_localOverride)       status |= ST_OVERRIDE;
  if (g_panicActive)         status |= ST_PANIC;
  if (stairRunning())        status |= ST_SEQ_RUN;
  if (g_heatDemandOut)       status |= ST_HEAT_DEM;

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
  setIregIfChanged(IREG_ACTIVE_SCENE, g_activeScene);

  // Readback is the COMMANDED level, so a write to HR 400..431 reads back
  // unchanged; the driver duty after curve and master is not a bus value.
  for (uint16_t reg = IREG_OUT_BASE; reg < IREG_OUT_BASE + 16; reg++) {
    const uint8_t base = (uint8_t)((reg - IREG_OUT_BASE) * 2);
    const uint8_t lo = chEffectiveSource(base);
    const uint8_t hi = (base + 1 < NUM_PWM) ? chEffectiveSource(base + 1) : 0;
    setIregIfChanged(reg, (uint16_t)((hi << 8) | lo));
  }

  setIregIfChanged(IREG_SEQ_STATE, stairIregState());
  setIregIfChanged(IREG_SEQ_STEP, g_seqStep);
  setIregIfChanged(IREG_HEAT_FLAGS, heatIregFlags());
  setIregIfChanged(IREG_ZONES_OPEN, g_heatOpenCount);
  setIregIfChanged(IREG_ZONE_MASK_LO, (uint16_t)(g_heatOpenMask & 0xFFFFu));
  setIregIfChanged(IREG_ZONE_MASK_HI, (uint16_t)((g_heatOpenMask >> 16) & 0xFFFFu));
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
  mb.addCoil(COIL_PANIC_ON);
  mb.setCoil(COIL_PANIC_ON, false);
  mb.addCoil(COIL_ALL_OFF);
  mb.setCoil(COIL_ALL_OFF, false);
  mb.addCoil(COIL_PANIC_OFF);
  mb.setCoil(COIL_PANIC_OFF, false);
  mb.addCoil(COIL_STAIR_UP);
  mb.setCoil(COIL_STAIR_UP, false);
  mb.addCoil(COIL_STAIR_DOWN);
  mb.setCoil(COIL_STAIR_DOWN, false);
  mb.addCoil(COIL_STAIR_STOP);
  mb.setCoil(COIL_STAIR_STOP, false);
  mb.addCoil(COIL_HEAT_EXERCISE);
  mb.setCoil(COIL_HEAT_EXERCISE, false);
  for (uint16_t i = 0; i < NUM_PWM; i++) {
    mb.addHreg(HR_PWM_BASE + i);
    mb.Hreg(HR_PWM_BASE + i, chRequest[i]);
    g_prevHrPwm[i] = chRequest[i];
  }
  g_prevHrPwmInit = true;
  mb.addHreg(HR_MASTER);
  mb.Hreg(HR_MASTER, g_masterLevel);
  mb.addHreg(HR_INHIBIT);
  mb.Hreg(HR_INHIBIT, g_seqInhibit ? 1 : 0);
  mb.addHreg(HR_NIGHT);
  mb.Hreg(HR_NIGHT, g_stairCfg.nightLevel);
  mb.addHreg(HR_DAY);
  mb.Hreg(HR_DAY, g_stairCfg.dayLevel);
  mb.addHreg(HR_SCENE);
  mb.Hreg(HR_SCENE, 0);
  mb.addHreg(HR_SUMMER);
  mb.Hreg(HR_SUMMER, g_heatCfg.summerMode ? 1 : 0);
  mb.addHreg(HR_NIGHT_WINDOW);
  mb.Hreg(HR_NIGHT_WINDOW, g_seqNightWindow ? 1 : 0);
  mb.addHreg(HR_MB_ADDR);
  mb.Hreg(HR_MB_ADDR, g_mb_address);
  mb.addHreg(HR_MB_BAUD);
  mb.Hreg(HR_MB_BAUD, (g_mb_baud > 65535UL) ? (uint16_t)0 : (uint16_t)g_mb_baud);
  hmRegisterIdentity(mb, HM_MODEL_ID, HM_FW_MAJOR, HM_FW_MINOR, HM_FW_PATCH, HM_MAP_VERSION);
}

void sendWebIdentity() {
  JSONVar id;
  id["model"] = HM_MODEL_ID;
  id["fw"]    = HM_FW;
  id["map"]   = HM_FW;
  id["addr"]  = g_mb_address;
  id["baud"]  = g_mb_baud;
  WebSerial.send("identity", id);
}

void sendWebStatus() {
  JSONVar st;
  st["linkOk"] = linkOkNow(millis()) ? 1 : 0;
  st["busFailsafe"] = g_busFailsafeActive ? 1 : 0;
  st["localOverride"] = g_localOverride ? 1 : 0;
  st["panic"] = g_panicActive ? 1 : 0;
  st["seq"] = (int)g_seqPhase;
  st["seqDir"] = (int)g_seqDir;
  st["seqStep"] = (int)g_seqStep;
  st["heatDemand"] = g_heatDemandOut ? 1 : 0;
  st["zones"] = (int)g_heatOpenCount;
  st["frost"] = g_heatFrost ? 1 : 0;
  st["summer"] = heatSummer() ? 1 : 0;
  st["heatDi"] = heatDiClosed() ? 1 : 0;
  st["heatDiForce"] = heatDiForcesClosed() ? 1 : 0;
  bool heatEx = false;
  for (uint8_t i = 0; i < NUM_PWM; i++) if (g_heatExercise[i]) { heatEx = true; break; }
  st["heatEx"] = heatEx ? 1 : 0;
  WebSerial.send("status", st);
}

// ============================================================================
//  WEBCONFIG — the config is delivered in named, acknowledged sections
//
//  Why, precisely: the USB CDC FIFOs are 256 bytes each way (tusb_config.h
//  CFG_TUD_CDC_TX/RX_BUFSIZE) and SimpleWebSerial's inbound line buffer is
//  256 bytes too (BufferSize). A single-shot config dump is written straight
//  into the TX FIFO by Serial.println() and blocks the main loop until the
//  host drains it — long enough, with a page that is busy or gone, to miss
//  the 4 s watchdog. Sections keep every write inside one FIFO and let the
//  page tell us it is safe to send the next one.
//
//  Each chunk carries {sec, part, parts}; the page replies with
//  command {action:"cfgack", sec, part}. A card hydrates only when its own
//  section has arrived, so the page can never echo defaults back at us.
// ============================================================================
static const uint8_t CFG_FS_PER_PART = NUM_PWM / 4;      // 8 channels
static const uint8_t CFG_CH_PER_PART = 3;                // 11 parts, last holds 2
static const uint8_t CFG_SCN_PER_PART = 1;               // 8 parts, one scene each
static const uint8_t CFG_IN_PER_PART  = 1;               // 3 parts, one input each

static uint8_t  g_cfgSec = SEC_COUNT;   // SEC_COUNT = transfer idle
static uint8_t  g_cfgPart = 0;
static bool     g_cfgPendingSend = false;
static uint32_t g_cfgSentMs = 0;
static uint8_t  g_cfgRetry = 0;
static const uint32_t CFG_ACK_MS = 1200;
static const uint8_t  CFG_MAX_RETRY = 3;
static const size_t   CFG_TX_BUDGET = 200;   // keep a chunk inside one FIFO

static void cfgChunkAppendHexByte(String& s, uint8_t v) {
  static const char* hex = "0123456789ABCDEF";
  s += hex[v >> 4];
  s += hex[v & 0x0F];
}

// Valve hours / cycles — on request, never on the 250 ms status line.
// One compact hex dump does not fit a 256-byte FIFO (32×4 + 32×4 + envelope),
// so the reply is two 16-channel parts. The page asks when the Heating card
// is in view and again every 30 s while it stays there.
static const uint8_t HEAT_STATS_PER_PART = 16;

void sendHeatStats() {
  for (uint8_t part = 0; part < 2; part++) {
    if (!hmUsbCanSend(CFG_TX_BUDGET)) return;
    JSONVar msg;
    const uint8_t base = (uint8_t)(part * HEAT_STATS_PER_PART);
    msg["o"] = (int)base;
    msg["parts"] = 2;
    String hh, cc;
    hh.reserve(HEAT_STATS_PER_PART * 4);
    cc.reserve(HEAT_STATS_PER_PART * 4);
    for (uint8_t k = 0; k < HEAT_STATS_PER_PART; k++) {
      const uint8_t i = (uint8_t)(base + k);
      cfgChunkAppendHexByte(hh, (uint8_t)(g_heatStats.hours[i] >> 8));
      cfgChunkAppendHexByte(hh, (uint8_t)g_heatStats.hours[i]);
      cfgChunkAppendHexByte(cc, (uint8_t)(g_heatStats.cycles[i] >> 8));
      cfgChunkAppendHexByte(cc, (uint8_t)g_heatStats.cycles[i]);
    }
    msg["hrs"] = hh;
    msg["cyc"] = cc;
    WebSerial.send("heatStats", msg);
  }
}

static void sendCfgChunkNow(uint8_t sec, uint8_t part) {
  JSONVar cfg;
  cfg["sec"]   = CFG_SEC_NAME[sec];
  cfg["part"]  = (int)part;
  cfg["parts"] = (int)CFG_SEC_PARTS[sec];

  switch (sec) {
    case SEC_BASE:
      cfg["addr"]   = g_mb_address;
      cfg["baud"]   = g_mb_baud;
      cfg["master"] = (int)g_masterLevel;
      cfg["panic"]  = (int)g_panicLevel;
      break;

    case SEC_INPUTS: {
      const uint8_t base = (uint8_t)(part * CFG_IN_PER_PART);
      cfg["o"] = (int)base;
      for (uint8_t k = 0; k < CFG_IN_PER_PART && (base + k) < NUM_DI; k++) {
        const uint8_t i = (uint8_t)(base + k);
        cfg["in"][k]["enabled"] = diCfg[i].enabled ? 1 : 0;
        cfg["in"][k]["invert"]  = diCfg[i].inverted ? 1 : 0;
        cfg["in"][k]["action"]  = diCfg[i].action;
        cfg["in"][k]["target"]  = diCfg[i].target;
        cfg["in"][k]["level"]   = (int)diCfg[i].level;
        cfg["in"][k]["param"]   = (int)diCfg[i].param;
      }
      break;
    }

    case SEC_BUTTONS:
      for (int i = 0; i < NUM_BTN; i++) {
        cfg["btn"][i]["action"] = btnCfg[i].action;
        cfg["btn"][i]["param"]  = (int)btnCfg[i].param;
      }
      break;

    case SEC_LEDS:
      for (int i = 0; i < NUM_LED; i++) {
        cfg["led"][i]["mode"]   = ledCfg[i].mode;
        cfg["led"][i]["source"] = ledCfg[i].source;
      }
      break;

    case SEC_FAILSAFE: {
      const uint8_t base = (uint8_t)(part * CFG_FS_PER_PART);
      cfg["o"] = (int)base;
      if (part == 0) {
        cfg["timeout"]   = (int)g_busTimeoutS;
        cfg["ovTimeout"] = (int)g_overrideTimeoutS;
      }
      for (uint8_t k = 0; k < CFG_FS_PER_PART; k++) {
        cfg["a"][k] = (int)g_failsafeAction[base + k];
        cfg["l"][k] = (int)g_failsafeLevel[base + k];
      }
      break;
    }

    case SEC_CHANNELS: {
      const uint8_t base = (uint8_t)(part * CFG_CH_PER_PART);
      cfg["o"] = (int)base;
      const uint8_t n = (base + CFG_CH_PER_PART > NUM_PWM)
        ? (uint8_t)(NUM_PWM - base) : CFG_CH_PER_PART;
      for (uint8_t k = 0; k < n; k++) {
        const ChCfg& c = chCfg[base + k];
        cfg["p"][k]  = (int)c.profile;
        cfg["c"][k]  = (int)c.curve;
        cfg["mn"][k] = (int)c.minLevel;
        cfg["mx"][k] = (int)c.maxLevel;
        cfg["r"][k]  = (int)c.rampMs;
        cfg["ao"][k] = (int)c.autoOffS;
        cfg["f"][k]  = (int)c.flags;
      }
      break;
    }

    case SEC_GROUPS:
      // Hex, not a number: a 32-bit mask round-trips exactly through a string,
      // where a JSON double is at the mercy of how it gets printed.
      for (uint8_t g = 0; g < NUM_GROUPS; g++) {
        String hex;
        hex.reserve(8);
        for (int8_t b = 3; b >= 0; b--) cfgChunkAppendHexByte(hex, (uint8_t)(groupMask[g] >> (b * 8)));
        cfg["grp"][g] = hex;
      }
      break;

    case SEC_SCENES: {
      const uint8_t base = (uint8_t)(part * CFG_SCN_PER_PART);
      cfg["o"] = (int)base;
      for (uint8_t k = 0; k < CFG_SCN_PER_PART; k++) {
        String hex;
        hex.reserve(NUM_PWM * 2);
        for (uint8_t ch = 0; ch < NUM_PWM; ch++) cfgChunkAppendHexByte(hex, sceneLevel[base + k][ch]);
        cfg["scn"][k]  = hex;
        cfg["used"][k] = (int)sceneUsed[base + k];
      }
      break;
    }

    case SEC_STAIRS:
      if (part == 0) {
        cfg["n"]  = (int)g_stairCfg.stepCount;
        cfg["md"] = (int)g_stairCfg.mode;
        cfg["pt"] = (int)g_stairCfg.pattern;
        cfg["ww"] = (int)g_stairCfg.waveWidth;
        cfg["im"] = (int)g_stairCfg.inputEndMap;
        cfg["be"] = (int)g_stairCfg.bothEnds;
        cfg["bw"] = (int)g_stairCfg.bothWindowMs;
        cfg["d"]  = (int)g_stairCfg.stepDelayMs;
        cfg["h"]  = (int)g_stairCfg.holdS;
        cfg["f"]  = (int)g_stairCfg.fadeOutMs;
        cfg["ov"] = (int)g_stairCfg.overlapPct;
      } else if (part == 1) {
        cfg["rt"] = (int)g_stairCfg.retrigger;
        cfg["op"] = (int)g_stairCfg.oppose;
        cfg["nl"] = (int)g_stairCfg.nightLevel;
        cfg["dl"] = (int)g_stairCfg.dayLevel;
        cfg["sf"] = (int)g_stairCfg.standbyFirst;
        cfg["sl"] = (int)g_stairCfg.standbyLast;
        cfg["nn"] = (int)g_stairCfg.nightLightLevel;
        cfg["fd"] = (int)g_stairCfg.stepFadeMs;
        cfg["db"] = (int)g_stairCfg.debounceMs;
        cfg["mr"] = (int)g_stairCfg.minRepeatMs;
      } else {
        String hex;
        hex.reserve(NUM_PWM * 2);
        for (uint8_t s = 0; s < NUM_PWM; s++) cfgChunkAppendHexByte(hex, g_stairCfg.stepChannel[s]);
        cfg["map"] = hex;
      }
      break;

    case SEC_HEATING: {
      cfg["p"]  = (int)g_heatCfg.slowPwmPeriodS;
      cfg["ph"] = (int)g_heatCfg.phaseSpreadPct;
      cfg["mx"] = (int)g_heatCfg.maxOpenZones;
      cfg["di"] = (int)g_heatCfg.diRole;
      cfg["fo"] = (int)g_heatCfg.firstOpenDelayS;
      cfg["ov"] = (int)g_heatCfg.overrunS;
      cfg["eh"] = (int)g_heatCfg.exerciseIntervalH;
      cfg["ed"] = (int)g_heatCfg.exerciseDurationS;
      cfg["af"] = (int)g_heatCfg.antifreezeHours;
      cfg["sm"] = (int)g_heatCfg.summerMode;
      cfg["mp"] = (int)heatMinPulsePct();
      String nc;
      nc.reserve(8);
      uint32_t ncMask = 0;
      for (uint8_t i = 0; i < NUM_PWM; i++) if (chCfg[i].flags & 0x01) ncMask |= (1UL << i);
      for (int8_t b = 3; b >= 0; b--) cfgChunkAppendHexByte(nc, (uint8_t)(ncMask >> (b * 8)));
      cfg["nc"] = nc;
      break;
    }

    default: break;
  }

  WebSerial.send("cfg", cfg);
}

static void cfgTransferStepTo(uint8_t sec, uint8_t part) {
  g_cfgSec = sec;
  g_cfgPart = part;
  g_cfgPendingSend = true;
  g_cfgRetry = 0;
}

static void cfgTransferNext() {
  if (g_cfgSec >= SEC_COUNT) return;
  uint8_t sec = g_cfgSec;
  uint8_t part = (uint8_t)(g_cfgPart + 1);
  while (sec < SEC_COUNT && part >= CFG_SEC_PARTS[sec]) {
    sec++;
    part = 0;
  }
  if (sec >= SEC_COUNT) {
    g_cfgSec = SEC_COUNT;
    g_cfgPendingSend = false;
    return;
  }
  cfgTransferStepTo(sec, part);
}

void cfgTransferStart() {
  cfgTransferStepTo(SEC_BASE, 0);
}

// One section on its own — used after a change so the page re-hydrates just
// the card that moved instead of taking the whole config again.
void sendCfgSection(uint8_t sec) {
  if (sec >= SEC_COUNT) return;
  if (g_cfgSec < SEC_COUNT) return;   // a full transfer is already running
  cfgTransferStepTo(sec, 0);
}

void handleCfgAck(const char* sec, int part) {
  if (g_cfgSec >= SEC_COUNT || g_cfgPendingSend) return;
  if (!sec || strcmp(sec, CFG_SEC_NAME[g_cfgSec]) != 0) return;
  if (part != (int)g_cfgPart) return;
  cfgTransferNext();
}

void serviceCfgTransfer(uint32_t now) {
  if (g_cfgSec >= SEC_COUNT) return;

  if (g_cfgPendingSend) {
    if (!hmUsbCanSend(CFG_TX_BUDGET)) return;   // no room yet — never block here
    sendCfgChunkNow(g_cfgSec, g_cfgPart);
    g_cfgPendingSend = false;
    g_cfgSentMs = now;
    return;
  }

  if ((uint32_t)(now - g_cfgSentMs) < CFG_ACK_MS) return;
  if (g_cfgRetry < CFG_MAX_RETRY) {
    g_cfgRetry++;
    g_cfgPendingSend = true;
    return;
  }
  // The page is not acknowledging. Move on rather than wedge the sequence;
  // its card simply stays un-hydrated and therefore read-only.
  wsLog(String("cfg: no ack for section ") + CFG_SEC_NAME[g_cfgSec]);
  cfgTransferNext();
}

// Current output levels — small enough to go as one message, and already sent
// periodically by the main loop, so it is not part of the section sequence.
void sendWebLevels() {
  if (!hmUsbCanSend(CFG_TX_BUDGET)) return;
  JSONVar ext;
  for (int i = 0; i < NUM_PWM; i++) ext["pwm"][i] = (int)chRequest[i];
  ext["tlc_ready"] = tlcReady ? 1 : 0;
  for (int i = 0; i < 4; i++) ext["tlc_chip"][i] = tlcChipOk[i] ? 1 : 0;
  WebSerial.send("ext", ext);
}

void sendWebCfg() {
  cfgTransferStart();
}

void sendWebBootstrap() {
  sendWebIdentity();
  sendWebStatus();
  cfgTransferStart();
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

  buildCurveLuts();          // stage 3 tables — needed before any level resolves
  setDefaults();
  if (!initFilesystemAndConfig()) wsLog("FATAL: Filesystem/config init failed");
  heatLoadStats();
  stairRebuildMask();
  if (stairEnabled()) stairApplyIdleBase();

  // Restored levels re-enter the pipeline; boot does not ramp, it lands.
  recomputeAllDrivers();
  for (uint8_t i = 0; i < NUM_PWM; i++) {
    pwmLevel[i] = chDriver[i];
    chRampLastMs[i] = millis();
  }

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

  if (!g_prevHrPwmInit) {
    for (int i = 0; i < NUM_PWM; i++) g_prevHrPwm[i] = (uint16_t)mb.Hreg(HR_PWM_BASE + i);
    g_prevHrPwmInit = true;
  }
  // A changed holding register is a command from the bus: it enters the
  // pipeline through setChannelRequest, which is also where groups propagate.
  // While panic / failsafe / local override hold the outputs the register is
  // accepted but not applied; releasing them replays it from the registers.
  const bool outputsHeld = outputsModbusLocked();
  for (uint8_t i = 0; i < NUM_PWM; i++) {
    uint16_t v = (uint16_t)mb.Hreg(HR_PWM_BASE + i);
    if (v == g_prevHrPwm[i]) continue;
    if (v > 255) { v = 255; mb.Hreg(HR_PWM_BASE + i, v); }
    g_prevHrPwm[i] = v;
    if (heatIsChannel(i)) {
      g_heatDuty[i] = (uint8_t)((v > 100) ? 100 : v);
      continue;                          // slow PWM owns the source
    }
    if (stairRunning() && stairOwnsChannel(i)) continue;
    if (outputsHeld) continue;
    setChannelRequest(i, (uint8_t)v, SRC_MODBUS);  // brightness not auto-persisted
  }
  processModbusHoldingWrites();

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
  serviceAutoOff(now);
  stairService(now);
  heatService(now);
  serviceRamp(now);              // stages 5 and 6 of the pipeline
  serviceCfgTransfer(now);

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

    sendWebLevels();
  }
}
