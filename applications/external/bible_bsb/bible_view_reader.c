#include "bible_view_reader.h"
#include "bible_renderer.h"
#include "bible_books.h"
#include "bible_loader.h"
#include "bible_storage.h"

#include <string.h>

/* ============================================================================
 * Reader layout constants
 * ============================================================================ */
#define READER_HEADER_Y                 8
#define READER_DIVIDER_Y                10
#define READER_CONTENT_Y                16
#define READER_HEADER_PAUSE_START_TICKS 50 /* ~1.7s pause at start (30fps) */
#define READER_HEADER_PAUSE_END_TICKS   50 /* ~1.7s pause at end (30fps) */
#define READER_HEADER_TICKS_PER_CHAR    7 /* ~4.3 chars/s horizontal scroll */

static void reader_draw_scrolling_text(
    Canvas* canvas,
    int16_t x,
    int16_t y,
    int16_t max_w,
    const char* text,
    uint32_t ticks,
    bool center_when_fits) {
    if(!canvas || !text || text[0] == '\0' || max_w <= 0) return;

    uint16_t full_w = canvas_string_width(canvas, text);
    if(full_w <= (uint16_t)max_w) {
        int16_t draw_x = center_when_fits ? (int16_t)(x + (max_w - (int16_t)full_w) / 2) : x;
        canvas_draw_str(canvas, draw_x, y, text);
        return;
    }

    size_t len = strlen(text);
    size_t max_offset = 0;
    while(max_offset + 1 < len &&
          canvas_string_width(canvas, text + max_offset) > (uint16_t)max_w) {
        max_offset++;
    }

    uint32_t scroll_span = (uint32_t)max_offset * READER_HEADER_TICKS_PER_CHAR;
    uint32_t cycle = READER_HEADER_PAUSE_START_TICKS + scroll_span + READER_HEADER_PAUSE_END_TICKS;
    uint32_t phase = ticks % cycle;

    size_t char_offset = 0;
    if(phase < READER_HEADER_PAUSE_START_TICKS) {
        char_offset = 0;
    } else if(phase < READER_HEADER_PAUSE_START_TICKS + scroll_span) {
        char_offset =
            (size_t)((phase - READER_HEADER_PAUSE_START_TICKS) / READER_HEADER_TICKS_PER_CHAR);
    } else {
        char_offset = max_offset;
    }

    char visible_buf[64];
    strlcpy(visible_buf, text + char_offset, sizeof(visible_buf));
    size_t vlen = strlen(visible_buf);
    while(vlen > 0 && canvas_string_width(canvas, visible_buf) > (uint16_t)max_w) {
        vlen--;
        visible_buf[vlen] = '\0';
    }

    canvas_draw_str(canvas, x, y, visible_buf);
}

/* ============================================================================
 * Display reference for the header
 * ============================================================================ */
static void bible_passage_display_ref(const BibleAppState* state, char* out, size_t out_len) {
    const BiblePassage* passage = state ? &state->passage : NULL;
    if(!passage || passage->verse_count == 0) {
        strlcpy(out, "No passage", out_len);
        return;
    }

    const char* book_name = state->selected_book_name[0] ? state->selected_book_name : "Book";
    if(passage->start_verse > 0) {
        if(passage->end_verse > passage->start_verse) {
            snprintf(
                out,
                out_len,
                "%s %u:%u-%u",
                book_name,
                (unsigned int)passage->chapter,
                (unsigned int)passage->start_verse,
                (unsigned int)passage->end_verse);
        } else {
            snprintf(
                out,
                out_len,
                "%s %u:%u",
                book_name,
                (unsigned int)passage->chapter,
                (unsigned int)passage->start_verse);
        }
    } else {
        snprintf(out, out_len, "%s %u", book_name, (unsigned int)passage->chapter);
    }
}

/* ============================================================================
 * Sticky section header lookup for the current verse
 * ============================================================================ */
