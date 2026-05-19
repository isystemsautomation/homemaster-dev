# HomeMaster Completes EMC, LVD and RED Conformity Assessment — Production Hardware Now Shipping

Hey everyone,

Six months ago we walked into an accredited test laboratory with the whole HomeMaster system, and some early-revision boards exposed weak points under EMC and electrical-safety testing. Today every product in the lineup ships with a signed EU Declaration of Conformity issued by HomeMaster as the manufacturer. The CE marking is on. We're shipping.

This post is the closure of the story we started in the [November pre-compliance write-up](https://www.reddit.com/r/HomeMaster/comments/1p928w3/homemaster_completes_intensive_precompliance/), with the engineering fixes we walked through in the [January follow-up](https://www.reddit.com/r/HomeMaster/) baked into production hardware. Same eleven modules on the bench, same tests, this time the verdicts come back in green.

Here's the full breakdown.

## What Changed Between Rounds (short version)

The November punch-list became roughly four months of engineering work. Headlines:

* **Power supply EMC hardening** on every module — ferrite beads on the 24 V input, improved LC filtering on the DC/DCs, tightened HF current loops.
* **USB-C ESD path + ground split** — strengthened shell grounding, plus an internal `GND` / `GND_USB` split connected only through filtering. No more RS-485 drops on ESD events.
* **Creepage and clearance** widened between SELV (24 V) and any mains-related circuitry — this is the fix for the 3.75 kV dielectric-strength breakdown we hit in November.
* **Isolation upgrades** system-wide: opto-isolators 2.5 kV → **8 kV**, isolated DC/DCs 1.5 kV → **6 kV**.
* **STR / RGB LED filtering**: dedicated LC stage on the external LED PSU + per-channel `BLM31PG601SN1L` ferrite bead with 4.7 nF + 27 Ω RC damping at each output.
* **DIM AC path**: common-mode chokes (`ACM1211-102-2PL-TL01`) on L/N, X-cap 100 nF across L–N, 100 nF + 100 Ω RC damping on the dimmer output.
* **Relay-equipped modules**: split `+5V` (logic) and `+5V_RELAY` (coils) rails, joined only through filtering.
* **Extra chokes** on RS-485 and digital input lines for general EMC margin.

We also added a TinySA spectrum analyser to the internal validation setup between rounds and ran old-vs-new spectrum comparisons in-house — clear reduction in 30–150 MHz noise on the new boards before they went back to Belgrade.

## All Eleven Modules on One Panel

Same idea as November — bring complete, real-world installations, not isolated modules — but with one key change. In the November round we had **two separate metal-backed panels**, one per system, wired and powered independently. For this round we consolidated everything onto **a single panel**: all eleven products on the same backplate, sharing the same chassis ground, the same earth bonding, and the same 24 V SELV bus.

The two systems below are now a logical grouping rather than a physical one:

**SYSTEM 1 (MiniPLC setup):** MiniPLC, AIO-422-R1, RGB-621-R1, STR-3221-R1, OpenTherm Gateway
**SYSTEM 2 (MicroPLC setup):** MicroPLC, DIO-430-R1, WLD-521-R1, ENM-223-R1, DIM-420-R1, ALM-173-R1

The RS-485 bus is split into two segments off the respective master (MiniPLC for SYSTEM 1, MicroPLC for SYSTEM 2). Real loads on the dimmer and the LED driver, a current transformer on the energy meter, 1-Wire and RTD sensors on the metering channels, Modbus traffic on both segments, Wi-Fi monitoring throughout. Every module had at least one active I/O channel doing real work. Recommended cabling — shielded RS-485, shielded analog, shielded 3-core 1-Wire — was used everywhere.

## The EMC Campaign

**Laboratory:** Idvorsky Laboratories Ltd., Belgrade — ATS-accredited (ATS 01-404), ISO/IEC 17025, ilac-MRA recognised.
**Dates:** 23 February – 20 March 2026.
**Report:** #1648.

EMC testing covers the usual list:

✔ Noise sent back into the power lines
✔ Radio noise radiated into the air
✔ Immunity to strong RF fields
✔ Noise injected directly into cables
✔ Fast electrical spikes (EFT/Burst)
✔ Electrostatic discharge shocks

Many of these tests involve kilovolt-level transients. Six months ago some tests exposed weak points in the early hardware revisions. This time the revised production hardware completed the campaign successfully.

### 1. Conducted RF Emissions (150 kHz – 30 MHz)

**Standard:** EN 55032 (Class B).

**What the lab did:** powered the whole system through a LISN, scanned with a spectrum analyser, RS-485 traffic and loads active.

**Purpose:** make sure switching regulators and cables don't inject too much noise into the power lines.

**What was observed:** below the Class B quasi-peak and average limits across the whole sweep, with comfortable margin. The DC/DC layout changes from the November punch-list — tighter HF loops, RC snubbers, ferrite filtering on the 24 V rail — did what they were supposed to do.

**Verdict: PASS.**

### 2. Radiated RF Emissions (30 MHz – 2.7 GHz)

**Standard:** EN 55032 (Class B), method EN 55016-2-3.

**What the lab did:** unlike the other tests, radiated emissions were measured in two separate configurations rather than on the full consolidated panel. First configuration: every expansion module driven from a single MicroPLC master, with the OpenTherm Gateway also on the panel. Second configuration: MiniPLC placed in the chamber on its own, isolated from all other modules. Both configurations swept with the BiLog antenna at 3 m, height swept 1 / 2.5 / 4 m, both polarisations.

**Purpose:** ensure the system doesn't unintentionally radiate RF into the environment.

**What was observed:** the 30–80 MHz region that produced several peaks in November is now clean in both configurations. Above 1 GHz the peak level stayed more than 10 dB under the limit across the full 1–2.7 GHz band.

**Verdict: PASS.**

### 3. RF Immunity, Swept (80 MHz – 1 GHz)

**Standard:** EN 55035 / EN IEC 61000-4-3, 3 V/m AM-modulated.

**What was observed:** the narrow-band RS-485 disturbances we saw in November at 115–175 MHz and ~514 MHz are completely gone — the shielded RS-485 cable specification eliminated them. No resets, no Modbus drops, nothing.

**Verdict: PASS, criterion A.**

### 4. RF Immunity, Spot (1.8 / 2.6 / 3.5 / 5 GHz)

**Standard:** same. 3 V/m at four spot frequencies covering the GSM / LTE / Wi-Fi bands.

**What was observed:** no changes at any of the four frequencies.

**Verdict: PASS, criterion A.**

### 5. Conducted RF Immunity (150 kHz – 80 MHz)

**Standard:** EN 55035 / EN IEC 61000-4-6.

**What the lab did:** CDN into AC mains and RS-485, EM clamp into analog, digital, PWM, RTD, 1-Wire cables.

**What was observed:** the AIO analog-input deviations at 15–21 MHz and 73–80 MHz that we had in November are gone — caused by unshielded twisted-pair analog cabling, fixed by the shielded analog cable that now goes into the manual. All eight injected ports stayed inside criterion A across the full sweep.

**Verdict: PASS, criterion A on every port.**

### 6. EFT / Burst Immunity

**Standard:** EN 61000-4-4. ±1 kV on AC mains via CDN, ±0.5 kV on signal ports via capacitive clamp. 5 kHz, 75 spikes per burst, 60 s per polarity.

**What was observed:** this is the test that exposed problems in November. DIO module showed occasional resets back then, DIM module showed lamp flicker. The redesigned DIO regulator section and the redesigned DIM gate drive are both fully stable now. Although criterion B performance was acceptable under the applicable immunity requirements for this equipment category, all tested ports maintained criterion A operation throughout the entire test sequence — no resets, no Modbus drops, no input glitches, no flicker.

**Verdict: PASS, criterion A with margin.**

### 7. Electrostatic Discharge (ESD)

**Standard:** EN IEC 61000-4-2. ±4 kV contact discharges to HCP, VCP, the board, every USB-C shell, and the antenna connectors. ±2 / ±4 / ±8 kV air discharges to displays, LEDs, buttons, enclosures.

**What was observed:** the November-era weakness is closed. November sometimes required a manual restart after −4 kV contact on the USB connector. This round, the worst observation on any USB shell was a brief self-recovering flicker of the test-load lamp — well inside criterion B. Air discharge at ±8 kV produced one observable spark on the OpenTherm enclosure (self-recovered); everywhere else there was no spark.

**Verdict: PASS, criterion B.**

## The LVD Programme — On Our Own Bench

Module A of Directive 2014/35/EU lets the manufacturer self-declare LVD without a notified body, as long as the testing is real. We built an in-house compliance lab for that: GW Instek GPT-9804 hi-pot / IR tester, calibrated true-RMS DMM with K-type probe, calibrated calliper, programmable AC source. All ISO 17025-traceable.

**Standard applied:** EN 62368-1:2020 + A11:2020.
**Construction class:** Class II throughout — no protective earth, double / reinforced insulation between mains-side and SELV.
**Hazard classification (clause 5):** mains and relay-output contacts at ES3, the 24 V SELV bus at ES1.

**Scope:** nine products — every module where mains can be present, either through a mains input or through relay output contacts that can switch user-supplied 250 V AC. The two outside the LVD scope: AIO-422-R1 (pure analog and SELV) and STR-3221-R1 (MOSFET LED outputs only).

### What the lab bench did

**Hi-pot (electric strength, clause 5.4.9).** A 4243 V DC dielectric-strength test was applied for 60 s across every SELV-to-mains isolation barrier, corresponding to the reinforced-insulation test level required by EN 62368-1. No breakdown, no arc — every barrier passed the reinforced-insulation electric-strength requirement with substantial margin.

**Insulation resistance (clause 5.4.10).** 500 V DC for 60 s on the same barriers. Above the instrument's 9999 MΩ over-range against the standard's 7 MΩ minimum at the reinforced-insulation level.

**Touch current (clause 5.7).** At 1.06× rated mains against external earth, both polarities, every accessible terminal. Below the 0.25 mA r.m.s. limit.

**Temperature rise (clauses 5.4.1.4 / 9).** Components instrumented with K-type thermocouples, EUT in worst-case operating mode, measurements stabilised over 1 h, corrected mathematically to 40 °C rated ambient per Annex B.3. Below the lower of the datasheet T_max and the EN 62368-1 Table 9 limit on every component.

**Single-fault conditions (Annex B.4).** Eight defined faults: mains primary short, AC/DC output short, relay output short, all relays simultaneously under load, DI over-voltage, AI over-voltage, ESP32 watchdog disabled, mains over-voltage transient. No fire, no electric-shock hazard, no glass-transition of insulation, no ES2/ES3 emerging at user-accessible terminals.

**PCB creepage and clearance (clause 5.4.2, Tables 11 and 14).** Working voltage up to 250 V r.m.s., OVC II, PD2, FR-4 group IIIa. Each insulation path measured on the assembled board with a creepage gauge, against the Reinforced minimums (≥ 3.0 mm clearance, ≥ 6.4 mm creepage). PASS on every path.

### An honest note from the bench

During the first hi-pot pass, a sample broke down at ~2.1 kV on the relay-to-SELV barrier — and failed again during retest at a lower threshold. Turned out to be the very first PCB revision: an early prototype build with a known design issue around the relay-output area, never released into production. Quarantined, labelled "DO NOT USE", retested on the current production-revision PCB — the production board passed the reinforced-insulation requirement with substantial margin. From now on every test report we issue cites the PCB revision explicitly.

## RED and RoHS — Inherited and Documented

The three Wi-Fi products (MiniPLC, MicroPLC, OpenTherm Gateway) all use the same radio: **Espressif ESP32-WROOM-32U-N16**. That module carries its own EU type-examination under Module B of Directive 2014/53/EU, performed by **Kiwa Nederland B.V., Notified Body 0063**, certificate **172141367/AA/02** issued 2 March 2023. Covers EN 300 328 V2.2.2, EN 301 489-1 V2.2.3, EN 301 489-17 V3.2.4. We operate the module within its certified conditions of use and inherit the radio compliance.

RoHS compliance is documented through supplier declarations and material traceability in accordance with EN IEC 63000.

## Who Needs What

LVD applies anywhere mains can be present — that includes products whose **relay contacts** can switch user-supplied mains, not just products with a mains input. RED applies to anything with Wi-Fi. EMC and RoHS apply to everything:

| Product | EMC | LVD | RED | RoHS |
|---|---|---|---|---|
| MiniPLC | ✓ | ✓ (AC variant + 6 relays) | ✓ (Wi-Fi) | ✓ |
| MicroPLC | ✓ | ✓ (1 relay) | ✓ (Wi-Fi) | ✓ |
| OpenTherm Gateway | ✓ | ✓ (AC variant + 1 relay) | ✓ (Wi-Fi) | ✓ |
| DIM-420-R1 | ✓ | ✓ (AC dimming) | — | ✓ |
| ENM-223-R1 | ✓ | ✓ (3-phase mains + 2 relays) | — | ✓ |
| ALM-173-R1 | ✓ | ✓ (3 relays) | — | ✓ |
| DIO-430-R1 | ✓ | ✓ (3 relays) | — | ✓ |
| RGB-621-R1 | ✓ | ✓ (1 relay) | — | ✓ |
| WLD-521-R1 | ✓ | ✓ (2 relays) | — | ✓ |
| AIO-422-R1 | ✓ | — | — | ✓ |
| STR-3221-R1 | ✓ | — | — | ✓ |

Nine of eleven products under LVD scope. Only AIO (pure analog and SELV) and STR (MOSFET LED outputs only) are outside it.

## What All of This Means

After pushing the entire HomeMaster ecosystem through two rounds of pre-compliance, a full accredited-lab EMC campaign, and an in-house LVD programme on every mains-touching product, the big picture in plain language:

**The architecture proved itself.** Every item on the November punch-list is closed on production hardware. DIO resets and DIM flicker under EFT — gone. AIO conducted-RF deviations — gone. USB-C ESD path — no more manual restarts. Relay-to-SELV isolation barriers now pass reinforced-insulation dielectric testing with substantial margin.

**The compliance documentation is complete and issued.** Eleven products, eleven Declarations of Conformity. EMC supported by Idvorsky Labs Report #1648. LVD supported by the in-house lab under Module A against EN 62368-1. RED via the pre-certified ESP32 module. RoHS via the EN IEC 63000 technical file.

**Compliance maturity is built into the process.** Each certified hardware revision is tied to controlled PCB revision identifiers, archived manufacturing files, compliance test records, and versioned technical documentation inside the product technical file. Every DoC references the specific hardware revision under which the assessment was performed.

**Production hardware is now shipping.** Each product ships with the certified hardware revision, updated user documentation, product labelling, and the EU Declaration of Conformity. Orders open at [home-master.eu](https://www.home-master.eu/).

## What's Next

The compliance assessment phase for the current hardware revisions is now complete. From here our focus moves back to the product side:

1. **Production ramp** — first production batches of all eleven modules are shipping now.
2. **Documentation rollout** — datasheets, manuals, DoCs and downloadable technical-file summaries on every product page.
3. **Build guides** — more real-installation write-ups like the [smart chimney with MicroPLC](https://www.home-master.eu/build/smart-chimney-microplc).
4. **Next hardware** — two new module concepts are already on the bench. More on those once the first revisions come back.

Full long-form write-up on the site: **[home-master.eu/build/march-2026-certification-retest](https://www.home-master.eu/build/march-2026-certification-retest)**

Thanks to everyone who followed along through the November round and the January engineering update, and asked the sharp questions. Happy to answer more in the comments.
