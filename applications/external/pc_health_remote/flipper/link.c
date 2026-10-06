#include "link.h"
#include <stdlib.h>
#include <string.h>

struct PhrLink {
    FuriMutex* mutex;
    PhrStream stream;
    PhrTelemetry last;
    bool have_any;
    uint32_t frames;
    uint32_t last_rx_ms;
    bool connected;
    bool hello_force;
    uint32_t last_hello_ms;
    bool hello_sent_once;
    uint8_t hello_seq;
};

PhrLink* phr_link_alloc(void) {
    PhrLink* l = malloc(sizeof(PhrLink));
    memset(l, 0, sizeof(PhrLink));
    l->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    return l;
}

void phr_link_free(PhrLink* l) {
    furi_mutex_free(l->mutex);
    free(l);
}

void phr_link_reset(PhrLink* l) {
    furi_mutex_acquire(l->mutex, FuriWaitForever);
    phr_stream_reset(&l->stream);
    l->have_any = false;
    l->frames = 0;
    l->connected = false;
    l->hello_force = false;
    l->hello_sent_once = false;
    furi_mutex_release(l->mutex);
}

static void on_frame(const PhrTelemetry* t, void* ctx) {
    PhrLink* l = ctx;
    l->last = *t;
    l->have_any = true;
    l->frames++;
    l->last_rx_ms = furi_get_tick();
}

void phr_link_feed(PhrLink* l, const uint8_t* data, size_t len) {
    furi_mutex_acquire(l->mutex, FuriWaitForever);
    phr_stream_feed(&l->stream, data, len, on_frame, l);
    furi_mutex_release(l->mutex);
}

void phr_link_set_connected(PhrLink* l, bool connected) {
    furi_mutex_acquire(l->mutex, FuriWaitForever);
    if(connected && !l->connected) l->hello_force = true;
    if(!connected) phr_stream_reset(&l->stream);
    l->connected = connected;
    furi_mutex_release(l->mutex);
}

void phr_link_snapshot(PhrLink* l, PhrLinkSnapshot* out) {
    furi_mutex_acquire(l->mutex, FuriWaitForever);
    out->have_any = l->have_any;
    out->frames = l->frames;
    out->connected = l->connected;
    out->telemetry = l->last;
    out->silence_ms = l->have_any ? (furi_get_tick() - l->last_rx_ms) : 0;
    if(!l->have_any)
        out->state = PhrLinkWaiting;
    else
        out->state = (out->silence_ms >= PHR_LINK_LOST_MS) ? PhrLinkLost : PhrLinkActive;
    furi_mutex_release(l->mutex);
}

bool phr_link_hello_due(PhrLink* l) {
    bool due = false;
    furi_mutex_acquire(l->mutex, FuriWaitForever);
    if(l->connected) {
        uint32_t now = furi_get_tick();
        bool fast = !l->have_any || (now - l->last_rx_ms) >= PHR_LINK_LOST_MS;
        uint32_t interval = fast ? PHR_HELLO_FAST_MS : PHR_HELLO_SLOW_MS;
        if(l->hello_force || !l->hello_sent_once || (now - l->last_hello_ms) >= interval) {
            due = true;
            l->hello_force = false;
            l->hello_sent_once = true;
            l->last_hello_ms = now;
        }
    }
    furi_mutex_release(l->mutex);
    return due;
}

size_t phr_link_build_hello(PhrLink* l, uint8_t transport, uint8_t* buf) {
    furi_mutex_acquire(l->mutex, FuriWaitForever);
    uint8_t seq = l->hello_seq++;
    furi_mutex_release(l->mutex);
    return phr_build_hello(buf, seq, transport, 1);
}