static const char*
    bible_sticky_section_for_verse(const BiblePassage* passage, uint16_t current_verse) {
    if(!passage || passage->section_count == 0) return NULL;

    const char* active = NULL;
    for(uint8_t i = 0; i < passage->section_count; i++) {
        if(passage->sections[i].start_verse <= current_verse &&
           passage->sections[i].title[0] != '\0') {
            active = passage->sections[i].title;
        }
    }
    return active;
}

/* ============================================================================
 * Verse-first-line navigation helpers
 * ============================================================================ */

static uint16_t
    reader_next_verse_offset(const BibleLine* lines, uint16_t line_count, uint16_t current) {
    if(!lines || current >= line_count) return current;

    uint16_t current_verse = lines[current].verse_number;
    for(uint16_t i = current + 1; i < line_count; i++) {
        if(lines[i].verse_number > current_verse) {
            return i;
        }
    }
    return current; /* no next verse */
}

static uint16_t
    reader_prev_verse_offset(const BibleLine* lines, uint16_t line_count, uint16_t current) {
    if(!lines || current >= line_count || line_count == 0) return 0;

    uint16_t current_verse = lines[current].verse_number;
    bool is_first = lines[current].is_verse_number;

    if(!is_first) {
        /* Jump to first line of current verse */
        for(uint16_t i = 0; i <= current && i < line_count; i++) {
            if(lines[i].verse_number == current_verse && lines[i].is_verse_number) {
                return i;
            }
        }
    }

    /* Find previous verse number */
    uint16_t prev_verse = 0;
    for(uint16_t i = 0; i < current && i < line_count; i++) {
        if(lines[i].verse_number < current_verse && lines[i].is_verse_number) {
            prev_verse = lines[i].verse_number;
        }
    }

    if(prev_verse == 0) {
        return 0; /* already at first verse */
    }

    for(uint16_t i = 0; i < current && i < line_count; i++) {
        if(lines[i].verse_number == prev_verse && lines[i].is_verse_number) {
            return i;
        }
    }
    return 0;
}

static uint16_t reader_first_line_for_verse(
    const BibleLine* lines,
    uint16_t line_count,
    uint16_t target_verse) {
    if(!lines || line_count == 0) return 0;

    for(uint16_t i = 0; i < line_count; i++) {
        if(lines[i].verse_number >= target_verse && lines[i].is_verse_number) {
            return i;
        }
    }
    for(uint16_t i = 0; i < line_count; i++) {
        if(lines[i].verse_number >= target_verse) {
            return i;
        }
    }
    return 0;
}

static uint16_t reader_last_verse_offset(const BibleLine* lines, uint16_t line_count) {
    if(!lines || line_count == 0) return 0;

    uint16_t last_verse = lines[line_count - 1].verse_number;
    return reader_first_line_for_verse(lines, line_count, last_verse);
}

static uint16_t
    reader_max_scroll_offset(const BibleLine* lines, uint16_t line_count, uint16_t max_visible) {
    uint16_t page_max = (line_count > max_visible) ? (line_count - max_visible) : 0;
    uint16_t last_verse_start = reader_last_verse_offset(lines, line_count);
    return (last_verse_start > page_max) ? last_verse_start : page_max;
}

/* ============================================================================
 * Chapter & book boundary continuation helpers
 * ============================================================================ */

static void
    reader_sync_book_chapter_state(BibleAppState* state, uint8_t book_index, uint16_t chapter) {
    state->selected_book = book_index;
    if(book_index < BIBLE_BOOK_COUNT && OSIS_BOOK_NAMES[book_index]) {
        strlcpy(
            state->selected_book_name,
            OSIS_BOOK_NAMES[book_index],
            sizeof(state->selected_book_name));
    }
    state->selected_chapter = chapter;
    state->selected_start_verse = 1;
    state->selected_end_verse = 1;
    state->verse_select_mode = 0;
    state->selected_genre = get_genre_for_book(book_index);
    state->selected_testament = BIBLE_GENRES[state->selected_genre].testament;

    if(state->book_filter_idx != 0) {
        uint8_t filtered[BIBLE_BOOK_COUNT];
        uint8_t count = get_filtered_books(state->book_filter_idx, filtered);
        bool found = false;
        for(uint8_t i = 0; i < count; i++) {
            if(filtered[i] == book_index) {
                found = true;
                break;
            }
        }
        if(!found) {
            state->book_filter_idx = 0;
        }
    }
}

