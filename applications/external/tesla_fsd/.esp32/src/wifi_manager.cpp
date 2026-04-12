#include "wifi_manager.h"
#include <WiFi.h>
#include <Arduino.h>

static const char AP_SSID[] = "Tesla-FSD";
static const char AP_PASS[] = "12345678";

bool wifi_ap_init() {
    WiFi.mode(WIFI_AP);
    bool ok = WiFi.softAP(AP_SSID, AP_PASS);
    if(ok) {
        Serial.printf("[WiFi] AP: \"%s\"  IP: %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
        Serial.println("[WiFi] Dashboard: http://192.168.4.1");
    } else {
        Serial.println("[WiFi] AP start FAILED — continuing without web");
    }
    return ok;
}
