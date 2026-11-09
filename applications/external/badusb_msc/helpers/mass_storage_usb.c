#include "mass_storage_usb.h"
#include <furi_hal.h>
#include "combo_hid.h"
#include "combo_usb_compat.h"

#define TAG "MassStorageUsb"

#define USB_MSC_RX_EP (0x02)
#define USB_MSC_TX_EP (0x82)

#define USB_MSC_RX_EP_SIZE (64UL)
#define USB_MSC_TX_EP_SIZE (64UL)

#define USB_MSC_BOT_GET_MAX_LUN (0xFE)
#define USB_MSC_BOT_RESET       (0xFF)

#define CBW_SIG                  (0x43425355)
#define CBW_FLAGS_DEVICE_TO_HOST (0x80)

#define CSW_SIG                (0x53425355)
#define CSW_STATUS_OK          (0)
#define CSW_STATUS_NOK         (1)
#define CSW_STATUS_PHASE_ERROR (2)

// must be SCSI_BLOCK_SIZE aligned
// larger than 0x10000 exceeds size_t, storage_file_* ops fail
#define USB_MSC_BUF_MAX (0x10000UL - SCSI_BLOCK_SIZE)

static usbd_respond usb_ep_config(usbd_device* dev, uint8_t cfg);
static usbd_respond usb_control(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* callback);

typedef enum {
    EventExit = 1 << 0,
    EventReset = 1 << 1,
    EventRxTx = 1 << 2,

    EventAll = EventExit | EventReset | EventRxTx,
} MassStorageEvent;

typedef struct {
    uint32_t sig;
    uint32_t tag;
    uint32_t len;
    uint8_t flags;
    uint8_t lun;
    uint8_t cmd_len;
    uint8_t cmd[16];
} __attribute__((packed)) CBW;

typedef struct {
    uint32_t sig;
    uint32_t tag;
    uint32_t residue;
    uint8_t status;
} __attribute__((packed)) CSW;

struct MassStorageUsb {
    FuriHalUsbInterface usb;
    FuriHalUsbInterface* usb_prev;
    struct usb_device_descriptor device_descriptor;

    FuriThread* thread;
    usbd_device* dev;
    SCSIDeviceFunc fn;

    bool connected;
    MassStorageUsbConnectionStatusCallback status_cb;
    void* status_cb_context;
};

