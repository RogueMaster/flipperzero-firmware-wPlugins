// USB transport: switches USB to a single CDC interface, reassembles frames from the byte
// stream and restores the previous USB configuration on stop.
#include <stdlib.h>
#include <string.h>
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb.h>
#include <furi_hal_usb_cdc.h>
#if __has_include(<cli/cli_vcp.h>)
#include <cli/cli_vcp.h>
#endif
#include "transport.h"

#define TAG          "PhrUsb"
#define USB_IF       0
#define USB_RX_CHUNK 64

typedef enum {
    WorkerFlagRx = (1 << 0),
    WorkerFlagCtrl = (1 << 1),
    WorkerFlagStop = (1 << 2),
    WorkerFlagHello = (1 << 3),
} WorkerFlag;

#define WORKER_ALL (WorkerFlagRx | WorkerFlagCtrl | WorkerFlagStop | WorkerFlagHello)

struct PhrTransport {
    PhrLink* link;
    FuriThread* thread;
    FuriHalUsbInterface* prev_usb;
    bool dtr; // host has the port open
    bool usb_connected;
#ifdef RECORD_CLI_VCP
    CliVcp* cli_vcp;
#endif
};

static void usb_rx_cb(void* ctx) {
    PhrTransport* t = ctx;
    furi_thread_flags_set(furi_thread_get_id(t->thread), WorkerFlagRx);
}

static void usb_state_cb(void* ctx, uint8_t state) {
    PhrTransport* t = ctx;
    t->usb_connected = (state == 1);
    if(!t->usb_connected) t->dtr = false;
    furi_thread_flags_set(furi_thread_get_id(t->thread), WorkerFlagCtrl);
}

static void usb_ctrl_line_cb(void* ctx, uint8_t state) {
    PhrTransport* t = ctx;
    t->dtr = (state & (1 << 0)) != 0; // DTR bit
    furi_thread_flags_set(furi_thread_get_id(t->thread), WorkerFlagCtrl);
}

static const CdcCallbacks usb_cdc_callbacks = {
    .tx_ep_callback = NULL,
    .rx_ep_callback = usb_rx_cb,
    .state_callback = usb_state_cb,
    .ctrl_line_callback = usb_ctrl_line_cb,
    .config_callback = NULL,
};

static int32_t usb_worker(void* ctx) {
    PhrTransport* t = ctx;
    uint8_t buf[USB_RX_CHUNK];
    for(;;) {
        uint32_t flags = furi_thread_flags_wait(WORKER_ALL, FuriFlagWaitAny, FuriWaitForever);
        if(flags & FuriFlagError) continue;
        if(flags & WorkerFlagStop) break;

        if(flags & WorkerFlagCtrl) {
            // A port that is open and enumerated counts as connected.
            phr_link_set_connected(t->link, t->dtr);
        }
        if(flags & WorkerFlagRx) {
            int32_t n;
            while((n = furi_hal_cdc_receive(USB_IF, buf, sizeof(buf))) > 0) {
                phr_link_feed(t->link, buf, (size_t)n);
            }
        }
        if((flags & WorkerFlagHello) && t->dtr) {
            uint8_t hello[PHR_HELLO_LEN];
            size_t n = phr_link_build_hello(t->link, 1, hello);
            furi_hal_cdc_send(USB_IF, hello, (uint16_t)n);
        }
    }
    return 0;
}

static PhrTransport* phr_usb_start(PhrLink* link) {
    PhrTransport* t = malloc(sizeof(PhrTransport));
    memset(t, 0, sizeof(PhrTransport));
    t->link = link;

#ifdef RECORD_CLI_VCP
    // New CLI: detach the shell from the VCP while we own the CDC interface.
    t->cli_vcp = furi_record_open(RECORD_CLI_VCP);
    cli_vcp_disable(t->cli_vcp);
#endif

    t->thread = furi_thread_alloc_ex("PhrUsbWorker", 1024, usb_worker, t);
    furi_thread_start(t->thread);

    t->prev_usb = furi_hal_usb_get_config();
    furi_hal_usb_unlock();
    if(!furi_hal_usb_set_config(&usb_cdc_single, NULL)) {
        FURI_LOG_E(TAG, "Failed to switch USB to CDC");
    }
    furi_hal_cdc_set_callbacks(USB_IF, (CdcCallbacks*)&usb_cdc_callbacks, t);
    return t;
}

static void phr_usb_stop(PhrTransport* t) {
    furi_hal_cdc_set_callbacks(USB_IF, NULL, NULL);
    furi_thread_flags_set(furi_thread_get_id(t->thread), WorkerFlagStop);
    furi_thread_join(t->thread);
    furi_thread_free(t->thread);

    furi_hal_usb_unlock();
    if(t->prev_usb) furi_hal_usb_set_config(t->prev_usb, NULL);
    phr_link_set_connected(t->link, false);

#ifdef RECORD_CLI_VCP
    cli_vcp_enable(t->cli_vcp);
    furi_record_close(RECORD_CLI_VCP);
#endif
    free(t);
}

static void phr_usb_tick(PhrTransport* t) {
    if(phr_link_hello_due(t->link)) {
        furi_thread_flags_set(furi_thread_get_id(t->thread), WorkerFlagHello);
    }
}

static const char* phr_usb_status(PhrTransport* t) {
    if(t->dtr) return "Port open";
    return t->usb_connected ? "Plugged in" : "Plug in USB cable";
}

static const char* phr_usb_device_name(PhrTransport* t) {
    UNUSED(t);
    return "USB serial";
}

const PhrTransportApi phr_transport_usb = {
    .name = "USB CDC",
    .wire_id = 1,
    .start = phr_usb_start,
    .stop = phr_usb_stop,
    .tick = phr_usb_tick,
    .status = phr_usb_status,
    .device_name = phr_usb_device_name,
};
