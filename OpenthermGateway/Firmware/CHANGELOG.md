# Changelog — HomeMaster OpenTherm Gateway

Firmware release history. OTA binaries and `manifest.json` live in this folder.

### v1.1.0 — current

- EN 18031-1 / RED Art. 3(3)(d): require ESPHome ≥ **2026.7.0**; enable `api.encryption` **without** a baked-in key (per-device key set at adoption) and add `provisioning:` (**15 min** window after power-on; power-cycle to reopen).
- Project version bumped from 1.0.7 to 1.1.0 (OTA downgrade protection uses this value).
- **LED:** until Home Assistant connects with the encryption key there is no API client, so status LED **U.2 slow-blinks** from boot until adoption (expected; not a fault). On 1.0.7 the unencrypted API usually cleared the LED sooner.

### v1.0.7 — documentation update

- **Documentation:** trademark attribution added; version sync.

## v1.0.7 — 2026-06-08

- Added `id:` to all named entities to satisfy the "Made for ESPHome"
  requirement that every entity/component must have an id:
  - text_sensor, platform: version  ("ESPHome Version")      → id: esphome_version
  - text_sensor, platform: wifi_info → ip_address ("ESP IP Address") → id: esp_ip_address
- Updated device documentation wording in index.md per maintainer review.
- Firmware recompiled with ESPHome 2026.5.3; new OTA binary published.

### v1.0.6 — documentation update

- **Safety:** Added relay output use-restriction warning to comply with the Basic-insulation rating between mains primary (L/N) and relay output (C, NC) tracks on the Relay board, REV1.0. The restriction will be lifted in REV2 of the hardware where Reinforced insulation is achieved by PCB redesign.

### v1.0.6

- Initial public firmware release for OTGW-R1 hardware V1.0.
