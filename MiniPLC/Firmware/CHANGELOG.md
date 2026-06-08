# Changelog — HomeMaster MiniPLC

Firmware release history. OTA binaries and `manifest.json` live in this folder.

### v1.0.1 — current

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
