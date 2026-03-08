#include <SPI.h>
#include <mcp2515.h>

struct can_frame canMsg;
MCP2515 mcp2515(10);

void setup() {
  Serial.begin(115200);
  
  mcp2515.reset();
  mcp2515.setBitrate(CAN_125KBPS, MCP_8MHZ);  // Set the speed for MS CAN bus
  mcp2515.setNormalMode();
  
  Serial.println("Reading LCD Values from CAN Bus...");
}

void loop() {
  if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
    if (canMsg.can_id == 0x290 || canMsg.can_id == 0x291) {
      // Decode and print the message for LCD
      Serial.print("ID: ");
      Serial.print(canMsg.can_id, HEX);
      Serial.print(" - ");
      
      for (int i = 0; i < canMsg.can_dlc; i++) {
        char ch = (char)canMsg.data[i];  // Convert ASCII byte code to character
        Serial.print(ch);
      }
      Serial.println();
    }
  }
}
