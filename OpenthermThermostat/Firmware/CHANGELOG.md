# Changelog

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
