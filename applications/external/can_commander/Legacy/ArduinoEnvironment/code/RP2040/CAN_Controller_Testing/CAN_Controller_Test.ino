// Custom CAN bus Controller Tester by Matthew KuKanich
#include <SPI.h>
#include <mcp2515.h>

bool asciiMode = false;  // False for HEX as default output
struct can_frame canMsg;
MCP2515 mcp2515(9);  // CS pin 9 for me, 10 for most

enum Mode {
  MODE_NONE,
  MODE_READ_ALL,
  MODE_READ_FILTERED,
  MODE_WRITE,
  MODE_SPEEDTEST
};

Mode currentMode = MODE_NONE;
int messageFrequency = 1;  // 1/second default

void setup() {
  Serial.begin(115200);
  while (!Serial)
    ;  // Wait for serial port to connect.

  mcp2515.reset();
  mcp2515.setBitrate(CAN_500KBPS, MCP_16MHZ);  // Set to default HS CAN bus, use 125 for MS
  mcp2515.setConfigMode();

  Serial.println("Select mode:");
  Serial.println("1: CAN Read All Traffic");
  Serial.println("2: CAN Write");
  Serial.println("3: CAN Speedtest");
  Serial.println("4: CAN Read Filtered Traffic");

  while (currentMode == MODE_NONE) {
    if (Serial.available() > 0) {
      int modeSelect = Serial.parseInt();
      switch (modeSelect) {
        case 1:
          currentMode = MODE_READ_ALL;
          Serial.println("CAN Read All Traffic selected.");
          Serial.println("Press 'a' to toggle ASCII mode on/off");
          mcp2515.setNormalMode();
          break;
        case 2:
          currentMode = MODE_WRITE;
          Serial.println("Enter CAN message to send (ID DLC DATA...):");
          while (!Serial.available())
            ;  // Wait for user input
          prepareCanWrite();
          Serial.println("CAN Write selected.");
          mcp2515.setNormalMode();
          break;
        case 3:
          currentMode = MODE_SPEEDTEST;
          Serial.println("CAN Speedtest selected.");
          mcp2515.setNormalMode();
          break;
        case 4:
          currentMode = MODE_READ_FILTERED;
          setupCanFilters();
          mcp2515.setNormalMode();
          Serial.println("CAN Read Filtered Traffic selected.");
          Serial.println("Press 'a' to toggle ASCII mode on/off");
          break;
        default:
          Serial.println("Invalid selection. Select 1, 2, 3, or 4.");
          break;
      }
    }
  }
}

void loop() {
  switch (currentMode) {
    case MODE_READ_ALL:
      if (Serial.available()) {
        char input = Serial.read();
        if (input == 'a' || input == 'A') {
          asciiMode = !asciiMode;
          Serial.print("ASCII mode ");
          Serial.println(asciiMode ? "ON" : "OFF");
        }
      }
      canRead();
      break;
    case MODE_WRITE:
      canWrite();
      break;
    case MODE_SPEEDTEST:
      canSpeedtest();
      break;
    case MODE_READ_FILTERED:
      if (Serial.available()) {
        char input = Serial.read();
        if (input == 'a' || input == 'A') {
          asciiMode = !asciiMode;
          Serial.print("ASCII mode ");
          Serial.println(asciiMode ? "ON" : "OFF");
        }
      }
      canRead();
      break;
    default:
      // No default case
      break;
  }
}

bool isPrintable(uint8_t byte) {
  return byte >= 32 && byte <= 126;
}

void canRead() {
  if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {

    Serial.print("ID: ");
    Serial.print(canMsg.can_id, HEX);
    Serial.print(" DLC: ");
    Serial.print(canMsg.can_dlc, HEX);
    Serial.print(" Data: ");

    for (int i = 0; i < canMsg.can_dlc; i++) {  // print the data
      if (asciiMode) {
        // Print as ASCII characters
        if (isPrintable(canMsg.data[i])) {
          Serial.print((char)canMsg.data[i]);
        } else {
          Serial.print('.');
        }
      } else {
        Serial.print(canMsg.data[i], HEX);
        Serial.print(" ");
      }
    }
    Serial.println();
  }
}

