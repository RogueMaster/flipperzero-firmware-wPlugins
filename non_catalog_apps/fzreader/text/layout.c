/**
 * @file layout.c
 * @brief Word wrapping of a chapter into pages.
 *
 * The engine never learns the book format: it only pulls ReaderSpans from
 * book->format, counts UTF-8 code points and asks ReaderLayoutMetrics how wide
 * the current line is. Every line keeps the trimmed text it is drawn with, so
 * drawing a page never reads the book again. The chapter stream stays open
 * between pages: a page continues where the previous one ended instead of
 * walking the chapter from its start.
 *
 * A line range always covers every code point between its start and the start
 * of the next line, including the whitespace the line was broken at. That keeps
 * page->next exact and lets a caller concatenate all pages back into the whole
 * chapter. Trailing whitespace is only trimmed when the line is measured and
 * drawn, never when it is counted.
 *
 * Style changes inside a line are kept as runs of code points. The runs always
 * cover the whole line, so the sum of their lengths is the length of the line.
 *
 * Bytes that are not valid UTF-8 stand for exactly one U+FFFD so offsets never
 * depend on whether the source was well formed.
 */

#include "layout.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "utf8.h"

/**
 * Bytes kept for the line being laid out, dropped before the next line.
 *
 * The measured line is copied into ReaderLine::text, so this is the same
 * budget as READER_LINE_TEXT in layout.h.
 */
#define LAYOUT_LINE_BYTES READER_LINE_TEXT

/** Stands in for a byte that is not valid UTF-8. */
#define LAYOUT_REPLACEMENT 0xFFFDu

/** Widest UTF-8 encoding of one code point. */
#define LAYOUT_UNIT_BYTES 4u

/**
 * Width reserved for the synthetic bold: a bold run is drawn twice, the second
 * pass one pixel to the right, so a line may use one pixel less.
 */
#define LAYOUT_BOLD_EXTRA 1u

/** One code point of the chapter stream. */
typedef struct {
    uint32_t code_point;
    char bytes[LAYOUT_UNIT_BYTES];
    size_t size;
    bool bold;
} LayoutUnit;

/** Sequential reader over the spans of one chapter. */
typedef struct {
    ReaderBook* book;
    const ReaderFormat* format;
    ReaderSpan span;
    size_t span_pos;
    size_t offset; /**< code point offset of the next unit in the chapter */
    bool bold; /**< bold flag of the span being read */
    bool eof;
} LayoutStream;

/** Stream with a single unit of push back, for breaks inside a word. */
typedef struct {
    LayoutStream stream;
    LayoutUnit pushed;
    bool has_pushed;
} LayoutCursor;

/** Scalar state of a line, saved so that one unit can be unread. */
typedef struct {
    size_t used;
    size_t length;
    size_t trimmed;
    size_t text;
} LayoutLineState;

/** A line under construction. */
typedef struct {
    char bytes[LAYOUT_LINE_BYTES]; /**< measurement buffer, not the text storage */
    size_t used; /**< bytes in bytes[] */
    size_t length; /**< code points in the line, trailing whitespace included */
    size_t trimmed; /**< bytes without trailing whitespace */
    size_t text; /**< code points without trailing whitespace */
    size_t break_length; /**< code points up to the last break opportunity */
    size_t break_used; /**< bytes up to the last break opportunity */
    bool tail_text; /**< whether the text after the break is not empty */
    size_t run_count; /**< runs in use, offsets are relative to the line start */
    ReaderLineRun runs[READER_LINE_RUNS];
} LayoutLine;

/** Result of laying out one line. */
typedef struct {
    size_t length;
    size_t run_count; /**< offsets are relative to the line start */
    ReaderLineRun runs[READER_LINE_RUNS];
    bool empty;
    size_t text_size; /**< bytes in text, trailing whitespace trimmed */
    char text[LAYOUT_LINE_BYTES]; /**< the text of the line as it is drawn */
} LayoutBuilt;

/*
 * The layout engine is strictly single threaded: reader_layout_page runs in the
 * GUI draw callback only. Keeping the working buffers here instead of on the
 * stack matters because the GUI thread has a 2 KiB stack and the book format
 * already uses much of it while the chapter is inflated.
 */
