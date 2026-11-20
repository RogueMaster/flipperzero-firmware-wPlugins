/**
 * @file fzreader_state.c
 * @brief Reading state persisted in the application data folder.
 *
 * APP_DATA_PATH resolves to /ext/apps_data/fzreader, the folder the storage
 * service creates for this app on the first write.
 *
 * The file is a plain list of key=value lines. Loading never fails loudly:
 * a missing file, an unknown version or a broken line leave the defaults in
 * place. Saving goes through a temporary file so that a power loss while
 * writing cannot leave a half-written state behind.
 */

#include "fzreader_state.h"

#include <furi.h>
#include <storage/storage.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fonts/reader_fonts.h"

#define FZREADER_STATE_PATH     APP_DATA_PATH("state.txt")
#define FZREADER_STATE_TMP_PATH APP_DATA_PATH("state.txt.tmp")

/** Longest line accepted from the file, the book path plus "book=" and "\n". */
#define FZREADER_STATE_LINE_MAX (FZREADER_STATE_BOOK_MAX + 64)

/** Chunk size used to read the file as a stream. */
#define FZREADER_STATE_CHUNK 128

/** Size of the buffer the whole state is formatted into. */
#define FZREADER_STATE_WRITE_MAX (FZREADER_STATE_BOOK_MAX + 128)

/** Largest number accepted from the file, anything above is a broken line. */
#define FZREADER_STATE_NUMBER_MAX (SIZE_MAX / 2)

static bool fzreader_state_parse_number(const char* value, size_t* number) {
    char* end = NULL;
    unsigned long long parsed = strtoull(value, &end, 10);
    if(end == value || parsed > FZREADER_STATE_NUMBER_MAX) {
        return false;
    }
    *number = (size_t)parsed;
    return true;
}

static void fzreader_state_parse_line(FzReaderState* state, const char* line, unsigned* version) {
    const char* separator = strchr(line, '=');
    if(separator == NULL) {
        return;
    }

    const size_t key_size = (size_t)(separator - line);
    const char* value = separator + 1;
    size_t number = 0;

    if(key_size == 4 && strncmp(line, "book", key_size) == 0) {
        strncpy(state->book, value, sizeof(state->book) - 1);
        state->book[sizeof(state->book) - 1] = '\0';
    } else if(key_size == 9 && strncmp(line, "book_size", key_size) == 0) {
        if(fzreader_state_parse_number(value, &number)) {
            state->book_size = (uint64_t)number;
        }
    } else if(key_size == 7 && strncmp(line, "chapter", key_size) == 0) {
        if(fzreader_state_parse_number(value, &number)) {
            state->chapter = number;
        }
    } else if(key_size == 6 && strncmp(line, "offset", key_size) == 0) {
        if(fzreader_state_parse_number(value, &number)) {
            state->offset = number;
        }
    } else if(key_size == 4 && strncmp(line, "font", key_size) == 0) {
        if(fzreader_state_parse_number(value, &number) && number < READER_FONT_COUNT) {
            state->font = number;
        }
    } else if(key_size == 7 && strncmp(line, "version", key_size) == 0) {
        if(fzreader_state_parse_number(value, &number) && number <= UINT32_MAX) {
            *version = (unsigned)number;
        }
    }
}

static void
    fzreader_state_parse(FzReaderState* state, char* line, size_t size, unsigned* version) {
    if(size >= FZREADER_STATE_LINE_MAX) {
        size = FZREADER_STATE_LINE_MAX - 1;
    }
    line[size] = '\0';

    /* Tolerate CRLF line endings. */
    if(size > 0 && line[size - 1] == '\r') {
        line[size - 1] = '\0';
    }

    fzreader_state_parse_line(state, line, version);
}

void fzreader_state_set_defaults(FzReaderState* state) {
    memset(state, 0, sizeof(FzReaderState));
    state->font = READER_FONT_DEFAULT;
}

void fzreader_state_load(FzReaderState* state) {
    fzreader_state_set_defaults(state);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, FZREADER_STATE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char chunk[FZREADER_STATE_CHUNK];
        char line[FZREADER_STATE_LINE_MAX];
        size_t line_size = 0;
        unsigned version = 0;

        for(;;) {
            size_t read = storage_file_read(file, chunk, sizeof(chunk));
            if(read == 0) {
                break;
            }
            for(size_t index = 0; index < read; index++) {
                char symbol = chunk[index];
                if(symbol == '\n') {
                    fzreader_state_parse(state, line, line_size, &version);
                    line_size = 0;
                } else if(line_size < sizeof(line)) {
                    line[line_size++] = symbol;
                }
            }
        }
        if(line_size > 0) {
            fzreader_state_parse(state, line, line_size, &version);
        }

        if(version != FZREADER_STATE_VERSION) {
            /* Unknown or missing version: keep the defaults. */
            fzreader_state_set_defaults(state);
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void fzreader_state_save(const FzReaderState* state) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, FZREADER_STATE_TMP_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char buffer[FZREADER_STATE_WRITE_MAX];
        int size = snprintf(
            buffer,
            sizeof(buffer),
            "version=%d\n"
            "book=%s\n"
            "book_size=%llu\n"
            "chapter=%zu\n"
            "offset=%zu\n"
            "font=%zu\n",
            FZREADER_STATE_VERSION,
            state->book,
            (unsigned long long)state->book_size,
            state->chapter,
            state->offset,
            state->font);

        bool written = false;
        if(size > 0 && (size_t)size < sizeof(buffer)) {
            written = storage_file_write(file, buffer, (size_t)size) == (size_t)size;
        }
        storage_file_close(file);

        if(written) {
            storage_common_rename(storage, FZREADER_STATE_TMP_PATH, FZREADER_STATE_PATH);
        }
    } else {
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

bool fzreader_state_file_size(const char* path, uint64_t* size) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FileInfo info;
    bool found = storage_common_stat(storage, path, &info) == FSE_OK &&
                 (info.flags & FSF_DIRECTORY) == 0;
    furi_record_close(RECORD_STORAGE);

    if(found && size != NULL) {
        *size = info.size;
    }
    return found;
}
