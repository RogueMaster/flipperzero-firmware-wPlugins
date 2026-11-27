#pragma once
/* Flipper Zero Bad USB HID identity fallback for SDKs that omit the constants.
 * Values copied from the official Flipper firmware furi_hal_usb_hid.h.
 */
#ifndef HID_VID_DEFAULT
#define HID_VID_DEFAULT 0x046D
#endif
#ifndef HID_PID_DEFAULT
#define HID_PID_DEFAULT 0xC529
#endif

/* Official HID USB manufacturer/product string buffer length. Some uFBT
 * FAP SDK variants expose FuriHalUsbHidConfig but not the internal macro.
 */
#ifndef HID_MANUF_PRODUCT_NAME_LEN
#define HID_MANUF_PRODUCT_NAME_LEN 32U
#endif
