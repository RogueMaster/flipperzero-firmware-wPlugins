#include "uart_listen.h"

#include <furi.h>
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>
#include <furi_hal_resources.h>
#include <expansion/expansion.h>

#define TAG "UartListen"

// Enough for a GPS sentence or a couple of PM-sensor frames. Anything past this
// is dropped in the interrupt rather than blocking it: what matters is that
// something is talking and at which rate, not capturing all of it.
#define UART_RX_BUFFER 256

typedef struct {
    FuriStreamBuffer* rx;
    volatile uint32_t frame_errors;
} UartRxContext;

// Runs in an interrupt. Nothing here may block, allocate, or take a mutex --
// which is why the byte goes into a stream buffer with a zero timeout and the
// error count is a plain increment.
static void uart_rx_callback(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* ctx) {
    UartRxContext* c = ctx;
    if(event & FuriHalSerialRxEventData) {
        uint8_t byte = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(c->rx, &byte, 1, 0);
    }
    // A wrong baud rate does not read as silence, it reads as garbage with
    // framing errors. Counting them is what lets the sweep pick a rate on
    // evidence instead of on the first thing that produced bytes.
    if(event & (FuriHalSerialRxEventFrameError | FuriHalSerialRxEventNoiseError)) {
        c->frame_errors++;
    }
}

// The expansion service owns LPUART and will fight for it. Standing it down is
// a documented requirement, not a precaution -- and the record may not exist on
// every firmware this app is built for, so its absence is not an error.
static Expansion* uart_expansion_stand_down(void) {
    if(!furi_record_exists(RECORD_EXPANSION)) return NULL;
    Expansion* expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(expansion);
    return expansion;
}

static void uart_expansion_restore(Expansion* expansion) {
    if(!expansion) return;
    expansion_enable(expansion);
    furi_record_close(RECORD_EXPANSION);
}

// Trusting a comment about which pin is which is how you end up debugging the
// wrong wire. The HAL knows; ask it.
static bool uart_pinout_is_expected(FuriHalSerialHandle* handle) {
    const GpioPin* rx = furi_hal_serial_get_gpio_pin(handle, FuriHalSerialDirectionRx);
    const GpioPin* tx = furi_hal_serial_get_gpio_pin(handle, FuriHalSerialDirectionTx);
    return rx == &gpio_ext_pc0 && tx == &gpio_ext_pc1;
}

// Acquire, run body, release -- in one function with no early return between
// the halves. A leaked serial handle takes the expansion service down until the
// Flipper is rebooted, which is a far worse outcome than any failure this code
// is trying to report.
typedef bool (*UartSession)(FuriHalSerialHandle* handle, UartRxContext* rx, void* context);

//
// ran, when given, reports whether the body was reached at all. Every reason it
// might not be -- the handle held elsewhere, the peripheral on different pins --
// leaves the caller knowing nothing about the wire, which is a different answer
// from having listened and heard silence.
static bool uart_with_lpuart(uint32_t baud, UartSession body, void* context, bool* ran) {
    if(ran) *ran = false;
    if(!body) return false;

    Expansion* expansion = uart_expansion_stand_down();
    FuriHalSerialHandle* handle = furi_hal_serial_control_acquire(FuriHalSerialIdLpuart);
    bool ok = false;

    if(!handle) {
        // Somebody else has it. A screen, not an assert.
        FURI_LOG_W(TAG, "LPUART busy");
    } else {
        furi_hal_serial_init(handle, baud);

        if(!uart_pinout_is_expected(handle)) {
            // The pins are the entire premise: the same two holes the I2C bus
            // is already in. If a firmware moves LPUART somewhere else, every
            // instruction this app gives about pin 15 and pin 16 becomes wrong,
            // so it stops rather than talking to whatever is there.
            FURI_LOG_E(TAG, "LPUART is not on PC0/PC1 on this firmware");
        } else {
            UartRxContext rx = {
                .rx = furi_stream_buffer_alloc(UART_RX_BUFFER, 1),
                .frame_errors = 0,
            };
            furi_hal_serial_async_rx_start(handle, uart_rx_callback, &rx, true);
            if(ran) *ran = true;
            ok = body(handle, &rx, context);
            furi_hal_serial_async_rx_stop(handle);
            furi_stream_buffer_free(rx.rx);
        }

        furi_hal_serial_deinit(handle);
        furi_hal_serial_control_release(handle);
    }

    uart_expansion_restore(expansion);
    return ok;
}

/* ---- listening ---- */

typedef struct {
    const uint32_t* bauds;
    size_t baud_count;
    uint32_t window_ms;
    const volatile bool* abort;
    UartListenResult best;
    bool found;
} ListenJob;

// The window, waited in slices so that leaving the app does not have to sit
// through the rest of one. Returns false if the flag went up: the caller still
// reads out what arrived before that, because bytes heard are bytes heard.
static bool listen_wait(uint32_t ms, const volatile bool* abort) {
    const uint32_t slice = 25;
    for(uint32_t waited = 0; waited < ms; waited += slice) {
        if(abort && *abort) return false;
        uint32_t left = ms - waited;
        furi_delay_ms(left < slice ? left : slice);
    }
    return !(abort && *abort);
}

