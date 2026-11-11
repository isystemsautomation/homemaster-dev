# STR-3221-R1 Firmware v0.1.0

First public release: 32-channel stair LED controller firmware set.

| Path | Purpose |
|------|---------|
| `default_str_3221_r1/` | Main Arduino sketch (Modbus, TLC59208F, WebConfig) |
| `ConfigToolPage.html` | USB Web Serial configuration UI |
| `default_str_3221_r1_plc/` | ESPHome Modbus package (32 outputs + 3 inputs) |

Default Modbus: address **3**, baud **19200**.
