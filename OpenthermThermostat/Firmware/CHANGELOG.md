# Changelog

## 1.1.2 — 11 November 2026

- **Boiler DHW Setpoint** and **Boiler Max CH Setpoint** now survive a
  restart. They shipped with neither a stored value nor a default, so
  every boot published them as "not a number": Home Assistant drew the
  slider at an undefined position, the encoder could not move them
  because a detent added to nothing is still nothing, and the boiler was
  handed a hot-water setpoint that meant nothing until somebody typed a
  figure in. They now default to 50 °C and 80 °C and keep whatever they
  are set to.
- The four comfort windows can be set **on the glass**. The Schedule
  menu has `WEEKDAY FROM`, `WEEKDAY TO`, `WEEKEND FROM` and
  `WEEKEND TO`; one detent moves a window ten minutes and it wraps at
  midnight. The page showed them before and let you change nothing.
- Boost moved off the Schedule menu — it is on the Climate menu, where
  it always was, and the repeat cost a row the windows needed.

## 1.1.1 — 11 November 2026

- Colour on the screen now says one thing at a time. Amber means a
  circuit is running — the burner, central heating, hot water, boost.
  A fault is red, on the tile and on the boiler icon in the header. The
  two places that print a word for a healthy state print it green:
  `OK` on the fault tile and `BOILER LINK OK`.
- A tile colours its icon and its value apart. The icon carries the
  state of the circuit, the value stays white because it is a reading:
  stored hot water at 59 °C used to be dimmed to the same grey as a
  missing one whenever nobody was drawing it.
- The Wi-Fi icon carries Wi-Fi and nothing else — white when
  associated, dim and crossed out when not. The Home Assistant link
  used to be hidden in the colour of that icon, grey against dim, which
  cannot be read from across a room. It has its own row on the System
  page now.
- Room humidity can be pushed in from Home Assistant, through **Room
  Humidity Input**. It follows the Room Temperature Source selector:
  set that to Home Assistant and both the room temperature and the
  humidity come from there. The reading straight off the SHT40 is kept
  as Room Humidity Raw, switched off by default.
- A return temperature below 5 °C is discarded. Boilers with no return
  sensor answer 0.00 instead of refusing the request, and the screen
  showed 0 degrees of return water beside 48 degrees of flow.

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
