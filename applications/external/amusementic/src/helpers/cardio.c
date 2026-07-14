/*
see also:
- flipperdevices/flipperzero-firmware@1.0.1:
    - targets/f7/furi_hal/furi_hal_usb_hid.c (a lot of this code is copied from there)
    - applications/debug/usb_mouse/usb_mouse.c
- CrazyRedMachine/PN5180-cardio@1c5a389:
    - CARDIOHID.cpp
*/

#include "cardio.h"
#include <usb.h>
#include <usb_hid.h>

// from furi_hal_usb_i.h
#define USB_EP0_SIZE 8
enum UsbDevDescStr {
    UsbDevLang = 0,
    UsbDevManuf = 1,
    UsbDevProduct = 2,
    UsbDevSerial = 3,
};

#define USAGE_PAGE 0xFFCA

#define HID_EP_IN    0x81
#define HID_EP_SIZE  64 // TODO: PN5180-cardio uses 64, furi_hal_usb_hid uses 0x10
#define HID_INTERVAL 1

// see: http://www.linux-usb.org/usb.ids
#define USB_VID 0x5B5D // ascii "[]"
#define USB_PID 0xFA1C // "f aic"

const char* String_Manufacturer = "object-Object";
const char* String_Product = "FlipAIC";
const char* String_Serial_Player1 = "CARDIOP1";
const char* String_Serial_Player2 = "CARDIOP2";

struct HidIntfDescriptor {
    struct usb_interface_descriptor hid;
    struct usb_hid_descriptor hid_desc;
    struct usb_endpoint_descriptor hid_ep_in;
};

struct HidConfigDescriptor {
    struct usb_config_descriptor config;
    struct HidIntfDescriptor intf_0;
} FURI_PACKED;

// HID report descriptor
static const uint8_t cardio_report_desc[] = {
    // clang-format off
    HID_RI_USAGE_PAGE(16, USAGE_PAGE),
    HID_USAGE(0x01),
    HID_COLLECTION(HID_APPLICATION_COLLECTION),
        HID_REPORT_ID(CardioReportIdISO15693),
        HID_RI_USAGE_PAGE(16, USAGE_PAGE),
        HID_USAGE(0x41),
        HID_LOGICAL_MINIMUM(0),
        HID_LOGICAL_MAXIMUM(-1),
        HID_REPORT_SIZE(8),
        HID_REPORT_COUNT(8),
        HID_INPUT(HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE),

        HID_REPORT_ID(CardioReportIdFeliCa),
        HID_RI_USAGE_PAGE(16, USAGE_PAGE),
        HID_USAGE(0x42),
        HID_LOGICAL_MINIMUM(0),
        HID_LOGICAL_MAXIMUM(-1),
        HID_REPORT_SIZE(8),
        HID_REPORT_COUNT(8),
        HID_INPUT(HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE),
    HID_END_COLLECTION,
    // clang-format on
};

// device descriptor
static struct usb_device_descriptor cardio_device_desc = {
    .bLength = sizeof(struct usb_device_descriptor),
    .bDescriptorType = USB_DTYPE_DEVICE,
    .bcdUSB = VERSION_BCD(2, 0, 0),
    .bDeviceClass = USB_CLASS_PER_INTERFACE,
    .bDeviceSubClass = USB_SUBCLASS_NONE,
    .bDeviceProtocol = USB_PROTO_NONE,
    .bMaxPacketSize0 = USB_EP0_SIZE,
    .idVendor = USB_VID,
    .idProduct = USB_PID,
    .bcdDevice = VERSION_BCD(1, 0, 0),
    .iManufacturer = UsbDevManuf,
    .iProduct = UsbDevProduct,
    .iSerialNumber = UsbDevSerial,
    .bNumConfigurations = 1,
};

