#include "dnd_spellbook_sort.h"
#include "dnd_data.h"
#include "dnd_fs.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DND_SORT_BATCH 24U
#define DND_SORT_BUFFER 256U
#define DND_SORT_PREFIX 1280U

typedef struct {
    uint32_t offset;
    uint32_t length; /* Record body only, excluding its line ending. */
    uint8_t level;
    char name[DND_SPELL_NAME_LEN];
} DndSortKey;

typedef struct {
    File* file;
    uint32_t offset;
    uint16_t used;
    uint16_t available;
    uint8_t bytes[DND_SORT_BUFFER];
} DndSortReader;

typedef struct {
    DndSortReader reader;
    DndSortKey keys[DND_SORT_BATCH];
    DndSortKey previous;
    DndSortKey candidates[2];
    DndSortKey heads[2];
    char prefix[DND_SORT_PREFIX];
    uint8_t copy_buffer[DND_SORT_BUFFER];
    uint32_t line_offset;
    uint32_t line_length;
    uint8_t ending_length;
    bool read_error;
    bool prefix_truncated;
    DndSpellbookSortStats stats;
} DndSortWork;

static bool dnd_sort_path(char* output, size_t size, const char* path, const char* suffix) {
    int length = snprintf(output, size, "%s%s", path, suffix);
    return length >= 0 && (size_t)length < size;
}

static bool dnd_sort_remove(Storage* storage, const char* path) {
    FileInfo info;
    FS_Error error = storage_common_stat(storage, path, &info);
    return error == FSE_NOT_EXIST ||
           (error == FSE_OK && !file_info_is_dir(&info) &&
            storage_common_remove(storage, path) == FSE_OK);
}

static File* dnd_sort_open(Storage* storage, const char* path, bool write) {
    File* file = storage_file_alloc(storage);
    if(file && !storage_file_open(
                   file, path, write ? FSAM_WRITE : FSAM_READ,
                   write ? FSOM_CREATE_ALWAYS : FSOM_OPEN_EXISTING)) {
        storage_file_close(file);
        storage_file_free(file);
        file = NULL;
    }
    return file;
}

static bool dnd_sort_close(File* file) {
    bool ok = true;
    if(file) {
        ok = storage_file_close(file);
        storage_file_free(file);
    }
    return ok;
}

static bool dnd_sort_reader_begin(DndSortWork* work, File* file) {
    if(!storage_file_seek(file, 0U, true)) return false;
    memset(&work->reader, 0, sizeof(work->reader));
    work->reader.file = file;
    work->read_error = false;
    ++work->stats.source_scans;
    return true;
}

/* The prefix is only for extracting a key. Long/unknown lines are never
 * truncated on disk: their full byte span is copied from the original file. */
static bool dnd_sort_line(DndSortWork* work) {
    DndSortReader* reader = &work->reader;
    size_t prefix_length = 0U;
    uint8_t previous = 0U;
    work->line_offset = reader->offset;
    work->line_length = 0U;
    work->ending_length = 0U;
    work->prefix_truncated = false;
    for(;;) {
        if(reader->used == reader->available) {
            reader->available = (uint16_t)storage_file_read(
                reader->file, reader->bytes, sizeof(reader->bytes));
            reader->used = 0U;
            if(!reader->available) {
                if(storage_file_get_error(reader->file) != FSE_OK ||
                   reader->offset != storage_file_size(reader->file))
                    work->read_error = true;
                break;
            }
        }
        if(reader->offset == UINT32_MAX) {
            work->read_error = true;
            return false;
        }
        uint8_t value = reader->bytes[reader->used++];
        ++reader->offset;
        ++work->line_length;
        if(value == '\n') {
            work->ending_length = previous == '\r' ? 2U : 1U;
            break;
        }
        if(value != '\r') {
            if(prefix_length + 1U < sizeof(work->prefix))
                work->prefix[prefix_length++] = (char)value;
            else
                work->prefix_truncated = true;
        }
        previous = value;
    }
    work->prefix[prefix_length] = '\0';
    return work->line_length != 0U && !work->read_error;
}

