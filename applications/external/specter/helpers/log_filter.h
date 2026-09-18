#pragma once

#include <stdbool.h>
#include <stddef.h>

/* Filtering the logbook by entry type.
 *
 * The .txt logbook stores each finding as two lines - a timestamp, then an
 * indented "TYPE detail" - which reads well but makes a long log tedious to
 * scan when you only care about one kind of finding. Writing up a survey means
 * skipping every SWEEP line; checking what a Watch caught overnight means
 * skipping everything else.
 *
 * So the viewer can filter. Keeping a matched entry means keeping BOTH of its
 * lines: dropping the timestamp would strip a finding of the one thing that
 * makes it evidence.
 *
 * Pure and header-only so the firmware and the host tests share one copy. */

/* Copy only the entries whose detail line starts with `type` from `text` into
 * `out`. A NULL or empty `type` copies everything. Always NUL-terminates (as
 * long as out_len > 0) and never writes past out_len. Returns entries kept.
 *
 * ENTRY-AWARE, not pair-aware. The first version assumed the text was a strict
 * sequence of (timestamp, detail) pairs, which is true of the file but NOT of
 * what the viewer actually passes in: the viewer shows only the last few KB and
 * trims to the first newline, so whenever that cut landed inside a timestamp
 * line the text began on a DETAIL line. Every line was then off by one - details
 * read as timestamps and timestamps as details - and a logbook full of READER
 * entries filtered to nothing at all. Detail lines are indented, so that is what
 * distinguishes them; a leading fragment with no timestamp of its own is dropped
 * rather than shown, because a finding without its timestamp is not evidence. */
static inline size_t
    specter_log_filter(const char* text, const char* type, char* out, size_t out_len) {
    if(!out || out_len == 0) return 0;
    out[0] = '\0';
    if(!text) return 0;

    bool all = (type == NULL) || (type[0] == '\0');
    size_t tl = 0;
    if(!all)
        while(type[tl])
            tl++;

    size_t w = 0, kept = 0;

    /* One pass, two sweeps per entry: find its extent, decide, then copy. */
    const char* p = text;
    while(*p) {
        /* An entry starts at a non-indented line. Skip any leading fragment. */
        if(*p == ' ') {
            while(*p && *p != '\n')
                p++;
            if(*p == '\n') p++;
            continue;
        }

        const char* entry = p;
        /* the stamp line */
        while(*p && *p != '\n')
            p++;
        if(*p == '\n') p++;

        /* every indented line after it belongs to this entry */
        bool match = all;
        while(*p == ' ') {
            const char* d = p;
            while(*d == ' ')
                d++;
            if(!match) {
                size_t i = 0;
                while(i < tl && d[i] && d[i] == type[i])
                    i++;
                if(i == tl) match = true;
            }
            while(*p && *p != '\n')
                p++;
            if(*p == '\n') p++;
        }

        if(match) {
            kept++;
            for(const char* q = entry; q < p && w + 1 < out_len; q++)
                out[w++] = *q;
            /* the file may end without a trailing newline */
            if(w && out[w - 1] != '\n' && w + 1 < out_len) out[w++] = '\n';
        }
    }

    out[w < out_len ? w : out_len - 1] = '\0';
    return kept;
}
