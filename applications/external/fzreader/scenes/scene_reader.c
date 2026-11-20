/**
 * @file scene_reader.c
 * @brief Drawing and input handling of an open book.
 *
 * The page is laid out in the draw callback: the layout engine needs a canvas
 * to measure text, and drawing runs in the GUI thread. At most one page is
 * kept for the current position; it is reused while the position, the font
 * and the screen metrics stay the same, so a bare redraw does not lay the
 * chapter out again.
 *
 * Navigation only moves the page anchor: the anchor stays the first line of
 * the page, so switching the font re-lays out from the same place. The history
 * keeps the anchors of the pages left behind, so going back never has to walk a
 * chapter.
 */

#include "scene_reader.h"

#include <furi.h>
#include <gui/canvas.h>

#include <stdio.h>
#include <string.h>

#include "../fonts/reader_fonts.h"
#include "../text/utf8.h"
#include "scene_info.h"

#define SCENE_READER_MARGIN        1
/** Buffer for one measured string, longer lines are measured truncated. */
#define SCENE_READER_MEASURE_BYTES READER_LINE_TEXT
/** Buffer for the book title in the status bar. */
#define SCENE_READER_TITLE_BYTES   64
/** Font used for messages such as "The End", it must have Cyrillic glyphs. */
#define SCENE_READER_MESSAGE_FONT  2
/** Smallest font, the only one narrow enough for the status bar. */
#define SCENE_READER_STATUS_FONT   0

_Static_assert(READER_FONT_COUNT > SCENE_READER_MESSAGE_FONT, "message font index out of range");

/* ---------------------------------------------------------------- layout -- */

static char scene_reader_measure_buffer[SCENE_READER_MEASURE_BYTES];

static size_t scene_reader_measure(void* context, const char* text, size_t size) {
    Canvas* canvas = context;
    furi_check(canvas != NULL);

    if(size >= sizeof(scene_reader_measure_buffer)) {
        size = sizeof(scene_reader_measure_buffer) - 1;
    }
    memcpy(scene_reader_measure_buffer, text, size);
    scene_reader_measure_buffer[size] = '\0';
    return canvas_string_width(canvas, scene_reader_measure_buffer);
}

void scene_reader_metrics(
    Canvas* canvas,
    size_t font,
    ReaderLayoutMetrics* metrics,
    size_t* max_lines) {
    const size_t width = canvas_width(canvas);
    const size_t height = canvas_height(canvas);

    canvas_set_custom_u8g2_font(canvas, reader_font_get(SCENE_READER_STATUS_FONT));
    const size_t status_height = canvas_current_font_height(canvas);
    const size_t text_height = height > status_height ? height - status_height : height;

    canvas_set_custom_u8g2_font(canvas, reader_font_get(font));
    const size_t line_height = canvas_current_font_height(canvas);
    const size_t descender = reader_font_descender(font);
    const size_t body_height = text_height > descender ? text_height - descender : text_height;

    metrics->context = canvas;
    metrics->measure = scene_reader_measure;
    metrics->line_height = line_height;
    metrics->line_width = width > 2 * SCENE_READER_MARGIN ? width - 2 * SCENE_READER_MARGIN :
                                                            width;
    if(max_lines != NULL) {
        *max_lines = line_height > 0 ? body_height / line_height : 0;
    }
}

/**
 * Lay out the page anchored at app->pos if the cached one does not match.
 * @returns false if the page could not be laid out.
 */
static bool scene_reader_page_ensure(
    FzReaderApp* app,
    const ReaderLayoutMetrics* metrics,
    size_t max_lines) {
    if(app->page != NULL && app->page_valid && app->page_font == app->state.font &&
       app->page_max_lines == max_lines && app->page->start.chapter == app->pos.chapter &&
       app->page->start.offset == app->pos.offset) {
        return true;
    }

    /*
     * The layout keeps the chapter stream open between pages, so a page turn
     * continues where the previous one ended. The stream belongs to one
     * chapter: drop it when the page cache is replaced from another chapter.
     * Jumps inside a chapter, and changes of the font metrics, are detected by
     * reader_layout_page itself, which reopens the stream at the new anchor.
     */
    if(app->page != NULL && app->page->start.chapter != app->pos.chapter) {
        reader_layout_reset();
    }

    if(app->page != NULL) {
        reader_page_free(app->page);
        app->page = NULL;
    }
    app->page_valid = false;

    if(max_lines == 0) {
        return false;
    }

    app->page = reader_layout_page(app->book, app->pos, metrics, max_lines);
    if(app->page == NULL) {
        return false;
    }

    app->page_font = app->state.font;
    app->page_max_lines = max_lines;
    app->page_valid = true;
    return true;
}

