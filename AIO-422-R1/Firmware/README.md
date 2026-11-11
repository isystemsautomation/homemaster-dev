# AIO-422-R1 — Firmware Developer Guide

This document describes how to set up a **new computer**, open the source code, modify the firmware, build it, and upload it to an **AIO-422-R1** module (MCU **RP2350**).

---

## 1. Project Contents

| Path | Purpose |
|------|---------|
| `default_aio_422_r1/default_aio_422_r1.ino` | Main firmware (Arduino) |
| `ConfigToolPage.html` | USB WebConfig tool (Web Serial) |
| `default_aio_422_r1_plc/*.yaml` | ESPHome profiles for MiniPLC (no Arduino IDE required) |

**Module hardware:** 4× AI (ADS1115), 2× AO (MCP4725), 2× RTD (MAX31865), 4 buttons, 4 LEDs, Modbus RTU over RS-485, USB Type-C.

---

## 2. Requirements

### 2.1. Hardware

- **AIO-422-R1** module with **RP2350A** MCU
- **USB Type-C** cable (data, not charge-only)
- PC: Windows 10/11, Linux (Fedora/Ubuntu, etc.), or macOS
- For field Modbus operation: **24 V DC** power, RS-485 line (not required for USB build/upload)

### 2.2. Software

- **Arduino IDE 2.3+** (recommended): https://www.arduino.cc/en/software  
- Internet access (board core and library installation)
- **Git** (to clone the repository): https://git-scm.com/

---

## 3. Getting the Source Code

```bash
git clone https://github.com/isystemsautomation/homemaster-dev.git
cd homemaster-dev/AIO-422-R1/Firmware
```

Alternatively, download a ZIP archive from GitHub and extract it.

Arduino IDE sketch path:

```text
homemaster-dev/AIO-422-R1/Firmware/v0.1.0/default_aio_422_r1/default_aio_422_r1.ino
```

Open the **`.ino`** file — Arduino IDE will load the entire sketch folder.

---

## 4. Installing Arduino IDE

### Windows

1. Download the installer from https://www.arduino.cc/en/software  
2. Install with administrator rights if required  
3. On first USB connection, Windows may install the USB (CDC) driver automatically  

### Linux (Fedora and similar)

**AppImage from the Arduino site is recommended** (full USB access):

1. Download the AppImage from https://www.arduino.cc/en/software  
2. Make it executable: `chmod +x arduino-ide_*.AppImage`  
3. Add your user to the serial group:

   ```bash
   sudo usermod -aG dialout $USER
   ```

4. Log out and log back in  

**Flatpak** (`cc.arduino.IDE2`): often **does not see the COM port**. If using Flatpak:

```bash
flatpak override --user cc.arduino.IDE2 --device=all
```

### macOS

Install Arduino IDE from the official site; allow USB access when prompted.

---

## 5. RP2350 Board Support (Board Package)

The firmware is built with the **Earle Philhower** core (arduino-pico), not bare Mbed without RP2350 support.

### 5.1. Board Manager URL

1. **File → Preferences**  
2. In **Additional boards manager URLs**, add (if empty, paste as a single line):

   ```text
   https://arduino.earlephilhower.com/version/stable/package_earlephilhower_index.json
   ```

3. **OK**

### 5.2. Installing the Core

1. **Tools → Board → Boards Manager**  
2. Search for: `pico` or `rp2350`  
3. Install:

   **Raspberry Pi Pico/RP2040/RP2350**  
   author: **Earle Philhower**

Wait for the download to finish (several hundred MB).

### 5.3. Board and Tool Settings

**Tools → Board** → **Raspberry Pi Pico/RP2040/RP2350** group:

- **Raspberry Pi Pico 2**, or  
- **Generic RP2350** / any variant with **rp2350** in the name  

(exact name depends on core version; must be **RP2350**, not RP2040/Pico 1 only).

| Setting (Tools) | Recommendation |
|-----------------|----------------|
| **Port** | After USB connect: `COMx` (Windows), `/dev/ttyACM0` (Linux) |
| Other options | Defaults for the selected board |

**LittleFS** and **watchdog** are included in the Philhower core — no separate install needed.

---

## 6. Arduino Libraries

**Sketch → Include Library → Manage Libraries…**

Install in order:

| # | Search in Library Manager | Header in code | Purpose |
|---|---------------------------|----------------|---------|
| 1 | **ADS1X15** (Rob Tillaart) | `ADS1X15.h` | Analog inputs, ADS1115 |
| 2 | **Adafruit MCP4725** | `Adafruit_MCP4725.h` | Analog outputs, DAC |
| 3 | **Adafruit MAX31865** | `Adafruit_MAX31865.h` | RTD, MAX31865 |
| 4 | **Adafruit BusIO** | — | Adafruit dependency (often installed automatically) |
| 5 | **Modbus Serial** (epsilonrt) | `ModbusSerial.h` | Modbus RTU slave |
| 6 | **SimpleWebSerial** | `SimpleWebSerial.h` | USB WebConfig |
| 7 | **Arduino_JSON** | `Arduino_JSON.h` | JSON for WebSerial |

### 6.1. Modbus — Important

Use the **epsilonrt** library with **`ModbusSerial.h`**.

**Do not install** the **`Modbus`** library by **UL DARA** — different API; on RP2040/RP2350 you often get **build errors** (`Modbus.h not found`, `byte` type conflict).

