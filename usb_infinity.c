/*
 * USB HID device that looks like a Disney Infinity base (VID 0E6F, PID 0129):
 * one interface, two 32-byte interrupt endpoints (0x81 IN, 0x01 OUT). The
 * descriptor values follow RPCS3's emulated base; the structure follows the
 * Lego Dimensions toy pad app that is known to work on a PS5.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
#include "usb_infinity.h"

#include <furi_hal_usb.h>
#include <string.h>
#include <usb.h>
#include <usb_hid.h>

#define INF_USB_VID  0x0E6F
#define INF_USB_PID  0x0129
#define INF_EP_IN    0x81
#define INF_EP_OUT   0x01
#define INF_EP_SIZE  0x20
#define INF_EP0_SIZE 64

PLACE_IN_SECTION("MB_MEM2") static uint32_t ubuf[0x20];

UsbInfStats usb_inf_stats;
InfBase usb_inf_base;

/* Vendor-defined, 32 bytes in and 32 bytes out (29 bytes long, 0x1D as the base reports). */
static const uint8_t report_desc[] = {
    0x06, 0x00, 0xFF, /* Usage Page (Vendor Defined) */
    0x09, 0x01, /* Usage (1) */
    0xA1, 0x01, /* Collection (Application) */
    0x19, 0x01, 0x29, 0x20, /* Usage Min/Max 1..32 */
    0x15, 0x00, 0x26, 0xFF, 0x00, /* Logical 0..255 */
    0x75, 0x08, 0x95, 0x20, /* 32 x 8 bits */
    0x81, 0x00, /* Input */
    0x19, 0x01, 0x29, 0x20, /* Usage Min/Max 1..32 */
    0x91, 0x00, /* Output */
    0xC0};

struct InfIntfDescriptor {
    struct usb_interface_descriptor intf;
    struct usb_hid_descriptor hid;
    struct usb_endpoint_descriptor ep_in;
    struct usb_endpoint_descriptor ep_out;
};

struct InfCfgDescriptor {
    struct usb_config_descriptor config;
    struct InfIntfDescriptor intf_0;
} __attribute__((packed));

static struct usb_device_descriptor dev_desc = {
    .bLength = sizeof(struct usb_device_descriptor),
    .bDescriptorType = USB_DTYPE_DEVICE,
    .bcdUSB = VERSION_BCD(2, 0, 0),
    .bDeviceClass = USB_CLASS_PER_INTERFACE,
    .bDeviceSubClass = USB_SUBCLASS_NONE,
    .bDeviceProtocol = USB_PROTO_NONE,
    .bMaxPacketSize0 = INF_EP0_SIZE,
    .idVendor = INF_USB_VID,
    .idProduct = INF_USB_PID,
    .bcdDevice = VERSION_BCD(2, 0, 0),
    /* No string descriptors: asking for one that does not exist would stall. */
    .iManufacturer = 0,
    .iProduct = 0,
    .iSerialNumber = 0,
    .bNumConfigurations = 1,
};

static const struct InfCfgDescriptor cfg_desc = {
    .config =
        {
            .bLength = sizeof(struct usb_config_descriptor),
            .bDescriptorType = USB_DTYPE_CONFIGURATION,
            .wTotalLength = sizeof(struct InfCfgDescriptor),
            .bNumInterfaces = 1,
            .bConfigurationValue = 1,
            .iConfiguration = NO_DESCRIPTOR,
            .bmAttributes = USB_CFG_ATTR_RESERVED,
            .bMaxPower = USB_CFG_POWER_MA(500),
        },
    .intf_0 =
        {
            .intf =
                {
                    .bLength = sizeof(struct usb_interface_descriptor),
                    .bDescriptorType = USB_DTYPE_INTERFACE,
                    .bInterfaceNumber = 0,
                    .bAlternateSetting = 0,
                    .bNumEndpoints = 2,
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
                    .wDescriptorLength0 = sizeof(report_desc),
                },
            .ep_in =
                {
                    .bLength = sizeof(struct usb_endpoint_descriptor),
                    .bDescriptorType = USB_DTYPE_ENDPOINT,
                    .bEndpointAddress = INF_EP_IN,
                    .bmAttributes = USB_EPTYPE_INTERRUPT,
                    .wMaxPacketSize = INF_EP_SIZE,
                    .bInterval = 1,
                },
            .ep_out =
                {
                    .bLength = sizeof(struct usb_endpoint_descriptor),
                    .bDescriptorType = USB_DTYPE_ENDPOINT,
                    .bEndpointAddress = INF_EP_OUT,
                    .bmAttributes = USB_EPTYPE_INTERRUPT,
                    .wMaxPacketSize = INF_EP_SIZE,
                    .bInterval = 1,
                },
        },
};

static usbd_device* g_dev = NULL;
static uint8_t pending[INF_PACKET_SIZE];
static bool pending_valid = false;
static FuriHalUsbInterface* prev_config = NULL;

/* Hand the next packet to the IN endpoint. Runs in the USB interrupt, or in a
 * critical section when called from the UI. The endpoint stays busy until the
 * host reads it, then the IN callback calls this again. */
static void try_send(void) {
    if(g_dev == NULL) return;
    if(!pending_valid) {
        if(!inf_base_next_in(&usb_inf_base, pending)) return;
        pending_valid = true;
    }
    if(usbd_ep_write(g_dev, INF_EP_IN, pending, INF_EP_SIZE) < 0) {
        usb_inf_stats.in_busy++;
        return;
    }
    pending_valid = false;
    usb_inf_stats.in_packets++;
}

static void ep_in_callback(usbd_device* dev, uint8_t event, uint8_t ep) {
    UNUSED(dev);
    UNUSED(event);
    UNUSED(ep);
    try_send();
}

