# AIO-422-R1 – Analog I/O & RTD Interface Module

The **AIO-422-R1** is a high-precision analog I/O expansion module designed for **home automation**, **HVAC**, **environmental monitoring**, applications. It connects to **MicroPLC** or **MiniPLC** controllers via **RS-485 (Modbus RTU)** and seamlessly integrates with **ESPHome** and **Home Assistant** for analog sensing and control in smart automation systems.

## 🚀 Quick Start (current version)

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

## 📦 Version History

| Version | Config path (`path:`) | Date | Changes |
|--------|------------------------|------|-----------|
| **v0.1.0** | `AIO-422-R1/Firmware/v0.1.0/default_aio_422_r1_plc/default_aio_422_r1_plc.yaml` | 2026-06 | First versioned release |

> **Reproducible firmware build (v0.2.0):** [Build environment (reproducible)](../../README.md#build-environment-reproducible) · [`sketch.yaml`](Firmware/v0.2.0/default_aio_422_r1/sketch.yaml)

---

## 🔧 Key Features

- **4× Analog Inputs (0–10 V)**
  - High-resolution 16-bit ADC (ADS1115)
  - Ideal for sensors, potentiometers, control signals

- **2× Analog Outputs (0–10 V)**
  - 12-bit DAC (MCP4725) with stable output
  - For actuators, dimmers, speed control

- **2× RTD Inputs (PT100/PT1000)**
  - Based on MAX31865
  - Supports 2-, 3-, or 4-wire sensors
  - Accurate temperature readings with fault detection

- **User Interface**
  - 4 front-panel buttons + status LEDs
  - USB Type-C for firmware updates & diagnostics

- **Modbus RTU over RS‑485**
  - Predefined register map
  - Fully compatible with  **MicroPLC**, and **MiniPLC**

---

## 🧠 Smart Home & Automation Integration

The AIO-422-R1 works out of the box with MicroPLC or MiniPLC systems and can be fully integrated into **Home Assistant** via ESPHome for:

- Real-time analog signal monitoring
- Analog output automation (lighting, HVAC, pumps)
- Accurate temperature-based triggers using RTDs
- Custom automation logic via MQTT or native ESPHome entities

---

## 📦 Firmware & Programming

- Programmable via **Arduino IDE** (recommended for full Modbus + WebConfig firmware)
- Detailed setup guide for toolchain, libraries, build, and upload: **[Firmware/README.md](./Firmware/README.md)**
- **ESPHome** profiles also available in `Firmware/v0.1.0/default_aio_422_r1_plc/`

---

## ⚙️ Technical Specifications

| Parameter                     | Value                                |
|------------------------------|--------------------------------------|
| Power Supply                 | 24 V DC                              |
| Analog Inputs                | 4 × 0–10 V (ADS1115, 16-bit)         |
| Analog Outputs               | 2 × 0–10 V (MCP4725, 12-bit)         |
| RTD Inputs                   | 2 × PT100/PT1000 (MAX31865)         |
| RTD Wiring Support           | 2-, 3-, 4-wire                       |
| Input/Output Protection      | ESD, overvoltage                     |
| Communication Interface      | RS‑485 (Modbus RTU)                  |
| USB Port                     | USB Type‑C                           |
| Dimensions                   | DIN-rail, 3 modules wide             |
| Compatibility                | MicroPLC, MiniPLC                    |

---

## 🏠 Example Use Cases

- Control 0–10 V dimmable lighting
- Read analog pressure or humidity sensors
- Automate HVAC dampers and valves
- Monitor RTD-based industrial temperature points
- Implement PID control loops locally on MicroPLC

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
| EU Declaration of Conformity (DoC) | [DoC-AIO-422-R1-V1.0.pdf](./Manuals/DoC-AIO-422-R1-V1.0.pdf) |
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
