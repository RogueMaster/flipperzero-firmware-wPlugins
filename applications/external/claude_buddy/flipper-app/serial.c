/** USB CDC transport implementation. */

#include "transport.h"
#include "protocol.h"
#include <string.h>
#include <furi_hal_usb.h>
#include <furi_hal_usb_cdc.h>

#define CDC_CH       0
#define RX_FLAG_DATA (1u << 0)
#define USB_TX_CHUNK 64   /* USB FS bulk endpoint max packet size */

typedef struct {
    Transport          base;   /* MUST be first */
    FuriThread*        rx_thread;
    TransportRxCallback callback;
    void*              callback_ctx;
    FuriHalUsbInterface* usb_mode_prev;
    FuriMutex*         tx_mutex;
    bool               running;
    char               rx_buf[PROTOCOL_MAX_MSG_LEN];
    int                rx_pos;
} UsbTransport;

/* ── RX thread ─────────────────────────────────────────────────── */

static void cdc_rx_callback(void* context) {
    UsbTransport* ut = context;
    if(!ut || !ut->rx_thread) return;
    furi_thread_flags_set(furi_thread_get_id(ut->rx_thread), RX_FLAG_DATA);
}

static int32_t usb_rx_thread(void* context) {
    UsbTransport* ut = context;
    if(!ut) return 0;
    uint8_t buf[64];

    while(ut->running) {
        uint32_t flags = furi_thread_flags_wait(RX_FLAG_DATA, FuriFlagWaitAny, 100);
        if(!(flags & RX_FLAG_DATA)) continue;

        while(true) {
            int32_t len = furi_hal_cdc_receive(CDC_CH, buf, sizeof(buf));
            if(len <= 0) break;

            for(int32_t i = 0; i < len; i++) {
                if(buf[i] == '\n') {
                    if(ut->rx_pos > 0) {
                        ut->rx_buf[ut->rx_pos] = '\0';
                        if(ut->callback)
                            ut->callback(ut->rx_buf, ut->callback_ctx);
                        ut->rx_pos = 0;
                    }
                } else if(ut->rx_pos < (int)sizeof(ut->rx_buf) - 1) {
                    ut->rx_buf[ut->rx_pos++] = (char)buf[i];
                }
            }
        }
    }
    return 0;
}

/* ── vtable implementations ────────────────────────────────────── */

static void usb_start(Transport* t, TransportRxCallback cb, void* ctx) {
    if(!t) return;
    UsbTransport* ut = (UsbTransport*)t;
    ut->callback     = cb;
    ut->callback_ctx = ctx;
    ut->running      = true;
    ut->rx_pos       = 0;

    ut->usb_mode_prev = furi_hal_usb_get_config();
    furi_hal_usb_unlock();
    furi_check(furi_hal_usb_set_config(&usb_cdc_single, NULL));

    static CdcCallbacks cdc_cb = {
        .tx_ep_callback    = NULL,
        .rx_ep_callback    = cdc_rx_callback,
        .state_callback    = NULL,
        .ctrl_line_callback = NULL,
        .config_callback   = NULL,
    };
    furi_hal_cdc_set_callbacks(CDC_CH, &cdc_cb, ut);
    furi_thread_start(ut->rx_thread);
}

static void usb_stop(Transport* t) {
    if(!t) return;
    UsbTransport* ut = (UsbTransport*)t;
    ut->running = false;
    furi_thread_flags_set(furi_thread_get_id(ut->rx_thread), RX_FLAG_DATA);
    furi_thread_join(ut->rx_thread);

    furi_hal_cdc_set_callbacks(CDC_CH, NULL, NULL);
    furi_hal_usb_unlock();
    furi_hal_usb_disable();
    furi_delay_ms(250);
    furi_hal_usb_enable();
    furi_delay_ms(250);
    if(ut->usb_mode_prev) {
        furi_hal_usb_set_config(ut->usb_mode_prev, NULL);
        furi_delay_ms(250);
    }
}

static void usb_send(Transport* t, const char* data, int len) {
    if(!t || !data) return;
    UsbTransport* ut = (UsbTransport*)t;
    /* Mutex prevents concurrent sends (e.g. pong interleaving with perm_resp)
       from corrupting multi-chunk messages on the wire. */
    furi_mutex_acquire(ut->tx_mutex, FuriWaitForever);
    int offset = 0;
    while(offset < len) {
        int chunk = len - offset;
        if(chunk > USB_TX_CHUNK) chunk = USB_TX_CHUNK;
        furi_hal_cdc_send(CDC_CH, (uint8_t*)(data + offset), (uint16_t)chunk);
        offset += chunk;
        if(offset < len) {
            furi_delay_ms(1);
        }
    }
    furi_mutex_release(ut->tx_mutex);
}

static void usb_free(Transport* t) {
    if(!t) return;
    UsbTransport* ut = (UsbTransport*)t;
    furi_thread_free(ut->rx_thread);
    furi_mutex_free(ut->tx_mutex);
    free(ut);
}

/* ── public factory ────────────────────────────────────────────── */

Transport* transport_usb_alloc(void) {
    UsbTransport* ut = malloc(sizeof(UsbTransport));
    furi_check(ut != NULL);
    memset(ut, 0, sizeof(UsbTransport));
    ut->base.start   = usb_start;
    ut->base.stop    = usb_stop;
    ut->base.send    = usb_send;
    ut->base.free_fn = usb_free;
    ut->rx_thread    = furi_thread_alloc_ex("CdcSerialRx", 2048, usb_rx_thread, ut);
    ut->tx_mutex     = furi_mutex_alloc(FuriMutexTypeNormal);
    return (Transport*)ut;
}
