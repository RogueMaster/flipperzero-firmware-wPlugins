/**
 * Ring buffer for the Desktop-mode transcript view.
 *
 * Data sources:
 *   1. Heartbeat `entries` array — desktop re-sends the full recent list
 *      on every snapshot.  nus_transcript_replace_from_entries() wipes
 *      and repopulates.
 *   2. Assistant `turn` events — text blocks from the content array are
 *      appended with nus_transcript_append_turn_text().
 *
 * The buffer is guarded by a mutex so the transport thread (writers)
 * and the GUI draw callback (reader) can't clash.  Readers copy into
 * caller-provided buffers to avoid holding the lock.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NUS_TRANSCRIPT_CAP      16
#define NUS_TRANSCRIPT_LINE_MAX 96

void nus_transcript_init(void);
void nus_transcript_free(void);
void nus_transcript_reset(void);

/* Parse a JSON array body (no surrounding brackets) like
 *   "10:42 git push","10:41 yarn test"
 * and replace the buffer with its contents (newest first). */
void nus_transcript_replace_from_entries(const char* body, int body_len);

/* Append a single line (copied). Used for turn-event text blocks. */
void nus_transcript_append(const char* line);

/* Returns the current line count (newest-first ordering). */
int nus_transcript_count(void);

/* Copy line `idx` (0 = newest) into `out`.  Returns true if idx was
 * in range. `out` is NUL-terminated even on truncation. */
bool nus_transcript_get(int idx, char* out, int out_size);

#ifdef __cplusplus
}
#endif
