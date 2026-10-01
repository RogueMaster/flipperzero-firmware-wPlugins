#include "logic.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Simple CRC32 implementation to replace restricted furi_hal_crc_calc_crc32
uint32_t calculate_crc32(uint32_t crc, const uint8_t *data, size_t size) {
    crc = ~crc;
    for (size_t i = 0; i < size; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
        }
    }
    return ~crc;
}

// Simple Insertion Sort to replace restricted qsort
static void sort_records(FileRecord *records, size_t count) {
    for (size_t i = 1; i < count; i++) {
        FileRecord key = records[i];
        int j = (int)i - 1;
        while (j >= 0 && (records[j].size > key.size ||
                          (records[j].size == key.size && records[j].hash > key.hash))) {
            records[j + 1] = records[j];
            j--;
        }
        records[j + 1] = key;
    }
}

static bool records_match(const FileRecord *a, const FileRecord *b) {
    return a->size == b->size && a->hash == b->hash;
}

void db_remove_record(HashDatabase *db, const char *filename) {
    for (size_t i = 0; i < db->count; i++) {
        if (strcmp(db->records[i].path, filename) == 0) {
            for (size_t j = i; j < db->count - 1; j++) {
                db->records[j] = db->records[j + 1];
            }
            db->count--;
            return;
        }
    }
}

void process_duplicates(HashDatabase *db) {
    db->num_groups = 0;
    if (db->count == 0)
        return;

    sort_records(db->records, db->count);

    for (size_t i = 0; i < db->count; i++) {
        bool is_start = false;
        if (i < db->count - 1 && records_match(&db->records[i], &db->records[i + 1])) {
            if (i == 0 || !records_match(&db->records[i], &db->records[i - 1])) {
                is_start = true;
            }
        }

        if (is_start) {
            size_t j = i;
            while (j < db->count && records_match(&db->records[j], &db->records[i])) {
                j++;
            }

            // Only add group if we have actual duplicates (> 1 file)
            if (j - i > 1) {
                db->groups[db->num_groups].hash = db->records[i].hash;
                db->groups[db->num_groups].size = db->records[i].size;
                db->groups[db->num_groups].start_index = i;
                db->groups[db->num_groups].count = j - i;
                db->num_groups++;
            }
        }
    }
}

bool is_sub_file(const char *name) {
    size_t len = strlen(name);
    if (len < 4)
        return false;
    const char *ext = name + len - 4;
    return tolower((unsigned char)ext[0]) == '.' && tolower((unsigned char)ext[1]) == 's' &&
           tolower((unsigned char)ext[2]) == 'u' && tolower((unsigned char)ext[3]) == 'b';
}

bool path_join(char *out, size_t cap, const char *dir, const char *name) {
    int written = snprintf(out, cap, "%s/%s", dir, name);
    return written >= 0 && (size_t)written < cap;
}

bool scan_dir_is_valid(const char *path) {
    if (path == NULL)
        return false;
    size_t len = strlen(path);
    if (len == 0 || len >= FULL_PATH_LEN)
        return false;
    if (strncmp(path, "/ext", 4) != 0)
        return false;
    if (len > 4 && path[4] != '/')
        return false;
    if (strstr(path, "..") != NULL)
        return false;
    return true;
}

ScanAdd scan_add_file(HashDatabase *db, ScanStats *stats, const char *name, uint32_t size,
                      uint32_t hash, bool read_ok) {
    if (!is_sub_file(name))
        return ScanAddOk;

    if (strlen(name) >= APP_MAX_PATH_LEN) {
        stats->name_too_long++;
        return ScanAddOk;
    }

    if (!read_ok) {
        stats->unreadable++;
        return ScanAddOk;
    }

    if (db->count >= MAX_FILES) {
        stats->hit_limit = true;
        return ScanAddStop;
    }

    FileRecord *record = &db->records[db->count];
    strncpy(record->path, name, APP_MAX_PATH_LEN - 1);
    record->path[APP_MAX_PATH_LEN - 1] = '\0';
    record->hash = hash;
    record->size = size;
    db->count++;
    stats->added++;
    return ScanAddOk;
}

bool path_is_ext_root(const char *path) {
    return path != NULL && strcmp(path, "/ext") == 0;
}

bool path_parent(const char *path, char *out, size_t cap) {
    if (path == NULL || path_is_ext_root(path))
        return false;

    const char *slash = strrchr(path, '/');
    if (slash == NULL || slash == path)
        return false;

    size_t len = (size_t)(slash - path);
    if (len >= cap)
        return false;

    memcpy(out, path, len);
    out[len] = '\0';
    return true;
}

void path_display_name(const char *path, char *out, size_t cap) {
    const char *name;
    if (path_is_ext_root(path)) {
        name = "SD card";
    } else {
        const char *slash = strrchr(path, '/');
        name = slash ? slash + 1 : path;
    }
    snprintf(out, cap, "%s", name);
}

void path_header_tail(const char *path, size_t max_chars, char *out, size_t cap) {
    size_t len = strlen(path);
    if (len <= max_chars) {
        snprintf(out, cap, "%s", path);
        return;
    }

    const char *ellipsis = "...";
    size_t ellipsis_len = strlen(ellipsis);
    size_t tail_len = max_chars > ellipsis_len ? max_chars - ellipsis_len : 0;
    snprintf(out, cap, "%s%s", ellipsis, path + (len - tail_len));
}

BrowseEntryDecision browse_decide_entry(const char *name, size_t count, size_t max_names) {
    if (name[0] == '.')
        return BrowseEntrySkip;
    if (strlen(name) >= APP_MAX_PATH_LEN)
        return BrowseEntryOverflow;
    if (count >= max_names)
        return BrowseEntryOverflow;
    return BrowseEntryAdd;
}
