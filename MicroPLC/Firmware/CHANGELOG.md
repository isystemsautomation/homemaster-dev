# Changelog — HomeMaster MicroPLC

Firmware release history. OTA binaries and `manifest.json` live in this folder alongside `microplc.yaml`.

### v1.3.1 — current

- Added the onboard **24 V digital input** as `DI #1` (**GPIO36**); the input existed in hardware from v1.0.0 but was never exposed as an entity.
- RS-485 UART aligned with MiniPLC: id renamed `mod_uart` → **`uart_modbus`**, matching the wiring example in every module README, and pins written as `GPIO17`/`GPIO16`.
- **UART baud rate corrected from 115200 to 19200** — the HomeMaster module bus runs at 19200, so the previous value prevented the factory firmware from communicating with any expansion module.
- Added diagnostic entities matching OpenthermGateway: **ESP Status**, **WiFi Signal**, **ESP32 Temperature**, **ESP Uptime Human**, **ESPHome Version** and **ESP IP Address**, all under the diagnostic entity category.
- Added a **Restart** button (reboot only; no settings are cleared).

### v1.3.0

- Project version bumped from 1.2.0 to **1.3.0**.
- Declared `flash_size: 16MB` (and `variant: esp32`) so ESPHome builds a partition table that matches the hardware; units already in the field need a full USB flash with erase before OTA can use the new layout.
- Added a `dallas_temp` sensor on the 1-Wire bus (GPIO4), exposing **1-Wire Bus 1 Temperature**; the bus was previously declared but never read. Bus id renamed `hub_1` → `ow_bus_1` to match OpenthermGateway and MiniPLC.

### v1.2.0

- Project version bumped from 1.1.1 to **1.2.0**.
- Removed the Wi-Fi fallback access point (`wifi.ap: {}`) and `captive_portal:` for RED Art. 3(3)(d) / EN 18031-1.

### v1.1.1


- Dropped the `substitutions` block; values inlined as literals — configuration aligned with OpenthermGateway and MiniPLC.
- Removed `mdns`, `network`, and the default-only keys `wifi.fast_connect`, `wifi.domain`, `esphome.area`, `logger.level`.
- `friendly_name` and `project.name` normalised to the HomeMaster trademark casing.
- Network updates: `http_request`, OTA via `http_request`, `update` polling `manifest.json` every **6 h**, plus a check **10 s** after Wi-Fi connect.

### v1.1.0

- EN 18031-1 / RED Art. 3(3)(d): require ESPHome ≥ **2026.7.0**; enable `api.encryption` **without** a baked-in key (per-device key set at adoption) and add `provisioning:` (**15 min** window after power-on; power-cycle to reopen).
- Vendor-managed network updates via `manifest.json` (`update.http_request`, poll every **6 h**).
- Project version string normalised to **1.1.0** (no `v` prefix), matching MiniPLC / OpenTherm Gateway for OTA downgrade protection.

### v1.0.0

- Initial public firmware release for MicroPLC-R1 hardware V1.0.
- ESPHome pre-installed with Improv Wi-Fi provisioning (BLE + USB Serial).
- RS-485 Modbus RTU interface for HomeMaster expansion modules.
- 1× isolated 24 V digital input, 1× relay output, 1-Wire bus, PCF8563 RTC.
