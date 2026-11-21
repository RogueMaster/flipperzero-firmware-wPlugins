#pragma once

#include <furi.h>

/** One run of text with uniform styling, as produced while reading a chapter. */
typedef struct {
    const char* text; /**< UTF-8, not zero-terminated */
    size_t size; /**< bytes in text */
    bool bold;
} ReaderSpan;

/** A book opened from a file, format agnostic. */
typedef struct ReaderBook ReaderBook;

struct ReaderBook {
    const struct ReaderFormat* format; /**< driver the book was opened with */
};

/** Format driver. Every entry point is mandatory unless stated otherwise. */
typedef struct ReaderFormat {
    const char* name;

    /** File extensions of the format, dot included, NULL-terminated. */
    const char* const* extensions;

    /** Open a book. @returns NULL if the file is not this format or is unreadable. */
    ReaderBook* (*open)(const char* path);

    void (*close)(ReaderBook* book);

    /** Book title for the status bar, may be an empty string. */
    const char* (*title)(const ReaderBook* book);

    /** Chapter title, or NULL. Optional. */
    const char* (*chapter_title)(const ReaderBook* book, size_t chapter);

    /** Number of chapters in reading order. */
    size_t (*chapter_count)(const ReaderBook* book);

    /**
     * Open a chapter for sequential reading.
     *
     * @returns false if the chapter cannot be read. After a successful call,
     *          read_span is valid until chapter_close.
     */
    bool (*chapter_open)(ReaderBook* book, size_t chapter);

    void (*chapter_close)(ReaderBook* book);

    /**
     * Get the next span of the chapter.
     *
     * @returns false at the end of the chapter. The text is valid until the next
     *          call. Spans are handed out in reading order, character offsets are
     *          counted in UTF-8 code points over the concatenation of all spans.
     */
    bool (*read_span)(ReaderBook* book, ReaderSpan* span);
} ReaderFormat;

/** Pick a format by file extension and open the file. @returns NULL on failure. */
ReaderBook* reader_book_open(const char* path);

/** Extension mask for the system file browser: the first extension of the first format. */
const char* reader_book_extension_mask(void);

/** Formats known to the reader, NULL-terminated. */
extern const ReaderFormat* const reader_formats[];
