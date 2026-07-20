#pragma once

#include <furi.h>
#include <stdbool.h>
#include <stdint.h>

/* The logbook: a plain-text record of what a sweep found, on the SD card.
 *
 * A bug-sweep is only half useful if the finding evaporates when you walk out of
 * the room. Each entry is RTC-stamped and readable straight off the card in any
 * text editor - no export step, no proprietary format:
 *
 *   2026-07-18 14:32:07
 *     READER POLLING
 *     204ms b20 d10% c88%
 *   2026-07-18 14:35:11
 *     SURVEY 60s
 *     CLEAN
 *     mx4% av1% f0% h0
 *
 * Detail lines are kept inside ~21 characters so they survive the on-device
 * text box without wrapping into soup. Nothing leaves the device. */

#define SPECTER_LOG_TAIL_BYTES 3072u // how much of the tail the viewer shows

/* Append one RTC-stamped line. Returns false if the card is missing or full. */
bool specter_log_append(const char* fmt, ...);

/* Read the last SPECTER_LOG_TAIL_BYTES of the log into `out`, trimmed to start
 * at a line boundary. Returns false if there is nothing to show. */
bool specter_log_read_tail(FuriString* out);

/* Truncate the logbook. */
bool specter_log_clear(void);

/* Size in bytes, 0 if absent. */
uint32_t specter_log_size(void);
