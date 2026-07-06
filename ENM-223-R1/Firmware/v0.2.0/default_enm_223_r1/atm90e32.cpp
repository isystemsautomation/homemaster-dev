#include "atm90e32.h"
#include <math.h>

// ---- Register map (subset) ----
static constexpr uint16_t ChannelMapI   = 0x01;
static constexpr uint16_t ChannelMapU   = 0x02;
static constexpr uint16_t MeterEn       = 0x00;
static constexpr uint16_t SagPeakDetCfg = 0x05;
static constexpr uint16_t ZXConfig      = 0x07;
static constexpr uint16_t SagTh         = 0x08;
static constexpr uint16_t FreqLoTh      = 0x0C;
static constexpr uint16_t FreqHiTh      = 0x0D;
static constexpr uint16_t PLconstH      = 0x31;
static constexpr uint16_t PLconstL      = 0x32;
static constexpr uint16_t MMode0        = 0x33;
static constexpr uint16_t MMode1        = 0x34;
static constexpr uint16_t PStartTh      = 0x35;
static constexpr uint16_t QStartTh      = 0x36;
static constexpr uint16_t SStartTh      = 0x37;
static constexpr uint16_t PPhaseTh      = 0x38;
static constexpr uint16_t QPhaseTh      = 0x39;
static constexpr uint16_t SPhaseTh      = 0x3A;
static constexpr uint16_t SoftReset     = 0x70;
static constexpr uint16_t EMMState0     = 0x71;
static constexpr uint16_t EMMState1     = 0x72;
static constexpr uint16_t EMMIntState0  = 0x73;
static constexpr uint16_t EMMIntState1  = 0x74;
static constexpr uint16_t EMMIntEn0     = 0x75;
static constexpr uint16_t EMMIntEn1     = 0x76;
static constexpr uint16_t LastSPIData   = 0x78;
static constexpr uint16_t CRCErrStatus  = 0x79;
static constexpr uint16_t CfgRegAccEn   = 0x7F;

// Cal registers
static constexpr uint16_t UgainA   = 0x61, IgainA   = 0x62, UoffsetA = 0x63, IoffsetA = 0x64;
static constexpr uint16_t UgainB   = 0x65, IgainB   = 0x66, UoffsetB = 0x67, IoffsetB = 0x68;
static constexpr uint16_t UgainC   = 0x69, IgainC   = 0x6A, UoffsetC = 0x6B, IoffsetC = 0x6C;

// Energies
static constexpr uint16_t APenergyT = 0x80;
static constexpr uint16_t APenergyA = 0x81;
static constexpr uint16_t APenergyB = 0x82;
static constexpr uint16_t APenergyC = 0x83;
static constexpr uint16_t ANenergyT = 0x84;
static constexpr uint16_t ANenergyA = 0x85;
static constexpr uint16_t ANenergyB = 0x86;
static constexpr uint16_t ANenergyC = 0x87;
static constexpr uint16_t RPenergyT = 0x88;
static constexpr uint16_t RPenergyA = 0x89;
static constexpr uint16_t RPenergyB = 0x8A;
static constexpr uint16_t RPenergyC = 0x8B;
static constexpr uint16_t RNenergyT = 0x8C;
static constexpr uint16_t RNenergyA = 0x8D;
static constexpr uint16_t RNenergyB = 0x8E;
static constexpr uint16_t RNenergyC = 0x8F;
static constexpr uint16_t SAenergyT = 0x90;
static constexpr uint16_t SAenergyA = 0x91;
static constexpr uint16_t SAenergyB = 0x92;
static constexpr uint16_t SAenergyC = 0x93;

// Live
static constexpr uint16_t PFmeanT  = 0xBC;
static constexpr uint16_t PFmeanA  = 0xBD;
static constexpr uint16_t PFmeanB  = 0xBE;
static constexpr uint16_t PFmeanC  = 0xBF;
static constexpr uint16_t PAngleA  = 0xF9;
static constexpr uint16_t PAngleB  = 0xFA;
static constexpr uint16_t PAngleC  = 0xFB;
static constexpr uint16_t Freq     = 0xF8;
static constexpr uint16_t Temp     = 0xFC;

// RMS
static constexpr uint16_t UrmsA     = 0xD9, UrmsB     = 0xDA, UrmsC     = 0xDB;
static constexpr uint16_t IrmsA     = 0xDD, IrmsB     = 0xDE, IrmsC     = 0xDF;
static constexpr uint16_t UrmsALSB  = 0xE9, UrmsBLSB  = 0xEA, UrmsCLSB  = 0xEB;
static constexpr uint16_t IrmsALSB  = 0xED, IrmsBLSB  = 0xEE, IrmsCLSB  = 0xEF;
static constexpr uint16_t IrmsN     = 0xDC;

