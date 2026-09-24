![Firmware Version](https://img.shields.io/badge/Firmware-v0.1.0-blue)
![Modbus](https://img.shields.io/badge/Protocol-Modbus%20RTU-brightgreen)
![License](https://img.shields.io/badge/License-MIT%20%2F%20CERN--OHL--W-blue)

# STR-3221-R1 — Module for Smart Lighting & I/O Control

**HOMEMASTER – Modular control. Custom logic.**

![Image](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/STR-3221-R1/Images/photo1.png)

**Document map:** [§1 Overview](#1-overview) · [§3 Specifications](#3-specifications) · [§4 Hardware](#4-hardware--interface) · [§5 Getting Started](#5-getting-started) · [§6 WebConfig](#6-webconfig-reference) · [§7 Modbus map](#7-modbus-register-map) · [§8 ESPHome](#8-esphome--home-assistant-integration) · [§9 Programming](#9-programming--build) · [§11 Downloads](#11-downloads--resources)

---

## 1. Overview

The **STR-3221-R1** is a **32-channel** low-side MOSFET output module for stair, architectural and
accent lighting, and for underfloor-heating actuators. Each of the thirty-two channels is
independently dimmable **0–255** over Modbus. It mounts on a **35 mm DIN rail** and connects to a
**MiniPLC/MicroPLC** (or any Modbus RTU master) over **RS-485**, with Home Assistant integration
via ESPHome packages.

**Key capabilities at a glance:**

- **32 independently dimmable low-side MOSFET channels** (**AO4882**) — 12–24 V DC loads, ≤1.5 A per channel, ≤18 A module total; nine groups, each with its own **+** rail: top row 4+4+2, bottom row 4+4+4+4+4+2
- **3 digital inputs** — **1 × IEC 61131-2 module-wetted 24 V discrete input** (**ISO1212**, galvanically isolated) and **2 × opto-isolated presence-sensor inputs** (**SFH6156**, 5.3 kV isolation)
- **2 fused +5 V sensor rails (SENS.A / SENS.B)** — supply for low-current PIR / presence sensors
- **4 buttons** — their pressed state is published on Modbus; they also form the on-board key combinations for USB firmware update (BOOT) and reset
- **2 configurable status LEDs** — Steady/Blink, source selectable in WebConfig
- **Driverless WebConfig** — USB-C + any Chromium-based browser (Chrome, Edge, Opera, Brave, Vivaldi; Chrome/Edge 89+, Opera 76+). No app or login required.
- **Persistent settings** — configuration stored in LittleFS flash and restored on boot

### Where the logic runs

This module is an **output module with reporting inputs**. It executes what it is told: a channel
holds the level last written to it, and the three inputs and four buttons are published on Modbus
for a controller to read.

Sequencing — stair animations, timed fades, scenes, direction of travel — is performed by the
**MiniPLC/MicroPLC or any Modbus master**, not on the module. The module has no sequencer, no
scene table and no timer. If the bus stops, the outputs hold their last commanded level until a
new command arrives or the module is power-cycled.

## Key advantages

- **32 dimmable channels in one 9-module DIN package**, with 3 inputs and 4 buttons published on Modbus — a focused product for stair, architectural and zone-actuator control.
- Native ESPHome API via the MiniPLC/MicroPLC controller — no MQTT broker, no manual Modbus register mapping for the package entities.
- Every channel is a **dimmable light** in Home Assistant, not an on/off switch.
- Open hardware (**CERN-OHL-W v2**) and firmware (**MIT**) — repairable, reproducible, no vendor lock-in.
- Standard **RS-485 Modbus RTU** — works with any Modbus master or industrial HMI/SCADA system, not locked to HomeMaster.
- Driverless **USB-C WebConfig** (Chrome, Edge, Opera); configuration persists in on-device flash (**LittleFS**).

---

## 2. Features

| Subsystem | Qty | Description |
|------------------:|----:|-------------|
| **MOSFET Outputs** | 32 | Low-side **AO4882** dual N-channel MOSFET stages on FieldBoard (**O1…O32**), 12–24 V loads; nine groups, each with its own **+** rail: top row 4+4+2, bottom row 4+4+4+4+4+2. Independently dimmable 0–255. |
| **PWM Drivers** | 4 | **TLC59208F** on MCU board (U9–U12): I²C PWM drivers, **8 channels each**, generating the level for all **32 main outputs** via the FieldBoard output stages. |
| **Digital Inputs** | 3 | **1 × IEC 61131-2 module-wetted 24 V DC discrete input** (**Gnd** + **I**, terminals 8–9, **ISO1212**, galvanically isolated) plus **2 × opto-isolated presence-sensor inputs** (**IN1**/**IN2**, terminals 10–15, **SFH6156** (IN1 = U18, IN2 = U17), 5.3 kV) |
| **Buttons** | 4 | SW1–SW4. Pressed state published on discrete inputs 20–23; also used for the BOOT and reset key combinations. |
| **Status LEDs** | 2 | On-board indicators (GPIO9 / GPIO8), user-assignable steady or blink; mirrored on discrete inputs 90–91. |
| **Sensor Rails** | 2 | Fused **+5 V** SENS.A / SENS.B rails (**F9**/**F10** PTC) for presence-sensor power only. |
| **Modbus RTU** | Yes | RS-485 via **MAX485** transceiver, **not** galvanically isolated; activity LEDs. |
| **USB-C** | Yes | **WebConfig over Web Serial** (Chromium-based: Chrome, Edge, Opera, Brave, Vivaldi; Chrome/Edge 89+, Opera 76+); ESD-protected port. |
| **Power** | 24 VDC | Reverse/surge-protected input; **AP64501** buck → **5 V**, **AMS1117-3.3** LDO → **3.3 V** logic. |
| **MCU** | RP2350 + **W25Q32** | Dual-core MCU with external QSPI flash for firmware/config. |
| **Protection** | TVS, PTC | Surge/ESD and resettable fuses across field & comms lines. |

### Applications

- Stair and walkway lighting, one channel per step, sequenced by the controller
- Architectural, cove and accent lighting with per-channel dimming
- Underfloor-heating manifolds — up to 32 zones of 24 V DC thermoelectric actuators
- Cabinet, shelf and display lighting split into many small zones
- Any installation needing a high channel count of dimmable low-side DC outputs on one DIN module

#### Example — stair lighting driven by motion sensors

The sequence runs on the controller; the module supplies the inputs and drives the steps.

1. Connect motion sensors to **IN1 (bottom)** and **IN2 (top)** terminals, powered from **SENS.A** / **SENS.B**.
2. Connect each stair LED segment to outputs **O1–O32** (low-side switching).
3. Set the module's **Modbus address** (and related options) in **WebConfig**.
4. Program the MicroPLC/MiniPLC to poll **IN1/IN2** and write the **O1…O32** levels in a timed sequence. Which sensor fired first tells the controller the direction of travel.

---

## 3. Specifications

### 3.1 I/O summary

| Interface | Qty | Description |
|-----------:|----:|-------------|
| **Digital Inputs** | 3 | **1 × module-wetted 24 V DC discrete input** (**Gnd** + **I**, **ISO1212**, F7) plus **2 × opto-isolated presence inputs** (**IN1**/**IN2**, **SFH6156** (IN1 = U18, IN2 = U17), **SMAJ6.8CA** clamp) |
| **Outputs** | 32 | Low-side **AO4882** N-channel MOSFET stages, nine groups, each with its own **+** rail: top row 4+4+2, bottom row 4+4+4+4+4+2; PWM from MCU-board **TLC59208F** drivers. |
| **Buttons** | 4 | SW1–SW4; pressed state published on Modbus. |
| **Status LEDs** | 2 | On-board indicators, assignable to a logic state; steady or blink. |
| **RS-485 (Modbus RTU)** | 1 | Communication bus; **A/B/COM** terminals. |
| **USB-C (Setup Port)** | 1 | WebConfig / firmware interface (not for powering field devices). |
| **Power Input** | 1 | **24 VDC (V+, 0V)**; reverse and surge-protected; onboard 5 V / 3.3 V regulation. |
| **Sensor Rails (SENS.A / SENS.B)** | 2 pairs | Fused **+5 V** auxiliary rails (**F9**/**F10** **1206L150THWR** PTC) for presence-sensor power only. |

### 3.2 Electrical ratings

| Parameter | Min | Typ | Max | Unit | Notes |
|------------|----:|----:|----:|------|-------|
| **Supply Voltage (V+)** | 20 | 24 | 30 | VDC | SELV input; reverse/surge protected. |
| **Logic Rails** | — | 5 / 3.3 | — | VDC | Generated internally (buck + LDO). |
| **Quiescent Current (no load)** | — | 60 | 100 | mA | Base electronics only. |
| **Full-Load Current (all outputs)** | — | — | 18 | A | Module total, shared across all channels in use. Per channel ≤1.5 A. |
| **Digital Input (DI only)** | — | — | — | — | Module-wetted dry contact via ISO1212 (terminals 8–9). Do not apply external voltage. |
| **Sensor Rail Output (SENS.A / SENS.B)** | — | 5 | — | VDC | **+5 V** via **F9**/**F10** PTC (**1206L150THWR**). For sensor power only. |
| **Output Type** | — | — | — | — | Low-side **AO4882** dual N-MOSFET; **≤1.5 A** per channel; **≤18 A** module total. |
| **Output Protection** | — | — | — | — | Gate RC + ferrite per channel (FieldBoard schematic); inductive LED wiring per installation practice. |
| **Communication** | — | — | — | — | RS-485 (**MAX485**); 9600, 19200, 38400, 57600 or 115200 bps. |
| **Input Front-Ends** | — | — | — | — | **DI:** **ISO1212** (module-wetted 24 V, galvanically isolated). **IN1/IN2:** **SFH6156** opto-isolated presence inputs, 5.3 kV. |

> ⚙️ **Design domains:**
> - Field side: 24 VDC (DI, outputs); **+5 V** fused SENS.A / SENS.B for presence sensors.
> - Logic side: 5 V / 3.3 V MCU, I²C bus, USB-C protected.
> - Communication side: RS-485 transient protection (TVS + PTC) — **not galvanically isolated**; see [RS-485 / Modbus RTU](#rs-485--modbus-rtu).

### 3.3 Mechanical & environmental

| Parameter | Value |
|---|---|
| Mounting | 35 mm DIN rail (EN 50022) |
| DIN width | 9 modules (≈ 158 mm) |
| Dimensions | 158 × 90.6 × 67.3 mm (L × W × H) |
| Terminals | Pluggable screw terminal blocks, 5.08 mm pitch; 0.25–1.5 mm² conductors |
| Operating temperature | 0 … +40 °C |
| Humidity | 95 % RH non-condensing |
| Ingress protection | IP20 — mount inside an enclosure |

![STR-3221-R1 mechanical drawing](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/STR-3221-R1/Images/STR-3221-R1%20Dimensions.png)

*Mechanical drawing: front and side view, dimensions in mm*

### 3.4 Communication defaults

| Item | Value |
|---|---|
| Terminal order on this module | **COM (5) – B (6) – A (7)** — read the silkscreen, the order differs across the HomeMaster range |
| Factory default address | `3` |
| Factory default baud | `19200`, 8N1 |
| Supported baud rates | 9600, 19200, 38400, 57600, 115200 |
| Address range | 1–247 |
| Termination | 120 Ω at the two physical ends of the bus only |

A module straight out of the box answers at address **3**, **19200 baud** — set a unique address
before putting a second module on the same bus. Address and baud are set in **WebConfig** over
USB-C, or over Modbus at **HR 480** (address) and **HR 481** (baud); see the caveat on HR 481 in
[§7.4](#74-holding-registers--fc-03--06--16-readwrite).

### 3.5 Reliability & protection

| Area | Provision |
|---|---|
| Digital input **DI** | Galvanic isolation (**ISO1212**); PTC fuse (**F7**, **1206L016WR**), TVS, reverse protection |
| Presence inputs **IN1/IN2** | Opto-isolation (**SFH6156**, 5.3 kV); **SMAJ6.8CA** TVS clamps |
| Sensor rails | Resettable PTC per rail (**F9**/**F10**) |
| Outputs **O1…O32** | Gate RC + ferrite per channel (**BLM31PG601SN1L**); **not** isolated from logic ground |
| Power input | Reverse-polarity protection, TVS surge suppression, EMI filtering, 1 A fuse (F8) |
| RS-485 | TVS, PTC, common-mode choke, fail-safe biasing — transient protection, **not** isolation |
| Configuration | Stored in LittleFS with a CRC; survives power loss |
| Watchdog | 4 s hardware watchdog; the module reboots itself if the main loop stalls |
| TLC59208F recovery | If the I²C drivers do not answer at boot the module retries every 5 s, with a full bus scan every 30 s, and reports status over WebConfig |

#### What is isolated and what is not

| Interface | Isolated? |
|---|---|
| **DI** (24 V discrete input) | **Yes** — **ISO1212** isolated digital-input receiver |
| **IN1 / IN2** (presence inputs) | **Yes** — **SFH6156** optocouplers, 5.3 kV |
| **O1…O32** (MOSFET outputs) | **No** — low-side switches referenced to field ground |
| **RS-485 A/B/COM** | **No** — the **MAX485** shares the device's logic ground |
| **SENS.A / SENS.B** supply rails | **No** — derived from the module's own supply |

Note that the input barriers only buy separation if the sensor is powered from its **own** supply.
A PIR powered from the module's **SENS** rail shares the module's ground by way of that rail, so
there is no galvanic separation between module and sensor in that arrangement — which is normal
and safe for a SELV presence sensor, but it is not isolation.

### RS-485 / Modbus RTU

<!-- hm:rs485-order:begin -->
> **Terminal order differs across the HomeMaster range.**
> Always read the silkscreen - do not wire by habit from another module.
> On this module the order is **COM-B-A**.
> Swapping A and B damages nothing but the node will not communicate.
> COM is required on every node.
<!-- hm:rs485-order:end -->

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
- Where the bus crosses into a different electrical installation with its own earthing reference — a utility or billing meter, another building, another cabinet's PE system — fit an external galvanic RS-485 isolator at that boundary. The on-board components are transient protection, not isolation, and will not survive a sustained ground-potential difference.

---

## 4. Hardware & Interface

### 4.1 Diagrams & pinouts

| Diagrams & Descriptions |
|--------------------------|
| ![System Diagram](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/STR-3221-R1/Images/STR_SystemBlockDiagram_New.png)<br>**System Block Diagram** — MCU, Modbus interface, power chain, and I/O groups. |
| ![FieldBoard Layout](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/STR-3221-R1/Images/FieldBoard_Diagram.png)**FieldBoard Layout** — 32 **AO4882** low-side outputs, **ISO1212** DI, **SFH6156** presence inputs, fused **+5 V** SENS rails. |
| ![MCUBoard Layout](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/STR-3221-R1/Images/MCUBoard_Diagram.png)**MCU Board Layout** — RP2350 MCU, TLC59208F drivers, MAX485, and USB-C. |
| ![Terminal Map](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/STR-3221-R1/Images/STR_MCU_Pinouts.png)**PinOut** — Field wiring view with power, DI, outputs, and RS-485. |

### 4.2 Connectors & terminal map

<!-- hm:terminal-map:begin -->

**Top row** (24Vdc | LED PS | RS-485 | DI 24Vdc | SENS.A | SENS.B | OUT O.1-O.10)

| Pos | Label | Group | Function |
|-----|-------|-------|----------|
| 1 | V+ | POWER | 24 V DC input |
| 2 | 0V | POWER | 24 V DC return |
| 3 | + | LED_PS | LED PSU positive |
| 4 | - | LED_PS | LED PSU negative |
| 5 | COM | RS485 | RS-485 signal reference |
| 6 | B | RS485 | RS-485 data - |
| 7 | A | RS485 | RS-485 data + |
| 8 | Gnd | DI | Discrete input return |
| 9 | I | DI | Discrete input |
| 10 | + | SENS_A | Presence sensor A supply |
| 11 | IN1 | SENS_A | Presence input A |
| 12 | Gnd | SENS_A | Presence A return |
| 13 | + | SENS_B | Presence sensor B supply |
| 14 | IN2 | SENS_B | Presence input B |
| 15 | Gnd | SENS_B | Presence B return |
| 16 | + | OUT_1_4 | Group rail for O.1-O.4 |
| 17 | O.1 | OUT_1_4 | Output 1 |
| 18 | O.2 | OUT_1_4 | Output 2 |
| 19 | O.3 | OUT_1_4 | Output 3 |
| 20 | O.4 | OUT_1_4 | Output 4 |
| 21 | + | OUT_5_8 | Group rail for O.5-O.8 |
| 22 | O.5 | OUT_5_8 | Output 5 |
| 23 | O.6 | OUT_5_8 | Output 6 |
| 24 | O.7 | OUT_5_8 | Output 7 |
| 25 | O.8 | OUT_5_8 | Output 8 |
| 26 | + | OUT_9_10 | Group rail for O.9-O.10 |
| 27 | O.9 | OUT_9_10 | Output 9 |
| 28 | O.10 | OUT_9_10 | Output 10 |

**Bottom row** (OUT O.11-O.32)

| Pos | Label | Group | Function |
|-----|-------|-------|----------|
| 1 | + | OUT_11_14 | Group rail for O.11-O.14 |
| 2 | O.11 | OUT_11_14 | Output 11 |
| 3 | O.12 | OUT_11_14 | Output 12 |
| 4 | O.13 | OUT_11_14 | Output 13 |
| 5 | O.14 | OUT_11_14 | Output 14 |
| 6 | + | OUT_15_18 | Group rail for O.15-O.18 |
| 7 | O.15 | OUT_15_18 | Output 15 |
| 8 | O.16 | OUT_15_18 | Output 16 |
| 9 | O.17 | OUT_15_18 | Output 17 |
| 10 | O.18 | OUT_15_18 | Output 18 |
| 11 | + | OUT_19_22 | Group rail for O.19-O.22 |
| 12 | O.19 | OUT_19_22 | Output 19 |
| 13 | O.20 | OUT_19_22 | Output 20 |
| 14 | O.21 | OUT_19_22 | Output 21 |
| 15 | O.22 | OUT_19_22 | Output 22 |
| 16 | + | OUT_23_26 | Group rail for O.23-O.26 |
| 17 | O.23 | OUT_23_26 | Output 23 |
| 18 | O.24 | OUT_23_26 | Output 24 |
| 19 | O.25 | OUT_23_26 | Output 25 |
| 20 | O.26 | OUT_23_26 | Output 26 |
| 21 | + | OUT_27_30 | Group rail for O.27-O.30 |
| 22 | O.27 | OUT_27_30 | Output 27 |
| 23 | O.28 | OUT_27_30 | Output 28 |
| 24 | O.29 | OUT_27_30 | Output 29 |
| 25 | O.30 | OUT_27_30 | Output 30 |
| 26 | + | OUT_31_32 | Group rail for O.31-O.32 |
| 27 | O.31 | OUT_31_32 | Output 31 |
| 28 | O.32 | OUT_31_32 | Output 32 |

**Ports & service interfaces**

| Id | Type | Note |
|----|------|------|
| USB-C |  | WebConfig / firmware interface (not for powering field devices). |

**Housing notes**

- Outputs are nine groups, each with its own + rail: top row 4+4+2, bottom row 4+4+4+4+4+2
- Low-side switching: load + to the group + rail, load - to O.n.
- Two separate presence inputs SENS.A and SENS.B, each with its own + and Gnd.
- Output path: per channel ≤1.5 A (ferrite BLM31PG601SN1L); module total ≤18 A.

<!-- hm:terminal-map:end -->

### 4.3 Front panel — buttons & LEDs

| Control | Function in firmware v0.1.0 |
|---|---|
| **SW1 – SW4** | Pressed state is published on discrete inputs 20–23 for the controller to read. The buttons perform **no local action** on the module itself. |
| **Status LED1 / LED2** | Assignable in WebConfig: source plus steady or blink. Physical state published on discrete inputs 90–91. |
| **PWR / TX / RX** | Fixed indicators — power present, and Modbus traffic on RS-485. |

**Key combinations** (these work at the hardware level, independently of firmware logic):

| Function | Combination | Behavior |
|-----------|--------------|-----------|
| **BOOT Mode** | **Buttons 1 + 2** | Forces the module into flash/bootloader mode |
| **Hardware Reset** | **Buttons 3 + 4** | Restarts the MCU without clearing configuration |
| **Factory Reset** | **All Buttons 1–4** on power-up | Clears configuration (returns to address 3, 19200 baud) |

---

## 5. Getting Started

### 5.1 Safety *(read before wiring)*

The **STR-3221-R1** is a **SELV (Safety Extra-Low Voltage)** device.
Improper wiring, power application, or grounding may cause malfunction or damage.

| Requirement | Detail |
|--------------|--------|
| **Qualified Personnel** | Only trained technicians familiar with control panels, PLCs, and SELV wiring should install or service this module. |
| **Power Isolation** | Always disconnect **24 VDC** power and RS-485 trunk before touching or rewiring terminals. |
| **Rated Voltages Only** | Use **SELV 24 VDC** power supplies; never connect AC mains or high-voltage lines. |
| **Grounding** | Properly bond the panel's protective earth (PE) to reduce EMI and static discharge. |
| **Enclosure** | Mount in a **clean, dry, ventilated enclosure**; avoid moisture, conductive dust, or vibration. |
| **Static Protection** | Handle circuit boards only with ESD precautions (grounded strap and antistatic mat). |

#### Installation practices

- **DIN Mounting:** mount securely on **35 mm DIN rail (EN 50022)** using the rear clip. Apply strain relief on all connected cables to prevent terminal stress. **DIN width: 9 modules (≈ 158 mm).**
- **Power domains:** the module uses separate power domains — **Field Power (24 VDC_FUSED)** for outputs and inputs, **Logic Power (5 V / 3.3 V)** for the MCU. Never short or bridge **GND_FUSED** (field ground) with **logic ground** unless specifically required by system design.
- **Sensor Power Connection:** power low-current PIR / presence sensors only from the fused **SENS.A** / **SENS.B** rails (**+5 V**, **F9**/**F10**). Check the sensor is rated for a 5 V supply before wiring it. The **DI** input (terminals 8–9) is a separate **module-wetted 24 V** channel — do **not** backfeed or parallel SENS rails with other supplies.
- **Wiring Discipline:** use ferruled, properly sized conductors (0.25–1.5 mm²). Route communication (RS-485) and power lines separately to reduce noise coupling.
- **Testing Before Power-Up:** verify all terminal polarities, check RS-485 A/B orientation, and confirm no shorts between supply rails.

#### I/O & interface warnings

**Power (24 VDC input / LED supply)**

| Area | Warning |
|-------|----------|
| **24 VDC Power (V+ / 0V)** | Use only clean, regulated SELV 24 VDC. Reverse polarity is protected but repeated mistakes may damage fuses. |
| **LED PS (+/–)** | Provides the external LED load voltage (typically 12–24 VDC). Do not short or exceed rated current capacity of field wiring. |
| **Sensor Rails (SENS.A / SENS.B)** | For **+5 V** presence-sensor power only (**F9**/**F10** PTC). Never use to drive LED loads or feed back external power sources. |

**Digital input — module-wetted 24 V (DI)**

| Area | Warning |
|-------|----------|
| **Input Type** | Terminals **8** (**Gnd**) and **9** (**I**) form one **module-wetted dry-contact** input via **ISO1212**. No AC or high-voltage inputs. |
| **Wiring** | Close a potential-free contact between **Gnd** (8) and **I** (9). **Do not** apply external voltage. |
| **Protection** | PTC/TVS protected (**F7**, **1206L016WR**). Replace fuses only with identical PTC parts. |

**Presence-sensor inputs (IN1, IN2)**

| Area | Warning |
|-------|----------|
| **Input Type** | **IN1** / **IN2** (terminals 11, 14) are **opto-isolated** via **SFH6156** (IN1 = U18, IN2 = U17); accept open-collector or dry-contact sensor outputs. |
| **Sensor Power** | Power sensors from **SENS.A** (+) / **SENS.B** (+) (**+5 V**, terminals 10, 13) with return to matching **Gnd** (terminals 12, 15). |
| **Protection** | **SMAJ6.8CA** TVS clamps on presence input lines. |

**Outputs (O1…O32)**

| Area | Warning |
|-------|----------|
| **Output Type** | **Low-side AO4882** N-MOSFET sinks; maximum load per channel **1.5 A** (12–24 VDC); module total **≤18 A**. |
| **Polarity** | Connect load +V to **+ group rail**, load – to output terminal (O#). |
| **Inductive Loads** | Primarily LED/resistive loads; for large inductive loads add external RC or TVS snubbers. |
| **Shared Rail** | Each **4-channel** group shares a **+** rail (nine groups) — ensure consistent LED supply voltage. |
| **Isolation** | Outputs are **not** isolated from the module's field ground. |

**USB-C (service / WebConfig)**

| Area | Warning |
|-------|----------|
| **Purpose** | For setup, diagnostics, and firmware only. Not for powering sensors or external devices. |
| **Connection** | Connect to PC via isolated USB hub if the RS-485 bus is long or exposed. |
| **During Operation** | Disconnect USB-C when running in the field; avoid ground loops with PLC systems. |
| **ESD** | Port is ESD-protected, but avoid static discharge when plugging in cables. |

> ⚠️ **Summary:**
> The STR-3221-R1 is designed for **SELV 24 VDC** systems. Never connect mains voltages.
> Always de-energize and confirm wiring before service.

### 5.2 What you need

| Item | Description |
|------|-------------|
| Module | STR-3221-R1 |
| Controller | MiniPLC/MicroPLC or Modbus RTU master |
| PSU | Regulated 24 VDC (module logic) |
| PSU | 12 or 24 VDC, sized for the LED / actuator load |
| Cable | USB-C and RS-485 twisted pair |
| Software | Browser with Web Serial support |

### 5.3 Power notes

The module needs **two separate supplies**, and mixing them up is the most common wiring mistake on this product.

| Supply | Terminals | Purpose | Sizing |
|---|---|---|---|
| **Module logic** | **V+** / **0V** (1, 2) | MCU, inputs, RS-485, sensor rails | 24 V DC nominal SELV, typical 0.2–0.5 W, logic only — size for electronics only |
| **LED load** | **LED PS +** / **−** (3, 4) | Feeds the nine output group rails | 12–24 V DC, sized for the total LED load |

The **+5 V SENS.A / SENS.B** rails for presence sensors are derived internally from the module
supply and individually fused (**F9** / **F10**, 1206L150THWR PTC). They are for sensor power
only — never for LED segments.

Both inputs are reverse-polarity and surge protected. Do not bridge **GND_FUSED** (field) and logic **GND** externally.

### 5.4 Step-by-step

#### Wire

Mount the module on a **35 mm DIN rail** inside a dry enclosure; disconnect **24 V DC** and the RS-485 trunk before wiring terminals.

**Power (24 V DC).** Connect a regulated **24 V DC SELV** supply to **V+** and **0V** for module logic, inputs, and RS-485 (reverse-polarity and surge protected; typical 0.2–0.5 W, logic only).

![24 V DC power supply wiring](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/STR-3221-R1/Images/STR_24Vdc_PowerSupply.png)
*Module **V+** / **0V** (24 V DC logic) — size for electronics only. LED / actuator loads use the separate **LED PS** input.*

**Outputs (32 channels).** Thirty-two low-side MOSFET sinks (**O1…O32**, FieldBoard **AO4882** stages) switch **12–24 V DC** loads: tie each load **+** to its **+** group rail (from the LED PSU) and load **−** to the channel terminal (max **1.5 A** per channel, **18 A** total module load).

**Digital trigger input.** One **IEC 61131-2** module-wetted discrete input uses terminals **Gnd** (8) and **I** (9) with a galvanically isolated **ISO1212** front-end (PTC fuse and TVS protected; module-wetted — do not apply external voltage).

Connect **potential-free (dry) contacts** — wall switches, push buttons, or relay outputs — between **Gnd** (terminal 8) and **I** (terminal 9). The module supplies wetting current via **ISO1212**; **do not feed external voltage into these terminals**.

![Digital trigger input wiring](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/STR-3221-R1/Images/STR_DigitalInput.png)
*Potential-free (dry) contact between **Gnd** (8) and **I** (9); module supplies wetting current — do not apply external voltage.*

**PIR / presence sensors (IN1, IN2).** Two **opto-isolated presence-sensor inputs** (**IN1**, **IN2**, **SFH6156** (IN1 = U18, IN2 = U17), 5.3 kV) accept PIR or motion detectors. Power low-current sensors from the fused **SENS.A** / **SENS.B** rails (**+5 V**, terminals 10 and 13, **F9**/**F10** **1206L150THWR**) — check the sensor is rated for a 5 V supply — and return sensor ground to the matching **Gnd** terminal (12 or 15). Wire the sensor output (open-collector or dry contact) between **IN1**/**IN2** (terminals 11/14) and the corresponding sensor ground.

**Example (PIR on IN1):** **SENS.A** + (10) → sensor **+5 V**; sensor **GND** → **Gnd** (12); sensor **OUT** → **IN1** (11) (open-collector to Gnd when motion detected).

![PIR motion sensor wiring](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/STR-3221-R1/Images/STR_PIRSensors.png)
*PIR sensors powered from **SENS** rail and signaling **IN1** / **IN2**.*

**RS-485 (Modbus RTU).** Bus hardware and wiring rules: [RS-485 / Modbus RTU](#rs-485--modbus-rtu). Fit 120 Ω at both ends of the bus only.

![RS-485 bus wiring](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/STR-3221-R1/Images/STR_RS485_Connection.png)
***A**, **B**, and **COM** to the controller or next module; external 120 Ω at both bus ends.*

**USB-C.** The **USB-C** port is for **WebConfig** setup and firmware update only; it is **not** a field power or runtime data bus — disconnect USB before energising the installation.

#### Configure

Connect USB-C, open [WebConfig](https://config.home-master.eu/STR-3221-R1/Firmware/v0.1.0/ConfigToolPage.html),
set a unique Modbus address and the baud rate used on your bus — the factory setting is address
**3** at **19200**. Enable the inputs you wired, invert where the sensor is normally-closed, and
check live I/O to confirm the wiring before the controller is involved. Full reference: [§6](#6-webconfig-reference).

#### Integrate

Add the ESPHome package to your MiniPLC / MicroPLC configuration ([§8](#8-esphome--home-assistant-integration))
with `str_address` matching what you set in WebConfig:

```yaml
packages:
  str1:
    url: https://github.com/isystemsautomation/homemaster-dev
    ref: main
    files:
      - path: STR-3221-R1/Firmware/v0.1.0/default_str_3221_r1_plc/default_str_3221_r1_plc.yaml
        vars:
          str_prefix: "STR#1"
          str_id: str_1
          str_address: 3
```

Entities appear in Home Assistant after the controller reboots. For a third-party Modbus master,
use the register map in [§7](#7-modbus-register-map) instead.

### 5.5 Verify

- **PWR LED steady** — module powered and running.
- **TX/RX blink** — the controller is polling and the module is answering.
- **WebConfig live I/O** — toggle each wired input and watch the state change; confirm the TLC59208F drivers report ready.
- **Outputs** — set one channel to 255 from WebConfig and confirm the correct segment lights. Use **Identify** to confirm which module you are looking at.
- **Home Assistant** — each channel appears as a dimmable light named `<prefix> O1 Light` … `O32 Light`.

---

## 6. WebConfig Reference

Configuration is done in the browser over USB-C — nothing to install. Open the
[WebConfig tool](https://config.home-master.eu/STR-3221-R1/Firmware/v0.1.0/ConfigToolPage.html)
in a Chromium-based browser (Chrome, Edge, Opera, Brave, Vivaldi; Chrome/Edge 89+, Opera 76+),
connect the module and grant serial access.

| Setting | What it does |
|---|---|
| **Modbus address** | Slave address on the RS-485 bus. Factory default `3`, valid 1–247. Every module on the bus needs a unique one |
| **Baud rate** | 9600, 19200, 38400, 57600 or 115200, 8N1. Factory default `19200`. Must match the controller |
| **Input enable / invert** | Per channel for **DI**, **IN1**, **IN2** — switch unused channels off, invert for normally-closed sensors |
| **LED mapping** | The two status LEDs: source and steady/blink mode |
| **Output levels** | Set any of the 32 channels 0–255 for commissioning; save explicitly to keep them as the power-up state |
| **Live I/O view** | Current input, button and LED states plus TLC59208F driver status |
| **Identify** | Pulses the first output group for 5 s — useful to find one module in a full cabinet |

> **Button mapping.** The page shows an action selector for SW1–SW4. In firmware **v0.1.0** the
> buttons have no local action — they only report their pressed state on Modbus — so the selector
> has no effect on the module.

Configuration is written to on-device flash (**LittleFS**) with a CRC and survives a power cut.
Changes to inputs, buttons and LEDs auto-save after 1.5 s; **output levels do not** — use Save.

---

## 7. Modbus Register Map

The module is a **Modbus RTU slave**. Factory default address `3`, factory default baud `19200`
(8N1); supported rates 9600, 19200, 38400, 57600 and 115200. Register numbers below are the
addresses used on the wire, as consumed by the shipped ESPHome package.

### 7.1 Address map (overview)

| Function code | Range | Contents |
|---|---|---|
| FC 02 — Discrete Inputs (read) | 1–3, 20–23, 90–91 | Input states, button states, LED states |
| FC 05 / 15 — Coils (write, self-clearing) | 300–302, 320–322 | Enable / disable an input |
| FC 03 / 06 / 16 — Holding Registers (read/write) | 400–431, 480–481 | Output levels, address and baud |
| FC 04 — Input Registers (read) | 200–204 | Identity and map version |

### 7.2 Discrete Inputs — FC 02 (read)

| Address | Name | Meaning |
|---:|---|---|
| 1 | IO1 | Module-wetted 24 V discrete input (**DI**, terminals 8–9), after enable and invert |
| 2 | IO2 | Presence input **IN1** (terminal 11), after enable and invert |
| 3 | IO3 | Presence input **IN2** (terminal 14), after enable and invert |
| 20 | BUTTON1 | Front-panel SW1, 1 = pressed |
| 21 | BUTTON2 | Front-panel SW2 |
| 22 | BUTTON3 | Front-panel SW3 |
| 23 | BUTTON4 | Front-panel SW4 |
| 90 | LED1 | Status LED 1 physical state |
| 91 | LED2 | Status LED 2 physical state |

### 7.3 Coils — FC 05 / 15 (write, self-clearing pulse)

| Address | Meaning |
|---:|---|
| 300–302 | Enable input IO1 / IO2 / IO3 |
| 320–322 | Disable input IO1 / IO2 / IO3 |

Writing `1` performs the action and the coil clears itself; the new enable state is persisted to
flash. This is the way to switch an input on or off from a controller without WebConfig.

### 7.4 Holding Registers — FC 03 / 06 / 16 (read/write)

| Address | Name | Range | Meaning |
|---:|---|---|---|
| 400–431 | O1…O32 | 0–255 | Per-channel brightness. `400` = O1, `431` = O32. `0` = off, `255` = full |
| 480 | Modbus address | 1–247 | Slave address. Values outside the range are clamped |
| 481 | Baud rate | see note | Bus baud rate |

> **HR 481 caveat.** The register holds the raw baud value, and 115200 does not fit in a 16-bit
> register — at 115200 the register **reads back as `0`**. That is expected, not a fault. Set
> 115200 through WebConfig rather than over Modbus.

### 7.5 Input Registers — FC 04 (read)

| Address | Field | Value on this module |
|---:|---|---|
| 200 | MODEL_ID | `8` |
| 201 | FW_MAJOR | `0` |
| 202 | FW_MINOR | `1` |
| 203 | FW_PATCH | `0` |
| 204 | MAP_VERSION | `1` |

Read 200–204 to identify a module and its register-map generation before trusting the rest of
the map.

### 7.6 Register use examples & polling

- **Brightness is a byte, not a bit.** Writing `255` to HR 400 turns O1 fully on; an intermediate
  value dims it via the TLC59208F PWM driver. There are no separate on/off coils for the outputs.
- **Output levels are not auto-persisted.** Values written over Modbus take effect immediately
  but are not saved; after a power cycle the module restores the last levels explicitly saved in
  WebConfig (factory default: all channels 0).
- **A disabled input always reads 0** on its discrete input, whatever the field wiring does.
- **Buttons are read-only.** SW1–SW4 report their pressed state on discrete inputs 20–23 and do
  not change any output on the module.
- **Polling.** 1 s is sufficient for stair lighting. Faster polling on a long bus with many
  modules needs the timing parameters in [§8.5](#85-troubleshooting-integration).
- **Changing address or baud** over HR 480 / 481 applies immediately — reconnect at the new
  settings afterwards.

---

## 8. ESPHome / Home Assistant Integration

The module is reached through a MiniPLC or MicroPLC running ESPHome. The controller holds the
`uart` and `modbus` components; the package below adds the module's entities.

### 8.1 Minimal YAML (controller side)

Your controller configuration needs a Modbus bus with the id `modbus_bus`, which the package
references:

```yaml
uart:
  id: mod_uart
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 19200
  stop_bits: 1

modbus:
  id: modbus_bus
  uart_id: mod_uart
```

Pin numbers are those of your controller — check the MiniPLC or MicroPLC README. The baud rate
must match what the module is set to; a factory-fresh module is at **19200**.

Then add the module package:

```yaml
packages:
  str1:
    url: https://github.com/isystemsautomation/homemaster-dev
    ref: main
    files:
      - path: STR-3221-R1/Firmware/v0.1.0/default_str_3221_r1_plc/default_str_3221_r1_plc.yaml
        vars:
          str_prefix: "STR#1"
          str_id: str_1
          str_address: 3
```

| Variable | Default | Purpose |
|---|---|---|
| `str_prefix` | `STR` | Name prefix for every entity — use a distinct one per module |
| `str_id` | `str` | Internal id for the `modbus_controller` — must be unique per module |
| `str_address` | `3` | Slave address; matches the module's factory default. Change both here and in WebConfig when adding a second module |
| `str_update_interval` | `1s` | Polling interval |
| `str_command_throttle` | `0ms` | Minimum gap between commands |

For a second module, include the package again with a different `str_prefix`, `str_id` and
`str_address`.

### 8.2 Entities exposed (from the package)

| Entity | Type | Source |
|---|---|---|
| `<prefix> IO1` | binary sensor | DI, discrete input 1 |
| `<prefix> IO2` | binary sensor | Presence IN1, discrete input 2 |
| `<prefix> IO3` | binary sensor | Presence IN2, discrete input 3 |
| `<prefix> Button1…4` | binary sensor | SW1–SW4, discrete inputs 20–23 |
| `<prefix> Status LED1…2` | binary sensor | Discrete inputs 90–91 |
| `<prefix> O1 Light` … `O32 Light` | light (monochromatic) | HR 400–431, dimmable 0–255 |

Each output is exposed as a **dimmable light**, not a switch — so a segment can be faded from
Home Assistant or an ESPHome script. `gamma_correct` is set to `0.0` in the package: the
TLC59208F already drives a linear PWM channel, and a second gamma curve on top would compress
the low end.

### 8.3 Optional: direct (manual) entity mapping

The command coils (300–302 / 320–322) are not exposed by the package. Use them from a
third-party Modbus master, or add `switch` entities of your own if you need them in Home
Assistant.

### 8.4 Home Assistant tips

Entities appear over the native ESPHome API — no MQTT broker and no register mapping inside
Home Assistant. A typical stair automation triggers on `<prefix> IO2` or `IO3` (the presence
inputs) and steps through the `O*` lights with a delay between them. **That sequence lives in
Home Assistant or in the controller's ESPHome configuration** — the module itself has no
sequencer, so the automation is what makes the steps follow one another.

### 8.5 Troubleshooting (integration)

ESPHome **2026.7.0** changed the Modbus timing defaults. On a bus with several modules, or
cable runs beyond a few metres, set these explicitly on the controller's `modbus` component
rather than relying on the defaults:

```yaml
modbus:
  id: modbus_bus
  uart_id: mod_uart
  send_wait_time: 250ms
```

and raise `str_command_throttle` if you see timeouts. Symptoms of timing that is too tight:
entities going unavailable intermittently, or one module on the bus dropping out while the
others stay up.

### 8.6 Notes & versions

The package path carries the firmware version. The current version is **v0.1.0**; when a new
version is released the `path:` changes with it, so pin the version your module is actually
running.

---

## 9. Programming & Build

### 9.1 Supported languages

* **MicroPython**
* **C / C++**
* **Arduino IDE**
* **PlatformIO**

> The STR-3221-R1 firmware is compatible with standard RP2350 toolchains and examples.
> It uses Modbus RTU libraries, Web Serial (for configuration), and I²C for LED drivers (TLC59208F).

### 9.2 Flashing (USB-C, hardware buttons)

Firmware updates and development are performed over the **USB-C** service port.
The module enumerates as a **USB Serial device** when connected to a PC.

**Steps:**

1. Connect the module to your PC via **USB-C**.
2. Hold **Buttons 1 + 2** → the module enters **BOOT mode**. (USB re-enumerates as a flashing device.)
3. Use the **Arduino IDE**, **PlatformIO**, or the provided update utility to upload firmware.
4. When flashing completes, press **Buttons 3 + 4** → triggers **hardware RESET** and runs the new firmware.
5. The module reboots and appears as a standard Modbus slave or WebConfig device.

| Function | Combination | Behavior |
|-----------|--------------|-----------|
| **BOOT Mode** | **Buttons 1 + 2** | Forces the module into flash/bootloader mode |
| **Hardware Reset** | **Buttons 3 + 4** | Restarts the MCU without clearing configuration |
| **Normal Operation** | — | Module runs stored firmware automatically |

### 9.3 Arduino / PlatformIO notes

#### Required libraries

```cpp
#include <Arduino.h>
#include <ModbusSerial.h>
#include <SimpleWebSerial.h>
#include <Arduino_JSON.h>
#include <LittleFS.h>
#include <Wire.h>
#include <utility>
#include "hardware/watchdog.h"
```

#### Board configuration

| Parameter | Setting |
|------------|----------|
| **Board** | Generic **RP2350** |
| **Flash Size** | 2 MB (Sketch 1 MB / FS 1 MB) |
| **Upload Port** | USB-C |
| **USB console baud** | 57600 |
| **Libraries** | Modbus RTU, SimpleWebSerial, JSON, LittleFS, Wire |

#### Pin mapping summary

Taken from the shipped sketch (`default_str_3221_r1.ino`), which is authoritative over the
board diagrams.

| Peripheral | MCU Pin | Description |
|-------------|----------|-------------|
| **RS-485 TX / RX** | GPIO4 / GPIO5 | UART2 to **MAX485**, auto DE/RE (`TxenPin = -1`) |
| **I²C SDA / SCL** | GPIO6 / GPIO7 | `Wire1` / I2C1 — 4× **TLC59208F** (U9–U12) |
| **Status LED1 / LED2** | GPIO9 / GPIO8 | On-board indicators — driven directly, **not** through the TLC59208F |
| **DI1 — 24 V discrete** | GPIO11 | **ISO1212**; idle 0 V, active HIGH |
| **DI2 — SENS.B (IN2)** | GPIO12 | **SFH6156** U17; idle 3 V, active LOW |
| **DI3 — SENS.A (IN1)** | GPIO10 | **SFH6156** U18; idle 3 V, active LOW |
| **Button 1–4** | GPIO16–GPIO19 | Active-HIGH via CD4069 (pressed = HIGH) |
| **QSPI Flash** | GPIO55–60 | W25Q32 32 Mbit flash memory |
| **USB D±** | GPIO51 / GPIO52 | USB-C data lines |

**TLC59208F addressing.** The driver auto-binds from an I²C scan at boot: it prefers the block
`0x20 0x21 0x22 0x23` (A1 strapped to SCL), falls back to `0x40 0x42 0x44 0x46` (GND/VCC strap),
and otherwise verifies candidates in the `0x20…0x5E` range. Scan results are reported over
WebConfig.

### 9.4 Firmware updates

**How to update**

1. Connect via **USB-C** to a PC.
2. Press **Buttons 1 + 2** to enter **BOOT mode**.
3. Upload new firmware — either the pre-built [`STR-3221-R1.uf2`](Firmware/v0.1.0/STR-3221-R1.uf2) or a build of `default_str_3221_r1.ino` from:
   - **Arduino IDE** → "Upload"
   - **PlatformIO** → `Upload and Monitor`
4. After flashing, press **Buttons 3 + 4** for a safe hardware reset.

**Preserving configuration.** All configuration parameters (address, baud, input, LED and button
settings) are stored in the MCU's **non-volatile flash** with a CRC and remain intact unless
manually erased via WebConfig or serial command.

**Recovery methods.** If flashing fails or the module is unresponsive:

- Disconnect USB-C, wait 10 seconds, and reconnect while holding **Buttons 1 + 2** (force BOOT mode).
- Reflash firmware again.
- If configuration corruption occurs, select **"Factory Reset"** in WebConfig. Note this returns the module to **address 3, 19200 baud**.

---

## 10. Maintenance & Troubleshooting

### 10.1 Status LEDs

| Indicator | Meaning |
|---|---|
| **PWR LED – steady ON** | Module powered and running normally. |
| **TX/RX LEDs – blink** | Active Modbus communication on RS-485. |
| **No TX/RX blink** | Check A/B polarity, COM reference, and termination resistors. |
| **Status LED1 / LED2** | Whatever source was assigned in WebConfig; also readable on discrete inputs 90–91. |

### 10.2 Resets

| Action | Combination |
|---|---|
| **Reset Device** | Press **Buttons 3 + 4** for a hardware reboot. |
| **Full Factory Reset** | Hold all **Buttons 1–4** on power-up to clear configuration (returns to address 3, 19200 baud). |

### 10.3 Common issues

| Symptom | Resolution |
|---------------------|----------------------|
| **Module does not answer at all** | A factory-fresh or factory-reset module is at **address 3, 19200 baud** — not at whatever the rest of your bus uses. |
| **Two modules answer at once** | Both are still at the default address 3. Disconnect one, set a unique address in WebConfig. |
| **HR 481 reads 0** | Expected at 115200 — the raw value does not fit a 16-bit register. Not a fault. |
| **Outputs not responding** | Check the LED PS supply and the output **+** group rail; then check TLC status in WebConfig live view. |
| **WebConfig reports TLC59208F offline** | I²C drivers not answering; the module retries every 5 s with a full scan every 30 s. Check the MCU-board ribbon and run `i2c_scan` from WebConfig. |
| **Digital inputs not changing** | Wire potential-free contact between **Gnd** (8) and **I** (9); do not apply external voltage. Check enable / invert in WebConfig — a disabled input always reads 0. |
| **Output levels lost after power cycle** | Levels written over Modbus are not auto-persisted — save explicitly in WebConfig. |
| **Pressing a button does nothing** | Expected in v0.1.0 — the buttons report their state on Modbus and perform no local action. Drive the outputs from the controller or from WebConfig. |
| **No communication via USB-C** | Use a Chromium-based browser (Chrome, Edge, Opera, Brave, Vivaldi; Chrome/Edge 89+, Opera 76+); close other serial apps. |
| **Entities unavailable in Home Assistant, other modules fine** | Modbus timing too tight for the bus length — see [§8.5](#85-troubleshooting-integration). |
| **Which module is this?** | Use **Identify** in WebConfig — the first output group pulses for 5 s. |

---

## 11. Downloads & Resources

### Version history

| Version | Config path (`path:`) | Date | Changes |
|--------|------------------------|------|-----------|
| **v0.1.0** (current) | `STR-3221-R1/Firmware/v0.1.0/default_str_3221_r1_plc/default_str_3221_r1_plc.yaml` | 2026-07-05 | First release — 32ch TLC59208F, unified WebConfig |

### Files

| Resource | Description |
|-----------|-------------|
| **🧠 Firmware (Arduino/PlatformIO)** | [`Firmware/v0.1.0/default_str_3221_r1/`](Firmware/v0.1.0/default_str_3221_r1/) — main sketch. |
| **📦 Pre-built Firmware** | [`Firmware/v0.1.0/STR-3221-R1.uf2`](Firmware/v0.1.0/STR-3221-R1.uf2) — flash over USB-C in BOOT mode (Buttons 1 + 2). |
| **⚙️ ESPHome package** | [`Firmware/v0.1.0/default_str_3221_r1_plc/`](Firmware/v0.1.0/default_str_3221_r1_plc/) — Modbus package for MiniPLC / MicroPLC. |
| **🛠 WebConfig Tool** | [`Firmware/v0.1.0/ConfigToolPage.html`](Firmware/v0.1.0/ConfigToolPage.html) — browser-based USB-C setup. |
| **📷 Images & Diagrams** | [`Images/`](Images/) — module photos, terminal maps, and block diagrams. |
| **📐 Schematics (PDF)** | [`Schematics/`](Schematics/) — FieldBoard and MCUBoard schematics for hardware developers. |
| **📄 Datasheet & Manual** | [`Manuals/`](Manuals/) — module datasheet and installation guide. |

---

## Open Source & Licensing

This project uses a hybrid licensing model.

**Hardware.** Hardware designs (schematics, PCB layouts, BOMs) are licensed under **CERN-OHL-W v2**.

**Firmware & ESPHome integration.** All firmware, ESPHome configurations, and software components
are licensed under the **MIT License**.

This ensures full compatibility with ESPHome and Home Assistant while protecting hardware designs.

See LICENSE files in each directory for full terms.

---

## 12. Compliance & Certifications

The STR-3221-R1 Stair LED Controller module is CE marked. **ISYSTEMS AUTOMATION S.R.L.** (HomeMaster® brand)
maintains the technical documentation and a signed EU Declaration of Conformity (DoC).

### Applicable EU directives

- **EMC Directive 2014/30/EU** — EN 55032:2015 + AC:2016-07 + A11:2020 + A1:2020 (Class B emissions),
  EN 55035:2017 + A11:2020 (immunity); tested by Idvorsky Laboratories Ltd., Belgrade, Serbia
  (Job #1648, 20 April 2026)
- **RoHS Directive 2011/65/EU** — EN IEC 63000 technical documentation
- **Low Voltage Directive 2014/35/EU** — does not apply. This is a SELV-only product
  powered from 24 V DC nominal (input voltage well below 75 V DC, the lower threshold of
  LVD scope per Annex I of the Directive). The product has no mains-capable terminals
  (no AC input, no relay outputs). No LVD test report is required nor has been issued.

### Compliance documents

| Document | File |
|---|---|
| EU Declaration of Conformity (DoC) | [DoC_STR-3221-R1.pdf](./Manuals/DoC_STR-3221-R1.pdf) |
| Datasheet | [STR-3221-R1_Datasheet.pdf](./Manuals/STR-3221-R1_Datasheet.pdf) |

### Trademark

**HomeMaster®** is a registered European Union trademark of ISYSTEMS AUTOMATION S.R.L.,
EUTM No. 019082911, registered with EUIPO on 15 January 2025.

---

## 13. Support

If you need help using or configuring the **STR-3221-R1**, visit:

- 🌐 **[Official Support Portal](https://www.home-master.eu/support)** – knowledge base, ticketing, and FAQs.
- 🧰 **[WebConfig Tool](https://config.home-master.eu/STR-3221-R1/Firmware/v0.1.0/ConfigToolPage.html)** – in-browser setup and diagnostics.
- ▶️ **[YouTube Channel](https://youtube.com/@HomeMaster)** – setup videos and feature walkthroughs.
- 💡 **[Hackster.io](https://hackster.io/homemaster)** – integration examples and community projects.
- 💬 **[Reddit](https://reddit.com/r/HomeMaster)** – discussion and troubleshooting community.
- 📸 **[Instagram](https://instagram.com/home_master.eu)** – updates, showcases, and announcements.

> Firefox: experimental only (Nightly with the Web Serial flag enabled). Safari and stable Firefox are not supported.

---

**Manufacturer:** ISYSTEMS AUTOMATION S.R.L. (HomeMaster® brand)
**Registered office (registered office):** Str. Domnisori, Nr. 81, Bl. 62, Scara A, Etaj 3, Ap. 12, 100284 Ploiesti, Jud. Prahova, Romania
**Office / Contact address:** Diligentei 18, Ploiesti, Romania
**CUI / VAT:** RO 21537032
**EUID:** ROONRC.J2007000919293
**Telephone:** +40 747 757 798
**Website:** [https://www.home-master.eu](https://www.home-master.eu)

---

> **HOMEMASTER – Modular control. Custom logic.**
