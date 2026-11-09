# HomeMaster OpenTherm Thermostat — Installer & User Manual

**Part No. OTT-R1 · Hardware V1.0 · Firmware 1.0.0**
**Document No.: MAN-OTT-R1-V1.0 · November 2026**

ISYSTEMS AUTOMATION S.R.L. · HomeMaster®

---

## Contents

1. Safety
2. About this Product
3. Technical Data
4. Installation
5. Wiring
6. Controls and Display
7. Network and Provisioning
8. Home Assistant
9. Firmware Updates
10. Heating Control
11. Troubleshooting
12. Compliance and Certifications
13. Support and Resources
14. Disposal

---

## 1. Safety

This chapter describes the safety rules that must be observed before
installation, during installation, and during normal operation of the HomeMaster
OpenTherm Thermostat. Failure to follow these rules can result in personal
injury, equipment damage, or fire.

### 1.1 Qualified personnel

Installation, wiring and commissioning of this device must be carried out by
personnel familiar with low-voltage wiring practice and with the boiler
installation it is connected to. Work on the boiler itself, and on the mains
supply feeding the 24 V power supply, must be carried out by qualified
electrical personnel in accordance with the local electrical code.

End users must not open the enclosure or perform wiring.

### 1.2 Safety symbols used in this manual

| Symbol | Meaning |
|---|---|
| ⚠ Danger | Hazardous situation that can cause serious injury or death if not avoided. |
| ⚠ Warning | Hazardous situation that can cause minor injury or significant equipment damage. |
| ℹ Note | Information that helps the installer understand the device or avoid common mistakes. |
| ✓ Tip | Recommended best practice. |

### 1.3 Safety summary

> **ℹ The device is SELV throughout**
> Every terminal and connector on this product is a Safety Extra-Low Voltage
> circuit: the 24 V DC input, the OpenTherm bus, and the USB-C connector. There
> is no mains circuit inside the enclosure and no hazardous voltage on any
> terminal.

> **⚠ Supply from 24 V DC only**
> Connect only a 24 V DC SELV supply to the +V / 0V terminals. Do not connect
> mains voltage to any terminal. Doing so destroys the device and creates a
> fire and electric-shock hazard. The input is protected against reverse
> polarity but not against overvoltage.

> **⚠ Disconnect power before wiring changes**
> The device clips onto its wall base. Remove the body from the base and
> disconnect the 24 V supply before changing any wiring.

> **⚠ The OpenTherm bus is not a mains circuit**
> OT+ and OT− carry the boiler's low-voltage OpenTherm signal. Do not run the
> OpenTherm pair in the same cable bundle or conduit as mains wiring. Do not
> connect it to a 230 V thermostat circuit — some boilers offer both a 230 V
> room-thermostat input and an OpenTherm input on adjacent terminals. Read the
> boiler's manual and identify the OpenTherm terminals before wiring.

> **⚠ This device commands the boiler**
> The thermostat is an OpenTherm master: it enables and disables central
> heating and sets the flow temperature. Commissioning it on a live heating
> system changes the boiler's behaviour immediately. Verify the boiler's own
> safety limits — maximum flow temperature, frost protection, overheat cut-out
> — are set and working before leaving the installation.

> **ℹ Frost protection**
> The firmware holds the comfort setpoint when it has no valid clock, so that a
> lost time source cannot let the house go cold. It does not replace the
> boiler's own frost-protection function. Leave the boiler's frost protection
> enabled.

### 1.4 Conditions of use

- Indoor use only, in dry locations.
- Operating temperature: 0 °C to +40 °C.
- Relative humidity: 0–90 %, non-condensing.
- Pollution degree: 2 (clean, dry environment).
- Altitude: up to 2 000 m above sea level.
- Not suitable for outdoor installation, bathrooms with direct water exposure,
  unheated outbuildings, or positions in direct sunlight.

---

## 2. About this Product

The HomeMaster OpenTherm Thermostat is an ESP32-based wall-mounted room
thermostat that controls a modulating boiler directly over the OpenTherm bus. It
carries a 2.8″ colour display, a rotary encoder and four keys, a 24 GHz presence
radar, and on-board temperature, humidity and ambient-light sensors.

### 2.1 Operating principle