static int32_t mass_thread_worker(void* context) {
    MassStorageUsb* mass = context;
    usbd_device* dev = mass->dev;
    SCSISession scsi = {
        .fn = mass->fn,
    };
    CBW cbw = {0};
    CSW csw = {0};
    uint8_t* buf = NULL;
    uint32_t buf_len = 0, buf_cap = 0, buf_sent = 0;
    enum {
        StateReadCBW,
        StateReadData,
        StateWriteData,
        StateBuildCSW,
        StateWriteCSW,
    } state = StateReadCBW;
    while(true) {
        uint32_t flags = furi_thread_flags_wait(EventAll, FuriFlagWaitAny, FuriWaitForever);
        if(flags & EventExit) {
            FURI_LOG_D(TAG, "exit");
            break;
        }
        if(flags & EventReset) {
            FURI_LOG_D(TAG, "reset");
            scsi.sk = 0;
            scsi.asc = 0;
            memset(&cbw, 0, sizeof(cbw));
            memset(&csw, 0, sizeof(csw));
            if(buf) {
                free(buf);
                buf = NULL;
            }
            buf_len = buf_cap = buf_sent = 0;
            state = StateReadCBW;
        }
        if(flags & EventRxTx) do {
                switch(state) {
                case StateReadCBW: {
                    FURI_LOG_T(TAG, "StateReadCBW");
                    int32_t len = usbd_ep_read(dev, USB_MSC_RX_EP, &cbw, sizeof(cbw));
                    if(len <= 0) {
                        FURI_LOG_T(TAG, "cbw not ready");
                        break;
                    }
                    if(!mass->connected) mass->connected = true;
                    if(len != sizeof(cbw) || cbw.sig != CBW_SIG) {
                        FURI_LOG_W(TAG, "bad cbw sig=%08lx", cbw.sig);
                        usbd_ep_stall(dev, USB_MSC_TX_EP);
                        usbd_ep_stall(dev, USB_MSC_RX_EP);
                        continue;
                    }
                    if(!scsi_cmd_start(&scsi, cbw.cmd, cbw.cmd_len)) {
                        FURI_LOG_W(TAG, "bad cmd");
                        usbd_ep_stall(dev, USB_MSC_RX_EP);
                        csw.sig = CSW_SIG;
                        csw.tag = cbw.tag;
                        csw.status = CSW_STATUS_NOK;
                        state = StateWriteCSW;
                        continue;
                    }
                    if(cbw.flags & CBW_FLAGS_DEVICE_TO_HOST) {
                        buf_len = 0;
                        buf_sent = 0;
                        state = StateWriteData;
                    } else {
                        buf_len = 0;
                        state = StateReadData;
                    }
                    continue;
                }; break;
                case StateReadData: {
                    FURI_LOG_T(TAG, "StateReadData %lu/%lu", buf_len, cbw.len);
                    if(!cbw.len) {
                        state = StateBuildCSW;
                        continue;
                    }
                    uint32_t buf_clamp = MIN(cbw.len, USB_MSC_BUF_MAX);
                    if(buf_clamp > buf_cap) {
                        FURI_LOG_T(TAG, "growing buf %lu -> %lu", buf_cap, buf_clamp);
                        if(buf) {
                            free(buf);
                        }
                        buf_cap = buf_clamp;
                        buf = malloc(buf_cap);
                    }
                    if(buf_len < buf_clamp) {
                        int32_t len =
                            usbd_ep_read(dev, USB_MSC_RX_EP, buf + buf_len, buf_clamp - buf_len);
                        if(len < 0) {
                            FURI_LOG_T(TAG, "rx not ready %ld", len);
                            break;
                        }
                        FURI_LOG_T(TAG, "clamp %lu len %ld", buf_clamp, len);
                        buf_len += len;
                    }
                    if(buf_len == buf_clamp) {
                        if(!scsi_cmd_rx_data(&scsi, buf, buf_len)) {
                            FURI_LOG_W(TAG, "short rx");
                            usbd_ep_stall(dev, USB_MSC_RX_EP);
                            csw.sig = CSW_SIG;
                            csw.tag = cbw.tag;
                            csw.status = CSW_STATUS_NOK;
                            csw.residue = cbw.len;
                            state = StateWriteCSW;
                            continue;
                        }
                        cbw.len -= buf_len;
                        buf_len = 0;
                    }
                    continue;
                }; break;
                case StateWriteData: {
                    FURI_LOG_T(TAG, "StateWriteData %lu", cbw.len);
                    if(!cbw.len) {
                        state = StateBuildCSW;
                        continue;
                    }
                    uint32_t buf_clamp = MIN(cbw.len, USB_MSC_BUF_MAX);
                    if(buf_clamp > buf_cap) {
                        FURI_LOG_T(TAG, "growing buf %lu -> %lu", buf_cap, buf_clamp);
                        if(buf) {
                            free(buf);
                        }
                        buf_cap = buf_clamp;
                        buf = malloc(buf_cap);
                    }
                    if(!buf_len && !scsi_cmd_tx_data(&scsi, buf, &buf_len, buf_clamp)) {
                        FURI_LOG_W(TAG, "short tx");
                        // usbd_ep_stall(dev, USB_MSC_TX_EP);
                        state = StateBuildCSW;
                        continue;
                    }
                    int32_t len = usbd_ep_write(
                        dev,
                        USB_MSC_TX_EP,
                        buf + buf_sent,
                        MIN(USB_MSC_TX_EP_SIZE, buf_len - buf_sent));
                    if(len < 0) {
                        FURI_LOG_T(TAG, "tx not ready %ld", len);
                        break;
                    }
                    buf_sent += len;
                    if(buf_sent == buf_len) {
                        cbw.len -= buf_len;
                        buf_len = 0;
                        buf_sent = 0;
                    }
                    continue;
                }; break;
                case StateBuildCSW: {
                    FURI_LOG_T(TAG, "StateBuildCSW");
                    csw.sig = CSW_SIG;
                    csw.tag = cbw.tag;
                    if(scsi_cmd_end(&scsi)) {
                        csw.status = CSW_STATUS_OK;
                    } else {
                        csw.status = CSW_STATUS_NOK;
                    }
                    csw.residue = cbw.len;
                    state = StateWriteCSW;
                    continue;
                }; break;
                case StateWriteCSW: {
                    FURI_LOG_T(TAG, "StateWriteCSW");
                    if(csw.status) {
                        FURI_LOG_W(
                            TAG,
                            "csw sig=%08lx tag=%08lx residue=%08lx status=%02x",
                            csw.sig,
                            csw.tag,
                            csw.residue,
                            csw.status);
                    }
                    int32_t len = usbd_ep_write(dev, USB_MSC_TX_EP, &csw, sizeof(csw));
                    if(len < 0) {
                        FURI_LOG_T(TAG, "csw not ready");
                        break;
                    }
                    if(len != sizeof(csw)) {
                        FURI_LOG_W(TAG, "bad csw write %ld", len);
                        usbd_ep_stall(dev, USB_MSC_TX_EP);
                        break;
                    }
                    memset(&cbw, 0, sizeof(cbw));
                    memset(&csw, 0, sizeof(csw));
                    state = StateReadCBW;
                    continue;
                }; break;
                }
                break;
            } while(true);
    }
    if(buf) {
        free(buf);
    }
    return 0;
}

