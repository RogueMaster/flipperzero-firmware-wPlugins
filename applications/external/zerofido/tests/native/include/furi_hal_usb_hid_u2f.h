#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "furi_hal.h"

typedef enum {
    HidU2fConnected = 0,
    HidU2fDisconnected = 1,
    HidU2fRequest = 2,
} HidU2fEvent;

typedef void (*HidU2fCallback)(HidU2fEvent event, void *context);

extern const FuriHalUsbInterface usb_hid_u2f;

size_t furi_hal_hid_u2f_get_request(uint8_t *packet);
void furi_hal_hid_u2f_send_response(const uint8_t *packet, size_t packet_len);
void furi_hal_hid_u2f_set_callback(HidU2fCallback callback, void *context);
bool furi_hal_hid_u2f_is_connected(void);
FuriHalUsbInterface *furi_hal_usb_get_config(void);
bool furi_hal_usb_set_config(const FuriHalUsbInterface *interface, void *context);
