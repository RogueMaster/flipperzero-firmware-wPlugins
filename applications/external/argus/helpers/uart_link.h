#pragma once

#include <furi.h>
#include "argus_db.h"

/**
 * Serial link to the Argus ESP32 companion (WiFi devboard) on the Flipper
 * USART (GPIO pins 13 TX / 14 RX) @ 115200 8N1. A worker thread parses the
 * line protocol and dispatches detections back to the app.
 *
 * Wire protocol (newline-terminated ASCII; SSIDs are sanitised to drop commas):
 *   ESP32 -> Flipper:
 *     AXHELLO,<fw_version>                                    on boot
 *     AXD,<src12hex>,<bssid12hex>,<ch>,<rssi>,<reason>,<kind> deauth(0)/disassoc(1)
 *     AXAP,<bssid12hex>,<ch>,<rssi>,<enc>,<ssid>              an AP beacon/probe-resp
 *   Flipper -> ESP32:
 *     START\n              begin sniffing
 *     STOP\n               stop sniffing
 *     CHAN:<0-13>\n        0 = hop all channels, 1..13 = lock to that channel
 *     GUARD:<ssid>\n       set the protected SSID (informational for the board)
 *     PING\n               ask the board to re-announce AXHELLO
 */
typedef struct UartLink UartLink;

typedef void (*UartLinkDeauthCallback)(
    void* context,
    ArgusThreatKind kind,
    const uint8_t src[6],
    const uint8_t bssid[6],
    uint8_t channel,
    int8_t rssi,
    uint8_t reason);

typedef void (*UartLinkApCallback)(
    void* context,
    const uint8_t bssid[6],
    uint8_t channel,
    int8_t rssi,
    ArgusEnc enc,
    const char* ssid);

typedef void (*UartLinkStatusCallback)(void* context, bool connected, const char* version);

UartLink* uart_link_alloc(void);
void uart_link_free(UartLink* link);

void uart_link_set_callbacks(
    UartLink* link,
    UartLinkDeauthCallback deauth_cb,
    UartLinkApCallback ap_cb,
    UartLinkStatusCallback status_cb,
    void* context);

void uart_link_start(UartLink* link);
void uart_link_stop(UartLink* link);
bool uart_link_is_running(UartLink* link);

void uart_link_send_command(UartLink* link, const char* cmd);