static constexpr uint16_t PmeanAF   = 0xD1, PmeanBF   = 0xD2, PmeanCF   = 0xD3;
static constexpr uint16_t PmeanAH   = 0xD5, PmeanBH   = 0xD6, PmeanCH   = 0xD7;
static constexpr uint16_t PmeanAFLSB= 0xE1, PmeanBFLSB= 0xE2, PmeanCFLSB= 0xE3;
static constexpr uint16_t PmeanAHLSB= 0xE5, PmeanBHLSB= 0xE6, PmeanCHLSB= 0xE7;

static constexpr uint16_t UPeakA    = 0xF1, UPeakB    = 0xF2, UPeakC    = 0xF3;
static constexpr uint16_t IPeakA    = 0xF5, IPeakB    = 0xF6, IPeakC    = 0xF7;

static constexpr uint16_t PmeanT    = 0xB0, PmeanA    = 0xB1, PmeanB    = 0xB2, PmeanC    = 0xB3;
static constexpr uint16_t QmeanT    = 0xB4, QmeanA    = 0xB5, QmeanB    = 0xB6, QmeanC    = 0xB7;
static constexpr uint16_t SmeanT    = 0xB8, SmeanA    = 0xB9, SmeanB    = 0xBA, SmeanC    = 0xBB;
static constexpr uint16_t PmeanTLSB = 0xC0, PmeanALSB = 0xC1, PmeanBLSB = 0xC2, PmeanCLSB = 0xC3;
static constexpr uint16_t QmeanTLSB = 0xC4, QmeanALSB = 0xC5, QmeanBLSB = 0xC6, QmeanCLSB = 0xC7;
static constexpr uint16_t SAmeanTLSB= 0xC8, SmeanALSB = 0xC9, SmeanBLSB = 0xCA, SmeanCLSB = 0xCB;
static constexpr double   POWER_LSB = 0.00032;

// ---- Low-level SPI ----
uint16_t ATM90E32::xfer(uint8_t rw, uint16_t reg, uint16_t val) {
  const uint16_t data_swapped = (uint16_t)((val >> 8) | (val << 8));
  const uint16_t addr = reg | (rw ? 0x8000 : 0x0000);
  const uint16_t addr_swapped = (uint16_t)((addr >> 8) | (addr << 8));

  SPISettings settings(spiHz_, MSBFIRST, spiMode_);
  spi_.beginTransaction(settings);
  csAssert();
  delayMicroseconds(10);

  const uint8_t *pa = reinterpret_cast<const uint8_t*>(&addr_swapped);
  spi_.transfer(pa[0]);
  spi_.transfer(pa[1]);

  delayMicroseconds(4);

  uint16_t out = 0;
  if (rw) {
    uint8_t b0 = spi_.transfer(0x00);
    uint8_t b1 = spi_.transfer(0x00);
    const uint16_t raw = (uint16_t)(b0 | (uint16_t(b1) << 8));
    out = (uint16_t)((raw >> 8) | (raw << 8));
  } else {
    const uint8_t *pd = reinterpret_cast<const uint8_t*>(&data_swapped);
    spi_.transfer(pd[0]);
    spi_.transfer(pd[1]);
    out = val;
  }

  csRelease();
  delayMicroseconds(10);
  spi_.endTransaction();
  return out;
}

double ATM90E32::readRmsLike(uint16_t regH, uint16_t regLSB, double sH, double sLb){
  const uint16_t h = read16(regH);
  const uint16_t l = read16(regLSB);
  return (h * sH) + (((l >> 8) & 0xFF) * sLb);
}

int32_t ATM90E32::read32Signed(uint16_t regH, uint16_t regL) {
  const int16_t  h = (int16_t)read16(regH);
  const uint16_t l = read16(regL);
  return (int32_t)(((uint32_t)(uint16_t)h << 16) | l);
}

int32_t ATM90E32::readMeanPowerW(uint16_t regH, uint16_t regL) {
  return (int32_t)lround((double)read32Signed(regH, regL) * POWER_LSB);
}

int32_t ATM90E32::readPmeanW(uint8_t phase) {
  static const uint16_t regH[] = {PmeanA, PmeanB, PmeanC, PmeanT};
  static const uint16_t regL[] = {PmeanALSB, PmeanBLSB, PmeanCLSB, PmeanTLSB};
  if (phase > 3) return 0;
  return readMeanPowerW(regH[phase], regL[phase]);
}

