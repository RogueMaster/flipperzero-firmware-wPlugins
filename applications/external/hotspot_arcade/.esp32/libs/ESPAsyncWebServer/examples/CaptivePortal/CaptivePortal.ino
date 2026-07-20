#include <DNSServer.h>
#ifdef ESP32
  #include <AsyncTCP.h>
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESPAsyncTCP.h>
#elif defined(TARGET_RP2040)
  #include <WebServer.h>
  #include <WiFi.h>
#endif
#include "ESPAsyncWebServer.h"

DNSServer dnsServer;
AsyncWebServer server(80);

class CaptiveRequestHandler : public AsyncWebHandler {
  public:
    bool canHandle(__unused AsyncWebServerRequest* request) const override {
      return true;
    }

    void handleRequest(AsyncWebServerRequest* request) {
      AsyncResponseStream* response = request->beginResponseStream("text/html");
      response->print("<!DOCTYPE html><html><head><title>Captive Portal</title></head><body>");
      response->print("<p>This is out captive portal front page.</p>");
      response->printf("<p>You were trying to reach: http://%s%s</p>", request->host().c_str(), request->url().c_str());
#ifndef CONFIG_IDF_TARGET_ESP32H2
      response->printf("<p>Try opening <a href='http://%s'>this link</a> instead</p>", WiFi.softAPIP().toString().c_str());
#endif
      response->print("</body></html>");
      request->send(response);
    }
};

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("Configuring access point...");

#ifndef CONFIG_IDF_TARGET_ESP32H2
  if (!WiFi.softAP("esp-captive")) {
    Serial.println("Soft AP creation failed.");
    while (1)
      ;
  }

  dnsServer.start(53, "*", WiFi.softAPIP());
#endif

  server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER); // only when requested from AP
  // more handlers...
  server.begin();
}

void loop() {
  dnsServer.processNextRequest();
}
