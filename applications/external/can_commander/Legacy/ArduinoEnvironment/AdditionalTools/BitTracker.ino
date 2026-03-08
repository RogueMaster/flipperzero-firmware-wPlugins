// CAN Commander Bit Tracker
// Matthew KuKanich

#include <esp32_can.h>

#define CAN_ID_FILTER 0x273

unsigned long lastPrintTime = 0;
const unsigned long printInterval = 0;

CAN_FRAME lastReceivedFrame;
bool isFirstFrame = true;

void setup() {
  Serial.begin(115200);
  Serial.println("Initializing CAN Bus...");

  CAN0.setCANPins(GPIO_NUM_4, GPIO_NUM_5);
  CAN0.begin(500000);
  CAN0.watchFor(CAN_ID_FILTER);
}

void loop() {
  CAN_FRAME can_message;

  // Read CAN messages
  if (CAN0.read(can_message)) {
    if (can_message.id == CAN_ID_FILTER) {
      if (isFirstFrame || !framesEqual(lastReceivedFrame, can_message)) {
        lastReceivedFrame = can_message;
        lastPrintTime = millis();
        printFrameBits(lastReceivedFrame);
        isFirstFrame = false;
      }
    }
  }
}

bool framesEqual(const CAN_FRAME &frame1, const CAN_FRAME &frame2) {
  if (frame1.length != frame2.length) return false;
  for (int i = 0; i < frame1.length; i++) {
    if (frame1.data.byte[i] != frame2.data.byte[i]) return false;
  }
  return true;
}

void printFrameBits(CAN_FRAME &frame) {
  if (frame.length < 1) return;

  Serial.println("New frame received:");
  for (int i = 0; i < frame.length; i++) {
    Serial.print("Byte ");
    Serial.print(i);
    Serial.print(": ");
    printByteInBinary(frame.data.byte[i]);
  }
  Serial.println("End of frame.\n");
}

void printByteInBinary(uint8_t byte) {
  for (int i = 7; i >= 0; i--) {
    Serial.print((byte >> i) & 0x01);  // Print each bit of the byte
  }
  Serial.println();
}
