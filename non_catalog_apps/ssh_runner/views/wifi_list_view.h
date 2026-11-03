/* WiFi network list (custom, not the stock VariableItemList: signal bars, not %): Submenu
 * geometry (header, 16 px rows, rounded highlight, stock scrollbar) with
 * the SSID on the left and, flush right, a lock for secured networks and four signal bars.
 * SPDX-License-Identifier: GPL-3.0-only */
#pragma once

#include <gui/view.h>

typedef struct WifiListView WifiListView;
typedef void (*WifiListCallback)(void* context, uint32_t index);

#define WIFI_LIST_MAX 16

WifiListView* wifi_list_view_alloc(void);
void wifi_list_view_free(WifiListView* wl);
View* wifi_list_view_get_view(WifiListView* wl);

void wifi_list_view_reset(WifiListView* wl, const char* header);
/* `bars` 0..4; the text is copied */
void wifi_list_view_add(WifiListView* wl, const char* ssid, uint8_t bars, bool secured);
void wifi_list_view_set_callback(WifiListView* wl, WifiListCallback cb, void* context);
