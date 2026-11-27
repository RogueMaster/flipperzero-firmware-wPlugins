#pragma once
/* Minimal USB HID descriptor definitions for this self-contained FAP driver.
 * Original HID descriptor structure: Flipper Devices firmware (GPL-3.0).
 * This header deliberately avoids depending on the firmware-private usb_hid.h. */
#include <stdint.h>

/* USB-IF Human Interface Device class code. Some external SDK headers omit it. */
#ifndef USB_CLASS_HID
#define USB_CLASS_HID 0x03U
#endif

#ifndef USB_DTYPE_HID
#define USB_DTYPE_HID 0x21
#endif
#ifndef USB_DTYPE_HID_REPORT
#define USB_DTYPE_HID_REPORT 0x22
#endif
#define USB_HID_GETREPORT      0x01
#define USB_HID_SETIDLE        0x0A
#define USB_HID_SETPROTOCOL    0x0B
#define USB_HID_SUBCLASS_BOOT  0x01
#define USB_HID_PROTO_KEYBOARD 0x01
#define USB_HID_COUNTRY_NONE   0x00

struct combo_usb_hid_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdHID;
    uint8_t bCountryCode;
    uint8_t bNumDescriptors;
    uint8_t bDescriptorType0;
    uint16_t wDescriptorLength0;
} __attribute__((packed));

#define HID_PAGE_DESKTOP             0x01
#define HID_PAGE_LED                 0x08
#define HID_PAGE_BUTTON              0x09
#define HID_PAGE_CONSUMER            0x0C
#define HID_DESKTOP_KEYBOARD         0x06
#define HID_DESKTOP_KEYPAD           0x07
#define HID_DESKTOP_MOUSE            0x02
#define HID_DESKTOP_POINTER          0x01
#define HID_DESKTOP_X                0x30
#define HID_DESKTOP_Y                0x31
#define HID_DESKTOP_WHEEL            0x38
#define HID_CONSUMER_CONTROL         0x01
#define HID_APPLICATION_COLLECTION   0x01
#define HID_PHYSICAL_COLLECTION      0x00
#define HID_IOF_DATA                 0x00
#define HID_IOF_CONSTANT             0x01
#define HID_IOF_VARIABLE             0x02
#define HID_IOF_ARRAY                0x00
#define HID_IOF_ABSOLUTE             0x00
#define HID_IOF_RELATIVE             0x04
#define HID_KEYBOARD_L_CTRL          0xE0
#define HID_KEYBOARD_R_GUI           0xE7
#define HID_USAGE_PAGE(x)            0x05, (x)
#define HID_USAGE(x)                 0x09, (x)
#define HID_USAGE_MINIMUM(x)         0x19, (x)
#define HID_USAGE_MAXIMUM(x)         0x29, (x)
#define HID_COLLECTION(x)            0xA1, (x)
#define HID_END_COLLECTION           0xC0
#define HID_REPORT_ID(x)             0x85, (x)
#define HID_LOGICAL_MINIMUM(x)       0x15, ((uint8_t)(x))
#define HID_LOGICAL_MAXIMUM(x)       0x25, ((uint8_t)(x))
#define HID_RI_LOGICAL_MAXIMUM(n, x) 0x26, ((uint8_t)(x)), ((uint8_t)((x) >> 8))
#define HID_RI_USAGE_MAXIMUM(n, x)   0x2A, ((uint8_t)(x)), ((uint8_t)((x) >> 8))
#define HID_REPORT_SIZE(x)           0x75, (x)
#define HID_REPORT_COUNT(x)          0x95, (x)
#define HID_INPUT(x)                 0x81, (x)
#define HID_OUTPUT(x)                0x91, (x)
