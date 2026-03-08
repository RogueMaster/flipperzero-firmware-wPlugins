#include <mcp2515.h>

struct can_frame canMsg;
MCP2515 mcp2515(10);

float lastRpm = -1.0;  // Initialize with an invalid value

void setup() {
  Serial.begin(115200);
  
  mcp2515.reset();
  mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ);
  mcp2515.setNormalMode();
}

void loop() {
  if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
    if (canMsg.can_id == 0x201) {
      
      unsigned int rpmData = canMsg.data[0] << 8 | canMsg.data[1];
      float rpm = rpmData / 4.0;

      if (rpm != lastRpm) {
        lastRpm = rpm;

        // Print the RPM value to the serial monitor
        Serial.print("RPM: ");
        Serial.println((int)rpm);
      }
    }
  }
}
