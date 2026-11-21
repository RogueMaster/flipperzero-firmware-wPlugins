#pragma once

#include <stddef.h>
#include <stdbool.h>

#include "../formats/reader_book.h"

/** Where reading is anchored: a chapter and a code point offset within it. */
typedef struct {
    size_t chapter;
    size_t offset;
} ReaderPosition;

/** Runs of uniform style kept for one laid out line. */
#define READER_LINE_RUNS 4

/**
 * Bytes of the text kept for one laid out line.
 *
 * Must match LAYOUT_LINE_BYTES in layout.c: the layout copies the line it
 * measured into ReaderLine::text, so a line that fits the layout buffer always
 * fits here.
 */
#define READER_LINE_TEXT 192

/** One run of code points of a line with the same style. */
typedef struct {
    size_t offset; /**< first code point of the run, in chapter coordinates */
    size_t length; /**< code points in the run */
    bool bold;
} ReaderLineRun;

/** One laid out line. */
typedef struct {
    ReaderPosition start; /**< first code point of the line */
    size_t length; /**< code points in the line */
    size_t run_count; /**< runs in use, the sum of their lengths is length */
    ReaderLineRun runs[READER_LINE_RUNS];
    size_t text_size; /**< bytes in text, trailing whitespace trimmed */
    char text[READER_LINE_TEXT]; /**< UTF-8 text of the line as it is drawn */
} ReaderLine;

/** A laid out page. */
typedef struct {
    ReaderPosition start; /**< where the page begins */
    ReaderPosition next; /**< where reading continues after this page */
    size_t line_count;
    ReaderLine lines[];
} ReaderPage;

/** Measurement hooks, so layout does not depend on the GUI. */
typedef struct {
    /** Context handed to measure, for example the canvas of the caller. */
    void* context;
    /** Width in pixels of a UTF-8 string. */
    size_t (*measure)(void* context, const char* text, size_t size);
    /** Height of one line in pixels, including leading. */
    size_t line_height;
    /** Usable line width in pixels. */
    size_t line_width;
} ReaderLayoutMetrics;

/**
 * Lay out one page of a chapter.
 *
 * Starts at exactly `from` and fills lines until `max_lines` or the end of the
 * chapter, whichever comes first. The returned page is malloc'ed; free with
 * reader_page_free.
 *
 * @returns NULL if the chapter cannot be opened.
 */
ReaderPage* reader_layout_page(
    ReaderBook* book,
    ReaderPosition from,
    const ReaderLayoutMetrics* metrics,
    size_t max_lines);

/**
 * Drop the chapter stream cached between calls to reader_layout_page.
 *
 * The layout keeps the chapter of the last page open and continues reading
 * where the page ended, so the next page does not have to walk the chapter
 * from its start again. The cache belongs to the book it was opened on: call
 * this before closing a book, and before any code of its own opens a chapter
 * (`chapter_open`), because the format serves one sequential reader at a time.
 */
void reader_layout_reset(void);

void reader_page_free(ReaderPage* page);
