#include "wifi_manager.h"
#include <WiFi.h>
#include <Arduino.h>

bool wifi_ap_init(const FSDState *state) {
    WiFi.mode(WIFI_AP);
    // softAP(ssid, password, channel, hidden, max_connection)
    bool ok = WiFi.softAP(state->wifi_ssid, state->wifi_pass, 1, state->wifi_hidden);
    if (ok) {
        Serial.printf("[WiFi] AP: \"%s\"%s IP: %s\n",
            state->wifi_ssid,
            state->wifi_hidden ? " (HIDDEN)" : "",
            WiFi.softAPIP().toString().c_str());
        Serial.println("[WiFi] Dashboard: http://192.168.4.1");
    } else {
        Serial.println("[WiFi] AP start FAILED — continuing without web");
    }
    return ok;
}
