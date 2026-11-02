#include "bible_app.h"
#include "bible_books.h"
#include "bible_renderer.h"
#include "bible_loader.h"
#include "bible_storage.h"
#include "bible_url.h"
#include "bible_nfc.h"

#include <string.h>

/* ============================================================================
 * VerseSelect layout constants
 * ============================================================================ */
#define VERSE_SELECT_HEADER_H 12
#define VERSE_SELECT_LINE_H   12
#define VERSE_SELECT_ROW_1_Y  (VERSE_SELECT_HEADER_H + 10) /* y = 22 */
#define VERSE_SELECT_ROW_2_Y  (VERSE_SELECT_ROW_1_Y + VERSE_SELECT_LINE_H) /* y = 34 */
#define VERSE_SELECT_ROW_3_Y  (VERSE_SELECT_ROW_2_Y + VERSE_SELECT_LINE_H) /* y = 46 */
#define VERSE_SELECT_HINT_Y   60

/* ============================================================================
 * VerseSelect view — draw callback
 * ============================================================================ */
void bible_bsb_view_verse_select_draw(Canvas* canvas, void* ctx) {
    BibleApp* app = bible_app_from_draw(ctx);
    if(!canvas) return;
    canvas_reset(canvas);
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    if(!app || !app->state) {
        canvas_draw_str(canvas, 2, 24, "Verses");
        return;
    }
    BibleAppState* state = app->state;
    if(state->show_data_help) {
        bible_draw_missing_data(canvas);
        return;
    }

    uint8_t mode = state->verse_select_mode;
    if(mode > 2) mode = 0;

    const char* action_prefix = state->verse_select_for_nfc ? "Share" : "Save";
    const char* book_name = state->selected_book_name[0] ? state->selected_book_name : "Book";
    char header[56];
    if(mode == 0) {
        snprintf(
            header,
            sizeof(header),
            "%s %s %u",
            action_prefix,
            book_name,
            (unsigned int)state->selected_chapter);
    } else if(state->selected_end_verse > state->selected_start_verse) {
        snprintf(
            header,
            sizeof(header),
            "%s %s %u:%u-%u",
            action_prefix,
            book_name,
            (unsigned int)state->selected_chapter,
            (unsigned int)state->selected_start_verse,
            (unsigned int)state->selected_end_verse);
    } else {
        snprintf(
            header,
            sizeof(header),
            "%s %s %u:%u",
            action_prefix,
            book_name,
            (unsigned int)state->selected_chapter,
            (unsigned int)state->selected_start_verse);
    }

    if(canvas_string_width(canvas, header) > BIBLE_SCREEN_WIDTH - 4) {
        canvas_set_font(canvas, FontSecondary);
    }
    canvas_draw_str(canvas, 2, 10, header);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_line(canvas, 0, VERSE_SELECT_HEADER_H, BIBLE_SCREEN_WIDTH, VERSE_SELECT_HEADER_H);

    char start_label[24];
    char end_label[24];
    snprintf(
        start_label, sizeof(start_label), "Start: %u", (unsigned int)state->selected_start_verse);
    snprintf(end_label, sizeof(end_label), "End: %u", (unsigned int)state->selected_end_verse);
    const char* rows[3] = {"Full chapter", start_label, end_label};
    const int16_t row_y[3] = {VERSE_SELECT_ROW_1_Y, VERSE_SELECT_ROW_2_Y, VERSE_SELECT_ROW_3_Y};
    for(uint8_t i = 0; i < 3; i++) {
        if(i == mode) {
            bible_draw_inverted_highlight(
                canvas,
                0,
                (uint8_t)(row_y[i] - 10),
                BIBLE_SCREEN_WIDTH,
                VERSE_SELECT_LINE_H,
                rows[i],
                4,
                (uint8_t)row_y[i]);
        } else {
            canvas_draw_str(canvas, 4, row_y[i], rows[i]);
        }
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(
        canvas,
        2,
        VERSE_SELECT_HINT_Y,
        mode == 0 ? "OK=confirm, Back=cancel" : "U/D=row L/R=vs OK=ok");
}

/* ============================================================================
 * VerseSelect view — input callback
 * ============================================================================ */
bool bible_bsb_view_verse_select_input(InputEvent* event, void* ctx) {
    BibleApp* app = ctx;
    BibleAppState* state = app->state;

    /* Only process Press, Repeat, Short, Long — ignore Release */
    if(event->type != InputTypePress && event->type != InputTypeRepeat &&
       event->type != InputTypeShort && event->type != InputTypeLong) {
        return false;
    }

    if(state->show_data_help) {
        bool retry = event->key == InputKeyOk && event->type == InputTypeShort;
        bool leave = event->key == InputKeyBack && event->type == InputTypeShort;
        if(!retry) {
            bible_app_request_redraw(app);
            return !leave;
        }
    }

    uint16_t max_v = state->passage.verse_count;
    if(max_v == 0) {
        max_v = max_verse_for_chapter(state->selected_book, state->selected_chapter);
    }
    uint16_t actual_max = (max_v > 0) ? max_v : 40;

    bool consumed = false;

    if(event->key == InputKeyUp &&
       (event->type == InputTypePress || event->type == InputTypeRepeat)) {
        /* Up: End -> Start -> All */
        if(state->verse_select_mode == 2) {
            state->verse_select_mode = 1; /* End -> Start */
        } else if(state->verse_select_mode == 1) {
            state->verse_select_mode = 0; /* Start -> All */
        }
        /* All stays All */
        consumed = true;

    } else if(
        event->key == InputKeyDown &&
        (event->type == InputTypePress || event->type == InputTypeRepeat)) {
        /* Down: All -> Start -> End */
        if(state->verse_select_mode == 0) {
            state->verse_select_mode = 1; /* All -> Start */
        } else if(state->verse_select_mode == 1) {
            state->verse_select_mode = 2; /* Start -> End */
        }
        /* End stays End */
        consumed = true;

    } else if(
        event->key == InputKeyLeft &&
        (event->type == InputTypePress || event->type == InputTypeRepeat)) {
        /* Left: decrement verse */
        if(state->verse_select_mode == 1) {
            /* Start mode: decrement start verse (min=1) */
            if(state->selected_start_verse > 1) {
                state->selected_start_verse -= 1;
            }
        } else if(state->verse_select_mode == 2) {
            /* End mode: decrement end verse (min=start_verse) */
            if(state->selected_end_verse > state->selected_start_verse) {
                state->selected_end_verse -= 1;
            }
        }
        consumed = true;

    } else if(
        event->key == InputKeyRight &&
        (event->type == InputTypePress || event->type == InputTypeRepeat)) {
        /* Right: increment verse */
        if(state->verse_select_mode == 1) {
            /* Start mode: increment start verse, auto-bump end if needed */
            if(state->selected_start_verse < actual_max) {
                state->selected_start_verse += 1;
                if(state->selected_end_verse < state->selected_start_verse) {
                    state->selected_end_verse = state->selected_start_verse;
                }
            }
        } else if(state->verse_select_mode == 2) {
            /* End mode: increment end verse (clamped to actual_max) */
            if(state->selected_end_verse < actual_max) {
                state->selected_end_verse += 1;
            }
        }
        consumed = true;

    } else if(event->key == InputKeyOk && event->type == InputTypeShort) {
        /* OK (Short): save selected range to collection or share via NFC */
        uint16_t start_verse = (state->verse_select_mode == 0) ? 0 : state->selected_start_verse;
        uint16_t end_verse = (state->verse_select_mode == 0) ? 0 : state->selected_end_verse;

        if(state->verse_select_for_nfc) {
            BiblePassage share_passage = state->passage;
            share_passage.book_index = state->selected_book;
            share_passage.chapter = state->selected_chapter;
            share_passage.start_verse = start_verse;
            share_passage.end_verse = end_verse;

            if(share_passage.verse_count > 0 &&
               bible_build_route_url(&share_passage, state->nfc_url, sizeof(state->nfc_url))) {
                if(bible_nfc_start_url(state)) {
                    scene_manager_next_scene(app->scene_manager, BibleSceneNfcShare);
                } else {
                    bible_toast_set(&state->toast, "NFC failed");
                    scene_manager_search_and_switch_to_another_scene(
                        app->scene_manager, BibleSceneReader);
                }
            } else {
                bible_toast_set(&state->toast, "URL failed");
                scene_manager_search_and_switch_to_another_scene(
                    app->scene_manager, BibleSceneReader);
            }
        } else {
            state->passage.start_verse = start_verse;
            state->passage.end_verse = end_verse;
            bible_save_passage(state);
            scene_manager_search_and_switch_to_another_scene(app->scene_manager, BibleSceneReader);
        }
        consumed = true;

    } else if(event->key == InputKeyBack && event->type == InputTypeShort) {
        /* Back (Short): cancel and return to Reader */
        scene_manager_search_and_switch_to_another_scene(app->scene_manager, BibleSceneReader);
        consumed = true;
    }

    bible_app_request_redraw(app);
    return consumed;
}