static LayoutCursor layout_cursor_storage;
static LayoutLine layout_line_storage;
static LayoutBuilt layout_built_storage;

/*
 * State of the chapter stream between calls: an open chapter at a known code
 * point offset. The next page usually starts where the previous one ended, so
 * the chapter is not closed and reopened with a walk from its start.
 */
static bool layout_cursor_open;
static size_t layout_cursor_chapter;
static size_t layout_cursor_offset; /**< code point offset of the next unit */
static size_t layout_cursor_width; /**< line width the buffer was filled with */
static size_t layout_cursor_height; /**< line height the buffer was filled with */

static bool layout_is_space(uint32_t code_point) {
    switch(code_point) {
    case ' ':
    case '\t':
    case '\r':
    case '\n':
    case '\v':
    case '\f':
    case 0x00A0u: /* no-break space still breaks a line, it is kept as it is */
        return true;
    default:
        return false;
    }
}

/**
 * Whitespace trimmed from the end of a drawn line: it is invisible, and a
 * trailing run of it would make the drawn text wider than the line.
 */
static bool layout_is_trailing_space(char symbol) {
    return symbol == ' ' || symbol == '\t' || symbol == '\r' || symbol == '\n' || symbol == '\v' ||
           symbol == '\f';
}

/* ---------------------------------------------------------------- stream -- */

static bool stream_next(LayoutStream* stream, LayoutUnit* unit) {
    if(stream->eof) {
        return false;
    }

    while(stream->span_pos >= stream->span.size) {
        if(!stream->format->read_span(stream->book, &stream->span)) {
            stream->eof = true;
            return false;
        }
        stream->span_pos = 0;
        stream->bold = stream->span.bold;
    }

    const char* text = stream->span.text + stream->span_pos;
    size_t size = stream->span.size - stream->span_pos;

    uint32_t code_point = 0;
    size_t used = utf8_decode(text, size, &code_point);

    if(used == 0) {
        /* Malformed input: one byte counts as one replacement code point. */
        code_point = LAYOUT_REPLACEMENT;
        used = 1;
        unit->size = utf8_encode(code_point, unit->bytes);
    } else {
        memcpy(unit->bytes, text, used);
        unit->size = used;
    }

    unit->code_point = code_point;
    unit->bold = stream->bold;

    stream->span_pos += used;
    stream->offset++;
    return true;
}

static bool cursor_open(LayoutCursor* cursor, ReaderBook* book, size_t chapter, size_t offset) {
    const ReaderFormat* format = book->format;
    if(format == NULL || format->chapter_open == NULL || format->read_span == NULL ||
       format->chapter_close == NULL) {
        return false;
    }
    if(!format->chapter_open(book, chapter)) {
        return false;
    }

    memset(cursor, 0, sizeof(LayoutCursor));
    cursor->stream.book = book;
    cursor->stream.format = format;
    cursor->stream.span.size = 0;

    LayoutUnit unit;
    while(cursor->stream.offset < offset) {
        if(!stream_next(&cursor->stream, &unit)) {
            break;
        }
    }
    return true;
}

static void cursor_close(LayoutCursor* cursor) {
    cursor->stream.format->chapter_close(cursor->stream.book);
    cursor->stream.eof = true;
}

static bool cursor_next(LayoutCursor* cursor, LayoutUnit* unit) {
    if(cursor->has_pushed) {
        cursor->has_pushed = false;
        *unit = cursor->pushed;
        return true;
    }
    return stream_next(&cursor->stream, unit);
}

static void cursor_unread(LayoutCursor* cursor, const LayoutUnit* unit) {
    cursor->pushed = *unit;
    cursor->has_pushed = true;
}

/* ------------------------------------------------------------------ runs -- */

/**
 * Append count code points with the same style to the runs of a line.
 *
 * When the line already uses every run slot the rest is folded into the last
 * run and drawn as plain text, so the offsets are never lost.
 */
