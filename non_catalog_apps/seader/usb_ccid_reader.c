#include "usb_ccid_reader.h"

#include <furi_hal_usb.h>
#include <usb.h>
#include <usb_std.h>
#include <usb_ccid.h>
#include <usbd_core.h>

#define TAG "SeaderUsbCcid"

/* IMPORTANT: on the Flipper, USB_LP_IRQHandler calls usbd_poll directly, so all
 * usbd endpoint/event callbacks run in ISR context. The ISR callbacks here must
 * NEVER block and must NOT use large stack buffers. All CCID message processing
 * (including the blocking SAM relay) happens on the ccid worker thread; the ISR
 * only reads packets into a stream buffer and signals the thread. */

/* Endpoints (index low nibble must be unique across in/out). */
#define CCID_EP_OUT      0x01 /* bulk OUT (host -> reader) */
#define CCID_EP_IN       0x82 /* bulk IN  (reader -> host) */
#define CCID_EP_INT      0x83 /* interrupt IN (slot change) */
#define CCID_BULK_EPSIZE 64
#define CCID_INT_EPSIZE  8

#define STR_MAX_CHARS 31
#define STR_BUF_SIZE  (2u + 2u * STR_MAX_CHARS)

#define CCID_RX_STREAM_SIZE (SEADER_CCID_MSG_MAX + 128u)

#define CCID_FLAG_RX      (1u << 0)
#define CCID_FLAG_TX_DONE (1u << 1)
#define CCID_FLAG_STOP    (1u << 2)
#define CCID_FLAG_ALL     (CCID_FLAG_RX | CCID_FLAG_TX_DONE | CCID_FLAG_STOP)

#define CCID_TX_TIMEOUT_MS 2000u

/* Full config descriptor: config + interface(3 EP) + CCID functional + 3 EPs. */
struct CcidConfigDescriptor {
    struct usb_config_descriptor config;
    struct usb_interface_descriptor intf;
    struct usb_ccid_descriptor ccid;
    struct usb_endpoint_descriptor ep_bulk_in;
    struct usb_endpoint_descriptor ep_bulk_out;
    struct usb_endpoint_descriptor ep_int_in;
} __attribute__((packed));

