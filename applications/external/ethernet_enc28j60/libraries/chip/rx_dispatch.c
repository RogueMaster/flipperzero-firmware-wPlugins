#include "rx_dispatch.h"
#include <furi_hal_gpio.h>
#include <furi_hal_resources.h>

#define RX_DISPATCH_MAX_HANDLERS 8
#define RX_DISPATCH_STACK_BYTES  4096

// F0.5c — wakeup is now driven by an external GPIO interrupt on the
// ENC28J60 INT pin (wired to PA14 / SWCLK / Flipper external pin 10).
// The chip drives INT low whenever any EIR.* flag is set with its
// matching EIE.* enable bit. enc28j60_start sets EIE_INTIE|EIE_PKTIE,
// so each received packet pulls INT low; receive_packet's ECON2.PKTDEC
// clears the flag and lets INT release (only when EPKTCNT hits 0).
//
// We use a falling-edge ISR that just sets a thread flag — ISR-safe,
// no chip I/O, no mutex. The dispatcher thread blocks on flag_wait
// with a 100 ms timeout so a missed INT (datasheet errata DS80349
// notes some races) still gets serviced within 100 ms instead of
// hanging forever.
#define RX_FLAG_INT         (1U << 0)
#define RX_POLL_FALLBACK_MS 100

struct rx_handle {
    rx_predicate_fn predicate;
    rx_handler_fn handler;
    void* ctx;
    bool in_use;
};

typedef struct {
    enc28j60_t* ethernet;
    FuriThread* thread;
    FuriThreadId thread_id;
    FuriMutex* mutex;
    volatile bool running;
    volatile uint8_t pause_count; // >0 = paused. Counter for nestable pause/resume.
    volatile bool acked_pause; // thread sets when it observes pause and idles
    struct rx_handle slots[RX_DISPATCH_MAX_HANDLERS];
} rx_dispatch_t;

static rx_dispatch_t g_dispatch;

// ISR context: only operations safe to do here are flag/queue/semaphore
// signaling. Furi's furi_thread_flags_set wraps xTaskNotifyFromISR.
static void rx_dispatch_int_isr(void* ctx) {
    rx_dispatch_t* d = (rx_dispatch_t*)ctx;
    if(d->thread_id) {
        furi_thread_flags_set(d->thread_id, RX_FLAG_INT);
    }
}

static int32_t rx_dispatch_thread_fn(void* context) {
    rx_dispatch_t* d = (rx_dispatch_t*)context;
    enc28j60_t* eth = d->ethernet;
    uint8_t* rx = eth->rx_buffer;

    while(d->running) {
        // Block until INT fires or fallback timeout. Idle traffic case:
        // ~10 wakeups/s instead of the 1000/s F0.4 polling cadence.
        // FuriFlagWaitAny default also clears matched flags on exit
        // (FuriFlagNoClear would suppress that).
        furi_thread_flags_wait(RX_FLAG_INT, FuriFlagWaitAny, RX_POLL_FALLBACK_MS);

        if(!d->running) break;

        if(d->pause_count > 0) {
            d->acked_pause = true;
            continue;
        }
        d->acked_pause = false;

        // Drain all queued packets in this wakeup. INT is falling-edge,
        // so it only re-triggers on H→L; if multiple packets arrived
        // between wakeups (or arrive while we're processing one), the
        // chip keeps INT low until EPKTCNT decrements to zero. Looping
        // here until receive_packet returns 0 ensures we don't leave
        // packets stranded for a full RX_POLL_FALLBACK_MS window.
        while(d->running) {
            if(d->pause_count > 0) break;
            uint16_t len = receive_packet(eth, rx, MAX_FRAMELEN);
            if(len == 0) break;

            // F0.5d — hold the dispatch mutex through predicate AND
            // handler invocation. Pre-fix this was a snapshot pattern
            // (copy slots out, run handlers without the lock). That
            // protected against torn iteration but NOT against the
            // handler ctx being freed mid-call: scanner_wait_for_packet
            // keeps its scanner_wait_state_t (with FuriSemaphore* signal)
            // on the caller's stack and frees both right after rx_unregister.
            // If the dispatcher was mid-handler when the caller timed out,
            // the freed semaphore would be released into a dangling pointer.
            // Holding the lock here makes rx_unregister wait for any
            // in-flight invocation to finish before returning, so the
            // caller's stack stays valid until the handler exits. Lock
            // order: dispatch mutex (outer) → chip mutex via handler's
            // send_packet (inner). No reverse path exists.
            furi_mutex_acquire(d->mutex, FuriWaitForever);
            for(uint8_t i = 0; i < RX_DISPATCH_MAX_HANDLERS; i++) {
                if(!d->slots[i].in_use) continue;
                if(d->slots[i].predicate(rx, len, d->slots[i].ctx)) {
                    d->slots[i].handler(rx, len, d->slots[i].ctx);
                }
            }
            furi_mutex_release(d->mutex);
        }
    }
    return 0;
}