static void reader_advance_forward(BibleAppState* state) {
    if(!state || !state->lines || state->line_count == 0) return;

    uint8_t book = state->passage.book_index;
    uint16_t chapter = state->passage.chapter;
    uint16_t last_loaded_verse = state->lines[state->line_count - 1].verse_number;

    /* If reading a partial verse range that ends before the chapter's final verse,
     * expand to the full chapter and advance to the next verse in the same chapter. */
    if(state->passage.start_verse > 0 && last_loaded_verse < state->passage.verse_count) {
        uint16_t saved_scroll = state->scroll_offset;
        if(bible_load_chapter(book, chapter, 0, 0, state)) {
            reader_sync_book_chapter_state(state, book, chapter);
            state->reader_on_chapter_page = false;
            state->scroll_offset = reader_first_line_for_verse(
                state->lines, state->line_count, last_loaded_verse + 1);
        } else {
            state->scroll_offset = saved_scroll;
        }
        return;
    }

    /* At the final verse of the chapter: show the next chapter/book's chapter page */
    uint8_t next_book = 0;
    uint16_t next_chapter = 0;
    if(!bible_next_chapter(book, chapter, &next_book, &next_chapter)) {
        return;
    }

    uint16_t saved_scroll = state->scroll_offset;
    if(bible_load_chapter(next_book, next_chapter, 0, 0, state)) {
        reader_sync_book_chapter_state(state, next_book, next_chapter);
        state->scroll_offset = 0;
        state->reader_on_chapter_page = true;
    } else {
        state->scroll_offset = saved_scroll;
    }
}

static void reader_retreat_backward(BibleAppState* state) {
    if(!state || !state->lines || state->line_count == 0) return;

    uint8_t book = state->passage.book_index;
    uint16_t chapter = state->passage.chapter;
    uint16_t first_loaded_verse = state->lines[0].verse_number;

    /* If reading a partial verse range starting after verse 1,
     * expand to the full chapter and step back to the preceding verse. */
    if(state->passage.start_verse > 0 && first_loaded_verse > 1) {
        uint16_t saved_scroll = state->scroll_offset;
        if(bible_load_chapter(book, chapter, 0, 0, state)) {
            reader_sync_book_chapter_state(state, book, chapter);
            state->reader_on_chapter_page = false;
            state->scroll_offset = reader_first_line_for_verse(
                state->lines, state->line_count, first_loaded_verse - 1);
        } else {
            state->scroll_offset = saved_scroll;
        }
        return;
    }

    /* At verse 1: show the chapter page if a previous chapter exists */
    uint8_t prev_book = 0;
    uint16_t prev_chapter = 0;
    if(bible_prev_chapter(book, chapter, &prev_book, &prev_chapter)) {
        state->reader_on_chapter_page = true;
    }
}

static void reader_retreat_from_chapter_page(BibleAppState* state, bool to_last_line) {
    if(!state) return;

    uint8_t prev_book = 0;
    uint16_t prev_chapter = 0;
    if(!bible_prev_chapter(
           state->passage.book_index, state->passage.chapter, &prev_book, &prev_chapter)) {
        state->reader_on_chapter_page = false;
        state->scroll_offset = 0;
        return;
    }

    if(bible_load_chapter(prev_book, prev_chapter, 0, 0, state)) {
        reader_sync_book_chapter_state(state, prev_book, prev_chapter);
        state->reader_on_chapter_page = false;
        if(to_last_line) {
            uint16_t max_visible = bible_max_visible_lines(READER_CONTENT_Y);
            state->scroll_offset =
                reader_max_scroll_offset(state->lines, state->line_count, max_visible);
        } else {
            state->scroll_offset = reader_last_verse_offset(state->lines, state->line_count);
        }
    }
}

