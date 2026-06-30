#include "pof_usb.h"
#include "xsm3/xsm3.h"
#include "furi_hal_random.h"

#define TAG "POF USB XBOX360"

#define HID_INTERVAL 1

#define USB_EP0_SIZE 8

#define POF_USB_VID (0x1430)
#define POF_USB_PID (0x1F17)

#define POF_USB_EP_IN (0x81)
#define POF_USB_EP_OUT (0x02)
#define POF_USB_X360_AUDIO_EP_IN1 (0x83)
#define POF_USB_X360_AUDIO_EP_OUT1 (0x04)
#define POF_USB_X360_AUDIO_EP_IN2 (0x85)
#define POF_USB_X360_AUDIO_EP_OUT2 (0x06)
#define POF_USB_X360_PLUGIN_MODULE_EP_IN (0x87)

#define POF_USB_ACTUAL_OUTPUT_SIZE 0x20

static const struct usb_string_descriptor dev_manuf_desc =
    USB_ARRAY_DESC(0x41, 0x63, 0x74, 0x69, 0x76, 0x69, 0x73, 0x69, 0x6f, 0x6e, 0x00);
static const struct usb_string_descriptor dev_product_desc =
    USB_ARRAY_DESC(0x53, 0x70, 0x79, 0x72, 0x6f, 0x20, 0x50, 0x6f, 0x72, 0x74, 0x61, 0x00);
static const struct usb_string_descriptor dev_security_desc =
    USB_ARRAY_DESC(0x58, 0x62, 0x6f, 0x78, 0x20, 0x53, 0x65, 0x63, 0x75, 0x72, 0x69, 0x74,
                   0x79, 0x20, 0x4d, 0x65, 0x74, 0x68, 0x6f, 0x64, 0x20, 0x33, 0x2c, 0x20,
                   0x56, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x20, 0x31, 0x2e, 0x30, 0x30,
                   0x2c, 0x20, 0xa9, 0x20, 0x32, 0x30, 0x30, 0x35, 0x20, 0x4d, 0x69, 0x63,
                   0x72, 0x6f, 0x73, 0x6f, 0x66, 0x74, 0x20, 0x43, 0x6f, 0x72, 0x70, 0x6f,
                   0x72, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x2e, 0x20, 0x41, 0x6c, 0x6c, 0x20,
                   0x72, 0x69, 0x67, 0x68, 0x74, 0x73, 0x20, 0x72, 0x65, 0x73, 0x65, 0x72,
                   0x76, 0x65, 0x64, 0x2e);

static usbd_respond pof_usb_ep_config(usbd_device* dev, uint8_t cfg);
static usbd_respond
pof_hid_control(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* callback);
static void pof_usb_send(usbd_device* dev, uint8_t* buf, uint16_t len);
static int32_t pof_usb_receive(usbd_device* dev, uint8_t* buf, uint16_t max_len);

static PoFUsb* pof_cur = NULL;

