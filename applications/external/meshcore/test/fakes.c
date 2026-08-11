#include "fakes.h"

#include <furi.h>

/* ======================================================================== *
 *  Fake clock
 * ======================================================================== */
static uint32_t g_tick;

uint32_t furi_get_tick(void) {
    return g_tick;
}

uint32_t furi_ms_to_ticks(uint32_t milliseconds) {
    return milliseconds; /* on the Flipper a tick is 1 ms */
}

void fake_clock_reset(void) {
    g_tick = 0;
}

void fake_clock_advance(uint32_t ms) {
    g_tick += ms;
}

/* ======================================================================== *
 *  Fake UART
 * ======================================================================== */
#define FAKE_BUF 4096

struct MeshCoreUart {
    int dummy;
};

static struct MeshCoreUart g_uart;

static uint8_t g_rx[FAKE_BUF];
static size_t g_rx_len;
static size_t g_rx_pos;

static uint8_t g_tx[FAKE_BUF];
static size_t g_tx_len;

static uint32_t g_rx_errors;
static uint32_t g_starved;

void fake_uart_reset(void) {
    g_rx_len = g_rx_pos = g_tx_len = 0;
    g_rx_errors = 0;
    g_starved = 0;
}

void fake_uart_push_rx(const uint8_t* data, size_t len) {
    assert(g_rx_len + len <= FAKE_BUF);
    memcpy(g_rx + g_rx_len, data, len);
    g_rx_len += len;
}

const uint8_t* fake_uart_tx_data(void) {
    return g_tx;
}

size_t fake_uart_tx_len(void) {
    return g_tx_len;
}

void fake_uart_set_rx_errors(uint32_t count) {
    g_rx_errors = count;
}

uint32_t fake_uart_starved_reads(void) {
    return g_starved;
}

MeshCoreUart* meshcore_uart_open(FuriHalSerialId serial_id) {
    UNUSED(serial_id); /* the fake serves whichever port is asked for */
    return &g_uart;
}

void meshcore_uart_close(MeshCoreUart* uart) {
    UNUSED(uart);
}

void meshcore_uart_tx(MeshCoreUart* uart, const uint8_t* data, size_t len) {
    UNUSED(uart);
    assert(g_tx_len + len <= FAKE_BUF);
    memcpy(g_tx + g_tx_len, data, len);
    g_tx_len += len;
}

size_t meshcore_uart_rx(MeshCoreUart* uart, uint8_t* buf, size_t cap, uint32_t timeout_ms) {
    UNUSED(uart);

    size_t available = g_rx_len - g_rx_pos;
    if(available == 0) {
        /* Model a blocking read that waits the whole timeout and comes back
         * empty. Advancing the clock here is what lets deadline logic
         * terminate instead of spinning. */
        g_starved++;
        fake_clock_advance(timeout_ms);
        return 0;
    }

    size_t n = available < cap ? available : cap;
    memcpy(buf, g_rx + g_rx_pos, n);
    g_rx_pos += n;

    /* Reading always costs a little time, so a stream of frames still makes
     * progress towards a deadline. */
    fake_clock_advance(1);
    return n;
}

void meshcore_uart_rx_flush(MeshCoreUart* uart) {
    UNUSED(uart);
    g_rx_pos = g_rx_len = 0;
}

uint32_t meshcore_uart_rx_errors(const MeshCoreUart* uart) {
    UNUSED(uart);
    return g_rx_errors;
}

/* ======================================================================== *
 *  Fake log
 * ======================================================================== */
struct MeshCoreLog {
    int dummy;
};

static struct MeshCoreLog g_log;
static uint32_t g_frames_tx;
static uint32_t g_frames_rx;

void fake_log_reset(void) {
    g_frames_tx = g_frames_rx = 0;
}

MeshCoreLog* fake_log_instance(void) {
    return &g_log;
}

uint32_t fake_log_frame_count(bool tx) {
    return tx ? g_frames_tx : g_frames_rx;
}

MeshCoreLog* meshcore_log_alloc(void) {
    return &g_log;
}

void meshcore_log_free(MeshCoreLog* log) {
    UNUSED(log);
}

void meshcore_log_printf(MeshCoreLog* log, const char* format, ...) {
    UNUSED(log);
    UNUSED(format);
}

void meshcore_log_frame(MeshCoreLog* log, bool tx, const uint8_t* payload, size_t len) {
    UNUSED(log);
    UNUSED(payload);
    UNUSED(len);
    if(tx) {
        g_frames_tx++;
    } else {
        g_frames_rx++;
    }
}

void meshcore_log_snapshot(MeshCoreLog* log, FuriString* out) {
    UNUSED(log);
    UNUSED(out);
}

void meshcore_log_clear(MeshCoreLog* log) {
    UNUSED(log);
}

uint32_t meshcore_log_revision(MeshCoreLog* log) {
    UNUSED(log);
    return g_frames_tx + g_frames_rx;
}