The thermostat operates as an **OpenTherm master**. It initiates all
communication on the OpenTherm bus; the boiler must therefore be configured as
an OpenTherm slave. This is the standard configuration of every OT-capable
boiler.

The heating control loop runs on the device itself. Every 30 seconds the
firmware:

1. picks the room target — the comfort setpoint inside the schedule window, the
   eco setpoint outside it, or comfort plus the boost offset while Boost is on;
2. compares it with the room temperature and applies hysteresis;
3. calculates the flow setpoint from the weather curve and caps it at the
   boiler's maximum;
4. enables or disables central heating accordingly.

Home Assistant is used for visibility and remote control, never as a dependency.
If the network or Home Assistant is unavailable, the room keeps being heated to
the stored schedule, and the display, the encoder and the keys keep working.

### 2.2 Key features

- OpenTherm master with full boiler telemetry — flow, return, DHW, modulation,
  pressure, outside temperature, fault flags.
- Local control loop: schedule, hysteresis, weather compensation.
- 2.8″ IPS colour display, 320 × 240, with four pages.
- Rotary encoder and four keys.
- 24 GHz presence radar — wakes the screen on approach, blanks it when the room
  empties, and reports occupancy to Home Assistant.
- On-board temperature, humidity and ambient-light sensors.
- ESPHome pre-installed; native Home Assistant API.
- Improv Wi-Fi onboarding over Bluetooth and USB serial.
- OTA updates over HTTP (vendor-managed) and over the ESPHome API.

### 2.3 Typical applications

- Single-zone control of a modulating gas or oil boiler in a house or flat.
- Replacement of an OpenTherm room unit supplied by the boiler manufacturer,
  where the installation is to be brought into Home Assistant.
- A room unit alongside an existing HomeMaster OpenTherm Gateway, where the
  gateway is in the boiler room and a wall unit is wanted in the living space.

### 2.4 What is in the box

- HomeMaster OpenTherm Thermostat (OTT-R1), assembled.
- Round wall base, or rectangular wall plate.
- Two wall screws and plugs.
- Quick-start card.

An external 2.4 GHz antenna with a U.FL connector, a 24 V DC power supply, and
cable are not supplied.

---

## 3. Technical Data

### 3.1 Electrical

| Item | Value |
|---|---|
| Supply voltage | 24 V DC nominal, SELV, reverse-polarity protected |
| Supply current | See the datasheet; the display backlight dominates |
| OpenTherm interface | Master, polarity-insensitive, 2-wire |
| USB | USB-C, serial only, CP2102 bridge |
| Wi-Fi | 2.4 GHz, 802.11 b/g/n, external antenna, U.FL |
| Bluetooth | BLE, used for Improv provisioning |
| Presence radar | 24 GHz, HLK-LD2410C |

### 3.2 Mechanical and environmental

Enclosure: Italtronic EYE BOX, ABS, white (RAL 9003), wall mounted.

| Dimension | Value | Italtronic part |
|---|---|---|
| Body outer diameter | Ø 94.6 mm | P61610001G frame ring |
| Front aperture (visible glass) | Ø 82.2 mm | P61610001G |
| Ring height | 20.5 mm | P61610001G |
| Wall base, round | Ø 88.7 mm, 10.5 mm deep, keyhole centres 58 mm | P61610002B |
| Wall plate, rectangular | 140 × 80 mm (149.9 mm over the tabs), 9 mm thick, cable aperture 45 × 30 mm, fixing centres 83.5 / 60 mm | P61610014A |
| PCB carrier | Ø 90.3 mm, 7.5 mm deep; PCB fixing pattern 58 × 36 mm, Ø 2.4 mm | P61610001V |

- Indoor use only, dry locations.
- Operating temperature: 0 … 40 °C, non-condensing.
- Two-part construction: wall base or plate fixed to the wall, body clipped on.

> **ℹ Room temperature accuracy.** The display backlight and the ESP32 raise the
> temperature inside the enclosure. The temperature sensor is mounted on a
> thermally isolated tab at the bottom of the board, below all heat sources, and
> the firmware exposes both the raw and the corrected reading so the offset can
> be verified on site. Use **Room Sensor Offset** to trim it against a reference
> thermometer after the device has been running for at least two hours in its
> final position.

### 3.3 Wiring terminals

