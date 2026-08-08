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
          str_address: 21
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

> **Status:** STR-3221-R1 is in development and not yet released for production; firmware, Modbus map and ESPHome integration are being finalized.

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
| **MOSFET Outputs** | 32 | Low-side **AO4882** dual N-channel MOSFET stages on FieldBoard (**O1…O32**), 12–24 V loads; grouped with shared **VCC** pins. |
| **LED Driver ICs** | 4 | **TLC59208F** on MCU board (U9–U12): I²C PWM channel drivers to FieldBoard output stages. |
| **Buttons** | 4 | SW1–SW4 for test/override or user logic. |
| **Status LEDs** | 4 | User-assignable (steady/blink) for power/activity/logic states. |
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
- **Defaults (changeable in WebConfig):**
  - **Address:** `21`
  - **Baud:** `115200` (8N1)

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


## 2.2 I/O Summary

| Interface | Qty | Description |
|-----------:|----:|-------------|
| **Digital Inputs** | 3 | **1 × module-wetted 24 V DC discrete input** (**Gnd** + **24Vdc**, **ISO1212**, F6/F7) plus **2 × opto-isolated presence inputs** (**IN1**/**IN2**, **SFH6156** U17/U18, **SMAJ6.8CA** clamp) |
| **Outputs** | 32 | Low-side **AO4882** N-channel MOSFET stages, grouped with shared **VCC** rails; PWM from MCU-board **TLC59208F** drivers. |
| **Buttons** | 4 | Local control / override / test switches. |
| **Status LEDs** | 4 | User-assignable (power, activity, or logic indicator). |
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
| **Full-Load Current (all outputs)** | — | — | 3.0 | A | At 24 VDC with max LED load. |
| **Digital Input Range (DI only)** | 9 | 24 | 30 | VDC | **ISO1212** module-wetted input (terminals 8–9). |
| **Input Threshold (DI, ON)** | — | 8 | — | VDC | Typical **ISO1212** threshold. |
| **Sensor Rail Output (SENS.A / SENS.B)** | — | 5 | — | VDC | **+5 V** via **F9**/**F10** (**1206L150THWR**); **≤150 mA** continuous per rail. |
| **Output Type** | — | — | — | — | Low-side **AO4882** dual N-MOSFET; **≤1 A** per channel (recommended **≤500 mA**). |
| **Output Protection** | — | — | — | — | Gate RC + ferrite per channel (FieldBoard schematic); inductive LED wiring per installation practice. |
| **Communication** | — | — | — | — | RS-485 (**MAX485**), 9600–115200 bps. |
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
| **Input Processing** | Debounced; logic reported via Modbus coils/registers. |
| **Output Control** | 32 channels controlled via Modbus write commands; supports PWM dimming and timed activation sequences. |
| **Button Actions** | Assignable in firmware: manual test, override ON/OFF, or reset function. |
| **LED Feedback** | Configurable for steady, blink, or activity indication via TLC59208F drivers. |
| **Override Priority** | Local overrides (buttons) take precedence over Modbus commands until released. |
| **WebConfig (USB-C)** | Provides Modbus address setup, baud-rate selection, live I/O status, and firmware update through Web Serial. |
| **Startup Logic** | On power-up, outputs default to OFF until first Modbus command or internal script execution. |
| **Fault Handling** | Overcurrent or thermal events trigger fault LED indication; recover automatically when condition clears. |

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
| **Output Type** | **Low-side AO4882** N-MOSFET sinks; maximum load per channel **1 A** (12–24 VDC; **≤500 mA** recommended). |
| **Polarity** | Connect load +V to **VCC group rail**, load – to output terminal (O#). |
| **Inductive Loads** | Primarily LED/resistive loads; for large inductive loads add external RC or TVS snubbers. |
| **Shared Rail** | Each 8-channel group shares a **VCC** rail — ensure consistent LED supply voltage. |

---

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
- Where the bus crosses into a different electrical installation with its own earthing reference — a utility or billing meter, another building, another cabinet's PE system — fit an external galvanic RS-485 isolator at that boundary. The on-board components are transient protection, not isolation, and will not survive a sustained ground-potential difference.


### USB-C (Service / WebConfig)

| Area | Warning |
|-------|----------|
| **Purpose** | For setup, diagnostics, and firmware only. Not for powering sensors or external devices. |
| **Connection** | Connect to PC via isolated USB hub if the RS-485 bus is long or exposed. |
| **During Operation** | Disconnect USB-C when running in the field; avoid ground loops with PLC systems. |
| **ESD** | Port is ESD-protected, but avoid static discharge when plugging in cables. |

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

- Describe 24 VDC input
- List expected current
- Explain isolated sensor power if present

## 5.3 Communication

- RS-485 pinout
- Address & baudrate setup
- Use of COM/GND reference

## 5.4 Installation & Wiring

Mount the module on a **35 mm DIN rail** inside a dry enclosure; disconnect **24 V DC** and the RS-485 trunk before wiring terminals. Use a separate **12 V or 24 V DC** LED PSU on **LED PS** (+/−) for stair segments — do **not** bridge **GND_FUSED** (field) and logic **GND** externally.

### Power (24 V DC)

Connect a regulated **24 V DC SELV** supply to **V+** and **0V** for module logic, inputs, and RS-485 (reverse-polarity and surge protected; typical 60–100 mA quiescent).

![24 V DC power supply wiring](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/STR-3221-R1/Images/STR_24Vdc_PowerSupply.png)
*Module **V+** / **0V** (24 V DC) — size the PSU for electronics plus LED load.*

### Stair LED outputs (32 channels)

Thirty-two low-side MOSFET sinks (**O1…O32**, FieldBoard **AO4882** stages) switch **12–24 V DC** LED segments: tie each load **+** to its **VCC** group rail (from the LED PSU) and load **−** to the channel terminal (max **1 A** per channel, **3 A** total module load).

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

Cover:
- WebConfig setup (address, baud)
- Input enable/invert/group
- Relay logic mode (group/manual)
- LED and Button mapping

## 5.6 Getting Started

Summarize steps in 3 phases:
1. Wiring
2. Configuration
3. Integration

---



# 6. Modbus RTU Communication

Include:
- Address range and map
- Input/holding register layout
- Coil/discrete inputs
- Register use examples
- Polling recommendations

---

# 7. ESPHome Integration Guide

Only if supported. Cover:
- YAML setup (`uart`, `modbus`, `package`)
- Entity list (inputs, relays, buttons, LEDs)
- Acknowledge, override controls
- Home Assistant integration tips

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
| **Baud Rate** | 115200 |
| **Libraries** | Modbus RTU, SimpleWebSerial, JSON, LittleFS, Wire |

### Pin Mapping Summary

| Peripheral | MCU Pin | Description |
|-------------|----------|-------------|
| **RS-485 TX / RX** | GPIO4 / GPIO5 | UART2 to **MAX485** (auto DE/RE) |
| **Button 1–4** | GPIO16–GPIO19 | Local input buttons |
| **LED 1–4** | I²C via TLC59208F | Status indicators |
| **I²C SCL / SDA** | GPIO7 / GPIO6 | **TLC59208F** U9–U12 on MCU board |
| **Field inputs IO1–IO3** | GPIO11 / GPIO10 / GPIO12 | From FieldBoard (**ISO1212** DI + **SFH6156** IN1/IN2) |
| **QSPI Flash** | GPIO55–60 | W25Q32 32 Mbit flash memory |
| **USB D±** | GPIO51 / GPIO52 | USB-C data lines |

---

## 8.4 Firmware Updates

### How to Update
1. Connect via **USB-C** to a PC.  
2. Press **Buttons 1 + 2** to enter **BOOT mode**.  
3. Upload new firmware (`default_str_3221_r1.ino` / UF2 when built) using:
   - **Arduino IDE** → “Upload”
   - **PlatformIO** → `Upload and Monitor`
4. After flashing, press **Buttons 3 + 4** for a safe hardware reset.

### Preserving Configuration
All configuration parameters (address, baud, LED/button mappings, etc.) are stored in the MCU’s **non-volatile flash** and remain intact unless manually erased via WebConfig or serial command.

### Recovery Methods
If flashing fails or the module is unresponsive:
- Disconnect USB-C, wait 10 seconds, and reconnect while holding **Buttons 1 + 2** (force BOOT mode).
- Reflash firmware again.
- If configuration corruption occurs, select **“Factory Reset”** in WebConfig.

---

# 9. Maintenance & Troubleshooting

| Indicator / Action | Meaning / Resolution |
|---------------------|----------------------|
| **PWR LED – steady ON** | Module powered and running normally. |
| **TX/RX LEDs – blink** | Active Modbus communication on RS-485. |
| **No TX/RX blink** | Check A/B polarity, COM reference, and termination resistors. |
| **Buttons unresponsive** | Verify 3.3 V logic; reboot using **Buttons 3 + 4**. |
| **No communication via USB-C** | Ensure a Chromium-based browser (Chrome, Edge, Opera, Brave, Vivaldi; Chrome/Edge 89+, Opera 76+); close other serial apps. |
| **Outputs not responding** | Check 24 V LED PS supply and output VCC rail. |
| **Digital inputs not changing** | Wire potential-free contact between **Gnd** (8) and **24Vdc** (9); do not apply external voltage. Check WebConfig enable/invert/debounce. |
| **WebConfig not connecting** | Use a Chromium-based browser (Chrome, Edge, Opera, Brave, Vivaldi; Chrome/Edge 89+, Opera 76+); allow serial access permission; reset module if busy. |
| **Reset Device** | Press **Buttons 3 + 4** for a hardware reboot. |
| **Full Factory Reset** | Hold all **Buttons 1–4** on power-up to clear configuration. |

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
| **🛠 WebConfig Tool** | [`Firmware/v0.1.0/ConfigToolPage.html`](Firmware/v0.1.0/ConfigToolPage.html) — browser-based USB-C setup. |
| **📷 Images & Diagrams** | [`Images/`](Images/) — module photos, terminal maps, and block diagrams. |
| **📐 Schematics (PDF)** | [`Schematics/`](Schematics/) — FieldBoard and MCUBoard schematics for hardware developers. |
| **📄 Datasheet & Manual** | [`Manuals/`](Manuals/) — module datasheet and installation guide. |
| **📦 Pre-built Firmware** | Not yet released. STR-3221-R1 is RP2350-based — the released binary will be a `.uf2` at `Firmware/v0.1.0/STR-3221-R1.uf2`. |

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
