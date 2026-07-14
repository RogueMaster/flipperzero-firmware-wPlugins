#include "http.h"

#include <furi.h>
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>
#include <expansion/expansion.h>
#include <string.h>

#define FHTTP_BAUD    115200
#define FHTTP_RX_SIZE 2048

struct FhttpClient {
    FuriHalSerialHandle* serial;
    Expansion* expansion;
    FuriStreamBuffer* rx;
    bool open;
};

// Runs in interrupt context — just push bytes into the stream buffer.
static void fhttp_rx_cb(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    FhttpClient* c = context;
    if(event & FuriHalSerialRxEventData) {
        while(furi_hal_serial_async_rx_available(handle)) {
            uint8_t b = furi_hal_serial_async_rx(handle);
            furi_stream_buffer_send(c->rx, &b, 1, 0);
        }
    }
}

FhttpClient* fhttp_alloc(void) {
    FhttpClient* c = malloc(sizeof(FhttpClient));
    c->serial = NULL;
    c->expansion = NULL;
    c->open = false;
    c->rx = furi_stream_buffer_alloc(FHTTP_RX_SIZE, 1);
    return c;
}

void fhttp_free(FhttpClient* c) {
    if(c->open) fhttp_close(c);
    furi_stream_buffer_free(c->rx);
    free(c);
}

bool fhttp_open(FhttpClient* c) {
    if(c->open) return true;
    c->expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(c->expansion);

    c->serial = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    if(!c->serial) {
        expansion_enable(c->expansion);
        furi_record_close(RECORD_EXPANSION);
        c->expansion = NULL;
        return false;
    }
    furi_hal_serial_init(c->serial, FHTTP_BAUD);
    furi_stream_buffer_reset(c->rx);
    furi_hal_serial_async_rx_start(c->serial, fhttp_rx_cb, c, false);
    c->open = true;
    return true;
}

void fhttp_close(FhttpClient* c) {
    if(!c->open) return;
    furi_hal_serial_async_rx_stop(c->serial);
    furi_hal_serial_deinit(c->serial);
    furi_hal_serial_control_release(c->serial);
    c->serial = NULL;
    expansion_enable(c->expansion);
    furi_record_close(RECORD_EXPANSION);
    c->expansion = NULL;
    c->open = false;
}

// --- low level ---

static void fhttp_send_line(FhttpClient* c, const char* s) {
    furi_hal_serial_tx(c->serial, (const uint8_t*)s, strlen(s));
    furi_hal_serial_tx(c->serial, (const uint8_t*)"\n", 1);
    furi_hal_serial_tx_wait_complete(c->serial);
}

static bool contains(const char* hay, const char* needle) {
    size_t nl = strlen(needle);
    for(const char* h = hay; *h; h++) {
        size_t i = 0;
        while(needle[i] && h[i] == needle[i]) i++;
        if(i == nl) return true;
    }
    return false;
}

// Accumulate incoming bytes (rolling window) until any of a/b appears, or
// [ERROR]/timeout. b may be NULL.
static bool fhttp_wait_any(FhttpClient* c, const char* a, const char* b, uint32_t timeout_ms) {
    char buf[256];
    size_t len = 0;
    buf[0] = '\0';
    uint32_t start = furi_get_tick();
    uint32_t to = furi_ms_to_ticks(timeout_ms);
    while(furi_get_tick() - start < to) {
        uint8_t ch;
        if(furi_stream_buffer_receive(c->rx, &ch, 1, furi_ms_to_ticks(50)) == 0) continue;
        if(len < sizeof(buf) - 1) {
            buf[len++] = (char)ch;
        } else {
            memmove(buf, buf + 1, sizeof(buf) - 2);
            buf[sizeof(buf) - 2] = (char)ch;
            len = sizeof(buf) - 1;
        }
        buf[len] = '\0';
        if(contains(buf, a)) return true;
        if(b && contains(buf, b)) return true;
        if(contains(buf, "[ERROR]")) return false;
    }
    return false;
}

bool fhttp_ping(FhttpClient* c) {
    if(!c->open) return false;
    furi_stream_buffer_reset(c->rx);
    fhttp_send_line(c, "[PING]");
    return fhttp_wait_any(c, "[PONG]", NULL, 2000);
}

bool fhttp_wifi(FhttpClient* c, const char* ssid, const char* pass) {
    if(!c->open) return false;
    char cmd[256];
    snprintf(
        cmd, sizeof(cmd), "[WIFI/SAVE]{\"ssid\":\"%s\",\"password\":\"%s\"}", ssid, pass);
    furi_stream_buffer_reset(c->rx);
    fhttp_send_line(c, cmd);
    fhttp_wait_any(c, "[SUCCESS]", NULL, 5000); // best-effort; connect confirms

    furi_stream_buffer_reset(c->rx);
    fhttp_send_line(c, "[WIFI/CONNECT]");
    return fhttp_wait_any(c, "CONNECTED", "SUCCESS", 20000);
}

bool fhttp_get(
    FhttpClient* c,
    const char* url,
    const char* const* headers,
    int header_count,
    char* out,
    size_t out_cap) {
    if(!c->open) return false;

    char cmd[512];
    int n = snprintf(cmd, sizeof(cmd), "[GET/HTTP]{\"url\":\"%s\",\"headers\":{", url);
    for(int i = 0; i < header_count && n < (int)sizeof(cmd) - 8; i++) {
        const char* h = headers[i];
        const char* colon = h;
        while(*colon && *colon != ':') colon++;
        if(!*colon) continue;
        char name[64];
        size_t nl = (size_t)(colon - h);
        if(nl > sizeof(name) - 1) nl = sizeof(name) - 1;
        memcpy(name, h, nl);
        name[nl] = '\0';
        const char* val = colon + 1;
        while(*val == ' ') val++;
        n += snprintf(
            cmd + n, sizeof(cmd) - n, "%s\"%s\":\"%s\"", (i ? "," : ""), name, val);
    }
    if(n < (int)sizeof(cmd) - 2) n += snprintf(cmd + n, sizeof(cmd) - n, "}}");

    furi_stream_buffer_reset(c->rx);
    fhttp_send_line(c, cmd);

    if(!fhttp_wait_any(c, "[GET/SUCCESS]", NULL, 15000)) return false;

    // Collect body until [GET/END].
    const char* endm = "[GET/END]";
    size_t el = strlen(endm);
    size_t len = 0;
    out[0] = '\0';
    uint32_t start = furi_get_tick();
    uint32_t to = furi_ms_to_ticks(15000);
    while(furi_get_tick() - start < to) {
        uint8_t ch;
        if(furi_stream_buffer_receive(c->rx, &ch, 1, furi_ms_to_ticks(50)) == 0) continue;
        if(len < out_cap - 1) {
            out[len++] = (char)ch;
            out[len] = '\0';
        }
        if(len >= el && memcmp(out + len - el, endm, el) == 0) {
            len -= el;
            while(len > 0 && (out[len - 1] == '\n' || out[len - 1] == '\r' || out[len - 1] == ' '))
                len--;
            out[len] = '\0';
            return true;
        }
    }
    return false;
}
