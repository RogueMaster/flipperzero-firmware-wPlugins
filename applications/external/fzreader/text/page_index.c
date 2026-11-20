/**
 * @file page_index.c
 * @brief Page starts of a chapter and page counts of a book.
 *
 * Both builds use reader_layout_page page by page and never lay a chapter out
 * from its start again: the layout keeps the chapter stream open where the
 * previous page ended, so one pass over the text costs O(length), not
 * O(length squared).
 *
 * A page counts when the layout returns at least one line for it, so the page
 * that would follow the end of a chapter is never recorded. A chapter that
 * cannot be read (an image only spine item, for example) still counts as one
 * page, so page numbers and the index stay consistent.
 */

#include "page_index.h"

#include <stdlib.h>
#include <string.h>

struct ReaderPageIndex {
    ReaderBook* book;
    size_t chapter;
    ReaderPosition from; /**< anchor of the page to lay out next */
    size_t* starts; /**< first line of every page found so far */
    size_t count;
    size_t capacity;
    bool complete;
    bool truncated; /**< the page limit, not the end of the chapter, ended the build */
};

struct ReaderPageCounter {
    ReaderBook* book;
    size_t chapters;
    uint16_t* pages; /**< pages of every chapter, 0 while not counted */
    size_t chapter; /**< chapter being counted */
    ReaderPosition from; /**< anchor of the page to count next */
    bool complete;
};

/* ------------------------------------------------------------- page index -- */

static bool reader_page_index_push(ReaderPageIndex* index, size_t offset) {
    if(index->count == index->capacity) {
        size_t capacity = index->capacity == 0 ? 32 : index->capacity * 2;
        if(capacity > READER_PAGE_INDEX_MAX) {
            capacity = READER_PAGE_INDEX_MAX;
        }
        if(capacity <= index->capacity) {
            index->truncated = true;
            return false;
        }
        size_t* starts = realloc(index->starts, capacity * sizeof(size_t));
        if(starts == NULL) {
            index->truncated = true;
            return false;
        }
        index->starts = starts;
        index->capacity = capacity;
    }
    index->starts[index->count++] = offset;
    return true;
}

ReaderPageIndex* reader_page_index_alloc(ReaderBook* book, size_t chapter) {
    if(book == NULL || book->format == NULL || book->format->chapter_count == NULL) {
        return NULL;
    }
    if(chapter >= book->format->chapter_count(book)) {
        return NULL;
    }

    ReaderPageIndex* index = malloc(sizeof(ReaderPageIndex));
    if(index == NULL) {
        return NULL;
    }
    memset(index, 0, sizeof(ReaderPageIndex));
    index->book = book;
    index->chapter = chapter;
    index->from.chapter = chapter;
    index->from.offset = 0;

    /* The first page of the chapter always starts at the beginning. */
    if(!reader_page_index_push(index, 0)) {
        free(index);
        return NULL;
    }
    return index;
}

bool reader_page_index_extend(
    ReaderPageIndex* index,
    const ReaderLayoutMetrics* metrics,
    size_t max_lines) {
    if(index == NULL || index->complete) {
        return false;
    }
    if(max_lines == 0) {
        index->complete = true;
        return false;
    }

    const ReaderPosition from = index->from;
    ReaderPage* page = reader_layout_page(index->book, from, metrics, max_lines);
    bool exists = page != NULL && page->line_count > 0;
    bool advanced = exists && page->next.chapter == index->chapter &&
                    page->next.offset > from.offset;
    if(page != NULL) {
        index->from = page->next;
        reader_page_free(page);
    }

    /* The first page was recorded when the index was allocated. */
    if(exists && index->starts[index->count - 1] < from.offset) {
        if(!reader_page_index_push(index, from.offset)) {
            advanced = false;
        }
    }
    if(!advanced) {
        index->complete = true;
    }
    return !index->complete;
}

bool reader_page_index_complete(const ReaderPageIndex* index) {
    /* A missing index is not a ready one: the screen has nothing to show. */
    return index != NULL && index->complete;
}

bool reader_page_index_truncated(const ReaderPageIndex* index) {
    return index != NULL && index->truncated;
}

ReaderPageIndex* reader_page_index_build(
    ReaderBook* book,
    size_t chapter,
    const ReaderLayoutMetrics* metrics,
    size_t max_lines) {
    ReaderPageIndex* index = reader_page_index_alloc(book, chapter);
    if(index == NULL) {
        return NULL;
    }

    while(reader_page_index_extend(index, metrics, max_lines)) {
    }

    /* The stream stands at the end of the chapter: let the reader open its own page. */
    reader_layout_reset();
    return index;
}