void rx_dispatch_init(App* app) {
    furi_assert(app);
    furi_assert(app->ethernet);

    if(g_dispatch.running) return;

    memset(&g_dispatch, 0, sizeof(g_dispatch));
    g_dispatch.ethernet = app->ethernet;
    g_dispatch.mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    g_dispatch.running = true;
    g_dispatch.thread = furi_thread_alloc_ex(
        "RX Dispatch", RX_DISPATCH_STACK_BYTES, rx_dispatch_thread_fn, &g_dispatch);

    furi_thread_start(g_dispatch.thread);
    g_dispatch.thread_id = furi_thread_get_id(g_dispatch.thread);

    // Configure PA14 (SWCLK / external pin 10) for falling-edge interrupt.
    // Pull-up so the line idles high when the chip releases INT (the
    // chip output is open-drain). Register ISR last so it can't fire
    // before thread_id is set.
    furi_hal_gpio_init(&gpio_swclk, GpioModeInterruptFall, GpioPullUp, GpioSpeedLow);
    furi_hal_gpio_add_int_callback(&gpio_swclk, rx_dispatch_int_isr, &g_dispatch);
}

void rx_dispatch_deinit(App* app) {
    UNUSED(app);
    if(!g_dispatch.running) return;

    // Stop new IRQs first, then restore the pin to a low-power state.
    furi_hal_gpio_remove_int_callback(&gpio_swclk);
    furi_hal_gpio_init(&gpio_swclk, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

    g_dispatch.running = false;
    // Kick the thread out of furi_thread_flags_wait so it can observe
    // running=false and exit. Without this, teardown waits up to one
    // RX_POLL_FALLBACK_MS window.
    if(g_dispatch.thread_id) {
        furi_thread_flags_set(g_dispatch.thread_id, RX_FLAG_INT);
    }
    furi_thread_join(g_dispatch.thread);
    furi_thread_free(g_dispatch.thread);
    g_dispatch.thread = NULL;
    g_dispatch.thread_id = NULL;

    furi_mutex_free(g_dispatch.mutex);
    g_dispatch.mutex = NULL;
}

rx_handle_t* rx_register(rx_predicate_fn predicate, rx_handler_fn handler, void* ctx) {
    furi_assert(predicate);
    furi_assert(handler);
    if(!g_dispatch.mutex) return NULL;

    furi_mutex_acquire(g_dispatch.mutex, FuriWaitForever);
    rx_handle_t* slot = NULL;
    for(uint8_t i = 0; i < RX_DISPATCH_MAX_HANDLERS; i++) {
        if(!g_dispatch.slots[i].in_use) {
            g_dispatch.slots[i].predicate = predicate;
            g_dispatch.slots[i].handler = handler;
            g_dispatch.slots[i].ctx = ctx;
            g_dispatch.slots[i].in_use = true;
            slot = &g_dispatch.slots[i];
            break;
        }
    }
    furi_mutex_release(g_dispatch.mutex);
    return slot;
}

void rx_unregister(rx_handle_t* handle) {
    if(!handle || !g_dispatch.mutex) return;
    furi_mutex_acquire(g_dispatch.mutex, FuriWaitForever);
    handle->in_use = false;
    handle->predicate = NULL;
    handle->handler = NULL;
    handle->ctx = NULL;
    furi_mutex_release(g_dispatch.mutex);
}

void rx_dispatch_pause(void) {
    if(!g_dispatch.mutex) return;
    furi_mutex_acquire(g_dispatch.mutex, FuriWaitForever);
    g_dispatch.pause_count++;
    g_dispatch.acked_pause = false;
    furi_mutex_release(g_dispatch.mutex);
    // F0.5d-wave2 — kick the thread out of furi_thread_flags_wait so it
    // observes pause_count immediately. Pre-fix the thread could sleep
    // up to RX_POLL_FALLBACK_MS (100 ms) before checking, while pause's
    // 40 ms ack loop returned silently with acked_pause still false —
    // a false-positive ack that let the caller race the dispatcher into
    // the chip.
    if(g_dispatch.thread_id) {
        furi_thread_flags_set(g_dispatch.thread_id, RX_FLAG_INT);
    }
    // Wait for the thread to ack — at most a few ms of polling.
    for(uint8_t i = 0; i < 20 && !g_dispatch.acked_pause; i++) {
        furi_delay_ms(2);
    }
}

void rx_dispatch_resume(void) {
    if(!g_dispatch.mutex) return;
    furi_mutex_acquire(g_dispatch.mutex, FuriWaitForever);
    if(g_dispatch.pause_count > 0) g_dispatch.pause_count--;
    furi_mutex_release(g_dispatch.mutex);
    // F0.5c — kick the thread out of flags_wait so it sees the resumed
    // state immediately. Without this it'd wait up to RX_POLL_FALLBACK_MS
    // (100 ms) before resuming actual packet reads.
    if(g_dispatch.thread_id) {
        furi_thread_flags_set(g_dispatch.thread_id, RX_FLAG_INT);
    }
}
