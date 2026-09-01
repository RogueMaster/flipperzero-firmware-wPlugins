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
 * long as out_len > 0) and never writes past out_len. Returns the number of
 * entries kept. */
static inline size_t
    specter_log_filter(const char* text, const char* type, char* out, size_t out_len) {
    if(!out || out_len == 0) return 0;
    out[0] = '\0';
    if(!text) return 0;

    /* No filter: straight copy, still bounded. */
    bool all = (type == NULL) || (type[0] == '\0');

    size_t w = 0, kept = 0;
    const char* p = text;

    while(*p) {
        /* line 1: the timestamp */
        const char* stamp = p;
        const char* nl = p;
        while(*nl && *nl != '\n') nl++;
        size_t stamp_len = (size_t)(nl - stamp);
        p = (*nl == '\n') ? nl + 1 : nl;

        /* line 2: the indented detail, if this entry has one */
        const char* detail = p;
        const char* nl2 = p;
        while(*nl2 && *nl2 != '\n') nl2++;
        size_t detail_len = (size_t)(nl2 - detail);
        bool have_detail = detail_len > 0;
        if(have_detail) p = (*nl2 == '\n') ? nl2 + 1 : nl2;

        bool match = all;
        if(!match && have_detail) {
            /* skip the two-space indent before comparing the type tag */
            const char* d = detail;
            size_t left = detail_len;
            while(left && (*d == ' ')) {
                d++;
                left--;
            }
            size_t tl = 0;
            while(type[tl]) tl++;
            if(left >= tl) {
                match = true;
                for(size_t i = 0; i < tl; i++) {
                    if(d[i] != type[i]) {
                        match = false;
                        break;
                    }
                }
            }
        }

        if(match) {
            kept++;
            for(size_t i = 0; i < stamp_len && w + 1 < out_len; i++) out[w++] = stamp[i];
            if(w + 1 < out_len) out[w++] = '\n';
            if(have_detail) {
                for(size_t i = 0; i < detail_len && w + 1 < out_len; i++) out[w++] = detail[i];
                if(w + 1 < out_len) out[w++] = '\n';
            }
        }
        if(!have_detail && *p == '\0') break;
    }

    out[w < out_len ? w : out_len - 1] = '\0';
    return kept;
}
