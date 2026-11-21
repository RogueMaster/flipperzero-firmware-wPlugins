#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Bytes reserved for the book path in the state file. */
#define FZREADER_STATE_BOOK_MAX 256

/** Format version written to the state file. */
#define FZREADER_STATE_VERSION 1

/** Reading state kept between runs. */
typedef struct {
    char book[FZREADER_STATE_BOOK_MAX]; /**< path of the last opened book, empty if none */
    uint64_t book_size; /**< size of the book file when it was opened */
    size_t chapter; /**< chapter of the first line on the page */
    size_t offset; /**< code point offset of the first line on the page */
    size_t font; /**< font size index */
} FzReaderState;

/** Fill the state with the defaults used when nothing is saved. */
void fzreader_state_set_defaults(FzReaderState* state);

/** Load the state. A missing or broken file yields defaults, never an error. */
void fzreader_state_load(FzReaderState* state);

/** Save the state atomically. Failures are silently ignored. */
void fzreader_state_save(const FzReaderState* state);

/** Size of an existing file. @returns false if the path is not a readable file. */
bool fzreader_state_file_size(const char* path, uint64_t* size);
