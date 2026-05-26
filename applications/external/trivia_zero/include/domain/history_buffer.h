#pragma once

#include <stdbool.h>
#include <stdint.h>

#define HISTORY_CAPACITY 5u

typedef struct {
    uint32_t ids[HISTORY_CAPACITY];
    uint8_t head; /* index of next slot to write */
    uint8_t len;  /* count of valid entries (<= HISTORY_CAPACITY) */
} HistoryBuffer;

void history_buffer_init(HistoryBuffer *h);
void history_buffer_clear(HistoryBuffer *h);
void history_buffer_push(HistoryBuffer *h, uint32_t id);

/* Peek `steps` positions back from the most-recent entry.
 * steps == 0 returns the most recent. Returns false if steps >= len. */
bool history_buffer_peek_back(const HistoryBuffer *h, uint8_t steps, uint32_t *out);

uint8_t history_buffer_len(const HistoryBuffer *h);
