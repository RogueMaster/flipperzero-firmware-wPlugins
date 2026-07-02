/*
 * Purpose: Draw ham keyer run, refusal, and assignment screens.
 * Owns: ham-specific live-view layout and status text.
 * Depends on: morse_flipper_app_i.h and ham runtime state.
 * Tests: firmware build; rendering is hardware-only.
 */

#include "morse_flipper_app_i.h"

void morse_flipper_draw_ham_start_refusal(Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignCenter, "Please connect");
    canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignCenter, "your paddle or SK");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignCenter, "Bk back");
}

void morse_flipper_draw_ham_assign(Canvas* canvas) {
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignCenter, "Press U/D/L/R/OK");
    canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignCenter, "to assign this message");
    canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignCenter, "Back cancel");
}

static void morse_flipper_draw_ham_assignment_row(
    Canvas* canvas,
    uint8_t row,
    const char* label,
    const char* text) {
    char line[64];
    const char* shown = (text != NULL && text[0] != '\0') ? text : "-";
    size_t shown_len = strlen(shown);

    snprintf(line, sizeof(line), "%s: %s", label, shown);
    while(shown_len > 0U && canvas_string_width(canvas, line) > 126U) {
        shown_len--;
        snprintf(line, sizeof(line), "%s: %.*s", label, (int)shown_len, shown);
    }

    canvas_draw_str(canvas, 2, (int32_t)(11U + (row * 9U)), line);
}

void morse_flipper_draw_ham_assignments(Canvas* canvas, MorseFlipperApp* app) {
    if(canvas == NULL || app == NULL) return;

    canvas_set_font(canvas, FontSecondary);
    for(uint8_t i = 0U; i < MORSE_FLIPPER_HAM_KEYER_ASSIGNMENTS; i++) {
        morse_flipper_draw_ham_assignment_row(
            canvas,
            i,
            morse_flipper_ham_keyer_dir_label(i),
            morse_flipper_ham_keyer_assignment_text(&app->ham_keyer, i));
    }
    elements_button_left(canvas, "Back");
}

void morse_flipper_draw_ham_copy_notice(Canvas* canvas, MorseFlipperApp* app) {
    const char* text = "Copied";

    if(canvas == NULL) return;
    if(app != NULL && app->ham.notice[0] != '\0') text = app->ham.notice;

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, text);
}

void morse_flipper_draw_ham_delete_confirm(Canvas* canvas) {
    if(canvas == NULL) return;

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, "Delete message?");
    elements_button_left(canvas, "No");
    elements_button_center(canvas, "Yes");
}

void morse_flipper_draw_ham_run(Canvas* canvas, MorseFlipperApp* app) {
    char browse_line[32];
    uint32_t now_ms;

    if(canvas == NULL || app == NULL) return;

    now_ms = furi_get_tick();

    snprintf(
        browse_line,
        sizeof(browse_line),
        "Back: Bkin %s hold L exit",
        app->ham_keyer.break_in_enabled ? "on" : "off");
    morse_flipper_draw_tx_history_screen_custom(canvas, app, "P16 PTT  P15 Key", browse_line);
    if(app->ham.macro_active && app->ham.macro_dir < MORSE_FLIPPER_HAM_KEYER_ASSIGNMENTS) {
        snprintf(
            browse_line,
            sizeof(browse_line),
            "Send %s",
            morse_flipper_ham_keyer_dir_label(app->ham.macro_dir));
        canvas_draw_str(canvas, 92, 54, browse_line);
    } else if(app->ham.notice[0] != '\0' && now_ms < app->ham.notice_until) {
        canvas_draw_str(canvas, 98, 54, app->ham.notice);
    }
}
