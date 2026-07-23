#include "verifier.h"

#include <furi_hal_serial.h>
#include <string.h>

/* Framings tried on the winning rate, in order of how often they turn up. */
static const HermesFraming verifier_framings[] = {
    HermesFraming8N1,
    HermesFraming8E1,
    HermesFraming8O1,
    HermesFraming7E1,
};
#define VERIFIER_FRAMING_COUNT (sizeof(verifier_framings) / sizeof(verifier_framings[0]))

struct Verifier {
    FuriThread* thread;
    FuriHalSerialHandle* serial;
    HermesPort port;

    uint32_t bauds[HERMES_BAUD_COUNT];
    size_t baud_count;

    volatile bool running;
    volatile bool stop_req;
    volatile uint8_t progress;
    volatile uint32_t current_baud;

    /* written from the serial interrupt during a window */
    volatile uint32_t rx_bytes;
    volatile uint32_t rx_printable;
    volatile uint32_t rx_newlines;
    volatile uint32_t rx_errors;
    volatile uint32_t rx_overruns;

    VerifierReport report;

    VerifierProgressCallback callback;
    void* context;
};

/* --------------------------------------------------------------- window --- */

static bool verifier_is_text(uint8_t b) {
    /* Tab, LF and CR are what a console actually emits alongside plain ASCII. */
    return (b >= 0x20 && b <= 0x7E) || b == '\t' || b == '\n' || b == '\r';
}

static void verifier_rx_isr(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* ctx) {
    Verifier* v = ctx;

    if(event & FuriHalSerialRxEventData) {
        while(furi_hal_serial_async_rx_available(handle)) {
            const uint8_t b = furi_hal_serial_async_rx(handle);
            v->rx_bytes++;
            if(verifier_is_text(b)) v->rx_printable++;
            if(b == '\n') v->rx_newlines++;
        }
    }

    /* The line disagreeing with our guess - this is the real signal. */
    if(event & (FuriHalSerialRxEventFrameError | FuriHalSerialRxEventNoiseError |
                FuriHalSerialRxEventParityError)) {
        v->rx_errors++;
    }

    /* An overrun means *we* were too slow, not that the rate is wrong. Counting
     * it as evidence would quietly punish the fastest (correct) candidates. */
    if(event & FuriHalSerialRxEventOverrunError) {
        v->rx_overruns++;
    }
}

/** Blend framing cleanliness with how much of the traffic reads as text.
 *
 * Cleanliness leads because it is the only signal that survives a binary
 * protocol; printability breaks ties and rewards an actual console.
 */
static uint8_t
    verifier_score(uint32_t bytes, uint32_t errors, uint32_t printable, uint32_t newlines) {
    if(bytes == 0) return 0; // silence proves nothing

    const uint32_t printable_pct = (printable * 100u) / bytes;
    const uint32_t error_rate = (errors * 100u) / (bytes + errors);
    const uint32_t clean = 100u - error_rate;

    uint32_t score = (clean * 60u + printable_pct * 40u) / 100u;

    /* Lines of readable text ending in newlines is what a boot log looks like. */
    if(newlines > 0 && printable_pct > 70u) score += 5u;

    return (uint8_t)((score > 100u) ? 100u : score);
}

static void
    verifier_measure(Verifier* v, uint32_t baud, HermesFraming framing, VerifierEntry* out) {
    v->current_baud = baud;

    furi_hal_serial_async_rx_stop(v->serial);
    furi_hal_serial_set_br(v->serial, baud);
    hermes_framing_apply(v->serial, framing);

    v->rx_bytes = 0;
    v->rx_printable = 0;
    v->rx_newlines = 0;
    v->rx_errors = 0;
    v->rx_overruns = 0;

    furi_hal_serial_async_rx_start(v->serial, verifier_rx_isr, v, true);
    furi_delay_ms(VERIFIER_WINDOW_MS);
    furi_hal_serial_async_rx_stop(v->serial);

    const uint32_t bytes = v->rx_bytes;
    const uint32_t errors = v->rx_errors;
    const uint32_t printable = v->rx_printable;

    out->baud = baud;
    out->framing = framing;
    out->bytes = (uint16_t)((bytes > UINT16_MAX) ? UINT16_MAX : bytes);
    out->errors = (uint16_t)((errors > UINT16_MAX) ? UINT16_MAX : errors);
    out->overruns = (uint16_t)((v->rx_overruns > UINT16_MAX) ? UINT16_MAX : v->rx_overruns);
    out->printable_pct = (uint8_t)(bytes ? ((printable * 100u) / bytes) : 0u);
    out->score = verifier_score(bytes, errors, printable, v->rx_newlines);
}

/* --------------------------------------------------------------- worker --- */

/** Report progress, unless we have been asked to stop.
 *
 * Staying quiet after a stop request matters: verifier_stop() blocks the GUI
 * thread in furi_thread_join, so that thread is not draining the event queue.
 * A callback here posts to that queue, and posting to a full queue would wait
 * on the very thread that is waiting on us.
 */
