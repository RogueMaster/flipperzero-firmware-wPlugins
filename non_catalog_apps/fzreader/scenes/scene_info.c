/**
 * @file scene_info.c
 * @brief Book information screen with a page chooser.
 *
 * The screen shows the book and chapter titles, the page in the book, the
 * chapter and the controls. Left and Right pick another page, a hold of either
 * key accelerates the step up to 64 pages per repeat; Ok returns to the reader
 * at the chosen page, Back leaves the screen without moving.
 *
 * Changing the number must not lay a page out: the page starts of the chapter
 * are kept in a ReaderPageIndex, so a page is a bookmark and no text is laid
 * out when the number changes. The index is built on entering the screen, a
 * couple of milliseconds per frame, and dropped when the screen closes or the
 * chapter changes. A chapter the page counter already knows to be short is laid
 * out in one go, so its progress screen never flashes.
 *
 * The book total comes from a ReaderPageCounter, which walks every chapter
 * once and keeps only the page counts. It is started on the first visit of the
 * screen and counted in the background while the screen is drawn: the totals
 * show "..." until it is done, so the reader is never blocked and nothing is
 * invented. Both builds must run where a Canvas exists to measure the text, so
 * they run in the draw callback, chunked by time.
 *
 * The index and the counter never work in the same frame: the counter starts on
 * the frame after the index becomes ready. Each step checks the elapsed time
 * after every page, so a frame lays out as many pages as fit the budget: short
 * pages take several, long ones a single page.
 *
 * A chapter longer than READER_PAGE_INDEX_MAX pages is cut at the limit. The
 * screen then marks its last page with "+" and does not jump there, because
 * that page is not the end of the chapter and the reader is probably past it.
 */

#include "scene_info.h"

#include <furi.h>
#include <gui/canvas.h>

#include <stdio.h>
#include <string.h>

#include "../fonts/reader_fonts.h"
#include "scene_reader.h"

#define SCENE_INFO_MARGIN      1
/** Bytes of a formatted line. */
#define SCENE_INFO_LINE_BYTES  64
/**
 * Most milliseconds one frame may spend laying text out, so drawing stays
 * smooth. Strictly below FZREADER_PROGRESS_TICK: a frame leaves time for the
 * drawing itself and for input, Back included.
 */
#define SCENE_INFO_FRAME_MS    2
/** Most pages laid out at once for a chapter the counter knows to be short. */
#define SCENE_INFO_SHORT_PAGES 16
/** Fonts of the screen: hints and titles, values, page number. */
#define SCENE_INFO_HINT_FONT   0
#define SCENE_INFO_VALUE_FONT  1
#define SCENE_INFO_PAGE_FONT   2
/** Repeats of one hold counted before the step stops growing. */
#define SCENE_INFO_HOLD_MAX    6
/** Largest step of the accelerated scroll, in pages per repeat. */
#define SCENE_INFO_STEP_MAX    64
/** Gap between the page number and the Left/Right arrows around it. */
#define SCENE_INFO_PAGE_GAP    2
/** Arrows drawn around the page number. */
#define SCENE_INFO_ARROW_LEFT  "<"
#define SCENE_INFO_ARROW_RIGHT ">"

_Static_assert(READER_FONT_COUNT > SCENE_INFO_PAGE_FONT, "page font index out of range");

/* --------------------------------------------------------------- helpers -- */

void scene_info_page_line(Canvas* canvas, const char* number, SceneInfoPageLine* line) {
    const int32_t center = (int32_t)(canvas_width(canvas) / 2);
    const int32_t edge = (int32_t)(canvas_width(canvas) - 1);

    /* An AlignCenter string starts at x - width / 2: the number is drawn that way. */
    line->number_width = (int32_t)canvas_string_width(canvas, number);
    line->number_left = center - line->number_width / 2;
    line->left_width = (int32_t)canvas_string_width(canvas, SCENE_INFO_ARROW_LEFT);
    line->right_width = (int32_t)canvas_string_width(canvas, SCENE_INFO_ARROW_RIGHT);

    /*
     * Each arrow keeps the same gap to the number and is clamped into the
     * margins, so it neither covers a digit nor leaves the screen. Both are
     * placed by their left edge, which is how they are drawn.
     */
    const int32_t number_right = line->number_left + line->number_width;

    line->arrow_left = line->number_left - SCENE_INFO_PAGE_GAP - line->left_width;
    if(line->arrow_left < SCENE_INFO_MARGIN) {
        line->arrow_left = SCENE_INFO_MARGIN;
    } else if(line->arrow_left > edge - line->left_width) {
        line->arrow_left = edge - line->left_width;
    }

    line->arrow_right = number_right + SCENE_INFO_PAGE_GAP;
    if(line->arrow_right < SCENE_INFO_MARGIN) {
        line->arrow_right = SCENE_INFO_MARGIN;
    } else if(line->arrow_right > edge - line->right_width) {
        line->arrow_right = edge - line->right_width;
    }
}