static int32_t pof_thread_worker(void* context) {
    PoFUsb* pof_usb = context;
    usbd_device* dev = pof_usb->dev;
    VirtualPortal* virtual_portal = pof_usb->virtual_portal;
    UNUSED(dev);

    uint32_t len_data = 0;
    uint8_t tx_data[POF_USB_TX_MAX_SIZE] = {0};
    uint32_t timeout = TIMEOUT_NORMAL;  // FuriWaitForever; //ms
    uint32_t last = 0;

    while (true) {
        uint32_t now = furi_get_tick();
        uint32_t flags = furi_thread_flags_wait(EventAll, FuriFlagWaitAny, timeout);
        if (flags & EventRx) {  // fast flag
            uint8_t buf[POF_USB_RX_MAX_SIZE];
            len_data = pof_usb_receive(dev, buf, POF_USB_RX_MAX_SIZE);
            // 360 controller packets have a header of 0x0b 0x14
            if (len_data > 0 && buf[0] == 0x0b && buf[1] == 0x14) {
                memset(tx_data, 0, sizeof(tx_data));
                // prepend packet with xinput header
                int send_len =
                    virtual_portal_process_message(virtual_portal, buf + 2, tx_data + 2);
                if (send_len > 0) {
                    tx_data[0] = 0x0b;
                    tx_data[1] = 0x14;
                    pof_usb_send(dev, tx_data, POF_USB_ACTUAL_OUTPUT_SIZE);
                    timeout = TIMEOUT_AFTER_RESPONSE;
                    last = now;
                    if (virtual_portal->speaker) {
                        timeout = TIMEOUT_AFTER_MUSIC;
                    }
                }
            } else if (len_data > 0 && buf[0] == 0x0b && buf[1] == 0x17) {
                // 360 audio packets start with 0b 17, samples start after the two byte header
                /*
                FURI_LOG_RAW_I("pof_usb_receive: ");
                for(uint32_t i = 2; i < len_data; i++) {
                    FURI_LOG_RAW_I("%02x", buf[i]);
                }
                FURI_LOG_RAW_I("\r\n");
                */
                virtual_portal_process_audio_360(virtual_portal, buf + 2, len_data - 2);
            }

            // Check next status time since the timeout based one might be starved by incoming packets.
            if (now > last + timeout) {
                memset(tx_data, 0, sizeof(tx_data));
                len_data = virtual_portal_send_status(virtual_portal, tx_data + 2);
                if (len_data > 0) {
                    tx_data[0] = 0x0b;
                    tx_data[1] = 0x14;
                    pof_usb_send(dev, tx_data, POF_USB_ACTUAL_OUTPUT_SIZE);
                }
                last = now;
                timeout = TIMEOUT_NORMAL;
                if (virtual_portal->speaker) {
                    timeout = TIMEOUT_AFTER_MUSIC;
                }
            }

            flags &= ~EventRx;  // clear flag
        }

        if (flags) {
            if (flags & EventResetSio) {
            }
            if (flags & EventTxComplete) {
                pof_usb->tx_complete = true;
            }

            if (flags & EventTxImmediate) {
                pof_usb->tx_immediate = true;
                if (pof_usb->tx_complete) {
                    flags |= EventTx;
                }
            }

            if (flags & EventTx) {
                pof_usb->tx_complete = false;
                pof_usb->tx_immediate = false;
            }

            if (flags & EventExit) {
                FURI_LOG_I(TAG, "exit");
                break;
            }
        }

        if (flags == (uint32_t)FuriFlagErrorISR) {  // timeout
            memset(tx_data, 0, sizeof(tx_data));
            len_data = virtual_portal_send_status(virtual_portal, tx_data + 2);
            if (len_data > 0) {
                tx_data[0] = 0x0b;
                tx_data[1] = 0x14;
                pof_usb_send(dev, tx_data, POF_USB_ACTUAL_OUTPUT_SIZE);
            }
            last = now;
            timeout = TIMEOUT_NORMAL;
            if (virtual_portal->speaker) {
                timeout = TIMEOUT_AFTER_MUSIC;
            }
        }
    }

    return 0;
}

static void pof_usb_init(usbd_device* dev, FuriHalUsbInterface* intf, void* ctx) {
    UNUSED(intf);
    PoFUsb* pof_usb = ctx;
    pof_cur = pof_usb;
    pof_usb->dev = dev;

    usbd_reg_config(dev, pof_usb_ep_config);
    usbd_reg_control(dev, pof_hid_control);
    UNUSED(pof_hid_control);
    usbd_connect(dev, true);

    pof_usb->thread = furi_thread_alloc();
    furi_thread_set_name(pof_usb->thread, "PoFUsb");
    furi_thread_set_stack_size(pof_usb->thread, 2 * 1024);
    furi_thread_set_context(pof_usb->thread, ctx);
    furi_thread_set_callback(pof_usb->thread, pof_thread_worker);

    furi_thread_start(pof_usb->thread);
}

static void pof_usb_deinit(usbd_device* dev) {
    usbd_reg_config(dev, NULL);
    usbd_reg_control(dev, NULL);

    PoFUsb* pof_usb = pof_cur;
    if (!pof_usb || pof_usb->dev != dev) {
        return;
    }
    pof_cur = NULL;

    furi_assert(pof_usb->thread);
    furi_thread_flags_set(furi_thread_get_id(pof_usb->thread), EventExit);
    furi_thread_join(pof_usb->thread);
    furi_thread_free(pof_usb->thread);
    pof_usb->thread = NULL;

    free(pof_usb->usb.str_prod_descr);
    pof_usb->usb.str_prod_descr = NULL;
    free(pof_usb->usb.str_serial_descr);
    pof_usb->usb.str_serial_descr = NULL;
    free(pof_usb);
}

