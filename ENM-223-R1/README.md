![Modbus](https://img.shields.io/badge/Protocol-Modbus%20RTU-brightgreen)
![License](https://img.shields.io/badge/License-GPLv3%20%2F%20CERN--OHL--W-blue)

## 🚀 Quick Start (current version)

**Current firmware line: `v0.2.0`** — see [Firmware/README.md](Firmware/README.md) for build, persistence, and publish notes.

```yaml
packages:
  enm223_1:
    url: https://github.com/isystemsautomation/homemaster-dev
    ref: main
    files:
      - path: ENM-223-R1/Firmware/v0.2.0/default_enm_223_r1_plc/default_enm_223_r1_plc.yaml
        vars:
          enm_id: enm223_1
          enm_address: 30
          enm_prefix: "ENM #1"
```

Set `enm_address` to the Modbus ID configured in WebConfig.

## 📦 Version History

| Version | Config path (`path:`) | Date | Changes |
|--------|------------------------|------|-----------|
| **v0.2.0** | `ENM-223-R1/Firmware/v0.2.0/default_enm_223_r1_plc/default_enm_223_r1_plc.yaml` | 2026-07 | Signed P/Q, peaks/neutral/THD, import/export energy labels, alarm engine + relay Alarm Controlled, phase mapping, 3P4W/3P3W, split LittleFS persistence |
| **v0.1.0** | `ENM-223-R1/Firmware/v0.1.0/default_enm_223_r1_plc/default_enm_223_r1_plc.yaml` | 2026-06 | First versioned release (legacy; do not extend) |

> **Reproducible firmware build (v0.2.0):** [Build environment (reproducible)](../../README.md#build-environment-reproducible) · [`sketch.yaml`](Firmware/v0.2.0/default_enm_223_r1/sketch.yaml)

# ENM-223-R1 — 3-Phase Power Metering & I/O Module

**HOMEMASTER – Modular control. Custom logic.**

**Document map:** [§1 Introduction](#1-introduction) · [§4 Installation & WebConfig](#4-installation--quick-start) · [§5 Technical Specification](#5-module-code--technical-specification) · [§6 Modbus map](#6-modbus-rtu-communication) · [§7 ESPHome](#7-esphome-integration-guide) · [§8 Programming](#8-programming--customization) · [Firmware folder](Firmware/README.md)

<img src="https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/main/ENM-223-R1/Images/photo1.png" align="right" width="440" alt="MODULE photo">

### Module Description

The **ENM-223-R1** is a configurable smart I/O module designed for **3-phase power quality and energy metering**.  
It includes **3 voltage inputs, 3 current channels**, **2 relays**, and optional **4 buttons / 4 LEDs**, with configuration via **WebConfig** using **USB-C (Web Serial)**.  
It connects over **RS-485 (Modbus RTU)** to a **MicroPLC/MiniPLC**, enabling use in **energy monitoring, automation, and smart building applications**.

---


## Table of Contents

* [1. Introduction](#1-introduction)
* [2. Use Cases](#2-use-cases)
* [3. Safety Information](#3-safety-information)
* [4. Installation & Quick Start](#4-installation-quick-start)
* [5. MODULE-CODE — Technical Specification](#5-module-code--technical-specification)
* [6. Modbus RTU Communication](#6-modbus-rtu-communication)
* [7. ESPHome Integration Guide (if applicable)](#7-esphome-integration-guide)
* [8. Programming & Customization](#8-programming--customization)
* [9. Maintenance & Troubleshooting](#9-maintenance--troubleshooting)
* [10. Open Source & Licensing](#10-open-source--licensing)
* [11. Downloads](#11-downloads)
* [12. Support](#12-support)

<br clear="left"/>

---

<a id="1-introduction"></a>

# 1. Introduction

## 1.1 Overview of the ENM‑223‑R1 Module ⚡

The **ENM‑223‑R1** is a modular **3‑phase energy metering + I/O** device for power monitoring, automation, and local control. It features **3 voltage channels (L1/L2/L3‑N)**, **3 current channels (external CTs)**, **2 SPDT relays**, **4 user LEDs**, and **4 buttons**—all driven by an **RP2350** MCU with QSPI flash and a dedicated **ATM90E32AS** metering IC.

It integrates with **MiniPLC/MicroPLC** controllers or any **Modbus RTU** master over **RS‑485**, and it’s configured in‑browser via **USB‑C Web Serial** (no drivers). Typical uses include **energy dashboards, demand response, alarm‑driven relay control, and building automation**. First‑boot firmware default is **Modbus address 30 @ 19200 8N1** (changeable in WebConfig; assign a unique plant ID on each RS‑485 segment).

> Quick device flow:  
> **Wire Lx/N/PE + CTs → set address/baud, line Hz, 3P4W/3P3W, phase mapping in WebConfig → calibrate → alarms (L1/L2/L3/Totals) → relay Alarm Controlled or Modbus → RS‑485 → poll Modbus.**

---

## 1.2 Features & Architecture

### Core Capabilities

| Subsystem       | Qty | Description |
|-----------------|-----|-------------|
| Voltage Inputs  | 3   | L1/L2/L3‑N measurement (divider network on FieldBoard) feeding ATM90E32AS |
| Current Inputs  | 3   | Differential CT inputs (IAP/IAN, IBP/IBN, ICP/ICN) with filtering/burdens |
| Relays          | 2   | **SPDT** dry contacts (NO/NC); opto‑driven; alarm‑ or Modbus‑controlled |
| LEDs            | 4   | User LEDs; steady or blink; follow relay logical state |
| Buttons         | 4   | Toggle relay 1/2 when in Modbus mode; state on Modbus DI |
| Metering & Energy | — | ATM90E32AS: Urms/Irms, **signed P/Q**, S, PF, angle, freq; **U/I peaks**, **Irms neutral**, **THD**; import/export energy (AP/AN, RP/RN, VAh) per phase & totals |
| Alarms & PQ | — | Threshold rules (Alarm/Warning/Event) with **hysteresis**; chip events (sag, OV, phase loss, over-I, frequency, phase sequence) as **Event**; Modbus DI 16–27 + ACK coils 16–19 |
| Meter wiring | WebConfig | **Phase mapping** (L1–L3 → phases A/B/C at ATM init); **3P4W / 3P3W** wiring mode |
| Persistence | LittleFS | **Settings** `/enm_cfg.bin` (migrated on `CFG_VERSION` bump); **calibration + energy** `/enm_meter.bin` (kept across firmware updates) |
| Config UI       | Web Serial | In‑browser **WebConfig** over **USB‑C** (Chromium-based: Chrome, Edge, Opera, Brave, Vivaldi; Chrome/Edge 89+, Opera 76+); live meter, calibration, alarms, relays, LEDs, buttons |
| Modbus RTU      | RS‑485 | Multi‑drop slave; address 1…247; baud 9600–115200 (default **19200 8N1**) |
| MCU             | RP2350 + QSPI | Dual‑core MCU, native USB, external W25Q32 flash; RS‑485 via MAX485 transceiver |
| Power           | 24 VDC | Buck to 5 V → 3.3 V LDO; **isolated analog domain** via B0505S DC‑DC + ISO7761 |
| Protection      | TVS, PTC, fuses | Surge/ESD on USB & RS‑485; resettable fuses on field I/O; reverse‑polarity protection |

---

## 1.3 System Role & Communication 🍰

The **ENM‑223‑R1** is a **smart Modbus RTU slave**. It executes local alarm logic (thresholds & acks) and mirrors states/values to a PLC or SCADA via registers/coils. Configuration (meter options, calibration, relay/LED logic, button actions, Modbus address/baud) is done via **USB‑C WebConfig**, stored to non‑volatile memory.

| Role                 | Description |
|----------------------|-------------|
| System Position      | Expansion meter+I/O on the **RS‑485** trunk (A/B/GND) |
| Master Controller    | MiniPLC / MicroPLC or any third‑party Modbus RTU **master** (polling) |
| Address / Baud       | Configurable 1…247 / **9600–115200**; **first-boot default: ID 30 @ 19200 8N1** |
| Bus Type             | RS‑485 half‑duplex; termination/bias per bus rules; share **GND** if separate PSUs |
| USB‑C Port           | Setup/diagnostics via Chromium browser (Web Serial); native USB D+/D− to MCU |
| Default Modbus ID    | **30** on fresh flash (set per site in WebConfig) |
| Daisy‑Chaining       | Multi‑drop on shared A/B; ensure unique IDs and end‑of‑line termination |

> **Note:** Per-channel **Alarm / Warning / Event** rules, **Ack required**, **Alarm Controlled** relays, **phase mapping**, and **3P4W/3P3W** are configured in WebConfig. Modbus exposes live alarm bits (DI 16–27) and ACK coils 16–19.


<a id="2-use-cases"></a>

# 2. Use Cases

This section outlines practical application examples for the **ENM‑223‑R1** module. Each use case includes a functional goal and a clear configuration procedure using the WebConfig tool and/or Modbus RTU integration.

These templates are applicable in energy management, automation, industrial control, and building infrastructure deployments.

---

## 2.1 Overcurrent Alarm with Manual Reset

**Purpose:** Activate **Relay 1** when current exceeds a configured threshold and hold it until manually acknowledged.

### Configuration:
- **Alarms** → Channel: `Totals`  
  - Enable **Alarm**  
  - Metric: `Current (Irms)`  
  - Max threshold: e.g. `> 5000` (for 5 A)  
  - Enable **Ack required**
- **Relays** → Relay 1  
  - Mode: `Alarm Controlled`  
  - Channel: `Totals`, Kinds: `Alarm`
- **LEDs** → LED 1  
  - Source: `Relay 1` (shows trip), Mode: `Steady`
- **Acknowledge**: via Web UI, Modbus coils `16–19` (L1/L2/L3/Total), or front panel button (if assigned)

---

## 2.2 Manual Relay Toggle via Button

**Purpose:** Allow field operators to toggle **Relay 2** with a front-panel button when the relay is **Modbus Controlled**.

### Configuration:
- **Relays** → Relay 2  
  - Mode: `Modbus Controlled`  
  - Enabled at power-on
- **Buttons** → Button 2  
  - Action: `Toggle Relay 2`
- **LEDs** → LED 2  
  - Source: `Relay 2`, Mode: `Steady` or `Blink`

> Button actions apply only in **Modbus Controlled** mode. Use **Alarm Controlled** when the relay must follow meter alarms (local load shed).

---

## 2.3 Environmental Voltage/Frequency Alarm with Auto-Clear

**Purpose:** Detect power quality faults (sag/swell or freq drift), activate **Relay 1** as an output, and auto-reset when back in range.

### Configuration:
- **Alarms** → Channel: `L1`  
  - Enable **Alarm**  
  - Metric: `Voltage (Urms)`  
  - Min: `21000` (210 V), Max: `25000` (250 V)  
  - Leave **Ack required** unchecked
- **Relays** → Relay 1  
  - Mode: `Alarm Controlled`, Channel: `L1`, Kinds: `Alarm`
- **LEDs** → LED 1  
  - Source: `Relay 1`, Mode: `Steady` (or poll Modbus DI **16** for Alarm L1 in PLC/HA)

---

## 2.4 Staged Load Shedding via Modbus Scenes

**Purpose:** Use a controller to shed non-critical loads as power consumption increases.

### Configuration:
- **Relays** → Relay 1 and Relay 2  
  - Mode: `Modbus Controlled`
- In PLC logic:
  - Monitor `Totals S (VA)` via Input Register
  - If `S > 8000`, write coil `0 = OFF` (Relay 1)
  - If `S > 10000`, write coil `1 = OFF` (Relay 2)
  - Restore relays when values drop below defined hysteresis limits

> Ideal for HVAC or lighting where priority-based power shedding is needed.

---

### Summary Table

| Use Case                               | Feature Used                | Reset Method         | Relay Mode         |
|----------------------------------------|-----------------------------|----------------------|--------------------|
| Overcurrent Alarm + Ack                | Alarms, Ack, Relay 1        | Manual (Ack)         | Alarm Controlled   |
| Manual relay toggle via button       | Button → relay              | Button toggle        | Modbus Controlled  |
| Voltage/Frequency Fault Auto-Reset     | Alarm (no ack), Relay       | Auto (value returns) | Alarm Controlled   |
| Load Shedding (Staged Scenes)          | PLC Modbus, Relay 1 & 2     | PLC-controlled       | Modbus Controlled  |

> 🛠 All parameters are configurable via USB‑C WebConfig. Modbus control assumes master-side logic (PLC, SCADA, or MicroPLC/MiniPLC).

---


<a id="3-safety-information"></a>

# 3. Safety Information

These safety guidelines apply to the **ENM‑223‑R1 3‑phase metering and I/O module**. Ignoring them may result in **equipment damage, system failure, or personal injury**.

> ⚠️ **Mixed Voltage Domains** — This device contains both **SELV (e.g., 24 V DC, RS‑485, USB)** and **non-SELV mains inputs (85–265 V AC)**. Proper isolation, wiring, and grounding are required. Never connect SELV and mains GND together.

---

## 3.1 General Requirements

| Requirement           | Detail |
|-----------------------|--------|
| Qualified Personnel   | Installation and servicing must be done by qualified personnel familiar with high-voltage and SELV control systems. |
| Power Isolation       | Disconnect both **24 V DC** and **voltage inputs (Lx/N)** before servicing. Use lockout/tagout where applicable. |
| Environmental Limits  | Mount in a clean, sealed enclosure. Avoid condensation, conductive dust, or vibration. |
| Grounding             | Bond the panel to PE. Wire **PE and N** to the module. Never bridge **GND_ISO** to logic GND. |
| Voltage Compliance    | CT inputs: 1 V or 333 mV RMS only. Voltage inputs: 85–265 V AC. Use upstream fusing and surge protection. |

---

## 3.2 Installation Practices

| Task                | Guidance |
|---------------------|----------|
| ESD Protection       | Handle only by the case. Use antistatic wrist strap and surface when the board is exposed. |
| DIN Rail Mounting    | Mount securely on **35 mm DIN rail** inside an IP-rated cabinet. Allow cable slack for strain relief. |
| Wiring               | Use correct gauge wire and torque terminal screws. Separate relay, CT, and RS‑485 wiring. |
| Isolation Domains    | Respect isolation: **Do not bridge GND_ISO to GND**. Keep analog and logic grounds isolated. |
| Commissioning        | Before power-up, verify voltage wiring, CT polarity, RS‑485 A/B orientation, and relay COM/NO/NC routing. |

---

## 3.3 I/O & Interface Warnings

### ⚡ Power

| Area             | Warning |
|------------------|---------|
| **24 V DC Input** | Use a clean, fused SELV power source. Reverse polarity is protected but may disable the module. |
| **Voltage Input** | Connect **L1/L2/L3/N/PE** only within rated range (85–265 V AC). Use circuit protection upstream. |
| **Sensor Domain** | Use **CTs with 1 V or 333 mV RMS** output. Never apply 5 A directly. Observe polarity and shielding. |

### 🧲 Inputs & Relays

| Area              | Warning |
|-------------------|---------|
| **CT Inputs**      | Accept only voltage-output CTs. Reversing polarity may affect power sign. Use GND_ISO reference. |
| **Relay Outputs**  | Dry contacts only. Rated: **5 A @ 250 VAC or 30 VDC**. Use snubber (RC/TVS) for inductive loads. |

### 🖧 Communication & USB

| Area            | Warning |
|-----------------|---------|
| **RS‑485 Bus**   | Use **twisted pair**. Terminate at both ends. Match A/B polarity. Share GND if powered from different PSUs. |
| **USB-C (Front)**| For **setup only**. Not for permanent field connection. Disconnect during storms or long idle periods. |

### 🎛 Front Panel

| Area               | Warning |
|--------------------|---------|
| **Buttons & LEDs** | Buttons toggle relays in Modbus mode only. Use **Alarm Controlled** relays for safety interlocks. |

### 🛡 Shielding & EMC

| Area             | Recommendation |
|------------------|----------------|
| **Cable Shields** | Terminate at one side only (preferably PLC/controller). Route away from VFDs and high-voltage cabling. |

---

### ✅ Pre‑Power Checklist

- [x] All wiring is torqued, labeled, and strain-relieved  
- [x] **No bridge between logic GND and GND_ISO**  
- [x] PE and N are wired to terminals  
- [x] RS‑485 A/B polarity and 120 Ω termination confirmed  
- [x] Relay loads do **not** exceed 5 A or contact voltage rating  
- [x] CTs installed with correct polarity and securely landed  
- [x] Voltage inputs fused, protected, and within spec (85–265 V AC)

> 🧷 **Tip:** In single-phase installations, energize **L1** and tie **L2/L3 → N** to prevent phantom voltages.



<a id="4-installation-quick-start"></a>

# 4. Installation & Quick Start

The **ENM‑223‑R1** connects to your system over **RS‑485 (Modbus RTU)** and supports configuration via **USB‑C WebConfig**. Setup involves:  
**1) Physical wiring**, **2) Digital setup** (WebConfig → Modbus or PLC/ESPHome control).

---

## 4.1 What You Need

| Category     | Item / Notes |
|--------------|--------------|
| **Hardware** | ENM‑223‑R1 module: DIN-rail, 3 voltage channels, 3 CTs, 2 relays, 4 buttons, 4 LEDs, RS‑485, USB‑C |
| **Controller** | MicroPLC, MiniPLC, or any Modbus RTU master |
| **24 VDC Power (SELV)** | Regulated 24 V DC @ ~100–200 mA |
| **RS‑485 Cable** | Twisted pair for A/B + COM/GND; external 120 Ω end-termination |
| **USB‑C Cable** | For WebConfig setup via Chromium-based browser (Chrome, Edge, Opera, Brave, Vivaldi; Chrome/Edge 89+, Opera 76+) |
| **Software** | Web browser (Web Serial enabled), ConfigToolPage.html |
| **Field Wiring** | L1/L2/L3/N/PE → voltage inputs, CT1/2/3 → external CTs |
| **Load Wiring** | Relay outputs (NO/COM/NC); observe relay max rating and use snubbers on inductive loads |
| **Isolation Domains** | GND (logic) ≠ GND_ISO (metering); never bond these directly |
| **Tools** | Torque screwdriver, ferrules, USB-capable PC, 120 Ω terminators if needed |

---

> **Quick Path**  
> ① Mount → ② wire **24 VDC + RS‑485 (A/B/COM)** → ③ connect **USB‑C** → ④ launch WebConfig →  
> Set **Address/Baud** → assign **Inputs/Relays/LEDs** → confirm data → ⑤ disconnect USB → hand control to Modbus master.

---

## 4.2 Power

The ENM‑223‑R1 uses **24 V DC** input for its interface domain and internally isolates metering circuits.

- **Power Terminals:** Top left: `V+` and `0V`
- **Voltage Range:** 22–28 V DC (nominal 24 V)
- **Typical Current:** 50–150 mA (relays off/on)
- **Protection:** Internally fused, reverse-polarity protected
- **Logic domain:** Powers MCU, RS‑485, LEDs, buttons, relays

### 4.2.1 Sensor Isolation

- **Metering IC** (ATM90E32AS) is powered from an isolated 5 V rail
- Analog domain uses **GND_ISO**, fully isolated from GND
- Do not connect **GND_ISO ↔ GND**; isolation via **B0505S + ISO7761**

> Only voltage inputs (Lx-N) and CTs connect to the isolated domain.

---

### 4.2.2 Power Tips

- **Do not power relays or outputs** from metering-side inputs
- Use separate fusing on L1–L3
- Tie **L2, L3 → N** if using single-phase only (prevents phantom voltage)
- Confirm PE is wired — improves stability & safety

---

## 4.3 Networking & Communication

### 4.3.1 RS‑485 (Modbus RTU)

#### Physical

| Terminals  | Description            |
|------------|------------------------|
| `A`, `B`   | Differential signal pair (twisted/shielded) |
| `COM`/`GND` | Logic reference (tie GNDs if on separate supplies) |

#### Cable & Topology

- Twisted pair (with or without shield)
- Terminate with **120 Ω** at each bus end (not inside module)
- Biasing resistors (pull-up/down) should be on the master

#### Defaults

| Setting       | Value        |
|---------------|--------------|
| Modbus Address | `30` (first boot; set per site in WebConfig) |
| Baud Rate      | `19200` |
| Format         | `8N1` |
| Address Range  | 1–247 |

> 🧷 Reversed A/B will cause CRC errors — check if no response.

---

### 4.3.2 USB‑C (WebConfig)

**Purpose:** Web-based configuration tool over native USB Serial. Supports:
- Live readings
- Address/baudrate config
- Phase mapping
- Relay/button/LED logic
- Alarm setup
- Calibration (gains/offsets)

#### Steps

1. Connect USB‑C to PC (Chromium-based browser)
2. Open `Firmware/v0.2.0/ConfigToolPage.html`  
3. Click **Connect**, select ENM serial port  
4. Configure settings: address, relays, LEDs, alarms, calibration  
5. Click **Save & Disconnect** when finished

> ⚠️ If **Connect** is greyed out: check browser support, OS permissions, and close any other apps using the port.


<a id="installation-wiring"></a>

## 4.4 Installation & Wiring

Use diagrams and explain:
- Inputs
- Relays
- Sensor rails (12/5V)
- RS-485 terminals
- USB port

<a id="software-ui-configuration"></a>

## 4.5 Software & UI Configuration

The **ENM‑223‑R1** is configured using the browser‑based **WebConfig Tool**  
(`Firmware/v0.2.0/ConfigToolPage.html`) over **USB‑C**.  
No drivers or software installation is required — configuration happens directly via **Web Serial API** in any Chromium-based browser (Chrome, Edge, Opera, Brave, Vivaldi; Chrome/Edge 89+, Opera 76+).

> Firefox: experimental only (Nightly with the Web Serial flag enabled). Safari and stable Firefox are not supported.

- WebConfig refreshes live data every 1 s.
- Click into a field to pause refresh for that field.
- **Press Enter** to apply a change.
- All settings are stored in non‑volatile flash.

---

### 1) Modbus Setup (Address & Baud)

<img src="https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/main/ENM-223-R1/Images/webconfig1.png" width="440" alt="WebConfig — Modbus address & baud" width="100%"/>

- Click **Connect** and select the USB serial port.
- The **Active Modbus Configuration** bar shows the current Address and Baud Rate.
- You can configure:
  - **Modbus Address**: `1–247` (first-boot default = `30`)
  - **Baud Rate**: `9600 / 19200 / 38400 / 57600 / 115200` (default = `19200`)
- Changes are live and applied on selection.
- If you change baud or address, remember to reconnect the controller with updated settings.

---

### 2) Meter Options & Calibration

<img src="https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/main/ENM-223-R1/Images/webconfig2.png" width="440" alt="Meter options and calibration" width="100%"/>

#### Meter Options
- **Line Frequency**: 50 / 60 Hz (ATM90E32 `MMode0` + sag thresholds)
- **Sum Mode**:  
  - `0 = algorithmic` (vector sum)  
  - `1 = absolute` (|P1| + |P2| + |P3|)
- **Wiring scheme**: **3P4W** (star, four-wire) or **3P3W** (three-wire) — sets ATM90E32 `MMode0` on apply
- **Phase mapping**: assign each logical channel **L1 / L2 / L3** to incoming meter **phase A / B / C** (written to ATM90E32 `ChannelMapU` / `ChannelMapI` on apply; use when field wiring does not match labels)
- **Sample Interval (ms)**: UI refresh hint only on v0.2.0 (meter poll ~1 s on device)

#### Calibration (per phase A/B/C — maps to L1/L2/L3 after phase mapping)
- **Ugain / Igain**: scaling gains (16-bit, 0–65535)
- **Uoffset / Ioffset**: calibration offsets (signed)
- Press **Enter** after editing to write the value to the module.

---

### 3) Alarms / Inputs (Per‑Channel Rules)

<img src="https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/main/ENM-223-R1/Images/webconfig3.png" width="440" alt="Alarms per channel" width="100%"/>

The ENM has **4 measurement channels**: L1, L2, L3, and Totals.

Each channel supports:
- **3 rule slots**: Alarm, Warning, Event
- **Metric types**:
  - Voltage (Urms)
  - Current (Irms)
  - Active Power P
  - Reactive Power Q
  - Apparent Power S
  - Frequency

You can configure:
- **Enable** toggle
- **Metric**, **Min**, and **Max** thresholds
- **Ack required** — latches the Alarm state until acknowledged

Acknowledgment:
- **Ack L1–L3 / Totals** in WebConfig
- Modbus coils **16–19** (write `1`; device auto-clears)
- Front-panel button mapped to toggle relay (optional local ack workflow via PLC)

**Chip power-quality events** (always published as **Event**, kind = 2 on Modbus DI):

| Event source (ATM90E32) | Channels |
|-------------------------|----------|
| Voltage **sag** | L1, L2, L3 (+ rolled into Total) |
| **Over-voltage** | L1, L2, L3 (+ Total) |
| **Phase loss** | L1, L2, L3 (+ Total) |
| **Over-current** | L1, L2, L3 (+ Total) |
| **Frequency** high/low | Total only |
| **Phase sequence** error | Total only |

Threshold **Alarm** and **Warning** rules use **2 % hysteresis** on the configured min/max band (with metric-specific minimum deadband for voltage, current, and frequency).

> 💡 ENM has no digital inputs (DIs). Alarm rules are virtual inputs driven by live metering and the metering IC status registers.

---

### 4) Relay Logic Modes

<img src="https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/main/ENM-223-R1/Images/webconfig6.png" width="440" alt="Relay logic options" width="100%"/>

Each of the two onboard **SPDT relays** can be configured independently.

Options:

| Setting               | Description |
|-----------------------|-------------|
| **Enabled at Power-On** | Relay state after boot (on/off) |
| **Inverted (active-low)** | Affects **both** relays; sets low = ON |
| **Mode**              | `None`, `Modbus Controlled`, or **`Alarm Controlled`** |
| **Toggle**            | Manually toggle the relay from the UI (Modbus mode only) |
| **Alarm Control Options** | Channel: `L1–L3` or `Totals`; kinds: **Alarm** / **Warning** / **Event** (bitmask) |

In **`Alarm Controlled`** mode the relay **energizes while the selected alarm condition is active** — typical use: **local load shed / trip** on overcurrent or PQ fault. The relay releases when the rule clears (with hysteresis) or after **Ack** when **Ack required** is set. Modbus coils **0/1** do not drive the relay in this mode.

---

### 5) Button & LED Mapping

<img src="https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/main/ENM-223-R1/Images/webconfig5.png" width="440" alt="Buttons and LED mapping" width="100%"/>

#### Buttons (1–4)
Each button can be mapped to:
- `None`
- `Toggle Relay 1` / `Toggle Relay 2` (only when relay is **Modbus Controlled**)

#### User LEDs (1–4)

Each LED has:
- **Mode**: `Steady` or `Blink`
- **Source**: `None`, or logical state of **Relay 1 / Relay 2** (useful to show Alarm Controlled trip)

---

### 6) Live Meter & Energies

<img src="https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/main/ENM-223-R1/Images/webconfig4.png" width="440" alt="Live meter values" width="100%"/>

**Live Meter View**:
- U (V), I (A), **signed P (W)**, **signed Q (var)**, S (VA)
- PF, angle, frequency, temperature
- **Upeak / Ipeak**, **Irms neutral**, **THD** (per phase)
- Per-channel tiles L1–L3 + Totals

**Energies** (import / export labels in WebConfig):
- **Active**: import (AP) / export (AN) / net kWh
- **Reactive**: import (RP) / export (RN) / net kvarh
- **Apparent**: kVAh (S)

> Use this screen to verify CT orientation, load phase mapping, and live alarm behavior during commissioning.

<a id="4-6-getting-started"></a>

## 4.6 Getting Started (3 Phases)

### Phase 1 — Wire

- **24 V DC** to `V+ / GND` (top left terminals)
- **Voltage inputs**: `PE / N / L1 / L2 / L3`  
  - For single-phase: energize **L1 only**, tie **L2/L3 → N**
- **CTs** to `CT1/CT2/CT3` with correct ± polarity (1 V or 333 mV RMS)  
  - Arrow → load; shielded pairs preferred
- **RS‑485 A/B/COM**  
  - Use shielded twisted pair; terminate bus ends with **120 Ω**
- (Optional) **Relay outputs**: `COM/NO/NC`  
  - Add **snubber** on inductive loads (RC/TVS)
- Ground panel PE and avoid bridging **GND ↔ GND_ISO**

👉 See: [Installation & Quick Start](#4-installation--quick-start)

---

### Phase 2 — Configure (WebConfig)

- Open `Firmware/v0.2.0/ConfigToolPage.html` in a Chromium-based browser (Chrome, Edge, Opera, Brave, Vivaldi)
- Connect via **USB‑C** → **Select port → Connect**
- Set:
  - **Modbus Address / Baud**  
  - **Line frequency, sum mode, 3P4W/3P3W, phase mapping**
  - **Alarm thresholds** per L1/L2/L3/Totals
  - **Relay modes**: Alarm or Modbus Controlled
  - Map **Buttons & LEDs** (relay toggle / status)
  - (Optional) Adjust **U/I gains**, save calibration

👉 See: [WebConfig UI](#45-software--ui-configuration)

---

### Phase 3 — Integrate (Controller)

- Connect controller via **RS‑485**
- Match **Modbus address / baud**
- Poll:
  - **Input registers**: meter values (U, I, P, Q, S, PF, angle, kWh, etc.)
  - **Coils**: relays (0/1), Ack (16–19), button state
- Send:
  - **Coil writes**: toggle relays, acknowledge alarms
- Use with:
  - HomeMaster MicroPLC / MiniPLC
  - SCADA / ESPHome

👉 See: [Modbus RTU Communication](#modbus-rtu) & [Integration Guide](#integration)

---

### ✅ Verify

| Area           | What to Check |
|----------------|---------------|
| **LEDs**       | `PWR = ON`; `TX/RX = blink` during comms |
| **Voltage**    | L1–L3 read ~230 V (or phase-neutral voltage) |
| **Current**   

---

<a id="5-module-code--technical-specification"></a>

# 5. ENM-223-R1 — Technical Specification

---

## 5.1 Diagrams & Pinouts

<div align="center">

<table>
<tr>
<td align="center">
<strong>System Diagram</strong><br>
<img src="https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/main/ENM-223-R1/Images/ENM_Diagram.png" alt="System Block Diagram" width="340">
</td>
<td align="center">
<strong>MCU Pinout</strong><br>
<img src="https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/main/ENM-223-R1/Images/ENM_MCU_Pinouts.png" alt="RP2350 MCU Pinout" width="340">
</td>
</tr>
<tr>
<td align="center">
<strong>Field Board Terminal Map</strong><br>
<img src="https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/main/ENM-223-R1/Images/FieldBoard_Diagram.png" alt="Field Board Layout" width="340">
</td>
<td align="center">
<strong>MCU Board Layout</strong><br>
<img src="https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/main/ENM-223-R1/Images/MCUBoard_Diagram.png" alt="MCU Board Layout" width="340">
</td>
</tr>
</table>

</div>

> 💡 **Note:** Pinouts correspond to hardware revision R1. Terminals are pluggable 5.08 mm pitch (26–12 AWG, torque 0.5–0.6 Nm).

---

## 5.2 I/O Summary

| Interface         | Qty | Description |
|-------------------|-----|-------------|
| **Voltage Inputs** | 3 | L1 / L2 / L3–N, 85–265 V AC via precision divider to ATM90E32AS metering IC |
| **Current Inputs** | 3 | CT1–CT3, external 333 mV / 1 V RMS split-core CTs |
| **Relay Outputs** | 2 | SPDT dry contact, HF115F series, opto-driven; 3 A @ 250 VAC / 30 VDC (module limit) |
| **User LEDs** | 4 | Steady/blink; optional mirror of relay 1/2 logical state (GPIO18–21) |
| **Buttons** | 4 | Momentary tactile switches (GPIO22–25) |
| **RS-485** | 1 | A/B/COM, Modbus RTU, MAX485 transceiver |
| **USB-C** | 1 | Native USB 2.0 (Web Serial + firmware flashing), ESD-protected |
| **Power Input** | 1 | 24 V DC (22–28 V) logic supply, reverse & surge protected |

---

## 5.3 Absolute Electrical Specifications

| Parameter | Min | Typ | Max | Unit | Notes |
|------------|-----|-----|-----|------|-------|
| **Supply Voltage (V+)** | 22 | 24 | 28 | V DC | SELV; reverse / surge protected input |
| **Power Consumption** | – | 1.85 | 3.0 | W | Module only, no external loads |
| **Logic Rails** | – | 3.3 / 5 | – | V | Buck (AP64501) + LDO (AMS1117-3.3) |
| **Isolated Sensor Rails** | – | +12 / +5 | – | V | From B0505S-1WR3 isolated DC-DC |
| **Voltage Inputs** | 85 | – | 265 | V AC | Divided to ATM90E32AS AFE |
| **Current Inputs** | – | 1 / 0.333 | – | V RMS | External CTs |
| **Relay Outputs** | – | – | 3 | A | SPDT; 3 A @ 250 VAC/30 VDC module limit; varistor + snubber recommended |
| **RS-485 Bus** | – | 115.2 | – | kbps | MAX485; short-circuit limited; fail-safe bias |
| **USB-C Port** | – | 5 | 5.25 | V DC | Native USB; ESD protected |
| **Operating Temp.** | 0 | – | 40 | °C | ≤ 95 % RH non-condensing |
| **Isolation (DC-DC)** | – | 1.5 | 3.0 | kV DC | Metering domain via B0505S-1WR3 |
| **Isolation (Digital)** | – | 5.0 | – | kV RMS | ISO7761 6-ch isolator between MCU ↔ AFE |

> 🧩 *Values validated from schematics and manufacturer datasheets for ATM90E32AS, ISO7761, B0505S-1WR3, HF115F, AP64501.*

> **Relay component vs module rating:** Relay components (HF115F class) are rated up to **12 A @ 250 VAC** at the device level. **This chip rating does NOT apply to the module** — PCB traces, terminals, and compliance testing limit the **module output to 3 A @ 250 VAC (resistive)**. Use interposing contactors for higher or inductive loads.

---

## 5.4 Connector / Terminal Map (Field Side)

| Block / Label | Pin(s) (left→right) | Function / Signal | Limits / Notes |
|----------------|--------------------|------------------|----------------|
| **POWER** | V+, 0V | 24 V DC SELV input | Reverse / surge protected |
| **VOLTAGE INPUT** | PE, N, L1, L2, L3 | AC sensing (85–265 V AC) | Isolated domain |
| **CT INPUT** | CT1+, CT1–, CT2+, CT2–, CT3+, CT3– | External CT (333 mV / 1 V RMS) | Shielded pairs recommended |
| **RS-485** | A, B, COM | Modbus RTU bus | Terminate 120 Ω at ends |
| **RELAY 1** | NO, C, NC | SPDT dry contact | 3 A @ 250 VAC/30 VDC (module limit) |
| **RELAY 2** | NO, C, NC | SPDT dry contact | 3 A @ 250 VAC/30 VDC (module limit) |
| **USB-C** | D+, D–, VBUS, GND | Web Serial / Setup | Not for field mount |
| **LED / BTN Interface** | – | Internal header MCU ↔ Field Board | Service only |

---

## 5.5 Reliability & Protection Specifics

- **Primary Protection:** Reverse-path diode + MOSFET high-side switch; distributed inline fuses  
- **Isolated rails:** Independent +12 V / +5 V DC with LC filters; isolated returns (GND_ISO)  
- **Inputs:** Per-channel TVS and RC filtering; debounced in firmware  
- **Relays:** Coil driven via SFH6156 optocoupler → S8050 transistor → HF115F SPDT; RC/TVS suppression recommended for inductive loads  
- **RS-485:** TVS (SMAJ6.8CA) + PTC; failsafe bias on idle; TX/RX LED feedback  
- **USB:** PRTR5V0U2X ESD array on D+/D–; CC pull-downs per USB-C spec  
- **Memory Retention:** **LittleFS** — settings `/enm_cfg.bin`, meter `/enm_meter.bin` (see [Firmware/README](Firmware/README.md))

---

## 5.6 Firmware / Functional Overview (v0.2.0)

- **Metering:** ATM90E32AS via SPI — Urms/Irms, **signed** P/Q, S, PF, angle, frequency; hardware **U/I peaks**, **neutral Irms**, **active-power THD**; energies accumulated from chip CF counters (import AP/RP, export AN/RN, apparent SA).
- **Wiring:** WebConfig **phase mapping** (`ChannelMapU/I`) and **3P4W / 3P3W** (`MMode0`) applied on ATM re-init.
- **Alarm engine:** Four channels (L1–L3 + Totals); slots **Alarm / Warning / Event** with min/max metrics and **hysteresis**; **Ack required** latches published Alarm bits until acknowledged.
- **Chip PQ events:** Sag, over-voltage, phase loss, over-current (per phase), frequency and phase-sequence faults surfaced as **Event** on DI 16–27.
- **Relay control:** `None` / `Modbus Controlled` / **`Alarm Controlled`** (local shed while alarm active); invert + enable per relay.
- **Setup:** WebConfig over USB-C; Modbus addr/baud, meter options, calibration, alarms, relays, buttons, LEDs.
- **Data retention (split blobs):**
  - **`/enm_cfg.bin`** — operational settings (`CFG_VERSION` **0x0022**); migrated across compatible firmware bumps; otherwise settings defaults on boot.
  - **`/enm_meter.bin`** — `ucal`, gains/offsets, energy ticks — **preserved** across firmware updates when `METER_VERSION` is unchanged.

---

## 5.7 Mechanical Details

<div align="center">
<img src="https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/main/ENM-223-R1/Images/photo1.png" width="320"><br>
</div>

| Property | Specification |
|-----------|---------------|
| **Mounting** | DIN rail EN 50022 (35 mm) |
| **Material / Finish** | PC / ABS V-0, matte light gray + smoke panel |
| **Dimensions (L × W × H)** | 70 × 90.6 × 67.3 mm (9 division units) |
| **Weight** | ~420 g |
| **Terminals** | 300 V / 20 A / 26–12 AWG (2.5 mm²) / torque 0.5–0.6 Nm / pitch 5.08 mm |
| **Ingress Protection** | IP20 (EN 60529) |
| **Operating Temp.** | 0–40 °C / ≤95 % RH (non-condensing) |

<div align="center">
<img src="https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/main/ENM-223-R1/Images/ENMDimensions.png" alt="Mechanical Dimensions" width="420"><br>
<em>ENM-223-R1 Physical Dimensions (DIN-rail enclosure)</em>
</div>

---

## 5.8 Standards & Compliance

| Standard / Directive | Description |
|----------------------|-------------|
| **Ingress Rating** | IP20 (panel mount only) |
| **Altitude Limit** | ≤ 2000 m |
| **Environment** | RoHS / REACH compliant |

---

<a id="6-modbus-rtu-communication"></a>

# 6. Modbus RTU Communication

The ENM‑223‑R1 communicates over **RS‑485 (Modbus RTU)** and supports:

- Real-time metering via **Input Registers**
- Configuration via **Holding Registers**
- Control and acknowledgment via **Coils**
- Status monitoring via **Discrete Inputs**

The device acts as a **Modbus Slave** and can be polled by a PLC, SCADA, ESPHome, or Home Assistant system.

---

## 6.1 Addressing & Protocol Settings

| Setting          | Value                   |
|------------------|-------------------------|
| Default Address  | `30` first boot (configurable **1–247** via WebConfig or HR0) |
| Baud Rate        | `19200 8N1` (configurable) |
| Physical Layer   | RS‑485 (half-duplex, A/B/COM) |
| Function Codes   | `0x01`, `0x02`, `0x03`, `0x04`, `0x05`, `0x06`, `0x10` |
| Termination      | External 120 Ω at bus ends |
| Fail-safe Bias   | Required on master side |

> 📌 Use the WebConfig tool over USB‑C to set Modbus address and baud rate.

---

## 6.2 Input Registers — Real-Time Telemetry (FC04)

Legacy addresses **0–11** and **20–46** are unchanged. **v0.2.0** adds peaks, neutral current, and THD in previously free slots.

| Address | Type | Metric | Unit | Scaling / notes |
|---------|------|--------|------|-----------------|
| 0–2 | U16 | Urms L1 / L2 / L3 | V | ×0.01 |
| 3–5 | U16 | Irms L1 / L2 / L3 | A | ×0.001 |
| 6 | U16 | Line frequency | Hz | ×0.01 |
| 7 | S16 | Temperature (internal) | °C | 1 |
| 8–11 | S16 | Power factor L1 / L2 / L3 / Total | – | ×0.001 |
| 12–14 | U16 | **Upeak** L1 / L2 / L3 | V | ×0.01 (ATM peak detector) |
| 15–17 | U16 | **Ipeak** L1 / L2 / L3 | A | ×0.001 |
| 18 | U16 | **Irms neutral** | A | ×0.001 |
| 19 | — | *reserved / free* | — | — |
| 20, 22, 24, 26 | **S32** | **Active power** L1 / L2 / L3 / Total | W | signed; import (+) / export (−) |
| 28, 30, 32, 34 | **S32** | **Reactive power** L1 / L2 / L3 / Total | var | signed |
| 36, 38, 40, 42 | S32 | Apparent power L1 / L2 / L3 / Total | VA | unsigned magnitude |
| 44–46 | S16 | Phase angle L1 / L2 / L3 | ° | ×0.1 |
| 47–49 | U16 | **THD** (active-power harmonic ratio) L1 / L2 / L3 | % | ×0.01 |
| 50–59 | — | *free* | — | — |

> 32-bit values use **two consecutive input registers** (high word first at base address).

---

## 6.3 Energy Registers (Wh / varh / VAh, FC04)

Import/export naming matches ATM90E32 forward/reverse energy accumulators:

| Address (base) | Type | Energy | Phase / Total | Unit | Meaning |
|----------------|------|--------|---------------|------|---------|
| 60, 62, 64, 66 | U32 | **Active import (AP)** | L1 / L2 / L3 / Total | Wh | Import kWh |
| 68, 70, 72, 74 | U32 | **Active export (AN)** | L1 / L2 / L3 / Total | Wh | Export kWh |
| 76, 78, 80, 82 | U32 | **Reactive import (RP)** | L1 / L2 / L3 / Total | varh | Import kvarh |
| 84, 86, 88, 90 | U32 | **Reactive export (RN)** | L1 / L2 / L3 / Total | varh | Export kvarh |
| 92, 94, 96, 98 | U32 | Apparent energy (SA) | L1 / L2 / L3 / Total | VAh | kVAh |

> Energy values are **32-bit unsigned integers** (two 16-bit registers per value). Net active energy ≈ AP − AN (compute in master).

---

## 6.4 Coils — Output Control (FC01/05)

| Address | Description |
|---------|-------------|
| 0 | Relay 1 (maintained ON/OFF) |
| 1 | Relay 2 (maintained ON/OFF) |
| 16–19 | Alarm acknowledge L1 / L2 / L3 / Total (write `1`; device auto-clears) |

---

## 6.5 Discrete Inputs — Read-only Status (FC02)

| Address | Description |
|---------|-------------|
| 0–3 | LED 1–4 physical state |
| 4–7 | Button 1–4 pressed |
| 8–9 | Relay 1–2 **logical** state (after mode/invert) |
| 16–27 | **Alarm flags** — `addr = 16 + channel×3 + kind` |

| Channel | Alarm (kind 0) | Warning (kind 1) | Event (kind 2) |
|---------|----------------|------------------|----------------|
| L1 | 16 | 17 | 18 |
| L2 | 19 | 20 | 21 |
| L3 | 22 | 23 | 24 |
| Total | 25 | 26 | 27 |

**Event** bits include ATM90E32 power-quality faults (sag, OV, phase loss, over-I, frequency, phase sequence) in addition to user **Event** threshold rules.

Meter calibration, alarm thresholds, phase mapping, wiring mode, and relay/LED mapping are configured via **WebConfig** (not exposed as Modbus holding registers in v0.2.0).

---

## 6.6 Holding Registers — Bus & Meter Options (FC03/06/16)

Writable registers (changes auto-saved to `/enm_cfg.bin`):

| Address | Type | Description |
|---------|------|-------------|
| 0 | U16 | Modbus slave address (1–247) |
| 1–2 | U32 | Baud rate (9600 / 19200 / 38400 / 57600 / 115200) |
| 4 | U16 | Line frequency `50` or `60` (queues ATM re-init) |
| 5 | U16 | Sum mode `0` = algebraic, `1` = absolute (queues ATM re-init) |
| 6 | — | *reserved* |
| 7–8 | U16 | Relay 1 / 2 **enable** at boot |

Phase mapping and 3P4W/3P3W are **WebConfig-only** in v0.2.0.

---

## 6.7 Scaling Summary

| Metric         | Register Type | Scale Factor |
|----------------|----------------|--------------|
| Voltage (V)    | Input Register  | ÷100         |
| Current (A)    | Input Register  | ÷1000        |
| Upeak / Ipeak  | Input Register  | ÷100 / ÷1000 |
| Irms neutral   | Input Register  | ÷1000        |
| THD (%)        | Input Register  | ÷100         |
| Power Factor   | Input Register  | ÷1000        |
| Frequency (Hz) | Input Register  | ÷100         |
| Angle (°)      | Input Register  | ÷10          |
| P / Q (W, var) | S32 Input       | 1 (signed)   |
| Energy (Wh)    | U32 Input       | 1            |

---

## 6.8 Polling Best Practices

- **Typical polling rate:** 1 s for live data (powers, voltages, current)  
- **Energy:** poll less often (e.g. every 5–10 s)  
- **Batch reads:** Use FC04 and FC03 with multi-register reads for speed  
- **Avoid writing frequently** to holding registers during normal operation (settings auto-save to flash)
- **Coils:** Relay and ACK writes are edge-triggered; ACK coils auto-clear

> 🛠 To reduce bus collisions, stagger multiple ENMs on a shared RS‑485 bus using different **poll intervals** and address spacing.

---

## 6.9 Modbus Integration Example (MiniPLC)

```yaml
modbus_controller:
  - id: enm223
    address: 30
    modbus_id: rtu_bus
    update_interval: 1s

sensor:
  - platform: modbus_controller
    modbus_controller_id: enm223
    name: "Urms L1"
    register_type: read
    address: 0
    value_type: U_WORD
    unit_of_measurement: "V"
    accuracy_decimals: 2
    filters:
      - multiply: 0.01

switch:
  - platform: modbus_controller
    modbus_controller_id: enm223
    name: "Relay 1"
    register_type: coil
    address: 0
```

<a id="7-esphome-integration-guide"></a>

# 7. ESPHome Integration Guide (MicroPLC/MiniPLC + ENM‑223‑R1)

The HomeMaster controller (MiniPLC or MicroPLC) running **ESPHome** acts as the **Modbus RTU master** over RS‑485. It polls one or more ENM‑223‑R1 modules and publishes all sensors, relays, LEDs, and alarms into **Home Assistant**.

No Home Assistant add-ons are required — all logic runs on the ESPHome controller.

---

## 7.1 Architecture & Data Flow

- **Topology**: Home Assistant → ESPHome (MicroPLC) → RS‑485 → ENM‑223‑R1
- **Roles**:
  - **ENM**: metering, alarm rules, relays, LEDs, buttons
  - **ESPHome**: Modbus polling, sensor/relay control, entity publishing
  - **HA**: dashboards, energy view, automations

> LED mappings, alarm logic, and relay modes are configured on the ENM module (via WebConfig). Home Assistant reads telemetry and alarm bits over Modbus.

---

## 7.2 Prerequisites (Power, Bus, I/O)

### 1. Power
- **ENM**: 24 V DC → V+ / 0V
- **Controller**: per spec
- If separate PSUs: share COM/GND between controller and ENM

### 2. RS‑485 Bus
- A—A, B—B (twisted pair), COM shared
- Terminate with 120 Ω resistors at both ends
- Default speed: **19200 baud**, set in WebConfig

### 3. Field I/O
- Voltage inputs: L1, L2, L3, N, PE
- CTs: CT1–CT3 (1 V or 333 mV)
- Relays: dry contact, driven by internal logic or Modbus
- Buttons / LEDs: wired to MCU, mapped in firmware/UI

---

## 7.3 ESPHome Minimal Config (Enable Modbus + Import ENM Package)

```yaml
uart:
  id: uart1
  tx_pin: 17
  rx_pin: 16
  baud_rate: 19200
  stop_bits: 1

modbus:
  id: rtu_bus
  uart_id: uart1

modbus_controller:
  - id: enm223_1
    address: 30
    modbus_id: rtu_bus
    update_interval: 1s

packages:
  enm223_1:
    url: https://github.com/isystemsautomation/homemaster-dev
    ref: main
    files:
      - path: ENM-223-R1/Firmware/v0.2.0/default_enm_223_r1_plc/default_enm_223_r1_plc.yaml
        vars:
          enm_id: enm223_1
          enm_address: 30
          enm_prefix: "ENM #1"
```

---

## 7.4 Entities Exposed by the Package

### Binary Sensors
- Relay states
- Button presses
- LED status
- Alarm conditions (Alarm / Warning / Event)

### Sensors
- **Urms, Irms** L1/L2/L3; **Upeak, Ipeak**; **Irms neutral**; **THD**
- **Signed P, Q** and **S** per phase + totals
- **Frequency**, **PF**, **Angle**, **temperature**
- **Energies**: import/export AP/AN, RP/RN, SA (Wh / varh / VAh)

### Switches
- **Relay 1/2** (Modbus coils 0/1 — effective only in Modbus Controlled mode)
- **Acknowledge** coils 16–19 (L1, L2, L3, Total)

---

## 7.6 Using Your MiniPLC YAML with ENM

1. Keep existing `uart:` and `modbus:` blocks  
2. Add the `packages:` block (as shown) and set `enm_address` from WebConfig  
3. Flash the controller — ESPHome discovers all sensors/entities automatically  
4. Add HA dashboard cards and `switches` for relay control and alarm acknowledge  

---

## 7.7 Home Assistant Setup & Automations

- Go to: **Settings → Devices & Services → ESPHome → Add** by hostname or IP
- Dashboard auto-discovers:
  - Energies (for HA Energy view)
  - Relays, buttons, LEDs
  - Alarm states
- You can create:
  - **Energy Dashboard** source: `VAh Total` or `AP Total`
  - **Automation**:


---



<a id="8-programming--customization"></a>

# 8. Programming & Customization

## 8.1 Supported Languages

- **MicroPython**
- **C/C++**
- **Arduino IDE**

---

## 8.2 Flashing via USB-C

1. Connect USB-C to your PC.
2. Enter boot/flash mode if required.
3. Upload the provided firmware/source.

**Boot/Reset combinations:**

- **Buttons 1 + 2** → forces the module into **BOOT mode**
- **Buttons 3 + 4** → triggers a hardware **RESET**

These combinations are handled in hardware. Use them when flashing or manually rebooting the module.

**🧭 Button layout reference:**

<img src="https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/main/ENM-223-R1/Images/buttons1.png" alt="Button layout" width="360"/>

---

## 8.3 Arduino IDE Setup

- **Board Profile:** Generic RP2350
- **Flash Size:** 2MB (Sketch: 1MB, FS: 1MB)
- **Recommended Libraries:**

```cpp
#include <Arduino.h>
#include <SPI.h>
#include <SimpleWebSerial.h>
#include <Arduino_JSON.h>
#include <LittleFS.h>
#include <math.h>
#include <limits>
```

- **Pin Notes:**
  - Buttons: GPIO22–25
  - LEDs: GPIO18–21
  - RS-485: MAX485, DE/RE control over TX
  - USB: native, no UART bridge

---

## 8.4 Firmware Updates

- **Upload via USB-C** using Arduino IDE (see [Firmware/README.md](Firmware/README.md))
- Enter **boot mode** (Buttons 1 + 2)
- Open sketch `Firmware/v0.2.0/default_enm_223_r1/default_enm_223_r1.ino`

**What is preserved:**

| Data | File | Typical firmware update |
|------|------|-------------------------|
| Calibration (U/I gains & offsets), energy counters | `/enm_meter.bin` | **Kept** while `METER_VERSION` unchanged |
| Modbus address, alarms, relays, phase map, wiring mode | `/enm_cfg.bin` | **Migrated** when `CFG_VERSION` migration exists; otherwise **settings defaults** on boot |

Recommission alarms, relay modes, bus address, and phase mapping in WebConfig after major firmware upgrades if the module boots with factory settings.


---

<a id="9-maintenance--troubleshooting"></a>

# 9. Maintenance & Troubleshooting

| Symptom               | Fix or Explanation                            |
|------------------------|-----------------------------------------------|
| Relay won’t activate   | Check mode: **Alarm Controlled** follows alarms; **Modbus** only via coils 0/1 |
| RS-485 not working     | A/B reversed or un-terminated bus             |
| LED doesn’t light up   | Reassign source in WebConfig or check GPIO18–21 |
| Button unresponsive    | Test DI 4–7; buttons only toggle relays in Modbus mode |
| CRC Errors             | Confirm baud, address, and wiring (A/B swap)  |
| Negative P/Q reading   | Expected for export; flip CT or adjust **phase mapping** in WebConfig |

---

<a id="10-open-source--licensing"></a>

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

The following key project resources are included in this repository:

- 🧠 **Firmware (v0.2.0)** — [Firmware/README.md](Firmware/README.md) · sketch [`default_enm_223_r1.ino`](Firmware/v0.2.0/default_enm_223_r1/default_enm_223_r1.ino)  
  Modbus RTU, signed P/Q, peaks/neutral/THD, alarm engine, Alarm Controlled relays, phase mapping, 3P4W/3P3W, split LittleFS persistence.

- 🧰 **WebConfig Tool**  
  [`Firmware/v0.2.0/ConfigToolPage.html`](Firmware/v0.2.0/ConfigToolPage.html)  
  USB Web Serial: meter options, calibration, alarms, relays, live import/export energy.

- 📦 **ESPHome YAML (v0.2.0)**  
  [`default_enm_223_r1_plc.yaml`](Firmware/v0.2.0/default_enm_223_r1_plc/default_enm_223_r1_plc.yaml)  
  Sensors (incl. peaks, THD, signed P/Q), alarm DI, relay/ACK switches.

- 🧠 **Legacy firmware (v0.1.0)** — [`Firmware/v0.1.0/`](Firmware/v0.1.0/) (frozen line)

- 🖼 **Images & UI Diagrams**  
  [`Images/`](Images/)  
  Front-panel photos, system diagrams, wiring illustrations, WebConfig screenshots.

- 📐 **Hardware Schematics**  
  [`Schematics/`](Schematics/)  
  PDF schematics for Field Board and MCU Board.

- 📄 **Datasheets & Manuals**  
  [`ENM-223-R1_Datasheet.pdf`](Manuals/ENM-223-R1_Datasheet.pdf)

> 🔁 Latest releases can also be found in the [Releases](../../releases) tab or in the `Firmware/` directory.

---

# 12. Support

If you need help using or configuring the ENM‑223‑R1 module, the following support options are available:

### 🛠 Official Resources

- 🧰 [WebConfig Tool (USB-C)](https://config.home-master.eu/ENM-223-R1/Firmware/v0.2.0/ConfigToolPage.html)  
  Configure the module directly from your browser — no drivers or software required.

- 📘 [Official Support Portal](https://www.home-master.eu/support)  
  Includes setup guides, firmware help, diagnostics, and contact form.

---

### 📡 Community & Updates

- 🔧 [Hackster Projects](https://hackster.io/homemaster) — System integration, code samples, wiring  
- 📺 [YouTube Channel](https://youtube.com/@HomeMaster) — Module demos, walkthroughs, and tutorials  
- 💬 [Reddit Community](https://reddit.com/r/HomeMaster) — Questions, answers, contributions  
- 📸 [Instagram](https://instagram.com/home_master.eu) — Visual updates and field applications

---

## Compliance & Certifications

The ENM-223-R1 module is CE marked. **ISYSTEMS AUTOMATION S.R.L.** (HomeMaster® brand)
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
| EU Declaration of Conformity (DoC) | [DoC-ENM-223-R1-V1.0 (1).pdf](./Manuals/DoC-ENM-223-R1-V1.0%20%281%29.pdf) |
| Datasheet | [ENM-223-R1_Datasheet.pdf](./Manuals/ENM-223-R1_Datasheet.pdf) |

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
