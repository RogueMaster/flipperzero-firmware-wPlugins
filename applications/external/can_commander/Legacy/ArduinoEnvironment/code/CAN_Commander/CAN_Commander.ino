/**
 * @file CAN_Commander.ino
 * @brief Custom CAN bus Controller by Matthew KuKanich
 */

#include <mcp2515.h>
#include <Arduino.h>
#include <SPI.h>

#ifdef ESP32
#include <esp_task_wdt.h> // Add support for ESP32 watchdog timer
#else
#include <avr/wdt.h> // Include the AVR watchdog timer header
#endif

#define WDT_TIMEOUT 1 // Watchdog timeout in seconds

// Some service PIDs, will add more later and eventually move to SD card
#define PID_COOLANT_TEMP 0x05
#define PID_ENGINE_RPM 0x0C
#define PID_VEHICLE_SPEED 0x0D
#define PID_THROTTLE_POSITION 0x11
#define PID_ENGINE_LOAD 0x04
#define PID_FUEL_LEVEL 0x2F
#define PID_INTAKE_TEMP 0x0F
#define PID_BAROMETRIC_PRESSURE 0x33
#define PID_RUNTIME 0x1F
#define PID_DISTANCE 0x31
#define PID_CONTROL_MODULE_VOLTAGE 0x42
#define PID_AMBIENT_TEMP 0x46
#define PID_FUEL_RAIL_PRESSURE 0x59
#define PID_FUEL_RAIL_GAUGE 0x5A
#define PID_O2_SENSOR_VOLTAGE 0x14
#define PID_O2_SENSOR_FUEL_AIR_EQUIVALENCE_RATIO 0x44
#define PID_O2_SENSOR_CURRENT 0x24
#define PID_O2_SENSOR_RESPONSE_TIME 0x26
#define PID_O2_SENSOR_CONTROL_MODULE_VOLTAGE 0x3C
#define PID_O2_SENSOR_FUEL_AIR_EQUIVALENCE_RATIO_VOLTAGE 0x3D
#define PID_O2_SENSOR_CURRENT_VOLTAGE 0x3E
#define PID_FUEL_PRESSURE 0x0A
#define PID_ENGINE_OIL_TEMP 0x5C
#define PID_ENGINE_OIL_PRESSURE 0x5D
#define PID_ENGINE_FUEL_RATE 0x5E
#define PID_ODOMETER 0xA6

bool asciiMode = false; // False for HEX as default output
bool stopExecution = false;

struct can_frame canMsg;
MCP2515 mcp2515(9); // CS pin 9 for me, 10 for most

enum Mode
{
  MODE_NONE,
  MODE_READ_ALL,
  MODE_READ_FILTERED,
  MODE_WRITE,
  MODE_SPEEDTEST,
  MODE_VALTRACKER,
  MODE_PID_MANAGER
};

Mode currentMode = MODE_NONE;
int messageFrequency = 1; // 1 second default
uint8_t pid = 0;          // Declare the pid variable

void setup()
{

  #ifdef ESP32
  // Initialize and enable the watchdog timer for ESP32
  esp_task_wdt_init(WDT_TIMEOUT, true); // Timeout in seconds
  esp_task_wdt_add(NULL); // Add the current task to the watchdog
  #else
  wdt_disable(); // For AVR, disable the watchdog
  #endif

  Serial.begin(115200);
  while (!Serial)
    ; // Wait for serial port to connect.

  mcp2515.reset();
  mcp2515.setBitrate(CAN_500KBPS, MCP_16MHZ); // Set to default HS CAN bus, use 125 for MS
  mcp2515.setConfigMode();

  Serial.println("Select mode:");
  Serial.println("1: CAN Read All Traffic");
  Serial.println("2: CAN Write");
  Serial.println("3: CAN Speedtest");
  Serial.println("4: CAN Read Filtered Traffic");
  Serial.println("5: Filter and Track CAN Values");
  Serial.println("6: Diagnostics and PID Manager");

  while (currentMode == MODE_NONE)
  {
    if (Serial.available() > 0)
    {
      int modeSelect = Serial.parseInt();
      switch (modeSelect)
      {
      case 1:
        currentMode = MODE_READ_ALL;
        Serial.println("CAN Read All Traffic selected.");
        Serial.println("Press 'a' to toggle ASCII mode on/off");
        mcp2515.setNormalMode();
        break;
      case 2:
        currentMode = MODE_WRITE;
        Serial.println("Enter CAN message to send (ID DLC DATA...):");
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
      case 5:
        currentMode = MODE_VALTRACKER;
        setupCanFilters();
        mcp2515.setNormalMode();
        break;
      case 6:
        currentMode = MODE_PID_MANAGER;
        Serial.println("Diagnostics and PID Manager selected.");
        pid = setupPID();
        mcp2515.setNormalMode();
        break;
      default:
        Serial.println("Invalid selection. Select 1, 2, 3, 4, 5, or 6.");
        break;
      }
    }
  }
}

