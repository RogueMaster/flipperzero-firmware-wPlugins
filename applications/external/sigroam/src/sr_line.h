#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "sr_types.h"

/*
 * ★ Line assembler. Must not include any furi header (ADR-003).
 *
 * Slices the UART byte stream into lines and hands them to codec->feed_line.
 * The buffer is allocated once with the host struct and never grows or shrinks
 * at runtime (Plan 3.5 / P13).
 *
 * ---------------------------------------------------------------------------
 * Byte rules (per V-008 + ADR-010)
 *
 *   '\n'          Line separator. V-002 verified that wardrive data rows are
 *                 Serial.print plus a literal '\n' (LF); the terminator used by
 *                 the CLI's Serial.println is unverified here and may be CRLF.
 *   '\r'          Always dropped (counted as cr_dropped). Handling it uniformly
 *                 mid-line and at end-of-line absorbs both CRLF and the double
 *                 terminator from "literal \r\n then println adds another".
 *   Other control Every byte < 0x20 except '\n', plus 0x7F (DEL), is dropped
 *                 (counted as ctrl_dropped; '\r' counts separately as
 *                 cr_dropped). TAB is dropped too -- Marauder output contains
 *                 no TAB, canvas does not handle it, and keeping it would only
 *                 misalign the Raw view. NUL must be dropped: keeping it makes
 *                 len disagree with strlen, so downstream C string handling
 *                 would truncate early.
 *   >= 0x80       Kept verbatim. Real SSIDs often contain UTF-8 (CJK / emoji),
 *                 and filtering would silently rewrite both the SSID and the
 *                 future WiGLE data. Rendering them as boxes is the UI layer's
 *                 problem; the data layer does not corrupt. (A deliberate
 *                 departure from the "filter non-ASCII" wording in Plan 4 / P8;
 *                 rationale in ADR-010.)
 *   Empty lines   A line that is zero-length after stripping is dropped
 *                 internally and does not set ready (counted as lines_empty).
 *                 The println double terminator is the main source of empty
 *                 lines, and the parser should not be woken for each one.
 *   Overlong      Once SR_RAW_LINE_MAX is reached the assembler enters
 *                 overflowing and discards through end-of-line (counted as
 *                 overflow_dropped); the line is delivered with truncated=true.
 *                 **On seeing truncated the upper layer MUST refuse to parse
 *                 and fall straight back to Raw**: half a line parses as valid
 *                 CSV and yields wrong data.
 * ---------------------------------------------------------------------------
 *
 * Usage (pull style, no callback -- the worker must be able to check its exit
 * flag after each line):
 *
 *   size_t off = 0;
 *   while(off < n) {
 *       off += sr_line_feed(line, buf + off, n - off);
 *       if(!sr_line_ready(line)) continue;
 *       size_t len;
 *       const char* text = sr_line_text(line, &len);
 *       if(sr_line_truncated(line)) {
 *           // straight to the Raw view, never to the parser
 *       } else {
 *           codec->feed_line(parser, text, len, ev);  // ev is a heap singleton, not on the stack
 *       }
 *       sr_line_consume(line);
 *       if(worker_should_exit) break;
 *   }
 *
 * text points into the internal buffer and becomes invalid after
 * sr_line_consume -- the same borrow contract as SrRawView (ADR-010).
 */

typedef struct {
    /* Plan 3.5: 512 B line assembly buffer = 511 effective chars + NUL */
    char buf[SR_RAW_LINE_MAX + 1];
    size_t len; /* Effective chars accumulated in buf, excluding the NUL */

    bool ready;       /* buf holds a complete line awaiting consumption */
    bool truncated;   /* The current ready line was truncated */
    bool overflowing; /* Discarding the rest of the current line until '\n' */

    /* Statistics. Preserved by sr_line_reset; only sr_line_init clears them. */
    uint32_t lines_total;     /* Lines delivered upward (excludes dropped empty lines) */
    uint32_t lines_truncated; /* Of those, how many were truncated */
    uint32_t lines_empty;     /* Lines empty after stripping, dropped internally */
    uint32_t cr_dropped;      /* Dropped '\r'; normal CRLF traffic counts here too */
    uint32_t ctrl_dropped;    /* Other dropped control chars -- growth means a line or peer problem */
    uint32_t overflow_dropped; /* Bytes dropped for being overlong */
} SrLine;

/* Clear line state and all statistics. */
void sr_line_init(SrLine* l);

/*
 * Discard a partial line and any unconsumed ready line, keeping statistics.
 * For reconnect / new session: last time's partial line must not be spliced
 * into the new stream.
 */
void sr_line_reset(SrLine* l);

/*
 * Feed data[0..n) and return how many bytes were actually consumed.
 *
 * If sr_line_ready() is true on return, buf holds a complete line and the
 * caller feeds the remaining bytes on the next call.
 * Calling this again while a ready line is unconsumed returns 0 and changes
 * nothing -- this prevents overwriting an unconsumed line.
 * Returns 0 when data is NULL or n is 0.
 * Always makes progress: while n > 0 and not ready, the return value is >= 1.
 */
size_t sr_line_feed(SrLine* l, const char* data, size_t n);

bool sr_line_ready(const SrLine* l);

/*
 * Get the current ready line. out_len may be NULL.
 * Returns NULL and sets *out_len to 0 when not ready.
 * The returned pointer becomes invalid after sr_line_consume / reset / init /
 * the next feed.
 */
const char* sr_line_text(const SrLine* l, size_t* out_len);

/* Whether the current ready line was truncated. Returns false when not ready. */
bool sr_line_truncated(const SrLine* l);

/* Consume the current ready line and prepare for the next. No-op when not ready. */
void sr_line_consume(SrLine* l);
