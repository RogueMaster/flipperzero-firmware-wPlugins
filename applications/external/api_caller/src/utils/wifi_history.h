#pragma once

#include <furi.h>

#include "../api_caller.h"

#define WIFI_HISTORY_FILETYPE "ApiCaller WiFi History"
#define WIFI_HISTORY_VERSION  1

/** Load the saved networks into app->wifi_history (count is updated). */
void wifi_history_load(AppContext* app);

/**
 * Add or update an entry (matched by SSID) and persist the history.
 * Returns false if the list is full and the SSID is new.
 */
bool wifi_history_add(AppContext* app, const char* ssid, const char* password);
