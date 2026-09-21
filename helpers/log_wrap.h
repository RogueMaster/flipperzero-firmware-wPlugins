#pragma once

#include <stddef.h>

/* Laying a logbook entry out for the on-device viewer.
 *
 * The entry was written as one long indented line, on the assumption that the
 * TextBox would wrap it sensibly. It does not - it wraps by CHARACTER, so a
 * Watch contact came out as
 *
 *     2026-09-21 12:19:28
 *       WATCH  contact 2 at 8s fiel
 *     d 17% peak 100% m:boost
 *
 * splitting "field" across lines and losing the indent that marks a detail as
 * belonging to the timestamp above it. Caught in a hardware screenshot.
 *
 * So wrap it here, at spaces, and indent the continuations. That second part is
 * what keeps specter_log_filter() working: it reads EVERY indented line after a
 * timestamp as part of that entry and keeps the whole entry when any of them
 * matches the type, so a wrapped detail is still one finding, and the type
 * still sits at the front of the first indented line where the filter looks.
 *
 * The .csv is untouched - it stays one flat row per finding, because a
 * spreadsheet wants a record, not a paragraph.
 *
 * Pure and header-only so the firmware and the host tests share one copy. */

/* Characters that fit one line of the viewer's TextBox. Empirical, from a 4x
 * device capture: "  WATCH  contact 2 at 8s fiel" is 29 characters and filled
 * the width exactly. The font is proportional, so a character budget is only a
 * proxy and 26 leaves room for a line of unusually wide glyphs. */
#define SPECTER_LOG_LINE_MAX 26u

/* Type names are padded to this so details line up under one another. */
#define SPECTER_LOG_TYPE_W 6u

/* Continuation lines are indented by this much - any indent at all marks them
 * as detail, and four keeps them clear of the timestamp column. */
#define SPECTER_LOG_CONT 4u

/* Render "  TYPE   detail" into `out`, wrapping at spaces onto indented
 * continuation lines, with a trailing newline. Returns the number of characters
 * written, or 0 if `out` was too small (in which case `out` is left empty).
 * Never writes past `cap`. */
static inline size_t
    specter_log_wrap(char* out, size_t cap, const char* type, const char* detail) {
    if(!out || cap == 0u) return 0u;
    out[0] = '\0';
    if(!type) type = "";
    if(!detail) detail = "";

    size_t w = 0u; // chars written
    size_t col = 0u; // chars on the current line

    /* Every write goes through here so nothing can run past the buffer. */
#define SPECTER_LW_PUT(ch)                 \
    do {                                   \
        if(w + 1u >= cap) {                \
            out[0] = '\0';                 \
            return 0u;                     \
        }                                  \
        out[w++] = (char)(ch);             \
        col++;                             \
    } while(0)

    /* first line: two spaces, then the type padded out */
    SPECTER_LW_PUT(' ');
    SPECTER_LW_PUT(' ');
    size_t tl = 0u;
    while(type[tl] != '\0') {
        SPECTER_LW_PUT(type[tl]);
        tl++;
    }
    while(tl < SPECTER_LOG_TYPE_W) {
        SPECTER_LW_PUT(' ');
        tl++;
    }

    const char* p = detail;
    while(*p != '\0') {
        while(*p == ' ') p++; // collapse runs of spaces
        if(*p == '\0') break;

        const char* start = p;
        while(*p != '\0' && *p != ' ') p++;
        size_t wl = (size_t)(p - start);

        /* Break before the word if it will not fit with its leading space.
         * A word too long for a whole line is hard-split below instead - that
         * is better than a line running off the screen. */
        if(col + 1u + wl > SPECTER_LOG_LINE_MAX && wl <= SPECTER_LOG_LINE_MAX - SPECTER_LOG_CONT) {
            SPECTER_LW_PUT('\n');
            col = 0u;
            for(size_t i = 0u; i < SPECTER_LOG_CONT; i++) SPECTER_LW_PUT(' ');
        } else {
            SPECTER_LW_PUT(' ');
        }

        for(size_t i = 0u; i < wl; i++) {
            if(col >= SPECTER_LOG_LINE_MAX) { // hard-split an over-long word
                SPECTER_LW_PUT('\n');
                col = 0u;
                for(size_t k = 0u; k < SPECTER_LOG_CONT; k++) SPECTER_LW_PUT(' ');
            }
            SPECTER_LW_PUT(start[i]);
        }
    }

    SPECTER_LW_PUT('\n');
#undef SPECTER_LW_PUT

    out[w] = '\0';
    return w;
}
