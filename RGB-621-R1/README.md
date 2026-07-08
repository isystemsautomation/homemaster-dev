![Modbus](https://img.shields.io/badge/Protocol-Modbus%20RTU-brightgreen)
![License](https://img.shields.io/badge/License-GPLv3%20%2F%20CERN--OHL--W-blue)

## 🚀 Quick Start (current version)

**Firmware shipped on new modules: `v0.1.0`**

```yaml
packages:
  rgb1:
    url: https://github.com/isystemsautomation/homemaster-dev
    ref: main
    files:
      - path: RGB-621-R1/Firmware/v0.1.0/default_rgb_621_r1_plc/default_rgb_621_r1_plc.yaml
        vars:
          rgb_prefix: "RGB#1"
          rgb_id: rgb_1
          rgb_address: 1
```

## 📦 Version History

| Version | Config path (`path:`) | Date | Changes |
|--------|------------------------|------|-----------|
| **v0.1.0** | `RGB-621-R1/Firmware/v0.1.0/default_rgb_621_r1_plc/default_rgb_621_r1_plc.yaml` | 2026-06 | First versioned release |

> **Reproducible firmware build (v0.2.0):** [Build environment (reproducible)](../../README.md#build-environment-reproducible) · [`sketch.yaml`](Firmware/v0.2.0/default_rgb_621_r1/sketch.yaml)

# RGB-621-R1 — Module for RGB+CCT LED Control

**HOMEMASTER – Modular control. Custom logic.**

![RGB-621-R1 photo](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/photo1.png)

# 1. Introduction

## 1.1 Overview of the RGB-621-R1

The **RGB-621-R1** is a **smart RGB + CCT LED controller module** designed for **HomeMaster automation systems** and other **Modbus RTU networks**.  
It features **5 high-current PWM outputs** for RGB and Tunable White (CCT) LED control, **2 isolated digital inputs** for wall switches or sensors, and **1 relay output** for switching external loads or LED drivers.

Powered by the **Raspberry Pi RP2350A** microcontroller, the module supports **RS-485 (Modbus RTU)** communication and configuration via **WebConfig over USB-C (Web Serial)** — no drivers or external software required.  
It connects directly to **HomeMaster MicroPLC** and **MiniPLC** controllers or operates as a **standalone Modbus slave** in any automation network.

Its **isolated I/O architecture**, **dual-board design**, and built-in **surge and short-circuit protection** ensure accurate dimming, stable communication, and reliable operation in demanding **home, ambient, or architectural lighting applications**.


---

## 1.2 Features & Architecture

| Subsystem         | Qty | Description |
|-------------------|-----|-------------|
| **Digital Inputs** | 2 | Galvanically isolated (ISO1212) dry-contact inputs with surge and reverse protection |
| **PWM Outputs** | 5 | N-channel MOSFET drivers (AP9990GH-HF), 12 V / 24 V LED channels for R / G / B / CW / WW |
| **Relay Output** | 1 | SPST-NO relay (HF115F/005-1ZS3), 5 V coil, rated 16 A @ 250 VAC / 30 VDC |
| **Buttons** | 2 | Local control or configuration triggers (SW1 / SW2) |
| **LED Indicators** | 8 | Power, TX/RX, input, and status LEDs for feedback and diagnostics |
| **Modbus RTU** | Yes | RS-485 interface via MAX485CSA+T transceiver; 120 Ω termination selectable |
| **USB-C** | Yes | WebConfig & firmware flashing with PRTR5V0U2X ESD protection |
| **Power Input** | 24 V DC | Protected by resettable fuses (1206L series), TVS (SMBJ33A), and reverse-blocking (STPS340U) |
| **Logic Supply** | — | AP64501SP-13 buck (5 V) + AMS1117-3.3 LDO chain |
| **MCU** | RP2350A | Dual-core Arm Cortex-M33 @ 133 MHz with 32 Mbit QSPI Flash (W25Q32JVUUIQ) |
| **Isolation & Protection** | — | Galvanic isolation, TVS diodes, PTC fuses, transient suppression on all field I/O |

**Architecture summary:**  
- **MCU Board:** manages logic, USB, Modbus, and power regulation  
- **Field Board:** contains LED drivers, relay circuit, and isolated input section  
This modular, two-board design ensures clean signal separation between logic and 24 V field wiring, improving reliability in mixed-voltage installations.

---

## 1.3 System Role & Communication

The **RGB-621-R1** operates as a **Modbus RTU slave** on an **RS-485 differential bus**, typically polled by a **HomeMaster controller** (MicroPLC / MiniPLC) or other Modbus master.  
Each module is assigned a unique Modbus address via WebConfig, supporting up to 32 devices per bus.

**Default communication parameters:**  
- **Address:** 1  
- **Baud rate:** 19200 bps  
- **Format:** 8 data bits, no parity, 1 stop bit (8N1)  
- **Termination:** 120 Ω enabled at end of bus  
- **Fail-safe:** retains last valid PWM and relay state if communication is lost  

The controller periodically polls holding registers to:  
- Write PWM duty values for R, G, B, CW, WW channels  
- Control the relay output  
- Read digital input and status bits  

WebConfig enables users to modify address, baud rate, test I/O, calibrate channels, and perform real-time diagnostics — simplifying setup and commissioning.

---

# 2. RGB-621-R1 — Technical Specification

## 2.1 Diagrams & Pinouts

| Diagram |
|---|
| ![System Block Diagram](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/RGB_SystemBlock.png) **System Block Diagram** |
| ![RP2350A Pinouts](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/RGB_MCU_Pinouts.png) **RP2350A MCU Pinout** |
| ![Field Board Diagram](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/RelayBoard_Diagram.png) **Field Board Layout** |
| ![MCU Board Diagram](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/RGB-621-R1/Images/MCUBoard_Diagram.png) **MCU Board Layout** |

---

## 2.2 Overview

RGB + CCT LED controller with:
- **5 PWM outputs**, **2 isolated digital inputs**, **1 relay**
- **RS-485 (Modbus RTU)** slave for HomeMaster controllers or SCADA
- Configurable via **USB-C WebConfig**
- Compact **DIN-rail** form factor

---

## 2.3 I/O Summary

| Interface | Qty | Notes |
|------------|-----|-------|
| **Digital Inputs** | 2 | 24 V isolated (ISO1212), dry-contact or sourcing |
| **Relay** | 1 | SPST-NO, 16 A @ 250 VAC / 30 VDC |
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
- **Relay:** 16 A @ 250 VAC / 30 V DC  
- **Isolation:** 3 kVrms (inputs ↔ logic)  
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
| Relay Contacts | — | — | 16 A @ 250 VAC / 30 V DC | Resistive |
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

The **RGB-621-R1** module is primarily designed for multi-channel lighting control but can also be used in broader automation and signaling tasks.  
Its combination of isolated inputs, PWM outputs, and a relay makes it suitable for ambient lighting, architectural control, and user-interactive automation.

---

### 🏠 Use Case 1 — RGB Scene Control with Wall Switch Inputs

**Purpose:**  
Use two wall switches to trigger and cycle through preset color or brightness scenes stored in the controller.

**How it works:**  
Each digital input acts as a trigger to change the lighting mode or adjust brightness levels.

**Setup Steps:**
1. Connect **DI1** and **DI2** to wall switches (dry contact).  
2. Wire **RGBW LED strips** to PWM outputs R, G, B, CW, WW.  
3. In **WebConfig**, assign Modbus address and test LED channels.  
4. In the **MicroPLC / MiniPLC**, define scene logic (e.g., DI1 → next scene, DI2 → off).  
5. Use Modbus holding registers to control PWM duty cycles for each channel.

---

### 💡 Use Case 2 — Relay-Based Power Switching for LED Drivers

**Purpose:**  
Control a 24 V LED power supply or auxiliary lighting circuit via the onboard relay.

**How it works:**  
The relay output switches the driver’s DC line or AC supply based on PLC logic or local input triggers.

**Setup Steps:**
1. Connect the **relay COM/NO terminals** in series with the LED driver’s supply.  
2. Wire LED outputs to PWM channels for dimming control.  
3. In **WebConfig**, enable relay control via Modbus coil.  
4. Program the controller to energize the relay only when active scenes are running.  
5. Optionally, use a wall switch on **DI1** as a manual override for relay control.

---

### 🌈 Use Case 3 — Tunable White (CCT) Control with Daylight Automation

**Purpose:**  
Implement human-centric lighting that adjusts color temperature (CCT) throughout the day.

**How it works:**  
Two PWM channels (CW and WW) mix warm and cool light based on time of day or ambient sensor input.

**Setup Steps:**
1. Connect **CW** and **WW** LED strips to respective PWM outputs.  
2. Define a time-based profile in the controller (morning = warm, midday = cool).  
3. Use Modbus registers to update CW/WW duty cycles automatically.  
4. Optionally, map DI1 as a manual “Day/Night” mode toggle.  
5. Adjust max/min PWM limits in WebConfig for consistent brightness.

---

### 🚨 Use Case 4 — Status Indicator / Alarm Signaling

**Purpose:**  
Display system or alarm status using color lighting patterns.

**How it works:**  
The module’s PWM channels can drive RGB indicators or stack lights controlled by alarm flags from the PLC.

**Setup Steps:**
1. Wire a small 12 V RGB LED indicator to PWM outputs R, G, and B.  
2. Connect the module to the same Modbus bus as the alarm controller.  
3. Assign registers to display alarm colors (e.g., red = alert, green = normal).  
4. Use DI1 as a manual alarm acknowledge input.  
5. Configure the relay as an auxiliary siren or warning signal driver.

---

### 🧠 Use Case 5 — Standalone Mood Lighting Controller

**Purpose:**  
Operate ambient RGB lighting locally without an external PLC, using onboard inputs and preloaded logic.

**How it works:**  
The module can store simple input-to-output mapping rules (through WebConfig or firmware) for local lighting control.

**Setup Steps:**
1. Power the module from a 24 V DC supply.  
2. Connect LED strips to PWM outputs and wall switches to DI1/DI2.  
3. In WebConfig, set input-to-PWM mapping rules or fading behavior.  
4. Adjust brightness levels and transition speeds.  
5. Optionally, connect to Modbus later for centralized control or monitoring.

---

These examples illustrate how the **RGB-621-R1** can serve as both a **dedicated lighting driver** and a **multi-purpose automation node**, combining smooth dimming, robust field isolation, and Modbus integration.

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
Isolation between these domains is provided internally through the ISO1212 and SFH6156 devices.

**LED Power and Output Wiring**  
- The LED power rail (+24 V) enters through the protected input (fuses F3/F4, diode D5 STPS340U, surge D6 SMBJ33A).  
- It passes the relay K1 (HF115F) and feeds the **COM (+24 V)** terminal on the bottom connector.  
- LED channel outputs (**R, G, B, CW, WW**) are **low-side PWM sinks** using **AP9990GH-HF MOSFETs**.  
- Connect **LED +** to **COM**, and each color cathode to its respective channel output.  
- Only **24 V LED strips** (common-anode type) are supported.

**Relay Wiring**  
- Type HF115F (5 V coil, SPST-NO).  
- Contact rating: 16 A @ 250 VAC / 30 V DC (resistive).  
- For inductive loads, add an **external flyback diode or RC snubber**.  
- Keep relay conductors away from signal wiring.

**Digital Input Wiring**  
- Inputs use **ISO1212 galvanic isolation**.  
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
| Isolation | Field side isolated from logic via DC/DC and opto-devices |
| Notes | Use a regulated SELV 24 V DC supply rated ≥ 1 A per module. Each module must have its own isolated 24 V supply rail. |

---

### 🟢 Digital Inputs

| Parameter | Specification |
|------------|---------------|
| Type | Galvanically isolated, dry-contact or sourcing 24 V DC input |
| Circuit | ISO1212 receiver with TVS (SMBJ26CA) + PTC protection |
| Operating Range | 9 – 36 V DC (typ. 24 V DC) |
| Isolation | 3 kVrms (input ↔ logic) |
| Notes | For switches or sensors only; debounce handled in firmware. |

---

### 🔴 Relay Output

| Parameter | Specification |
|------------|---------------|
| Type | SPST-NO mechanical relay (HF115F/005-1ZS3) |
| Coil Voltage | 5 V DC (via SFH6156 optocoupler + S8050 driver) |
| Contact Rating | 16 A @ 250 VAC / 30 V DC (resistive) |
| Protection | External RC snubber / flyback diode recommended |
| Notes | Keep field wiring separate from logic; observe polarity and isolation boundaries. |

---

### 🔵 RS-485 Communication

| Parameter | Specification |
|------------|---------------|
| Transceiver | MAX485CSA+T |
| Bus Type | Differential, multi-drop (A/B lines) |
| Default Settings | 19200 bps · 8N1 |
| Termination | 120 Ω enabled only at end-of-line device |
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

## 4.3 Communication

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
  Enable the 120 Ω termination resistor **only** at the last module in the chain.

- **Default Modbus settings:**  
  - **Address:** 1  
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

Use diagrams and explain:
- Inputs
- Relays
- Sensor rails (12/5V)
- RS-485 terminals
- USB port

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

> ESPHome package reads **FC04 @0 count=5** (DI/LED/status) and **FC04 @26 count=3** (applied light + relay flags) every 5 s. Event counters **6–20** are optional for masters that need WirenBoard-style press accounting.

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

## 6.3 Holding Registers (FC03/06/16)

| Address | Name | Range | Description |
|---------|------|-------|-------------|
| 400–404 | **R, G, B, WW, CW** | 0–255 | PWM setpoints (8-bit API; scaled to 12-bit internally; slew-smoothed) |
| 410–414 | **R, G, B, WW, CW (12-bit)** | 0–4095 | Fine-grained PWM setpoints (same targets as HR 400–404) |
| 480 | **MB_ADDR** | 1–255 | Modbus address |
| 481 | **MB_BAUD** | enum | 0=9600 … 4=115200 |
| **500–579** | **ENGINE_CFG** | — | Local input engine (see below) |

### Holding block 500+ — local input engine (v0.2.0)

| Address | Name | Description |
|---------|------|-------------|
| 500–504 | Timings | debounce, *(reserved)*, multiClickGap, holdDelay, holdRepeat (ms) |
| 505 | Safe flags | bit0 = allowLocalWhenOffline |
| 506–510 | chSafe[0..4] | 0=OFF, 1=ON, 2=RESTORE_LAST on link loss |
| 511–530 | PWM cfg | minTrim, maxTrim, fadeMs, powerOn per channel |
| 531 | *(reserved)* | — | Former singleButtonDim — do not use |
| 532 | **dimFullRangeMs** | Hold-to-dim full 0..100% traverse while held (800–8000 ms, default 3000) |
| 533 | *(reserved)* | Former holdRampMs — do not use |
| 534 | *(reserved)* | Former CCT memberMask — do not use |
| 535 | **dimFullRangeMs** | Mirror of HR 532 (legacy CCT offset) |
| 536 | *(reserved)* | Former holdRampMs mirror — do not use |

Group **RGB** (R, G, B) and **CCT** (WW, CW) membership is fixed by hardware. **dimFullRangeMs** (HR 532) sets how long a full hold-to-dim sweep takes for both groups.

| 537–540 | Relay1 follow | mode (0=manual,1=follow), watchMask, offDelayMs |
| 541–555 | Input binds | DI1/DI2: flags + single/double/hold; SW2: single/double only (momentary) |
| 560–579 | Scenes[4][5] | Preset channel levels (0–255 API) |
| 580–581 | Output quality | gammaEnable, gammaTenths (22 = γ 2.2) |

---

## 6.4 Output quality (12-bit + gamma + slew)

- **Internal resolution:** 12-bit (0–4095) on all five PWM channels (`analogWriteResolution(12)`).
- **API:** Modbus HR **400–404** and WebConfig accept **0–255**; HR **410–414** accept **0–4095** for finer control. Firmware scales 8-bit writes to 12-bit setpoints.
- **Gamma:** configurable (default γ 2.2) via HR **580–581** / WebConfig; 4096-entry LUT applied at the final `analogWrite` stage only.
- **Trim:** per-channel min/max (HR 511–520) applied in perceived space before gamma.
- **Slew:** each channel has `current` and `target`; Modbus, gestures, and scenes update **target** and ramp over per-channel `fadeMs` (default **400 ms**, HR 521–525). **Hold-to-dim** uses a separate `dimFullRangeMs` traverse (HR 532, default **3000 ms**) — continuous, not stepped. `fadeMs = 0` = instant.
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

**Hold-to-dim:** after `holdDelayMs` (default **650 ms**), brightness ramps **continuously** toward min (0) or max trim over `dimFullRangeMs` (default **3000 ms**, HR 532). Direction and target come from each input's **Hold** gesture (Dim up / Dim down / Dim toggle dir + target). Release freezes at the current level (saved for RESTORE_LAST). Group dimming preserves channel ratios within RGB / CCT / RGB+CCT.

**Gesture targets** (WebConfig / `action<<8|target`): **7** = Group RGB (R,G,B), **8** = Group CCT (WW,CW), **10** = RGB+CCT (both groups in lockstep), **1** = Relay1, **9** = All. Per-channel PWM targets remain internal-only.

**Gesture actions:** None, Toggle, On, Off, Dim up, Dim down, **Dim toggle dir** (action **7**), Relay pulse, **Scene 1–4** (actions **10, 12, 13, 14** — target hidden/unused), Identify (**11**, onboard SW2 only). Legacy **Set 100%** (action **8**) maps to None. Legacy action **4** (All off) maps to Off+All. **Relay pulse** is offered only when target is Relay1.

**Scenes:** four presets (HR **560–579**); Scene *n* recalls `applyScene(n−1)`.

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
| Configure DI1 hold action | HR **545** (`action<<8|target`; target 7=RGB, 8=CCT, 10=RGB+CCT) |

---

## 6.7 Polling Recommendations

- **DI / LED / status:** one FC04 read **0..4** every 5 s (ESPHome package default)  
- **Applied PWM + relay flags:** one FC04 read **26..28** every 5 s (STATE block; drives HA light/relay feedback)  
- **PWM holding 400–404:** write via Light only; do not poll HR from ESPHome  
- **Relay/service coils:** write only on demand  
- **HA light transitions:** ESPHome package sets `default_transition_length: 0s` so HA sends one final value per channel; smooth crossfade is done on-module via per-channel **fadeMs** (WebConfig, default 400 ms). HA’s transition slider does not apply to this entity.

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
  Check A/B polarity, termination at bus ends (120 Ω), baud/ID match, and shared COM reference if separate PSUs.
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
- **WebConfig (HTML page):** `RGB-621-R1/Firmware/v0.1.0/ConfigToolPage.html`
- **Schematics (PDF):** `RGB-621-R1/Schematics/`
- **Datasheet & docs:** `RGB-621-R1/Manuals/`
- **Images & diagrams:** `RGB-621-R1/Images/`

---

# 12. Support

- **Official Support:** https://www.home-master.eu/support  
- **WebConfig Tool (RGB-621-R1):** https://config.home-master.eu/RGB-621-R1/Firmware/v0.1.0/ConfigToolPage.html  
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
