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

/* A hard ceiling on each logbook file.
 *
 * Watch mode can append every few seconds for as long as you leave it standing
 * guard, and nothing here ever deleted anything: left running, this would grow
 * at a few megabytes a day until it filled the card - taking every other app's
 * storage with it. A cap is the honest fix. At roughly 55 bytes an entry this
 * still holds on the order of twenty thousand findings.
 *
 * When it is reached the app stops writing and SAYS so, rather than quietly
 * dropping findings or quietly eating the card. Clear the logbook in Settings
 * to carry on. */
#define SPECTER_LOG_MAX_BYTES (1024u * 1024u)

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

/* True when the logbook has hit SPECTER_LOG_MAX_BYTES and is refusing writes.
 * Lets the UI distinguish "full" from "the card went away". */
bool specter_log_is_full(void);
