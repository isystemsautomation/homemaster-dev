# HomeMaster Thermostat for OpenTherm

<!-- Restore this badge after the Made for ESPHome application is approved.
<a href="https://devices.esphome.io/devices/homemaster-openthermthermostat/"><img src="https://media.esphome.io/made-for-esphome/made-for-esphome-black-on-white.svg" alt="Made for ESPHome" height="28"></a>
-->

![Device](https://config.home-master.eu/OpenthermThermostat/Images/thermostat-front.jpg)

**Part No.:** OTT-R1 · **Hardware Version:** V1.0 · **Manufacturer:** ISYSTEMS AUTOMATION S.R.L.

## Description

The HomeMaster Thermostat for OpenTherm is a wall-mounted ESP32 **OpenTherm master** with a 2.8″ colour display, a rotary encoder, four keys and a 24 GHz presence radar. It regulates a modulating boiler directly over OpenTherm — weather-compensated flow temperature, room setpoint with hysteresis, comfort/eco schedule — and reports everything to Home Assistant through the native ESPHome API.

The device initiates all OpenTherm communication with the boiler (the boiler must be an OpenTherm slave — standard on every OT-capable boiler). Heating control runs on the device itself: if Home Assistant or the network is unavailable, the room keeps being heated to the stored schedule. The display, the encoder and the keys continue to work offline.

This repository includes the full ESPHome configuration used on shipped devices (including vendor OTA update settings).

## Key advantages

- ESP32 **OpenTherm master** with an on-board room sensor — a complete thermostat, not a remote display for someone else's controller.
- **Control loop runs locally.** No Home Assistant, no cloud, no internet required for heating.
- **2.8″ 320×240 colour display**, rotary encoder and four keys — room temperature, setpoint, boiler modulation, flow and system pressure are readable and adjustable from the wall, with no phone involved.
- **24 GHz presence radar** wakes the screen on approach and blanks it when the room is empty.
- **ESPHome pre-installed**; native Home Assistant API — no MQTT, no register mapping.
- **Improv** Wi-Fi onboarding over Bluetooth and USB serial.
- Open hardware (**CERN-OHL-W v2**) and firmware (**MIT**) — no vendor lock-in.

## Quick Start

1. Fix the back plate to the wall and clip the body onto it.
2. Connect 24 V DC to the +V / 0V terminals.
3. Wire OT+ and OT− to the boiler's OpenTherm terminals.
4. Power on and open https://improv-wifi.com in Chrome or Edge — provision Wi-Fi over **Bluetooth**, or over **USB serial** with a USB-C cable. Either route works; use USB when the machine or the browser has no Bluetooth.
5. Within **15 minutes** of power-on, open ESPHome Device Builder → **Take control** to import the configuration and establish the API encryption key. Then add the device in Home Assistant (Settings → Devices & Services → ESPHome).

| Resource | Link |
|---|---|
| 🛒 Product page | [home-master.eu](https://www.home-master.eu/) |
| 📁 Repository | [GitHub](https://github.com/isystemsautomation/homemaster-dev/tree/site/OpenthermThermostat) |
| ⚙️ Default Firmware (YAML) | [thermostat.yaml](https://github.com/isystemsautomation/homemaster-dev/blob/main/OpenthermThermostat/Firmware/thermostat.yaml) |
| 🔧 Schematics | [Schematics/](https://github.com/isystemsautomation/homemaster-dev/tree/site/OpenthermThermostat/Schematics) |
| 🏠 Maker | [home-master.eu](https://www.home-master.eu/) |

## Table of Contents

- [Description](#description)
- [Key advantages](#key-advantages)
- [Quick Start](#quick-start)
- [Features](#features)
- [Electrical and Safety Notes](#electrical-and-safety-notes)
- [Mechanical and Environmental](#mechanical-and-environmental)
- [Installation](#installation)
- [Wiring](#wiring)
- [GPIO Map](#gpio-map)
- [User Interface](#user-interface)
- [Network Requirements](#network-requirements)
- [First Boot & Wi-Fi Setup](#first-boot--wi-fi-setup)
- [Home Assistant Integration](#home-assistant-integration)
- [Firmware Updates](#firmware-updates)
- [Troubleshooting](#troubleshooting)
- [Compliance & Certifications](#compliance--certifications)
- [Photographs](#photographs)
- [License](#license)

## Features

- ESP32-WROOM-32U (16 MB flash, external antenna connector)
- OpenTherm master interface (OT+ / OT−)
- 2.8″ IPS colour display, 320 × 240, ST7789V over SPI
- Rotary encoder with four perimeter keys
- HLK-LD2410C 24 GHz presence radar, UART plus instant GPIO output
- SHT40 temperature and humidity sensor
- BH1750 ambient light sensor
- Power input: 24 V DC
- USB Type-C for serial flashing and Improv provisioning
- Wi-Fi 2.4 GHz (pre-certified radio module) and Bluetooth
- ESPHome pre-installed
- OTA updates (ESPHome + HTTP)
- Improv provisioning over BLE and serial
- Wall mounting on a separate back plate
- Modular architecture: MCU Board + Display Board

## Electrical and Safety Notes

> ⚠️ **Safety — read before installation:**
> - The device is supplied from **24 V DC only**. Do not connect mains voltage to any terminal.
> - **Disconnect power before wiring changes.**
> - The OpenTherm bus is a low-voltage, polarity-insensitive two-wire interface. Do not run it in the same bundle as mains cabling.
> - Installation by qualified personnel, in accordance with local wiring regulations.

## Mechanical and Environmental

Enclosure: Italtronic EYE BOX, ABS, white (RAL 9003), wall mounted.

| Dimension | Value | Italtronic part |
|---|---|---|
| Cover plate, visible | 151.7 × 106.7 mm | P61610003A |
| Body outer diameter | Ø 94.6 mm | P61610002G knurled ring |
| Front aperture (visible glass) | Ø 82.2 mm | P61610002G |
| Ring height | 20.5 mm | P61610002G |
| Wall base, round | Ø 88.7 mm, 10.5 mm deep, keyhole centres 58 mm | P61610002B |
| Wall plate, rectangular | 140 × 80 mm (149.9 mm over the tabs), 9 mm thick, cable aperture 45 × 30 mm, fixing centres 83.5 / 60 mm | P61610014A |
| Toothed ring | Ø 89.7 mm | P61610003G |
| PCB tray | 64 × 45 mm, 8.5 mm deep | P61610003V |

- Overall depth from the wall: 41 mm
- Two-part construction: wall base or plate fixed to the wall, body clipped on
- Front: round glass with a printed brand arc and a rectangular display aperture
- Mounting: two screws straight into the wall at 58 mm centres, or onto a flush-mounting box — the plate is drilled for both **60 mm** (round European device box, Ø 68 mm bore) and **83.5 mm** (Italian three-module box, 503). What decides whether a box fits is the distance between its device screws, not the shape of the box
- Indoor use only, dry locations
- Operating temperature: 0 … 40 °C, non-condensing

> **Note on room temperature accuracy.** The display backlight and the ESP32 raise the temperature inside the enclosure. The SHT40 is mounted on a thermally isolated tab at the bottom of the board, below all heat sources, and the firmware exposes both the raw and the corrected reading so the offset can be verified on site.

## Installation

1. Choose a position about 1.5 m above the floor, on an internal wall, away from direct sunlight, draughts, radiators and door openings.
2. Fix the round base with two screws at 58 mm centres, or screw the rectangular plate onto a flush box — fixing holes at 60 mm and 83.5 mm.
3. Bring the 24 V DC supply and the OpenTherm pair through the opening in the back plate.
4. Terminate the wires, clip the body onto the back plate.

The radar looks straight out of the front face. Keep the area in front of the device clear of large metal surfaces; they reflect 24 GHz and cause false presence.

## Wiring

### Power input

| Terminal | Function |
|---|---|
| +V | 24 V DC |
| 0V | 0 V |

> ⚠️ **No mains power supply is included.** The thermostat takes 24 V DC SELV, 50–100 mA, 2.4 W maximum, and that supply is the installer's to provide — a DIN-rail unit in the distribution board or the boiler cabinet, or a module rated for installation inside a deep flush box. Mains voltage must not reach any terminal on the device. The 24 V pair and the OpenTherm pair are both SELV and may share one four-core cable or run separately; neither may be bundled with mains cabling.

### OpenTherm bus

| Terminal | Function |
|---|---|
| OT+ | OpenTherm, boiler terminal 1 |
| OT− | OpenTherm, boiler terminal 2 |

The OpenTherm interface is polarity-insensitive. Use a twisted pair; do not exceed 50 m.

## GPIO Map

| Function | GPIO |
|---|---|
| OpenTherm in / out | 21 / 26 |
| Display SCK / MOSI | 5 / 23 |
| Display CS / DC / RST | 17 / 19 / 13 |
| Display backlight PWM | 25 |
| I²C SDA / SCL | 18 / 22 |
| Radar UART TX / RX | 14 / 16 |
| Radar presence output | 4 |
| Encoder A / B | 32 / 33 |
| Key ▶ / ▲ / ▼ / ◀ | 34 / 35 / 36 / 39 |

GPIO5 is a strapping pin, used here as the display clock. It is driven only after boot and carries no external pull resistor.

## User Interface

The thermostat is operated entirely from the glass: a 2.8″ colour screen, a
rotary encoder and four keys. Everything on the screen is also available in
Home Assistant, but nothing on the screen depends on Home Assistant being
connected — the device keeps controlling the boiler on its own.

The screen is 320 × 240 pixels in landscape. It redraws every two seconds, and
again immediately after any key press, any encoder detent and every control
pass.

### Backlight and presence

The backlight comes up to 85 % on any key press or encoder movement. It turns
off 45 seconds after the presence radar stops seeing anyone in the room; while
the radar still sees someone, it stays on. When the screen blanks, the
interface also returns to the Climate page with the menu closed, so the unit is
always in the same state when you walk up to it.

The backlight is driven at 25 kHz, above the audible range. At lower
frequencies the ceramic capacitors on the 5 V rail sing.

### The header

The top strip is the same on every page.

| Position | Shows |
|---|---|
| Left | Page name — `CLIMATE`, `BOILER`, `SCHEDULE`, `SYSTEM` |
| After the name | Four square dots, one per page; the current page is white, the others dim |
| Right | Clock, `HH:MM`. `--:--` until the clock is set |
| Left of the clock | Wi-Fi status |
| Left of that | Boiler status |

The clock is taken from Home Assistant when it is connected and from an
internet time server otherwise.

### Icons

These are the only icons the interface uses. They are drawn in colour, and the
colour carries as much meaning as the shape: **amber means active or wrong**,
white means a normal reading, dim grey means idle or unavailable.

| Icon | Meaning | Colour |
|---|---|---|
| <img src="https://config.home-master.eu/OpenthermThermostat/Images/icons/wifi.png" width="40"> | Wi-Fi is associated | Normal grey when Home Assistant is also connected, dim when it is not |
| <img src="https://config.home-master.eu/OpenthermThermostat/Images/icons/wifi-off.png" width="40"> | No Wi-Fi connection | Dim |
| <img src="https://config.home-master.eu/OpenthermThermostat/Images/icons/water-boiler.png" width="40"> | In the header: the boiler reports a fault. In a tile: boiler water or hot-water temperature | Amber on fault or while that circuit is active, dim otherwise |
| <img src="https://config.home-master.eu/OpenthermThermostat/Images/icons/fire.png" width="40"> | Burner. The value is the relative modulation level, or `ON` / `IDLE` when the boiler does not report modulation | Amber while the flame is on, dim when it is out |
| <img src="https://config.home-master.eu/OpenthermThermostat/Images/icons/thermometer.png" width="40"> | Outside temperature on the Climate page, return temperature on the Boiler page | White |
| <img src="https://config.home-master.eu/OpenthermThermostat/Images/icons/gauge.png" width="40"> | Central-heating water pressure, in bar | Amber when the boiler reports low pressure, white otherwise |
| <img src="https://config.home-master.eu/OpenthermThermostat/Images/icons/alert.png" width="40"> | Boiler fault summary — `FAULT` or `OK` | Amber on fault, dim when clear |

A reading the boiler has not sent, or a sensor that is absent, is shown as
`--` or `--.-` in dim grey rather than as a zero.

### Navigation

Four keys and the encoder. The keys do the same thing on every page.

| Control | Menu closed | Menu open |
|---|---|---|
| **Left** | Previous page; wraps around | Closes the menu |
| **Right** | Next page; wraps around | Closes the menu |
| **Up** | Opens the menu on the first item | Moves the selection up; wraps |
| **Down** | Opens the menu on the first item | Moves the selection down; wraps |
| **Encoder** | On Climate: changes the setpoint directly, in 0.5 °C steps. On the other pages: nothing | Changes the selected item |

The selected menu item is drawn as a dim band with a blue bar down its left
edge. Changes take effect as you turn — there is nothing to confirm, and
nothing to save.

### Pages

#### Climate

<img src="https://config.home-master.eu/OpenthermThermostat/Images/screens/screen-climate.png?v=3" alt="Climate page" width="420">

The page you use day to day.

- Room temperature, large, to one decimal.
- The setpoint currently in force below it, in cyan.
- To its right, which setpoint that is: `COMFORT`, `ECO` or `BOOST`.
- Four tiles: burner, boiler water temperature, outside temperature, system
  pressure.

Turning the encoder here changes the setpoint of whichever window is active —
comfort during the comfort window, eco outside it.

**Menu**

| Item | Does |
|---|---|
| `COMFORT SETPOINT` / `ECO SETPOINT` | The active setpoint, 0.5 °C per detent |
| `HEATING` | `AUTO` or `MANUAL`. Manual suspends the control loop and leaves the boiler as it stands |
| `BOOST` | Comfort plus the boost offset until the next scheduled change |
| `HYSTERESIS` | Switching band around the setpoint, 0.1 °C per detent |

#### Boiler

<img src="https://config.home-master.eu/OpenthermThermostat/Images/screens/screen-boiler.png?v=3" alt="Boiler page" width="420">

What the boiler itself reports.

- Boiler water temperature, large.
- The flow setpoint the thermostat is asking for, in cyan, labelled
  `FLOW SET`. This is computed from the weather curve and capped by the
  maximum flow setting.
- Four tiles: modulation level, return temperature, hot-water temperature,
  fault summary.

**Menu**

| Item | Does |
|---|---|
| `DHW SETPOINT` | Hot-water temperature, 1 °C per detent |
| `MAX FLOW` | Upper limit for the flow setpoint, 1 °C per detent |
| `DHW` | Hot water on or off at the boiler |

#### Schedule

<img src="https://config.home-master.eu/OpenthermThermostat/Images/screens/screen-schedule.png?v=3" alt="Schedule page" width="420">

- Which window is in force now, `COMFORT` or `ECO`, and its setpoint.
- The weekday window and the weekend window, as `HH:MM - HH:MM`.
- Whether boost is on.

The windows themselves are set from Home Assistant, not on the screen: the
device exposes four time entities for weekday start and end and weekend start
and end. The screen shows them so you can check them without a phone.

**Menu**

| Item | Does |
|---|---|
| `COMFORT` | Comfort setpoint, 0.5 °C per detent |
| `ECO` | Eco setpoint, 0.5 °C per detent |
| `BOOST OFFSET` | How far above comfort boost goes, 0.5 °C per detent |
| `BOOST` | Boost on or off |

#### System

<img src="https://config.home-master.eu/OpenthermThermostat/Images/screens/screen-system.png?v=4" alt="System page" width="420">

Diagnostics, for commissioning and for support calls.

| Row | Shows |
|---|---|
| `IP` | Address on the network |
| `FIRMWARE` | Firmware version |
| `BOILER LINK` | `OK` once the boiler has answered, amber `NONE` until then |
| `ROOM RAW` | The room sensor before the offset is applied |
| `LIGHT` | Ambient light in lux |
| `PRESENCE` | Whether the radar sees anyone right now |

**Menu**

| Item | Does |
|---|---|
| `ROOM OFFSET` | Correction added to the room sensor, 0.1 °C per detent |

Set the room offset after the unit has been on the wall for at least two hours:
compare `ROOM RAW` against a reference thermometer in the middle of the room
and enter the difference.

### How the control loop uses all this

Every 30 seconds the thermostat:

1. picks the target — comfort inside the window, eco outside it, comfort plus
   the boost offset while boost is on;
2. compares it against the room temperature with the hysteresis band, so the
   burner is not asked to start and stop around a single value;
3. computes a flow temperature from the weather curve and caps it at the
   maximum flow setting;
4. enables or disables central heating at the boiler to match.

With `HEATING` set to `MANUAL` none of this runs and the boiler is left exactly
as it was found, which is what you want while servicing it.

## Network Requirements

- 2.4 GHz Wi-Fi
- mDNS on the local subnet for ESPHome discovery
- Outbound HTTPS to the vendor update manifest, only if managed updates are used

The device holds no cloud dependency. Heating continues with Wi-Fi down.

## First Boot & Wi-Fi Setup

### Improv over Bluetooth (recommended)

1. Power the device.
2. Open https://improv-wifi.com in Chrome or Edge on a device with Bluetooth.
3. Select the thermostat and enter the Wi-Fi credentials.

### Improv over USB serial

Connect USB-C and use the same page, choosing serial instead of Bluetooth. No driver is needed on current operating systems; the USB-serial bridge is a CP2102.

The provisioning window stays open for 15 minutes after power-on. Power-cycle the device to reopen it.

## Home Assistant Integration

Add the device under Settings → Devices & Services → ESPHome. All entities appear automatically; no YAML is needed on the Home Assistant side.

> ⚠️ **Taking control.** Importing the configuration in ESPHome Device Builder replaces the factory API encryption key with one of your own. Save that key before adding the device in Home Assistant.
Home Assistant sorts the entities into four cards of its own accord.
<table>
<tr>
<td align="center"><b>Controls</b><br><img src="https://config.home-master.eu/OpenthermThermostat/Images/ha/ha-controls.png" width="300"></td>
<td align="center"><b>Sensors</b><br><img src="https://config.home-master.eu/OpenthermThermostat/Images/ha/ha-sensors.png" width="300"></td>
</tr>
<tr>
<td align="center"><b>Configuration</b><br><img src="https://config.home-master.eu/OpenthermThermostat/Images/ha/ha-configuration.png" width="300"></td>
<td align="center"><b>Diagnostic</b><br><img src="https://config.home-master.eu/OpenthermThermostat/Images/ha/ha-diagnostic.png" width="300"></td>
</tr>
</table>
**Controls** is what you operate day to day. **Sensors** is what the thermostat and the boiler report — in the capture the boiler is heating water, not the house: the comfort window is closed and the room is above the eco setpoint, so central heating is switched off at the boiler. **Configuration** holds the settings, including the two source selectors and the time zone. **Diagnostic** is for commissioning and support calls.
The three numbers Home Assistant writes into — room temperature, room humidity and outside temperature — sit in Configuration:
<img src="https://config.home-master.eu/OpenthermThermostat/Images/ha/ha-inputs.png" width="320">
The whole device page:
<img src="https://config.home-master.eu/OpenthermThermostat/Images/ha/ha-device-page.png" width="640">

## Firmware Updates

### ESPHome updates (user-controlled)

Adopt the device in ESPHome Device Builder and build from the published configuration. This is the route for anyone who wants to modify the firmware.

### Managed updates (HTTP)

The shipped configuration carries an `update` entity backed by a vendor JSON manifest. New releases appear in Home Assistant under Settings → Updates and install over the air.

## Troubleshooting

| Symptom | Cause |
|---|---|
| Boiler entities unavailable | OpenTherm pair not connected, or the boiler is not in OpenTherm slave mode |
| Screen stays dark | Radar has not seen anyone and the timeout expired — approach the device |
| Screen never blanks | Radar still detects movement; reduce the still-distance gate |
| Room temperature reads high | Check the device is not in direct sunlight or above a heat source |
| Device not found in Home Assistant | mDNS blocked between VLANs; add the device by IP address |

## Compliance & Certifications

### Applicable EU directives

- Radio Equipment Directive 2014/53/EU
- RoHS 2011/65/EU
- WEEE 2012/19/EU

### Trademarks

OpenTherm® is a registered trademark of the OpenTherm Association. HomeMaster® is a registered EU trademark of ISYSTEMS AUTOMATION S.R.L. (EUTM 019082911).

## Photographs

| | |
|---|---|
| ![Front, screen on](https://config.home-master.eu/OpenthermThermostat/Images/thermostat-front.jpg) | ![Front, screen off](https://config.home-master.eu/OpenthermThermostat/Images/thermostat-front-off.jpg) |
| ![Three-quarter view](https://config.home-master.eu/OpenthermThermostat/Images/thermostat-angle-2.jpg) | ![View from below, USB-C port](https://config.home-master.eu/OpenthermThermostat/Images/thermostat-angle-3.jpg) |
| ![Wall plate, rear face](https://config.home-master.eu/OpenthermThermostat/Images/thermostat-back.jpg) | |

## License

### Hardware

Schematics, PCB layouts and bills of materials are licensed under **CERN-OHL-W v2**.

### Firmware & ESPHome Integration

All firmware, ESPHome configurations and software components are licensed under the **MIT License**.