static void ep_out_callback(usbd_device* dev, uint8_t event, uint8_t ep) {
    UNUSED(event);
    UNUSED(ep);
    uint8_t buf[INF_PACKET_SIZE] = {0};
    const int32_t len = usbd_ep_read(dev, INF_EP_OUT, buf, INF_EP_SIZE);
    if(len <= 0) return;
    usb_inf_stats.out_packets++;
    usb_inf_stats.last_rx_tick = furi_get_tick();
    usb_inf_stats.suspended = false;
    inf_base_handle_out(&usb_inf_base, buf);
    try_send();
}

static usbd_respond ep_config(usbd_device* dev, uint8_t cfg) {
    switch(cfg) {
    case 0:
        usbd_ep_deconfig(dev, INF_EP_IN);
        usbd_ep_deconfig(dev, INF_EP_OUT);
        usbd_reg_endpoint(dev, INF_EP_IN, 0);
        usbd_reg_endpoint(dev, INF_EP_OUT, 0);
        return usbd_ack;
    case 1:
        usbd_ep_config(dev, INF_EP_IN, USB_EPTYPE_INTERRUPT, INF_EP_SIZE);
        usbd_reg_endpoint(dev, INF_EP_IN, ep_in_callback);
        usbd_ep_config(dev, INF_EP_OUT, USB_EPTYPE_INTERRUPT, INF_EP_SIZE);
        usbd_reg_endpoint(dev, INF_EP_OUT, ep_out_callback);
        /* A new session: forget queued packets and the old handshake. Figures stay;
         * the game finds them with its first "which figures are present" query. */
        pending_valid = false;
        inf_base_reset_link(&usb_inf_base);
        usb_inf_stats.enum_configs++;
        usb_inf_stats.suspended = false;
        usb_inf_stats.last_rx_tick = furi_get_tick();
        return usbd_ack;
    default:
        return usbd_fail;
    }
}

static usbd_respond control_request(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* callback) {
    UNUSED(callback);
    usb_inf_stats.ctrl_requests++;

    if(((USB_REQ_RECIPIENT | USB_REQ_TYPE) & req->bmRequestType) ==
           (USB_REQ_INTERFACE | USB_REQ_CLASS) &&
       req->wIndex == 0) {
        switch(req->bRequest) {
        case USB_HID_SETIDLE:
        case USB_HID_SETPROTOCOL:
            return usbd_ack;
        default:
            break;
        }
    }
    if(((USB_REQ_RECIPIENT | USB_REQ_TYPE) & req->bmRequestType) ==
           (USB_REQ_INTERFACE | USB_REQ_STANDARD) &&
       req->wIndex == 0 && req->bRequest == USB_STD_GET_DESCRIPTOR) {
        switch(req->wValue >> 8) {
        case USB_DTYPE_HID:
            dev->status.data_ptr = (uint8_t*)&cfg_desc.intf_0.hid;
            dev->status.data_count = sizeof(cfg_desc.intf_0.hid);
            return usbd_ack;
        case USB_DTYPE_HID_REPORT:
            dev->status.data_ptr = (uint8_t*)report_desc;
            dev->status.data_count = sizeof(report_desc);
            return usbd_ack;
        default:
            break;
        }
    }

    usb_inf_stats.ctrl_rejected++;
    usb_inf_stats.last_rejected_type = req->bmRequestType;
    usb_inf_stats.last_rejected_req = req->bRequest;
    return usbd_fail;
}

static void inf_usb_init(usbd_device* dev, FuriHalUsbInterface* intf, void* ctx) {
    UNUSED(intf);
    UNUSED(ctx);
    g_dev = dev;
    pending_valid = false;

    usbd_reg_config(dev, ep_config);
    usbd_reg_control(dev, control_request);

    /* The Flipper starts its USB stack with an 8-byte control endpoint; ours is
     * declared as 64 (as the working Lego toy pad app does), so start it again. */
    usbd_init(dev, &usbd_hw, INF_EP0_SIZE, ubuf, sizeof(ubuf));
    usbd_connect(dev, true);
}

static void inf_usb_deinit(usbd_device* dev) {
    g_dev = NULL;
    usbd_init(dev, &usbd_hw, 8, ubuf, sizeof(ubuf));
    usbd_reg_config(dev, NULL);
    usbd_reg_control(dev, NULL);
}

static void inf_usb_wakeup(usbd_device* dev) {
    UNUSED(dev);
    usb_inf_stats.suspended = false;
}

static void inf_usb_suspend(usbd_device* dev) {
    UNUSED(dev);
    usb_inf_stats.suspends++;
    usb_inf_stats.suspended = true;
}

static FuriHalUsbInterface usb_inf_iface = {
    .init = inf_usb_init,
    .deinit = inf_usb_deinit,
    .wakeup = inf_usb_wakeup,
    .suspend = inf_usb_suspend,

    .dev_descr = &dev_desc,

    .str_manuf_descr = NULL,
    .str_prod_descr = NULL,
    .str_serial_descr = NULL,

    .cfg_descr = (void*)&cfg_desc,
};

void usb_infinity_start(void) {
    memset((void*)&usb_inf_stats, 0, sizeof(usb_inf_stats));
    prev_config = furi_hal_usb_get_config();
    furi_hal_usb_unlock();
    furi_check(furi_hal_usb_set_config(&usb_inf_iface, NULL) == true);
}

void usb_infinity_stop(void) {
    if(prev_config != NULL) {
        furi_hal_usb_set_config(prev_config, NULL);
        prev_config = NULL;
    }
}

void usb_infinity_kick(void) {
    FURI_CRITICAL_ENTER();
    try_send();
    FURI_CRITICAL_EXIT();
}
