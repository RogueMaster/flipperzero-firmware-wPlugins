#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <storage/storage.h>

// Safe string parsing utilities (strtok replacement)

// Buffered line reader for storage files.
// Reads the file in chunks but always returns complete lines, so lines that
// straddle a chunk boundary are never split or corrupted.
#define FILE_LINE_READER_BUFFER_SIZE 256

typedef struct {
    File* file;
    char buffer[FILE_LINE_READER_BUFFER_SIZE];
    size_t len;
    size_t pos;
} FileLineReader;

// Initialize reader over an already-opened file (positioned at read start)
void file_line_reader_init(FileLineReader* reader, File* file);

// Read next line into `line` (null-terminated, '\n' and '\r' stripped).
// Lines longer than line_size - 1 are truncated (remainder is consumed).
// Returns false when the file is exhausted.
bool file_line_reader_next(FileLineReader* reader, char* line, size_t line_size);

// Split a CSV line into fields (does not handle quoted commas)
// Returns number of fields found
int string_split_csv(char* line, char* fields[], int max_fields);