static const struct CcidConfigDescriptor ccid_cfg_desc = {
    .config =
        {
            .bLength = sizeof(struct usb_config_descriptor),
            .bDescriptorType = USB_DTYPE_CONFIGURATION,
            .wTotalLength = sizeof(struct CcidConfigDescriptor),
            .bNumInterfaces = 1,
            .bConfigurationValue = 1,
            .iConfiguration = 0,
            .bmAttributes = USB_CFG_ATTR_RESERVED | USB_CFG_ATTR_SELFPOWERED,
            .bMaxPower = 0xFA, /* 500 mA */
        },
    .intf =
        {
            .bLength = sizeof(struct usb_interface_descriptor),
            .bDescriptorType = USB_DTYPE_INTERFACE,
            .bInterfaceNumber = 0,
            .bAlternateSetting = 0,
            .bNumEndpoints = 3,
            .bInterfaceClass = USB_CLASS_CCID,
            .bInterfaceSubClass = USB_CCID_SUBCLASS,
            .bInterfaceProtocol = USB_CCID_PROTO_CCID,
            .iInterface = 0,
        },
    .ccid =
        {
            .bLength = sizeof(struct usb_ccid_descriptor),
            .bDescriptorType = USB_DTYPE_CCID_FUNCTIONAL,
            .bcdCCID = CCID_CURRENT_SPEC_RELEASE_NUMBER,
            .bMaxSlotIndex = 0x00,
            .bVoltageSupport = 0x07, /* 5V | 3V | 1.8V */
            .dwProtocols = 0x00000003, /* T=0 | T=1 (spec bit0=T0, bit1=T1) */
            /* Clock/data-rate matched to a genuine ACR39U. dwDataRate must be
               consistent with dwDefaultClock at the initial ETU (clock/372). */
            .dwDefaultClock = 0x000012C0, /* 4.8 MHz */
            .dwMaximumClock = 0x000012C0,
            .bNumClockSupported = 0,
            .dwDataRate = 0x00003267, /* 12903 bps */
            .dwMaxDataRate = 0x000C9A90, /* 826000 bps */
            .bNumDataRatesSupported = 0,
            .dwMaxIFSD = 0x000000F7, /* 247 */
            .dwSynchProtocols = 0,
            .dwMechanical = 0,
            /* Genuine ACR39U dwFeatures is 0x000107BA (TPDU level). We use the
               short+extended APDU level (0x40000): at short-APDU level (0x20000)
               WUDF caps the R-APDU buffer at 258 bytes, so key-scan replies of
               264 bytes overflow it (SCardTransmit 0x0000050B). Extended level
               gives WUDF an extended-sized response buffer. WUDF still corrupts
               the class byte of EXTENDED APDUs (A0->00) and re-encodes their
               length to short at every exchange level on some Windows builds;
               sam_reader.c rebuilds the extended PUT DATA commands to
               compensate. */
            .dwFeatures = 0x000407BA,
            .dwMaxCCIDMessageLength = SEADER_CCID_MSG_MAX,
            .bClassGetResponse = 0xFF,
            .bClassEnvelope = 0xFF,
            .wLcdLayout = 0x0000,
            .bPINSupport = 0x00,
            .bMaxCCIDBusySlots = 0x01,
        },
    .ep_bulk_in =
        {
            .bLength = sizeof(struct usb_endpoint_descriptor),
            .bDescriptorType = USB_DTYPE_ENDPOINT,
            .bEndpointAddress = CCID_EP_IN,
            .bmAttributes = USB_EPTYPE_BULK,
            .wMaxPacketSize = CCID_BULK_EPSIZE,
            .bInterval = 0,
        },
    .ep_bulk_out =
        {
            .bLength = sizeof(struct usb_endpoint_descriptor),
            .bDescriptorType = USB_DTYPE_ENDPOINT,
            .bEndpointAddress = CCID_EP_OUT,
            .bmAttributes = USB_EPTYPE_BULK,
            .wMaxPacketSize = CCID_BULK_EPSIZE,
            .bInterval = 0,
        },
    .ep_int_in =
        {
            .bLength = sizeof(struct usb_endpoint_descriptor),
            .bDescriptorType = USB_DTYPE_ENDPOINT,
            .bEndpointAddress = CCID_EP_INT,
            .bmAttributes = USB_EPTYPE_INTERRUPT,
            .wMaxPacketSize = CCID_INT_EPSIZE,
            .bInterval = 16,
        },
};

typedef struct {
    usbd_device* dev;
    SeaderCcidReaderConfig cfg;
    FuriHalUsbInterface* prev;

    struct usb_device_descriptor dev_descr;
    uint8_t str_manuf[STR_BUF_SIZE];
    uint8_t str_prod[STR_BUF_SIZE];
    uint8_t str_serial[STR_BUF_SIZE];
    FuriHalUsbInterface iface;

    /* Worker thread + ISR->thread plumbing. */
    FuriThread* thread;
    FuriThreadId thread_id;
    FuriStreamBuffer* rx_stream;
    volatile bool running;
    volatile bool configured;

    /* All owned by the worker thread. */
    uint8_t rx_msg[SEADER_CCID_MSG_MAX]; /* reassembled CCID message */
    uint16_t rx_msg_len;
    uint8_t tx_buf[10 + SEADER_CCID_MAX_RESP]; /* response being sent */
    uint8_t xfr_resp[SEADER_CCID_MAX_RESP]; /* SAM response scratch */

    /* Diagnostics (read cross-thread by the UI). */
    volatile uint8_t last_cmd;
    volatile uint32_t cmd_count;
    volatile uint16_t dbg_atr_len;
    volatile int32_t dbg_tx_last;
} UsbCcid;

static UsbCcid* g_ccid = NULL;

/* -------------------------------------------------------------------------- */

static void ccid_build_string(uint8_t* buf, const char* s) {
    size_t n = strlen(s);
    if(n > STR_MAX_CHARS) n = STR_MAX_CHARS;
    buf[0] = (uint8_t)(2u + 2u * n);
    buf[1] = USB_DTYPE_STRING;
    for(size_t i = 0; i < n; i++) {
        buf[2 + 2 * i] = (uint8_t)s[i];
        buf[2 + 2 * i + 1] = 0x00;
    }
}

/* -------- TX from the worker thread (blocking-safe, packetized) -------- */