static size_t scene_info_chapters(const FzReaderApp* app) {
    return app->book->format->chapter_count(app->book);
}

/**
 * Lay the chapter out for a few milliseconds.
 *
 * The clock is checked after every page, not before it: the first page is
 * always laid out, so the screen makes progress, and a page that runs past the
 * budget ends the frame instead of starting another one.
 *
 * @returns true once the whole chapter is indexed.
 */
static bool
    scene_info_index_step(FzReaderApp* app, const ReaderLayoutMetrics* metrics, size_t max_lines) {
    const uint32_t started = furi_get_tick();
    bool more = true;
    do {
        more = reader_page_index_extend(app->page_index, metrics, max_lines);
    } while(more && furi_get_tick() - started < SCENE_INFO_FRAME_MS);
    return !more;
}

/**
 * Count pages of the book for a few milliseconds.
 * @returns true once every chapter is counted.
 */
static bool
    scene_info_counts_step(FzReaderApp* app, const ReaderLayoutMetrics* metrics, size_t max_lines) {
    const uint32_t started = furi_get_tick();
    bool more = true;
    do {
        more = reader_page_counter_extend(app->page_counts, metrics, max_lines);
    } while(more && furi_get_tick() - started < SCENE_INFO_FRAME_MS);
    return !more;
}

/**
 * Start the index of `chapter` and select its first page.
 *
 * The new index is allocated before the old one is dropped: if memory fails the
 * selection and the old index stay as they were, so the screen can be closed
 * with a message instead of showing a chapter it has no pages for.
 *
 * @returns false if the chapter cannot be indexed, the state is unchanged then.
 */
static bool scene_info_enter_chapter(FzReaderApp* app, size_t chapter, bool at_pos, bool at_end) {
    if(chapter >= scene_info_chapters(app)) {
        return false;
    }

    ReaderPageIndex* index = reader_page_index_alloc(app->book, chapter);
    if(index == NULL) {
        return false;
    }

    reader_page_index_free(app->page_index);
    app->page_index = index;
    app->info_chapter = chapter;
    app->info_page = 0;
    app->info_page_at_pos = at_pos;
    app->info_page_at_end = at_end;
    /* The new chapter still has to be laid out: keep the frames coming. */
    app->info_pending = true;
    return true;
}

/** Put the screen into its error state: it shows the message until Back. */
static void scene_info_out_of_memory(FzReaderApp* app) {
    app->info_error = true;
    app->info_pending = false;
}

/**
 * Whether the counter already knows the selected chapter to be short.
 *
 * Only then is the whole chapter laid out in one frame: the page count comes
 * from the counter, because a chapter size is not known before it is laid out.
 */
static bool scene_info_chapter_short(const FzReaderApp* app) {
    if(app->page_counts == NULL || !reader_page_counter_complete(app->page_counts)) {
        return false;
    }

    uint32_t before = 0;
    uint32_t after = 0;
    if(!reader_page_counter_pages_before(app->page_counts, app->info_chapter, &before) ||
       !reader_page_counter_pages_before(app->page_counts, app->info_chapter + 1, &after)) {
        return false;
    }
    return after - before <= SCENE_INFO_SHORT_PAGES;
}

/** Place the selected page once the index of the chapter is complete. */
static void scene_info_index_ready(FzReaderApp* app) {
    /* Building moved the chapter stream: let the reader lay its own page out again. */
    reader_layout_reset();

    const size_t pages = reader_page_index_count(app->page_index);
    if(app->info_page_at_pos) {
        app->info_page = reader_page_index_lookup(app->page_index, app->pos);
    } else if(app->info_page_at_end) {
        app->info_page = pages - 1;
    }
    app->info_page_at_pos = false;
    app->info_page_at_end = false;
    if(app->info_page >= pages) {
        app->info_page = pages - 1;
    }

    /* The first index to become ready shows the page the screen was opened at. */
    if(app->info_initial_page == SIZE_MAX) {
        app->info_initial_page = app->info_page;
    }
}

/** Whether the selection differs from the one the screen was opened at. */
static bool scene_info_changed(const FzReaderApp* app) {
    return app->info_chapter != app->info_initial_pos.chapter ||
           app->info_page != app->info_initial_page;
}

