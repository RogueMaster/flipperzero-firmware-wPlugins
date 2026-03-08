// CAN Commander Data Reversal Tool
// Matthew KuKanich

#include <esp32_can.h>

#define GREEN_LED_PIN 27
#define BLUE_LED_PIN 26

const unsigned long calibrationTime = 10000; // Calibration phase duration in milliseconds
const unsigned long monitoringTime = 20000; // Monitoring phase duration in milliseconds
unsigned long startTime;

// Define the maximum number of CAN IDs we expect to handle
const int MAX_CAN_IDS = 80;
const int MAX_CAN_DLC = 8;

struct CANData {
  uint32_t id;
  uint8_t data[MAX_CAN_DLC];
  bool disqualifiedBytes[MAX_CAN_DLC];
  bool disqualified;
};

CANData canDataArray[MAX_CAN_IDS];
int canDataCount = 0;

bool calibrationComplete = false;
bool monitoringComplete = false;
bool readMode = false;

uint32_t filterId = 0;
bool filterBytes[MAX_CAN_DLC];
bool readAllBytes = true;

void resetCalibration() {
  canDataCount = 0;
  calibrationComplete = false;
  monitoringComplete = false;
  readMode = false;
  startTime = millis();
  Serial.println("--------- CAN Calibration Start ----------");
  digitalWrite(BLUE_LED_PIN, HIGH);
}

void resetMonitoring() {
  calibrationComplete = true;
  monitoringComplete = false;
  readMode = false;
  startTime = millis();
  Serial.println("--------- Monitoring Phase Start ----------");
  digitalWrite(GREEN_LED_PIN, HIGH);
}

void startReadMode() {
  Serial.println("Enter the CAN ID to filter (in hex):");
  while (Serial.available() == 0) {
    delay(100); // Wait for user input
  }
  filterId = strtol(Serial.readString().c_str(), NULL, 16);

  Serial.println("Enter the bytes to read (0-7) separated by spaces, or 'all' to read all bytes:");
  while (Serial.available() == 0) {
    delay(100); // Wait for user input
  }
  String input = Serial.readString();
  if (input == "all") {
    readAllBytes = true;
  } else {
    readAllBytes = false;
    memset(filterBytes, 0, MAX_CAN_DLC);
    int byteIndex = 0;
    char* token = strtok((char*)input.c_str(), " ");
    while (token != NULL && byteIndex < MAX_CAN_DLC) {
      int byte = atoi(token);
      if (byte >= 0 && byte < MAX_CAN_DLC) {
        filterBytes[byte] = true;
      }
      token = strtok(NULL, " ");
      byteIndex++;
    }
  }
  readMode = true;
  Serial.print("------- Reading CAN ID: ");
  Serial.print(filterId, HEX);
  if (readAllBytes) {
    Serial.println(" (All Bytes) -------");
  } else {
    Serial.print(" (Bytes: ");
    for (int i = 0; i < MAX_CAN_DLC; i++) {
      if (filterBytes[i]) {
        Serial.print(i);
        Serial.print(" ");
      }
    }
    Serial.println(") -------");
  }
}

void setup() {
  Serial.begin(115200);

  Serial.println("-----------------------------");
  Serial.println("    CAN Reverse Tool     ");
  Serial.println("-----------------------------");

  pinMode(BLUE_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);

  CAN0.setCANPins(GPIO_NUM_4, GPIO_NUM_5); // First is RX Pin, Second is TX Pin
  CAN0.begin(500000); // Typical HS CAN baud
  CAN0.watchFor();

  Serial.println("Type 'c' to restart calibration, 'm' to restart monitoring phase, or 'r' to enter read mode");

  resetCalibration();
}

