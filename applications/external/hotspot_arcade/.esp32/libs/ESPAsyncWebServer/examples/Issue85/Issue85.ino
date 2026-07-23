/**
 *
 * Connect to AP and run in bash:
 *
 * > while true; do echo "PING"; sleep 0.1; done | websocat ws://192.168.4.1/ws
 *
 */
#include <Arduino.h>
#ifdef ESP8266
  #include <ESP8266WiFi.h>
#endif
#ifdef ESP32
  #include <WiFi.h>
#endif
#include <ESPAsyncWebServer.h>

#include <list>

size_t msgCount = 0;
size_t window = 100;
std::list<uint32_t> times;

void connect_wifi() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP("esp-captive");

  // Serial.print("Connecting");
  // while (WiFi.status() != WL_CONNECTED) {
  //   delay(500);
  //   Serial.print(".");
  // }
  // Serial.println();

  // Serial.print("Connected, IP address: ");
  // Serial.println(WiFi.localIP());
}

void notFound(AsyncWebServerRequest* request) {
  request->send(404, "text/plain", "Not found");
}

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// initial stack
char* stack_start;

void printStackSize() {
  char stack;
  Serial.print(F("stack size "));
  Serial.print(stack_start - &stack);
  Serial.print(F(" | Heap free:"));
  Serial.print(ESP.getFreeHeap());
#ifdef ESP8266
  Serial.print(F(" frag:"));
  Serial.print(ESP.getHeapFragmentation());
  Serial.print(F(" maxFreeBlock:"));
  Serial.print(ESP.getMaxFreeBlockSize());
#endif
  Serial.println();
}

void onWsEventEmpty(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
  msgCount++;
  Serial.printf("count: %d\n", msgCount);

  times.push_back(millis());
  while (times.size() > window)
    times.pop_front();
  if (times.size() == window)
    Serial.printf("%f req/s\n", 1000.0 * window / (times.back() - times.front()));

  printStackSize();

  client->text("PONG");
}

void serve_upload(AsyncWebServerRequest* request, const String& filename, size_t index, uint8_t* data, size_t len, bool final) {
  Serial.print("> onUpload ");
  Serial.print("index: ");
  Serial.print(index);
  Serial.print(" len:");
  Serial.print(len);
  Serial.print(" final:");
  Serial.print(final);
  Serial.println();
}

void setup() {
  char stack;
  stack_start = &stack;

  Serial.begin(115200);
  Serial.println("\n\n\nStart");
  Serial.printf("stack_start: %p\n", stack_start);

  connect_wifi();

  server.onNotFound(notFound);

  ws.onEvent(onWsEventEmpty);
  server.addHandler(&ws);

  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest* request) { request->send(200); }, serve_upload);

  server.begin();
  Serial.println("Server started");
}

String msg = "";
uint32_t count = 0;
void loop() {
  // ws.cleanupClients();
  // count += 1;
  // // Concatenate some string, and clear it after some time
  // static unsigned long millis_string = millis();
  // if (millis() - millis_string > 1) {
  //   millis_string += 100;
  //   if (count % 100 == 0) {
  //     // printStackSize();
  //     // Serial.print(msg);
  //     msg = String();
  //   } else {
  //     msg += 'T';
  //   }
  // }
}