static bool ccid_send(const uint8_t* data, uint16_t len) {
    uint16_t pos = 0;
    do {
        uint16_t chunk = len - pos;
        if(chunk > CCID_BULK_EPSIZE) chunk = CCID_BULK_EPSIZE;
        furi_thread_flags_clear(CCID_FLAG_TX_DONE);
        int32_t w = usbd_ep_write(g_ccid->dev, CCID_EP_IN, data + pos, chunk);
        g_ccid->dbg_tx_last = w;
        if(w < 0) {
            /* EP momentarily busy: wait for the in-flight packet to drain. */
            uint32_t f = furi_thread_flags_wait(
                CCID_FLAG_TX_DONE | CCID_FLAG_STOP, FuriFlagWaitAny, CCID_TX_TIMEOUT_MS);
            if((f & FuriFlagError) || (f & CCID_FLAG_STOP)) return false;
            continue;
        }
        pos += (uint16_t)w;
        uint32_t f = furi_thread_flags_wait(
            CCID_FLAG_TX_DONE | CCID_FLAG_STOP, FuriFlagWaitAny, CCID_TX_TIMEOUT_MS);
        if((f & FuriFlagError) || (f & CCID_FLAG_STOP)) return false;
    } while(pos < len);

    /* A transfer that is a whole multiple of the packet size needs a ZLP so the
       host knows it ended. */
    if(len > 0 && (len % CCID_BULK_EPSIZE) == 0) {
        furi_thread_flags_clear(CCID_FLAG_TX_DONE);
        usbd_ep_write(g_ccid->dev, CCID_EP_IN, NULL, 0);
        furi_thread_flags_wait(
            CCID_FLAG_TX_DONE | CCID_FLAG_STOP, FuriFlagWaitAny, CCID_TX_TIMEOUT_MS);
    }
    return true;
}

static void ccid_send_data_block(uint8_t seq, const uint8_t* data, uint16_t len) {
    if(len > SEADER_CCID_MAX_RESP) len = SEADER_CCID_MAX_RESP;
    uint8_t* p = g_ccid->tx_buf;
    p[0] = RDR_TO_PC_DATABLOCK; /* 0x80 */
    p[1] = (uint8_t)(len & 0xFF);
    p[2] = (uint8_t)((len >> 8) & 0xFF);
    p[3] = 0;
    p[4] = 0;
    p[5] = 0; /* bSlot */
    p[6] = seq;
    p[7] = 0x00; /* bStatus: ICC present+active, cmd OK */
    p[8] = 0x00; /* bError */
    p[9] = 0x00; /* bChainParameter */
    if(len) memcpy(p + 10, data, len);
    ccid_send(p, (uint16_t)(10 + len));
}

static void ccid_send_slot_status(uint8_t seq) {
    uint8_t* p = g_ccid->tx_buf;
    p[0] = RDR_TO_PC_SLOTSTATUS; /* 0x81 */
    memset(p + 1, 0, 9);
    p[6] = seq;
    ccid_send(p, 10);
}

static void
    ccid_send_parameters(uint8_t seq, uint8_t proto, const uint8_t* params, uint8_t nparams) {
    if(nparams > 7) nparams = 7;
    uint8_t* p = g_ccid->tx_buf;
    p[0] = RDR_TO_PC_PARAMETERS; /* 0x82 */
    p[1] = nparams;
    p[2] = 0;
    p[3] = 0;
    p[4] = 0;
    p[5] = 0;
    p[6] = seq;
    p[7] = 0x00; /* bStatus */
    p[8] = 0x00; /* bError */
    p[9] = proto ? proto : 0x01; /* bProtocolNum (default T=1) */
    if(nparams) {
        memcpy(p + 10, params, nparams);
    } else {
        /* Default T=1 parameter block. */
        static const uint8_t t1[7] = {0x11, 0x10, 0x00, 0x45, 0x00, 0xFE, 0x00};
        memcpy(p + 10, t1, 7);
        p[1] = 7;
        p[9] = 0x01;
        nparams = 7;
    }
    ccid_send(p, (uint16_t)(10 + nparams));
}

/* -------- Process one complete CCID message (worker thread) -------- */

