# HomeMaster Thermostat for OpenTherm — Datasheet

**Part No. OTT-R1 · Hardware V1.0 · Firmware 1.1.2**
**Document No.: DS-OTT-R1-V1.0 · November 2026**

ISYSTEMS AUTOMATION S.R.L. · HomeMaster®

---

## 1. Description

Wall-mounted ESP32 room thermostat that acts as an **OpenTherm master** for
modulating boilers. 2.8″ colour display, rotary encoder and four keys, 24 GHz
presence radar, on-board temperature, humidity and ambient-light sensors.

The heating control loop — schedule, room hysteresis, weather-compensated flow
setpoint — runs on the device. Home Assistant is used for visibility and remote
control, not as a dependency: with the network down the room keeps being heated
to the stored schedule.

The outside temperature and the room temperature can each be taken from the
device's own source — the boiler's outside sensor, the on-board SHT40 — or from
a Home Assistant entity, selected per temperature. Weather compensation can also
be switched off entirely, in which case the boiler is asked for a fixed flow
setpoint.

ESPHome is pre-installed. The device appears in Home Assistant over the native
ESPHome API, with no MQTT and no register mapping.

## 2. Key features

- OpenTherm master, full boiler telemetry and modulating control
- Local control loop — no cloud, no internet, no Home Assistant required
- 2.8″ IPS colour display, 320 × 240, ST7789V
- Rotary encoder and four perimeter keys
- HLK-LD2410C 24 GHz presence radar, UART plus instant GPIO output
- SHT40 temperature and humidity sensor on a thermally isolated tab
- BH1750 ambient light sensor
- Improv Wi-Fi onboarding over Bluetooth and USB serial
- OTA updates over HTTP (vendor-managed) and over the ESPHome API
- Open hardware (CERN-OHL-W v2) and firmware (MIT)

## 3. Absolute maximum ratings

| Parameter | Min | Max | Unit |
|---|---|---|---|
| Supply voltage at +V / 0V | −30 | +30 | V DC |
| Operating ambient temperature | 0 | +40 | °C |
| Storage temperature | −20 | +70 | °C |
| Relative humidity, non-condensing | 0 | 90 | % |

Stresses beyond those listed may cause permanent damage. The supply input is
reverse-polarity protected by a series Schottky diode.

## 4. Electrical characteristics

| Parameter | Value |
|---|---|
| Nominal supply | 24 V DC, SELV |
| Supply current, measured across operating states | 50–100 mA |
| Input power, maximum | 2.4 W |
| Energy source classification | ES1 throughout; PS1 |
| Equipment class | Class III, no protective earth |
| OpenTherm interface | Master, 2-wire, polarity-insensitive |
| OpenTherm cable length | 50 m max., twisted pair |
| USB | USB-C, serial only, CP2102 |

Current and power figures are those measured on the assembled unit at 24 V DC
with the backlight at 100 % and both radios active, as recorded in safety
assessment SAFETY-OTT-R1-2026-001.

## 5. Radio

| | Wi-Fi / Bluetooth | Presence radar |
|---|---|---|
| Module | ESP32-WROOM-32U-N16 | HLK-LD2410C |
| Band | 2402–2480 MHz | 24.00–24.25 GHz |
| Max. radiated power | ≤ 20 dBm EIRP | −5.70 dBm e.i.r.p., measured |
| Antenna | External, U.FL connector | On-module |
| Standards | 802.11 b/g/n, BLE | FMCW presence detection |

Detection range of the radar is configurable per gate from Home Assistant
(**Radar Max Move Gate**, **Radar Max Still Gate**, **Radar Timeout**).

## 6. Display and controls

| Item | Value |
|---|---|
| Panel | 2.8″ IPS, 320 × 240, ST7789V, SPI |
| Backlight | LEDC PWM at 25 kHz, above the audible range |
| Backlight level, awake | 85 % |
| Encoder | Quadrature, 2 counts per detent |
| Keys | Four: ◀ ▶ ▲ ▼ |
| Screen refresh | Every 2 s, and immediately on any input |
| Screen blanking | Radar-driven, 45 s after the room empties |

Four pages — Climate, Boiler, Schedule, System — each with a menu reached with
▲ or ▼. Every setting the heating loop uses, the comfort windows included, can
be set from the glass without Home Assistant. Colour carries one meaning each: amber a running circuit, red a fault or
a lost link, green a healthy state where a word says so, white a measurement,
dim grey no reading. The interface is described in full in Chapter 6 of the user
manual.

## 7. Sensors

| Sensor | Part | Interface | Address |
|---|---|---|---|
| Temperature and humidity | SHT40 | I²C | 0x44 |
| Ambient light | BH1750 | I²C | 0x23 |

## 8. GPIO map

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

GPIO5 is a strapping pin, used here as the display clock. It is driven only
after boot and carries no external pull resistor. GPIO34/35/36/39 are
input-only; each key has an external 10 kΩ pull-up to 3V3 and a 100 nF debounce
capacitor.

