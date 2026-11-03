HomeMaster® RGB-621-R1 is a DIN-rail LED driver for RGB, RGBW and tunable-white installations. Five low-side MOSFET channels (R, G, B, cool white, warm white) operate at approximately 1 kHz with 12-bit dimming, gamma correction and configurable fade profiles. LED power comes from a separate 12 or 24 V DC supply on the LED PS terminals, fused at 10 A for the whole module; any single channel may use the full budget or share it across active channels.

Logic and LED supplies must remain separate; common-anode wiring connects strip positive to COM (LED+). Two dry-contact inputs and one relay output (3 A @ 250 VAC system rating) support wall switches and external LED PSU switching. RS-485 Modbus RTU exposes levels and states; USB-C WebConfig handles scenes, trim and timing.

Terminal blocks are DB129V-5.08 rated 24 A per pole; thermal limits of the 3-DIN module may apply before the fuse. Reverse-polarity protection on the LED feed uses an ideal-diode stage (LM74610 with N-channel MOSFET).

Do not bridge GND_FUSED and logic GND externally. Size conductors for the maximum channel current you plan to use. Configure minimum and maximum trim levels in WebConfig before handing over to the end user so off-state leakage matches the fixture.

Verify fade times with the installed strip driver before handing over to the end user. Keep LED PSU negative separate from logic 0V at all times.

Install inside a cabinet. Size the external LED PSU for strip length and W/m. Qualified personnel only. CE marked with EU DoC. Map the module from a Modbus master; typical Home Assistant setups use ESPHome on a HomeMaster PLC.