static void ccid_process_message(const uint8_t* msg, uint16_t total) {
    uint8_t type = msg[0];
    uint32_t dwLength = (uint32_t)msg[1] | ((uint32_t)msg[2] << 8) | ((uint32_t)msg[3] << 16) |
                        ((uint32_t)msg[4] << 24);
    uint8_t seq = msg[6];
    UNUSED(total);

    g_ccid->last_cmd = type;
    g_ccid->cmd_count++;

    switch(type) {
    case PC_TO_RDR_ICCPOWERON: {
        uint16_t atr_len = 0;
        /* Reuse tx_buf tail as scratch for the ATR, then send. */
        uint8_t* atr = g_ccid->xfr_resp;
        if(g_ccid->cfg.get_atr) g_ccid->cfg.get_atr(g_ccid->cfg.ctx, atr, &atr_len);
        g_ccid->dbg_atr_len = atr_len;
        ccid_send_data_block(seq, atr, atr_len);
        break;
    }
    case PC_TO_RDR_ICCPOWEROFF:
    case PC_TO_RDR_GETSLOTSTATUS:
        ccid_send_slot_status(seq);
        break;
    case PC_TO_RDR_XFRBLOCK: {
        const uint8_t* apdu = msg + 10;
        uint16_t apdu_len = (uint16_t)dwLength;
        uint16_t resp_len = 0;
        if(g_ccid->cfg.xfr &&
           g_ccid->cfg.xfr(g_ccid->cfg.ctx, apdu, apdu_len, g_ccid->xfr_resp, &resp_len)) {
            ccid_send_data_block(seq, g_ccid->xfr_resp, resp_len);
        } else {
            uint8_t sw[2] = {0x6F, 0x00};
            ccid_send_data_block(seq, sw, 2);
        }
        break;
    }
    case PC_TO_RDR_SETPARAMETERS:
        ccid_send_parameters(seq, msg[7], msg + 10, (uint8_t)dwLength);
        break;
    case PC_TO_RDR_GETPARAMETERS:
    case PC_TO_RDR_RESETPARAMETERS:
        ccid_send_parameters(seq, 0x01, NULL, 0);
        break;
    default:
        ccid_send_slot_status(seq);
        break;
    }
}

static void ccid_drain_and_process(void) {
    for(;;) {
        size_t space = sizeof(g_ccid->rx_msg) - g_ccid->rx_msg_len;
        size_t got = 0;
        if(space > 0) {
            got = furi_stream_buffer_receive(
                g_ccid->rx_stream, g_ccid->rx_msg + g_ccid->rx_msg_len, space, 0);
            g_ccid->rx_msg_len += (uint16_t)got;
        }

        bool progressed = false;
        while(g_ccid->rx_msg_len >= 10) {
            uint32_t dwLength = (uint32_t)g_ccid->rx_msg[1] | ((uint32_t)g_ccid->rx_msg[2] << 8) |
                                ((uint32_t)g_ccid->rx_msg[3] << 16) |
                                ((uint32_t)g_ccid->rx_msg[4] << 24);
            uint32_t expected = 10u + dwLength;
            if(expected > sizeof(g_ccid->rx_msg)) {
                g_ccid->rx_msg_len = 0; /* oversized/garbage: resync */
                break;
            }
            if(g_ccid->rx_msg_len < expected) break;

            ccid_process_message(g_ccid->rx_msg, (uint16_t)expected);
            uint16_t remain = g_ccid->rx_msg_len - (uint16_t)expected;
            if(remain) memmove(g_ccid->rx_msg, g_ccid->rx_msg + expected, remain);
            g_ccid->rx_msg_len = remain;
            progressed = true;
        }

        if(got == 0 && !progressed) break;
    }
}

static int32_t ccid_worker(void* context) {
    UNUSED(context);
    while(g_ccid->running) {
        uint32_t flags = furi_thread_flags_wait(
            CCID_FLAG_RX | CCID_FLAG_STOP, FuriFlagWaitAny, FuriWaitForever);
        if(flags & FuriFlagError) continue;
        if(flags & CCID_FLAG_STOP) break;
        if(flags & CCID_FLAG_RX) ccid_drain_and_process();
    }
    return 0;
}

/* -------- ISR endpoint/event callbacks (minimal, non-blocking) -------- */

static void ccid_rx_isr(usbd_device* dev, uint8_t event, uint8_t ep) {
    UNUSED(ep);
    if(event != usbd_evt_eprx) return;
    uint8_t buf[CCID_BULK_EPSIZE];
    int32_t len = usbd_ep_read(dev, CCID_EP_OUT, buf, sizeof(buf));
    if(len > 0 && g_ccid && g_ccid->rx_stream) {
        furi_stream_buffer_send(g_ccid->rx_stream, buf, (size_t)len, 0);
        if(g_ccid->thread_id) furi_thread_flags_set(g_ccid->thread_id, CCID_FLAG_RX);
    }
}