/**
 * Whether the selected page is the cut end of a truncated index.
 *
 * The page limit, not the chapter, ends such an index: the page after the last
 * known one may exist, and the reader is probably past it, so it must not be a
 * jump target.
 */
static bool scene_info_cut_end(const FzReaderApp* app) {
    return reader_page_index_truncated(app->page_index) &&
           app->info_page + 1 >= reader_page_index_count(app->page_index);
}

/** Move the selection, switching the chapter when it leaves the current one. */
static void scene_info_move(FzReaderApp* app, int direction, size_t step) {
    const size_t pages = reader_page_index_count(app->page_index);
    if(pages == 0) {
        return;
    }
    if(app->info_page >= pages) {
        app->info_page = pages - 1;
    }

    if(direction > 0) {
        if(step <= pages - 1 - app->info_page) {
            app->info_page += step;
        } else if(app->info_chapter + 1 < scene_info_chapters(app)) {
            if(!scene_info_enter_chapter(app, app->info_chapter + 1, false, false)) {
                scene_info_out_of_memory(app);
            }
        } else {
            app->info_page = pages - 1;
        }
        return;
    }

    if(step <= app->info_page) {
        app->info_page -= step;
    } else if(app->info_chapter > 0) {
        if(!scene_info_enter_chapter(app, app->info_chapter - 1, false, true)) {
            scene_info_out_of_memory(app);
        }
    } else {
        app->info_page = 0;
    }
}

/** Step of one repeat of a hold: 1, 2, 4, ..., 64 pages. */
static size_t scene_info_hold_step(FzReaderApp* app) {
    size_t step = SCENE_INFO_STEP_MAX;
    if(app->info_hold < SCENE_INFO_HOLD_MAX) {
        step = (size_t)1 << app->info_hold;
        app->info_hold++;
    }
    return step;
}

static void scene_info_close(FzReaderApp* app, bool apply) {
    /*
     * Ok jumps only when the number was changed and the page is a real one.
     * Without a change the reader is already there: moving it to the start of
     * the "canonical" page would shift the anchor, because the reader lays the
     * chapter out from the anchor it has, not from the start of the chapter.
     */
    bool moved = false;
    if(apply && scene_info_changed(app) && !scene_info_cut_end(app)) {
        ReaderPosition position;
        if(reader_page_index_position(app->page_index, app->info_page, &position)) {
            scene_reader_move_to(app, position);
            moved = true;
        }
    }

    reader_page_index_free(app->page_index);
    app->page_index = NULL;
    app->screen = FzReaderScreenReader;
    app->info_pending = false;
    app->info_error = false;
    app->info_hold = 0;
    app->info_initial_page = SIZE_MAX;
    /*
     * Dropping the layout stream on Back would make the next page turn walk the
     * chapter from its start again, while the cached page is still valid and
     * the stream still stands where it is. Only a jump has to reset it, and
     * scene_reader_move_to already did.
     */
    if(moved) {
        reader_layout_reset();
    }
}

/* ----------------------------------------------------------------- paint -- */

static void scene_info_draw_message(Canvas* canvas, const char* text) {
    canvas_set_custom_u8g2_font(canvas, reader_font_get(SCENE_INFO_VALUE_FONT));
    canvas_draw_str_aligned(
        canvas,
        (int32_t)(canvas_width(canvas) / 2),
        (int32_t)(canvas_height(canvas) / 2),
        AlignCenter,
        AlignCenter,
        text);
}

static void scene_info_draw_progress(Canvas* canvas, FzReaderApp* app, const char* what) {
    char line[SCENE_INFO_LINE_BYTES];
    const int32_t center = (int32_t)(canvas_width(canvas) / 2);

    canvas_set_custom_u8g2_font(canvas, reader_font_get(SCENE_INFO_VALUE_FONT));
    canvas_draw_str_aligned(canvas, center, 22, AlignCenter, AlignCenter, what);

    canvas_set_custom_u8g2_font(canvas, reader_font_get(SCENE_INFO_HINT_FONT));
    snprintf(
        line,
        sizeof(line),
        "Chapter %u of %u",
        (unsigned)(app->info_chapter + 1),
        (unsigned)scene_info_chapters(app));
    canvas_draw_str_aligned(canvas, center, 34, AlignCenter, AlignCenter, line);

    snprintf(line, sizeof(line), "%u pages", (unsigned)reader_page_index_count(app->page_index));
    canvas_draw_str_aligned(canvas, center, 45, AlignCenter, AlignCenter, line);

    canvas_draw_str_aligned(canvas, center, 57, AlignCenter, AlignCenter, "Back cancel");
}

