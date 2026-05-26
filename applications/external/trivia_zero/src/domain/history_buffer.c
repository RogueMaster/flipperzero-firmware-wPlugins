#include "include/domain/history_buffer.h"
#include <string.h>

void history_buffer_init(HistoryBuffer *h) {
    if (!h) {
        return;
    }
    memset(h, 0, sizeof(*h));
}

void history_buffer_clear(HistoryBuffer *h) {
    history_buffer_init(h);
}

void history_buffer_push(HistoryBuffer *h, uint32_t id) {
    if (!h) {
        return;
    }
    h->ids[h->head] = id;
    h->head = (uint8_t)((h->head + 1u) % HISTORY_CAPACITY);
    if (h->len < HISTORY_CAPACITY) {
        h->len++;
    }
}

bool history_buffer_peek_back(const HistoryBuffer *h, uint8_t steps, uint32_t *out) {
    if (!h || !out || steps >= h->len) {
        return false;
    }
    /* head points to next-write slot. The most recent entry is at (head - 1). */
    const uint8_t idx = (uint8_t)((h->head + HISTORY_CAPACITY - 1u - steps) % HISTORY_CAPACITY);
    *out = h->ids[idx];
    return true;
}

uint8_t history_buffer_len(const HistoryBuffer *h) {
    return h ? h->len : 0u;
}
