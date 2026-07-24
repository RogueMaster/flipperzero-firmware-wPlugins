#include "glitch_serial.h"
#include "glitch_config.h" // GLITCH_SUCCESS_MAX

#include <furi.h>
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>
#include <string.h>

#define GLITCH_SERIAL_STREAM 512

struct GlitchSerial {
    FuriHalSerialHandle* handle;
    FuriStreamBuffer* stream;
    bool running;

    char needle[GLITCH_SUCCESS_MAX];
    size_t nlen;
    char win[GLITCH_SUCCESS_MAX]; // sliding window of the last nlen bytes
    size_t wlen;
    bool matched;
};

/* interrupt context: push received bytes into the stream buffer */
static void
    glitch_serial_rx_isr(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    GlitchSerial* s = context;
    if(event & FuriHalSerialRxEventData) {
        while(furi_hal_serial_async_rx_available(handle)) {
            uint8_t b = furi_hal_serial_async_rx(handle);
            furi_stream_buffer_send(s->stream, &b, 1, 0);
        }
    }
}

GlitchSerial* glitch_serial_alloc(void) {
    GlitchSerial* s = malloc(sizeof(GlitchSerial));
    memset(s, 0, sizeof(GlitchSerial));
    s->stream = furi_stream_buffer_alloc(GLITCH_SERIAL_STREAM, 1);
    return s;
}

void glitch_serial_free(GlitchSerial* s) {
    furi_assert(s);
    glitch_serial_stop(s);
    furi_stream_buffer_free(s->stream);
    free(s);
}

bool glitch_serial_start(GlitchSerial* s, uint32_t baud, const char* needle) {
    furi_assert(s);
    if(s->running) glitch_serial_stop(s);
    if(!needle || needle[0] == '\0') return false;

    strncpy(s->needle, needle, GLITCH_SUCCESS_MAX - 1);
    s->needle[GLITCH_SUCCESS_MAX - 1] = '\0';
    s->nlen = strlen(s->needle);
    s->wlen = 0;
    s->matched = false;

    if(furi_hal_serial_control_is_busy(FuriHalSerialIdUsart)) return false;
    s->handle = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    if(!s->handle) return false;

    furi_stream_buffer_reset(s->stream);
    furi_hal_serial_init(s->handle, baud);
    furi_hal_serial_async_rx_start(s->handle, glitch_serial_rx_isr, s, false);
    s->running = true;
    return true;
}

void glitch_serial_stop(GlitchSerial* s) {
    furi_assert(s);
    if(!s->running) return;
    furi_hal_serial_async_rx_stop(s->handle);
    furi_hal_serial_deinit(s->handle);
    furi_hal_serial_control_release(s->handle);
    s->handle = NULL;
    s->running = false;
}

bool glitch_serial_running(const GlitchSerial* s) {
    return s->running;
}

static void glitch_serial_feed(GlitchSerial* s, uint8_t b) {
    if(s->nlen == 0) return;
    if(s->wlen < s->nlen) {
        s->win[s->wlen++] = (char)b;
    } else {
        memmove(s->win, s->win + 1, s->nlen - 1);
        s->win[s->nlen - 1] = (char)b;
    }
    if(s->wlen == s->nlen && memcmp(s->win, s->needle, s->nlen) == 0) {
        s->matched = true;
    }
}

void glitch_serial_poll(GlitchSerial* s) {
    furi_assert(s);
    if(!s->running || s->nlen == 0) return;
    uint8_t buf[64];
    size_t n;
    while((n = furi_stream_buffer_receive(s->stream, buf, sizeof(buf), 0)) > 0) {
        for(size_t i = 0; i < n; i++)
            glitch_serial_feed(s, buf[i]);
    }
}

bool glitch_serial_matched(const GlitchSerial* s) {
    return s->matched;
}

void glitch_serial_clear(GlitchSerial* s) {
    furi_assert(s);
    s->matched = false;
}
