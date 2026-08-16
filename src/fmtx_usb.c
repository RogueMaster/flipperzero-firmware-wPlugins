#include "fmtx_usb.h"

#include <usb.h>
#include <usb_std.h>

#define FMTX_USB_EP     0x01
#define FMTX_USB_PACKET 96U

enum {
    FmtxUsbStringManufacturer = 1,
    FmtxUsbStringProduct,
};

static struct usb_device_descriptor fmtx_usb_device = {
    .bLength = sizeof(struct usb_device_descriptor),
    .bDescriptorType = USB_DTYPE_DEVICE,
    .bcdUSB = VERSION_BCD(2, 0, 0),
    .bDeviceClass = USB_CLASS_PER_INTERFACE,
    .bDeviceSubClass = USB_SUBCLASS_NONE,
    .bDeviceProtocol = USB_PROTO_NONE,
    .bMaxPacketSize0 = 8,
    .idVendor = 0x0483,
    .idProduct = 0x5742,
    .bcdDevice = VERSION_BCD(1, 0, 0),
    .iManufacturer = FmtxUsbStringManufacturer,
    .iProduct = FmtxUsbStringProduct,
    .bNumConfigurations = 1,
};

static const struct usb_string_descriptor fmtx_usb_manufacturer = USB_STRING_DESC("YO3GND");
static const struct usb_string_descriptor fmtx_usb_product =
    USB_STRING_DESC("YO3GND Flipper FMTX");

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

static FmtxUsbRx fmtx_usb_callback;
static void* fmtx_usb_context;
static FuriHalUsbInterface* fmtx_usb_previous;
static uint8_t fmtx_usb_alt;
static uint8_t fmtx_usb_phase;
static int32_t fmtx_usb_sum;
static uint8_t fmtx_usb_reply;
static volatile bool fmtx_usb_active;
static volatile bool fmtx_usb_configured;
static usbd_device* volatile fmtx_usb_dev;

static void fmtx_usb_rx(usbd_device* dev, uint8_t event, uint8_t ep);

static void fmtx_usb_stream(usbd_device* dev, bool on) {
    if(on) {
        usbd_ep_config(dev, FMTX_USB_EP, USB_EPTYPE_ISOCHRONUS, FMTX_USB_PACKET);
        usbd_reg_endpoint(dev, FMTX_USB_EP, fmtx_usb_rx);
    } else {
        usbd_ep_deconfig(dev, FMTX_USB_EP);
        usbd_reg_endpoint(dev, FMTX_USB_EP, NULL);
    }
}

static void fmtx_usb_rx(usbd_device* dev, uint8_t event, uint8_t ep) {
    int16_t input[FMTX_USB_PACKET / 2U];
    int16_t output[FMTX_USB_PACKET / 12U];
    size_t count = 0;
    int32_t n;
    if(event != usbd_evt_eprx) return;
    n = usbd_ep_read(dev, ep, input, sizeof(input));
    if(n <= 0 || fmtx_usb_alt != 1U) return;
    fmtx_usb_configured = true;
    for(int32_t i = 0; i < n / 2; i++) {
        fmtx_usb_sum += input[i];
        fmtx_usb_phase++;
        if(fmtx_usb_phase == 6U) {
            output[count++] = fmtx_usb_sum / 6;
            fmtx_usb_phase = 0;
            fmtx_usb_sum = 0;
        }
    }
    if(fmtx_usb_callback && count) fmtx_usb_callback(output, count, fmtx_usb_context);
}

static usbd_respond fmtx_usb_ep_config(usbd_device* dev, uint8_t cfg) {
    fmtx_usb_configured = false;
    fmtx_usb_alt = 0;
    fmtx_usb_phase = 0;
    fmtx_usb_sum = 0;
    if(cfg == 0U) {
        fmtx_usb_stream(dev, false);
        return usbd_ack;
    }
    if(cfg == 1U) {
        fmtx_usb_configured = true;
        return usbd_ack;
    }
    return usbd_fail;
}

