# EMC ASSESSMENT REPORT

HomeMaster OpenTherm Thermostat · OTT-R1

**Report No.: EMC-OTT-R1-2026-001 · Revision 1.0**

Issued by ISYSTEMS AUTOMATION S.R.L. internal compliance laboratory
Diligentei 18, Ploiești, Romania · CUI RO 21537032

> **STATUS: NOT ISSUED.** Sections 1 to 6 and 9 are complete. Section 7 carries
> the result tables to be filled from measurement, and section 8 the equipment
> actually used. Section 10 is signed only once section 7 is complete and every
> verdict is PASS. An assessment report issued over tables that were never
> filled is a false record.

---

## 1. Purpose and route

This report records the electromagnetic compatibility assessment of the
HomeMaster OpenTherm Thermostat, carried out by the manufacturer under the
internal production control procedure (**Module A**, Annex II of Directive
2014/53/EU), in support of the essential requirement of Article 3.1(b).

The product is radio equipment within the scope of Directive 2014/53/EU and is
therefore excluded from Directive 2014/30/EU by its Article 2(2)(a). The EMC
essential requirement is met through RED Article 3.1(b) and the EN 301 489
series.

## 2. Equipment under test

| | |
|---|---|
| Product name | HomeMaster OpenTherm Thermostat |
| Part number | OTT-R1 |
| Hardware version | V1.0 |
| Firmware version | 1.0.0 |
| Serial number of the sample | |
| Enclosure | Italtronic EYE BOX, ABS, Ø 94.6 mm, wall mounted |
| Power supply | 24 V DC nominal, SELV, reverse-polarity protected |
| Intended environment | Residential, commercial and light industrial |

### 2.1 Radio functions

| Function | Band | Max. power | Module |
|---|---|---|---|
| Wi-Fi 2.4 GHz / BLE | 2402–2480 MHz | ≤ 20 dBm e.i.r.p. | ESP32-WROOM-32U-N16 |
| Presence radar | 24 155–24 245 MHz | −5.70 dBm e.i.r.p. | HLK-LD2410C |

### 2.2 Ports

| Port | Type | Cable length in use | Tested |
|---|---|---|---|
| +V / 0V | DC power input | > 3 m | Yes |
| OT+ / OT− | Signal, OpenTherm, 2-wire | up to 50 m | Yes |
| Antenna | 2.4 GHz, U.FL to external antenna | < 3 m | Enclosure port |
| USB-C | Service only | n/a | No — see 2.3 |
| Enclosure | Radiated | — | Yes |

### 2.3 Ports not tested, with justification

**USB-C.** A service port used only for firmware recovery and Improv
provisioning during commissioning. It is not connected during normal operation,
and no cable is attached in the intended use of the product. Excluded per
EN 301 489-1 clause 5.1.

**Mains port.** The product has none. All mains-related tests of EN 301 489-1
— conducted emissions on the AC port, harmonic current emissions
(EN 61000-3-2), voltage fluctuations and flicker (EN 61000-3-3), voltage dips
and interruptions (EN 61000-4-11) and surge on the AC port (EN 61000-4-5) — are
therefore not applicable.

## 3. Standards applied

| Standard | Scope |
|---|---|
| ETSI EN 301 489-1 V2.2.3 | Common technical requirements |
| ETSI EN 301 489-3 V2.3.2 | Specific conditions for short range devices |
| ETSI EN 301 489-17 V3.2.4 | Specific conditions for broadband data transmission systems |
| EN 55032:2015 + A11:2020 + A1:2020 | Emission limits and methods, referenced by EN 301 489-1 |
| EN IEC 61000-4-2:2009 | Electrostatic discharge immunity |
| EN IEC 61000-4-3:2020 | Radiated RF electromagnetic field immunity |
| EN 61000-4-4:2012 | Electrical fast transient / burst immunity |
| EN 61000-4-6:2023 | Immunity to conducted disturbances induced by RF fields |
| EN 61000-4-29:2000 | Voltage dips and interruptions on DC input power ports |

## 4. Operating and test configuration