// device configuration descriptor
static const struct HidConfigDescriptor cardio_cfg_desc = {
    .config =
        {
            .bLength = sizeof(struct usb_config_descriptor),
            .bDescriptorType = USB_DTYPE_CONFIGURATION,
            .wTotalLength = sizeof(struct HidConfigDescriptor),
            .bNumInterfaces = 1,
            .bConfigurationValue = 1,
            .iConfiguration = NO_DESCRIPTOR,
            .bmAttributes = USB_CFG_ATTR_RESERVED | USB_CFG_ATTR_SELFPOWERED,
            .bMaxPower = USB_CFG_POWER_MA(500),
        },
    .intf_0 =
        {
            .hid =
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
            .hid_desc =
                {
                    .bLength = sizeof(struct usb_hid_descriptor),
                    .bDescriptorType = USB_DTYPE_HID,
                    .bcdHID = VERSION_BCD(1, 0, 0),
                    .bCountryCode = USB_HID_COUNTRY_NONE,
                    .bNumDescriptors = 1,
                    .bDescriptorType0 = USB_DTYPE_HID_REPORT,
                    .wDescriptorLength0 = sizeof(cardio_report_desc),
                },
            .hid_ep_in =
                {
                    .bLength = sizeof(struct usb_endpoint_descriptor),
                    .bDescriptorType = USB_DTYPE_ENDPOINT,
                    .bEndpointAddress = HID_EP_IN,
                    .bmAttributes = USB_EPTYPE_INTERRUPT,
                    .wMaxPacketSize = HID_EP_SIZE,
                    .bInterval = HID_INTERVAL,
                },
        },
};

static void cardio_init(usbd_device* dev, FuriHalUsbInterface* intf, void* ctx);
static void cardio_deinit(usbd_device* dev);
static void cardio_on_wakeup(usbd_device* dev);
static void cardio_on_suspend(usbd_device* dev);

FuriHalUsbInterface usb_cardio = {
    .init = cardio_init,
    .deinit = cardio_deinit,
    .wakeup = cardio_on_wakeup,
    .suspend = cardio_on_suspend,

    .dev_descr = &cardio_device_desc,

    .str_manuf_descr = NULL,
    .str_prod_descr = NULL,
    .str_serial_descr = NULL,

    .cfg_descr = (void*)&cardio_cfg_desc,
};

static usbd_respond cardio_ep_config(usbd_device* dev, uint8_t cfg);
static usbd_respond
    cardio_control(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* callback);

static usbd_device* usb_dev;
static FuriSemaphore* cardio_semaphore = NULL;
static bool cardio_connected = false;
static HidStateCallback callback;
static void* callback_ctx;

bool cardio_is_connected(void) {
    return cardio_connected;
}

bool cardio_send_report(CardioReportId report_id, const uint8_t value[8]) {
    if((cardio_semaphore == NULL) || !cardio_connected) return false;

    FuriStatus status = furi_semaphore_acquire(cardio_semaphore, HID_INTERVAL * 2);
    if(status == FuriStatusErrorTimeout) return false;
    furi_check(status == FuriStatusOk);

    if(!cardio_connected) return false;

    uint8_t data[9];
    data[0] = report_id;
    memcpy(data + 1, value, 8);
    usbd_ep_write(usb_dev, HID_EP_IN, data, sizeof(data));

    return true;
}

void cardio_set_state_callback(HidStateCallback cb, void* ctx) {
    if(callback != NULL && cardio_connected) {
        callback(false, callback_ctx);
    }

    callback = cb;
    callback_ctx = ctx;

    if(callback != NULL && cardio_connected) {
        callback(true, callback_ctx);
    }
}

static void* set_string_descr(const char* str) {
    furi_assert(str);

    size_t len = strlen(str);
    struct usb_string_descriptor* dev_str_desc = malloc(len * 2 + 2);
    dev_str_desc->bLength = len * 2 + 2;
    dev_str_desc->bDescriptorType = USB_DTYPE_STRING;
    for(size_t i = 0; i < len; i++)
        dev_str_desc->wString[i] = str[i];

    return dev_str_desc;
}

