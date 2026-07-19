#pragma once

#include <gui/view.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Lean log row, decoupled from the DB layer. */
typedef struct {
    uint8_t kind; // 0 deauth, 1 disassoc, 2 evil twin
    char ssid[33];
    uint8_t channel;
    int8_t rssi;
    uint8_t reason; // 802.11 reason code
    uint32_t age_s; // seconds since the event (computed by the scene)
} LogRow;

typedef struct ThreatLogView ThreatLogView;

ThreatLogView* threat_log_view_alloc(void);
void threat_log_view_free(ThreatLogView* v);
View* threat_log_view_get_view(ThreatLogView* v);

void threat_log_view_update(ThreatLogView* v, const LogRow* rows, size_t count, bool esp_connected);
