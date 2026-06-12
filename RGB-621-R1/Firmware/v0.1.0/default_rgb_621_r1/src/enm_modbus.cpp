#include "enm_modbus.h"
#include "atm90e32.h"
namespace enm223 {

static ModbusSerial mb(Serial2, SLAVE_ID, TXEN_PIN);
static uint8_t  g_addr  = SLAVE_ID;
static uint32_t g_baud  = 19200;

// SFINAE for setSlaveId differences across library versions
template <class M>
inline auto _setSlaveIdIfAvailable(M& m, uint8_t id)
-> decltype(std::declval<M&>().setSlaveId(uint8_t{}), void()) { m.setSlaveId(id); }
inline void _setSlaveIdIfAvailable(...) {}

static inline void _put_u32(uint16_t reg, uint32_t v){
  mb.Ireg(reg+0, (uint16_t)((v>>16)&0xFFFF));
  mb.Ireg(reg+1, (uint16_t)(v & 0xFFFF));
}
static inline void _put_s32(uint16_t reg, int32_t v){
  mb.Ireg(reg+0, (uint16_t)((v>>16)&0xFFFF));
  mb.Ireg(reg+1, (uint16_t)(v & 0xFFFF));
}

// ---------- Lifecycle ----------
void MB_buildRegisterMap(uint16_t sample_ms, uint16_t lineHz, uint8_t sumAbs, uint16_t ucal)
{
  // Telemetry URMS/IRMS
  for (uint16_t i=0;i<3;i++) mb.addIreg(100 + i);
  for (uint16_t i=0;i<3;i++) mb.addIreg(110 + i);

  // P/Q/S as 4x s32 pairs each block (8 regs per block)
  for (uint16_t i=0;i<8;i++) mb.addIreg(200 + i);
  for (uint16_t i=0;i<8;i++) mb.addIreg(210 + i);
  for (uint16_t i=0;i<8;i++) mb.addIreg(220 + i);

  // PF (L1..L3) + total
  for (uint16_t i=0;i<3;i++) mb.addIreg(240 + i);
  mb.addIreg(243);
  // Angles
  for (uint16_t i=0;i<3;i++) mb.addIreg(244 + i);

  // Freq/Temp
  mb.addIreg(250);
  mb.addIreg(251);

  // Energies (40 regs: 20 pairs)
  for (uint16_t r=300; r<=338+1; ++r) mb.addIreg(r);

  // Diagnostics
  for (uint16_t r=360; r<=365; ++r) mb.addIreg(r);

  // Holding (RW) options
  mb.addHreg(400); mb.Hreg(400, sample_ms);
  mb.addHreg(401); mb.Hreg(401, lineHz);
  mb.addHreg(402); mb.Hreg(402, sumAbs);
  mb.addHreg(403); mb.Hreg(403, ucal);

  // Discrete inputs: LEDs, Buttons, Relays, Alarms
  for (uint16_t i=0;i<4;i++) mb.addIsts(500 + i); // LEDs
  for (uint16_t i=0;i<4;i++) mb.addIsts(520 + i); // Buttons
  for (uint16_t i=0;i<2;i++) mb.addIsts(540 + i); // Relays
  for (uint16_t i=0;i<CH_COUNT*AK_COUNT;i++) mb.addIsts(560 + i); // Alarms

  // Coils: Relays + Alarm ACKs
  mb.addCoil(600); mb.Coil(600, 0);
  mb.addCoil(601); mb.Coil(601, 0);
  for (uint16_t i=0;i<CH_COUNT;i++){ mb.addCoil(610 + i); mb.Coil(610 + i, 0); }
}

void MB_begin(uint8_t slaveId, uint32_t baud)
{
  g_addr = slaveId; g_baud = baud;

  Serial2.setTX(TX2_PIN);
  Serial2.setRX(RX2_PIN);
  Serial2.begin(g_baud);

  mb.config(g_baud);
  _setSlaveIdIfAvailable(mb, g_addr);

  // Build map with safe defaults (caller may overwrite immediately)
  MB_buildRegisterMap(/*sample_ms*/200, /*lineHz*/50, /*sumAbs*/1, /*ucal*/25256);
}

void MB_applySettings(uint8_t addr, uint32_t baud, JSONVar *status)
{
  if (g_baud != baud) {
    Serial2.end();
    Serial2.begin(baud);
    mb.config(baud);
    g_baud = baud;
  }
  if (g_addr != addr) {
    _setSlaveIdIfAvailable(mb, addr);
    g_addr = addr;
  }
  if (status) {
    (*status)["address"] = g_addr;
    (*status)["baud"]    = g_baud;
  }
}

void MB_task(){ mb.task(); }

// ---------- HREG watcher ----------
static uint16_t prevSm = 0xFFFF, prevLf = 0xFFFF, prevSu = 0xFFFF, prevUc = 0xFFFF;

bool MB_serviceHoldingRegs(
  uint16_t &sample_ms,
  uint16_t &atm_lineFreqHz,
  uint8_t  &atm_sumModeAbs,
  uint16_t &atm_ucal,
  void (*reinit_cb)()
){
  bool changed=false, reinit=false;

  const uint16_t sm = mb.Hreg(400);
  const uint16_t lf = mb.Hreg(401);
  const uint16_t su = mb.Hreg(402);
  const uint16_t uc = mb.Hreg(403);

  if (sm != prevSm) {
    prevSm = sm;
    int v = (int)sm; if (v<10) v=10; if (v>5000) v=5000;
    sample_ms = (uint16_t)v;
    changed = true;
  }
  if (lf != prevLf) {
    prevLf = lf;
    atm_lineFreqHz = (lf==60)?60:50;
    reinit=true; changed=true;
  }
  if (su != prevSu) {
    prevSu = su;
    atm_sumModeAbs = su?1:0;
    reinit=true; changed=true;
  }
  if (uc != prevUc) {
    prevUc = uc;
    atm_ucal = uc;
    changed = true;
  }

  if (reinit && reinit_cb) reinit_cb();
  return changed;
}

// ---------- Pushers ----------
void MB_setURMS(const uint16_t urms_cV[3]){ for (int i=0;i<3;i++) mb.Ireg(100+i, urms_cV[i]); }
void MB_setIRMS(const uint16_t irms_mA[3]){ for (int i=0;i<3;i++) mb.Ireg(110+i, irms_mA[i]); }

void MB_setPFraw(const int16_t pf_raw[4]){
  for (int i=0;i<3;i++) mb.Ireg(240+i, (uint16_t)pf_raw[i]);
  mb.Ireg(243, (uint16_t)pf_raw[3]);
}
void MB_setAngles(const int16_t ang_raw[3]){ for (int i=0;i<3;i++) mb.Ireg(244+i, (uint16_t)ang_raw[i]); }

void MB_setFreqTemp(uint16_t f_x100, int16_t tempC){
  mb.Ireg(250, f_x100);
  mb.Ireg(251, (uint16_t)tempC);
}

void MB_setPQS(const int32_t P_W[4], const int32_t Q_var[4], const int32_t S_VA[4]){
  _put_s32(200+0, P_W[0]); _put_s32(200+2, P_W[1]); _put_s32(200+4, P_W[2]); _put_s32(200+6, P_W[3]);
  _put_s32(210+0, Q_var[0]); _put_s32(210+2, Q_var[1]); _put_s32(210+4, Q_var[2]); _put_s32(210+6, Q_var[3]);
  _put_s32(220+0, S_VA[0]); _put_s32(220+2, S_VA[1]); _put_s32(220+4, S_VA[2]); _put_s32(220+6, S_VA[3]);
}

void MB_setEnergies(
  const uint32_t ap_Wh[4], const uint32_t an_Wh[4],
  const uint32_t rp_varh[4], const uint32_t rn_varh[4],
  const uint32_t s_VAh[4]
){
  // AP 300..306
  _put_u32(300, ap_Wh[0]); _put_u32(302, ap_Wh[1]); _put_u32(304, ap_Wh[2]); _put_u32(306, ap_Wh[3]);
  // AN 308..314
  _put_u32(308, an_Wh[0]); _put_u32(310, an_Wh[1]); _put_u32(312, an_Wh[2]); _put_u32(314, an_Wh[3]);
  // RP 316..322
  _put_u32(316, rp_varh[0]); _put_u32(318, rp_varh[1]); _put_u32(320, rp_varh[2]); _put_u32(322, rp_varh[3]);
  // RN 324..330
  _put_u32(324, rn_varh[0]); _put_u32(326, rn_varh[1]); _put_u32(328, rn_varh[2]); _put_u32(330, rn_varh[3]);
  // S  332..338
  _put_u32(332, s_VAh[0]);  _put_u32(334, s_VAh[1]);  _put_u32(336, s_VAh[2]);  _put_u32(338, s_VAh[3]);
}

void MB_setDiag(const M90DiagRegs &d){
  mb.Ireg(360, d.EMMState0);
  mb.Ireg(361, d.EMMState1);
  mb.Ireg(362, d.EMMIntState0);
  mb.Ireg(363, d.EMMIntState1);
  mb.Ireg(364, d.CRCErrStatus);
  mb.Ireg(365, d.LastSPIData);
}

// ---------- ISTS helpers ----------
void MB_setLedIsts(uint8_t idx, bool on){       if (idx<4) mb.setIsts(500 + idx, on); }
void MB_setButtonIsts(uint8_t idx, bool on){    if (idx<4) mb.setIsts(520 + idx, on); }
void MB_setRelayIsts(uint8_t idx, bool on){     if (idx<2) mb.setIsts(540 + idx, on); }
void MB_setAlarmIsts(uint8_t ch, uint8_t kind, bool on){
  if (ch < CH_COUNT && kind < AK_COUNT) mb.setIsts(560 + (ch*AK_COUNT + kind), on);
}

// ---------- Coils handler ----------
bool MB_serviceCoils(
  bool canExternalWrite[2],
  bool desiredRelay[2],
  void (*ack_cb)(uint8_t ch)
){
  bool wantsChange=false;

  // Relay 0
  if (canExternalWrite[0]) {
    bool coil = mb.Coil(600);
    if (coil != desiredRelay[0]) { desiredRelay[0] = coil; wantsChange=true; }
  } else {
    if (mb.Coil(600) != desiredRelay[0]) mb.Coil(600, desiredRelay[0]);
  }

  // Relay 1
  if (canExternalWrite[1]) {
    bool coil = mb.Coil(601);
    if (coil != desiredRelay[1]) { desiredRelay[1] = coil; wantsChange=true; }
  } else {
    if (mb.Coil(601) != desiredRelay[1]) mb.Coil(601, desiredRelay[1]);
  }

  // Alarm ACKs
  for (uint16_t ch=0; ch<CH_COUNT; ++ch) {
    uint16_t addr = 610 + ch;
    if (mb.Coil(addr)) {
      if (ack_cb) ack_cb((uint8_t)ch);
      mb.Coil(addr, 0);
    }
  }

  return wantsChange;
}

// ---------- Status filler ----------
void MB_fillStatus(JSONVar *status){
  if (!status) return;
  (*status)["address"] = g_addr;
  (*status)["baud"]    = g_baud;
}

} // namespace enm223