int32_t ATM90E32::readQmean_var(uint8_t phase) {
  static const uint16_t regH[] = {QmeanA, QmeanB, QmeanC, QmeanT};
  static const uint16_t regL[] = {QmeanALSB, QmeanBLSB, QmeanCLSB, QmeanTLSB};
  if (phase > 3) return 0;
  return readMeanPowerW(regH[phase], regL[phase]);
}

int32_t ATM90E32::readSmean_VA(uint8_t phase) {
  static const uint16_t regH[] = {SmeanA, SmeanB, SmeanC, SmeanT};
  static const uint16_t regL[] = {SmeanALSB, SmeanBLSB, SmeanCLSB, SAmeanTLSB};
  if (phase > 3) return 0;
  return readMeanPowerW(regH[phase], regL[phase]);
}

double ATM90E32::readUPeak_V(uint8_t phase) {
  static const uint16_t reg[] = {UPeakA, UPeakB, UPeakC};
  if (phase > 2) return 0.0;
  return (double)read16(reg[phase]) * 0.01;
}

double ATM90E32::readIPeak_A(uint8_t phase) {
  static const uint16_t reg[] = {IPeakA, IPeakB, IPeakC};
  if (phase > 2) return 0.0;
  return (double)read16(reg[phase]) * 0.001;
}

double ATM90E32::readIrmsN_A() {
  return (double)read16(IrmsN) * 0.001;
}

int32_t ATM90E32::readPmeanFundW(uint8_t phase) {
  static const uint16_t regH[] = {PmeanAF, PmeanBF, PmeanCF};
  static const uint16_t regL[] = {PmeanAFLSB, PmeanBFLSB, PmeanCFLSB};
  if (phase > 2) return 0;
  return readMeanPowerW(regH[phase], regL[phase]);
}

int32_t ATM90E32::readPmeanHarmW(uint8_t phase) {
  static const uint16_t regH[] = {PmeanAH, PmeanBH, PmeanCH};
  static const uint16_t regL[] = {PmeanAHLSB, PmeanBHLSB, PmeanCHLSB};
  if (phase > 2) return 0;
  return readMeanPowerW(regH[phase], regL[phase]);
}

uint16_t ATM90E32::readThdPct_x100(uint8_t phase) {
  if (phase > 2) return 0;
  const int32_t pf = readPmeanFundW(phase);
  const int32_t ph = readPmeanHarmW(phase);
  const double denom = fmax(fabs((double)pf), 1.0);
  const double pct = 100.0 * fabs((double)ph) / denom;
  long x = lround(pct * 100.0);
  if (x < 0) x = 0;
  if (x > 65535) x = 65535;
  return (uint16_t)x;
}

static uint16_t buildChannelMapReg(const uint8_t phaseMap[3]) {
  return (uint16_t)(phaseMap[0] | (uint16_t(phaseMap[1]) << 3) | (uint16_t(phaseMap[2]) << 6));
}

