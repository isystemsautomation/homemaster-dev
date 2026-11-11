# HomeMaster Thermostat for OpenTherm — Installer & User Manual

**Part No. OTT-R1 · Hardware V1.0 · Firmware 1.1.3**
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
Thermostat for OpenTherm. Failure to follow these rules can result in personal
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

The HomeMaster Thermostat for OpenTherm is an ESP32-based wall-mounted room
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
   boiler's maximum, or uses the fixed flow setpoint when compensation is off
   or no outside temperature is available;
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

- HomeMaster Thermostat for OpenTherm (OTT-R1), assembled.
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
| Supply current | 50–100 mA across operating states |
| Input power, maximum | 2.4 W |
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

### 4.2 What the thermostat mounts on

The device is supplied with the Italtronic EYE BOX mounting parts and goes up
one of two ways.

**Straight to the wall.** The round base, Italtronic P61610002B, Ø 88.7 mm and
10.5 mm deep, takes two screws into the wall at **58 mm** centres through its
keyholes. Nothing else is needed; the cables come through the opening in the
base.

**On a flush-mounting box.** The rectangular plate, Italtronic P61610014A,
140 × 80 mm and 9 mm thick, screws onto a box already in the wall. It carries
two pairs of fixing holes, and Italtronic describes it as a *"wall mounting
element for electrical box use UK/USA/EUROPE respectively Ø 60, 3 gang and
Ø 60 + 3 gang"*:

| Hole centres | Fits |
|---|---|
| **60 mm** | The round device box used across continental Europe — bore Ø 68 mm, depth from 35 mm. Also the square boxes whose device screws sit at 60 mm |
| **83.5 mm** | The Italian three-module box, 503 or 503E |

What decides whether a box fits is the **distance between its device screws**,
not the shape of the box or the size of the hole in the wall. Measure that
distance before ordering anything: 60 mm and 83.5 mm are the two the plate is
drilled for.

The plate has a cable aperture of 45 × 30 mm, and it sets the body 9 mm off the
wall, which is where the cable turns.

> **ℹ A box is not required.** The round base screws to bare plasterwork. A
> flush box is worth having for one reason: it is where the supply can live,
> and it keeps the cable joints out of the wall cavity.

**Sequence**

1. Fix the round base, or the plate on the box, with two screws.
2. Bring the 24 V DC supply and the OpenTherm pair through the opening.
3. Terminate the wires.
4. Clip the body on.

### 4.3 Power supply — not included

**No mains power supply is shipped with the thermostat.** The device takes
**24 V DC SELV**, draws 50–100 mA and never more than 2.4 W, and that supply is
the installer's to provide. Mains voltage must not reach any terminal on the
device.

There are two sensible places to put it.

**In the distribution board or the boiler cabinet.** A DIN-rail 24 V supply
feeds the thermostat over the same route as the OpenTherm pair. This is the
tidier arrangement and the one to prefer: the supply is accessible, it is in a
ventilated enclosure, and nothing mains-powered sits behind the thermostat.

**In a deep flush box behind the thermostat.** Only with a supply that is built
for installation inside a closed box, in a box deep enough for it, and wired by
someone qualified to work on mains. The room-temperature reading is the other
reason to think twice: a supply dissipating a watt or two directly behind the
sensor warms the wall it is screwed to.

**Cabling.** Two pairs reach the device — 24 V DC, and OpenTherm. They may
share one cable or run separately; both are SELV, so a single four-core cable
is fine. OpenTherm is polarity-insensitive, takes a twisted pair and must not
exceed 50 m. Neither pair may be bundled with mains cabling.

### 4.4 Removal

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

Everything the thermostat does can be done from the glass. Home Assistant adds
remote access and history; it does not add control that is missing here.

The screen is 320 × 240 pixels. It redraws every two seconds, and immediately
after any key press or encoder movement.

### 6.1 Backlight and presence

