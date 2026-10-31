# STR-3221-R1 Firmware v0.3.0

Current released firmware for the 32-channel stair LED / heating-zone module.
v0.2.0 is Legacy. The Modbus register map is unchanged from v0.2.0.

| Path | Purpose |
|------|---------|
| `default_str_3221_r1/` | Main Arduino sketch (Modbus, TLC59208F, WebConfig) |
| `ConfigToolPage.html` | USB Web Serial configuration UI |
| `default_str_3221_r1_plc/` | ESPHome Modbus package (32 outputs + 3 inputs) |

Default Modbus: address **3**, baud **19200**.
`CFG_VERSION` is **0x0005** — settings survive the upgrade without a factory reset.
