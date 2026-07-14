#include "usb_hid_switch.h"
#include <furi_hal.h>
#include <usb_std.h>
#include <usbd_core.h>

// HID Report Descriptor for POKKEN CONTROLLER (HORI 0x0F0D/0x0092)
// Based on kashalls/NintendoSwitchController - proven working implementation
// 8-byte input + 8-byte output (Switch REQUIRES output descriptor!)
static const uint8_t hid_report_descriptor[] = {
    0x05,
    0x01, // Usage Page (Generic Desktop)
    0x09,
    0x05, // Usage (Game Pad)
    0xA1,
    0x01, // Collection (Application)

    // Buttons (16 buttons in 2 bytes)
    0x15,
    0x00, //   Logical Minimum (0)
    0x25,
    0x01, //   Logical Maximum (1)
    0x35,
    0x00, //   Physical Minimum (0)
    0x45,
    0x01, //   Physical Maximum (1)
    0x75,
    0x01, //   Report Size (1)
    0x95,
    0x10, //   Report Count (16) - buttons
    0x05,
    0x09, //   Usage Page (Button)
    0x19,
    0x01, //   Usage Minimum (Button 1)
    0x29,
    0x10, //   Usage Maximum (Button 16)
    0x81,
    0x02, //   Input (Data,Var,Abs)

    // HAT Switch (D-Pad)
    0x05,
    0x01, //   Usage Page (Generic Desktop)
    0x25,
    0x07, //   Logical Maximum (7)
    0x46,
    0x3B,
    0x01, //   Physical Maximum (315)
    0x75,
    0x04, //   Report Size (4)
    0x95,
    0x01, //   Report Count (1)
    0x65,
    0x14, //   Unit (Degrees)
    0x09,
    0x39, //   Usage (Hat Switch)
    0x81,
    0x42, //   Input (Data,Var,Abs,Null)

    // Padding (4 bits to align to byte boundary)
    0x65,
    0x00, //   Unit (None)
    0x95,
    0x01, //   Report Count (1)
    0x81,
    0x01, //   Input (Const,Array,Abs)

    // Analog Sticks (4 axes: X, Y, Z, Rz - 8-bit each)
    0x26,
    0xFF,
    0x00, //   Logical Maximum (255)
    0x46,
    0xFF,
    0x00, //   Physical Maximum (255)
    0x09,
    0x30, //   Usage (X)
    0x09,
    0x31, //   Usage (Y)
    0x09,
    0x32, //   Usage (Z)
    0x09,
    0x35, //   Usage (Rz)
    0x75,
    0x08, //   Report Size (8)
    0x95,
    0x04, //   Report Count (4) - axes
    0x81,
    0x02, //   Input (Data,Var,Abs)

    // Vendor Specific byte (REQUIRED by Switch!)
    0x75,
    0x08, //   Report Size (8)
    0x95,
    0x01, //   Report Count (1)
    0x81,
    0x03, //   Input (Const,Var,Abs)

    // Output Report (8 bytes) - Switch REQUIRES this!
    0x75,
    0x08, //   Report Size (8)
    0x95,
    0x08, //   Report Count (8)
    0x91,
    0x02, //   Output (Data,Var,Abs)

    0xC0 // End Collection
};

// USB device descriptor
static const struct usb_device_descriptor hid_switch_device_descriptor = {
    .bLength = sizeof(struct usb_device_descriptor),
    .bDescriptorType = USB_DTYPE_DEVICE,
    .bcdUSB = VERSION_BCD(2, 0, 0),
    .bDeviceClass = USB_CSCP_NoDeviceClass,
    .bDeviceSubClass = USB_CSCP_NoDeviceSubclass,
    .bDeviceProtocol = USB_CSCP_NoDeviceProtocol,
    .bMaxPacketSize0 = USB_EP0_SIZE,
    .idVendor = USB_VID_NINTENDO,
    .idProduct = USB_PID_SWITCH_PRO,
    .bcdDevice = VERSION_BCD(2, 0, 0),
    .iManufacturer = UsbDevManuf,
    .iProduct = UsbDevProduct,
    .iSerialNumber = UsbDevSerial,
    .bNumConfigurations = 1,
};