static void ccid_tx_isr(usbd_device* dev, uint8_t event, uint8_t ep) {
    UNUSED(dev);
    UNUSED(ep);
    if(event != usbd_evt_eptx) return;
    if(g_ccid && g_ccid->thread_id) furi_thread_flags_set(g_ccid->thread_id, CCID_FLAG_TX_DONE);
}

static void ccid_int_isr(usbd_device* dev, uint8_t event, uint8_t ep) {
    UNUSED(dev);
    UNUSED(event);
    UNUSED(ep);
}

static void ccid_notify_slot_change(usbd_device* dev) {
    uint8_t buf[2] = {RDR_TO_PC_NOTIFYSLOTCHANGE, 0x03}; /* slot 0: present + changed */
    usbd_ep_write(dev, CCID_EP_INT, buf, sizeof(buf));
}

static usbd_respond ccid_control(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* callback) {
    UNUSED(dev);
    UNUSED(callback);
    if((req->bmRequestType & USB_REQ_TYPE) == USB_REQ_CLASS &&
       (req->bmRequestType & USB_REQ_RECIPIENT) == USB_REQ_INTERFACE) {
        if(req->bRequest == CCID_ABORT) return usbd_ack;
    }
    return usbd_fail;
}

static usbd_respond ccid_ep_config(usbd_device* dev, uint8_t cfg) {
    switch(cfg) {
    case 0:
        usbd_ep_deconfig(dev, CCID_EP_IN);
        usbd_ep_deconfig(dev, CCID_EP_OUT);
        usbd_ep_deconfig(dev, CCID_EP_INT);
        usbd_reg_endpoint(dev, CCID_EP_IN, NULL);
        usbd_reg_endpoint(dev, CCID_EP_OUT, NULL);
        usbd_reg_endpoint(dev, CCID_EP_INT, NULL);
        g_ccid->configured = false;
        return usbd_ack;
    case 1:
        usbd_ep_config(dev, CCID_EP_IN, USB_EPTYPE_BULK, CCID_BULK_EPSIZE);
        usbd_ep_config(dev, CCID_EP_OUT, USB_EPTYPE_BULK, CCID_BULK_EPSIZE);
        usbd_ep_config(dev, CCID_EP_INT, USB_EPTYPE_INTERRUPT, CCID_INT_EPSIZE);
        usbd_reg_endpoint(dev, CCID_EP_IN, ccid_tx_isr);
        usbd_reg_endpoint(dev, CCID_EP_OUT, ccid_rx_isr);
        usbd_reg_endpoint(dev, CCID_EP_INT, ccid_int_isr);
        g_ccid->rx_msg_len = 0;
        g_ccid->configured = true;
        ccid_notify_slot_change(dev);
        return usbd_ack;
    }
    return usbd_fail;
}

static void ccid_init(usbd_device* dev, FuriHalUsbInterface* intf, void* ctx) {
    UNUSED(intf);
    UNUSED(ctx);
    if(!g_ccid) return;
    g_ccid->dev = dev;
    usbd_reg_config(dev, ccid_ep_config);
    usbd_reg_control(dev, ccid_control);
    usbd_connect(dev, true);
}

static void ccid_deinit(usbd_device* dev) {
    usbd_reg_config(dev, NULL);
    usbd_reg_control(dev, NULL);
}

static void ccid_on_wakeup(usbd_device* dev) {
    UNUSED(dev);
}

static void ccid_on_suspend(usbd_device* dev) {
    UNUSED(dev);
}

/* -------------------------------------------------------------------------- */

