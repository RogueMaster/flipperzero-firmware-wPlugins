#include "bible_app.h"
#include "bible_books.h"
#include "bible_renderer.h"

#include <string.h>

/* ============================================================================
 * BookFilter layout constants
 * ============================================================================ */
#define BOOK_FILTER_COLS             5
#define BOOK_FILTER_CELL_W           25
#define BOOK_FILTER_CELL_H           14
#define BOOK_FILTER_MAX_VISIBLE_ROWS 3
#define BOOK_FILTER_OPTIONS          BIBLE_FILTER_COUNT

/* ============================================================================
 * BookFilter view — draw callback
 * ============================================================================ */
void bible_bsb_view_book_filter_draw(Canvas* canvas, void* ctx) {
    BibleApp* app = bible_app_from_draw(ctx);
    BibleAppState* state = app->state;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    /* Header */
    canvas_draw_str(canvas, BIBLE_MARGIN_X, 10, "< Select Filter");
    canvas_draw_line(canvas, 0, 12, BIBLE_SCREEN_WIDTH, 12);

    /* Compute visible row window */
    uint8_t selected_row = state->book_filter_idx / BOOK_FILTER_COLS;
    uint8_t start_row = (selected_row >= BOOK_FILTER_MAX_VISIBLE_ROWS) ?
                            (selected_row - BOOK_FILTER_MAX_VISIBLE_ROWS + 1) :
                            0;

    /* Total rows for scroll indicator */
    uint8_t total_rows = (BOOK_FILTER_OPTIONS + BOOK_FILTER_COLS - 1) / BOOK_FILTER_COLS;

    /* Draw each visible cell */
    for(uint8_t i = 0; i < BOOK_FILTER_OPTIONS; i++) {
        uint8_t row = i / BOOK_FILTER_COLS;
        uint8_t col = i % BOOK_FILTER_COLS;

        if(row < start_row || row >= start_row + BOOK_FILTER_MAX_VISIBLE_ROWS) {
            continue;
        }

        uint8_t display_row = row - start_row;
        uint8_t x = BIBLE_MARGIN_X + (col * BOOK_FILTER_CELL_W);
        uint8_t y = 14 + (display_row * BOOK_FILTER_CELL_H);
        bool is_selected = (i == state->book_filter_idx);

        if(is_selected) {
            canvas_draw_box(canvas, x, y, BOOK_FILTER_CELL_W, BOOK_FILTER_CELL_H);
            canvas_set_color(canvas, ColorWhite);
        }

        canvas_draw_str(canvas, x + 4, y + 11, BIBLE_FILTER_LABELS[i]);

        if(is_selected) {
            canvas_set_color(canvas, ColorBlack);
        }
    }

    /* Scroll indicator */
    if(total_rows > BOOK_FILTER_MAX_VISIBLE_ROWS) {
        int32_t thumb_height = ((BOOK_FILTER_MAX_VISIBLE_ROWS * 50) / total_rows);
        if(thumb_height < 4) {
            thumb_height = 4;
        }
        int32_t thumb_y = 14;
        if(total_rows > BOOK_FILTER_MAX_VISIBLE_ROWS) {
            thumb_y += (int32_t)start_row * (50 - thumb_height) /
                       (int32_t)(total_rows - BOOK_FILTER_MAX_VISIBLE_ROWS);
        }
        canvas_draw_box(canvas, 126, thumb_y, 2, thumb_height);
    }
}

/* ============================================================================
 * BookFilter view — input callback
 * ============================================================================ */
bool bible_bsb_view_book_filter_input(InputEvent* event, void* ctx) {
    BibleApp* app = ctx;
    BibleAppState* state = app->state;

    /* Only process Press, Repeat, Short, Long — ignore Release */
    if(event->type != InputTypePress && event->type != InputTypeRepeat &&
       event->type != InputTypeShort && event->type != InputTypeLong) {
        return false;
    }

    bool consumed = false;

    if(event->key == InputKeyUp &&
       (event->type == InputTypePress || event->type == InputTypeRepeat)) {
        /* Move up by one row */
        if(state->book_filter_idx >= BOOK_FILTER_COLS) {
            state->book_filter_idx -= BOOK_FILTER_COLS;
        }
        consumed = true;

    } else if(
        event->key == InputKeyDown &&
        (event->type == InputTypePress || event->type == InputTypeRepeat)) {
        /* Move down by one row, snap to last item if partial row */
        if(state->book_filter_idx + BOOK_FILTER_COLS < BOOK_FILTER_OPTIONS) {
            state->book_filter_idx += BOOK_FILTER_COLS;
        } else if(state->book_filter_idx < BOOK_FILTER_OPTIONS - 1) {
            /* Snap to last item if moving down from row above partial row */
            state->book_filter_idx = BOOK_FILTER_OPTIONS - 1;
        }
        consumed = true;

    } else if(
        event->key == InputKeyLeft &&
        (event->type == InputTypePress || event->type == InputTypeRepeat)) {
        /* Left from index 0 ("All") navigates back to BookList */
        if(state->book_filter_idx == 0) {
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        } else {
            state->book_filter_idx -= 1;
            consumed = true;
        }

    } else if(
        event->key == InputKeyRight &&
        (event->type == InputTypePress || event->type == InputTypeRepeat)) {
        /* Right moves to next column */
        if(state->book_filter_idx + 1 < BOOK_FILTER_OPTIONS) {
            state->book_filter_idx += 1;
        }
        consumed = true;

    } else if(event->key == InputKeyOk && event->type == InputTypeShort) {
        /* OK Short: apply filter and return to flat BookList */
        state->genre_mode_enabled = false;
        state->browse_depth = 0;
        uint8_t filtered[BIBLE_BOOK_COUNT];
        uint8_t filtered_count = get_filtered_books(state->book_filter_idx, filtered);
        if(filtered_count > 0) {
            state->selected_book = filtered[0];
            state->book_scroll = 0;
        }
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;

    } else if(event->key == InputKeyBack && event->type == InputTypeShort) {
        /* Back Short: let SceneManager handle via BookFilter scene on_event */
        consumed = false;
    }

    bible_app_request_redraw(app);
    return consumed;
}