static uint8_t dnd_sort_fold(char value) {
    return (uint8_t)((value >= 'A' && value <= 'Z') ? value + ('a' - 'A') : value);
}

static int dnd_sort_compare(const DndSortKey* left, const DndSortKey* right) {
    if(left->level != right->level) return left->level < right->level ? -1 : 1;
    for(size_t i = 0U; i < sizeof(left->name); ++i) {
        uint8_t a = dnd_sort_fold(left->name[i]);
        uint8_t b = dnd_sort_fold(right->name[i]);
        if(a != b) return a < b ? -1 : 1;
        if(!a) break;
    }
    if(left->offset == right->offset) return 0;
    return left->offset < right->offset ? -1 : 1;
}

static int8_t dnd_sort_hex(char value) {
    if(value >= '0' && value <= '9') return (int8_t)(value - '0');
    if(value >= 'A' && value <= 'F') return (int8_t)(value - 'A' + 10);
    if(value >= 'a' && value <= 'f') return (int8_t)(value - 'a' + 10);
    return -1;
}

static bool dnd_sort_number(const char* begin, const char* end, int32_t* result) {
    if(begin == end) return false;
    bool negative = *begin == '-';
    if(negative && ++begin == end) return false;
    uint32_t limit = negative ? (uint32_t)INT32_MAX + 1U : (uint32_t)INT32_MAX;
    uint32_t value = 0U;
    while(begin < end) {
        if(*begin < '0' || *begin > '9') return false;
        uint8_t digit = (uint8_t)(*begin++ - '0');
        if(value > limit / 10U || (value == limit / 10U && digit > limit % 10U)) return false;
        value = value * 10U + digit;
    }
    *result = negative ? value == (uint32_t)INT32_MAX + 1U ? INT32_MIN : -(int32_t)value :
                        (int32_t)value;
    return true;
}

/* Recognize the same eight-field, nine/ten-number records as storage's
 * dnd_storage_parse_spell_record(), including empty names and uint8/clamped
 * imported levels. Extra pipe fields have the storage parser's legacy behavior.
 * Malformed records stay in place as opaque bytes. */
static bool dnd_sort_key(DndSortWork* work, DndSortKey* key) {
    const char* line = work->prefix;
    if(strncmp(line, "S|", 2U)) return false;
    const char* name = line + 2U;
    const char* name_end = strchr(name, '|');
    if(!name_end) return false;
    const char* numbers = line;
    for(uint8_t i = 0U; i < 7U; ++i) {
        numbers = strchr(numbers, '|');
        if(!numbers) return false;
        ++numbers;
    }
    const char* field_end = strchr(numbers, '|');
    if(!field_end) {
        if(work->prefix_truncated) return false;
        field_end = numbers + strlen(numbers);
    }
    uint8_t fields = 0U;
    int32_t first = 0;
    while(numbers < field_end && fields < 10U) {
        const char* end = numbers;
        while(end < field_end && *end != ',') ++end;
        int32_t value = 0;
        if(!dnd_sort_number(numbers, end, &value)) return false;
        if(!fields) first = value;
        ++fields;
        if(end == field_end) {
            numbers = end;
            break;
        }
        numbers = end + 1U;
        if(numbers == field_end) return false;
    }
    if(numbers != field_end || (fields != 9U && fields != 10U)) return false;
    uint8_t level = (uint8_t)first;
    if(level > 9U) level = 9U;
    memset(key, 0, sizeof(*key));
    key->offset = work->line_offset;
    key->length = work->line_length - work->ending_length;
    key->level = (uint8_t)level;
    size_t used = 0U;
    while(name < name_end && used + 1U < sizeof(key->name)) {
        if(*name == '%' && name + 2U < name_end) {
            int8_t high = dnd_sort_hex(name[1]);
            int8_t low = dnd_sort_hex(name[2]);
            if(high >= 0 && low >= 0) {
                key->name[used++] = (char)(((uint8_t)high << 4U) | (uint8_t)low);
                name += 3U;
                continue;
            }
        }
        key->name[used++] = *name++;
    }
    return true;
}

