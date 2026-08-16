#include "fmtx_usb.h"

#include <usb.h>

static struct usb_device_descriptor fmtx_usb_device = {
    .bLength = sizeof(struct usb_device_descriptor),
    .bDescriptorType = USB_DTYPE_DEVICE,
    .bcdUSB = VERSION_BCD(2, 0, 0),
    .bDeviceClass = USB_CLASS_PER_INTERFACE,
    .bDeviceSubClass = USB_SUBCLASS_NONE,
    .bDeviceProtocol = USB_PROTO_NONE,
    .bMaxPacketSize0 = 64,
    .idVendor = 0x0483,
    .idProduct = 0x5742,
    .bcdDevice = VERSION_BCD(1, 0, 0),
    .bNumConfigurations = 1,
};

// clang-format off
static const uint8_t fmtx_usb_config[] = {
    9, USB_DTYPE_CONFIGURATION, 100, 0, 2, 1, 0, USB_CFG_ATTR_RESERVED | USB_CFG_ATTR_SELFPOWERED,
    USB_CFG_POWER_MA(500),

    9, USB_DTYPE_INTERFACE, 0, 0, 0, USB_CLASS_AUDIO, 1, 0, 0,
    9, USB_DTYPE_CS_INTERFACE, 1, 0x00, 0x01, 30, 0, 1, 1,
    12, USB_DTYPE_CS_INTERFACE, 2, 1, 0x01, 0x01, 0, 1, 0, 0, 0, 0,
    9, USB_DTYPE_CS_INTERFACE, 3, 2, 0x01, 0x03, 0, 1, 0,

    9, USB_DTYPE_INTERFACE, 1, 0, 0, USB_CLASS_AUDIO, 2, 0, 0,
    9, USB_DTYPE_INTERFACE, 1, 1, 1, USB_CLASS_AUDIO, 2, 0, 0,
    7, USB_DTYPE_CS_INTERFACE, 1, 1, 1, 0, 0,
    11, USB_DTYPE_CS_INTERFACE, 2, 1, 1, 2, 16, 1, 0x80, 0xbb, 0x00,
    9, USB_DTYPE_ENDPOINT, 0x01, USB_EPTYPE_ISOCHRONUS | USB_EPATTR_ADAPTIVE, 96, 0, 1, 0, 0,
    7, USB_DTYPE_CS_ENDPOINT, 1, 0, 0, 0, 0,
};
// clang-format on

_Static_assert(sizeof(fmtx_usb_config) == 100, "usb descriptor size");

static void fmtx_usb_init(usbd_device* dev, FuriHalUsbInterface* intf, void* ctx) {
    (void)dev;
    (void)intf;
    (void)ctx;
}

static void fmtx_usb_deinit(usbd_device* dev) {
    (void)dev;
}

static void fmtx_usb_wakeup(usbd_device* dev) {
    (void)dev;
}

static void fmtx_usb_suspend(usbd_device* dev) {
    (void)dev;
}

FuriHalUsbInterface fmtx_usb_audio = {
    .init = fmtx_usb_init,
    .deinit = fmtx_usb_deinit,
    .wakeup = fmtx_usb_wakeup,
    .suspend = fmtx_usb_suspend,
    .dev_descr = &fmtx_usb_device,
    .cfg_descr = (void*)fmtx_usb_config,
};