static void pof_usb_send(usbd_device* dev, uint8_t* buf, uint16_t len) {
    // Hide frequent responses
    /*
    if(buf[0] != 'S' && buf[0] != 'J') {
        FURI_LOG_RAW_D("> ");
        for(size_t i = 0; i < len; i++) {
            FURI_LOG_RAW_D("%02x", buf[i]);
        }
        FURI_LOG_RAW_D("\r\n");
    }
    */
    usbd_ep_write(dev, POF_USB_EP_IN, buf, len);
}

static int32_t pof_usb_receive(usbd_device* dev, uint8_t* buf, uint16_t max_len) {
    int32_t len = usbd_ep_read(dev, POF_USB_EP_OUT, buf, max_len);
    return ((len < 0) ? 0 : len);
}

static void pof_usb_wakeup(usbd_device* dev) {
    UNUSED(dev);
}

static void pof_usb_suspend(usbd_device* dev) {
    PoFUsb* pof_usb = pof_cur;
    if (!pof_usb || pof_usb->dev != dev) return;
}

static void pof_usb_rx_ep_callback(usbd_device* dev, uint8_t event, uint8_t ep) {
    UNUSED(dev);
    UNUSED(event);
    UNUSED(ep);
    PoFUsb* pof_usb = pof_cur;
    furi_thread_flags_set(furi_thread_get_id(pof_usb->thread), EventRx);
}

static void pof_usb_tx_ep_callback(usbd_device* dev, uint8_t event, uint8_t ep) {
    UNUSED(dev);
    UNUSED(event);
    UNUSED(ep);
    PoFUsb* pof_usb = pof_cur;
    furi_thread_flags_set(furi_thread_get_id(pof_usb->thread), EventTxComplete);
}

static usbd_respond pof_usb_ep_config(usbd_device* dev, uint8_t cfg) {
    switch (cfg) {
        case 0:  // deconfig
            usbd_ep_deconfig(dev, POF_USB_EP_OUT);
            usbd_ep_deconfig(dev, POF_USB_EP_IN);
            usbd_reg_endpoint(dev, POF_USB_EP_OUT, NULL);
            usbd_reg_endpoint(dev, POF_USB_EP_IN, NULL);
            usbd_reg_endpoint(dev, POF_USB_X360_AUDIO_EP_IN1, NULL);
            usbd_reg_endpoint(dev, POF_USB_X360_AUDIO_EP_IN2, NULL);
            usbd_reg_endpoint(dev, POF_USB_X360_AUDIO_EP_OUT1, NULL);
            usbd_reg_endpoint(dev, POF_USB_X360_AUDIO_EP_OUT2, NULL);
            usbd_reg_endpoint(dev, POF_USB_X360_PLUGIN_MODULE_EP_IN, NULL);
            usbd_ep_deconfig(dev, POF_USB_X360_AUDIO_EP_IN1);
            usbd_ep_deconfig(dev, POF_USB_X360_AUDIO_EP_IN2);
            usbd_ep_deconfig(dev, POF_USB_X360_AUDIO_EP_OUT1);
            usbd_ep_deconfig(dev, POF_USB_X360_AUDIO_EP_OUT2);
            usbd_ep_deconfig(dev, POF_USB_X360_PLUGIN_MODULE_EP_IN);
            return usbd_ack;
        case 1:  // config
            usbd_ep_config(dev, POF_USB_EP_IN, USB_EPTYPE_INTERRUPT, POF_USB_EP_IN_SIZE);
            usbd_ep_config(dev, POF_USB_EP_OUT, USB_EPTYPE_INTERRUPT, POF_USB_EP_OUT_SIZE);
            usbd_reg_endpoint(dev, POF_USB_EP_IN, pof_usb_tx_ep_callback);
            usbd_reg_endpoint(dev, POF_USB_EP_OUT, pof_usb_rx_ep_callback);
            usbd_ep_config(dev, POF_USB_X360_AUDIO_EP_IN1, USB_EPTYPE_INTERRUPT, POF_USB_EP_IN_SIZE);
            usbd_ep_config(dev, POF_USB_X360_AUDIO_EP_IN2, USB_EPTYPE_INTERRUPT, POF_USB_EP_OUT_SIZE);
            usbd_ep_config(dev, POF_USB_X360_AUDIO_EP_OUT1, USB_EPTYPE_INTERRUPT, POF_USB_EP_IN_SIZE);
            usbd_ep_config(dev, POF_USB_X360_AUDIO_EP_OUT2, USB_EPTYPE_INTERRUPT, POF_USB_EP_OUT_SIZE);
            usbd_ep_config(dev, POF_USB_X360_PLUGIN_MODULE_EP_IN, USB_EPTYPE_INTERRUPT, POF_USB_EP_OUT_SIZE);
            return usbd_ack;
    }
    return usbd_fail;
}