static bool dnd_sort_scan(
    DndSortWork* work, File* source, const DndSortKey* ignored, bool* ordered) {
    if(!dnd_sort_reader_begin(work, source)) return false;
    bool have_previous = false;
    uint32_t count = 0U;
    *ordered = true;
    while(dnd_sort_line(work)) {
        DndSortKey* key = &work->heads[0];
        if(!dnd_sort_key(work, key)) continue;
        if(count == UINT32_MAX / sizeof(DndSortKey)) return false;
        ++count;
        if(ignored && key->offset == ignored->offset) continue;
        if(have_previous && dnd_sort_compare(&work->previous, key) > 0) {
            if(!ignored && *ordered) {
                work->candidates[0] = work->previous;
                work->candidates[1] = *key;
            }
            *ordered = false;
        }
        work->previous = *key;
        have_previous = true;
    }
    if(!ignored) work->stats.records = count;
    return !work->read_error;
}

static bool dnd_sort_write_key(File* file, const DndSortKey* key) {
    return storage_file_write(file, key, sizeof(*key)) == sizeof(*key);
}

static bool dnd_sort_read_key(File* file, DndSortKey* key) {
    return storage_file_read(file, key, sizeof(*key)) == sizeof(*key) &&
           storage_file_get_error(file) == FSE_OK;
}

static bool dnd_sort_flush_run(DndSortWork* work, File* output, uint8_t count) {
    for(uint8_t i = 1U; i < count; ++i) {
        DndSortKey key = work->keys[i];
        uint8_t j = i;
        while(j && dnd_sort_compare(&key, &work->keys[j - 1U]) < 0) {
            work->keys[j] = work->keys[j - 1U];
            --j;
        }
        work->keys[j] = key;
    }
    return storage_file_write(output, work->keys, count * sizeof(DndSortKey)) ==
           count * sizeof(DndSortKey);
}

static bool dnd_sort_make_index(
    DndSortWork* work, File* source, File* output, const DndSortKey* single) {
    if(!dnd_sort_reader_begin(work, source)) return false;
    uint8_t count = 0U;
    bool inserted = false;
    while(dnd_sort_line(work)) {
        DndSortKey* key = &work->heads[0];
        if(!dnd_sort_key(work, key)) continue;
        if(single) {
            if(single->offset == key->offset) continue;
            if(!inserted && dnd_sort_compare(single, key) < 0) {
                if(!dnd_sort_write_key(output, single)) return false;
                inserted = true;
            }
            if(!dnd_sort_write_key(output, key)) return false;
        } else {
            work->keys[count++] = *key;
            if(count == DND_SORT_BATCH) {
                if(!dnd_sort_flush_run(work, output, count)) return false;
                count = 0U;
            }
        }
    }
    if(work->read_error) return false;
    if(single && !inserted && !dnd_sort_write_key(output, single)) return false;
    if(count && !dnd_sort_flush_run(work, output, count)) return false;
    return storage_file_sync(output);
}

static bool dnd_sort_merge(
    DndSortWork* work, File* left, File* right, File* output, uint32_t run) {
    const uint32_t total = work->stats.records;
    for(uint32_t start = 0U; start < total;) {
        uint32_t remaining[2];
        remaining[0] = total - start < run ? total - start : run;
        remaining[1] = total - start - remaining[0];
        if(remaining[1] > run) remaining[1] = run;
        if(!storage_file_seek(left, start * sizeof(DndSortKey), true) ||
           !storage_file_seek(right, (start + remaining[0]) * sizeof(DndSortKey), true))
            return false;
        File* inputs[2] = {left, right};
        for(uint8_t side = 0U; side < 2U; ++side)
            if(remaining[side] && !dnd_sort_read_key(inputs[side], &work->heads[side]))
                return false;
        uint32_t merged = remaining[0] + remaining[1];
        while(remaining[0] || remaining[1]) {
            uint8_t side = !remaining[0] ? 1U : !remaining[1] ? 0U :
                              dnd_sort_compare(&work->heads[0], &work->heads[1]) <= 0 ? 0U : 1U;
            if(!dnd_sort_write_key(output, &work->heads[side])) return false;
            --remaining[side];
            if(remaining[side] && !dnd_sort_read_key(inputs[side], &work->heads[side]))
                return false;
        }
        start += merged;
    }
    return storage_file_sync(output);
}

