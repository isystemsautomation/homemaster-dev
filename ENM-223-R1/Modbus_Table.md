# ENM-223-R1 Modbus Register Table

Firmware **0.2.0**, Modbus map **v2** (`CFG_VERSION` **0x0023**).  
Chip: **ATM90E32AS** (Atmel-46003B). Legacy addresses **0–99** and coils/DI layout are **unchanged** from map v1.

## Function Codes

| FC | Access |
|----|--------|
| **FC01** | Read coils |
| **FC02** | Read discrete inputs |
| **FC03** | Read holding registers |
| **FC04** | Read input registers |
| **FC05** | Write single coil |
| **FC06** | Write single holding register |
| **FC16** | Write multiple holding registers |

Default slave address **30**, baud **19200 8N1** (configurable via WebConfig or HR0/HR1–2).

---

## Input Registers (FC04) — Real-Time Telemetry

### RMS, PF, Peaks (U16)

| Address | Name | Type | Unit | Scale | ATM90 / notes |
|---------|------|------|------|-------|---------------|
| 0–2 | Urms L1/L2/L3 | U16 | V | ×0.01 | UrmsA/B/C |
| 3–5 | Irms L1/L2/L3 | U16 | A | ×0.001 | IrmsA/B/C |
| 6 | Line frequency | U16 | Hz | ×0.01 | |
| 7 | Temperature | S16 | °C | 1 | internal |
| 8–11 | Power factor L1/L2/L3/Total | S16 | — | ×0.001 | |
| 12–14 | **Upeak** L1/L2/L3 | U16 | V | ×0.01 | **UPeakA/B/C** (0xF1–0xF3); `UPeak[V]=reg×Ugain/(2¹³×100)` |
| 15–17 | **Ipeak** L1/L2/L3 | U16 | A | ×0.001 | **IPeakA/B/C** (0xF5–0xF7); `IPeak[A]=reg×Igain/(2¹³×1000)` |
| 18 | Irms neutral | U16 | A | ×0.001 | IrmsN |
| 19 | *reserved* | — | — | — | free |
| 47–49 | **THD** L1/L2/L3 | U16 | % | ×0.01 | **Computed** active-power THD: `100×|P_harm|/|P_fund|` — **not** chip THD+N (ATM90E32AS has **none**) |

### Signed Power (S32 — 2 registers, high word first)

| Address (base) | Name | Type | Unit | ATM90 register (Table-14, LSB 0.00032 W) |
|----------------|------|------|------|-------------------------------------------|
| 20, 22, 24, 26 | Active power L1/L2/L3/Total | S32 | W | PmeanA/B/C/T + LSB |
| 28, 30, 32, 34 | Reactive power L1/L2/L3/Total | S32 | var | QmeanA/B/C/T + LSB |
| 36, 38, 40, 42 | Apparent power L1/L2/L3/Total | S32 | VA | SmeanA/B/C/T + LSB |
| 44–46 | Phase angle L1/L2/L3 | S16 | ° | ×0.1 | |
| **105, 107, 109, 111** | **Fundamental active power** L1/L2/L3/Total | S32 | W | **PmeanAF/BF/CF/TF** (D1–D3/D0 + E1–E3/E0) |
| **113, 115, 117, 119** | **Harmonic active power** L1/L2/L3/Total | S32 | W | **PmeanAH/BH/CH/TH** (D5–D7/D4 + E5–E7/E4) |

### Status & Chip Events (U16)

| Address | Name | Description |
|---------|------|-------------|
| 50–59 | *free* | — |
| 100 | Status flags | bit1=linkOk, bit3=cfgDirty |
| 101 | Chip PQ events L1 | U16 bitfield |
| 102 | Chip PQ events L2 | U16 bitfield |
| 103 | Chip PQ events L3 | U16 bitfield |
| 104 | Chip PQ events Total | U16 bitfield |

---

## Input Registers (FC04) — Energy (U32, high word first)

Chip energy registers are **read-clears** (0.01 CF). The MCU accumulates ticks and publishes Wh/varh/VAh.

| Address (base) | Name | Type | Unit | ATM90 (Table-12) |
|----------------|------|------|------|------------------|
| 60, 62, 64, 66 | Active import (AP) | U32 | Wh | APenergyA/B/C/T |
| 68, 70, 72, 74 | Active export (AN) | U32 | Wh | ANenergyA/B/C/T |
| 76, 78, 80, 82 | Reactive import (RP) | U32 | varh | RPenergyA/B/C/T |
| 84, 86, 88, 90 | Reactive export (RN) | U32 | varh | RNenergyA/B/C/T |
| 92, 94, 96, 98 | Apparent (SA) | U32 | VAh | SAenergyA/B/C/T |
| **121, 123, 125, 127** | **Harmonic active import** | U32 | Wh | **APenergyTH/AH/BH/CH** (0xA8–0xAB) |

---

## Coils (FC01/FC05)

| Address | Name | Description |
|---------|------|-------------|
| 0 | Relay 1 | Maintained ON/OFF (Modbus Controlled mode) |
| 1 | Relay 2 | Maintained ON/OFF |
| 16–19 | Alarm ACK L1/L2/L3/Total | Write `1`; auto-clears |

---

## Discrete Inputs (FC02)

| Address | Name | Description |
|---------|------|-------------|
| 0–3 | LED 1–4 | Physical LED state |
| 4–7 | Button 1–4 | Pressed |
| 8–9 | Relay 1–2 | Logical state (after mode/invert) |
| 16–27 | Alarm flags | `addr = 16 + channel×3 + kind` (kind: 0=Alarm, 1=Warning, 2=Event) |

---

## Holding Registers (FC03) — Writable Settings

| Address | Type | Description |
|---------|------|-------------|
| 0 | U16 | Modbus slave address (1–247) |
| 1–2 | U32 | Baud rate |
| 4 | U16 | Line frequency 50/60 Hz |
| 5 | U16 | Sum mode 0=algebraic, 1=absolute |
| 7–8 | U16 | Relay 1/2 enable at boot |

Phase mapping and 3P4W/3P3W are **WebConfig-only**.

---

## ATM90E32AS Harmonics — Important Limits

Per **Atmel-46003B**:

- **No per-order harmonic spectrum** and **no THD+N registers**.
- Addresses **0xF1–0xF3** = voltage peak, **0xF5–0xF7** = current peak (not THD; that mapping applies to ATM90E36 only).
- Harmonic content is available only as **fundamental/harmonic active power** (Table-14) and **harmonic active import energy** (Table-12).
- Datasheet **±5 % accuracy** on total harmonics (p.75).
- IR **47–49** in this firmware = **computed** active-power THD from `P_fund` / `P_harm`, unchanged from map v1.

---

## Scaling Reference

| Quantity | Formula / scale |
|----------|-----------------|
| P_fund, P_harm | `signed32 × 0.00032 W` (rounded to integer W in Modbus) |
| Ipeak | `IPeakReg × Igain / (8192 × 1000)` A → Modbus U16 ×0.001 |
| Upeak | `UPeakReg × Ugain / (8192 × 100)` V → Modbus U16 ×0.01 |
| Harmonic energy | MCU sums 0.01 CF read-clear ticks → Wh via `MC_imp_per_kWh` |

Igain/Ugain are the values written to ATM registers **0x62/0x66/0x6A** and **0x61/0x65/0x69** during calibration.

---

## Example Reads

**Fundamental power L1 (W):** FC04, addresses **105–106**, S32, high word at 105.

**Harmonic active energy Total (Wh):** FC04, addresses **127–128**, U32.

**Current peak L2 (A):** FC04, address **16**, U16, ×0.001.