void seader_usb_ccid_reader_start(const SeaderCcidReaderConfig* cfg) {
    furi_check(cfg);
    if(g_ccid) seader_usb_ccid_reader_stop();

    g_ccid = malloc(sizeof(UsbCcid));
    memset(g_ccid, 0, sizeof(UsbCcid));
    g_ccid->cfg = *cfg;
    g_ccid->rx_stream = furi_stream_buffer_alloc(CCID_RX_STREAM_SIZE, 1);
    g_ccid->running = true;

    g_ccid->thread = furi_thread_alloc_ex("SeaderCcid", 2048, ccid_worker, NULL);
    furi_thread_start(g_ccid->thread);
    g_ccid->thread_id = furi_thread_get_id(g_ccid->thread);

    g_ccid->dev_descr = (struct usb_device_descriptor){
        .bLength = sizeof(struct usb_device_descriptor),
        .bDescriptorType = USB_DTYPE_DEVICE,
        .bcdUSB = 0x0200,
        .bDeviceClass = 0x00,
        .bDeviceSubClass = 0x00,
        .bDeviceProtocol = 0x00,
        .bMaxPacketSize0 = 8,
        .idVendor = cfg->vid,
        .idProduct = cfg->pid,
        .bcdDevice = 0x0100,
        .iManufacturer = 1, /* manufacturer string present (PC/SC name = manuf + product) */
        .iProduct = 2,
        .iSerialNumber = 3,
        .bNumConfigurations = 1,
    };

    ccid_build_string(g_ccid->str_manuf, cfg->manuf ? cfg->manuf : "Seader");
    ccid_build_string(g_ccid->str_prod, cfg->product ? cfg->product : "SAM Reader");
    ccid_build_string(g_ccid->str_serial, "SEADER-SAM-1");

    g_ccid->iface = (FuriHalUsbInterface){
        .init = ccid_init,
        .deinit = ccid_deinit,
        .wakeup = ccid_on_wakeup,
        .suspend = ccid_on_suspend,
        .dev_descr = &g_ccid->dev_descr,
        .str_manuf_descr = g_ccid->str_manuf,
        .str_prod_descr = g_ccid->str_prod,
        .str_serial_descr = g_ccid->str_serial,
        .cfg_descr = (void*)&ccid_cfg_desc,
    };

    g_ccid->prev = furi_hal_usb_get_config();
    furi_hal_usb_unlock();
    if(!furi_hal_usb_set_config(&g_ccid->iface, NULL)) {
        FURI_LOG_E(TAG, "furi_hal_usb_set_config failed");
    }
    FURI_LOG_I(TAG, "CCID reader up: '%s' %04x:%04x", g_ccid->str_prod + 2, cfg->vid, cfg->pid);
}

void seader_usb_ccid_reader_stop(void) {
    if(!g_ccid) return;

    /* Restore USB first so no more ISR callbacks reference our state. */
    FuriHalUsbInterface* prev = g_ccid->prev;
    furi_hal_usb_unlock();
    furi_hal_usb_set_config(prev, NULL);

    /* Tear down the worker thread. */
    g_ccid->running = false;
    if(g_ccid->thread_id) furi_thread_flags_set(g_ccid->thread_id, CCID_FLAG_STOP);
    furi_thread_join(g_ccid->thread);
    furi_thread_free(g_ccid->thread);

    furi_stream_buffer_free(g_ccid->rx_stream);
    free(g_ccid);
    g_ccid = NULL;
    FURI_LOG_I(TAG, "CCID reader down");
}

void seader_usb_ccid_reader_stats(uint8_t* last_cmd, uint32_t* count) {
    if(g_ccid) {
        if(last_cmd) *last_cmd = g_ccid->last_cmd;
        if(count) *count = g_ccid->cmd_count;
    } else {
        if(last_cmd) *last_cmd = 0;
        if(count) *count = 0;
    }
}

void seader_usb_ccid_reader_debug(uint16_t* atr_len, int32_t* tx_last) {
    if(g_ccid) {
        if(atr_len) *atr_len = g_ccid->dbg_atr_len;
        if(tx_last) *tx_last = g_ccid->dbg_tx_last;
    } else {
        if(atr_len) *atr_len = 0;
        if(tx_last) *tx_last = 0;
    }
}

const char* seader_usb_ccid_cmd_name(uint8_t type) {
    switch(type) {
    case 0x00:
        return "-";
    case PC_TO_RDR_ICCPOWERON:
        return "PowerOn";
    case PC_TO_RDR_ICCPOWEROFF:
        return "PowerOff";
    case PC_TO_RDR_GETSLOTSTATUS:
        return "SlotStatus";
    case PC_TO_RDR_XFRBLOCK:
        return "XfrBlock";
    case PC_TO_RDR_GETPARAMETERS:
        return "GetParams";
    case PC_TO_RDR_SETPARAMETERS:
        return "SetParams";
    case PC_TO_RDR_RESETPARAMETERS:
        return "RstParams";
    default:
        return "?";
    }
}
