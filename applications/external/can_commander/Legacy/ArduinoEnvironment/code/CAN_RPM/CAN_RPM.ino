#include <SPI.h>
#include <mcp2515.h>

struct can_frame canMsg;
MCP2515 mcp2515(10);

float lastRpm = -1.0;  // Initialize with an invalid value

void setup() {
  Serial.begin(115200);
  
  mcp2515.reset();
  mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ);  // Changed to 500 kbps and 8 MHz
  mcp2515.setNormalMode();
  
  Serial.println("Reading Engine RPM from CAN Bus...");
}

void loop() {
  if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
    if (canMsg.can_id == 0x201) { // Check if the CAN ID is 201
      
      // Combine first two bytes to get RPM data
      unsigned int rpmData = canMsg.data[0] << 8 | canMsg.data[1];
      
      // Calculate the RPM
      float rpm = rpmData / 4.0;
      
      // Update and print the RPM only if it has changed
      if (rpm != lastRpm) {
        lastRpm = rpm;  // Update the lastRpm value
        Serial.print("Engine RPM: ");
        Serial.println(rpm);
      }
    }
  }
}