static void cardio_init(usbd_device* dev, FuriHalUsbInterface* intf, void* ctx) {
    UNUSED(intf);
    CardioUsbHidConfig* cfg = (CardioUsbHidConfig*)ctx;

    if(cardio_semaphore == NULL) {
        cardio_semaphore = furi_semaphore_alloc(1, 1);
    }
    usb_dev = dev;

    const char* str_serial_descr = String_Serial_Player1;
    if(cfg != NULL) {
        switch(cfg->player_id) {
        case CardioPlayerId1:
            str_serial_descr = String_Serial_Player1;
            break;
        case CardioPlayerId2:
            str_serial_descr = String_Serial_Player2;
            break;
        }
    }

    usb_cardio.str_manuf_descr = set_string_descr(String_Manufacturer);
    usb_cardio.str_prod_descr = set_string_descr(String_Product);
    usb_cardio.str_serial_descr = set_string_descr(str_serial_descr);

    usbd_reg_config(dev, cardio_ep_config);
    usbd_reg_control(dev, cardio_control);

    usbd_connect(dev, true);
}

static void cardio_deinit(usbd_device* dev) {
    usbd_reg_config(dev, NULL);
    usbd_reg_control(dev, NULL);

    free(usb_cardio.str_manuf_descr);
    free(usb_cardio.str_prod_descr);
    free(usb_cardio.str_serial_descr);
}

static void cardio_on_wakeup(usbd_device* dev) {
    UNUSED(dev);
    if(!cardio_connected) {
        cardio_connected = true;
        if(callback != NULL) {
            callback(true, callback_ctx);
        }
    }
}

static void cardio_on_suspend(usbd_device* dev) {
    UNUSED(dev);
    if(cardio_connected) {
        cardio_connected = false;
        furi_semaphore_release(cardio_semaphore);
        if(callback != NULL) {
            callback(false, callback_ctx);
        }
    }
}

static void cardio_txrx_ep_callback(usbd_device* dev, uint8_t event, uint8_t ep) {
    UNUSED(dev);
    UNUSED(ep);
    if(event == usbd_evt_eptx) {
        furi_semaphore_release(cardio_semaphore);
    }
}

// configure endpoints
static usbd_respond cardio_ep_config(usbd_device* dev, uint8_t cfg) {
    switch(cfg) {
    case 0:
        // deconfiguring device
        usbd_ep_deconfig(dev, HID_EP_IN);
        usbd_reg_endpoint(dev, HID_EP_IN, 0);
        return usbd_ack;

    case 1:
        // configuring device
        usbd_ep_config(dev, HID_EP_IN, USB_EPTYPE_INTERRUPT, HID_EP_SIZE);
        usbd_reg_endpoint(dev, HID_EP_IN, cardio_txrx_ep_callback);
        usbd_ep_write(dev, HID_EP_IN, 0, 0);
        return usbd_ack;

    default:
        return usbd_fail;
    }
}

// control request handler
static usbd_respond
    cardio_control(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* callback) {
    UNUSED(callback);
    // HID control requests
    if(((USB_REQ_RECIPIENT | USB_REQ_TYPE) & req->bmRequestType) ==
           (USB_REQ_INTERFACE | USB_REQ_CLASS) &&
       req->wIndex == 0) {
        switch(req->bRequest) {
        case USB_HID_SETIDLE:
            return usbd_ack;
        case USB_HID_GETREPORT:
            // dev->status.data_ptr = &hid_report;
            // dev->status.data_count = sizeof(hid_report);
            return usbd_ack;
        default:
            return usbd_fail;
        }
    }
    if(((USB_REQ_RECIPIENT | USB_REQ_TYPE) & req->bmRequestType) ==
           (USB_REQ_INTERFACE | USB_REQ_STANDARD) &&
       req->wIndex == 0 && req->bRequest == USB_STD_GET_DESCRIPTOR) {
        switch(req->wValue >> 8) {
        case USB_DTYPE_HID:
            dev->status.data_ptr = (uint8_t*)&(cardio_cfg_desc.intf_0.hid_desc);
            dev->status.data_count = sizeof(cardio_cfg_desc.intf_0.hid_desc);
            return usbd_ack;
        case USB_DTYPE_HID_REPORT:
            dev->status.data_ptr = (uint8_t*)cardio_report_desc;
            dev->status.data_count = sizeof(cardio_report_desc);
            return usbd_ack;
        default:
            return usbd_fail;
        }
    }
    return usbd_fail;
}