void setupCanFilters() {
  char inputBuffer[10];  // Buffer for the input

  // Clear the Serial buffer
  while (Serial.available() > 0) {
    Serial.read();
  }

  // Read mask in hex
  Serial.println("Enter Mask in HEX (e.g., 7FF for all bits):");
  while (!Serial.available())
    ;  // Wait for user input
  Serial.readBytesUntil('\n', inputBuffer, sizeof(inputBuffer) - 1);
  inputBuffer[sizeof(inputBuffer) - 1] = 0;  // Ensure null-termination
  uint32_t mask = strtol(inputBuffer, NULL, 16);

  while (Serial.available() > 0) {
    Serial.read();
  }

  // Read filter in hex
  Serial.println("Enter Filter in HEX (e.g., 201 for specific ID):");
  while (!Serial.available())
    ;
  Serial.readBytesUntil('\n', inputBuffer, sizeof(inputBuffer) - 1);
  inputBuffer[sizeof(inputBuffer) - 1] = 0;
  uint32_t filter = strtol(inputBuffer, NULL, 16);


  Serial.print("Setting Mask: 0x");
  Serial.println(mask, HEX);
  Serial.print("Setting Filter: 0x");
  Serial.println(filter, HEX);

  mcp2515.setFilterMask(MCP2515::MASK0, false, mask);  // You must setup all masks/filters
  mcp2515.setFilterMask(MCP2515::MASK1, false, mask);  // Even when you only need 1

  mcp2515.setFilter(MCP2515::RXF0, false, filter);
  mcp2515.setFilter(MCP2515::RXF1, false, filter);
  mcp2515.setFilter(MCP2515::RXF2, false, filter);
  mcp2515.setFilter(MCP2515::RXF3, false, filter);
  mcp2515.setFilter(MCP2515::RXF4, false, filter);
  mcp2515.setFilter(MCP2515::RXF5, false, filter);
}


void prepareCanWrite() {
  char inputBuffer[20];  // Buffer size

  // Function to clear the Serial buffer
  auto clearSerialBuffer = []() {
    while (Serial.available() > 0) {
      Serial.read();
    }
  };

  // Read CAN ID in hex
  Serial.println("Enter CAN ID in HEX:");
  clearSerialBuffer();
  while (!Serial.available())
    ;                                           // Wait for user input
  memset(inputBuffer, 0, sizeof(inputBuffer));  // Clear input buffer
  Serial.readBytesUntil('\n', inputBuffer, sizeof(inputBuffer) - 1);
  canMsg.can_id = strtol(inputBuffer, NULL, 16);
  Serial.print("CAN ID: 0x");
  Serial.println(canMsg.can_id, HEX);

  // Read CAN DLC in decimal
  Serial.println("Enter CAN DLC:");
  clearSerialBuffer();
  while (!Serial.available())
    ;                                           // Wait for user input
  memset(inputBuffer, 0, sizeof(inputBuffer));  // Clear input buffer
  Serial.readBytesUntil('\n', inputBuffer, sizeof(inputBuffer) - 1);
  canMsg.can_dlc = strtol(inputBuffer, NULL, 10);
  Serial.print("DLC: ");
  Serial.println(canMsg.can_dlc);

  // Read CAN DATA in hex
  for (int i = 0; i < canMsg.can_dlc; i++) {
    Serial.print("Enter DATA byte ");
    Serial.print(i);
    Serial.println(" in HEX:");
    clearSerialBuffer();
    while (!Serial.available())
      ;                                           // Wait for user input
    memset(inputBuffer, 0, sizeof(inputBuffer));  // Clear input buffer
    Serial.readBytesUntil('\n', inputBuffer, sizeof(inputBuffer) - 1);
    canMsg.data[i] = strtol(inputBuffer, NULL, 16);
    Serial.print("Data byte ");
    Serial.print(i);
    Serial.print(": 0x");
    Serial.println(canMsg.data[i], HEX);
  }

  Serial.println("Custom message prepared.");
}


void canWrite() {
  Serial.println("Enter CAN Write Speed (messages/s):");
  while (Serial.available() > 0) {
    Serial.read();
  }
  while (!Serial.available())
    ;  // Wait for user input

  // Reading the entire frequency value as an integer
  messageFrequency = Serial.parseInt();
  Serial.print("Message frequency set to: ");
  Serial.print(messageFrequency);
  Serial.println(" messages/s");

  // Calculating delay based on message frequency
  int messageDelay = 1000 / messageFrequency;

  // Sending messages at the specified frequency
  while (true) {  // TODO: Exit condition
    mcp2515.sendMessage(&canMsg);
    Serial.println("Message sent");
    delay(messageDelay);  // Delay based on frequency
  }
}


void canSpeedtest() {
  static int cntr = 0;
  static unsigned long oldTime = 0;

  if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
    cntr++;
  }

  if ((millis() - oldTime) > 1000) {
    oldTime = millis();
    Serial.print(cntr);
    Serial.println(" msg/sec");
    cntr = 0;
  }
}
