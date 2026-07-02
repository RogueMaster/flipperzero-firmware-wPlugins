/*
 * Purpose: Keep the rolling Free Practice text history.
 * Owns: append, trimming, and reset behaviour for run-history text.
 * Depends on: morse_flipper_run_history.h.
 * Tests: tests/test_run_history.c.
 */

#include "morse_flipper_run_history.h"

#include <string.h>

static void morse_flipper_run_history_drop_left(MorseFlipperRunHistory* history) {
    size_t len;

    if(history == NULL) return;
    len = strlen(history->text);
    if(len == 0U) return;

    memmove(history->text, history->text + 1U, len);
    while(history->text[0] == ' ' || history->text[0] == '\n') {
        len = strlen(history->text);
        if(len == 0U) break;
        memmove(history->text, history->text + 1U, len);
    }
}

static void morse_flipper_run_history_append_ch(MorseFlipperRunHistory* history, char ch) {
    size_t len;

    if(history == NULL || ch == '\0') return;
    if(ch == '|') ch = ' ';

    len = strlen(history->text);

    if(ch == '\n') {
        if(len == 0U || history->text[len - 1U] == '\n') return;

        while(len + 1U >= MORSE_FLIPPER_RUN_HISTORY_TEXT) {
            morse_flipper_run_history_drop_left(history);
            len = strlen(history->text);
        }

        history->text[len] = ch;
        history->text[len + 1U] = '\0';
        return;
    }

    if(ch == ' ') {
        if(len == 0U || history->text[len - 1U] == ' ' || history->text[len - 1U] == '\n') return;
    }

    while(len + 1U >= MORSE_FLIPPER_RUN_HISTORY_TEXT) {
        morse_flipper_run_history_drop_left(history);
        len = strlen(history->text);
    }

    if(ch == ' ' && len == 0U) return;
    history->text[len] = ch;
    history->text[len + 1U] = '\0';
}

void morse_flipper_run_history_reset(MorseFlipperRunHistory* history) {
    if(history == NULL) return;

    memset(history, 0, sizeof(*history));
}

void morse_flipper_run_history_append(MorseFlipperRunHistory* history, const char* text) {
    if(history == NULL || text == NULL) return;

    while(*text) {
        morse_flipper_run_history_append_ch(history, *text++);
    }
}

const char* morse_flipper_run_history_text(const MorseFlipperRunHistory* history) {
    return history == NULL ? "" : history->text;
}
