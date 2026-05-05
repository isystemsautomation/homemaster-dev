# HomeMaster MicroPLC

![HomeMaster MicroPLC](./Images/MicroPLC.png)

> **Project status:** under active development and testing.
> Hardware and firmware are pre-release and may change.
> Check [Releases](../../releases) for stable versions.

## Description

HomeMaster MicroPLC is a compact open-source automation controller based on `ESP32-WROOM-32U`.
It is designed for Home Assistant integration via ESPHome and supports local control, sensor
inputs, and industrial communication with RS-485 Modbus RTU.

| Resource | Link |
|---|---|
| 🛒 Product page | [home-master.eu](https://www.home-master.eu/shop/esp32-microplc-56) |
| 📁 Repository | [GitHub](https://github.com/isystemsautomation/homemaster-dev/tree/main/MicroPLC) |
| ⚙️ Default Firmware (YAML) | [microplc.yaml](https://github.com/isystemsautomation/homemaster-dev/blob/main/MicroPLC/Firmware/microplc.yaml) |
| 🔧 Schematics | [Schematic/](https://github.com/isystemsautomation/homemaster-dev/tree/main/MicroPLC/Schematic) |
| 🏠 Maker | [home-master.eu](https://www.home-master.eu/) |

## Table of Contents

- [Description](#description)
- [Features](#features)
- [Quick Start](#quick-start)
- [Programming](#programming)
- [Bus System Configuration](#bus-system-configuration)
- [Specifications](#specifications)
- [Pinout](#pinout)
- [MicroPLC Functional Block Diagram](#microplc-functional-block-diagram)
- [License](#license)

## Features

- `ESP32-WROOM-32U` microcontroller with Wi-Fi and Bluetooth
- ESPHome-compatible firmware for Home Assistant integration
- RS-485 Modbus RTU interface for extension module communication
- USB Type-C for programming, debugging, and power
- 1-Wire interface with ESD and overvoltage protection
- PCF8563 RTC for time-based automation
- One industrial-grade relay with varistor and opto-isolation
- One 24 V digital input with surge protection (ISO1212)
- Four front-panel buttons and status LEDs
- DIN-rail mounting for standard control cabinets

## Quick Start

1. Mount the device on a 35 mm DIN rail inside a closed cabinet.
2. Power on the device.
3. Open [improv-wifi.com](https://www.improv-wifi.com) in Chrome/Edge.
4. Connect over USB (serial) or Bluetooth LE and enter Wi-Fi credentials.
5. Open ESPHome Dashboard and click **Take Control** after discovery.

## Programming

The MicroPLC ships with ESPHome and can be configured in three standard ways.

### Improv Wi-Fi Setup

1. Power on your HomeMaster MicroPLC.
2. Go to [improv-wifi.com](https://www.improv-wifi.com).
3. Connect via USB (serial) or Bluetooth LE.
4. Enter your Wi-Fi SSID and password, then connect.
5. The device joins your Wi-Fi and becomes available in Home Assistant / ESPHome.

### One-Click Import (ESPHome Dashboard)

Once connected to Wi-Fi, the MicroPLC is auto-discovered in ESPHome Dashboard.
Click **Take Control** to import the official configuration from GitHub.

### USB Type-C Flashing (ESPHome Dashboard)

1. Connect the MicroPLC to your computer using USB Type-C.
2. Open the YAML config:
   [microplc.yaml](https://github.com/isystemsautomation/homemaster-dev/blob/main/MicroPLC/Firmware/microplc.yaml)
3. Import it into ESPHome Dashboard and set your Wi-Fi credentials.
4. Flash the device directly from ESPHome Dashboard.
5. The board supports automatic reset/boot control (no manual BOOT/RESET sequence needed).

## Bus System Configuration

### I2C

| Signal | Pin |
|---|---|
| SDA | GPIO32 |
| SCL | GPIO33 |

### I2C Addresses

| Device | Address |
|---|---|
| PCF8563 | `0x51` |

## Specifications

| Feature | Details |
|---|---|
| Microcontroller | ESP32-WROOM-32U |
| Power Supply | 5 V via USB-C (programming) or 24 V via terminal |
| Relay Output | 1× 16 A (optically isolated) |
| Digital Input | 1× 24 V DI (ISO1212-based) |
| Communication | RS-485, Wi-Fi, Bluetooth, USB-C |
| RTC | PCF8563 |
| 1-Wire | 1 channel (ESD/OVP protected) |
| Mounting | DIN-rail |
| Firmware | ESPHome (pre-installed), Arduino |

## Pinout

![MicroPLC Pinout](./Images/pinout.png)

## MicroPLC Functional Block Diagram

![MicroPLC Block Diagram](./Images/diagram.png)

## License

This project uses a hybrid licensing model.

### Hardware

Hardware designs (schematics, PCB layouts, BOMs) are licensed under **CERN-OHL-W v2**.

### Firmware & ESPHome Integration

All firmware, ESPHome configurations, and software components are licensed under the **MIT License**.

This ensures full compatibility with ESPHome and Home Assistant while protecting hardware designs.
See LICENSE files in each directory for full terms.

---

> 🔧 **HOMEMASTER - Modular control. Custom logic.**

## Compliance & Certifications

The MicroPLC module is CE marked. **ISYSTEMS AUTOMATION S.R.L.** (HomeMaster® brand)
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
| EU Declaration of Conformity (DoC) | [DoC-MicroPLC-V1.0.pdf](./Manuals/DoC-MicroPLC-V1.0.pdf) |
| Datasheet | [MicroPLC_Datasheet.pdf](./Manuals/MicroPLC_Datasheet.pdf) |

### Trademark

**HomeMaster®** is a registered European Union trademark of ISYSTEMS AUTOMATION S.R.L.,
EUTM No. 019082911, registered with EUIPO on 15 January 2025.

---

**Manufacturer:** ISYSTEMS AUTOMATION S.R.L. (HomeMaster® brand)
**Registered office (sediul social):** Str. Domnișori, Nr. 81, Bl. 62, Scara A, Etaj 3, Ap. 12, 100284 Ploiești, Jud. Prahova, Romania
**Office / Contact address:** Diligentei 18, Ploiești, Romania
**CUI / VAT:** RO 21537032
**EUID:** ROONRC.J2007000919293
**Telephone:** +40 747 757 798
**Website:** [https://www.home-master.eu](https://www.home-master.eu)
