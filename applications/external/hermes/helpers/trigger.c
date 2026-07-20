#include "trigger.h"

#include <string.h>

/* Knuth-Morris-Pratt. The pattern can overlap itself ("aab" must still be found
 * in "aaab"), and the stream arrives in arbitrary chunks, so the matcher has to
 * carry its position across calls and back off correctly on a mismatch rather
 * than restarting. A precomputed failure table is what makes that exact - and
 * it costs one byte per pattern character. */
struct Trigger {
    char pattern[TRIGGER_PATTERN_MAX + 1]; // as typed, for display
    char folded[TRIGGER_PATTERN_MAX]; // lowercased, for comparing
    uint8_t fail[TRIGGER_PATTERN_MAX]; // fail[i] = longest proper prefix-suffix of [0..i]
    uint8_t len;
    uint8_t pos; // how much of the pattern has matched so far
    uint32_t hits;
};

static char trigger_fold(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c + ('a' - 'A')) : c;
}

static void trigger_build_failure(Trigger* tr) {
    if(tr->len == 0) return;

    tr->fail[0] = 0;
    uint8_t k = 0;

    for(uint8_t i = 1; i < tr->len; i++) {
        while(k > 0 && tr->folded[i] != tr->folded[k]) {
            k = tr->fail[k - 1];
        }
        if(tr->folded[i] == tr->folded[k]) k++;
        tr->fail[i] = k;
    }
}

Trigger* trigger_alloc(void) {
    Trigger* tr = malloc(sizeof(Trigger));
    memset(tr, 0, sizeof(Trigger));
    return tr;
}

void trigger_free(Trigger* tr) {
    furi_assert(tr);
    free(tr);
}

void trigger_set(Trigger* tr, const char* pattern) {
    furi_assert(tr);
    furi_assert(pattern);

    strncpy(tr->pattern, pattern, TRIGGER_PATTERN_MAX);
    tr->pattern[TRIGGER_PATTERN_MAX] = '\0';
    tr->len = (uint8_t)strlen(tr->pattern);

    /* Fold once here rather than on every byte of a boot log. */
    for(uint8_t i = 0; i < tr->len; i++) {
        tr->folded[i] = trigger_fold(tr->pattern[i]);
    }
    trigger_build_failure(tr);

    tr->pos = 0;
    tr->hits = 0;
}

const char* trigger_pattern(const Trigger* tr) {
    furi_assert(tr);
    return tr->pattern;
}

bool trigger_is_armed(const Trigger* tr) {
    furi_assert(tr);
    return tr->len > 0;
}

void trigger_reset(Trigger* tr) {
    furi_assert(tr);
    tr->pos = 0;
    tr->hits = 0;
}

bool trigger_feed(Trigger* tr, const uint8_t* data, size_t len) {
    furi_assert(tr);
    furi_assert(data);
    if(tr->len == 0) return false;

    bool fired = false;

    for(size_t i = 0; i < len; i++) {
        const char c = trigger_fold((char)data[i]);

        while(tr->pos > 0 && c != tr->folded[tr->pos]) {
            tr->pos = tr->fail[tr->pos - 1];
        }
        if(c == tr->folded[tr->pos]) tr->pos++;

        if(tr->pos == tr->len) {
            tr->hits++;
            fired = true;
            /* Resume from the longest self-overlap, so back-to-back and
             * overlapping occurrences are both counted. */
            tr->pos = tr->fail[tr->len - 1];
        }
    }

    return fired;
}

uint32_t trigger_hits(const Trigger* tr) {
    furi_assert(tr);
    return tr->hits;
}