/* ============================================================================
 * Reader view — draw callback
 * ============================================================================ */
void bible_bsb_view_reader_draw(Canvas* canvas, void* ctx) {
    BibleApp* app = bible_app_from_draw(ctx);
    if(!canvas) return;
    canvas_reset(canvas);
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    if(!app || !app->state) {
        canvas_draw_str(canvas, 2, 24, "Reader");
        return;
    }
    BibleAppState* state = app->state;
    if(state->show_data_help) {
        bible_draw_missing_data(canvas);
        return;
    }

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    /* Header with sticky section title on left + compact OSIS ref (e.g. JHN 3:16) on right */
    if(state->passage.verse_count > 0) {
        canvas_set_font(canvas, FontSecondary);

        uint16_t current_verse = 1;
        uint8_t right_w = 0;
        char osis_ref[20];
        osis_ref[0] = '\0';
        const char* osis = (state->passage.book_index < BIBLE_BOOK_COUNT &&
                            OSIS_BOOK_CODES[state->passage.book_index]) ?
                               OSIS_BOOK_CODES[state->passage.book_index] :
                               "BSB";

        if(!state->reader_on_chapter_page && state->lines && state->line_count > 0) {
            uint16_t idx = (state->scroll_offset < state->line_count) ?
                               state->scroll_offset :
                               (uint16_t)(state->line_count - 1);
            current_verse = state->lines[idx].verse_number;
            if(current_verse == 0) current_verse = 1;

            snprintf(
                osis_ref,
                sizeof(osis_ref),
                "%s %u:%u",
                osis,
                (unsigned int)state->passage.chapter,
                (unsigned int)current_verse);
            uint8_t text_w = canvas_string_width(canvas, osis_ref);
            right_w = text_w + 5;
        }

        char header_buf[64];
        const char* sticky = (!state->reader_on_chapter_page) ?
                                 bible_sticky_section_for_verse(&state->passage, current_verse) :
                                 NULL;
        const void* scroll_key = state->reader_on_chapter_page ?
                                     (const void*)1 :
                                     (sticky ? (const void*)sticky : (const void*)2);
        if(state->header_scroll_key != scroll_key) {
            state->header_scroll_key = scroll_key;
            state->header_scroll_ticks = 0;
        }

        if(sticky && sticky[0] != '\0') {
            strlcpy(header_buf, sticky, sizeof(header_buf));
        } else {
            bible_passage_display_ref(state, header_buf, sizeof(header_buf));
        }

        /* Fit or horizontally scroll sticky header to the space left of the OSIS badge */
        int16_t max_left_w = (int16_t)BIBLE_SCREEN_WIDTH - (int16_t)right_w - BIBLE_MARGIN_X -
                             (right_w > 0 ? 4 : BIBLE_MARGIN_X);
        if(max_left_w < 20) max_left_w = 20;

        reader_draw_scrolling_text(
            canvas,
            BIBLE_MARGIN_X,
            READER_HEADER_Y,
            max_left_w,
            header_buf,
            state->header_scroll_ticks,
            false);

        if(right_w > 0) {
            int16_t box_x = BIBLE_SCREEN_WIDTH - (int16_t)right_w;
            if(box_x < 0) box_x = 0;

            /* Inverted black tab anchored to top-right & divider line */
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_box(canvas, (uint8_t)box_x, 0, right_w, READER_DIVIDER_Y + 1);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, box_x + 3, READER_HEADER_Y, osis_ref);
            canvas_set_color(canvas, ColorBlack);
        }

        canvas_draw_line(canvas, 0, READER_DIVIDER_Y, BIBLE_SCREEN_WIDTH, READER_DIVIDER_Y);
    }

    /* Chapter boundary page */
    if(state->reader_on_chapter_page) {
        const char* book_name = state->selected_book_name[0] ? state->selected_book_name : "Book";
        char chapter_str[24];
        snprintf(
            chapter_str, sizeof(chapter_str), "Chapter %u", (unsigned int)state->passage.chapter);

        bool has_subtitle =
            (state->passage.section_count > 0 && state->passage.sections[0].title[0] != '\0');
        int16_t book_y = has_subtitle ? 26 : 32;
        int16_t ch_y = has_subtitle ? 40 : 46;

        canvas_set_font(canvas, FontPrimary);
        uint8_t book_w = canvas_string_width(canvas, book_name);
        int16_t book_x = (BIBLE_SCREEN_WIDTH > book_w) ?
                             (int16_t)((BIBLE_SCREEN_WIDTH - book_w) / 2) :
                             BIBLE_MARGIN_X;
        canvas_draw_str(canvas, book_x, book_y, book_name);

        uint8_t ch_w = canvas_string_width(canvas, chapter_str);
        int16_t ch_x = (BIBLE_SCREEN_WIDTH > ch_w) ? (int16_t)((BIBLE_SCREEN_WIDTH - ch_w) / 2) :
                                                     BIBLE_MARGIN_X;
        canvas_draw_str(canvas, ch_x, ch_y, chapter_str);

        if(has_subtitle) {
            canvas_set_font(canvas, FontSecondary);
            reader_draw_scrolling_text(
                canvas,
                BIBLE_MARGIN_X,
                54,
                BIBLE_SCREEN_WIDTH - (BIBLE_MARGIN_X * 2),
                state->passage.sections[0].title,
                state->header_scroll_ticks,
                true);
        }

        bible_draw_toast(canvas, &state->toast);
        return;
    }

    /* Render 4 lines of paginated text */
    bible_render_page(
        canvas, state->lines, state->line_count, state->scroll_offset, READER_CONTENT_Y);

    /* Toast */
    bible_draw_toast(canvas, &state->toast);
}

