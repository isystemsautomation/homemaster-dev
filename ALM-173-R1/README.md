![Modbus](https://img.shields.io/badge/Protocol-Modbus%20RTU-brightgreen)
![License](https://img.shields.io/badge/License-GPLv3%20%2F%20CERN--OHL--W-blue)

# ALM-173-R1 — Alarm & Annunciator I/O Module

**HOMEMASTER – Modular control. Custom logic.**

![ALM-173-R1 module photo](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/ALM-173-R1/Images/photo1.png)

**Document map:** [§1 Overview](#1-overview) · [§2 Features](#2-features--applications) · [§3 Alarm logic](#3-alarm-logic--how-it-works) · [§4 Specifications](#4-specifications) · [§5 Hardware](#5-hardware--interface) · [§6 Getting Started](#6-installation--getting-started) · [§7 WebConfig](#7-webconfig-reference) · [§8 Modbus map](#8-modbus-register-map) · [§9 ESPHome](#9-esphome--home-assistant-integration) · [§10 Programming](#10-programming--build) · [§11 Maintenance](#11-maintenance--troubleshooting) · [§12 Downloads](#12-downloads--resources) · [§13 Compliance](#13-compliance--certifications) · [§14 Support](#14-support)

**Firmware shipped on new modules:** `v0.2.0`

```yaml
packages:
  alm1:
    url: https://github.com/isystemsautomation/homemaster-dev
    ref: main
    files:
      - path: ALM-173-R1/Firmware/v0.2.0/default_alm_173_r1_plc/default_alm_173_r1_plc.yaml
        vars:
          alm_prefix: "ALM#1"
          alm_id: alm_1
          alm_address: 3
```

---

## 1. Overview

The **ALM-173-R1** is a configurable **alarm and annunciator I/O module** for intrusion detection, fault signalling, and local supervision. It mounts on a **35 mm DIN rail**, connects to a **MicroPLC, MiniPLC, or any Modbus RTU master** over **RS-485**, and is configured via **USB-C WebConfig** (Web Serial).

**Key capabilities at a glance:**

- **17 opto-isolated digital inputs** — 5 V DC signalling; **3.75 kVrms** optocoupler isolation per channel
- **3 SPDT dry-contact relays** — **3 A @ 250 VAC** (resistive); follow alarm groups, Modbus manual override, or button override
- **4 buttons + 4 user LEDs** — acknowledge alarms, relay override (long-hold 3 s), status indication
- **On-board alarm engine** — inputs (zones) → **Alarm Groups 1–3** → relays/LEDs; non-latched or latched-until-ack modes
- **Driverless WebConfig** — USB-C + any Chromium-based browser; no app or login
- **Persistent settings** — configuration stored in LittleFS flash

Alarm groups, zone types, local arming, and bell cut-off are described in [§3 Alarm Logic — How It Works](#3-alarm-logic--how-it-works). This is **not** DIO-style per-input relay mapping.

> **Quick path:** wire inputs → assign groups → set latch modes → map relays/LEDs → RS-485 + WebConfig address/baud → integrate with PLC or Home Assistant.

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
| **Modbus RTU slave** | Discrete Inputs (telemetry) + Coils (commands); config via WebConfig, not holding registers |
| **ESPHome / HA** | Ready-made YAML package; inputs, alarms, relays, ack/override actions |
| **Identity** | **MODEL_ID = 1**; firmware **0.2.0**; **MAP_VERSION = 2** (Input Registers; see [§8](#8-modbus-register-map)) |
| **Extras** | PLC group pulses (510–512); optional **local arming**; **bell cut-off**; per-zone alarm memory |

### Applications

Typical uses for the ALM-173-R1:

- **Intrusion / zone alarm panels** — map PIR/door contacts to groups; drive sirens on latched groups
- **Equipment-room annunciators** — aggregate fault inputs into summary relays and front-panel LEDs
- **BMS / SCADA alarm expansion** — Modbus RTU endpoint with local ack and override
- **Access-control supervision** — door contacts, strike monitoring, summary strobe
- **Home Assistant** — via ESPHome on MiniPLC/MicroPLC; alarm entities and acknowledge automations

---

## 3. Alarm Logic — How It Works

> **Zones, groups, relays.** Each input is a **zone** assigned to one of three **Alarm Groups** (G1–G3). Each group has a mode: **None** (disabled), **Non-latched** (follows live zone state), or **Latched** (trips on activation and holds until **ACK**). Each relay is bound to a group and energizes while that group is in alarm. Relay control priority: **Button override → Modbus manual → Group**.

**PLC group pulses** (Modbus coils 510–512) can force a one-scan group activation for controller-driven annunciation. **Button override:** long hold **3 s** on a button configured for Relay 1–3 enters manual relay control; another long hold exits and returns control to the alarm group.

### Zone types (when Local arming is enabled)

Each zone has a type that defines when its activation contributes to its group:

| Zone type | Disarmed | Armed | Typical use |
|-----------|----------|-------|-------------|
| **Instant** | does not trigger | triggers immediately | Perimeter: windows, glass-break |
| **Delayed** | does not trigger | starts **Entry delay**; alarm if not disarmed in time | Entry doors |
| **24h / Tamper** | **always triggers** | **always triggers** | Tamper, sabotage, 24-hour loops |

### Local arming (optional, default off)

The module supports minimal **local arming** so protection continues if the controller is offline:

- **ARM** (coil **530** or configured button) → **Exit delay** (**Exit-pending**): activations are ignored so you can leave. When the timer expires → **Armed**.
- While **Armed**: Instant zones trigger immediately; a **Delayed** zone starts **Entry delay** (**Entry-pending**) — if **DISARM** (coil **531**) does not arrive before the timer expires, the group alarms; **24h/Tamper** zones trigger regardless.
- **DISARM** → **Disarmed** immediately; Entry/Exit-pending are cleared. **Latched groups are not cleared** — only **ACK** (coil **500** or **501–503**) clears them.
- With **Local arming disabled** (factory default), behaviour matches legacy firmware: all enabled zones feed their group at all times; zone types and arm delays are ignored.

**State machine (simplified):** `Disarmed → (ARM) → Exit-pending → Armed → (Delayed zone) → Entry-pending → Triggered → (ACK) → Armed/Disarmed`. Non-latched, latched, and 24h/Tamper behaviour apply on top of this.

### Bell cut-off (siren timeout)

Each relay can be configured with an **auto-off** time (seconds). When a relay is ON **because its alarm group is active**, if it stays ON longer than the configured time it is turned OFF (the siren stops), but the **group remains latched** — Home Assistant / the PLC still sees the alarm. A new alarm edge on that group can turn the relay ON again. **0** = disabled.

### Alarm memory (per-zone latch)

When local arming is enabled and a zone contributes to a **latched** group alarm, the module records **which zones** triggered (Modbus Discrete Inputs **100–116**). Cleared by **ACK** for the relevant group(s).

### Module vs Home Assistant

Full alarm-panel features — **Home / Away / Night** modes, codes, keypads, schedules — belong in the **Home Assistant Alarm Control Panel** (the de-facto standard). The ALM exposes zones, drives sirens/relays, and provides optional minimal local arming for resilience.

**Limitations:** No **EOL line-supervision** — dry contact / SELV signalling only; no analogue end-of-line resistor networks.

---

## 4. Specifications

### 4.1 I/O summary

| Subsystem | Qty | Description |
|-----------|-----|-------------|
| Digital Inputs | 17 | Opto-isolated, 5 V DC; 3.75 kVrms isolation; dry contact / SELV |
| Relays | 3 | SPDT (NO/NC/COM), HF115F/005-1ZS3; **3 A @ 250 VAC** resistive (module rating) |
| Buttons | 4 | Configurable ack / relay override |
| User LEDs | 4 | Configurable Steady/Blink + PWR/TX/RX status |
| Modbus RTU | 1 | RS-485; address 1–247; 9600–115200 baud |
| USB-C | 1 | WebConfig (Web Serial); UF2 flashing |
| Power | 24 V DC | 18–30 V DC nominal; 1 A time-lag fuse, reverse diode, TVS |
| Sensor rails | 2 | Isolated **+12 V** (PS/1) and **+5 V** (PS/2); ~2 W / ~150 mA usable on 12 V rail |
| MCU | RP2350A | Dual-core; QSPI flash; LittleFS |

### 4.2 Electrical ratings

| Parameter | Min | Typ | Max | Unit | Notes |
|-----------|----:|----:|----:|:----:|-------|
| Supply voltage | 18 | 24 | 30 | V DC | SELV; 1 A time-lag fuse, reverse-polarity diode, TVS |
| Module power | — | 1.85 | 3.0 | W | Excludes external relay load currents |
| Digital inputs | — | 5 | — | V DC | Opto-isolated; 3.75 kVrms (optocoupler) |
| Relay contact (module) | — | — | 3 | A | @ 250 VAC resistive |
| Relay contact voltage | — | — | 250 | V AC | or 30 V DC max |
| RS-485 data rate | — | 19.2 | 115.2 | kbps | Default 19200 8N1 |
| Operating temp. | 0 | — | 40 | °C | ≤ 95 % RH, non-condensing |

> **Relay component vs module rating:** Relay components (HF115F class) are rated up to **12 A @ 250 VAC** at the device level. **This chip rating does NOT apply to the module** — PCB traces, terminals, and compliance testing limit the **module output to 3 A @ 250 VAC (resistive)**. Use interposing contactors for higher or inductive loads.

> **Power budgeting:** logic + LEDs + relay coils + sensor rails → add ≥ 30 % PSU headroom.

### 4.3 Mechanical & environmental

| Property | Specification |
|----------|---------------|
| Mounting | DIN-rail EN 50022 (35 mm) |
| Enclosure | PC/ABS V-0 |
| Dimensions | 157.4 × 91 × 58.4 mm (W × H × D) |
| Terminals | Pluggable 5.08 mm; 0.2–2.5 mm²; 0.4 Nm max |
| Ingress protection | IP20 (panel interior) |
| Operating temp | 0–40 °C, ≤ 95 % RH (non-condensing) |

![ALM-173-R1 Dimensions](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/ALM-173-R1/Images/ALMMDimensions.png)

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
- Opto-isolated digital inputs (3.75 kVrms); isolated sensor rails with PTC/fuse limiting.
- Relay drivers with onboard suppression; add external RC/MOV for inductive field loads.
- RS-485: TVS, series protection, fail-safe biasing.
- USB-C ESD-protected; service port only.
- Auto-save to flash after WebConfig changes (~1.5 s quiet period).

---

## 5. Hardware & Interface

### 5.1 Diagrams & pinouts

<table>
  <tr>
    <th>System block</th><th>MCU pinout</th><th>Field board</th><th>MCU board</th>
  </tr>
  <tr>
    <td><img width="240" src="https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/ALM-173-R1/Images/ALM_SystemBlockDiagram.png"></td>
    <td><img width="240" src="https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/ALM-173-R1/Images/ALM_MCU_Pinouts.png"></td>
    <td><img width="240" src="https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/ALM-173-R1/Images/FieldBoard-Diagram.png"></td>
    <td><img width="240" src="https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/ALM-173-R1/Images/MCUBoard-Diagram.png"></td>
  </tr>
</table>

### 5.2 Connectors & terminal map

| Block | Pins | Function | Notes |
|-------|------|----------|-------|
| **POWER** | V+, 0V | 24 V DC input | 18–30 V DC; SELV; fused |
| **DI1…DI17** | INx, GND I.x | Opto-isolated inputs | 5 V DC loop; dry contact or SELV; each return isolated |
| **RELAY1–3** | NO, C, NC | SPDT dry contacts | 3 A @ 250 VAC module rating |
| **PS/1** | +12 V ISO | Sensor supply | ~2 W (~150 mA usable); isolated |
| **PS/2** | +5 V ISO | Sensor supply | Low-power sensors only; PTC limited |
| **RS-485** | A, B, COM | Modbus RTU | Terminate 120 Ω at bus ends |
| **USB-C** | — | WebConfig / UF2 | Not a field power source |

![Terminal labeling](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/ALM-173-R1/Images/photo1.png)

**Digital inputs.** Each input is **opto-isolated** (5 V DC signalling, **3.75 kVrms** optocoupler isolation). Wire a dry contact between **INx** and **GND I.x**. Do not apply mains or non-SELV voltages.

**Sensor rails.** **PS/1 (+12 V)** and **PS/2 (+5 V)** are isolated, fuse/PTC limited outputs for **low-power sensors only**. Do not backfeed or parallel with external supplies.

### 5.3 I/O warnings

| Area | Warning |
|------|---------|
| **24 V input** | SELV only; correct polarity; upstream fuse |
| **Inputs** | Dry contact / SELV only; respect Enable/Invert/Group in WebConfig |
| **Relays** | Dry contacts; **3 A @ 250 VAC** module limit; snub inductive loads |
| **Sensor rails** | Low power only; shorts may trip PTCs |
| **RS-485** | Twisted pair; daisy-chain; 120 Ω at both ends; shared COM/GND |
| **USB-C** | Setup/maintenance only |
| **Buttons** | Can ack alarms or override relays — document procedures for safety-critical installs |

### 5.4 Front panel — buttons & LEDs

![Button layout](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/ALM-173-R1/Images/buttons1.png)

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

### 6.3 Step-by-step

**Phase 1 — Wire**

| 24 V DC | Digital inputs | Relays | RS-485 |
|:---:|:---:|:---:|:---:|
| ![24 V](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/ALM-173-R1/Images/ALM_24Vdc_PowerSupply.png) | ![DI](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/ALM-173-R1/Images/ALM_DigitalInputs.png) | ![Relays](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/ALM-173-R1/Images/ALM_RelayConnection.png) | ![RS-485](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/ALM-173-R1/Images/ALM_RS485Connection.png) |

**Phase 2 — Configure (WebConfig)**

1. Connect **USB-C**; open [WebConfig v0.2.0](https://config.home-master.eu/ALM-173-R1/Firmware/v0.2.0/ConfigToolPage.html) → **Connect**.
2. Set **Modbus address** and **baud** ([§4.4](#44-communication-defaults)).
3. Configure alarm modes, inputs, relays, buttons, LEDs ([§7](#7-webconfig-reference)).
4. Disconnect USB-C; hand control to RS-485 master.

**Phase 3 — Integrate**

Add ESPHome package on controller ([§9](#9-esphome--home-assistant-integration)) or poll Modbus from PLC/SCADA ([§8](#8-modbus-register-map)).

### 6.4 Verify

| Check | Expected |
|-------|----------|
| **PWR LED** | ON |
| **TX/RX** | Blink on Modbus traffic |
| **Inputs** | Live dots in WebConfig; IN1–IN17 on Modbus DI 1–17 |
| **Alarms** | Group / Any indicators follow wiring |
| **Relays** | Follow group or manual override |

---

## 7. WebConfig Reference

Open **[ALM-173-R1 WebConfig v0.2.0](https://config.home-master.eu/ALM-173-R1/Firmware/v0.2.0/ConfigToolPage.html)** in a **Chromium-based browser** (Chrome, Edge, Opera, Brave, Vivaldi; Chrome/Edge 89+, Opera 76+). **Firefox:** experimental only (Nightly with Web Serial enabled). **Safari** and stable Firefox are not supported.

![WebConfig — overview, connection & status (Armed/Entry/Exit/Tamper)](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/ALM-173-R1/Images/webconfig1.png)

| Section | Settings |
|---------|----------|
| **Device setup** | Modbus address (1–247), baud 9600–115200 |
| **Alarm status & modes** | Live Any / G1 / G2 / G3; per-group None / Active-while / Latched |
| **Digital inputs (17)** | Enable, Invert, Alarm Group, **Zone type** (Instant / Delayed / 24h-Tamper) |
| **Relays (3)** | Enable, Invert, Alarm Group, Power-on, **Bell cut-off (s)** |
| **Local arming** | Opt-in (default off); **Entry** / **Exit delay (s)**; status pills Armed/Entry/Exit/Tamper |
| **Buttons (4)** | Action: None, Ack All, Ack G1–G3, Relay override |
| **LEDs (4)** | Mode Steady/Blink; source Any / G1–G3 / override |
| **Tools** | Identify (~5 s), Factory reset, Reboot |

![Device setup — Modbus address & baud, serial log](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/ALM-173-R1/Images/webconfig2.png)  
![Alarm status & per-group latch modes](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/ALM-173-R1/Images/webconfig3.png)  
![Digital inputs — enable, invert, alarm group, zone type (latched badge)](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/ALM-173-R1/Images/webconfig4.png)  
![Relays (bell cut-off), buttons & user LEDs](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/ALM-173-R1/Images/webconfig5.png)

Changes auto-save to flash after a short idle period. See [§3 Alarm Logic](#3-alarm-logic--how-it-works) for zone types, local arming, and bell cut-off behaviour.

---

## 8. Modbus Register Map

**Slave:** Modbus RTU over RS-485 (8N1). **Configuration** is via WebConfig (LittleFS), **not** holding registers.

**Identity (Input Registers FC04, base 200 / 0x00C8):** MODEL_ID, FW_MAJOR, FW_MINOR, FW_PATCH, **MAP_VERSION** (= **2** in firmware 0.2.0 with alarm extensions).

### 8.1 Discrete Inputs (FC02) — telemetry

| Address | Name | Description |
|---------|------|-------------|
| 1–17 | **IN1…IN17** | Input state (after enable + invert) |
| 50 | **Any Alarm** | Any group active |
| 51–53 | **Alarm G1…G3** | Group alarm active |
| 60–62 | **Relay 1…3** | Effective relay output |
| 90–93 | **LED 1…4** | Physical LED state |

### 8.2 Coils (FC01/05) — commands (write `1`, auto-clear)

| Address | Name | Description |
|---------|------|-------------|
| 200–216 | **Enable IN1…17** | Enable input |
| 300–316 | **Disable IN1…17** | Disable input |
| 400–402 | **Relay ON** | Manual override ON (per relay) |
| 420–422 | **Relay OFF** | Manual override OFF |
| 500 | **Ack All** | Clear all latched groups |
| 501–503 | **Ack G1…G3** | Clear latched group |
| 510–512 | **Alarm pulse G1…G3** | One-scan group activation (PLC) |

**Override priority:** Button override → Modbus manual override → Alarm group.

Manual override coils hold until cleared (matching OFF coil or override released).

### 8.3 MAP v2 — extended alarm features (firmware 0.2.0)

Additional Discrete Inputs and Coils when local arming / zone types are used:

| Address | Name | Description |
|---------|------|-------------|
| 70 | **Armed** | Local arming active |
| 71 | **Entry pending** | Entry delay in progress |
| 72 | **Exit pending** | Exit delay in progress |
| 73 | **Tamper any** | Any 24h/Tamper zone fault |
| 100–116 | **Zone latched** | Per-input alarm memory (IN1…IN17) |
| 530 | **ARM** | Start arm / exit delay (pulse; requires local arming enabled) |
| 531 | **DISARM** | Disarm (pulse) |

When **local arming is disabled** (default), zone types and arm delays are ignored — behaviour matches the base MAP (§8.1–8.2).

---

## 9. ESPHome & Home Assistant Integration

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

### 9.2 Entities (summary)

- **Binary sensors:** IN1–IN17, Any Alarm, G1–G3, relay/LED mirrors; **MAP v2:** Armed, Entry/Exit pending, Tamper, Zone 1–17 latched
- **Switches / buttons:** Relay ON/OFF, Ack All / G1–G3, **Arm / Disarm**, override helpers
- Configure LED/button mapping on-module via WebConfig; HA consumes resulting states

### 9.3 Troubleshooting

- **Timeouts:** A/B polarity, COM/GND reference, termination
- **Wrong address:** `alm_address` must match WebConfig
- **Latched won't clear:** send Ack; input must return to normal for non-latched groups

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
- **Reproducible build:** [Build environment](../../README.md#build-environment-reproducible)

---

## 11. Maintenance & Troubleshooting

| Symptom | Checks |
|---------|--------|
| No Modbus | Address/baud, A/B, termination, COM/GND |
| Input stuck | Enable/Invert/Group; wiring INx–GND I.x |
| Relay won't follow group | Relay enabled; group assigned; not in button/manual override |
| USB won't connect | Chromium browser; close other serial apps |
| Config lost | Factory reset clears flash; normal updates preserve config |

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

## 13. Compliance & Certifications

The ALM-173-R1 is CE marked. **ISYSTEMS AUTOMATION S.R.L.** (HomeMaster® brand) maintains technical documentation and EU DoC.

| Directive | Standard |
|-----------|----------|
| EMC 2014/30/EU | EN 55032 (Class B), EN 55035 |
| LVD 2014/35/EU | EN 62368-1:2020 + A11:2020 |
| RoHS 2011/65/EU | EN IEC 63000 |

| Document | File |
|----------|------|
| EU DoC | [DoC-ALM-173-R1-V1.0.pdf](Manuals/DoC-ALM-173-R1-V1.0.pdf) |
| Datasheet | [ALM-173-R1_Datasheet.pdf](Manuals/ALM-173-R1_Datasheet.pdf) |

**HomeMaster®** — EUTM No. 019082911 (EUIPO, 15 January 2025).

---

## 14. Support

- [WebConfig v0.2.0](https://config.home-master.eu/ALM-173-R1/Firmware/v0.2.0/ConfigToolPage.html)
- [Official support](https://www.home-master.eu/support)
- [GitHub repository](https://github.com/isystemsautomation/homemaster-dev/tree/main/ALM-173-R1)

**Manufacturer:** ISYSTEMS AUTOMATION S.R.L. · [www.home-master.eu](https://www.home-master.eu)
