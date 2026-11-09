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
| C5 | HLK-LD2410C CE test reports — EN 300 440, EN 301 489-3 | **GET — blocking** | See C5 note below. Supplier declarations alone are not evidence |
| C6 | Antenna specification and gain, 2.4 GHz external antenna | MAKE | |
| C7 | EN 300 328 assessment for the finished product | MAKE | Needed if the external antenna differs from the module's certified one |

### C5 note — what exists for the LD2410 family, checked 09/11/2026

**Hi-Link publishes nothing usable.** The LD2410C product page carries one
sentence, "24GHz ISM band, compliant with FCC and CE spectrum regulations", and
names no standard, no report number and no laboratory. The company's
Certification page is a single image with no readable text and no linked
documents. The documents link for the LD2410C is a Google Drive folder whose
listing could not be read.

**FCC filings exist for the family, under grantee code 2AD56:**

| FCC ID | Variant |
|---|---|
| 2AD56HLK-LD2410-P | LD2410 |
| 2AD56HLK-LD2410B-P | LD2410B |

No filing for the **LD2410C** surfaced. If the C variant has no FCC grant of its
own, that is worth knowing before relying on anything said about the family.

The FCC filings are public and include the full test reports — measured field
strength, occupied band, the test laboratory. Open them at
`fccid.io/2AD56HLK-LD2410-P` and `fccid.io/2AD56HLK-LD2410B-P`. Those sites
refuse automated retrieval but open normally in a browser.

**What the FCC data is and is not worth.** It cannot be cited in an EU
declaration — different regime, different limits, different standard. It is
worth pulling anyway, as engineering input before paying for a test:

- it gives the **measured lower band edge**, which settles whether the module
  transmits below 24.05 GHz and therefore outside the EU radiodetermination
  allocation — the single question most likely to sink the product;
- it gives the measured field strength, from which the EN 300 440 margin can be
  estimated;
- it names a laboratory that has already measured this module at 24 GHz.

**What to ask Hi-Link for** (sales@hlktech.com) — ask for reports, not
declarations, and say so:

1. EU Declaration of Conformity for the LD2410C.
2. Test report against EN 300 440 (radio spectrum).
3. Test report against EN 301 489-3 (EMC).
4. Test report against EN 62479 or EN 62311 (RF exposure).
5. Module integration conditions: antenna, keep-out, ground plane, supply
   filtering — the conditions under which any of the above remains valid.
6. Confirmation of the actual transmitted band, lower edge included.

**If they do not produce reports**, there are two routes and they should be
weighed before the PCB is frozen: test the finished thermostat against
EN 300 440 and EN 301 489-3, or change to a 24 GHz module whose supplier
publishes EU evidence. The second is cheaper if it is decided early and much
more expensive if it is decided late.

## D. EMC and safety — RED Articles 3.1(a) and 3.1(b)

| # | Document | Status | Note |
|---|---|---|---|
| D1 | EMC test report — EN 301 489-1, EN 301 489-17, EN 301 489-3 | **GET — blocking** | Accredited laboratory. The gateway used Idvorsky Laboratories Ltd., Belgrade. See D.0 for the scope |
| D2 | Safety assessment — EN IEC 62368-1 | MAKE | Energy-source classification, touch temperature of the front panel, single-fault behaviour of the 24 V input. No mains circuit, so no dielectric programme |
| D3 | RF exposure assessment — EN 62479 | MAKE | Low-power exemption calculation for both radios |
| D4 | Thermal measurements at 40 °C ambient | MAKE | Also validates the room-temperature sensor offset claimed in the manual |
| D5 | MiniPLC system campaign — configuration list, emission plots, EUT photographs | GET | From the existing campaign. See D.0; this is what carries the display across |
| D6 | Equivalence note — MiniPLC display section and 24 V converter vs. this board | MAKE | One page. See D.0 |

### D.0 Reusing the existing test reports — what carries across and what does not

Two existing campaigns are relevant: the OpenTherm Gateway's reports, and the
MiniPLC system campaign, in which a complete installation — MiniPLC with its
display plus the expansion modules — was tested as one assembly and passed.

Read-across between variants of a product family is normal practice and
laboratories accept it. It is bounded by one rule: the tested sample must
represent the worst case. What is **removed** from a tested configuration
carries across without argument. What is **added** carries across only if an
equivalent was present in some tested configuration.

| Feature of the thermostat | Covered by | Carries across? |
|---|---|---|
| Relay board, mains PSU, 1-Wire — all absent | Gateway, by removal | Yes. Removal only, and it drops the mains conducted-emission path entirely |
| ESP32 section, OpenTherm interface | Gateway | Yes, if the board section is electrically identical |
| Colour display and its SPI bus | MiniPLC system campaign — near-identical panel, same bus architecture, measured clean | Yes |
| 24 V DC-DC converter | MiniPLC system campaign — **this was the one emitting part**; everything else was quiet | Only if the same converter, the same switching frequency and an equivalent layout and filter are used here |
| 24 GHz presence radar | Nothing | **No.** A radio function neither tested assembly has |

**What this leaves.** The display is not a risk and should not be treated as
one: the MiniPLC campaign covers that architecture and the measurements put the
emissions elsewhere. The real delta is two items:

1. **The 24 V DC-DC converter on this board.** It is the known emitter from the
   MiniPLC work. If the thermostat uses the same part at the same switching
   frequency with an equivalent input filter and layout, the read-across holds
   and the technical file should say so explicitly, with the schematic
   comparison attached. If the converter, its frequency or its filtering
   differ, this is the item to measure.
2. **The radar.** See C5.

**What the technical file needs to make the read-across stick on paper.** An
auditor will not accept "the display was tested" as a sentence; they will ask
for the document. Pull from the MiniPLC campaign:

- the **test configuration list** or EUT description page, showing the display
  was present, powered and active during the emission runs;
- the **radiated emission plots**, which are what support the claim that the
  SPI bus and panel are quiet;
- the EUT photographs.

File those alongside a one-page equivalence note comparing the MiniPLC display
section and this one — panel, controller, clock rate, cable length, routing —
and the same for the 24 V converter. That note is the whole argument, and it is
cheap to write.

**One honest caveat about the standard.** The existing reports were issued
against EN 55032 and EN 55035. Equipment containing a radio transmitter is
assessed under the RED, where the EMC set is EN 301 489-1 with the relevant
part (-17 for Wi-Fi, -3 for the radar). The measurements largely overlap, and
a laboratory can usually map existing 55032/55035 data onto 301 489 rather than
repeat it — but ask them to confirm that in writing, and get the mapping into
the file.

**What to send the laboratory.** An assembled unit in its enclosure, the 24 V
supply that will ship or be recommended with it, the antenna that will be
fitted, the firmware that will ship, and both existing campaigns' reports with
the equivalence note. Ask them to quote a delta: the radar, and the 24 V
converter if it is not the MiniPLC's.

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

1. **The 24 GHz radar's conformity evidence.** The only genuinely open item.
   No notified-body certificate exists and Hi-Link publishes nothing. See the
   C5 note for what to request and what the public FCC filings are worth.
2. **A delta EMC campaign** — the radar, plus the 24 V converter if it is not
   the one already measured on the MiniPLC. The display and the SPI bus are
   covered; see D.0.
3. **The EN IEC 62368-1 and EN 62479 assessments**, which can be done in-house.
4. **The equivalence note and the MiniPLC configuration documents** that make
   item 2 small instead of large. Cheap to write, and nothing else in this
   file saves as much.

Everything else on this list is paperwork that can be written in parallel.