The EUT is assembled in its enclosure, mounted on a non-conductive support, and
supplied from the 24 V DC power supply that is sold or recommended with the
product. The external 2.4 GHz antenna fitted is the one specified in the bill of
materials.

| Item | Configuration |
|---|---|
| Power | 24 V DC through the supply cable in its intended length |
| OpenTherm | Connected to an OpenTherm slave simulator or a boiler, link established |
| Wi-Fi | Associated with an access point, API connection established, traffic continuous |
| Radar | Enabled, continuously transmitting |
| Display | On, backlight at the default 85 % |
| Firmware | The shipping build, version 1.0.0 |

### 4.1 Test modes

| Mode | Description | Used for |
|---|---|---|
| TM1 | Normal operation: all radios active, display on, OpenTherm link up | Emission and immunity |
| TM2 | Radar disabled, Wi-Fi active | Emission, to separate the contributions |
| TM3 | Display blanked by the radar timeout | Emission, worst-case check |

The mode producing the highest emission is reported. Where modes differ by less
than 2 dB, TM1 is reported.

## 5. Performance criteria

Declared for this product, in the terms of EN 301 489-1 clause 6.

**Primary functions**, whose loss constitutes a failure:

1. The heating control loop continues to run and the OpenTherm link to the
   boiler is maintained.
2. The central-heating enable state and the flow setpoint sent to the boiler do
   not change other than as the control loop requires.
3. Stored setpoints, schedule and configuration are not altered.
4. The display continues to show the room temperature and the active setpoint.

**Criterion A — during the test.** All four primary functions maintained. The
Wi-Fi connection may degrade in throughput but must not drop. Transient display
artefacts that clear within one refresh interval (2 s) are acceptable.

**Criterion B — during the test, recovery after.** Loss of the Wi-Fi connection
is acceptable if it re-establishes without intervention. Loss of the OpenTherm
link is acceptable only if it re-establishes within 60 s. Primary functions 2
and 3 must hold throughout.

**Criterion C — recovery by operator.** Not accepted for this product. The
device is wall-mounted and unattended; a state requiring a power cycle is a
failure.

**Monitoring.** Throughout each immunity test the following are logged over the
ESPHome API or the serial port: room temperature, heating target, CH enable
state, flow setpoint, OpenTherm link state, Wi-Fi state, radar presence output.
A video record of the display is kept. Any discontinuity is noted against the
test and the moment it occurred.

## 6. Test plan

### 6.1 Emission

| # | Test | Standard | Port | Limit | Applicable |
|---|---|---|---|---|---|
| E1 | Radiated emission, 30 MHz – 1 GHz | EN 55032 Class B | Enclosure | 30 dBμV/m at 3 m (30–230 MHz), 37 dBμV/m at 3 m (230–1000 MHz), quasi-peak | Yes |
| E2 | Radiated emission, 1 GHz – 6 GHz | EN 55032 Class B | Enclosure | 50 dBμV/m average, 70 dBμV/m peak at 3 m | Yes |
| E3 | Conducted emission, 150 kHz – 30 MHz | EN 55032 Class B | DC power | 66–56 dBμV QP (0.15–0.5 MHz), 56 dBμV QP (0.5–5 MHz), 60 dBμV QP (5–30 MHz) | Yes — DC supply cable exceeds 3 m |
| E4 | Harmonic current emissions | EN 61000-3-2 | AC mains | — | No — no mains port |
| E5 | Voltage fluctuations and flicker | EN 61000-3-3 | AC mains | — | No — no mains port |

> **Exclusion bands.** The transmitter's own fundamental and its intended
> emissions are excluded from E1 and E2: 2400–2483.5 MHz for the Wi-Fi section
> and 24 155–24 245 MHz for the radar. The radar's fundamental lies above the
> upper measurement limit in any case.

### 6.2 Immunity

