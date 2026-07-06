# ENM-223-R1 — Firmware

Arduino sketch, WebConfig page, and ESPHome package for the **ENM-223-R1** 3-phase meter + I/O module (RP2350 + ATM90E32AS).

| Path | Purpose |
|------|---------|
| `v0.2.0/default_enm_223_r1/` | Main firmware sketch (`default_enm_223_r1.ino`, `atm90e32.*`, `hm_common.h`) |
| `v0.2.0/ConfigToolPage.html` | USB WebConfig (Web Serial) |
| `v0.2.0/default_enm_223_r1_plc/` | ESPHome Modbus package for MicroPLC / MiniPLC |
| `v0.1.0/` | Legacy firmware line (do not change unless maintaining v0.1.0) |

**Current line:** `v0.2.0` (firmware string `0.2.0`, Modbus map v1)

---

## v0.2.0 feature summary

| Area | Detail |
|------|--------|
| **Signed P/Q** | Active and reactive power from ATM90E32AS published as **S32** Modbus values (import/export sign) |
| **Peaks & neutral** | Hardware peak U/I (IR 12–17), neutral Irms (IR 18) |
| **THD** | Active-power THD estimate per phase (IR 47–49, ×0.01 %) |
| **Import / export energy** | AP/RP = import, AN/RN = export; WebConfig shows import/export/net labels |
| **Alarm engine** | Per-channel Alarm / Warning / Event rules with **hysteresis**; chip PQ events (sag, OV, phase loss, over-I, freq, phase sequence) as **Event** |
| **Modbus alarms** | DI 16–27 (active flags); ACK coils 16–19 |
| **Relay Alarm Controlled** | Relay follows selected alarm channel/kinds — local load shed until condition clears or **Ack** |
| **Phase mapping** | WebConfig: L1/L2/L3 → meter phase A/B/C; written to ATM90E32 `ChannelMap` on apply |
| **Wiring mode** | WebConfig: **3P4W** (star) or **3P3W**; sets ATM90E32 `MMode0` on apply |
| **Persistence** | **Settings** `/enm_cfg.bin` (CFG v0x0022); **meter** `/enm_meter.bin` (calibration + energy). Meter data survives typical firmware updates; settings are migrated when possible |

Legacy Modbus addresses (Urms 0–11, P/Q/S 20–42, energies from 60, coils/DI 0–9 and 16–19) are **unchanged**.

---

## Persistence (LittleFS)

| File | Contents | On firmware update |
|------|----------|-------------------|
| `/enm_cfg.bin` | Modbus address/baud, line Hz, sum mode, wire mode, phase map, relay/LED/button/alarm configuration | **Migrated** when `CFG_VERSION` has a migration path (e.g. 0x0021 → 0x0022). If the blob cannot be loaded, **settings revert to firmware defaults** on next boot. |
| `/enm_meter.bin` | `ucal`, per-phase Ugain/Igain/offsets, energy counter ticks | **Preserved** while `METER_VERSION` matches (independent of settings version). |

Flashing a new `.uf2` / sketch does **not** format LittleFS by itself. Recommission **alarms, relays, phase map, and bus address** after major firmware jumps if WebConfig shows defaults.

---

## Publishing to GitHub (after compile)

Commit **source files** and the **UF2 where Arduino writes it** (`build/<board>/`).  
Do **not** copy the UF2 next to the sketch — Arduino does not update that path.

### Commit these

| Item | Path (v0.2.0 example) |
|------|------------------------|
| Sketch source | `default_enm_223_r1/default_enm_223_r1.ino`, `atm90e32.cpp`, `atm90e32.h` |
| Shared headers | `default_enm_223_r1/hm_common.h` |
| **Release UF2** | `default_enm_223_r1/build/rp2040.rp2040.generic_rp2350/default_enm_223_r1.ino.uf2` |
| WebConfig | `ConfigToolPage.html`, `simple-web-serial.min.js` |
| ESPHome package | `default_enm_223_r1_plc/default_enm_223_r1_plc.yaml` |

Public download URL (on `main`, current board folder):

`https://github.com/isystemsautomation/homemaster-dev/raw/refs/heads/main/ENM-223-R1/Firmware/v0.2.0/default_enm_223_r1/build/rp2040.rp2040.generic_rp2350/default_enm_223_r1.ino.uf2`

### Do **not** commit

| Item | Reason |
|------|--------|
| `*.bin`, `*.elf`, `*.map` | Build intermediates (ignored) |
| `default_enm_223_r1.ino.uf2` next to the sketch | Stale copy — use `build/…` only |

---

## Build (Arduino IDE)

- **Board:** Generic RP2350 (rp2040 core 5.x) → build folder `build/rp2040.rp2040.generic_rp2350/`
- **Sketch:** open `v0.2.0/default_enm_223_r1/default_enm_223_r1.ino`
- **Libraries:** ModbusSerial, Arduino_JSON, LittleFS, SimpleWebSerial (see [module README §8.3](../README.md#83-arduino-ide-setup))
- **Reproducible build:** [`sketch.yaml`](v0.2.0/default_enm_223_r1/sketch.yaml) — see [root build environment](../../README.md#build-environment-reproducible)

UF2 flashing: hold **Buttons 1 + 2**, release **1**, then release **2** together → **RPI-RP2** USB drive. See [module README §8.2](../README.md#82-flashing-via-usb-c).

---

## WebConfig entry point

Open in a Chromium-based browser (file or hosted):

`Firmware/v0.2.0/ConfigToolPage.html`

Connect over USB-C Web Serial. Meter options (line Hz, sum mode, **3P4W/3P3W**, **phase mapping**), calibration, alarms, and relays are applied via `ext.atm` / `alarm` / `relay` messages and auto-saved to `/enm_cfg.bin`.

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
          enm_address: 30
          enm_prefix: "ENM #1"
```

Set `enm_address` to the value shown in WebConfig after commissioning.
