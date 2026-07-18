#pragma once
#include <Arduino.h>
#include <ModbusSerial.h>
#include "hardware/watchdog.h"
// ===== HomeMaster common helpers (v0.2.0) =====
// ---- Identity (Input Registers, FC=04), base 200 = 0x00C8 ----
// layout: +0 MODEL_ID, +1 FW_MAJOR, +2 FW_MINOR, +3 FW_PATCH, +4 MAP_VERSION
static const uint16_t HM_IDENT_BASE = 0x00C8;
inline void hmRegisterIdentity(ModbusSerial& mb, uint16_t model,
                               uint8_t fwMajor, uint8_t fwMinor, uint8_t fwPatch,
                               uint16_t mapVersion) {
  mb.addIreg(HM_IDENT_BASE + 0); mb.Ireg(HM_IDENT_BASE + 0, model);
  mb.addIreg(HM_IDENT_BASE + 1); mb.Ireg(HM_IDENT_BASE + 1, fwMajor);
  mb.addIreg(HM_IDENT_BASE + 2); mb.Ireg(HM_IDENT_BASE + 2, fwMinor);
  mb.addIreg(HM_IDENT_BASE + 3); mb.Ireg(HM_IDENT_BASE + 3, fwPatch);
  mb.addIreg(HM_IDENT_BASE + 4); mb.Ireg(HM_IDENT_BASE + 4, mapVersion);
}
// ---- Modbus parameter validation ----
inline uint8_t hmValidAddress(int addr) {
  if (addr < 1)   return 1;
  if (addr > 247) return 247;     // 248..255 reserved by Modbus spec
  return (uint8_t)addr;
}
inline uint32_t hmValidBaud(uint32_t baud) {
  const uint32_t allowed[] = {9600, 19200, 38400, 57600, 115200};
  for (uint32_t b : allowed) if (b == baud) return baud;
  return 19200;                   // default if baud not in whitelist
}
// ---- Watchdog ----
inline void hmWatchdogArm(uint32_t timeout_ms = 4000) {
  watchdog_enable(timeout_ms, true);   // true = pause when debugger attached
}
inline void hmWatchdogFeed() { watchdog_update(); }
// ---- Power-on output policy (v0.2.0 Phase B) ----
enum HmPowerOn : uint8_t { HM_PWR_OFF = 0, HM_PWR_ON = 1, HM_PWR_RESTORE = 2 };
