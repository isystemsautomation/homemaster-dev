# AIO-422-R1 – Analog I/O & RTD Interface Module

The **AIO-422-R1** puts **analog in, analog out, and RTD** in one DIN-rail Modbus module: **4× 0–10 V inputs (16-bit)**, **2× 0–10 V outputs**, and **2× PT100/PT1000** for HVAC and process sensing/actuation. It connects to **MiniPLC/MicroPLC** (or any Modbus RTU master) over RS-485 and integrates with **ESPHome / Home Assistant** via the controller packages.

## Key advantages

- **4× AI 0–10 V (16-bit) + 2× AO 0–10 V + 2× RTD (PT100/PT1000)** in one DIN module — analog sensing and actuation for HVAC/process without stacking separate boards.
- Native ESPHome API via the MiniPLC/MicroPLC controller — no MQTT broker, no manual Modbus register mapping for the package entities.
- Local-first / edge-resilient — onboard logic keeps working if the network or Home Assistant is down.
- Open hardware (**CERN-OHL-W v2**) and firmware (**MIT**) — repairable, reproducible, no vendor lock-in.
- Standard **RS-485 Modbus RTU** — works with any Modbus master or industrial HMI/SCADA system, not locked to HomeMaster.
- Driverless **USB-C WebConfig** (Chrome, Edge, Opera); configuration persists in on-device flash (**LittleFS**).

## Quick Start (current version)

**Firmware shipped on new modules: `v0.1.0`**

```yaml
packages:
  aio1:
    url: https://github.com/isystemsautomation/homemaster-dev
    ref: main
    files:
      - path: AIO-422-R1/Firmware/v0.1.0/default_aio_422_r1_plc/default_aio_422_r1_plc.yaml
        vars:
          aio_prefix: "AIO#1"
          aio_id: aio_1
          aio_address: 3
```

## Version History

| Version | Config path (`path:`) | Date | Changes |
|--------|------------------------|------|-----------|
| **v0.1.0** | `AIO-422-R1/Firmware/v0.1.0/default_aio_422_r1_plc/default_aio_422_r1_plc.yaml` | 2026-06 | **Current — shipped on new modules** (first versioned release) |
| v0.2.0 | `AIO-422-R1/Firmware/v0.2.0/default_aio_422_r1_plc/default_aio_422_r1_plc.yaml` | — | **Beta** — in-tree build / register-map work; not the shipped firmware |