// Configuration descriptor
static const struct {
    struct usb_config_descriptor config;
    struct usb_interface_descriptor hid_interface;
    struct usb_hid_descriptor hid;
    struct usb_endpoint_descriptor hid_ep_in;
} __attribute__((packed)) hid_switch_config_descriptor = {
    .config =
        {
            .bLength = sizeof(struct usb_config_descriptor),
            .bDescriptorType = USB_DTYPE_CONFIGURATION,
            .wTotalLength = sizeof(hid_switch_config_descriptor),
            .bNumInterfaces = 1,
            .bConfigurationValue = 1,
            .iConfiguration = NO_DESCRIPTOR,
            .bmAttributes = USB_CFG_ATTR_RESERVED,
            .bMaxPower = USB_CFG_POWER_MA(500),
        },
    .hid_interface =
        {
            .bLength = sizeof(struct usb_interface_descriptor),
            .bDescriptorType = USB_DTYPE_INTERFACE,
            .bInterfaceNumber = 0,
            .bAlternateSetting = 0,
            .bNumEndpoints = 1,
            .bInterfaceClass = USB_CLASS_HID,
            .bInterfaceSubClass = USB_HID_SUBCLASS_NONBOOT,
            .bInterfaceProtocol = USB_HID_PROTO_NONBOOT,
            .iInterface = NO_DESCRIPTOR,
        },
    .hid =
        {
            .bLength = sizeof(struct usb_hid_descriptor),
            .bDescriptorType = USB_DTYPE_HID,
            .bcdHID = VERSION_BCD(1, 1, 1),
            .bCountryCode = USB_HID_COUNTRY_NONE,
            .bNumDescriptors = 1,
            .bDescriptorType0 = USB_DTYPE_HID_REPORT,
            .wDescriptorLength0 = sizeof(hid_report_descriptor),
        },
    .hid_ep_in =
        {
            .bLength = sizeof(struct usb_endpoint_descriptor),
            .bDescriptorType = USB_DTYPE_ENDPOINT,
            .bEndpointAddress = HID_EP_IN,
            .bmAttributes = USB_EPTYPE_INTERRUPT,
            .wMaxPacketSize = HID_EP_SZ,
            .bInterval = HID_INTERVAL,
        },
};

// String descriptors - POKKEN CONTROLLER
static const struct usb_string_descriptor dev_manuf_desc = USB_STRING_DESC("HORI CO.,LTD.");
static const struct usb_string_descriptor dev_prod_desc = USB_STRING_DESC("POKKEN CONTROLLER");

// USB interface callbacks
static usbd_respond hid_switch_ep_config(usbd_device* dev, uint8_t cfg);
static usbd_respond
    hid_switch_control(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* callback);
static void hid_switch_init(usbd_device* dev, FuriHalUsbInterface* intf, void* ctx);
static void hid_switch_deinit(usbd_device* dev);
static usbd_device* usb_dev = NULL;

// USB interface structure
static FuriHalUsbInterface usb_hid_switch = {
    .init = hid_switch_init,
    .deinit = hid_switch_deinit,
    .wakeup = NULL,
    .suspend = NULL,
    .dev_descr = (struct usb_device_descriptor*)&hid_switch_device_descriptor,
    .str_manuf_descr = (void*)&dev_manuf_desc,
    .str_prod_descr = (void*)&dev_prod_desc,
    .str_serial_descr = NULL,
    .cfg_descr = (void*)&hid_switch_config_descriptor,
};

// Endpoint configuration callback
static usbd_respond hid_switch_ep_config(usbd_device* dev, uint8_t cfg) {
    switch(cfg) {
    case 0:
        // Deconfigure
        usbd_ep_deconfig(dev, HID_EP_IN);
        usbd_reg_endpoint(dev, HID_EP_IN, NULL);
        return usbd_ack;
    case 1:
        // Configure
        usbd_ep_config(dev, HID_EP_IN, USB_EPTYPE_INTERRUPT, HID_EP_SZ);
        usbd_reg_endpoint(dev, HID_EP_IN, NULL);
        return usbd_ack;
    default:
        return usbd_fail;
    }
}

