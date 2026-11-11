#pragma once

#include "usb.h"

#define USB_EP0_SIZE 8

/* String descriptors */
enum UsbDevDescStr {
    UsbDevLang = 0,
    UsbDevManuf = 1,
    UsbDevProduct = 2,
    UsbDevSerial = 3,
};

/* Shared manufacturer string descriptor ("Flipper Devices Inc."), defined in furi_hal_usb_cdc.c */
extern const struct usb_string_descriptor furi_hal_usb_manuf_desc;
