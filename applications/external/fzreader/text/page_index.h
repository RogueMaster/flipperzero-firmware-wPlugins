#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../formats/reader_book.h"
#include "layout.h"

/**
 * Most page starts kept for one chapter.
 *
 * The one 4-byte offset per page is what the limit is about: 4096 pages is
 * 16 KiB of RAM on the device. A chapter with more pages is cut at the limit
 * and reader_page_index_truncated reports it, so the screen can say that the
 * chapter continues past the last page it knows. The reader itself is not
 * affected.
 */
#define READER_PAGE_INDEX_MAX 4096

/**
 * Page starts of one chapter, laid out with one set of metrics.
 *
 * The index answers both directions in O(log n): a page number gives the
 * position of its first line, a position gives the page that contains it. It
 * is built page by page through reader_layout_page, so the starts are exactly
 * the anchors the reader itself visits.
 *
 * Building is incremental: reader_page_index_extend lays out one page, so a
 * caller can spread the work over several frames. reader_page_index_build does
 * the same in one call.
 */
typedef struct ReaderPageIndex ReaderPageIndex;

/**
 * Start an index of one chapter; the first page begins at offset 0.
 *
 * @returns NULL if the chapter is out of range or memory fails.
 */
ReaderPageIndex* reader_page_index_alloc(ReaderBook* book, size_t chapter);

/**
 * Lay out one more page and remember where it starts.
 *
 * max_lines must be > 0.
 *
 * @returns false when the index is complete: the chapter ended, cannot be read
 *          or the page limit was reached. The index keeps the pages found so
 *          far, at least one.
 */
bool reader_page_index_extend(
    ReaderPageIndex* index,
    const ReaderLayoutMetrics* metrics,
    size_t max_lines);

/** Whether every page of the chapter has been found; false for a NULL index. */
bool reader_page_index_complete(const ReaderPageIndex* index);

/**
 * Whether the page limit cut the chapter short.
 *
 * When this is true the last page of the index is not the end of the chapter:
 * the screen must say so and refuse to treat the last page as the chapter end.
 */
bool reader_page_index_truncated(const ReaderPageIndex* index);

/** Build page starts for one chapter with the given metrics. @returns NULL on failure. */
ReaderPageIndex* reader_page_index_build(
    ReaderBook* book,
    size_t chapter,
    const ReaderLayoutMetrics* metrics,
    size_t max_lines);

void reader_page_index_free(ReaderPageIndex* index);

/** Number of pages in the chapter, at least 1; 0 for a NULL index. */
size_t reader_page_index_count(const ReaderPageIndex* index);

/** Position of the first line of a page. @returns false if the page is out of range. */
bool reader_page_index_position(const ReaderPageIndex* index, size_t page, ReaderPosition* out);

/** Page that contains the position, 0 if it is before the first page. */
size_t reader_page_index_lookup(const ReaderPageIndex* index, ReaderPosition position);

/**
 * Pages per chapter for the metrics of one book.
 *
 * Only the page counts are kept, not the starts: a whole book is counted at the
 * cost of one pass over its text and two bytes per chapter, so the information
 * screen can say "page X of Y" for the book. Counting is incremental as well:
 * reader_page_counter_extend counts one page.
 */
typedef struct ReaderPageCounter ReaderPageCounter;

/** Start counting every chapter of the book. @returns NULL on failure. */
ReaderPageCounter* reader_page_counter_alloc(ReaderBook* book);

/**
 * Count one more page of the book.
 *
 * max_lines must be > 0: a zero line budget counts the book as uncounted
 * instead of pretending that every chapter is one blank page.
 *
 * @returns false when every chapter is counted.
 */
bool reader_page_counter_extend(
    ReaderPageCounter* counter,
    const ReaderLayoutMetrics* metrics,
    size_t max_lines);

/** Whether every chapter has a page count. */
bool reader_page_counter_complete(const ReaderPageCounter* counter);

/** Chapters the book has. */
size_t reader_page_counter_chapters(const ReaderPageCounter* counter);

/** Chapter being counted, equal to the chapter count when the counter is complete. */
size_t reader_page_counter_progress(const ReaderPageCounter* counter);

/** Pages in all counted chapters; equal to the book total once complete. */
uint32_t reader_page_counter_total(const ReaderPageCounter* counter);

/**
 * Pages in the chapters before `chapter`.
 *
 * @returns false while at least one of them is not counted yet.
 */
bool reader_page_counter_pages_before(
    const ReaderPageCounter* counter,
    size_t chapter,
    uint32_t* pages);

void reader_page_counter_free(ReaderPageCounter* counter);
