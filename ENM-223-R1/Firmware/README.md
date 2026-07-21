# ENM-223-R1 — Firmware

Arduino sketch, WebConfig page, and ESPHome package for the **ENM-223-R1** 3-phase meter + I/O module (RP2350 + ATM90E32AS).

| Path | Purpose |
|------|---------|
| `v0.2.0/default_enm_223_r1/` | Main firmware sketch (`default_enm_223_r1.ino`, `atm90e32.*`, `hm_common.h`) |
| `v0.2.0/ConfigToolPage.html` | USB WebConfig (Web Serial) |
| `v0.2.0/default_enm_223_r1_plc/` | ESPHome Modbus package for MicroPLC / MiniPLC |
| `v0.2.0/ENM-223-R1.uf2` | Released binary (flash this) |
| `v0.1.0/` | Legacy firmware line (do not change unless maintaining v0.1.0) |

**Current line:** `v0.2.0` (firmware string `0.2.0`, `HM_MAP_VERSION` **`0x0020`**, `CFG_VERSION` **`0x0025`**, `METER_VERSION` **`0x0003`**, Model ID **`2`**)

See also: [Modbus_Table.md](../Modbus_Table.md) for the full FC04 register map (`0..85`).

---

## v0.2.0 feature summary

| Area | Detail |
|------|--------|
| **Contiguous Modbus map** | FC04 **`0..85`** (86 registers) — one sweep; **no FC02** discrete inputs |
| **Primary-side metering** | CT ratio + PGA in WebConfig; U/I/P/Q/S and energy on the **primary** side |
| **Signed P/Q** | Active and reactive power as **S32** (import/export sign) |
| **Energy** | Primary Wh/varh import/export (U32); decoupled from CT/calibration changes; reset via WebConfig / button only |
| **Alarm engine** | L1–L3/Total × Alarm/Warning/Event; optional ack latch; chip PQ in IR 3–6; ACK coils 16–19 |
| **Relay modes** | None / Modbus / Alarm; coils 0/1 write-only; state from IR0 bits 8/9 |
| **LEDs / buttons** | LED alarm-kind sources; button Ack actions |
| **Phase mapping / wiring** | WebConfig: L1–L3 → A/B/C; **3P4W / 3P3W** |
| **Persistence** | Settings `/enm_cfg.bin` (`CFG_VERSION` **0x0025**); meter `/enm_meter.bin` (`METER_VERSION` **0x0003**) |
| **WebConfig-only live** | Peaks, Pfund/Pharm, VAh, harmonic energy (kept off the bus map) |

---

## Persistence (LittleFS)

| File | Contents | On firmware update |
|------|----------|-------------------|
| `/enm_cfg.bin` | Modbus address/baud, line Hz, sum mode, wire mode, phase map, CT/PGA, relay/LED/button/alarm configuration | **Migrated** when `CFG_VERSION` has a migration path. If the blob cannot be loaded, **settings revert to firmware defaults** on next boot. |
| `/enm_meter.bin` | `ucal`, per-phase Ugain/Igain/offsets, energy counter ticks | **Preserved** while `METER_VERSION` matches. |

Flashing a new `.uf2` / sketch does **not** format LittleFS by itself. Recommission **alarms, relays, phase map, and bus address** after major firmware jumps if WebConfig shows defaults.

---

## Publishing to GitHub (after compile)

Commit **source files** and the **released UF2** at `Firmware/v0.2.0/ENM-223-R1.uf2` (Rule 7 — not a stale copy under `build/`).

### Commit these

| Item | Path (v0.2.0 example) |
|------|------------------------|
| Sketch source | `default_enm_223_r1/default_enm_223_r1.ino`, `atm90e32.cpp`, `atm90e32.h` |
| Shared headers | `default_enm_223_r1/hm_common.h` |
| **Release UF2** | `ENM-223-R1.uf2` (canonical path next to the version folder) |
| WebConfig | `ConfigToolPage.html`, `simple-web-serial.min.js` |
| ESPHome package | `default_enm_223_r1_plc/default_enm_223_r1_plc.yaml` |

Public download URL (on `main`):

`https://github.com/isystemsautomation/homemaster-dev/raw/refs/heads/main/ENM-223-R1/Firmware/v0.2.0/ENM-223-R1.uf2`

---

## Build (Arduino IDE)

- **Board:** Generic RP2350 (rp2040 core 5.x)
- **Sketch:** open `v0.2.0/default_enm_223_r1/default_enm_223_r1.ino`
- **Libraries:** ModbusSerial, Arduino_JSON, LittleFS, SimpleWebSerial (see [module README §8.3](../README.md#83-arduino-ide-setup))
- **Reproducible build:** [`sketch.yaml`](v0.2.0/default_enm_223_r1/sketch.yaml) — see [root build environment](../../README.md#build-environment-reproducible)

UF2 flashing: hold **Buttons 1 + 2**, release **1**, then release **2** together → **RPI-RP2** USB drive. See [module README §8.2](../README.md#82-flashing-via-usb-c).

---

## WebConfig entry point

Open in a Chromium-based browser (file or hosted):

`Firmware/v0.2.0/ConfigToolPage.html`

Live: [config.home-master.eu …/v0.2.0/ConfigToolPage.html](https://config.home-master.eu/ENM-223-R1/Firmware/v0.2.0/ConfigToolPage.html)

Connect over USB-C Web Serial. Meter options (line Hz, sum mode, **3P4W/3P3W**, **phase mapping**, **CT ratio**, **PGA**), calibration, alarms, and relays are applied via `ext.atm` / `alarm` / `relay` messages and auto-saved to `/enm_cfg.bin`.

---

## ESPHome package

```yaml
packages:
  enm223_1:
    url: https://github.com/isystemsautomation/homemaster-dev
    ref: main
    files:
      - path: ENM-223-R1/Firmware/v0.2.0/default_enm_223_r1_plc/default_enm_223_r1_plc.yaml
        vars:
          enm_id: enm223_1
          enm_address: 3
          enm_prefix: "ENM #1"
```

Set `enm_address` to the value shown in WebConfig after commissioning (first-boot default **3** @ 19200 8N1).