// Control request handler
static usbd_respond
    hid_switch_control(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* callback) {
    UNUSED(callback);

    if(((USB_REQ_RECIPIENT | USB_REQ_TYPE) & req->bmRequestType) ==
       (USB_REQ_INTERFACE | USB_REQ_STANDARD)) {
        switch(req->bRequest) {
        case USB_STD_GET_DESCRIPTOR:
            if(req->wValue == ((USB_DTYPE_HID_REPORT << 8) | 0)) {
                dev->status.data_ptr = (uint8_t*)hid_report_descriptor;
                dev->status.data_count = sizeof(hid_report_descriptor);
                return usbd_ack;
            } else if(req->wValue == ((USB_DTYPE_HID << 8) | 0)) {
                dev->status.data_ptr = (uint8_t*)&hid_switch_config_descriptor.hid;
                dev->status.data_count = sizeof(hid_switch_config_descriptor.hid);
                return usbd_ack;
            }
            return usbd_fail;
        default:
            return usbd_fail;
        }
    }

    if(((USB_REQ_RECIPIENT | USB_REQ_TYPE) & req->bmRequestType) ==
       (USB_REQ_INTERFACE | USB_REQ_CLASS)) {
        switch(req->bRequest) {
        case USB_HID_SETIDLE:
            return usbd_ack;
        case USB_HID_GETREPORT:
            return usbd_fail;
        default:
            return usbd_fail;
        }
    }

    return usbd_fail;
}

// USB interface init callback
static void hid_switch_init(usbd_device* dev, FuriHalUsbInterface* intf, void* ctx) {
    UNUSED(intf);
    UNUSED(ctx);

    // Store device handle
    usb_dev = dev;

    // Register callbacks
    usbd_reg_config(dev, hid_switch_ep_config);
    usbd_reg_control(dev, hid_switch_control);

    // Configure endpoint
    usbd_ep_config(dev, HID_EP_IN, USB_EPTYPE_INTERRUPT, HID_EP_SZ);
    usbd_reg_endpoint(dev, HID_EP_IN, NULL);
}

// USB interface deinit callback
static void hid_switch_deinit(usbd_device* dev) {
    // Deconfigure endpoint
    usbd_ep_deconfig(dev, HID_EP_IN);
    usbd_reg_endpoint(dev, HID_EP_IN, NULL);

    // Clear device handle
    usb_dev = NULL;
}

bool usb_hid_switch_init() {
    // Set USB configuration
    furi_hal_usb_unlock();
    bool result = furi_hal_usb_set_config(&usb_hid_switch, NULL);
    return result;
}

void usb_hid_switch_deinit() {
    furi_hal_usb_unlock();
    furi_hal_usb_set_config(NULL, NULL);
}

bool usb_hid_switch_send_report(SwitchControllerState* state) {
    if(!state || !usb_dev) return false;

    // Build POKKEN CONTROLLER HID report (8 bytes, NO Report ID)
    // Based on kashalls/NintendoSwitchController proven implementation
    uint8_t report[8];

    // Bytes 0-1: Buttons (16 buttons across 2 bytes)
    // Byte 0: Y, B, A, X, L, R, ZL, ZR (bits 0-7)
    // Byte 1: -, +, LStick, RStick, Home, Capture, reserved (bits 0-5)
    report[0] = state->buttons & 0xFF;
    report[1] = (state->buttons >> 8) & 0xFF;

    // Byte 2: HAT Switch (lower 4 bits) + padding (upper 4 bits)
    report[2] = (state->hat & 0x0F);

    // Bytes 3-6: Analog sticks (4 axes, 8-bit each, 0-255 range)
    report[3] = state->lx; // Left stick X (axis 0 = X)
    report[4] = state->ly; // Left stick Y (axis 1 = Y)
    report[5] = state->rx; // Right stick X (axis 2 = Z)
    report[6] = state->ry; // Right stick Y (axis 3 = Rz)

    // Byte 7: Vendor-specific byte (set to 0)
    report[7] = 0x00;

    // Send report (exactly 8 bytes, no Report ID)
    int result = usbd_ep_write(usb_dev, HID_EP_IN, report, 8);

    return (result == 8);
}

void usb_hid_switch_reset_state(SwitchControllerState* state) {
    if(!state) return;

    state->buttons = 0;
    state->hat = SWITCH_HAT_NEUTRAL;
    state->lx = STICK_CENTER;
    state->ly = STICK_CENTER;
    state->rx = STICK_CENTER;
    state->ry = STICK_CENTER;
}