/* ============================================================================
 * Reader view — input callback
 * ============================================================================ */
bool bible_bsb_view_reader_input(InputEvent* event, void* ctx) {
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

    uint16_t max_visible = bible_max_visible_lines(READER_CONTENT_Y);
    uint16_t max_scroll = reader_max_scroll_offset(state->lines, state->line_count, max_visible);

    bool consumed = false;

    if(event->key == InputKeyUp &&
       (event->type == InputTypePress || event->type == InputTypeRepeat)) {
        /* Up: jump to previous verse's first line, or retreat across chapter boundary */
        if(state->reader_on_chapter_page) {
            reader_retreat_from_chapter_page(state, false);
        } else if(state->scroll_offset == 0) {
            reader_retreat_backward(state);
        } else {
            uint16_t new_offset =
                reader_prev_verse_offset(state->lines, state->line_count, state->scroll_offset);
            state->scroll_offset = new_offset;
        }
        consumed = true;

    } else if(
        event->key == InputKeyDown &&
        (event->type == InputTypePress || event->type == InputTypeRepeat)) {
        /* Down: proceed from chapter page, jump to next verse, or advance to next chapter page */
        if(state->reader_on_chapter_page) {
            state->reader_on_chapter_page = false;
            state->scroll_offset = 0;
        } else {
            uint16_t new_offset =
                reader_next_verse_offset(state->lines, state->line_count, state->scroll_offset);
            if(new_offset > state->scroll_offset) {
                state->scroll_offset = (new_offset <= max_scroll) ? new_offset : max_scroll;
            } else {
                reader_advance_forward(state);
            }
        }
        consumed = true;

    } else if(
        event->key == InputKeyLeft &&
        (event->type == InputTypePress || event->type == InputTypeRepeat)) {
        /* Left: scroll up by 1 line, or retreat across chapter boundary */
        if(state->reader_on_chapter_page) {
            reader_retreat_from_chapter_page(state, true);
        } else if(state->scroll_offset > 0) {
            state->scroll_offset -= 1;
        } else {
            reader_retreat_backward(state);
        }
        consumed = true;

    } else if(
        event->key == InputKeyRight &&
        (event->type == InputTypePress || event->type == InputTypeRepeat)) {
        /* Right: proceed from chapter page, scroll down by 1 line, or advance to next chapter page */
        if(state->reader_on_chapter_page) {
            state->reader_on_chapter_page = false;
            state->scroll_offset = 0;
        } else if(state->scroll_offset < max_scroll) {
            state->scroll_offset += 1;
        } else {
            reader_advance_forward(state);
        }
        consumed = true;

    } else if(event->key == InputKeyOk && event->type == InputTypeLong) {
        /* OK Long: open range picker pre-populated with current verse to save */
        bible_reader_open_verse_select(state, app->scene_manager, false);
        consumed = true;

    } else if(event->key == InputKeyOk && event->type == InputTypeShort) {
        /* OK Short: open ActionMenu with goto_verse initialized to current verse */
        state->action_menu_selection = 0;
        state->action_menu_from_collection = false;
        if(!state->reader_on_chapter_page && state->lines && state->line_count > 0) {
            uint16_t idx = (state->scroll_offset < state->line_count) ?
                               state->scroll_offset :
                               (uint16_t)(state->line_count - 1);
            state->goto_verse =
                state->lines[idx].verse_number > 0 ? state->lines[idx].verse_number : 1;
        } else {
            state->goto_verse = 1;
        }
        scene_manager_next_scene(app->scene_manager, BibleSceneActionMenu);
        consumed = true;

    } else if(event->key == InputKeyBack && event->type == InputTypeShort) {
        /* Back: return to Collection, BookList (for 1-chapter books), or ChapterList */
        if(state->reader_came_from_collection) {
            scene_manager_search_and_switch_to_another_scene(
                app->scene_manager, BibleSceneCollection);
        } else if(
            state->selected_book < BIBLE_BOOK_COUNT &&
            BOOK_CHAPTER_COUNTS[state->selected_book] == 1) {
            scene_manager_search_and_switch_to_another_scene(
                app->scene_manager, BibleSceneBookList);
        } else {
            scene_manager_search_and_switch_to_another_scene(
                app->scene_manager, BibleSceneChapterList);
        }
        consumed = true;
    }

    bible_app_request_redraw(app);
    return consumed;
}

