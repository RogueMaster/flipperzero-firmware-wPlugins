#include "bible_app.h"
#include "bible_books.h"
#include "bible_loader.h"
#include "bible_renderer.h"

#include <string.h>

/* ============================================================================
 * ChapterList layout constants
 * ============================================================================ */
#define CHAPTER_LIST_COLS     5
#define CHAPTER_LIST_CELL_W   24
#define CHAPTER_LIST_CELL_H   14
#define CHAPTER_LIST_HEADER_H 12
#define CHAPTER_LIST_MAX_ROWS 3

/* ============================================================================
 * ChapterList view — draw callback
 * ============================================================================ */
void bible_bsb_view_chapter_list_draw(Canvas* canvas, void* ctx) {
    BibleApp* app = bible_app_from_draw(ctx);
    if(!canvas) return;
    canvas_reset(canvas);
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    if(!app || !app->state) {
        canvas_draw_str(canvas, 2, 24, "Chapters");
        return;
    }
    BibleAppState* state = app->state;

    /* Name was copied on the book list. Do not index the rodata pointer table here. */
    canvas_draw_str(canvas, BIBLE_MARGIN_X, 10, state->selected_book_name);
    canvas_draw_line(canvas, 0, CHAPTER_LIST_HEADER_H, BIBLE_SCREEN_WIDTH, CHAPTER_LIST_HEADER_H);

    uint8_t max_chapter = BOOK_CHAPTER_COUNTS[state->selected_book];
    uint8_t total_rows = (max_chapter + CHAPTER_LIST_COLS - 1) / CHAPTER_LIST_COLS;
    if(total_rows < 1) total_rows = 1;

    uint8_t selected_row = (state->selected_chapter - 1) / CHAPTER_LIST_COLS;

    uint8_t start_row =
        (selected_row >= CHAPTER_LIST_MAX_ROWS) ? (selected_row - CHAPTER_LIST_MAX_ROWS + 1) : 0;

    for(uint8_t row = start_row; row < start_row + CHAPTER_LIST_MAX_ROWS && row < total_rows;
        row++) {
        for(uint8_t col = 0; col < CHAPTER_LIST_COLS; col++) {
            uint8_t chapter_num = row * CHAPTER_LIST_COLS + col + 1;
            if(chapter_num > max_chapter) {
                break;
            }

            uint8_t display_row = row - start_row;
            uint8_t x = 4 + (col * CHAPTER_LIST_CELL_W);
            uint8_t y = CHAPTER_LIST_HEADER_H + 4 + (display_row * CHAPTER_LIST_CELL_H);
            bool is_selected = (chapter_num == state->selected_chapter);

            char num[4];
            snprintf(num, sizeof(num), "%u", (unsigned int)chapter_num);
            uint8_t cell_box_w = CHAPTER_LIST_CELL_W - 2;
            uint8_t num_w = canvas_string_width(canvas, num);
            uint8_t text_x = x + ((cell_box_w > num_w) ? ((cell_box_w - num_w) / 2) : 1);
            if(is_selected) {
                bible_draw_inverted_highlight(
                    canvas, x, y, cell_box_w, CHAPTER_LIST_CELL_H - 2, num, text_x, y + 10);
            } else {
                canvas_draw_str(canvas, text_x, y + 10, num);
            }
        }
    }

    if(total_rows > CHAPTER_LIST_MAX_ROWS) {
        int32_t thumb_height = (CHAPTER_LIST_MAX_ROWS * 50) / total_rows;
        if(thumb_height < 4) thumb_height = 4;
        int32_t thumb_y = 14 + (int32_t)start_row * (50 - thumb_height) /
                                   (int32_t)(total_rows - CHAPTER_LIST_MAX_ROWS);
        canvas_draw_box(canvas, 126, thumb_y, 2, (uint8_t)thumb_height);
    }
}

/* ============================================================================
 * ChapterList view — input callback
 * ============================================================================ */
bool bible_bsb_view_chapter_list_input(InputEvent* event, void* ctx) {
    BibleApp* app = ctx;
    BibleAppState* state = app->state;

    /* Only process Press, Repeat, Short, Long — ignore Release */
    if(event->type != InputTypePress && event->type != InputTypeRepeat &&
       event->type != InputTypeShort && event->type != InputTypeLong) {
        return false;
    }

    uint8_t max_chapter = BOOK_CHAPTER_COUNTS[state->selected_book];
    uint8_t cols = CHAPTER_LIST_COLS;
    bool consumed = false;

    if(event->key == InputKeyUp &&
       (event->type == InputTypePress || event->type == InputTypeRepeat)) {
        /* Move up by 5 chapters */
        if(state->selected_chapter > cols) {
            state->selected_chapter -= cols;
        }
        consumed = true;

    } else if(
        event->key == InputKeyDown &&
        (event->type == InputTypePress || event->type == InputTypeRepeat)) {
        /* Move down by 5 chapters, clamped to max */
        if(state->selected_chapter + cols <= max_chapter) {
            state->selected_chapter += cols;
        } else if(state->selected_chapter < max_chapter) {
            state->selected_chapter = max_chapter;
        }
        consumed = true;

    } else if(
        event->key == InputKeyLeft &&
        (event->type == InputTypePress || event->type == InputTypeRepeat)) {
        /* Decrement chapter by 1 */
        if(state->selected_chapter > 1) {
            state->selected_chapter -= 1;
        }
        consumed = true;

    } else if(
        event->key == InputKeyRight &&
        (event->type == InputTypePress || event->type == InputTypeRepeat)) {
        /* Increment chapter by 1 */
        if(state->selected_chapter < max_chapter) {
            state->selected_chapter += 1;
        }
        consumed = true;

    } else if(event->key == InputKeyOk && event->type == InputTypeShort) {
        /* Load full chapter directly and open Reader */
        state->verse_select_mode = 0;
        state->selected_start_verse = 1;
        state->selected_end_verse = 1;
        bool loaded =
            bible_load_chapter(state->selected_book, state->selected_chapter, 0, 0, state);
        if(loaded && state->passage.verse_count > 0) {
            state->scroll_offset = 0;
            state->reader_on_chapter_page = false;
            state->reader_came_from_collection = false;
            scene_manager_next_scene(app->scene_manager, BibleSceneReader);
        } else if(state->show_data_help) {
            scene_manager_next_scene(app->scene_manager, BibleSceneReader);
        } else if(!bible_toast_active(&state->toast)) {
            bible_toast_set(&state->toast, "No BSB data");
        }
        consumed = true;

    } else if(event->key == InputKeyBack && event->type == InputTypeShort) {
        /* Return to BookList — let SceneManager handle it via on_event. */
        consumed = false;
    }

    bible_app_request_redraw(app);
    return consumed;
}
