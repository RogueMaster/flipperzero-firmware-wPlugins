#pragma once

#include "../flipper/protocol/wfr_protocol.h"
#include <stdbool.h>

/*
 * Network management: connect to WiFi, get current connection, rollback.
 * Supports NetworkManager (nmcli) and fallback to /etc/network/interfaces.
 */

/* Detect which network management system is active.
 * Returns true if NetworkManager is running, false otherwise. */
bool wfr_net_has_networkmanager(void);

/* Get the SSID of the currently active WiFi connection.
 * Writes to out (null-terminated), up to out_size bytes.
 * Returns true if a WiFi connection is active, false otherwise. */
bool wfr_net_get_current(char* out, size_t out_size);

/* Connect to a WiFi network using the provided credentials.
 * Returns true on success, false on failure. */
bool wfr_net_connect(const WfrWifiCreds* creds);

/* Get the current IP address of the WiFi interface.
 * Writes to out (null-terminated), up to out_size bytes.
 * Returns true if an IP address was found, false otherwise. */
bool wfr_net_get_ip(char* out, size_t out_size);

/* Reconnect to a previously saved SSID (for rollback).
 * Returns true on success, false on failure. */
bool wfr_net_rollback(const char* ssid);
