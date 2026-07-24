#pragma once

#include <furi.h>
#include <stdbool.h>
#include <stdint.h>

/* The logbook: a plain-text record of what a sweep found, on the SD card.
 *
 * A bug-sweep is only half useful if the finding evaporates when you walk out of
 * the room. Every entry is written twice, from one call, so it is both readable
 * on the device and ready for a report without an export step:
 *
 *   logbook.txt   - grouped for the on-device viewer
 *       2026-07-18 14:35:11
 *         SURVEY  60s ACTIVE max74 avg21 infield38 hits5
 *
 *   logbook.csv   - one flat row per entry, opens straight in a spreadsheet
 *       timestamp,type,detail
 *       2026-07-18 14:35:11,SURVEY,60s ACTIVE max74 avg21 infield38 hits5
 *
 * Nothing leaves the device. Details must not contain a comma (it is the CSV
 * separator) - callers phrase metrics without one. */

#define SPECTER_LOG_TAIL_BYTES 3072u // how much of the .txt tail the viewer shows

/* Append one RTC-stamped entry under a short type tag ("READER", "SURVEY",
 * "SWEEP", "WATCH"). The detail is a single line, comma-free. Returns false if
 * the card is missing or full. */
bool specter_log_append(const char* type, const char* fmt, ...)
    __attribute__((format(printf, 2, 3)));

/* Read the last SPECTER_LOG_TAIL_BYTES of the .txt log into `out`, trimmed to
 * start at a line boundary. Returns false if there is nothing to show. */
bool specter_log_read_tail(FuriString* out);

/* Truncate both the .txt and .csv logbooks. */
bool specter_log_clear(void);

/* Size of the .txt logbook in bytes, 0 if absent. */
uint32_t specter_log_size(void);
