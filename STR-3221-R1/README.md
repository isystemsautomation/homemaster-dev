![Firmware Version](https://img.shields.io/badge/Firmware-v0.2.0-blue)
![Modbus](https://img.shields.io/badge/Protocol-Modbus%20RTU-brightgreen)
![License](https://img.shields.io/badge/License-MIT%20%2F%20CERN--OHL--W-blue)

# STR-3221-R1 — Module for Smart Lighting & I/O Control

**HOMEMASTER – Modular control. Custom logic.**

![Image](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/STR-3221-R1/Images/photo1.png)

**Document map:** [§1 Overview](#1-overview) · [§3 Specifications](#3-specifications) · [§4 Hardware](#4-hardware--interface) · [§5 Getting Started](#5-getting-started) · [§6 WebConfig](#6-webconfig-reference) · [§7 Modbus map](#7-modbus-register-map) · [§8 ESPHome](#8-esphome--home-assistant-integration) · [§9 Programming](#9-programming--build) · [§11 Downloads](#11-downloads--resources)

---

## 1. Overview

The **STR-3221-R1** is a **32-channel** low-side MOSFET output module for stair and
architectural lighting and for underfloor-heating actuators. It mounts on a **35 mm
DIN rail** and connects to a **MiniPLC / MicroPLC** (or any Modbus RTU master) over
**RS-485**, with Home Assistant integration via the ESPHome package.

The module itself runs the stair sequence and the heating-zone cycle. Those two
engines keep working with the controller switched off and the bus unplugged.
**Zone setpoints still come from the bus.** The module does not measure room or
floor temperature and does not run a PI loop — it holds valves, duty, local
safety on the discrete input, valve exercise and frost protection. The
controller holds temperature, the pump/boiler relay and the schedule.

**Key capabilities at a glance:**

- **32 independently dimmable low-side MOSFET channels** — 12–24 V DC loads, ≤1.5 A per channel, ≤18 A module total; nine physical groups of four, each with its own **+** rail
- **On-module stair sequencer** — direction from the two presence inputs, six drawing patterns, day/night levels, accent and night-light; runs with no controller
- **On-module heating loop** — slow PWM, phase spread, simultaneous-open cap, first-open delay, pump-demand flag, valve exercise, frost protection, NC/NO per channel
- **3 digital inputs** — 1 × IEC 61131-2 module-wetted 24 V discrete input (**ISO1212**, galvanically isolated) and 2 × opto-isolated presence inputs (**SFH6156**, 5.3 kV)
- **2 fused +5 V sensor rails (SENS.A / SENS.B)** — for low-current PIR / presence sensors only
- **4 buttons** with local actions (factory: SW1 all on, SW2 all off, SW3 none, SW4 clear local override)
- **2 configurable status LEDs**
- **Local override** and **bus failsafe** (failsafe ships disabled)
- **Driverless WebConfig** over USB-C
- **Persistent settings** in LittleFS, migrated across `CFG_VERSION` 0x0003 → 0x0004 → 0x0005

### How local and remote control coexist

A button or an input can act on the outputs with no controller present. A local
action takes **priority** over Modbus writes until it is released — by a button
set to *Clear override*, by coil **331**, or by an optional timeout.

If the RS-485 bus goes quiet longer than the configured **bus failsafe**
timeout, LED / raw / indicator channels do what they were told (Hold / Off /
Level). Stair-table channels keep running. Heat channels do **not** use that
per-channel action: after the configured silent hours they enter frost
protection instead. Failsafe ships **disabled** (timeout 0).

Panic (coil **332**) drives LED / raw / indicator channels to the panic level
and **keeps heat channels closed**. A fire alarm must not open the valves.

---

## 2. Features

Only behaviour confirmed in the v0.2.0 firmware.

| Area | Detail |
|---|---|
| **Outputs** | 32 low-side MOSFET sinks, 0–255 commanded level (heat profile: 0–100 % duty). Four **TLC59208F** chips drive the 32 main outputs, eight channels each. |
| **Channel profile** | Per channel: **LED**, **Heat**, **Raw**, **Indicator**. LED uses curve and min/max. Heat interprets the holding register as 0–100 %. Raw skips min/max and the curve; the master level still applies. Indicator is full-on or off. |
| **Curve, window, ramp** | Linear / gamma 2.2 / CIE 1931; min/max rescale; ramp 0–60 000 ms. |
| **Master level** | HR 432, 0–255 multiplier applied last to every profile, including raw. |
| **Auto-off** | Per-channel switch-off delay. Never arms on a heat channel. Does not fire while panic or failsafe is held. |
| **Groups** | Eight logical groups. Writing one member mirrors once to the others. Independent of the nine physical + rails. |
| **Scenes** | Eight stored sets of 32 levels. Recalled from HR 436, a button, an input, or WebConfig. |
| **Save levels** | Coil 330 writes the current levels to flash as the power-up state (rate-limited 10 s). |
| **Panic** | Coil 332 / button / WebConfig. LED/raw/indicator → panic level and held. Heat stays **closed**. Coil 334 clears. |
| **Inputs** | Per input: enable, invert, action (none / toggle / pulse / recall scene), target, level. A heating DI role on IO1 enables that input itself. |
| **Buttons** | None, All ON, All OFF, Ramp test, Clear override, Recall scene, Panic, Clear panic. |
| **Local override** | A local action holds the outputs against the bus until released. |
| **Status LEDs** | Two; solid or blink; source none / IO1 / IO2 / IO3. |
| **Stair sequencer** | Step count 2–32, free channel map, six patterns (sequential, all, wave, comet, centre, random), launch one-shot or hold-while-presence, timings, overlap, retrigger, opposite-end reverse, both-ends window, day/night levels, accent, night light, inhibit. Debounce and minimum-repeat are **stair** settings, not general input settings. |
| **Heating loop** | Slow PWM 300–3600 s (factory **900 s / 15 minutes**), phase spread, max open zones (0 = no limit), first-open delay, overrun, DI role (none / enable / inhibit / demand echo), valve exercise, frost (15 min on / 45 min off after T silent hours), summer mode, NC/NO, min-pulse hysteresis. A period of 0 (user-set) leaves every heat channel closed. |
| **Bus failsafe** | Timeout 0 = disabled. Per-channel Hold / Off / Level for LED/raw/indicator. Heat uses frost instead. Stair-table channels keep running. |
| **Diagnostics** | TLC ready/mask, I²C errors, reset reason, link age, uptime, hours and cycles per heat channel (`heat.stats`). |
| **Modbus RTU** | One merged FC 04 of IR 0…31. Identity at IR 200…204 is not polled. |

### Applications

#### Stair lighting

Presence sensors on **IN1** (bottom) and **IN2** (top), powered from **SENS.A /
SENS.B (+5 V)**. The first edge starts the sequence; which input fired first
sets the direction. The whole run lives on the module: it continues with Home
Assistant down, the controller off and the bus unplugged. Home Assistant still
chooses the day/night **window** (HR 438) and can inhibit new runs (HR 433).

#### Underfloor heating — who does what

| On the module | On the controller |
|---|---|
| Valve heads, slow PWM, phase spread, simultaneous-open cap | Room / floor temperature |
| Local safety on the 24 V DI (enable / inhibit / demand echo) | PI / schedule |
| First-open delay and the aggregate **heat-demand** flag | Pump and boiler relays |
| Valve exercise and frost protection | — |

**MicroPLC pump wiring.** The MicroPLC relay is brought out as **C and NC only**
— there is no NO terminal. The contact is **closed when the relay is off or the
controller is unpowered**. Hydraulically that is the safe side (the pump stays
able to run). For a demand that **closes** the circuit when the relay is
energised, use a **MiniPLC** or an external relay. Invert the demand in
software only if you have accepted that a dead controller leaves the pump on.

---

## 3. Specifications


<!-- hm:specs:start -->
| Specification | Details |
|---|---|
| Microcontroller | RP2350A dual-core microcontroller |
| Storage | External QSPI Flash (32 Mbit) |
| Power Input | 24 V DC nominal |
| Input Protection | 1 A fuse (F8), TVS surge suppression, EMI filtering |
| Main Logic Supply | Buck regulator 24 V → 5 V (~4.96 V nominal), 3.3 V LDO regulator |
| LEDs supply input | External 12–24 V DC (independent from logic supply) |
| Document revision | DS-STR-3221-R1 Rev. B · 2026-09 · Hardware R1 (V1.0) |
| LED Outputs | 32 × low-side MOSFET outputs (O1–O32), driven by 4 × TLC59208F (8 channels per chip). The TLC chips drive the main outputs, not the status LEDs. |
| LED Output Voltage | External 12–24 V DC supply |
| Maximum Total LED Current | 18 A per module, shared across all channels in use |
| Maximum current per channel | 1.5 A per channel |
| Output Structure | Open-drain, grouped by VCC rails, each channel with gate resistor + RC + ferrite |
| Digital Inputs | 1 × 24 V DC discrete input (ISO1212, galvanically isolated), module-wetted dry contact, PTC + TVS on the field side. Outputs and RS-485 are not isolated. |
| Isolation | DI isolated (ISO1212); presence inputs optically isolated (SFH6156); outputs and RS-485 not isolated. |
| Presence sensor inputs (SENS.A / SENS.B) | 2 × opto-isolated presence inputs (SFH6156). SENS.A / SENS.B are +5 V rails. Do not connect a 24 V PIR to these terminals. |
| User Interface | 4 buttons; 2 assignable status LEDs; power, RX and TX indicators; 32 channel-state indicators |
| Local control | Buttons SW1–SW4 and the three inputs act on the outputs; a local action holds priority until released. |
| Bus failsafe | Per-channel Hold / Off / Level on loss of the master; timeout 0 = off. Heat uses frost protection instead. |
| Sequencing | Stair sequence and heating-zone cycle run on the module. |
| Heating demand | Aggregate demand flag and open-zone count for the controller’s pump / boiler relay. |
| RS-485 | Half-duplex Modbus RTU, not galvanically isolated; surge protection and fail-safe biasing. Factory default address 3, 19200 baud, 8N1. |
| USB | USB-C, 5 V logic, ESD protected |
| Modbus defaults | Address 3, 19200 baud, 8N1 |
| Typical Power Consumption | 0.2–0.5 W |
| Operating temperature | 0 °C to +40 °C |
| Storage temperature | −10 °C to +55 °C |
| Relative humidity | 0–90 % RH, non-condensing |
| Ingress protection | IP20 (inside cabinet only) |
| Installation | Indoor control cabinet only; not for outdoor or exposed installation |
| Maximum altitude | 2000 m |
| Pollution degree | 2 |
| Dimensions | 158 × 90.6 × 67.3 mm (L × W × H) |
| DIN width | 9 modules (≈ 158 mm) |
| Mounting | 35 mm DIN rail |
| Enclosure | PC/ABS industrial enclosure |
| Terminal type | Pluggable screw terminal blocks, 5.08 mm pitch |
| Wire cross-section | 0.2–2.5 mm² (AWG 24–12) |
| Tightening torque | 0.4–0.6 Nm |
| Net weight | 150 g |
| Gross weight | 248 g |
| Pack size | 230 × 140 × 87 mm (L × W × H) |
<!-- hm:specs:end -->

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

| Control | Function in firmware v0.2.0 |
|---|---|
| **SW1** | Factory **All ON** (skips heat channels). Configurable. Engages local override. |
| **SW2** | Factory **All OFF**. Configurable. Engages local override. |
| **SW3** | Factory **none**. Configurable (same action list as SW1). |
| **SW4** | Factory **Clear override**. Configurable (same action list as SW1). |
| **Status LED1 / LED2** | Assignable source (none / IO1 / IO2 / IO3), solid or blink. State in **IR 2**. |
| **PWR / TX / RX** | Fixed — power, Modbus traffic. |

Pressed state of SW1–SW4 is **IR 1** (bits 0–3), not the v0.1.0 discrete inputs 20–23.

Hardware key combinations are unchanged: Buttons 1+2 BOOT, 3+4 reset, all four
on power-up factory-reset (address 3, 19200).

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

Connect USB-C, open [WebConfig](https://config.home-master.eu/STR-3221-R1/Firmware/v0.2.0/ConfigToolPage.html),
set a unique Modbus address and the baud rate used on your bus — the factory setting is address
**3** at **19200**. Enable the inputs you wired, invert where the sensor is normally-closed, and
check live I/O to confirm the wiring before the controller is involved. Full reference: [§6](#6-webconfig-reference).

After wiring, set channel profiles (LED vs Heat), then fill the Stairs or Heating card.
Presence sensors must be **5 V** parts on SENS.A / SENS.B.

#### Integrate

Add the ESPHome package to your MiniPLC / MicroPLC configuration ([§8](#8-esphome--home-assistant-integration))
with `str_address` matching what you set in WebConfig:

```yaml
packages:
  str1:
    url: https://github.com/isystemsautomation/homemaster-dev
    ref: main
    files:
      - path: STR-3221-R1/Firmware/v0.2.0/default_str_3221_r1_plc/default_str_3221_r1_plc.yaml
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
- **Home Assistant** — each channel appears as a dimmable light named `<prefix> O1 Light` … `O32 Light`. Heat zones that need 0–100 % replace the matching `light:` with a `number:` (see §8).

---

## 6. WebConfig Reference

Open
[WebConfig](https://config.home-master.eu/STR-3221-R1/Firmware/v0.2.0/ConfigToolPage.html)
in a Chromium-based browser. The config arrives in named, acknowledged
sections; a card hydrates only after its own section has arrived. Shared tools:
Identify (pulses O1–O8 and both status LEDs for 5 s), save / load / factory /
reboot.

Header pills: connection, bus link, failsafe, override, panic, scene, model,
firmware, address, baud.

### Device Setup

Modbus address 1–247 (factory 3) and baud 9600 / 19200 / 38400 / 57600 / 115200
(factory 19200, 8N1). Address and baud persist without a separate Save.

### Output Channels

Sliders O1–O32, 0–255. All OFF and Ramp test. A WebConfig write releases local
override, then applies.

### Master & panic

Master level 0–255 (255). Panic level 0–255 (255). **Panic hold** and **Clear
panic**. Panic does not open heat channels.

### Channel profiles

Set-all and per-channel: profile (LED / Heat / Raw / Indicator), curve (linear /
gamma 2.2 / CIE 1931), min, max, ramp (ms), auto-off (s).

### Groups

Eight groups × 32 channel checkboxes.

### Scenes

Eight scenes: Save current / Recall / Clear.

### Stairs

Live phase / step / direction. Run up, run down, stop. Step count; pattern;
wave width; launch mode (one-shot / hold while presence); step delay; hold;
fade-out; overlap; retrigger (ignore / restart / extend hold); opposite end
(ignore / reverse / hold both); IN1 is bottom or top; both-ends behaviour and window;
day level and night level; standby first / last; night-light level; step fade;
debounce; minimum repeat; step→channel map.

**Night window** toggle on this card — runtime, from status, not stored in
the `stairs` config section. Same state as HR 438. Day and night **levels**
are on the card and in HR 434/435.

### Heating

Live zones, demand, DI, summer, frost, exercise. **Exercise now**. Slow-PWM
period (factory 900 s / 15 minutes; 0 = off, zones stay closed); min pulse; phase spread; max open zones (0 = no limit); DI role;
first-open delay; overrun; summer mode; exercise interval and duration;
antifreeze hours; NC/NO per channel; hours and cycle counters (requested with
`heat.stats`, not stored in the config blob).

A channel with profile **Heat** ignores its failsafe Hold / Off / Level row.
Frost protection replaces that action.

### Bus failsafe

Bus timeout in seconds (0 = off) and optional override-release timeout. Set-all
and per-channel action (Hold / Off / Level) plus the Level value. Heat-profile
rows are stored but **not applied**.

### Digital Inputs

Per input (IO1 / IO2 / IO3): enabled, inverted, action (none / toggle / pulse /
recall scene), target (none / all), on-level, scene binding. Assigning a
heating role to IO1 enables that input even if the checkbox is cleared.

### Buttons

Per SW1–SW4: action and scene parameter. Factory SW1 All ON, SW2 All OFF,
SW3 none, SW4 Clear override.

### Status LEDs

Per LED: solid / blink; source none / IO1 / IO2 / IO3.

---

## 7. Modbus Register Map

The module is a **Modbus RTU slave**. Factory address `3`, baud `19200` 8N1.
Register numbers are the addresses on the wire, as in the shipped ESPHome
package. **FC 02 discrete inputs are gone.** All runtime state is one
contiguous **FC 04** block, **IR 0…31**, one transaction.

The **bus-failsafe timeout is not on Modbus**. It is configuration, and
configuration lives in WebConfig.

A write to HR **400…431** on a channel the sequencer currently owns is
accepted but **not applied** until the run ends. Then the channel returns to
accent or the bus value.

### 7.1 Address map (overview)

| Function code | Range | Contents |
|---|---|---|
| FC 04 — Input Registers | 0–31 | Entire runtime state (polled) |
| FC 04 — Input Registers | 200–204 | Identity — **not** polled |
| FC 05 / 15 — Coils | 300–302, 320–322, 330–334, 340–342, 345 | Commands, self-clearing |
| FC 03 / 06 / 16 — Holding Registers | 400–431, 432–438, 480–481 | Levels and runtime config |

### 7.2 Input Registers (FC 04) — IR 0…31

| Address | Name | Meaning |
|---:|---|---|
| 0 | `DI_MASK` | Bit 0 = DI (IO1), bit 1 = IN1 (IO2), bit 2 = IN2 (IO3), after enable and invert |
| 1 | `BTN_MASK` | Bit 0…3 = SW1…SW4 pressed |
| 2 | `LED_MASK` | Bit 0…1 = status LED1…LED2 physical state |
| 3 | `STATUS_FLAGS` | See bits below |
| 4 | `TLC_MASK` | Bit 0…3 = TLC59208F U9…U12 answered |
| 5 | `I2C_ERRORS` | Saturating I²C failure count |
| 6 | `RESET_REASON` | 0 = power-on / unknown · 1 = watchdog stall · 2 = commanded reboot |
| 7 | `LINK_AGE_S` | Seconds since the last frame to this slave |
| 8 | `UPTIME_MIN` | Minutes since boot |
| 9 | `ACTIVE_SCENE` | 0 = none, 1…8 = last recalled; cleared by a non-scene level change |
| 10–25 | `OUT_READBACK` | Commanded levels, **two channels per register** (low byte = even channel). Not the duty after curve and master — a write to HR 400 reads back unchanged |
| 26 | `SEQ_STATE` | Low byte: phase 0 idle · 1 up · 2 down · 3 hold · 4 fade-out. High byte: direction 0 none · 1 up · 2 down |
| 27 | `SEQ_STEP` | Current step, 0 = none |
| 28 | `HEAT_FLAGS` | See bits below |
| 29 | `ZONES_OPEN` | Number of open heat zones |
| 30 | `ZONE_MASK_LO` | Open-zone mask, channels 1…16 |
| 31 | `ZONE_MASK_HI` | Open-zone mask, channels 17…32 |

**`STATUS_FLAGS` (IR 3), 8 bits**

| Bit | Name |
|---:|---|
| 0 | TLC ready |
| 1 | Link OK |
| 2 | Bus failsafe active |
| 3 | Config dirty |
| 4 | Local override |
| 5 | Panic |
| 6 | Sequencer running |
| 7 | Heat demand |

**`HEAT_FLAGS` (IR 28), 5 bits**

| Bit | Name |
|---:|---|
| 0 | Heat demand |
| 1 | Summer mode |
| 2 | Heating DI closed |
| 3 | Frost protection active |
| 4 | Valve exercise active |

### 7.2b Identity (FC 04) — IR 200…204, not polled

| Address | Field | v0.2.0 |
|---:|---|---|
| 200 | `MODEL_ID` | `8` |
| 201 | `FW_MAJOR` | `0` |
| 202 | `FW_MINOR` | `2` |
| 203 | `FW_PATCH` | `0` |
| 204 | `MAP_VERSION` | `0x0020` |

### 7.3 Coils (FC 05 / 15) — 15 commands, self-clearing

| Address | Meaning |
|---:|---|
| 300–302 | Enable input IO1 / IO2 / IO3 |
| 320–322 | Disable input IO1 / IO2 / IO3 |
| 330 | Save current output levels to flash (rate-limited 10 s) |
| 331 | Release local override |
| 332 | Enter panic — LED/raw/indicator to panic level; **heat stays closed** |
| 333 | All off (one shot, not latched) |
| 334 | Clear panic |
| 340 | Stair run up |
| 341 | Stair run down |
| 342 | Stair abort |
| 345 | Exercise all heat valves now |

### 7.4 Holding Registers (FC 03 / 06 / 16) — 41 registers

| Address | Name | Range | Meaning |
|---:|---|---|---|
| 400–431 | O1…O32 | 0–255 (heat: 0–100) | Commanded level / duty |
| 432 | Master | 0–255 | Multiplier, default 255 |
| 433 | Inhibit | 0/1 | Blocks **new** stair starts; a run already in progress may finish |
| 434 | Night level | 0–255 | Level only — **does not** select the window |
| 435 | Day level | 0–255 | Level only — **does not** select the window |
| 436 | Scene recall | write 1–8 | Applies that scene; reads back 0 |
| 437 | Summer mode | 0/1 | Heat closed; valve exercise still runs |
| 438 | Night window | 0 = day, 1 = night | **The only register that selects day vs night** |
| 480 | Modbus address | 1–247 | **Published only.** A write from the bus is not applied. Set address in WebConfig |
| 481 | Baud | whitelist | **Published only.** A write from the bus is not applied. 115200 does not fit `uint16` and **reads back as 0**. Set baud in WebConfig |

**434 and 435 are levels. The day/night window is 438.** An integrator who
writes 434 expecting “switch to night” will not get that.

**480 and 481 are published, not applied.** The firmware writes the live
address and baud into those registers so a master can read them. A write
from the bus is accepted into the register image and then overwritten by
`applyModbusSettings` or a reboot. Same behaviour as v0.1.0; it was never
documented. Changing the slave address from the same bus that uses it is
deliberately refused. Use WebConfig.

### 7.5 Examples & polling

- Poll **IR 0…31** in one FC 04 every second. Do not poll 200…204 in the main loop.
- Cadence in the HomeMaster package is `update_interval: 1s`, `command_throttle: 0ms`.
- Output levels written on the bus are not auto-saved; use coil 330 or WebConfig Save.
- A disabled input reads 0. A heating DI role forces IO1 enabled.

### 7.6 v0.1.0 → v0.2.0 replacements

| v0.1.0 (FC 02, gone) | v0.2.0 |
|---|---|
| Discrete 1, 2, 3 | IR 0 bits 0, 1, 2 |
| Discrete 20–23 | IR 1 bits 0–3 |
| Discrete 90, 91 | IR 2 bits 0–1 |

---

## 8. ESPHome / Home Assistant Integration

The package reads the whole IR 0…31 block in **one** FC 04. Cadence is
`update_interval: 1s` / `command_throttle: 0ms` (Rule 15).

```yaml
packages:
  str1:
    url: https://github.com/isystemsautomation/homemaster-dev
    ref: main
    files:
      - path: STR-3221-R1/Firmware/v0.2.0/default_str_3221_r1_plc/default_str_3221_r1_plc.yaml
        vars:
          str_prefix: "STR#1"
          str_id: str_1
          str_address: 3
```

### Entities (from the package)

| Entity | Type | Source |
|---|---|---|
| `<prefix> IO1…IO3` | binary sensor | IR 0 bits 0–2 |
| `<prefix> Button1…4` | binary sensor | IR 1 |
| `<prefix> Status LED1…2` | binary sensor | IR 2 |
| `<prefix> TLC ready`, `Link OK`, `Bus failsafe`, `Config dirty`, `Local override`, `Panic`, `Sequence running`, `Heat demand` | binary sensor | IR 3 bits 0–7 |
| `<prefix> TLC chip 1…4 OK` | binary sensor | IR 4 |
| `<prefix> Heating enable input`, `Frost protection active`, `Valve exercise active` | binary sensor | IR 28 bits 2, 3, 4 |
| `<prefix> I2C error count`, `Reset reason`, `Link age`, `Uptime` | sensor | IR 5, 6, 7, 8 |
| `<prefix> Active scene` | sensor | IR 9 |
| `<prefix> Sequence state`, `Current step` | sensor | IR 26, 27 |
| `<prefix> Open zones` | sensor | IR 29 |
| `<prefix> O1 Light` … `O32 Light` | light 0–255 | HR 400–431 |
| `<prefix> Master level` | number | HR 432 |
| `<prefix> Inhibit` | number | HR 433 |
| `<prefix> Night level`, `Day level` | number | HR 434, 435 |
| `<prefix> Recall scene` | number | HR 436 |
| `<prefix> Summer mode` | number | HR 437 |
| `<prefix> Night window` | switch | HR 438 |
| Save levels / Release override / Panic / Clear panic / All off / Run up / Run down / Stop / Exercise valves | button | coils 330, 331, 332, 334, 333, 340, 341, 342, 345 |

**Heat channels as percentages.** The package exposes every channel as a
`light` 0–255, because one YAML covers every profile. For a zone whose profile
is **Heat**, the firmware reads HR 400…431 as **0–100 %**. To show those zones
as sliders in Home Assistant, **comment the matching `light:` entry** and **add
a 0–100 `number:` item to the existing `number:` section** (template at the
bottom of the package). Do not add a second `number:` key. One entity type per
channel — both would double-write the register.

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
3. Upload new firmware — either the pre-built [`STR-3221-R1.uf2`](Firmware/v0.2.0/STR-3221-R1.uf2) or a build of `default_str_3221_r1.ino` from:
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
| **Pressing a button does nothing** | Factory SW3 is none. SW1 All ON / SW2 All OFF skip heat channels. All ON/OFF/ramp/scene are blocked during failsafe or panic. |
| **No communication via USB-C** | Use a Chromium-based browser (Chrome, Edge, Opera, Brave, Vivaldi; Chrome/Edge 89+, Opera 76+); close other serial apps. |
| **Entities unavailable in Home Assistant, other modules fine** | Point the package `path:` at `Firmware/v0.2.0/…` and recompile. An old v0.1.0 package reads the wrong addresses. |
| **Which module is this?** | Use **Identify** in WebConfig — the first output group pulses for 5 s. |


Status LEDs and reset combinations as in §4.3. Additions for v0.2.0:

| Symptom | What to read |
|---|---|
| Bus dead / failsafe | IR 3 bit 2, IR 7 link age. Failsafe is off until you set a timeout. |
| Sequencer ignored a write | Expected while a run owns that channel. Wait for idle (IR 26 phase 0) or coil 342. |
| 434/435 did not switch day/night | Those are levels. Write **HR 438**. |
| All heat zones shut with the input “disabled” | A heating DI role enables IO1 itself. Check the role, not only the checkbox. |
| Panic opened no valves | By design. Coil 332 keeps heat closed. |
| Buttons do nothing | Factory SW3 is none. All ON/OFF/ramp/scene are blocked during failsafe or panic. |
| Heat duty looks like 8-bit brightness | The HA `light` is 0–255. Use a `number` 0–100 on that channel (see §8). |

### Valve counters

Hours energised and cycle count live in `/heat_stats.bin`. WebConfig asks for
them with `heat.stats` when the Heating card is in view. They are not in `cfg`
and not on the 250 ms status line.

A seized or stuck-open head shows a **cycle count that stops** while **hours
keep climbing** (or the opposite: hours frozen, cycles still incrementing on a
chattering actuator). Compare neighbouring zones on the same manifold.

---

---

## 11. Downloads & Resources

### Version history

| Version | Config path (`path:`) | Date | Changes |
|--------|------------------------|------|-----------|
| **v0.2.0** (current) | `STR-3221-R1/Firmware/v0.2.0/default_str_3221_r1_plc/default_str_3221_r1_plc.yaml` | 2026-09-24 | Stair sequencer, heating loop, channel engine, merged FC 04 map, WebConfig sections |
| v0.1.0 (legacy) | `STR-3221-R1/Firmware/v0.1.0/default_str_3221_r1_plc/default_str_3221_r1_plc.yaml` | 2026-07-05 | First release — 32ch TLC59208F, unified WebConfig |

### Files

| Resource | Path |
|---|---|
| Firmware sketch | `Firmware/v0.2.0/default_str_3221_r1/` |
| Pre-built UF2 | `Firmware/v0.2.0/STR-3221-R1.uf2` |
| ESPHome package | `Firmware/v0.2.0/default_str_3221_r1_plc/` |
| WebConfig | `Firmware/v0.2.0/ConfigToolPage.html` |

---

### v0.2.0 notes

#### Breaking changes

### FC 02 discrete inputs are gone

All runtime state moved to input registers, read with FC 04 over one contiguous
block, **IR 0…31**. An integration written against v0.1.0 will stall.

| v0.1.0 (FC 02, gone) | Was | v0.2.0 replacement |
|---|---|---|
| 1, 2, 3 | DI, IN1, IN2 | **IR 0** bits 0, 1, 2 |
| 20, 21, 22, 23 | SW1–SW4 pressed | **IR 1** bits 0–3 |
| 90, 91 | Status LED1, LED2 | **IR 2** bits 0–1 |

Users of the HomeMaster ESPHome package get the new map by changing `path:` to
`Firmware/v0.2.0/…` and recompiling the controller. **Third-party masters,
custom scripts and hand-written maps must be rewritten** before a module is
upgraded.

Holding registers 400–431, 480 and 481 keep their numbers. A system that only
writes output levels still talks to the same addresses — but a write to a
channel the sequencer currently owns is ignored until the run ends.

### Configuration format

`CFG_VERSION` is **0x0005**. Migration from **0x0003** and **0x0004** is
automatic; address, baud, input, LED, button and channel settings are copied
forward. No reconfiguration is required for those. Stair and heating blocks
are new. Factory and a migration from **0x0003** / **0x0004** fill them with
the v0.2.0 defaults: stair engine off, slow-PWM period **900 s**. A board
that already ran a previous **0x0005** beta keeps whatever it stored
(zeros stay zeros) until a factory reset.

---

#### Behaviour you need to know

1. **Panic does not open valves.** Coil 332 drives LED / raw / indicator
   channels to the panic level and **keeps every heat-profile channel closed**.
   A fire alarm must not throw the loops open.

2. **The sequencer owns its channels while it runs.** Writes to HR 400…431 on
   those channels are accepted but not applied until the sequence finishes.
   Then the channel returns to accent or the bus value.

3. **Frost replaces per-channel failsafe on heat.** The Hold / Off / Level row
   of a heat channel is stored and not used. After the configured silent hours
   the zone cycles 15 minutes on / 45 minutes off.

4. **Auto-off is not armed on heat** and does not fire while panic or failsafe
   is held.

5. **HR 434 and 435 are levels.** The day/night **window** is **HR 438**
   (0 = day, 1 = night). Writing 434 does not switch the staircase to night.

6. **A heating DI role enables the input.** The “input enabled” checkbox can
   no longer silently shut every zone by leaving IO1 disabled while the role
   is Enable or Inhibit.

7. **Failsafe ships disabled** (timeout 0). After the upgrade the module
   behaves as v0.1.0 did until a timeout is set. A failsafe that killed a
   staircase or a manifold the first time the controller rebooted would be
   the worse default.

8. **Factory button actions:** SW1 all on, SW2 all off, SW3 none, SW4
   **clear local override**. All ON skips heat channels.

9. **Valve hours and cycles** are requested with the WebConfig command
   `heat.stats`. They are not in the config transfer and not on the 250 ms
   status line.

10. **Config over USB** is delivered as named sections; the page acknowledges
    each part. A card that has not been hydrated must not be saved.

11. **The bus-failsafe timeout is not on Modbus.** It is set in WebConfig only.

12. **Heat channels in Home Assistant** still appear as `light` 0–255. For
    0–100 % sliders, comment the matching `light:` and add a `number:` to the
    existing `number:` section of the package.

13. **HR 480 and 481 are published, not applied.** The firmware writes the
    live address and baud so a master can read them. A write from the bus is
    not applied — same as v0.1.0, now stated. Set address and baud in
    WebConfig.

14. **Factory slow-PWM period is 15 minutes** (`slowPwmPeriodS = 900`). A
    user-set period of 0 leaves every heat channel closed; the Heating card
    then shows “Slow PWM off — zones will not open”.

---

#### Upgrade notes

- Flash the `.uf2` as usual (Buttons 1 + 2 for BOOT, 3 + 4 to reset).
- Point the ESPHome package `path:` at `Firmware/v0.2.0/…` and recompile.
  An old package against new firmware reads the wrong addresses.
- Use the v0.2.0 WebConfig page; the compatibility gate refuses a mismatched
  pairing.
- After a factory reset the buttons act (SW1/SW2). Where that is unwanted,
  set the actions to none before the module goes back on the wall.

---

#### Also in this release

- Address and baud set in WebConfig persist without a separate Save.
- `linkOk` reflects real bus traffic to this slave.
- I²C writes are batched — a full 32-channel update is four transactions.
- Reset reason distinguishes power-on, watchdog stall and commanded reboot.
- WebConfig does not send a section before it has received the device’s own
  values for it.

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
- 🧰 **[WebConfig Tool](https://config.home-master.eu/STR-3221-R1/Firmware/v0.2.0/ConfigToolPage.html)** – in-browser setup and diagnostics.
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
