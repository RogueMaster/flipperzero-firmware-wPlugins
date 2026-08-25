#pragma once

#include <furi.h>

#include "../api_caller.h"

/** Initialize the FlipperHTTP UART link (opens Usart, starts RX thread). */
bool wifi_manager_init(AppContext* app);

/** Release the FlipperHTTP context and the UART link. */
void wifi_manager_deinit(AppContext* app);

/** Send [PING] and wait for [PONG]. */
bool wifi_manager_ping(AppContext* app);

/** Send [VERSION] and store the firmware version string. */
bool wifi_manager_get_version(AppContext* app, FuriString* version);

/** Send [WIFI/SCAN] and prepare for an asynchronous result poll. */
void wifi_manager_scan_start(AppContext* app);

/**
 * Check whether the scan reply arrived. When done (reply received, error or
 * timeout), the newline-separated SSID list is stored and true is returned.
 */
bool wifi_manager_scan_poll(AppContext* app, FuriString* ssid_list);

/** Send [WIFI/SSID] and store the current SSID (empty if disconnected). */
bool wifi_manager_get_ssid(AppContext* app, char* ssid, size_t ssid_size);

/** Send [IP/ADDRESS] and store the board local IP (empty on failure). */
bool wifi_manager_get_ip(AppContext* app, char* ip, size_t ip_size);

/** Send [WIFI/STATUS] and report whether the board is connected. */
bool wifi_manager_is_connected(AppContext* app);

/**
 * Send [WIFI/SAVE]{"ssid":..,"password":..}; the board connects immediately.
 * Stores the board reply (e.g. [SUCCESS]...) into reply if not NULL.
 */
bool wifi_manager_save_and_connect(
    AppContext* app,
    const char* ssid,
    const char* password,
    FuriString* reply);

/** Send [WIFI/DISCONNECT] and wait for the confirmation. */
bool wifi_manager_disconnect(AppContext* app);