static bool dnd_sort_copy_span(
    DndSortWork* work, File* input, File* output, uint32_t offset, uint32_t length) {
    if(!storage_file_seek(input, offset, true)) return false;
    while(length) {
        size_t bytes = length < sizeof(work->copy_buffer) ? length : sizeof(work->copy_buffer);
        if(storage_file_read(input, work->copy_buffer, bytes) != bytes ||
           storage_file_get_error(input) != FSE_OK ||
           storage_file_write(output, work->copy_buffer, bytes) != bytes)
            return false;
        length -= (uint32_t)bytes;
    }
    return true;
}

static bool dnd_sort_emit(
    DndSortWork* work, File* source, File* raw, File* index, File* output) {
    if(!dnd_sort_reader_begin(work, source)) return false;
    uint32_t records = 0U;
    while(dnd_sort_line(work)) {
        if(dnd_sort_key(work, &work->heads[0])) {
            if(!dnd_sort_read_key(index, &work->heads[1])) return false;
            const DndSortKey* key = &work->heads[1];
            if((uint64_t)key->offset + key->length > storage_file_size(raw) ||
               !dnd_sort_copy_span(work, raw, output, key->offset, key->length))
                return false;
            if(work->ending_length &&
               !dnd_sort_copy_span(
                   work, raw, output,
                   work->line_offset + work->line_length - work->ending_length,
                   work->ending_length))
                return false;
            ++records;
        } else if(!dnd_sort_copy_span(work, raw, output, work->line_offset, work->line_length)) {
            return false;
        }
    }
    return !work->read_error && records == work->stats.records && storage_file_sync(output);
}

static bool dnd_sort_publish(Storage* storage, const char* temp, const char* live) {
    char backup[DND_FS_LONG_PATH_LEN];
    if(!dnd_sort_path(backup, sizeof(backup), live, ".sort.bak") ||
       !dnd_fs_recover_sort(storage, live, NULL))
        return false;
    FileInfo info;
    FS_Error error = storage_common_stat(storage, live, &info);
    bool had_live = error == FSE_OK && !file_info_is_dir(&info);
    if(error != FSE_NOT_EXIST && !had_live) return false;
    if(had_live && storage_common_rename_safe(storage, live, backup) != FSE_OK) return false;
    if(storage_common_rename_safe(storage, temp, live) != FSE_OK) {
        if(had_live) (void)storage_common_rename_safe(storage, backup, live);
        return false;
    }
    /* A cleanup error is recoverable; the newly published live is authoritative. */
    if(had_live) (void)dnd_sort_remove(storage, backup);
    return true;
}

static bool dnd_sort_snapshot(
    DndSortWork* work, Storage* storage, const char* source, const char* snapshot) {
    char temp[DND_FS_LONG_PATH_LEN];
    if(!dnd_sort_path(temp, sizeof(temp), snapshot, ".sort.tmp")) return false;
    File* input = dnd_sort_open(storage, source, false);
    File* output = dnd_sort_open(storage, temp, true);
    bool ok = input && output && storage_file_size(input) <= UINT32_MAX &&
              dnd_sort_copy_span(work, input, output, 0U, (uint32_t)storage_file_size(input)) &&
              storage_file_sync(output);
    if(!dnd_sort_close(output)) ok = false;
    if(!dnd_sort_close(input)) ok = false;
    if(ok) ok = dnd_sort_publish(storage, temp, snapshot);
    if(!ok) (void)dnd_sort_remove(storage, temp);
    return ok;
}

