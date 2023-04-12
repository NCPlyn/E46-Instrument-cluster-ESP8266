# E46-Instrument-cluster-ESP8266
- E46 Instrumental cluster controlled with ESP8266 over WiFi UDP game telemetry
## Working games:
- Any Forza game (port: 8080)
- BeamNG.drive /OutGauge protocol (port: 8000)
## Future to work games:
- ETS2
- Suggest more or make pull request
## Working:
- Handbrake/parkbrake engagement
- Traction control light
- Blinkers, highbeam
- Wheel speed
- Engine speed
- Coolant temperature
- Fuel level (needs DACs, opamp and resistors, working, check schema)
- Fuel consumption (in reality not true, being faked, check code)
## Easy to implement via CAN or KBUS but not used in much games with telemetry:
- Check engine light, fog lights, oil/coolant temp light, cruise light...
## Harder to implement, needs ESP32 because of lack of pins on 8266:
- Airbag light, battery charge fault, control illumination of cluster...
## How to build:
- Edit wifi credentials in .ino file
- Upload code to ESP
- Connect everything according to schema or header of .ino file
- Connect to power
- Start game and edit telemetry configuration
## Used material to build:
- ESP8266
- MCP2515 CAN board (w/TJA1050)
- Logic level shifter (BSS138)
- Fuel:
  - 2x MCP4725 DAC
  - LM358 opamp
  - 2x 120R resistor
## Used references:
- https://www.ms4x.net/index.php?title=Siemens_MS43_CAN_Bus
- https://github.com/tsharp42/E46ClusterDriver
- https://www.bimmerforums.com/forum/showthread.php?1887229-E46-Can-bus-project
- https://www.bmwgm5.com/E46_IKE_Connections.htm