static void line_runs_add(LayoutLine* line, bool bold, size_t count) {
    if(count == 0) {
        return;
    }

    if(line->run_count == 0) {
        ReaderLineRun* run = &line->runs[line->run_count++];
        run->offset = 0;
        run->length = count;
        run->bold = bold;
        return;
    }

    ReaderLineRun* last = &line->runs[line->run_count - 1];
    if(last->bold == bold) {
        last->length += count;
        return;
    }
    if(line->run_count < READER_LINE_RUNS) {
        ReaderLineRun* run = &line->runs[line->run_count++];
        run->offset = last->offset + last->length;
        run->length = count;
        run->bold = bold;
        return;
    }

    last->bold = false;
    last->length += count;
}

/** Cut the runs down to length code points, undoing the latest pushes. */
static void line_runs_trim(LayoutLine* line, size_t length) {
    size_t total = 0;
    for(size_t index = 0; index < line->run_count; index++) {
        if(total + line->runs[index].length <= length) {
            total += line->runs[index].length;
            continue;
        }
        size_t keep = length - total;
        line->run_count = keep > 0 ? index + 1 : index;
        if(keep > 0) {
            line->runs[index].length = keep;
        }
        return;
    }
}

/** Drop the first length code points; the remaining runs start at offset 0. */
static void line_runs_drop_prefix(LayoutLine* line, size_t length) {
    size_t index = 0;
    while(index < line->run_count && length >= line->runs[index].length) {
        length -= line->runs[index].length;
        index++;
    }

    size_t used = 0;
    size_t offset = 0;
    for(size_t source = index; source < line->run_count; source++) {
        line->runs[used] = line->runs[source];
        if(used == 0 && length > 0) {
            line->runs[used].length -= length;
            length = 0;
        }
        line->runs[used].offset = offset;
        offset += line->runs[used].length;
        used++;
    }
    line->run_count = used;
}

/** Copy the first length code points of a line, offsets start at 0. */
static void line_runs_prefix(const LayoutLine* line, size_t length, LayoutBuilt* built) {
    size_t taken = 0;
    size_t offset = 0;
    built->run_count = 0;

    for(size_t index = 0; index < line->run_count && taken < length; index++) {
        size_t room = length - taken;
        size_t count = line->runs[index].length < room ? line->runs[index].length : room;
        bool bold = line->runs[index].bold;

        if(built->run_count > 0 && built->runs[built->run_count - 1].bold == bold) {
            built->runs[built->run_count - 1].length += count;
        } else {
            ReaderLineRun* run = &built->runs[built->run_count++];
            run->offset = offset;
            run->length = count;
            run->bold = bold;
        }

        taken += count;
        offset += count;
    }
}

/* ------------------------------------------------------------------ line -- */

static void line_reset(LayoutLine* line) {
    line->used = 0;
    line->length = 0;
    line->trimmed = 0;
    line->text = 0;
    line->break_length = 0;
    line->break_used = 0;
    line->tail_text = false;
    line->run_count = 0;
}

static void line_save(const LayoutLine* line, LayoutLineState* state) {
    state->used = line->used;
    state->length = line->length;
    state->trimmed = line->trimmed;
    state->text = line->text;
}

static void line_restore(LayoutLine* line, const LayoutLineState* state) {
    line->used = state->used;
    line->length = state->length;
    line->trimmed = state->trimmed;
    line->text = state->text;
    line_runs_trim(line, state->length);
}

/** Append one unit; the caller has already made room for it. */
static void line_push(LayoutLine* line, const LayoutUnit* unit, bool space) {
    memcpy(line->bytes + line->used, unit->bytes, unit->size);
    line->used += unit->size;
    line->length++;
    line_runs_add(line, unit->bold, 1);

    if(!space) {
        line->trimmed = line->used;
        line->text++;
        line->tail_text = true;
    }
}

/**
 * Copy the text of the built line: the first limit bytes of the line buffer
 * without the trailing whitespace that is never drawn.
 */
static void built_copy_text(const LayoutLine* line, size_t limit, LayoutBuilt* built) {
    if(limit > line->used) {
        limit = line->used;
    }
    while(limit > 0 && layout_is_trailing_space(line->bytes[limit - 1])) {
        limit--;
    }
    if(limit >= sizeof(built->text)) {
        /* The line buffer never holds more than READER_LINE_TEXT - 1 bytes. */
        limit = sizeof(built->text) - 1;
    }
    memcpy(built->text, line->bytes, limit);
    built->text[limit] = '\0';
    built->text_size = limit;
}