static void verifier_report_progress(Verifier* v) {
    if(v->callback && !v->stop_req) v->callback(v->context);
}

static int32_t verifier_worker(void* ctx) {
    Verifier* v = ctx;
    VerifierReport* r = &v->report;

    v->serial = furi_hal_serial_control_acquire(hermes_port_serial_id(v->port));
    if(!v->serial) {
        v->running = false;
        verifier_report_progress(v);
        return 0;
    }

    furi_hal_serial_init(v->serial, v->bauds[0]);

    /* Listen only. init() claims the TX pin too, and Hermes has no business
     * driving the target's RX line until the user opens the console. */
    furi_hal_serial_disable_direction(v->serial, FuriHalSerialDirectionTx);

    /* Phase A - find the rate, holding framing at the overwhelmingly common 8N1. */
    const size_t total_steps = v->baud_count + VERIFIER_FRAMING_COUNT;
    size_t step = 0;

    for(size_t i = 0; i < v->baud_count && !v->stop_req; i++) {
        verifier_measure(v, v->bauds[i], HermesFraming8N1, &r->entries[r->entry_count]);
        if(r->entries[r->entry_count].bytes > 0) r->saw_traffic = true;
        r->entry_count++;

        v->progress = (uint8_t)((++step * 100u) / total_steps);
        verifier_report_progress(v);
    }

    /* Pick the winner of phase A. */
    VerifierEntry best = {0};
    for(uint8_t i = 0; i < r->entry_count; i++) {
        if(r->entries[i].score > best.score) best = r->entries[i];
    }

    /* Phase B - now pin the rate and let the framing compete. A right rate with
     * the wrong parity shows up as a clean-ish byte stream riddled with frame
     * errors, which is exactly what this pass resolves. */
    if(best.bytes > 0 && !v->stop_req) {
        for(size_t i = 0; i < VERIFIER_FRAMING_COUNT && !v->stop_req; i++) {
            VerifierEntry e;
            verifier_measure(v, best.baud, verifier_framings[i], &e);
            if(e.score > best.score) best = e;

            v->progress = (uint8_t)((++step * 100u) / total_steps);
            verifier_report_progress(v);
        }
    }

    r->best = best;
    r->complete = !v->stop_req;
    v->progress = 100;

    furi_hal_serial_deinit(v->serial);
    furi_hal_serial_control_release(v->serial);
    v->serial = NULL;

    v->running = false;
    verifier_report_progress(v); // the "done" edge, unless we were cancelled
    return 0;
}

/* ------------------------------------------------------------------ api --- */

Verifier* verifier_alloc(void) {
    Verifier* v = malloc(sizeof(Verifier));
    memset(v, 0, sizeof(Verifier));
    return v;
}

void verifier_free(Verifier* v) {
    furi_assert(v);
    verifier_stop(v);
    free(v);
}

void verifier_set_callback(Verifier* v, VerifierProgressCallback cb, void* context) {
    furi_assert(v);
    v->callback = cb;
    v->context = context;
}

bool verifier_start(Verifier* v, HermesPort port, const uint32_t* bauds, size_t count) {
    furi_assert(v);
    furi_assert(bauds);
    if(v->running) return false;
    if(count == 0) return false;
    if(count > HERMES_BAUD_COUNT) count = HERMES_BAUD_COUNT;

    /* A run that finished on its own leaves its handle behind. Reclaim it, or
     * the next alloc below would drop the reference and leak the thread. */
    if(v->thread) {
        furi_thread_join(v->thread);
        furi_thread_free(v->thread);
        v->thread = NULL;
    }

    memset(&v->report, 0, sizeof(VerifierReport));
    for(size_t i = 0; i < count; i++) {
        v->bauds[i] = bauds[i];
    }
    v->baud_count = count;
    v->port = port;
    v->stop_req = false;
    v->progress = 0;
    v->current_baud = bauds[0];
    v->running = true;

    v->thread = furi_thread_alloc_ex("HermesVerify", 2048, verifier_worker, v);
    furi_thread_start(v->thread);
    return true;
}

void verifier_stop(Verifier* v) {
    furi_assert(v);
    v->stop_req = true;
    if(v->thread) {
        furi_thread_join(v->thread);
        furi_thread_free(v->thread);
        v->thread = NULL;
    }
    v->running = false;
}

bool verifier_is_running(const Verifier* v) {
    furi_assert(v);
    return v->running;
}

uint8_t verifier_progress(const Verifier* v) {
    furi_assert(v);
    return v->progress;
}

uint32_t verifier_current_baud(const Verifier* v) {
    furi_assert(v);
    return v->current_baud;
}

const VerifierReport* verifier_report(const Verifier* v) {
    furi_assert(v);
    return &v->report;
}