| # | Test | Standard | Port | Level | Criterion | Applicable |
|---|---|---|---|---|---|---|
| I1 | Electrostatic discharge | EN IEC 61000-4-2 | Enclosure | ±4 kV contact, ±8 kV air | B | Yes |
| I2 | Radiated RF field, 80 MHz – 6 GHz | EN IEC 61000-4-3 | Enclosure | 3 V/m, 80 % AM 1 kHz | A | Yes |
| I3 | Fast transient / burst | EN 61000-4-4 | DC power | ±0.5 kV, 5/50 ns, 5 kHz | B | Yes |
| I4 | Fast transient / burst | EN 61000-4-4 | OpenTherm | ±0.5 kV, capacitive clamp | B | Yes — cable exceeds 3 m |
| I5 | Conducted RF, 150 kHz – 80 MHz | EN 61000-4-6 | DC power, OpenTherm | 3 V rms, 80 % AM 1 kHz | A | Yes |
| I6 | Voltage dips and interruptions, DC | EN 61000-4-29 | DC power | 30 % for 10 ms, 60 % for 100 ms, 100 % for 10 ms | B / C per standard | Yes |
| I7 | Surge | EN 61000-4-5 | AC mains | — | — | No — no mains port |
| I8 | Voltage dips, AC | EN 61000-4-11 | AC mains | — | — | No — no mains port |

> **Exclusion bands for I2 and I5.** During radiated and conducted RF immunity
> testing, the frequency ranges in which the EUT's own receivers operate are
> excluded, per EN 301 489-17 clause 7 and EN 301 489-3: **2400–2483.5 MHz**.
> The exclusion is recorded in the results, not silently skipped.

## 7. Results

To be completed from measurement. Every row requires the measured value, the
limit and the verdict. Attach the plots as Annex A and the photographs of each
setup as Annex B.

### 7.1 E1 — Radiated emission, 30 MHz – 1 GHz

Test mode: ____ · Detector: quasi-peak · Distance: ____ m · Date: ____

| Frequency (MHz) | Polarisation | Reading (dBμV) | Correction (dB) | Level (dBμV/m) | Limit (dBμV/m) | Margin (dB) |
|---|---|---|---|---|---|---|
| | | | | | | |
| | | | | | | |
| | | | | | | |
| | | | | | | |
| | | | | | | |
| | | | | | | |

Verdict: ____

### 7.2 E2 — Radiated emission, 1 GHz – 6 GHz

Test mode: ____ · Detector: peak and average · Distance: ____ m · Date: ____

| Frequency (MHz) | Polarisation | Detector | Level (dBμV/m) | Limit (dBμV/m) | Margin (dB) |
|---|---|---|---|---|---|
| | | | | | |
| | | | | | |
| | | | | | |
| | | | | | |

Verdict: ____

### 7.3 E3 — Conducted emission, DC power port

Test mode: ____ · Date: ____

| Frequency (MHz) | Line | Detector | Level (dBμV) | Limit (dBμV) | Margin (dB) |
|---|---|---|---|---|---|
| | | | | | |
| | | | | | |
| | | | | | |
| | | | | | |

Verdict: ____

### 7.4 I1 — Electrostatic discharge

Date: ____

| Point | Type | Level (kV) | Discharges | Observed behaviour | Criterion | Verdict |
|---|---|---|---|---|---|---|
| Front panel, glass | Air | ±8 | 10 each polarity | | B | |
| Front panel, bezel | Air | ±8 | 10 each polarity | | B | |
| Encoder shaft | Contact | ±4 | 10 each polarity | | B | |
| Keys | Air | ±8 | 10 each polarity | | B | |
| Horizontal coupling plane | Contact | ±4 | 10 each polarity | | B | |
| Vertical coupling plane | Contact | ±4 | 10 each polarity | | B | |

Verdict: ____

### 7.5 I2 — Radiated RF field immunity

Field strength: 3 V/m · Modulation: 80 % AM 1 kHz · Step: 1 % · Dwell: ____ s
Excluded band: 2400–2483.5 MHz · Date: ____

| Range (MHz) | Polarisation | Orientation | Observed behaviour | Criterion | Verdict |
|---|---|---|---|---|---|
| 80 – 2400 | H | | | A | |
| 80 – 2400 | V | | | A | |
| 2483.5 – 6000 | H | | | A | |
| 2483.5 – 6000 | V | | | A | |

Verdict: ____

### 7.6 I3 and I4 — Fast transient / burst

Date: ____

