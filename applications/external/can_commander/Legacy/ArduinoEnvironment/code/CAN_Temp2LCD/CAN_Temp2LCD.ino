#include <SPI.h>
#include <mcp2515.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

Adafruit_BME280 bme;

struct can_frame canMsg;
MCP2515 mcp2515(10);

unsigned long previousMillis = 0; 
unsigned long interval = 100; 
unsigned long startMillis; 
bool timingInProgress = false;
uint8_t count = 0;

void setup() {
  Serial.begin(115200);

  if (!bme.begin(0x76)) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }

  mcp2515.reset();
  mcp2515.setBitrate(CAN_125KBPS, MCP_8MHZ);
  mcp2515.setNormalMode();

  Serial.println("Listening to MS CAN Bus and measuring environment...");
}

void loop() {
  float tempC = bme.readTemperature();
  float tempF = tempC * 9.0 / 5.0 + 32.0;
  float humidity = bme.readHumidity();

  String tempStr = "T: " + String(tempF, 1);  // E.g., "T: 75.5"
  String humStr = "H: " + String(humidity) + "%";  // E.g., "H: 50%"

  // Truncate or pad the strings to ensure they are 7 bytes
  while (tempStr.length() < 7) tempStr += " ";
  while (humStr.length() < 7) humStr += " ";

  unsigned long currentMillis = millis();

  if (!timingInProgress) {
    startMillis = currentMillis;
    timingInProgress = true;
  }

  if (currentMillis - startMillis < 10000) {
    if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK && canMsg.can_id == 0x290) {
      interval = currentMillis - previousMillis;
      count++;
      previousMillis = currentMillis;
    }
  } else {
    if (count > 0) {
      interval = interval / count * 0.95;  // Adjusting timing to be slightly faster
      count = 0;  // Reset count
    }
  }

  if (currentMillis - previousMillis > interval) {
    previousMillis = currentMillis;

    canMsg.can_id = 0x290;
    for (int i = 0; i < tempStr.length(); i++) {
      canMsg.data[i + 1] = tempStr[i];
    }
    mcp2515.sendMessage(&canMsg);

    canMsg.can_id = 0x291;
    for (int i = 0; i < humStr.length(); i++) {
      canMsg.data[i + 1] = humStr[i];
    }
    mcp2515.sendMessage(&canMsg);
  }
}
