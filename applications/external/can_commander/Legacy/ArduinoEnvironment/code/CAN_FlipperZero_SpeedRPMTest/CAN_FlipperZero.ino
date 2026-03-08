#include <SPI.h>
#include <mcp2515.h>

struct can_frame canMsg;
MCP2515 mcp2515(10);

enum Mode { NONE, RPM, SPEED };
Mode currentMode = NONE;

void setup() {
  Serial.begin(115200);
  
  mcp2515.reset();
  mcp2515.setBitrate(CAN_125KBPS, MCP_8MHZ);  // Set the speed for MS CAN bus
  mcp2515.setNormalMode();
  
  Serial.println("Enter 'rpm' to display RPM or 'speed' to display speed:");
}

void loop() {
  // Check for user input
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim(); // Remove any whitespace or newline characters

    if (input == "rpm") {
      currentMode = RPM;
      Serial.println("Displaying RPM values...");
    } else if (input == "speed") {
      currentMode = SPEED;
      Serial.println("Displaying Speed values...");
    } else {
      currentMode = NONE;
      Serial.println("Invalid choice. Enter 'rpm' or 'speed'.");
    }
  }

  if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
    if (canMsg.can_id == 0x201) { 
      if (currentMode == RPM) {
        unsigned int rpmData = canMsg.data[0] << 8 | canMsg.data[1];
        float rpm = rpmData / 4.0;
        Serial.print("RPM: ");
        Serial.println(rpm);
      } else if (currentMode == SPEED) {
        unsigned int speedData = canMsg.data[4] << 8 | canMsg.data[5];
        float speedKmh = (speedData - 10000) / 100;
        float speedMph = speedKmh * 0.621371;
        Serial.print("Speed: ");
        Serial.print(speedMph, 2);
        Serial.println(" mph");
      }
    }
  }
}