struct usb_xbox_intf_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t reserved[2];
    uint8_t subtype;
    uint8_t reserved2;
    uint8_t bEndpointAddressIn;
    uint8_t bMaxDataSizeIn;
    uint8_t reserved3[5];
    uint8_t bEndpointAddressOut;
    uint8_t bMaxDataSizeOut;
    uint8_t reserved4[2];
} __attribute__((packed));

struct PoFUsbDescriptorXbox360 {
    struct usb_config_descriptor config;
    struct usb_interface_descriptor intf;
    struct usb_xbox_intf_descriptor xbox_desc;
    struct usb_endpoint_descriptor ep_in;
    struct usb_endpoint_descriptor ep_out;
    struct usb_interface_descriptor intfAudio;
    uint8_t audio_desc[0x1B];
    struct usb_endpoint_descriptor ep_in_audio1;
    struct usb_endpoint_descriptor ep_out_audio1;
    struct usb_endpoint_descriptor ep_in_audio2;
    struct usb_endpoint_descriptor ep_out_audio2;
    struct usb_interface_descriptor intfPluginModule;
    uint8_t plugin_module_desc[0x09];
    struct usb_endpoint_descriptor ep_in_plugin_module;
    struct usb_interface_descriptor intfSecurity;
    uint8_t security_desc[0x06];
} __attribute__((packed));
struct XInputVibrationCapabilities_t {
    uint8_t rid;
    uint8_t rsize;
    uint8_t padding;
    uint8_t left_motor;
    uint8_t right_motor;
    uint8_t padding_2[3];
} __attribute__((packed));

struct XInputInputCapabilities_t {
    uint8_t rid;
    uint8_t rsize;
    uint16_t buttons;
    uint8_t leftTrigger;
    uint8_t rightTrigger;
    uint16_t leftThumbX;
    uint16_t leftThumbY;
    uint16_t rightThumbX;
    uint16_t rightThumbY;
    uint8_t reserved[4];
    uint16_t flags;
} __attribute__((packed));

static const struct usb_device_descriptor usb_pof_dev_descr_xbox_360 = {
    .bLength = sizeof(struct usb_device_descriptor),
    .bDescriptorType = USB_DTYPE_DEVICE,
    .bcdUSB = VERSION_BCD(2, 0, 0),
    .bDeviceClass = 0xFF,
    .bDeviceSubClass = 0xFF,
    .bDeviceProtocol = 0xFF,
    .bMaxPacketSize0 = USB_EP0_SIZE,
    .idVendor = POF_USB_VID,
    .idProduct = POF_USB_PID,
    .bcdDevice = VERSION_BCD(1, 0, 0),
    .iManufacturer = 1,  // UsbDevManuf
    .iProduct = 2,       // UsbDevProduct
    .iSerialNumber = 0,
    .bNumConfigurations = 1,
};

static const uint8_t xbox_serial[] = {0x12, 0x14, 0x32, 0xEF};

