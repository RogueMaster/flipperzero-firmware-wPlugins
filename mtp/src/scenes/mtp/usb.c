#include "main.h"
#include "usb.h"
#include "usbd_core.h"
#include "usb_std.h"
#include "usb_desc.h"
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb.h>

#include "mtp.h"

AppMTP* global_mtp;
uint8_t last_ep;

typedef enum {
    EventExit = 1 << 0,
    EventReset = 1 << 1,
    EventRx = 1 << 2,
    EventTx = 1 << 3,
    EventRxOther = 1 << 4,
    EventInterrupt = 1 << 5,
    EventAll = EventExit | EventReset | EventRx | EventTx | EventRxOther | EventInterrupt,
} MTPEvent;

int32_t usb_mtp_worker(void* ctx) {
    AppMTP* mtp = ctx;
    usbd_device* dev = mtp->dev;
    uint8_t buffer[MTP_MAX_PACKET_SIZE];

    while(true) {
        MTPEvent flags = furi_thread_flags_wait(EventAll, FuriFlagWaitAny, FuriWaitForever);
        if(flags & EventExit) {
            FURI_LOG_W("MTP", "Worker thread exit");
            break;
        }

        if(flags & EventReset) {
            FURI_LOG_W("MTP", "USB reset");
            // Handle USB reset if necessary
        }

        if(flags & EventTx) {
            FURI_LOG_W("MTP", "USB Tx event");
            // Handle MTP RX/TX data here
            // Implement the logic for processing MTP requests, sending responses, etc.
            mtp->write_pending = false;
        }

        if(flags & EventRx) {
            FURI_LOG_W("MTP", "USB Rx event");

            int32_t readBytes = usbd_ep_read(dev, MTP_EP_OUT_ADDR, buffer, MTP_MAX_PACKET_SIZE);
            FURI_LOG_I("MTP", "Received %ld bytes on RX", readBytes);

            if(readBytes > 0) {
                mtp_handle_bulk(mtp, buffer, readBytes);
            }

            // Handle MTP RX/TX data here
            // Implement the logic for processing MTP requests, sending responses, etc.
        }

        if(flags & EventRxOther) {
            FURI_LOG_W("MTP", "USB Rx Other event. ep: %02x", last_ep);
        }

        if(flags & EventInterrupt) {
            FURI_LOG_W("MTP", "USB Interrupt event");
            // Handle MTP RX/TX data here
            // Implement the logic for processing MTP requests, sending responses, etc.
        }
    }

    return 0;
}

usbd_respond usb_mtp_control(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* callback) {
    UNUSED(callback);

    AppMTP* mtp = global_mtp;
    int value = -1;
    uint16_t w_index = req->wIndex;
    uint16_t w_value = req->wValue;
    uint16_t w_length = req->wLength;

    FURI_LOG_I(
        "MTP",
        "Control Request: bmRequestType=0x%02x, bRequest=0x%02x, wValue=0x%04x, wIndex=0x%04x, wLength=0x%04x",
        req->bmRequestType,
        req->bRequest,
        w_value,
        w_index,
        w_length);

    if(req->bRequest == USB_STD_GET_DESCRIPTOR) {
        if((w_value >> 8) == USB_DTYPE_STRING && (w_value & 0xff) == 0xee) {
            value = (w_length < usb_mtp_os_string_len ? w_length : usb_mtp_os_string_len);
            memcpy(req->data, usb_mtp_os_string, value);
            return usbd_ack;
        }
    } else if((req->bmRequestType & USB_REQ_TYPE) == USB_REQ_VENDOR) {
        if(req->bRequest == 1 && (req->bmRequestType & USB_EPDIR_IN) &&
           (w_index == 4 || w_index == 5)) {
            value =
                (w_length < sizeof(mtp_ext_config_desc) ? w_length : sizeof(mtp_ext_config_desc));
            memcpy(req->data, &mtp_ext_config_desc, value);
            return usbd_ack;
        }
    } else if((req->bmRequestType & USB_REQ_TYPE) == USB_REQ_CLASS) {
        value = mtp_handle_class_control(mtp, dev, req);
    }

    if(value >= 0) {
        if(value > w_length) {
            value = w_length;
        }
        usbd_ep_write(dev, 0x00, req->data, value);
        return usbd_ack;
    }

    return usbd_fail;
}

void usb_mtp_txrx(usbd_device* dev, uint8_t event, uint8_t ep) {
    UNUSED(ep);
    UNUSED(event);

    AppMTP* mtp = global_mtp;
    if(!mtp || mtp->dev != dev) return;

    if(ep == MTP_EP_OUT_ADDR) {
        furi_thread_flags_set(furi_thread_get_id(mtp->worker_thread), EventRx);
    } else if(ep == MTP_EP_IN_ADDR) {
        furi_thread_flags_set(furi_thread_get_id(mtp->worker_thread), EventTx);
    } else {
        furi_thread_flags_set(furi_thread_get_id(mtp->worker_thread), EventRxOther);
        last_ep = ep;
    }
}

