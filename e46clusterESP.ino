//before upload, change wifi credentials
//IP to set in game is the one printed out in serial or espcluster.local
//supported game : port to set in game
//FH4 / FH5 / FM    : 8080
//BeamNG / OutGauge : 8000

//https://www.ms4x.net/index.php?title=Siemens_MS43_CAN_Bus
//https://github.com/tsharp42/E46ClusterDriver
//https://www.bimmerforums.com/forum/showthread.php?1887229-E46-Can-bus-project

//pinout esp8266 = refer to github or this & (https://www.bmwgm5.com/E46_IKE_Connections.htm):
//GND = ESP-GND | BLK-1,20,22,24,26 | WHT-4
//12V = ESP-VIN | BLK-4,5,6,7
//D6(12/MISO), D7(13/MOSI), D5(14/CLK), D8(15/CS) : SPI<->CAN (9-BLK:CANHI , 10-BLK:CANLOW)
//D4(2/TXD1) : KBUS via level shifter (14-BLK)
//D3(0) : speed tone via lever shifter (19-BLK)
//D0(16) : park brake via level shifter (23-BLK)
//D1(5/SCL), D2(4,SDA) : fuel DAC 0x60(12-BLK:TANK1- , 11-BLK:TANK1+) 0x61(16-BLK:TANK2- , 15-BLK:TANK2+) both with 120R resistor with opamp in voltage follower

#include <SPI.h>
#include <mcp2515.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>
#include <Wire.h>
#include <Adafruit_MCP4725.h>

const char* ssid = "wifiname";
const char* pwd = "wifipass";

int availPorts[] = {8000,8080};
int udpPort = 8000;
WiFiUDP udp;

struct can_frame DME2, DME1, ASC1, DME4;
MCP2515 mcp2515(15);

byte kbusmsg[] = {0xD0, 0x08, 0xBF, 0x5B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x58, 0x00};

Adafruit_MCP4725 Tank1, Tank2;

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(10);

  Serial1.begin(9600, SERIAL_8E1);

  Tank1.begin(0x60);
  Tank2.begin(0x61);

  WiFi.hostname("espcluster");
  WiFi.begin(ssid, pwd);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  MDNS.begin("espcluster");

  udp.begin(udpPort);

  DME1.can_id  = 0x316; //engine speed = data[2]=LSB, data[3]=MSB : (rpm*6.4 = b3&&b2)
  DME1.can_dlc = 8;
  DME1.data[0] = 0x05;
  DME1.data[1] = 0x14;
  DME1.data[2] = 0x80;
  DME1.data[3] = 0xA2;
  DME1.data[4] = 0x14;
  DME1.data[5] = 0x17;
  DME1.data[6] = 0x0;
  DME1.data[7] = 0x16;
  
  DME2.can_id  = 0x329; //cruise, throtle, temp = data[1] : (bin = (c+48,373)/0,75)
  DME2.can_dlc = 8;
  DME2.data[0] = 0x07;
  DME2.data[1] = 0xA9;
  DME2.data[2] = 0xB2;
  DME2.data[3] = 0x19;
  DME2.data[4] = 0x0;
  DME2.data[5] = 0xEE;
  DME2.data[6] = 0x0;
  DME2.data[7] = 0x0;

  DME4.can_id  = 0x545; //fuel usage = data[1]=LSB, data[2]=MSB (rate of change), data[0] = check engine, oil temp warn, water temp warn
  DME4.can_dlc = 8;
  DME4.data[0] = 0x0;
  DME4.data[1] = 0x0;
  DME4.data[2] = 0x0;
  DME4.data[3] = 0x0;
  DME4.data[4] = 0x7E;
  DME4.data[5] = 0x10;
  DME4.data[6] = 0x0;
  DME4.data[7] = 0x18;

  ASC1.can_id  = 0x153; //asc demand
  ASC1.can_dlc = 8;
  ASC1.data[0] = 0x0;
  ASC1.data[1] = 0x0;
  ASC1.data[2] = 0x0;
  ASC1.data[3] = 0x0;
  ASC1.data[4] = 0x0;
  ASC1.data[5] = 0x0;
  ASC1.data[6] = 0x0;
  ASC1.data[7] = 0x0;

  mcp2515.reset();
  mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ);
  mcp2515.setNormalMode();

  pinMode(0,OUTPUT);
  pinMode(16,OUTPUT);
}

static union {
    uint32_t i;
    uint8_t b[sizeof (float)];
    float f;
} nums;

int speedIn = 0, rpmIn = 0, tempIn = 40, parkIn = 0, lightIn = 0, ascIn = 0, mpgloop = 0, fuelIn = 0, lastFuel = 0, curPort = 0;
unsigned long delayVar, lastIndicatorL, lastIndicatorR, lastPacket, lastScan;

