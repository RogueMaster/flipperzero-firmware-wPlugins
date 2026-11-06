#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb_cdc.h>
#include <cli/cli_vcp.h>

#include "line_buffer.h"
#include "line_sender.h"
#include "proto_encode.h"
#include "transport_usb.h"

#define USB_CDC_IF        0
#define USB_TX_TIMEOUT_MS 100 /* per packet; the host is not reading after that */
#define USB_WORKER_STACK  2048

_Static_assert(CDC_DATA_SZ <= LINE_SENDER_PACKET_MAX, "a CDC packet must fit the line sender");

typedef enum {
    UsbEventStop = 1 << 0,
    UsbEventRx = 1 << 1,
    UsbEventCtrlLine = 1 << 2,
} UsbEvent;

#define USB_EVENTS_ALL (UsbEventStop | UsbEventRx | UsbEventCtrlLine)

typedef struct {
    Transport base;
    FuriHalUsbInterface* usb_prev;
    FuriThread* worker;
    FuriSemaphore* tx_done; /* released when a packet completes */
    FuriMutex* tx_lock; /* guards sender and tx_busy */
    bool tx_busy; /* a packet went out and did not complete yet */
    volatile bool tx_lost; /* disconnected: no packet in flight completes */
    LineSender sender;
    LineBuffer lines;
    char* line_storage;
    TransportRxCallback rx_callback;
    void* rx_ctx;
    volatile bool dtr;
    bool opened;
} UsbTransport;

static void cdc_tx_done(void* ctx) {
    UsbTransport* t = ctx;
    furi_semaphore_release(t->tx_done);
}

static void cdc_rx(void* ctx) {
    UsbTransport* t = ctx;
    furi_thread_flags_set(furi_thread_get_id(t->worker), UsbEventRx);
}

static void cdc_state(void* ctx, CdcState state) {
    UsbTransport* t = ctx;
    if(state == CdcStateDisconnected) {
        t->dtr = false;
        t->tx_lost = true;
        furi_thread_flags_set(furi_thread_get_id(t->worker), UsbEventCtrlLine);
    }
}

static void cdc_ctrl_line(void* ctx, CdcCtrlLine ctrl_lines) {
    UsbTransport* t = ctx;
    t->dtr = (ctrl_lines & CdcCtrlLineDTR) != 0;
    furi_thread_flags_set(furi_thread_get_id(t->worker), UsbEventCtrlLine);
}

static CdcCallbacks cdc_callbacks = {
    .tx_ep_callback = cdc_tx_done,
    .rx_ep_callback = cdc_rx,
    .state_callback = cdc_state,
    .ctrl_line_callback = cdc_ctrl_line,
    .config_callback = NULL,
};

static void on_line(const char* line, size_t len, void* ctx) {
    UsbTransport* t = ctx;
    if(t->rx_callback) t->rx_callback(line, len, t->rx_ctx);
}

static int32_t usb_worker(void* ctx) {
    UsbTransport* t = ctx;
    uint8_t packet[CDC_DATA_SZ];
    bool was_open = false;
    for(;;) {
        uint32_t events = furi_thread_flags_wait(USB_EVENTS_ALL, FuriFlagWaitAny, FuriWaitForever);
        if(events & FuriFlagError) continue;
        if(events & UsbEventStop) break;
        if(events & UsbEventCtrlLine) {
            bool open = t->dtr;
            /* a new host session never continues the old one's partial line */
            if(open != was_open) line_buffer_reset(&t->lines);
            was_open = open;
        }
        if(events & UsbEventRx) {
            int32_t len;
            while((len = furi_hal_cdc_receive(USB_CDC_IF, packet, sizeof(packet))) > 0) {
                line_buffer_feed(&t->lines, packet, (size_t)len);
            }
        }
    }
    return 0;
}

static void cli_vcp_set(bool enabled) {
    CliVcp* cli_vcp = furi_record_open(RECORD_CLI_VCP);
    if(enabled) {
        cli_vcp_enable(cli_vcp);
    } else {
        cli_vcp_disable(cli_vcp);
    }
    furi_record_close(RECORD_CLI_VCP);
}

