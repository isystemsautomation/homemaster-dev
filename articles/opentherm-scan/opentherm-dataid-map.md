# OpenTherm Data-ID map (0–127): spec vs ESPHome component vs a real boiler

A reference table of every OpenTherm **Data-ID (0–127)** with three views side by side:

1. **OpenTherm 2.2 standard** — what the register means in the protocol (name, data type, read/write).
2. **ESPHome `opentherm` component** — the entity/field the component maps this ID to (`— (not mapped)` = the component has no entity for it).
3. **Real boiler** — whether a **Viessmann E3 Vitodens 100 (BHC 0122)** actually answers this ID, captured with a full **0–255** scan (only 0–127 shown here; the upper half is summarised at the bottom). `✓` = READ_ACK (supported), `✗` = Unknown-DataId (not supported), `— no reply` = timeout/ignored.

Notes:
- **`f8.8`** = fixed-point °C/value; **`u8/u8`, `s8/s8`** = two bytes; **`u16/s16`** = one 16-bit integer; **`flag8`** = bitfield. Some IDs (33 Exhaust, 35 Fan speed) are **integers, not `f8.8`** — decoding them as float gives wrong numbers.
- IDs **70–91** are the OpenTherm **Ventilation/Heat-Recovery** application block (irrelevant to a plain boiler).
- Reserved/manufacturer IDs vary between vendors and spec revisions; treat those rows as indicative.
- The boiler column is one specific unit — **your boiler will differ**. Run the scanner (`opentherm-SCAN.yaml`) to map your own.
- **Write-only IDs (R/W = `W`)** answer `Unknown-DataId` to this READ scan — that is **expected, not missing support**. They are written, not read (e.g. **ID 1 CH Setpoint** works perfectly as a write; a read of it just returns Unknown). Such rows are marked `n/a (write-only, read not expected)`.