static void scene_info_draw_page(Canvas* canvas, FzReaderApp* app) {
    const ReaderFormat* format = app->book->format;
    const int32_t center = (int32_t)(canvas_width(canvas) / 2);
    const size_t available = canvas_width(canvas) > 2 * SCENE_INFO_MARGIN ?
                                 canvas_width(canvas) - 2 * SCENE_INFO_MARGIN :
                                 canvas_width(canvas);

    canvas_set_custom_u8g2_font(canvas, reader_font_get(SCENE_INFO_HINT_FONT));

    const char* title = format->title(app->book);
    if(title != NULL && title[0] != '\0') {
        scene_reader_draw_trimmed(canvas, title, SCENE_INFO_MARGIN, available, 7);
    }

    /* A format that does not name its chapters leaves the line empty. */
    const char* chapter_title =
        format->chapter_title != NULL ? format->chapter_title(app->book, app->info_chapter) : NULL;
    if(chapter_title != NULL && chapter_title[0] != '\0') {
        scene_reader_draw_trimmed(canvas, chapter_title, SCENE_INFO_MARGIN, available, 14);
    }

    const bool have_total = app->page_counts != NULL &&
                            reader_page_counter_complete(app->page_counts);
    const uint32_t total = have_total ? reader_page_counter_total(app->page_counts) : 0;
    uint32_t before = 0;
    const bool have_before =
        reader_page_counter_pages_before(app->page_counts, app->info_chapter, &before);
    const uint32_t page = before + (uint32_t)app->info_page + 1;

    /*
     * The cut end of a truncated index is "the page and more": the chapter goes
     * on past it, so a plain number would claim the chapter ends there.
     */
    char number[16];
    if(scene_info_cut_end(app)) {
        snprintf(number, sizeof(number), "%u+", (unsigned)page);
    } else {
        snprintf(number, sizeof(number), "%u", (unsigned)page);
    }

    char line[SCENE_INFO_LINE_BYTES];
    canvas_set_custom_u8g2_font(canvas, reader_font_get(SCENE_INFO_PAGE_FONT));
    if(have_before && have_total) {
        snprintf(line, sizeof(line), "%s of %u", number, (unsigned)total);
    } else if(have_before) {
        snprintf(line, sizeof(line), "%s of ...", number);
    } else {
        snprintf(line, sizeof(line), "... of ...");
    }

    /*
     * The arrows stand for the keys that pick the page: they keep the same gap
     * to the number on both sides and stay off the screen edge whatever its
     * width is. Both are drawn from their left edge, so the right one does not
     * shift back by its own width and cover the last digit.
     */
    SceneInfoPageLine page_line;
    scene_info_page_line(canvas, line, &page_line);

    canvas_draw_str_aligned(
        canvas, page_line.arrow_left, 23, AlignLeft, AlignCenter, SCENE_INFO_ARROW_LEFT);
    canvas_draw_str_aligned(
        canvas, page_line.arrow_right, 23, AlignLeft, AlignCenter, SCENE_INFO_ARROW_RIGHT);
    canvas_draw_str_aligned(canvas, center, 23, AlignCenter, AlignCenter, line);

    canvas_set_custom_u8g2_font(canvas, reader_font_get(SCENE_INFO_VALUE_FONT));
    snprintf(
        line,
        sizeof(line),
        "Chapter %u of %u",
        (unsigned)(app->info_chapter + 1),
        (unsigned)scene_info_chapters(app));
    canvas_draw_str_aligned(canvas, center, 34, AlignCenter, AlignCenter, line);

    canvas_set_custom_u8g2_font(canvas, reader_font_get(SCENE_INFO_HINT_FONT));
    canvas_draw_str_aligned(canvas, center, 43, AlignCenter, AlignCenter, "hold Back: exit");

    if(app->page_counts != NULL && !reader_page_counter_complete(app->page_counts)) {
        /* The book total is still being counted: say so instead of a number. */
        size_t progress = reader_page_counter_progress(app->page_counts);
        size_t chapters = reader_page_counter_chapters(app->page_counts);
        if(progress >= chapters) {
            progress = chapters > 0 ? chapters - 1 : 0;
        }
        snprintf(
            line,
            sizeof(line),
            "counting: chapter %u of %u",
            (unsigned)(progress + 1),
            (unsigned)chapters);
        canvas_draw_str_aligned(canvas, center, 51, AlignCenter, AlignCenter, line);
    } else {
        canvas_draw_str_aligned(canvas, center, 51, AlignCenter, AlignCenter, "hold: faster");
    }

    canvas_draw_str_aligned(canvas, center, 59, AlignCenter, AlignCenter, "OK go, Back cancel");
}

