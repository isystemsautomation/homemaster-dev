#pragma once
#include <Arduino.h>
#include <ModbusSerial.h>
#include <Arduino_JSON.h>

namespace enm223 {
struct M90DiagRegs; 
// ================= Pins & constants =================
constexpr int  TX2_PIN   = 4;
constexpr int  RX2_PIN   = 5;
constexpr int  TXEN_PIN  = -1;     // not used
constexpr int  SLAVE_ID  = 1;

enum : uint8_t { CH_L1=0, CH_L2, CH_L3, CH_TOT, CH_COUNT };
enum : uint8_t { AK_ALARM=0, AK_WARNING, AK_EVENT, AK_COUNT };

// =============== Lifecycle ===============
/** Bring up UART and Modbus, build the whole register map and seed initial values. */
void MB_begin(uint8_t slaveId, uint32_t baud);

/** Reconfigure address/baud safely and reflect into a JSON status blob (optional). */
void MB_applySettings(uint8_t addr, uint32_t baud, JSONVar *status /*nullable*/);

/** Must be called from loop(). */
void MB_task();

// =============== Holding-register watcher ===============
/** 
 * Watches HREG 400..403 (sample_ms, lineHz, sumAbs, ucal).
 * Calls reinit_cb() when lineHz/sumAbs changed.
 * Returns true if any of the 4 values changed.
 */
bool MB_serviceHoldingRegs(
  uint16_t &sample_ms,
  uint16_t &atm_lineFreqHz,
  uint8_t  &atm_sumModeAbs,
  uint16_t &atm_ucal,
  void (*reinit_cb)()
);

// =============== Pushers (write data into Modbus map) ===============
void MB_setURMS   (const uint16_t urms_cV[3]);       // Ireg 100..102 (0.01V)
void MB_setIRMS   (const uint16_t irms_mA[3]);       // Ireg 110..112 (0.001A)
void MB_setPFraw  (const int16_t  pf_raw[4]);        // Ireg 240..243 (x1000, signed)
void MB_setAngles (const int16_t  ang_raw[3]);       // Ireg 244..246 (0.1 deg, signed)
void MB_setFreqTemp(uint16_t f_x100, int16_t tempC); // Ireg 250,251

// P/Q/S are signed 32-bit whole units, mapped as high-word then low-word:
void MB_setPQS(const int32_t P_W[4], const int32_t Q_var[4], const int32_t S_VA[4]); // Ireg 200..207,210..217,220..227

// Energies (u32, whole Wh/varh/VAh) — pairs high/low words:
void MB_setEnergies(
  const uint32_t ap_Wh[4], const uint32_t an_Wh[4],
  const uint32_t rp_varh[4], const uint32_t rn_varh[4],
  const uint32_t s_VAh[4]                                // Ireg 300..339
);

// Diagnostics block:
void MB_setDiag(const M90DiagRegs &d);                   // Ireg 360..365

// =============== ISTS helpers (Discrete Inputs) ===============
void MB_setLedIsts(uint8_t idx, bool on);                // ISTS 500..503
void MB_setButtonIsts(uint8_t idx, bool pressed);        // ISTS 520..523
void MB_setRelayIsts(uint8_t idx, bool on);              // ISTS 540..541
void MB_setAlarmIsts(uint8_t ch, uint8_t kind, bool on); // ISTS 560..(560+CH*AK+kind)

// =============== Coils & external control ===============
/**
 * Handle coils:
 *  - Relay coils 600/601:
 *      If `canExternalWrite[idx]` true, desiredRelay[idx] becomes coil value.
 *      If false, the coil is forced back to desiredRelay[idx].
 *  - Alarm ACK coils 610..(610+CH_COUNT-1): when a bit is 1, we call ack_cb(ch) and clear it.
 *
 * Returns true if it wants you to change a relay state (compare returned desiredRelay).
 */
bool MB_serviceCoils(
  bool canExternalWrite[2],
  bool desiredRelay[2],                    // in/out: current physical or target state
  void (*ack_cb)(uint8_t ch)               // nullable
);

// =============== Register-map builder (called from MB_begin) ===============
void MB_buildRegisterMap(uint16_t sample_ms, uint16_t lineHz, uint8_t sumAbs, uint16_t ucal);

// =============== Convenience for your UI echo ===============
/** Fill "status" JSON with current Modbus addr/baud (if status!=nullptr). */
void MB_fillStatus(JSONVar *status);

} // namespace enm223
