#include "sr_line.h"

#include <string.h>

/*
 * ★ Pure logic; must not include furi (ADR-003).
 * All rules and rationale live in the header comment of sr_line.h; this file holds only the implementation.
 */

/*
 * Plan section 3.5 budgets 512 B for the line assembly buffer. Metadata (len + 3 bools + 6
 * counters) pushing it to 576 is the ceiling; any more means somebody put something here that
 * does not belong.
 */
_Static_assert(sizeof(SrLine) <= 576, "SrLine over Plan 3.5 line-buffer budget");

void sr_line_init(SrLine* l) {
    if(l == NULL) {
        return;
    }
    memset(l, 0, sizeof(*l));
}

void sr_line_reset(SrLine* l) {
    if(l == NULL) {
        return;
    }
    l->len = 0;
    l->ready = false;
    l->truncated = false;
    l->overflowing = false;
    /* Statistics are not cleared -- a reconnect should not zero the drop/malformed counters. */
}

size_t sr_line_feed(SrLine* l, const char* data, size_t n) {
    size_t i = 0;

    if(l == NULL || data == NULL || n == 0) {
        return 0;
    }
    /* The previous line is unconsumed: change no state and let the caller take it first. */
    if(l->ready) {
        return 0;
    }

    while(i < n) {
        unsigned char c = (unsigned char)data[i];
        i++;

        if(c == '\n') {
            if(l->len == 0) {
                /*
                 * Nothing but an empty line after stripping, so drop it internally.
                 * overflowing is necessarily false here -- it is set only when len == SR_RAW_LINE_MAX.
                 */
                l->lines_empty++;
                continue;
            }
            l->buf[l->len] = '\0';
            l->truncated = l->overflowing;
            l->overflowing = false;
            l->ready = true;
            l->lines_total++;
            if(l->truncated) {
                l->lines_truncated++;
            }
            return i;
        }

        if(c == '\r') {
            l->cr_dropped++;
            continue;
        }

        if(c < 0x20U || c == 0x7FU) {
            l->ctrl_dropped++;
            continue;
        }

        /* Visible ASCII and anything >= 0x80 are always kept. */
        if(l->overflowing) {
            l->overflow_dropped++;
            continue;
        }
        if(l->len >= (size_t)SR_RAW_LINE_MAX) {
            l->overflowing = true;
            l->overflow_dropped++;
            continue;
        }
        l->buf[l->len++] = (char)c;
    }

    return i;
}

bool sr_line_ready(const SrLine* l) {
    return l != NULL && l->ready;
}

const char* sr_line_text(const SrLine* l, size_t* out_len) {
    if(l == NULL || !l->ready) {
        if(out_len != NULL) {
            *out_len = 0;
        }
        return NULL;
    }
    if(out_len != NULL) {
        *out_len = l->len;
    }
    return l->buf;
}

bool sr_line_truncated(const SrLine* l) {
    return l != NULL && l->ready && l->truncated;
}

void sr_line_consume(SrLine* l) {
    if(l == NULL || !l->ready) {
        return;
    }
    l->len = 0;
    l->ready = false;
    l->truncated = false;
}