## 9. Terminals

| Terminal | Function |
|---|---|
| +V | 24 V DC supply |
| 0V | 0 V |
| OT+ | OpenTherm bus, boiler terminal 1 |
| OT− | OpenTherm bus, boiler terminal 2 |

## 10. Mechanical

Enclosure: Italtronic EYE BOX, ABS to UL94 V-0, white (RAL 9003), wall mounted.

| Dimension | Value | Italtronic part |
|---|---|---|
| Body outer diameter | Ø 94.6 mm | P61610001G |
| Front aperture | Ø 82.2 mm | P61610001G |
| Ring height | 20.5 mm | P61610001G |
| Wall base, round | Ø 88.7 mm, 10.5 mm deep, keyhole centres 58 mm | P61610002B |
| Wall plate, rectangular | 140 × 80 mm, 9 mm thick, aperture 45 × 30 mm, fixings 83.5 / 60 mm | P61610014A |
| PCB carrier | Ø 90.3 mm, 7.5 mm deep; PCB holes 58 × 36 mm, Ø 2.4 mm | P61610001V |

Assembled unit: base plate 152 × 107 mm, round body Ø 95 mm, total height from
base to screen 47 mm.

## 11. Default entity set

Sensors: Room Temperature, Room Temperature Raw, Room Humidity, Ambient Light,
Heating Target, Flow Setpoint By Curve, Boiler Water Temperature, Boiler Return
Temperature, Boiler DHW Temperature, Boiler CH Pressure, Boiler Relative
Modulation Level, Outside Temperature, Outside Temperature In Use, Uptime,
WiFi Signal, Radar distances.

Binary sensors: Boiler Flame On, Boiler CH Active, Boiler DHW Active, Boiler
Fault Indication, Boiler Low Water Pressure, Boiler Flame Fault, Boiler Service
Request, Comfort Window, Heat Demand, Presence, Radar Target.

Numbers: Comfort Temperature, Eco Temperature, Boost Offset, Room Hysteresis,
Room Sensor Offset, Curve Cold/Warm Outside, Curve Cold/Warm Flow, Fixed Flow
Setpoint, Boiler DHW Setpoint, Boiler Max CH Setpoint, Radar gates and timeout.

Selects: Outside Temperature Source, Room Temperature Source.

Written from Home Assistant: Room Temperature Input, Room Humidity Input,
Outside Temperature Input. A value written to any of them is ignored once it is
older than 30 minutes.

Datetimes: Weekday and Weekend Comfort Start and End.

Room Temperature Raw, Room Humidity Raw, Boiler Return Temperature and Boiler
DHW Temperature are created disabled in Home Assistant and have to be enabled on
the device page before they record.

Switches: Heating Automatic, Weather Compensation, Boost Until Next Change,
Boiler CH Enable, Boiler DHW Enable.

Other: Backlight (light), Firmware Update (update), IP Address and ESPHome
Version (text sensors).

## 12. Defaults

| Setting | Default |
|---|---|
| Comfort Temperature | 21.5 °C |
| Eco Temperature | 19.0 °C |
| Boost Offset | 1.0 °C |
| Room Hysteresis | 0.3 °C |
| Weekday comfort window | 17:00 – 23:00 |
| Weekend comfort window | 09:00 – 23:00 |
| Weather curve | −15 °C → 65 °C, +15 °C → 35 °C |
| Weather Compensation | On |
| Fixed Flow Setpoint | 55 °C |
| Boiler DHW Setpoint | 50 °C |
| Boiler Max CH Setpoint | 80 °C |
| Outside Temperature Source | Boiler, then Home Assistant |
| Room Temperature Source | Built-in sensor |
| Control interval | 30 s |
| Screen timeout | 45 s |

## 13. Compliance

Directive 2014/53/EU (RED), Directive 2011/65/EU and (EU) 2015/863 (RoHS),
Directive 2012/19/EU (WEEE), Regulation (EU) 2023/988 (GPSR). See
DoC-OTT-R1-V1.0 and Chapter 12 of the user manual.

## 14. Ordering information

| Part number | Description |
|---|---|
| OTT-R1 | HomeMaster Thermostat for OpenTherm, wall mount, 24 V DC |

## 15. Manufacturer

ISYSTEMS AUTOMATION S.R.L.
Diligentei 18, Ploiești, Romania
CUI RO 21537032 · EUID ROONRC.J2007000919293
Tel.: +40 747 757 798 · https://www.home-master.eu

---

HomeMaster® is a registered European Union trademark (EUTM No. 019082911) of
ISYSTEMS AUTOMATION S.R.L. OpenTherm® is a registered trademark of the OpenTherm
Association. All other trademarks are the property of their respective owners.

© 2026 ISYSTEMS AUTOMATION S.R.L. · DS-OTT-R1-V1.0
