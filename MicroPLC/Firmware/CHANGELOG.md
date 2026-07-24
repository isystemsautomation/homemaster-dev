### v1.1.0 — current

- EN 18031-1 / RED Art. 3(3)(d): require ESPHome ≥ 2026.7.0; enable `api.encryption` without a baked-in key and add `provisioning:` (15 min window).
- Project version bumped to v1.1.0 (OTA downgrade protection uses this value).

# Changelog — HomeMaster MicroPLC

Firmware release history. ESPHome configuration (`microplc.yaml`) lives in this folder.

### v1.0.0

- Initial public firmware release for MicroPLC-R1 hardware V1.0.
- ESPHome pre-installed with Improv Wi-Fi provisioning (BLE + USB Serial).
- RS-485 Modbus RTU interface for HomeMaster expansion modules.
- 1× isolated 24 V digital input, 1× relay output, 1-Wire bus, PCF8563 RTC.