The backlight rises to 85 % on any key press or encoder movement, and the
presence radar lights it when somebody approaches. It turns off 45 seconds after
the radar stops seeing anyone; while the radar still sees someone, it stays on.

When the screen blanks, the interface also returns to the **Climate** page with
the menu closed, so the unit is in the same state every time you walk up to it.

The backlight is driven at 25 kHz, above the audible range. At lower
frequencies the ceramic capacitors on the 5 V rail sing.

### 6.2 Navigation

Four keys and the encoder. The keys mean the same thing on every page.

| Control | Menu closed | Menu open |
|---|---|---|
| ◀ Left | Previous page; wraps around | Closes the menu |
| ▶ Right | Next page; wraps around | Closes the menu |
| ▲ Up | Opens the menu on the first item | Moves the selection up; wraps |
| ▼ Down | Opens the menu on the first item | Moves the selection down; wraps |
| Encoder | On Climate: changes the active setpoint, 0.5 °C per detent. On the other pages: nothing | Changes the selected item |

The selected menu item is drawn as a dim band with a blue bar down its left
edge. Changes take effect as you turn the encoder — there is nothing to confirm
and nothing to save.

### 6.3 The header

The top strip is the same on every page.

| Position | Shows |
|---|---|
| Left | Page name — `CLIMATE`, `BOILER`, `SCHEDULE`, `SYSTEM` |
| After the name | Four square dots, one per page; the current page is white, the others dim |
| Right | Clock, `HH:MM`, or `--:--` until the clock is set |
| Left of the clock | Wi-Fi — white when associated, dim and crossed out when not |
| Left of that | Boiler — red when the boiler reports a fault |

The clock comes from Home Assistant when it is connected and from an internet
time server otherwise.

### 6.4 Colour

Four colours, one meaning each. Nothing on the glass is coloured for
decoration.

| Colour | Means | Where you see it |
|---|---|---|
| **Amber** | A circuit is running | Burner alight, central heating active, hot water being drawn, boost on |
| **Red** | A fault, or a link that is down | `FAULT`, low water pressure, the boiler icon in the header, `BOILER LINK NONE` |
| **Green** | A healthy state, where a word says so | `OK` on the fault tile, `BOILER LINK OK`, `HOME ASSISTANT OK` |
| **White** | A measurement | Every number that is a reading |
| Dim grey | No reading — dashes stand in for the value | A sensor that is absent, or a value the boiler has not sent |

Within a tile the icon and the number are coloured apart: the **icon** carries
the state of the circuit, the **number** stays white because it is a
measurement. Hot water stored at 59 °C reads white whether or not anyone is
drawing it; only the boiler icon beside it goes dim when the circuit is idle.

### 6.5 Icons

These are the only icons the interface uses.

| Icon | Meaning | Colour |
|---|---|---|
| <img src="https://config.home-master.eu/OpenthermThermostat/Images/icons/wifi.png" width="36"> | Wi-Fi is associated | White |
| <img src="https://config.home-master.eu/OpenthermThermostat/Images/icons/wifi-off.png" width="36"> | No Wi-Fi connection | Dim. The Home Assistant link is a separate row on the System page |
| <img src="https://config.home-master.eu/OpenthermThermostat/Images/icons/water-boiler.png" width="36"> | In the header: the boiler reports a fault. In a tile: boiler water or hot-water temperature | Red on fault; amber while that circuit is active, grey otherwise |
| <img src="https://config.home-master.eu/OpenthermThermostat/Images/icons/fire.png" width="36"> | Burner. The value is the relative modulation level, or `ON` / `IDLE` when the boiler does not report modulation | Amber while the flame is on, grey when it is out |
| <img src="https://config.home-master.eu/OpenthermThermostat/Images/icons/thermometer.png" width="36"> | Outside temperature on the Climate page, return temperature on the Boiler page | Grey; the number beside it is white |
| <img src="https://config.home-master.eu/OpenthermThermostat/Images/icons/gauge.png" width="36"> | Central-heating water pressure, in bar | Red when the boiler reports low pressure, grey otherwise |
| <img src="https://config.home-master.eu/OpenthermThermostat/Images/icons/alert.png" width="36"> | Boiler fault summary — `FAULT` or `OK` | Red on fault, green when clear |

