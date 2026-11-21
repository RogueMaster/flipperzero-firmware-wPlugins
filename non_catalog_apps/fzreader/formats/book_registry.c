/**
 * @file book_registry.c
 * @brief Known book formats and opening by file extension.
 */
#include "reader_book.h"

#include <string.h>

#include "epub/epub.h"

const ReaderFormat* const reader_formats[] = {&reader_format_epub, NULL};

/** Case-insensitive check of the trailing extension, dot included. */
static bool reader_has_extension(const char* path, const char* extension) {
    size_t length = strlen(path);
    size_t extension_length = strlen(extension);
    if(length < extension_length) {
        return false;
    }

    const char* tail = path + length - extension_length;
    for(size_t i = 0; i < extension_length; i++) {
        char byte = tail[i];
        if(byte >= 'A' && byte <= 'Z') {
            byte = (char)(byte - 'A' + 'a');
        }
        if(byte != extension[i]) {
            return false;
        }
    }
    return true;
}

/** Whether the path has one of the extensions the format declares. */
static bool reader_format_accepts(const ReaderFormat* format, const char* path) {
    if(format->extensions == NULL) {
        return true;
    }

    for(size_t i = 0; format->extensions[i] != NULL; i++) {
        if(reader_has_extension(path, format->extensions[i])) {
            return true;
        }
    }
    return false;
}

ReaderBook* reader_book_open(const char* path) {
    if(path == NULL) {
        return NULL;
    }

    for(size_t i = 0; reader_formats[i] != NULL; i++) {
        const ReaderFormat* format = reader_formats[i];
        if(format->open == NULL || !reader_format_accepts(format, path)) {
            continue;
        }
        ReaderBook* book = format->open(path);
        if(book != NULL) {
            return book;
        }
    }

    return NULL;
}

const char* reader_book_extension_mask(void) {
    for(size_t i = 0; reader_formats[i] != NULL; i++) {
        const char* const* extensions = reader_formats[i]->extensions;
        if(extensions != NULL && extensions[0] != NULL) {
            return extensions[0];
        }
    }
    return NULL;
}