// needed in usb_deinit, usb_suspend, usb_rxtx_ep_callback, usb_control,
// where if_ctx isn't passed
static MassStorageUsb* mass_cur = NULL;

static void usb_init(usbd_device* dev, FuriHalUsbInterface* intf, void* ctx) {
    UNUSED(intf);
    MassStorageUsb* mass = ctx;
    mass_cur = mass;
    mass->dev = dev;
    mass->connected = false;
    hid_init(dev, NULL, NULL);

    usbd_reg_config(dev, usb_ep_config);
    usbd_reg_control(dev, usb_control);
    usbd_connect(dev, true);

    furi_thread_start(mass->thread);
}

static void usb_deinit(usbd_device* dev) {
    usbd_reg_config(dev, NULL);
    usbd_reg_control(dev, NULL);

    MassStorageUsb* mass = mass_cur;
    if(!mass || mass->dev != dev) {
        FURI_LOG_E(TAG, "deinit mass_cur leak");
        return;
    }
    mass_cur = NULL;
    hid_deinit(dev);

    if(mass->thread) {
        furi_thread_flags_set(furi_thread_get_id(mass->thread), EventExit);
        furi_thread_join(mass->thread);
    }

    if(!mass->connected && mass->status_cb) {
        mass->status_cb(false, mass->status_cb_context);
    }
}

static void usb_wakeup(usbd_device* dev) {
    hid_on_wakeup(dev);
}

static void usb_suspend(usbd_device* dev) {
    MassStorageUsb* mass = mass_cur;
    if(!mass || mass->dev != dev) return;
    furi_thread_flags_set(furi_thread_get_id(mass->thread), EventReset);
    hid_on_suspend(dev);
}

static void usb_rxtx_ep_callback(usbd_device* dev, uint8_t event, uint8_t ep) {
    UNUSED(ep);
    UNUSED(event);
    MassStorageUsb* mass = mass_cur;
    if(!mass || mass->dev != dev) return;
    furi_thread_flags_set(furi_thread_get_id(mass->thread), EventRxTx);
}

static usbd_respond usb_ep_config(usbd_device* dev, uint8_t cfg) {
    switch(cfg) {
    case 0: // deconfig
        hid_ep_config(dev, cfg);
        usbd_ep_deconfig(dev, USB_MSC_RX_EP);
        usbd_ep_deconfig(dev, USB_MSC_TX_EP);
        usbd_reg_endpoint(dev, USB_MSC_RX_EP, NULL);
        usbd_reg_endpoint(dev, USB_MSC_TX_EP, NULL);
        return usbd_ack;
    case 1: // config
        hid_ep_config(dev, cfg);
        usbd_ep_config(
            dev, USB_MSC_RX_EP, USB_EPTYPE_BULK /* | USB_EPTYPE_DBLBUF*/, USB_MSC_RX_EP_SIZE);
        usbd_ep_config(
            dev, USB_MSC_TX_EP, USB_EPTYPE_BULK /* | USB_EPTYPE_DBLBUF*/, USB_MSC_TX_EP_SIZE);
        usbd_reg_endpoint(dev, USB_MSC_RX_EP, usb_rxtx_ep_callback);
        usbd_reg_endpoint(dev, USB_MSC_TX_EP, usb_rxtx_ep_callback);
        return usbd_ack;
    }
    return usbd_fail;
}

static usbd_respond usb_control(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* callback) {
    if((req->wIndex & 0xff) == 0) return hid_control(dev, req, callback);
    UNUSED(callback);
    if((req->wIndex & 0xff) != 1 || ((USB_REQ_RECIPIENT | USB_REQ_TYPE) & req->bmRequestType) !=
                                        (USB_REQ_INTERFACE | USB_REQ_CLASS)) {
        return usbd_fail;
    }
    switch(req->bRequest) {
    case USB_MSC_BOT_GET_MAX_LUN: {
        static uint8_t max_lun = 0;
        dev->status.data_ptr = &max_lun;
        dev->status.data_count = 1;
        return usbd_ack;
    }; break;
    case USB_MSC_BOT_RESET: {
        MassStorageUsb* mass = mass_cur;
        if(!mass || mass->dev != dev) return usbd_fail;
        furi_thread_flags_set(furi_thread_get_id(mass->thread), EventReset);
        return usbd_ack;
    }; break;
    }
    return usbd_fail;
}