void loop()
{
  switch (currentMode)
  {
  case MODE_READ_ALL:
    if (Serial.available())
    {
      char input = Serial.read();
      if (input == 'a' || input == 'A')
      {
        asciiMode = !asciiMode;
        Serial.print("ASCII mode ");
        Serial.println(asciiMode ? "ON" : "OFF");
      }
      if (input == 's')
      {
        stopExecution = true;
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
    if (Serial.available())
    {
      char input = Serial.read();
      if (input == 'a' || input == 'A')
      {
        asciiMode = !asciiMode;
        Serial.print("ASCII mode ");
        Serial.println(asciiMode ? "ON" : "OFF");
      }
      if (input == 's')
      {
        stopExecution = true;
      }
    }
    canRead();
    break;
  case MODE_VALTRACKER:
    if (Serial.available())
    {
      char input = Serial.read();
      if (input == 's')
      {
        stopExecution = true;
      }
    }
    canValTracker();
    break;
  case MODE_PID_MANAGER:
    if (Serial.available())
    {
      char input = Serial.read();
      if (input == 's')
      {
        stopExecution = true;
      }
    }
    sendPidRequest(pid);
    managePID(pid);
    break;
  default:
    // No default case
    break;
  }
  
  #ifdef ESP32
  esp_task_wdt_reset(); // Regularly reset the watchdog timer
  #endif
}

void resetDevice() {
    #ifdef ESP32
    esp_task_wdt_init(1, true); // Set a short timeout for a quick reset
    while(true); // Wait for the watchdog to trigger a reset
    #else
    wdt_enable(WDTO_15MS); // For AVR
    while(true);
    #endif
}

/**
 * @brief Check if a byte is printable ASCII character
 * @param byte The byte to check
 * @return True if the byte is printable, false otherwise
 */
bool isPrintable(uint8_t byte)
{
  return byte >= 32 && byte <= 126;
}

/**
 * @brief Read and print CAN messages
 */
void canRead()
{
  if (stopExecution == true)
  {
    Serial.println("Stopping execution and resetting...");
    resetDevice();
    while (true)
      ; // Wait for the watchdog timer to reset the Arduino
  }
  if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK)
  {

    Serial.print("ID: ");
    Serial.print(canMsg.can_id, HEX);
    Serial.print(" DLC: ");
    Serial.print(canMsg.can_dlc, HEX);
    Serial.print(" Data: ");

    for (int i = 0; i < canMsg.can_dlc; i++)
    { // Print the data
      if (asciiMode)
      {
        // Print as ASCII characters
        if (isPrintable(canMsg.data[i]))
        {
          Serial.print((char)canMsg.data[i]);
        }
        else
        {
          Serial.print('.');
        }
      }
      else
      {
        Serial.print(canMsg.data[i], HEX);
        Serial.print(" ");
      }
    }
    Serial.println();
  }
}

void canValTracker()
{
  static uint8_t lastData[8] = {0};
  static uint8_t lastDlc = 0;
  static uint32_t lastId = 0;
  static bool firstRun = true;
  static uint8_t minValue = 255;
  static uint8_t maxValue = 0;

  if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK)
  {
    if (firstRun)
    {
      memcpy(lastData, canMsg.data, 8);
      lastDlc = canMsg.can_dlc;
      lastId = canMsg.can_id;
      firstRun = false;
    }
    else
    {
      if (lastDlc != canMsg.can_dlc || lastId != canMsg.can_id)
      {
        Serial.println("DLC or ID changed, resetting...");
        firstRun = true;
        return;
      }
      for (int i = 0; i < canMsg.can_dlc; i++)
      {
        if (lastData[i] != canMsg.data[i])
        {
          Serial.print("Byte ");
          Serial.print(i);
          Serial.print(" changed from 0x");
          Serial.print(lastData[i], HEX);
          Serial.print(" (");
          Serial.print(lastData[i], DEC);
          Serial.print(") to 0x");
          Serial.print(canMsg.data[i], HEX);
          Serial.print(" (");
          Serial.print(canMsg.data[i], DEC);
          Serial.println(")");
          lastData[i] = canMsg.data[i];

          // Update min and max values
          if (canMsg.data[i] < minValue)
          {
            minValue = canMsg.data[i];
          }
          if (canMsg.data[i] > maxValue)
          {
            maxValue = canMsg.data[i];
          }
        }
      }
    }
  }

  // Watchdog interrupt handling for min/max values
  if (Serial.available())
  {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input == "min")
    {
      Serial.print("Minimum value: ");
      Serial.println(minValue);
    }
    else if (input == "max")
    {
      Serial.print("Maximum value: ");
      Serial.println(maxValue);
    }
    else if (input == "s")
    {
      Serial.println("Stopping execution and resetting...");
      resetDevice();
    }
  }
}

