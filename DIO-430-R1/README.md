![Modbus](https://img.shields.io/badge/Protocol-Modbus%20RTU-brightgreen)
![License](https://img.shields.io/badge/License-GPLv3%20%2F%20CERN--OHL--W-blue)

## 🚀 Quick Start (current version)

**Firmware shipped on new modules: `v0.2.0`**

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
          dio_address: 4
```

## 📦 Version History

| Version | Config path (`path:`) | Date | Status |
|--------|------------------------|------|--------|
| **v0.2.0** | `DIO-430-R1/Firmware/v0.2.0/default_dio_430_r1_plc/default_dio_430_r1_plc.yaml` | 2026-06 | **Current — shipped on new modules** |
| v0.1.0 | `DIO-430-R1/Firmware/v0.1.0/default_dio_430_r1_plc/default_dio_430_r1_plc.yaml` | 2026-06 | Legacy (superseded by v0.2.0) |

> **Reproducible firmware build (v0.2.0):** [Build environment (reproducible)](../../README.md#build-environment-reproducible) · [`sketch.yaml`](Firmware/v0.2.0/default_DIO_430_R1/sketch.yaml)

# DIO-430-R1 — Module for Smart I/O Control

**HOMEMASTER – Modular control. Custom logic.**

![MODULE photo](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/DIO-430-R1/Images/photo1.png)

---

## How it works & how to set it up

### What it is
The DIO-430-R1 is a digital input/output module for a DIN rail (the box that clips into an electrical cabinet). In simple terms it can **switch power to things** and **react to switches and sensors**.

Inside it has:
- **3 relays** — think of them as 3 remote-controlled switches. Each one can turn a load (a light, a fan, a pump, a valve) on or off.
- **4 inputs** — where you connect wall switches, push-buttons, or dry-contact sensors (door/window contacts, motion relays, etc.).
- **3 buttons (2 user-configurable, shown as Button 1 / Button 2; the third is used only for the firmware-update combo)** — handy for testing and simple local control.
- **Status lights** — for each relay and each input, plus 3 extra lights you can assign a meaning to.

It talks to a controller (MicroPLC / MiniPLC) over **RS-485** (two wires), and you configure it over **USB-C** from a web browser.

### The big idea: it works from everywhere, all the time
The relays can be switched from **any** source — the wall switch wired to an input, the front buttons, or Home Assistant over the network — **all at once**. The last command wins. Why it matters:
- If the network, controller, or Home Assistant is **off or rebooting**, your wall switches **still work**.
- When everything is online, you also get full control from your phone, dashboards, voice, and automations.

There is **no "local vs remote" switch to flip** — local and remote control always coexist.

### What each input can do
For every input you choose, in the browser, what it should do:
- **Wall switch (toggle):** every time the switch changes position, it flips its relay on/off. Default for a normal light switch. (Because you can also control the relay from your phone, the physical switch position may not always match the light — normal for any system mixing a wall switch with app control.)
- **Wall switch (follow) — optional:** the relay exactly mirrors the switch position (up = on, down = off). Use only when the wall switch should be the *only* control for that load.
- **Push-button (momentary):** a button that springs back. A **short press** does one thing, a **long press** another (e.g. short = toggle light, long = all off). Each press is also **reported to Home Assistant** (single / double / long), so HA can run scenes — without slowing the local light.
- **Sensor / "tell Home Assistant only":** the input just reports its state and switches no relay itself. For door contacts, motion, etc.

For switch and button actions you pick the **target**: relay 1, 2, 3, **all relays**, or **none**.

### Handy extras (all optional, set in the browser)
- **All Off** — one input or button turns every relay off at once (great for a door "everything off" button). Also works from Home Assistant.
- **Child lock** — temporarily disable a single input (cleaning, or so kids can't flip a switch). It still reports to Home Assistant; only local switching pauses.
- **Auto-off timer (staircase timer)** — a relay turns itself off after a set time (stairwell light, bathroom fan). Switching on again restarts the timer.
- **Shutter / roller mode (interlock)** — pair two relays for an up/down motor so they can **never be on together**, with a short pause when reversing. Protects blinds/gate motors.
- **Status lights** — the 3 extra lights can show: connection to the controller, child-lock active, a custom Home-Assistant indicator (alarm/notification), or "Identify" (blink to find this exact module in a full cabinet).

### How to set it up (step by step)
1. **Wire it.** Power the module, connect switches/sensors to the inputs and loads to the relays, and run the RS-485 pair to your MicroPLC/MiniPLC controller.
2. **Open the configurator.** Plug **USB-C** from the module to your computer. In Chrome or Edge, open the module's **WebConfig** page and click **Connect**. No app, no login.
3. **Set the basics.** Choose the **Modbus address** (default **3**) and **baud rate** (default **19200**). The address must be unique on the RS-485 line.
4. **Tell each input what to do.** For every input pick its type (toggle switch / push-button / sensor) and which relay it controls. Set any extras (All Off, child lock, auto-off time, shutter pair).
5. **You're done — it saves automatically.** Changes are written to the module's memory a moment after you make them; there is no "Save" button.
6. **Connect to Home Assistant.** On your MicroPLC/MiniPLC (running ESPHome), add the DIO package and set the same Modbus address. Home Assistant then shows the relays as switches, inputs as sensors, and button presses as events for automations.

### Configuration & firmware

- **WebConfig field reference (canonical):** [§5.5 Software & UI Configuration](#55-software--ui-configuration)
- **Firmware UF2 upgrade:** [§8.2 Flashing](#82-flashing-usbc-hardware-buttons-only)

---

# 1. Introduction

The **DIO-430-R1** is a configurable smart digital I/O module designed for **digital input monitoring and relay-based output control** in **building automation, lighting, HVAC, alarms, and general control systems**.  
It offers **4 opto-isolated digital inputs**, **3 high-current SPDT relays**, **3 buttons (2 user-configurable, shown as Button 1 / Button 2; the third is used only for the firmware-update combo)**, and **3 configurable user LEDs**. All I/O channels are individually configurable, allowing flexible logic such as maintained/momentary input mapping, manual override, and status indication.

It connects via **RS-485 (Modbus RTU)** to a **MicroPLC, MiniPLC, or any compatible controller**, and can also integrate with **Home Assistant (ESPHome)** or **SCADA/PLC systems**.  
Configuration and diagnostics are performed through a driverless **Web Serial interface via USB-C**, using the browser-based **WebConfig Tool**. The module supports both **master-controlled** and **standalone local logic** modes.

| Subsystem         | Qty | Description |
|------------------|-----|-------------|
| Digital Inputs    | 4   | Opto-isolated, dry contact compatible, noise-protected |
| Relays            | 3   | SPDT (NO/NC), 16 A rated, dry contacts |
| LEDs              | 3   | Configurable: Steady or Blink modes, linked to relays |
| Buttons           | 3   | 2 user-configurable (Button 1 / Button 2 in WebConfig); third for firmware-update combo only |
| Modbus RTU        | Yes | RS-485 interface (Configurable: Addr 1–255, 9600–115200 baud) |
| USB-C             | Yes | WebConfig tool access via Web Serial (Chrome/Edge) |
| Power             | 24 V DC | Fused input, reverse-polarity and surge protected |
| MCU               | RP2350 | Dual-core, with QSPI flash, USB, UART, LittleFS |
| Protection        | TVS, PTC | ESD, surge, and short-circuit protection on I/O and power |

### System Role & Communication

The module communicates over the **RS-485 Modbus RTU bus**, using A/B differential lines and a shared COM/GND reference. It supports **poll-based communication**, where a master device reads input states and writes relay commands.  
All configuration — including input-to-relay mapping, LED modes, and button logic — is stored persistently in internal flash via **LittleFS** and can be changed live through **USB-C + WebConfig**.

**Factory default communication settings:**
- **Modbus Address:** `3`  
- **Baud Rate:** `19200`  
- **Parity:** `None`  
- **Stop Bits:** `1`

---

# 2. DIO-430-R1 — Technical Specification

## 2.1 Diagrams & Pinouts

System overview, board callouts, and pin mapping:

- ![System Block Diagram](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/DIO-430-R1/Images/DIO_SystemBlockDiagram.png)
- ![Control Board Diagram](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/DIO-430-R1/Images/ControlBoard_Diagram.png)
- ![Relay Board Diagram](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/DIO-430-R1/Images/RelayBoard_Diagram.png)
- ![RP2350A Pin Map](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/DIO-430-R1/Images/DIO_MCU_Pinouts.png)

---

## 2.2 I/O and Electrical Summary

| Interface | Qty | Description |
|------------|----:|-------------|
| **Digital Inputs** | 4 | Galvanically isolated (ISO1212 class). Supports dry contacts or 24 V signals. PTC + TVS per channel. |
| **Relay Outputs** | 3 | SPDT (NO/NC/COM), 16 A dry contacts. Use RC/MOV snubbers or interposing contactors for inductive/mains loads. |
| **User LEDs** | 3 | Configurable (Steady/Blink). Follow relay or logic status. |
| **Buttons** | 3 | Momentary. 2 user-configurable (Button 1 / Button 2); third for firmware-update combo only. |
| **RS-485 (Modbus RTU)** | 1 | A/B/COM terminals. Daisy-chain topology. 120 Ω termination at both ends. |
| **USB-C** | 1 | Web Serial setup, diagnostics, and firmware flashing (ESD-protected). |
| **Power Input** | 1 | 24 V DC SELV. Reverse-polarity + surge protected. |

**Electrical Ratings**

| Parameter | Min | Typ | Max | Unit | Notes |
|------------|----:|----:|----:|:----:|------|
| Supply Voltage | 22 | 24 | 28 | V DC | SELV/PELV input |
| Logic Consumption | – | 1.5 | 3.0 | W | Excludes relay loads |
| Digital Input Range | 0 | 24 | 30 | V DC | Isolated, noise-protected |
| Relay Contact Current | – | – | 16 | A | SPDT dry contacts |
| Relay Contact Voltage | – | – | 250 | V AC | or 30 V DC max |
| RS-485 Data Rate | – | 19.2 | 115.2 | kbps | Default 19200 8N1 |
| USB-C Voltage | 4.75 | 5.0 | 5.25 | V DC | Service only |
| Operating Temp. | 0 | – | 40 | °C | ≤ 95 % RH, non-condensing |

> **Power budgeting:** logic + LEDs + up to 3 relay coils + sensor loads → add ≥ 30 % PSU headroom.

---

## 2.3 Connectors & Terminal Map

| Block | Pins | Function | Notes |
|--------|------|-----------|-------|
| **POWER** | 0V, V+ | 24 V DC input | Reverse/surge protected |
| **RELAY 1-3** | NO, C, NC | SPDT contacts | Add RC/MOV for inductive loads |
| **DI 1-4** | INx, GNDx | Isolated digital inputs | 24 V field or dry contact |
| **RS-485** | B, A, COM | Modbus RTU bus | Terminate 120 Ω at ends |
| **USB-C** | D+, D−, VBUS, GND | Setup / Service port | Not for field powering |

---

## 2.4 Reliability & Protection

- Reverse-path diode + high-side MOSFET on 24 V input.  
- Local PTC + TVS protection on field interfaces.  
- Relay drivers opto-isolated; RC/MOV suppression recommended.  
- RS-485 with TVS, series protection, and fail-safe biasing.  
- USB-C ESD-protected; CC resistors per spec.  
- Non-volatile flash with **auto-save** after configuration changes.

---

## 2.5 Functional Overview

- **Modbus RTU slave** (factory defaults in [§1 Introduction](#1-introduction)).  
- **Inputs → Relays:** per-input Enable/Invert/**Type** (Maintained or Momentary). **Maintained** → mode Toggle/Follow + target relay. **Momentary** → Short/Long actions from {None, Toggle, On, Off, All off} + target (R1–R3 or All).  
- **Buttons:** Button 1 / Button 2 assignable to relay actions (toggle, on, off, all off).  
- **LEDs:** configurable Steady/Blink following relay status.  
- **Setup via WebConfig:** USB-C → Chrome/Edge; set comms and I/O mapping.  
- **Persistent config:** stored in LittleFS and restored on boot.

---

## 2.6 Mechanical & Environmental

| Property | Specification |
|-----------|---------------|
| Mounting | DIN-rail EN 50022 (35 mm) |
| Enclosure | PC/ABS V-0, panel mount |
| Dimensions | 70 × 90.6 × 67.3 mm (W × H × D) |
| Terminals | Pluggable 5.08 mm, 26–12 AWG (≤ 2.5 mm²), 0.5–0.6 Nm |
| Ingress Protection | IP20 (panel interior) |
| Operating Temp | 0–40 °C, ≤ 95 % RH (non-condensing) |

![DIO-430-R1 Dimensions](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/DIO-430-R1/Images/DIODimensions.png)

---

## 2.7 Standards & Compliance

| Standard / Directive | Description |
|----------------------|-------------|
| Ingress Rating | IP20 (panel-mount only) |
| Altitude | ≤ 2000 m |
| Environmental | RoHS / REACH compliant components |

---

# 3. Use Cases

The **DIO‑430‑R1** supports both **lighting** and **motor/pump control** — making it ideal for mixed automation tasks in smart homes, greenhouses, HVAC, and industrial setups.  
Below are 3 versatile examples combining both types of loads.

---

## 3.1 Staircase Light with Motion Sensor + Circulation Pump

Automatically turns ON a staircase light and a circulation pump when motion is detected.

**Setup Instructions:**
- Set **IN1** to **Type = Momentary**, Short = **On**, Target = **Relay 1** (light).
- Set **IN2** to **Type = Momentary**, Short = **On**, Target = **Relay 2** (pump).
- Enable **Relay 1** for the staircase lighting.
- Enable **Relay 2** for the circulation pump.
- Set **LED 1** = `Blink`, source = `Relay 1`.
- Set **LED 2** = `Steady`, source = `Relay 2`.

---

## 3.2 Manual Light + Fan Override (Wall Panel)

Wall-mounted buttons allow users to toggle lights and exhaust fans independently.

**Setup Instructions:**
- Assign **Button 1** → `Relay 1 override (toggle)` → Room Light  
- Assign **Button 2** → `Relay 2 override (toggle)` → Ventilation Fan  
- Enable both **Relay 1** and **Relay 2**.  
- Set **LED 1** and **LED 2** to `Steady`, following respective relays.  
- Optionally use Modbus **coils 0–2** for remote relay control.

---

## 3.3 Greenhouse Light + Irrigation Pump Automation

Lights and irrigation are controlled via digital inputs or remotely from a PLC.

**Setup Instructions:**
- **IN3** → **Type = Maintained**, mode **Toggle**, Target = **Relay 1** → Grow Light  
- **IN4** → **Type = Momentary**, Short = **On**, Target = **Relay 2** → Irrigation Pump  
- Enable **Relay 1** and **Relay 2**.  
- Assign **Button 2** to `Relay 2 override (toggle)` for manual watering.  
- Set **LED 1** = `Steady` (light status), **LED 2** = `Blink` (pump running).

---

# 4. Safety Information

These guidelines apply to the DIO-430-R1 I/O module. Ignoring them may result in equipment damage, system failure, or personal injury.

> ⚠️ **SELV/PELV Domains Only**
>
> - The **DIO-430-R1** operates entirely within **SELV/PELV** low-voltage domains (e.g., **24 V DC**, **RS-485**, **USB 5 V**).  
> - **Do not** connect mains voltage to **any** terminal. Use interposing contactors/PSUs for mains loads.  
> - **Respect isolation boundaries:** never bridge logic **GND** with isolated field grounds (e.g., **GND_ISO / FGND**).  
> - Connect sensor returns only to the **isolated field ground**; connect RS-485 **COM/GND** only within the same SELV domain.

---

## 4.1 General Requirements

| Requirement          | Detail |
|---------------------|--------|
| Qualified Personnel | Installation and servicing must be done by qualified personnel familiar with 24 V control systems and RS-485. |
| Power Isolation     | Disconnect the **24 V DC** input before wiring. Lockout/tagout where applicable. |
| Environmental Limits| Mount in a clean, sealed enclosure; avoid condensation, conductive dust, or vibration. |
| Grounding           | Bond the panel to PE. Keep RS-485 COM/GND shared with the controller side. |
| Voltage Compliance  | **SELV only** on all terminals. Follow relay contact ratings on the product label/datasheet. Use upstream fusing and surge protection. |

---

## 4.2 Installation Practices

| Task              | Guidance |
|-------------------|----------|
| ESD Protection    | Handle by the enclosure/edge only. Use an antistatic wrist strap when the board is exposed. |
| DIN Rail Mounting | Mount securely on **35 mm DIN** rail inside an IP-rated cabinet. Leave cable slack for strain relief. |
| Wiring            | Use correct wire gauge and torque terminal screws. Separate **power**, **DI**, **relay**, and **RS-485** harnesses. |
| Isolation Domains | Respect isolation: **do not bridge** logic **GND** to isolated field grounds (e.g., **GND_ISO/FGND**). Keep analog/sensor returns on the isolated side. |
| Commissioning     | Before power-up, verify polarity, relay NO/NC routing, RS-485 **A/B** orientation and termination. |

---

## 4.3 I/O & Interface Warnings

### 🔌 Power

| Area         | Warning |
|--------------|---------|
| 24 V DC Input| Use a clean, fused SELV supply. Reverse-polarity protection exists but may disable the module when triggered. |
| Sensor Rail  | Power sensors from a SELV rail. Observe polarity. Fuse external branches as required. |
| Surge/Noise  | In noisy panels, add upstream surge/EMI suppression and keep high-current wiring away from control wiring. |

### ⏽ Inputs (Digital)

| Area        | Warning |
|-------------|---------|
| Type        | **Dry contact / 24 V signaling only**, per your standard. Do not inject mains or undefined levels. |
| Isolation   | Inputs are isolated from logic. Keep sensor returns on the **field/isolated** domain; do not bond to logic GND. |
| Debounce    | Firmware provides debounce, but route away from contactors/VFDs and use shielded/twisted pairs for long runs. |
| Polarity    | Configure invert/action in WebConfig; verify state transitions after wiring. |

### ⚙️ Relays (Outputs)

| Area           | Warning |
|----------------|---------|
| Contact Type   | **SPDT (NO/NC/COM)** dry contacts. Follow the contact rating on the device label/datasheet. |
| Inductive Loads| For motors/solenoids/contactors, add an **RC snubber or MOV** at the load. Consider interposing relays/ contactors for higher power. |
| Separation     | Keep relay load wiring physically separate from signal wiring. De-energize before servicing. |
| Verification   | After wiring, verify NO/NC behavior and load polarity before enabling automation. |

### 🖧 RS-485 (Modbus RTU)

| Area          | Warning |
|---------------|---------|
| Topology      | Use twisted pair; **daisy-chain** (no stubs). Terminate with **120 Ω** at both physical ends. |
| Polarity      | Maintain **A/B** polarity consistently. Share **COM/GND** reference between nodes (same SELV domain). |
| EMC           | Route away from VFDs, contactors, and mains bundles. Use shielded cable in high-EMI environments. |
| Protection    | Port includes protection, but good wiring practice is still required to avoid transients. |

### 🔌 USB-C (Front / Setup)

| Area     | Warning |
|----------|---------|
| Purpose  | **Setup & maintenance only** (WebConfig / firmware). Not intended for powering field devices. |
| ESD/EMI  | Avoid hot-plugging in high-EMI areas. Use a grounded service laptop. Disconnect after commissioning. |

### 🔆 Front Panel (Buttons & LEDs)

| Area          | Warning |
|---------------|---------|
| Buttons & LEDs| Buttons can override relays; document operating procedures. Lock out overrides for safety-critical installs. |

### 🛡️ Shielding & EMC

| Area        | Recommendation |
|-------------|----------------|
| Cable Shields| Terminate shields at **one end** (typically the PLC/controller). Keep runs short and away from high-voltage/EMI sources. |

---

## ✅ Pre-Power Checklist

- [ ] All wiring is torqued, labeled, and strain-relieved  
- [ ] **No bridge** between logic **GND** and isolated **GND_ISO/FGND**  
- [ ] Panel PE is bonded; SELV supply negative and COM/GND are properly landed  
- [ ] RS-485 **A/B** polarity and **120 Ω** termination confirmed at bus ends  
- [ ] Relay loads do **not** exceed the contact rating; snubbers added for inductive loads  
- [ ] Inputs wired to **dry contact/SELV** only; sensor polarity and returns verified  
- [ ] USB-C used only for configuration; disconnected for normal operation

---

# 5. Installation & Quick Start

The DIO-430-R1 joins your system over **RS-485 (Modbus RTU)**. Setup has two parts:  
1) **Physical wiring**, 2) **Digital configuration** (WebConfig → optional PLC/ESPHome).

---

## 5.1 What You Need

| Category          | Item / Notes |
|-------------------|--------------|
| **Hardware**      | **DIO-430-R1** — DIN-rail module with **4× DI**, **3× SPDT relays**, **3 buttons (2 user-configurable, shown as Button 1 / Button 2; the third is used only for the firmware-update combo)**, **3× LEDs**, **USB-C**, **RS-485**.  |
| **Controller (master)** | HomeMaster **MiniPLC/MicroPLC** or any **Modbus RTU** master. |
| **24 VDC PSU (SELV)** | Regulated **24 VDC**; size for logic + relay coils + sensors; inline panel fuse/breaker. Power input stage includes fuse/TVS/reverse-polarity protection.  |
| **RS-485 cable**  | Twisted pair for **A/B** + **COM/GND** reference, 120 Ω termination at both ends of the trunk.  |
| **USB-C cable**   | For WebConfig via a Chromium browser (service/commissioning).  |
| **Software**      | **Chromium-based browser** with Web Serial (Chrome/Edge). Web page exposes **Address/Baud** + I/O mapping.  |
| **Field I/O**     | **Dry contacts** to DI1…DI4 (isolated front-end per channel). **Relays** (NO/NC/COM) drive LV loads or interposing contactors; add RC/MOV snubbers for inductive loads.  |

> **Quick path:** mount → wire **24 VDC** + **RS-485 A/B/COM** → connect **USB-C** → WebConfig: set **Address/Baud** + map **inputs → relays/LEDs** → disconnect USB → hand over to controller. 

---

## 5.2 Power

The module uses **24 VDC** primary. Onboard regulation provides **5 V → 3.3 V** for logic; DI front-end is isolated.

### 5.2.1 Supply Types
- **24 VDC DIN-rail PSU** → **24Vdc(+) / 0V(–)** power terminals (top row: POWER).   
- **Sensor side (DI)** — isolated input receivers accept field signals; feed your sensors from the 24 V field rail and return to the **DI GND** pins (per-channel). Do **not** back-power logic from sensor rails. 

### 5.2.2 Sizing (rule of thumb)
Account for:
- Base electronics + LEDs  
- **Relay coils** (up to **3** simultaneously)  
- **Sensor rails** (DI field side, if powered from the same 24 V source)

> Size PSU for **worst-case relays + sensors**, then add **≥30 % headroom**.

### 5.2.3 Power Safety
- Correct polarity; keep logic **GND** and DI field ground **separate** (respect isolation domains).   
- Keep upstream **fusing/breaker** in place; the board also has input fuse/TVS/reverse-polarity MOSFET.   
- Use **snubbers** on inductive loads; prefer **interposing contactors** for motors/pumps.   
- **De-energize** before wiring; check shorts before power-up.

---

## 5.3 Networking & Communication

Runtime control is via **RS-485 (Modbus RTU)**. **USB-C** is for local setup/diagnostics (Web Serial).

### 5.3.1 RS-485 (Modbus RTU)

**Physical**
- **Terminals (lower front row):** **B**, **A**, **COM/GND** → then DI and DI-GNDs. Maintain A/B polarity, share the **COM/GND** reference with the controller.   
- **Cable:** Twisted pair (preferably shielded) for A/B + reference.  
- **Termination:** **120 Ω** at both physical ends of the trunk; avoid stubs. 

**Protocol**
- Role: **RTU slave**; controller is **master**.  
- **Address:** 1–255. **Factory default**: **Address 3**, **19200 8N1**.   
- Required: Dedicated **24 VDC** power (bus is data-only).

**Checklist**
- A→A, B→B, **COM→COM** (GND ref).  
- Two end terminations only; daisy-chain topology.  
- Consistent A/B polarity end-to-end.

### 5.3.2 USB-C (WebConfig)

**Purpose:** Chromium (Chrome/Edge) Web Serial setup/diagnostics page. 

**Steps**
1. Connect **USB-C** to the module.  
2. Open the **DIO-430-R1 WebConfig** page and click **Connect**.   
3. Set **Modbus Address & Baud** (header shows **Active Modbus Configuration**).   
4. Configure **Inputs / Relays / LEDs / Buttons**; changes apply live and are saved to flash.   
5. Use **Reset Device** from the page if needed (dialog confirms). 

> If **Connect** is disabled: ensure Chromium + serial permission; close other apps that might hold the port.

---

## 5.4 Installation & Wiring

This section shows typical wiring for **power**, **inputs**, **relays**, **RS-485**, and the **USB-C** service port.  
> ⚠️ Work on de-energized equipment only. Use SELV/PELV supplies for logic and field inputs. Mains on relay contacts must be wired by qualified personnel.

---

### A) Power — 24 VDC (SELV)

Wire the regulated **24 VDC** supply to the top POWER terminals: **V+** and **0V**.

![24Vdc wiring](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/DIO-430-R1/Images/DIO_24Vdc.png)

**Notes**
- Keep V+/0V as a twisted pair; route away from motor cables/contactors.
- The module includes input protection (fuse/TVS/reverse-polarity MOSFET).

---

### B) Digital Inputs (DI1…DI4)

Each input is **isolated**. Land the contact/sensor on **INx** with the paired **GNDx** return.

![Digital inputs](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/DIO-430-R1/Images/DIO_DIInputs.png)

**Tips**
- Supports **dry contacts** or compatible 24 V field signals.
- Configure per [§5.5 WebConfig reference](#55-software--ui-configuration-webconfig-reference) (Enabled/Invert/Type/actions/target).
- Keep field wiring shielded/twisted for long runs; terminate shield at one end only.

---

### C) Relay Outputs (R1…R3)

Relays provide **dry SPDT contacts** (**NO/NC/COM**) for switching low-voltage loads **or** driving an **interposing contactor** for mains/inductive loads.

![Relay wiring example](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/DIO-430-R1/Images/DIO_RelayConnection.png)

**Best practices**
- Add **RC/MOV snubbers** across inductive loads (fans, pumps, contactors).
- Keep load and logic wiring separated; observe conductor ratings and local code.

---

### D) Sensor Rails (12 V / 5 V)

This module **does not export** auxiliary 12 V/5 V rails for field devices.  
- Power sensors from your **panel 24 V** rail (or external rails as required).  
- Return sensor commons to the **matching DI GNDx** terminals; **do not** bond field ground to logic GND.

---

### E) RS-485 (Modbus RTU)

The lower left terminals expose **B**, **A**, and **COM (GND)**. Use shielded twisted pair and daisy-chain topology.

![RS-485 connection](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/DIO-430-R1/Images/DIO_RS485Connection.png)

**Checklist**
- Wire **A→A**, **B→B**, and share **COM/GND** with the controller.
- Terminate the **two physical bus ends** with **120 Ω**.
- Default protocol: **Address 3**, **19200 8N1** (change via WebConfig).

---

### F) USB-C (Service / WebConfig)

- Use **USB-C** for **commissioning and diagnostics** only (Web Serial in Chrome/Edge).  
- Not for powering field devices. Disconnect after setup and hand control to the RS-485 master.

## 5.5 Software & UI Configuration (WebConfig reference)

Open **https://www.home-master.eu/configtool-dio-430-r1** in Chrome/Edge, connect via **USB-C**, and click **Connect**. Changes apply immediately and are saved to flash (no Save button).

![WebConfig — Header & Tools](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/DIO-430-R1/Images/webconfig1.png)

### Header (read-only status)

Status pills (not settings): **Connection** (USB), **Bus** (RS-485), **Model**, **FW**. A banner warns if model/firmware mismatches this configurator.

### Device Setup

| Field | Values | Meaning |
|---|---|---|
| Modbus Address | 1–247 (default 3) | Modbus RTU slave address; must be unique on the bus. |
| Baud Rate | 9600 / 19200 / 38400 / 57600 / 115200 (default 19200) | RS-485 speed **8N1**; must match the controller. |

![WebConfig — Device Setup & Serial Log](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/DIO-430-R1/Images/webconfig2.png)

### Tools

| Button | What it does |
|---|---|
| Identify (~5 s) | Blinks user LEDs to locate the module. |
| Factory reset | Restores all settings to defaults. |
| Reboot | Restarts the module. |

### Digital Inputs (IN1–IN4)

| Field | Values | Meaning |
|---|---|---|
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

![WebConfig — Digital Inputs](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/DIO-430-R1/Images/webconfig3.png)

### Relays (Relay 1–3)

| Field | Values | Meaning |
|---|---|---|
| Enabled | on / off | Relay output active. |
| Inverted | on / off | Invert drive polarity. |
| Power-on | OFF / ON / Restore last | State after power-up. |
| Auto-off, s | 0–65535 (0 = off) | Staircase timer; 0 disables. |

Defaults: enabled, not inverted, OFF at power-on, auto-off 0.

### Interlock

| Field | Values | Meaning |
|---|---|---|
| Enabled | on / off | Pair two relays (motor up/down). |
| Relay A / B | R1 / R2 / R3 | Interlocked pair. |
| Pause, ms | integer (default 500) | Dead-time when reversing. |

### Timing

| Field | Values | Meaning |
|---|---|---|
| Long-press, ms | 50–5000 (default 700) | Long-press threshold. |
| Multi-click gap, ms | 50–2000 (default 300) | Double/triple click window. |
| Debounce, ms | 1–500 (default 30) | Input debounce. |
| Link timeout, ms | 500–60000 (default 5000) | RS-485 link-loss timeout. |

![WebConfig — Relays & Interlock](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/DIO-430-R1/Images/webconfig4.png)

### Buttons (Button 1–2; GPIO2 / GPIO3)

| Field | Values | Meaning |
|---|---|---|
| Short/Long → action | None / Toggle / On / Off / All off | Press action. |
| Short/Long → target | All / R1 / R2 / R3 / None | Target relay(s). |

Defaults: Button 1 → Short = Toggle R1, Long = All off; Button 2 → Short = Toggle R2, Long = None. (Third front button: firmware-update combo only.)

### User LEDs (LED 1–3)

| Field | Values | Meaning |
|---|---|---|
| Source | Off / HA / Link / Local / Child lock / Safe mode / Identify / Relay | LED meaning (8 firmware sources). |
| Mode | Steady / Blink | Display mode. |
| Inverted | on / off | Invert LED level. |
| Arg | integer | Relay # or DI index for Child lock / Relay sources. |

Defaults: LED1 = Link; LED2 = Off; LED3 = HA.

![WebConfig — Buttons & User LEDs](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/DIO-430-R1/Images/webconfig5.png)

### Action / Target reference

| Action | Meaning |
|---|---|
| None | Report only (no local relay action). |
| Toggle / On / Off / All off | Flip, energize, de-energize, or all-off. |

| Target | Meaning |
|---|---|
| All / R1 / R2 / R3 / None | Relay scope. |

## 5.6 Getting Started (3 Phases)

### Phase 1 — Wire
- **24 VDC** to **V+ / 0V (GND)** (top POWER terminals)  
  Use a regulated SELV supply; keep pairs twisted.
- **Digital inputs (IN1–IN4)**: dry contacts → **INx / GNDx**  
  Respect isolation domains; do **not** bridge logic GND ↔ field GND.
- **Relay outputs (R1–R3)**: **COM / NO / NC**  
  Prefer interposing contactors for motors/pumps; add **RC/MOV snubber** on inductive loads.
- **RS-485**: **A / B / COM (GND)**  
  Shielded twisted pair; daisy-chain; terminate bus ends with **120 Ω**.
- **USB-C (service)**: for WebConfig only (no field powering).  
👉 See: **Installation & Quick Start**

---

### Phase 2 — Configure (WebConfig)
- Open the [configurator](https://www.home-master.eu/configtool-dio-430-r1) in **Chrome/Edge** → **USB-C** → **Connect**.
- Set Modbus address/baud and map inputs, relays, buttons, LEDs per [§5.5](#55-software--ui-configuration-webconfig-reference).
- Settings auto-save to flash.

---

### Phase 3 — Integrate (Controller)
- Connect controller (MiniPLC/MicroPLC/PLC/SCADA/ESPHome) via **RS-485**.
- Match **address** and **baud**.
- **Poll**:
  - **Input registers** 0–5 for DI/relay/LED/button masks (FC04)
- **Write**:
  - **Coils** to control relays (e.g., R1/R2/R3 ON/OFF)
- Use with:
  - **HomeMaster MicroPLC / MiniPLC**
  - **ESPHome / SCADA / PLC**  
👉 See: **Modbus RTU Communication & Integration Guide**

---

### ✅ Verify
| Area | What to Check |
|---|---|
| LEDs | **PWR** = ON; **TX/RX** blink during RS-485 traffic |
| Inputs | Toggling a wall switch/sensor changes **INx** state in WebConfig/Modbus |
| Relays | Coil writes toggle **R1–R3**; loads switch correctly; snubber installed for inductive loads |
| Address/Baud | Controller reads module at the configured address/baud without errors |
| Isolation | No unintended bond between logic **GND** and DI field **GNDx** |

---

# 6. Modbus RTU Communication

**Role:** RTU **slave** (controller is master).  
**Defaults:** Address **3**, **19200 8N1** — see [§1 Introduction](#1-introduction).

> v0.2.0 map uses **zero-based offsets** matching `default_dio_430_r1_plc.yaml`. Poll **Input Registers (FC04)** for live state; use **Coils (FC05)** for commands.

---

## 6.1 Address Map (Overview)

| Type | Offsets | Purpose |
|------|---------|---------|
| **Input Registers** (FC04) | `0…29` | State masks, status flags, event counters |
| **Coils** (FC01/05) | `0…14` | Relays, LEDs, DI lock, device commands |
| **Holding Registers** (FC03) | `0…46` | Identity, DI/relay/button/LED config, timing |

---

## 6.2 Input Registers (FC04) — State & Counters

| Reg | Name | Encoding | Description |
|----:|------|----------|-------------|
| 0 | **DI_STATE_MASK** | bitmask | bit0..3 → DI1..DI4 |
| 1 | **RLY_STATE_MASK** | bitmask | bit0..2 → R1..R3 |
| 2 | **BTN_STATE_MASK** | bitmask | bit0..1 → Btn1..Btn2 |
| 3 | **LED_STATE_MASK** | bitmask | bit0..2 → LED1..LED3 |
| 4 | **STATUS_FLAGS** | bitmask | bit1 = link OK, bit3 = config dirty |
| 5 | **LOCK_MASK** | bitmask | bit0..3 → DI1..DI4 child-lock |
| 6–29 | **Event counters** | u16 | Index = `6 + source×4 + type`; sources: DI1..4 = 0..3, Btn1..2 = 4..5; types: 0=single, 1=double, 2=triple, 3=long |

---

## 6.3 Coils (FC01/05) — Commands

| Coil | Name | Description |
|-----:|------|-------------|
| 0–2 | **R1–R3** | Relay 1–3 ON/OFF (maintained) |
| 3 | **ALL_OFF** | Turn all relays off (pulse) |
| 4 | **LOCAL_LOGIC** | Reserved (internal local-logic flag) |
| 5 | **IDENTIFY** | Front-panel identify blink (pulse) |
| 6 | **SAVE_CFG** | Persist settings to flash (pulse) |
| 7 | **REBOOT** | Soft reset (pulse) |
| 8–10 | **LED1–3_HA** | Home Assistant LED override |
| 11–14 | **DI1–4_LOCK** | Child-lock per digital input |

---

## 6.4 Holding Registers (FC03) — Configuration

Configuration is normally done via **WebConfig**. Holding registers mirror the persisted settings (offsets 0–46):

| Reg | Name | R/W | Encoding | Notes |
|----:|------|:---:|----------|-------|
| 0 | **MODEL_ID** | R | u16 | Device model ID (**5** for DIO-430-R1) |
| 1 | **FW_VERSION** | R | u16 | Packed `(major<<8)\|minor` |
| 2 | **MAP_VERSION** | R | u16 | Modbus map version |
| 3 | **MB_ADDR** | R/W | u16 | Modbus address 1–255 |
| 4 | **MB_BAUD** | R/W | enum | 0=9600, 1=19200, 2=38400, 3=57600, 4=115200 |
| 8 | **DI_EN_MASK** | R/W | bitmask | bit0..3 → DI1..DI4 enable |
| 9 | **DI_INV_MASK** | R/W | bitmask | bit0..3 → DI1..DI4 invert |
| 10 | **DI_TYPE_MASK** | R/W | bitmask | bit0..3 → 0=Maintained, 1=Momentary |
| 11 | **DI_LOCK_MASK** | R/W | bitmask | bit0..3 → child-lock per DI |
| 12–15 | **DI_FOLLOW** | R/W | u16×4 | Follow target per DI (Maintained mode) |
| 16–19 | **DI_SHORT** | R/W | packed | Short-press action+target per DI (Momentary) |
| 20–23 | **DI_LONG** | R/W | packed | Long-press action+target per DI (Momentary) |
| 24 | **RLY_EN_MASK** | R/W | bitmask | bit0..2 → R1..R3 enable |
| 25 | **RLY_INV_MASK** | R/W | bitmask | bit0..2 → R1..R3 invert |
| 26 | **RLY_POWERON** | R/W | bitmask | Power-on state per relay (0=OFF, 1=ON, 2=restore) |
| 27–29 | **RLY_AUTOOFF** | R/W | u16×3 | Auto-off timer (s) per relay; 0=disabled |
| 30–31 | **BTN1_SHORT/LONG** | R/W | packed | Button 1 short/long action+target |
| 32–33 | **BTN2_SHORT/LONG** | R/W | packed | Button 2 short/long action+target |
| 34–36 | **LED1–3_CFG** | R/W | packed | Per LED: source, mode, invert, arg (relay/DI index) |
| 40 | **INTERLOCK** | R/W | packed | Interlock enable + relay pair |
| 41 | **INTERLOCK_PAUSE** | R/W | u16 | Interlock dead-time (ms) |
| 42 | **DI_MAINT_MODE** | R/W | bitmask | Maintained mode per DI: 0=Toggle, 1=Follow |
| 43 | **LONGPRESS_MS** | R/W | u16 | Long-press threshold (ms) |
| 44 | **MULTICLICK_MS** | R/W | u16 | Multi-click gap (ms) |
| 45 | **DEBOUNCE_MS** | R/W | u16 | Debounce time (ms) |
| 46 | **LINKTIMEOUT_MS** | R/W | u16 | RS-485 link timeout (ms) |

**Packed action+target byte:** upper 3 bits = action (0=None, 1=Toggle, 2=On, 3=Off, 4=All off); lower 3 bits = target (0=None, 1=R1, 2=R2, 3=R3, 4=All).

---

## 6.5 Register Use Examples

### A) Toggle Relay 2 from a PLC
1. Write `1` to **Coil 1** → Relay 2 ON  
2. Write `0` to **Coil 1** → Relay 2 OFF

### B) Read DI3 state
- Read **Input Register 0** (FC04), test bit 2 of **DI_STATE_MASK**

### C) Map IN3 as maintained toggle → Relay 1
1. Set bit2 in **HREG 8** (enable IN3)  
2. Clear bit2 in **HREG 10** (Maintained type)  
3. Write follow target `1` to **HREG 14** (DI3 follow → R1)  
4. Pulse **Coil 6 (SAVE_CFG)**

### D) Map Button 1 short press → toggle Relay 2
- Write packed Toggle+R2 to **HREG 30**; pulse **Coil 6**

### E) Persist and reboot
- Pulse **Coil 6 (SAVE_CFG)** then **Coil 7 (REBOOT)**

---

## 6.6 Polling Recommendations

- **Input registers 0–5:** 5–10 Hz (100–200 ms) for DI/relay/LED masks  
- **Event counters 6–29:** 1–2 s (change slowly)  
- **Coils:** write on change only; relays 0–2 are maintained  
- **Holding:** configure at commissioning; avoid frequent writes  
- **Edge logic:** use **Maintained/Toggle** for latching inputs; **Momentary/On/Off** when the PLC supervises timers

---

# 7. ESPHome Integration Guide (MiniPLC/MicroPLC + DIO-430-R1)

> **Support status:** ✔️ Supported via ESPHome `uart` + `modbus` + `modbus_controller` and a reusable **package**.  
> **Module role:** Modbus RTU **slave** on RS-485.  
> **Defaults:** Address **3**, **19200 8N1** (change in WebConfig).

---

## 7.1 Minimal YAML (Controller side)

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
          dio_prefix: "DIO#1"  # shown in Home Assistant entity names
          dio_id: dio_1        # internal unique id
          dio_address: 4       # Modbus address set in WebConfig for this DIO
    refresh: 1d
```

> For **multiple** DIOs, duplicate the `dio1:` block (`dio2:`, `dio3:`…) with unique `dio_id`, `dio_prefix`, and `dio_address`.

---

## 7.2 Entities exposed (from the package)

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

---

## 7.3 Optional: direct (manual) entity mapping

If you prefer not to use the package, you can expose the core points directly:

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
  # Relays as Coils (0x offsets 0..2)
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
  # LED and Button masks from Input Registers 3/2 (FC04)
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

---

## 7.4 Home Assistant tips (dashboards & automations)

- **Dashboards**
  - **Lighting panel:** Card for **Relay 1–3** plus DI tiles (e.g., wall switch/sensor feedback).
  - **Maintenance card:** **Relay 1–3** switches + **Identify** / **All off** template buttons
- **Automations**
  - **DI → Relay:** If you keep the logic in HA/PLC (instead of module mapping), trigger relay switches when a DI goes high.  
  - **Night mode:** When `input_boolean.night_mode` is on, force a specific **Override** ON and release it in the morning.
- **Naming**
  - Use `dio_prefix` to keep entities readable (`DIO#1 Relay 1`, `DIO#2 DI3`, etc.).

---

## 7.5 Troubleshooting

- **No response / timeouts:** check A/B polarity, shared **COM/GND** reference, and **120 Ω** termination at bus ends.
- **Wrong device:** make sure `dio_address` in the package matches the WebConfig address.
- **Relays don’t switch:** ensure the relay is **Enabled** in WebConfig and not “held” by an **Override**.
- **DI not changing:** verify wiring to **INx/GNDx** (respect isolation); check **Enable/Invert/Type** and Short/Long actions in WebConfig.

---

## 7.7 Notes & Versions

- Works with recent ESPHome releases (e.g., 2025.x).  
- Keep `update_interval` modest (e.g., 200–500 ms) unless you need faster DI polling.  
- For multiple devices on one bus, stagger `update_interval`/`command_throttle` to reduce collisions.

---

# 8. Programming & Customization (DIO-430-R1)

## 8.1 Supported Languages
- **Arduino**
- **C++** (PlatformIO)
- **MicroPython** (community builds for RP23xx-class MCUs)

---

## 8.2 Flashing (USB‑C, Hardware Buttons Only)

> The module exposes a USB device for flashing. **All reset/boot actions are done with the front buttons in hardware.**

**Button layout (front panel):**  

  ![Button Layout 1‑2‑3](https://raw.githubusercontent.com/isystemsautomation/homemaster-dev/refs/heads/main/DIO-430-R1/Images/buttons1.png)

**Combinations**
- **1 + 2 + 3 → BOOT mode** (enter bootloader for UF2 flashing): press and hold **Buttons 1, 2, and 3**; release **Button 1**; then release **Buttons 2 and 3** together. The module appears as a USB flash drive (**RPI-RP2**).
- **1 + 3 → RESET** (hardware reset/restart)

**Steps (UF2/IDE)**
1. Connect **USB‑C** to a PC (disconnect RS‑485 during flashing).
2. Enter **BOOT** mode using the **1 + 2 + 3** sequence above. The board mounts as a USB flash drive (**RPI-RP2**) for UF2 drag-and-drop, or as a serial port for IDE upload.
3. Flash:
   - **UF2**: download the pre-built **v0.2.0** image [`default_DIO_430_R1.ino.uf2`](https://github.com/isystemsautomation/homemaster-dev/raw/refs/heads/main/DIO-430-R1/Firmware/v0.2.0/default_DIO_430_R1/build/rp2040.rp2040.generic_rp2350/default_DIO_430_R1.ino.uf2) (or build from source), then drag-and-drop it onto the mounted **RPI-RP2** drive; the module restarts automatically.
   - **PlatformIO / Arduino IDE**: select the correct board/port and upload.
4. If needed, press **Buttons 1 + 3** for a hardware **RESET**.

> No factory‑reset function is provided. Configuration remains intact across normal firmware updates.

---

## 8.3 Arduino / PlatformIO Notes

**Board / Toolchain**
- **Board:** Generic **RP2350** (or vendor core for **RP2350A**)
- **USB:** CDC enabled (serial logging)
- **FS:** LittleFS partition recommended (for settings)

**Required Libraries (Library Manager names / versions)**
- `Arduino_JSON` (0.2.0)
- `Modbus-Arduino` (1.3.0) + `Modbus-Serial` (2.0.6) — `#include <ModbusSerial.h>` in sketch
- `Simple Web Serial` (1.0.0)
- **From core:** `LittleFS`, `Wire` (no separate install)

**Pin Mapping:** see [§2.1 Diagrams & Pinouts](#21-diagrams--pinouts) (`DIO_MCU_Pinouts.png`) and firmware `default_DIO_430_R1.ino`.

**Build Tips**
- Start at **19200 8N1** on RS‑485 during bring‑up.
- After flashing, disconnect USB‑C and return control to the master on RS‑485.

---

## 8.4 Firmware Updates

See [§8.2 Flashing](#82-flashing-usbc-hardware-buttons-only) and [§11 Downloads](#11-downloads). Configuration in flash/LittleFS is preserved across normal updates unless explicitly erased.

---

# 9. Maintenance & Troubleshooting

## 9.1 Status LEDs (typical)
- **PWR** — ON in normal operation
- **TX/RX** — blink on Modbus traffic
- **User LEDs (1–3)** — follow relay logic (Steady/Blink based on WebConfig mode)

## 9.2 Resets
- **Power cycle:** remove 24 V, wait 5 s, re‑apply
- Use **Buttons 1 + 3** for a hardware **RESET**

## 9.3 Common Issues

| Symptom | Checks |
|---|---|
| No Modbus comms | A/B polarity, **COM/GND** reference, 120 Ω termination, address/baud match, only two end terminators |
| Relays don’t actuate | Relay **Enabled** in WebConfig, no active **Override** holding state, coil invert setting, Modbus coil writes acknowledged |
| DI not changing | Wire to **INx/GNDx** (isolated field side), check **Enable/Invert/Action/Target** in WebConfig, debounce expectations |
| USB won’t connect | Chrome/Edge with Web Serial, close other serial apps, check cable/port permissions |
| Config not saved | Allow idle for auto‑save or use *Save* if available; verify LittleFS space |

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

- **Firmware v0.2.0 (pre-built UF2 — drag-and-drop upgrade)**  
  - [`default_DIO_430_R1.ino.uf2`](https://github.com/isystemsautomation/homemaster-dev/raw/refs/heads/main/DIO-430-R1/Firmware/v0.2.0/default_DIO_430_R1/build/rp2040.rp2040.generic_rp2350/default_DIO_430_R1.ino.uf2)
- **Firmware publishing (maintainers — what to commit after compile)**  
  - [`DIO-430-R1/Firmware/README.md`](https://github.com/isystemsautomation/homemaster-dev/blob/main/DIO-430-R1/Firmware/README.md)
- **Firmware source (Arduino, v0.2.0)**  
  - [`DIO-430-R1/Firmware/v0.2.0/default_DIO_430_R1/`](https://github.com/isystemsautomation/homemaster-dev/tree/main/DIO-430-R1/Firmware/v0.2.0/default_DIO_430_R1)
- **YAML configs (ESPHome, v0.2.0)**  
  - Package: [`DIO-430-R1/Firmware/v0.2.0/default_dio_430_r1_plc/`](https://github.com/isystemsautomation/homemaster-dev/tree/main/DIO-430-R1/Firmware/v0.2.0/default_dio_430_r1_plc)
- **WebConfig tool (HTML/JS, v0.2.0)**  
  - [`DIO-430-R1/Firmware/v0.2.0/ConfigToolPage.html`](https://github.com/isystemsautomation/homemaster-dev/blob/main/DIO-430-R1/Firmware/v0.2.0/ConfigToolPage.html)
- **Firmware source (Arduino, v0.1.0 — legacy)**  
  - [`DIO-430-R1/Firmware/v0.1.0/default_DIO_430_R1/`](https://github.com/isystemsautomation/homemaster-dev/tree/main/DIO-430-R1/Firmware/v0.1.0/default_DIO_430_R1)
- **YAML configs (ESPHome, v0.1.0 — legacy)**  
  - Package & examples: [`DIO-430-R1/Firmware/v0.1.0/default_dio_430_r1_plc/`](https://github.com/isystemsautomation/homemaster-dev/tree/main/DIO-430-R1/Firmware/v0.1.0/default_dio_430_r1_plc)
- **WebConfig tool (HTML/JS, v0.1.0 — legacy)**  
  - [`DIO-430-R1/Firmware/v0.1.0/ConfigToolPage.html`](https://github.com/isystemsautomation/homemaster-dev/blob/main/DIO-430-R1/Firmware/v0.1.0/ConfigToolPage.html)
- **Schematics (PDF)**  
  - Field Board: [`Schematics/DIO-430-R1-FieldBoard.pdf`](https://github.com/isystemsautomation/homemaster-dev/blob/main/DIO-430-R1/Schematics/DIO-430-R1-FieldBoard.pdf)  
  - MCU Board: [`Schematics/DIO-430-R1-MCUBoard.pdf`](https://github.com/isystemsautomation/homemaster-dev/blob/main/DIO-430-R1/Schematics/DIO-430-R1-MCUBoard.pdf)
- **Images & diagrams**  
  - [`DIO-430-R1/Images/`](https://github.com/isystemsautomation/homemaster-dev/tree/main/DIO-430-R1/Images)
- **Datasheets**  
  - Refer to the `Schematics/` folder BOM notes for part numbers (e.g., ISO1212, MAX485, HF115F).

---

# 12. Support

- **Official Support Portal:** https://www.home-master.eu/support
- **WebConfig Tool:** https://www.home-master.eu/configtool-dio-430-r1
- **YouTube:** https://youtube.com/@HomeMaster
- **Hackster:** https://hackster.io/homemaster
- **Reddit:** https://reddit.com/r/HomeMaster
- **Instagram:** https://instagram.com/home_master.eu

## Compliance & Certifications

The DIO-430-R1 module is CE marked. **ISYSTEMS AUTOMATION S.R.L.** (HomeMaster® brand)
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
| EU Declaration of Conformity (DoC) | [DoC-DIO-430-R1-V1.0.pdf](./Manuals/DoC-DIO-430-R1-V1.0.pdf) |
| Datasheet | [DIO-430-R1_Datasheet.pdf](./Manuals/DIO-430-R1_Datasheet.pdf) |

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
