HomeMaster® STR-3221-R1 is a 32-channel low-side MOSFET module for staircase lighting and underfloor-heating actuators. Outputs switch 12–24 V DC loads at 18 A module total and 1.5 A per channel. Each channel is independently dimmable 0–255, or 0–100 % duty on a heat profile. The stair sequence and the heating-zone cycle run on the module and keep working with the controller off. Two presence inputs set direction; one IEC digital input can enable, inhibit or echo heat demand.

Logic power is 24 V DC on V+/0V with fuse, reverse-polarity and TVS protection. LED and actuator power uses a separate supply on dedicated terminals; do not mix logic and load returns. RS-485 Modbus RTU (default address 3, 19200 baud) reports channel, stair and heat state; USB-C provides configuration and firmware update access.

Fused +5 V auxiliary rails supply low-current presence sensors — check the sensor is rated for a 5 V supply before wiring it. The digital input is galvanically isolated (ISO1212) and the presence inputs are opto-isolated (SFH6156, 5.3 kV); the outputs and the RS-485 port are not isolated. Output stages use gate RC networks and ferrites; inductive loads may need external snubbers at the fixture. The module is a Modbus slave for any compliant master, including HomeMaster PLCs with ESPHome modbus integration.

Plan step wiring so return currents share the LED PSU negative and not the logic 0V. Set channel profiles, then fill the Stairs or Heating card in WebConfig. Verify presence sensors in the live view before commissioning. Failsafe ships disabled; heat uses frost protection after the silent hours you set.

Allow adequate airflow above the heat sink path of the LED returns. Commission each flight so presence direction matches traffic flow. Document the Modbus map — v0.2.0 dropped discrete inputs and reads all runtime state in one FC 04 block.

Mount on 35 mm DIN rail inside an IP20 enclosure. Size the LED or actuator PSU for strip length or head inrush. CE marked; EU DoC and datasheet included. Open hardware (CERN-OHL-W v2) and published firmware sources.