void bible_reader_open_verse_select(
    BibleAppState* state,
    SceneManager* scene_manager,
    bool for_nfc) {
    if(!state || !scene_manager) return;

    state->verse_select_for_nfc = for_nfc;
    state->selected_book = state->passage.book_index;
    if(state->selected_book < BIBLE_BOOK_COUNT && OSIS_BOOK_NAMES[state->selected_book]) {
        strlcpy(
            state->selected_book_name,
            OSIS_BOOK_NAMES[state->selected_book],
            sizeof(state->selected_book_name));
    }
    state->selected_chapter = state->passage.chapter;

    if(state->reader_on_chapter_page || !state->lines || state->line_count == 0) {
        state->selected_start_verse = 1;
        state->selected_end_verse = 1;
        state->verse_select_mode = 0; /* Full chapter */
    } else {
        uint16_t idx = (state->scroll_offset < state->line_count) ?
                           state->scroll_offset :
                           (uint16_t)(state->line_count - 1);
        uint16_t cur_v = state->lines[idx].verse_number;
        if(cur_v == 0) cur_v = 1;
        state->selected_start_verse = cur_v;
        state->selected_end_verse = cur_v;
        state->verse_select_mode =
            2; /* Highlight End row so OK saves cur_v or Right extends range */
    }

    scene_manager_next_scene(scene_manager, BibleSceneVerseSelect);
}
