# CE Technical File — HomeMaster OpenTherm Thermostat (OTT-R1)

**Document No.: TF-OTT-R1-V1.0** · ISYSTEMS AUTOMATION S.R.L.

This is the index of the technical documentation required by Annex V of
Directive 2014/53/EU and Article 7(b) of Directive 2011/65/EU. It must be kept
for **10 years** after the last unit is placed on the market, and produced to a
market surveillance authority on request.

Status column: **HAVE** — the document exists; **MAKE** — it has to be written;
**GET** — it has to be obtained from a supplier or a laboratory.

## A. Product identification

| # | Document | Status | Note |
|---|---|---|---|
| A1 | General description of the product | MAKE | The README and datasheet cover this; extract a one-page version |
| A2 | Photographs of the product, inside and out | MAKE | Front, back, PCB top and bottom, enclosure open |
| A3 | Product label artwork | MAKE | Must carry: CE mark, manufacturer name and address, part number OTT-R1, hardware version, 24 V DC rating, WEEE crossed-out bin, batch or serial |
| A4 | Packaging artwork and markings | MAKE | |

## B. Design documentation

| # | Document | Status | Note |
|---|---|---|---|
| B1 | Schematics — MCU board | HAVE | KiCad source; export to `Schematics/MCUBoard.pdf` |
| B2 | Schematics — display board | HAVE | KiCad source; export to `Schematics/DisplayBoard.pdf` |
| B3 | PCB layout, all layers | HAVE | KiCad source |
| B4 | Bill of materials with manufacturer part numbers | MAKE | Also feeds the RoHS file (E1) |
| B5 | Block diagram and description of operation | MAKE | |
| B6 | Mechanical drawings of the enclosure | HAVE | Italtronic EYE BOX part drawings: P61610001G, P61610001V, P61610002B, P61610014A |
| B7 | Firmware source and build identification | HAVE | `Firmware/thermostat.yaml`, version 1.0.0, in the public repository |

## C. Radio — RED Articles 3.1(b) and 3.2

| # | Document | Status | Note |
|---|---|---|---|
| C1 | ESP32-WROOM-32U-N16 datasheet | GET | Espressif |
| C2 | ESP32 EU type-examination certificate 172141367/AA/02, Kiwa NB 0063 | HAVE | Already in the gateway's file; copy it in |
| C3 | ESP32 module integration conditions and antenna specification | GET | Espressif integration guide; the external antenna used must be within the module's certified antenna list, or the product needs its own EN 300 328 testing |
| C4 | HLK-LD2410C datasheet | GET | Hi-Link |
| C5 | HLK-LD2410C CE test reports — EN 305 550-2, EN 301 489-3 | **GET — blocking** | Supplier declarations alone are not evidence. If the supplier cannot produce full reports, the finished product must be tested |
| C6 | Antenna specification and gain, 2.4 GHz external antenna | MAKE | |
| C7 | EN 300 328 assessment for the finished product | MAKE | Needed if the external antenna differs from the module's certified one |

## D. EMC and safety — RED Articles 3.1(a) and 3.1(b)

| # | Document | Status | Note |
|---|---|---|---|
| D1 | EMC test report — EN 301 489-1, EN 301 489-17, EN 301 489-3 | **GET — blocking** | Accredited laboratory. The gateway used Idvorsky Laboratories Ltd., Belgrade |
| D2 | Safety assessment — EN IEC 62368-1 | MAKE | Energy-source classification, touch temperature of the front panel, single-fault behaviour of the 24 V input. No mains circuit, so no dielectric programme |
| D3 | RF exposure assessment — EN 62479 | MAKE | Low-power exemption calculation for both radios |
| D4 | Thermal measurements at 40 °C ambient | MAKE | Also validates the room-temperature sensor offset claimed in the manual |

## E. RoHS — Directive 2011/65/EU

| # | Document | Status | Note |
|---|---|---|---|
| E1 | RoHS compliance matrix per EN IEC 63000 | MAKE | One line per BOM item with the supplier's RoHS declaration |
| E2 | Supplier RoHS declarations | GET | |

## F. Cybersecurity — RED Article 3(3)(d), EN 18031-1

| # | Document | Status | Note |
|---|---|---|---|
| F1 | CYB-001 — security risk assessment | MAKE | Adapt the gateway's |
| F2 | CYB-002 — verification records | MAKE | Adapt the gateway's |
| F3 | Update mechanism description | HAVE | `update:` over HTTPS with a signed manifest; `ota:` over the ESPHome API |
| F4 | Secure provisioning description | HAVE | Improv over BLE and serial; API encryption key replaced by the user on Take Control |

## G. Declaration and user information

| # | Document | Status | Note |
|---|---|---|---|
| G1 | EU Declaration of Conformity | DRAFT | `DoC_OTT-R1.md` — cannot be signed until C5, D1, D2, D3 exist |
| G2 | Installer and user manual | DRAFT | `OpenThermThermostat_UserManual.md` |
| G3 | Datasheet | DRAFT | `OpenThermThermostat_Datasheet.md` |
| G4 | Safety and disposal information in the language of each market | MAKE | Romanian is mandatory for the Romanian market; English for the rest of the EU unless a member state requires its own |

## H. Other obligations that are not part of the technical file

| # | Obligation | Status | Note |
|---|---|---|---|
| H1 | WEEE registration and producer number | CHECK | Already held for the existing products; confirm the new product category is covered |
| H2 | Packaging EPR registration | CHECK | Same |
| H3 | GPSR Regulation (EU) 2023/988 — economic operator details on the product or packaging | MAKE | Name, address, email or web contact |
| H4 | Product traceability — batch or serial number | MAKE | |
| H5 | EU declaration published or supplied with the product | MAKE | Per RED Art. 10(9), either the full DoC or a simplified DoC with a URL must accompany the product |

## The short version

Three things block the CE mark, in this order:

1. **The 24 GHz radar's conformity evidence.** No notified-body certificate
   exists. Get the supplier's reports or test the product.
2. **An accredited EMC test report** for the finished product.
3. **The EN IEC 62368-1 and EN 62479 assessments**, which can be done in-house.

Everything else on this list is paperwork that can be written in parallel.