void reader_page_index_free(ReaderPageIndex* index) {
    if(index == NULL) {
        return;
    }
    free(index->starts);
    free(index);
}

size_t reader_page_index_count(const ReaderPageIndex* index) {
    if(index == NULL) {
        return 0;
    }
    return index->count > 0 ? index->count : 1;
}

bool reader_page_index_position(const ReaderPageIndex* index, size_t page, ReaderPosition* out) {
    if(index == NULL || page >= index->count) {
        return false;
    }
    if(out != NULL) {
        out->chapter = index->chapter;
        out->offset = index->starts[page];
    }
    return true;
}

size_t reader_page_index_lookup(const ReaderPageIndex* index, ReaderPosition position) {
    if(index == NULL || index->count == 0 || position.chapter != index->chapter) {
        return 0;
    }

    /* The last start that is not past the position. */
    size_t low = 0;
    size_t high = index->count;
    while(low + 1 < high) {
        size_t middle = low + (high - low) / 2;
        if(index->starts[middle] <= position.offset) {
            low = middle;
        } else {
            high = middle;
        }
    }
    return low;
}

/* ---------------------------------------------------------- page counter -- */

ReaderPageCounter* reader_page_counter_alloc(ReaderBook* book) {
    if(book == NULL || book->format == NULL || book->format->chapter_count == NULL) {
        return NULL;
    }

    size_t chapters = book->format->chapter_count(book);
    if(chapters == 0) {
        chapters = 1;
    }

    ReaderPageCounter* counter = malloc(sizeof(ReaderPageCounter));
    if(counter == NULL) {
        return NULL;
    }
    memset(counter, 0, sizeof(ReaderPageCounter));
    counter->pages = calloc(chapters, sizeof(uint16_t));
    if(counter->pages == NULL) {
        free(counter);
        return NULL;
    }

    counter->book = book;
    counter->chapters = chapters;
    counter->from.chapter = 0;
    counter->from.offset = 0;
    return counter;
}

bool reader_page_counter_extend(
    ReaderPageCounter* counter,
    const ReaderLayoutMetrics* metrics,
    size_t max_lines) {
    if(counter == NULL || counter->complete) {
        return false;
    }
    if(max_lines == 0) {
        /* Same contract as the index: a zero line budget is an error, not a page. */
        counter->complete = true;
        return false;
    }

    const ReaderPosition from = counter->from;
    ReaderPage* page = reader_layout_page(counter->book, from, metrics, max_lines);
    bool exists = page != NULL && page->line_count > 0;
    bool advanced = exists && page->next.chapter == counter->chapter &&
                    page->next.offset > from.offset;
    if(page != NULL) {
        counter->from = page->next;
        reader_page_free(page);
    }

    if(exists && counter->pages[counter->chapter] < UINT16_MAX) {
        counter->pages[counter->chapter]++;
    }
    if(counter->pages[counter->chapter] == UINT16_MAX) {
        /* The count saturates: a chapter cannot have more pages than this. */
        advanced = false;
    }

    if(!advanced) {
        /* An empty or unreadable chapter is still one blank page. */
        if(counter->pages[counter->chapter] == 0) {
            counter->pages[counter->chapter] = 1;
        }
        counter->chapter++;
        counter->from.chapter = counter->chapter;
        counter->from.offset = 0;
        if(counter->chapter >= counter->chapters) {
            counter->complete = true;
        }
    }

    return !counter->complete;
}

bool reader_page_counter_complete(const ReaderPageCounter* counter) {
    return counter == NULL || counter->complete;
}

size_t reader_page_counter_chapters(const ReaderPageCounter* counter) {
    return counter != NULL ? counter->chapters : 0;
}

size_t reader_page_counter_progress(const ReaderPageCounter* counter) {
    return counter != NULL ? counter->chapter : 0;
}

uint32_t reader_page_counter_total(const ReaderPageCounter* counter) {
    if(counter == NULL) {
        return 0;
    }

    uint32_t total = 0;
    for(size_t chapter = 0; chapter < counter->chapters; chapter++) {
        total += counter->pages[chapter];
    }
    return total;
}

bool reader_page_counter_pages_before(
    const ReaderPageCounter* counter,
    size_t chapter,
    uint32_t* pages) {
    if(counter == NULL || chapter > counter->chapters) {
        return false;
    }

    uint32_t total = 0;
    for(size_t index = 0; index < chapter; index++) {
        if(counter->pages[index] == 0) {
            return false;
        }
        total += counter->pages[index];
    }

    if(pages != NULL) {
        *pages = total;
    }
    return true;
}

void reader_page_counter_free(ReaderPageCounter* counter) {
    if(counter == NULL) {
        return;
    }
    free(counter->pages);
    free(counter);
}
