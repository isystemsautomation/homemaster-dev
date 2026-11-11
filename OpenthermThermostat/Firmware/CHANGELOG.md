# Changelog

## 1.1.0 — 11 November 2026

- The outside and the room temperature can be pushed in from Home
  Assistant, through Outside Temperature Input and Room Temperature
  Input. Any entity can feed them.
- A pushed value is ignored once it is older than thirty minutes.
- The System page shows the firmware version it was built with.
- Requires ESPHome 2026.10.0 or newer.

## 1.0.0 — 11 November 2026

First release.

- OpenTherm master with the full boiler telemetry set.
- Local control loop: comfort and eco schedule, hysteresis, weather
  compensation.
- Outside and room temperature can each come from the device or from
  Home Assistant.
- Weather compensation can be switched off; with no outside reading the
  boiler is asked for Fixed Flow Setpoint.
- Provisioning window of 15 minutes, no fallback access point.
