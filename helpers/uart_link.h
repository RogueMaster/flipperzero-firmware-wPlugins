#pragma once

#include <furi.h>
#include "ble_signatures.h"

/**
 * Serial link to the GhostTag ESP32 companion (WiFi/BLE devboard) on the
 * Flipper USART (pins 13/14) @ 115200. A worker thread parses the line protocol
 * and dispatches detections back to the app.
 *
 * Wire protocol (newline-terminated ASCII):
 *   ESP32 -> Flipper:
 *     GT1,<mac12hex>,<rssi>,<typecode>,<name>   one BLE tracker detection
 *     GTHELLO,<fw_version>                      on boot, and in reply to PING
 *     GTALIVE,<seen_count>                      heartbeat, ~every 2 s
 *   Flipper -> ESP32:
 *     START   begin scanning
 *     STOP    stop scanning
 *     PING    answer with GTHELLO
 *
 * The heartbeat is not decoration. Liveness used to be inferred from
 * detections alone, so a board sitting in a quiet room with no trackers around
 * it went silent and the app reported NO BOARD - telling the user their
 * hardware was broken at exactly the moment it was working and finding nothing.
 */
typedef struct UartLink UartLink;

typedef void (*UartLinkRxCallback)(
    void* context,
    const uint8_t mac[6],
    TrackerType type,
    int8_t rssi,
    const char* name);

typedef void (*UartLinkStatusCallback)(void* context, bool connected, const char* version);

UartLink* uart_link_alloc(void);
void uart_link_free(UartLink* link);

void uart_link_set_callbacks(
    UartLink* link,
    UartLinkRxCallback rx_cb,
    UartLinkStatusCallback status_cb,
    void* context);

void uart_link_start(UartLink* link);
void uart_link_stop(UartLink* link);
bool uart_link_is_running(UartLink* link);

void uart_link_send_command(UartLink* link, const char* cmd);

/**
 * Tick of the last COMPLETE line received from the board, of any kind.
 *
 * Counting any line rather than only detections is what makes the link
 * indicator tell the truth in a quiet room.
 */
uint32_t uart_link_last_rx_tick(UartLink* link);

/** True once the board has identified itself this session. */
bool uart_link_has_greeted(UartLink* link);
