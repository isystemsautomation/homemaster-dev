HomeMaster® AIO-422-R1 extends a Modbus panel with precision analog and temperature measurement. Four voltage inputs (0–10 V, 16-bit ADS1115) and two voltage outputs (0–10 V, 12-bit MCP4725) suit level transmitters, pressure sensors and control signals. Two MAX31865 RTD inputs accept PT100 or PT1000 probes in two-, three- or four-wire configuration.

The module runs as an RS-485 Modbus RTU slave with transient protection and fail-safe biasing; galvanic isolation is not provided. Logic power is 24 V DC. USB-C WebConfig sets scaling, filtering and Modbus parameters; configuration persists in onboard flash.

Typical applications include tank level monitoring, duct temperature measurement, boiler flow loops and environmental sensing alongside HomeMaster PLC controllers. Home Assistant integration is implemented on the Modbus master through ESPHome or comparable integrations—this module does not run ESPHome itself.

Analog inputs share a common reference on the terminal block; route sensor returns on the same 0V domain as the module. RTD leads should follow the wire count selected in WebConfig. Keep analog pairs twisted and separated from relay or mains cabling inside the cabinet.

Calibrate RTD wire mode after installation and before connecting the bus master. Record scaling factors for each analog input in the integrator documentation.

Install inside a cabinet on DIN rail. Use screened pairs for long analog runs where noise is a concern. Observe 0.5 Nm terminal torque on fixed PCB screw terminals, 5.08 mm pitch blocks. CE marked with published EU DoC. Analog output load must stay within the stated per-channel current limit. The pack includes the module datasheet and EU Declaration of Conformity.