| Terminal | Function |
|---|---|
| +V | 24 V DC supply |
| 0V | 0 V |
| OT+ | OpenTherm bus, boiler terminal 1 |
| OT− | OpenTherm bus, boiler terminal 2 |

### 3.4 Compliance

See Chapter 12.

---

## 4. Installation

### 4.1 Choosing a position

- About 1.5 m above the floor, on an internal wall.
- Away from direct sunlight, draughts, radiators, and the swing of a door.
- Not above a heat source, a television, or a lamp.
- Not in an alcove or behind a curtain — the radar needs a clear view of the
  room, and the temperature sensor needs free air.

> **⚠ Radar and metal.** 24 GHz reflects off large metal surfaces. A steel door,
> a metal-clad flue, or a mirror directly in front of the device causes false
> presence. Keep the area in front of the device clear.

> **ℹ Radar and movement that is not a person.** The radar sees any movement,
> including a curtain over a radiator, a ceiling fan, and a pet. If the screen
> never blanks, reduce **Radar Max Still Gate** before suspecting a fault.

### 4.2 Mounting

1. Fix the wall base or wall plate with two screws, or mount it on a flush box.
2. Bring the 24 V DC supply and the OpenTherm pair through the opening.
3. Terminate the wires.
4. Clip the body onto the base.

### 4.3 Removal

Hold the body and pull it straight off the base. Disconnect the 24 V supply
before touching the terminals.

---

## 5. Wiring

### 5.1 Power input

Connect a 24 V DC SELV supply to +V and 0V. The input is reverse-polarity
protected; a reversed supply does not damage the device but it will not start.

### 5.2 OpenTherm bus

Connect OT+ and OT− to the boiler's OpenTherm terminals. The interface is
polarity-insensitive, so the two wires may be swapped.

- Use a twisted pair.
- Do not exceed 50 m.
- Do not run the pair in a bundle with mains cabling.

> **⚠ Identify the boiler terminals before wiring.** Many boilers place a 230 V
> room-thermostat input next to the OpenTherm input. Connecting OT+/OT− to the
> 230 V terminals destroys the device.

### 5.3 Cable routing

Bring both cables through the wall base opening. Leave enough slack to pull the
body away from the wall for service, but not so much that the cable fouls the
clip.

---

## 6. Controls and Display

### 6.1 Navigation

Four keys and one encoder, with the same meaning on every page:

| Control | Action |
|---|---|
| ◀ ▶ | previous / next page |
| ▲ ▼ | open the menu, move through it |
| encoder | change the page's main value, or the selected menu item |
| ◀ with the menu open | close the menu |

The screen shows the room temperature, the active setpoint and the state of the
boiler with no menu visible. The menu is only needed to change something.

### 6.2 Pages

| Page | Shows |
|---|---|
| CLIMATE | Room temperature, active setpoint, flame and modulation, boiler water temperature, outside temperature, system pressure |
| BOILER | Boiler water temperature, flow setpoint from the curve, modulation, return temperature, DHW temperature, fault state |
| SCHEDULE | Active mode, setpoint, weekday and weekend comfort windows, boost state |
| SYSTEM | IP address, firmware version, boiler link state, raw room temperature, ambient light, presence |

### 6.3 Backlight and screen blanking

The backlight is driven at 25 kHz, above the audible range, so the ceramic
capacitors on the supply rail stay silent. The radar blanks the screen 45
seconds after the room empties and lights it again on approach. On waking, the
display returns to the CLIMATE page with the menu closed.

---

## 7. Network and Provisioning

### 7.1 Network requirements

- 2.4 GHz Wi-Fi.
- mDNS on the local subnet for ESPHome discovery.
- Outbound HTTPS to the vendor update manifest, only if managed updates are
  used.

The device holds no cloud dependency. Heating continues with Wi-Fi down.

### 7.2 Improv over Bluetooth (recommended)

1. Power the device.
2. Open https://improv-wifi.com in Chrome or Edge on a device with Bluetooth.
3. Select the thermostat and enter the Wi-Fi credentials.

### 7.3 Improv over USB serial

Connect USB-C and use the same page, choosing serial instead of Bluetooth. No
driver is needed on current operating systems; the USB-serial bridge is a
CP2102.

