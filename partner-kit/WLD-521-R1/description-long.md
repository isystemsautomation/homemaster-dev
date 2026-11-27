HomeMaster® WLD-521-R1 centralises water monitoring in one Modbus RTU slave. Five isolated 5 V digital inputs are active HIGH (sense between the module +5V_ISO rail and Ix) and accept leak probes, soil-moisture contacts or pulse outputs from mechanical meters with configurable pulses-per-litre scaling. Passiveive two-electrode probes wire between +5V_ISO (PS 5Vdc) and Ix — not between Ix and GND_ISO. A protected 1-Wire bus supports up to ten DS18B20 sensors for supply and return temperature.

The firmware computes indicative thermal energy from flow and temperature delta; this figure is informative only and is not MID-certified or suitable for billing. Two relay outputs (3 A @ 250 VAC system rating) can drive valve actuators with external protection. Isolated +12 V and +5 V rails supply small external sensors within published current limits; never connect +12V_ISO to an input (max input 5 V).

RS-485 Modbus RTU reports counters, temperatures, computed power and relay states. USB-C WebConfig configures input modes, debounce, alarm thresholds and Modbus parameters. Logic power is 24 V DC with fuse and TVS protection.

Assign each input to leak, counter or moisture mode before closing the panel. Pair supply and return temperature probes on the 1-Wire bus for heat calculation. State clearly in user documentation that energy totals are indicative, not for utility billing.

Exercise relay outputs with the actual valve actuators before enabling automatic shut-off. Keep isolated sensor rails within the published current limits.

Install inside a DIN-rail enclosure. Use with a Modbus master; local shut-off logic can run on the module while the PLC supervises the bus. CE marked with EU DoC. Open hardware and published firmware. Qualified personnel required for wiring to valves and mains-powered accessories. The pack includes the module datasheet and EU Declaration of Conformity for partner resale documentation.