/** Emit the whole line being built and start the next one empty. */
static void built_take_line(LayoutLine* line, LayoutBuilt* built, bool empty) {
    built->length = line->length;
    built->empty = empty;
    line_runs_prefix(line, line->length, built);
    /*
     * The whole buffer belongs to the line: no-break space is a break
     * opportunity but stays in the drawn text, like it did before the line
     * carried its own text.
     */
    built_copy_text(line, line->used, built);
    line_reset(line);
}

/**
 * Emit the text before the last break opportunity and keep the rest of the
 * buffer as the beginning of the next line.
 */
static void line_break(LayoutLine* line, LayoutBuilt* built) {
    built->length = line->break_length;
    built->empty = false;
    line_runs_prefix(line, line->break_length, built);
    /* The line is the prefix up to the break, not the word being built after it. */
    built_copy_text(line, line->break_used, built);

    size_t tail = line->used - line->break_used;
    memmove(line->bytes, line->bytes + line->break_used, tail);
    line_runs_drop_prefix(line, line->break_length);

    line->used = tail;
    line->length -= line->break_length;
    line->text = line->tail_text ? line->length : 0;
    line->trimmed = line->used;
    line->break_length = 0;
    line->break_used = 0;
    line->tail_text = false;
}

/**
 * Lay out the next line.
 *
 * @returns false when the chapter ends and no line is left over.
 */
static bool layout_line(
    LayoutCursor* cursor,
    LayoutLine* line,
    const ReaderLayoutMetrics* metrics,
    LayoutBuilt* built) {
    line->break_length = 0;
    line->break_used = 0;
    line->tail_text = false;

    for(;;) {
        /*
         * Keep room for the widest unit so that no unit is ever lost, and stay
         * within the byte budget of the callers that measure and draw the line.
         */
        if(line->used >= sizeof(line->bytes) - LAYOUT_UNIT_BYTES) {
            built_take_line(line, built, line->text == 0);
            return true;
        }

        LayoutUnit unit;
        if(!cursor_next(cursor, &unit)) {
            if(line->length == 0) {
                return false;
            }
            built_take_line(line, built, line->text == 0);
            return true;
        }

        bool space = layout_is_space(unit.code_point);

        if(unit.code_point == '\n') {
            /* Hard break: the newline belongs to this line but is not drawn. */
            line_push(line, &unit, true);
            built_take_line(line, built, line->text == 0);
            return true;
        }

        LayoutLineState state;
        line_save(line, &state);
        line_push(line, &unit, space);

        if(space) {
            if(line->text > 0) {
                line->break_length = line->length;
                line->break_used = line->used;
                line->tail_text = false;
            }
            continue; /* whitespace never makes a trimmed line wider */
        }

        size_t width = metrics->measure(metrics->context, line->bytes, line->trimmed);
        if(width + LAYOUT_BOLD_EXTRA <= metrics->line_width) {
            continue;
        }

        if(line->break_length > 0) {
            line_break(line, built);
            return true;
        }

        if(state.text > 0) {
            /* A word too long for the line: break it before this unit. */
            line_restore(line, &state);
            cursor_unread(cursor, &unit);
            built_take_line(line, built, false);
            return true;
        }

        /* A single unit is wider than the line: keep it, or nothing would fit. */
        built_take_line(line, built, false);
        return true;
    }
}

/* ------------------------------------------------------------- interface -- */

void reader_page_free(ReaderPage* page) {
    free(page);
}

/** Extend the last line of a page, keeping its runs consistent. */
static void line_extend(ReaderLine* line, size_t count) {
    if(line->run_count == 0) {
        line->run_count = 1;
        line->runs[0].offset = line->start.offset;
        line->runs[0].length = 0;
        line->runs[0].bold = false;
    }
    line->length += count;
    line->runs[line->run_count - 1].length += count;
}

