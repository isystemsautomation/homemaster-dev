# HomeMaster MicroPLC

![HomeMaster MicroPLC](./Images/MicroPLC.png)

> **Firmware:** Firmware is updated per module; see each module’s `Firmware/` folder (and [config.home-master.eu](https://config.home-master.eu/)) for the current version.

## Description

HomeMaster MicroPLC is a compact ESP32 Modbus RTU master with ESPHome pre-installed — the entry point that drives a bus of HomeMaster I/O modules over RS-485 while running local control offline for Home Assistant.

| Resource | Link |
|---|---|
| 🛒 Product page | [home-master.eu](https://www.home-master.eu/shop/esp32-microplc-56) |
| 📁 Repository | [GitHub](https://github.com/isystemsautomation/homemaster-dev/tree/main/MicroPLC) |
| ⚙️ Default Firmware (YAML) | [microplc.yaml](https://github.com/isystemsautomation/homemaster-dev/blob/main/MicroPLC/Firmware/microplc.yaml) |
| 📝 Changelog | [CHANGELOG.md](Firmware/CHANGELOG.md) |
| 🔧 Schematics | [Schematics/](https://github.com/isystemsautomation/homemaster-dev/tree/main/MicroPLC/Schematics) |
| 🏠 Maker | [home-master.eu](https://www.home-master.eu/) |

## Key advantages

- Compact ESP32 **Modbus RTU master** with **ESPHome pre-installed** — the entry point that drives a bus of HomeMaster I/O modules.
- Native Home Assistant API — no MQTT broker, no Modbus register mapping for onboard entities.
- Onboard I/O for room-level use: **1 relay**, **1 DI**, **1-Wire**, RTC; expand via RS-485 modules.
- *The controller does the thinking; the server only visualizes* — critical logic stays on the MicroPLC when HA is offline.
- **Improv** Wi-Fi onboarding (BLE/USB); runs local and offline.
- Open hardware (**CERN-OHL-W v2**) and firmware (**MIT**) — no vendor lock-in.

## Table of Contents

- [Description](#description)
- [Key advantages](#key-advantages)
- [Features](#features)
- [Quick Start](#quick-start)
- [LED Behaviour](#led-behaviour)
- [Programming](#programming)
- [USB Serial Driver & Port Access](#usb-serial-driver--port-access)
- [Firmware Updates](#firmware-updates)
- [Bus System Configuration](#bus-system-configuration)
- [Specifications](#specifications)
- [Entity Reference](#entity-reference)
- [Pinout](#pinout)
- [MicroPLC Functional Block Diagram](#microplc-functional-block-diagram)
- [License](#license)

## Features

- `ESP32-WROOM-32U-N16` microcontroller with Wi-Fi and Bluetooth (16 MB flash)
- ESPHome-compatible firmware for Home Assistant integration
- RS-485 Modbus RTU interface for extension module communication
- USB Type-C for programming, debugging, and power
- 1-Wire interface with ESD and overvoltage protection
- PCF8563 RTC for time-based automation
- One industrial-grade relay with varistor and opto-isolation
- One 24 V digital input on **GPIO36** with surge protection (ISO1212)
- Four front-panel buttons and status LEDs
- DIN-rail mounting for standard control cabinets
- **24 V DC only** field supply (plus USB-C for programming) — no AC mains input
- **No onboard Ethernet or microSD** (unlike MiniPLC); expand I/O over RS-485 modules

## Quick Start

1. Mount the device on a 35 mm DIN rail inside a closed cabinet.
2. Power on the device.
3. Open [improv-wifi.com](https://www.improv-wifi.com) in Chrome/Edge.
4. Connect over USB (serial) or Bluetooth LE and enter Wi-Fi credentials.
5. Within **15 minutes** of power-on, open ESPHome Device Builder and click **Take control** after discovery. Home Assistant will then prompt for the API encryption key and install it automatically.


## LED Behaviour

<!-- LED behaviour during the provisioning window is not documented by upstream
     ESPHome and is not confirmed here — awaiting clarification. Do not document
     observed quirks (e.g. LED staying dark while waiting for setup) until upstream does. -->

The firmware-configurable status LED is driven by ESPHome `status_led` on **GPIO25**
(`inverted: true`).

| Behaviour | Meaning |
|---|---|
| Off | Normal operation — no warning or error |
| Slow blink (~1 Hz) | Warning active. Warnings include Wi-Fi disruption and the native API being present with **no client connected** |
| Fast blink | Error found during setup |

Other front-panel LEDs (power, relay, DI, RS-485 RX/TX) are hardware-driven and
are not controlled by this component.

<!-- TODO: status-LED photo of the MicroPLC front panel, if available -->

## Programming

The MicroPLC ships with ESPHome and can be configured in three standard ways. Firmware **1.1.0** requires **ESPHome ≥ 2026.7.0**.

### Improv Wi-Fi Setup

1. Power on your HomeMaster MicroPLC.
2. Go to [improv-wifi.com](https://www.improv-wifi.com).
3. Connect via USB (serial) or Bluetooth LE.
4. Enter your Wi-Fi SSID and password, then connect.
5. The device joins your Wi-Fi and becomes available in Home Assistant / ESPHome Device Builder.

From firmware **1.1.0** the device accepts initial configuration only for
**15 minutes after power-on**. When the window closes, new native API clients are
refused and BLE Improv stops accepting Wi-Fi credentials. **Power-cycle** the
device to reopen the window for another 15 minutes. Serial provisioning over USB
continues to work regardless, because it requires physical access.

### One-Click Import (ESPHome Dashboard)

Once connected to Wi-Fi, the MicroPLC is auto-discovered in ESPHome Device Builder
as `homemaster-microplc-<mac>` running `Homemaster.MicroPLC 1.1.0`.
Click **Take control** to import the official configuration from GitHub. That step
generates the **API encryption key** and **OTA password** for this device.

From **1.1.0** the native API is encrypted. Each device gets its own key at
adoption — no key is baked into the published factory YAML, because that file is
identical for every unit and public on GitHub. **Save the key**; you need it when
moving the device between Home Assistant instances. Retrieve it later from
**Device info → Show encryption key**.

<!-- TODO: screenshots — Take control discovery banner; ESPHome Encryption Key dialog; Device info → Show encryption key -->


### USB Type-C Flashing (ESPHome Dashboard)

1. Connect the MicroPLC to your computer using USB Type-C.
2. Open the YAML config:
   [microplc.yaml](https://github.com/isystemsautomation/homemaster-dev/blob/main/MicroPLC/Firmware/microplc.yaml)
3. Import it into ESPHome Dashboard and set your Wi-Fi credentials.
4. Flash the device directly from ESPHome Dashboard.
5. The board supports automatic reset/boot control (no manual BOOT/RESET sequence needed).

### USB Serial Driver & Port Access

The USB Type-C port uses a **Silicon Labs CP2102N** USB-to-UART bridge for serial console, Improv Wi-Fi provisioning over USB Serial, and ESPHome USB flashing.

- **Windows** — The CP210x driver installs automatically via **Windows Update** on first connect. The port appears as `COMx` in Device Manager.
- **macOS** — Install the **Silicon Labs CP210x VCP driver**, then **enable its system extension**: on **macOS 15 / 26**, open **System Settings → General → Login Items & Extensions → Extensions**; on older macOS, use **System Settings → Privacy & Security** and allow the Silicon Labs extension. Log out and back in, or reboot, if prompted.
- **Linux** — Support is **in-kernel** (`cp210x`). Add your user to the **`dialout`** group (`sudo usermod -aG dialout $USER`), then log out and back in. The port appears as `/dev/ttyUSB0` or similar.

**Bluetooth (BLE Improv):** no driver is needed. **Web Bluetooth** works in Chrome/Edge on most platforms; on **desktop Linux** it is **off by default** (use USB Serial or enable the browser flag); **Firefox** and **iOS** do not support Web Bluetooth — use USB Serial or Chrome/Edge on Android for BLE provisioning.


## Firmware Updates

The device supports two firmware update methods:

### ESPHome Updates (User-controlled)

After taking control in ESPHome Device Builder, firmware can be updated manually:

- Build new firmware from ESPHome
- Upload via OTA or USB
- Full control over configuration

### Managed Updates (HTTP)

The device also supports vendor-provided firmware updates.

A firmware update entity is exposed in Home Assistant, allowing the device to check for new firmware versions and install updates directly.

This mechanism uses the `update.http_request` component with a hosted firmware manifest,
downloading updates over HTTPS directly to the device.

If a newer firmware version is available, it can be installed directly from Home Assistant.

The device polls the firmware manifest every 6 hours (`update_interval: 6h`) at
`https://isystemsautomation.github.io/homemaster-dev/MicroPLC/Firmware/manifest.json`.
To disable vendor-managed OTA, remove the `update:`, `http_request:`, and
`ota: platform: http_request` blocks from your YAML. Updates will then only be
possible via ESPHome OTA or USB.

> ℹ️ **OTA security:** OTA updates are downloaded over HTTPS from GitHub Pages. Trust depends on the security of the HomeMaster GitHub account; firmware files are not separately signed. If you need a stricter trust model, take control in ESPHome Device Builder and manage updates yourself.

> ⚠️ **OTA safety:** Do not interrupt a firmware update once started.
> If an OTA update is interrupted mid-flash, the device may fail to boot.
> If this occurs, reflash via USB-C using ESPHome or the ESP flashing tool.
> ESPHome safe mode is active for the first 10 boot attempts after a
> failed OTA — connect via USB and reflash to recover.

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

### RS-485 UART

ESPHome id **`uart_modbus`**. Attach your own `modbus:` / `modbus_controller:` blocks to this UART (same pattern as every HomeMaster module README).

| Item | Value |
|---|---|
| TX | GPIO17 |
| RX | GPIO16 |
| Baud rate | **19200** |

## Specifications

| Feature | Details |
|---|---|
| Microcontroller | ESP32-WROOM-32U-N16 (16 MB flash) |
| Power Supply | **24 V DC only** via terminal (field); 5 V via USB-C (programming) |
| Onboard expansion | **No Ethernet**, **no microSD** (unlike MiniPLC) |
| Relay Output | 1× SPDT relay (HF115F/005-1ZS3); 3 A @ 250 VAC module limit (relay component rated higher) |
| Digital Input | 1× 24 V DI on **GPIO36** (ISO1212-based) |
| Communication | RS-485 Modbus RTU (MAX485, half-duplex, non-isolated), Wi-Fi, Bluetooth, USB-C |
| RTC | PCF8563 |
| 1-Wire | 1 channel (ESD/OVP protected) |
| Mounting | DIN-rail |
| DIN width | 2 modules (2 × 17.5 mm) |
| Operating temperature | 0 °C to +40 °C |
| Storage temperature | −10 °C to +55 °C |
| Relative humidity | 0–90 % RH, non-condensing |
| Firmware | ESPHome (pre-installed), Arduino |
| Minimum ESPHome | **2026.7.0** (`esphome.min_version`; required for `provisioning:`) |

## Entity Reference

<details>
<summary>Click to expand full entity reference table</summary>

| Entity | Type | Default | Description |
|---|---|---|---|
| ESP Status | Binary Sensor | Enabled (diagnostic) | Wi-Fi / API connection status |
| DI #1 | Binary Sensor | Enabled | 24 V DC digital input (GPIO36, ISO1212) |
| 1-Wire Bus 1 Temperature | Sensor | Enabled | DS18B20 on GPIO4 |
| Relay | Switch | Enabled | Onboard SPDT relay (GPIO26) |
| Restart | Button | Enabled (config) | Reboot only — does not clear Wi-Fi or API key |
| WiFi Signal | Sensor | Enabled (diagnostic) | RSSI in dBm |
| ESP32 Temperature | Sensor | Enabled (diagnostic) | Internal chip temperature |
| ESP Uptime Human | Text Sensor | Enabled (diagnostic) | Human-readable uptime |
| ESPHome Version | Text Sensor | Enabled (diagnostic) | Running ESPHome version |
| ESP IP Address | Text Sensor | Enabled (diagnostic) | Device IP address |
| Firmware Update | Update | Enabled | Vendor OTA update entity (HTTP) |

</details>

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

## Pinout

![MicroPLC Pinout](./Images/pinout.png)


### 4.2 Connectors & terminal map

<!-- hm:terminal-map:begin -->

**Top row** (24Vdc | DI 24Vdc | RELAY)

| Pos | Label | Group | Function |
|-----|-------|-------|----------|
| 1 | +V | POWER | 24 V DC input |
| 2 | 0V | POWER | 24 V DC return |
| 3 | I.1 | DI1 | DI1 input |
| 4 | GND | DI1 | DI1 return |
| 5 | C | RELAY | Relay common |
| 6 | NC | RELAY | Relay normally closed |

**Bottom row** (RS-485 | BUS)

| Pos | Label | Group | Function |
|-----|-------|-------|----------|
| 1 | COM | RS485 | RS-485 signal reference |
| 2 | B | RS485 | RS-485 data - |
| 3 | A | RS485 | RS-485 data + |
| 4 | +5V | BUS | 1-Wire sensor supply |
| 5 | D | BUS | 1-Wire data |
| 6 | Gnd | BUS | 1-Wire ground |

**Ports & service interfaces**

| Id | Type | Note |
|----|------|------|
| USB-C |  | 5 V via USB-C (programming) |

**Housing notes**

- Relay exposes C and NC only. There is no NO terminal and the contact is CLOSED when the relay is de-energised - the same inverted behaviour as the OpenTherm Gateway.
- ONE 1-Wire bus. The '1WIRE 1/2' legend labels the two data terminals of a single bus, not two buses.
- DI pair reads signal-then-GND, like DIM and opposite to DIO.
- Power reads +V then 0V.

<!-- hm:terminal-map:end -->

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

Firmware release history: [Firmware/CHANGELOG.md](Firmware/CHANGELOG.md)

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
| EU Declaration of Conformity (DoC) | [DoC_MicroPLC.pdf](./Manuals/DoC_MicroPLC.pdf) |
| Datasheet | [MicroPLC_Datasheet.pdf](./Manuals/MicroPLC_Datasheet.pdf) |

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
