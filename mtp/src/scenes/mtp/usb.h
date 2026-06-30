#pragma once
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb.h>

/* === START furi_hal_usb_i.h === */
// https://github.com/flipperdevices/flipperzero-firmware/blob/03196fa11007c0f1e002cbb0b82102d8492456b5/targets/f7/furi_hal/furi_hal_usb_i.h#L5
#define USB_EP0_SIZE 8

enum UsbDevDescStr {
    UsbDevLang = 0,
    UsbDevManuf = 1,
    UsbDevProduct = 2,
    UsbDevSerial = 3,
};
/* ===   END furi_hal_usb_i.h === */

void usb_mtp_init(usbd_device* dev, FuriHalUsbInterface* intf, void* ctx);
void usb_mtp_deinit(usbd_device* dev);
void usb_mtp_wakeup(usbd_device* dev);
void usb_mtp_suspend(usbd_device* dev);
