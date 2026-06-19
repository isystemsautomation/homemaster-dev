![Modbus](https://img.shields.io/badge/Protocol-Modbus%20RTU-brightgreen)
![License](https://img.shields.io/badge/License-GPLv3%20%2F%20CERN--OHL--W-blue)

# DIO-430-R1 — Module for Smart I/O Control

**HOMEMASTER – Modular control. Custom logic.**

![MODULE photo](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/DIO-430-R1/Images/photo1.png)

**Document map:** [§1 Overview](#1-overview) · [§3 Specifications](#3-specifications) · [§4 Hardware](#4-hardware--interface) · [§5 Getting Started](#5-getting-started) · [§6 WebConfig](#6-webconfig-reference) · [§7 Modbus map](#7-modbus-register-map) · [§8 ESPHome](#8-esphome--home-assistant-integration) · [§9 Programming](#9-programming--build) · [§11 Downloads](#11-downloads--resources)

---

## 1. Overview

The **DIO-430-R1** is a configurable smart digital I/O module for **digital input monitoring and relay-based output control** in building automation, lighting, HVAC, alarms, and general control systems. It mounts on a **35 mm DIN rail** and connects to a **MicroPLC, MiniPLC, or any Modbus RTU master** over **RS-485**, with optional **Home Assistant (ESPHome)** integration.

**Key capabilities at a glance:**

- **4 opto-isolated digital inputs** — dry contacts or 24 V signals; per-input Maintained/Momentary logic
- **3 SPDT relays** — 3 A @ 250 VAC (resistive) dry contacts (NO/NC/COM); use interposing contactors for loads above 3 A
- **3 buttons (2 user-configurable).** Button 1 and Button 2 are configurable in WebConfig (short/long-press actions). The third button has no software function — it is used only as part of the on-board key combination for USB firmware-update (BOOTSEL) and reset.
- **3 configurable user LEDs** — Steady/Blink; multiple sources (Link, HA, relay, etc.)
- **Standalone local logic** — wall switches and front buttons work even when the network or controller is offline
- **Driverless WebConfig** — USB-C + any Chromium-based browser (Chrome, Edge, Opera, Brave, Vivaldi; Chrome/Edge 89+, Opera 76+). No app or login required.
- **Persistent settings** — configuration stored in LittleFS flash and restored on boot

### How local and remote control coexist

Relays can be switched from **any** source — wired inputs, front buttons, or Home Assistant — **simultaneously**; the last command wins. There is no "local vs remote" mode to flip: if the network or controller reboots, wall switches **still work**; when online, you also get full remote control.

---

## 2. Features

| Area | Detail |
|------|--------|
| **Isolation** | Galvanically isolated DI front-end (ISO1212 class); opto-isolated relay drivers |
| **Configurable I/O** | Per-input Enable/Invert/**Type** (Maintained or Momentary). **Maintained** → mode Toggle/Follow + target relay. **Momentary** → Short/Long actions from {None, Toggle, On, Off, All off} + target (R1–R3 or All). |
| **Buttons** | 3 buttons (2 user-configurable): Button 1 / Button 2 assignable to relay actions (toggle, on, off, all off) |
| **LEDs** | Configurable Steady/Blink; 8 firmware sources (Off, HA, Link, Local, Child lock, Safe mode, Identify, Relay) |
| **WebConfig** | USB-C → Chromium-based browser (Chrome, Edge, Opera, Brave, Vivaldi); set comms and I/O mapping live; auto-save to flash |
| **Modbus RTU slave** | Poll-based RS-485; factory defaults in [§3 Specifications](#3-specifications) |
| **ESPHome / HA** | Ready-made YAML package; DI/relay/button/event entities |
| **Extras** | All Off, child-lock per input, auto-off (staircase timer), shutter/interlock mode, Identify |

### Applications

Typical uses for the DIO-430-R1:

- **Lighting** — wall switches or push-buttons drive the relays directly, with optional auto-off (staircase) timers. A controller can override or monitor the same outputs over Modbus.
- **Pumps, fans, motors** — switch circulation pumps, exhaust fans, or irrigation valves from inputs, buttons, or a PLC. Relay interlock prevents two outputs (e.g. motor up/down) from being on at the same time.
- **Standalone or controller-driven** — runs its configured logic with no controller attached, and integrates with Home Assistant / ESPHome or any Modbus RTU master when one is present.

---

## 3. Specifications

### 3.1 I/O summary

| Subsystem | Qty | Description |
|-----------|-----|-------------|
| Digital Inputs | 4 | Opto-isolated, dry contact compatible, noise-protected |
| Relays | 3 | SPDT (NO/NC), 3 A @ 250 VAC (resistive), dry contacts |
| LEDs | 3 | Configurable: Steady or Blink modes, linked to relays/logic |
| Buttons | 3 | 3 buttons (2 user-configurable); third — boot/reset combo only |
| Modbus RTU | Yes | RS-485 interface (address 1–247, 9600–115200 baud) |
| USB-C | Yes | WebConfig via Web Serial (Chromium-based browsers; see [§6](#6-webconfig-reference)) |
| Power | 24 V DC | Fused input, reverse-polarity and surge protected |
| MCU | RP2350 | Dual-core, QSPI flash, USB, UART, LittleFS |
| Protection | TVS, PTC | ESD, surge, and short-circuit protection on I/O and power |

| Interface | Qty | Description |
|-----------|----:|-------------|
| **Digital Inputs** | 4 | Galvanically isolated (ISO1212 class). Dry contacts or 24 V signals. PTC + TVS per channel. |
| **Relay Outputs** | 3 | SPDT (NO/NC/COM), 3 A @ 250 VAC (resistive) dry contacts. Relay component rated higher, but module output is limited to 3 A — use interposing contactors for larger or inductive/mains loads. |
| **User LEDs** | 3 | Configurable (Steady/Blink). Follow relay or logic status. |
| **Buttons** | 3 | Momentary. 3 buttons (2 user-configurable); third — boot/reset combo only. |
| **RS-485 (Modbus RTU)** | 1 | A/B/COM terminals. Daisy-chain. 120 Ω termination at both ends. |
| **USB-C** | 1 | Web Serial setup, diagnostics, firmware flashing (ESD-protected). |
| **Power Input** | 1 | 24 V DC SELV. Reverse-polarity + surge protected. |

### 3.2 Electrical ratings

| Parameter | Min | Typ | Max | Unit | Notes |
|-----------|----:|----:|----:|:----:|-------|
| Supply Voltage | 22 | 24 | 28 | V DC | SELV/PELV input |
| Logic Consumption | – | 1.5 | 3.0 | W | Excludes relay loads |
| Digital Input Range | 0 | 24 | 30 | V DC | Isolated, noise-protected |
| Relay Contact Current | – | – | 3 | A | @ 250 VAC resistive; module/trace-limited (relay component rated higher) |
| Relay Contact Voltage | – | – | 250 | V AC | or 30 V DC max |
| RS-485 Data Rate | – | 19.2 | 115.2 | kbps | Default 19200 8N1 |
| USB-C Voltage | 4.75 | 5.0 | 5.25 | V DC | Service only |
| Operating Temp. | 0 | – | 40 | °C | ≤ 95 % RH, non-condensing |

> **Power budgeting:** logic + LEDs + up to 3 relay coils + sensor loads → add ≥ 30 % PSU headroom.

### 3.3 Mechanical & environmental

| Property | Specification |
|----------|---------------|
| Mounting | DIN-rail EN 50022 (35 mm) |
| Enclosure | PC/ABS V-0, panel mount |
| Dimensions | 70 × 90.6 × 67.3 mm (W × H × D) |
| Terminals | Pluggable 5.08 mm, 26–12 AWG (≤ 2.5 mm²), 0.5–0.6 Nm |
| Ingress Protection | IP20 (panel interior) |
| Operating Temp | 0–40 °C, ≤ 95 % RH (non-condensing) |

![DIO-430-R1 Dimensions](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/DIO-430-R1/Images/DIODimensions.png)

### 3.4 Communication defaults *(canonical)*

Factory settings applied to every new module:

| Parameter | Default |
|-----------|---------|
| **Modbus Address** | `3` |
| **Baud Rate** | `19200` |
| **Parity** | `None` |
| **Stop Bits** | `1` |

Address range **1–247**; baud options 9600 / 19200 / 38400 / 57600 / 115200. Change via [WebConfig](#6-webconfig-reference) or Modbus holding registers — see [§7 Modbus Register Map](#7-modbus-register-map).

The module communicates over **RS-485 Modbus RTU** (A/B differential + shared COM/GND). Configuration is stored persistently in **LittleFS** and can be changed live through **USB-C + WebConfig**.

### 3.5 Reliability & protection

- Reverse-path diode + high-side MOSFET on 24 V input.
- Local PTC + TVS protection on field interfaces.
- Relay drivers opto-isolated; RC/MOV suppression recommended.
- RS-485 with TVS, series protection, and fail-safe biasing.
- USB-C ESD-protected; CC resistors per spec.
- Non-volatile flash with **auto-save** after configuration changes.

---

## 4. Hardware & Interface

### 4.1 Diagrams & pinouts *(canonical)*

- ![System Block Diagram](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/DIO-430-R1/Images/DIO_SystemBlockDiagram.png)
- ![Control Board Diagram](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/DIO-430-R1/Images/ControlBoard_Diagram.png)
- ![Relay Board Diagram](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/DIO-430-R1/Images/RelayBoard_Diagram.png)
- ![RP2350A Pin Map](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/DIO-430-R1/Images/DIO_MCU_Pinouts.png)

### 4.2 Connectors & terminal map

| Block | Pins | Function | Notes |
|-------|------|----------|-------|
| **POWER** | 0V, V+ | 24 V DC input | Reverse/surge protected |
| **RELAY 1-3** | NO, C, NC | SPDT contacts | Add RC/MOV for inductive loads |
| **DI 1-4** | INx, GNDx | Isolated digital inputs | 24 V field or dry contact |
| **RS-485** | B, A, COM | Modbus RTU bus | Terminate 120 Ω at ends |
| **USB-C** | D+, D−, VBUS, GND | Setup / Service port | Not for field powering |

### 4.3 Front panel — buttons & LEDs

![Button Layout 1‑2‑3](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/DIO-430-R1/Images/buttons1.png)

| Control | Function |
|---------|----------|
| **Button 1 / 2** | User-configurable local relay control (see [§6 WebConfig](#6-webconfig-reference)) |
| **Button 3** | No software function — part of on-board boot/reset key combo (with Buttons 1+2 → BOOT mode) |
| **Buttons 1+2+3** | Enter **BOOT** mode for UF2 flashing (see [§9 Programming](#9-programming--build)) |
| **Buttons 1+3** | Hardware **RESET** |
| **PWR LED** | Power present |
| **TX/RX LEDs** | Modbus traffic activity |
| **User LEDs 1–3** | Configurable status indicators |

---

## 5. Getting Started

### 5.1 Safety *(read before wiring)*

> ⚠️ **SELV/PELV domains only** — 24 V DC, RS-485, USB 5 V. **Do not** connect mains to any terminal. Use interposing contactors for mains loads. **Never bridge** logic GND with isolated field grounds (GND_ISO / FGND).

| Requirement | Detail |
|-------------|--------|
| Qualified personnel | Installation by personnel familiar with 24 V control and RS-485 |
| Power isolation | Disconnect 24 V DC before wiring; lockout/tagout where applicable |
| Environmental limits | Clean, sealed enclosure; avoid condensation and conductive dust |
| Grounding | Bond panel to PE; share RS-485 COM/GND with controller (same SELV domain) |
| Relay loads | Follow contact ratings; add RC/MOV snubbers for inductive loads |
| RS-485 | Twisted pair, daisy-chain, **120 Ω** at both physical ends, consistent A/B polarity |
| USB-C | Setup/maintenance only; disconnect after commissioning |

**Pre-power checklist**

- [ ] Wiring torqued, labeled, strain-relieved
- [ ] No bridge between logic GND and isolated GND_ISO/FGND
- [ ] Panel PE bonded; SELV supply and COM/GND properly landed
- [ ] RS-485 A/B polarity and 120 Ω termination at bus ends
- [ ] Relay loads within rating; snubbers on inductive loads
- [ ] Inputs wired to dry contact/SELV only
- [ ] USB-C disconnected for normal operation

#### Installation practices

| Task | Guidance |
|------|----------|
| ESD Protection | Handle by the enclosure/edge only. Use an antistatic wrist strap when the board is exposed. |
| DIN Rail Mounting | Mount securely on **35 mm DIN** rail inside an IP-rated cabinet. Leave cable slack for strain relief. |
| Wiring | Use correct wire gauge and torque terminal screws. Separate **power**, **DI**, **relay**, and **RS-485** harnesses. |
| Isolation Domains | Respect isolation: **do not bridge** logic **GND** to isolated field grounds (e.g., **GND_ISO/FGND**). Keep analog/sensor returns on the isolated side. |
| Commissioning | Before power-up, verify polarity, relay NO/NC routing, RS-485 **A/B** orientation and termination. |

#### I/O & interface warnings

**Power**

| Area | Warning |
|------|---------|
| 24 V DC Input | Use a clean, fused SELV supply. Reverse-polarity protection exists but may disable the module when triggered. |
| Sensor Rail | Power sensors from a SELV rail. Observe polarity. Fuse external branches as required. |
| Surge/Noise | In noisy panels, add upstream surge/EMI suppression and keep high-current wiring away from control wiring. |

**Inputs (digital)**

| Area | Warning |
|------|---------|
| Type | **Dry contact / 24 V signaling only**. Do not inject mains or undefined levels. |
| Isolation | Inputs are isolated from logic. Keep sensor returns on the **field/isolated** domain; do not bond to logic GND. |
| Debounce | Firmware provides debounce; route away from contactors/VFDs; use shielded/twisted pairs for long runs. |
| Polarity | Configure invert/action in WebConfig; verify state transitions after wiring. |

**Relays (outputs)**

| Area | Warning |
|------|---------|
| Contact Type | **SPDT (NO/NC/COM)** dry contacts. Follow contact rating on the device label/datasheet. |
| Inductive Loads | Add **RC snubber or MOV** at the load. Consider interposing relays/contactors for higher power. |
| Separation | Keep relay load wiring physically separate from signal wiring. De-energize before servicing. |
| Verification | After wiring, verify NO/NC behavior and load polarity before enabling automation. |

**RS-485 (Modbus RTU)**

| Area | Warning |
|------|---------|
| Topology | Use twisted pair; **daisy-chain** (no stubs). Terminate with **120 Ω** at both physical ends. |
| Polarity | Maintain **A/B** polarity consistently. Share **COM/GND** reference between nodes (same SELV domain). |
| EMC | Route away from VFDs, contactors, and mains bundles. Use shielded cable in high-EMI environments. |
| Protection | Port includes protection; good wiring practice still required to avoid transients. |

**USB-C (setup)**

| Area | Warning |
|------|---------|
| Purpose | **Setup & maintenance only** (WebConfig / firmware). Not for powering field devices. |
| ESD/EMI | Avoid hot-plugging in high-EMI areas. Use a grounded service laptop. Disconnect after commissioning. |

**Front panel (buttons & LEDs)**

| Area | Warning |
|------|---------|
| Buttons & LEDs | Buttons can override relays; document operating procedures. Lock out overrides for safety-critical installs. |

**Shielding & EMC**

| Area | Recommendation |
|------|----------------|
| Cable Shields | Terminate shields at **one end** (typically the PLC/controller). Keep runs short and away from high-voltage/EMI sources. |

### 5.2 What you need

| Category | Item / Notes |
|----------|--------------|
| **Hardware** | DIO-430-R1 — 4× DI, 3× SPDT relays, 3 buttons (2 user-configurable), 3× LEDs, USB-C, RS-485 |
| **Controller** | HomeMaster MiniPLC/MicroPLC or any Modbus RTU master |
| **24 VDC PSU** | Regulated SELV; size for logic + relay coils + sensors; ≥ 30 % headroom |
| **RS-485 cable** | Twisted pair A/B + COM/GND; 120 Ω termination at both trunk ends |
| **USB-C cable** | WebConfig via Chromium-based browser (commissioning) |
| **Software** | Chromium-based browser with Web Serial (Chrome, Edge, Opera, Brave, Vivaldi); [WebConfig tool](https://www.home-master.eu/configtool-dio-430-r1) |

### 5.3 Power notes

The module uses **24 VDC** primary. Onboard regulation provides **5 V → 3.3 V** for logic; DI front-end is isolated.

- **24 VDC DIN-rail PSU** → **24Vdc(+) / 0V(–)** power terminals (top row: POWER).
- **Sensor side (DI)** — isolated receivers; feed sensors from the 24 V field rail and return to **DI GND** pins (per-channel). Do **not** back-power logic from sensor rails.
- Size PSU for base electronics + LEDs + **relay coils** (up to 3 simultaneously) + sensor rails; add **≥ 30 % headroom** (see [§3.2](#32-electrical-ratings)).
- Correct polarity; keep logic **GND** and DI field ground **separate**; upstream **fusing/breaker** required.

### 5.4 Step-by-step

**Phase 1 — Wire**

- **24 VDC** → **V+ / 0V** (top POWER terminals). Regulated SELV; keep pairs twisted.
- **Digital inputs** → **INx / GNDx** (isolated; do not bridge logic GND ↔ field GND).
- **Relay outputs** → **COM / NO / NC**. Interposing contactors for motors/pumps; RC/MOV on inductive loads.
- **RS-485** → **A / B / COM (GND)**. Shielded twisted pair; daisy-chain; 120 Ω at both ends.

![24Vdc wiring](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/DIO-430-R1/Images/DIO_24Vdc.png)

![Digital inputs](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/DIO-430-R1/Images/DIO_DIInputs.png)

![Relay wiring example](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/DIO-430-R1/Images/DIO_RelayConnection.png)

![RS-485 connection](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/DIO-430-R1/Images/DIO_RS485Connection.png)

> This module does **not** export auxiliary 12 V/5 V sensor rails. Power sensors from your panel 24 V rail; return to matching **DI GNDx** terminals.

**Phase 2 — Configure (WebConfig)**

1. Connect **USB-C**; open [WebConfig](https://www.home-master.eu/configtool-dio-430-r1) in a Chromium-based browser → **Connect**.
2. Set **Modbus address** and **baud rate** (factory defaults in [§3.4](#34-communication-defaults-canonical)).
3. Map inputs, relays, buttons, LEDs per [§6 WebConfig Reference](#6-webconfig-reference). Changes auto-save to flash.
4. Disconnect USB-C; hand control to the RS-485 master.

**Phase 3 — Integrate (controller / Home Assistant)**

Add the ESPHome package on your MiniPLC/MicroPLC (or poll/write Modbus from your PLC/SCADA). Match **address** and **baud** to WebConfig settings.

```yaml
packages:
  dio1:
    url: https://github.com/isystemsautomation/homemaster-dev
    ref: main
    files:
      - path: DIO-430-R1/Firmware/v0.2.0/default_dio_430_r1_plc/default_dio_430_r1_plc.yaml
        vars:
          dio_prefix: "DIO#1"
          dio_id: dio_1
          dio_address: 4   # must match WebConfig Modbus address
```

Full UART/modbus setup and entity list: [§8 ESPHome Integration](#8-esphome--home-assistant-integration). Modbus register addresses: [§7 Modbus Register Map](#7-modbus-register-map).

### 5.5 Verify

| Area | What to check |
|------|---------------|
| LEDs | **PWR** = ON; **TX/RX** blink during RS-485 traffic |
| Inputs | Wall switch/sensor changes **INx** state in WebConfig / Modbus IREG 0 |
| Relays | Coil writes toggle **R1–R3**; loads switch correctly |
| Address/Baud | Controller reads module without errors |
| Isolation | No unintended bond between logic GND and DI field GNDx |

---

## 6. WebConfig Reference

*(Canonical description of all configuration fields.)*

Open **https://www.home-master.eu/configtool-dio-430-r1** in any Chromium-based browser (Chrome, Edge, Opera, Brave, Vivaldi; Chrome/Edge 89+, Opera 76+), connect via **USB-C**, and click **Connect**. Changes apply immediately and are saved to flash (no Save button).

> Firefox: experimental only (Nightly with the Web Serial flag enabled). Safari and stable Firefox are not supported.

### Status & Tools

![DIO-430-R1 WebConfig — Module status (Model, FW, Modbus ID, Baud) and Tools — Identify / Factory reset / Reboot](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/main/DIO-430-R1/Images/webconfig1.png)

Status pills (read-only, not settings): **Connection** (USB), **Bus** (RS-485), **Model**, **FW**. A banner warns if model/firmware mismatches this configurator.

| Button | What it does |
|--------|--------------|
| Identify (~5 s) | Blinks user LEDs to locate the module. |
| Factory reset | Restores all settings to defaults. |
| Reboot | Restarts the module. |

### Device Setup

![DIO-430-R1 WebConfig — Serial connection & Modbus addressing (address, baud) with serial log](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/main/DIO-430-R1/Images/webconfig2.png)

| Field | Values | Meaning |
|-------|--------|---------|
| Modbus Address | 1–247 (default 3) | Modbus RTU slave address; must be unique on the bus. |
| Baud Rate | 9600 / 19200 / 38400 / 57600 / 115200 (default 19200) | RS-485 speed **8N1**; must match the controller. |

### Digital Inputs

![DIO-430-R1 WebConfig — Per-input setup: Enabled / Inverted / Child-lock, Type (Maintained/Momentary), Short-Long actions & target](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/main/DIO-430-R1/Images/webconfig3.png)

| Field | Values | Meaning |
|-------|--------|---------|
| Enabled | on / off | Whether this input is processed. |
| Inverted | on / off | Invert the read level (NC contacts). |
| Child lock | on / off | Suspend local switching; still reports to Home Assistant. |
| Type | Maintained / Momentary | Wall switch vs push-button. |
| Maintained mode | Toggle / Follow | **Toggle**: flip relay on each change. **Follow**: relay mirrors switch. |
| Target *(Maintained)* | All / R1 / R2 / R3 / None | Controlled relay(s). |
| Short → action *(Momentary)* | None / Toggle / On / Off / All off | Short-press action. |
| Short → target | All / R1 / R2 / R3 / None | Short-press target relay(s). |
| Long → action *(Momentary)* | None / Toggle / On / Off / All off | Long-press action. |
| Long → target | All / R1 / R2 / R3 / None | Long-press target relay(s). |

Defaults: IN1–IN3 = Maintained / Toggle → R1/R2/R3; IN4 = Momentary, Short = All off.

### Relays & Interlock

![DIO-430-R1 WebConfig — Relay enable / invert / power-on / auto-off, and relay interlock (Relay A/B, pause)](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/main/DIO-430-R1/Images/webconfig4.png)

**Relays (Relay 1–3)**

| Field | Values | Meaning |
|-------|--------|---------|
| Enabled | on / off | Relay output active. |
| Inverted | on / off | Invert drive polarity. |
| Power-on | OFF / ON / Restore last | State after power-up. |
| Auto-off, s | 0–65535 (0 = off) | Staircase timer; 0 disables. |

Defaults: enabled, not inverted, OFF at power-on, auto-off 0.

**Interlock**

| Field | Values | Meaning |
|-------|--------|---------|
| Enabled | on / off | Pair two relays (motor up/down). |
| Relay A / B | R1 / R2 / R3 | Interlocked pair. |
| Pause, ms | integer (default 500) | Dead-time when reversing. |

**Timing**

| Field | Values | Meaning |
|-------|--------|---------|
| Long-press, ms | 50–5000 (default 700) | Long-press threshold. |
| Multi-click gap, ms | 50–2000 (default 300) | Double/triple click window. |
| Debounce, ms | 1–500 (default 30) | Input debounce. |
| Link timeout, ms | 500–60000 (default 5000) | RS-485 link-loss timeout. |

### Buttons & User LEDs

![DIO-430-R1 WebConfig — Button short/long actions and user-LED source / mode](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/main/DIO-430-R1/Images/webconfig5.png)

**Buttons (Button 1–2; GPIO2 / GPIO3)**

| Field | Values | Meaning |
|-------|--------|---------|
| Short/Long → action | None / Toggle / On / Off / All off | Press action. |
| Short/Long → target | All / R1 / R2 / R3 / None | Target relay(s). |

Defaults: Button 1 → Short = Toggle R1, Long = All off; Button 2 → Short = Toggle R2, Long = None. (Third button: no software function — boot/reset combo only.)

**User LEDs (LED 1–3)**

| Field | Values | Meaning |
|-------|--------|---------|
| Source | Off / HA / Link / Local / Child lock / Safe mode / Identify / Relay | LED meaning (8 firmware sources). |
| Mode | Steady / Blink | Display mode. |
| Inverted | on / off | Invert LED level. |
| Arg | integer | Relay # or DI index for Child lock / Relay sources. |

Defaults: LED1 = Link; LED2 = Off; LED3 = HA.

### Action / Target reference

| Action | Meaning |
|--------|---------|
| None | Report only (no local relay action). |
| Toggle / On / Off / All off | Flip, energize, de-energize, or all-off. |

| Target | Meaning |
|--------|---------|
| All / R1 / R2 / R3 / None | Relay scope. |

---

## 7. Modbus Register Map

*(Canonical source for all register addresses.)*

**Role:** RTU **slave** (controller is master). **Defaults:** see [§3.4 Communication defaults](#34-communication-defaults-canonical).

> v0.2.0 map uses **zero-based offsets** matching `default_dio_430_r1_plc.yaml`. Poll **Input Registers (FC04)** for live state; use **Coils (FC05)** for commands.

### 7.1 Address map (overview)

| Type | Offsets | Purpose |
|------|---------|---------|
| **Input Registers** (FC04) | `0…29` | State masks (DI/relay/button/LED), status, event counters |
| **Coils** (FC01/05) | `0…14` | Relays, device commands, LED HA override, DI child-lock |
| **Holding Registers** (FC03) | `0…46` | Identity, comms, DI/relay/button/LED config, timing |

> **No discrete-input bank (FC02):** firmware does not call `addIsts()`. Read DI/relay/button/LED states from **Input Register** masks below.

### 7.2 Input Registers (FC04) — state & counters

| Reg | Name | Encoding | Description |
|----:|------|----------|-------------|
| 0 | **DI_MASK** | bitmask | bit0..3 → IN1..IN4 (after invert) |
| 1 | **RLY_MASK** | bitmask | bit0..2 → R1..R3 |
| 2 | **BTN_MASK** | bitmask | bit0..1 → Button1..2 |
| 3 | **LED_MASK** | bitmask | bit0..2 → LED1..3 |
| 4 | **STATUS_FLAGS** | bitmask | bit1 = link OK, bit3 = config dirty |
| 5 | **LOCK_MASK** | bitmask | bit0..3 → child-lock DI1..4 |
| 6–29 | **Event counters** | u16 | Index = `source×4 + type` (base 6); sources: DI1..4 = 0..3, Btn1..2 = 4..5; types: 0=single, 1=double, 2=triple, 3=long |

### 7.3 Coils (FC01/05) — commands

| Coil | Name | Description |
|-----:|------|-------------|
| 0 | **R1** | Relay 1 ON/OFF (maintained) |
| 1 | **R2** | Relay 2 ON/OFF (maintained) |
| 2 | **R3** | Relay 3 ON/OFF (maintained) |
| 3 | **ALL_OFF** | Turn all relays off (pulse) |
| 4 | *(reserved)* | Local-logic flag (internal) |
| 5 | **IDENTIFY** | Front-panel identify blink (pulse) |
| 6 | **SAVE_CFG** | Persist settings to flash (pulse) |
| 7 | **REBOOT** | Soft reset (pulse) |
| 8 | **LED1 HA** | Home Assistant LED1 override |
| 9 | **LED2 HA** | Home Assistant LED2 override |
| 10 | **LED3 HA** | Home Assistant LED3 override |
| 11–14 | **DI1–4 lock** | Child-lock per digital input |

### 7.4 Holding Registers (FC03) — configuration

Configuration is normally done via **WebConfig** ([§6](#6-webconfig-reference)). Holding registers mirror persisted settings (offsets 0–46):

| Reg | Name | R/W | Encoding | Notes |
|----:|------|:---:|----------|-------|
| 0 | **MODEL_ID** | R | u16 | **5** (DIO-430-R1; not 0x0430) |
| 1 | **FW_VERSION** | R | u16 | Packed `(major<<8)\|minor` (not a build date) |
| 2 | **MAP_VERSION** | R | u16 | Modbus map version |
| 3 | **MB_ADDR** | R/W | u16 | Modbus address **1–247** (default 3) |
| 4 | **MB_BAUD** | R/W | enum | 0=9600, 1=19200, 2=38400, 3=57600, 4=115200 |
| 8 | **DI_EN_MASK** | R/W | bitmask | bit0..3 → DI1..DI4 enable |
| 9 | **DI_INV_MASK** | R/W | bitmask | bit0..3 → DI1..DI4 invert |
| 10 | **DI_TYPE_MASK** | R/W | bitmask | bit0..3 → 0=Maintained, 1=Momentary |
| 11 | **DI_LOCK_MASK** | R/W | bitmask | bit0..3 → child-lock per DI |
| 12–15 | **DI1–4 FOLLOW** | R/W | u16 | Follow target (Maintained/Follow mode) |
| 16–19 | **DI1–4 SHORT** | R/W | packed | Short-press action+target (Momentary) |
| 20–23 | **DI1–4 LONG** | R/W | packed | Long-press action+target (Momentary) |
| 24 | **RLY_EN_MASK** | R/W | bitmask | bit0..2 → R1..R3 enable |
| 25 | **RLY_INV_MASK** | R/W | bitmask | bit0..2 → R1..R3 invert |
| 26 | **RLY_POWERON** | R/W | packed | 2 bits per relay: 0=Off, 1=On, 2=Restore |
| 27–29 | **RLY1–3 AUTO-OFF** | R/W | u16 | Auto-off timer (s) per relay; 0=disabled |
| 30 / 31 | **BTN1 SHORT / LONG** | R/W | packed | Button 1 short/long action+target |
| 32 / 33 | **BTN2 SHORT / LONG** | R/W | packed | Button 2 short/long action+target |
| 34–36 | **LED1–3 config** | R/W | packed | source / mode / inverted / arg per LED |
| 40 | **INTERLOCK** | R/W | packed | Interlock enable + relay pair |
| 41 | **INTERLOCK_PAUSE** | R/W | u16 | Interlock dead-time (ms); default 500 |
| 42 | **DI_MAINT_MODE_MASK** | R/W | bitmask | Maintained mode per DI: 0=Toggle, 1=Follow |
| 43 | **LONGPRESS_MS** | R/W | u16 | Default 700 |
| 44 | **MULTICLICK_MS** | R/W | u16 | Default 300 |
| 45 | **DEBOUNCE_MS** | R/W | u16 | Default 30 |
| 46 | **LINKTIMEOUT_MS** | R/W | u16 | Default 5000 |

**Packed action+target byte:** upper 3 bits = action (0=None, 1=Toggle, 2=On, 3=Off, 4=All off); lower 3 bits = target (0=None, 1=R1, 2=R2, 3=R3, 4=All).

### 7.5 Register use examples

**A) Toggle Relay 2 from a PLC** — write `1` to **Coil 1** (ON), then `0` (OFF).

**B) Read DI3 state** — read **Input Register 0** (FC04), test bit 2 of **DI_MASK**.

**C) Map IN3 as Maintained Toggle → Relay 1** — set bit2 in **HREG 8**; clear bit2 in **HREG 10** and **HREG 42**; write follow target `1` to **HREG 14**; pulse **Coil 6 (SAVE_CFG)**.

**D) Map Button 1 short press → Toggle Relay 2** — write packed Toggle+R2 to **HREG 30**; pulse **Coil 6**.

**E) Change Modbus address / baud** — **HREG 3 (MB_ADDR)** and **HREG 4 (MB_BAUD)**; pulse **Coil 6**; optional **Coil 7 (REBOOT)**.

**F) Persist and reboot** — pulse **Coil 6 (SAVE_CFG)** then **Coil 7 (REBOOT)**.

### 7.6 Polling recommendations

- **Input registers 0–5:** 5–10 Hz (100–200 ms) for DI/relay/LED masks
- **Event counters 6–29:** 1–2 s (change slowly)
- **Coils:** write on change only; relays 0–2 are maintained
- **Holding:** configure at commissioning; avoid frequent writes
- **Edge logic:** use **Maintained/Toggle** for latching inputs; **Momentary/On/Off** when the PLC supervises timers

---

## 8. ESPHome / Home Assistant Integration

> **Module role:** Modbus RTU **slave** on RS-485. Comms defaults: [§3.4](#34-communication-defaults-canonical).

### 8.1 Minimal YAML (controller side)

Use this on the **MiniPLC/MicroPLC** (ESPHome). It enables the RS-485 bus and imports a ready-made DIO package.

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
  dio1:
    url: https://github.com/isystemsautomation/homemaster-dev
    ref: main
    files:
      - path: DIO-430-R1/Firmware/v0.2.0/default_dio_430_r1_plc/default_dio_430_r1_plc.yaml
        vars:
          dio_prefix: "DIO#1"
          dio_id: dio_1
          dio_address: 4       # Modbus address set in WebConfig
    refresh: 1d
```

> For **multiple** DIOs, duplicate the `dio1:` block (`dio2:`, `dio3:`…) with unique `dio_id`, `dio_prefix`, and `dio_address`.

### 8.2 Entities exposed (from the package)

- **Binary Sensors** (from **Input Registers**, FC04 bitmasks)
  - **DI1…DI4** (IREG 0)
  - **Btn1…Btn2** (IREG 2)
  - **LED1…LED3** (IREG 3)
  - **Link OK / Config dirty** (IREG 4)
  - **DI child-lock flags** (IREG 5)
- **Switches**
  - **Relay 1–3** (coils 0–2, maintained)
  - **LED1–3 HA override** (coils 8–10)
  - **DI1–4 child lock** (coils 11–14)
- **Sensors**
  - **Event counters** DI1..4 / Btn1..2 × single/double/triple/long (IREG 6–29)
- **Buttons (template)**
  - **All off**, **Identify** (pulse coils 3, 5)

> The package matches `default_dio_430_r1_plc.yaml`: **no discrete-input bank** — DI/relay states are read from **Input Register** masks.

### 8.3 Optional: direct (manual) entity mapping

```yaml
modbus_controller:
  - id: dio430_4
    address: 4
    modbus_id: modbus_bus
    update_interval: 200ms
    command_throttle: 100ms

binary_sensor:
  # DI1..DI4 from Input Register 0 (FC04 bitmasks)
  - platform: modbus_controller
    modbus_controller_id: dio430_4
    name: "DIO#1 DI1"
    register_type: read
    address: 0
    bitmask: 0x0001
  - platform: modbus_controller
    modbus_controller_id: dio430_4
    name: "DIO#1 DI2"
    register_type: read
    address: 0
    bitmask: 0x0002
  - platform: modbus_controller
    modbus_controller_id: dio430_4
    name: "DIO#1 DI3"
    register_type: read
    address: 0
    bitmask: 0x0004
  - platform: modbus_controller
    modbus_controller_id: dio430_4
    name: "DIO#1 DI4"
    register_type: read
    address: 0
    bitmask: 0x0008

switch:
  - platform: modbus_controller
    modbus_controller_id: dio430_4
    name: "DIO#1 Relay 1"
    register_type: coil
    address: 0
  - platform: modbus_controller
    modbus_controller_id: dio430_4
    name: "DIO#1 Relay 2"
    register_type: coil
    address: 1
  - platform: modbus_controller
    modbus_controller_id: dio430_4
    name: "DIO#1 Relay 3"
    register_type: coil
    address: 2

sensor:
  - platform: modbus_controller
    modbus_controller_id: dio430_4
    name: "DIO#1 LED Mask"
    register_type: read
    address: 3
    value_type: U_WORD
    accuracy_decimals: 0
  - platform: modbus_controller
    modbus_controller_id: dio430_4
    name: "DIO#1 Button Mask"
    register_type: read
    address: 2
    value_type: U_WORD
    accuracy_decimals: 0
```

### 8.4 Home Assistant tips

- **Dashboards:** lighting panel with Relay 1–3 + DI tiles; maintenance card with Identify / All off buttons.
- **Automations:** trigger relay switches from DI edges if logic runs in HA instead of module mapping.
- **Naming:** use `dio_prefix` for readable entities (`DIO#1 Relay 1`, `DIO#2 DI3`, etc.).

### 8.5 Troubleshooting (integration)

- **No response / timeouts:** A/B polarity, shared **COM/GND**, **120 Ω** termination at bus ends.
- **Wrong device:** `dio_address` must match WebConfig address.
- **Relays don't switch:** relay **Enabled** in [§6 WebConfig](#6-webconfig-reference).
- **DI not changing:** wiring to **INx/GNDx**; check Enable/Invert/Type and Short/Long actions.

### 8.6 Notes & versions

- Works with recent ESPHome releases (e.g., 2025.x).
- Keep `update_interval` modest (200–500 ms) unless faster DI polling is needed.
- For multiple devices, stagger `update_interval` / `command_throttle` to reduce collisions.

---

## 9. Programming & Build

### 9.1 Supported languages

- **Arduino**
- **C++** (PlatformIO)
- **MicroPython** (community builds for RP23xx-class MCUs)

### 9.2 Flashing (USB-C, hardware buttons)

> All reset/boot actions use the **front buttons**. UF2 image: [§11 Downloads](#11-downloads--resources).

**Combinations** (see [§4.3 Front panel](#43-front-panel--buttons--leds)):

- **Buttons 1 + 2 + 3** → **BOOT** mode (RPI-RP2 USB drive for UF2 drag-and-drop)
- **Buttons 1 + 3** → hardware **RESET**

**Steps**

1. Connect **USB-C** to a PC (disconnect RS-485 during flashing).
2. Enter **BOOT** mode. Board mounts as **RPI-RP2** (UF2) or serial port (IDE).
3. Flash UF2 from [§11](#11-downloads--resources) or upload via PlatformIO / Arduino IDE.
4. If needed, **Buttons 1 + 3** for hardware reset.

> No factory-reset function. Configuration remains intact across normal firmware updates.

### 9.3 Arduino / PlatformIO notes

**Board / toolchain**

- **Board:** Generic **RP2350** (or vendor core for **RP2350A**)
- **USB:** CDC enabled (serial logging)
- **FS:** LittleFS partition recommended (for settings)

**Required libraries (Library Manager names / versions)**

- `Arduino_JSON` (0.2.0)
- `Modbus-Arduino` (1.3.0) + `Modbus-Serial` (2.0.6) — `#include <ModbusSerial.h>` in sketch
- `Simple Web Serial` (1.0.0)
- **From core:** `LittleFS`, `Wire` (no separate install)

**Pin mapping:** see [§4.1 Diagrams & pinouts](#41-diagrams--pinouts-canonical) (`DIO_MCU_Pinouts.png`) and firmware `default_DIO_430_R1.ino`.

**Build tips**

- Use factory RS-485 defaults from [§3.4](#34-communication-defaults-canonical) during bring-up.
- After flashing, disconnect USB-C and return control to the RS-485 master.

### 9.4 Firmware updates

See [§9.2 Flashing](#92-flashing-usb-c-hardware-buttons) and [§11 Downloads](#11-downloads--resources). Configuration in flash/LittleFS is preserved across normal updates unless explicitly erased.

---

## 10. Maintenance & Troubleshooting

### 10.1 Status LEDs

- **PWR** — ON in normal operation
- **TX/RX** — blink on Modbus traffic
- **User LEDs (1–3)** — follow relay logic (Steady/Blink per [§6 WebConfig](#6-webconfig-reference))

### 10.2 Resets

- **Power cycle:** remove 24 V, wait 5 s, re-apply
- **Buttons 1 + 3** — hardware RESET

### 10.3 Common issues

| Symptom | Checks |
|---------|--------|
| No Modbus comms | A/B polarity, **COM/GND** reference, 120 Ω termination, address/baud match ([§3.4](#34-communication-defaults-canonical)), only two end terminators |
| Relays don't actuate | Relay **Enabled** in WebConfig; verify coil writes to 0–2; check invert setting |
| DI not changing | Wire to **INx/GNDx**; check Enable/Invert/Type and Short/Long actions in [§6](#6-webconfig-reference) |
| USB won't connect | Chromium-based browser with Web Serial (not Safari/stable Firefox); close other serial apps, check cable/port permissions |
| Config not saved | Allow idle for auto-save; verify LittleFS space |

---

## 11. Downloads & Resources

### Version history

| Version | Config path (`path:`) | Date | Status |
|---------|------------------------|------|--------|
| **v0.2.0** | `DIO-430-R1/Firmware/v0.2.0/default_dio_430_r1_plc/default_dio_430_r1_plc.yaml` | 2026-06 | **Current — shipped on new modules** |
| v0.1.0 | `DIO-430-R1/Firmware/v0.1.0/default_dio_430_r1_plc/default_dio_430_r1_plc.yaml` | 2026-06 | Legacy (superseded by v0.2.0) |

> **Reproducible firmware build (v0.2.0):** [Build environment (reproducible)](../../README.md#build-environment-reproducible) · [`sketch.yaml`](Firmware/v0.2.0/default_DIO_430_R1/sketch.yaml)

### Files

- **Firmware v0.2.0 (pre-built UF2 — drag-and-drop upgrade)** *(canonical UF2 link)*
  - [`default_DIO_430_R1.ino.uf2`](https://github.com/isystemsautomation/homemaster-dev/raw/refs/heads/main/DIO-430-R1/Firmware/v0.2.0/default_DIO_430_R1/build/rp2040.rp2040.generic_rp2350/default_DIO_430_R1.ino.uf2)
- **Firmware publishing (maintainers)**
  - [`DIO-430-R1/Firmware/README.md`](https://github.com/isystemsautomation/homemaster-dev/blob/main/DIO-430-R1/Firmware/README.md)
- **Firmware source (Arduino, v0.2.0)**
  - [`DIO-430-R1/Firmware/v0.2.0/default_DIO_430_R1/`](https://github.com/isystemsautomation/homemaster-dev/tree/main/DIO-430-R1/Firmware/v0.2.0/default_DIO_430_R1)
- **YAML configs (ESPHome, v0.2.0)**
  - [`DIO-430-R1/Firmware/v0.2.0/default_dio_430_r1_plc/`](https://github.com/isystemsautomation/homemaster-dev/tree/main/DIO-430-R1/Firmware/v0.2.0/default_dio_430_r1_plc)
- **WebConfig tool (HTML/JS, v0.2.0)**
  - [`DIO-430-R1/Firmware/v0.2.0/ConfigToolPage.html`](https://github.com/isystemsautomation/homemaster-dev/blob/main/DIO-430-R1/Firmware/v0.2.0/ConfigToolPage.html)
- **Firmware source (Arduino, v0.1.0 — legacy)**
  - [`DIO-430-R1/Firmware/v0.1.0/default_DIO_430_R1/`](https://github.com/isystemsautomation/homemaster-dev/tree/main/DIO-430-R1/Firmware/v0.1.0/default_DIO_430_R1)
- **YAML configs (ESPHome, v0.1.0 — legacy)**
  - [`DIO-430-R1/Firmware/v0.1.0/default_dio_430_r1_plc/`](https://github.com/isystemsautomation/homemaster-dev/tree/main/DIO-430-R1/Firmware/v0.1.0/default_dio_430_r1_plc)
- **WebConfig tool (HTML/JS, v0.1.0 — legacy)**
  - [`DIO-430-R1/Firmware/v0.1.0/ConfigToolPage.html`](https://github.com/isystemsautomation/homemaster-dev/blob/main/DIO-430-R1/Firmware/v0.1.0/ConfigToolPage.html)
- **Schematics (PDF)**
  - Field Board: [`Schematics/DIO-430-R1-FieldBoard.pdf`](https://github.com/isystemsautomation/homemaster-dev/blob/main/DIO-430-R1/Schematics/DIO-430-R1-FieldBoard.pdf)
  - MCU Board: [`Schematics/DIO-430-R1-MCUBoard.pdf`](https://github.com/isystemsautomation/homemaster-dev/blob/main/DIO-430-R1/Schematics/DIO-430-R1-MCUBoard.pdf)
- **Images & diagrams**
  - [`DIO-430-R1/Images/`](https://github.com/isystemsautomation/homemaster-dev/tree/main/DIO-430-R1/Images)
- **Datasheets**
  - Refer to `Schematics/` BOM notes for part numbers (e.g., ISO1212, MAX485, HF115F).

---

## Open Source & Licensing

This project uses a hybrid licensing model.

**Hardware** — schematics, PCB layouts, BOMs: **CERN-OHL-W v2**

**Firmware & ESPHome integration** — firmware, ESPHome configs, software: **MIT License**

This ensures full compatibility with ESPHome and Home Assistant while protecting hardware designs. See LICENSE files in each directory for full terms.

---

## 12. Compliance & Certifications

The DIO-430-R1 module is CE marked. **ISYSTEMS AUTOMATION S.R.L.** (HomeMaster® brand) maintains the technical documentation and a signed EU Declaration of Conformity (DoC).

### Applicable EU directives

- **EMC Directive 2014/30/EU** — EN 55032:2015 + AC:2016-07 + A11:2020 + A1:2020 (Class B emissions), EN 55035:2017 + A11:2020 (immunity); tested by Idvorsky Laboratories Ltd., Belgrade, Serbia (Job #1648, 20 April 2026)
- **Low Voltage Directive 2014/35/EU** — EN 62368-1:2020 + A11:2020; in-house dielectric and isolation test by ISYSTEMS AUTOMATION S.R.L. internal compliance laboratory
- **RoHS Directive 2011/65/EU** — EN IEC 63000 technical documentation

| Standard / Directive | Description |
|----------------------|-------------|
| Ingress Rating | IP20 (panel-mount only) |
| Altitude | ≤ 2000 m |
| Environmental | RoHS / REACH compliant components |

### Compliance documents

| Document | File |
|----------|------|
| EU Declaration of Conformity (DoC) | [DoC-DIO-430-R1-V1.0.pdf](./Manuals/DoC-DIO-430-R1-V1.0.pdf) |
| Datasheet | [DIO-430-R1_Datasheet.pdf](./Manuals/DIO-430-R1_Datasheet.pdf) |

### Trademark

**HomeMaster®** is a registered European Union trademark of ISYSTEMS AUTOMATION S.R.L., EUTM No. 019082911, registered with EUIPO on 15 January 2025.

---

## 13. Support

- **Official Support Portal:** https://www.home-master.eu/support
- **WebConfig Tool:** https://www.home-master.eu/configtool-dio-430-r1
- **YouTube:** https://youtube.com/@HomeMaster
- **Hackster:** https://hackster.io/homemaster
- **Reddit:** https://reddit.com/r/HomeMaster
- **Instagram:** https://instagram.com/home_master.eu

**Manufacturer:** ISYSTEMS AUTOMATION S.R.L. (HomeMaster® brand)  
**Registered office:** Str. Domnisori, Nr. 81, Bl. 62, Scara A, Etaj 3, Ap. 12, 100284 Ploiesti, Jud. Prahova, Romania  
**Office / Contact:** Diligentei 18, Ploiesti, Romania  
**CUI / VAT:** RO 21537032 · **EUID:** ROONRC.J2007000919293  
**Telephone:** +40 747 757 798  
**Website:** [https://www.home-master.eu](https://www.home-master.eu)
