#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Wi-Fi transport through a FlipperHTTP board on the GPIO header (Wi-Fi Dev
 * Board, ESP32 or Pico W boards flashed with FlipperHTTP). Non-blocking: call
 * yt_wifi_tick() from the event loop and read yt_wifi_state(). */

typedef enum {
    YtWifiStarting, /* Waiting for [PONG] from the board */
    YtWifiConnecting, /* Joining the saved (or configured) network */
    YtWifiReady,
    YtWifiFetching,
    YtWifiDone, /* A response is ready: yt_wifi_take_result() */
    YtWifiNoBoard, /* No answer on the UART */
    YtWifiFailed, /* Could not join a network; see yt_wifi_error() */
} YtWifiState;

typedef struct YtWifi YtWifi;

/* NULL if the UART is busy. Disables the expansion-module service while open. */
YtWifi* yt_wifi_alloc(void);
void yt_wifi_free(YtWifi* wifi);

/* Starts (or restarts) the handshake. ssid/password may be empty: the board
 * then joins the network it has saved, e.g. from the FlipperHTTP app. */
void yt_wifi_start(YtWifi* wifi, const char* ssid, const char* password);
void yt_wifi_tick(YtWifi* wifi);
YtWifiState yt_wifi_state(YtWifi* wifi);
const char* yt_wifi_error(YtWifi* wifi);

/* Starts an HTTPS GET; only in YtWifiReady. */
bool yt_wifi_get(YtWifi* wifi, const char* url);

/* In YtWifiDone: copies the body (NUL-terminated, truncated to size) and the
 * HTTP status (0 when the board reported an error, which goes to error).
 * Returns false if the body was truncated. The state returns to Ready. */
bool yt_wifi_take_result(
    YtWifi* wifi,
    char* body,
    size_t size,
    uint16_t* http_status,
    char* error,
    size_t error_size);