ReaderPage* reader_layout_page(
    ReaderBook* book,
    ReaderPosition from,
    const ReaderLayoutMetrics* metrics,
    size_t max_lines) {
    if(book == NULL || book->format == NULL || metrics == NULL || metrics->measure == NULL) {
        return NULL;
    }
    if(max_lines > (SIZE_MAX - sizeof(ReaderPage)) / sizeof(ReaderLine)) {
        return NULL;
    }

    ReaderPage* page = malloc(sizeof(ReaderPage) + max_lines * sizeof(ReaderLine));
    if(page == NULL) {
        return NULL;
    }
    page->start = from;
    page->next = from;
    page->line_count = 0;

    if(max_lines == 0) {
        return page;
    }

    /*
     * Continue the chapter stream of the previous page when it stands exactly
     * at the anchor and the metrics did not change. The line buffer belongs to
     * the stream: it may already hold the beginning of the line that starts at
     * `from`, read while the previous line was broken, so it must be kept.
     */
    LayoutCursor* cursor = &layout_cursor_storage;
    LayoutLine* line = &layout_line_storage;
    bool reuse = layout_cursor_open && cursor->stream.book == book &&
                 layout_cursor_chapter == from.chapter && layout_cursor_offset == from.offset &&
                 layout_cursor_width == metrics->line_width &&
                 layout_cursor_height == metrics->line_height;
    if(!reuse) {
        if(layout_cursor_open) {
            cursor_close(cursor);
            layout_cursor_open = false;
        }
        if(!cursor_open(cursor, book, from.chapter, from.offset)) {
            free(page);
            return NULL;
        }
        layout_cursor_open = true;
        layout_cursor_chapter = from.chapter;
        layout_cursor_offset = from.offset;
        layout_cursor_width = metrics->line_width;
        layout_cursor_height = metrics->line_height;
        /* A stream opened at the anchor has nothing buffered yet. */
        line_reset(line);
    }

    size_t offset = from.offset;
    bool last_empty = false;

    while(page->line_count < max_lines) {
        LayoutBuilt* built = &layout_built_storage;
        if(!layout_line(cursor, line, metrics, built)) {
            break;
        }

        if(built->empty && last_empty && built->text_size == 0) {
            /*
             * At most one empty line in a row: extend the empty line instead.
             * Only a line that trims to nothing may be merged, otherwise the
             * text kept by the line would no longer be its own trimmed text.
             */
            line_extend(&page->lines[page->line_count - 1], built->length);
            offset += built->length;
            continue;
        }

        ReaderLine* entry = &page->lines[page->line_count++];
        entry->start.chapter = from.chapter;
        entry->start.offset = offset;
        entry->length = built->length;
        entry->run_count = built->run_count;
        entry->text_size = built->text_size;
        memcpy(entry->text, built->text, built->text_size);
        entry->text[entry->text_size] = '\0';
        for(size_t index = 0; index < built->run_count; index++) {
            entry->runs[index] = built->runs[index];
            entry->runs[index].offset += offset;
        }

        offset += built->length;
        last_empty = built->empty;
    }

    /*
     * A full page can end on a blank line. Swallow the hard breaks that follow
     * it, so the next page does not open with a second blank line.
     */
    if(page->line_count == max_lines && last_empty && !cursor->has_pushed) {
        ReaderLine* blank = &page->lines[page->line_count - 1];
        LayoutUnit unit;
        while(cursor_next(cursor, &unit)) {
            if(unit.code_point != '\n' && unit.code_point != '\r') {
                cursor_unread(cursor, &unit);
                break;
            }
            line_extend(blank, 1);
            offset++;
        }
    }

    /*
     * The chapter stays open: the next page most often begins where this one
     * ended. The saved offset is the first code point the cursor has not handed
     * out yet, pushed back unit included, so it must be the same offset as
     * page->next.
     */
    layout_cursor_open = true;
    layout_cursor_chapter = from.chapter;
    layout_cursor_offset = offset;

    page->next.chapter = from.chapter;
    page->next.offset = offset;
    return page;
}

void reader_layout_reset(void) {
    if(!layout_cursor_open) {
        return;
    }
    cursor_close(&layout_cursor_storage);
    layout_cursor_open = false;
    layout_cursor_chapter = 0;
    layout_cursor_offset = 0;
    layout_cursor_width = 0;
    layout_cursor_height = 0;
}
