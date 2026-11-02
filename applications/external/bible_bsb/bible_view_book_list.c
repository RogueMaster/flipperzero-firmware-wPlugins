#include "bible_app.h"
#include "bible_books.h"
#include "bible_loader.h"
#include "bible_renderer.h"

#include <string.h>

/* ============================================================================
 * BookList layout constants
 * ============================================================================ */
#define BOOK_LIST_LINE_HEIGHT   10
#define BOOK_LIST_HEADER_HEIGHT 12

/* ============================================================================
 * BookList view — draw callback
 * ============================================================================ */
void bible_bsb_view_book_list_draw(Canvas* canvas, void* ctx) {
    BibleApp* app = bible_app_from_draw(ctx);
    if(!canvas) return;
    canvas_reset(canvas);
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    if(!app || !app->state) {
        canvas_draw_str(canvas, 2, 24, "Book list");
        return;
    }
    BibleAppState* state = app->state;
    if(!state->bsb_data_present) {
        bible_draw_missing_data(canvas);
        return;
    }
    if(state->selected_book < BIBLE_BOOK_COUNT && OSIS_BOOK_NAMES[state->selected_book]) {
        strlcpy(
            state->selected_book_name,
            OSIS_BOOK_NAMES[state->selected_book],
            sizeof(state->selected_book_name));
    }

    uint8_t max_visible =
        (BIBLE_SCREEN_HEIGHT - BOOK_LIST_HEADER_HEIGHT - 2) / BOOK_LIST_LINE_HEIGHT;
    if(max_visible == 0) max_visible = 1;

    /* Hierarchy mode: Testament -> Genre (when no A-Z filter overrides it) */
    if(state->genre_mode_enabled && state->book_filter_idx == 0 && state->browse_depth < 2) {
        if(state->browse_depth == 0) {
            /* Step 1: Testament selection */
            canvas_draw_str(canvas, BIBLE_MARGIN_X, 10, "< Testament >");
            canvas_draw_line(
                canvas, 0, BOOK_LIST_HEADER_HEIGHT, BIBLE_SCREEN_WIDTH, BOOK_LIST_HEADER_HEIGHT);

            for(uint8_t i = 0; i < BIBLE_TESTAMENT_COUNT; i++) {
                int16_t y =
                    BOOK_LIST_HEADER_HEIGHT + 2 + (i * BOOK_LIST_LINE_HEIGHT) + BIBLE_CHAR_HEIGHT;
                const char* label = BIBLE_TESTAMENT_LABELS[i];
                if(i == state->selected_testament) {
                    bible_draw_inverted_highlight(
                        canvas,
                        0,
                        (uint8_t)(y - 9),
                        BIBLE_SCREEN_WIDTH,
                        BOOK_LIST_LINE_HEIGHT,
                        label,
                        BIBLE_MARGIN_X + 4,
                        (uint8_t)y);
                } else {
                    canvas_draw_str(canvas, BIBLE_MARGIN_X + 4, y, label);
                }
            }
        } else {
            /* Step 2: Genre selection within selected Testament */
            canvas_draw_str(
                canvas,
                BIBLE_MARGIN_X,
                10,
                state->selected_testament == 0 ? "< Old Testament >" : "< New Testament >");
            canvas_draw_line(
                canvas, 0, BOOK_LIST_HEADER_HEIGHT, BIBLE_SCREEN_WIDTH, BOOK_LIST_HEADER_HEIGHT);

            uint8_t genres[BIBLE_GENRE_COUNT];
            uint8_t genre_count = get_genres_for_testament(state->selected_testament, genres);
            for(uint8_t i = 0; i < genre_count && i < max_visible; i++) {
                uint8_t g_idx = genres[i];
                int16_t y =
                    BOOK_LIST_HEADER_HEIGHT + 2 + (i * BOOK_LIST_LINE_HEIGHT) + BIBLE_CHAR_HEIGHT;
                const char* label = BIBLE_GENRES[g_idx].name;
                if(g_idx == state->selected_genre) {
                    bible_draw_inverted_highlight(
                        canvas,
                        0,
                        (uint8_t)(y - 9),
                        BIBLE_SCREEN_WIDTH,
                        BOOK_LIST_LINE_HEIGHT,
                        label,
                        BIBLE_MARGIN_X + 4,
                        (uint8_t)y);
                } else {
                    canvas_draw_str(canvas, BIBLE_MARGIN_X + 4, y, label);
                }
            }
        }

        bible_draw_toast(canvas, &state->toast);
        return;
    }

    /* Step 3 (Genre Books) or Flat Book List */
    char header[24];
    if(state->book_filter_idx != 0 && state->book_filter_idx < BIBLE_FILTER_COUNT) {
        snprintf(header, sizeof(header), "< %s >", BIBLE_FILTER_LABELS[state->book_filter_idx]);
    } else if(state->genre_mode_enabled && state->selected_genre < BIBLE_GENRE_COUNT) {
        strlcpy(header, BIBLE_GENRES[state->selected_genre].header_name, sizeof(header));
    } else {
        strlcpy(header, "< All >", sizeof(header));
    }
    canvas_draw_str(canvas, BIBLE_MARGIN_X, 10, header);

    /* Divider line */
    canvas_draw_line(
        canvas, 0, BOOK_LIST_HEADER_HEIGHT, BIBLE_SCREEN_WIDTH, BOOK_LIST_HEADER_HEIGHT);

    /* Build filtered list (by Genre or by A-Z filter) */
    uint8_t filtered[BIBLE_BOOK_COUNT];
    uint8_t filtered_count = 0;
    if(state->genre_mode_enabled && state->book_filter_idx == 0) {
        filtered_count = get_books_for_genre(state->selected_genre, filtered);
    } else {
        filtered_count = get_filtered_books(state->book_filter_idx, filtered);
    }
    if(filtered_count == 0) {
        bible_draw_toast(canvas, &state->toast);
        return;
    }

    uint8_t start_idx = state->book_scroll;
    if(start_idx + max_visible > filtered_count) {
        start_idx = filtered_count > max_visible ? filtered_count - max_visible : 0;
    }

    /* Draw each visible book with a full-width inverted bar. */
    for(uint8_t i = 0; i < max_visible; i++) {
        uint8_t list_idx = start_idx + i;
        if(list_idx >= filtered_count) break;

        uint8_t book_idx = filtered[list_idx];
        if(book_idx >= BIBLE_BOOK_COUNT) continue;
        int16_t y = BOOK_LIST_HEADER_HEIGHT + 2 + (i * BOOK_LIST_LINE_HEIGHT) + BIBLE_CHAR_HEIGHT;
        const char* name = OSIS_BOOK_NAMES[book_idx];
        if(!name) continue;
        if(book_idx == state->selected_book) {
            bible_draw_inverted_highlight(
                canvas,
                0,
                (uint8_t)(y - 9),
                BIBLE_SCREEN_WIDTH,
                BOOK_LIST_LINE_HEIGHT,
                name,
                BIBLE_MARGIN_X + 4,
                (uint8_t)y);
        } else {
            canvas_draw_str(canvas, BIBLE_MARGIN_X + 4, y, name);
        }
    }

    bible_draw_scroll_indicator(
        canvas, state->book_scroll, filtered_count, max_visible, BOOK_LIST_HEADER_HEIGHT);
    bible_draw_toast(canvas, &state->toast);
}

