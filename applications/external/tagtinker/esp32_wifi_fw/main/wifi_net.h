/*
 * WiFi station + NVS-backed credential storage.
 *
 * Credentials live under NVS namespace "tt_wifi", keys "ssid" / "pwd".
 * wifi_net_init() reads them and tries to connect; if missing, the radio
 * stays parked until wifi_net_set_creds() is called by the Flipper.
 */
#ifndef TT_WIFI_NET_H
#define TT_WIFI_NET_H

#include <stdbool.h>
#include <stdint.h>

#include "tt_wifi_proto.h"

void wifi_net_init(void);

/* Returns the current state. The lower 4 bits map to TT_WIFI_*. */
uint8_t wifi_net_state(void);
int8_t wifi_net_rssi(void);
const char* wifi_net_ssid(void);
const char* wifi_net_ip(void);

/* Persists creds, drops the current AP, reconnects. */
void wifi_net_set_creds(const char* ssid, const char* pwd);
void wifi_net_forget(void);

/* Block (with timeout) until connected. Returns true on success. */
bool wifi_net_wait_connected(uint32_t timeout_ms);

#endif /* TT_WIFI_NET_H */