/* ----------------------------------------------------------------- paint -- */

static void scene_reader_draw_message(Canvas* canvas, const char* text) {
    canvas_set_custom_u8g2_font(canvas, reader_font_get(SCENE_READER_MESSAGE_FONT));
    canvas_draw_str_aligned(
        canvas,
        (int32_t)(canvas_width(canvas) / 2),
        (int32_t)(canvas_height(canvas) / 2),
        AlignCenter,
        AlignCenter,
        text);
}

void scene_reader_draw_trimmed(
    Canvas* canvas,
    const char* text,
    int32_t x,
    size_t available,
    int32_t baseline) {
    char buffer[SCENE_READER_TITLE_BYTES];
    size_t used = 0;
    size_t offset = 0;
    size_t text_size = strlen(text);

    if(available == 0) {
        return;
    }

    buffer[0] = '\0';
    while(offset < text_size && used + 4 < sizeof(buffer)) {
        uint32_t code_point = 0;
        size_t unit = utf8_decode(text + offset, text_size - offset, &code_point);
        if(unit == 0) {
            break;
        }

        memcpy(buffer + used, text + offset, unit);
        buffer[used + unit] = '\0';
        if(canvas_string_width(canvas, buffer) > available) {
            break;
        }

        used += unit;
        offset += unit;
    }
    buffer[used] = '\0';

    if(used > 0) {
        canvas_draw_str(canvas, x, baseline, buffer);
    }
}

static void
    scene_reader_draw_status(Canvas* canvas, FzReaderApp* app, size_t chapters, int32_t baseline) {
    canvas_set_custom_u8g2_font(canvas, reader_font_get(SCENE_READER_STATUS_FONT));

    char counter[24];
    snprintf(
        counter, sizeof(counter), "%u/%u", (unsigned)(app->pos.chapter + 1), (unsigned)chapters);
    canvas_draw_str(canvas, SCENE_READER_MARGIN, baseline, counter);

    const char* font_name = reader_font_name(app->state.font);
    size_t font_name_width = canvas_string_width(canvas, font_name);
    canvas_draw_str(
        canvas,
        (int32_t)(canvas_width(canvas) - SCENE_READER_MARGIN - font_name_width),
        baseline,
        font_name);

    const char* title = app->book->format->title(app->book);
    if(title != NULL && title[0] != '\0') {
        int32_t left = SCENE_READER_MARGIN + (int32_t)canvas_string_width(canvas, counter) + 3;
        int32_t right =
            (int32_t)(canvas_width(canvas) - SCENE_READER_MARGIN) - (int32_t)font_name_width - 3;
        if(right > left) {
            scene_reader_draw_trimmed(canvas, title, left, (size_t)(right - left), baseline);
        }
    }
}