static usbd_respond
    fmtx_usb_control(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* callback) {
    (void)callback;
    if((req->bmRequestType & (USB_REQ_RECIPIENT | USB_REQ_TYPE)) !=
           (USB_REQ_INTERFACE | USB_REQ_STANDARD) ||
       req->wIndex > 1U)
        return usbd_fail;
    if(req->bRequest == USB_STD_SET_INTERFACE) {
        if((req->bmRequestType & USB_REQ_DIRECTION) != USB_REQ_HOSTTODEV || req->wLength != 0U ||
           req->wValue > (req->wIndex == 1U ? 1U : 0U))
            return usbd_fail;
        if(req->wIndex == 1U) {
            if(fmtx_usb_alt == 1U) fmtx_usb_stream(dev, false);
            fmtx_usb_alt = req->wValue;
            fmtx_usb_phase = 0;
            fmtx_usb_sum = 0;
            if(fmtx_usb_alt == 1U) fmtx_usb_stream(dev, true);
        }
        return usbd_ack;
    }
    if(req->bRequest == USB_STD_GET_INTERFACE) {
        if((req->bmRequestType & USB_REQ_DIRECTION) != USB_REQ_DEVTOHOST || req->wValue != 0U ||
           req->wLength < 1U)
            return usbd_fail;
        fmtx_usb_reply = req->wIndex == 1U ? fmtx_usb_alt : 0U;
        dev->status.data_ptr = &fmtx_usb_reply;
        dev->status.data_count = 1U;
        return usbd_ack;
    }
    return usbd_fail;
}

static void fmtx_usb_init(usbd_device* dev, FuriHalUsbInterface* intf, void* ctx) {
    (void)intf;
    (void)ctx;
    fmtx_usb_dev = dev;
    usbd_reg_config(dev, fmtx_usb_ep_config);
    usbd_reg_control(dev, fmtx_usb_control);
    usbd_connect(dev, true);
}

static void fmtx_usb_deinit(usbd_device* dev) {
    fmtx_usb_configured = false;
    fmtx_usb_alt = 0;
    usbd_reg_config(dev, NULL);
    usbd_reg_control(dev, NULL);
    fmtx_usb_dev = NULL;
}

static void fmtx_usb_wakeup(usbd_device* dev) {
    fmtx_usb_configured = dev->status.device_state == usbd_state_configured;
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
    .str_manuf_descr = (void*)&fmtx_usb_manufacturer,
    .str_prod_descr = (void*)&fmtx_usb_product,
    .cfg_descr = (void*)fmtx_usb_config,
};

bool fmtx_usb_start(FmtxUsbRx callback, void* ctx) {
    if(fmtx_usb_active || !callback) return false;
    fmtx_usb_previous = furi_hal_usb_get_config();
    fmtx_usb_callback = callback;
    fmtx_usb_context = ctx;
    fmtx_usb_phase = 0;
    fmtx_usb_sum = 0;
    fmtx_usb_configured = false;
    if(!furi_hal_usb_set_config(&fmtx_usb_audio, NULL)) {
        fmtx_usb_callback = NULL;
        fmtx_usb_context = NULL;
        fmtx_usb_previous = NULL;
        return false;
    }
    fmtx_usb_active = true;
    return true;
}

void fmtx_usb_stop(void) {
    FuriHalUsbInterface* previous;
    if(!fmtx_usb_active) return;
    fmtx_usb_callback = NULL;
    fmtx_usb_context = NULL;
    fmtx_usb_configured = false;
    fmtx_usb_alt = 0;
    previous = fmtx_usb_previous;
    fmtx_usb_previous = NULL;
    fmtx_usb_active = false;
    (void)furi_hal_usb_set_config(previous, NULL);
}

bool fmtx_usb_connected(void) {
    usbd_device* dev = fmtx_usb_dev;
    return fmtx_usb_active && fmtx_usb_configured && dev &&
           dev->status.device_state == usbd_state_configured;
}
