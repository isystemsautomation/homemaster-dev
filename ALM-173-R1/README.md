![Modbus](https://img.shields.io/badge/Protocol-Modbus%20RTU-brightgreen)
![License](https://img.shields.io/badge/License-MIT%20%2F%20CERN--OHL--W-blue)

# ALM-173-R1 — Alarm & Annunciator I/O Module

**HOMEMASTER – Modular control. Custom logic.**

![17-input wired sensor hub, DIN-rail module for Home Assistant and Modbus](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/ALM-173-R1/Images/photo1.png)

**Document map:** [§1 Overview](#1-overview) · [§2 Features](#2-features--applications) · [What You Can Connect](#what-you-can-connect) · [§3 Alarm logic](#3-alarm-logic--how-it-works) · [§4 Specifications](#4-specifications) · [§5 Hardware](#5-hardware--interface) · [§6 Getting Started](#6-installation--getting-started) · [§7 WebConfig](#7-webconfig-reference) · [§8 Modbus map](#8-modbus-register-map) · [§9 ESPHome](#9-esphome--home-assistant-integration) · [§10 Programming](#10-programming--build) · [§11 Maintenance](#11-maintenance--troubleshooting) · [§12 Downloads](#12-downloads--resources) · [Licensing](#open-source--licensing) · [§13 Compliance](#13-compliance--certifications) · [FAQ](#faq) · [§14 Support](#14-support)

---

## 1. Overview

The **ALM-173-R1** connects **17 wired sensors** to Home Assistant, a PLC, or any SCADA system from a single DIN-rail module — and powers those sensors itself from built-in isolated **+12 V** and **+5 V** rails.

That second part is the one people usually discover too late. Wired PIR detectors, glass-break sensors and smoke detectors need power as well as a contact, which normally means a second PSU and another distribution block in the cabinet. Here both sensor supplies are on the module.

Seventeen opto-isolated inputs, three relays, an alarm engine that keeps running when the network doesn't, **RS-485 Modbus RTU** back to the controller, and configuration through a browser over USB-C. Open hardware, CE marked.

It is **not a certified or insurance-grade intruder alarm**.

**Key capabilities at a glance:**

- **17 opto-isolated digital inputs** — 5 V DC signalling; **5300 VRMS** optocoupler isolation test voltage per channel
- **3 SPDT dry-contact relays** — **3 A @ 250 VAC** (resistive); follow alarm groups, Modbus manual override, or button override
- **4 buttons + 4 user LEDs** — acknowledge alarms, relay override (long-hold 3 s), status indication
- **On-board alarm engine** — inputs (zones) → **Alarm Groups 1–3** → relays/LEDs; non-latched or latched-until-ack modes
- **Driverless WebConfig** — USB-C + any Chromium-based browser; no app or login
- **Persistent settings** — configuration stored in LittleFS flash

Alarm groups, zone types, local arming, and bell cut-off are described in [§3 Alarm Logic — How It Works](#3-alarm-logic--how-it-works). This is **not** DIO-style per-input relay mapping.

> **Quick path:** wire inputs → assign groups → set latch modes → map relays/LEDs → RS-485 + WebConfig address/baud → integrate with PLC or Home Assistant.

## Key advantages

- **17 opto-isolated inputs + 3 relays + AUX detector-loop power** and an on-board alarm engine (groups, latching, optional local arming) that runs offline. Automation/monitoring module — **not a certified intruder alarm**.
- Native ESPHome API via the MiniPLC/MicroPLC controller — no MQTT broker, no manual Modbus register mapping for the package entities.
- Local-first / edge-resilient — onboard logic keeps working if the network or Home Assistant is down.
- Open hardware (**CERN-OHL-W v2**) and firmware (**MIT**) — repairable, reproducible, no vendor lock-in.
- Standard **RS-485 Modbus RTU** — works with any Modbus master or industrial HMI/SCADA system, not locked to HomeMaster.
- Driverless **USB-C WebConfig** (Chrome, Edge, Opera); configuration persists in on-device flash (**LittleFS**).

---

## 2. Features & Applications

| Area | Detail |
|------|--------|
| **Inputs** | 17 × opto-isolated (5 V DC); per-input Enable, Invert, Alarm Group (None / G1 / G2 / G3) |
| **Alarm groups** | 3 groups + **Any Alarm** aggregate; non-latched or latched-until-ack |
| **Relays** | 3 × SPDT; follow group, Modbus manual ON/OFF, or button override; per-relay Enable/Invert; power-on policy (OFF / ON / Restore) |
| **Buttons** | Ack All, Ack G1–G3, or Relay 1–3 override (long-hold 3 s to enter/exit override) |
| **LEDs** | 4 user LEDs — Steady/Blink; sources: None, Any alarm, G1–G3, relay override |
| **WebConfig** | USB-C → Chromium-based browser; Modbus addr/baud, I/O mapping, alarm modes, live status |
| **Modbus RTU slave** | Input Registers (FC04) bitmasks for telemetry + Coils (commands); config via WebConfig, not holding registers |
| **ESPHome / HA** | Ready-made YAML package; inputs, alarms, relays, ack/override actions |
| **Identity** | **MODEL_ID = 1**; firmware **0.2.0**; **MAP_VERSION = 4** (Input Registers + stateful coils; see [§8](#8-modbus-register-map)) |
| **Extras** | PLC group pulses (coils **8–10**); optional **local arming**; **bell cut-off**; per-zone alarm memory |

### Applications

**Residential**

- New-build wired sensor networks for Home Assistant
- Intrusion and zone monitoring (PIR, door/window contacts, glass-break)
- Water leak protection — a relay can close a solenoid valve when a leak contact trips
- Heating system supervision — aggregate boiler, pump and heat pump fault contacts

**Commercial and industrial**

- Equipment-room fault signalling with summary relays and front-panel LEDs
- BMS and SCADA alarm expansion over Modbus RTU with local ack and override
- Machine and plant status monitoring
- Pump stations and water treatment — dry-run, float and pressure switch inputs
- Server rooms — door contacts, under-floor leak detection, UPS and generator status
- Access-control supervision — door contacts, strike and lock status

**Agriculture and other**

- Greenhouses and barns — door contacts, equipment fault and environmental switch inputs
- Cold storage — door contacts, defrost status and compressor fault monitoring

---

## What You Can Connect

Isolated **+12 V** (**PS/1**) and **+5 V** (**PS/2**) rails run detectors directly — no second power supply and no separate distribution block in the cabinet for those loads. Budget the **150 mA** total on the 12 V rail carefully.

### Sensors powered by the module (12 V rail, 150 mA total)

| Sensor type | Typical current | How many fit |
|-------------|----------------:|-------------:|
| PIR motion detector | 10–20 mA | 6–10 |
| Curtain PIR | 10–15 mA | 8–12 |
| Dual-technology detector (PIR + microwave) | 20–30 mA | 4–6 |
| Glass-break detector | 15–25 mA | 6–8 |
| 12 V smoke detector with relay output | 20–50 mA | 3–6 |
| Gas or CO detector with relay output | 30–60 mA | 2–4 |
| Infrared beam sensor or photoelectric barrier | 20–40 mA | 3–6 |
| Inductive or capacitive proximity sensor (NPN) | 5–15 mA | 8–15 |

**150 mA total.** Eight PIRs at 15 mA is 120 mA and fits. Eight smoke detectors at 40 mA does not — use an external supply. An isolated **+5 V** rail (**PS/2**) is also available for low-power sensors that accept 5 V.

### Dry-contact devices (no power needed)

| Category | Devices |
|----------|---------|
| **Openings** | Door and window reed contacts, gate contacts, garage door position, roller shutter end positions, mailbox contacts |
| **Water and fluids** | Contact-type leak detectors, float switches, level switches, flow switches, pressure switches |
| **Heating and HVAC** | Thermostat contacts, frost stats, boiler fault contacts, pump fault contacts, heat pump alarm outputs, burner lockout |
| **Security** | Tamper switches, vibration and shock sensors, key switches, panic buttons, door strike and lock status |
| **Controls** | Momentary wall switches, push buttons, rotary and toggle switches, limit switches |
| **Equipment status** | UPS alarm contacts, generator run and fault contacts, machine fault relays, filter-blocked switches, belt-break detectors |
| **Weather** | Rain sensors and wind switches with contact output |

### What you cannot connect

| Signal / device | Use instead |
|-----------------|-------------|
| Mains voltage on an input | Interposing contactor or relay — inputs are SELV and dry contact only |
| Analogue sensors 0–10 V / 4–20 mA / PT100 / PT1000 | **AIO-422-R1** |
| Pulse counting from S0 meter outputs | **DIO-430-R1** |
| Addressable fire alarm loops | Conventional detectors with relay output only |
| End-of-line resistor supervision | Not supported — see the NC tip below |

### Tip: get cable-cut detection for free

There is no EOL resistor line-supervision. Wiring contacts **normally closed** with **Invert** enabled in WebConfig makes a cut or disconnected cable read the same as an activation, so a damaged loop is visible instead of failing silently. Any security zone should be wired NC.

---

## 3. Alarm Logic — How It Works

> **Zones, groups, relays.** Each input is a **zone** assigned to one of three **Alarm Groups** (G1–G3). Each group has a mode: **None** (disabled), **Non-latched** (follows live zone state), or **Latched** (trips on activation and holds until **ACK**). Each relay is bound to a group and energizes while that group is in alarm. Relay control priority: **Button override → Modbus manual → Group**.

**PLC group pulses** (Modbus coils **8–10**) can force a one-scan group activation for controller-driven annunciation. **Button override:** long hold **3 s** on a button configured for Relay 1–3 enters manual relay control; another long hold exits and returns control to the alarm group.

### Zone types (when Local arming is enabled)

Each zone has a type that defines when its activation contributes to its group:

| Zone type | Disarmed | Armed | Typical use |
|-----------|----------|-------|-------------|
| **Instant** | does not trigger | triggers immediately | Perimeter: windows, glass-break |
| **Delayed** | does not trigger | starts **Entry delay**; alarm if not disarmed in time | Entry doors |
| **24h / Tamper** | **always triggers** | **always triggers** | Tamper, sabotage, 24-hour loops |

### Local arming (optional, default off)

The module supports minimal **local arming** so protection continues if the controller is offline:

- **Arm** (stateful coil **3** = `1`, or configured button) → **Exit delay** (**Exit-pending**): activations are ignored so you can leave. When the timer expires → **Armed**.
- While **Armed**: Instant zones trigger immediately; a **Delayed** zone starts **Entry delay** (**Entry-pending**) — if **Disarm** (coil **3** = `0`) does not arrive before the timer expires, the group alarms; **24h/Tamper** zones trigger regardless.
- **Disarm** (coil **3** = `0`) → **Disarmed** immediately; Entry/Exit-pending are cleared. **Latched groups are not cleared** — only **ACK** (coil **4** or **5–7**) clears them.
- With **Local arming disabled** (factory default), behaviour matches legacy firmware: all enabled zones feed their group at all times; zone types and arm delays are ignored.

**State machine (simplified):** `Disarmed → (ARM) → Exit-pending → Armed → (Delayed zone) → Entry-pending → Triggered → (ACK) → Armed/Disarmed`. Non-latched, latched, and 24h/Tamper behaviour apply on top of this.

### Bell cut-off (siren timeout)

Each relay can be configured with an **auto-off** time (seconds). When a relay is ON **because its alarm group is active**, if it stays ON longer than the configured time it is turned OFF (the siren stops), but the **group remains latched** — Home Assistant / the PLC still sees the alarm. A new alarm edge on that group can turn the relay ON again. **0** = disabled.

### Alarm memory (per-zone latch)

When local arming is enabled and a zone contributes to a **latched** group alarm, the module records **which zones** triggered (Input Register bitmasks **@2/@3**). Cleared by **ACK** for the relevant group(s).

### Module vs Home Assistant

Full alarm-panel features — **Home / Away / Night** modes, codes, keypads, schedules — belong in the **Home Assistant Alarm Control Panel** (the de-facto standard). The ALM exposes zones, drives sirens/relays, and provides optional minimal local arming for resilience.

**Limitations:** No **EOL line-supervision** — dry contact / SELV signalling only; no analogue end-of-line resistor networks.

---

## 4. Specifications

### 4.1 I/O summary

| Subsystem | Qty | Description |
|-----------|-----|-------------|
| Digital Inputs | 17 | Opto-isolated, 5 V DC; 5300 VRMS isolation test voltage; dry contact / SELV |
| Relays | 3 | SPDT (NO/NC/COM), HF115F/005-1ZS3; **3 A @ 250 VAC** resistive (module rating) |
| Buttons | 4 | Configurable ack / relay override |
| User LEDs | 4 | Configurable Steady/Blink + PWR/TX/RX status |
| Modbus RTU | 1 | RS-485; address 1–247; 9600–115200 baud |
| USB-C | 1 | WebConfig (Web Serial); UF2 flashing |
| Power | 24 V DC | 24 V DC nominal; 1 A time-lag fuse, reverse diode, TVS |
| Sensor rails | 2 | Isolated **+12 V** (PS/1) and **+5 V** (PS/2); ~2 W / ~150 mA usable on 12 V rail |
| MCU | RP2350A | Dual-core; QSPI flash; LittleFS |

### 4.2 Electrical ratings

| Parameter | Min | Typ | Max | Unit | Notes |
|-----------|----:|----:|----:|:----:|-------|
| Supply voltage | — | 24 | — | V DC | SELV; 1 A time-lag fuse, reverse-polarity diode, TVS |
| Module power | — | 1.85 | 3.0 | W | Excludes external relay load currents |
| Digital inputs | — | 5 | — | V DC | Opto-isolated; 5300 VRMS test voltage (SFH6156 optocoupler) |
| Relay contact (module) | — | — | 3 | A | @ 250 VAC resistive |
| Relay contact voltage | — | — | 250 | V AC | or 30 V DC max |
| RS-485 data rate | — | 19.2 | 115.2 | kbps | Default 19200 8N1 |
| Operating temp. | 0 | — | 40 | °C | ≤ 95 % RH, non-condensing |

> **Relay component vs module rating:** Relay components (HF115F class) are rated up to **16 A @ 250 VAC** at the device level. **This chip rating does NOT apply to the module** — PCB traces, terminals, and compliance testing limit the **module output to 3 A @ 250 VAC (resistive)**. The margin is deliberate: at 3 A the contacts work far below their rating, so arcing stays low and the contacts do not burn. Use interposing contactors for higher or inductive loads.

> **Isolation figure:** 5300 VRMS is the optocoupler's **isolation test voltage** (VISO, one minute, per the SFH6156 datasheet) — the industry-standard figure to compare parts by. It is not a working voltage: continuous operation is governed by VIORM = 890 V. Neither matters in practice here, because the inputs are 5 V DC dry-contact signalling and the barrier never approaches either. The figure is there to confirm a real barrier is present, **not** to permit mains on the input terminals — see the SELV warnings in §5.3 and §6.1.

> **Power budgeting:** logic + LEDs + relay coils + sensor rails → add ≥ 30 % PSU headroom.

### 4.3 Mechanical & environmental

| Property | Specification |
|----------|---------------|
| Mounting | DIN-rail EN 50022 (35 mm) |
| DIN width | 9 modules (9 × 17.5 mm) |
| Enclosure | PC/ABS V-0 |
| Dimensions | 157.4 × 91 × 58.4 mm (W × H × D) |
| Terminals | Pluggable 5.08 mm; 0.2–2.5 mm²; 0.4 Nm max |
| Ingress protection | IP20 (panel interior) |
| Operating temp | 0–40 °C, ≤ 95 % RH (non-condensing) |

![ALM-173-R1 DIN-rail wired sensor hub dimensions](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/ALM-173-R1/Images/ALMMDimensions.png)

### 4.4 Communication defaults

| Parameter | Default |
|-----------|---------|
| **Modbus address** | `3` |
| **Baud rate** | `19200` |
| **Parity** | None |
| **Stop bits** | 1 |
| **MODEL_ID** | `1` |
| **Firmware** | `0.2.0` |

Address **1–247** (248–255 reserved by Modbus); baud 9600 / 19200 / 38400 / 57600 / 115200. Set via [WebConfig](#6-webconfig-reference) over USB-C (recommended).

Configuration is stored in **LittleFS** (`/cfg.bin`); relay restore snapshot optional for power-on **Restore** policy.

### 4.5 Reliability & protection

- Reverse-polarity diode + TVS on 24 V input; 1 A time-lag fuse.
- Opto-isolated digital inputs (5300 VRMS); isolated sensor rails with PTC/fuse limiting.
- Relay drivers with onboard suppression; add external RC/MOV for inductive field loads.
- RS-485: see [RS-485 / Modbus RTU](#rs-485--modbus-rtu).
- USB-C ESD-protected; service port only.
- Auto-save to flash after WebConfig changes (~1.5 s quiet period).

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

## 5. Hardware & Interface

### 5.1 Diagrams & pinouts

<table>
  <tr>
    <th>System block</th><th>MCU pinout</th><th>Field board</th><th>MCU board</th>
  </tr>
  <tr>
    <td><img width="240" src="https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/ALM-173-R1/Images/ALM_SystemBlockDiagram.png"></td>
    <td><img width="240" src="https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/ALM-173-R1/Images/ALM_MCU_Pinouts.png"></td>
    <td><img width="240" src="https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/ALM-173-R1/Images/FieldBoard-Diagram.png"></td>
    <td><img width="240" src="https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/ALM-173-R1/Images/MCUBoard-Diagram.png"></td>
  </tr>
</table>

### 5.2 Connectors & terminal map

<!-- hm:terminal-map:begin -->

**Top row** (24Vdc | DIGITAL INPUTS | RELAY)

| Pos | Label | Group | Function |
|-----|-------|-------|----------|
| 1 | V+ | POWER | 24 V DC input |
| 2 | 0V | POWER | 24 V DC return |
| 3 | Gnd | DI1 | DI1 isolated return |
| 4 | I.1 | DI1 | DI1 input |
| 5 | Gnd | DI2 | DI2 isolated return |
| 6 | I.2 | DI2 | DI2 input |
| 7 | Gnd | DI3 | DI3 isolated return |
| 8 | I.3 | DI3 | DI3 input |
| 9 | Gnd | DI4 | DI4 isolated return |
| 10 | I.4 | DI4 | DI4 input |
| 11 | Gnd | DI5 | DI5 isolated return |
| 12 | I.5 | DI5 | DI5 input |
| 13 | Gnd | DI6 | DI6 isolated return |
| 14 | I.6 | DI6 | DI6 input |
| 15 | Gnd | DI7 | DI7 isolated return |
| 16 | I.7 | DI7 | DI7 input |
| 17 | Gnd | DI8 | DI8 isolated return |
| 18 | I.8 | DI8 | DI8 input |
| 19 | Gnd | DI9 | DI9 isolated return |
| 20 | I.9 | DI9 | DI9 input |
| 21 | Gnd | DI10 | DI10 isolated return |
| 22 | I.10 | DI10 | DI10 input |
| 23 | NC | RELAY1 | Relay 1 normally closed |
| 24 | C | RELAY1 | Relay 1 common |
| 25 | NO | RELAY1 | Relay 1 normally open |
| 26 | NC | RELAY3 | Relay 3 normally closed |
| 27 | C | RELAY3 | Relay 3 common |
| 28 | NO | RELAY3 | Relay 3 normally open |

**Bottom row** (OUTPUT 12Vdc | OUTPUT 5Vdc | RS-485 | DIGITAL INPUTS | RELAY)

| Pos | Label | Group | Function |
|-----|-------|-------|----------|
| 1 | + | PS_12V_1 | Isolated 12 V rail 1 positive |
| 2 | - | PS_12V_1 | Isolated 12 V rail 1 return |
| 3 | + | PS_12V_2 | Isolated 12 V rail 2 positive |
| 4 | - | PS_12V_2 | Isolated 12 V rail 2 return |
| 5 | + | PS_5V_1 | Isolated 5 V rail 1 positive |
| 6 | - | PS_5V_1 | Isolated 5 V rail 1 return |
| 7 | + | PS_5V_2 | Isolated 5 V rail 2 positive |
| 8 | - | PS_5V_2 | Isolated 5 V rail 2 return |
| 9 | COM | RS485 | RS-485 signal reference |
| 10 | B | RS485 | RS-485 data - |
| 11 | A | RS485 | RS-485 data + |
| 12 | GND | DI11 | DI11 isolated return |
| 13 | I.11 | DI11 | DI11 input |
| 14 | GND | DI12 | DI12 isolated return |
| 15 | I.12 | DI12 | DI12 input |
| 16 | GND | DI13 | DI13 isolated return |
| 17 | I.13 | DI13 | DI13 input |
| 18 | GND | DI14 | DI14 isolated return |
| 19 | I.14 | DI14 | DI14 input |
| 20 | GND | DI15 | DI15 isolated return |
| 21 | I.15 | DI15 | DI15 input |
| 22 | GND | DI16 | DI16 isolated return |
| 23 | I.16 | DI16 | DI16 input |
| 24 | GND | DI17 | DI17 isolated return |
| 25 | I.17 | DI17 | DI17 input |
| 26 | NO | RELAY2 | Relay 2 normally open |
| 27 | C | RELAY2 | Relay 2 common |
| 28 | NC | RELAY2 | Relay 2 normally closed |

**Ports & service interfaces**

| Id | Type | Note |
|----|------|------|
| USB-C | WebConfig / UF2 | Not a field power source |

**Housing notes**

- RELAY1 and RELAY3 are on the TOP row, RELAY2 on the BOTTOM.
- Top relays read NC-C-NO; RELAY2 reads NO-C-NC. The contact order is reversed between rows.
- Inputs DI1-DI10 are on the top row, DI11-DI17 on the bottom.
- FOUR isolated sensor rails: 12 V x2 and 5 V x2, each a separate + / - pair. The README documents only a single 12 V rail - per-rail current limits need confirming.
- No end-of-line supervision. Wire alarm loops normally closed with invert so a cable break reads as an alarm.

<!-- hm:terminal-map:end -->

![ALM-173-R1 terminal labeling — 17 wired sensor inputs, relays and isolated 12 V / 5 V sensor rails](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/ALM-173-R1/Images/photo1.png)

**Digital inputs.** Each input is **opto-isolated** (5 V DC signalling, **5300 VRMS** optocoupler isolation test voltage). Wire a dry contact between **INx** and **GND I.x**. Do not apply mains or non-SELV voltages.

**Sensor rails.** **PS/1 (+12 V)** and **PS/2 (+5 V)** are isolated, fuse/PTC limited outputs for **low-power sensors only**. Do not backfeed or parallel with external supplies.

### 5.3 I/O warnings

| Area | Warning |
|------|---------|
| **24 V input** | SELV only; correct polarity; upstream fuse |
| **Inputs** | Dry contact / SELV only; respect Enable/Invert/Group in WebConfig |
| **Relays** | Dry contacts; **3 A @ 250 VAC** module limit; snub inductive loads |
| **Sensor rails** | Low power only; shorts may trip PTCs |
| **RS-485** | See [RS-485 / Modbus RTU](#rs-485--modbus-rtu) |
| **USB-C** | Setup/maintenance only |
| **Buttons** | Can ack alarms or override relays — document procedures for safety-critical installs |

### 5.4 Front panel — buttons & LEDs

![ALM-173-R1 front-panel buttons and LEDs for alarm acknowledge and relay override](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/ALM-173-R1/Images/buttons1.png)

| Control | Function |
|---------|----------|
| **Buttons 1–4** | Configurable: Ack All, Ack G1–G3, or Relay 1–3 override |
| **Long hold (3 s)** | Enter/exit **button override** for relays (actions 5–7) |
| **Buttons 1 + 2** | **BOOT** mode (UF2 drag-and-drop) |
| **Buttons 3 + 4** | Hardware **RESET** |
| **PWR / TX / RX** | Power and Modbus activity |
| **User LEDs 1–4** | Configurable status (Any / G1–G3 / override) |

---

## 6. Installation & Getting Started

### 6.1 Safety *(read before wiring)*

> ⚠️ **SELV only** — 24 V DC, RS-485, USB 5 V. Never connect mains to logic, input, or relay terminals. Use interposing contactors for mains loads.

| Requirement | Detail |
|-------------|--------|
| Qualified personnel | 24 V control and RS-485 experience |
| Power isolation | Disconnect 24 V before wiring; lockout/tagout where applicable |
| Grounding | Bond panel to PE; share RS-485 COM/GND in same SELV domain |
| Relay loads | 3 A @ 250 VAC module rating; external snubbers on inductive loads |

**Pre-power checklist**

- [ ] Terminals torqued; no bridge between logic GND and **GND_ISO**
- [ ] RS-485 A/B polarity and 120 Ω termination
- [ ] Relays wired COM/NO/NC; snubbers on inductive loads
- [ ] Sensor rail load within limits

### 6.2 What you need

| Category | Item |
|----------|------|
| **Hardware** | ALM-173-R1 — 17 opto DI, 3 SPDT relays, 4 buttons, 4 LEDs, RS-485, USB-C |
| **Controller** | MiniPLC/MicroPLC or Modbus RTU master |
| **24 V PSU** | Regulated SELV 18–30 V DC |
| **RS-485 cable** | Twisted pair A/B + COM; 120 Ω at trunk ends |
| **Browser** | Chromium-based (Chrome, Edge, Opera, Brave, Vivaldi; Chrome/Edge 89+, Opera 76+). Firefox: experimental only (Nightly + Web Serial flag). Safari/stable Firefox not supported. |
| **WebConfig** | [ConfigToolPage.html v0.2.0](https://config.home-master.eu/ALM-173-R1/Firmware/v0.2.0/ConfigToolPage.html) |

### 6.3 Power notes

The module uses **24 V DC** primary (18–30 V DC nominal). Onboard regulation provides logic and isolated sensor rails.

- **24 V DC DIN-rail PSU** → **V+ / 0V** power terminals.
- **Digital inputs** — opto-isolated **5 V DC** signalling; dry contact or open-collector to **INx / GND I.x** (isolated return per channel). Do **not** apply mains to input terminals.
- **Sensor rails (isolated):** **PS/1 = +12 V** (~2 W, ~150 mA usable) and **PS/2 = +5 V** for low-power detectors; returns on **0V PS/1** and **0V PS/2**. Not for heavy loads.
- Size PSU for base electronics + front-panel LEDs + **relay coils** (up to 3) + sensor-rail load; add **≥ 30 % headroom** (see [§4.2](#42-electrical-ratings)).
- Correct polarity; keep logic **0V** and isolated input/sensor returns **separate**; upstream **fusing/breaker** required.

### 6.4 Step-by-step

**Phase 1 — Wire**

| 24 V DC | Digital inputs | Relays | RS-485 |
|:---:|:---:|:---:|:---:|
| ![24 V DC power wiring for ALM-173-R1 wired sensor hub](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/ALM-173-R1/Images/ALM_24Vdc_PowerSupply.png) | ![Wiring dry-contact and powered detectors to ALM-173-R1 digital inputs](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/ALM-173-R1/Images/ALM_DigitalInputs.png) | ![ALM-173-R1 relay output wiring for sirens, valves and summary loads](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/ALM-173-R1/Images/ALM_RelayConnection.png) | ![RS-485 Modbus wiring from ALM-173-R1 to MiniPLC or MicroPLC](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/ALM-173-R1/Images/ALM_RS485Connection.png) |

**Phase 2 — Configure (WebConfig)**

1. Connect **USB-C**; open [WebConfig v0.2.0](https://config.home-master.eu/ALM-173-R1/Firmware/v0.2.0/ConfigToolPage.html) → **Connect**.
2. Set **Modbus address** and **baud** ([§4.4](#44-communication-defaults)).
3. Configure alarm modes, inputs, relays, buttons, LEDs ([§7](#7-webconfig-reference)).
4. Disconnect USB-C; hand control to RS-485 master.

**Phase 3 — Integrate**

Add ESPHome package on controller ([§9](#9-esphome--home-assistant-integration)) or poll Modbus from PLC/SCADA ([§8](#8-modbus-register-map)).

### 6.5 Verify

| Check | Expected |
|-------|----------|
| **PWR LED** | ON |
| **TX/RX** | Blink on Modbus traffic |
| **Inputs** | Live dots in WebConfig; IN1–IN17 in Input Register **@0/@1** (FC04) |
| **Alarms** | Group / Any indicators follow wiring |
| **Relays** | Follow group or manual override |

---

## 7. WebConfig Reference

Open **[ALM-173-R1 WebConfig v0.2.0](https://config.home-master.eu/ALM-173-R1/Firmware/v0.2.0/ConfigToolPage.html)** in a **Chromium-based browser** (Chrome, Edge, Opera, Brave, Vivaldi; Chrome/Edge 89+, Opera 76+). Connect via **USB-C** and click **Connect**. Changes apply immediately and are saved to flash after a short idle period (no Save button).

> **Firefox:** experimental only (Nightly with Web Serial enabled). **Safari** and stable Firefox are not supported.

See [§3 Alarm Logic](#3-alarm-logic--how-it-works) for zone types, local arming, and bell cut-off behaviour.

### Status & Tools

![WebConfig — overview, connection & status (Armed/Entry/Exit/Tamper)](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/ALM-173-R1/Images/webconfig1.png)

Status pills (read-only): **Connection** (USB), **Bus** (RS-485), **Model**, **FW**, **WebConfig**, **Modbus ID**, **Baud**. When local arming is enabled: **Armed**, **Entry**, **Exit**, **Tamper**.

| Button | What it does |
|--------|--------------|
| Identify (~5 s) | Blinks user LEDs to locate the module. |
| Factory reset | Restores all settings to defaults. |
| Reboot | Restarts the module. |

### Device Setup

![Device setup — Modbus address & baud, serial log](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/ALM-173-R1/Images/webconfig2.png)

| Field | Values | Meaning |
|-------|--------|---------|
| Modbus Address | 1–247 (default 3) | Modbus RTU slave address; must be unique on the bus. |
| Baud Rate | 9600 / 19200 / 38400 / 57600 / 115200 (default 19200) | RS-485 speed **8N1**; must match the controller. |

Changed over **USB-C** only (not writable via Modbus holding registers).

### Alarm Status & Modes

![Alarm status & per-group latch modes](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/ALM-173-R1/Images/webconfig3.png)

Live indicators: **Any Alarm**, **Alarm Group 1–3**.

| Field | Values | Meaning |
|-------|--------|---------|
| Mode (per group) | None / Non-latched / Latched | **None** — group disabled. **Non-latched** — follows live zone state. **Latched** — trips on activation and holds until **ACK**. |

### Digital Inputs (17)

![WebConfig — 17 wired sensor inputs: enable, invert, alarm group, zone type](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/ALM-173-R1/Images/webconfig4.png)

| Field | Values | Meaning |
|-------|--------|---------|
| Enabled | on / off | Whether this zone is processed. |
| Inverted | on / off | Invert the read level (NC contacts). |
| Alarm Group | None / 1 / 2 / 3 | Which alarm group this zone feeds. |
| Type | Instant / Delayed / 24h-Tamper | Zone behaviour when [local arming](#local-arming-optional-default-off) is enabled (ignored when off). |
| Latched badge | on screen | Per-zone alarm memory (IREG **@2/@3** bitmasks); cleared by **ACK**. |

### Relays (3)

| Field | Values | Meaning |
|-------|--------|---------|
| Enabled | on / off | Relay output active. |
| Inverted | on / off | Invert drive polarity. |
| Alarm Group | None / 1 / 2 / 3 | Group that energizes this relay while in alarm. |
| Power-on | OFF / ON / Restore | State after power-up. |
| Bell cut-off, s | 0–65535 (0 = off) | Auto-off timer when relay is ON due to group alarm; group stays latched. |

### Buttons (4)

| Field | Values | Meaning |
|-------|--------|---------|
| Action | None / Ack all / Ack G1–G3 / Override R1–R3 | Front-panel button function. **Override:** long hold **3 s** to enter/exit manual relay control. |

### User LEDs (4)

| Field | Values | Meaning |
|-------|--------|---------|
| Mode | Steady / Blink-when-active | Display mode. |
| Trigger source | None / Any alarm / G1–G3 / Override R1–R3 | What drives the LED. |

### Arming (local)

| Field | Values | Meaning |
|-------|--------|---------|
| Local arming | on / off (default **off**) | Enables zone types, entry/exit delays, **Armed** coil (stateful @3). |
| Entry delay, s | 0–65535 (default 30) | Delayed-zone entry timer while armed. |
| Exit delay, s | 0–65535 (default 30) | Exit timer after ARM before zones are active. |

> Full **Home / Away / Night** modes, alarm codes, and keypads belong in the **Home Assistant Alarm Control Panel** — not on the module. See [§3](#3-alarm-logic--how-it-works).

![Relays (bell cut-off), buttons & user LEDs](https://cdn.jsdelivr.net/gh/isystemsautomation/homemaster-dev@main/ALM-173-R1/Images/webconfig5.png)

---

## 8. Modbus Register Map

**Role:** RTU **slave** (controller is master). **Defaults:** see [§4.4 Communication defaults](#44-communication-defaults).

**Configuration** is via **WebConfig** (LittleFS), **not** holding registers (FC03).

> v0.2.0 (**MAP v4**) uses **Input Registers (FC04)** for telemetry — **bit-packed masks @0..3** (read in one block) — and **Coils (FC01/05) @0..10** for outputs and momentary actions. **Identity** is FC04 at base **200** (0x00C8): MODEL_ID, FW_MAJOR, FW_MINOR, FW_PATCH, **MAP_VERSION** (= **4**).

The ALM Modbus model is aligned with the **HomeMaster line standard** (reference: [DIO-430-R1](../DIO-430-R1/README.md)): **bit-packed Input Registers** for live states and **stateful coils** for relay/armed outputs (industry-standard, same as DIO and Waveshare-style I/O). Home Assistant entities are uniform across modules — toggles for outputs, sensors for telemetry, buttons for momentary actions.

Per-input **enable/invert/group/type**, relay **group-mapping/bell-cutoff/power-on**, and button/LED mapping are configured in **WebConfig only** — not over Modbus (same principle as momentary/interlock settings on KinCony-class devices).

> **No FC02 (Discrete Inputs)** and **no Holding Registers (FC03)** for configuration — use [§7 WebConfig](#7-webconfig-reference).

### 8.1 Input Registers (FC04) — telemetry (bit-packed)

Poll **registers 0..3** as one contiguous block (500 ms typical):

| IReg | Bit | Signal |
|------|-----|--------|
| **@0** | 0–15 | **IN1…IN16** (after enable + invert) |
| **@1** | 0 | **IN17** |
| | 1–4 | **Any Alarm** / **G1** / **G2** / **G3** |
| | 8–11 | **LED 1…4** |
| | 13–15 | **Entry pending** / **Exit pending** / **Tamper any** |
| **@2** | 0–15 | **Zone latched 1…16** (per-zone alarm memory) |
| **@3** | 0 | **Zone latched 17** |

> **Relay 1–3** and **Armed** state are **not duplicated here** — read them via **coil read-back** ([§8.2](#82-coils-fc0105--stateful--momentary)). Firmware still mirrors relay/armed bits in IREG 1 internally; integrators should use coils **0–3** for those outputs.

When **local arming is disabled** (default), zone types and arm delays are ignored; Entry/Exit-pending bits (13–14) stay inactive unless local arming is enabled in WebConfig.

### 8.2 Coils (FC01/05) — stateful + momentary

| Coil | Type | Purpose |
|------|------|---------|
| **0–2** | **Stateful R/W** | **Relay 1/2/3** — write `1`=ON / `0`=OFF; read back actual state (like DIO/Waveshare) |
| **3** | **Stateful R/W** | **Armed** — write `1`=arm / `0`=disarm; read back actual armed state *(requires local arming in WebConfig)* |
| **4** | Momentary | **Ack All** — clear all latched groups |
| **5–7** | Momentary | **Ack G1/G2/G3** — clear latched group |
| **8–10** | Momentary | **Group test-pulse G1/G2/G3** — one-scan group activation (PLC) |

**Override priority:** Button override → Modbus manual (coils **0–2**) → Alarm group.

Stateful coils **0–3** hold until changed; momentary coils **4–10** auto-clear in firmware after execution.

### 8.3 Identity (FC04 @200)

| Offset | Field | Value (v0.2.0) |
|--------|-------|----------------|
| 200 | MODEL_ID | **1** (ALM-173-R1) |
| 201 | FW_MAJOR | **0** |
| 202 | FW_MINOR | **2** |
| 203 | FW_PATCH | **0** |
| 204 | MAP_VERSION | **4** |

Read once at startup or after firmware update.

### 8.4 Register use examples

**A) Acknowledge Alarm Group 1** — write `1` to **Coil 5** (Ack G1). Module auto-clears the coil.

**B) Arm / Disarm (local arming enabled)** — write **`1`** or **`0`** to **Coil 3 (Armed)**. Requires local arming enabled in WebConfig.

**C) Manual relay 2 ON/OFF** — write **`1`** or **`0`** to **Coil 1** (Relay 2). Read coil to confirm state; button override takes priority until released.

**D) Read zone 3 alarm memory** — read **Input Register 2** (FC04), test **bit 2** (Zone 3 latched).

**E) PLC-driven annunciation** — pulse **Coils 8–10** for one-scan activation of Alarm Group 1–3.

**F) Read IN7 state** — read **Input Register 0**, test **bit 6**.

**G) Configure input enable/invert/group** — use [WebConfig](#7-webconfig-reference); not available via Modbus coils.

### 8.5 Polling recommendations

- **Input registers 0..3:** 2–5 Hz (200–500 ms) — single contiguous read covers all telemetry
- **Stateful coils 0..3:** read on demand or at same rate as outputs if supervising relay/armed state
- **Momentary coils 4..10:** write on change only; firmware auto-clears — do not poll for state
- **Identity (IREG @200):** read once at startup or after firmware update
- **Local arming bits (IREG 1, bits 13–14):** same rate as summary alarms when arming is used

---

## 9. ESPHome & Home Assistant Integration

> **Module role:** Modbus RTU **slave** on RS-485. Comms defaults: [§4.4](#44-communication-defaults).

The **MiniPLC/MicroPLC** running **ESPHome** polls the ALM over RS-485 and publishes entities to **Home Assistant**.

### 9.1 Minimal package (v0.2.0)

```yaml
uart:
  id: uart_modbus
  tx_pin: 17
  rx_pin: 16
  baud_rate: 19200
  parity: NONE
  stop_bits: 1

modbus:
  id: modbus_bus
  uart_id: uart_modbus
  turnaround_time: 100ms
  send_wait_time: 250ms

packages:
  alm1:
    url: https://github.com/isystemsautomation/homemaster-dev
    ref: main
    files:
      - path: ALM-173-R1/Firmware/v0.2.0/default_alm_173_r1_plc/default_alm_173_r1_plc.yaml
        vars:
          alm_prefix: "ALM#1"
          alm_id: alm_1
          alm_address: 3   # must match WebConfig
    refresh: 1d
```

> **ESPHome 2026.7.0 raised the Modbus client defaults** — `turnaround_time` from
> 100 ms to 600 ms and `send_wait_time` from 250 ms to 2000 ms (PR #11969).
>
> At the 600 ms default the bus carries only about 1.5 Modbus transactions per
> second, whatever the UART speed, because the controller stays silent for 600 ms
> after every response. Two modules on one RS-485 bus become slow to respond.
> With three or more, one module can stop being polled altogether — with no
> timeout and no CRC error in the log.
>
> On ESPHome 2026.7.0 and newer, set both values explicitly as shown above.
> On earlier releases these were the defaults and the two lines are not required.


### 9.2 Entities exposed (from the package)

- **Binary sensors** (from **Input Register** bitmasks, FC04)
  - **IN1…IN17** (IREG **0** bits 0–15, **1** bit 0)
  - **Any Alarm / G1–G3** (IREG **1** bits 1–4)
  - **LED 1–4** (IREG **1** bits 8–11)
  - **Entry / Exit / Tamper** (IREG **1** bits 13–15)
  - **Zone 1–17 latched** (IREG **2** bits 0–15, **3** bit 0)
- **Switches** (stateful coils, read-back)
  - **Relay 1–3** (coils **0–2**)
  - **Armed** (coil **3**)
- **Buttons** (template → internal momentary coils)
  - **Ack All / G1–G3** (coils **4–7**)
  - **Pulse G1–G3** (coils **8–10**)

> The package matches `default_alm_173_r1_plc.yaml`: **no FC02 discrete-input bank** — telemetry from **Input Registers 0..3**; relay/armed state from **stateful coil switches**, not duplicated as binary sensors.

### 9.3 Optional: direct (manual) entity mapping

```yaml
modbus_controller:
  - id: alm_1
    address: 3
    modbus_id: modbus_bus
    update_interval: 500ms
    command_throttle: 100ms

binary_sensor:
  - platform: modbus_controller
    modbus_controller_id: alm_1
    name: "ALM#1 IN1"
    register_type: read
    address: 0
    bitmask: 0x0001
  - platform: modbus_controller
    modbus_controller_id: alm_1
    name: "ALM#1 Alarm Group 1"
    register_type: read
    address: 1
    bitmask: 0x0004
    device_class: problem

switch:
  - platform: modbus_controller
    modbus_controller_id: alm_1
    name: "ALM#1 Relay 2"
    register_type: coil
    address: 1
  - platform: modbus_controller
    modbus_controller_id: alm_1
    id: alm1_ack_g1
    register_type: coil
    address: 5
    internal: true

button:
  - platform: template
    name: "ALM#1 Ack Group 1"
    on_press:
      - switch.turn_on: alm1_ack_g1
```

### 9.4 Home Assistant tips

- **Dashboards:** alarm summary tile (Any + G1–G3), zone tiles for IN1–IN17, siren/relay switches, Ack buttons.
- **Automations:** trigger on **IREG 1** alarm bits or **IREG 2/3** zone-latch bits; use **Ack** buttons (coils **4–7**) to clear latched groups.
- **Alarm Control Panel:** use HA's built-in panel for Home/Away/Night; ALM provides zones and relay/siren outputs via Modbus.
- **Naming:** use `alm_prefix` for readable entities (`ALM#1 IN5`, `ALM#1 Alarm Group 2`, etc.).

### 9.5 Troubleshooting (integration)

- **Timeouts:** A/B polarity, shared **COM/GND**, **120 Ω** termination at bus ends.
- **Wrong device:** `alm_address` must match WebConfig Modbus address.
- **Latched won't clear:** press **Ack** button (coil **4** or **5–7**); non-latched groups clear when the input returns to normal.
- **Arm/Disarm no effect:** enable **Local arming** in WebConfig first; use **Armed** switch (coil **3**).

### 9.6 Notes & versions

- Package matches `default_alm_173_r1_plc.yaml` — **MAP v4** (firmware **0.2.0**); poll **IREG 0..3** in one block (`update_interval: 500ms` recommended).
- For multiple ALM modules, duplicate the package block with unique `alm_id`, `alm_prefix`, and `alm_address`.

---

## 10. Programming & Build

### 10.1 Supported toolchains

- **Arduino IDE** / **arduino-cli** (Generic RP2350)
- **PlatformIO**
- **MicroPython** (community RP2350 builds)

### 10.2 Flashing (USB-C)

| Combination | Action |
|-------------|--------|
| **Buttons 1 + 2** | **BOOT** mode → RPI-RP2 UF2 drive |
| **Buttons 3 + 4** | Hardware **RESET** |

1. Connect USB-C; enter BOOT (Buttons 1+2).
2. Copy UF2 from [§12 Downloads](#12-downloads--resources) or build from source.
3. Configuration in LittleFS is preserved unless factory reset.

### 10.3 Build notes

- **FQBN:** `rp2040:rp2040:generic_rp2350:flash=2097152_1048576` (see [`sketch.yaml`](Firmware/v0.2.0/default_alm_173_r1/sketch.yaml))
- **Libraries:** Arduino_JSON, Modbus-Serial, Simple Web Serial, PCF8574
- **Reproducible build:** [Build environment](../README.md#build-environment-reproducible)

### 10.4 Firmware updates

See [§10.2 Flashing](#102-flashing-usb-c) and [§12 Downloads](#12-downloads--resources). Drag-and-drop the release **UF2** in BOOT mode (Buttons 1+2). Configuration in LittleFS is preserved across normal updates unless **Factory reset** is used from WebConfig.

---

## 11. Maintenance & Troubleshooting

### 11.1 Status LEDs

- **PWR** — ON in normal operation
- **TX/RX** — blink on Modbus traffic
- **User LEDs (1–4)** — follow alarm/override mapping (Steady / Blink per [§7 WebConfig](#7-webconfig-reference))

### 11.2 Resets

- **Power cycle:** remove 24 V, wait 5 s, re-apply
- **Buttons 3 + 4** — hardware **RESET**
- **WebConfig → Reboot** — soft restart (settings preserved)
- **WebConfig → Factory reset** — restores defaults (clears flash config)

### 11.3 Common issues

| Symptom | Checks |
|---------|--------|
| No Modbus | Address/baud match controller; A/B polarity; termination; COM/GND reference |
| Input stuck / wrong | Enable/Invert/Group in WebConfig; wiring **INx–GND I.x** (isolated return) |
| Relay won't follow group | Relay **Enabled**; **Alarm Group** assigned; group **Mode** not None; not in button/manual override |
| Latched alarm won't clear | Send **Ack** (button, coil **4** or **5–7**); check group mode |
| **LED4 dark / Button4 dead** | Requires firmware **v0.2.0** (LED4 pin fix) — see [§12 Downloads](#12-downloads--resources) |
| USB won't connect | Chromium-based browser; close other serial apps using the port |
| Config lost | Factory reset clears flash; normal firmware updates preserve config |

**Version history**

| Version | Status | Config / firmware path |
|---------|--------|-------------------------|
| **v0.2.0** | **Current; shipped on new modules** | `ALM-173-R1/Firmware/v0.2.0/` |
| v0.1.0 | Legacy | `ALM-173-R1/Firmware/v0.1.0/` |

> **Firmware shipped on new modules:** `v0.2.0`

---

## 12. Downloads & Resources

| Resource | Link |
|----------|------|
| **Firmware source** | [`Firmware/v0.2.0/default_alm_173_r1/`](Firmware/v0.2.0/default_alm_173_r1/) |
| **Pre-built UF2** | [`default_alm_173_r1.uf2`](https://github.com/isystemsautomation/homemaster-dev/raw/refs/heads/main/ALM-173-R1/Firmware/v0.2.0/default_alm_173_r1.uf2) |
| **ESPHome YAML** | [`default_alm_173_r1_plc.yaml`](Firmware/v0.2.0/default_alm_173_r1_plc/default_alm_173_r1_plc.yaml) |
| **WebConfig** | [config.home-master.eu v0.2.0](https://config.home-master.eu/ALM-173-R1/Firmware/v0.2.0/ConfigToolPage.html) |
| **Schematics** | [`Schematics/`](Schematics/) |
| **Datasheet** | [`ALM-173-R1_Datasheet.pdf`](Manuals/ALM-173-R1_Datasheet.pdf) |

---

## Open Source & Licensing

This project uses a hybrid licensing model.

**Hardware** — schematics, PCB layouts, BOMs: **CERN-OHL-W v2** ([`Schematics/LICENSE`](Schematics/LICENSE))

**Firmware & ESPHome integration** — firmware, ESPHome configs, software: **MIT License** ([`Firmware/LICENSE`](Firmware/LICENSE))

This ensures full compatibility with ESPHome and Home Assistant while protecting hardware designs. See LICENSE files in each directory for full terms.

---

## 13. Compliance & Certifications

The ALM-173-R1 is CE marked. **ISYSTEMS AUTOMATION S.R.L.** (HomeMaster® brand) maintains technical documentation and EU DoC.

| Directive | Standard |
|-----------|----------|
| EMC 2014/30/EU | EN 55032 (Class B), EN 55035 |
| LVD 2014/35/EU | EN 62368-1:2020 + A11:2020 |
| RoHS 2011/65/EU | EN IEC 63000 |

| Document | File |
|----------|------|
| EU DoC | [DoC_ALM-173-R1.pdf](Manuals/DoC_ALM-173-R1.pdf) |
| Datasheet | [ALM-173-R1_Datasheet.pdf](Manuals/ALM-173-R1_Datasheet.pdf) |

**HomeMaster®** — EUTM No. 019082911 (EUIPO, 15 January 2025).

---

## FAQ

### Can I connect wired sensors to Home Assistant with this?

Yes. The ALM-173-R1 exposes **17** opto-isolated inputs over **RS-485 Modbus RTU**. With a MiniPLC or MicroPLC running ESPHome, those inputs appear in Home Assistant as ready-made entities. Any Modbus master or SCADA system can poll the same registers.

### Does it power the sensors, or do I need a separate supply?

The module includes an isolated **+12 V** rail (**PS/1**, about **150 mA** usable) and an isolated **+5 V** rail (**PS/2**) for detector and sensor power. Many 12 V PIRs, glass-break and smoke detectors with relay outputs can run from the module. Budget the total current — if the load exceeds 150 mA on the 12 V rail, use an external supply.

### How many motion sensors can I connect?

Up to **17** inputs are available. On the 12 V rail, typical PIR detectors draw 10–20 mA each, so roughly **6–10** PIRs fit within the **150 mA** budget. The exact count depends on each detector's datasheet current.

### Is this an alternative to Konnected?

Similar idea, different design. Konnected targets retrofitting existing alarm panels; this is a DIN-rail module for new installations, with sensor power built in, RS-485 instead of Wi-Fi, and alarm logic on the module rather than the server.

### Does it work if Home Assistant goes down?

Yes for the on-board alarm engine. Groups, latching, relays and optional local arming continue on the module when the network or Home Assistant is offline. Full Home/Away/Night modes and codes still belong in Home Assistant.

### Can I connect 230 V devices to the inputs?

No. The inputs are SELV dry-contact / **5 V DC** signalling only. Use an interposing contactor or relay for mains voltages.

### Can it detect a cut sensor cable?

There is no EOL resistor line-supervision. Wiring contacts **normally closed** with **Invert** enabled in WebConfig makes a cut or disconnected cable read the same as an activation, so a damaged loop is visible instead of failing silently. Wire security zones NC.

### Can I count pulses from a water or energy meter?

No. This module does not provide pulse counting. Use the **DIO-430-R1** for S0 / pulse inputs.

### What temperature and humidity sensors work with it?

Dry-contact and relay-output detectors only. Analogue sensors (**0–10 V**, **4–20 mA**, **PT100**, **PT1000**) need the **AIO-422-R1**. Temperature and humidity I²C or 1-Wire sensors are not wired to this module's inputs.

### Do I need special software to set it up?

No. Configuration uses the browser-based WebConfig over USB-C in a Chromium-based browser. No app or login is required. Settings persist in on-device flash (LittleFS).

---

## 14. Support

- [WebConfig v0.2.0](https://config.home-master.eu/ALM-173-R1/Firmware/v0.2.0/ConfigToolPage.html)
- [Official support](https://www.home-master.eu/support)
- [GitHub repository](https://github.com/isystemsautomation/homemaster-dev/tree/main/ALM-173-R1)

**Manufacturer:** ISYSTEMS AUTOMATION S.R.L. · [www.home-master.eu](https://www.home-master.eu)
