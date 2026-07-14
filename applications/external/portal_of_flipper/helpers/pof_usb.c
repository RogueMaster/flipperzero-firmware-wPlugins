#include "pof_usb.h"

#define TAG "POF USB"

#define HID_INTERVAL 1

#define USB_EP0_SIZE 8

#define POF_USB_VID (0x1430)
#define POF_USB_PID (0x0150)

#define POF_USB_EP_IN  (0x81)
#define POF_USB_EP_OUT (0x02)

#define POF_USB_ACTUAL_OUTPUT_SIZE 0x20

static const struct usb_string_descriptor dev_manuf_desc =
    USB_ARRAY_DESC(0x41, 0x63, 0x74, 0x69, 0x76, 0x69, 0x73, 0x69, 0x6f, 0x6e, 0x00);
static const struct usb_string_descriptor dev_product_desc =
    USB_ARRAY_DESC(0x53, 0x70, 0x79, 0x72, 0x6f, 0x20, 0x50, 0x6f, 0x72, 0x74, 0x61, 0x00);

static const uint8_t hid_report_desc[] = {0x06, 0x00, 0xFF, 0x09, 0x01, 0xA1, 0x01, 0x19,
                                          0x01, 0x29, 0x40, 0x15, 0x00, 0x26, 0xFF, 0x00,
                                          0x75, 0x08, 0x95, 0x20, 0x81, 0x00, 0x19, 0x01,
                                          0x29, 0x40, 0x91, 0x00, 0xC0};

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
    uint32_t timeout = TIMEOUT_NORMAL; // FuriWaitForever; //ms
    uint32_t last = 0;

    while(true) {
        uint32_t now = furi_get_tick();
        uint32_t flags = furi_thread_flags_wait(EventAll, FuriFlagWaitAny, timeout);
        if(flags & EventRx) { // fast flag
            if(virtual_portal->speaker) {
                uint8_t buf[POF_USB_RX_MAX_SIZE];
                len_data = pof_usb_receive(dev, buf, POF_USB_RX_MAX_SIZE);
                // https://github.com/xMasterX/all-the-plugins/blob/dev/base_pack/wav_player/wav_player_hal.c
                if(len_data > 0) {
                    /*
                    FURI_LOG_RAW_I("pof_usb_receive: ");
                    for(uint32_t i = 0; i < len_data; i++) {
                        FURI_LOG_RAW_I("%02x", buf[i]);
                    }
                    FURI_LOG_RAW_I("\r\n");
                    */
                    virtual_portal_process_audio(virtual_portal, buf, len_data);
                }
            }
            if(pof_usb->dataAvailable > 0) {
                memset(tx_data, 0, sizeof(tx_data));
                int send_len =
                    virtual_portal_process_message(virtual_portal, pof_usb->data, tx_data);
                if(send_len > 0) {
                    pof_usb_send(dev, tx_data, POF_USB_ACTUAL_OUTPUT_SIZE);
                    timeout = TIMEOUT_AFTER_RESPONSE;
                    if(virtual_portal->speaker) {
                        timeout = TIMEOUT_AFTER_MUSIC;
                    }
                    last = now;
                }
                pof_usb->dataAvailable = 0;
            }

            // Check next status time since the timeout based one might be starved by incoming packets.
            if(now > last + timeout) {
                memset(tx_data, 0, sizeof(tx_data));
                len_data = virtual_portal_send_status(virtual_portal, tx_data);
                if(len_data > 0) {
                    pof_usb_send(dev, tx_data, POF_USB_ACTUAL_OUTPUT_SIZE);
                }
                last = now;
                timeout = TIMEOUT_NORMAL;
                if(virtual_portal->speaker) {
                    timeout = TIMEOUT_AFTER_MUSIC;
                }
            }

            flags &= ~EventRx; // clear flag
        }

        if(flags) {
            if(flags & EventResetSio) {
            }
            if(flags & EventTxComplete) {
                pof_usb->tx_complete = true;
            }

            if(flags & EventTxImmediate) {
                pof_usb->tx_immediate = true;
                if(pof_usb->tx_complete) {
                    flags |= EventTx;
                }
            }

            if(flags & EventTx) {
                pof_usb->tx_complete = false;
                pof_usb->tx_immediate = false;
            }

            if(flags & EventExit) {
                FURI_LOG_I(TAG, "exit");
                break;
            }
        }

        if(flags == (uint32_t)FuriFlagErrorISR) { // timeout
            memset(tx_data, 0, sizeof(tx_data));
            len_data = virtual_portal_send_status(virtual_portal, tx_data);
            if(len_data > 0) {
                pof_usb_send(dev, tx_data, POF_USB_ACTUAL_OUTPUT_SIZE);
            }
            last = now;
            timeout = TIMEOUT_NORMAL;
            if(virtual_portal->speaker) {
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
    if(!pof_usb || pof_usb->dev != dev) {
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
    if(!pof_usb || pof_usb->dev != dev) return;
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
    switch(cfg) {
    case 0: // deconfig
        usbd_ep_deconfig(dev, POF_USB_EP_OUT);
        usbd_ep_deconfig(dev, POF_USB_EP_IN);
        usbd_reg_endpoint(dev, POF_USB_EP_OUT, NULL);
        usbd_reg_endpoint(dev, POF_USB_EP_IN, NULL);
        return usbd_ack;
    case 1: // config
        usbd_ep_config(dev, POF_USB_EP_IN, USB_EPTYPE_INTERRUPT, POF_USB_EP_IN_SIZE);
        usbd_ep_config(dev, POF_USB_EP_OUT, USB_EPTYPE_INTERRUPT, POF_USB_EP_OUT_SIZE);
        usbd_reg_endpoint(dev, POF_USB_EP_IN, pof_usb_tx_ep_callback);
        usbd_reg_endpoint(dev, POF_USB_EP_OUT, pof_usb_rx_ep_callback);
        return usbd_ack;
    }
    return usbd_fail;
}

struct PoFUsbDescriptor {
    struct usb_config_descriptor config;
    struct usb_interface_descriptor intf;
    struct usb_hid_descriptor hid_desc;
    struct usb_endpoint_descriptor ep_in;
    struct usb_endpoint_descriptor ep_out;
} __attribute__((packed));

static const struct usb_device_descriptor usb_pof_dev_descr = {
    .bLength = sizeof(struct usb_device_descriptor),
    .bDescriptorType = USB_DTYPE_DEVICE,
    .bcdUSB = VERSION_BCD(2, 0, 0),
    .bDeviceClass = USB_CLASS_PER_INTERFACE,
    .bDeviceSubClass = USB_SUBCLASS_NONE,
    .bDeviceProtocol = USB_PROTO_NONE,
    .bMaxPacketSize0 = USB_EP0_SIZE,
    .idVendor = POF_USB_VID,
    .idProduct = POF_USB_PID,
    .bcdDevice = VERSION_BCD(1, 0, 0),
    .iManufacturer = 1, // UsbDevManuf
    .iProduct = 2, // UsbDevProduct
    .iSerialNumber = 0,
    .bNumConfigurations = 1,
};

static const struct PoFUsbDescriptor usb_pof_cfg_descr = {
    .config =
        {
            .bLength = sizeof(struct usb_config_descriptor),
            .bDescriptorType = USB_DTYPE_CONFIGURATION,
            .wTotalLength = sizeof(struct PoFUsbDescriptor),
            .bNumInterfaces = 1,
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
            .bInterfaceClass = USB_CLASS_HID,
            .bInterfaceSubClass = USB_HID_SUBCLASS_NONBOOT,
            .bInterfaceProtocol = USB_HID_PROTO_NONBOOT,
            .iInterface = NO_DESCRIPTOR,
        },
    .hid_desc =
        {
            .bLength = sizeof(struct usb_hid_descriptor),
            .bDescriptorType = USB_DTYPE_HID,
            .bcdHID = VERSION_BCD(1, 1, 1),
            .bCountryCode = USB_HID_COUNTRY_NONE,
            .bNumDescriptors = 1,
            .bDescriptorType0 = USB_DTYPE_HID_REPORT,
            .wDescriptorLength0 = sizeof(hid_report_desc),
        },
    .ep_in =
        {
            .bLength = sizeof(struct usb_endpoint_descriptor),
            .bDescriptorType = USB_DTYPE_ENDPOINT,
            .bEndpointAddress = POF_USB_EP_IN,
            .bmAttributes = USB_EPTYPE_INTERRUPT,
            .wMaxPacketSize = 0x40,
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
};

/* Control requests handler */
static usbd_respond
    pof_hid_control(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* callback) {
    UNUSED(callback);
    uint8_t wValueH = req->wValue >> 8;
    uint16_t length = req->wLength;

    PoFUsb* pof_usb = pof_cur;

    /* HID control requests */
    if(((USB_REQ_RECIPIENT | USB_REQ_TYPE) & req->bmRequestType) ==
           (USB_REQ_INTERFACE | USB_REQ_CLASS) &&
       req->wIndex == 0) {
        switch(req->bRequest) {
        case USB_HID_SETIDLE:
            return usbd_ack;
        case USB_HID_SETPROTOCOL:
            return usbd_ack;
        case USB_HID_GETREPORT:
            dev->status.data_ptr = pof_usb->tx_data;
            dev->status.data_count = sizeof(pof_usb->tx_data);
            return usbd_ack;
        case USB_HID_SETREPORT:
            if(wValueH == HID_REPORT_TYPE_INPUT) {
                if(length == POF_USB_RX_MAX_SIZE) {
                    return usbd_ack;
                }
            } else if(wValueH == HID_REPORT_TYPE_OUTPUT) {
                memcpy(pof_usb->data, req->data, req->wLength);
                pof_usb->dataAvailable += req->wLength;
                furi_thread_flags_set(furi_thread_get_id(pof_usb->thread), EventRx);

                return usbd_ack;
            } else if(wValueH == HID_REPORT_TYPE_FEATURE) {
                return usbd_ack;
            }
            return usbd_fail;
        default:
            return usbd_fail;
        }
    }

    if(((USB_REQ_RECIPIENT | USB_REQ_TYPE) & req->bmRequestType) ==
           (USB_REQ_INTERFACE | USB_REQ_STANDARD) &&
       req->wIndex == 0 && req->bRequest == USB_STD_GET_DESCRIPTOR) {
        switch(wValueH) {
        case USB_DTYPE_HID:
            dev->status.data_ptr = (uint8_t*)&(usb_pof_cfg_descr.hid_desc);
            dev->status.data_count = sizeof(usb_pof_cfg_descr.hid_desc);
            return usbd_ack;
        case USB_DTYPE_HID_REPORT:
            dev->status.data_ptr = (uint8_t*)hid_report_desc;
            dev->status.data_count = sizeof(hid_report_desc);
            return usbd_ack;
        default:
            return usbd_fail;
        }
    }
    return usbd_fail;
}

PoFUsb* pof_usb_start(VirtualPortal* virtual_portal) {
    PoFUsb* pof_usb = malloc(sizeof(PoFUsb));
    pof_usb->virtual_portal = virtual_portal;
    pof_usb->dataAvailable = 0;
    furi_hal_usb_unlock();
    pof_usb->usb_prev = furi_hal_usb_get_config();
    pof_usb->usb.init = pof_usb_init;
    pof_usb->usb.deinit = pof_usb_deinit;
    pof_usb->usb.wakeup = pof_usb_wakeup;
    pof_usb->usb.suspend = pof_usb_suspend;
    pof_usb->usb.dev_descr = (struct usb_device_descriptor*)&usb_pof_dev_descr;
    pof_usb->usb.str_manuf_descr = (void*)&dev_manuf_desc;
    pof_usb->usb.str_prod_descr = (void*)&dev_product_desc;
    pof_usb->usb.str_serial_descr = NULL;
    pof_usb->usb.cfg_descr = (void*)&usb_pof_cfg_descr;

    if(!furi_hal_usb_set_config(&pof_usb->usb, pof_usb)) {
        FURI_LOG_E(TAG, "USB locked, can not start");
        if(pof_usb->usb.str_manuf_descr) {
            free(pof_usb->usb.str_manuf_descr);
        }
        if(pof_usb->usb.str_prod_descr) {
            free(pof_usb->usb.str_prod_descr);
        }
        if(pof_usb->usb.str_serial_descr) {
            free(pof_usb->usb.str_serial_descr);
        }

        free(pof_usb);
        pof_usb = NULL;
        return NULL;
    }
    return pof_usb;
}

void pof_usb_stop(PoFUsb* pof_usb) {
    if(pof_usb) {
        furi_hal_usb_set_config(pof_usb->usb_prev, NULL);
    }
}
