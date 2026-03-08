#include <SPI.h>
#include <mcp2515.h>

struct can_frame canMsg;
MCP2515 mcp2515(10);

float lastRpm = -1.0;  
float lastSpeed = -1.0;
String receivedCommand = "";
bool readRpm = false;
bool readSpeed = false;

void setup() {
  Serial.begin(115200);
  
  mcp2515.reset();
  mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ);  // Changed to 500 kbps and 8 MHz
  mcp2515.setNormalMode();
  
  Serial.println("Waiting for command...");
}

void loop() {
  // Check for incoming serial data
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    receivedCommand += inChar;

    if (inChar == '\n') {  // Check for the end of the command
      if (receivedCommand.startsWith("rpm")) {
        readRpm = true;  
      } else if (receivedCommand.startsWith("speed")) {
        readSpeed = true;
      } else if (receivedCommand.startsWith("stop")) {
        readRpm = false;
        readSpeed = false;
      }
      receivedCommand = "";  // Clear the receivedCommand string
    }
  }

  if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
    if (canMsg.can_id == 0x201) {
      if(readRpm) {
        getRPM();
      }
      if(readSpeed) {
        getSpeed();
      }
    }
  }
}

void getSpeed() {
    unsigned int speedData = canMsg.data[4] << 8 | canMsg.data[5];
    float speedKmh = (speedData - 10000) / 100;
    float speedMph = speedKmh * 0.621371;
    if (speedMph != lastSpeed) {
      lastSpeed = speedMph;
      Serial.print("Speed: ");
      Serial.print(speedMph, 2);
      Serial.println(" mph");
    }
}

void getRPM() {
    unsigned int rpmData = canMsg.data[0] << 8 | canMsg.data[1];
    float rpm = rpmData / 4.0;
    if (rpm != lastRpm) {
      lastRpm = rpm;  
      Serial.print("Engine RPM: ");
      Serial.println(rpm);
    }
}
