# HomeMaster OpenTherm Gateway

![Device](./Images/opentherm.png)

**Part No.:** OpenTherm Gateway-R1 · **Hardware Version:** V1.0 · **Manufacturer:** ISYSTEMS AUTOMATION S.R.L.

## Description

The HomeMaster OpenTherm Gateway is an ESP32-based DIN-rail device designed to interface with OpenTherm-compatible boilers.

The device provides a hardware OpenTherm interface together with one relay output and 1-Wire temperature sensor support. It is designed for local operation using ESPHome and integrates directly with Home Assistant.

The device operates as an **OpenTherm master**. It initiates all communication with the boiler. The boiler must be configured as OpenTherm slave (standard for all OT-capable boilers). If no boiler is connected to the OT terminals, the OpenTherm entities will be unavailable but the relay and 1-Wire functions continue to operate normally.

This repository includes the full ESPHome configuration used on shipped devices (including vendor OTA update settings).

| Resource | Link |
|---|---|
| 🛒 Product page | [home-master.eu](https://www.home-master.eu/shop/opentherm-gateway-59) |
| 📁 Repository | [GitHub](https://github.com/isystemsautomation/homemaster-dev/tree/main/OpenthermGateway) |
| 📄 Datasheet (PDF) | [OpenTherm_Datasheet.pdf](https://github.com/isystemsautomation/homemaster-dev/blob/main/OpenthermGateway/Manuals/OpenTherm_Datasheet.pdf) |
| ⚙️ Default Firmware (YAML) | [opentherm.yaml](https://github.com/isystemsautomation/homemaster-dev/blob/main/OpenthermGateway/Firmware/opentherm.yaml) |
| 🔧 Schematics | [Schematic/](https://github.com/isystemsautomation/homemaster-dev/tree/main/OpenthermGateway/Schematic) |
| 🏠 Maker | [home-master.eu](https://www.home-master.eu/) |

## Table of Contents

- [Description](#description)
- [Features](#features)
- [Electrical and Safety Notes](#electrical-and-safety-notes)
- [Mechanical and Environmental](#mechanical-and-environmental)
- [Installation](#installation)
- [Cable Recommendations & Shield Grounding](#cable-recommendations--shield-grounding)
- [Wiring](#wiring)
- [Pinout](#pinout)
- [Terminal Reference](#terminal-reference)
- [LED and Button Behaviour](#led-and-button-behaviour)
- [GPIO Map](#gpio-map)
- [Network Requirements](#network-requirements)
- [First Boot & Wi-Fi Setup](#first-boot--wi-fi-setup)
- [Home Assistant Integration](#home-assistant-integration)
- [Firmware Updates](#firmware-updates)
- [Device Behaviour Reference](#device-behaviour-reference)
- [Troubleshooting](#troubleshooting)
- [Entity Reference](#entity-reference)
- [Default Firmware Configuration](#default-firmware-configuration)
- [Support & Community](#support--community)
- [Compliance & Certifications](#compliance--certifications)
- [License](#license)

## Features

- ESP32-WROOM-32U-N16 (16 MB flash)
- OpenTherm interface (OT+ / OT-)
- Relay output: 1 x SPDT, C and NC contacts accessible, system limit 3 A @ 250 VAC (resistive), 90 W @ 30 VDC
- Two 1-Wire buses
- Power input options: 24 V DC, 85-265 V AC, or 120-370 V DC
- USB Type-C
- Wi-Fi 2.4 GHz (pre-certified radio module) and Bluetooth
- ESPHome pre-installed
- OTA updates (ESPHome + HTTP)
- Improv provisioning
- DIN-rail mounting
- Modular architecture: MCU Board + Field Board

## Electrical and Safety Notes

> ⚠️ **Safety — read before installation:**
> - **L / N terminals carry hazardous mains voltage.** Installation
>   by qualified personnel only.
> - **Use only ONE power input at a time** (24 V DC or AC/DC L/N).
>   Never connect multiple power inputs simultaneously.
> - **Disconnect all power before wiring changes.**
> - Relay output is **not internally fused** — always add an
>   external fuse or circuit breaker on the load circuit.
> - Install inside a closed control cabinet only.
>   Protect all terminals from accidental contact.
> - **24 V DC input** is SELV (Safety Extra-Low Voltage).
> - The 24 V DC input is protected against reverse polarity
>   by a Schottky diode (STPS340U).
> - Follow local electrical code and boiler manufacturer
>   OpenTherm wiring requirements.

## Mechanical and Environmental

- Operating temperature: `0 °C` to `+40 °C`
- Storage temperature: `-10 °C` to `+55 °C`
- Relative humidity: `0–90 % RH`, non-condensing
- Protection rating: `IP20` (inside cabinet)
- Dimensions: `35.5 × 90.6 × 67.3 mm` (L × W × H)
- Mounting: `35 mm DIN rail` (2 DIN modules)
- Pack size: `140 × 96 × 95 mm` (L × W × H)

## Installation

### DIN Rail Mounting
- Mount on 35 mm DIN rail. The device occupies 2 DIN modules (≈ 36 mm width).
- Install only inside a ventilated control cabinet.
- The cabinet must include a protective front plate covering all terminals and a closing protective door.
- Not suitable for outdoor or exposed installation.

### Terminal Wiring
- Terminal type: pluggable screw terminal blocks, 5.08 mm pitch.
- Wire cross-section: 0.2–2.5 mm² (AWG 24–12), solid or stranded copper.
- Use ferrules for stranded wire. Tightening torque: 0.4 Nm maximum.
- All wiring terminals must be protected against accidental contact by an insulating front plate, wiring duct, or terminal cover. **Exposed live terminals are not permitted.**

## Cable Recommendations & Shield Grounding

### General Routing Rules
- Route low-level signal cables (1-Wire / OT) separately from mains, relay output, contactors, and power wiring.
- If crossing power cables is unavoidable, cross at 90°.
- Keep cable runs as short as practical; avoid long parallel runs next to high-current conductors.

### OpenTherm Cable
- Construction: twisted pair.
- Overall shield recommended in cabinets or high-EMI environments.
- Recommended types: `J-Y(ST)Y 2×2×0.5 mm²` or `LI2YCY PiMF 2×2×0.50`.

### 1-Wire Cable
- Recommended: shielded 3-core (+5V / DATA / GND).
- High-EMI or long runs: shielded pairs + overall shield (e.g., `LI2YCY PiMF 2×2×0.50`).
- Topology: **daisy-chain (bus) only** — star wiring is not supported.
- Keep sensor stubs ≤ 0.5 m.
- Maximum total bus length: **100 m** (standard DS18B20 with external power).
  For longer runs reduce pull-up to 2.2 kΩ.
- Maximum recommended sensors per bus: **10** (with correct topology and pull-up).
- DATA pull-up: 4.7 kΩ typical; 2.2–3.3 kΩ for long or heavily loaded buses.

### Shield Grounding
- Bond cable shields to cabinet PE/EMC ground at the controller side only (single-end bonding).
- Do not connect shields directly to signal terminals (1-Wire / OT).
- If both ends are in equipotential-bonded cabinets, both-end bonding is permitted using proper 360° clamps.

## Wiring

### Power Input

| Input | Terminals | Range |
|---|---|---|
| 24 V DC | +V / 0V | 24 V DC nominal |
| AC Mains | L / N | 85–265 V AC |
| Wide DC | L / N | 120–370 V DC |

| 24 V DC Input | 230 V AC Input |
|:---:|:---:|
| ![24V DC wiring](./Images/OpenTherm_24Vdc.png)<br>*Connect +V to positive (24 V DC), 0V to negative. Use only one power input at a time.* | ![230V AC wiring](./Images/OpenTherm_230Vac.png)<br>*Connect L to line (live), N to neutral. Include external fuse on the L conductor.* |
| Connect + to V+, − to 0V | Connect Live to L, Neutral to N |

### OpenTherm Bus Wiring
Connect OT+ and OT− between the gateway and the boiler OpenTherm interface.
Keep OT wiring separated from mains and relay output conductors.

### Relay Output Wiring

> ⚠️ **NC contact behaviour:** When the relay is de-energised (switched OFF
> or on device reboot), the NC contact is **closed** and the load is
> **powered**. If you are wiring a boiler, pump, or valve to the NC contact,
> it will be active by default until the relay is commanded ON.
> Design your installation accordingly and ensure this is safe for your load.

The relay output exposes **C and NC contacts only** (normally-closed).
System load limits: **3 A @ 250 VAC** (resistive) · **750 VA @ 250 VAC** max · **90 W @ 30 VDC** max.

> ⚠️ The relay output is **not internally fused**. Always add an external fuse or circuit breaker. Use an external contactor for loads above 3 A or for inductive / high-inrush loads.

### 1-Wire Sensor Wiring
Two independent 1-Wire channels support DS18B20-compatible temperature sensors.

| OpenTherm Bus | Relay Output | 1-Wire Sensors |
|:---:|:---:|:---:|
| ![OT wiring](./Images/OpenTherm_OTConnection.png)<br>*Connect OT+ and OT− to the boiler OpenTherm terminals. If communication fails, try swapping polarity.* | ![Relay wiring](./Images/OpenTherm_RelayConnection.png)<br>*NC contact is closed when relay is de-energised — load is powered by default. Add external fuse on the load circuit.* | ![1-Wire wiring](./Images/OpenTherm_1WireConnection.png)<br>*Use daisy-chain topology only. Connect +5V, DATA (D1 or D2), and Gnd. Keep stubs ≤ 0.5 m.* |
| Connect OT+ and OT− to boiler | C and NC contacts only | Daisy-chain only · stubs ≤ 0.5 m |

#### 1-Wire Bus Notes

- The default configuration does not define fixed sensor `address`
  values. With no address specified, ESPHome reads the first sensor
  discovered on the bus.
- **For reliable operation: use one sensor per bus** (GPIO4 and GPIO5).
- Connecting multiple sensors on the same bus without addresses
  results in non-deterministic sensor assignment.
- Maximum total bus length: **100 m** (DS18B20 with external power).
- Maximum recommended sensors per bus: **10**
  (with correct topology and pull-up value).
- For multiple sensors on one bus: assign explicit `address` values
  via ESPHome YAML. Addresses are visible in ESPHome logs at boot.
  See [ESPHome Dallas Temperature docs](https://esphome.io/components/sensor/dallas_temp.html).

## Pinout

![Pinout](./Images/pinout.png)

## Terminal Reference

### Top Terminals (Signal)

| Terminal | Signal | Description |
|---|---|---|
| Gnd | Ground | Common ground reference |
| D1 | 1-Wire Bus 1 DATA | DS18B20-compatible, GPIO4 |
| D2 | 1-Wire Bus 2 DATA | DS18B20-compatible, GPIO5 |
| +5V | +5 V output | Auxiliary 5 V supply for 1-Wire sensors (max 50 mA) |
| O+ | OpenTherm + | OpenTherm bus positive |
| O- | OpenTherm − | OpenTherm bus negative |

### Bottom Terminals (Power & Relay)

| Terminal | Signal | Description |
|---|---|---|
| 0V | DC Ground | 24 V DC negative / ground |
| +V | DC Power + | 24 V DC positive input |
| L | AC Line / Wide DC + | AC mains live (85–265 V AC) or DC+ (120–370 V DC) |
| N | AC Neutral / Wide DC − | AC mains neutral or DC− |
| C | Relay Common | Dry-contact relay common |
| NC | Relay NC | Normally closed contact |

> The +5V terminal is an auxiliary output for powering 1-Wire sensors only.
> Do not connect other loads to this terminal.

## LED and Button Behaviour

### LEDs

The device has 4 LEDs on the front panel: **PWR**, **O.1**, **U.1**, **U.2**.
O.1 reflects the relay output state. U.2 is the ESPHome status LED (GPIO33).
U.1 is user-assignable via ESPHome YAML.

| LED | Behaviour | Meaning |
|---|---|---|
| PWR | Solid ON | Device is powered |
| O.1 | Solid ON | Relay is energised |
| U.1 | Firmware-controlled | Configurable via ESPHome YAML |
| U.2 | Solid ON | Normal operation (Wi-Fi + API connected) |
| U.2 | Fast blink | Wi-Fi connecting or API disconnected |
| U.2 | Blink pattern | OTA update in progress |

> U.2 is configured as the ESPHome `status_led` (GPIO33) and its behaviour
> is controlled by ESPHome firmware. U.1 is a user-assignable LED,
> configurable via ESPHome YAML automations.
> LED colours are not documented here — refer to the physical device or BOM.

### Button (GPIO35)
The physical button is exposed as a binary sensor in ESPHome (`button_1`).
Default behaviour: read-only input — pressing it triggers the `button_1`
binary sensor.
You can add automations in ESPHome or Home Assistant to assign actions
(e.g., restart device, toggle relay).

## GPIO Map

All hardware-assigned GPIOs are listed below.
Do not reassign reserved GPIOs in custom ESPHome YAML.

| GPIO | Function | User-configurable |
|---|---|---|
| GPIO4 | 1-Wire Bus 1 (D1 terminal) | No — reserved |
| GPIO5 | 1-Wire Bus 2 (D2 terminal) — strapping pin, pulled HIGH via 10 kΩ at boot | No — reserved |
| GPIO21 | OpenTherm IN (OT−) via optocoupler | No — reserved |
| GPIO26 | OpenTherm OUT (OT+) via optocoupler | No — reserved |
| GPIO25 | User LED U.1 | Yes — add to ESPHome YAML as output |
| GPIO32 | Relay output | No — reserved |
| GPIO33 | Status LED U.2 (inverted) — ESPHome status_led | No — reserved |
| GPIO35 | Button input (inverted, input only) | No — reserved |

## Network Requirements

- Device and Home Assistant must be on the **same subnet**.
- **mDNS** must be functional on the network for auto-discovery.
  In VLAN setups, configure an mDNS repeater or use a static IP
  assigned via ESPHome YAML.
- ESPHome API uses **TCP port 6053**. Ensure this port is not blocked
  by firewall rules between the device and Home Assistant.
- Vendor-managed OTA updates require outbound **HTTPS (port 443)**
  access to GitHub Pages from the device.

## First Boot & Wi-Fi Setup

The device supports two setup methods:

- **Improv Wi-Fi (recommended)**
- **Fallback Access Point (HomeMaster OT Fallback)**

### Improv Wi-Fi Setup (Recommended)

1. Power on the device
2. Open https://improv-wifi.com
3. Connect via USB or Bluetooth
4. Enter Wi-Fi credentials
5. Wait for connection

After connection, the device will appear automatically in:

- ESPHome Dashboard
- Home Assistant

Click **Take Control** to import the full configuration.

### Fallback Access Point (HomeMaster OT Fallback)

If the device cannot connect to Wi-Fi, it starts a fallback Access Point.

**SSID:** `HomeMaster OT Fallback`

#### Steps

1. Power on the device and wait approximately 60 seconds
2. Connect to: HomeMaster OT Fallback
3. Open a browser and navigate to: http://192.168.4.1
4. Enter your Wi-Fi credentials and save

The device will restart and connect to your network.

### Notes

- The captive portal page may open automatically. If it does not, open `http://192.168.4.1` manually.
- Mobile devices may continue using mobile data; disable it if the page does not load.
- The fallback Access Point is only active when the device cannot connect to Wi-Fi.
- Improv Wi-Fi is the preferred setup method.

## Home Assistant Integration

After Wi-Fi provisioning, the device appears automatically in:
- **ESPHome Dashboard** — for configuration and logs
- **Home Assistant** — under Settings → Devices & Services → ESPHome

Click **Take Control** in ESPHome Dashboard to import the full
configuration and manage firmware yourself.

### ⚠️ Note on Taking Control
After taking control, vendor-managed OTA updates stop working
unless you keep the `http_request`, `ota: platform: http_request`,
and `update` blocks from the original configuration in your YAML.

If you remove these blocks, update via ESPHome OTA or USB instead.

### ESPHome Compatibility
- Minimum ESPHome version used and tested: **2026.4.1**

## Firmware Updates

The device supports two firmware update methods:

### ESPHome Updates (User-controlled)

After taking control in ESPHome Dashboard, firmware can be updated manually:

- Build new firmware from ESPHome
- Upload via OTA or USB
- Full control over configuration

### Managed Updates (HTTP)

The device also supports vendor-provided firmware updates.

A firmware update entity is exposed in Home Assistant, allowing the device to check for new firmware versions and install updates directly.

This mechanism uses the `update.http_request` component with a hosted firmware manifest,
downloading updates over HTTPS directly to the device.

If a newer firmware version is available, it can be installed directly from Home Assistant.

> ⚠️ **OTA safety:** Do not interrupt a firmware update once started.
> If an OTA update is interrupted mid-flash, the device may fail to boot.
> If this occurs, reflash via USB-C using ESPHome or the ESP flashing tool.
> ESPHome safe mode is active for the first 10 boot attempts after a
> failed OTA — connect via USB and reflash to recover.

## Troubleshooting

### Device does not appear in Home Assistant or ESPHome Dashboard
- Confirm the device is powered (PWR LED solid ON).
- Confirm Wi-Fi provisioning completed successfully.
- Check that Home Assistant and the device are on the same network/VLAN.
- If U.2 LED is fast-blinking, the device is in Wi-Fi connect mode —
  wait up to 60 seconds.
- If Wi-Fi fails, the device starts the fallback AP
  `HomeMaster OT Fallback` — reconnect and re-enter credentials.

### No OpenTherm communication (all OT entities unavailable)
- Verify O+ and O− wiring. OpenTherm is polarity-sensitive on some boilers.
- Confirm the boiler has an OpenTherm interface enabled in boiler settings.
- Check for short circuits or incorrect voltage on the OT terminals.
- Review ESPHome logs for OpenTherm timeout or CRC errors.

### 1-Wire sensor shows unknown or no value
- Confirm sensor is wired correctly: +5V, DATA (D1 or D2), Gnd.
- If using multiple sensors on one bus, see the section below on
  multiple DS18B20 sensors.
- Keep stubs ≤ 0.5 m and use daisy-chain topology only.
- For long buses consider reducing pull-up resistor to 2.2–3.3 kΩ.

### Relay does not switch
- Check the `Relay` switch entity is enabled in Home Assistant.
- Verify external wiring on C / NC terminals.
- Confirm external fuse or breaker is not tripped.

### Firmware update fails
- Confirm the device has a working internet connection.
- Check update source is reachable:
  `https://isystemsautomation.github.io/homemaster-dev/OpenthermGateway/Firmware/manifest.json`
- If you took control in ESPHome and removed the `http_request` / `update`
  blocks, vendor OTA is no longer available — update via ESPHome OTA instead.

### Recovery & Troubleshooting Access

#### Wi-Fi credentials changed or forgotten
The device starts the fallback AP **HomeMaster OT Fallback**
automatically after ~60 seconds of failed Wi-Fi connection.

1. Wait 60 seconds after powering on
2. Connect to Wi-Fi: **HomeMaster OT Fallback**
3. Navigate to `http://192.168.4.1`
4. Enter new credentials and save

> On mobile: disable mobile data if the page does not load.

#### Full reset — no OTA, device unreachable
Reflash via USB. The default firmware has no factory reset button.

**Power:** USB can be connected with or without external power.
The VBUS line is Schottky-diode protected — no backfeed.
Disconnecting USB does not reboot the device if external power
is present.

**Driver:** Device uses **CP2102N** (Silicon Labs).
Windows may need driver from:
`silabs.com/developers/usb-to-uart-bridge-vcp-drivers`
macOS/Linux: auto-detected.

**Flash options:**
- Web Flasher: `https://web.esphome.io`
- ESPHome Dashboard → Install → Plug into this computer


> Boot mode is handled automatically by the CP2102N auto-reset
> circuit. No button press required.

#### Verifying OpenTherm without Home Assistant
Open ESPHome Dashboard → your device → **Logs**.
Look for `[opentherm]` lines:

| Message | Meaning |
|---|---|
| `Received response` | Boiler responding correctly |
| `Timeout waiting for response` | Check OT wiring |
| `Invalid response` | Try swapping OT+ / OT− |

For a browser interface, add `web_server: port: 80` to your YAML
after taking control, then open `http://<device_ip>`.

### Device Behaviour Reference

| Condition | CH Enable | DHW Enable | Relay | OT Bus |
|---|---|---|---|---|
| Normal operation | Controlled by HA | Controlled by HA | Controlled by HA | Active polling |
| Wi-Fi lost | Holds last state | Holds last state | Holds last state | Continues polling |
| HA disconnected | Holds last state | Holds last state | Holds last state | Continues polling |
| ESP reboot | Restores ON | Restores ON | Restores OFF (NC closes) | Restarts polling |
| OT communication failure | Remains ON | Remains ON | Unchanged | Retries |
| No boiler connected to OT | CH/DHW entities unavailable | CH/DHW entities unavailable | Unaffected | No response |

> ⚠️ After reboot CH Enable and DHW Enable restore to ON
> (`restore_mode: RESTORE_DEFAULT_ON`). The relay restores to OFF —
> NC contact closes and the load is powered.
> Verify this is safe for your installation before deploying.

## Entity Reference

<details>
<summary>Click to expand full entity reference table</summary>

| Entity | Type | Default | Description |
|---|---|---|---|
| Button | Binary Sensor | Enabled | Physical button (GPIO35) |
| ESP Status | Binary Sensor | Enabled | Wi-Fi / API connection status |
| Relay | Switch | Enabled | Dry-contact relay output (GPIO32) |
| Boiler CH Enable | Switch | Enabled | Enable central heating |
| Boiler DHW Enable | Switch | Enabled | Enable domestic hot water |
| Boiler CH Setpoint | Number | Enabled | CH flow setpoint 20–80 °C |
| Boiler DHW Setpoint | Number | Enabled | DHW setpoint 35–65 °C |
| Boiler Water Temperature | Sensor | Enabled | Boiler flow temperature |
| Boiler Relative Modulation Level | Sensor | Enabled | Burner modulation % |
| Boiler Flame On | Binary Sensor | Enabled | Flame active |
| Boiler CH Active | Binary Sensor | Enabled | CH mode active |
| Boiler DHW Active | Binary Sensor | Enabled | DHW mode active |
| Boiler Fault Indication | Binary Sensor | Enabled (diagnostic) | Boiler fault flag |
| Boiler Service Request | Binary Sensor | Enabled (diagnostic) | Service due |
| Boiler Lockout Reset | Binary Sensor | Enabled (diagnostic) | Lockout reset flag |
| Boiler Low Water Pressure | Binary Sensor | Enabled (diagnostic) | Low pressure fault |
| Boiler Flame Fault | Binary Sensor | Enabled (diagnostic) | Flame sensor fault |
| Boiler Air Pressure Fault | Binary Sensor | Enabled (diagnostic) | Air pressure fault |
| Boiler Water Overtemperature | Binary Sensor | Enabled (diagnostic) | Overtemperature fault |
| Boiler DHW Setpoint Transfer Enabled | Binary Sensor | Enabled (diagnostic) | DHW setpoint transfer capability |
| Boiler Max CH Setpoint Transfer Enabled | Binary Sensor | Enabled (diagnostic) | Max CH setpoint transfer capability |
| Boiler DHW Setpoint RW | Binary Sensor | Enabled (diagnostic) | DHW setpoint read/write capability |
| Boiler Max CH Setpoint RW | Binary Sensor | Enabled (diagnostic) | Max CH setpoint read/write capability |
| 1-Wire Bus 1 Temperature | Sensor | Enabled | GPIO4 temperature sensor |
| 1-Wire Bus 2 Temperature | Sensor | Enabled | GPIO5 temperature sensor |
| Firmware Update | Update | Enabled | Vendor OTA update entity |
| WiFi Signal | Sensor | Enabled (diagnostic) | RSSI in dBm |
| ESP IP Address | Text Sensor | Enabled (diagnostic) | Device IP address |
| ESPHome Version | Text Sensor | Enabled (diagnostic) | Running ESPHome version |
| ESP Uptime Human | Text Sensor | Enabled (diagnostic) | Human-readable uptime |
| ESP32 Temperature | Sensor | Enabled (diagnostic) | Internal chip temperature |
| Boiler Return Temperature | Sensor | **Disabled** | Requires boiler support |
| Boiler DHW Temperature | Sensor | **Disabled** | Requires boiler support |
| Boiler Outside Temperature | Sensor | **Disabled** | Requires boiler support |
| Boiler CH Pressure | Sensor | **Disabled** | Requires boiler support |
| Boiler DHW Flow Rate | Sensor | **Disabled** | Requires boiler support |
| Boiler Storage Temperature | Sensor | **Disabled** | Requires boiler support |
| Boiler Collector Temperature | Sensor | **Disabled** | Requires boiler support |
| Boiler CH2 Flow Temperature | Sensor | **Disabled** | Requires boiler support |
| Boiler DHW2 Temperature | Sensor | **Disabled** | Requires boiler support |
| Boiler Exhaust Temperature | Sensor | **Disabled** | Requires boiler support |
| Boiler Max CH Setpoint | Number | **Disabled** | Requires boiler support |
| Boiler Max Relative Modulation | Number | **Disabled** | Requires boiler support |
| Boiler OTC Heat Curve Ratio | Number | **Disabled** | Requires boiler support |
| Boiler Cooling Enable | Switch | **Disabled** | Requires boiler support |
| Boiler OTC Active | Switch | **Disabled** | Requires boiler support |
| Boiler CH2 Active | Switch | **Disabled** | Requires boiler support |
| Boiler Summer Mode Active | Switch | **Disabled** | Requires boiler support |
| Boiler DHW Block | Switch | **Disabled** | Requires boiler support |
| Boiler Diagnostic Indication | Binary Sensor | **Disabled** | Extended diagnostic |

</details>

## Default Firmware Configuration

The full shipped configuration is available in the repository:
[opentherm.yaml](https://github.com/isystemsautomation/homemaster-dev/blob/main/OpenthermGateway/Firmware/opentherm.yaml)

<details>
<summary>Click to expand full ESPHome configuration</summary>

```yaml
esphome:
  name: homemaster-opentherm
  name_add_mac_suffix: true
  friendly_name: HomeMaster OpenTherm Gateway
  project:
    name: homemaster.opentherm_gateway
    version: "1.0.6"

esp32:
  variant: esp32
  board: esp32dev
  flash_size: 16MB
  framework:
    type: esp-idf

logger:

api:

wifi:
  ap:
    ssid: "HomeMaster OT Fallback"
  on_connect:
    then:
      - delay: 10s
      - component.update: firmware_update

captive_portal:

esp32_improv:
  authorizer: none

improv_serial:

dashboard_import:
  package_import_url: github://isystemsautomation/homemaster-dev/OpenthermGateway/Firmware/opentherm.yaml@main
  import_full_config: true

http_request:

ota:
  - platform: esphome
  - platform: http_request

update:
  - platform: http_request
    id: firmware_update
    name: "Firmware Update"
    source: https://isystemsautomation.github.io/homemaster-dev/OpenthermGateway/Firmware/manifest.json
    update_interval: 6h

opentherm:
  id: ot_bus
  in_pin: GPIO21
  out_pin: GPIO26

binary_sensor:
  - platform: status
    id: esp_status
    name: "ESP Status"
    entity_category: diagnostic

  - platform: gpio
    id: button_1
    name: "Button"
    pin:
      number: GPIO35
      inverted: true
      mode:
        input: true

  - platform: opentherm
    # Core (minimum) set: IDs 0, 5, 6.
    fault_indication:
      id: ot_fault_indication
      name: "Boiler Fault Indication"
      entity_category: diagnostic
    flame_on:
      id: ot_flame_on
      name: "Boiler Flame On"
    ch_active:
      id: ot_ch_active
      name: "Boiler CH Active"
    dhw_active:
      id: ot_dhw_active
      name: "Boiler DHW Active"
    service_request:
      id: ot_service_request
      name: "Boiler Service Request"
      entity_category: diagnostic
    lockout_reset:
      id: ot_lockout_reset
      name: "Boiler Lockout Reset"
      entity_category: diagnostic
    low_water_pressure:
      id: ot_low_water_pressure
      name: "Boiler Low Water Pressure"
      entity_category: diagnostic
    flame_fault:
      id: ot_flame_fault
      name: "Boiler Flame Fault"
      entity_category: diagnostic
    air_pressure_fault:
      id: ot_air_pressure_fault
      name: "Boiler Air Pressure Fault"
      entity_category: diagnostic
    water_over_temp:
      id: ot_water_over_temp
      name: "Boiler Water Overtemperature"
      entity_category: diagnostic
    dhw_setpoint_transfer_enabled:
      id: ot_dhw_setpoint_transfer_enabled
      name: "Boiler DHW Setpoint Transfer Enabled"
      entity_category: diagnostic
    max_ch_setpoint_transfer_enabled:
      id: ot_max_ch_setpoint_transfer_enabled
      name: "Boiler Max CH Setpoint Transfer Enabled"
      entity_category: diagnostic
    dhw_setpoint_rw:
      id: ot_dhw_setpoint_rw
      name: "Boiler DHW Setpoint RW"
      entity_category: diagnostic
    max_ch_setpoint_rw:
      id: ot_max_ch_setpoint_rw
      name: "Boiler Max CH Setpoint RW"
      entity_category: diagnostic

    # Extended set (model-dependent). Disabled by default.
    diagnostic_indication:
      id: ot_diagnostic_indication
      name: "Boiler Diagnostic Indication"
      entity_category: diagnostic
      disabled_by_default: true

one_wire:
  - platform: gpio
    id: ow_bus_1
    pin: GPIO4

  - platform: gpio
    id: ow_bus_2
    pin: GPIO5

sensor:
  - platform: uptime
    id: esp_uptime
    internal: true
    update_interval: 60s

  - platform: wifi_signal
    id: wifi_signal_db
    name: "WiFi Signal"
    update_interval: 60s
    entity_category: diagnostic

  - platform: internal_temperature
    id: esp32_temperature
    name: "ESP32 Temperature"
    update_interval: 60s
    entity_category: diagnostic

  - platform: opentherm
    # Core (minimum) set: IDs 17, 24.
    t_boiler:
      id: ot_t_boiler
      name: "Boiler Water Temperature"
      unit_of_measurement: "°C"
    rel_mod_level:
      id: ot_rel_mod_level
      name: "Boiler Relative Modulation Level"
      unit_of_measurement: "%"

    # Extended set (model-dependent). Disabled by default.
    t_ret:
      id: ot_t_ret
      name: "Boiler Return Temperature"
      unit_of_measurement: "°C"
      disabled_by_default: true
    t_dhw:
      id: ot_t_dhw
      name: "Boiler DHW Temperature"
      unit_of_measurement: "°C"
      disabled_by_default: true
    t_outside:
      id: ot_t_outside
      name: "Boiler Outside Temperature"
      unit_of_measurement: "°C"
      disabled_by_default: true
    ch_pressure:
      id: ot_ch_pressure
      name: "Boiler CH Pressure"
      unit_of_measurement: "bar"
      disabled_by_default: true
    dhw_flow_rate:
      id: ot_dhw_flow_rate
      name: "Boiler DHW Flow Rate"
      unit_of_measurement: "l/min"
      disabled_by_default: true
    t_storage:
      id: ot_t_storage
      name: "Boiler Storage Temperature"
      unit_of_measurement: "°C"
      disabled_by_default: true
    t_collector:
      id: ot_t_collector
      name: "Boiler Collector Temperature"
      unit_of_measurement: "°C"
      disabled_by_default: true
    t_flow_ch2:
      id: ot_t_flow_ch2
      name: "Boiler CH2 Flow Temperature"
      unit_of_measurement: "°C"
      disabled_by_default: true
    t_dhw2:
      id: ot_t_dhw2
      name: "Boiler DHW2 Temperature"
      unit_of_measurement: "°C"
      disabled_by_default: true
    t_exhaust:
      id: ot_t_exhaust
      name: "Boiler Exhaust Temperature"
      unit_of_measurement: "°C"
      disabled_by_default: true

  - platform: dallas_temp
    id: ow_bus_1_temperature
    one_wire_id: ow_bus_1
    name: "1-Wire Bus 1 Temperature"
    unit_of_measurement: "°C"

  - platform: dallas_temp
    id: ow_bus_2_temperature
    one_wire_id: ow_bus_2
    name: "1-Wire Bus 2 Temperature"
    unit_of_measurement: "°C"

switch:
  - platform: opentherm
    # Core control (ID 0).
    ch_enable:
      id: ot_ch_enable
      name: "Boiler CH Enable"
      restore_mode: RESTORE_DEFAULT_ON
    dhw_enable:
      id: ot_dhw_enable
      name: "Boiler DHW Enable"
      restore_mode: RESTORE_DEFAULT_ON
    # Extended control (model-dependent). Disabled by default.
    cooling_enable:
      id: ot_cooling_enable
      name: "Boiler Cooling Enable"
      disabled_by_default: true
    otc_active:
      id: ot_otc_active
      name: "Boiler OTC Active"
      disabled_by_default: true
    ch2_active:
      id: ot_ch2_active
      name: "Boiler CH2 Active"
      disabled_by_default: true
    summer_mode_active:
      id: ot_summer_mode_active
      name: "Boiler Summer Mode Active"
      disabled_by_default: true
    dhw_block:
      id: ot_dhw_block
      name: "Boiler DHW Block"
      disabled_by_default: true

  - platform: gpio
    id: relay_1
    name: "Relay"
    pin: GPIO32

number:
  - platform: opentherm
    # Core (minimum) set: IDs 1, 56.
    t_set:
      id: ot_t_set
      name: "Boiler CH Setpoint"
      min_value: 20
      max_value: 80
      step: 1
    t_dhw_set:
      id: ot_t_dhw_set
      name: "Boiler DHW Setpoint"
      min_value: 35
      max_value: 65
      step: 1

    # Extended controls (model-dependent). Disabled by default.
    max_t_set:
      id: ot_max_t_set
      name: "Boiler Max CH Setpoint"
      min_value: 30
      max_value: 85
      step: 1
      disabled_by_default: true
    max_rel_mod_level:
      id: ot_max_rel_mod_level
      name: "Boiler Max Relative Modulation Level"
      min_value: 0
      max_value: 100
      step: 1
      disabled_by_default: true
    otc_hc_ratio:
      id: ot_otc_hc_ratio
      name: "Boiler OTC Heat Curve Ratio"
      min_value: 0
      max_value: 127
      step: 1
      disabled_by_default: true

text_sensor:
  - platform: template
    id: esp_uptime_human
    name: "ESP Uptime Human"
    entity_category: diagnostic
    update_interval: 60s
    lambda: |-
      if (isnan(id(esp_uptime).state)) {
        return {};
      }
      int total_seconds = (int) id(esp_uptime).state;
      int days = total_seconds / 86400;
      int hours = (total_seconds % 86400) / 3600;
      if (days > 0) {
        return {to_string(days) + "d " + to_string(hours) + "h"};
      }
      int minutes = (total_seconds % 3600) / 60;
      if (hours > 0) {
        return {to_string(hours) + "h " + to_string(minutes) + "m"};
      }
      return {to_string(minutes) + "m"};

  - platform: version
    name: "ESPHome Version"
    entity_category: diagnostic

  - platform: wifi_info
    ip_address:
      name: "ESP IP Address"
      entity_category: diagnostic

status_led:
  pin:
    number: GPIO33
    inverted: true
```

</details>

## Support & Community

| Channel | Link |
|---|---|
| 🛠️ Official Support | [home-master.eu/support](https://www.home-master.eu/support) |
| 📺 YouTube | [youtube.com/@HomeMaster](https://youtube.com/@HomeMaster) |
| 🛡️ Reddit | [reddit.com/r/HomeMaster](https://reddit.com/r/HomeMaster) |
| 📷 Instagram | [instagram.com/home_master.eu](https://instagram.com/home_master.eu) |
| 🔬 Hackster | [hackster.io/homemaster](https://hackster.io/homemaster) |
| 🐙 GitHub | [isystemsautomation](https://github.com/isystemsautomation/homemaster-dev) |

## Compliance & Certifications

CE marked · **EMC** 2014/30/EU · **LVD** 2014/35/EU · **RED** 2014/53/EU · **RoHS** 2011/65/EU · **EN 62368-1** · Full Declaration of Conformity available on request from ISYSTEMS AUTOMATION S.R.L.

### Radio
The product integrates a pre-certified ESP32 Wi-Fi radio module (2.4 GHz).
Conformity with RED 2014/53/EU is demonstrated by maintained technical
documentation and conformity assessment of the complete device.

### Safety Notice
- **L / N terminals** carry hazardous mains voltage — qualified personnel only.
- **24 V DC input** is SELV (Safety Extra-Low Voltage).

## License

This project uses a hybrid licensing model.

### Hardware
Hardware designs (schematics, PCB layouts, BOMs) are licensed under **CERN-OHL-W v2**.

### Firmware & ESPHome Integration
All firmware, ESPHome configurations, and software components are licensed under the **MIT License**.

This ensures full compatibility with ESPHome and Home Assistant while protecting hardware designs.
See LICENSE files in each directory for full terms.