// ---- Public API ----
void ATM90E32::begin(uint16_t lineHz, uint8_t sumAbs, uint8_t wireMode, const uint8_t phaseMap[3],
                     uint16_t ucal, const M90PhaseCal cal[3]) {
  lineHz_ = (lineHz == 60) ? 60 : 50;
  sumAbs_ = sumAbs ? 1 : 0;
  wireMode_ = wireMode ? 1 : 0;
  for (int i = 0; i < 3; i++) phaseMap_[i] = phaseMap[i];
  ucal_   = ucal;

  // PM pins are MCU->ATM control pins in your schematic
  pinMode(pm0_, OUTPUT);
  pinMode(pm1_, OUTPUT);
  digitalWrite(pm0_, HIGH);
  digitalWrite(pm1_, HIGH);
  delay(5);

  pinMode(cs_, OUTPUT);
  csRelease();
  delay(5);

  write16(SoftReset, 0x789A);
  delay(5);

  write16(CfgRegAccEn, 0x55AA);
  write16(MeterEn, 0x0001);

  // Sag/freq thresholds
  uint16_t sagV, FreqHiThresh, FreqLoThresh;
  if (lineHz_ == 60){ sagV=90;  FreqHiThresh=6100; FreqLoThresh=5900; }
  else              { sagV=190; FreqHiThresh=5100; FreqLoThresh=4900; }

  const double ucalRatio = ucal_ / 32768.0;
  const uint16_t vSagTh = (uint16_t)((sagV * 100.0 * sqrt(2.0)) / (2.0 * ucalRatio));

  write16(SagPeakDetCfg, 0x143F);
  write16(SagTh,        vSagTh);
  write16(FreqHiTh,     FreqHiThresh);
  write16(FreqLoTh,     FreqLoThresh);

  write16(EMMIntEn0, 0xB76F);
  write16(EMMIntEn1, 0xDDFD);
  write16(EMMIntState0, 0x0001);
  write16(EMMIntState1, 0x0001);

  write16(ZXConfig, 0xD654);

  const uint16_t chMap = buildChannelMapReg(phaseMap_);
  write16(ChannelMapU, chMap);
  write16(ChannelMapI, chMap);

  // Modes: 0x0087 = 3P4W (EnPA/B/C); 3P3W sets bit8 and clears EnPB (bit1)
  uint16_t m0 = 0x0087;
  if (lineHz_ == 60) m0 |= (1u << 12);
  if (wireMode_) {
    m0 |= (1u << 8);
    m0 &= static_cast<uint16_t>(~(1u << 1));
  }
  m0 &= static_cast<uint16_t>(~(0b11u << 3));
  m0 |= static_cast<uint16_t>((sumAbs_ ? 0b11u : 0b00u) << 3);

  auto gainCode = [](uint8_t g)->uint8_t{ if(g==1)return 0; if(g==2)return 1; if(g==4)return 2; return 1; };
  const uint8_t gIA=2,gIB=2,gIC=2;  // PGA=2× for ZEMCTK05 (167 mV @ 50A)
  uint8_t m1=0; m1|=(gainCode(gIA)<<0); m1|=(gainCode(gIB)<<2); m1|=(gainCode(gIC)<<4);

  write16(PLconstH, 0x0861);
  write16(PLconstL, 0xC468);

  write16(MMode0, m0);
  write16(MMode1, m1);

  write16(PStartTh, 0x1D4C);
  write16(QStartTh, 0x1D4C);
  write16(SStartTh, 0x1D4C);
  write16(PPhaseTh, 0x02EE);
  write16(QPhaseTh, 0x02EE);
  write16(SPhaseTh, 0x02EE);

  applyCalibration(cal);

  write16(CfgRegAccEn, 0x0000);
}

void ATM90E32::applyCalibration(const M90PhaseCal cal[3]) {
  write16(CfgRegAccEn, 0x55AA);

  write16(UgainA,   cal[0].Ugain);   write16(IgainA,   cal[0].Igain);
  write16(UoffsetA, (uint16_t)cal[0].Uoffset); write16(IoffsetA, (uint16_t)cal[0].Ioffset);

  write16(UgainB,   cal[1].Ugain);   write16(IgainB,   cal[1].Igain);
  write16(UoffsetB, (uint16_t)cal[1].Uoffset); write16(IoffsetB, (uint16_t)cal[1].Ioffset);

  write16(UgainC,   cal[2].Ugain);   write16(IgainC,   cal[2].Igain);
  write16(UoffsetC, (uint16_t)cal[2].Uoffset); write16(IoffsetC, (uint16_t)cal[2].Ioffset);

  write16(CfgRegAccEn, 0x0000);
}

void ATM90E32::readPhaseCal(M90PhaseCal out[3]) {
  write16(CfgRegAccEn, 0x55AA);
  out[0].Ugain   = read16(UgainA);
  out[0].Igain   = read16(IgainA);
  out[0].Uoffset = (int16_t)read16(UoffsetA);
  out[0].Ioffset = (int16_t)read16(IoffsetA);
  out[1].Ugain   = read16(UgainB);
  out[1].Igain   = read16(IgainB);
  out[1].Uoffset = (int16_t)read16(UoffsetB);
  out[1].Ioffset = (int16_t)read16(IoffsetB);
  out[2].Ugain   = read16(UgainC);
  out[2].Igain   = read16(IgainC);
  out[2].Uoffset = (int16_t)read16(UoffsetC);
  out[2].Ioffset = (int16_t)read16(IoffsetC);
  write16(CfgRegAccEn, 0x0000);
}

// RMS helpers
double ATM90E32::readUrmsA_V(){ return readRmsLike(UrmsA, UrmsALSB, 0.01, 0.01/256.0); }
double ATM90E32::readUrmsB_V(){ return readRmsLike(UrmsB, UrmsBLSB, 0.01, 0.01/256.0); }
double ATM90E32::readUrmsC_V(){ return readRmsLike(UrmsC, UrmsCLSB, 0.01, 0.01/256.0); }

