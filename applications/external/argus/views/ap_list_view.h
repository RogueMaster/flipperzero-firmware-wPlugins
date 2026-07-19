#pragma once

#include <gui/view.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* A self-contained AP row so this view does not depend on the DB layer. */
typedef struct {
    char ssid[33];
    uint8_t bssid[6];
    uint8_t channel;
    int8_t rssi;
    uint8_t enc; // ArgusEnc code: 0 Open .. 5 ?
    bool clone; // evil twin of the guarded SSID
} ApRow;

typedef struct ApListView ApListView;

ApListView* ap_list_view_alloc(void);
void ap_list_view_free(ApListView* v);
View* ap_list_view_get_view(ApListView* v);

void ap_list_view_set_title(ApListView* v, const char* title);
void ap_list_view_update(ApListView* v, const ApRow* rows, size_t count, bool esp_connected);