void setupCanFilters()
{
  char inputBuffer[10]; // Buffer for the input

  // Clear the Serial buffer
  while (Serial.available() > 0)
  {
    Serial.read();
  }

  // Read mask in hex
  Serial.println("Enter Mask in HEX (e.g., 7FF for all bits):");
  while (!Serial.available())
    ; // Wait for user input
  Serial.readBytesUntil('\n', inputBuffer, sizeof(inputBuffer) - 1);
  inputBuffer[sizeof(inputBuffer) - 1] = 0; // Ensure null-termination
  uint32_t mask = strtol(inputBuffer, NULL, 16);

  while (Serial.available() > 0)
  {
    Serial.read();
  }

  // Read filter in hex
  Serial.println("Enter Filter in HEX (e.g., 201 for specific ID):");
  while (!Serial.available())
    ;
  Serial.readBytesUntil('\n', inputBuffer, sizeof(inputBuffer) - 1);
  inputBuffer[sizeof(inputBuffer) - 1] = 0;
  uint32_t filter = strtol(inputBuffer, NULL, 16);

  if (mask == 0)
  {
    mask = 2047;
  }
  Serial.print("Setting Mask: 0x");
  Serial.println(mask, HEX);
  Serial.print("Setting Filter: 0x");
  Serial.println(filter, HEX);

  mcp2515.setFilterMask(MCP2515::MASK0, false, mask); // You must setup all masks/filters
  mcp2515.setFilterMask(MCP2515::MASK1, false, mask); // Even when you only need 1

  mcp2515.setFilter(MCP2515::RXF0, false, filter);
  mcp2515.setFilter(MCP2515::RXF1, false, filter);
  mcp2515.setFilter(MCP2515::RXF2, false, filter);
  mcp2515.setFilter(MCP2515::RXF3, false, filter);
  mcp2515.setFilter(MCP2515::RXF4, false, filter);
  mcp2515.setFilter(MCP2515::RXF5, false, filter);
}

/**
 * @brief Prepare a custom CAN message to send
 */
