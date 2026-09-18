#pragma once

#include <gui/gui.h>
#include <stdbool.h>

/* The furniture every measurement screen shares.
 *
 * Sweep, Fingerprint, Site Survey and Watch are meant to read as four views of
 * one instrument, and they did not: the header state word was right-aligned at
 * x=116 on two of them and x=126 on the other two, only two drew the presence
 * dot, the same "radio running, nothing found" state was called SCANNING here,
 * LISTENING there and ARMED on the third, and the single condition "another app
 * holds the NFC radio" was described in four different wordings - each screen
 * managing to name the fault twice and disagree with itself ("NFC BUSY" in the
 * header, "NFC unavailable" in the body).
 *
 * None of that was a decision. It was four screens written at different times.
 * Putting the chrome in one place is what makes them siblings. */

/* The state word for a measurement screen, so one state has one name. */
static inline const char* specter_chrome_state(bool error, bool armed, bool present) {
    if(error) return "NFC BUSY";
    if(!armed) return "IDLE";
    return present ? "READER" : "LISTENING";
}

/* Title left, state right, presence dot, divider. Identical on all four. */
static inline void
    specter_chrome_header(Canvas* canvas, const char* title, const char* state, bool present) {
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 9, title);
    canvas_draw_str_aligned(canvas, 116, 9, AlignRight, AlignBottom, state);
    /* Filled when a carrier is there, hollow when it is not - the same dot in
     * the same place on every screen, so it reads as one indicator. */
    if(present) {
        canvas_draw_disc(canvas, 123, 5, 2);
    } else {
        canvas_draw_circle(canvas, 123, 5, 2);
    }
    canvas_draw_line(canvas, 0, 11, 127, 11);
}

/* Just the divider, for a screen whose header carries something else. */
static inline void specter_chrome_rule(Canvas* canvas) {
    canvas_draw_line(canvas, 0, 11, 127, 11);
}

/* One fault, one wording, one layout. Says what happened AND what to do. */
static inline void specter_chrome_nfc_error(Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignCenter, "NFC radio busy");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "Close any other app");
    canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignCenter, "using NFC and retry.");
}
