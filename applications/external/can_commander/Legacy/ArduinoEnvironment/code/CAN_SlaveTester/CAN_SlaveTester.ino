#include <SPI.h>          // Library for using SPI Communication 
#include <mcp2515.h>     

struct can_frame canMsg;
MCP2515 mcp2515(10);

void setup() {
  while (!Serial);
  Serial.begin(115200);
  SPI.begin();               // Begins SPI communication

  mcp2515.reset();
  mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ); // Sets CAN at speed 500KBPS and Clock 8MHz
  mcp2515.setNormalMode();
}

void loop() {
  // Echo Received CAN Messages
  if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
    Serial.print("Received message with ID: 0x");
    Serial.print(canMsg.can_id, HEX);
    Serial.print(" DLC: ");
    Serial.print(canMsg.can_dlc);
    Serial.print(" Data: ");
    for (int i = 0; i < canMsg.can_dlc; i++) {
      Serial.print(canMsg.data[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
  }

  // Send Own Messages at a Set Frequency
  static unsigned long lastMessageTime = 0;
  static bool toggle = false; // Toggle between two messages

  if (millis() - lastMessageTime >= 400) {
    lastMessageTime = millis();

    if (toggle) {
      // Message 1
      canMsg.can_id = 0x210;
      canMsg.can_dlc = 6;
      byte message1[6] = {0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x21};
      memcpy(canMsg.data, message1, sizeof(message1));
    } else {
      // Message 2
      canMsg.can_id = 0x4D5;
      canMsg.can_dlc = 8;
      byte message2[8] = {0x54, 0x65, 0x73, 0x74, 0x69, 0x6E, 0x67, 0x21};
      memcpy(canMsg.data, message2, sizeof(message2));
    }

    mcp2515.sendMessage(&canMsg);
    toggle = !toggle; // Switch to the other message for next time
  }
}