/* ============================================================================
 * BookList view — input callback
 * ============================================================================ */
bool bible_bsb_view_book_list_input(InputEvent* event, void* ctx) {
    BibleApp* app = ctx;
    BibleAppState* state = app->state;

    /* Only process Press, Repeat, Short, Long — ignore Release */
    if(event->type != InputTypePress && event->type != InputTypeRepeat &&
       event->type != InputTypeShort && event->type != InputTypeLong) {
        return false;
    }

    if(!state->bsb_data_present) {
        if(event->key == InputKeyOk && event->type == InputTypeShort) {
            state->bsb_data_present = bible_chapter_data_present();
        } else if(event->key == InputKeyBack && event->type == InputTypeShort) {
            bible_app_request_redraw(app);
            return false;
        }
        bible_app_request_redraw(app);
        return true;
    }

    uint8_t max_visible =
        (BIBLE_SCREEN_HEIGHT - BOOK_LIST_HEADER_HEIGHT - 2) / BOOK_LIST_LINE_HEIGHT;
    if(max_visible == 0) max_visible = 1;

    /* Hierarchy Mode — Depth 0 (< Testament >) & Depth 1 (< Old/New Testament >) */
    if(state->genre_mode_enabled && state->book_filter_idx == 0 && state->browse_depth < 2) {
        bool consumed = false;

        if(state->browse_depth == 0) {
            /* Testament level: between < All > (Left) and < Select Filter (Right) */
            if((event->key == InputKeyUp || event->key == InputKeyDown) &&
               (event->type == InputTypePress || event->type == InputTypeRepeat)) {
                state->selected_testament = (state->selected_testament == 0) ? 1 : 0;
                consumed = true;
            } else if(event->key == InputKeyOk && event->type == InputTypeShort) {
                uint8_t genres[BIBLE_GENRE_COUNT];
                uint8_t g_count = get_genres_for_testament(state->selected_testament, genres);
                if(g_count > 0) {
                    if(BIBLE_GENRES[state->selected_genre].testament !=
                       state->selected_testament) {
                        state->selected_genre = genres[0];
                    }
                    state->browse_depth = 1;
                }
                consumed = true;
            } else if(event->key == InputKeyLeft && event->type == InputTypeShort) {
                /* Slide Left back to < All > */
                state->genre_mode_enabled = false;
                state->browse_depth = 0;
                state->book_scroll = (state->selected_book >= max_visible) ?
                                         (state->selected_book - (max_visible - 1)) :
                                         0;
                consumed = true;
            } else if(event->key == InputKeyRight && event->type == InputTypeShort) {
                /* Slide Right to < Select Filter */
                scene_manager_next_scene(app->scene_manager, BibleSceneBookFilter);
                consumed = true;
            }
        } else {
            /* Genre level (browse_depth == 1) */
            uint8_t genres[BIBLE_GENRE_COUNT];
            uint8_t g_count = get_genres_for_testament(state->selected_testament, genres);
            uint8_t cur_g = 0;
            for(uint8_t i = 0; i < g_count; i++) {
                if(genres[i] == state->selected_genre) {
                    cur_g = i;
                    break;
                }
            }

            if(event->key == InputKeyUp &&
               (event->type == InputTypePress || event->type == InputTypeRepeat)) {
                if(g_count > 0) {
                    uint8_t next_g = (cur_g > 0) ? (cur_g - 1) : (g_count - 1);
                    state->selected_genre = genres[next_g];
                }
                consumed = true;
            } else if(
                event->key == InputKeyDown &&
                (event->type == InputTypePress || event->type == InputTypeRepeat)) {
                if(g_count > 0) {
                    uint8_t next_g = (cur_g + 1 < g_count) ? (cur_g + 1) : 0;
                    state->selected_genre = genres[next_g];
                }
                consumed = true;
            } else if(event->key == InputKeyOk && event->type == InputTypeShort) {
                const BibleGenre* g = &BIBLE_GENRES[state->selected_genre];
                state->selected_book = g->start_book;
                state->book_scroll = 0;
                if(g->start_book == g->end_book) {
                    /* Single-book genre (Acts, Revelation): jump straight to ChapterList */
                    if(state->selected_book < BIBLE_BOOK_COUNT &&
                       OSIS_BOOK_NAMES[state->selected_book]) {
                        strlcpy(
                            state->selected_book_name,
                            OSIS_BOOK_NAMES[state->selected_book],
                            sizeof(state->selected_book_name));
                    }
                    state->selected_chapter = 1;
                    scene_manager_next_scene(app->scene_manager, BibleSceneChapterList);
                } else {
                    state->browse_depth = 2;
                }
                consumed = true;
            } else if(event->key == InputKeyLeft && event->type == InputTypeShort) {
                state->browse_depth = 0;
                consumed = true;
            } else if(event->key == InputKeyRight && event->type == InputTypeShort) {
                scene_manager_next_scene(app->scene_manager, BibleSceneBookFilter);
                consumed = true;
            }
        }

        bible_app_request_redraw(app);
        return consumed;
    }

    /* Build filtered list (by Genre at browse_depth == 2, or by A-Z filter) */
    uint8_t filtered[BIBLE_BOOK_COUNT];
    uint8_t filtered_count = 0;
    if(state->genre_mode_enabled && state->book_filter_idx == 0) {
        filtered_count = get_books_for_genre(state->selected_genre, filtered);
    } else {
        filtered_count = get_filtered_books(state->book_filter_idx, filtered);
    }
    if(filtered_count == 0) {
        return false;
    }

    /* Find current position within filtered list */
    uint8_t current_idx = 0;
    for(uint8_t i = 0; i < filtered_count; i++) {
        if(filtered[i] == state->selected_book) {
            current_idx = i;
            break;
        }
    }

    bool consumed = false;

    if(event->key == InputKeyUp &&
       (event->type == InputTypePress || event->type == InputTypeRepeat)) {
        /* Move up, wrap to end */
        uint8_t next_idx = (current_idx > 0) ? current_idx - 1 : filtered_count - 1;
        state->selected_book = filtered[next_idx];

        /* Auto-scroll */
        if(next_idx < state->book_scroll) {
            state->book_scroll = next_idx;
        } else if(next_idx >= state->book_scroll + max_visible) {
            state->book_scroll = next_idx >= max_visible ? next_idx - (max_visible - 1) : 0;
        }
        consumed = true;

    } else if(
        event->key == InputKeyDown &&
        (event->type == InputTypePress || event->type == InputTypeRepeat)) {
        /* Move down, wrap to start */
        uint8_t next_idx = (current_idx + 1 < filtered_count) ? current_idx + 1 : 0;
        state->selected_book = filtered[next_idx];

        /* Auto-scroll */
        if(next_idx >= state->book_scroll + max_visible) {
            state->book_scroll = next_idx >= max_visible ? next_idx - (max_visible - 1) : 0;
        } else if(next_idx < state->book_scroll) {
            state->book_scroll = next_idx;
        }
        consumed = true;

    } else if(event->key == InputKeyLeft && event->type == InputTypeShort) {
        if(state->genre_mode_enabled && state->book_filter_idx == 0) {
            /* Step back up to Genre list */
            state->browse_depth = 1;
        } else {
            /* From < All >, slide Left to Collection > */
            scene_manager_next_scene(app->scene_manager, BibleSceneCollection);
        }
        consumed = true;

    } else if(event->key == InputKeyRight && event->type == InputTypeShort) {
        if(!state->genre_mode_enabled && state->book_filter_idx == 0) {
            /* From < All >, slide Right to < Testament > */
            state->genre_mode_enabled = true;
            state->browse_depth = 0;
            state->selected_genre = get_genre_for_book(state->selected_book);
            state->selected_testament = BIBLE_GENRES[state->selected_genre].testament;
        } else {
            /* Slide Right to < Select Filter */
            scene_manager_next_scene(app->scene_manager, BibleSceneBookFilter);
        }
        consumed = true;

    } else if(event->key == InputKeyOk && event->type == InputTypeShort) {
        /* Single-chapter books skip the 1-cell ChapterList and open Reader directly */
        state->selected_chapter = 1;
        if(state->selected_book < BIBLE_BOOK_COUNT && OSIS_BOOK_NAMES[state->selected_book]) {
            strlcpy(
                state->selected_book_name,
                OSIS_BOOK_NAMES[state->selected_book],
                sizeof(state->selected_book_name));
        }
        if(state->selected_book < BIBLE_BOOK_COUNT &&
           BOOK_CHAPTER_COUNTS[state->selected_book] == 1) {
            bool loaded = bible_load_chapter(state->selected_book, 1, 0, 0, state);
            if(loaded || state->show_data_help) {
                state->scroll_offset = 0;
                state->reader_on_chapter_page = false;
                state->reader_came_from_collection = false;
                scene_manager_next_scene(app->scene_manager, BibleSceneReader);
            } else if(!bible_toast_active(&state->toast)) {
                bible_toast_set(&state->toast, "No BSB data");
            }
        } else {
            scene_manager_next_scene(app->scene_manager, BibleSceneChapterList);
        }
        consumed = true;

    } else if(event->key == InputKeyBack && event->type == InputTypeShort) {
        /* Let SceneManager handle Back: BookList scene on_event will reset
         * filter (if active) or stop the app (if already "All"). */
        consumed = false;
    }

    bible_app_request_redraw(app);
    return consumed;
}