static const struct usb_device_descriptor usb_mass_dev_descr = {
    .bLength = sizeof(struct usb_device_descriptor),
    .bDescriptorType = USB_DTYPE_DEVICE,
    .bcdUSB = VERSION_BCD(2, 0, 0),
    .bDeviceClass = USB_CLASS_PER_INTERFACE,
    .bDeviceSubClass = USB_SUBCLASS_NONE,
    .bDeviceProtocol = USB_PROTO_NONE,
    .bMaxPacketSize0 = 8, // USB_EP0_SIZE
    .idVendor = 0x1209,
    .idProduct = 0xF10C,
    .bcdDevice = VERSION_BCD(1, 0, 0),
    .iManufacturer = 1, // UsbDevManuf
    .iProduct = 2, // UsbDevProduct
    .iSerialNumber = 3, // UsbDevSerial
    .bNumConfigurations = 1,
};

// USB descriptors must outlive set_config; never point at a temporary config.
static struct usb_string_descriptor* mass_usb_string(const char* ascii, size_t max_len) {
    size_t len = 0;
    while(len < max_len && ascii[len])
        len++;
    struct usb_string_descriptor* desc = malloc(len * 2 + 2);
    desc->bLength = len * 2 + 2;
    desc->bDescriptorType = USB_DTYPE_STRING;
    for(size_t i = 0; i < len; i++)
        desc->wString[i] = (uint8_t)ascii[i];
    return desc;
}

MassStorageUsb* mass_storage_usb_start(
    const char* filename,
    SCSIDeviceFunc fn,
    const FuriHalUsbHidConfig* usb_cfg) {
    MassStorageUsb* mass = malloc(sizeof(MassStorageUsb));
    // Initialize callback pointers before USB can invoke init/deinit handlers.
    memset(mass, 0, sizeof(*mass));
    mass->usb_prev = furi_hal_usb_get_config();
    mass->usb.init = usb_init;
    mass->usb.deinit = usb_deinit;
    mass->usb.wakeup = usb_wakeup;
    mass->usb.suspend = usb_suspend;
    mass->device_descriptor = usb_mass_dev_descr;
    // The composite device must use its own configurable descriptor, not the
    // stock HID-only descriptor and not an immutable, hardcoded VID/PID.
    if(usb_cfg && usb_cfg->vid > 0 && usb_cfg->vid <= 0xffff && usb_cfg->pid > 0 &&
       usb_cfg->pid <= 0xffff) {
        mass->device_descriptor.idVendor = usb_cfg->vid;
        mass->device_descriptor.idProduct = usb_cfg->pid;
    }
    mass->usb.dev_descr = &mass->device_descriptor;
    mass->usb.str_prod_descr = NULL;
    mass->usb.str_serial_descr = NULL;
    mass->usb.cfg_descr = combo_usb_config_descriptor();

    const char* manufacturer = "Flipper Devices Inc.";
    const char* name = furi_hal_version_get_device_name_ptr();
    if(!name) name = "Flipper Zero";
    if(usb_cfg) {
        if(usb_cfg->manuf[0]) manufacturer = usb_cfg->manuf;
        if(usb_cfg->product[0]) name = usb_cfg->product;
    }
    // The manufacturer descriptor is allocated per USB session, just like
    // the product and serial descriptors. This keeps Config/ID effective.
    mass->usb.str_manuf_descr = mass_usb_string(manufacturer, HID_MANUF_PRODUCT_NAME_LEN - 1);
    mass->usb.str_prod_descr = mass_usb_string(name, HID_MANUF_PRODUCT_NAME_LEN - 1);
    mass->usb.str_serial_descr = mass_usb_string(filename, 63);

    mass->fn = fn;
    mass->thread = furi_thread_alloc_ex("MassStorageUsb", 1024, mass_thread_worker, mass);
    if(!furi_hal_usb_set_config(&mass->usb, mass)) {
        FURI_LOG_E(TAG, "USB locked, cannot start Mass Storage");
        furi_thread_free(mass->thread);
        free(mass->usb.str_manuf_descr);
        free(mass->usb.str_prod_descr);
        free(mass->usb.str_serial_descr);
        free(mass);
        return NULL;
    }
    return mass;
}

void mass_storage_usb_stop(MassStorageUsb* mass) {
    furi_hal_usb_set_config(mass->usb_prev, NULL);
    furi_thread_free(mass->thread);
    free(mass->usb.str_manuf_descr);
    free(mass->usb.str_prod_descr);
    free(mass->usb.str_serial_descr);
    free(mass);
}

void mass_storage_usb_set_connection_status_callback(
    MassStorageUsb* mass,
    MassStorageUsbConnectionStatusCallback cb,
    void* context) {
    mass->status_cb = cb;
    mass->status_cb_context = context;
}
