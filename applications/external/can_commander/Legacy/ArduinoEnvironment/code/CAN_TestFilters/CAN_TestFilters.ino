// CAN filter tester by Matthew KuKanich
#include <SPI.h>
#include <mcp2515.h>

const int SPI_CS_PIN = 9; // Change this to your CS pin

MCP2515 mcp2515(SPI_CS_PIN);
can_frame canMsg;

void setup() {
    Serial.begin(115200);
    while (!Serial); // Wait for serial port to connect

    mcp2515.reset();
    mcp2515.setBitrate(CAN_500KBPS, MCP_16MHZ);
    mcp2515.setConfigMode();

    // Set both masks to check all 29 bits (0x1FFFFFFF)
    mcp2515.setFilterMask(MCP2515::MASK0, false, 0x7FF);
    mcp2515.setFilterMask(MCP2515::MASK1, false, 0x7FF);

    // Set all filters to only allow CAN ID 0x1A1010B1
    mcp2515.setFilter(MCP2515::RXF0, false, 0x225);
    mcp2515.setFilter(MCP2515::RXF1, false, 0x225);
    mcp2515.setFilter(MCP2515::RXF2, false, 0x225);
    mcp2515.setFilter(MCP2515::RXF3, false, 0x225);
    mcp2515.setFilter(MCP2515::RXF4, false, 0x225);
    mcp2515.setFilter(MCP2515::RXF5, false, 0x225);

    mcp2515.setNormalMode();
}

void loop() {
    if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
        // Since hardware filtering is set up, all messages read here should have the ID 0x210
        Serial.print("ID: ");
        Serial.print(canMsg.can_id, HEX);
        Serial.print(" DLC: ");
        Serial.print(canMsg.can_dlc, HEX);
        Serial.print(" Data: ");
        for (int i = 0; i < canMsg.can_dlc; i++) {
            Serial.print(canMsg.data[i], HEX);
            Serial.print(" ");
        }
        Serial.println();
    }
}

