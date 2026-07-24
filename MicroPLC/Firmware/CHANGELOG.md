# Changelog — HomeMaster MicroPLC

Firmware release history. OTA binaries and `manifest.json` live in this folder alongside `microplc.yaml`.

### v1.1.0 — current

- EN 18031-1 / RED Art. 3(3)(d): require ESPHome ≥ **2026.7.0**; enable `api.encryption` **without** a baked-in key (per-device key set at adoption) and add `provisioning:` (**15 min** window after power-on; power-cycle to reopen).
- Vendor-managed network updates via `manifest.json` (`update.http_request`, poll every **6 h**).
- Project version string normalised to **1.1.0** (no `v` prefix), matching MiniPLC / OpenTherm Gateway for OTA downgrade protection.

### v1.0.0

- Initial public firmware release for MicroPLC-R1 hardware V1.0.
- ESPHome pre-installed with Improv Wi-Fi provisioning (BLE + USB Serial).
- RS-485 Modbus RTU interface for HomeMaster expansion modules.
- 1× isolated 24 V digital input, 1× relay output, 1-Wire bus, PCF8563 RTC.