/* ------------------------------------------------------------- interface -- */

void scene_info_open(FzReaderApp* app) {
    if(app->book == NULL || app->book->format == NULL || app->screen != FzReaderScreenReader) {
        return;
    }

    if(app->page_counts == NULL) {
        app->page_counts = reader_page_counter_alloc(app->book);
    }

    /* Remember where the reader is: Ok must not move it when nothing changed. */
    app->info_initial_pos = app->pos;
    app->info_initial_page = SIZE_MAX;
    app->info_hold = 0;
    app->screen = FzReaderScreenInfo;
    app->info_error = false;

    if(!scene_info_enter_chapter(app, app->pos.chapter, true, false)) {
        /* No index: the screen shows the message and only Back leaves it. */
        scene_info_out_of_memory(app);
        return;
    }
    app->info_pending = true;
}

void scene_info_draw(Canvas* canvas, FzReaderApp* app) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    app->info_pending = false;

    if(app->book == NULL || app->book->format == NULL) {
        return;
    }
    if(app->info_error || app->page_index == NULL) {
        scene_info_draw_message(canvas, "Out of memory");
        return;
    }

    /* The chapter must be laid out exactly as the reader lays it out. */
    ReaderLayoutMetrics metrics;
    size_t max_lines = 0;
    scene_reader_metrics(canvas, app->state.font, &metrics, &max_lines);

    if(!reader_page_index_complete(app->page_index)) {
        bool done;
        if(scene_info_chapter_short(app)) {
            /*
             * The counter knows the chapter is short, so the whole index is
             * laid out at once: stepping through it would only flash a
             * progress screen for a couple of frames.
             */
            ReaderPageIndex* index =
                reader_page_index_build(app->book, app->info_chapter, &metrics, max_lines);
            if(index == NULL) {
                scene_info_out_of_memory(app);
                scene_info_draw_message(canvas, "Out of memory");
                return;
            }
            reader_page_index_free(app->page_index);
            app->page_index = index;
            done = true;
        } else {
            done = scene_info_index_step(app, &metrics, max_lines);
        }

        if(!done) {
            scene_info_draw_progress(canvas, app, "Laying out");
            app->info_pending = true;
            return;
        }
        scene_info_index_ready(app);

        /*
         * The counter never shares a frame with the index: it starts on the next
         * one, so one frame is bounded by a single kind of work.
         */
        app->info_pending = app->page_counts != NULL &&
                            !reader_page_counter_complete(app->page_counts);
        scene_info_draw_page(canvas, app);
        return;
    }

    if(app->page_counts != NULL && !reader_page_counter_complete(app->page_counts)) {
        if(scene_info_counts_step(app, &metrics, max_lines)) {
            /* Counting left a chapter of the book open. */
            reader_layout_reset();
        }
        app->info_pending = !reader_page_counter_complete(app->page_counts);
    }

    scene_info_draw_page(canvas, app);
}

bool scene_info_input(FzReaderApp* app, const InputEvent* event) {
    bool redraw = false;

    if(event->key == InputKeyBack) {
        /*
         * Both gestures wait for the release, so the Short of this screen is not
         * taken by the reader for its own. A hold leaves the app, like Back does
         * on the reading screen.
         */
        if(event->type == InputTypeShort) {
            scene_info_close(app, false);
            redraw = true;
        } else if(event->type == InputTypeLong) {
            scene_info_close(app, false);
            app->running = false;
        }
        return redraw;
    }

    const bool ready = app->page_index != NULL && !app->info_error &&
                       reader_page_index_complete(app->page_index);
    if(ready) {
        switch(event->key) {
        case InputKeyOk:
            if(event->type == InputTypePress) {
                scene_info_close(app, true);
                redraw = true;
            }
            break;
        case InputKeyLeft:
        case InputKeyRight: {
            const int direction = event->key == InputKeyLeft ? -1 : 1;
            if(event->type == InputTypePress) {
                app->info_hold = 0;
                scene_info_move(app, direction, 1);
                redraw = true;
            } else if(event->type == InputTypeRelease) {
                app->info_hold = 0;
            } else if(event->type == InputTypeRepeat) {
                scene_info_move(app, direction, scene_info_hold_step(app));
                redraw = true;
            }
            break;
        }
        default:
            break;
        }
    }

    return redraw;
}
