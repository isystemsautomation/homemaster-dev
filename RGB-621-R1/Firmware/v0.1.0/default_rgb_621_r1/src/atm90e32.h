// ================================================
// File: src/atm90e32.h
// Externalized ATM90E32 driver (SPI, no external libs)
// Target: RP2350/RP2040 (Arduino core)
// License: same as project
// ================================================
#pragma once
#include <Arduino.h>
#include <SPI.h>

namespace enm223 {

struct M90PhaseCal {
  uint16_t Ugain;   // voltage gain
  uint16_t Igain;   // current gain
  int16_t  Uoffset; // voltage offset
  int16_t  Ioffset; // current offset
};

struct M90DiagRegs {
  uint16_t EMMState0;
  uint16_t EMMState1;
  uint16_t EMMIntState0;
  uint16_t EMMIntState1;
  uint16_t CRCErrStatus;
  uint16_t LastSPIData;
};

class ATM90E32 {
public:
  // Construct with SPI bus + pins
  ATM90E32(SPIClass &spi, uint8_t pinCS, uint8_t pinPM0, uint8_t pinPM1,
           uint32_t spiHz = 200000, uint8_t spiMode = SPI_MODE0,
           bool csActiveHigh = false)
  : spi_(spi), cs_(pinCS), pm0_(pinPM0), pm1_(pinPM1),
    spiHz_(spiHz), spiMode_(spiMode), csActiveHigh_(csActiveHigh) {}

  // Initialize the chip with options + calibration
  void begin(uint16_t lineHz, uint8_t sumAbs, uint16_t ucal, const M90PhaseCal cal[3]);

  // Change just the calibration (no full init)
  void applyCalibration(const M90PhaseCal cal[3]);

  // Simple helpers the app expects
  double   readRmsLike(uint16_t regH, uint16_t regLSB, double sH, double sLb);
  int16_t  readPFmeanA(); int16_t readPFmeanB(); int16_t readPFmeanC(); int16_t readPFmeanT();
  int16_t  readPAngleA(); int16_t readPAngleB(); int16_t readPAngleC();
  uint16_t readFreq_x100();
  int16_t  readTempC();

  // Energy counters (raw chip counts)
  uint16_t rdAP_A(); uint16_t rdAP_B(); uint16_t rdAP_C(); uint16_t rdAP_T();
  uint16_t rdAN_A(); uint16_t rdAN_B(); uint16_t rdAN_C(); uint16_t rdAN_T();
  uint16_t rdRP_A(); uint16_t rdRP_B(); uint16_t rdRP_C(); uint16_t rdRP_T();
  uint16_t rdRN_A(); uint16_t rdRN_B(); uint16_t rdRN_C(); uint16_t rdRN_T();
  uint16_t rdSA_A(); uint16_t rdSA_B(); uint16_t rdSA_C(); uint16_t rdSA_T();

  // Diagnostics block
  M90DiagRegs readDiag();

private:
  // Low‑level SPI
  uint16_t xfer(uint8_t rw, uint16_t reg, uint16_t val);
  inline void csAssert()  { digitalWrite(cs_, csActiveHigh_ ? HIGH : LOW);  }
  inline void csRelease() { digitalWrite(cs_, csActiveHigh_ ? LOW  : HIGH); }
  inline uint16_t read16(uint16_t reg)        { return xfer(1, reg, 0xFFFF); }
  inline void     write16(uint16_t reg, uint16_t val) { (void)xfer(0, reg, val); }

  // Cached options
  uint16_t lineHz_ = 50;
  uint8_t  sumAbs_ = 1;
  uint16_t ucal_   = 25256;

  // HW
  SPIClass &spi_;
  uint8_t cs_, pm0_, pm1_;
  uint32_t spiHz_;
  uint8_t  spiMode_;
  bool     csActiveHigh_;
};

} // namespace enm223
