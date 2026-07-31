![Modbus](https://img.shields.io/badge/Protocol-Modbus%20RTU-brightgreen)
![License](https://img.shields.io/badge/License-MIT%20%2F%20CERN--OHL--W-blue)

# AIO-422-R1 — Analog I/O & RTD Interface Module

**HOMEMASTER – Modular control. Custom logic.**

![MODULE photo](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/AIO-422-R1/Images/photo1.png)

**Document map:** [§1 Overview](#1-overview) · [§2 Features](#2-features) · [§3 Specifications](#3-specifications) · [§4 Hardware](#4-hardware--interface) · [§5 Getting Started](#5-getting-started) · [§6 WebConfig](#6-webconfig-reference) · [§7 Modbus map](#7-modbus-register-map) · [§8 ESPHome](#8-esphome--home-assistant-integration) · [§9 Programming](#9-programming--build) · [§11 Downloads](#11-downloads--resources)

---

## 1. Overview

The **AIO-422-R1** puts **analog in, analog out, and RTD** in one DIN-rail Modbus module: **4× 0–10 V inputs (16-bit)**, **2× 0–10 V outputs**, and **2× PT100/PT1000** for HVAC and process sensing/actuation. It connects to **MiniPLC/MicroPLC** (or any Modbus RTU master) over **RS-485** and integrates with **ESPHome / Home Assistant** via the controller packages.

**Key capabilities at a glance:**

- **4× AI 0–10 V (16-bit ADS1115)** — field voltage in mV; voltage only (no 4–20 mA)
- **2× AO 0–10 V (12-bit MCP4725)** — raw 0–4095 over Modbus; power-on policy in WebConfig
- **2× RTD (MAX31865)** — PT100/PT1000, 2-/3-/4-wire; Rref follows sensor type (PT100→400 Ω, PT1000→4000 Ω)
- **4 front buttons + 4 user LEDs** — configurable actions and LED sources
- **Driverless WebConfig** — USB-C + Chromium-based browser; settings in LittleFS
- **One FC04 poll** for live state in the ESPHome package (STD-009)

### How local and remote control coexist

Analog outputs and LED/button behaviour can be driven from **WebConfig**, **front buttons**, or **Modbus / Home Assistant** at the same time — the last write wins. RTD and AI values are always measured on the module; a controller only reads them.

## Key advantages

- **4× AI 0–10 V (16-bit) + 2× AO 0–10 V + 2× RTD (PT100/PT1000)** in one DIN module — analog sensing and actuation without stacking separate boards.
- Native ESPHome API via the MiniPLC/MicroPLC controller — no MQTT broker, no manual Modbus register mapping for the package entities.
- Local-first / edge-resilient — onboard logic keeps working if the network or Home Assistant is down.
- Open hardware (**CERN-OHL-W v2**) and firmware (**MIT**) — repairable, reproducible, no vendor lock-in.
- Standard **RS-485 Modbus RTU** — works with any Modbus master or industrial HMI/SCADA system, not locked to HomeMaster.
- Driverless **USB-C WebConfig** (Chrome, Edge, Opera); configuration persists in on-device flash (**LittleFS**).

---

## 2. Features

| Area | Detail |
|------|--------|
| **Identity** | Firmware **0.2.0**; `HM_MAP_VERSION` = `0x0020` (derived from major/minor/patch — not a standalone map number) |
| **Analog inputs** | 4× 0–10 V, 16-bit; published as mV (U_WORD). **AI3/AI4 terminal mapping corrected vs v0.1.0** |
| **Analog outputs** | 2× 0–10 V, 12-bit; command on HREG 200/201 (clamped 0–4095); state mirrored on IREG 10/11 |
| **RTD** | 2× MAX31865; PT100/PT1000; 2/3/4-wire; Rref **derived** from sensor type (not free-choice) |
| **Buttons** | 10 actions: flag toggle; AO1/AO2 on/off; AO1/AO2 on/off restore last level; AO1/AO2 ±10 %; all AO off |
| **User LEDs** | 11 sources: Manual; AO1/AO2 at 0 % / 100 %; Bus link OK; RTD1/RTD2 fault; AO1/AO2 active; Hardware fault |
| **WebConfig** | USB-C → Chromium browser; Modbus, AI/AO/RTD, buttons & LEDs; auto-save |
| **Modbus RTU slave** | Contiguous IREG 0..11 for live state + legacy ISTS/HREG retained |
| **ESPHome / HA** | Package `Firmware/v0.2.0/default_aio_422_r1_plc/`; one FC04 @0 count=12 per second |
| **Extras** | Identify / Factory reset / Reboot; Link OK & Config dirty flags; fault mask bits |

### Applications

- 0–10 V dimmable lighting and HVAC dampers/valves
- Analog pressure / humidity / process sensors (0–10 V)
- PT100/PT1000 temperature points with fault indication
- Local button control of analog outputs without a controller

---

## 3. Specifications

### 3.1 I/O summary

| Subsystem | Qty | Description |
|-----------|-----|-------------|
| Analog Inputs | 4 | 0–10 V, ADS1115 16-bit; **voltage only — no 4–20 mA** |
| Analog Outputs | 2 | 0–10 V, MCP4725 12-bit; **voltage only — no 4–20 mA** |
| RTD Inputs | 2 | PT100/PT1000, MAX31865; 2-/3-/4-wire |
| User LEDs | 4 | Configurable sources (see Features) |
| Buttons | 4 | Configurable actions (see Features) |
| Status LEDs | 3 | Power, RX, TX |
| Modbus RTU | Yes | RS-485 (address 1–247, 9600–115200 baud) |
| USB-C | Yes | WebConfig / diagnostics / UF2 flash |
| Power | 24 V DC | DIN-rail module supply |
| MCU | RP2350 | Dual-core, LittleFS, USB, UART |

### 3.2 Electrical ratings

| Parameter | Value | Notes |
|-----------|-------|-------|
| Supply | 24 V DC | SELV |
| AI range | 0–10 V | Field voltage; published as mV |
| AO range | 0–10 V | DAC raw 0–4095 |
| RTD | PT100 / PT1000 | Rref 400 Ω / 4000 Ω (from type) |
| RS-485 | up to 115200 bit/s | Default 19200 8N1 |
| Operating temp | 0–40 °C | ≤ 95 % RH non-condensing |

### 3.3 Mechanical & environmental

| Property | Specification |
|----------|---------------|
| Mounting | DIN-rail 35 mm, **3 modules** wide |
| Ingress | IP20 (panel interior) |

### 3.4 Communication defaults

| Parameter | Default |
|-----------|---------|
| **Modbus Address** | `3` |
| **Baud Rate** | `19200` |
| **Parity** | `None` |
| **Stop Bits** | `1` |
| **Firmware** | `0.2.0` |

Address **1–247**; baud 9600 / 19200 / 38400 / 57600 / 115200. **Set via [WebConfig](#6-webconfig-reference) over USB-C.**

### 3.5 Reliability & protection

- ESD / overvoltage protection on field interfaces
- Modbus DAC writes clamped to 0–4095
- Button debounce 30 ms; LED logical vs physical state separated (Identify does not corrupt manual LED memory)

### RS-485 / Modbus RTU

All HomeMaster controllers and modules share the same RS-485 front end.

| Item | Value |
|---|---|
| Transceiver | MAX485CSA+T, half-duplex |
| Galvanic isolation | **None** — the transceiver shares the device's logic ground |
| Common-mode range | −7 V … +12 V referred to the device's own ground (MAX485 limit) |
| Terminals | A / B / COM |
| Surge protection | 3 × SMAJ6.8CA TVS (A–COM, B–COM, A–B) |
| Overcurrent | 2 × resettable PTC, 1.5 A hold, in series with A and B |
| EMI filtering | Common-mode choke on the A/B pair; COM referenced through 1 MΩ ∥ 4.7 nF |
| Idle state | Fail-safe biasing on board — do not add external bias resistors |
| Termination | 120 Ω at the two physical ends of the bus only |

**Bus wiring rules — apply to every device on the bus:**

- One twisted pair for A/B, 120 Ω characteristic impedance.
- Run **COM** to every node. Required, not optional: the ports are not isolated, and COM is what bounds the common-mode voltage the transceivers see.
- Prefer one power supply for the whole bus, distributed in star topology. With separate supplies, additionally tie the 0 V references together at a single point.
- Bond the cable shield to cabinet PE at one end only. Never land a shield on A, B or COM.
- Where the bus crosses into a different electrical installation with its own earthing reference, fit an external galvanic RS-485 isolator at that boundary.

---

## 4. Hardware & Interface

### 4.1 Diagrams & pinouts

![System block diagram](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/AIO-422-R1/Images/AIO_SystemBlockDiagram.png)

![MCU board](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/AIO-422-R1/Images/AIO-MCUBoard.png)
![Field board](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/AIO-422-R1/Images/AIO-FieldBoard.png)

### 4.2 Connectors

| Interface | Notes |
|-----------|--------|
| AI1–AI4 | 0–10 V field inputs — **AI3/AI4 board mapping corrected in firmware v0.2.0** |
| AO1–AO2 | 0–10 V outputs |
| RTD1–RTD2 | Match the on-board PT100/PT1000 jumper to the sensor type selected in WebConfig |
| RS-485 | A / B / COM |
| Power | 24 V DC |
| USB-C | WebConfig and firmware update |

### 4.3 Front panel

4 user buttons, 4 user LEDs, plus power / RX / TX status LEDs.

---

## 5. Getting Started

### 5.1 Safety

- SELV 24 V DC only. Disconnect power before wiring.
- Trained personnel for installation and service.

### 5.2 What you need

| Item | Purpose |
|------|---------|
| 24 V DC PSU | Module supply |
| RS-485 bus | To MiniPLC/MicroPLC or other master |
| USB-C cable | WebConfig commissioning |
| Chromium browser | Chrome, Edge, Opera, Brave, or Vivaldi |

### 5.3 Power notes

Power the module from 24 V DC before or while connecting USB-C for configuration.

### 5.4 Step-by-step

**Phase 1 — Wire**

1. Mount on a 35 mm DIN rail; connect 24 V DC.
2. Wire AI / AO / RTD / RS-485 (A/B/COM) as required.
3. Set each RTD channel’s on-board jumper to PT100 or PT1000 to match the sensor.

**Phase 2 — Configure (WebConfig)**

1. Connect USB-C; open [WebConfig v0.2.0](https://config.home-master.eu/AIO-422-R1/Firmware/v0.2.0/ConfigToolPage.html) → **Connect**.
2. Set a unique Modbus address and baud (must match the controller).
3. Configure RTD type/wires, AO power-on, buttons and LEDs. Changes auto-save.

**Phase 3 — Integrate**

Add the ESPHome package on MiniPLC/MicroPLC (see [§8](#8-esphome--home-assistant-integration)). Match **address** and **baud** to WebConfig.

```yaml
packages:
  aio1:
    url: https://github.com/isystemsautomation/homemaster-dev
    ref: main
    files:
      - path: AIO-422-R1/Firmware/v0.2.0/default_aio_422_r1_plc/default_aio_422_r1_plc.yaml
        vars:
          aio_prefix: "AIO#1"
          aio_id: aio_1
          aio_address: "3"
```

### 5.5 Verify

| Check | Expected |
|-------|----------|
| PWR LED | ON |
| WebConfig Connection | Connected |
| AI / RTD / AO | Live values update |
| Link OK (after bus poll) | ON while the master polls this address |
| Home Assistant entities | Appear from the package after the controller reloads |

---

## 6. WebConfig Reference

Open **[AIO-422-R1 WebConfig v0.2.0](https://config.home-master.eu/AIO-422-R1/Firmware/v0.2.0/ConfigToolPage.html)** in a Chromium-based browser. Connect via USB-C. Changes apply live and save to flash after a short idle period.

> Firefox: experimental only. Safari and stable Firefox are not supported.

### Connection, status & tools

![AIO-422-R1 WebConfig — connection, Modbus address/baud, tools and serial log](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/AIO-422-R1/Images/webconfig1.png)

| Control | Meaning |
|---------|---------|
| Identify (~5 s) | Blinks user LEDs |
| Factory reset | Restores defaults |
| Reboot | Restarts the module |
| Modbus Address / Baud | Slave ID and RS-485 speed |

### Analog inputs, outputs & RTD

![AIO-422-R1 WebConfig — live AI/AO values and RTD channels](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/AIO-422-R1/Images/webconfig2.png)

| Area | Notes |
|------|--------|
| AI1–AI4 | Live 0–10 V (mV) |
| AO1–AO2 | Raw slider 0–4095, power-on policy |
| RTD1–RTD2 | Temperature, fault, diagnostics; wire mode; PT100/PT1000. **Rref is not selectable** — PT100→400 Ω, PT1000→4000 Ω |

### Buttons & user LEDs

![AIO-422-R1 WebConfig — button actions and LED sources](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/AIO-422-R1/Images/webconfig3.png)

**Button actions (0–9):** Toggle flag (LED + Modbus ISTS); AO1/AO2 on/off; AO1/AO2 on/off restore last level; AO1/AO2 ±10 %; All AO off.

**LED sources (0–10):** Manual; AO1/AO2 = 0 %; AO1/AO2 = 100 %; Bus link OK; RTD1/RTD2 fault; AO1/AO2 active; Hardware fault.

---

## 7. Modbus Register Map

Live state for the ESPHome package is a **contiguous input-register block** (FC04). Legacy discrete inputs and holding registers remain registered for clients that still use them.

### 7.1 Overview

| Space | Addresses | Role |
|-------|-----------|------|
| Input registers (FC04) | 0..11 | Live state — **one poll** in the package |
| Input registers (FC04) | 200..204 | Identity (not polled by the package) |
| Discrete inputs (FC02) | 1..4, 20..23 | Buttons / LEDs — **legacy**, retained |
| Holding registers (FC03) | 120..121, 140..143 | RTD / AI — **legacy**, retained |
| Holding registers (FC03/06) | 200..201 | AO **command** (write); clamped 0..4095 |

### 7.2 Input Registers (FC04) — primary

| Address | Name | Format | Notes |
|---------|------|--------|-------|
| 0 | BTN_MASK | bits 0..3 | Buttons 1..4 (debounced) |
| 1 | LED_MASK | bits 0..3 | LEDs 1..4 (physical) |
| 2 | STATUS_FLAGS | bit1, bit3 | bit1 = Link OK; bit3 = Config dirty |
| 3 | FAULT_MASK | bits 0..6 | bit0/1 RTD1/2 sensor fault; bit2 !ads_ok; bit3/4 !dac_ok[0/1]; bit5/6 !rtd_ok[0/1] |
| 4..5 | RTD1..2 | S_WORD | °C ×10 |
| 6..9 | AI1..AI4 | U_WORD | mV |
| 10..11 | AO1..AO2 state | U_WORD | 0..4095 (state mirror, not the command path) |
| 200 | Model ID | U_WORD | Identity |
| 201..203 | FW major/minor/patch | U_WORD | Identity |
| 204 | Map version | U_WORD | `HM_MAP_VERSION` (0x0020 for v0.2.0) |

### 7.3 Discrete Inputs (FC02) — legacy

| Address | Signal |
|---------|--------|
| 1–4 | Button 1–4 |
| 20–23 | LED 1–4 |

### 7.4 Holding Registers (FC03) — legacy reads + AO command

| Address | Signal | Format | Notes |
|---------|--------|--------|-------|
| 120–121 | RTD1–2 temperature | S_WORD | °C ×10 (still updated) |
| 140–143 | AI1–4 | U_WORD | mV (still updated; **AI3/AI4 corrected**) |
| 200–201 | AO1–2 command | U_WORD | Write 0–4095; firmware clamps |

### 7.5 Polling (ESPHome package)

- `update_interval: 1s`, `command_throttle: 0ms` (STD-009)
- One **FC04 @0 count=12** per cycle
- AO: write-only holding outputs @200/201; displayed state from IREG 10/11

---

## 8. ESPHome / Home Assistant Integration

### Minimal YAML

```yaml
uart:
  id: rs485_uart
  tx_pin: GPIO4
  rx_pin: GPIO5
  baud_rate: 19200
  parity: NONE
  stop_bits: 1

modbus:
  id: modbus_bus
  uart_id: rs485_uart
  turnaround_time: 100ms
  send_wait_time: 250ms

packages:
  aio1:
    url: https://github.com/isystemsautomation/homemaster-dev
    ref: main
    files:
      - path: AIO-422-R1/Firmware/v0.2.0/default_aio_422_r1_plc/default_aio_422_r1_plc.yaml
        vars:
          aio_prefix: "AIO#1"
          aio_id: aio_1
          aio_address: "3"
```

> On ESPHome 2026.7.0+, set `turnaround_time` / `send_wait_time` explicitly as above (defaults became much slower).

### Entities (package)

Buttons 1–4, LEDs 1–4, Link OK, Config dirty, RTD1/2 fault, ADS/DAC/RTD chip fault bits, RTD1/2 temperature (°C), AI1–4 (mV), AO1/AO2 Raw (template number, state from IREG).

### Upgrade notes (v0.1.0 → v0.2.0)

- **Revert any Home Assistant AI3/AI4 entity renames** made to compensate for the v0.1.0 terminal swap — the firmware now matches the silk-screen terminals.
- Point the package path at `Firmware/v0.2.0/...`.
- Stored button actions 3 and 4 keep working with the expanded meanings (restore last level).

---

## 9. Programming & Build

- Flash **UF2** over USB-C (BOOTSEL) or build from `Firmware/v0.2.0/default_aio_422_r1/` with `arduino-cli compile --profile default`.
- Reproducible pins: [`sketch.yaml`](Firmware/v0.2.0/default_aio_422_r1/sketch.yaml) · [Build environment](../README.md#build-environment-reproducible).
- Pre-built binary: [`AIO-422-R1.uf2`](Firmware/v0.2.0/AIO-422-R1.uf2).

---

## 10. Maintenance & Troubleshooting

| Symptom | Check |
|---------|--------|
| No Modbus | Address/baud; A/B/COM wiring; termination |
| RTD wrong by ~100 °C | On-board jumper vs sensor type; v0.2.0 derives Rref (a stored 200 Ω Rref on v0.1.0 produced ~−117 °C on a room-temperature Pt100) |
| AI3/AI4 swapped vs labels | Update to v0.2.0; remove compensatory HA renames |
| Link OK off while polling | Master must address **this** slave; timeout 5 s |
| AO stuck at 0/4095 | Write path is HREG 200/201; state feedback is IREG 10/11 |

---

## 11. Downloads & Resources

### Version history

| Version | Config path (`path:`) | Date | Status |
|---------|------------------------|------|--------|
| **v0.2.0** | `AIO-422-R1/Firmware/v0.2.0/default_aio_422_r1_plc/default_aio_422_r1_plc.yaml` | 2026-07 | **Current — shipped on new modules** |
| v0.1.0 | `AIO-422-R1/Firmware/v0.1.0/default_aio_422_r1_plc/default_aio_422_r1_plc.yaml` | 2026-06 | Legacy (superseded by v0.2.0) |

> **v0.2.0 highlights:** contiguous IREG poll block; AI3/AI4 correction; derived RTD Rref; expanded button/LED options; non-blocking sensor path; DAC clamp; 30 ms button debounce.

### Files

- **Firmware v0.2.0 (UF2):** [`AIO-422-R1/Firmware/v0.2.0/AIO-422-R1.uf2`](https://github.com/isystemsautomation/homemaster-dev/raw/refs/heads/main/AIO-422-R1/Firmware/v0.2.0/AIO-422-R1.uf2)
- **ESPHome package (v0.2.0):** [`default_aio_422_r1_plc/`](https://github.com/isystemsautomation/homemaster-dev/tree/main/AIO-422-R1/Firmware/v0.2.0/default_aio_422_r1_plc)
- **WebConfig (v0.2.0):** [ConfigToolPage.html](https://config.home-master.eu/AIO-422-R1/Firmware/v0.2.0/ConfigToolPage.html)
- **Sketch source (v0.2.0):** [`default_aio_422_r1/`](https://github.com/isystemsautomation/homemaster-dev/tree/main/AIO-422-R1/Firmware/v0.2.0/default_aio_422_r1)
- **ESPHome package (v0.1.0 — legacy):** [`Firmware/v0.1.0/default_aio_422_r1_plc/`](https://github.com/isystemsautomation/homemaster-dev/tree/main/AIO-422-R1/Firmware/v0.1.0/default_aio_422_r1_plc)
- **WebConfig (v0.1.0 — legacy):** [ConfigToolPage.html](https://config.home-master.eu/AIO-422-R1/Firmware/v0.1.0/ConfigToolPage.html)
- **Schematics / manuals:** under `AIO-422-R1/Schematics/`, `AIO-422-R1/Manuals/`

---

## Open Source & Licensing

Hardware designs (schematics, PCB layouts, BOMs): **CERN-OHL-W v2**.

Firmware, ESPHome configurations, and software: **MIT License**.

See `LICENSE` files in each directory for full terms.

---

## 12. Compliance & Certifications

The AIO-422-R1 module is CE marked. **ISYSTEMS AUTOMATION S.R.L.** (HomeMaster® brand) maintains the technical documentation and a signed EU Declaration of Conformity (DoC).

### Applicable EU directives

- **EMC Directive 2014/30/EU** — EN 55032:2015 + AC:2016-07 + A11:2020 + A1:2020 (Class B emissions), EN 55035:2017 + A11:2020 (immunity); tested by Idvorsky Laboratories Ltd., Belgrade, Serbia (Job #1648, 20 April 2026)
- **RoHS Directive 2011/65/EU** — EN IEC 63000 technical documentation
- **Low Voltage Directive 2014/35/EU** — does not apply (SELV-only 24 V DC product; no mains-capable terminals)

### Compliance documents

| Document | File |
|---|---|
| EU Declaration of Conformity (DoC) | [DoC_AIO-422-R1.pdf](./Manuals/DoC_AIO-422-R1.pdf) |
| Datasheet | [AIO-422-R1_Datasheet.pdf](./Manuals/AIO-422-R1_Datasheet.pdf) |

### Trademark

**HomeMaster®** is a registered European Union trademark of ISYSTEMS AUTOMATION S.R.L., EUTM No. 019082911, registered with EUIPO on 15 January 2025.

---

## 13. Support

- **Support:** https://www.home-master.eu/support
- **Forum:** https://www.home-master.eu/forum/aio-422-r1-9
- **Shop:** https://www.home-master.eu/shop/aio-422-r1-analog-io-rtd-735
- **WebConfig:** https://config.home-master.eu/AIO-422-R1/Firmware/v0.2.0/ConfigToolPage.html

**Manufacturer:** ISYSTEMS AUTOMATION S.R.L. (HomeMaster® brand)  
**Registered office:** Str. Domnisori, Nr. 81, Bl. 62, Scara A, Etaj 3, Ap. 12, 100284 Ploiesti, Jud. Prahova, Romania  
**Office / Contact address:** Diligentei 18, Ploiesti, Romania  
**CUI / VAT:** RO 21537032  
**EUID:** ROONRC.J2007000919293  
**Telephone:** +40 747 757 798  
**Website:** [https://www.home-master.eu](https://www.home-master.eu)
