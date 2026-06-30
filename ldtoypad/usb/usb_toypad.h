#pragma once
#include <furi.h>
#include <furi_hal.h>
#include <pthread.h>

#include "frame.h"
#include "ToyPadEmu.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HID_EP_SZ  0x20 // 32 bytes packet size
#define HID_EP_IN  0x81
#define HID_EP_OUT 0x01

enum ConnectedStatus {
    ConnectedStatusDisconnected = 0,
    ConnectedStatusConnected = 1,
    ConnectedStatusReconnecting = 2,
    ConnectedStatusCleanupWanted = 3
};

extern FuriHalUsbInterface usb_hid_ldtoypad;

usbd_device* get_usb_device();

uint8_t get_connected_status();
void set_connected_status(enum ConnectedStatus status);

#ifdef __cplusplus
}
#endif