void prepareCanWrite()
{
  char inputBuffer[20]; // Buffer size

  // Function to clear the Serial buffer
  auto clearSerialBuffer = []()
  {
    while (Serial.available() > 0)
    {
      Serial.read();
    }
  };

  // Read CAN ID in hex
  Serial.println("Enter CAN ID in HEX:");
  clearSerialBuffer();
  while (!Serial.available())
    ;                                          // Wait for user input
  memset(inputBuffer, 0, sizeof(inputBuffer)); // Clear input buffer
  Serial.readBytesUntil('\n', inputBuffer, sizeof(inputBuffer) - 1);
  canMsg.can_id = strtol(inputBuffer, NULL, 16);
  Serial.print("CAN ID: 0x");
  Serial.println(canMsg.can_id, HEX);

  // Read CAN DLC in decimal
  Serial.println("Enter CAN DLC:");
  clearSerialBuffer();
  while (!Serial.available())
    ;                                          // Wait for user input
  memset(inputBuffer, 0, sizeof(inputBuffer)); // Clear input buffer
  Serial.readBytesUntil('\n', inputBuffer, sizeof(inputBuffer) - 1);
  canMsg.can_dlc = strtol(inputBuffer, NULL, 10);
  Serial.print("DLC: ");
  Serial.println(canMsg.can_dlc);

  // Read CAN DATA in hex
  for (int i = 0; i < canMsg.can_dlc; i++)
  {
    Serial.print("Enter DATA byte ");
    Serial.print(i);
    Serial.println(" in HEX:");
    clearSerialBuffer();
    while (!Serial.available())
      ;                                          // Wait for user input
    memset(inputBuffer, 0, sizeof(inputBuffer)); // Clear input buffer
    Serial.readBytesUntil('\n', inputBuffer, sizeof(inputBuffer) - 1);
    canMsg.data[i] = strtol(inputBuffer, NULL, 16);
    Serial.print("Data byte ");
    Serial.print(i);
    Serial.print(": 0x");
    Serial.println(canMsg.data[i], HEX);
  }

  Serial.println("Custom message prepared.");
}

/**
 * @brief Send CAN messages at a specified frequency
 */
void canWrite()
{
  Serial.println("Enter CAN Write Speed (messages/s):");
  while (Serial.available() > 0)
  {
    Serial.read();
  }
  while (!Serial.available())
    ; // Wait for user input

  messageFrequency = Serial.parseInt();
  Serial.print("Message frequency set to: ");
  Serial.print(messageFrequency);
  Serial.println(" messages/s");

  int messageDelay = 1000 / messageFrequency;
  while (true)
  {
    mcp2515.sendMessage(&canMsg);
    Serial.println("Message sent");
    delay(messageDelay);
    if (Serial.available() > 0)
    {
      String command = Serial.readStringUntil('\n');
      if (command == "s")
      {
        Serial.println("Stopping and resetting...");

        // Enable the watchdog for a reset
        resetDevice();
      }
    }
  }
}

/**
 * @brief Perform CAN speed test
 */
void canSpeedtest()
{
  static int cntr = 0;
  static unsigned long oldTime = 0;

  if (Serial.available() > 0)
  {
    String command = Serial.readStringUntil('\n');
    if (command == "s")
    {
      Serial.println("Stopping and resetting...");

      // Enable the watchdog for a reset
      resetDevice();
    }
  }
  if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK)
  {
    cntr++;
  }

  if ((millis() - oldTime) > 1000)
  {
    oldTime = millis();
    Serial.print(cntr);
    Serial.println(" msg/sec");
    cntr = 0;
  }
}

uint8_t setupPID()
{
  char inputBuffer[10]; // Buffer for the input

  // Clear the Serial buffer
  while (Serial.available() > 0)
  {
    Serial.read();
  }

  Serial.println("Coolant Temp: 0x05");
  Serial.println("Engine RPM: 0x0C");
  Serial.println("Vehicle Speed: 0x0D");
  Serial.println("Throttle Position: 0x11");
  Serial.println("Engine Load: 0x04");
  Serial.println("Fuel Level: 0x2F");
  Serial.println("Intake Temp: 0x0F");
  Serial.println("Barometric Pressure: 0x33");
  Serial.println("");
  Serial.println("Use code 0x00 to view all PIDs");
  Serial.println("");

  // Read PID in hex
  Serial.println("Enter PID in HEX (e.g., 0x0C for engine RPM):");
  while (!Serial.available())
    ; // Wait for user input
  Serial.readBytesUntil('\n', inputBuffer, sizeof(inputBuffer) - 1);
  inputBuffer[sizeof(inputBuffer) - 1] = 0; // Ensure null-termination

  // Remove leading "0x" if present
  if (strncmp(inputBuffer, "0x", 2) == 0)
  {
    memmove(inputBuffer, inputBuffer + 2, strlen(inputBuffer) - 1);
  }

  pid = strtoul(inputBuffer, NULL, 16); // Parse as hexadecimal
  Serial.print("Setting PID: 0x");
  Serial.println(pid, HEX);
  sendPidRequest(pid);
  return pid;
}

