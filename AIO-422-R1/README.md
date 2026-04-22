# 🚧 Project Status: Under Active Development & Testing

> **Important Notice:** This documentation, hardware designs, and firmware are for the **pre-release version** of the HomeMaster system. All information is preliminary and may contain errors or be subject to change.
>
> - **Hardware:** Modules are currently in the prototyping and testing phase. Final production versions may differ.
> - **Firmware:** Firmware is under active development and is considered **beta**. Features, configurations, and stability are being refined.
>
> Please use this information for evaluation and development purposes only. Check the [Releases page](../../releases) for the latest stable versions and updates.

---
# AIO-422-R1 – Analog I/O & RTD Interface Module

The **AIO-422-R1** is a high-precision analog I/O expansion module designed for **home automation**, **HVAC**, **environmental monitoring**, applications. It connects to **MicroPLC** or **MiniPLC** controllers via **RS-485 (Modbus RTU)** and seamlessly integrates with **ESPHome** and **Home Assistant** for analog sensing and control in smart automation systems.

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

- Comes preloaded with **MicroPython firmware**
- Programmable via:
  - **MicroPython**
  - **C/C++**
  - **Arduino IDE**
- Flexible customization for edge logic, timers, PID loops, etc.

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

This project uses a dual licensing model.

Hardware (schematics, PCB layouts, BOMs) is licensed under:
CERN-OHL-W v2

All firmware, ESPHome configurations, and software components are licensed under:
MIT License

See LICENSE files in each directory for full terms.

---

> 🔧 **HOMEMASTER – Modular control. Custom logic.**
