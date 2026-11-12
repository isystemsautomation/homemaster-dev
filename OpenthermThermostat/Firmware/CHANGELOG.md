# Changelog

## 1.1.4 — 12 November 2026

- The central heating setpoint the thermostat asks the boiler for is now a
  number, **Boiler CH Setpoint**, instead of an output with no name. Nothing
  changes on the OpenTherm bus: the same message carries the same value, and
  the heating behaves exactly as before. What changes is that the figure the
  control loop has worked out is now visible in Home Assistant instead of
  being hidden inside the firmware.
- The thermostat keeps ownership of that number. It writes the flow setpoint
  its own weather curve produced, and anything typed in by hand is overwritten
  on the next pass of the control loop.
- The change was required by the ESPHome device catalogue: its automated
  review insists that every component in a published configuration carries an
  identifier, and an OpenTherm output cannot carry one — ESPHome does not keep
  an identifier at that level.

## 1.1.3 — 11 November 2026

- **The schedule keeps local time without Home Assistant.** NTP carries
  the moment but never the zone, and no zone was set in the
  configuration, so the firmware inherited the timezone of whatever
  machine built it — UTC. A thermostat with Home Assistant was unaffected
  because Home Assistant sends its own zone from 2026.3.0 and the device
  applies it. One running on its own, which is how this product is meant
  to be able to run, held its comfort window one or two hours late and
  kept heating past midnight, with a clock on the glass that looked
  perfectly correct.
- Two settings on the System page, in the menu and in Home Assistant,
  which between them describe any place on earth: **UTC Offset**, from
  −12 to +14 in quarter hours, because not every country is on the hour;
  and **Summer Time** — `Off`, `Europe`, `North America`, `Australia`,
  `New Zealand`. `Off` gives correct standard time the year round
  anywhere the four rules do not apply. Both are kept across restarts and
  applied before the first schedule pass.
- The System page shows the zone in force, as `UTC+1 EU` and the like.
  With Home Assistant connected it reads `FROM HA`, because the zone then
  comes from there and these two settings are not what the clock is
  using.
- The configuration now states `Europe/Berlin` as the built-in default,
  so a factory image never again carries the build server's timezone.

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
