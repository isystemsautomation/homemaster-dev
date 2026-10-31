# STR-3221-R1 Firmware v0.4.0

Beta. Current released firmware remains v0.3.0.
Copied from v0.3.0 to open the next work tree. The Modbus register map is unchanged.

| Path | Purpose |
|------|---------|
| `default_str_3221_r1/` | Main Arduino sketch (Modbus, TLC59208F, WebConfig) |
| `ConfigToolPage.html` | USB Web Serial configuration UI |
| `default_str_3221_r1_plc/` | ESPHome Modbus package (32 outputs + 3 inputs) |

Default Modbus: address **3**, baud **19200**.
`CFG_VERSION` is **0x0006** — settings survive a flash from v0.3.0 (`0x0005`) without a factory reset; the migrator keeps the old standby levels and sets first/last channels to Follow step table.
