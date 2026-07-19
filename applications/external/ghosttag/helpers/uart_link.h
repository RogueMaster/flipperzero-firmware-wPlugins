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
 *     GTHELLO,<fw_version>                       sent on ESP32 boot
 *   Flipper -> ESP32:
 *     START   begin scanning
 *     STOP    stop scanning
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