bool dnd_spellbook_sort(
    Storage* storage, const char* live, const char* snapshot, DndSpellbookSortStats* stats) {
    if(stats) memset(stats, 0, sizeof(*stats));
    if(!storage || !live || !live[0] || (snapshot && !strcmp(live, snapshot))) return false;
    char indices[2][DND_FS_LONG_PATH_LEN];
    char temp[DND_FS_LONG_PATH_LEN];
    if(!dnd_sort_path(indices[0], sizeof(indices[0]), live, ".sort.idx0") ||
       !dnd_sort_path(indices[1], sizeof(indices[1]), live, ".sort.idx1") ||
       !dnd_sort_path(temp, sizeof(temp), live, ".sort.tmp") ||
       !dnd_fs_recover_sort(storage, live, NULL) ||
       (snapshot && !dnd_fs_recover_sort(storage, snapshot, NULL)))
        return false;
    /* Uncommitted work has no authority; never prefer it to the recovered live. */
    if(!dnd_sort_remove(storage, indices[0]) || !dnd_sort_remove(storage, indices[1]) ||
       !dnd_sort_remove(storage, temp))
        return false;
    FileInfo info;
    FS_Error live_error = storage_common_stat(storage, live, &info);
    if(live_error == FSE_NOT_EXIST) return true;
    if(live_error != FSE_OK || file_info_is_dir(&info)) return false;
    DndSortWork* work = calloc(1U, sizeof(*work));
    if(!work) return false;
    File* source = dnd_sort_open(storage, live, false);
    File* left = NULL;
    File* right = NULL;
    File* output = NULL;
    bool ordered = false;
    bool ok = source && storage_file_size(source) <= UINT32_MAX &&
              dnd_sort_scan(work, source, NULL, &ordered);
    if(!ok || ordered) goto finished;
    const DndSortKey* single = NULL;
    for(uint8_t candidate = 0U; candidate < 2U; ++candidate) {
        if(!dnd_sort_scan(work, source, &work->candidates[candidate], &ordered)) {
            ok = false;
            goto finished;
        }
        if(ordered) {
            single = &work->candidates[candidate];
            break;
        }
    }
    output = dnd_sort_open(storage, indices[0], true);
    ok = output && dnd_sort_make_index(work, source, output, single);
    if(!dnd_sort_close(output)) ok = false;
    output = NULL;
    if(!ok) goto finished;
    uint8_t current = 0U;
    if(single) {
        work->stats.single_record_reinserted = true;
    } else {
        for(uint32_t run = DND_SORT_BATCH; run < work->stats.records; run *= 2U) {
            left = dnd_sort_open(storage, indices[current], false);
            right = dnd_sort_open(storage, indices[current], false);
            output = dnd_sort_open(storage, indices[1U - current], true);
            ok = left && right && output && dnd_sort_merge(work, left, right, output, run);
            if(!dnd_sort_close(left)) ok = false;
            if(!dnd_sort_close(right)) ok = false;
            if(!dnd_sort_close(output)) ok = false;
            left = right = output = NULL;
            if(!ok) goto finished;
            current = 1U - current;
            ++work->stats.merge_passes;
        }
    }
    left = dnd_sort_open(storage, live, false);
    right = dnd_sort_open(storage, indices[current], false);
    output = dnd_sort_open(storage, temp, true);
    ok = left && right && output && dnd_sort_emit(work, source, left, right, output);
    if(!dnd_sort_close(left)) ok = false;
    if(!dnd_sort_close(right)) ok = false;
    if(!dnd_sort_close(output)) ok = false;
    if(!dnd_sort_close(source)) ok = false;
    left = right = output = source = NULL;
    if(ok && snapshot) ok = dnd_sort_snapshot(work, storage, temp, snapshot);
    if(ok) ok = dnd_sort_publish(storage, temp, live);
    if(ok) work->stats.changed = true;
finished:
    dnd_sort_close(source);
    dnd_sort_close(left);
    dnd_sort_close(right);
    dnd_sort_close(output);
    (void)dnd_sort_remove(storage, indices[0]);
    (void)dnd_sort_remove(storage, indices[1]);
    (void)dnd_sort_remove(storage, temp);
    if(stats) *stats = work->stats;
    free(work);
    return ok;
}
