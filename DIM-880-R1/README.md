![Modbus](https://img.shields.io/badge/Protocol-Modbus%20RTU-brightgreen)
![License](https://img.shields.io/badge/License-MIT%20%2F%20CERN--OHL--W-blue)
![Status](https://img.shields.io/badge/Status-In%20development-orange)

# DIM-880-R1 — Module for 8-Channel AC Dimming

**HOMEMASTER – Modular control. Custom logic.**

> 🚧 **This module is in development.** Hardware design is in progress and no firmware has been released yet. This page describes the design as it stands and will be expanded as the module moves towards release. Values marked *in development* are not final.

**Document map:** [§1 Overview](#1-overview) · [§3 Specifications](#3-specifications) · [§4 Hardware](#4-hardware--interface) · [§5 Getting Started](#5-getting-started) · [§6 WebConfig](#6-webconfig-reference) · [§7 Modbus map](#7-modbus-register-map) · [§8 ESPHome](#8-esphome--home-assistant-integration) · [§9 Programming](#9-programming--build) · [§11 Downloads](#11-downloads--resources)

---

## 1. Overview

The **DIM-880-R1** is an 8-channel phase-cut AC dimmer with **8 digital inputs** — the 8-channel counterpart of the [DIM-420-R1](../DIM-420-R1/README.md). It mounts on a **35 mm DIN rail** and connects to **MiniPLC/MicroPLC** (or any Modbus RTU master) over **RS-485**, with Home Assistant integration via ESPHome packages.

The module shares its control board with the **[DIO-880-R1](../DIO-880-R1/README.md)** relay module; the field board differs.

**Key capabilities at a glance (design):**

- **8 MOSFET phase-cut AC dimming channels** — opto-isolated control, mains zero-cross detection, per-channel fuse, varistor and RC snubber
- **8 IEC 61131-2 compliant 24 V digital inputs (ISO1212 front-end)** — dry-contact (module-wetted), same input stage as the DIO-880-R1
- **Separate L / N input and output terminals per channel**
- **Standalone local logic** *(in development)* — wall switches keep working without a controller or network
- **Driverless WebConfig** over USB-C *(in development)*

## Key advantages

- **8 dimming channels + 8 inputs in one module.**
- Native ESPHome API via the MiniPLC/MicroPLC controller — no MQTT broker *(package in development)*.
- Local-first / edge-resilient — onboard logic keeps working if the network or Home Assistant is down.
- Open hardware (**CERN-OHL-W v2**) and firmware (**MIT**) — repairable, reproducible, no vendor lock-in.
- Standard **RS-485 Modbus RTU** — works with any Modbus master or industrial HMI/SCADA system, not locked to HomeMaster.

> **Limitation:** Dimmer channels have **no per-channel power or energy metering**.

---

## 2. Features

| Area | Detail |
|------|--------|
| **Dimming outputs** | 8 × MOSFET phase-cut AC channels; leading/trailing edge per channel *(firmware in development)* |
| **Zero-cross** | Mains zero-cross detection for synchronisation |
| **Digital inputs** | 8 × IEC 61131-2 24 V inputs (ISO1212), module-wetted, per-input return terminal |
| **WebConfig** | *In development* |
| **Modbus RTU slave** | RS-485; register map *in development* |
| **ESPHome / HA** | Package *in development* |

### Applications

- **Dimmable lighting** — incandescent, halogen and dimmable LED lamps, eight independent circuits.
- **Scene control** — wall switches on the inputs, scenes from Home Assistant or a PLC.

---

## 3. Specifications

> *Preliminary — values follow the current hardware design and may change before release.*

| Specification | Details |
|---|---|
| Microcontroller | RP2350A dual-core microcontroller |
| Storage | External QSPI Flash (W25Q32JV) |
| Power Input | 24 V DC nominal |
| Document revision | Draft · 2026-11 · Hardware R1 (in development) |
| Dimming outputs | 8 × phase-cut AC channels — load rating per channel *in development* |
| Channel protection | Fuse, varistor and RC snubber per channel |
| Digital Inputs | 8 × IEC 61131-2 compliant 24 V digital inputs (ISO1212 front-end) (DIx + GNDx), dry-contact (module-wetted) |
| RS-485 | half-duplex Modbus RTU, not galvanically isolated |
| USB | USB-C (configuration and firmware upload) |
| Modbus defaults | *In development* |
| Dimensions / DIN width | *In development* |
| Terminal type | Pluggable screw terminal blocks, 5.08 mm pitch |
| Operating / storage temperature, humidity, IP | *In development* |

### 3.4 Communication defaults

*In development.* The module will use the same RS-485 Modbus RTU interface as the rest of the HomeMaster range; address and baud rate will be set in WebConfig.

### 3.5 Reliability & protection

- Dimming stages controlled through optocouplers.
- Per-channel fuse, varistor and RC snubber on the mains side.
- Digital inputs: protected ISO1212 front-end, as on the DIM-420-R1 and DIO-880-R1.

---

### RS-485 / Modbus RTU

All HomeMaster controllers and modules share the same RS-485 front end.

| Item | Value |
|---|---|
| Transceiver | MAX485, half-duplex |
| Galvanic isolation | **None** — the transceiver shares the device's logic ground |
| Terminals | A / B / COM — **terminal order will be confirmed on release; always read the silkscreen** |
| Surge protection | 3 × TVS (A–COM, B–COM, A–B) |
| Overcurrent | 2 × resettable PTC in series with A and B |
| EMI filtering | Common-mode choke on the A/B pair; COM referenced through 1 MΩ ∥ 4.7 nF |
| Termination | 120 Ω at the two physical ends of the bus only |

## 4. Hardware & Interface

### 4.1 Diagrams & pinouts

*In development.* Block diagram, board diagrams and pinout images will be added before release. Current schematics: [§11 Downloads](#11-downloads--resources).

### 4.2 Connectors & terminal map

> *Preliminary — positions and silkscreen labels are not final.*

| Group | Terminals | Function |
|-------|-----------|----------|
| POWER | V+, 0V | 24 V DC input |
| CH1…CH8 | L IN, N IN, L OUT, N OUT (per channel) | Mains feed and dimmed output of each channel |
| RS485 | A, B, COM | Modbus RTU |
| DI1…DI8 | Ix, GNDx (×8) | Digital inputs and their returns |

**Input power / wetting.** The digital inputs use the same ISO1212 front-end as the DIO-880-R1 and are wetted from the module's own fused 24 V supply. Wire a dry contact between **Ix** and **GNDx**.

### 4.3 Front panel — buttons & LEDs

*In development.* The control board carries front-panel buttons and status LEDs for the inputs and outputs, with hardware key combinations for firmware upload (BOOT) and reset.

---

## 5. Getting Started

### 5.1 Safety *(read before wiring)*

> ⚠️ **The dimming channels carry mains voltage.** Installation by qualified personnel only. Disconnect all supplies before wiring. 24 V DC, RS-485, digital inputs and USB are SELV.

Full safety and installation guidance will be added before release.

### 5.2 What you need

| Category | Item / Notes |
|----------|--------------|
| **Hardware** | DIM-880-R1 |
| **Controller** | HomeMaster MiniPLC/MicroPLC or any Modbus RTU master |
| **24 VDC PSU** | Regulated SELV |
| **RS-485 cable** | Twisted pair A/B + COM; 120 Ω termination at both trunk ends |
| **USB-C cable** | WebConfig and firmware upload |

### 5.3 – 5.5 Power notes, step-by-step, verify

*In development.*

---

## 6. WebConfig Reference

*In development.* Planned, following the DIM-420-R1: Modbus address/baud, cut mode and load thresholds per channel, input press logic, buttons and LEDs.

---

## 7. Modbus Register Map

*In development.* The register map will be published together with the first firmware release.

---

## 8. ESPHome / Home Assistant Integration

*In development.* An ESPHome package for MiniPLC/MicroPLC will be provided, following the other HomeMaster modules.

---

## 9. Programming & Build

*In development.* Firmware upload will be via UF2 over USB-C, as on the other RP2350-based modules.

---

## 10. Maintenance & Troubleshooting

*In development.*

---

## 11. Downloads & Resources

| Resource | Status |
|----------|--------|
| Firmware (UF2, source) | *In development* |
| ESPHome package | *In development* |
| WebConfig tool | *In development* |
| Schematics (PDF) | Field Board: [`Schematics/DIM-880-R1-FieldBoard.pdf`](https://config.home-master.eu/DIM-880-R1/Schematics/DIM-880-R1-FieldBoard.pdf) · MCU Board: [`Schematics/DIM-880-R1-MCUBoard.pdf`](https://config.home-master.eu/DIM-880-R1/Schematics/DIM-880-R1-MCUBoard.pdf) — work in progress |
| Images & diagrams | *In development* |

---

## Open Source & Licensing

This project uses a hybrid licensing model.

**Hardware** — schematics, PCB layouts, BOMs: **CERN-OHL-W v2**

**Firmware & ESPHome integration** — firmware, ESPHome configs, software: **MIT License**

---

## 12. Compliance & Certifications

*In development.* The module is not yet released. Conformity assessment will be completed before release.

### Trademark

**HomeMaster®** is a registered European Union trademark of ISYSTEMS AUTOMATION S.R.L., EUTM No. 019082911, registered with EUIPO on 15 January 2025.

---

## 13. Support

- **Official Support Portal:** https://www.home-master.eu/support
- **YouTube:** https://youtube.com/@HomeMaster
- **Hackster:** https://hackster.io/homemaster
- **Reddit:** https://reddit.com/r/HomeMaster
- **Instagram:** https://instagram.com/home_master.eu

**Manufacturer:** ISYSTEMS AUTOMATION S.R.L. (HomeMaster® brand)  
**Registered office:** Str. Domnisori, Nr. 81, Bl. 62, Scara A, Etaj 3, Ap. 12, 100284 Ploiesti, Jud. Prahova, Romania  
**Office / Contact:** Diligentei 18, Ploiesti, Romania  
**CUI / VAT:** RO 21537032 · **EUID:** ROONRC.J2007000919293  
**Telephone:** +40 747 757 798  
**Website:** [https://www.home-master.eu](https://www.home-master.eu)
