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

Commit **source files** and the **UF2 where Arduino writes it** (`build/<board>/`).  
Do **not** copy the UF2 next to the sketch — Arduino does not update that path.

### Commit these

| Item | Path (v0.2.0 example) |
|------|------------------------|
| Sketch source | `default_DIO_430_R1/default_DIO_430_R1.ino` |
| Shared headers | `default_DIO_430_R1/hm_common.h` |
| **Release UF2** | `default_DIO_430_R1/build/rp2040.rp2040.generic_rp2350/default_DIO_430_R1.ino.uf2` |
| WebConfig | `ConfigToolPage.html`, `simple-web-serial.min.js` |
| ESPHome package | `default_dio_430_r1_plc/*.yaml` |

The UF2 path follows the **board/FQBN** folder Arduino creates under `build/`. If you change the board target, update links in README/product page to match.

Public download URL (on `main`, current board folder):

`https://github.com/isystemsautomation/homemaster-dev/raw/refs/heads/main/DIO-430-R1/Firmware/v0.2.0/default_DIO_430_R1/build/rp2040.rp2040.generic_rp2350/default_DIO_430_R1.ino.uf2`

### Do **not** commit

| Item | Reason |
|------|--------|
| `*.bin`, `*.elf`, `*.map` | Build intermediates (ignored) |
| `default_DIO_430_R1.ino.uf2` next to the sketch | Stale copy — use `build/…` only |

### Workflow after Arduino **Compile** or **Export compiled Binary**

1. Arduino updates the UF2 in place, e.g.  
   `default_DIO_430_R1/build/rp2040.rp2040.generic_rp2350/default_DIO_430_R1.ino.uf2`
2. Stage sources + that UF2:

   ```bash
   git add DIO-430-R1/Firmware/v0.2.0/default_DIO_430_R1/default_DIO_430_R1.ino
   git add DIO-430-R1/Firmware/v0.2.0/default_DIO_430_R1/hm_common.h
   git add DIO-430-R1/Firmware/v0.2.0/default_DIO_430_R1/build/rp2040.rp2040.generic_rp2350/default_DIO_430_R1.ino.uf2
   ```

3. Commit and push. Do **not** `git add` `.bin`, `.elf`, or `.map`.

---

## Build (Arduino IDE)

- **Board:** Generic RP2350 (rp2040 core 5.x) → build folder `build/rp2040.rp2040.generic_rp2350/`
- **Sketch:** open `v0.2.0/default_DIO_430_R1/default_DIO_430_R1.ino`
- **Libraries:** ModbusSerial, Arduino_JSON, LittleFS, SimpleWebSerial (see module README §8.3)

UF2 flashing (module front panel): hold **Buttons 1 + 2 + 3**, release **1**, then release **2 + 3** together → **RPI-RP2** USB drive. See [DIO-430-R1 README §8.2](../README.md#82-flashing-usbc-hardware-buttons-only).

---

## Gesture event counters (Momentary)

Digital inputs and user buttons (Button 1–2) in **Momentary** mode recognize press gestures and expose **four counters** each: **single**, **double**, **triple**, and **long**.

- Counters are published as **Modbus Input Registers** (FC04, IR 6…29) and appear in Home Assistant through `default_dio_430_r1_plc.yaml`.
- Home Assistant automations should trigger on a **counter increase** (e.g. double-click → `double` +1), so gestures are not lost when the bus is polled infrequently.
- Use this for HA scenes on single/double/triple clicks and long holds without dropping short pulses on RS-485.
- Counters run **only** for **Type = Momentary**; **Maintained** inputs have no gesture counters.
- **HA-only input** (no local relay control): **Type = Momentary**, **Short = None**, **Long = None** — gestures are still counted.
- Tune **Long-press ms** and **Multi-click gap ms** in WebConfig **Timing**.

Short/Long actions in WebConfig: **None / Toggle / On / Off**; target **R1–R3 / All / None**. To switch all relays off, set action **Off** and target **All** (legacy stored action value 4 is still accepted by firmware).
