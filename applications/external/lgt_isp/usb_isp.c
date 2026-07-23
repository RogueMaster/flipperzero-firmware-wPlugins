/* ============================================================
 * usb_isp.c  —  USB-CDC (virtuelles COM) + STK500-Worker.
 *
 * Struktur 1:1 nach der offiziellen AVR-ISP-App (helpers/avr_isp_worker.c):
 *   - usb_cdc_DUAL + furi_hal_usb_lock(); Kanal 0 bleibt die Flipper-CLI,
 *     Kanal 1 (CDC_CH) ist der Programmer-Port. Die CLI-VCP wird um den
 *     Config-Wechsel herum disabled/enabled.  << das war der Brick-Fix:
 *     v0.2.x hatte mit usb_cdc_single die aktive CLI-USB-Config zerstoert.
 *   - Callbacks setzen nur Thread-Flags; furi_hal_cdc_receive/send laufen
 *     ausschliesslich im Worker-Thread.
 *   - Kein PWM-Takt auf PA4 (der LGT hat internen Oszillator).
 * ============================================================ */
#include "usb_isp.h"
#include "stk500.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb.h>
#include <furi_hal_usb_cdc.h>
#include <cli/cli_vcp.h>

#define CDC_CH  1 /* Kanal 1: Programmer (Kanal 0 = Flipper-CLI) */
#define CDC_PKT 64 /* USB-FS Bulk-Paketgroesse */

#define EvtStop       (1UL << 0)
#define EvtRx         (1UL << 1)
#define EvtTxComplete (1UL << 2)

struct UsbIsp {
    FuriThread* worker;
    FuriStreamBuffer* rx;
    CliVcp* cli_vcp;
    volatile bool run;
    volatile uint32_t activity;
};

/* --- CDC-Callbacks: nur Flags setzen (IRQ-sicher) --- */
static void cb_tx_complete(void* ctx) {
    UsbIsp* u = ctx;
    furi_thread_flags_set(furi_thread_get_id(u->worker), EvtTxComplete);
}
static void cb_rx(void* ctx) {
    UsbIsp* u = ctx;
    furi_thread_flags_set(furi_thread_get_id(u->worker), EvtRx);
}
static void cb_state(void* ctx, uint8_t st) {
    UNUSED(ctx);
    UNUSED(st);
}
static void cb_ctrl(void* ctx, uint8_t st) {
    UNUSED(ctx);
    UNUSED(st);
}
static void cb_line(void* ctx, struct usb_cdc_line_coding* cfg) {
    UNUSED(ctx);
    UNUSED(cfg);
}
static const CdcCallbacks cdc_cb = {
    .tx_ep_callback = cb_tx_complete,
    .rx_ep_callback = cb_rx,
    .state_callback = cb_state,
    .ctrl_line_callback = cb_ctrl,
    .config_callback = cb_line,
};

/* --- STK500-I/O (komplett im Worker-Thread) --- */
static bool io_get(void* ctx, uint8_t* b, uint32_t to) {
    UsbIsp* u = ctx;
    if(furi_stream_buffer_is_empty(u->rx)) {
        uint32_t fl = furi_thread_flags_wait(EvtRx | EvtStop, FuriFlagWaitAny, to);
        if(fl & EvtStop) {
            u->run = false;
            return false;
        }
        if(fl & EvtRx) {
            uint8_t tmp[CDC_PKT];
            int32_t n;
            do { /* alles Verfuegbare abziehen (gegen Paket-Coalescing) */
                n = furi_hal_cdc_receive(CDC_CH, tmp, sizeof(tmp));
                if(n > 0) furi_stream_buffer_send(u->rx, tmp, (size_t)n, 0);
            } while(n == (int32_t)sizeof(tmp));
        }
    }
    return furi_stream_buffer_receive(u->rx, b, 1, 0) == 1;
}
static void io_send(void* ctx, const uint8_t* buf, size_t n) {
    UNUSED(ctx);
    size_t off = 0;
    while(n) {
        size_t chunk = n > CDC_PKT ? CDC_PKT : n;
        furi_thread_flags_clear(EvtTxComplete);
        furi_hal_cdc_send(CDC_CH, (uint8_t*)buf + off, (uint16_t)chunk);
        furi_thread_flags_wait(EvtTxComplete, FuriFlagWaitAny, 200); /* Flusskontrolle */
        off += chunk;
        n -= chunk;
    }
}
static void io_activity(void* ctx) {
    UsbIsp* u = ctx;
    u->activity++;
}

/* --- USB auf/zu, exakt nach offizieller App --- */
static void usb_open(UsbIsp* u) {
    u->cli_vcp = furi_record_open(RECORD_CLI_VCP);
    cli_vcp_disable(u->cli_vcp);
    furi_hal_usb_set_config(&usb_cdc_dual, NULL);
    furi_hal_usb_lock();
    cli_vcp_enable(u->cli_vcp);
    furi_hal_cdc_set_callbacks(CDC_CH, (CdcCallbacks*)&cdc_cb, u);
}
static void usb_close(UsbIsp* u) {
    furi_hal_cdc_set_callbacks(CDC_CH, NULL, NULL);
    cli_vcp_disable(u->cli_vcp);
    furi_hal_usb_unlock();
    furi_hal_usb_set_config(&usb_cdc_single, NULL);
    cli_vcp_enable(u->cli_vcp);
    furi_record_close(RECORD_CLI_VCP);
}

static int32_t usb_worker(void* ctx) {
    UsbIsp* u = ctx;
    usb_open(u);
    Stk500Io io = {
        .ctx = u,
        .get = io_get,
        .send = io_send,
        .run = &u->run,
        .activity = io_activity,
    };
    stk500_run(&io);
    usb_close(u);
    return 0;
}

UsbIsp* usb_isp_start(void) {
    UsbIsp* u = malloc(sizeof(UsbIsp));
    memset(u, 0, sizeof(UsbIsp));
    u->run = true;
    u->rx = furi_stream_buffer_alloc(1024, 1);
    u->worker = furi_thread_alloc_ex("LgtStk500", 2048, usb_worker, u);
    furi_thread_start(u->worker);
    return u;
}

void usb_isp_stop(UsbIsp* u) {
    u->run = false;
    furi_thread_flags_set(furi_thread_get_id(u->worker), EvtStop);
    furi_thread_join(u->worker);
    furi_thread_free(u->worker);
    furi_stream_buffer_free(u->rx);
    free(u);
}

uint32_t usb_isp_activity(UsbIsp* u) {
    return u->activity;
}