double ATM90E32::readIrmsA_A(){ return readRmsLike(IrmsA, IrmsALSB, 0.001, 0.001/256.0); }
double ATM90E32::readIrmsB_A(){ return readRmsLike(IrmsB, IrmsBLSB, 0.001, 0.001/256.0); }
double ATM90E32::readIrmsC_A(){ return readRmsLike(IrmsC, IrmsCLSB, 0.001, 0.001/256.0); }

// PF / Angles / Freq / Temp
int16_t  ATM90E32::readPFmeanA(){ return (int16_t)read16(PFmeanA); }
int16_t  ATM90E32::readPFmeanB(){ return (int16_t)read16(PFmeanB); }
int16_t  ATM90E32::readPFmeanC(){ return (int16_t)read16(PFmeanC); }
int16_t  ATM90E32::readPFmeanT(){ return (int16_t)read16(PFmeanT); }
int16_t  ATM90E32::readPAngleA(){ return (int16_t)read16(PAngleA); }
int16_t  ATM90E32::readPAngleB(){ return (int16_t)read16(PAngleB); }
int16_t  ATM90E32::readPAngleC(){ return (int16_t)read16(PAngleC); }
uint16_t ATM90E32::readFreq_x100(){ return read16(Freq); }
int16_t  ATM90E32::readTempC(){ return (int16_t)read16(Temp); }

// Energies
uint16_t ATM90E32::rdAP_A(){ return read16(APenergyA); }
uint16_t ATM90E32::rdAP_B(){ return read16(APenergyB); }
uint16_t ATM90E32::rdAP_C(){ return read16(APenergyC); }
uint16_t ATM90E32::rdAP_T(){ return read16(APenergyT); }

uint16_t ATM90E32::rdAN_A(){ return read16(ANenergyA); }
uint16_t ATM90E32::rdAN_B(){ return read16(ANenergyB); }
uint16_t ATM90E32::rdAN_C(){ return read16(ANenergyC); }
uint16_t ATM90E32::rdAN_T(){ return read16(ANenergyT); }

uint16_t ATM90E32::rdRP_A(){ return read16(RPenergyA); }
uint16_t ATM90E32::rdRP_B(){ return read16(RPenergyB); }
uint16_t ATM90E32::rdRP_C(){ return read16(RPenergyC); }
uint16_t ATM90E32::rdRP_T(){ return read16(RPenergyT); }

uint16_t ATM90E32::rdRN_A(){ return read16(RNenergyA); }
uint16_t ATM90E32::rdRN_B(){ return read16(RNenergyB); }
uint16_t ATM90E32::rdRN_C(){ return read16(RNenergyC); }
uint16_t ATM90E32::rdRN_T(){ return read16(RNenergyT); }

uint16_t ATM90E32::rdSA_A(){ return read16(SAenergyA); }
uint16_t ATM90E32::rdSA_B(){ return read16(SAenergyB); }
uint16_t ATM90E32::rdSA_C(){ return read16(SAenergyC); }
uint16_t ATM90E32::rdSA_T(){ return read16(SAenergyT); }

M90DiagRegs ATM90E32::readDiag() {
  M90DiagRegs d{};
  d.EMMState0    = read16(EMMState0);
  d.EMMState1    = read16(EMMState1);
  d.EMMIntState0 = read16(EMMIntState0);
  d.EMMIntState1 = read16(EMMIntState1);
  d.CRCErrStatus = read16(CRCErrStatus);
  d.LastSPIData  = read16(LastSPIData);
  return d;
}

void decodeM90ChipEv(const M90DiagRegs& d, M90ChipEv& out) {
  const uint16_t s0 = d.EMMState0;
  const uint16_t s1 = d.EMMState1;
  out.ov[0]        = (s0 & 0x1000) != 0;
  out.ov[1]        = (s0 & 0x0800) != 0;
  out.ov[2]        = (s0 & 0x0400) != 0;
  out.overI[0]     = (s0 & 0x8000) != 0;
  out.overI[1]     = (s0 & 0x4000) != 0;
  out.overI[2]     = (s0 & 0x2000) != 0;
  out.revPhase     = (s0 & 0x0200) != 0;
  out.sag[0]       = (s1 & 0x4000) != 0;
  out.sag[1]       = (s1 & 0x2000) != 0;
  out.sag[2]       = (s1 & 0x1000) != 0;
  out.phaseLoss[0] = (s1 & 0x0400) != 0;
  out.phaseLoss[1] = (s1 & 0x0200) != 0;
  out.phaseLoss[2] = (s1 & 0x0100) != 0;
  out.freqHi       = (s1 & 0x8000) != 0;
  out.freqLo       = (s1 & 0x0800) != 0;
}