### 7.4 Fallback access point

If the stored credentials fail, the device raises an access point named
**HomeMaster Thermostat Fallback** with a captive portal for entering new
credentials.

---

## 8. Home Assistant

Add the device under Settings → Devices & Services → ESPHome. All entities
appear automatically; no YAML is needed on the Home Assistant side.

> **⚠ Taking control.** Importing the configuration in ESPHome Device Builder
> replaces the factory API encryption key with one of your own. Save that key
> before adding the device in Home Assistant.

### 8.1 Entities

| Entity | Type |
|---|---|
| Room Temperature | sensor |
| Room Temperature Raw | sensor (diagnostic) |
| Room Humidity | sensor |
| Ambient Light | sensor |
| Comfort Temperature, Eco Temperature | number |
| Boost Offset, Room Hysteresis, Room Sensor Offset | number (config) |
| Curve Cold/Warm Outside, Curve Cold/Warm Flow | number (config) |
| Weekday / Weekend Comfort Start and End | datetime (config) |
| Heating Target, Flow Setpoint By Curve | sensor |
| Boiler Water Temperature, Return, DHW, Pressure, Modulation, Outside | sensor |
| Boiler Flame On, CH Active, DHW Active, Fault Indication | binary_sensor |
| Boiler CH Enable, Boiler DHW Enable | switch |
| Heating Automatic, Boost Until Next Change | switch |
| Comfort Window, Heat Demand | binary_sensor |
| Presence, Radar Target | binary_sensor |
| Backlight | light |
| Firmware Update | update |

---

## 9. Firmware Updates

### 9.1 Vendor-managed OTA (HTTP)

The shipped configuration carries a **Firmware Update** entity backed by a
vendor JSON manifest. New releases appear in Home Assistant under Settings →
Updates and install over the air.

### 9.2 ESPHome OTA (user-controlled)

Adopt the device in ESPHome Device Builder and build from the published
configuration. This is the route for anyone who wants to modify the firmware.

### 9.3 USB-C recovery flashing

If the device no longer boots, connect USB-C and flash the factory binary with
ESPHome Web or esptool.

### 9.4 ESPHome compatibility

Requires ESPHome 2026.7.0 or newer, for the `mipi_spi` display platform and the
`opentherm` component.

---

## 10. Heating Control

### 10.1 Setpoints

| Setting | Default | Meaning |
|---|---|---|
| Comfort Temperature | 21.5 °C | Target inside the schedule window |
| Eco Temperature | 19.0 °C | Target outside it |
| Boost Offset | 1.0 °C | Added to comfort while Boost is on |
| Room Hysteresis | 0.3 °C | Dead band around the target |

### 10.2 Schedule

| Setting | Default |
|---|---|
| Weekday Comfort Start / End | 17:00 / 23:00 |
| Weekend Comfort Start / End | 09:00 / 23:00 |

The clock comes from Home Assistant when it is available and from SNTP
otherwise. With neither, the firmware holds comfort.

### 10.3 Weather compensation

The flow setpoint is interpolated between two points on a curve and capped at
the boiler's maximum CH setpoint.

| Setting | Default |
|---|---|
| Curve Cold Outside / Curve Cold Flow | −15 °C → 65 °C |
| Curve Warm Outside / Curve Warm Flow | +15 °C → 35 °C |

Without an outside temperature from the boiler, the curve collapses to its warm
end — the result under-shoots rather than over-heats.

### 10.4 Heating Automatic

Turning **Heating Automatic** off suspends the whole control loop and leaves the
boiler in whatever state it was found. Use it when commissioning, or when the
boiler is to be controlled from elsewhere.

---

## 11. Troubleshooting

| Symptom | Cause |
|---|---|
| Boiler entities unavailable | OpenTherm pair not connected, or the boiler is not in OpenTherm slave mode |
| Screen stays dark | Radar has not seen anyone and the timeout expired — approach the device |
| Screen never blanks | Radar still detects movement; reduce the still-distance gate |
| Room temperature reads high | Device in direct sunlight or above a heat source; or the offset needs trimming after warm-up |
| Device not found in Home Assistant | mDNS blocked between VLANs; add the device by IP address |
| Setpoint changes on the wheel do not reach the boiler | Check Heating Automatic is on and the boiler link is OK on the SYSTEM page |
| Device will not start | Supply polarity reversed, or the supply is below 24 V under load |

