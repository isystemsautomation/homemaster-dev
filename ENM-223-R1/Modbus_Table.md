# ENM-223-R1 — Modbus Register Map (v0.2.0)

**Identity:** Model ID `2` · Firmware `0.2.0` · `CFG_VERSION 0x0025` · `METER_VERSION 0x0003`
**Transport:** Modbus RTU slave, default address **3**, default baud **19200** (8N1).

## What changed vs v0.1.0
- **Contiguous Input-Register map, addresses `0..85` (86 registers).** The whole
  telemetry set is read in **one FC04 request** (86 ≤ the 125-register Modbus limit).
- **No Discrete Inputs (FC02).** All digital state is bit-packed into Input Registers.
- **Currents are signed 32-bit** (`S_DWORD`, ×0.001 A, primary side) — full CT range.
- **Energy is accumulated in primary Wh/varh** (32-bit), decoupled from CT ratio and
  calibration; reset only by command (WebConfig) or the module button, never by a
  settings change.
- **Command coils are write-only** (relays, Identify, ACK); relay state is read back
  from the IO-mask input register, not from the coil.

## Function codes

| FC | Use |
|----|-----|
| **FC04** | Read Input Registers `0..85` — all telemetry (one sweep) |
| **FC01 / FC05** | Read / write Coils — relay command, Identify, alarm ACK |
| **FC03 / FC06 / FC16** | Read / write Holding Registers — address, baud, line frequency, sum-abs, relay enable |
| FC02 | *Not used* — no discrete inputs |

Word order for 32-bit values is **high word first** (ESPHome `S_DWORD` / `U_DWORD`).

---

## Input Registers (FC04)

### Bit-packed status (U16 bitmask)

| Addr | Register | Bits |
|------|----------|------|
| 0 | **I/O mask** | b0–b3 = LED1–LED4, b4–b7 = BTN1–BTN4, b8 = Relay 1 state, b9 = Relay 2 state |
| 1 | **Status flags** | b1 = Link OK, b3 = Config dirty |
| 2 | **Alarm flags** | bit = `3·ch + kind`, ch 0–3 = L1/L2/L3/Total, kind 0–2 = Alarm/Warning/Event (b0 = L1 Alarm … b11 = Total Event) |
| 3 | **Chip events L1** | b0 = Sag, b1 = Over-voltage, b2 = Phase loss, b3 = Over-current |
| 4 | **Chip events L2** | same bits as reg 3 |
| 5 | **Chip events L3** | same bits as reg 3 |
| 6 | **Chip events Total** | b0 = Sag, b1 = Over-voltage, b2 = Phase loss, b3 = Over-current, b4 = Frequency, b5 = Reverse phase sequence |

### Instantaneous scalars

| Addr | Value | Type | Unit | Scale |
|------|-------|------|------|-------|
| 7–9 | Urms L1 / L2 / L3 | U16 | V | ×0.01 |
| 10 | Line frequency | U16 | Hz | ×0.01 |
| 11 | Temperature | S16 | °C | ×1 |
| 12–15 | Power factor L1 / L2 / L3 / Total | S16 | — | ×0.001 |
| 16–18 | Phase angle L1 / L2 / L3 | S16 | ° | ×0.1 |
| 19–21 | THD L1 / L2 / L3 | U16 | % | ×0.01 |

### Currents (S32, high word first)

| Addr | Value | Type | Unit | Scale |
|------|-------|------|------|-------|
| 22–23 | Irms L1 | S32 | A | ×0.001 |
| 24–25 | Irms L2 | S32 | A | ×0.001 |
| 26–27 | Irms L3 | S32 | A | ×0.001 |
| 28–29 | Irms Neutral | S32 | A | ×0.001 |

### Power (S32, high word first)

| Addr | Value | Type | Unit |
|------|-------|------|------|
| 30 / 32 / 34 / 36 | Active power P — L1 / L2 / L3 / Total | S32 | W |
| 38 / 40 / 42 / 44 | Reactive power Q — L1 / L2 / L3 / Total | S32 | var |
| 46 / 48 / 50 / 52 | Apparent power S — L1 / L2 / L3 / Total | S32 | VA |

### Energy (U32, high word first, primary Wh/varh)

Read-and-hold accumulators (not read-clear on the Modbus side). Reset only via the
`reset_energy` command (WebConfig) or the module button.

| Addr | Value | Type | Unit |
|------|-------|------|------|
| 54 / 56 / 58 / 60 | Active energy **import** — L1 / L2 / L3 / Total | U32 | Wh |
| 62 / 64 / 66 / 68 | Active energy **export** — L1 / L2 / L3 / Total | U32 | Wh |
| 70 / 72 / 74 / 76 | Reactive energy **import** — L1 / L2 / L3 / Total | U32 | varh |
| 78 / 80 / 82 / 84 | Reactive energy **export** — L1 / L2 / L3 / Total | U32 | varh |

---

## Coils (FC01 read / FC05 write) — write-only commands

| Addr | Coil | Notes |
|------|------|-------|
| 0 | Relay 1 command | write ON/OFF; **state is read from IR 0 bit 8** (works in Modbus mode; in Alarm/None mode the relay is driven by internal logic and a written value is overridden) |
| 1 | Relay 2 command | state from IR 0 bit 9 |
| 5 | Identify | momentary — blinks the LEDs ~5 s |
| 16 | ACK L1 | momentary — clears the latched alarm on channel L1 |
| 17 | ACK L2 | momentary |
| 18 | ACK L3 | momentary |
| 19 | ACK Total | momentary |

> Reboot / save-config / energy-reset are **not** exposed over Modbus (shared-bus
> safety). Config autosaves; reboot / energy-reset / factory are available via WebConfig.

## Holding Registers (FC03 read / FC06·FC16 write) — configuration

| Addr | Field | Range |
|------|-------|-------|
| 0 | Modbus address | 1–247 |
| 1–2 | Modbus baud (U32, high word first) | 9600 / 19200 / 38400 / 57600 / 115200 |
| 4 | Line frequency | 50 / 60 |
| 5 | Sum-abs mode | 0 / 1 |
| 7 | Relay 1 enable | 0 / 1 |
| 8 | Relay 2 enable | 0 / 1 |

(HR 3 and 6 are reserved.)

---

## Notes

- **One-sweep read:** a single FC04 of `0..85` returns all telemetry. Fast changing
  values (U/I/P/Q/S) are at the start; energy counters at the end may be polled less
  often by the master.
- **Primary-side values:** currents, power and energy are scaled to the primary side
  by the CT ratio *N = primary A / secondary mA* configured in WebConfig.
- **CT-ratio change** does not rewrite accumulated energy (past totals stay valid); it
  only affects future accumulation.
- **Word order:** every 32-bit register is high-word-first.
- **No discrete inputs:** the module has no digital inputs; alarm/chip-event state is
  reported through the bit-packed input registers above.