static bool usb_open(Transport* base) {
    UsbTransport* t = (UsbTransport*)base;
    if(t->opened) return true;

    t->usb_prev = furi_hal_usb_get_config();
    cli_vcp_set(false);
    furi_hal_usb_unlock();
    if(!furi_hal_usb_set_config(&usb_cdc_single, NULL)) {
        cli_vcp_set(true);
        return false;
    }

    line_buffer_reset(&t->lines);
    t->tx_busy = false; /* the new configuration has nothing in flight */
    t->tx_lost = false;
    furi_thread_start(t->worker);
    furi_hal_cdc_set_callbacks(USB_CDC_IF, &cdc_callbacks, t);
    t->dtr = (furi_hal_cdc_get_ctrl_line_state(USB_CDC_IF) & CdcCtrlLineDTR) != 0;
    t->opened = true;
    return true;
}

static void usb_close(Transport* base) {
    UsbTransport* t = (UsbTransport*)base;
    if(!t->opened) return;

    furi_hal_cdc_set_callbacks(USB_CDC_IF, NULL, NULL);
    furi_thread_flags_set(furi_thread_get_id(t->worker), UsbEventStop);
    furi_thread_join(t->worker);
    t->dtr = false;
    t->opened = false;

    furi_hal_usb_unlock();
    /* a failed restore is logged, not fatal: the CLI must still come back */
    if(!furi_hal_usb_set_config(t->usb_prev, NULL)) {
        FURI_LOG_E("FliBuddy", "USB: restoring the previous configuration failed");
    }
    cli_vcp_set(true);
}

static bool tx_wait(UsbTransport* t) {
    if(furi_semaphore_acquire(t->tx_done, furi_ms_to_ticks(USB_TX_TIMEOUT_MS)) != FuriStatusOk) {
        return false;
    }
    t->tx_busy = false;
    return true;
}

/* furi_hal_cdc_send() only queues the packet; it went out once tx_done says
 * so. */
static LinePacketResult send_packet(const uint8_t* data, size_t len, void* ctx) {
    UsbTransport* t = ctx;
    if(t->tx_lost) {
        t->tx_lost = false;
        t->tx_busy = false;
    }
    if(t->tx_busy && !tx_wait(t)) return LinePacketNotSent; /* the last one is still stuck */
    /* with none of ours in flight, a completion is stale or the zero-length
     * packet the HAL sends on configuration */
    while(furi_semaphore_acquire(t->tx_done, 0) == FuriStatusOk) {
    }
    furi_hal_cdc_send(USB_CDC_IF, (uint8_t*)data, (uint16_t)len);
    t->tx_busy = true;
    return tx_wait(t) ? LinePacketSent : LinePacketMaybeSent;
}

static TransportSendResult usb_send_line(Transport* base, const char* line, size_t len) {
    UsbTransport* t = (UsbTransport*)base;
    if(!t->opened || !t->dtr || len >= PROTO_LINE_MAX) return TransportNotSent;

    furi_mutex_acquire(t->tx_lock, FuriWaitForever);
    TransportSendResult result = line_sender_send(&t->sender, line, len);
    furi_mutex_release(t->tx_lock);
    return result;
}

static void usb_set_rx_callback(Transport* base, TransportRxCallback callback, void* ctx) {
    UsbTransport* t = (UsbTransport*)base;
    furi_check(!t->opened);
    t->rx_callback = callback;
    t->rx_ctx = ctx;
}

static bool usb_is_connected(const Transport* base) {
    const UsbTransport* t = (const UsbTransport*)base;
    return t->opened && t->dtr;
}

Transport* transport_usb_alloc(void) {
    UsbTransport* t = malloc(sizeof(UsbTransport));
    memset(t, 0, sizeof(UsbTransport));
    t->base.open = usb_open;
    t->base.close = usb_close;
    t->base.send_line = usb_send_line;
    t->base.set_rx_callback = usb_set_rx_callback;
    t->base.is_connected = usb_is_connected;
    t->line_storage = malloc(PROTO_LINE_MAX);
    line_buffer_init(&t->lines, t->line_storage, PROTO_LINE_MAX, on_line, t);
    line_sender_init(&t->sender, CDC_DATA_SZ, send_packet, t);
    t->tx_done = furi_semaphore_alloc(1, 0);
    t->tx_lock = furi_mutex_alloc(FuriMutexTypeNormal);
    t->worker = furi_thread_alloc_ex("FliBuddyUsb", USB_WORKER_STACK, usb_worker, t);
    return &t->base;
}

void transport_usb_free(Transport* base) {
    UsbTransport* t = (UsbTransport*)base;
    usb_close(base);
    furi_thread_free(t->worker);
    furi_mutex_free(t->tx_lock);
    furi_semaphore_free(t->tx_done);
    free(t->line_storage);
    free(t);
}