> **Shipped firmware is `v0.1.0`.** The reproducible Arduino / `sketch.yaml` build notes for **v0.2.0 (beta)** are here: [Build environment (reproducible)](../README.md#build-environment-reproducible) · [`sketch.yaml`](Firmware/v0.2.0/default_aio_422_r1/sketch.yaml).

---

## Key Features

- **4× Analog Inputs (0–10 V)**
  - High-resolution 16-bit ADC (ADS1115)
  - Ideal for sensors, potentiometers, control signals
  - **0–10 V only** (no 4–20 mA)

- **2× Analog Outputs (0–10 V)**
  - 12-bit DAC (MCP4725) with stable output
  - For actuators, dimmers, speed control
  - **0–10 V only** (no 4–20 mA)

- **2× RTD Inputs (PT100/PT1000)**
  - Based on MAX31865
  - Supports 2-, 3-, or 4-wire sensors
  - Accurate temperature readings with fault detection

- **User Interface**
  - 4 front-panel buttons, 4 user LEDs (plus power, RX, TX status LEDs)
  - USB Type-C for firmware updates & diagnostics

- **Modbus RTU over RS‑485**
  - Predefined register map
  - Fully compatible with  **MicroPLC**, and **MiniPLC**

---

## Smart Home & Automation Integration

The AIO-422-R1 works out of the box with MicroPLC or MiniPLC systems and can be fully integrated into **Home Assistant** via ESPHome for:

- Real-time analog signal monitoring
- Analog output automation (lighting, HVAC, pumps)
- Accurate temperature-based triggers using RTDs
- Custom automation logic via native ESPHome entities (no MQTT broker required)

---

## Firmware & Programming

- Programmable via **Arduino IDE** (recommended for full Modbus + WebConfig firmware)
- **WebConfig** over USB-C: open [ConfigToolPage.html](https://config.home-master.eu/AIO-422-R1/Firmware/v0.1.0/ConfigToolPage.html) in any Chromium-based browser (Chrome, Edge, Opera, Brave, Vivaldi; Chrome/Edge 89+, Opera 76+)

> Firefox: experimental only (Nightly with the Web Serial flag enabled). Safari and stable Firefox are not supported.

- Detailed setup guide for toolchain, libraries, build, and upload: **[Firmware/README.md](./Firmware/README.md)**
- **ESPHome** profiles also available in `Firmware/v0.1.0/default_aio_422_r1_plc/`

---

## Technical Specifications

| Parameter                     | Value                                |
|------------------------------|--------------------------------------|
| Power Supply                 | 24 V DC                              |
| Analog Inputs                | 4 × 0–10 V (ADS1115, 16-bit); **voltage only — no 4–20 mA** |
| Analog Outputs               | 2 × 0–10 V (MCP4725, 12-bit); **voltage only — no 4–20 mA** |
| RTD Inputs                   | 2 × PT100/PT1000 (MAX31865)         |
| RTD Wiring Support           | 2-, 3-, 4-wire                       |
| Input/Output Protection      | ESD, overvoltage                     |
| Communication Interface      | RS‑485 (Modbus RTU)                  |
| USB Port                     | USB Type‑C                           |
| User Interface               | 4 buttons; 7× LEDs (power, 4 user LEDs, RX, TX) |
| Dimensions                   | DIN-rail, 3 modules wide             |
| Compatibility                | MicroPLC, MiniPLC                    |

---

## Example Use Cases

- Control 0–10 V dimmable lighting
- Read analog pressure or humidity sensors
- Automate HVAC dampers and valves
- Monitor RTD-based industrial temperature points
- Implement PID control loops locally on MicroPLC

---

### RS-485 / Modbus RTU

All HomeMaster controllers and modules share the same RS-485 front end.

| Item | Value |
|---|---|
| Transceiver | MAX485CSA+T, half-duplex |
| Galvanic isolation | **None** — the transceiver shares the device's logic ground |
| Common-mode range | −7 V … +12 V referred to the device's own ground (MAX485 limit) |
| Terminals | A / B / COM |
| Surge protection | 3 × SMAJ6.8CA TVS (A–COM, B–COM, A–B) |
| Overcurrent | 2 × resettable PTC, 1.5 A hold, in series with A and B |
| EMI filtering | Common-mode choke on the A/B pair; COM referenced through 1 MΩ ∥ 4.7 nF |
| Idle state | Fail-safe biasing on board — do not add external bias resistors |
| Termination | 120 Ω at the two physical ends of the bus only |

**Bus wiring rules — apply to every device on the bus:**

- One twisted pair for A/B, 120 Ω characteristic impedance.
- Run **COM** to every node. Required, not optional: the ports are not isolated, and COM is what bounds the common-mode voltage the transceivers see.
- Prefer one power supply for the whole bus, distributed in star topology. With separate supplies, additionally tie the 0 V references together at a single point.
- Bond the cable shield to cabinet PE at one end only. Never land a shield on A, B or COM.
- Where the bus crosses into a different electrical installation with its own earthing reference — a utility or billing meter, another building, another cabinet's PE system — fit an external galvanic RS-485 isolator at that boundary. The on-board components are transient protection, not isolation, and will not survive a sustained ground-potential difference.

## Modbus RTU Register Map

The tables below match the **v0.2.0 (beta)** package map (`default_aio_422_r1_plc_full.yaml`). Controllers shipping with **v0.1.0** should use the register map and ESPHome package under `Firmware/v0.1.0/`.

### Discrete Inputs (FC02)

| Address | Signal |
|---------|--------|
| 1–4 | Button 1–4 pressed |
| 20–23 | LED 1–4 state |

### Holding Registers (FC03)

| Address | Signal | Format | Notes |
|---------|--------|--------|-------|
| 120–121 | RTD1–2 temperature | S_WORD | °C ×10 |
| 140–143 | AI1–4 field voltage | U_WORD | mV |
| 200–201 | AO0–1 raw DAC | U_WORD | 0–4095 |

> Full firmware details: [Firmware/README.md §12](./Firmware/README.md#12-modbus-map-reference)

## License

Licensing

This project uses a hybrid licensing model.

Hardware

Hardware designs (schematics, PCB layouts, BOMs) are licensed under:
CERN-OHL-W v2

Firmware & ESPHome Integration

All firmware, ESPHome configurations, and software components are licensed under:
MIT License

This ensures full compatibility with ESPHome and Home Assistant while protecting hardware designs.

See LICENSE files in each directory for full terms.

---

# Downloads

- **Firmware & examples:** `AIO-422-R1/Firmware/`
- **WebConfig (HTML page):** [ConfigToolPage.html](https://config.home-master.eu/AIO-422-R1/Firmware/v0.1.0/ConfigToolPage.html)

---

# Support

- **Official Support:** https://www.home-master.eu/support
- **WebConfig Tool (AIO-422-R1):** https://config.home-master.eu/AIO-422-R1/Firmware/v0.1.0/ConfigToolPage.html

---

> 🔧 **HOMEMASTER – Modular control. Custom logic.**

## Compliance & Certifications

The AIO-422-R1 module is CE marked. **ISYSTEMS AUTOMATION S.R.L.** (HomeMaster® brand)
maintains the technical documentation and a signed EU Declaration of Conformity (DoC).

### Applicable EU directives

- **EMC Directive 2014/30/EU** — EN 55032:2015 + AC:2016-07 + A11:2020 + A1:2020 (Class B emissions),
  EN 55035:2017 + A11:2020 (immunity); tested by Idvorsky Laboratories Ltd., Belgrade, Serbia
  (Job #1648, 20 April 2026)
- **RoHS Directive 2011/65/EU** — EN IEC 63000 technical documentation
- **Low Voltage Directive 2014/35/EU** — does not apply. This is a SELV-only product
  powered from 24 V DC nominal (input voltage well below 75 V DC, the lower threshold of
  LVD scope per Annex I of the Directive). The product has no mains-capable terminals
  (no AC input, no relay outputs). No LVD test report is required nor has been issued.

### Compliance documents

| Document | File |
|---|---|
| EU Declaration of Conformity (DoC) | [DoC_AIO-422-R1.pdf](./Manuals/DoC_AIO-422-R1.pdf) |
| Datasheet | [AIO-422-R1_Datasheet.pdf](./Manuals/AIO-422-R1_Datasheet.pdf) |

### Trademark

**HomeMaster®** is a registered European Union trademark of ISYSTEMS AUTOMATION S.R.L.,
EUTM No. 019082911, registered with EUIPO on 15 January 2025.

---

**Manufacturer:** ISYSTEMS AUTOMATION S.R.L. (HomeMaster® brand)
**Registered office (registered office):** Str. Domnisori, Nr. 81, Bl. 62, Scara A, Etaj 3, Ap. 12, 100284 Ploiesti, Jud. Prahova, Romania
**Office / Contact address:** Diligentei 18, Ploiesti, Romania
**CUI / VAT:** RO 21537032
**EUID:** ROONRC.J2007000919293
**Telephone:** +40 747 757 798
**Website:** [https://www.home-master.eu](https://www.home-master.eu)
