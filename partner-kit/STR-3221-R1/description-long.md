HomeMaster® STR-3221-R1 is a high-channel LED controller for sequential staircase and walkway lighting. Thirty-two low-side MOSFET outputs switch 12–24 V DC loads with a module total current limit of 18 A at 24 V and 1.5 A per channel. Each channel is independently dimmable 0–255 over Modbus. Two presence-sensor inputs and one IEC digital input report their state on Modbus; the connected controller decides what the channels do.

Logic power is 24 V DC on V+/0V with fuse, reverse-polarity and TVS protection. LED power uses a separate supply on dedicated terminals; do not mix logic and LED returns. RS-485 Modbus RTU (default address 3, 19200 baud) reports channel and sensor states; USB-C provides configuration and firmware update access.

Fused +5 V auxiliary rails supply low-current presence sensors — check the sensor is rated for a 5 V supply before wiring it. The digital input is galvanically isolated (ISO1212) and the presence inputs are opto-isolated (SFH6156, 5.3 kV); the outputs and the RS-485 port are not isolated. Output stages use gate RC networks and ferrites; inductive loads may need external snubbers at the fixture. The module is a Modbus slave for any compliant master, including HomeMaster PLCs with ESPHome modbus integration.

Plan step wiring so return currents share the LED PSU negative and not the logic 0V. Group channels per flight in the controller's automation — the module has no on-board sequencer or scene table, so the stepping, timing and fades are written once in the PLC or in Home Assistant. Verify presence sensors in WebConfig live view before commissioning that automation.

Allow adequate airflow above the heat sink path of the LED returns. Commission each flight separately so presence direction matches physical traffic flow. Document Modbus register addresses in the integrator handover pack.

Mount on 35 mm DIN rail inside an IP20 enclosure. Size the LED PSU for strip length and wattage per metre. CE marked; EU DoC and datasheet included. Open hardware (CERN-OHL-W v2) and published firmware sources.
