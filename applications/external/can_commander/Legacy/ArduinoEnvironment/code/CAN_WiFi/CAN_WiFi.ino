// CAN WiFi Websocket Link by Matthew KuKanich
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <HardwareSerial.h>

const char* ssid = "CAN_Commander";
const char* password = "12345678";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

HardwareSerial ArduinoSerial(1); // Using the second serial port on ESP32

void setup() {
    Serial.begin(115200); // Serial for debugging
    ArduinoSerial.begin(115200, SERIAL_8N1, 18, 17); // Serial communication with Arduino

    Serial.println("Serial Connected");
    WiFi.softAP(ssid, password);
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);
    server.begin();
    Serial.println("Using SSID: CAN_Commander");
}

void loop() {
    if (ArduinoSerial.available()) {
        String dataFromArduino = ArduinoSerial.readStringUntil('\n');
        ws.textAll(dataFromArduino);
        Serial.println(dataFromArduino);
    }
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Serial.println("WebSocket client connected");
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.println("WebSocket client disconnected");
    } else if (type == WS_EVT_DATA) {
        uint8_t *safeData = new uint8_t[len + 1];
        memcpy(safeData, data, len);
        safeData[len] = 0; // Null-terminate the new buffer
        String message = String((char *)safeData);
        delete[] safeData; // Don't forget to free the allocated memory

        Serial.print("Received command to Arduino: ");
        Serial.println(message);

        ArduinoSerial.println(message + "\n"); // Send the command to Arduino

        client->text("Command sent to Arduino: " + message);
    }
}
