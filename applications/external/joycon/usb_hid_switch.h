#pragma once

#include <furi.h>
#include <furi_hal_usb.h>
#include <furi_hal_usb_hid.h>
#include <usb_hid.h>

#ifdef __cplusplus
extern "C" {
#endif

// Nintendo Switch POKKEN CONTROLLER VID/PID
// Using HORI POKKEN CONTROLLER identifiers (proven to work with Switch)
// Based on Arduino JoyCon Library implementation
#define USB_VID_NINTENDO   0x0f0d // HORI (licensed Switch controller manufacturer)
#define USB_PID_SWITCH_PRO 0x0092 // HORI POKKEN CONTROLLER

// Button bit positions for Switch Pro Controller
#define SWITCH_BTN_Y       (1 << 0)
#define SWITCH_BTN_B       (1 << 1)
#define SWITCH_BTN_A       (1 << 2)
#define SWITCH_BTN_X       (1 << 3)
#define SWITCH_BTN_L       (1 << 4)
#define SWITCH_BTN_R       (1 << 5)
#define SWITCH_BTN_ZL      (1 << 6)
#define SWITCH_BTN_ZR      (1 << 7)
#define SWITCH_BTN_MINUS   (1 << 8)
#define SWITCH_BTN_PLUS    (1 << 9)
#define SWITCH_BTN_LSTICK  (1 << 10)
#define SWITCH_BTN_RSTICK  (1 << 11)
#define SWITCH_BTN_HOME    (1 << 12)
#define SWITCH_BTN_CAPTURE (1 << 13)

// D-Pad values (HAT switch) - POKKEN Controller format
#define SWITCH_HAT_UP         0x00
#define SWITCH_HAT_UP_RIGHT   0x01
#define SWITCH_HAT_RIGHT      0x02
#define SWITCH_HAT_DOWN_RIGHT 0x03
#define SWITCH_HAT_DOWN       0x04
#define SWITCH_HAT_DOWN_LEFT  0x05
#define SWITCH_HAT_LEFT       0x06
#define SWITCH_HAT_UP_LEFT    0x07
#define SWITCH_HAT_NEUTRAL    0x08 // Neutral state (no direction pressed)

// Analog stick center position (8-bit for POKKEN Controller)
#define STICK_CENTER 128 // 0x80 - center value for 8-bit sticks (0-255 range)

// USB HID specific constants (not always in SDK headers)
#ifndef USB_DTYPE_HID
#define USB_DTYPE_HID 0x21
#endif

#ifndef USB_DTYPE_HID_REPORT
#define USB_DTYPE_HID_REPORT 0x22
#endif

#ifndef USB_CLASS_HID
#define USB_CLASS_HID 0x03
#endif

#ifndef USB_HID_SUBCLASS_NONBOOT
#define USB_HID_SUBCLASS_NONBOOT 0x00
#endif

#ifndef USB_HID_PROTO_NONBOOT
#define USB_HID_PROTO_NONBOOT 0x00
#endif

#ifndef USB_HID_COUNTRY_NONE
#define USB_HID_COUNTRY_NONE 0x00
#endif

#ifndef USB_HID_SETIDLE
#define USB_HID_SETIDLE 0x0A
#endif

#ifndef USB_HID_GETREPORT
#define USB_HID_GETREPORT 0x01
#endif

#ifndef USB_EPTYPE_INTERRUPT
#define USB_EPTYPE_INTERRUPT 0x03
#endif

#ifndef USB_CSCP_NoDeviceClass
#define USB_CSCP_NoDeviceClass 0x00
#endif

#ifndef USB_CSCP_NoDeviceSubclass
#define USB_CSCP_NoDeviceSubclass 0x00
#endif

#ifndef USB_CSCP_NoDeviceProtocol
#define USB_CSCP_NoDeviceProtocol 0x00
#endif

#ifndef USB_EP0_SIZE
#define USB_EP0_SIZE 64
#endif

// USB descriptor type constants
#ifndef USB_DTYPE_DEVICE
#define USB_DTYPE_DEVICE 0x01
#endif

#ifndef USB_DTYPE_CONFIGURATION
#define USB_DTYPE_CONFIGURATION 0x02
#endif

#ifndef USB_DTYPE_STRING
#define USB_DTYPE_STRING 0x03
#endif

#ifndef USB_DTYPE_INTERFACE
#define USB_DTYPE_INTERFACE 0x04
#endif

#ifndef USB_DTYPE_ENDPOINT
#define USB_DTYPE_ENDPOINT 0x05
#endif

// USB string descriptor indices
#ifndef UsbDevManuf
#define UsbDevManuf 1
#endif

#ifndef UsbDevProduct
#define UsbDevProduct 2
#endif

#ifndef UsbDevSerial
#define UsbDevSerial 3
#endif

#ifndef NO_DESCRIPTOR
#define NO_DESCRIPTOR 0
#endif

// USB standard request
#ifndef USB_STD_GET_DESCRIPTOR
#define USB_STD_GET_DESCRIPTOR 0x06
#endif

// USB configuration attributes
#ifndef USB_CFG_ATTR_RESERVED
#define USB_CFG_ATTR_RESERVED 0x80
#endif

#ifndef USB_CFG_POWER_MA
#define USB_CFG_POWER_MA(mA) ((mA) >> 1)
#endif

// USB request type and recipient masks
#ifndef USB_REQ_RECIPIENT
#define USB_REQ_RECIPIENT (3 << 0)
#endif

#ifndef USB_REQ_TYPE
#define USB_REQ_TYPE (3 << 5)
#endif

#ifndef USB_REQ_INTERFACE
#define USB_REQ_INTERFACE (1 << 0)
#endif

#ifndef USB_REQ_STANDARD
#define USB_REQ_STANDARD (0 << 5)
#endif

#ifndef USB_REQ_CLASS
#define USB_REQ_CLASS (1 << 5)
#endif

// VERSION_BCD macro
#ifndef VERSION_BCD
#define VERSION_BCD(maj, min, rev) (((maj & 0xFF) << 8) | ((min & 0x0F) << 4) | (rev & 0x0F))
#endif

// HID endpoint configuration
#define HID_EP_IN    0x81
#define HID_EP_SZ    64
#define HID_INTERVAL 5

// POKKEN Controller report configuration (NO Report ID, raw 8 bytes)
#define POKKEN_REPORT_SIZE 8 // 8 bytes: 2 buttons + 1 HAT + 4 axes + 1 vendor

// Switch controller state structure (POKKEN Controller format)
// Uses 8-bit analog sticks (0-255 range, 128 = center)
typedef struct {
    uint16_t buttons; // 16 buttons across 2 bytes
    uint8_t hat; // Hat switch (0-7 directions, 15 = neutral)
    uint8_t lx; // Left stick X (0-255, 128 = center)
    uint8_t ly; // Left stick Y (0-255, 128 = center)
    uint8_t rx; // Right stick X (0-255, 128 = center)
    uint8_t ry; // Right stick Y (0-255, 128 = center)
} SwitchControllerState;

// Initialize USB HID for Switch Pro Controller
bool usb_hid_switch_init();

// Deinitialize USB HID
void usb_hid_switch_deinit();

// Send controller state
bool usb_hid_switch_send_report(SwitchControllerState* state);

// Reset controller state to neutral
void usb_hid_switch_reset_state(SwitchControllerState* state);

#ifdef __cplusplus
}
#endif