| Port | Level (kV) | Coupling | Duration | Observed behaviour | Criterion | Verdict |
|---|---|---|---|---|---|---|
| DC power | ±0.5 | Direct | 1 min each polarity | | B | |
| OpenTherm | ±0.5 | Capacitive clamp | 1 min each polarity | | B | |

Verdict: ____

### 7.7 I5 — Conducted RF immunity

Level: 3 V rms · Modulation: 80 % AM 1 kHz · Date: ____

| Port | Range (MHz) | Observed behaviour | Criterion | Verdict |
|---|---|---|---|---|
| DC power | 0.15 – 80 | | A | |
| OpenTherm | 0.15 – 80 | | A | |

Verdict: ____

### 7.8 I6 — Voltage dips and interruptions, DC input

Date: ____

| Reduction | Duration | Observed behaviour | Criterion | Verdict |
|---|---|---|---|---|
| 30 % | 10 ms | | B | |
| 60 % | 100 ms | | B | |
| 100 % | 10 ms | | B | |

Verdict: ____

## 8. Test facility and equipment

| Item | Manufacturer and model | Serial | Calibration due |
|---|---|---|---|
| | | | |
| | | | |
| | | | |
| | | | |
| | | | |

Test site: ____

### 8.1 Measurement uncertainty

State the expanded uncertainty U for each measurand, with coverage factor k = 2
(approximately 95 % confidence), and whether it is applied to the verdict.
EN 301 489-1 applies shared risk: the measured value is compared with the limit
and the uncertainty is not added.

| Measurand | Uncertainty |
|---|---|
| Radiated emission, 30 MHz – 1 GHz | |
| Radiated emission, 1 GHz – 6 GHz | |
| Conducted emission | |
| Radiated field strength, immunity | |

## 9. Deviations from the standard method

Record here, honestly and in full, every point at which the method used departs
from the standard: measurement distance, site validation, antenna calibration,
detector, chamber or open-area test site, amplifier capability, substitution
methods. State the effect of each deviation on the result and why the
conclusion still holds.

A deviation recorded and justified is defensible. A deviation omitted is what
turns a technical file into a liability, because it is the first thing an
authority finds when it repeats a measurement and gets a different number.

| # | Clause | Deviation | Effect on the result | Justification |
|---|---|---|---|---|
| | | | | |
| | | | | |
| | | | | |

### 9.1 Note on what this laboratory can and cannot perform

Fill this in before planning the work, not after. The tests below require
equipment and a site that most in-house laboratories do not hold. Where the
capability is absent, either the test is subcontracted or the deviation in
section 9 must carry the weight.

| Test | Needs | Available here |
|---|---|---|
| E1, E2 radiated emission | Validated test site (semi-anechoic chamber or OATS), calibrated antennas with antenna factors, measuring receiver with CISPR detectors | |
| E3 conducted emission | LISN for the DC port, measuring receiver | |
| I1 ESD | ESD generator, coupling planes | |
| I2 radiated immunity | Chamber, broadband amplifier, field probe, field uniformity validation | |
| I3, I4 EFT | Burst generator, capacitive clamp | |
| I5 conducted immunity | CDN, RF generator and amplifier | |
| I6 DC dips | Programmable DC source or dip simulator | |

## 10. Conclusion

To be completed once section 7 is complete.

The HomeMaster OpenTherm Thermostat, part number OTT-R1, hardware version V1.0,
firmware version 1.0.0, ____ the requirements of ETSI EN 301 489-1 V2.2.3,
ETSI EN 301 489-3 V2.3.2 and ETSI EN 301 489-17 V3.2.4, and therefore ____ the
essential requirement of Article 3.1(b) of Directive 2014/53/EU.

This report applies to the sample identified in section 2 and to series
production of that hardware version without significant modification.

Place: Ploiești, Romania

Date: ____

Name: ____

Position: ____

Signature: ____

---

## Annex A — Measurement plots

## Annex B — Photographs of the test setups

## Annex C — Monitoring logs from the immunity tests

---

© 2026 ISYSTEMS AUTOMATION S.R.L. · EMC-OTT-R1-2026-001