// Clean bytes beat noisy bytes, and more clean bytes beat fewer. A rate that
// produced only framing errors is still kept if nothing better turns up: it
// says "something is transmitting", which is the difference between a silent
// bus and a dead part, even when the rate is wrong.
static bool listen_is_better(const UartListenResult* candidate, const UartListenResult* best) {
    bool candidate_clean = candidate->bytes > 0 && candidate->frame_errors == 0;
    bool best_clean = best->bytes > 0 && best->frame_errors == 0;
    if(candidate_clean != best_clean) return candidate_clean;
    return candidate->bytes > best->bytes;
}

// The whole sweep inside one acquire. Changing rate is what set_br is for, and
// re-acquiring per rate would mean four more chances to leak the handle and
// four more rounds of shutting the expansion service down and bringing it back.
static bool uart_listen_body(FuriHalSerialHandle* handle, UartRxContext* rx, void* context) {
    ListenJob* job = context;

    for(size_t i = 0; i < job->baud_count; i++) {
        uint32_t baud = job->bauds[i];
        if(!furi_hal_serial_is_baud_rate_supported(handle, baud)) continue;

        // Quiet the receiver while the peripheral is reconfigured, then start
        // from an empty buffer and a zero error count so each rate is judged on
        // what arrived at that rate alone.
        furi_hal_serial_async_rx_stop(handle);
        furi_hal_serial_set_br(handle, baud);
        furi_stream_buffer_reset(rx->rx);
        rx->frame_errors = 0;
        furi_hal_serial_async_rx_start(handle, uart_rx_callback, rx, true);

        bool completed = listen_wait(job->window_ms, job->abort);

        uint8_t buffer[UART_RX_BUFFER];
        size_t got = furi_stream_buffer_receive(rx->rx, buffer, sizeof(buffer), 0);

        UartListenResult result = {
            .baud = baud,
            .bytes = got,
            .frame_errors = rx->frame_errors,
            .sample_len = got < UART_LISTEN_MAX_SAMPLE ? got : UART_LISTEN_MAX_SAMPLE,
        };
        memcpy(result.sample, buffer, result.sample_len);

        if(got || result.frame_errors) {
            if(!job->found || listen_is_better(&result, &job->best)) {
                job->best = result;
                job->found = true;
            }
        }
        if(!completed) break; // asked to stop; what was heard is still kept
    }
    return job->found;
}

UartListenOutcome uart_listen_sweep(
    const uint32_t* bauds,
    size_t baud_count,
    uint32_t window_ms,
    const volatile bool* abort,
    UartListenResult* out) {
    furi_check(out);
    memset(out, 0, sizeof(*out));
    if(!bauds || !baud_count) return UartListenUnavailable;

    ListenJob job = {
        .bauds = bauds,
        .baud_count = baud_count,
        .window_ms = window_ms,
        .abort = abort,
    };
    bool ran = false;
    bool heard = uart_with_lpuart(bauds[0], uart_listen_body, &job, &ran);
    if(!ran) return UartListenUnavailable;
    if(!heard) return UartListenSilent;

    *out = job.best;
    return UartListenHeard;
}

/* ---- loopback self-test ---- */

// Deliberately not 0x00 or 0xFF: a stuck line reads as one of those, and a test
// that passes on a stuck line is worse than no test. Mixed bit patterns also
// exercise framing rather than a run of identical edges.
static const uint8_t uart_selftest_pattern[] = {0x55, 0xAA, 0x0F, 0xF0, 0x3C, 0xC3};

// The buffer travels with its size instead of as a bare pointer. The size used
// to live here as a literal 32 that had to keep agreeing with a declaration in
// another function, and that is the kind of agreement that stops being true
// quietly.
typedef struct {
    char* text;
    size_t size;
} SelftestDetail;

static bool uart_selftest_body(FuriHalSerialHandle* handle, UartRxContext* rx, void* context) {
    SelftestDetail* detail = context;

    furi_hal_serial_tx(handle, uart_selftest_pattern, sizeof(uart_selftest_pattern));
    furi_hal_serial_tx_wait_complete(handle);
    furi_delay_ms(50); // the last byte still has to arrive and be handled

    uint8_t got[sizeof(uart_selftest_pattern) * 2] = {0};
    size_t len = furi_stream_buffer_receive(rx->rx, got, sizeof(got), 0);

    bool ok = len == sizeof(uart_selftest_pattern) &&
              memcmp(got, uart_selftest_pattern, len) == 0 && rx->frame_errors == 0;

    if(detail && detail->text && detail->size) {
        if(ok) {
            snprintf(detail->text, detail->size, "%u bytes back, clean", (unsigned)len);
        } else if(len == 0) {
            snprintf(detail->text, detail->size, "nothing came back");
        } else {
            snprintf(
                detail->text,
                detail->size,
                "%u bytes, %lu errors",
                (unsigned)len,
                rx->frame_errors);
        }
    }
    return ok;
}

bool uart_selftest_loopback(uint32_t baud, char* detail, size_t detail_size) {
    // Written first and only overwritten if the body actually ran. Never
    // reaching the wire has to read differently from reaching it and hearing
    // nothing -- on this screen most of all, because it is the one place a
    // user is told the transport itself is sound.
    if(detail && detail_size) snprintf(detail, detail_size, "LPUART unavailable");
    char line[40] = {0};
    SelftestDetail sink = {.text = line, .size = sizeof(line)};
    bool ok = uart_with_lpuart(baud, uart_selftest_body, &sink, NULL);
    if(detail && detail_size && line[0]) snprintf(detail, detail_size, "%s", line);
    return ok;
}
