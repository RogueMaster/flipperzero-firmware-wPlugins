#include "selftest.h"

#include <furi_hal_serial.h>
#include <string.h>

/* Slow to fast: a cable that passes 9600 but fails 921600 is marginal, not
 * dead, and that is a genuinely different diagnosis. */
static const uint32_t selftest_rates[SELFTEST_STEPS] = {9600, 115200, 460800, 921600};

/* 0x55 is alternating bits - the pattern that exercises the line hardest. The
 * rest catch a wire that only passes certain levels. */
static const uint8_t selftest_pattern[] = {0x55, 0xAA, 0x00, 0xFF, 'H', 'e', 'r', 'm', 'e', 's'};
#define SELFTEST_PATTERN_LEN (sizeof(selftest_pattern))

#define SELFTEST_SETTLE_MS (60u)

struct SelfTest {
    FuriThread* thread;
    FuriHalSerialHandle* serial;
    HermesPort port;

    volatile bool running;
    volatile bool stop_req;
    volatile uint8_t progress;

    /* filled by the rx interrupt during a step */
    volatile uint16_t rx_count;
    uint8_t rx_buf[SELFTEST_PATTERN_LEN];

    SelfTestResult result;

    SelfTestCallback callback;
    void* context;
};

static void selftest_rx_isr(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* ctx) {
    SelfTest* st = ctx;
    if(!(event & FuriHalSerialRxEventData)) return;

    while(furi_hal_serial_async_rx_available(handle)) {
        const uint8_t b = furi_hal_serial_async_rx(handle);
        if(st->rx_count < SELFTEST_PATTERN_LEN) {
            st->rx_buf[st->rx_count] = b;
        }
        st->rx_count++;
    }
}

static void selftest_notify(SelfTest* st) {
    /* Same rule as the verifier: stay quiet once a stop is pending, because
     * the thread waiting on us is the one that drains the event queue. */
    if(st->callback && !st->stop_req) st->callback(st->context);
}

static void selftest_run_step(SelfTest* st, uint32_t baud, SelfTestStep* step) {
    furi_hal_serial_async_rx_stop(st->serial);
    furi_hal_serial_set_br(st->serial, baud);
    hermes_framing_apply(st->serial, HermesFraming8N1);

    st->rx_count = 0;
    memset(st->rx_buf, 0, sizeof(st->rx_buf));

    furi_hal_serial_async_rx_start(st->serial, selftest_rx_isr, st, false);

    furi_hal_serial_tx(st->serial, selftest_pattern, SELFTEST_PATTERN_LEN);
    furi_hal_serial_tx_wait_complete(st->serial);

    /* The last byte is still in flight when tx_wait_complete returns. */
    furi_delay_ms(SELFTEST_SETTLE_MS);
    furi_hal_serial_async_rx_stop(st->serial);

    uint16_t matched = 0;
    const uint16_t got = st->rx_count;
    for(uint16_t i = 0; i < SELFTEST_PATTERN_LEN && i < got; i++) {
        if(st->rx_buf[i] == selftest_pattern[i]) matched++;
    }

    step->baud = baud;
    step->sent = SELFTEST_PATTERN_LEN;
    step->echoed = matched;
    step->ok = (matched == SELFTEST_PATTERN_LEN) && (got == SELFTEST_PATTERN_LEN);
}

static int32_t selftest_worker(void* ctx) {
    SelfTest* st = ctx;
    SelfTestResult* r = &st->result;

    st->serial = furi_hal_serial_control_acquire(hermes_port_serial_id(st->port));
    if(!st->serial) {
        r->verdict = SelfTestFailed;
        st->running = false;
        selftest_notify(st);
        return 0;
    }

    furi_hal_serial_init(st->serial, selftest_rates[0]);

    for(uint32_t i = 0; i < SELFTEST_STEPS && !st->stop_req; i++) {
        selftest_run_step(st, selftest_rates[i], &r->steps[i]);
        r->completed++;
        st->progress = (uint8_t)((r->completed * 100u) / SELFTEST_STEPS);
        selftest_notify(st);
    }

    uint8_t passed = 0;
    for(uint8_t i = 0; i < r->completed; i++) {
        if(r->steps[i].ok) passed++;
    }

    if(r->completed == 0) {
        r->verdict = SelfTestFailed;
    } else if(passed == r->completed) {
        r->verdict = SelfTestPassed;
    } else if(passed == 0) {
        r->verdict = SelfTestFailed;
    } else {
        r->verdict = SelfTestPartial;
    }

    furi_hal_serial_deinit(st->serial);
    furi_hal_serial_control_release(st->serial);
    st->serial = NULL;

    st->progress = 100;
    st->running = false;
    selftest_notify(st);
    return 0;
}

/* ------------------------------------------------------------------ api --- */

SelfTest* selftest_alloc(void) {
    SelfTest* st = malloc(sizeof(SelfTest));
    memset(st, 0, sizeof(SelfTest));
    return st;
}

void selftest_free(SelfTest* st) {
    furi_assert(st);
    selftest_stop(st);
    free(st);
}

void selftest_set_callback(SelfTest* st, SelfTestCallback cb, void* context) {
    furi_assert(st);
    st->callback = cb;
    st->context = context;
}

bool selftest_start(SelfTest* st, HermesPort port) {
    furi_assert(st);
    if(st->running) return false;

    if(st->thread) { // reclaim a finished run's handle
        furi_thread_join(st->thread);
        furi_thread_free(st->thread);
        st->thread = NULL;
    }

    memset(&st->result, 0, sizeof(SelfTestResult));
    st->port = port;
    st->stop_req = false;
    st->progress = 0;
    st->running = true;

    st->thread = furi_thread_alloc_ex("HermesSelfTest", 1024, selftest_worker, st);
    furi_thread_start(st->thread);
    return true;
}

void selftest_stop(SelfTest* st) {
    furi_assert(st);
    st->stop_req = true;
    if(st->thread) {
        furi_thread_join(st->thread);
        furi_thread_free(st->thread);
        st->thread = NULL;
    }
    st->running = false;
}

bool selftest_is_running(const SelfTest* st) {
    furi_assert(st);
    return st->running;
}

uint8_t selftest_progress(const SelfTest* st) {
    furi_assert(st);
    return st->progress;
}

const SelfTestResult* selftest_result(const SelfTest* st) {
    furi_assert(st);
    return &st->result;
}

const char* selftest_verdict_text(SelfTestVerdict verdict) {
    switch(verdict) {
    case SelfTestPassed:
        return "Wiring is good";
    case SelfTestPartial:
        return "Marginal at speed";
    case SelfTestFailed:
        return "Nothing came back";
    case SelfTestRunning:
        return "Testing...";
    case SelfTestIdle:
    default:
        return "Bridge 13 to 14";
    }
}
