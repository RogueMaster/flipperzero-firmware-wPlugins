// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright 2016-2026 Hristo Gochkov, Mathieu Carbou, Emil Muratov, Will Miles

#include "Arduino.h"
#include "AsyncTCP.h"
#include "WiFi.h"

// Run a server at the root of the project with:
// > python3 -m http.server 3333
// Now you can open a browser and test it works by visiting http://192.168.125.122:3333/ or http://192.168.125.122:3333/README.md
#define HOST "192.168.125.122"
#define PORT 3333

// WiFi SSID to connect to
#define WIFI_SSID "*********"
#define WIFI_PASS "*********"

bool client_running = false;

void makeRequest() {
  client_running = true;
  AsyncClient *client = new AsyncClient;
  if (client == nullptr) {
    Serial.println("** could not allocate client");
    client_running = false;
    return;
  }

  client->onError([](void *arg, AsyncClient *client, int8_t error) {
    Serial.printf("** error occurred %s \n", client->errorToString(error));
    client->close();
    delete client;
    client_running = false;
  });

  client->onConnect([](void *arg, AsyncClient *client) {
    Serial.printf("** client has been connected: %" PRIu16 "\n", client->localPort());

    client->onDisconnect([](void *arg, AsyncClient *client) {
      Serial.printf("** client has been disconnected: %" PRIu16 "\n", client->localPort());
      client->close();
      delete client;
      client_running = false;
    });

    client->onData([](void *arg, AsyncClient *client, void *data, size_t len) {
      Serial.printf("** data received by client: %" PRIu16 ": len=%u\n", client->localPort(), len);
    });

    client->write("GET /README.md HTTP/1.1\r\nHost: " HOST "\r\nUser-Agent: ESP\r\nConnection: close\r\n\r\n");
  });

  if (!client->connect(HOST, PORT)) {
    Serial.println("** connection failed");
    client_running = false;
  }
}

void setup() {
  Serial.begin(115200);

  Serial.print("Connecting to ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected to WiFi. IP: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  if (!client_running) {
    makeRequest();
  }
  delay(1000);
  Serial.printf("** free heap: %" PRIu32 "\n", ESP.getFreeHeap());
}
