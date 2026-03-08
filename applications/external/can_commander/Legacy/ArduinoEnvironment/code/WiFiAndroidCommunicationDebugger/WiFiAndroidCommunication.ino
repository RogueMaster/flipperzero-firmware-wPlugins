// WiFi WebSocket debugger by Matthew KuKanich
#include <WiFi.h>
#include <WebSocketsServer.h>
// Simple server for testing websockets
// Use this to debug communications before running the full program
const char* ssid = "yourSSID";
const char* password = "yourPASSWORD";
const int ledPin = 2;

WebSocketsServer webSocket = WebSocketsServer(81);

void setup() {
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop() {
  webSocket.loop();
  webSocket.broadcastTXT("CAN data from rp2040");
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_TEXT:
      if (strcmp((char *)payload, "turnOnLED") == 0) {
        digitalWrite(ledPin, HIGH);
      } else if (strcmp((char *)payload, "turnOffLED") == 0) {
        digitalWrite(ledPin, LOW);
      }
      break;
  }
}
