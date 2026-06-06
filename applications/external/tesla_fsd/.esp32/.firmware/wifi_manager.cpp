#include "wifi_manager.h"
#include <WiFi.h>
#include <Arduino.h>

static void wifi_print_urls(const String &ip) {
    Serial.printf("[WiFi] Dashboard: http://%s\n", ip.c_str());
    Serial.printf("[WiFi] HTTP CAN stream: http://%s:82/stream\n", ip.c_str());
}

bool wifi_ap_init(const FSDState *state) {
    WiFi.mode(WIFI_AP);
    // softAP(ssid, password, channel, hidden, max_connection)
    bool ok = WiFi.softAP(state->wifi_ssid, state->wifi_pass, 1, state->wifi_hidden);
    if (ok) {
        String ip = WiFi.softAPIP().toString();
        Serial.printf("[WiFi] AP: \"%s\"%s IP: %s\n",
            state->wifi_ssid,
            state->wifi_hidden ? " (HIDDEN)" : "",
            ip.c_str());
        wifi_print_urls(ip);
    } else {
        Serial.println("[WiFi] AP start FAILED — continuing without web");
    }
    return ok;
}

static bool wifi_sta_init(const FSDState *state) {
    if (state->wifi_sta_ssid[0] == '\0') {
        Serial.println("[WiFi] STA SSID not configured — starting AP");
        return false;
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(state->wifi_sta_ssid,
               state->wifi_sta_pass[0] == '\0' ? nullptr : state->wifi_sta_pass);
    Serial.printf("[WiFi] Connecting to \"%s\"", state->wifi_sta_ssid);

    uint32_t start_ms = millis();
    while (WiFi.status() != WL_CONNECTED &&
           (uint32_t)(millis() - start_ms) < WIFI_STA_CONNECT_TIMEOUT_MS) {
        delay(250);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        String ip = WiFi.localIP().toString();
        Serial.printf("[WiFi] STA connected: \"%s\"\n", state->wifi_sta_ssid);
        Serial.printf("[WiFi] Device IP address: %s\n", ip.c_str());
        wifi_print_urls(ip);
        return true;
    }

    Serial.printf("[WiFi] STA connect failed status=%d — falling back to AP\n",
                  (int)WiFi.status());
    WiFi.disconnect(true);
    delay(100);
    return false;
}

bool wifi_init(const FSDState *state) {
    if (wifi_sta_init(state)) return true;
    return wifi_ap_init(state);
}

void wifi_print_status() {
    wifi_mode_t mode = WiFi.getMode();

    if ((mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA) &&
        WiFi.status() == WL_CONNECTED) {
        String ip = WiFi.localIP().toString();
        Serial.printf("[WiFi] Mode: STA connected to \"%s\"\n", WiFi.SSID().c_str());
        Serial.printf("[WiFi] Device IP address: %s\n", ip.c_str());
        wifi_print_urls(ip);
        return;
    }

    if (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA) {
        String ip = WiFi.softAPIP().toString();
        Serial.printf("[WiFi] Mode: AP \"%s\"\n", WiFi.softAPSSID().c_str());
        Serial.printf("[WiFi] Device IP address: %s\n", ip.c_str());
        wifi_print_urls(ip);
        return;
    }

    Serial.printf("[WiFi] Not connected. mode=%d status=%d\n",
                  (int)mode, (int)WiFi.status());
}
