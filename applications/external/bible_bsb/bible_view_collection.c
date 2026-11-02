#include "bible_view_collection.h"
#include "bible_renderer.h"
#include "bible_storage.h"
#include "bible_nfc.h"

#include <string.h>

/* ============================================================================
 * Collection layout constants
 * ============================================================================ */
#define COLLECTION_HEADER_Y  10
#define COLLECTION_DIVIDER_Y 12
#define COLLECTION_LINE_H    12

/* ============================================================================
 * Collection view — draw callback
 * ============================================================================ */
void bible_bsb_view_collection_draw(Canvas* canvas, void* ctx) {
    BibleApp* app = bible_app_from_draw(ctx);
    BibleAppState* state = app->state;
    if(state->show_data_help) {
        bible_draw_missing_data(canvas);
        return;
    }

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    /* Header */
    canvas_draw_str(canvas, BIBLE_MARGIN_X, COLLECTION_HEADER_Y, "Collection >");
    canvas_draw_line(canvas, 0, COLLECTION_DIVIDER_Y, BIBLE_SCREEN_WIDTH, COLLECTION_DIVIDER_Y);

    /* Empty state — also when the entry array failed to allocate */
    if(state->collection_count == 0 || state->collection == NULL) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 4, 30, "No saved passages");
        canvas_draw_str(canvas, 4, 44, "Back to home");
        bible_draw_toast(canvas, &state->toast);
        return;
    }

    /* List of saved passages (3 rows above the bottom hint bar) */
    uint8_t max_visible = (BIBLE_SCREEN_HEIGHT - COLLECTION_DIVIDER_Y - 14) / COLLECTION_LINE_H;
    if(max_visible == 0) max_visible = 1;

    if(state->collection_selected >= state->collection_count) {
        state->collection_selected = state->collection_count - 1;
    }
    uint16_t start = state->collection_scroll;
    if(start + max_visible > state->collection_count) {
        start = (state->collection_count > max_visible) ? (state->collection_count - max_visible) :
                                                          0;
    }

    for(uint8_t i = 0; i < max_visible; i++) {
        uint16_t idx = start + i;
        if(idx >= state->collection_count) break;

        uint8_t y = COLLECTION_DIVIDER_Y + 4 + (i * COLLECTION_LINE_H) + BIBLE_CHAR_HEIGHT;

        if(idx == state->collection_selected) {
            bible_draw_inverted_highlight(
                canvas,
                0,
                y - 10,
                BIBLE_SCREEN_WIDTH,
                COLLECTION_LINE_H,
                state->collection[idx].scripture_display_ref,
                BIBLE_MARGIN_X + 4,
                y);
        } else {
            canvas_draw_str(
                canvas, BIBLE_MARGIN_X + 4, y, state->collection[idx].scripture_display_ref);
        }
    }

    /* Scroll indicator */
    bible_draw_scroll_indicator(
        canvas, start, state->collection_count, max_visible, COLLECTION_DIVIDER_Y);

    /* Bottom hint */
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, BIBLE_MARGIN_X, 62, "L=export OK=menu");

    bible_draw_toast(canvas, &state->toast);
}

/* ============================================================================
 * Collection view — input callback
 * ============================================================================ */
bool bible_bsb_view_collection_input(InputEvent* event, void* ctx) {
    BibleApp* app = ctx;
    BibleAppState* state = app->state;

    /* Only process Press, Repeat, Short, Long — ignore Release */
    if(event->type != InputTypePress && event->type != InputTypeRepeat &&
       event->type != InputTypeShort && event->type != InputTypeLong) {
        return false;
    }

    if(state->show_data_help) {
        bible_app_request_redraw(app);
        return !(event->key == InputKeyBack && event->type == InputTypeShort);
    }

    bool consumed = false;

    uint8_t max_visible = (BIBLE_SCREEN_HEIGHT - COLLECTION_DIVIDER_Y - 14) / COLLECTION_LINE_H;
    if(max_visible == 0) max_visible = 1;

    /* Up/Down: move selection with smooth viewport scrolling */
    if(event->key == InputKeyUp &&
       (event->type == InputTypePress || event->type == InputTypeRepeat)) {
        if(state->collection_count > 0) {
            state->collection_selected = (state->collection_selected > 0) ?
                                             (state->collection_selected - 1) :
                                             (state->collection_count - 1);
            if(state->collection_selected < state->collection_scroll) {
                state->collection_scroll = state->collection_selected;
            } else if(state->collection_selected >= state->collection_scroll + max_visible) {
                state->collection_scroll = state->collection_selected >= max_visible ?
                                               state->collection_selected - (max_visible - 1) :
                                               0;
            }
        }
        consumed = true;
    } else if(
        event->key == InputKeyDown &&
        (event->type == InputTypePress || event->type == InputTypeRepeat)) {
        if(state->collection_count > 0) {
            state->collection_selected =
                (state->collection_selected + 1 < state->collection_count) ?
                    (state->collection_selected + 1) :
                    0;
            if(state->collection_selected >= state->collection_scroll + max_visible) {
                state->collection_scroll = state->collection_selected >= max_visible ?
                                               state->collection_selected - (max_visible - 1) :
                                               0;
            } else if(state->collection_selected < state->collection_scroll) {
                state->collection_scroll = state->collection_selected;
            }
        }
        consumed = true;
    }

    /* Right (Short): navigate to BookList */
    else if(event->key == InputKeyRight && event->type == InputTypeShort) {
        /* BookList is NOT in the scene history stack (Collection was entered
         * directly from BookList via scene_manager_next_scene), so
         * we must search-and-switch rather than previous_scene. */
        scene_manager_search_and_switch_to_another_scene(app->scene_manager, BibleSceneBookList);
        consumed = true;
    }

    /* Left (Press/Short): NFC bulk export */
    else if(
        event->key == InputKeyLeft &&
        (event->type == InputTypePress || event->type == InputTypeShort)) {
        if(state->collection_count > 0) {
            /* Build Kindled JSON export into a buffer */
            char export_buf[1024]; /* NTAG215 has ~500 bytes for payload */
            bool built = bible_storage_build_kindled_json(state, export_buf, sizeof(export_buf));
            if(built) {
                strlcpy(state->nfc_url, "Export JSON", sizeof(state->nfc_url));
                state->nfc_is_export = true;
                if(bible_nfc_start_text(state, export_buf)) {
                    scene_manager_next_scene(app->scene_manager, BibleSceneNfcShare);
                } else {
                    state->nfc_url[0] = '\0';
                    state->nfc_is_export = false;
                    bible_toast_set(&state->toast, "Export too large");
                }
            } else if(!bible_toast_active(&state->toast)) {
                bible_toast_set(&state->toast, "Export failed");
            }
        }
        consumed = true;
    }

    /* OK (Short): open intermediary action menu for selected passage */
    else if(event->key == InputKeyOk && event->type == InputTypeShort) {
        if(state->collection != NULL && state->collection_selected < state->collection_count) {
            state->action_menu_selection = 0;
            state->action_menu_from_collection = true;
            scene_manager_next_scene(app->scene_manager, BibleSceneActionMenu);
        }
        consumed = true;
    }

    /* Back (Short): return to BookList */
    else if(event->key == InputKeyBack && event->type == InputTypeShort) {
        scene_manager_search_and_switch_to_another_scene(app->scene_manager, BibleSceneBookList);
        consumed = true;
    }

    bible_app_request_redraw(app);
    return consumed;
}