| ID | OpenTherm 2.2 standard | Type | R/W | ESPHome component | This Vitodens 100 |
|---:|---|---|:--:|---|---|
| 0 | Status (master/slave status bits) | `flag8/flag8` | R | STATUS | ✓ HB0/LB4 |
| 1 | Control setpoint — CH water temp (TSet) | `f8.8` | W | CH_SETPOINT | n/a (write-only, read not expected) |
| 2 | Master config / MemberID | `flag8/u8` | W | CONTROLLER_CONFIG | n/a (write-only, read not expected) |
| 3 | Slave config / MemberID | `flag8/u8` | R | DEVICE_CONFIG | ✓ HB81/LB33 |
| 4 | Remote command | `u8/u8` | W | COMMAND_CODE | n/a (write-only, read not expected) |
| 5 | Application-specific fault flags / OEM fault code | `flag8/u8` | R | FAULT_FLAGS | ✓ HB0/LB255 |
| 6 | Remote boiler parameter flags | `flag8/flag8` | R | REMOTE | ✓ HB3/LB3 |
| 7 | Cooling control signal | `f8.8` | W | COOLING_CONTROL | n/a (write-only, read not expected) |
| 8 | Control setpoint CH2 (TSetCH2) | `f8.8` | W | CH2_SETPOINT | n/a (write-only, read not expected) |
| 9 | Remote override room setpoint | `f8.8` | R | CH_SETPOINT_OVERRIDE | ✓ 0.00 |
| 10 | Number of Transparent Slave Parameters (TSP) | `u8/u8` | R | TSP_COUNT | ✗ Unknown-DataId |
| 11 | TSP index / value | `u8/u8` | RW | TSP_COMMAND | ✗ Unknown-DataId |
| 12 | Fault History Buffer size | `u8/u8` | R | FHB_SIZE | ✗ Unknown-DataId |
| 13 | Fault History Buffer index / value | `u8/u8` | R | FHB_COMMAND | ✗ Unknown-DataId |
| 14 | Max relative modulation level setting | `f8.8` | W | MAX_MODULATION_LEVEL | n/a (write-only, read not expected) |
| 15 | Max boiler capacity / min modulation level | `u8/u8` | R | MAX_BOILER_CAPACITY | ✓ HB25/LB10 (25 kW / 10% min) |
| 16 | Room setpoint (TrSet) | `f8.8` | W | ROOM_SETPOINT | n/a (write-only, read not expected) |
| 17 | Relative modulation level | `f8.8` | R | MODULATION_LEVEL | ✓ 0.00 |
| 18 | CH water pressure | `f8.8` | R | CH_WATER_PRESSURE | ✓ 1.70 |
| 19 | DHW flow rate (l/min) | `f8.8` | R | DHW_FLOW_RATE | ✓ 0.00 |
| 20 | Day of week & time | `special` | RW | DAY_TIME | ✗ Unknown-DataId |
| 21 | Date (month/day) | `u8/u8` | RW | DATE | ✗ Unknown-DataId |
| 22 | Year | `u16` | RW | YEAR | ✗ Unknown-DataId |
| 23 | Room setpoint CH2 (TrSetCH2) | `f8.8` | W | ROOM_SETPOINT_CH2 | n/a (write-only, read not expected) |
| 24 | Room temperature (Tr) | `f8.8` | W | ROOM_TEMP | n/a (write-only, read not expected) |
| 25 | Boiler flow water temperature | `f8.8` | R | FEED_TEMP | ✓ 60.20 |
| 26 | DHW temperature | `f8.8` | R | DHW_TEMP | ✓ 55.70 |
| 27 | Outside temperature | `f8.8` | R | OUTSIDE_TEMP | — no reply |
| 28 | Return water temperature | `f8.8` | R | RETURN_WATER_TEMP | ✗ Unknown-DataId |
| 29 | Solar storage temperature | `f8.8` | R | SOLAR_STORE_TEMP | ✗ Unknown-DataId |
| 30 | Solar collector temperature | `s16` | R | SOLAR_COLLECT_TEMP | ✗ Unknown-DataId |
| 31 | Flow water temperature CH2 | `f8.8` | R | FEED_TEMP_CH2 | ✗ Unknown-DataId |
| 32 | DHW2 temperature | `f8.8` | R | DHW2_TEMP | ✗ Unknown-DataId |
| 33 | Exhaust temperature | `s16 (integer!)` | R | EXHAUST_TEMP | ✓ 50  (HB0/LB50) |
| 34 | Boiler heat exchanger temperature | `f8.8` | R | — (not mapped) | ✗ Unknown-DataId |
| 35 | Boiler fan speed setpoint & actual | `u8/u8 (integer!)` | R | FAN_SPEED | ✓ HB28/LB28 |
| 36 | Electrical current through burner flame (µA) | `f8.8` | R | FLAME_CURRENT | ✗ Unknown-DataId |
| 37 | Room temperature CH2 (TrCH2) | `f8.8` | W | ROOM_TEMP_CH2 | n/a (write-only, read not expected) |
| 38 | Relative humidity | `u8/u8` | R | REL_HUMIDITY | ✗ Unknown-DataId |
| 39 | Reserved / manufacturer-specific | `—` | — | — (not mapped) | ✗ Unknown-DataId |
| 40 | Reserved / manufacturer-specific | `—` | — | — (not mapped) | ✗ Unknown-DataId |
| 41 | Reserved / manufacturer-specific | `—` | — | — (not mapped) | ✗ Unknown-DataId |
| 42 | Reserved / manufacturer-specific | `—` | — | — (not mapped) | ✗ Unknown-DataId |
| 43 | Reserved / manufacturer-specific | `—` | — | — (not mapped) | ✗ Unknown-DataId |
| 44 | Reserved / manufacturer-specific | `—` | — | — (not mapped) | ✗ Unknown-DataId |
| 45 | Reserved / manufacturer-specific | `—` | — | — (not mapped) | ✗ Unknown-DataId |
| 46 | Reserved / manufacturer-specific | `—` | — | — (not mapped) | ✗ Unknown-DataId |
| 47 | Reserved / manufacturer-specific | `—` | — | — (not mapped) | ✗ Unknown-DataId |
| 48 | DHW setpoint bounds (upper/lower) | `s8/s8` | R | DHW_BOUNDS | ✓ HB60/LB30 |
| 49 | Max CH setpoint bounds (upper/lower) | `s8/s8` | R | CH_BOUNDS | ✓ HB82/LB5 |
| 50 | OTC heat-curve ratio bounds (upper/lower) | `s8/s8` | R | OTC_CURVE_BOUNDS | ✗ Unknown-DataId |
| 51 | Reserved / manufacturer-specific | `—` | — | — (not mapped) | ✗ Unknown-DataId |
| 52 | Reserved / manufacturer-specific | `—` | — | — (not mapped) | ✗ Unknown-DataId |
| 53 | Reserved / manufacturer-specific | `—` | — | — (not mapped) | ✗ Unknown-DataId |
| 54 | Reserved / manufacturer-specific | `—` | — | — (not mapped) | ✗ Unknown-DataId |
| 55 | Reserved / manufacturer-specific | `—` | — | — (not mapped) | ✗ Unknown-DataId |
| 56 | DHW setpoint | `f8.8` | RW | DHW_SETPOINT | ✓ 50.00 |
| 57 | Max CH water setpoint | `f8.8` | RW | MAX_CH_SETPOINT | ✓ 20.00 |
| 58 | OTC heat-curve ratio | `f8.8` | RW | OTC_CURVE_RATIO | ✗ Unknown-DataId |
| 59 | Reserved / manufacturer-specific | `—` | — | — (not mapped) | ✗ Unknown-DataId |
| 60 | Reserved / manufacturer-specific | `—` | — | — (not mapped) | ✗ Unknown-DataId |
| 61 | Reserved / manufacturer-specific | `—` | — | — (not mapped) | ✗ Unknown-DataId |
| 62 | Reserved / manufacturer-specific | `—` | — | — (not mapped) | ✗ Unknown-DataId |
| 63 | Reserved / manufacturer-specific | `—` | — | — (not mapped) | ✗ Unknown-DataId |
| 64 | Reserved / manufacturer-specific | `—` | — | — (not mapped) | ✗ Unknown-DataId |
| 65 | Reserved / manufacturer-specific | `—` | — | — (not mapped) | ✗ Unknown-DataId |
| 66 | Reserved / manufacturer-specific | `—` | — | — (not mapped) | ✗ Unknown-DataId |
| 67 | Reserved / manufacturer-specific | `—` | — | — (not mapped) | ✗ Unknown-DataId |
| 68 | Reserved / manufacturer-specific | `—` | — | — (not mapped) | ✗ Unknown-DataId |
| 69 | Reserved / manufacturer-specific | `—` | — | — (not mapped) | ✗ Unknown-DataId |
| 70 | V/H status | `flag8/flag8` | R | HVAC_STATUS | ✗ Unknown-DataId |
| 71 | V/H control setpoint | `u8/-` | W | REL_VENT_SETPOINT | n/a (write-only, read not expected) |
| 72 | V/H fault flags/code | `flag8/u8` | R | — (not mapped) | ✗ Unknown-DataId |
| 73 | V/H OEM diagnostic code | `u16` | R | — (not mapped) | ✗ Unknown-DataId |
| 74 | V/H config/MemberID | `flag8/u8` | R | DEVICE_VENT | ✗ Unknown-DataId |
| 75 | V/H OpenTherm version | `f8.8` | R | HVAC_VER_ID | ✗ Unknown-DataId |
| 76 | V/H product version | `u8/u8` | R | — (not mapped) | ✗ Unknown-DataId |
| 77 | Relative ventilation (%) | `u8/-` | R | REL_VENTILATION | ✗ Unknown-DataId |
| 78 | Relative humidity (V/H) | `u8/-` | RW | REL_HUMID_EXHAUST | ✗ Unknown-DataId |
| 79 | CO2 level (ppm) | `u16` | RW | EXHAUST_CO2 | ✗ Unknown-DataId |
| 80 | Supply inlet temperature | `f8.8` | R | SUPPLY_INLET_TEMP | ✗ Unknown-DataId |
| 81 | Supply outlet temperature | `f8.8` | R | SUPPLY_OUTLET_TEMP | ✗ Unknown-DataId |
| 82 | Exhaust inlet temperature | `f8.8` | R | EXHAUST_INLET_TEMP | ✗ Unknown-DataId |
| 83 | Exhaust outlet temperature | `f8.8` | R | EXHAUST_OUTLET_TEMP | ✗ Unknown-DataId |
| 84 | Exhaust fan speed (rpm) | `u16` | R | EXHAUST_FAN_SPEED | ✗ Unknown-DataId |
| 85 | Supply fan speed (rpm) | `u16` | R | SUPPLY_FAN_SPEED | ✗ Unknown-DataId |
| 86 | V/H remote parameter flags | `flag8/flag8` | R | REMOTE_VENTILATION_PARAM | ✗ Unknown-DataId |
| 87 | Nominal ventilation value | `u8/-` | RW | NOM_REL_VENTILATION | ✗ Unknown-DataId |
| 88 | V/H TSP number | `u8/u8` | R | HVAC_NUM_TSP | ✗ Unknown-DataId |
| 89 | V/H TSP index/value | `u8/u8` | RW | HVAC_IDX_TSP | ✗ Unknown-DataId |
| 90 | V/H FHB size | `u8/u8` | R | HVAC_FHB_SIZE | ✗ Unknown-DataId |
| 91 | V/H FHB index/value | `u8/u8` | R | HVAC_FHB_IDX | ✗ Unknown-DataId |
| 92 | Reserved / manufacturer-specific | `—` | — | — (not mapped) | ✗ Unknown-DataId |
| 93 | Reserved / manufacturer-specific | `—` | — | — (not mapped) | ✓ 9.34 (OEM, raw-only) |
| 94 | Reserved / manufacturer-specific | `—` | — | — (not mapped) | ✓ 23.19 (OEM, raw-only) |
| 95 | Reserved / manufacturer-specific | `—` | — | — (not mapped) | ✓ 16.21 (OEM, raw-only) |
| 96 | Reserved / manufacturer-specific | `—` | — | — (not mapped) | ✗ Unknown-DataId |
| 97 | Reserved / manufacturer-specific | `—` | — | — (not mapped) | ✗ Unknown-DataId |
| 98 | RF sensor / electricity stats (OEM) | `special` | R | RF_SIGNAL | ✗ Unknown-DataId |
| 99 | Remote override operating mode (OEM use varies) | `special` | R | DHW_MODE | ✓ HB0/LB0 |
| 100 | Remote override function | `flag8/-` | R | OVERRIDE_FUNC | ✓ HB0/LB0 |
| 101 | Solar storage master/slave mode | `flag8/flag8` | R | SOLAR_MODE_FLAGS | — no reply |
| 102 | Solar storage fault flags | `flag8/u8` | R | SOLAR_ASF | ✗ Unknown-DataId |
| 103 | Solar storage slave config | `flag8/u8` | R | SOLAR_VERSION_ID | ✗ Unknown-DataId |
| 104 | Solar storage product version | `u8/u8` | R | SOLAR_PRODUCT_ID | ✗ Unknown-DataId |
| 105 | Solar storage TSP number | `u8/u8` | R | SOLAR_NUM_TSP | ✗ Unknown-DataId |
| 106 | Solar storage TSP index/value | `u8/u8` | RW | SOLAR_IDX_TSP | ✗ Unknown-DataId |
| 107 | Solar storage FHB size | `u8/u8` | R | SOLAR_FHB_SIZE | ✗ Unknown-DataId |
| 108 | Solar storage FHB index/value | `u8/u8` | R | SOLAR_FHB_IDX | ✗ Unknown-DataId |
| 109 | Electricity producer starts | `u16` | R | SOLAR_STARTS | ✗ Unknown-DataId |
| 110 | Electricity producer hours | `u16` | R | SOLAR_HOURS | ✗ Unknown-DataId |
| 111 | Electricity production | `u16` | R | SOLAR_ENERGY | ✗ Unknown-DataId |
| 112 | Cumulative electricity production | `u16` | R | SOLAR_TOTAL_ENERGY | ✗ Unknown-DataId |
| 113 | Unsuccessful burner starts | `u16` | RW | FAILED_BURNER_STARTS | ✗ Unknown-DataId |
| 114 | Flame signal too low count | `u16` | RW | BURNER_FLAME_LOW | ✗ Unknown-DataId |
| 115 | OEM diagnostic/service code | `u16` | R | OEM_DIAGNOSTIC | ✓ 0  (HB0/LB0) |
| 116 | Burner starts | `u16` | RW | BURNER_STARTS | ✗ Unknown-DataId |
| 117 | CH pump starts | `u16` | RW | CH_PUMP_STARTS | ✗ Unknown-DataId |
| 118 | DHW pump/valve starts | `u16` | RW | DHW_PUMP_STARTS | ✗ Unknown-DataId |
| 119 | DHW burner starts | `u16` | RW | DHW_BURNER_STARTS | ✗ Unknown-DataId |
| 120 | Burner operation hours | `u16` | RW | BURNER_HOURS | ✗ Unknown-DataId |
| 121 | CH pump operation hours | `u16` | RW | CH_PUMP_HOURS | ✗ Unknown-DataId |
| 122 | DHW pump/valve operation hours | `u16` | RW | DHW_PUMP_HOURS | ✗ Unknown-DataId |
| 123 | DHW burner operation hours | `u16` | RW | DHW_BURNER_HOURS | ✗ Unknown-DataId |
| 124 | OpenTherm version — Master | `f8.8` | W | OT_VERSION_CONTROLLER | n/a (write-only, read not expected) |
| 125 | OpenTherm version — Slave | `f8.8` | R | OT_VERSION_DEVICE | ✓ 4.10 |
| 126 | Master product type/version | `u8/u8` | W | VERSION_CONTROLLER | n/a (write-only, read not expected) |
| 127 | Slave product type/version | `u8/u8` | R | VERSION_DEVICE | ✓ HB37/LB123 |

## Summary for this boiler

- **Supported (answered READ_ACK): 25 IDs** — 0, 3, 5, 6, 9, 15, 17, 18, 19, 25, 26, 33, 35, 48, 49, 56, 57, 93, 94, 95, 99, 100, 115, 125, 127
- **Extra beyond the component:** IDs **93, 94, 95** answered with data but the ESPHome component has **no entity** for them (labelled `<INVALID>`). Only a raw scan/lambda can read them — likely a small block of Viessmann OEM registers (values ~9.3 / 23.2 / 16.2, not gas).
- **Upper half 128–255:** the full 8-bit space was scanned. **Nothing genuine up there** — every id returned Unknown-DataId or timed out, with no 7-bit aliasing. Notably **id 162** (the register a Daikin D2C uses for its DHW setpoint) returns **Unknown** on this boiler.
- **Not exposed:** all operating-hours / start counters (116–123), return temp (28), outside temp (27) → the boiler keeps these off the bus (ViCare cloud only). There is **no gas-volume Data-ID in the standard at all**.
- **Read-scan caveat:** write-only IDs (R/W = `W`) return `Unknown-DataId` to a read — that is normal. Control of them still works (the CH setpoint, ID 1, is written and works even though it reads back as Unknown).
