# RGB-621-R1 — Module for RGB+CCT LED Control

**HOMEMASTER – Modular control. Custom logic.**

The **RGB-621-R1** is an **RGB + tunable-white (CCT) LED controller** with **5 PWM channels**, **2 digital inputs**, **1 relay**, **Modbus RTU / Home Assistant** integration, and **USB-C WebConfig** setup.

![RGB-621-R1 photo](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/photo1.png)

![Modbus](https://img.shields.io/badge/Protocol-Modbus%20RTU-brightgreen)
![License](https://img.shields.io/badge/License-GPLv3%20%2F%20CERN--OHL--W-blue)

## Contents

- [Quick Start](#-quick-start)
- [Version History](#-version-history)
- [Hardware notes (current revision)](#hardware-notes-current-revision)
- [1. Introduction](#1-introduction)
- [2. Technical Specification](#2-rgb-621-r1--technical-specification)
- [3. Use Cases](#3-use-cases)
- [4. Safety Information](#4-safety-information)
- [5. Installation & Quick Start](#5-installation--quick-start)
- [6. Modbus RTU Communication](#6-modbus-rtu-communication)
- [7. ESPHome Integration Guide](#7-esphome-integration-guide)
- [8. Firmware & Programming](#8-firmware--programming)
- [9. Maintenance & Troubleshooting](#9-maintenance--troubleshooting)
- [10. Open Source & Licensing](#10-open-source--licensing)
- [11. Downloads](#11-downloads)
- [12. Support](#12-support)
- [Compliance & Certifications](#compliance--certifications)

> **v0.1.0 is deprecated — use v0.2.0.** v0.1.0 remains available for existing installs but is no longer maintained.

## 🚀 Quick Start

New modules ship firmware **v0.2.0**. Add the ESPHome package to your **MicroPLC** / **MiniPLC** — see [§7 ESPHome Integration](#7-esphome-integration-guide) for the ready-to-copy YAML. Give each module a **unique Modbus address** (default **3**).

## 📦 Version History

| Version | Config path (`path:`) | Date | Changes |
|--------|------------------------|------|-----------|
| **v0.2.0** | `RGB-621-R1/Firmware/v0.2.0/default_rgb_621_r1_plc/default_rgb_621_r1_plc.yaml` | 2026-07 | **Current release.** Local input engine (momentary/maintained, multi-click, hold-to-dim), 12-bit PWM + gamma + slew, scenes, relay FOLLOW, HA STATE readback; Modbus engine-config removed (config is USB WebConfig only). |
| **v0.1.0** | `RGB-621-R1/Firmware/v0.1.0/default_rgb_621_r1_plc/default_rgb_621_r1_plc.yaml` | 2026-01 | Deprecated (legacy) — superseded by v0.2.0. Kept for existing installs; no longer maintained. |

> **Reproducible firmware build (v0.2.0):** [Build environment (reproducible)](../../README.md#build-environment-reproducible) · [`sketch.yaml`](Firmware/v0.2.0/default_rgb_621_r1/sketch.yaml)

# Hardware notes (current revision)

1. **I.1/I.2 panel indicators swapped** — silkscreen only (I.1 shows DI2's state, I.2 shows DI1's); logical inputs, Modbus registers, and WebConfig are unaffected.
2. **Only SW2 is a usable logic input** (Button 1 in WebConfig); the second front button is not a user input — it enters BOOT mode only (see [§8.1](#81-updating-firmware-regular-users)).

# 1. Introduction

## 1.1 Overview of the RGB-621-R1

The **RGB-621-R1** is a **smart RGB + CCT LED controller module** designed for **HomeMaster automation systems** and other **Modbus RTU networks**.  
It features **5 high-current PWM outputs** for RGB and Tunable White (CCT) LED control, **2 IEC 61131-2 compliant digital inputs** for potential-free wall switches or contacts, and **1 relay output** for switching external loads or LED drivers.

Powered by the **Raspberry Pi RP2350A** microcontroller, the module supports **RS-485 (Modbus RTU)** communication and configuration via **WebConfig over USB-C (Web Serial)** — no drivers or external software required.  
It connects directly to **HomeMaster MicroPLC** and **MiniPLC** controllers or operates as a **standalone Modbus slave** in any automation network.

Its **dual-board I/O architecture**, **surge and short-circuit protection**, and field/logic separation ensure accurate dimming, stable communication, and reliable operation in demanding **home, ambient, or architectural lighting applications**.


---

## 1.2 Features & Architecture

| Subsystem         | Qty | Description |
|-------------------|-----|-------------|
| **Digital Inputs** | 2 | IEC 61131-2 compliant 24 V digital inputs (ISO1212 front-end), **dry-contact (module-wetted)**, with PTC fuse, TVS surge and reverse-polarity protection |
| **PWM Outputs** | 5 | N-channel MOSFET drivers (AP9990GH-HF), 12 V / 24 V LED channels for R / G / B / CW / WW |
| **Relay Output** | 1 | SPST-NO relay (HF115F/005-1ZS3), 5 V coil; 3 A @ 250 VAC / 30 VDC (module/PCB limit) |
| **Buttons** | 2 | Local control or configuration triggers (SW1 / SW2) |
| **LED Indicators** | 8 | PWR, TX, RX, I.1, I.2, O.1, and two user LEDs (LED1/LED2) |
| **Modbus RTU** | Yes | RS-485 interface via MAX485CSA+T transceiver (external 120 Ω bus termination) |
| **USB-C** | Yes | WebConfig & firmware flashing with PRTR5V0U2X ESD protection |
| **Power Input** | 24 V DC ±10 % (SELV/PELV) | Protected by resettable fuses (1206L series), TVS (SMBJ33A), and reverse-blocking (STPS340U) |
| **Logic Supply** | — | AP64501SP-13 buck (5 V) + AMS1117-3.3 LDO chain |
| **MCU** | RP2350A | Dual-core Arm Cortex-M33 @ 133 MHz with 32 Mbit QSPI Flash (W25Q32JVUUIQ) |
| **Protection** | — | Digital-input front-end per IEC 61131-2 (ISO1212); surge/EMI protected; TVS diodes, PTC fuses, transient suppression on field I/O |

**Architecture summary:**  
- **MCU Board:** manages logic, USB, Modbus, and power regulation  
- **Field Board:** contains LED drivers, relay circuit, and digital input section  
This modular, two-board design ensures clean signal separation between logic and 24 V field wiring, improving reliability in mixed-voltage installations.

---

## 1.3 System Role & Communication

The **RGB-621-R1** operates as a **Modbus RTU slave** on an **RS-485 differential bus**, typically polled by a **HomeMaster controller** (MicroPLC / MiniPLC) or other Modbus master.  
Each module on the bus must have a **unique** Modbus address (default **3**); change it in WebConfig to avoid collisions. Up to 32 devices per bus are supported.

**Default communication parameters:**  
- **Address:** 3  
- **Baud rate:** 19200 bps  
- **Format:** 8 data bits, no parity, 1 stop bit (8N1)  
- **Bus termination:** external 120 Ω at both physical bus ends (not on the module)  
- **Fail-safe:** retains last valid PWM and relay state if communication is lost  

The controller periodically polls holding registers to:  
- Write PWM duty values for R, G, B, CW, WW channels  
- Control the relay output  
- Read digital input and status bits  

WebConfig enables users to modify address, baud rate, test I/O, calibrate channels, and perform real-time diagnostics — simplifying setup and commissioning.

---

# 2. RGB-621-R1 — Technical Specification

## 2.1 Diagrams & Pinouts

| System Block | RP2350A Pinout | Field Board | MCU Board |
|:---:|:---:|:---:|:---:|
| <img src="https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/RGB_SystemBlock.png" width="200"> | <img src="https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/RGB_MCU_Pinouts.png" width="200"> | <img src="https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/RelayBoard_Diagram.png" width="200"> | <img src="https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/MCUBoard_Diagram.png" width="200"> |

---

## 2.2 Overview

See [§1.2 Features & Architecture](#12-features--architecture) for the subsystem overview.

---

## 2.3 I/O Summary

I/O counts only — full descriptions in [§1.2](#12-features--architecture).

| Interface | Qty |
|------------|-----|
| **Digital Inputs** | 2 |
| **Relay** | 1 |
| **PWM Outputs** | 5 |
| **RS-485 (Modbus)** | 1 |
| **USB-C** | 1 |
| **MCU** | 1 |
| **Buttons** | 2 |
| **LED Indicators** | 8 |

---

## 2.4 Terminals & Pinout

![Front Terminals](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/photo1.png)

**Top:** V+/0 V (24 V DC input), Relay C / NO, Inputs I1/I2 (+ GND)  
**Bottom:** PWM R/G/B/CW/WW, **COM (LED+)**, RS-485 A/B (+ RS-485 COM opt.)

---

## 2.5 Electrical & Environmental

- **Supply:** 24 V DC ±10 % (SELV/PELV), ≈ 2 W (no LED load)  
- **PWM Drive:** total LED current limited by onboard **10 A PTC fuse**; per-channel and track limits apply — LED PSU sizing: [⚠️ IMPORTANT — POWER](#important-power)
- **Relay:** 3 A @ 250 VAC / 30 VDC (module/PCB limit)  
- **Digital inputs:** IEC 61131-2 front-end (ISO1212), **dry-contact (module-wetted)**; surge/EMI protected  
- **RS-485:** 19200 bps 8N1 (default), 115.2 kbps max  
- **USB-C:** WebConfig / firmware only, ESD-protected  
- **Env.:** 0 – 40 °C, ≤ 95 % RH non-condensing

---

## 2.6 MCU, Protections & Build

- **MCU:** Raspberry Pi RP2350A dual-core M33  
- **Storage:** W25Q32 32 Mbit Flash  
- **Protections:** PTC fuses, TVS diodes, reverse polarity & ESD networks  
- **Mounting:** DIN-rail EN 50022 (35 mm), IP20 enclosure  
- **Dimensions:** 52.5 × 90.6 × 67.3 mm · Weight ≈ 0.25 kg

![Dimensions](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/RGB-621-R1Dimensions.png)

---

## 2.7 Absolute Ratings

| Parameter | Min | Typ | Max | Notes |
|------------|-----|-----|-----|-------|
| Supply Voltage | 21.6 V | 24 V | 26.4 V | 24 V DC ±10 % (SELV/PELV); input protected |
| Power Use | — | 1.85 W | 3.0 W | No LED load |
| Relay Contacts | — | — | 3 A @ 250 VAC / 30 VDC | Module/PCB limit (resistive) |
| LED Rail Current | — | — | 10 A total | Onboard 10 A PTC fuse; per-channel MOSFET/track limited; sum of all channels ≤ 10 A |
| RS-485 Rate | — | — | 115.2 kbps | Half-duplex |
| USB Voltage | 4.75 V | 5 V | 5.25 V | Logic only |
| Operating Temp | 0 °C | — | 40 °C | ≤ 95 % RH |

> **Installer Tip:** Use upstream fusing and snubbers for inductive loads.

---

## 2.8 Firmware & Operation

Modbus RTU slave; configured via USB-C WebConfig. Registers/coils control PWM and relay and read inputs — see [§6 Modbus](#6-modbus-rtu-communication). Front-panel LEDs — see [§9.1](#91-status-leds-front-panel).

---

# 3. Use Cases

The RGB-621-R1 is a 5-channel RGB + tunable-white (CCT) LED controller with two wall-switch inputs,
one relay, and an on-module control engine — it runs standalone or as a Modbus RTU / Home Assistant
node. I/O: 5 PWM channels (R, G, B, WW, CW), 2 digital inputs, 1 SPST-NO relay.

### 🎚️ Use Case 1 — Standalone wall-switch dimming, color and scenes (no controller)
Drive an RGB or RGB+CCT strip directly from one or two momentary wall buttons, all logic on the
module: short-press toggles a group, press-and-hold ramps brightness smoothly (hold-to-dim over a
configurable time), double-press jumps to full or recalls one of four on-module scenes. DI1 handles
the RGB group, DI2 the warm/cool (CCT) group by default — fully configurable in WebConfig. Works
with the bus and Home Assistant offline.

### 🔌 Use Case 2 — Relay as automatic LED-PSU power-cut (energy saving)
Set the onboard relay to **FOLLOW** mode and wire **Relay C / NO** **externally in series** with the LED driver supply (+): the relay closes while any watched channel is on and opens after an off-delay once everything is dark — no standby draw or driver heating. The module does **not** switch the **COM (LED+)** rail internally; FOLLOW only drives the separate dry-contact output. Or use the relay in **Manual** mode as a free switched output driven by a gesture, Modbus, or Home Assistant.
Module output is 3 A @ 250 VAC (PCB limit); use an interposing contactor for larger loads.

### 🏠 Use Case 3 — Full Home Assistant integration with live state
Add the ESPHome package (see [§7](#7-esphome-integration-guide)) to a MicroPLC/MiniPLC and the strip appears in Home
Assistant as an RGB+CCT light with the relay as a switch. Control color, brightness and CCT from HA
with 12-bit gamma-corrected, step-free dimming, while wall switches keep working locally; the module
reports its actual state back so HA stays in sync — and everything keeps working if the controller
or network goes down.

---

# 4. Safety Information

## 4.1 General Requirements

| Requirement | Detail |
|--------------|--------|
| **Qualified Personnel** | Installation, wiring, and servicing must be performed by trained technicians familiar with 24 V DC SELV/PELV control systems and, where applicable, mains switching on relay contacts per local regulations. |
| **Power Isolation** | Always disconnect the **24 V DC module supply**, **LED PSU**, and RS-485 network before wiring or servicing. |
| **Rated Voltages** | **Mains (230 VAC)** may be switched **only** on **Relay C / NO** (≤ **3 A**), by qualified personnel per local regulations — enclosure, SELV/mains separation, load fuse/breaker, strain relief. **All other terminals** (V+/0V, I1/I2, RS-485 A/B/**RS-485 COM**, **LED PS**, USB-C) are SELV/PELV — **never** apply mains to them. Module and LED supply: see [⚠️ IMPORTANT — POWER](#important-power) in [§5](#5-installation--quick-start). |
| **Power** | See [⚠️ IMPORTANT — POWER](#important-power) in [§5](#5-installation--quick-start). |
| **Grounding** | Ensure proper protective-earth (PE) connection of the control cabinet and shielded bus cable. |
| **Enclosure** | Mount the device on a DIN rail inside a dry, clean enclosure. Avoid condensation, dust, or corrosive atmosphere. |

---

## 4.2 Installation Practices

Safety practices for qualified installers. Field wiring map: [§5.4](#54-installation--wiring). Power layout: [⚠️ IMPORTANT — POWER](#important-power) in [§5](#5-installation--quick-start).

- Mount on a **35 mm DIN rail (EN 60715)** inside a dry, clean enclosure ([§4.1](#41-general-requirements)).
- Provide at least **10 mm** clearance above/below for airflow and terminal access.
- **Disconnect** the **24 V DC module supply**, **LED PSU**, and RS-485 network before wiring or servicing.
- Route **LED-power wiring separately** from RS-485 and signal lines.
- **Do not** externally bridge `GND_FUSED` (field) and `GND` (logic/USB) — domains are separated on the PCB.
- Relay coil drive is isolated from contacts via **SFH6156 optocoupler** (**basic insulation**); digital inputs use an ISO1212 IEC 61131-2 front-end wetted from module 24 V — **not** a galvanic isolator.
- For inductive relay loads, add an **external flyback diode or RC snubber**; keep relay conductors away from signal wiring.
- Follow local electrical codes for fusing, grounding, and enclosure class.

---

## 4.3 Interface Warnings

### ⚡ Power Supply (24 V DC)

| Parameter | Specification |
|------------|---------------|
| Nominal Voltage | 24 V DC ±10 % (SELV/PELV) |
| Input Protection | PTC fuses (F1–F4), reverse-polarity diode (STPS340U), surge TVS (SMBJ33A) |
| Ground Reference | Field return `GND_FUSED` |
| Front-end | IEC 61131-2 digital-input front-end (ISO1212); surge/EMI protected |
| Notes | Use a regulated SELV 24 V DC supply rated ≥ 1 A per module. |

---

### 🟢 Digital Inputs

| Parameter | Specification |
|------------|---------------|
| Type | IEC 61131-2 compliant, **dry-contact (module-wetted)** 24 V DC input |
| Circuit | ISO1212 receiver with TVS (SMBJ26CA) + PTC protection |
| Input voltage | 24 V DC ±10 % (SELV/PELV) — supplied by module wetting; do not apply external voltage to I1/I2 |
| Protection | PTC fuse, TVS surge and reverse-polarity protection |
| Notes | Connect **potential-free (dry) contacts** — wall switches, push buttons, or relay / open-collector / transistor outputs that simply close I1/I2 to **GND**. The module supplies the input current (wetting) from its own 24 V rail; do **not** feed external voltage into I1/I2. Three-wire sensors must be powered from your own supply, with their switching output wired to the input (the module provides no sensor-supply rail). Debounce handled in firmware. |

---

### 🔴 Relay Output

| Parameter | Specification |
|------------|---------------|
| Type | SPST-NO mechanical relay (HF115F/005-1ZS3) |
| Coil Voltage | 5 V DC (via SFH6156 optocoupler + S8050 driver) |
| Contact Rating | 3 A @ 250 VAC / 30 VDC (module/PCB limit, resistive) |
| Insulation | Basic insulation between SELV coil drive and contacts; external contactor for reinforced isolation or heavy/inductive loads |
| Component note | HF115F relay component rated up to 12 A @ 250 VAC — **module output limited to 3 A**; use an external contactor for higher or inductive loads |
| Protection | External RC snubber / flyback diode recommended |
| Notes | Independent SPST-NO dry contact (**Relay C** / **NO**); not in the LED anode rail. For FOLLOW-mode LED-PSU cut, wire **Relay C / NO** externally in series with the LED driver supply. Keep field wiring separate from logic. |

---

### 🔵 RS-485 Communication

| Parameter | Specification |
|------------|---------------|
| Transceiver | MAX485CSA+T |
| Bus Type | Differential, multi-drop (A/B lines) |
| Default Settings | 19200 bps · 8N1 |
| Bus termination | External 120 Ω at both bus ends |
| Protection | Surge/ESD network integrated |
| Notes | Observe polarity (A = +, B = –). Use shielded twisted-pair cable; ground shield at one end only. |

---

### 🧰 USB-C Interface

| Parameter | Specification |
|------------|---------------|
| Function | WebConfig setup & firmware update only |
| Protection | PRTR5V0U2X ESD + CG0603MLC-05E current limiters |
| Supply | 5 V DC from host computer (logic domain) |
| Isolation | Shares logic ground (`GND`); not isolated from RS-485 logic |
| Notes | USB-C is for WebConfig setup and firmware update. It may be connected at any time, including while the module is powered from its 24 V supply. Not a field power or data bus. |

---

> Power layout and two-supply rules: see [⚠️ IMPORTANT — POWER](#important-power) in [§5](#5-installation--quick-start). Mains switching rules: [§4.1](#41-general-requirements). Follow local electrical codes for fusing and grounding.

---

# 5. Installation & Quick Start

<a id="important-power"></a>

> ⚠️ **IMPORTANT — POWER**  
> Module logic/RS-485/inputs: regulated **24 V DC ±10 % SELV/PELV** on V+/0V (or shared 24 V bus, fused per branch). LED strip: a **separate 12 V or 24 V DC LED PSU** on **LED PS** (+/−), within the module's **10 A** LED-rail limit. **LED PS** = module terminal; **LED PSU** = external supply. Keep GND_FUSED (field) and GND (logic/USB) unbridged.

## 5.1 What You Need

| Item | Description |
|------|-------------|
| **Module** | RGB-621-R1 LED control module |
| **Controller** | HomeMaster **MicroPLC** / **MiniPLC** or any **Modbus RTU master** |
| **Module PSU** | Regulated **24 V DC SELV/PELV** — see [⚠️ IMPORTANT — POWER](#important-power) |
| **LED PSU (separate)** | Regulated **12 V or 24 V DC** — see [⚠️ IMPORTANT — POWER](#important-power) |
| **Cables** | 1× **USB-C** cable (for setup), 1× **twisted-pair RS-485** cable |
| **Software** | Any browser that supports the **Web Serial API** (for WebConfig) |
| **Optional** | Shielded wiring for long RS-485 runs, DIN-rail enclosure, terminal labels |

---

## 5.2 Power

Power: see the [⚠️ IMPORTANT — POWER](#important-power) block in [§5](#5-installation--quick-start).

- **LED path (LED PS → COM (LED+)):** the positive rail from the external LED PSU enters through:
  - **PTC fuses** (10 A LED-rail limit)  
  - **Reverse-polarity protection** (Schottky)  
  - **TVS surge suppression**  
  - Then feeds **COM (LED+)** directly — **not** through the onboard relay.

- **PWM outputs (R / G / B / CW / WW):** **low-side PWM sinks** (AP9990GH-HF); strip must be **12/24 V common-anode**. Per-channel current limited by MOSFET and PCB tracks; **sum of all channels ≤ 10 A**.

- **Relay (Relay C / NO):** independent **SPST-NO dry-contact** rated **3 A @ 250 VAC / 30 VDC** (module/PCB limit); suitable for **230 VAC** loads when installed per [§4.1](#41-general-requirements). **Basic insulation** between SELV coil and contacts; external contactor for reinforced isolation or heavy/inductive loads. For **FOLLOW-mode** LED-PSU cut, wire **Relay C / NO** **externally in series** with the LED PSU (+) feed ([Use Case 2](#-use-case-2--relay-as-automatic-led-psu-power-cut-energy-saving)).

- **Current consumption (typical):**
  - Logic + RS-485: ≈ 100 mA  
  - Relay coil: ≈ 30 mA (active)  
  - LED load: dependent on connected strips (size LED PSU within **10 A** module limit)

- **Module input protection (V+ / 0V):** PTC fuses (F1–F4), reverse-polarity diode (STPS340U), surge TVS (SMBJ33A).

---

## 5.3 Communication

**RS-485 Pinout (bottom connector):**

| Terminal | Signal | Description |
|-----------|---------|-------------|
| **A** | RS-485 A (+) | Non-inverting line |
| **B** | RS-485 B (–) | Inverting line |
| **RS-485 COM** | Common reference (optional) | Field ground reference (GND_FUSED) for long bus runs |

- Use a **twisted-pair shielded cable** (e.g., Cat-5 or RS-485 grade).  
  Connect the shield to protective earth (PE) at **one end only**.

- **Network topology:**  
  Daisy-chain (bus) — no star wiring.  
  Fit an external 120 Ω resistor across A/B at each of the two physical bus ends.

- **Default Modbus settings:**  
  - **Address:** 3 (each module on the bus must be unique — change in WebConfig to avoid collisions)  
  - **Baud rate:** 19200 bps  
  - **Data format:** 8 data bits, no parity, 1 stop bit (**8N1**)  

- **Configuration:**  
  - Connect via **USB-C** and open **WebConfig** in any browser that supports the **Web Serial API**.  
  - Set module address, baud rate, and optional relay/input parameters.  
  - Save settings to non-volatile memory.  

- **Ground reference use:**  
  - In most RS-485 systems, differential A/B are sufficient.  
  - The **RS-485 COM** terminal may be connected between devices only if bus transceivers require a shared reference (rare in modern isolated networks).

---

> ⚙️ **Quick Summary**
> 1. Mount the module on a DIN rail.  
> 2. Wire power per [⚠️ IMPORTANT — POWER](#important-power).  
> 3. Connect LED strips (common-anode to **COM (LED+)**, cathodes to R/G/B/CW/WW).  
> 4. Wire RS-485 A/B to the controller.  
> 5. Plug in USB-C, open WebConfig, assign address, set baudrate, test outputs.  
> 6. Verify Modbus communication over RS-485.

---

## 5.4 Installation & Wiring

Diagram-first wiring map. Power details: [§5.2](#52-power). RS-485: [§5.3](#53-communication). Power layout: [⚠️ IMPORTANT — POWER](#important-power).

### Power

![Power supply wiring — module V+/0V and LED PS](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/RGB_PowerSupply.png)
*Wire **V+** / **0V** and **LED PS** (+/−) per [⚠️ IMPORTANT — POWER](#important-power); protection detail in [§5.2](#52-power).*

### LED outputs (5× PWM)

| RGB (3 colour) | RGB + CW |
|:---:|:---:|
| ![RGB LED strip wiring](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/RGB_RGB_Connection.png) | ![RGB + Cool White wiring](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/RGB_RGBCW_Connection.png) |
| *Strip **+** on **COM (LED+)**; cathodes on **R** / **G** / **B**.* | *Adds **CW** for RGB + cool-white mixes.* |

| Tunable white (CWWW) | Full RGBCCT (RGB + CCT) |
|:---:|:---:|
| ![CCT / tunable white wiring](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/RGB_CWWW_Connection.png) | ![Full RGBCCT wiring](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/RGB_RGBCWWW_Connection.png) |
| ***CW** and **WW** only.* | *All five channels — native operating mode.* |

### Digital inputs

![Digital inputs — dry-contact wiring to I1 and I2](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/RGB_DigitalInputs.png)
*Potential-free (dry) contacts between **I1** / **I2** and **GND** — module supplies wetting current from its 24 V rail; do **not** apply external voltage to I1/I2. Three-wire sensors: power from your own supply, switching output to the input only. Shielded cable for runs > 10 m ([§5.2](#52-power) protection: PTC F5/F6, TVS, reverse diodes).*

### Relay

![Relay output — NO and C to external load](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/RGB_RelayConnectioin.png)
*External load on **Relay C** / **NO** — independent of **COM (LED+)**; FOLLOW PSU cut: external series wiring ([Use Case 2](#-use-case-2--relay-as-automatic-led-psu-power-cut-energy-saving), [§5.2](#52-power)).*

### RS-485 (Modbus RTU)

![RS-485 A/B/COM Modbus RTU wiring](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/RGB_RS485Connection.png)
***A** / **B** / optional **RS-485 COM** — daisy-chain to controller; see [§5.3](#53-communication).*

### USB-C

*WebConfig and firmware only — may be connected at any time; not a field power or data bus.*

## 5.5 Software & UI Configuration

Configure over USB-C in a browser with **Web Serial API** support: open [https://config.home-master.eu/RGB-621-R1/Firmware/v0.2.0/ConfigToolPage.html](https://config.home-master.eu/RGB-621-R1/Firmware/v0.2.0/ConfigToolPage.html), click **Connect**, pick the module's port. Changes apply live and save to flash.

### 1) Connection, light levels & presets

<img src="https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/webconfig1.png" width="720">

| Setting | What it does | Set to |
|---|---|---|
| Modbus Address | RS-485 bus ID; unique per module | 1–255 (default 3) |
| Baud Rate | Must match controller/bus | 9600/19200/38400/57600/115200 (default 19200) |
| Serial Log | USB diagnostics stream | — |
| Light Levels (R/G/B/WW/CW) | Live 0–255 sliders; test strip without a controller | any level for commissioning |
| Quick presets | OFF / WHITE / RGB / FULL test patterns | verify wiring |

### 2) Wall-switch inputs & onboard button

<img src="https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/webconfig2.png" width="720">

| Setting | What it does | Set to |
|---|---|---|
| Enabled (per DI) | turns the input on/off | on to use |
| Inverted (per DI) | flips contact sense | as wired |
| Child lock (per DI) | blocks local control (auto-unlocks when Modbus link is offline) | off normally |
| Mode | Momentary (gestures) or Maintained (closed=ON/open=OFF) | per switch |
| Output target | what the input drives | Group RGB / Group CCT / RGB+CCT / Relay1 / All |
| Single / Double / Hold actions | action per gesture | None, Toggle, On, Off, Dim up, Dim down, Dim toggle dir, Relay pulse, Scene 1–4, Identify |
| Onboard button SW2 (Enabled + Single/Double) | usable front button actions | e.g. Double = Identify |
| Engine timings | press detection | Debounce 25 · Double-click 350 · Hold start 650 · Hold repeat 60 ms |
| Allow wall switches when offline | keep local control if the bus drops | on (recommended) |

### 3) PWM channels — Red/Green/Blue/Warm White/Cool White

<img src="https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/webconfig3.png" width="720">

| Setting | What it does | Set to |
|---|---|---|
| Min trim | per-channel floor (fix low-end flicker) | 0–255 (default 1) |
| Max trim | per-channel ceiling (cap brightness) | 0–255 (default 255) |
| Transition, ms | fade smoothing on level change | default 400 |
| Power-on | channel state at power-up | OFF at power-on / ON at power-on / Restore last |

### 4) Dimming & scenes

<img src="https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/webconfig4.png" width="720">

| Setting | What it does | Set to |
|---|---|---|
| Dimming time (ms, full range) | hold ramp 0→100 % | default 3000 |
| Scenes 1–4 (per-channel R/G/B/WW/CW) | stored presets | set levels; **Capture current** saves live values |

### 5) Relay, user LEDs & output quality

<img src="https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/webconfig5.png" width="720">

| Setting | What it does | Set to |
|---|---|---|
| Relay Power-on state | relay position at power-up | OFF at power-on / ON at power-on / Restore last |
| Relay mode | Manual (free output) or Follow (LED-PSU cut) | per use |
| Follow channels | groups watched in Follow mode | RGB only / CCT only / both |
| Off delay (seconds) | delay before cutting after all dark | default 45 |
| User LED1 / LED2 — Mode | how each LED behaves | per preference |
| User LED1 / LED2 — Source | what each LED reflects | per preference |
| Gamma correction (enable) | perceptual dimming curve on/off | on (recommended) |
| Gamma (×10) | curve value | 22 (= 2.2) |

## 5.6 Getting Started

Follow these steps for a first-time install (field wiring detail: [§5.4](#54-installation--wiring); Home Assistant integration: [§7](#7-esphome-integration-guide)).

1. Mount on a **35 mm DIN rail**; wire power per [⚠️ IMPORTANT — POWER](#important-power); connect a **common-anode** strip (strip **+** to **COM (LED+)**, cathodes to **R** / **G** / **B** / **CW** / **WW**).
2. Connect **RS-485** **A** / **B** / **RS-485 COM** to the controller (**MicroPLC** / **MiniPLC**).
3. Plug **USB-C** into a PC; open the **WebConfig** tool in a browser with **Web Serial API** support and click **Connect**.
4. Set a **unique Modbus address** (each module on the bus must differ; default is **3**) and baud **19200**; save to flash.
5. Optionally assign **DI1** / **DI2** wall-switch actions; test the light from WebConfig.
6. Add the [§7 ESPHome](#7-esphome-integration-guide) package to the controller with the matching `rgb_address`.
7. The light and relay appear in **Home Assistant**.

---

# 6. Modbus RTU Communication

The RGB‑621‑R1 communicates as a **Modbus RTU slave** over **RS‑485**. Register map matches `default_rgb_621_r1_plc.yaml` (v0.2.0) and firmware v0.2.0.

**Defaults:** Address **3**, **19200 8N1** (change in WebConfig).

---

## 6.0 Register Map (complete)

Master reference for firmware **v0.2.0** (`MAP_VERSION` **3**). Per-function-code detail in [§6.1](#61-input-registers-fc04--map_version-3)–[§6.3](#63-holding-registers-fc030616). Gesture/scene/trim/relay-mode/gamma configuration is **USB WebConfig only** — not on Modbus.

| Register / Address | Name | FC / Access | Type | Units / Range | Default | Effect | Firmware |
|--------------------|------|-------------|------|---------------|---------|--------|----------|
| **1** | **DI1** | FC02 R | discrete input | 0 / 1 | 0 | Wall-switch logical state (after enable + invert) | v0.2.0 |
| **2** | **DI2** | FC02 R | discrete input | 0 / 1 | 0 | Wall-switch logical state (after enable + invert) | v0.2.0 |
| **60** | **Relay1** | FC02 R | discrete input | 0 / 1 | 0 | Relay logical state | v0.2.0 |
| **90** | **LED1** | FC02 R | discrete input | 0 / 1 | 0 | User LED1 logical state | v0.2.0 |
| **91** | **LED2** | FC02 R | discrete input | 0 / 1 | 0 | User LED2 logical state | v0.2.0 |
| **0** | **DI_STATE_MASK** | FC04 R | input register | bit0..1 | — | DI1..DI2 bitmask | v0.2.0 |
| **1** | **RLY_STATE_MASK** | FC04 R | input register | bit0 | — | Relay1 logical state | v0.2.0 |
| **2** | **BTN_STATE_MASK** | FC04 R | input register | bit0 | — | SW2 onboard button | v0.2.0 |
| **3** | **LED_STATE_MASK** | FC04 R | input register | bit0..1 | — | LED1..LED2 bitmask | v0.2.0 |
| **4** | **STATUS_FLAGS** | FC04 R | input register | bit1 linkOk, bit3 cfgDirty | — | Link / config status | v0.2.0 |
| **6–20** | **EVT_COUNTERS** | FC04 R | input register | uint16 | 0 | Press counters: DI1 / DI2 / SW2 × 5 gestures each | v0.2.0 |
| **21** | **PWM_RAW R** | FC04 R | input register | 0–4095 | 0 | Red channel 12-bit diagnostic current | v0.2.0 |
| **22** | **PWM_RAW G** | FC04 R | input register | 0–4095 | 0 | Green channel 12-bit diagnostic current | v0.2.0 |
| **23** | **PWM_RAW B** | FC04 R | input register | 0–4095 | 0 | Blue channel 12-bit diagnostic current | v0.2.0 |
| **24** | **PWM_RAW WW** | FC04 R | input register | 0–4095 | 0 | Warm-white channel 12-bit diagnostic current | v0.2.0 |
| **25** | **PWM_RAW CW** | FC04 R | input register | 0–4095 | 0 | Cool-white channel 12-bit diagnostic current | v0.2.0 |
| **26** | **STATE RG** | FC04 R | input register | packed | — | Applied R (high byte) + G (low byte), API 0–255 | v0.2.0 |
| **27** | **STATE BWW** | FC04 R | input register | packed | — | Applied B (high byte) + WW (low byte), API 0–255 | v0.2.0 |
| **28** | **STATE CW+flags** | FC04 R | input register | packed | — | Applied CW (high byte); flags: anyOn, rgbGroupOn, cctGroupOn, relay1 | v0.2.0 |
| **0** | **Relay1** | FC01 R / FC05 W | coil | 0 / 1 | 0 | Toggle relay — primary control path | v0.2.0 |
| **5** | **IDENTIFY** | FC05 W | coil (pulse) | write 1 | 0 | LED identify blink | v0.2.0 |
| **6** | **SAVE_CFG** | FC05 W | coil (pulse) | write 1 | 0 | Save config to flash | v0.2.0 |
| **7** | **REBOOT** | FC05 W | coil (pulse) | write 1 | 0 | Reboot module | v0.2.0 |
| **200** | **Relay1 ON** | FC05 W | coil (pulse) | write 1 | 0 | Legacy pulse — energize relay | v0.2.0 |
| **210** | **Relay1 OFF** | FC05 W | coil (pulse) | write 1 | 0 | Legacy pulse — de-energize relay | v0.2.0 |
| **300** | **DI1 Enable** | FC05 W | coil (pulse) | write 1 | 0 | Enable DI1 | v0.2.0 |
| **301** | **DI2 Enable** | FC05 W | coil (pulse) | write 1 | 0 | Enable DI2 | v0.2.0 |
| **320** | **DI1 Disable** | FC05 W | coil (pulse) | write 1 | 0 | Disable DI1 | v0.2.0 |
| **321** | **DI2 Disable** | FC05 W | coil (pulse) | write 1 | 0 | Disable DI2 | v0.2.0 |
| **400** | **R** | FC03 R / FC06 W | holding | 0–255 | 0 | Red PWM setpoint (8-bit API; slew-smoothed) | v0.2.0 |
| **401** | **G** | FC03 R / FC06 W | holding | 0–255 | 0 | Green PWM setpoint | v0.2.0 |
| **402** | **B** | FC03 R / FC06 W | holding | 0–255 | 0 | Blue PWM setpoint | v0.2.0 |
| **403** | **WW** | FC03 R / FC06 W | holding | 0–255 | 0 | Warm-white PWM setpoint | v0.2.0 |
| **404** | **CW** | FC03 R / FC06 W | holding | 0–255 | 0 | Cool-white PWM setpoint | v0.2.0 |
| **410** | **R (12-bit)** | FC03 R / FC06 W | holding | 0–4095 | 0 | Red fine PWM setpoint | v0.2.0 |
| **411** | **G (12-bit)** | FC03 R / FC06 W | holding | 0–4095 | 0 | Green fine PWM setpoint | v0.2.0 |
| **412** | **B (12-bit)** | FC03 R / FC06 W | holding | 0–4095 | 0 | Blue fine PWM setpoint | v0.2.0 |
| **413** | **WW (12-bit)** | FC03 R / FC06 W | holding | 0–4095 | 0 | Warm-white fine PWM setpoint | v0.2.0 |
| **414** | **CW (12-bit)** | FC03 R / FC06 W | holding | 0–4095 | 0 | Cool-white fine PWM setpoint | v0.2.0 |
| **480** | **MB_ADDR** | FC03 R / FC06 W | holding | 1–247 | **3** | Modbus slave address | v0.2.0 |
| **481** | **MB_BAUD** | FC03 R / FC06 W | holding | 9600 / 19200 / 38400 / 57600 / 115200 | **19200** | Modbus baud rate (bps) | v0.2.0 |

---

## 6.1 Input Registers (FC04) — MAP_VERSION 3

> Summary: see [§6.0](#60-register-map-complete). Detail below.

| Address | Name | Bits | Description |
|---------|------|------|-------------|
| **0** | **DI_STATE_MASK** | bit0..1 | DI1..DI2 |
| **1** | **RLY_STATE_MASK** | bit0 | Relay1 logical state |
| **2** | **BTN_STATE_MASK** | bit0 | SW2 onboard button (GPIO1) |
| **3** | **LED_STATE_MASK** | bit0..1 | LED1..LED2 |
| **4** | **STATUS_FLAGS** | bit1 linkOk, bit3 cfgDirty | Link / config status |
| **6–20** | **EVT_COUNTERS** | uint16 | Press counters: 3 sources × 5 gestures |
| **21–25** | **PWM_RAW** | 0–4095 | 12-bit perceived current per channel (diagnostic) |
| **26–28** | **STATE** | packed | Applied PWM levels (API 0–255, after slew) + status flags |

**STATE block (IREG 26–28)** — one FC04 read for HA feedback:

| Address | Format | Description |
|---------|--------|-------------|
| **26** | `(R<<8)\|G` | Applied red (high byte) and green (low byte), API 0–255 |
| **27** | `(B<<8)\|WW` | Applied blue and warm white |
| **28** | `(CW<<8)\|flags` | Applied cold white (high byte); flags low byte: bit0 **anyOn**, bit1 **rgbGroupOn**, bit2 **cctGroupOn**, bit3 **relay1** |

Sources: **0** = DI1, **1** = DI2, **2** = SW2. Gestures per source at `EVT_BASE + source×5 + gesture`.

> ESPHome package reads **FC04 @0 count=5** (DI/LED/status) and **FC04 @26 count=3** (applied light + relay flags) every 5 s. Event counters **6–20** are optional for masters that need register-based press accounting.

---

## 6.2 Coils (FC01/05)

> Summary: see [§6.0](#60-register-map-complete). Detail below.

| Address | Name | Description |
|---------|------|-------------|
| **0** | **Relay1** | Toggle coil — write `1`/`0` (DIO-compatible) |
| **5** | **IDENTIFY** | Pulse — LED identify blink |
| **6** | **SAVE_CFG** | Pulse — save config to flash |
| **7** | **REBOOT** | Pulse — reboot module |
| **200** | **Relay1 ON** | Legacy pulse coil — write `1` to energize |
| **210** | **Relay1 OFF** | Legacy pulse coil — write `1` to de-energize |
| **300–301** | **DI1–DI2 Enable** | Pulse per input |
| **320–321** | **DI1–DI2 Disable** | Pulse per input |

> Relay coil **0** is the primary control path (ESPHome template switch). Pulse coils **200/210** remain for legacy tools. Service coils: write-only from ESPHome (`assumed_state`).

---

## 6.2.1 Discrete Inputs (FC02)

> Summary: see [§6.0](#60-register-map-complete). Detail below.

| Address | Name | Description |
|---------|------|-------------|
| **1–2** | **DI1–DI2** | Wall-switch logical state (after enable + invert) |
| **60** | **Relay1** | Relay logical state |
| **90–91** | **LED1–LED2** | User LED logical state |

---

## 6.3 Holding Registers (FC03/06/16)

> Summary: see [§6.0](#60-register-map-complete). Detail below.

| Address | Name | Range | Description |
|---------|------|-------|-------------|
| 400–404 | **R, G, B, WW, CW** | 0–255 | PWM setpoints (8-bit API; scaled to 12-bit internally; slew-smoothed) |
| 410–414 | **R, G, B, WW, CW (12-bit)** | 0–4095 | Fine-grained PWM setpoints (same targets as HR 400–404) |
| 480 | **MB_ADDR** | 1–255 | Modbus address |
| 481 | **MB_BAUD** | bps | 9600 / 19200 / 38400 / 57600 / 115200 |

> Module configuration (inputs, gestures, dimming, scenes, trim, relay mode, gamma, etc.) is performed via **USB WebConfig only**; it is **not exposed on Modbus**.

---

## 6.4 Output quality (12-bit + gamma + slew)

- **Internal resolution:** 12-bit (0–4095) on all five PWM channels (`analogWriteResolution(12)`).
- **API:** Modbus HR **400–404** and WebConfig accept **0–255**; HR **410–414** accept **0–4095** for finer control. Firmware scales 8-bit writes to 12-bit setpoints.
- **Gamma:** configurable (default γ 2.2) via WebConfig; 4096-entry LUT applied at the final `analogWrite` stage only.
- **Trim:** per-channel min/max applied in perceived space before gamma (WebConfig).
- **Slew:** each channel has `current` and `target`; Modbus, gestures, and scenes update **target** and ramp over per-channel `fadeMs` (default **400 ms**, WebConfig). **Hold-to-dim** uses a separate `dimFullRangeMs` traverse (default **3000 ms**) — continuous, not stepped. `fadeMs = 0` = instant.
- **Diagnostics:** FC04 IREG **21–25** expose raw 12-bit current.

---

## 6.5 Local input logic (v0.2.0)

Wall switches (DI1/DI2) run the full gesture engine (momentary/maintained, hold-to-dim). Onboard **SW2** is a fixed tactile button: momentary **single** and **double** only.

| Input | Default single | Default double | Default hold |
|-------|----------------|----------------|--------------|
| **DI1** | Toggle Group RGB | — | Dim up → Group RGB |
| **DI2** | Toggle Group CCT | — | Dim down → Group CCT |
| **SW2** | Toggle All | Identify | *(not used)* |

**Maintained mode:** contact closed = ON, open = OFF on **`maintTarget`** (independent of momentary gesture targets). Defaults: DI1 → Group RGB, DI2 → Group CCT.

**Hold-to-dim:** after `holdDelayMs` (default **650 ms**), brightness ramps **continuously** toward min (0) or max trim over `dimFullRangeMs` (default **3000 ms**). Direction and target come from each input's **Hold** gesture (Dim up / Dim down / Dim toggle dir + target). Release freezes at the current level (saved for RESTORE_LAST). Group dimming preserves channel ratios within RGB / CCT / RGB+CCT.

**Gesture targets** (WebConfig / `action<<8|target`): **7** = Group RGB (R,G,B), **8** = Group CCT (WW,CW), **10** = RGB+CCT (both groups in lockstep), **1** = Relay1, **9** = All. Per-channel PWM targets remain internal-only.

**Gesture actions:** None, Toggle, On, Off, Dim up, Dim down, **Dim toggle dir** (action **7**), Relay pulse, **Scene 1–4** (actions **10, 12, 13, 14** — target hidden/unused), Identify (**11**, onboard SW2 only). Legacy **Set 100%** (action **8**) maps to None. Legacy action **4** (All off) maps to Off+All. **Relay pulse** is offered only when target is Relay1.

**Scenes:** four presets configured in WebConfig; recalled by the Scene 1–4 gesture actions.

**Relay1 FOLLOW:** when mode=FOLLOW, **Relay C / NO** energizes while any watched group is on; after all outputs reach zero, the contact opens after `offDelayMs` (default 45 s). Wire **Relay C / NO** **externally in series** with the LED PSU (+) for power-cut — the module does not switch **COM (LED+)** internally.

**Child lock:** ignores local gestures; Modbus/HA control still works.

**Press counters:** IREG 6–20 increment per gesture so masters never miss events between polls.

---

## 6.6 Register Use Examples

| Operation | Write / Read |
|-----------|--------------|
| Set red to 128 | Holding **400** ← 128 |
| Toggle relay | Coil **0** ← 1/0 (or ESPHome switch) |
| Read DI2 | FC04 IR **0**, bitmask `0x0002` |
| Read button state | FC04 IR **2**, bitmask `0x0001` |
| Read relay state | FC04 STATE **28**, flags bit3 *(or legacy IR **1**)* |
| Read applied light levels | FC04 STATE **26–28** (one contiguous read) |
| Enable DI1 | Coil **300** ← 1 (pulse) |

| Read press counters (DI1 singles) | FC04 IR **6** |

---

## 6.7 Polling Recommendations

- **DI / LED / status:** one FC04 read **0..4** every 5 s (ESPHome package default)  
- **Applied PWM + relay flags:** one FC04 read **26..28** every 5 s (STATE block; drives HA light/relay feedback)  
- **PWM holding 400–404:** write via Light only; do not poll HR from ESPHome  
- **Relay/service coils:** write only on demand  
- **HA light transitions:** ESPHome package sets `default_transition_length: 0s` so HA sends one final value per channel; smooth crossfade is done on-module via per-channel **fadeMs** (WebConfig, default 400 ms). HA’s transition slider does not apply to this entity.  
- **HA light readback:** STATE@26–28 updates HA display via publish-only `remote_values` (4 s HA-command gate) — never re-writes HR 400–404. Template holders have no lambda so `publish_state` values persist.

---

# 7. ESPHome Integration Guide

Add this package to your **MicroPLC** or **MiniPLC** ESPHome configuration. Set `rgb_address` to the Modbus ID configured in WebConfig (default **3**; each module on the bus must be unique).

```yaml
packages:
  rgb1:
    url: https://github.com/isystemsautomation/homemaster-dev
    ref: main
    files:
      - path: RGB-621-R1/Firmware/v0.2.0/default_rgb_621_r1_plc/default_rgb_621_r1_plc.yaml
        vars:
          rgb_prefix: "RGB#1"
          rgb_id: rgb_1
          rgb_address: 3
```

The package exposes an **RGB+CCT light**, **relay switch**, **digital inputs**, and **button** entities in Home Assistant. UART and Modbus are configured on the controller; poll intervals and STATE readback match firmware v0.2.0 (see [§6](#6-modbus-rtu-communication)).

---

# 8. Firmware & Programming

## 8.1 Updating firmware (regular users)

No IDE or build tools required — RP2350 uses **UF2 drag-and-drop**.

1. Download the latest RGB-621-R1 firmware `.uf2` — module card on [config.home-master.eu](https://config.home-master.eu) (Firmware → UF2) or the repository.
2. **Enter BOOT mode:** hold **both** front buttons (Button 1 + Button 2) and, keeping them held, power-cycle the module (or use **Reset** in WebConfig). Release after power returns. The module mounts as a USB drive named **RPI-RP2**.
3. Drag-and-drop the `.uf2` onto the **RPI-RP2** drive. The module flashes and reboots automatically.
4. Settings (Modbus address/baud, trims, scenes, input mappings, relay mode, gamma) are stored in flash and preserved across updates.
5. **Recovery:** if it doesn't enumerate, power-cycle and retry BOOT.

![Button 1 and Button 2 positions](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/buttons1.png)

## 8.2 Building & flashing from source (advanced / developers)

For modifying or rebuilding the firmware.

- **Languages:** Arduino (RP2350 core), C/C++ (Pico SDK / PlatformIO), MicroPython.
- **Toolchain:** PlatformIO or Arduino IDE.
  - Board/MCU: Raspberry Pi RP2350 / Generic RP235x
  - USB upload: Serial (CDC)
  - Flash layout (Arduino): e.g. 2 MB (Sketch 1 MB / FS 1 MB)
  - Libraries (Arduino examples): ModbusSerial, Arduino_JSON, LittleFS, SimpleWebSerial.
- Build the sketch (see the [reproducible build environment](../../README.md#build-environment-reproducible) and [`sketch.yaml`](Firmware/v0.2.0/default_rgb_621_r1/sketch.yaml)), enter BOOT mode ([§8.1](#81-updating-firmware-regular-users) step 2), then upload the new build over USB, **or** export a `.uf2` and drag-drop it onto **RPI-RP2**.
- **Reset:** power-cycle the module (remove and re-apply **24 V DC**) or use **Reset** in WebConfig — there is no reset button combo.

---

# 9. Maintenance & Troubleshooting

## 9.1 Status LEDs (front panel)

| Indicator | Driven by | Meaning | Modbus |
|---|---|---|---|
| PWR | supply rail | Module 24 V DC supply present | — |
| TX | MAX485 | RS-485 transmit activity | — |
| RX | MAX485 | RS-485 receive activity | — |
| I.1 | input circuit | Digital input 1 state | discrete **1** / IREG DI mask bit 0 |
| I.2 | input circuit | Digital input 2 state | discrete **2** / IREG DI mask bit 1 |
| O.1 | relay circuit | Relay output 1 state | discrete **60** |
| LED1 | MCU (GPIO2) | User/status LED; Mode + Source set in WebConfig | discrete **90** / IREG LED mask bit 0 |
| LED2 | MCU (GPIO3) | User/status LED; Mode + Source set in WebConfig | discrete **91** / IREG LED mask bit 1 |

> ⚠️ On this hardware revision the **I.1** and **I.2** indicators are swapped (**I.1** shows DI2's state and vice versa). Logical inputs, Modbus registers and WebConfig are unaffected — see [Hardware notes](#hardware-notes-current-revision).

> There is no **RUN** or **ERR** indicator on this module.

## 9.2 Resets & Modes

- **BOOT mode** and **reset:** see [§8.1](#81-updating-firmware-regular-users).

## 9.3 Common Issues

- **No communication (TX/RX dark):**  
  Check A/B polarity, external 120 Ω termination at both bus ends (not on module), baud/ID match, and shared **RS-485 COM** reference if separate PSUs.
- **Relay won’t trigger:**  
  Confirm Modbus control vs. local override mode, verify coil/state in WebConfig, and ensure external wiring is on **Relay C / NO** (dry contact). Add snubber for inductive loads.
- **LED channels do not light:**  
  Verify **COM (LED+)** to strip, channel cathodes on **R/G/B/CW/WW**, correct polarity, and adequate **12/24 V** LED PSU sizing (≤ **10 A** total through module).
- **Inputs not detected:**  
  Wire the contact between **I1/I2** and the **GND** terminal of the **DI 24Vdc** block (not **0V** of the power input, not the LED **COM**, not the **RS-485 COM**). Use a potential-free contact — do not apply external voltage. In WebConfig check the input is **Enabled**, **Inverted** is off, and **Debounce** (default 25 ms) is not set too high.
- **USB not detected:**  
  Use a data-capable USB-C cable; close any app holding the port; re-enter [BOOT mode](#81-updating-firmware-regular-users).

---

# 10. Open Source & Licensing

Licensing

This project uses a hybrid licensing model.

Hardware

Hardware designs (schematics, PCB layouts, BOMs) are licensed under:
CERN-OHL-W v2

Firmware & ESPHome Integration

All firmware, ESPHome configurations, and software components are licensed under:
MIT License

This ensures full compatibility with ESPHome and Home Assistant while protecting hardware designs.

See LICENSE files in each directory for full terms.

---

# 11. Downloads

- **Repository (module path):**  
  [`RGB-621-R1` on GitHub](https://github.com/isystemsautomation/homemaster-dev/tree/main/RGB-621-R1)
- **Firmware & examples:** `RGB-621-R1/Firmware/`
- **WebConfig (HTML page):** `RGB-621-R1/Firmware/v0.2.0/ConfigToolPage.html`
- **Schematics (PDF):** `RGB-621-R1/Schematics/`
- **Datasheet & docs:** `RGB-621-R1/Manuals/`
- **Images & diagrams:** `RGB-621-R1/Images/`

---

# 12. Support

- **Official Support:** https://www.home-master.eu/support  
- **WebConfig Tool (RGB-621-R1):** https://config.home-master.eu/RGB-621-R1/Firmware/v0.2.0/ConfigToolPage.html  
- **YouTube:** https://youtube.com/@HomeMaster  
- **Hackster:** https://hackster.io/homemaster  
- **Reddit:** https://reddit.com/r/HomeMaster  
- **Instagram:** https://instagram.com/home_master.eu

## Compliance & Certifications

The RGB-621-R1 module is CE marked. **ISYSTEMS AUTOMATION S.R.L.** (HomeMaster® brand)
maintains the technical documentation and a signed EU Declaration of Conformity (DoC).

### Applicable EU directives

- **EMC Directive 2014/30/EU** — EN 55032:2015 + AC:2016-07 + A11:2020 + A1:2020 (Class B emissions),
  EN 55035:2017 + A11:2020 (immunity); tested by Idvorsky Laboratories Ltd., Belgrade, Serbia
  (Job #1648, 20 April 2026)
- **Low Voltage Directive 2014/35/EU** — EN 62368-1:2020 + A11:2020; in-house dielectric and isolation
  test by ISYSTEMS AUTOMATION S.R.L. internal compliance laboratory
- **RoHS Directive 2011/65/EU** — EN IEC 63000 technical documentation

### Compliance documents

| Document | File |
|---|---|
| EU Declaration of Conformity (DoC) | [DoC-RGB-621-R1-V1.0.pdf](./Manuals/DoC-RGB-621-R1-V1.0.pdf) |
| Datasheet | [RGB-621-R1_Datasheet.pdf](./Manuals/RGB-621-R1_Datasheet.pdf) |

### Trademark

**HomeMaster®** is a registered European Union trademark of ISYSTEMS AUTOMATION S.R.L.,
EUTM No. 019082911, registered with EUIPO on 15 January 2025.

---

**Manufacturer:** ISYSTEMS AUTOMATION S.R.L. (HomeMaster® brand)
**Registered office (registered office):** Str. Domnisori, Nr. 81, Bl. 62, Scara A, Etaj 3, Ap. 12, 100284 Ploiesti, Jud. Prahova, Romania
**Office / Contact address:** Diligentei 18, Ploiesti, Romania
**CUI / VAT:** RO 21537032
**EUID:** ROONRC.J2007000919293
**Telephone:** +40 747 757 798
**Website:** [https://www.home-master.eu](https://www.home-master.eu)
