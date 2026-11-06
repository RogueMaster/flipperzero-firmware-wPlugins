/* Waits for the tag the reader read last to leave the field before the
 * reader looks for a tag again (nfc_read.c), so that a spool left on the
 * Flipper is not read over and over, on the result or any later screen.
 *
 * The reader polls the field every TAG_WATCH_POLL_MS and reports the UID
 * that answered (length 0 when none did). The remembered tag is gone after
 * TAG_WATCH_MISSES polls in a row without an answer, so one flaky poll does
 * not count as a removal, or at once when another tag answers.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "tag_dump.h"

#define TAG_WATCH_MISSES  3
#define TAG_WATCH_POLL_MS 150

typedef struct {
    uint8_t uid[TAG_DUMP_MAX_UID];
    uint8_t uid_len; /* 0: no tag to wait for */
    uint8_t misses;
} TagWatch;

void tag_watch_clear(TagWatch* watch);

/* Remembers the tag just read; a length of 0 or above TAG_DUMP_MAX_UID
 * clears the watch. */
void tag_watch_set(TagWatch* watch, const uint8_t* uid, size_t len);

/* Whether the remembered tag has not left yet. */
bool tag_watch_waiting(const TagWatch* watch);

/* One poll; returns tag_watch_waiting() afterwards. */
bool tag_watch_poll(TagWatch* watch, const uint8_t* uid, size_t len);
