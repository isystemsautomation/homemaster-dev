# RGB-621-R1 — Module for RGB+CCT LED Control

**HOMEMASTER – Modular control. Custom logic.**

The **RGB-621-R1** is an **RGB + tunable-white (CCT) LED controller** with **5 PWM channels**, **2 digital inputs**, **1 relay**, **Modbus RTU / Home Assistant** integration, and **USB-C WebConfig** setup.

![RGB-621-R1 photo](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/photo1.png)

![Modbus](https://img.shields.io/badge/Protocol-Modbus%20RTU-brightgreen)
![License](https://img.shields.io/badge/License-GPLv3%20%2F%20CERN--OHL--W-blue)

> **v0.1.0 is deprecated — use v0.2.0.** v0.1.0 remains available for existing installs but is no longer maintained.

## 🚀 Quick Start (current version)

**Firmware shipped on new modules: `v0.2.0`**

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

## 📦 Version History

| Version | Config path (`path:`) | Date | Changes |
|--------|------------------------|------|-----------|
| **v0.2.0** | `RGB-621-R1/Firmware/v0.2.0/default_rgb_621_r1_plc/default_rgb_621_r1_plc.yaml` | 2026-07 | **Current release.** Local input engine (momentary/maintained, multi-click, hold-to-dim), 12-bit PWM + gamma + slew, scenes, relay FOLLOW, HA STATE readback; Modbus engine-config removed (config is USB WebConfig only). |
| **v0.1.0** | `RGB-621-R1/Firmware/v0.1.0/default_rgb_621_r1_plc/default_rgb_621_r1_plc.yaml` | 2026-01 | Deprecated (legacy) — superseded by v0.2.0. Kept for existing installs; no longer maintained. |

> **Reproducible firmware build (v0.2.0):** [Build environment (reproducible)](../../README.md#build-environment-reproducible) · [`sketch.yaml`](Firmware/v0.2.0/default_rgb_621_r1/sketch.yaml)

# Hardware notes (current revision)

1. DI status LEDs swapped: on this hardware revision, the front-panel indication for DI1 and DI2 is swapped (DI1 LED shows DI2 state and vice versa). This is a silkscreen/panel indication issue only; logical inputs, Modbus registers, and WebConfig are correct.
2. Onboard buttons: only SW2 is a usable logic input (Button 1 in WebConfig). The second front button is not wired as a user input; it is used only to enter BOOT mode.
3. Entering BOOT mode: hold BOTH front buttons while power-cycling the module (or trigger Reset from WebConfig while keeping both buttons held). Release after power returns. The module then enumerates in BOOT/flash mode for firmware upload.

# 1. Introduction

## 1.1 Overview of the RGB-621-R1

The **RGB-621-R1** is a **smart RGB + CCT LED controller module** designed for **HomeMaster automation systems** and other **Modbus RTU networks**.  
It features **5 high-current PWM outputs** for RGB and Tunable White (CCT) LED control, **2 IEC 61131-2 compliant digital inputs** for wall switches or sensors, and **1 relay output** for switching external loads or LED drivers.

Powered by the **Raspberry Pi RP2350A** microcontroller, the module supports **RS-485 (Modbus RTU)** communication and configuration via **WebConfig over USB-C (Web Serial)** — no drivers or external software required.  
It connects directly to **HomeMaster MicroPLC** and **MiniPLC** controllers or operates as a **standalone Modbus slave** in any automation network.

Its **isolated I/O architecture**, **dual-board design**, and built-in **surge and short-circuit protection** ensure accurate dimming, stable communication, and reliable operation in demanding **home, ambient, or architectural lighting applications**.


---

## 1.2 Features & Architecture

| Subsystem         | Qty | Description |
|-------------------|-----|-------------|
| **Digital Inputs** | 2 | IEC 61131-2 compliant 24 V digital inputs (ISO1212 front-end), dry-contact or sourcing, with PTC fuse, TVS surge and reverse-polarity protection |
| **PWM Outputs** | 5 | N-channel MOSFET drivers (AP9990GH-HF), 12 V / 24 V LED channels for R / G / B / CW / WW |
| **Relay Output** | 1 | SPST-NO relay (HF115F/005-1ZS3), 5 V coil; 3 A @ 250 VAC / 30 VDC (module/PCB limit) |
| **Buttons** | 2 | Local control or configuration triggers (SW1 / SW2) |
| **LED Indicators** | 8 | Power, TX/RX, input, and status LEDs for feedback and diagnostics |
| **Modbus RTU** | Yes | RS-485 interface via MAX485CSA+T transceiver (external 120 Ω bus termination) |
| **USB-C** | Yes | WebConfig & firmware flashing with PRTR5V0U2X ESD protection |
| **Power Input** | 24 V DC | Protected by resettable fuses (1206L series), TVS (SMBJ33A), and reverse-blocking (STPS340U) |
| **Logic Supply** | — | AP64501SP-13 buck (5 V) + AMS1117-3.3 LDO chain |
| **MCU** | RP2350A | Dual-core Arm Cortex-M33 @ 133 MHz with 32 Mbit QSPI Flash (W25Q32JVUUIQ) |
| **Isolation & Protection** | — | Digital-input front-end per IEC 61131-2 (ISO1212); surge/EMI protected; TVS diodes, PTC fuses, transient suppression on field I/O |

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

RGB + CCT LED controller with:
- **5 PWM outputs**, **2 IEC 61131-2 digital inputs**, **1 relay**
- **RS-485 (Modbus RTU)** slave for HomeMaster controllers or SCADA
- Configurable via **USB-C WebConfig**
- Compact **DIN-rail** form factor

---

## 2.3 I/O Summary

| Interface | Qty | Notes |
|------------|-----|-------|
| **Digital Inputs** | 2 | IEC 61131-2 compliant 24 V digital inputs (ISO1212 front-end), dry-contact or sourcing, with PTC fuse, TVS surge and reverse-polarity protection |
| **Relay** | 1 | SPST-NO, 3 A @ 250 VAC / 30 VDC (module/PCB limit) |
| **PWM Outputs** | 5 | Low-side MOSFETs (AP9990GH-HF) for R/G/B/CW/WW |
| **RS-485 (Modbus)** | 1 | MAX485 transceiver, 19200 bps 8N1 default |
| **USB-C** | 1 | Config & firmware upload (logic only) |
| **MCU** | 1 | RP2350A @ 133 MHz, 32 Mbit QSPI Flash |
| **Buttons / LEDs** | — | Local control, TX/RX & status indicators |

---

## 2.4 Terminals & Pinout

![Front Terminals](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/photo1.png)

**Top:** V+/0 V (24 V DC input), Relay C/NO, Inputs I1/I2 (+ GND)  
**Bottom:** PWM R/G/B/CW/WW (24 V COM +), RS-485 A/B (+ COM opt.)

---

## 2.5 Electrical & Environmental

- **Supply:** 24 V DC ±10 % (SELV/PELV), ≈ 2 W (no LED load)  
- **PWM Drive:** up to 5 A per channel (25 A max total)  
- **Relay:** 3 A @ 250 VAC / 30 VDC (module/PCB limit)  
- **Digital inputs:** IEC 61131-2 front-end (ISO1212); surge/EMI protected  
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
| Supply Voltage | 20 V | 24 V | 30 V | SELV input protected |
| Power Use | — | 1.85 W | 3.0 W | No LED load |
| Relay Contacts | — | — | 3 A @ 250 VAC / 30 VDC | Module/PCB limit (resistive) |
| PWM Current | — | — | 5 A per ch | External PSU limited |
| RS-485 Rate | — | — | 115.2 kbps | Half-duplex |
| USB Voltage | 4.75 V | 5 V | 5.25 V | Logic only |
| Operating Temp | 0 °C | — | 40 °C | ≤ 95 % RH |

> **Installer Tip:** Use upstream fusing and snubbers for inductive loads.

---

## 2.8 Firmware & Operation

- Operates as **Modbus RTU slave**  
- Configurable via **WebConfig (USB-C)**  
- Registers control **PWM and Relay**; inputs readable as **coils/discretes**  
- **Buttons:** local test / override  
- **LED Indicators:**
  - **PWR:** Power OK  
  - **TX/RX:** Communication activity  
  - **DI1/DI2:** Input state  
  - **RUN/ERR:** Status / fault pattern  

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
Set the onboard relay to FOLLOW mode: it powers the LED driver whenever any channel is on and cuts
its supply after an off-delay once everything is dark — no standby draw or driver heating. Or use
the relay in Manual mode as a free switched output driven by a gesture, Modbus, or Home Assistant.
Module output is 3 A @ 250 VAC (PCB limit); use an interposing contactor for larger loads.

### 🏠 Use Case 3 — Full Home Assistant integration with live state
Add the ESPHome package (see Quick Start) to a MicroPLC/MiniPLC and the strip appears in Home
Assistant as an RGB+CCT light with the relay as a switch. Control color, brightness and CCT from HA
with 12-bit gamma-corrected, step-free dimming, while wall switches keep working locally; the module
reports its actual state back so HA stays in sync — and everything keeps working if the controller
or network goes down.

---

# 4. Safety Information

## 4.1 General Requirements

| Requirement | Detail |
|--------------|--------|
| **Qualified Personnel** | Installation, wiring, and servicing must be performed by trained technicians familiar with 24 V DC SELV/PELV control systems. |
| **Power Isolation** | Always disconnect the 24 V DC supply and RS-485 network before wiring or servicing. |
| **Rated Voltages Only** | Operate only from a **Safety Extra-Low Voltage (SELV/PELV) 24 V DC** source. **12 V DC is not supported.** Never connect mains (230 V AC) to any terminal. |
| **Independent Power** | Each controller and I/O module must have its **own 24 V DC power supply**, sized for its load and fused appropriately. |
| **Grounding** | Ensure proper protective-earth (PE) connection of the control cabinet and shielded bus cable. |
| **Enclosure** | Mount the device on a DIN rail inside a dry, clean enclosure. Avoid condensation, dust, or corrosive atmosphere. |

---

## 4.2 Installation Practices

**DIN-Rail Mounting**  
- Mount on a **35 mm DIN rail (EN 60715)**.  
- Provide at least **10 mm** clearance above/below for airflow and terminal access.  
- Route LED-power wiring separately from communication lines.

**Electrical Domains**  
Two distinct domains exist:  

- **Field Power (24 V DC)** — supplies LED drivers, relay, and input circuits.  
- **Logic Power (5 V / 3.3 V)** — internal regulation for MCU, USB, and RS-485.  

The field return is **`GND_FUSED`**; the logic return is **`GND`**.  
🟡 **Important:** Do **not** externally bridge `GND_FUSED` and `GND`.  
Isolation between logic and relay-drive domains is provided internally through the SFH6156 optocoupler (relay coil driver). Digital inputs use an ISO1212 IEC 61131-2 front-end wetted from the module 24 V supply — not a galvanic isolator.

**LED Power and Output Wiring**  
- The LED power rail (+24 V) enters through the protected input (fuses F3/F4, diode D5 STPS340U, surge D6 SMBJ33A).  
- It passes the relay K1 (HF115F) and feeds the **COM (+24 V)** terminal on the bottom connector.  
- LED channel outputs (**R, G, B, CW, WW**) are **low-side PWM sinks** using **AP9990GH-HF MOSFETs**.  
- Connect **LED +** to **COM**, and each color cathode to its respective channel output.  
- Only **24 V LED strips** (common-anode type) are supported.

**Relay Wiring**  
- Type HF115F (5 V coil, SPST-NO).  
- Contact rating: **3 A @ 250 VAC / 30 VDC (module/PCB limit)** (resistive).  
- Relay component (HF115F class) rated up to 12 A @ 250 VAC at chip level — **that rating does not apply to the module**; use interposing contactors for higher or inductive loads.  
- For inductive loads, add an **external flyback diode or RC snubber**.  
- Keep relay conductors away from signal wiring.

**Digital Input Wiring**  
- Inputs use an **ISO1212 IEC 61131-2 digital-input front-end** (not galvanic isolation); wetted from the module 24 V supply.  
- Connect **dry contacts** or **24 V DC sourcing sensors** only.  
- Each input path has a **PTC fuse (F5/F6)**, **TVS D9**, and **reverse diodes (D10–D14)**.  
- Do not inject external voltage into DI pins.  
- Use shielded twisted-pair cable for runs > 10 m.

---

## 4.3 Interface Warnings

### ⚡ Power Supply (24 V DC)

| Parameter | Specification |
|------------|---------------|
| Nominal Voltage | 24 V DC ± 10 % |
| Input Protection | PTC fuses (F1–F4), reverse-polarity diode (STPS340U), surge TVS (SMBJ33A) |
| Ground Reference | Field return `GND_FUSED` |
| Front-end | IEC 61131-2 digital-input front-end (ISO1212); surge/EMI protected |
| Notes | Use a regulated SELV 24 V DC supply rated ≥ 1 A per module. |

---

### 🟢 Digital Inputs

| Parameter | Specification |
|------------|---------------|
| Type | IEC 61131-2 compliant, dry-contact or sourcing 24 V DC input |
| Circuit | ISO1212 receiver with TVS (SMBJ26CA) + PTC protection |
| Operating Range | 9 – 36 V DC (typ. 24 V DC) |
| Protection | PTC fuse, TVS surge and reverse-polarity protection |
| Notes | For switches or sensors only; debounce handled in firmware. |

---

### 🔴 Relay Output

| Parameter | Specification |
|------------|---------------|
| Type | SPST-NO mechanical relay (HF115F/005-1ZS3) |
| Coil Voltage | 5 V DC (via SFH6156 optocoupler + S8050 driver) |
| Contact Rating | 3 A @ 250 VAC / 30 VDC (module/PCB limit, resistive) |
| Component note | HF115F relay component rated up to 12 A @ 250 VAC — **not usable module output**; use interposing contactors for larger/inductive loads |
| Protection | External RC snubber / flyback diode recommended |
| Notes | Keep field wiring separate from logic; observe polarity and isolation boundaries. |

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
| Notes | Use only when field power is disconnected; not for continuous operation in field. |

---

> ⚠️ **Important:**  
> • The **RGB-621-R1** operates **only on 24 V DC SELV/PELV** power.  
> • **12 V DC** operation is **not supported**.  
> • Each module and controller has its own 24 V DC supply.  
> • Never connect mains voltage to any terminal.  
> • Maintain isolation between `GND_FUSED` (field) and `GND` (logic).  
> • Follow local electrical codes for fusing and grounding.

---

# 5. Installation & Quick Start

## 5.1 What You Need

| Item | Description |
|------|-------------|
| **Module** | RGB-621-R1 LED control module |
| **Controller** | HomeMaster **MicroPLC** / **MiniPLC** or any **Modbus RTU master** |
| **Power Supply (PSU)** | Regulated **24 V DC SELV/PELV**, sized for module and LED load |
| **Cables** | 1× **USB-C** cable (for setup), 1× **twisted-pair RS-485** cable |
| **Software** | Any Chromium-based browser (Chrome, Edge, Opera, Brave, Vivaldi; Chrome/Edge 89+, Opera 76+) with **Web Serial** support for WebConfig |
| **Optional** | Shielded wiring for long RS-485 runs, DIN-rail enclosure, terminal labels |

---

## 5.2 Power

- The RGB-621-R1 operates exclusively from a **24 V DC SELV/PELV** supply.  
  Connect the **+24 V** and **0 V (GND)** to the top power terminals marked **V+** and **0V** or **LED PS**.

- The LED strip’s positive rail (**+24 V**) is routed internally through:
  - **PTC fuses (F3/F4)** for over-current protection  
  - **Reverse-polarity diode (STPS340U)**  
  - **Surge suppressor (SMBJ33A)**  
  - **Relay K1 (HF115F)**, which switches the LED power output (COM terminal)  

  The LED channels (R/G/B/CW/WW) act as **low-side PWM sinks**, and the LED strip must be **24 V common-anode**.

- **Current consumption (typical):**
  - Logic + RS-485: ≈ 100 mA  
  - Relay coil: ≈ 30 mA (active)  
  - LED load: dependent on connected strips (sized per external 24 V LED PSU)

- **Ground references:**  
  - `GND_FUSED` → field ground for LED and inputs  
  - `GND` → logic/USB ground  
  These are internally isolated — do **not** tie them together externally.

---

## 5.3 Communication

**RS-485 Pinout (bottom connector):**

| Terminal | Signal | Description |
|-----------|---------|-------------|
| **A** | RS-485 A (+) | Non-inverting line |
| **B** | RS-485 B (–) | Inverting line |
| **COM** | Common reference (optional) | Field ground reference (GND_FUSED) for long bus runs |

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
  - Connect via **USB-C** and open **WebConfig** in any Chromium-based browser (Chrome, Edge, Opera, Brave, Vivaldi; Chrome/Edge 89+, Opera 76+).  
  - Set module address, baud rate, and optional relay/input parameters.  
  - Save settings to non-volatile memory.  

> Firefox: experimental only (Nightly with the Web Serial flag enabled). Safari and stable Firefox are not supported.

- **Ground reference use:**  
  - In most RS-485 systems, differential A/B are sufficient.  
  - The **COM** terminal may be connected between devices only if bus transceivers require a shared reference (rare in modern isolated networks).

---

> ⚙️ **Quick Summary**
> 1. Mount the module on a DIN rail.  
> 2. Wire +24 V and 0 V to the **LED PS** terminals.  
> 3. Connect LED strips (common-anode to COM, cathodes to R/G/B/CW/WW).  
> 4. Wire RS-485 A/B to the controller.  
> 5. Plug in USB-C, open WebConfig, assign address, set baudrate, test outputs.  
> 6. Disconnect USB, power up the system, and verify Modbus communication.

---

## 5.4 Installation & Wiring

Wire the module on a **35 mm DIN rail** inside a dry enclosure; disconnect field power before making connections. Two supplies are used: **24 V DC** for module logic (**V+** / **0V**) and a separate **12 V or 24 V DC** LED PSU for the strip (**LED PS** +/−). Do **not** bridge `GND_FUSED` (field) and `GND` (logic/USB) externally.

### Power

Connect a regulated **24 V DC SELV/PELV** supply to **V+** and **0V** for module logic, RS-485, and input wetting; connect a separate **12 V or 24 V DC** LED PSU to the **LED PS** (+/−) terminals for the strip load (PTC fuses, reverse-polarity diode, and TVS protect both paths).

![Power supply wiring — module V+/0V and LED PS](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/RGB_PowerSupply.png)
*Module **V+** / **0V** (24 V DC) and **LED PS** (+/−) for a 12/24 V common-anode strip.*

### LED outputs (5× PWM)

Five low-side PWM sinks (**R**, **G**, **B**, **CW**, **WW**) drive **12/24 V DC common-anode** LED strips: tie strip **+** to **COM** (switched LED positive) and each cathode to its channel; unused channels may be left open.

| RGB (3 colour) | RGB + CW |
|:---:|:---:|
| ![RGB LED strip wiring](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/RGB_RGB_Connection.png) | ![RGB + Cool White wiring](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/RGB_RGBCW_Connection.png) |
| *R, G, B channels — strip + on **COM**.* | *Adds **CW** for RGB + cool-white mixes.* |

| Tunable white (CWWW) | Full RGBCCT (RGB + CCT) |
|:---:|:---:|
| ![CCT / tunable white wiring](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/RGB_CWWW_Connection.png) | ![Full RGBCCT wiring](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/RGB_RGBCWWW_Connection.png) |
| ***CW** and **WW** only — colour-temperature control without RGB.* | *All five channels — native operating mode of the module.* |

### Digital inputs

Two **IEC 61131-2** digital inputs (**I1**, **I2**) use an **ISO1212** front-end wetted from the module 24 V supply; connect **dry contacts** or **24 V DC sourcing** sensors to **I1**/**I2** and **GND** (PTC fuse, TVS, and reverse-polarity protection on each path — do not inject external voltage into the DI pins).

![Digital inputs — dry-contact wiring to I1 and I2](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/RGB_DigitalInputs.png)
*Wall switches or sensors on **I1** / **I2** with shared **GND**.*

### Relay

One **SPST-NO** dry-contact relay (**C** / **NO**) switches an external load at up to **3 A @ 250 VAC** (module/PCB limit); add an external fuse or breaker sized for the load, and use a flyback diode or RC snubber on inductive circuits.

![Relay output — NO and C to external load](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/RGB_RelayConnectioin.png)
*Normally-open contact between **C** and **NO** (relay may also cut LED PSU via FOLLOW mode).*

### RS-485 (Modbus RTU)

Wire **A** (+) and **B** (−) on shielded twisted-pair cable in a daisy-chain bus; **COM** is an optional field-ground reference for long runs — fit an external 120 Ω resistor across A/B at the two physical ends of the bus (the module has no onboard terminator).

![RS-485 A/B/COM Modbus RTU wiring](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/RGB_RS485Connection.png)
***A**, **B**, and optional **COM** to the controller or next module.*

### USB-C

The **USB-C** port is for **WebConfig** setup and firmware update only (5 V from the host PC, logic domain); it is **not** a field power or data bus — disconnect USB before energising the installation and before handing control to RS-485.

## 5.5 Software & UI Configuration

Configuration is performed via **USB-C WebConfig** ([v0.2.0 ConfigToolPage](Firmware/v0.2.0/ConfigToolPage.html) or [hosted copy](https://config.home-master.eu/RGB-621-R1/Firmware/v0.2.0/ConfigToolPage.html)) in any Chromium-based browser with Web Serial support. Settings are saved automatically to flash.

- Modbus address and baud rate
- Live light levels, quick presets, and identify/factory-reset tools
- Wall-switch inputs (momentary/maintained, gestures, hold-to-dim targets)
- Onboard button (SW2) gestures
- Per-channel PWM trim, fade, and power-on state
- Hold-to-dim timing and four scene presets
- Relay FOLLOW mode, user LED mapping, 12-bit gamma output quality

### WebConfig (USB-C)

![WebConfig — connection, Modbus, and live light levels](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/webconfig1.png)
*Connection & Modbus address/baud, serial log, live RGB+CCT levels, and quick presets (OFF / WHITE / RGB / FULL).*

![WebConfig — wall-switch inputs and gesture engine](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/webconfig2.png)
*Wall-switch inputs (DI1/DI2) with mode, output target, and per-gesture actions; onboard button (SW2); engine debounce, multi-click, and hold timings; offline wall-switch unlock.*

![WebConfig — PWM channel trim and fade](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/webconfig3.png)
*Per-channel min/max trim, transition (fade) ms, and power-on state for Red, Green, Blue, Warm white, and Cool white.*

![WebConfig — hold-to-dim and scenes](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/webconfig4.png)
*Global hold-to-dim traverse time (default 3000 ms) and four scene presets with per-channel levels and Capture current.*

![WebConfig — relay, LEDs, and output quality](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/webconfig5.png)
*Relay FOLLOW (PSU cut) with follow channels and off delay; user LED source/mode; 12-bit PWM with gamma correction.*

## 5.6 Getting Started

Follow these steps for a first-time install (field wiring detail: [§5.4](#54-installation--wiring); Home Assistant integration: [§7](#7-esphome-integration-guide)).

1. Mount on a **35 mm DIN rail**; wire **24 V DC** to **V+** / **0V** and the LED PSU to **LED PS**; connect a **common-anode** strip (strip **+** to **COM**, cathodes to **R** / **G** / **B** / **CW** / **WW**).
2. Connect **RS-485** **A** / **B** / **COM** to the controller (**MicroPLC** / **MiniPLC**).
3. Plug **USB-C** into a PC; open the **WebConfig** tool in Chrome or Edge and click **Connect**.
4. Set a **unique Modbus address** (each module on the bus must differ; default is **3**) and baud **19200**; save to flash.
5. Optionally assign **DI1** / **DI2** wall-switch actions; test the light from WebConfig.
6. Add the [Quick Start](#-quick-start-current-version) ESPHome package to the controller with the matching `rgb_address`.
7. The light and relay appear in **Home Assistant**.

---

# 6. Modbus RTU Communication

The RGB‑621‑R1 communicates as a **Modbus RTU slave** over **RS‑485**. Register map matches `default_rgb_621_r1_plc.yaml` (v0.2.0) and firmware v0.2.0.

**Defaults:** Address **3**, **19200 8N1** (change in WebConfig).

---

## 6.1 Input Registers (FC04) — MAP_VERSION 3

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

| Address | Name | Description |
|---------|------|-------------|
| **1–2** | **DI1–DI2** | Wall-switch logical state (after enable + invert) |
| **60** | **Relay1** | Relay logical state |
| **90–91** | **LED1–LED2** | User LED logical state |

---

## 6.3 Holding Registers (FC03/06/16)

| Address | Name | Range | Description |
|---------|------|-------|-------------|
| 400–404 | **R, G, B, WW, CW** | 0–255 | PWM setpoints (8-bit API; scaled to 12-bit internally; slew-smoothed) |
| 410–414 | **R, G, B, WW, CW (12-bit)** | 0–4095 | Fine-grained PWM setpoints (same targets as HR 400–404) |
| 480 | **MB_ADDR** | 1–255 | Modbus address |
| 481 | **MB_BAUD** | enum | 0=9600 … 4=115200 |

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

**Relay1 FOLLOW:** when mode=FOLLOW, relay energizes while any watched group is on; after all outputs reach zero, relay opens after `offDelayMs` (default 45 s) — typical LED PSU cut.

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

Only if supported. Cover:
- YAML setup (`uart`, `modbus`, `package`)
- Entity list (inputs, relays, buttons, LEDs)
- Acknowledge, override controls
- Home Assistant integration tips

---

# 8. Programming & Customization (RGB-621-R1)

## 8.1 Supported Languages

- Arduino (RP2350 core)
- C/C++ (PIO / SDK)
- MicroPython

## 8.2 Flashing

**USB-C (Web Serial / CDC)**
1. Connect a USB-C cable from your PC to the module’s **USB** port.
2. **Enter BOOT mode:** press **Button 1 + Button 2** together (see photo below).
3. Flash using **PlatformIO** or **Arduino IDE** (serial upload).
4. When flashing completes, disconnect and power-cycle the module.

> **Reset:** this module **does not** have a button combo for reset.  
> To reset, **remove 24 VDC power for ≥5 s** and re-apply.

**PlatformIO / Arduino IDE setup**
- **Board/MCU:** *Raspberry Pi RP2350 / Generic RP235x*
- **USB upload:** Serial (CDC)
- **Flash layout (Arduino):** e.g. 2 MB (Sketch 1 MB / FS 1 MB)
- **Recommended libs (Arduino examples):**
  - `ModbusSerial` (RTU master/slave helpers)
  - `Arduino_JSON`
  - `LittleFS`
  - `SimpleWebSerial` (for WebConfig bridge)

**Buttons reference (RGB-621-R1 front)**

  ![Button 1 and Button 2 positions](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/buttons1.png)

- **Button 1 + Button 2** → **BOOT mode**  
- **Reset** → **power-cycle 24 VDC for ≥5 s**

## 8.3 Firmware Updates

- Open the project in **PlatformIO** or **Arduino IDE**.
- Put device in **BOOT** (Button **1+2**) and upload the new build.
- **Configuration persistence:** device settings (address/baud, channel trims, etc.) are stored in flash and **kept** across updates unless you explicitly erase.
- **Recovery:** if the device won’t enumerate, power-cycle 24 VDC (≥5 s) and retry **BOOT** (1+2). If needed, flash a minimal “factory” image first, then restore config via WebConfig backup.

---

# 9. Maintenance & Troubleshooting

## 9.1 Status LEDs (front panel)

| LED  | Meaning |
|-----|---------|
| **PWR** | Steady when powered and firmware is running. |
| **TX**  | Blinks on Modbus transmit. |
| **RX**  | Blinks on Modbus receive. |
| **I.1 / I.2** | Reflect isolated input states. |
| **RUN/ERR** (if present) | Heartbeat / fault pattern (refer to firmware notes). |

## 9.2 Resets & Modes

- **BOOT mode:** **Button 1 + Button 2** (for flashing).
- **Reset:** **remove 24 VDC for ≥5 s** and re-apply.

## 9.3 Common Issues

- **No communication (TX/RX dark):**  
  Check A/B polarity, external 120 Ω termination at both bus ends (not on module), baud/ID match, and shared COM reference if separate PSUs.
- **Relay won’t trigger:**  
  Confirm Modbus control vs. local override mode, verify coil/state in WebConfig, and ensure external wiring is on **C/NO** (dry contact). Add snubber for inductive loads.
- **LED channels do not light:**  
  Verify **COM (+24 V)** to strip, channel cathodes on **R/G/B/CW/WW**, correct polarity, and adequate 24 V PSU sizing.
- **Inputs not detected:**  
  Use **DI 24Vdc** terminals (I1/I2 with GND). Confirm sensor type (dry contact or 24 V sourcing) and debounce/invert settings in WebConfig.
- **USB not detected:**  
  Use a data-capable USB-C cable; close any app holding the port; re-enter **BOOT** (1+2).

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