void loop() {
  if (Serial.available() > 0) {
    char input = Serial.read();
    if (input == 'c') {
      resetCalibration();
    } else if (input == 'm' && calibrationComplete) {
      resetMonitoring();
    } else if (input == 'r') {
      startReadMode();
    }
  }

  CAN_FRAME can_message;

  if (CAN0.read(can_message)) {
    uint32_t canId = can_message.id;
    int index = -1;

    for (int i = 0; i < canDataCount; i++) {
      if (canDataArray[i].id == canId) {
        index = i;
        break;
      }
    }

    if (index == -1 && canDataCount < MAX_CAN_IDS) {
      index = canDataCount++;
      canDataArray[index].id = canId;
    }

    if (index != -1 && !canDataArray[index].disqualified) {
      for (int i = 0; i < can_message.length; i++) {
        if (!canDataArray[index].disqualifiedBytes[i]) {
          if (canDataArray[index].data[i] != 0 && canDataArray[index].data[i] != can_message.data.byte[i]) {
            canDataArray[index].disqualifiedBytes[i] = true;
          } else {
            canDataArray[index].data[i] = can_message.data.byte[i];
          }
        }
      }

      bool allBytesDisqualified = true;
      for (int i = 0; i < can_message.length; i++) {
        if (!canDataArray[index].disqualifiedBytes[i]) {
          allBytesDisqualified = false;
          break;
        }
      }

      if (allBytesDisqualified) {
        canDataArray[index].disqualified = true;
      }
    }
  }

  if (!calibrationComplete && millis() - startTime > calibrationTime) {
    calibrationComplete = true;
    startTime = millis(); // Reset start time for the monitoring phase

    Serial.println("--------- Calibration Complete ----------");
    digitalWrite(BLUE_LED_PIN, LOW);
    for (int i = 0; i < canDataCount; i++) {
      if (!canDataArray[i].disqualified) {
        Serial.print(canDataArray[i].id, HEX);
        Serial.print(" ");
        for (int j = 0; j < MAX_CAN_DLC; j++) {
          if (!canDataArray[i].disqualifiedBytes[j]) {
            Serial.print(canDataArray[i].data[j], HEX);
            Serial.print(" ");
          } else {
            Serial.print("XX ");
          }
        }
        Serial.println();
      }
    }
    Serial.println("--------- Monitoring Phase Start ----------");
    digitalWrite(GREEN_LED_PIN, HIGH);
  }

  if (calibrationComplete && !monitoringComplete && millis() - startTime > monitoringTime) {
    monitoringComplete = true;
    Serial.println("--------- Monitoring Phase Complete ----------");
    digitalWrite(GREEN_LED_PIN, LOW);
  }

  if (calibrationComplete && !monitoringComplete) {
    // Monitoring phase: report changes compared to the calibrated results
    if (CAN0.read(can_message)) {
      uint32_t canId = can_message.id;
      int index = -1;

      for (int i = 0; i < canDataCount; i++) {
        if (canDataArray[i].id == canId) {
          index = i;
          break;
        }
      }

      if (index != -1 && !canDataArray[index].disqualified) {
        for (int i = 0; i < can_message.length; i++) {
          if (!canDataArray[index].disqualifiedBytes[i]) {
            if (canDataArray[index].data[i] != can_message.data.byte[i]) {
              Serial.print("Change detected for ID: ");
              Serial.print(canId, HEX);
              Serial.print(" Byte: ");
              Serial.print(i);
              Serial.print(" Old Value: ");
              Serial.print(canDataArray[index].data[i], HEX);
              Serial.print(" New Value: ");
              Serial.println(can_message.data.byte[i], HEX);
              // Update the stored value
              canDataArray[index].data[i] = can_message.data.byte[i];
            }
          }
        }
      }
    }
  }

  if (readMode) {
    // Read mode: filter and display specified CAN ID and bytes
    if (CAN0.read(can_message)) {
      if (can_message.id == filterId) {
        Serial.print("ID: ");
        Serial.print(can_message.id, HEX);
        Serial.print(" Data: ");
        for (int i = 0; i < can_message.length; i++) {
          if (readAllBytes || filterBytes[i]) {
            Serial.print(can_message.data.byte[i], HEX);
            Serial.print(" ");
          }
        }
        Serial.println();
      }
    }
  }
}