void loop() {
  if(lastPacket+10000 < millis() && lastScan+2000 < millis()) {
    udp.stop();
    if(curPort+1 == sizeof(availPorts)) {
      curPort = 0;
    } else {
      curPort++;
    }
    udpPort = availPorts[curPort];
    udp.begin(udpPort);
    lastScan = millis();
  }

  char packet_buffer[600];
  int packet_size = udp.parsePacket();
  if (packet_size) {
    int len = udp.read(packet_buffer, packet_size);
    if (len > 0) {
      lastPacket = millis();
      if(udpPort == 8080) { //fh5/fh4...
        tempIn = 90;
        lightIn = 0;

        nums.b[0] = packet_buffer[96]; //ASC
        nums.b[1] = packet_buffer[97];
        nums.b[2] = packet_buffer[98];
        nums.b[3] = packet_buffer[99];
        if(nums.f > 1) {
          ascIn = 1;
        } else {
          ascIn = 0;
        }

        nums.b[0] = packet_buffer[16]; //RPM
        nums.b[1] = packet_buffer[17];
        nums.b[2] = packet_buffer[18];
        nums.b[3] = packet_buffer[19];
        rpmIn = nums.f;

        nums.b[0] = packet_buffer[256]; //speed
        nums.b[1] = packet_buffer[257];
        nums.b[2] = packet_buffer[258];
        nums.b[3] = packet_buffer[259];
        speedIn = nums.f*3.6;

        if(packet_buffer[318] == 255) { //pbrake
          parkIn = 1;
        } else {
          parkIn = 0;
        }
      }
      if(udpPort == 8000) { //beamng
        // TEMP
        nums.b[0] = packet_buffer[24];
        nums.b[1] = packet_buffer[25];
        nums.b[2] = packet_buffer[26];
        nums.b[3] = packet_buffer[27];
        tempIn = nums.f;
                
        // RPM
        nums.b[0] = packet_buffer[16];
        nums.b[1] = packet_buffer[17];
        nums.b[2] = packet_buffer[18];
        nums.b[3] = packet_buffer[19];
        rpmIn = nums.f;
    
        // Speed
        nums.b[0] = packet_buffer[12];
        nums.b[1] = packet_buffer[13];
        nums.b[2] = packet_buffer[14];
        nums.b[3] = packet_buffer[15];
        speedIn = nums.f * 3.6;
    
        // Fuel %
        nums.b[0] = packet_buffer[28];
        nums.b[1] = packet_buffer[29];
        nums.b[2] = packet_buffer[30];
        nums.b[3] = packet_buffer[31];
        fuelIn = nums.f * 100.0;
    
        // Lights
        nums.b[0] = packet_buffer[44];
        nums.b[1] = packet_buffer[45];
        nums.b[2] = packet_buffer[46];
        nums.b[3] = packet_buffer[47];
    
        if ((nums.i & 0x0040) != 0) { //right
          bitWrite(lightIn,6,1);
          lastIndicatorR = millis();
        } else {
          if(lastIndicatorR+500 < millis()) {
            bitWrite(lightIn,6,0);
          }
        }
        
        if ((nums.i & 0x0020) != 0) { //left
          bitWrite(lightIn,5,1);
          lastIndicatorL = millis();
        } else {
          if(lastIndicatorL+500 < millis()) {
            bitWrite(lightIn,5,0);
          }
        }
    
        if ((nums.i & 0x0002) != 0) { //highbeams
          bitWrite(lightIn,2,1);
        } else {
          bitWrite(lightIn,2,0);
        }
    
        if ((nums.i & 0x0010) != 0) { //TSC
          ascIn = 1;
        } else {
          ascIn = 0;
        }
    
        if ((nums.i & 0x0400) != 0) { //ABS
          ascIn = 1;
        } else {
          ascIn = 0;
        }
    
        if ((nums.i & 0x0004) != 0) { //handbrake
          parkIn = 1;
        } else {
          parkIn = 0;
        }
      }
    }
    udp.flush();
  }

  //park
  if(parkIn) {
    digitalWrite(16,LOW);
  } else {
    digitalWrite(16,HIGH);
  }

  //speed gauge
  if(speedIn < 1) {
    noTone(0);
  } else {
    tone(0, 6.6 * speedIn);
  }
  
  if(fuelIn != lastFuel) {
    lastFuel = fuelIn;
    //0 = 0v, 4095 = vref(6v)
    //tank2 max 33L || tank1 max 23.3L
    int outVal = map(fuelIn, 0, 100, 500, 2070);
    if(outVal > 1700) {
      Tank1.setVoltage(1700,false);
    } else {
      Tank1.setVoltage(outVal,false);
    }
    Tank2.setVoltage(outVal,false);
  }

  if(millis() > delayVar+6) {
    delayVar = millis();

    //DME1 - rpm
    int tempRPM = rpmIn*6.4;
    DME1.data[2] = tempRPM & 0xff;
    DME1.data[3] = tempRPM >> 8;
    mcp2515.sendMessage(&DME1);
    delay(1);
  
    //DME2 - temperature
    DME2.data[1] = (uint8_t)(tempIn+48.373)/0.75;
    mcp2515.sendMessage(&DME2);
    delay(1);
  
    //DME4 - L/100km(is computed in cluster from speed and this rate of change), not done & check engine & overheat
    if(mpgloop == 0xFFFF) {
      mpgloop = 0x0;
    } else {
      mpgloop += rpmIn/130;
    }
    DME4.data[1] = mpgloop & 0xff;
    DME4.data[2] = mpgloop >> 8;
    mcp2515.sendMessage(&DME4);
    delay(1);
  
    //ASC1 - asc intervention
    if(ascIn) {
      ASC1.data[1] = 0x01;
    } else {
      ASC1.data[1] = 0x00;
    }
    mcp2515.sendMessage(&ASC1);
    delay(1);
  
    //K-bus
    if(lightIn != kbusmsg[4]) {
      kbusmsg[4] = lightIn;
      sendKbus(kbusmsg);
    }
  }
}

void sendKbus(byte *data) {
  int end_i = data[1]+2 ;
  data[end_i-1] = iso_checksum(data, end_i-1);
  Serial1.write(data, end_i + 1);
}

byte iso_checksum(byte *data, byte len) { //len is the number of bytes (not the # of last byte)
  byte crc=0;
  for(byte i=0; i<len; i++) {    
    crc=crc^data[i];
  }
  return crc;
}
