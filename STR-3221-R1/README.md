**Firmware Version:** v0.1.0

![Firmware Version](https://img.shields.io/badge/Firmware-v0.1.0-blue)
![Modbus](https://img.shields.io/badge/Protocol-Modbus%20RTU-brightgreen)
![License](https://img.shields.io/badge/License-MIT%20%2F%20CERN--OHL--W-blue)

## Quick Start (current version)

**Firmware v0.1.0** — ESPHome package:

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

## Version History

| Version | Config path (`path:`) | Date | Changes |
|--------|------------------------|------|-----------|
| **v0.1.0** | `STR-3221-R1/Firmware/v0.1.0/default_str_3221_r1_plc/default_str_3221_r1_plc.yaml` | 2026-07-05 | First release — 32ch TLC59208F, unified WebConfig |

# STR-3221-R1 — Module for Smart Lighting & I/O Control

**HOMEMASTER – Modular control. Custom logic.**

![Image](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/STR-3221-R1/Images/photo1.png)


# 1. Introduction

## 1.1 Overview of the STR-3221-R1

The **STR-3221-R1** is a **32-channel** low-side MOSFET LED controller for staircase and architectural lighting, with **motion-triggered animations**, **2 presence-sensor inputs**, one 24 V discrete input, and local buttons. WebConfig over USB-C sets address and behaviour; MiniPLC/MicroPLC (or any Modbus master) supervise it over RS-485.

**One-line purpose:** a high-density stair/architectural lighting node with presence-driven sequences and local-first operation.

> **Status:** in production and shipping. Firmware **v0.1.0**, Modbus map and ESPHome package are released — see [§6](#6-modbus-rtu-communication) and [§7](#7-esphome-integration-guide).

## Key advantages

- **32-channel** low-side MOSFET LED controller with motion-triggered staircase animations and **2 presence inputs** — a focused product for stair and architectural lighting.
- Native ESPHome API via the MiniPLC/MicroPLC controller — no MQTT broker, no manual Modbus register mapping for the package entities.
- Local-first / edge-resilient — onboard logic keeps working if the network or Home Assistant is down.
- Open hardware (**CERN-OHL-W v2**) and firmware (**MIT**) — repairable, reproducible, no vendor lock-in.
- Standard **RS-485 Modbus RTU** — works with any Modbus master or industrial HMI/SCADA system, not locked to HomeMaster.
- Driverless **USB-C WebConfig** (Chrome, Edge, Opera); configuration persists in on-device flash (**LittleFS**).

---

## 1.2 Features & Architecture

| Subsystem         | Qty | Description |
|------------------:|----:|-------------|
| **Digital Inputs** | 3 | **1 × IEC 61131-2 module-wetted 24 V DC discrete input** (**Gnd** + **24Vdc**, terminals 8–9, **ISO1212**) plus **2 × opto-isolated presence-sensor inputs** (**IN1**/**IN2**, terminals 10–15, **SFH6156** U17/U18) |
| **MOSFET Outputs** | 32 | Low-side **AO4882** dual N-channel MOSFET stages on FieldBoard (**O1…O32**), 12–24 V loads; grouped in fours, each group with its own **+** rail (nine groups). |
| **LED Driver ICs** | 4 | **TLC59208F** on MCU board (U9–U12): I²C PWM channel drivers to FieldBoard output stages. |
| **Buttons** | 4 | SW1–SW4 for test/override or user logic. |
| **Status LEDs** | 2 | On-board indicators (GPIO9 / GPIO8), user-assignable steady or blink; mirrored on discrete inputs 90–91. |
| **Modbus RTU** | Yes | RS-485 via **MAX485** transceiver; activity LEDs. |
| **USB-C** | Yes | **WebConfig over Web Serial** (Chromium-based: Chrome, Edge, Opera, Brave, Vivaldi; Chrome/Edge 89+, Opera 76+); ESD-protected port. |
| **Power** | 24 VDC | Reverse/surge-protected input; **AP64501** buck → **5 V**, **AMS1117-3.3** LDO → **3.3 V** logic. |
| **MCU** | RP2350 + **W25Q32** | Dual-core MCU with external QSPI flash for firmware/config. |
| **Protection** | TVS, PTC | Surge/ESD and resettable fuses across field & comms lines. |

---

## 1.3 System Role & Communication

- **Connection to RS-485 bus:** wire controller **A/B/COM** to the module’s **A/B/COM** terminals (daisy-chain friendly, terminate the ends).  
- **Operating mode:** **Modbus RTU slave**; can run simple local patterns/tests from buttons, while a PLC/SCADA/HA supervises over Modbus.  
- **Polling:** Controller reads **DI**, **IN1**, and **IN2** state and writes/reads **O1…O32**; optional mirrors for LEDs/buttons.  
- **Factory defaults (changeable in WebConfig):**
  - **Address:** `3`
  - **Baud:** `19200` (8N1)

---

# 2. STR-3221-R1 — Technical Specification

## 2.1 Diagrams & Pinouts

| Diagrams & Descriptions |
|--------------------------|
| ![System Diagram](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/STR-3221-R1/Images/STR_SystemBlockDiagram_New.png)<br>**System Block Diagram** — MCU, Modbus interface, power chain, and I/O groups. |
| ![FieldBoard Layout](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/STR-3221-R1/Images/FieldBoard_Diagram.png)**FieldBoard Layout** — 32 **AO4882** low-side outputs, **ISO1212** DI, **SFH6156** presence inputs, fused **+5 V** SENS rails. |
| ![MCUBoard Layout](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/STR-3221-R1/Images/MCUBoard_Diagram.png)**MCU Board Layout** — RP2350 MCU, TLC59208F drivers, MAX485, and USB-C. |
| ![Terminal Map](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/STR-3221-R1/Images/STR_MCU_Pinouts.png)**PinOut** — Field wiring view with power, DI, outputs, and RS-485. |

---



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

- Outputs are grouped in fours, each group with its own + rail - nine groups in total.
- Low-side switching: load + to the group + rail, load - to O.n.
- Two separate presence inputs SENS.A and SENS.B, each with its own + and Gnd.
- Output path: per channel ≤1.5 A (ferrite BLM31PG601SN1L); module total ≤18 A (DB128L-5.08 green/orange field terminals). LED PS **input** path is separate: grey PowerSupply terminals 20 A (path limit), SL1265-1R0M choke 24 A — not the same as the 18 A output total.

<!-- hm:terminal-map:end -->

## 2.2 I/O Summary

| Interface | Qty | Description |
|-----------:|----:|-------------|
| **Digital Inputs** | 3 | **1 × module-wetted 24 V DC discrete input** (**Gnd** + **24Vdc**, **ISO1212**, F6/F7) plus **2 × opto-isolated presence inputs** (**IN1**/**IN2**, **SFH6156** U17/U18, **SMAJ6.8CA** clamp) |
| **Outputs** | 32 | Low-side **AO4882** N-channel MOSFET stages, grouped in fours, each group with its own **+** rail (nine groups); PWM from MCU-board **TLC59208F** drivers. |
| **Buttons** | 4 | Local control / override / test switches. |
| **Status LEDs** | 2 | On-board indicators, assignable to a logic state; steady or blink. |
| **RS-485 (Modbus RTU)** | 1 | Communication bus; **A/B/COM** terminals. |
| **USB-C (Setup Port)** | 1 | WebConfig / firmware interface (not for powering field devices). |
| **Power Input** | 1 | **24 VDC (V+, 0V)**; reverse and surge-protected; onboard 5 V / 3.3 V regulation. |
| **Sensor Rails (SENS.A / SENS.B)** | 2 pairs | Fused **+5 V** auxiliary rails (**F9**/**F10** **1206L150THWR** PTC, **150 mA** per rail) for presence-sensor power only. |

---

## 2.3 Electrical Specifications

| Parameter | Min | Typ | Max | Unit | Notes |
|------------|----:|----:|----:|------|-------|
| **Supply Voltage (V+)** | 20 | 24 | 30 | VDC | SELV input; reverse/surge protected. |
| **Logic Rails** | — | 5 / 3.3 | — | VDC | Generated internally (buck + LDO). |
| **Quiescent Current (no load)** | — | 60 | 100 | mA | Base electronics only. |
| **Full-Load Current (all outputs)** | — | — | 18 | A | **Output** path limit (DB128L green/orange terminals). Per channel ≤1.5 A (BLM). LED PS **input** path ≤20 A (grey terminals; choke 24 A). |
| **Digital Input Range (DI only)** | 9 | 24 | 30 | VDC | **ISO1212** module-wetted input (terminals 8–9). |
| **Input Threshold (DI, ON)** | — | 8 | — | VDC | Typical **ISO1212** threshold. |
| **Sensor Rail Output (SENS.A / SENS.B)** | — | 5 | — | VDC | **+5 V** via **F9**/**F10** (**1206L150THWR**); **≤150 mA** continuous per rail. |
| **Output Type** | — | — | — | — | Low-side **AO4882** dual N-MOSFET; **≤1.5 A** per channel; **≤18 A** module total. |
| **Output Protection** | — | — | — | — | Gate RC + ferrite per channel (FieldBoard schematic); inductive LED wiring per installation practice. |
| **Communication** | — | — | — | — | RS-485 (**MAX485**); 9600, 19200, 38400, 57600 or 115200 bps. |
| **Input Front-Ends** | — | — | — | — | **DI:** **ISO1212** (module-wetted 24 V). **IN1/IN2:** **SFH6156** opto-isolated presence inputs. |
| **Operating Temperature** | 0 | — | 40 | °C | 95 % RH non-condensing. |

> ⚙️ **Design domains:**  
> - Field side: 24 VDC (DI, outputs); **+5 V** fused SENS.A / SENS.B for presence sensors.  
> - Logic side: 5 V / 3.3 V MCU, I²C bus, USB-C protected.  
> - Communication side: RS-485 transient protection (TVS + PTC) — **not galvanically isolated**; see [RS-485 / Modbus RTU](#rs-485--modbus-rtu).

---

## 2.4 Firmware Behavior

| Function | Description |
|-----------|-------------|
| **Input Processing** | Per-channel enable and invert; logical state published on discrete inputs 1–3. A disabled channel always reads 0. |
| **Output Control** | 32 channels driven from holding registers 400–431, 0–255 per channel (TLC59208F PWM). |
| **Button Actions** | SW1–SW4 assignable in WebConfig; state also published on discrete inputs 20–23. |
| **LED Feedback** | 2 on-board status LEDs, steady or blink, source selectable; state published on discrete inputs 90–91. |
| **Override Priority** | Local overrides take precedence over Modbus commands until released. |
| **WebConfig (USB-C)** | Modbus address and baud, input enable/invert, button and LED mapping, live I/O, output levels. |
| **Startup Logic** | Configuration is restored from flash at power-up, including the last **saved** output levels. Levels written over Modbus are applied immediately but are **not** auto-persisted — save explicitly in WebConfig to make them the power-up state. Factory defaults are all channels at 0. |
| **TLC59208F recovery** | If the I²C LED drivers do not answer at boot the module retries every 5 s, with a full bus scan every 30 s, and reports status over WebConfig. |
| **Watchdog** | 4 s hardware watchdog; the module reboots itself if the main loop stalls. |

---

> 🧩 **Note:**  
> The STR-3221-R1 shares the same firmware architecture as other HOMEMASTER I/O modules, enabling unified Modbus mapping, button/LED behavior, and WebConfig interface.

---
# 3. Use Cases

These example illustrate how the **STR-3221-R1** can be integrated into real-world automation or lighting systems.

---

### Motion-Based Stair Lighting

**What it does:**  
Automatically lights stair LEDs in sequence when motion is detected at the top or bottom of the staircase.

**Setup:**
1. Connect motion sensors to **IN1 (bottom)** and **IN2 (top)** terminals.  
2. Connect each stair LED segment to outputs **O1–O32** (low-side switching).  
3. Set the module’s **Modbus address** (and related options) in **WebConfig**.  
4. Program MicroPLC/MiniPLC to poll **IN1/IN2** and activate LEDs in a timed sequence.  
5. Use **Button 1** as “Manual Test / All ON” and **Button 2** as “All OFF”.

---

# 4. Safety Information

The **STR-3221-R1** is a **SELV (Safety Extra-Low Voltage)** device.  
Improper wiring, power application, or grounding may cause malfunction or damage.  
Follow all safety and wiring practices described below.

---

## 4.1 General Requirements

| Requirement | Detail |
|--------------|--------|
| **Qualified Personnel** | Only trained technicians familiar with control panels, PLCs, and SELV wiring should install or service this module. |
| **Power Isolation** | Always disconnect **24 VDC** power and RS-485 trunk before touching or rewiring terminals. |
| **Rated Voltages Only** | Use **SELV 24 VDC** power supplies; never connect AC mains or high-voltage lines. |
| **Grounding** | Properly bond the panel’s protective earth (PE) to reduce EMI and static discharge. |
| **Enclosure** | Mount in a **clean, dry, ventilated enclosure**; avoid moisture, conductive dust, or vibration. |
| **Static Protection** | Handle circuit boards only with ESD precautions (grounded strap and antistatic mat). |

---

## 4.2 Installation Practices

- **DIN Mounting:**  
  Mount securely on **35 mm DIN rail (EN 50022)** using the rear clip. Apply strain relief on all connected cables to prevent terminal stress.

**DIN width: 9 modules (9 × 17.5 mm).**
- **Isolation Domains:**  
  The module uses separate power domains:
  - **Field Power (24 VDC_FUSED)** for outputs and inputs  
  - **Logic Power (5 V / 3.3 V)** for MCU  
  Never short or bridge **GND_FUSED** (field ground) with **logic ground** unless specifically required by system design.

- **Sensor Power Connection:**  
  Power low-current PIR / presence sensors only from fused **SENS.A** / **SENS.B** rails (**+5 V**, **F9**/**F10**, **≤150 mA** per rail). The **DI** input (terminals 8–9) is a separate **module-wetted 24 V** channel — do **not** backfeed or parallel SENS rails with other supplies.

- **Wiring Discipline:**  
  Use ferruled, properly sized conductors (0.25–1.5 mm²).  
  Route communication (RS-485) and power lines separately to reduce noise coupling.

- **Testing Before Power-Up:**  
  Verify all terminal polarities, check RS-485 A/B orientation, and confirm no shorts between supply rails.

---

## 4.3 Interface Warnings

### Power (24 VDC Input / LED Supply)

| Area | Warning |
|-------|----------|
| **24 VDC Power (V+ / 0V)** | Use only clean, regulated SELV 24 VDC. Reverse polarity is protected but repeated mistakes may damage fuses. |
| **LED PS (+/–)** | Provides the external LED load voltage (typically 12–24 VDC). Do not short or exceed rated current capacity of field wiring. |
| **Sensor Rails (SENS.A / SENS.B)** | For **+5 V** presence-sensor power only (**F9**/**F10** PTC, **150 mA** per rail). Never use to drive LED loads or feed back external power sources. |

---

### Digital Input — module-wetted 24 V (DI)

| Area | Warning |
|-------|----------|
| **Input Type** | Terminals **8** (**Gnd**) and **9** (**24Vdc**) form one **module-wetted dry-contact** input via **ISO1212**. No AC or high-voltage inputs. |
| **Wiring** | Close a potential-free contact between **Gnd** (8) and **24Vdc** (9). **Do not** apply external voltage. |
| **Protection** | PTC/TVS protected (**F6**/**F7**, **1206L016**). Replace fuses only with identical PTC parts. |

---

### Presence-sensor inputs (IN1, IN2)

| Area | Warning |
|-------|----------|
| **Input Type** | **IN1** / **IN2** (terminals 11, 14) are **opto-isolated** via **SFH6156** (U17, U18); accept open-collector or dry-contact sensor outputs. |
| **Sensor Power** | Power sensors from **SENS.A** (+) / **SENS.B** (+) (**+5 V**, terminals 10, 13) with return to matching **Gnd** (terminals 12, 15). **≤150 mA** per rail (**F9**/**F10**). |
| **Protection** | **SMAJ6.8CA** TVS clamps on presence input lines. |

---

### Outputs (O1…O32)

| Area | Warning |
|-------|----------|
| **Output Type** | **Low-side AO4882** N-MOSFET sinks; maximum load per channel **1.5 A** (12–24 VDC); module total **≤18 A**. |
| **Polarity** | Connect load +V to **+ group rail**, load – to output terminal (O#). |
| **Inductive Loads** | Primarily LED/resistive loads; for large inductive loads add external RC or TVS snubbers. |
| **Shared Rail** | Each **4-channel** group shares a **+** rail (nine groups) — ensure consistent LED supply voltage. |

---

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


### USB-C (Service / WebConfig)

| Area | Warning |
|-------|----------|
| **Purpose** | For setup, diagnostics, and firmware only. Not for powering sensors or external devices. |
| **Connection** | Connect to PC via isolated USB hub if the RS-485 bus is long or exposed. |
| **During Operation** | Disconnect USB-C when running in the field; avoid ground loops with PLC systems. |
| **ESD** | Port is ESD-protected, but avoid static discharge when plugging in cables. |

> **USB connection and grounding.** The USB port is not galvanically isolated — USB ground is connected to the device's 0 V.
> - Device powered from external 24 V → run the laptop **on battery** (charger unplugged).
> - Device not externally powered (supplied from USB only) → the laptop may stay on its charger.
>
> A laptop on its charger combined with an externally powered device forms a ground loop through the USB cable and can cause USB dropouts, failed firmware uploads or WebConfig errors.

---

> ⚠️ **Summary:**  
> The STR-3221-R1 is designed for **SELV 24 VDC** systems. Never connect mains voltages.  
> Always de-energize and confirm wiring before service. Proper isolation, grounding, and shielding ensure safe and reliable operation.

# 5. Installation & Quick Start

## 5.1 What You Need

| Item | Description |
|------|-------------|
| Module | STR-3221-R1 |
| Controller | MiniPLC/MicroPLC or Modbus RTU master |
| PSU | Regulated 24 VDC |
| Cable | USB-C and RS-485 twisted pair |
| Software | Browser with Web Serial support |

## 5.2 Power

The module needs **two separate supplies**, and mixing them up is the most common wiring mistake on this product.

| Supply | Terminals | Purpose | Sizing |
|---|---|---|---|
| **Module logic** | **V+** / **0V** (1, 2) | MCU, inputs, RS-485, sensor rails | 20–30 V DC SELV, 60–100 mA quiescent — size for electronics only |
| **LED load** | **LED PS +** / **−** (3, 4) | Feeds the nine output group rails | 12–24 V DC, sized for the total LED load; input path ≤20 A |

The **+5 V SENS.A / SENS.B** rails for presence sensors are derived internally from the module supply and fused at **150 mA** each (**F9** / **F10**, 1206L150THWR). They are for sensor power only — never for LED segments.

Both inputs are reverse-polarity and surge protected. Do not bridge **GND_FUSED** (field) and logic **GND** externally.

## 5.3 Communication

| Item | Value |
|---|---|
| Terminal order on this module | **COM (5) – B (6) – A (7)** — read the silkscreen, the order differs across the HomeMaster range |
| Factory default address | `3` |
| Factory default baud | `19200`, 8N1 |
| Supported baud rates | 9600, 19200, 38400, 57600, 115200 |
| Termination | 120 Ω at the two physical ends of the bus only |

A module straight out of the box answers at address **3**, **19200 baud** — set a unique address
before putting a second module on the same bus. Address and baud are set in **WebConfig** over
USB-C, or over Modbus at **HR 480** (address) and **HR 481** (baud); see the caveat on HR 481 in
[§6.1](#61-register-map). Run **COM** to every node — the port is not galvanically isolated, and
COM is what bounds the common-mode voltage the transceiver sees. Full bus rules:
[RS-485 / Modbus RTU](#rs-485--modbus-rtu).

## 5.4 Installation & Wiring

Mount the module on a **35 mm DIN rail** inside a dry enclosure; disconnect **24 V DC** and the RS-485 trunk before wiring terminals. Use a separate **12 V or 24 V DC** LED PSU on **LED PS** (+/−) for stair segments — do **not** bridge **GND_FUSED** (field) and logic **GND** externally.

### Power (24 V DC)

Connect a regulated **24 V DC SELV** supply to **V+** and **0V** for module logic, inputs, and RS-485 (reverse-polarity and surge protected; typical 60–100 mA quiescent).

![24 V DC power supply wiring](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/STR-3221-R1/Images/STR_24Vdc_PowerSupply.png)
*Module **V+** / **0V** (24 V DC logic) — size for electronics only. LED / actuator loads use the separate **LED PS** input.*

### Stair LED outputs (32 channels)

Thirty-two low-side MOSFET sinks (**O1…O32**, FieldBoard **AO4882** stages) switch **12–24 V DC** LED segments: tie each load **+** to its **+** group rail (from the LED PSU) and load **−** to the channel terminal (max **1.5 A** per channel, **18 A** total module load).

### Digital trigger input

One **IEC 61131-2** module-wetted discrete input uses terminals **Gnd** (8) and **24Vdc** (9) with an **ISO1212** front-end (PTC fuse and TVS protected — do not exceed 30 V DC).

Connect **potential-free (dry) contacts** — wall switches, push buttons, or relay outputs — between **Gnd** (terminal 8) and **24Vdc** (terminal 9). The module supplies wetting current via **ISO1212**; **do not feed external voltage into these terminals**.

![Digital trigger input wiring](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/STR-3221-R1/Images/STR_DigitalInput.png)
*Potential-free (dry) contact between **Gnd** (8) and **24Vdc** (9); module supplies wetting current — do not apply external voltage.*

### PIR / presence sensors (IN1, IN2)

Two **opto-isolated presence-sensor inputs** (**IN1**, **IN2**, **SFH6156** U17/U18) accept PIR or motion detectors. Power low-current sensors from fused **SENS.A** / **SENS.B** rails (**+5 V**, terminals 10 and 13, **≤150 mA** per rail via **F9**/**F10** **1206L150THWR**); return sensor ground to the matching **Gnd** terminal (12 or 15). Wire the sensor output (open-collector or dry contact) between **IN1**/**IN2** (terminals 11/14) and the corresponding sensor ground.

**Example (PIR on IN1):** **SENS.A** + (10) → sensor **+5 V**; sensor **GND** → **Gnd** (12); sensor **OUT** → **IN1** (11) (open-collector to Gnd when motion detected).

![PIR motion sensor wiring](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/STR-3221-R1/Images/STR_PIRSensors.png)
*PIR sensors powered from **SENS** rail and signaling **IN1** / **IN2**.*

### RS-485 (Modbus RTU)

Bus hardware and wiring rules: [RS-485 / Modbus RTU](#rs-485--modbus-rtu).

![RS-485 bus wiring](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/STR-3221-R1/Images/STR_RS485_Connection.png)
***A**, **B**, and **COM** to the controller or next module; external 120 Ω at both bus ends.*

### USB-C

The **USB-C** port is for **WebConfig** setup and firmware update only; it is **not** a field power or runtime data bus — disconnect USB before energising the installation.

## 5.5 Software & UI Configuration

Configuration is done in the browser over USB-C — nothing to install. Open the
[WebConfig tool](https://config.home-master.eu/STR-3221-R1/Firmware/v0.1.0/ConfigToolPage.html)
in a Chromium-based browser (Chrome, Edge, Opera, Brave, Vivaldi; Chrome/Edge 89+, Opera 76+),
connect the module and grant serial access.

| Setting | What it does |
|---|---|
| **Modbus address** | Slave address on the RS-485 bus. Factory default `3`, valid 1–247. Every module on the bus needs a unique one |
| **Baud rate** | 9600, 19200, 38400, 57600 or 115200, 8N1. Factory default `19200`. Must match the controller |
| **Input enable / invert** | Per channel for **DI**, **IN1**, **IN2** — switch unused channels off, invert for normally-closed sensors |
| **Button mapping** | SW1–SW4 action assignment |
| **LED mapping** | The two status LEDs: source and steady/blink mode |
| **Output levels** | Set any of the 32 channels 0–255 for commissioning; save explicitly to keep them as the power-up state |
| **Live I/O view** | Current input, button and LED states plus TLC59208F driver status |
| **Identify** | Pulses the first output group for 5 s — useful to find one module in a full cabinet |

Configuration is written to on-device flash (**LittleFS**) with a CRC and survives a power cut.
Changes to inputs, buttons and LEDs auto-save after 1.5 s; **output levels do not** — use Save.

## 5.6 Getting Started

**1. Wiring.** Mount on DIN rail. Connect the 24 V DC module supply to **V+ / 0V** and the
LED PSU to **LED PS**. Wire LED segments to the output groups, sensors to **IN1 / IN2** with
power from **SENS.A / SENS.B**, and **A / B / COM** to the bus. Fit 120 Ω at both ends of the
bus only.

**2. Configuration.** Connect USB-C, open WebConfig, set a unique Modbus address and the baud
rate used on your bus — the factory setting is address 3 at 19200. Enable the inputs you wired,
invert where the sensor is normally-closed, and check live I/O to confirm the wiring before the
controller is involved.

**3. Integration.** Add the ESPHome package to your MiniPLC / MicroPLC configuration
([§7](#7-esphome-integration-guide)) with `str_address` matching what you set in WebConfig.
Entities appear in Home Assistant after the controller reboots. For a third-party Modbus
master, use the register map in [§6](#6-modbus-rtu-communication) instead.

---

# 6. Modbus RTU Communication

The module is a **Modbus RTU slave**. Factory default address `3`, factory default baud `19200`
(8N1); supported rates 9600, 19200, 38400, 57600 and 115200. Register numbers below are the
addresses used on the wire, as consumed by the shipped ESPHome package.

## 6.1 Register map

### Discrete inputs — FC 02 (read)

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

### Command coils — FC 05 / 15 (write, self-clearing pulse)

| Address | Meaning |
|---:|---|
| 300–302 | Enable input IO1 / IO2 / IO3 |
| 320–322 | Disable input IO1 / IO2 / IO3 |

Writing `1` performs the action and the coil clears itself; the new enable state is persisted to
flash. This is the way to switch an input on or off from a controller without WebConfig.

### Holding registers — FC 03 / 06 / 16 (read / write)

| Address | Name | Range | Meaning |
|---:|---|---|---|
| 400–431 | O1…O32 | 0–255 | Per-channel brightness. `400` = O1, `431` = O32. `0` = off, `255` = full |
| 480 | Modbus address | 1–247 | Slave address. Values outside the range are clamped |
| 481 | Baud rate | see note | Bus baud rate |

> **HR 481 caveat.** The register holds the raw baud value, and 115200 does not fit in a 16-bit
> register — at 115200 the register **reads back as `0`**. That is expected, not a fault. Set
> 115200 through WebConfig rather than over Modbus.

### Input registers — FC 04 (read)

| Address | Field | Value on this module |
|---:|---|---|
| 200 | MODEL_ID | `8` |
| 201 | FW_MAJOR | `0` |
| 202 | FW_MINOR | `1` |
| 203 | FW_PATCH | `0` |
| 204 | MAP_VERSION | `1` |

Read 200–204 to identify a module and its register-map generation before trusting the rest of
the map.

## 6.2 Usage notes

- **Brightness is a byte, not a bit.** Writing `255` to HR 400 turns O1 fully on; an intermediate
  value dims it via the TLC59208F PWM driver. There are no separate on/off coils for the outputs.
- **Output levels are not auto-persisted.** Values written over Modbus take effect immediately
  but are not saved; after a power cycle the module restores the last levels explicitly saved in
  WebConfig (factory default: all channels 0).
- **A disabled input always reads 0** on its discrete input, whatever the field wiring does.
- **Buttons override.** A local override from SW1–SW4 takes precedence over Modbus writes until
  it is released.
- **Polling.** 1 s is sufficient for stair lighting. Faster polling on a long bus with many
  modules needs the timing parameters in [§7.4](#74-timing-on-longer-buses).
- **Changing address or baud** over HR 480 / 481 applies immediately — reconnect at the new
  settings afterwards.

---

# 7. ESPHome Integration Guide

The module is reached through a MiniPLC or MicroPLC running ESPHome. The controller holds the
`uart` and `modbus` components; the package below adds the module's entities.

## 7.1 Controller side

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

## 7.2 Adding the module

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

## 7.3 Entities created

| Entity | Type | Source |
|---|---|---|
| `<prefix> IO1` | binary sensor | DI, discrete input 1 |
| `<prefix> IO2` | binary sensor | Presence IN1, discrete input 2 |
| `<prefix> IO3` | binary sensor | Presence IN2, discrete input 3 |
| `<prefix> Button1…4` | binary sensor | SW1–SW4, discrete inputs 20–23 |
| `<prefix> Status LED1…2` | binary sensor | Discrete inputs 90–91 |
| `<prefix> O1 Light` … `O32 Light` | light (monochromatic) | HR 400–431, dimmable 0–255 |

Each output is exposed as a **dimmable light**, not a switch — so a stair segment can be
faded from Home Assistant or an ESPHome script. `gamma_correct` is set to `0.0` in the
package: the TLC59208F already drives a linear PWM channel, and a second gamma curve on top
would compress the low end.

The command coils (300–302 / 320–322) are not exposed by the package. Use them from a
third-party Modbus master, or add `switch` entities of your own if you need them in Home
Assistant.

## 7.4 Timing on longer buses

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

## 7.5 Home Assistant

Entities appear over the native ESPHome API — no MQTT broker and no register mapping inside
Home Assistant. A typical stair automation triggers on `<prefix> IO2` or `IO3` (the presence
inputs) and steps through the `O*` lights with a delay between them. Because the module keeps
its configuration in flash, the wiring-level behaviour set in WebConfig continues to work
when Home Assistant is unavailable.

---

# 8. Programming & Customization

## 8.1 Supported Languages

* **MicroPython**
* **C / C++**
* **Arduino IDE**
* **PlatformIO**

> The STR-3221-R1 firmware is compatible with standard RP2350 toolchains and examples.  
> It uses Modbus RTU libraries, Web Serial (for configuration), and I²C for LED drivers (TLC59208F).

---

## 8.2 Flashing via USB-C

Firmware updates and development are performed over the **USB-C** service port.  
The module enumerates as a **USB Serial device** when connected to a PC.

**Steps:**
1. Connect the module to your PC via **USB-C**.
2. Hold **Buttons 1 + 2** → the module enters **BOOT mode**.  
   (USB re-enumerates as a flashing device.)
3. Use the **Arduino IDE**, **PlatformIO**, or the provided update utility to upload firmware.
4. When flashing completes, press **Buttons 3 + 4** → triggers **hardware RESET** and runs the new firmware.
5. The module reboots and appears as a standard Modbus slave or WebConfig device.

📷 **Button Combination Reference**

| Function | Combination | Behavior |
|-----------|--------------|-----------|
| **BOOT Mode** | **Buttons 1 + 2** | Forces the module into flash/bootloader mode |
| **Hardware Reset** | **Buttons 3 + 4** | Restarts the MCU without clearing configuration |
| **Normal Operation** | — | Module runs stored firmware automatically |

---

## 8.3 Arduino / PlatformIO Notes

### Required Libraries
For Arduino or PlatformIO environments, include:

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

### Board Configuration

| Parameter | Setting |
|------------|----------|
| **Board** | Generic **RP2350** |
| **Flash Size** | 2 MB (Sketch 1 MB / FS 1 MB) |
| **Upload Port** | USB-C |
| **USB console baud** | 57600 |
| **Libraries** | Modbus RTU, SimpleWebSerial, JSON, LittleFS, Wire |

### Pin Mapping Summary

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

---

## 8.4 Firmware Updates

### How to Update
1. Connect via **USB-C** to a PC.  
2. Press **Buttons 1 + 2** to enter **BOOT mode**.  
3. Upload new firmware — either the pre-built [`STR-3221-R1.uf2`](Firmware/v0.1.0/STR-3221-R1.uf2) or a build of `default_str_3221_r1.ino` from:
   - **Arduino IDE** → “Upload”
   - **PlatformIO** → `Upload and Monitor`
4. After flashing, press **Buttons 3 + 4** for a safe hardware reset.

### Preserving Configuration
All configuration parameters (address, baud, input, LED and button settings) are stored in the MCU’s **non-volatile flash** with a CRC and remain intact unless manually erased via WebConfig or serial command.

### Recovery Methods
If flashing fails or the module is unresponsive:
- Disconnect USB-C, wait 10 seconds, and reconnect while holding **Buttons 1 + 2** (force BOOT mode).
- Reflash firmware again.
- If configuration corruption occurs, select **“Factory Reset”** in WebConfig. Note this returns the module to **address 3, 19200 baud**.

---

# 9. Maintenance & Troubleshooting

| Indicator / Action | Meaning / Resolution |
|---------------------|----------------------|
| **PWR LED – steady ON** | Module powered and running normally. |
| **TX/RX LEDs – blink** | Active Modbus communication on RS-485. |
| **No TX/RX blink** | Check A/B polarity, COM reference, and termination resistors. |
| **Module does not answer at all** | A factory-fresh or factory-reset module is at **address 3, 19200 baud** — not at whatever the rest of your bus uses. |
| **Two modules answer at once** | Both are still at the default address 3. Disconnect one, set a unique address in WebConfig. |
| **HR 481 reads 0** | Expected at 115200 — the raw value does not fit a 16-bit register. Not a fault. |
| **Outputs not responding** | Check the LED PS supply and the output **+** group rail; then check TLC status in WebConfig live view. |
| **WebConfig reports TLC59208F offline** | I²C drivers not answering; the module retries every 5 s with a full scan every 30 s. Check the MCU-board ribbon and run `i2c_scan` from WebConfig. |
| **Digital inputs not changing** | Wire potential-free contact between **Gnd** (8) and **24Vdc** (9); do not apply external voltage. Check enable / invert in WebConfig — a disabled input always reads 0. |
| **Output levels lost after power cycle** | Levels written over Modbus are not auto-persisted — save explicitly in WebConfig. |
| **No communication via USB-C** | Use a Chromium-based browser (Chrome, Edge, Opera, Brave, Vivaldi; Chrome/Edge 89+, Opera 76+); close other serial apps. |
| **Entities unavailable in Home Assistant, other modules fine** | Modbus timing too tight for the bus length — see [§7.4](#74-timing-on-longer-buses). |
| **Which module is this?** | Use **Identify** in WebConfig — the first output group pulses for 5 s. |
| **Reset Device** | Press **Buttons 3 + 4** for a hardware reboot. |
| **Full Factory Reset** | Hold all **Buttons 1–4** on power-up to clear configuration. |

## FAQ

### USB keeps disconnecting / upload fails while the module is on 24 V

The USB port is not galvanically isolated — USB ground is connected to the device's 0 V. If the module runs on external 24 V and the laptop is on its charger, a ground loop can close through the USB cable (24 V PSU/PE ↔ charger earth or Y-capacitor leakage), causing USB dropouts, failed uploads or WebConfig errors. Run the laptop **on battery** (charger unplugged) while the device is externally powered. If the device is supplied from USB only, the laptop may stay on its charger.

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

# 12. Support

If you need help using or configuring the **STR-3221-R1**, visit:

- 🌐 **[Official Support Portal](https://www.home-master.eu/support)** – knowledge base, ticketing, and FAQs.  

> Firefox: experimental only (Nightly with the Web Serial flag enabled). Safari and stable Firefox are not supported.

- 🧰 **[WebConfig Tool](https://config.home-master.eu/STR-3221-R1/Firmware/v0.1.0/ConfigToolPage.html)** – in-browser setup and diagnostics.  
- ▶️ **[YouTube Channel](https://youtube.com/@HomeMaster)** – setup videos and feature walkthroughs.  
- 💡 **[Hackster.io](https://hackster.io/homemaster)** – integration examples and community projects.  
- 💬 **[Reddit](https://reddit.com/r/HomeMaster)** – discussion and troubleshooting community.  
- 📸 **[Instagram](https://instagram.com/home_master.eu)** – updates, showcases, and announcements.

---

> **HOMEMASTER – Modular control. Custom logic.**

## Compliance & Certifications

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

**Manufacturer:** ISYSTEMS AUTOMATION S.R.L. (HomeMaster® brand)
**Registered office (registered office):** Str. Domnisori, Nr. 81, Bl. 62, Scara A, Etaj 3, Ap. 12, 100284 Ploiesti, Jud. Prahova, Romania
**Office / Contact address:** Diligentei 18, Ploiesti, Romania
**CUI / VAT:** RO 21537032
**EUID:** ROONRC.J2007000919293
**Telephone:** +40 747 757 798
**Website:** [https://www.home-master.eu](https://www.home-master.eu)