A reading the boiler has not sent, or a sensor that is absent, is shown as `--`
or `--.-` in dim grey, never as a zero. A return temperature below 5 °C is
treated as no reading: a boiler with no return sensor answers 0.00 instead of
refusing the request, and 0 °C of return water beside 48 °C of flow is not a
measurement.

### 6.6 Climate page

The page for day-to-day use.

![Climate page](https://config.home-master.eu/OpenthermThermostat/Images/screens/screen-climate.png)

- Room temperature, large, to one decimal.
- The setpoint currently in force below it, in cyan.
- To its right: `COMFORT`, `ECO` or `BOOST`.
- Four tiles: burner, boiler water temperature, outside temperature, system
  pressure.

Turning the encoder here changes the setpoint of whichever window is active —
comfort during the comfort window, eco outside it.

| Menu item | Effect |
|---|---|
| `COMFORT SETPOINT` / `ECO SETPOINT` | The active setpoint, 0.5 °C per detent |
| `HEATING` | `AUTO` or `MANUAL`; manual suspends the control loop |
| `BOOST` | Comfort plus the boost offset until the next scheduled change |
| `HYSTERESIS` | Switching band around the setpoint, 0.1 °C per detent |

### 6.7 Boiler page

What the boiler itself reports.

![Boiler page](https://config.home-master.eu/OpenthermThermostat/Images/screens/screen-boiler.png)

- Boiler water temperature, large.
- The flow setpoint the thermostat is asking for, in cyan, labelled `FLOW SET`.
- Four tiles: modulation level, return temperature, hot-water temperature,
  fault summary.

| Menu item | Effect |
|---|---|
| `DHW SETPOINT` | Hot-water temperature, 1 °C per detent |
| `MAX FLOW` | Upper limit for the flow setpoint, 1 °C per detent |
| `DHW` | Hot water on or off at the boiler |

### 6.8 Schedule page

![Schedule page](https://config.home-master.eu/OpenthermThermostat/Images/screens/screen-schedule.png)

- Which window is in force now, and its setpoint.
- The weekday and weekend windows, as `HH:MM - HH:MM`.
- Whether boost is on.

The windows are set here, on the glass, and in Home Assistant through four
time entities — whichever is to hand. The menu on this page is seven items
long.

| Menu item | Effect |
|---|---|
| `COMFORT` | Comfort setpoint, 0.5 °C per detent |
| `ECO` | Eco setpoint, 0.5 °C per detent |
| `BOOST OFFSET` | How far above comfort boost goes, 0.5 °C per detent |
| `WEEKDAY FROM` | Start of the weekday comfort window, 10 minutes per detent |
| `WEEKDAY TO` | End of it, 10 minutes per detent |
| `WEEKEND FROM` | Start of the weekend comfort window, 10 minutes per detent |
| `WEEKEND TO` | End of it, 10 minutes per detent |

A window wraps at midnight: turning `WEEKEND TO` past `23:50` brings it to
`00:00`. Boost is not repeated on this menu — it is on the Climate menu.

### 6.9 System page

Diagnostics, for commissioning and for support calls.

![System page](https://config.home-master.eu/OpenthermThermostat/Images/screens/screen-system.png)

| Row | Shows |
|---|---|
| `IP` | Address on the network |
| `FIRMWARE` | Firmware version |
| `BOILER LINK` | Green `OK` once the boiler has answered, red `NONE` until then |
| `HOME ASSISTANT` | Green `OK` while the ESPHome API is connected, grey `NONE` otherwise. Heating does not depend on it |
| `TIME ZONE` | The zone the clock is keeping, as `UTC+1 EU`. `FROM HA` while Home Assistant is connected, because the zone then comes from there |
| `ROOM RAW` | The room sensor before the offset is applied |
| `LIGHT` | Ambient light in lux |
| `PRESENCE` | Whether the radar sees anyone right now |

| Menu item | Effect |
|---|---|
| `ROOM OFFSET` | Correction added to the room sensor, 0.1 °C per detent |
| `UTC OFFSET` | −12 to +14 hours, 0.25 h per detent |
| `SUMMER TIME` | `Off`, `Europe`, `North America`, `Australia`, `New Zealand`; one detent moves to the next |

> **⚠ Set these two on a thermostat that has no Home Assistant.** The device
> learns the time from an internet time server, and a time server sends the
> moment but never the zone. With Home Assistant connected the zone comes from
> there and both settings are ignored — the System page then reads `FROM HA`.
> Without it the schedule runs on what is set here, and a wrong setting shifts
> the comfort window by hours while the clock on the glass still looks
> perfectly correct.

The offset moves in quarter hours because not every country is on the hour —
India is on the half, Nepal and the Chatham Islands on the quarter. The four
summer-time rules are every schedule in common use:

| Rule | Turns forward | Turns back |
|---|---|---|
| `Europe` | Last Sunday in March | Last Sunday in October |
| `North America` | Second Sunday in March | First Sunday in November |
| `Australia` | First Sunday in October | First Sunday in April |
| `New Zealand` | Last Sunday in September | First Sunday in April |

Anywhere else — most of Asia, Africa and South America, and the countries that
keep dates of their own — choose `Off`. The clock then holds correct standard
time the year round.

✓ Set the room offset after the unit has been on the wall for at least two
hours: compare `ROOM RAW` against a reference thermometer in the middle of the
room and enter the difference. The same reading is called **Room Temperature
Raw** in Home Assistant, where it is one of the entities that arrive switched
off — do this on the glass and nothing has to be enabled first.

---

## 7. Network and Provisioning

### 7.1 Network requirements

- 2.4 GHz Wi-Fi.
- mDNS on the local subnet for ESPHome discovery.
- Outbound NTP (UDP 123) to `pool.ntp.org`, for a thermostat used without
  Home Assistant. There is no battery-backed clock on the board and no other
  way to learn the time; with Home Assistant connected the time comes from
  there instead.
- Outbound HTTPS to the vendor update manifest, only if managed updates are
  used.

The device holds no cloud dependency. Heating continues with Wi-Fi down.

### 7.2 The provisioning window

Wi-Fi credentials are accepted only during a **15-minute window that opens when
the device is powered on**. When the window closes, the device logs that it has
closed and accepts no further credentials until it is power-cycled.

> **ℹ Why there is no setup access point.** Earlier HomeMaster firmware raised
> an open access point with a captive portal when it could not join the stored
> network. That was removed under Article 3(3)(d) of the Radio Equipment
> Directive and EN 18031-1: an open access point that accepts new Wi-Fi
> credentials is an unauthenticated path to reconfigure the device. **This
> product raises no access point under any circumstance.** If the stored
> credentials stop working, power-cycle the device to reopen the provisioning
> window and set them again by one of the methods below.

### 7.3 Improv over Bluetooth (recommended)

1. Power the device. The provisioning window is now open for 15 minutes.
2. Open https://improv-wifi.com in Chrome or Edge on a computer or phone with
   Bluetooth. Safari and iOS cannot do this — they have no Web Bluetooth.
3. Select the thermostat and enter the Wi-Fi credentials.

> **ℹ On Linux**, Web Bluetooth has to be enabled once in Chrome, under
> `chrome://flags/#enable-experimental-web-platform-features`.

### 7.4 Improv over USB serial

Connect USB-C and use the same page, choosing serial instead of Bluetooth. No
driver is needed on current operating systems; the USB-serial bridge is a
CP2102. This is the route to use when Bluetooth is unavailable.

### 7.5 Changing the network later

Power-cycle the device and re-provision within 15 minutes, by Bluetooth or by
USB. There is no other path, by design.

---

## 8. Home Assistant

Add the device under Settings → Devices & Services → ESPHome. All entities
appear automatically; no YAML is needed on the Home Assistant side.

> **⚠ Taking control.** Importing the configuration in ESPHome Device Builder
> replaces the factory API encryption key with one of your own. Save that key
> before adding the device in Home Assistant.

Home Assistant sorts the entities into four cards of its own accord.
**Controls** is what you operate day to day; **Sensors** is what the thermostat
and the boiler report; **Diagnostic** is for commissioning and support calls.

<table>
<tr>
<td align="center"><b>Controls</b><br><img src="https://config.home-master.eu/OpenthermThermostat/Images/ha/ha-controls.png" style="width:74mm"></td>
<td align="center"><b>Sensors</b><br><img src="https://config.home-master.eu/OpenthermThermostat/Images/ha/ha-sensors.png" style="width:74mm"></td>
</tr>
</table>

In the Sensors capture the boiler is heating water, not the house: the comfort
window is closed and the room is above the eco setpoint, so there is no heat
demand and central heating is switched off at the boiler. `Outside Temperature`
is missing because that boiler has no outside sensor; `Outside Temperature In
Use` shows what the control loop is actually working from, which here comes
from Home Assistant.

**Configuration** holds the settings — the weather curve, the hysteresis, the
two source selectors, the time zone, the radar gates and the four comfort
times. It is a long card; every entity on it is listed in 8.1, and it is
reproduced in full on the device page in the repository.

<table>
<tr>
<td align="center"><b>Diagnostic</b><br><img src="https://config.home-master.eu/OpenthermThermostat/Images/ha/ha-diagnostic.png" style="width:74mm"></td>
<td align="center"><b>The three inputs, in Configuration</b><br><img src="https://config.home-master.eu/OpenthermThermostat/Images/ha/ha-inputs.png" style="width:74mm"></td>
</tr>
</table>

### 8.1 Entities

| Entity | Type |
|---|---|
| Room Temperature | sensor |
| Room Temperature Raw | sensor (diagnostic, off by default) |
| Room Humidity | sensor |
| Room Humidity Raw | sensor (diagnostic, off by default) |
| Ambient Light | sensor |
| Comfort Temperature, Eco Temperature | number |
| Boost Offset, Room Hysteresis, Room Sensor Offset | number (config) |
| Curve Cold/Warm Outside, Curve Cold/Warm Flow | number (config) |
| Fixed Flow Setpoint | number (config) |
| Outside Temperature Source, Room Temperature Source | select (config) |
| Summer Time | select (config) |
| UTC Offset | number (config) |
| Boiler DHW Setpoint, Boiler Max CH Setpoint | number |
| Room Temperature Input, Outside Temperature Input, Room Humidity Input | number (config, written from Home Assistant) |
| Weather Compensation | switch |
| Outside Temperature In Use | sensor |
| Weekday / Weekend Comfort Start and End | datetime (config) |
| Heating Target, Flow Setpoint By Curve | sensor |
| Boiler Water Temperature, Pressure, Modulation, Outside | sensor |
| Boiler Return Temperature, Boiler DHW Temperature | sensor (off by default) |
| Boiler Flame On, CH Active, DHW Active, Fault Indication | binary_sensor |
| Boiler CH Enable, Boiler DHW Enable | switch |
| Heating Automatic, Boost Until Next Change | switch |
| Comfort Window, Heat Demand | binary_sensor |
| Presence, Radar Target | binary_sensor |
| Backlight | light |
| Firmware Update | update |

> **ℹ Four entities arrive switched off.** **Room Temperature Raw**, **Room
> Humidity Raw**, **Boiler Return Temperature** and **Boiler DHW Temperature**
> are created disabled, so
> that a device added to Home Assistant does not bring a dozen rarely-read
> values with it. Home Assistant shows them on the device page as *disabled
> entities*; open that line, pick the entity and enable it. It starts recording
> from the moment it is enabled — there is no back-history.

---

## 9. Firmware Updates

### 9.1 Vendor-managed OTA (HTTP)

The shipped configuration carries a **Firmware Update** entity backed by a
vendor JSON manifest. New releases appear in Home Assistant under Settings →
Updates and install over the air.

### 9.2 ESPHome OTA (user-controlled)

Adopt the device in ESPHome Device Builder and build from the published
configuration. This is the route for anyone who wants to modify the firmware.

> **ℹ Taking control hands you the whole configuration**, not a reference to it,
> so the device stops following vendor releases from that point on. To go back
> to vendor updates, adopt the published configuration again.

### 9.3 USB-C recovery flashing

If the device no longer boots, connect USB-C and flash the factory binary with
ESPHome Web or esptool.

### 9.4 ESPHome compatibility

Requires ESPHome 2026.10.0 or newer, for the `improv_ble` component. The
`mipi_spi` display platform and the `opentherm` component have been available
since 2026.7.0.

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
| Fixed Flow Setpoint | 55 °C |
| Weather Compensation | On |

**Weather Compensation** turns the curve off. With it off, the boiler is asked
for **Fixed Flow Setpoint** and nothing else — use this on a system that is
balanced for one flow temperature, or while commissioning.

**With no outside temperature available**, from either source, the thermostat
also asks for Fixed Flow Setpoint. It does not fall back to the warm end of the
curve: on a January morning that is a silent under-heat which looks like a
broken thermostat rather than a missing sensor.

Both limits still apply: the value is capped by **Boiler Max CH Setpoint**, and
the room hysteresis still decides whether heat is called for at all.

### 10.4 Where the temperatures come from

Two selects decide the sources. Both default to the device's own source, so a
thermostat that is never added to Home Assistant behaves as a self-contained
unit.

| Select | Options | Default |
|---|---|---|
| **Outside Temperature Source** | `Boiler, then Home Assistant` · `Boiler` · `Home Assistant` | Boiler, then Home Assistant |
| **Room Temperature Source** | `Built-in sensor` · `Home Assistant` | Built-in sensor |

The default for the outside temperature prefers the boiler's own sensor and
falls back to Home Assistant only when the boiler reports none. The boiler's
sensor does not depend on the network, so it is the safer source when both
exist.

> **ℹ Most boilers have no outside sensor.** If yours does not, the curve has
> nothing to work from until you either fit one or point the thermostat at a
> Home Assistant entity — a weather integration is enough. Until then the
> thermostat uses Fixed Flow Setpoint.

> **⚠ Home Assistant as a source is a dependency.** With a source set to
> `Home Assistant`, that value stops arriving when the network or Home
> Assistant does. The room temperature then has no reading and heating stops
> being regulated; the outside temperature falls back to Fixed Flow Setpoint.
> Leave the room source on the built-in sensor unless you have a reason not to.

**Room Sensor Offset applies to the built-in sensor only.** A value coming from
Home Assistant arrives from a thermometer that is already calibrated, and
correcting it again here would be a second, invisible correction.

The sensor **Outside Temperature In Use** publishes the value actually feeding
the curve, after the select has been applied, and the Climate page shows that
same value. Use it to confirm which source is live.

### Feeding a temperature from Home Assistant

The thermostat does not go looking for an entity by name. Home Assistant writes
the value into the device, through three numbers that appear in the device's
**Configuration** section:

| Number | Feeds |
|---|---|
| `Room Temperature Input` | The room temperature, when Room Temperature Source is set to Home Assistant |
| `Room Humidity Input` | The room humidity, under the same selector — a room whose temperature comes from Home Assistant takes its humidity from there too |
| `Outside Temperature Input` | The outside temperature, when the outside source is set to or falls back to Home Assistant |

Any entity can feed them — a wall sensor, a weather integration, an average of
several rooms — and nothing in the firmware has to change.

One automation per value. Replace the sensor in both places with your own:

```yaml
alias: Room temperature to the thermostat
triggers:
  - trigger: state
    entity_id: sensor.your_room_sensor
  - trigger: time_pattern
    minutes: "/5"
  - trigger: homeassistant
    event: start
actions:
  - action: number.set_value
    target:
      entity_id: number.homemaster_thermostat_room_temperature_input
    data:
      value: "{{ states('sensor.your_room_sensor') | float }}"
mode: single
```

The outside temperature and the humidity are the same automation with
`number.homemaster_thermostat_outside_temperature_input` and
`number.homemaster_thermostat_room_humidity_input` and the matching sensors.

Humidity is not used for heating — it is reported for comfort and for damp
rooms — so leaving `Room Humidity Input` unfed costs nothing but the reading.

> **⚠ The repeating trigger is not optional.** A pushed value is ignored once it
> is older than **30 minutes**. Without the five-minute trigger a quiet sensor
> would silently take the thermostat out of regulation after half an hour. With
> it, the value is refreshed long before it expires.

The trigger on Home Assistant starting matters too: after a restart the value
arrives at once instead of up to five minutes later.

**What happens when the value stops arriving.** The thermostat does not keep
using it. The room temperature becomes unavailable and heating is no longer
regulated; the outside temperature falls back to Fixed Flow Setpoint. This is
deliberate — regulating a house on a reading from yesterday is worse than
admitting the reading is gone.

Until something writes to them, both numbers read −50 °C, which is the bottom of
their range rather than a measurement. The thermostat knows nothing has been
written and ignores them.

### 10.5 Heating Automatic

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
| Wi-Fi icon crossed out | No Wi-Fi association. A normal icon in a dim colour means Wi-Fi is up but Home Assistant is not connected — that is not a fault |
| Cannot set Wi-Fi credentials | The 15-minute provisioning window has closed. Power-cycle the device and try again within 15 minutes |
| No setup access point appears | There is none, by design — see 7.2 |
| Room temperature reads high | Device in direct sunlight or above a heat source; or the offset needs trimming after warm-up |
| Device not found in Home Assistant | mDNS blocked between VLANs; add the device by IP address |
| Setpoint changes on the wheel do not reach the boiler | Check Heating Automatic is on and `BOILER LINK` is `OK` on the System page |
| House is cool in cold weather although the boiler runs | No outside temperature, so the flow setpoint is the Fixed Flow Setpoint. Check `Outside Temperature In Use`; either raise the fixed value or give the thermostat an outside source (see 10.4) |
| Outside temperature shows `--` | The boiler reports none and no Home Assistant entity is configured or reachable |
| Room temperature unavailable after a network outage | Room Temperature Source is set to Home Assistant; switch it back to the built-in sensor |
| Device will not start | Supply polarity reversed, or the supply is below 24 V under load |

---

## 12. Compliance and Certifications

The HomeMaster Thermostat for OpenTherm is CE marked and designed to comply with
the applicable European Union legislation. ISYSTEMS AUTOMATION S.R.L. maintains
the technical documentation and a signed EU Declaration of Conformity (document
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
| Max. radiated power | ≤ 20 dBm EIRP | −5.70 dBm e.i.r.p., measured |

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

- Support: https://www.home-master.eu/support
- Email: support@home-master.eu

### 13.2 Online resources

- Product page: https://www.home-master.eu
- Repository: https://github.com/isystemsautomation/homemaster-dev/tree/main/OpenthermThermostat
- Firmware: `OpenthermThermostat/Firmware/thermostat.yaml`

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
Association. All other trademarks are the property of their
respective owners.

© 2026 ISYSTEMS AUTOMATION S.R.L. · MAN-OTT-R1-V1.0