---

## 12. Compliance and Certifications

The HomeMaster OpenTherm Thermostat is CE marked and designed to comply with the
applicable European Union legislation. ISYSTEMS AUTOMATION S.R.L. maintains the
technical documentation and a signed EU Declaration of Conformity (document
number DoC-OTT-R1-V1.0).

### 12.1 Applicable EU legislation

- Directive 2014/53/EU — Radio Equipment Directive (RED). Covers health and
  safety, electromagnetic compatibility, radio spectrum and cybersecurity for
  this product.
- Directive 2011/65/EU and (EU) 2015/863 — Restriction of Hazardous Substances
  (RoHS).
- Directive 2012/19/EU — Waste Electrical and Electronic Equipment (WEEE).
- Regulation (EU) 2023/988 — General Product Safety Regulation (GPSR).

### 12.2 Harmonised standards applied

| Area | Standards |
|---|---|
| Health and safety — Art. 3.1(a) | EN IEC 62368-1:2020 + A11:2020 |
| RF exposure | EN 62479:2010 |
| EMC — general | EN 301 489-1 V2.2.3 |
| EMC — wideband data | EN 301 489-17 V3.2.4 |
| EMC — short-range devices | EN 301 489-3 V2.3.2 |
| Radio spectrum — 2.4 GHz | EN 300 328 V2.2.2 |
| Radio spectrum — 24 GHz SRD | EN 300 440 V2.2.1 |
| Cybersecurity — Art. 3(3)(d) | EN 18031-1:2024 |
| RoHS technical documentation | EN IEC 63000 |

### 12.3 Radio

| | 2.4 GHz | 24 GHz |
|---|---|---|
| Module | ESP32-WROOM-32U-N16 | HLK-LD2410C |
| Manufacturer | Espressif Systems | Shenzhen Hi-Link Electronic |
| Band | 2402–2480 MHz | 24.00–24.25 GHz |
| Max. radiated power | ≤ 20 dBm EIRP | see the Declaration of Conformity |

EU type-examination for the 2.4 GHz module: No. 172141367/AA/02, issued
02 March 2023 by Kiwa Nederland B.V. (Notified Body 0063), Wilmersdorf 50,
7327 AC Apeldoorn, The Netherlands.

### 12.4 Manufacturer / EU economic operator (per GPSR)

ISYSTEMS AUTOMATION S.R.L.
Diligentei 18, Ploiești, Romania
Tel.: +40 747 757 798
Web: https://www.home-master.eu

### 12.5 Declaration of Conformity

The full EU Declaration of Conformity (DoC-OTT-R1-V1.0) is available on request
and is published in the manufacturer's GitHub repository under
`OpenthermThermostat/Manuals/`.

---

## 13. Support and Resources

### 13.1 Support channels

- Forum: https://www.home-master.eu/forum
- Email: support@home-master.eu

### 13.2 Online resources

- Product page: https://www.home-master.eu
- Repository: https://github.com/isystemsautomation/homemaster-dev/tree/main/OpenthermThermostat
- Firmware: `OpenthermThermostat/Firmware/thermostat.yaml`
- Device page: https://devices.esphome.io/devices/homemaster-openthermthermostat/

### 13.3 License

Hardware — schematics, PCB layouts and bills of materials — is licensed under
CERN-OHL-W v2. Firmware and ESPHome configuration are licensed under the MIT
License.

---

## 14. Disposal

The product and its packaging carry the crossed-out wheeled bin symbol.

This product must not be disposed of with household waste. At the end of its
life, hand it to a collection point for waste electrical and electronic
equipment, or return it to the manufacturer. Separate collection prevents
hazardous substances reaching the environment and allows the materials to be
recovered.

---

HomeMaster® is a registered European Union trademark (EUTM No. 019082911) of
ISYSTEMS AUTOMATION S.R.L. OpenTherm® is a registered trademark of the OpenTherm
Association. ESPHome and the Made for ESPHome logo are used under permission
from the Open Home Foundation. All other trademarks are the property of their
respective owners.

© 2026 ISYSTEMS AUTOMATION S.R.L. · MAN-OTT-R1-V1.0
