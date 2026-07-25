# Changelog — HomeMaster MiniPLC

Firmware release history. OTA binaries and `manifest.json` live in this folder.

### v1.2.0 — current

- Project version bumped to **1.2.0**.
- Removed the Wi-Fi fallback access point (`wifi.ap`) and `captive_portal:` from `miniplc.yaml` for RED Art. 3(3)(d) / EN 18031-1 (no unauthenticated open AP after first boot). Ethernet (`config-eth.yaml`) is unchanged aside from the version bump.

### v1.1.0


- EN 18031-1 / RED Art. 3(3)(d): require ESPHome ≥ **2026.7.0**; enable `api.encryption` **without** a baked-in key (per-device key set at adoption) and add `provisioning:` (**15 min** window after power-on; power-cycle to reopen).
- Removed `web_server:` from `miniplc.yaml` and `config-eth.yaml`. ESPHome’s web server has no CSRF protection and a permissive CORS policy by design, and unlike the API key / OTA password it is **not** configured automatically on adoption — so it would have stayed unauthenticated forever. Users can re-add it with `auth:` after adoption (`dashboard_import` uses `import_full_config: true`).
- Project version bumped from 1.0.1 to **1.1.0** (OTA downgrade protection uses this value).
- `friendly_name` normalised to the HomeMaster trademark; devices that install this update over OTA will be renamed in Home Assistant.

### v1.0.1

- Aligned README structure with the OpenTherm Gateway README.
- Removed captive-portal fallback AP from documentation — provisioning is now Improv-only (BLE + USB Serial).
- Added Real-Time Clock battery notes (coin cell not installed by default; install if you need offline timekeeping).
- Added detailed RTD DIP-switch configuration tables for PT100 / PT1000 and 2/3/4-wire modes.
- Added YAML configuration sections for optional RTD (MAX31865) and 1-Wire sensors, including the GPIO1/GPIO3 vs USB serial trade-off.
- Added YAML configuration section for optional wired Ethernet (LAN8720).
- Documented compatible HomeMaster expansion modules (DIO-430-R1, DIM-420-R1, ENM-223-R1, ALM-173-R1, AIO-422-R1).
- All ESPHome entities given explicit `id:` values (Made for ESPHome compliant).
- Updated I²C address map: PCF8574 expanders at 0x20 / 0x21 (was 0x38 / 0x39 in earlier prototypes).
- Added `web_server: port: 80` for built-in local web UI.
- `mcp4725` (DAC) explicit `address: 0x60`.
- HTTP OTA `update.firmware_update` entity sourcing manifest from GitHub Pages with verified MD5.

### v1.0.0

- Initial public firmware release for MiniPLC-R1 hardware V1.0.