// Function to send a PID request over CAN bus
void sendPidRequest(uint8_t pid)
{
  can_frame pidRequestFrame;

  pidRequestFrame.can_id = 0x7DF; // Standard OBD-II request ID
  pidRequestFrame.can_dlc = 8;

  // Constructing the 8 data bytes
  pidRequestFrame.data[0] = 0x02; // Number of additional data bytes
  pidRequestFrame.data[1] = 0x01; // Service 01 - show current data, new modes soon
  pidRequestFrame.data[2] = pid;  // PID code
  // Padding the unused bytes with 0xCC
  for (int i = 3; i < 8; i++)
  {
    pidRequestFrame.data[i] = 0xCC;
  }

  // Send the PID request frame
  delay(3); // Delay to prevent CAN bus errors (needs fine tuning (1-5ms works > 8 causes errors))
  if (mcp2515.sendMessage(&pidRequestFrame) != MCP2515::ERROR_OK)
  {
    Serial.println("Error sending PID request");
  }
}

void managePID(uint8_t pid)
{
  if (stopExecution == true)
  {
    Serial.println("Stopping execution and resetting...");
    resetDevice();
  }
  if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK)
  {
    if (canMsg.can_id >= 0x7E8 && canMsg.can_id <= 0x7EF)
    {                            // Standard OBD-II response ID filtering
      if (canMsg.data[2] == pid) // Had issues using switch/case so temporarily using if/else
      {
        if (pid == PID_COOLANT_TEMP)
        {
          int temp = canMsg.data[3] - 40;
          Serial.print("Coolant Temp: ");
          Serial.print(temp);
          Serial.println(" C");
        }
        else if (pid == PID_ENGINE_RPM)
        {
          int rpm = ((canMsg.data[3] * 256) + canMsg.data[4]) / 4;
          Serial.print("Engine RPM: ");
          Serial.print(rpm);
          Serial.println(" RPM");
        }
        else if (pid == PID_VEHICLE_SPEED)
        {
          int speed = canMsg.data[3];
          Serial.print("Vehicle Speed: ");
          Serial.print(speed);
          Serial.println(" km/h");
        }
        else if (pid == PID_THROTTLE_POSITION)
        {
          int throttle = canMsg.data[3] * 100 / 255;
          Serial.print("Throttle Position: ");
          Serial.print(throttle);
          Serial.println("%");
        }
        else if (pid == PID_ENGINE_LOAD)
        {
          int load = canMsg.data[3] * 100 / 255;
          Serial.print("Engine Load: ");
          Serial.print(load);
          Serial.println("%");
        }
        else if (pid == PID_FUEL_LEVEL)
        {
          int fuel = canMsg.data[3] * 100 / 255;
          Serial.print("Fuel Level: ");
          Serial.print(fuel);
          Serial.println("%");
        }
        else if (pid == PID_INTAKE_TEMP)
        {
          int intakeTemp = canMsg.data[3] - 40;
          Serial.print("Intake Air Temp: ");
          Serial.print(intakeTemp);
          Serial.println(" C");
        }
        else if (pid == PID_BAROMETRIC_PRESSURE)
        {
          int pressure = canMsg.data[3];
          Serial.print("Barometric Pressure: ");
          Serial.print(pressure);
          Serial.println(" kPa");
        }
        else if (pid == PID_ODOMETER) {
          unsigned long odometerKm = ((unsigned long)(canMsg.data[3] == 0xCC ? 0 : canMsg.data[3]) << 24) |
                                    ((unsigned long)(canMsg.data[4] == 0xCC ? 0 : canMsg.data[4]) << 16) |
                                    ((unsigned long)(canMsg.data[5] == 0xCC ? 0 : canMsg.data[5]) << 8)  |
                                    (unsigned long)(canMsg.data[6] == 0xCC ? 0 : canMsg.data[6]);
          odometerKm /= 10; // Divide by 10 as per the formula

          // Convert kilometers to miles
          float odometerMiles = odometerKm * 0.621371;

          Serial.print("Odometer: ");
          Serial.print(odometerKm);
          Serial.print(" km | ");
          Serial.print(odometerMiles, 2); // Print miles with two decimal places
          Serial.println(" miles");
        }
        else
        {
          Serial.print("PID not parsed, data dump: ");
          for (int i = 3; i < canMsg.can_dlc; i++)
          { // Print the data
            Serial.print(canMsg.data[i], HEX);
            Serial.println(" ");
          }
        }
      }
    }
  }
}
