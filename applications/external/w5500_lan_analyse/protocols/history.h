#pragma once

#include <stdint.h>
#include <stdbool.h>

/* History file storage path */
#define HISTORY_DIR APP_DATA_PATH("history")

/* Max files to list (16 × 68 = 1088 bytes) */
#define HISTORY_MAX_FILES 16

/* Max filename length */
#define HISTORY_FILENAME_LEN 48

/* History file entry */
typedef struct {
    char filename[HISTORY_FILENAME_LEN];
    char label[20]; /* Short display label for submenu */
} HistoryEntry;

/* History browser state */
typedef struct {
    HistoryEntry files[HISTORY_MAX_FILES];
    uint16_t file_count;
    uint16_t selected;
} HistoryState;

/**
 * Generate a timestamped filename.
 * type: scan type string (e.g. "arp_scan", "dns_lookup")
 * out: output buffer (at least HISTORY_FILENAME_LEN bytes)
 */
void history_make_filename(const char* type, char* out, uint16_t out_size);

/**
 * Save content to a timestamped file.
 * type: scan type
 * content: text to save
 * Returns true on success.
 */
bool history_save(const char* type, const char* content);

/**
 * List all saved history files.
 * state: output
 * Returns number of files found.
 */
uint16_t history_list(HistoryState* state);

/**
 * Read a history file's contents into a FuriString.
 * filename: just the filename (not full path)
 * out: output string (caller must free if needed)
 * Returns true on success.
 */
bool history_read_file(const char* filename, char* out, uint16_t out_size);

/**
 * Delete a history file.
 * filename: just the filename
 * Returns true on success.
 */
bool history_delete_file(const char* filename);
