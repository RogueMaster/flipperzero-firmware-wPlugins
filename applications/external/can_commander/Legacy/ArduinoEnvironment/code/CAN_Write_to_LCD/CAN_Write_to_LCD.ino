#include <SPI.h>
#include <mcp2515.h>

// CAN Write to LCD by Matthew KuKanich
struct can_frame canMsg;
MCP2515 mcp2515(10);

void setup() {
    Serial.begin(115200);
  
    mcp2515.reset();
    mcp2515.setBitrate(CAN_125KBPS, MCP_8MHZ);
    mcp2515.setNormalMode();
  
    Serial.println("Listening and Modifying MS CAN Bus...");
}
// These are CAN ID's for my display
// For older cars / non smart cars
// The LCD display will work very similarly
void loop() {
    if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
        if (canMsg.can_id == 0x290) {
            canMsg.data[1] = 'H';
            canMsg.data[2] = 'E';
            canMsg.data[3] = 'L';
            canMsg.data[4] = 'L';
            canMsg.data[5] = 'L';
            canMsg.data[6] = 'L';
            canMsg.data[7] = 'L';
            mcp2515.sendMessage(&canMsg);  // Transmit the modified message for 290
        } else if (canMsg.can_id == 0x291) {
            canMsg.data[1] = 'H';
            canMsg.data[2] = 'E';
            canMsg.data[3] = 'L';
            canMsg.data[4] = 'L';
            canMsg.data[5] = 'L';
            canMsg.data[6] = 'L';
            canMsg.data[7] = 'L';
            mcp2515.sendMessage(&canMsg);  // Transmit the modified message for 291
        }
    }
}
