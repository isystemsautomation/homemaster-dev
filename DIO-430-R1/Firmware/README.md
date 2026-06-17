# DIO-430-R1 — Firmware

Arduino sketch, WebConfig page, and ESPHome package for the **DIO-430-R1** module (RP2350).

| Path | Purpose |
|------|---------|
| `v0.2.0/default_DIO_430_R1/` | Main firmware sketch (`default_DIO_430_R1.ino`, `hm_common.h`) |
| `v0.2.0/ConfigToolPage.html` | USB WebConfig (Web Serial) |
| `v0.2.0/default_dio_430_r1_plc/` | ESPHome Modbus package for MicroPLC / MiniPLC |
| `v0.1.0/` | Legacy firmware line (do not change unless maintaining v0.1.0) |

**Current line:** `v0.2.0`

---

## Publishing to GitHub (after compile)

Only **source files** and the **pre-built UF2** belong in this repository.  
Do **not** commit intermediate Arduino build artifacts.

### Commit these

| Item | Path (v0.2.0 example) |
|------|------------------------|
| Sketch source | `default_DIO_430_R1/default_DIO_430_R1.ino` |
| Shared headers | `default_DIO_430_R1/hm_common.h` (and any other `.h` in the sketch folder) |
| **Release UF2** | `default_DIO_430_R1/default_DIO_430_R1.ino.uf2` |
| WebConfig | `ConfigToolPage.html`, `simple-web-serial.min.js` |
| ESPHome package | `default_dio_430_r1_plc/*.yaml` |

The UF2 is the **only** compiled binary users need for drag-and-drop upgrades (no toolchain).  
Public download URL (on `main`):

`https://github.com/isystemsautomation/homemaster-dev/raw/refs/heads/main/DIO-430-R1/Firmware/v0.2.0/default_DIO_430_R1/default_DIO_430_R1.ino.uf2`

### Do **not** commit

| Item | Typical path | Reason |
|------|----------------|--------|
| `build/` folder | `default_DIO_430_R1/build/` | Local Arduino output |
| `*.bin` | `default_DIO_430_R1.ino.bin` | Not used for end-user flashing |
| `*.elf` | `default_DIO_430_R1.ino.elf` | Debug link map only |
| `*.map` | `default_DIO_430_R1.ino.map` | Linker listing only |

These are ignored via `.gitignore` in the sketch folder.

### Workflow after Arduino **Compile** or **Verify**

1. Locate the UF2 from the build log or folder, e.g.  
   `default_DIO_430_R1/build/rp2040.rp2040.generic_rp2350/default_DIO_430_R1.ino.uf2`
2. Copy it next to the sketch (overwrite the published copy):

   ```bash
   cp default_DIO_430_R1/build/rp2040.rp2040.generic_rp2350/default_DIO_430_R1.ino.uf2 \
      default_DIO_430_R1/default_DIO_430_R1.ino.uf2
   ```

3. Stage only sources + UF2:

   ```bash
   git add DIO-430-R1/Firmware/v0.2.0/default_DIO_430_R1/default_DIO_430_R1.ino
   git add DIO-430-R1/Firmware/v0.2.0/default_DIO_430_R1/hm_common.h
   git add DIO-430-R1/Firmware/v0.2.0/default_DIO_430_R1/default_DIO_430_R1.ino.uf2
   # plus ConfigToolPage.html / yaml if you changed them
   ```

4. Commit and push. Do **not** `git add` `build/`, `.bin`, `.elf`, or `.map`.

---

## Build (Arduino IDE)

- **Board:** Generic RP2350 (rp2040 core 5.x)
- **Sketch:** open `v0.2.0/default_DIO_430_R1/default_DIO_430_R1.ino`
- **Libraries:** ModbusSerial, Arduino_JSON, LittleFS, SimpleWebSerial (see module README §8.3)

UF2 flashing (module front panel): hold **Buttons 1 + 2 + 3**, release **1**, then release **2 + 3** together → **RPI-RP2** USB drive. See [DIO-430-R1 README §8.2](../README.md#82-flashing-usbc-hardware-buttons-only).
