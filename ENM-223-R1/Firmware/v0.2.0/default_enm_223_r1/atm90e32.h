#pragma once
#include <Arduino.h>
#include <SPI.h>

// RP2350-focused ATM90E32 driver (SPI, no external libs)
// - Uses SPI transactions
// - Assumes CS is active-low by default
// - PM0/PM1 are MCU outputs into ATM (per your schematic)

struct M90PhaseCal {
  uint16_t Ugain;
  uint16_t Igain;
  int16_t  Uoffset;
  int16_t  Ioffset;
};

struct M90DiagRegs {
  uint16_t EMMState0;
  uint16_t EMMState1;
  uint16_t EMMIntState0;
  uint16_t EMMIntState1;
  uint16_t CRCErrStatus;
  uint16_t LastSPIData;
};

struct M90ChipEv {
  bool sag[3];
  bool ov[3];
  bool phaseLoss[3];
  bool overI[3];
  bool freqHi;
  bool freqLo;
  bool revPhase;
};

void decodeM90ChipEv(const M90DiagRegs& d, M90ChipEv& out);

class ATM90E32 {
public:
  ATM90E32(SPIClass &spi, uint8_t pinCS, uint8_t pinPM0, uint8_t pinPM1,
           uint32_t spiHz = 200000, uint8_t spiMode = SPI_MODE0,
           bool csActiveHigh = false)
  : spi_(spi), cs_(pinCS), pm0_(pinPM0), pm1_(pinPM1),
    spiHz_(spiHz), spiMode_(spiMode), csActiveHigh_(csActiveHigh) {}

  void begin(uint16_t lineHz, uint8_t sumAbs, uint8_t wireMode, const uint8_t phaseMap[3],
             uint16_t ucal, const M90PhaseCal cal[3]);
  void applyCalibration(const M90PhaseCal cal[3]);
  void readPhaseCal(M90PhaseCal out[3]);

  // Live helpers (scales chosen to match typical UI: V in 0.01, A in 0.001)
  double readUrmsA_V(); double readUrmsB_V(); double readUrmsC_V();
  double readIrmsA_A(); double readIrmsB_A(); double readIrmsC_A();

  int16_t  readPFmeanA(); int16_t readPFmeanB(); int16_t readPFmeanC(); int16_t readPFmeanT();
  int32_t  readPmeanW(uint8_t phase);   // 0=A,1=B,2=C,3=Total
  int32_t  readQmean_var(uint8_t phase);
  int32_t  readSmean_VA(uint8_t phase);
  double   readUPeak_V(uint8_t phase);  // 0=A,1=B,2=C
  double   readIPeak_A(uint8_t phase);
  double   readIrmsN_A();
  int32_t  readPmeanFundW(uint8_t phase);
  int32_t  readPmeanHarmW(uint8_t phase);
  uint16_t readThdPct_x100(uint8_t phase); // active-power THD, 0.01 % units
  int16_t  readPAngleA(); int16_t readPAngleB(); int16_t readPAngleC();
  uint16_t readFreq_x100();
  int16_t  readTempC();

  // Energy counters (raw chip counts)
  uint16_t rdAP_A(); uint16_t rdAP_B(); uint16_t rdAP_C(); uint16_t rdAP_T();
  uint16_t rdAN_A(); uint16_t rdAN_B(); uint16_t rdAN_C(); uint16_t rdAN_T();
  uint16_t rdRP_A(); uint16_t rdRP_B(); uint16_t rdRP_C(); uint16_t rdRP_T();
  uint16_t rdRN_A(); uint16_t rdRN_B(); uint16_t rdRN_C(); uint16_t rdRN_T();
  uint16_t rdSA_A(); uint16_t rdSA_B(); uint16_t rdSA_C(); uint16_t rdSA_T();

  M90DiagRegs readDiag();
  // Debug helper (raw 16-bit register read)
  uint16_t debugRead16(uint16_t reg);
  void clearDiagInterrupts();
  void resetPeakRegisters();

  // Config currently applied
  uint16_t lineHz() const { return lineHz_; }
  uint8_t  sumAbs() const { return sumAbs_; }
  uint8_t  wireMode() const { return wireMode_; }
  uint16_t ucal()   const { return ucal_; }

private:
  uint16_t xfer(uint8_t rw, uint16_t reg, uint16_t val);
  inline void csAssert()  { digitalWrite(cs_, csActiveHigh_ ? HIGH : LOW);  }
  inline void csRelease() { digitalWrite(cs_, csActiveHigh_ ? LOW  : HIGH); }
  inline uint16_t read16(uint16_t reg) { return xfer(1, reg, 0xFFFF); }
  inline void     write16(uint16_t reg, uint16_t val) { (void)xfer(0, reg, val); }

  double readRmsLike(uint16_t regH, uint16_t regLSB, double sH, double sLb);
  int32_t  read32Signed(uint16_t regH, uint16_t regL);
  int32_t  readMeanPowerW(uint16_t regH, uint16_t regL);

  uint16_t lineHz_ = 50;
  uint8_t  sumAbs_ = 1;
  uint8_t  wireMode_ = 0;
  uint8_t  phaseMap_[3] = {0, 1, 2};
  uint16_t ucal_   = 25256;
  uint16_t sagPeakDetCfg_ = 0x143F;

  SPIClass &spi_;
  uint8_t cs_, pm0_, pm1_;
  uint32_t spiHz_;
  uint8_t  spiMode_;
  bool     csActiveHigh_;
};
