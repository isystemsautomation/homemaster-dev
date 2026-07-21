![Modbus](https://img.shields.io/badge/Protocol-Modbus%20RTU-brightgreen)
![License](https://img.shields.io/badge/License-GPLv3%20%2F%20CERN--OHL--W-blue)

# ENM-223-R1 — 3-Phase Power Metering & I/O Module

**HOMEMASTER – Modular control. Custom logic.**

![MODULE photo](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/main/ENM-223-R1/Images/photo1.png)

**Document map:** [§1 Overview](#1-overview) · [§3 Specifications](#3-specifications) · [§4 Hardware](#4-hardware--interface) · [§5 Getting Started](#5-getting-started) · [§6 WebConfig](#6-webconfig-reference) · [§7 Modbus map](#7-modbus-register-map) · [§8 ESPHome](#8-esphome--home-assistant-integration) · [§9 Programming](#9-programming--build) · [§11 Downloads](#11-downloads--resources)

---

## 1. Overview

The **ENM-223-R1** is a configurable smart I/O module designed for **3-phase power quality and energy metering**.  
It includes **3 voltage inputs, 3 current channels**, **2 relays**, and optional **4 buttons / 4 LEDs**, with configuration via **WebConfig** using **USB-C (Web Serial)**.  
It connects over **RS-485 (Modbus RTU)** to a **MicroPLC/MiniPLC**, enabling use in **energy monitoring, automation, and smart building applications**.

The **ENM‑223‑R1** is a modular **3‑phase energy metering + I/O** device for power monitoring, automation, and local control. It features **3 voltage channels (L1/L2/L3‑N)**, **3 current channels (external CTs)**, **2 SPDT relays**, **4 user LEDs**, and **4 buttons**—all driven by an **RP2350** MCU with QSPI flash and a dedicated **ATM90E32AS** metering IC.

It integrates with **MiniPLC/MicroPLC** controllers or any **Modbus RTU** master over **RS‑485**, and it’s configured in‑browser via **USB‑C Web Serial** (no drivers). Typical uses include **energy dashboards, demand response, alarm‑driven relay control, and building automation**. First‑boot firmware default is **Modbus address 3 @ 19200 8N1** (changeable in WebConfig; assign a unique plant ID on each RS‑485 segment).

> Quick device flow:  
> **Wire Lx/N/PE + CTs → set address/baud, line Hz, 3P4W/3P3W, phase mapping in WebConfig → calibrate → alarms (L1/L2/L3/Totals) → relay Alarm Controlled or Modbus → RS‑485 → poll Modbus.**

The **ENM‑223‑R1** is a **smart Modbus RTU slave**. It executes local alarm logic (thresholds & acks) and mirrors states/values to a PLC or SCADA via registers/coils. Configuration (meter options, calibration, relay/LED logic, button actions, Modbus address/baud) is done via **USB‑C WebConfig**, stored to non‑volatile memory.

**Key capabilities at a glance:**

- **3-phase metering** — Urms/Irms (+neutral), signed P/Q, S, PF, angle, frequency, temperature, THD; primary Wh/varh energy on Modbus FC04 `0..85`
- **2 SPDT relays** — None / Modbus / Alarm modes; state on IR0 bits 8/9
- **4 buttons / 4 LEDs** — toggle / Ack actions; LED alarm-kind sources
- **Alarm engine** — L1–L3/Total × Alarm/Warning/Event; chip PQ in IR 3–6; ACK coils 16–19
- **CT ratio + PGA** — primary-side reporting; energy decoupled from settings changes
- **Driverless WebConfig** — USB-C + Chromium browser; persistent LittleFS settings
- **ESPHome / HA** — ready-made YAML package

| Role                 | Description |
|----------------------|-------------|
| System Position      | Expansion meter+I/O on the **RS‑485** trunk (A/B/GND) |
| Master Controller    | MiniPLC / MicroPLC or any third‑party Modbus RTU **master** (polling) |
| Address / Baud       | Configurable 1…247 / **9600–115200**; **first-boot default: ID 3 @ 19200 8N1** |
| Bus Type             | RS‑485 half‑duplex; termination/bias per bus rules; share **GND** if separate PSUs |
| USB‑C Port           | Setup/diagnostics via Chromium browser (Web Serial); native USB D+/D− to MCU |
| Default Modbus ID    | **3** on fresh flash (set per site in WebConfig) |
| Daisy‑Chaining       | Multi‑drop on shared A/B; ensure unique IDs and end‑of‑line termination |

> **Note:** Per-channel **Alarm / Warning / Event** rules, **Ack required**, **Alarm Controlled** relays, **phase mapping**, **CT ratio / PGA**, and **3P4W/3P3W** are configured in WebConfig. Modbus exposes alarm flags in IR2, chip events in IR3–6, and write-only ACK coils 16–19.

---

## 2. Features

| Subsystem       | Qty | Description |
|-----------------|-----|-------------|
| **Identity** | — | Firmware **`0.2.0`** · Model ID **`2`** · `HM_MAP_VERSION` **`0x0020`** · `CFG_VERSION` **`0x0025`** · `METER_VERSION` **`0x0003`** |
| Voltage Inputs  | 3   | L1/L2/L3‑N measurement (divider network on FieldBoard) feeding ATM90E32AS |
| Current Inputs  | 3   | Differential CT inputs (IAP/IAN, IBP/IBN, ICP/ICN); **CT ratio + PGA** set in WebConfig; primary-side reporting |
| Relays          | 2   | **SPDT** dry contacts; modes **None / Modbus / Alarm**; state mirrored on IR0 bits 8/9 |
| LEDs            | 4   | User LEDs; steady or blink; source = None / relay / **alarm-kind per channel** (L1–L3/Total) |
| Buttons         | 4   | Toggle Relay 1/2, or **Ack** all / per channel (L1–L3/Total); state on IR0 bits 4–7 |
| Metering & Energy | — | ATM90E32AS: Urms/Irms (+neutral), **signed P/Q**, S, PF, angle, freq, THD; **primary Wh/varh** import/export energy (per phase + totals). Peaks / Pfund / Pharm / VAh: WebConfig live view only |
| Alarms & PQ | — | L1/L2/L3/Total × Alarm/Warning/Event; min/max metrics; optional ack latch; chip PQ bits in IR 3–6; ACK coils 16–19 |
| Meter wiring | WebConfig | **Phase mapping**; **3P4W / 3P3W**; line frequency; sum-abs; CT ratio; PGA |
| Persistence | LittleFS | **Settings** `/enm_cfg.bin` (`CFG_VERSION` **0x0025**); **calibration + energy** `/enm_meter.bin` (`METER_VERSION` **0x0003**) — energy never wiped by a settings change |
| Config UI       | Web Serial | In‑browser **WebConfig** over **USB‑C** (Chromium); live meter, CT/PGA, calibration, alarms, relays, LEDs, buttons |
| Modbus RTU      | RS‑485 | Slave; address 1…247; baud 9600–115200; first-boot **ID 3 @ 19200 8N1**; FC04 **`0..85`** one-sweep; **no FC02** |
| MCU             | RP2350 + QSPI | Dual‑core MCU, native USB, external W25Q32 flash; RS‑485 via MAX485 transceiver |
| Power           | 24 VDC | Buck to 5 V → 3.3 V LDO; **isolated analog domain** via B0505S DC‑DC + ISO7761 |
| Protection      | TVS, PTC, fuses | Surge/ESD on USB & RS‑485; resettable fuses on field I/O; reverse‑polarity protection |

### Firmware / functional overview (v0.2.0)

- **Identity:** Firmware **`0.2.0`** · Model ID **`2`** · `HM_MAP_VERSION` **`0x0020`** · `CFG_VERSION` **`0x0025`** · `METER_VERSION` **`0x0003`**.
- **Metering (Modbus):** ATM90E32AS — Urms/Irms (+neutral), **signed** P/Q, S, PF, angle, frequency, temperature, THD; **primary Wh/varh** import/export energy (per phase + totals). Contiguous FC04 **`0..85`**.
- **WebConfig-only live metrics:** U/I peaks, fundamental/harmonic power, apparent energy (VAh), harmonic active energy (kept off the bus map for a single-sweep 86-register read).
- **CT / PGA:** Configurable CT ratio (primary A : secondary mA) and PGA ×1/×2/×4; currents, power and energy reported on the **primary** side. Energy counters are **decoupled** from CT/calibration changes.
- **Wiring:** WebConfig **phase mapping** and **3P4W / 3P3W** applied on ATM re-init; line frequency; sum-abs mode.
- **Alarm engine:** Four channels (L1–L3 + Total) × **Alarm / Warning / Event**; min/max metrics; optional **ack-required** latch. Chip PQ bits in IR 3–6; ACK coils 16–19.
- **Relay control:** `None` / `Modbus` / **`Alarm`**; state read from IR0 bits 8/9 (coils 0/1 are write-only commands).
- **LEDs / buttons:** LED sources include alarm-kind per channel; buttons can toggle relays or Ack.
- **Setup:** WebConfig over USB-C (Chromium); reboot / save / energy-reset are WebConfig-only (not on Modbus).
- **Data retention (split blobs):**
  - **`/enm_cfg.bin`** — operational settings (`CFG_VERSION` **0x0025**).
  - **`/enm_meter.bin`** — calibration + energy (`METER_VERSION` **0x0003**) — **preserved** across settings changes and compatible firmware updates.

### Applications

Typical uses for the ENM-223-R1:

- **Overcurrent alarm with manual reset** — trip a relay on overcurrent and hold until Ack (WebConfig alarm rules + ACK coils 16–19).
- **Manual relay toggle via button** — front-panel button toggles a relay when that relay is Modbus-controlled.
- **Voltage / frequency environmental alarm** — detect sag/swell/frequency drift, trip a relay, auto-clear when back in range.
- **Staged load shedding via Modbus** — a PLC sheds loads as consumption rises by writing relay coils over RS-485.

Detailed WebConfig steps for these patterns are in [§6 WebConfig Reference](#6-webconfig-reference).

---

## 3. Specifications

### 3.1 I/O summary

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

### 3.2 Electrical ratings

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

> **Relay component vs module rating:** Relay components (HF115F class) are rated up to **16 A @ 250 VAC** at the device level. **This chip rating does NOT apply to the module** — PCB traces, terminals, and compliance testing limit the **module output to 3 A @ 250 VAC (resistive)**. The margin is deliberate: at 3 A the contacts work far below their rating, so arcing stays low and the contacts do not burn. Use interposing contactors for higher or inductive loads.

### 3.3 Mechanical & environmental

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

### 3.4 Communication defaults

Factory settings applied to every new module:

| Parameter | Default |
|-----------|---------|
| **Modbus Address** | `3` |
| **Baud Rate** | `19200` |
| **Parity** | `None` |
| **Stop Bits** | `1` |

Address **1–247**; baud 9600 / 19200 / 38400 / 57600 / 115200. **Set via [WebConfig](#6-webconfig-reference) over USB-C — recommended.**

> 🧷 Reversed A/B will cause CRC errors — check if no response.

The module communicates over **RS-485 Modbus RTU** (A/B differential + shared COM/GND). Configuration is stored persistently in **LittleFS** and can be changed live through **USB-C + WebConfig**.

### 3.5 Reliability & protection

- **Primary Protection:** Reverse-path diode + MOSFET high-side switch; distributed inline fuses  
- **Isolated rails:** Independent +12 V / +5 V DC with LC filters; isolated returns (GND_ISO)  
- **Inputs:** Per-channel TVS and RC filtering; debounced in firmware  
- **Relays:** Coil driven via SFH6156 optocoupler → S8050 transistor → HF115F SPDT; RC/TVS suppression recommended for inductive loads  
- **RS-485:** TVS (SMAJ6.8CA) + PTC; failsafe bias on idle; TX/RX LED feedback  
- **USB:** PRTR5V0U2X ESD array on D+/D–; CC pull-downs per USB-C spec  
- **Memory Retention:** **LittleFS** — settings `/enm_cfg.bin`, meter `/enm_meter.bin` (see [Firmware/README](Firmware/README.md))

### Standards & compliance

| Standard / Directive | Description |
|----------------------|-------------|
| **Ingress Rating** | IP20 (panel mount only) |
| **Altitude Limit** | ≤ 2000 m |
| **Environment** | RoHS / REACH compliant |

---

## 4. Hardware & Interface

### 4.1 Diagrams & pinouts

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

### 4.2 Connectors & terminal map

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

### 4.3 Front panel — buttons & LEDs

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

## 5. Getting Started

### 5.1 Safety *(read before wiring)*

These safety guidelines apply to the **ENM‑223‑R1 3‑phase metering and I/O module**. Ignoring them may result in **equipment damage, system failure, or personal injury**.

> ⚠️ **Mixed Voltage Domains** — This device contains both **SELV (e.g., 24 V DC, RS‑485, USB)** and **non-SELV mains inputs (85–265 V AC)**. Proper isolation, wiring, and grounding are required. Never connect SELV and mains GND together.

---

#### General requirements

| Requirement           | Detail |
|-----------------------|--------|
| Qualified Personnel   | Installation and servicing must be done by qualified personnel familiar with high-voltage and SELV control systems. |
| Power Isolation       | Disconnect both **24 V DC** and **voltage inputs (Lx/N)** before servicing. Use lockout/tagout where applicable. |
| Environmental Limits  | Mount in a clean, sealed enclosure. Avoid condensation, conductive dust, or vibration. |
| Grounding             | Bond the panel to PE. Wire **PE and N** to the module. Never bridge **GND_ISO** to logic GND. |
| Voltage Compliance    | CT inputs: 1 V or 333 mV RMS only. Voltage inputs: 85–265 V AC. Use upstream fusing and surge protection. |

---

#### Installation practices

| Task                | Guidance |
|---------------------|----------|
| ESD Protection       | Handle only by the case. Use antistatic wrist strap and surface when the board is exposed. |
| DIN Rail Mounting    | Mount securely on **35 mm DIN rail** inside an IP-rated cabinet. Allow cable slack for strain relief. |
| Wiring               | Use correct gauge wire and torque terminal screws. Separate relay, CT, and RS‑485 wiring. |
| Isolation Domains    | Respect isolation: **Do not bridge GND_ISO to GND**. Keep analog and logic grounds isolated. |
| Commissioning        | Before power-up, verify voltage wiring, CT polarity, RS‑485 A/B orientation, and relay COM/NO/NC routing. |

---

#### I/O & interface warnings

#### ⚡ Power

| Area             | Warning |
|------------------|---------|
| **24 V DC Input** | Use a clean, fused SELV power source. Reverse polarity is protected but may disable the module. |
| **Voltage Input** | Connect **L1/L2/L3/N/PE** only within rated range (85–265 V AC). Use circuit protection upstream. |
| **Sensor Domain** | Use **CTs with 1 V or 333 mV RMS** output. Never apply 5 A directly. Observe polarity and shielding. |

#### 🧲 Inputs & Relays

| Area              | Warning |
|-------------------|---------|
| **CT Inputs**      | Accept only voltage-output CTs. Reversing polarity may affect power sign. Use GND_ISO reference. |
| **Relay Outputs**  | Dry contacts only. Rated: **3 A @ 250 VAC or 30 VDC** (module limit). Use snubber (RC/TVS) for inductive loads. |

#### 🖧 Communication & USB

| Area            | Warning |
|-----------------|---------|
| **RS‑485 Bus**   | Use **twisted pair**. Terminate at both ends. Match A/B polarity. Share GND if powered from different PSUs. |
| **USB-C (Front)**| For **setup only**. Not for permanent field connection. Disconnect during storms or long idle periods. |

#### 🎛 Front Panel

| Area               | Warning |
|--------------------|---------|
| **Buttons & LEDs** | Buttons toggle relays in Modbus mode only. Use **Alarm Controlled** relays for safety interlocks. |

#### 🛡 Shielding & EMC

| Area             | Recommendation |
|------------------|----------------|
| **Cable Shields** | Terminate at one side only (preferably PLC/controller). Route away from VFDs and high-voltage cabling. |

---

#### ✅ Pre‑Power Checklist

- [x] All wiring is torqued, labeled, and strain-relieved  
- [x] **No bridge between logic GND and GND_ISO**  
- [x] PE and N are wired to terminals  
- [x] RS‑485 A/B polarity and 120 Ω termination confirmed  
- [x] Relay loads do **not** exceed 3 A or contact voltage rating  
- [x] CTs installed with correct polarity and securely landed  
- [x] Voltage inputs fused, protected, and within spec (85–265 V AC)

> 🧷 **Tip:** In single-phase installations, energize **L1** and tie **L2/L3 → N** to prevent phantom voltages.

### 5.2 What you need

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

> **Quick Path**  
> ① Mount → ② wire **24 VDC + RS‑485 (A/B/COM)** → ③ connect **USB‑C** → ④ launch WebConfig →  
> Set **Address/Baud** → assign **Inputs/Relays/LEDs** → confirm data → ⑤ disconnect USB → hand control to Modbus master.

### 5.3 Power notes

The ENM‑223‑R1 uses **24 V DC** input for its interface domain and internally isolates metering circuits.

- **Power Terminals:** Top left: `V+` and `0V`
- **Voltage Range:** 22–28 V DC (nominal 24 V)
- **Typical Current:** 50–150 mA (relays off/on)
- **Protection:** Internally fused, reverse-polarity protected
- **Logic domain:** Powers MCU, RS‑485, LEDs, buttons, relays

#### Sensor isolation

- **Metering IC** (ATM90E32AS) is powered from an isolated 5 V rail
- Analog domain uses **GND_ISO**, fully isolated from GND
- Do not connect **GND_ISO ↔ GND**; isolation via **B0505S + ISO7761**

> Only voltage inputs (Lx-N) and CTs connect to the isolated domain.

---

#### Power tips

- **Do not power relays or outputs** from metering-side inputs
- Use separate fusing on L1–L3
- Tie **L2, L3 → N** if using single-phase only (prevents phantom voltage)
- Confirm PE is wired — improves stability & safety

### 5.4 Step-by-step

#### Wire

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

Mount the module on a **35 mm DIN rail** inside a suitable enclosure; only qualified personnel may wire **mains voltage**, **CT**, or relay load circuits. Do **not** bond **GND** (logic) to **GND_ISO** (metering domain).

#### Power (24 V DC)

Connect a regulated **24 V DC SELV** supply to **V+** and **0V** for MCU, RS-485, relays, and status LEDs (reverse-polarity protected; typical 50–150 mA).

<img src="https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/main/ENM-223-R1/Images/ENM_24Vdc.png" width="440" alt="24 V DC power wiring to V+ and 0V">

*Regulated **24 V DC** to **V+** / **0V** — fuse the feed upstream per local rules.*

#### 3-phase voltage inputs

Wire **L1**, **L2**, **L3**, **N**, and **PE** to the voltage-sensing terminals for your **3P4W** or **3P3W** installation — these terminals can carry hazardous mains voltage.

<img src="https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/main/ENM-223-R1/Images/ENM_PhaseConnection.png" width="440" alt="3-phase voltage input wiring L1/L2/L3-N/PE">

*Phase and neutral sensing inputs per terminal map; set wiring scheme and phase mapping in WebConfig.*

#### Current transformers (CT)

Connect external CT secondary pairs to **CT1**, **CT2**, and **CT3** with correct polarity and the rated output level (333 mV or 1 V RMS).

<img src="https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/main/ENM-223-R1/Images/ENM_CTConnection.png" width="440" alt="Current transformer CT1/CT2/CT3 wiring">

*Shielded CT leads recommended; observe arrow polarity for correct signed power readings.*

#### Relays (2× SPDT)

Two **SPDT** dry-contact relays (**NO** / **COM** / **NC**) switch external loads at up to **3 A @ 250 VAC** (module/PCB limit); provide external fusing and RC snubbers on inductive circuits.

<img src="https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/main/ENM-223-R1/Images/ENM_RelayConnection.png" width="440" alt="Relay NO/COM/NC wiring">

*Dry contacts only — external load supply and overcurrent protection are mandatory.*

#### RS-485 (Modbus RTU)

Wire **A**, **B**, and **COM** on twisted-pair cable in a daisy-chain bus; place **120 Ω** termination at the physical ends of the segment.

<img src="https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/main/ENM-223-R1/Images/ENM_RS485.png" width="440" alt="RS-485 A/B/COM Modbus wiring">

***A** → A, **B** → B, **COM** → reference ground as required by the network.*

#### USB-C

The **USB-C** port is for **WebConfig** setup and firmware update only (5 V from the host PC, logic domain); it is **not** a field power or runtime data bus — disconnect USB before energising the installation and before handing control to RS-485.

#### RS‑485 (Modbus RTU)

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
| Modbus Address | `3` (first boot; set per site in WebConfig) |
| Baud Rate      | `19200` |
| Format         | `8N1` |
| Address Range  | 1–247 |

> 🧷 Reversed A/B will cause CRC errors — check if no response.

---

#### USB‑C (WebConfig)

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

#### Configure (WebConfig)


- Open `Firmware/v0.2.0/ConfigToolPage.html` in a Chromium-based browser (Chrome, Edge, Opera, Brave, Vivaldi)
- Connect via **USB‑C** → **Select port → Connect**
- Set:
  - **Modbus Address / Baud**  
  - **Line frequency, sum mode, 3P4W/3P3W, phase mapping**
  - **Alarm thresholds** per L1/L2/L3/Totals
  - **Relay modes**: Alarm or Modbus Controlled
  - Map **Buttons & LEDs** (relay toggle / status)
  - (Optional) Adjust **U/I gains**, save calibration

👉 See: [WebConfig UI](#6-webconfig-reference)

#### Integrate (Controller)


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

👉 See: [Modbus RTU Communication](#7-modbus-register-map) & [Integration Guide](#8-esphome--home-assistant-integration)

ESPHome package import (current firmware):

```yaml
packages:
  enm223_1:
    url: https://github.com/isystemsautomation/homemaster-dev
    ref: main
    files:
      - path: ENM-223-R1/Firmware/v0.2.0/default_enm_223_r1_plc/default_enm_223_r1_plc.yaml
        vars:
          enm_id: enm223_1
          enm_address: 3
          enm_prefix: "ENM #1"
```

Set `enm_address` to the Modbus ID configured in WebConfig (first-boot default **3** @ 19200 8N1).

### 5.5 Verify


| Area           | What to Check |
|----------------|---------------|
| **LEDs**       | `PWR = ON`; `TX/RX = blink` during comms |
| **Voltage**    | L1–L3 read ~230 V (or phase-neutral voltage) |
| **Current**

---

## 6. WebConfig Reference

Open **https://config.home-master.eu/ENM-223-R1/Firmware/v0.2.0/ConfigToolPage.html** in any Chromium-based browser (Chrome, Edge, Opera, Brave, Vivaldi; Chrome/Edge 89+, Opera 76+), connect via **USB-C**, and click **Connect**. Changes apply immediately and are saved to flash (no Save button). Live meter / energy panels refresh about once per second; click into a field to pause refresh for that field. **Press Enter** (or leave a numeric field) to write calibration and CT values.

> Firefox: experimental only (Nightly with the Web Serial flag enabled). Safari and stable Firefox are not supported.

Field names and dropdown options below match `Firmware/v0.2.0/ConfigToolPage.html`.

### Status & Tools

![ENM-223-R1 WebConfig — status pills and Tools](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/main/ENM-223-R1/Images/webconfig1.png)

Status pills (read-only): **Connection** (USB), **Bus** (RS-485 link), **Model**, **FW**, **WebConfig**, **Modbus ID**, **Baud**. A compatibility banner (`hm-compat`) blocks writes if the connected firmware/model does not match this configurator.

| Button | What it does |
|--------|--------------|
| **Identify (~5 s)** | Blinks user LEDs to locate the module (`identify` command). |
| **Factory reset** | Restores settings to defaults (`factory` command — confirm dialog). |
| **Reboot** | Soft-restarts the module. |

### Device Setup

| Field | Values | Meaning |
|-------|--------|---------|
| **Modbus Address** | 1–247 (default **3**) | Modbus RTU slave address; must be unique on the bus. |
| **Baud Rate** | 9600 / 19200 / 38400 / 57600 / 115200 (default **19200**) | RS-485 speed **8N1**; must match the controller. |

### Serial Log

Live USB serial log (communication messages and status). **Copy log** copies the buffer to the clipboard.

### Meter Options

![ENM-223-R1 WebConfig — Meter Options and Calibration](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/main/ENM-223-R1/Images/webconfig2.png)

| Field | Values | Meaning |
|-------|--------|---------|
| **Line Frequency (Hz)** | 50 / 60 | Line frequency for the ATM90E32 (`MMode0` / sag thresholds). |
| **Wiring Scheme** | **3P4W (star, 4-wire)** / **3P3W (3-wire)** | Installation topology; applied on ATM re-init. |
| **CT primary (A)** | number, 1–10000 (UI default 100) | CT primary rating in amperes. |
| **CT secondary (mA)** | number, 1–60 (UI default 50) | CT secondary rating in milliamperes. Software scale **N = primary / secondary_mA**; currents, power and energy are reported on the **primary** side. |
| **Sum Mode** *(Advanced settings)* | **0** = algorithmic (vector) / **1** = absolute (\|P1\|+\|P2\|+\|P3\|) | How phase powers are summed for totals. |
| **PGA (advanced)** *(Advanced settings)* | **1×** / **2×** / **4×** (UI default **2×**) | ATM90 analog gain — written to the chip; change only together with the CT/input path. |
| **L1 / L2 / L3 channel → meter phase** *(Advanced settings)* | Phase A / B / C | Maps each logical channel to ATM90 phase A/B/C (`ChannelMapU` / `ChannelMapI`). |

There is **no** Sample Interval field in this tool.

### Calibration (Phase A / B / C)

Per metering phase (A/B/C — after phase mapping these correspond to L1/L2/L3):

| Field | Meaning |
|-------|---------|
| **Ugain** / **Igain** | Scaling gains (press **Enter** to write). |
| **Uoffset** / **Ioffset** | Calibration offsets (press **Enter** to write). |

Live refresh pauses while a calibration field is being edited.

### Alarms (L1, L2, L3, Totals)

![ENM-223-R1 WebConfig — Alarms](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/main/ENM-223-R1/Images/webconfig3.png)

Four channels (**L1**, **L2**, **L3**, **Totals**), each with three rule slots (**Alarm** / **Warning** / **Event**).

| Control | Values / behaviour |
|---------|-------------------|
| **Enabled** | Per rule slot. |
| **Metric** | Voltage (Urms) / Current (Irms) / Active power P / Reactive power Q / Apparent power S / Frequency |
| **Min** / **Max** | Thresholds; empty side = unused. Active below min and/or above max. |
| **Ack required** | Latches the channel state after the condition clears until Ack. |
| **Ack L1 / Ack L2 / Ack L3 / Ack Totals** | Per-channel acknowledge buttons. |
| **Ack All** | Acknowledges all four channels. |

Threshold **Alarm** and **Warning** rules use **2 % hysteresis** on the configured min/max band (firmware). Chip PQ faults are shown under [Chip Events](#chip-events-atm90), not as threshold metrics.

Ack is also available from front-panel **Buttons** (Ack All / Ack L1–L3 / Ack Total) and over Modbus coils **16–19**.

> ENM has no digital inputs (DIs). Alarm rules are virtual channels driven by live metering.

### Relays (2)

![ENM-223-R1 WebConfig — Relays and Live Meter](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/main/ENM-223-R1/Images/webconfig4.png)

Each relay is configured **independently** (including invert).

| Field | Values | Meaning |
|-------|--------|---------|
| **Enabled** | on / off | Relay output active. |
| **Inverted** | on / off | **Per relay** — invert drive polarity (`rlyCfg[i].inverted` in firmware). |
| **Mode** | None / Modbus Controlled / Alarm Controlled | How the relay is driven. |
| **Channel** *(Alarm Controlled)* | L1 / L2 / L3 / Totals | Alarm channel to follow. |
| **Alarm / Warning / Event** *(Alarm Controlled)* | checkboxes | Which rule kinds trip the relay. |

In **Alarm Controlled** mode the relay follows the selected channel/kinds; Modbus coils **0/1** do not drive it in that mode. Relay **state** is shown on the card and on Modbus IR0 bits 8/9.

### Live Meter

Updated from `ENM_Meter` / `ext.meter` (~1 s).

| Channel cards | Values shown |
|---------------|--------------|
| **L1 / L2 / L3** | U (V), I (A), P (W), P<sub>fund</sub> (W), P<sub>harm</sub> (W), Q (var), S (VA), PF, Angle (°) |
| **Totals** | P / P<sub>fund</sub> / P<sub>harm</sub> / Q / S, PF (tot), **Freq (Hz)**, **Temp (°C)** |

P and Q are **signed**. Peaks, THD and neutral current are on the next screen — not here.

### Peaks & Quality

![ENM-223-R1 WebConfig — Peaks, Chip Events, Energies](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/main/ENM-223-R1/Images/webconfig5.png)

Dedicated screen (ATM90 peak registers F1–F3 / F5–F7; computed active-power THD, **not** THD+N):

| Card | Values |
|------|--------|
| **L1 / L2 / L3** | U<sub>peak</sub> (V), I<sub>peak</sub> (A), THD (%) |
| **Neutral** | I<sub>N</sub> (A) |

### Chip Events (ATM90)

Live metering-IC PQ fault panel per **L1 / L2 / L3 / Total**:

| Indicator |
|-----------|
| Sag / Overvoltage / Phase loss / Overcurrent / Frequency / Rev Phase |

> The WebConfig hint text mentions Modbus IR 101–104; firmware **v0.2.0** publishes these chip PQ masks at input registers **3–6** — see [§7 Modbus Register Map](#7-modbus-register-map).

### Energies

Accumulators from `ENM_Sync` / `ext.energy` (saved in flash), cards **Phase A / B / C** and **Totals**:

| Field |
|-------|
| Active import (kWh) / Active export (kWh) / Active net (kWh) |
| Reactive import (kvarh) / Reactive export (kvarh) / Reactive net (kvarh) |
| Apparent (kVAh) |
| Harmonic active import (kWh) |

**Reset counters** sends the `reset_energy` command (confirm dialog). Energy reset is **WebConfig-only** — not on Modbus.

### Buttons (4)

![ENM-223-R1 WebConfig — Buttons and User LEDs](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/main/ENM-223-R1/Images/webconfig6.png)

Each button has a live state indicator and one **Action**:

| Action |
|--------|
| None |
| Toggle Relay 1 / Toggle Relay 2 |
| **Ack All** / **Ack L1** / **Ack L2** / **Ack L3** / **Ack Total** |

Toggle actions apply when the target relay is **Modbus Controlled**. Ack actions acknowledge alarm latches directly on the module (same path as the Alarms **Ack** buttons / coils 16–19).

### User LEDs (4)

| Field | Values |
|-------|--------|
| **Mode** | Steady / Blink |
| **Source** | **None** / **Override R1** / **Override R2** / **Alarm** L1\|L2\|L3\|Total / **Warning** L1\|L2\|L3\|Total / **Event** L1\|L2\|L3\|Total |

**Override R1 / Override R2** follow relay 1/2 logical state. Alarm / Warning / Event sources follow latched alarm state per channel (as labelled in the tool).

---

## 7. Modbus Register Map

The ENM‑223‑R1 is a **Modbus RTU slave** on RS‑485. Firmware **v0.2.0** exposes a **contiguous** map designed for **one FC04 sweep**:

| Area | Detail |
|------|--------|
| **FC04** Input Registers | **`0..85`** (86 registers) — I/O mask, status, alarms, chip events, scalars, S32 currents/powers, U32 energies |
| **FC01 / FC05** Coils | Write-only commands: relays **0/1**, Identify **5**, ACK **16–19** |
| **FC03 / FC06 / FC16** Holding | Address, baud, line frequency, sum-abs, relay enable (HR **0..8**, 3 & 6 reserved) |
| **FC02** Discrete Inputs | **Not used** — digital state is bit-packed into input registers |

| Setting | Value |
|---------|-------|
| Default address | **`3`** first boot (configurable **1–247**) |
| Default baud | **`19200 8N1`** (9600–115200) |
| Identity | Model **`2`** · Firmware **`0.2.0`** · `CFG_VERSION` **`0x0025`** · `METER_VERSION` **`0x0003`** · `HM_MAP_VERSION` **`0x0020`** |

> Full addresses, types, scales and bit layouts: **[Modbus_Table.md](Modbus_Table.md)**.

### 7.1 Map summary (FC04)

| Addr | Content |
|------|---------|
| 0 | I/O mask (LEDs, buttons, **relay state** bits 8/9) |
| 1 | Status (link / config dirty) |
| 2 | Alarm flags (L1–Total × Alarm/Warning/Event) |
| 3–6 | Chip PQ event masks |
| 7–21 | Urms, frequency, temperature, PF, angle, THD |
| 22–29 | Irms L1/L2/L3/N (**S32**, ×0.001 A, primary) |
| 30–52 | P / Q / S L1–Total (**S32**) |
| 54–84 | Active/reactive import & export energy (**U32** Wh/varh, primary) |

### 7.2 Coils (write-only)

| Addr | Coil |
|------|------|
| 0 / 1 | Relay 1 / 2 command (state from IR0) |
| 5 | Identify (~5 s LED blink) |
| 16–19 | ACK L1 / L2 / L3 / Total |

Reboot, save-config and energy-reset are **not** on Modbus — use WebConfig.

### 7.3 Holding registers

| Addr | Field |
|------|-------|
| 0 | Modbus address |
| 1–2 | Baud (U32) |
| 4 | Line frequency 50/60 |
| 5 | Sum-abs mode |
| 7–8 | Relay 1 / 2 enable |

### 7.4 Polling

- Prefer **one FC04** of `0..85` at ~1 s for live data.
- Energy (54–84) may be polled less often; the ESPHome package uses `skip_updates: 2` on energy sensors.
- Stagger multiple ENMs on a shared bus (unique IDs, end-of-line termination).

### 7.5 Integrator note (upgrading from v0.1.0)

The map is **not** a drop-in for masters that still poll discrete inputs, peaks on Modbus, or addresses above 85. Point ESPHome at the v0.2.0 package and regenerate entities.

```yaml
modbus_controller:
  - id: enm223
    address: 3
    modbus_id: rtu_bus
    update_interval: 1s
```

---

## 8. ESPHome / Home Assistant Integration

The HomeMaster controller (MiniPLC or MicroPLC) running **ESPHome** acts as the **Modbus RTU master** over RS‑485. It polls one or more ENM‑223‑R1 modules and publishes all sensors, relays, LEDs, and alarms into **Home Assistant**.

No Home Assistant add-ons are required — all logic runs on the ESPHome controller.

---

### 8.1 Architecture & Data Flow

- **Topology**: Home Assistant → ESPHome (MicroPLC) → RS‑485 → ENM‑223‑R1
- **Roles**:
  - **ENM**: metering, alarm rules, relays, LEDs, buttons
  - **ESPHome**: Modbus polling, sensor/relay control, entity publishing
  - **HA**: dashboards, energy view, automations

> LED mappings, alarm logic, and relay modes are configured on the ENM module (via WebConfig). Home Assistant reads telemetry and alarm bits over Modbus.

---

### 8.2 Prerequisites (Power, Bus, I/O)

#### Power
- **ENM**: 24 V DC → V+ / 0V
- **Controller**: per spec
- If separate PSUs: share COM/GND between controller and ENM

#### RS‑485 Bus
- A—A, B—B (twisted pair), COM shared
- Terminate with 120 Ω resistors at both ends
- Default speed: **19200 baud**, set in WebConfig

#### Field I/O
- Voltage inputs: L1, L2, L3, N, PE
- CTs: CT1–CT3 (1 V or 333 mV)
- Relays: dry contact, driven by internal logic or Modbus
- Buttons / LEDs: wired to MCU, mapped in firmware/UI

---

### 8.3 ESPHome Minimal Config (Enable Modbus + Import ENM Package)

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
    address: 3
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
          enm_address: 3
          enm_prefix: "ENM #1"
```

---

### 8.4 Entities Exposed by the Package

Package path: [`Firmware/v0.2.0/default_enm_223_r1_plc/default_enm_223_r1_plc.yaml`](Firmware/v0.2.0/default_enm_223_r1_plc/default_enm_223_r1_plc.yaml).

#### Binary Sensors (from IR bitmasks)
- Link OK, config dirty
- LED 1–4, Button 1–4, Relay 1–2 **state** (IR0)
- Alarm / Warning / Event per L1–L3 / Total (IR2)
- Chip PQ events (IR3–6)

#### Sensors
- **Urms** L1–L3, **frequency**, **temperature** (integer °C), **PF**, **angle**, **THD**
- **Irms** L1–L3 + **neutral** (S32, primary A)
- **Signed P / Q / S** per phase + totals
- **Energies** (U32 Wh/varh, `accuracy_decimals: 0`): active/reactive import & export L1–L3 + totals; template **net** active/reactive totals

#### Switches / Buttons
- **Switches:** Relay 1/2 → coils 0/1 (commands; state from IR)
- **Buttons:** Identify (coil 5), ACK L1–Total (coils 16–19)

Holding registers are not mirrored as HA entities in the package (configure bus options in WebConfig).

---

### 8.6 Using Your MiniPLC YAML with ENM

1. Keep existing `uart:` and `modbus:` blocks  
2. Add the `packages:` block (as shown) and set `enm_address` from WebConfig  
3. Flash the controller — ESPHome discovers all sensors/entities automatically  
4. Add HA dashboard cards and `switches` for relay control and alarm acknowledge  

---

### 8.7 Home Assistant Setup & Automations

- Go to: **Settings → Devices & Services → ESPHome → Add** by hostname or IP
- Dashboard auto-discovers:
  - Energies (for HA Energy view)
  - Relays, buttons, LEDs
  - Alarm states
- You can create:
  - **Energy Dashboard** source: `VAh Total` or `AP Total`
  - **Automation**:


---

## 9. Programming & Build

### 9.1 Supported Languages

- **MicroPython**
- **C/C++**
- **Arduino IDE**

---

### 9.2 Flashing via USB-C

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

### 9.3 Arduino IDE Setup

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

### 9.4 Firmware Updates

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

## 10. Maintenance & Troubleshooting

| Symptom               | Fix or Explanation                            |
|------------------------|-----------------------------------------------|
| Relay won’t activate   | Check mode: **Alarm Controlled** follows alarms; **Modbus** only via coils 0/1 |
| RS-485 not working     | A/B reversed or un-terminated bus             |
| LED doesn’t light up   | Reassign source in WebConfig or check GPIO18–21 |
| Button unresponsive    | Test DI 4–7; buttons only toggle relays in Modbus mode |
| CRC Errors             | Confirm baud, address, and wiring (A/B swap)  |
| Negative P/Q reading   | Expected for export; flip CT or adjust **phase mapping** in WebConfig |

---

## 11. Downloads & Resources

### Version history

| Version | Config path (`path:`) | Date | Changes |
|--------|------------------------|------|-----------|
| **v0.2.0** | `ENM-223-R1/Firmware/v0.2.0/default_enm_223_r1_plc/default_enm_223_r1_plc.yaml` | 2026-07 | **Current; shipped on new modules.** Contiguous FC04 `0..85`, primary-Wh energy, CT ratio + PGA, alarm engine, relay modes, LED/button Ack, write-only coils, no FC02 |
| v0.1.0 | `ENM-223-R1/Firmware/v0.1.0/default_enm_223_r1_plc/default_enm_223_r1_plc.yaml` | 2026-06 | **Legacy** — previous map/layout; kept for existing units |

> **Reproducible firmware build (v0.2.0):** [Build environment (reproducible)](../README.md#build-environment-reproducible) · sketch [`default_enm_223_r1.ino`](Firmware/v0.2.0/default_enm_223_r1/default_enm_223_r1.ino)

### Files

- 🧠 **Firmware (v0.2.0 — current)** — [Firmware/README.md](Firmware/README.md) · sketch [`default_enm_223_r1.ino`](Firmware/v0.2.0/default_enm_223_r1/default_enm_223_r1.ino) · UF2 [`ENM-223-R1.uf2`](Firmware/v0.2.0/ENM-223-R1.uf2)  
  Contiguous Modbus map FC04 `0..85`, primary-Wh energy, CT/PGA, alarm engine. Full register map: [Modbus_Table.md](Modbus_Table.md).

- 🧰 **WebConfig Tool**  
  [`Firmware/v0.2.0/ConfigToolPage.html`](Firmware/v0.2.0/ConfigToolPage.html) · live: [config.home-master.eu …/v0.2.0/ConfigToolPage.html](https://config.home-master.eu/ENM-223-R1/Firmware/v0.2.0/ConfigToolPage.html)

- 📦 **ESPHome YAML (v0.2.0 — current)**  
  [`default_enm_223_r1_plc.yaml`](Firmware/v0.2.0/default_enm_223_r1_plc/default_enm_223_r1_plc.yaml)

- 🗂 **Legacy firmware (v0.1.0)** — [`Firmware/v0.1.0/`](Firmware/v0.1.0/) — previous map; kept for existing units.

- 🖼 **Images & UI Diagrams**  
  [`Images/`](Images/)  
  Front-panel photos, system diagrams, wiring illustrations, WebConfig screenshots.

- 📐 **Hardware Schematics**  
  [`Schematics/`](Schematics/)  
  PDF schematics for Field Board and MCU Board.

- 📄 **Datasheets & Manuals**  
  [`ENM-223-R1_Datasheet.pdf`](Manuals/ENM-223-R1_Datasheet.pdf)

> 🔁 The current firmware version is stated at [config.home-master.eu](https://config.home-master.eu/ENM-223-R1/); every version is in the [`Firmware/`](Firmware/) directory.

---

## Open Source & Licensing

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

## 12. Compliance & Certifications

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
| EU Declaration of Conformity (DoC) | [DoC-ENM-223-R1-V1.0.pdf](./Manuals/DoC-ENM-223-R1-V1.0.pdf) |
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

---

## 13. Support

If you need help using or configuring the ENM‑223‑R1 module, the following support options are available:

### Official resources

- 🧰 [WebConfig Tool (USB-C)](https://config.home-master.eu/ENM-223-R1/Firmware/v0.2.0/ConfigToolPage.html)  
  Configure the module directly from your browser — no drivers or software required.

- 📘 [Official Support Portal](https://www.home-master.eu/support)  
  Includes setup guides, firmware help, diagnostics, and contact form.

---

### Community & updates

- 🔧 [Hackster Projects](https://hackster.io/homemaster) — System integration, code samples, wiring  
- 📺 [YouTube Channel](https://youtube.com/@HomeMaster) — Module demos, walkthroughs, and tutorials  
- 💬 [Reddit Community](https://reddit.com/r/HomeMaster) — Questions, answers, contributions  
- 📸 [Instagram](https://instagram.com/home_master.eu) — Visual updates and field applications