void usb_mtp_interrupt(usbd_device* dev, uint8_t event, uint8_t ep) {
    UNUSED(ep);
    UNUSED(event);

    FURI_LOG_I("MTP", "USB Interrupt");

    AppMTP* mtp = global_mtp;
    if(!mtp || mtp->dev != dev) return;

    furi_thread_flags_set(furi_thread_get_id(mtp->worker_thread), EventInterrupt);
}

usbd_respond usb_mtp_ep_config(usbd_device* dev, uint8_t cfg) {
    AppMTP* mtp = global_mtp;
    FURI_LOG_I("MTP", "USB EP config: cfg=%d", cfg);

    switch(cfg) {
    case 0: // deconfigure
        FURI_LOG_I("MTP", "USB deconfigure");
        usbd_ep_deconfig(dev, MTP_EP_IN_ADDR);
        usbd_ep_deconfig(dev, MTP_EP_OUT_ADDR);
        usbd_ep_deconfig(dev, MTP_EP_INT_IN_ADDR);
        usbd_reg_endpoint(dev, MTP_EP_IN_ADDR, NULL);
        usbd_reg_endpoint(dev, MTP_EP_OUT_ADDR, NULL);
        usbd_reg_endpoint(dev, MTP_EP_INT_IN_ADDR, NULL);
        if(mtp != NULL) mtp->usb_connected = false;
        break;
    case 1: // configure
        FURI_LOG_I("MTP", "USB configure");
        usbd_ep_config(dev, MTP_EP_OUT_ADDR, USB_EPTYPE_BULK, MTP_MAX_PACKET_SIZE);
        usbd_ep_config(dev, MTP_EP_IN_ADDR, USB_EPTYPE_BULK, MTP_MAX_PACKET_SIZE);
        usbd_ep_config(dev, MTP_EP_INT_IN_ADDR, USB_EPTYPE_INTERRUPT, USB_MAX_INTERRUPT_SIZE);
        usbd_reg_endpoint(dev, MTP_EP_OUT_ADDR, usb_mtp_txrx);
        usbd_reg_endpoint(dev, MTP_EP_IN_ADDR, usb_mtp_txrx);
        usbd_reg_endpoint(dev, MTP_EP_INT_IN_ADDR, usb_mtp_interrupt);
        if(mtp != NULL) mtp->usb_connected = true;
        //usbd_ep_write(dev, MTP_MAX_PACKET_SIZE, 0, 0);
        break;
    default:
        return usbd_fail;
    }

    return usbd_ack;
}

void usb_mtp_init(usbd_device* dev, FuriHalUsbInterface* intf, void* ctx) {
    UNUSED(intf);
    if(dev == NULL) {
        FURI_LOG_E("MTP", "dev is NULL");
    }

    // Disconnect the device first.
    usbd_connect(dev, false);

    AppMTP* mtp = ctx;
    global_mtp = mtp;
    mtp->dev = dev;

    FURI_LOG_I("MTP", "Initializing MTP device");

    // Register the configuration and control handlers

    usbd_reg_config(dev, usb_mtp_ep_config);
    usbd_reg_control(dev, usb_mtp_control);
    FURI_LOG_I("MTP", "Registered configuration and control handlers");

    // Connect the device
    usbd_connect(dev, true);
    FURI_LOG_I("MTP", "Connected device");

    // Initialize worker thread
    mtp->worker_thread = furi_thread_alloc();
    furi_thread_set_name(mtp->worker_thread, "FlipperMTPUsb");
    furi_thread_set_stack_size(mtp->worker_thread, 1024);
    furi_thread_set_context(mtp->worker_thread, ctx);
    furi_thread_set_callback(mtp->worker_thread, usb_mtp_worker);
    furi_thread_start(mtp->worker_thread);
    FURI_LOG_I("MTP", "Started worker thread");
}

void usb_mtp_deinit(usbd_device* dev) {
    usbd_reg_config(dev, NULL);
    usbd_reg_control(dev, NULL);
    FURI_LOG_I("MTP", "Unregistered configuration and control handlers");

    AppMTP* mtp = global_mtp;
    if(!mtp || mtp->dev != dev) {
        FURI_LOG_E("MTP", "deinit mtp_cur leak");
        return;
    }

    global_mtp = NULL;

    furi_assert(mtp->worker_thread);
    FURI_LOG_I("MTP", "Worker thread Condition pass");

    furi_thread_flags_set(furi_thread_get_id(mtp->worker_thread), EventExit);
    furi_thread_join(mtp->worker_thread);
    furi_thread_free(mtp->worker_thread);
    mtp->worker_thread = NULL;

    mtp->usb_connected = false;
    mtp->is_working = false;

    FURI_LOG_I("MTP", "Deinit worker thread");
}

void usb_mtp_wakeup(usbd_device* dev) {
    AppMTP* mtp = global_mtp;
    if(!mtp || mtp->dev != dev) return;

    FURI_LOG_I("MTP", "USB wakeup");

    mtp->usb_connected = true;
    UNUSED(dev);
}

void usb_mtp_suspend(usbd_device* dev) {
    AppMTP* mtp = global_mtp;
    if(!mtp || mtp->dev != dev) return;

    FURI_LOG_I("MTP", "USB suspend");

    mtp->usb_connected = false;
    furi_thread_flags_set(furi_thread_get_id(mtp->worker_thread), EventReset);
}
