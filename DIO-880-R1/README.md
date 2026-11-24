![Modbus](https://img.shields.io/badge/Protocol-Modbus%20RTU-brightgreen)
![License](https://img.shields.io/badge/License-MIT%20%2F%20CERN--OHL--W-blue)
![Status](https://img.shields.io/badge/Status-In%20development-orange)

# DIO-880-R1 — Module for 8-Channel Relay, Lighting & Analog Control

**HOMEMASTER – Modular control. Custom logic.**

> 🚧 **This module is in development.** Hardware design is in progress and no firmware has been released yet. This page describes the design as it stands and will be expanded as the module moves towards release. Values marked *in development* are not final.

**Document map:** [§1 Overview](#1-overview) · [§3 Specifications](#3-specifications) · [§4 Hardware](#4-hardware--interface) · [§5 Getting Started](#5-getting-started) · [§6 WebConfig](#6-webconfig-reference) · [§7 Modbus map](#7-modbus-register-map) · [§8 ESPHome](#8-esphome--home-assistant-integration) · [§9 Programming](#9-programming--build) · [§11 Downloads](#11-downloads--resources)

---

## 1. Overview

The **DIO-880-R1** is an 8-input / 8-relay DIN-rail module for lighting and general switching — lights, pumps, heating elements, fans. Next to the relays it carries a controller interface for **DALI (IEC 62386)** drivers, two **0–10 V analog outputs** and a **1-Wire** port for temperature sensors. It connects to **MiniPLC/MicroPLC** (or any Modbus RTU master) over **RS-485**, with Home Assistant integration via ESPHome packages.

The module shares its control board with the **[DIM-880-R1](../DIM-880-R1/README.md)** 8-channel dimmer; the field board differs.

**Key capabilities at a glance (design):**

- **8 IEC 61131-2 compliant 24 V digital inputs (ISO1212 front-end)** — dry-contact (module-wetted), same input stage as the DIM-880-R1
- **8 SPDT relays** (NO / C / NC), opto-isolated drivers, varistor across each contact pair
- **DALI (IEC 62386) controller interface** — optically isolated, polarity-independent; the bus is powered by an external DALI bus power supply
- **2 × 0–10 V analog outputs** — 12-bit, for 0–10 V / 1–10 V dimmable drivers, valves and fans
- **1-Wire port** — 5 V, for DS18B20-type temperature sensors; ESD-protected, fused supply
- **Standalone local logic** *(in development)* — wall switches keep working without a controller or network
- **Driverless WebConfig** over USB-C *(in development)*

## Key advantages

- **Relays, DALI, 0–10 V and 1-Wire in one module** — one module per lighting group instead of three.
- Native ESPHome API via the MiniPLC/MicroPLC controller — no MQTT broker *(package in development)*.
- Local-first / edge-resilient — onboard logic keeps working if the network or Home Assistant is down.
- Open hardware (**CERN-OHL-W v2**) and firmware (**MIT**) — repairable, reproducible, no vendor lock-in.
- Standard **RS-485 Modbus RTU** — works with any Modbus master or industrial HMI/SCADA system, not locked to HomeMaster.

---

## 2. Features

| Area | Detail |
|------|--------|
| **Digital inputs** | 8 × IEC 61131-2 24 V inputs (ISO1212), module-wetted, per-input return terminal |
| **Relay outputs** | 8 × SPDT (NO / C / NC), opto-isolated drivers |
| **DALI (IEC 62386)** | Application-controller interface; addressing, groups and scenes configured from the module *(firmware in development)* |
| **Analog outputs** | 2 × 0–10 V, 12-bit |
| **1-Wire** | One 1-Wire bus, 5 V |
| **WebConfig** | *In development* |
| **Modbus RTU slave** | RS-485; register map *in development* |
| **ESPHome / HA** | Package *in development* |

### Applications

- **Lighting** — on/off circuits on relays, dimmable groups on DALI or 0–10 V, wall switches on the inputs.
- **Pumps, fans, heating elements** — switched through the relays (use contactors for loads above the relay rating).
- **Temperature** — room or pipe temperature from 1-Wire sensors for heating and ventilation logic.

---

## 3. Specifications

> *Preliminary — values follow the current hardware design and may change before release.*

| Specification | Details |
|---|---|
| Microcontroller | RP2350A dual-core microcontroller |
| Storage | External QSPI Flash (W25Q32JV) |
| Power Input | 24 V DC nominal |
| Document revision | Draft · 2026-11 · Hardware R1 (in development) |
| Digital Inputs | 8 × IEC 61131-2 compliant 24 V digital inputs (ISO1212 front-end) (DIx + GNDx), dry-contact (module-wetted) |
| Relay outputs | 8 × SPDT (NO / C / NC); relay rated 6 A @ 250 VAC — module contact limit *in development* |
| Relay contact protection | 275 V rms varistor across each contact pair (C–NO, C–NC) |
| DALI (IEC 62386) | 1 × bus interface, optically isolated, polarity-independent; external bus power supply required (16 V typ., ≤ 250 mA) |
| Analog outputs | 2 × 0–10 V, 12-bit, referenced to module GND (not galvanically isolated) |
| 1-Wire | 1 × bus, 5 V; resettable fuse on supply and data, ESD protection |
| RS-485 | half-duplex Modbus RTU, not galvanically isolated |
| USB | USB-C (configuration and firmware upload) |
| Modbus defaults | *In development* |
| Dimensions / DIN width | *In development* |
| Terminal type | Pluggable screw terminal blocks, 5.08 mm pitch |
| Operating / storage temperature, humidity, IP | *In development* |

### 3.4 Communication defaults

*In development.* The module will use the same RS-485 Modbus RTU interface as the rest of the HomeMaster range; address and baud rate will be set in WebConfig.

### 3.5 Reliability & protection

- Relay drivers opto-isolated; varistor across each relay contact pair.
- DALI interface isolated by optocouplers; TVS across the bus terminals.
- 0–10 V outputs: series resistor, TVS and filter capacitor on each output.
- 1-Wire: ESD protection diode with series resistors on data, resettable fuses on supply and data, reverse-feed diode on the sensor supply.
- Digital inputs: same protected ISO1212 front-end as the DIO-430-R1 and DIM-880-R1.

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

*In development.* Block diagram, board diagrams and pinout images will be added before release.

### 4.2 Connectors & terminal map

> *Preliminary — positions and silkscreen labels are not final.*

**Top row** (24Vdc | RELAY OUTPUT)

| Group | Terminals | Function |
|-------|-----------|----------|
| POWER | V+, 0V | 24 V DC input |
| — | empty | Separation between 24 V and mains-side relay contacts |
| RELAY1…RELAY8 | NO, C, NC (×8) | Relay contacts |

**Bottom row** (RS-485 | DI 24Vdc | DALI | 0–10 V | 1-Wire)

| Group | Terminals | Function |
|-------|-----------|----------|
| RS485 | A, B, COM | Modbus RTU |
| DI1…DI8 | Ix, GNDx (×8) | Digital inputs and their returns |
| DALI | DA, DA | DALI bus (IEC 62386), polarity-independent, separated from neighbouring terminals by an empty position |
| AO | AO1, AO2, GND | 0–10 V outputs |
| 1-WIRE | +5V, DATA, GND | 1-Wire sensors |

**Input power / wetting.** The digital inputs use the same ISO1212 front-end as the DIM-880-R1 and are wetted from the module's own fused 24 V supply. Wire a dry contact between **Ix** and **GNDx**.

**DALI bus.** The module does not power the DALI bus. Connect an external DALI bus power supply to the same two wires. The bus side is isolated from the rest of the module: never connect DA to GND, PE or any other terminal.

**0–10 V outputs.** Referenced to the module GND. Connect only to drivers whose 0–10 V / 1–10 V control input is SELV, and do not run these wires in the same cable as mains.

### 4.3 Front panel — buttons & LEDs

*In development.* The control board carries front-panel buttons and status LEDs for the inputs and outputs; hardware key combinations for firmware upload (BOOT) and reset are the same as on the DIM-880-R1.

---

## 5. Getting Started

### 5.1 Safety *(read before wiring)*

> ⚠️ **The relay contacts may carry mains voltage, and the DALI bus is not SELV** (DALI wiring is often run in the same cable as mains). Installation by qualified personnel only. Disconnect all supplies before wiring. 24 V DC, RS-485, digital inputs, 0–10 V, 1-Wire and USB are SELV.

> ⚠️ **Capacitor motors (roller shutters, blinds, awnings, gate and garage tubular motors) must NOT be connected directly to the relay outputs.** Each relay contact carries a 275 V arc-suppression varistor that is destroyed by the open-contact voltage of a capacitor motor. Use interposing relays or contactors with ≥ 400 V contacts and no RC or varistor across the contacts.

Full safety and installation guidance will be added before release.

### 5.2 What you need

| Category | Item / Notes |
|----------|--------------|
| **Hardware** | DIO-880-R1 |
| **Controller** | HomeMaster MiniPLC/MicroPLC or any Modbus RTU master |
| **24 VDC PSU** | Regulated SELV |
| **DALI bus PSU** | External DALI bus power supply, only if the DALI interface is used |
| **RS-485 cable** | Twisted pair A/B + COM; 120 Ω termination at both trunk ends |
| **USB-C cable** | WebConfig and firmware upload |

### 5.3 – 5.5 Power notes, step-by-step, verify

*In development.*

---

## 6. WebConfig Reference

*In development.* Planned: Modbus address/baud, input and relay logic, DALI commissioning (search, identify, addressing, groups, scenes, driver replacement), 0–10 V and 1-Wire settings.

---

## 7. Modbus Register Map

*In development.* The register map will be published together with the first firmware release.

---

## 8. ESPHome / Home Assistant Integration

*In development.* An ESPHome package for MiniPLC/MicroPLC will be provided, following the other HomeMaster modules.

---

## 9. Programming & Build

*In development.* The module uses the same RP2350A control board as the DIM-880-R1; firmware upload will be via UF2 over USB-C.

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
| Schematics (PDF) | *In development* — control board: see [DIM-880-R1 MCU Board](https://config.home-master.eu/DIM-880-R1/Schematics/DIM-880-R1-MCUBoard.pdf) (shared) |
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

DALI is a registered trademark of the Digital Illumination Interface Alliance (DiiA). The DIO-880-R1 is not DALI-certified; it implements the IEC 62386 interface for use with DALI drivers.

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