static const struct XInputVibrationCapabilities_t XInputVibrationCapabilities = {
    rid : 0x00,
    rsize : sizeof(struct XInputVibrationCapabilities_t),
    padding : 0x00,
    left_motor : 0x00,
    right_motor : 0x00,
    padding_2 : {0x00, 0x00, 0x00}
};
static const struct XInputInputCapabilities_t XInputInputCapabilities = {
    rid : 0x00,
    rsize : sizeof(struct XInputInputCapabilities_t),
    buttons : 0x0000,
    leftTrigger : 0x00,
    rightTrigger : 0x00,
    leftThumbX : 0x0000,
    leftThumbY : 0x0000,
    rightThumbX : 0x0000,
    rightThumbY : 0x0000,
    reserved : {0x00, 0x00, 0x00, 0x00},
    flags : 0x00
};

static const struct PoFUsbDescriptorXbox360 usb_pof_cfg_descr_x360 = {
    .config =
        {
            .bLength = sizeof(struct usb_config_descriptor),
            .bDescriptorType = USB_DTYPE_CONFIGURATION,
            .wTotalLength = sizeof(struct PoFUsbDescriptorXbox360),
            .bNumInterfaces = 4,
            .bConfigurationValue = 1,
            .iConfiguration = NO_DESCRIPTOR,
            .bmAttributes = USB_CFG_ATTR_RESERVED,
            .bMaxPower = USB_CFG_POWER_MA(500),
        },
    .intf =
        {
            .bLength = sizeof(struct usb_interface_descriptor),
            .bDescriptorType = USB_DTYPE_INTERFACE,
            .bInterfaceNumber = 0,
            .bAlternateSetting = 0,
            .bNumEndpoints = 2,
            .bInterfaceClass = 0xFF,
            .bInterfaceSubClass = 0x5D,
            .bInterfaceProtocol = 0x01,
            .iInterface = NO_DESCRIPTOR,
        },
    .xbox_desc =
        {
            .bLength = sizeof(struct usb_xbox_intf_descriptor),
            .bDescriptorType = 0x21,
            .reserved = {0x10, 0x01},
            .subtype = 0x24,
            .reserved2 = 0x25,
            .bEndpointAddressIn = POF_USB_EP_IN,
            .bMaxDataSizeIn = 0x14,
            .reserved3 = {0x03, 0x03, 0x03, 0x04, 0x13},
            .bEndpointAddressOut = POF_USB_EP_OUT,
            .bMaxDataSizeOut = 0x08,
            .reserved4 = {0x03, 0x03},
        },
    .ep_in =
        {
            .bLength = sizeof(struct usb_endpoint_descriptor),
            .bDescriptorType = USB_DTYPE_ENDPOINT,
            .bEndpointAddress = POF_USB_EP_IN,
            .bmAttributes = USB_EPTYPE_INTERRUPT,
            .wMaxPacketSize = 0x20,
            .bInterval = HID_INTERVAL,
        },
    .ep_out =
        {
            .bLength = sizeof(struct usb_endpoint_descriptor),
            .bDescriptorType = USB_DTYPE_ENDPOINT,
            .bEndpointAddress = POF_USB_EP_OUT,
            .bmAttributes = USB_EPTYPE_INTERRUPT,
            .wMaxPacketSize = 0x40,
            .bInterval = HID_INTERVAL,
        },
    .intfAudio =
        {
            .bLength = sizeof(struct usb_interface_descriptor),
            .bDescriptorType = USB_DTYPE_INTERFACE,
            .bInterfaceNumber = 1,
            .bAlternateSetting = 0,
            .bNumEndpoints = 4,
            .bInterfaceClass = 0xFF,
            .bInterfaceSubClass = 0x5D,
            .bInterfaceProtocol = 0x03,
            .iInterface = NO_DESCRIPTOR,
        },
    .audio_desc =
        {0x1B, 0x21, 0x00, 0x01, 0x01, 0x01, POF_USB_X360_AUDIO_EP_IN1, 0x40, 0x01, POF_USB_X360_AUDIO_EP_OUT1,
         0x20, 0x16, POF_USB_X360_AUDIO_EP_IN2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16,
         POF_USB_X360_AUDIO_EP_OUT2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    .ep_in_audio1 =
        {
            .bLength = sizeof(struct usb_endpoint_descriptor),
            .bDescriptorType = USB_DTYPE_ENDPOINT,
            .bEndpointAddress = POF_USB_X360_AUDIO_EP_IN1,
            .bmAttributes = USB_EPTYPE_INTERRUPT,
            .wMaxPacketSize = 0x20,
            .bInterval = 2,
        },
    .ep_out_audio1 =
        {
            .bLength = sizeof(struct usb_endpoint_descriptor),
            .bDescriptorType = USB_DTYPE_ENDPOINT,
            .bEndpointAddress = POF_USB_X360_AUDIO_EP_OUT1,
            .bmAttributes = USB_EPTYPE_INTERRUPT,
            .wMaxPacketSize = 0x20,
            .bInterval = 4,
        },
    .ep_in_audio2 =
        {
            .bLength = sizeof(struct usb_endpoint_descriptor),
            .bDescriptorType = USB_DTYPE_ENDPOINT,
            .bEndpointAddress = POF_USB_X360_AUDIO_EP_IN2,
            .bmAttributes = USB_EPTYPE_INTERRUPT,
            .wMaxPacketSize = 0x20,
            .bInterval = 0x40,
        },
    .ep_out_audio2 =
        {
            .bLength = sizeof(struct usb_endpoint_descriptor),
            .bDescriptorType = USB_DTYPE_ENDPOINT,
            .bEndpointAddress = POF_USB_X360_AUDIO_EP_OUT2,
            .bmAttributes = USB_EPTYPE_INTERRUPT,
            .wMaxPacketSize = 0x20,
            .bInterval = 0x10,
        },

    .intfPluginModule =
        {
            .bLength = sizeof(struct usb_interface_descriptor),
            .bDescriptorType = USB_DTYPE_INTERFACE,
            .bInterfaceNumber = 2,
            .bAlternateSetting = 0,
            .bNumEndpoints = 1,
            .bInterfaceClass = 0xFF,
            .bInterfaceSubClass = 0x5D,
            .bInterfaceProtocol = 0x02,
            .iInterface = NO_DESCRIPTOR,
        },
    .plugin_module_desc =
        {0x09, 0x21, 0x00, 0x01, 0x01, 0x22, POF_USB_X360_PLUGIN_MODULE_EP_IN, 0x07, 0x00},
    .ep_in_plugin_module =
        {
            .bLength = sizeof(struct usb_endpoint_descriptor),
            .bDescriptorType = USB_DTYPE_ENDPOINT,
            .bEndpointAddress = POF_USB_X360_PLUGIN_MODULE_EP_IN,
            .bmAttributes = USB_EPTYPE_INTERRUPT,
            .wMaxPacketSize = 0x20,
            .bInterval = 16,
        },
    .intfSecurity =
        {
            .bLength = sizeof(struct usb_interface_descriptor),
            .bDescriptorType = USB_DTYPE_INTERFACE,
            .bInterfaceNumber = 3,
            .bAlternateSetting = 0,
            .bNumEndpoints = 0,
            .bInterfaceClass = 0xFF,
            .bInterfaceSubClass = 0xFD,
            .bInterfaceProtocol = 0x13,
            .iInterface = 4,
        },
    .security_desc =
        {0x06, 0x41, 0x00, 0x01, 0x01, 0x03},
};

uint8_t serial[0x0C];
short state = 2;  // 1 = in-progress, 2 = complete
/* Control requests handler */
static usbd_respond
pof_hid_control(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* callback) {
    UNUSED(callback);
    uint8_t wValueH = req->wValue >> 8;
    uint8_t wValueL = req->wValue & 0xFF;
    if (req->bmRequestType == 0xC0 && req->bRequest == USB_HID_GETREPORT && req->wValue == 0x0000) {
        dev->status.data_ptr = (uint8_t*)xbox_serial;
        dev->status.data_count = sizeof(xbox_serial);
        return usbd_ack;
    }
    if (req->bmRequestType == 0xC1 && req->bRequest == USB_HID_GETREPORT && req->wValue == 0x0100) {
        dev->status.data_ptr = (uint8_t*)&(XInputInputCapabilities);
        dev->status.data_count = sizeof(XInputInputCapabilities);
        return usbd_ack;
    }
    if (req->bmRequestType == 0xC1 && req->bRequest == USB_HID_GETREPORT && req->wValue == 0x0000) {
        dev->status.data_ptr = (uint8_t*)&(XInputVibrationCapabilities);
        dev->status.data_count = sizeof(XInputVibrationCapabilities);
        return usbd_ack;
    }
    if (req->bmRequestType == 0x41 && req->bRequest == 00 && (req->wValue == 0x1F || req->wValue == 0x1E)) {
        return usbd_ack;
    }
    switch (req->bRequest) {
        case 0x81:
            uint8_t serial[0x0C];
            for (size_t i = 0; i < sizeof(serial); i++) {
                serial[i] = furi_hal_random_get() & 0xFF;
            }
            xsm3_set_vid_pid(serial, POF_USB_VID, POF_USB_PID);
            xsm3_initialise_state();
            xsm3_set_identification_data(xsm3_id_data_ms_controller);
            dev->status.data_ptr = (uint8_t*)(xsm3_id_data_ms_controller);
            dev->status.data_count = sizeof(xsm3_id_data_ms_controller);
            return usbd_ack;
        case 0x82:
            xsm3_do_challenge_init((uint8_t *)dev->status.data_ptr);
            return usbd_ack;
        case 0x87:
            xsm3_do_challenge_verify((uint8_t *)dev->status.data_ptr);
            return usbd_ack;
        case 0x84:
            return usbd_ack;
        case 0x83:
            dev->status.data_ptr = (uint8_t*)(xsm3_challenge_response);
            dev->status.data_count = req->wLength;
            return usbd_ack;
        case 0x86:
            dev->status.data_ptr = (uint8_t*)&(state);
            dev->status.data_count = sizeof(state);
            return usbd_ack;
    }

    if (((USB_REQ_RECIPIENT | USB_REQ_TYPE) & req->bmRequestType) ==
            (USB_REQ_DEVICE | USB_REQ_STANDARD) &&
        req->bRequest == USB_STD_GET_DESCRIPTOR) {
        switch (wValueH) {
            case USB_DTYPE_STRING:
                if (wValueL == 4) {
                    dev->status.data_ptr = (uint8_t*)&dev_security_desc;
                    dev->status.data_count = dev_security_desc.bLength;
                    return usbd_ack;
                }
                return usbd_fail;
            default:
                return usbd_fail;
        }
    }
    return usbd_fail;
}

PoFUsb* pof_usb_start_xbox360(VirtualPortal* virtual_portal) {
    PoFUsb* pof_usb = malloc(sizeof(PoFUsb));
    pof_usb->virtual_portal = virtual_portal;
    pof_usb->dataAvailable = 0;

    furi_hal_usb_unlock();
    pof_usb->usb_prev = furi_hal_usb_get_config();
    pof_usb->usb.init = pof_usb_init;
    pof_usb->usb.deinit = pof_usb_deinit;
    pof_usb->usb.wakeup = pof_usb_wakeup;
    pof_usb->usb.suspend = pof_usb_suspend;
    pof_usb->usb.dev_descr = (struct usb_device_descriptor*)&usb_pof_dev_descr_xbox_360;
    pof_usb->usb.cfg_descr = (void*)&usb_pof_cfg_descr_x360;
    pof_usb->usb.str_manuf_descr = (void*)&dev_manuf_desc;
    pof_usb->usb.str_prod_descr = (void*)&dev_product_desc;
    pof_usb->usb.str_serial_descr = NULL;
    if (!furi_hal_usb_set_config(&pof_usb->usb, pof_usb)) {
        FURI_LOG_E(TAG, "USB locked, can not start");
        if (pof_usb->usb.str_manuf_descr) {
            free(pof_usb->usb.str_manuf_descr);
        }
        if (pof_usb->usb.str_prod_descr) {
            free(pof_usb->usb.str_prod_descr);
        }
        if (pof_usb->usb.str_serial_descr) {
            free(pof_usb->usb.str_serial_descr);
        }

        free(pof_usb);
        pof_usb = NULL;
        return NULL;
    }
    return pof_usb;
}

void pof_usb_stop_xbox360(PoFUsb* pof_usb) {
    if (pof_usb) {
        furi_hal_usb_set_config(pof_usb->usb_prev, NULL);
    }
}