static void scene_reader_draw_text(Canvas* canvas, FzReaderApp* app, size_t line_height) {
    canvas_set_custom_u8g2_font(canvas, reader_font_get(app->state.font));

    for(size_t index = 0; index < app->page->line_count; index++) {
        ReaderLine* line = &app->page->lines[index];
        const size_t used = line->text_size;

        if(used == 0) {
            continue;
        }

        /*
         * Bold is synthetic: the run is drawn twice, the second time one pixel
         * to the right. The advance comes from the normal pass, so the layout
         * does not shift and the reserved pixel covers the extra width.
         *
         * The runs are counted in code points, the text in bytes: walk the
         * text once and note where the style changes.
         */
        size_t segment_start[READER_LINE_RUNS];
        bool segment_bold[READER_LINE_RUNS];
        size_t segment_count = 0;

        size_t run_index = 0;
        size_t run_used = 0;
        size_t position = 0;

        while(position < used) {
            while(run_index < line->run_count && run_used >= line->runs[run_index].length) {
                run_index++;
                run_used = 0;
            }
            bool bold = run_index < line->run_count && line->runs[run_index].bold;

            uint32_t code_point = 0;
            size_t unit = utf8_decode(line->text + position, used - position, &code_point);
            if(unit == 0) {
                /* Never written by the layout; count the byte and move on. */
                unit = 1;
            }

            if(segment_count == 0 || segment_bold[segment_count - 1] != bold) {
                if(segment_count < READER_LINE_RUNS) {
                    segment_start[segment_count] = position;
                    segment_bold[segment_count] = bold;
                    segment_count++;
                }
            }

            position += unit;
            run_used++;
        }

        const int32_t baseline = (int32_t)((index + 1) * line_height);
        int32_t x = SCENE_READER_MARGIN;

        for(size_t segment = 0; segment < segment_count; segment++) {
            size_t begin = segment_start[segment];
            if(begin >= used) {
                break;
            }
            size_t end = segment + 1 < segment_count ? segment_start[segment + 1] : used;
            if(end > used) {
                end = used;
            }

            /*
             * The text is already terminated at text_size, cut it at the end of
             * the segment for the duration of the call instead of copying the
             * line to the stack.
             */
            char saved = line->text[end];
            line->text[end] = '\0';
            size_t width = canvas_string_width(canvas, line->text + begin);
            canvas_draw_str(canvas, x, baseline, line->text + begin);
            if(segment_bold[segment]) {
                canvas_draw_str(canvas, x + 1, baseline, line->text + begin);
            }
            line->text[end] = saved;

            x += (int32_t)width;
        }
    }
}

void scene_reader_draw(Canvas* canvas, FzReaderApp* app) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(app->book == NULL || app->book->format == NULL) {
        return;
    }

    const size_t height = canvas_height(canvas);
    const int32_t status_baseline = (int32_t)(height > 1 ? height - 2 : 0);

    ReaderLayoutMetrics metrics;
    size_t max_lines = 0;
    scene_reader_metrics(canvas, app->state.font, &metrics, &max_lines);
    const size_t line_height = metrics.line_height;

    const size_t chapters = app->book->format->chapter_count(app->book);

    bool end_of_book = false;
    bool layout_failed = false;

    if(chapters == 0) {
        end_of_book = true;
    } else {
        for(size_t guard = 0; guard <= chapters; guard++) {
            if(!scene_reader_page_ensure(app, &metrics, max_lines)) {
                /* A spine item may be unreadable (for example an image only page). */
                if(app->pos.chapter + 1 < chapters) {
                    app->pos.chapter++;
                    app->pos.offset = 0;
                    continue;
                }
                layout_failed = true;
                break;
            }

            if(app->page->line_count > 0) {
                app->resume_check = false;
                break;
            }

            if(app->resume_check && app->pos.offset > 0) {
                /* The saved offset is past the end of the chapter: read its start. */
                app->resume_check = false;
                app->pos.offset = 0;
                continue;
            }

            app->resume_check = false;

            if(app->pos.chapter + 1 < chapters) {
                /* An empty page is an empty chapter: skip it. */
                app->pos.chapter++;
                app->pos.offset = 0;
                continue;
            }

            end_of_book = true;
            break;
        }
    }

    if(layout_failed) {
        scene_reader_draw_message(canvas, "Book error");
    } else if(end_of_book) {
        scene_reader_draw_message(canvas, "The End");
    } else if(app->page != NULL) {
        scene_reader_draw_text(canvas, app, line_height);
    }

    if(chapters > 0) {
        scene_reader_draw_status(canvas, app, chapters, status_baseline);
    }
}

/* ------------------------------------------------------------- navigation -- */

static void scene_reader_history_push(FzReaderApp* app, ReaderPosition position) {
    if(app->history_count == FZREADER_HISTORY_SIZE) {
        memmove(
            &app->history[0],
            &app->history[1],
            (FZREADER_HISTORY_SIZE - 1) * sizeof(ReaderPosition));
        app->history_count--;
    }

    app->history[app->history_count++] = position;
}

