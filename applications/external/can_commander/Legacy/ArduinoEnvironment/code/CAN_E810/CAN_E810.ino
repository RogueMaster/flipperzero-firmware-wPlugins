// CAN E810 controller tester by Matthew KuKanich
#include <Arduino.h>

void setup() {
    Serial.begin(115200);
    delay(1000);  // give some time for serial connection to establish
    Serial.println("Testing E810 module...");

    // Enter configuration mode
    Serial.print("+++");
    delay(3100);  // wait for 3.1 seconds to ensure the module enters config mode
    // Send AT command and wait for response
    Serial.println("Sending AT...");
    Serial.print("AT\r");
}

void loop() {
    if (Serial.available()) {
        String response = Serial.readString();
        Serial.print("Received: ");
        Serial.println(response);
        delay(1000);
    }
}