If Modbus Serial is not in Library Manager, install manually:

```bash
mkdir -p ~/Arduino/libraries
cd ~/Arduino/libraries
git clone https://github.com/epsilonrt/modbus-arduino.git Modbus-Arduino
```

Restart Arduino IDE.

### 6.2. Include Order

`default_aio_422_r1.ino` already uses the correct order:

```cpp
#include <Arduino.h>
#include <ModbusSerial.h>   // must be BEFORE Adafruit
#include <Wire.h>
// ...
```

Do not add `#include <utility>` — on RP2350 it can conflict with Modbus `byte`.

---

## 7. Opening the Project and Compiling

1. **File → Open** → select  
   `default_aio_422_r1/default_aio_422_r1.ino`
2. Check **Tools → Board** (RP2350) and **Tools → Port** (if the module is connected)
3. Click **Verify** (checkmark) or **Sketch → Verify/Compile**

Expected result: **compile with no errors**, firmware size roughly **150–165 KB** (depends on core version).

### Common Build Errors

| Message | Fix |
|---------|-----|
| `Modbus.h: No such file` | Install **epsilonrt** Modbus; remove Modbus UL DARA from `~/Arduino/libraries` |
| `byte` ambiguous | Put `ModbusSerial.h` before Adafruit; remove `<utility>` |
| `PersistConfig was not declared` | Do not move `struct PersistConfig` to end of file — keep it near the top of `.ino` |
| Board not RP2350 | Install Philhower core (section 5) |

---

## 8. Uploading Firmware

### 8.1. Via USB (normal method)

1. Connect the module via USB-C to the PC  
2. **Tools → Port** → select the port (`COM…` / `ttyACM0`)  
3. Click **Upload** (right arrow)  
4. The module may briefly enter bootloader mode — do not disconnect the cable  

After upload, Serial Monitor (115200 baud) or WebConfig should show a successful boot message (Boot OK).

### 8.2. Via UF2 (if no serial port)

1. Disconnect USB  
2. Hold **BOOTSEL** on the MCU board, connect USB, release BOOTSEL  
3. A **RPI-RP2** drive appears in the system  
4. Copy the **`.uf2`** file from the Arduino build folder (after Compile) to the RPI-RP2 drive  
5. The module reboots with the new firmware  

Example `.uf2` path after build (Linux):

```text
/tmp/arduino_build_*/default_aio_422_r1.ino.uf2
```

Or check the compile output / `build` folder next to the sketch if verbose build is enabled.

### 8.3. Resetting Module Configuration

Settings are stored in flash (**LittleFS**, file `/cfg.bin`). When the **config format version** changes, settings reset to factory defaults. After the first upload of a new version you may need to set Modbus address and button/LED mappings again via WebConfig.

---

## 9. WebConfig (setup without rebuild)

1. Open `ConfigToolPage.html` in Chrome or Edge (double-click or drag into the browser)  
2. Connect the module via USB  
3. Click **Connect** → select the module COM port  
4. Available: Modbus address/baud, AI/AO/RTD (diagnostics), buttons, LEDs, RTD config  

Modbus over RS-485 runs in parallel (default address **3**, baud **19200**).

---

## 10. ESPHome / MiniPLC (separate from Arduino)

Files in `default_aio_422_r1_plc/` are for **ESPHome** integration, not Arduino IDE:

| File | Description |
|------|-------------|
| `default_aio_422_r1_plc.yaml` | Basic set: AI, AO, RTD |
| `default_aio_422_r1_plc_full.yaml` | + buttons and LEDs (Modbus discrete) |

Validate YAML (on a PC with ESPHome installed):

```bash
esphome config default_aio_422_r1_plc.yaml
```

---

## 11. New Computer Checklist

```
[ ] Git: homemaster-dev repository cloned
[ ] Arduino IDE 2.x installed
[ ] Linux: user in dialout group; if Flatpak — device=all
[ ] Preferences: earlephilhower package_earlephilhower_index.json URL
[ ] Boards Manager: Raspberry Pi Pico/RP2040/RP2350 (Earle Philhower)
[ ] Board: RP2350 / Pico 2
[ ] Libraries: ADS1X15, Adafruit MCP4725, Adafruit MAX31865, Adafruit BusIO,
                 Modbus Serial (epsilonrt), SimpleWebSerial, Arduino_JSON
[ ] NO Modbus library (UL DARA)
[ ] default_aio_422_r1.ino opened
[ ] Verify — no errors
[ ] Upload or UF2 — success
[ ] ConfigToolPage.html — Connect over USB works
```

---

## 12. Modbus Map (reference)

| Registers | Addresses | Description |
|-----------|-----------|-------------|
| Buttons | ISTS 1–4 | Discrete inputs |
| LED | ISTS 20–23 | Discrete inputs |
| RTD | HREG 120–121 | °C×10, S_WORD |
| AI mV | HREG 140–143 | U_WORD |
| AO raw | HREG 200–201 | U_WORD, 0–4095 |

See comments at the top of `default_aio_422_r1.ino` and ESPHome YAML for details.

---

## 13. Support

- Repository: https://github.com/isystemsautomation/homemaster-dev  
- Manufacturer: ISYSTEMS AUTOMATION S.R.L. (HomeMaster®)  
- Website: https://www.home-master.eu  

For build errors, attach the **full Arduino IDE Output text** plus versions: IDE, Philhower core, OS.