static void scene_reader_next_page(FzReaderApp* app) {
    if(app->page == NULL || !app->page_valid) {
        return;
    }

    ReaderPosition current = app->page->start;
    ReaderPosition next = app->page->next;
    if(next.chapter == current.chapter && next.offset == current.offset) {
        /* The page is the last one of the chapter. */
        const size_t chapters = app->book->format->chapter_count(app->book);
        if(app->pos.chapter + 1 >= chapters) {
            return;
        }
        scene_reader_history_push(app, current);
        app->pos.chapter++;
        app->pos.offset = 0;
        return;
    }

    scene_reader_history_push(app, current);
    app->pos = next;
}

static void scene_reader_prev_page(FzReaderApp* app) {
    if(app->history_count > 0) {
        app->pos = app->history[--app->history_count];
        return;
    }

    if(app->pos.chapter > 0) {
        /* Without a history entry the previous chapter is entered at its start. */
        app->pos.chapter--;
        app->pos.offset = 0;
        return;
    }

    app->pos.offset = 0;
}

void scene_reader_move_to(FzReaderApp* app, ReaderPosition position) {
    if(position.chapter == app->pos.chapter && position.offset == app->pos.offset) {
        return;
    }

    /* The page left behind stays reachable with the back navigation. */
    scene_reader_history_push(app, app->pos);
    app->pos = position;
    app->page_valid = false;
    reader_layout_reset();
}

static void scene_reader_font_step(FzReaderApp* app, int delta) {
    if(delta > 0) {
        if(app->state.font + 1 < READER_FONT_COUNT) {
            app->state.font++;
            app->page_valid = false;
        }
    } else if(delta < 0) {
        if(app->state.font > 0) {
            app->state.font--;
            app->page_valid = false;
        }
    }

    /* Page numbers depend on the font: count them again when they are needed. */
    if(!app->page_valid) {
        reader_page_counter_free(app->page_counts);
        app->page_counts = NULL;
    }
}

bool scene_reader_input(FzReaderApp* app, const InputEvent* event) {
    bool redraw = false;

    switch(event->key) {
    case InputKeyBack:
        /*
         * A short press picks another book without leaving the app, a hold
         * leaves it. The Press itself is not served: which of the two it is
         * only becomes clear on the release or when the hold fires.
         */
        if(event->type == InputTypeShort) {
            fzreader_app_pick_book(app);
            redraw = true;
        } else if(event->type == InputTypeLong) {
            app->running = false;
        }
        break;
    case InputKeyOk:
        if(event->type == InputTypePress) {
            scene_info_open(app);
            redraw = true;
        }
        break;
    case InputKeyLeft:
        if(event->type == InputTypePress) {
            scene_reader_prev_page(app);
            redraw = true;
        }
        break;
    case InputKeyRight:
        if(event->type == InputTypePress) {
            scene_reader_next_page(app);
            redraw = true;
        }
        break;
    case InputKeyUp:
        if(event->type == InputTypePress || event->type == InputTypeRelease) {
            /* A new hold of this key: the next repeat may change the font again. */
            if(app->font_hold_key == InputKeyUp) {
                app->font_hold_key = InputKeyMAX;
            }
        } else if(event->type == InputTypeRepeat && app->font_hold_key != InputKeyUp) {
            /*
             * One step per hold. Laying the chapter out again is slow, so the
             * repeats arriving while it runs must not step the font further.
             * The latch remembers which key stepped, so a hold of Down does not
             * feed on the latch of Up.
             */
            app->font_hold_key = InputKeyUp;
            scene_reader_font_step(app, 1);
            redraw = true;
        }
        break;
    case InputKeyDown:
        if(event->type == InputTypePress || event->type == InputTypeRelease) {
            if(app->font_hold_key == InputKeyDown) {
                app->font_hold_key = InputKeyMAX;
            }
        } else if(event->type == InputTypeRepeat && app->font_hold_key != InputKeyDown) {
            app->font_hold_key = InputKeyDown;
            scene_reader_font_step(app, -1);
            redraw = true;
        }
        break;
    default:
        break;
    }

    return redraw;
}
