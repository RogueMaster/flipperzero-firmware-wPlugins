/* Wait decisions of the NFC reader worker (nfc_read.c), kept free of the
 * Flipper SDK so they are tested on the host.
 *
 * The SDK's sync pollers (iso14443_3a_poller_sync_read and the MIFARE ones)
 * wait on thread flag bit 0 of the calling thread and clear it, so the
 * reader's own flags must stay off bit 0: a stop flag there could be
 * swallowed by a poll. The stop flag only wakes the worker early; the
 * `stopping` variable is what decides, and every wait is bounded.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define READER_FLAG_STOP     (1U << 8)
#define READER_FLAG_DETECTED (1U << 9)

#define READER_FLAG_ERROR_BIT 0x80000000U /* FuriFlagError */

/* Longest a worker waits before it looks at `stopping` again. */
#define READER_WAIT_MS 200

/* Whether to stop after furi_thread_flags_wait() returned `flags` (an error
 * status such as a timeout has the top bit set). */
bool reader_wait_stop(uint32_t flags, bool stopping);

/* Whether a sync read found a tag. A poller woken early by a stray flag
 * returns success with a zeroed result, so a UID length of 0 is no tag. */
bool reader_tag_answered(bool read_ok, uint8_t uid_len);
