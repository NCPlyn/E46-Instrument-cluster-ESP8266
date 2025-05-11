## !! If you want to see which CAN/K-BUS messages to send to control the cluster, check my implementation here: [r00li/CarCluster-BMWe46](https://github.com/r00li/CarCluster/blob/main/CarCluster/src/Clusters/BMW_E46/BMWE46Cluster.cpp) !!
# E46-Instrument-cluster-ESP8266
- Video of this working: https://youtu.be/MROyCdkAhm4
- E46 Instrumental cluster controlled with ESP8266 over WiFi UDP game telemetry
- If wanted, I'm willing to make plug&play PCB with ESP32-S2 that will plug into the cluster connector, also with ability to control over serial/USB
- I'm not planning to implement this into SimHub custom hardware, if you want to, you can do it yourself and you're free to source from this code **OR** you can simply fordward the UDP data in SimHub configuration to the ESP
## Working games:
- Any Forza game (port: 8080)
- BeamNG.drive / OutGauge protocol (port: 8000)
  ### Future to work games:
  - ETS2
  - Assetto Corsa
  - ...or make pull request with needed code
## Working instrument functions:
- Handbrake/parkbrake engagement
- Traction control light
- Blinkers, highbeam
- Wheel speed
- Engine speed
- Coolant temperature
- Fuel level (needs DACs, opamp and resistors, working, check schema)
- Fuel consumption (in reality not true, being faked, check code)
  ### Easy to implement via CAN or KBUS but not used in much games with telemetry:
  - Check engine light, fog lights, oil/coolant temp light, cruise light...
  ### Harder to implement, needs ESP32 because of lack of pins on 8266:
  - Airbag light, battery charge fault, control illumination of cluster...
## Used material to build:
- ESP8266
- MCP2515 CAN board (w/TJA1050)
- Logic level shifter (BSS138)
- Fuel:
  - 2x MCP4725 DAC
  - LM358 opamp
  - 2x 120R resistor
  ### How to build:
  - Edit wifi credentials in .ino file
  - Upload code to ESP
  - Connect everything according to schema or header of .ino file
  - Connect to power
  - Start game and edit telemetry configuration
## Used references:
- https://www.ms4x.net/index.php?title=Siemens_MS43_CAN_Bus
- https://github.com/tsharp42/E46ClusterDriver
- https://www.bimmerforums.com/forum/showthread.php?1887229-E46-Can-bus-project
- https://www.bmwgm5.com/E46_IKE_Connections.htm
