#include "dndolphins_progression_store.h"
#include "dnd_profile_handoff.h"

#include <furi.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DND_PROGRESS_PATH_LEN    96U
#define DND_PROGRESS_LINE_LEN    768U
#define DND_PROGRESS_READ_BUFFER 256U

#define DND_FEATURES_HEADER "DNDFeatures=1\n"
#define DND_APPLIED_HEADER  "DNDAppliedGrants=1\n"

typedef struct {
    File* file;
    uint8_t buffer[DND_PROGRESS_READ_BUFFER];
    uint16_t position;
    uint16_t count;
    uint32_t raw_offset;
} DndProgressReader;

void dndolphins_progression_store_feature_path(char* out, size_t size, uint32_t profile) {
    snprintf(out, size, "%s/feats_%lu.txt", DND_CHARACTER_DATA_ROOT, (unsigned long)profile);
}

void dndolphins_progression_store_applied_path(char* out, size_t size, uint32_t profile) {
    snprintf(
        out, size, "%s/appliedgrants_%lu.txt", DND_CHARACTER_DATA_ROOT, (unsigned long)profile);
}

static void dndolphins_progression_store_work_path(
    char* out,
    size_t size,
    uint32_t profile,
    const char* kind,
    const char* suffix) {
    snprintf(
        out, size, "%s/%s_%lu.%s", DND_CHARACTER_DATA_ROOT, kind, (unsigned long)profile, suffix);
}

static bool dndolphins_progression_store_write_raw(File* file, const char* text) {
    size_t length = strlen(text);
    return storage_file_write(file, text, length) == length;
}

static void dndolphins_progression_store_reader_init_at(
    DndProgressReader* reader,
    File* file,
    uint32_t raw_offset) {
    memset(reader, 0, sizeof(*reader));
    reader->file = file;
    reader->raw_offset = raw_offset;
}

static void dndolphins_progression_store_reader_init(DndProgressReader* reader, File* file) {
    dndolphins_progression_store_reader_init_at(reader, file, 0U);
}

static bool dndolphins_progression_store_reader_byte(DndProgressReader* reader, char* value) {
    if(reader->position >= reader->count) {
        reader->count =
            (uint16_t)storage_file_read(reader->file, reader->buffer, sizeof(reader->buffer));
        reader->position = 0U;
        if(!reader->count) return false;
    }
    *value = (char)reader->buffer[reader->position++];
    if(reader->raw_offset != UINT32_MAX) ++reader->raw_offset;
    return true;
}

static bool
    dndolphins_progression_store_read_line(DndProgressReader* reader, char* line, size_t size) {
    size_t used = 0U;
    char value = '\0';
    bool saw = false;
    while(dndolphins_progression_store_reader_byte(reader, &value)) {
        saw = true;
        if(value == '\n') break;
        if(value == '\r') continue;
        if(used + 1U < size)
            line[used++] = value;
        else {
            /* Oversize feature lines are treated as unreadable rather than
               truncated and rewritten. */
            while(value != '\n' && dndolphins_progression_store_reader_byte(reader, &value)) {
            }
            line[0] = '\0';
            return false;
        }
    }
    line[used] = '\0';
    return saw;
}

static bool dndolphins_progression_store_publish(
    Storage* storage,
    const char* temp,
    const char* live,
    const char* backup) {
    bool had_live = storage_file_exists(storage, live);
    storage_common_remove(storage, backup);
    if(had_live && storage_common_rename(storage, live, backup) != FSE_OK) {
        storage_common_remove(storage, temp);
        return false;
    }
    if(storage_common_rename(storage, temp, live) == FSE_OK) {
        if(had_live) storage_common_remove(storage, backup);
        return true;
    }
    if(had_live) storage_common_rename(storage, backup, live);
    storage_common_remove(storage, temp);
    return false;
}

static bool dndolphins_progression_store_encode(char* out, size_t size, const char* value) {
    static const char hex[] = "0123456789ABCDEF";
    size_t used = 0U;
    if(!out || !size || !value) return false;
    for(size_t i = 0U; value[i]; ++i) {
        uint8_t c = (uint8_t)value[i];
        bool escape = c == '%' || c == '|' || c == '\n' || c == '\r' || c < 0x20U;
        size_t need = escape ? 3U : 1U;
        if(used + need + 1U > size) return false;
        if(escape) {
            out[used++] = '%';
            out[used++] = hex[c >> 4U];
            out[used++] = hex[c & 0x0FU];
        } else {
            out[used++] = (char)c;
        }
    }
    out[used] = '\0';
    return true;
}

static uint8_t dndolphins_progression_store_hex(char c) {
    if(c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if(c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10U);
    if(c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10U);
    return 0xFFU;
}

static void dndolphins_progression_store_decode(char* out, size_t size, const char* value) {
    size_t used = 0U;
    if(!out || !size) return;
    for(size_t i = 0U; value && value[i] && used + 1U < size; ++i) {
        if(value[i] == '%' && value[i + 1U] && value[i + 2U]) {
            uint8_t hi = dndolphins_progression_store_hex(value[i + 1U]);
            uint8_t lo = dndolphins_progression_store_hex(value[i + 2U]);
            if(hi != 0xFFU && lo != 0xFFU) {
                out[used++] = (char)((hi << 4U) | lo);
                i += 2U;
                continue;
            }
        }
        out[used++] = value[i];
    }
    out[used] = '\0';
}

static bool dndolphins_progression_store_write_feature(File* file, const DndFeature* feature) {
    /* Stream the two escaped strings instead of assembling an ~700-byte record
       plus two encoded copies on the stack. */
    char encoded[(DND_DETAIL_LEN * 3U) + 1U];
    if(!dndolphins_progression_store_write_raw(file, "F|")) return false;
    if(!dndolphins_progression_store_encode(encoded, sizeof(encoded), feature->name) ||
       !dndolphins_progression_store_write_raw(file, encoded) ||
       !dndolphins_progression_store_write_raw(file, "|"))
        return false;
    if(!dndolphins_progression_store_encode(encoded, sizeof(encoded), feature->detail) ||
       !dndolphins_progression_store_write_raw(file, encoded))
        return false;
    char tail[96];
    int length = snprintf(
        tail,
        sizeof(tail),
        "|%d|%d|%u|%u|%u|%u|%u\n",
        feature->uses_current,
        feature->uses_max,
        feature->class_index,
        feature->class_level_gained,
        feature->recharge,
        feature->resource_formula,
        feature->resource_ability);
    return length > 0 && (size_t)length < sizeof(tail) &&
           storage_file_write(file, tail, (size_t)length) == (size_t)length;
}

static bool dndolphins_progression_store_parse_i32(
    const char* text,
    int32_t minimum,
    int32_t maximum,
    int32_t* out) {
    if(!text || !*text || !out) return false;
    char* end = NULL;
    long value = strtol(text, &end, 10);
    if(!end || *end || value < minimum || value > maximum) return false;
    *out = (int32_t)value;
    return true;
}

static bool dndolphins_progression_store_parse_feature(char* line, DndFeature* feature) {
    if(!line || !feature || strncmp(line, "F|", 2U) != 0) return false;
    char* fields[9];
    char* cursor = line + 2U;
    for(uint8_t i = 0U; i < 9U; ++i) {
        fields[i] = cursor;
        if(i == 8U) break;
        char* separator = strchr(cursor, '|');
        if(!separator) return false;
        *separator = '\0';
        cursor = separator + 1U;
    }
    int32_t values[7];
    if(!dndolphins_progression_store_parse_i32(fields[2], INT16_MIN, INT16_MAX, &values[0]) ||
       !dndolphins_progression_store_parse_i32(fields[3], INT16_MIN, INT16_MAX, &values[1]) ||
       !dndolphins_progression_store_parse_i32(fields[4], 0, DND_MAX_CLASSES - 1U, &values[2]) ||
       !dndolphins_progression_store_parse_i32(fields[5], 0, 20, &values[3]) ||
       !dndolphins_progression_store_parse_i32(fields[6], 0, DndRechargeCount - 1U, &values[4]) ||
       !dndolphins_progression_store_parse_i32(
           fields[7], 0, DndResourceFormulaCount - 1U, &values[5]) ||
       !dndolphins_progression_store_parse_i32(fields[8], 0, DndAbilityCharisma, &values[6]))
        return false;
    memset(feature, 0, sizeof(*feature));
    dndolphins_progression_store_decode(feature->name, sizeof(feature->name), fields[0]);
    dndolphins_progression_store_decode(feature->detail, sizeof(feature->detail), fields[1]);
    feature->uses_current = (int16_t)values[0];
    feature->uses_max = (int16_t)values[1];
    feature->class_index = (uint8_t)values[2];
    feature->class_level_gained = (uint8_t)values[3];
    feature->recharge = (uint8_t)values[4];
    feature->resource_formula = (uint8_t)values[5];
    feature->resource_ability = (uint8_t)values[6];
    return true;
}

static bool dndolphins_progression_store_open_rewrite(
    Storage* storage,
    uint32_t profile,
    const char* kind,
    const char* live,
    File** input_out,
    File** output_out,
    char* temp,
    size_t temp_size,
    char* backup,
    size_t backup_size) {
    dndolphins_progression_store_work_path(temp, temp_size, profile, kind, "tmp");
    dndolphins_progression_store_work_path(backup, backup_size, profile, kind, "bak");
    storage_common_remove(storage, temp);
    File* input = storage_file_alloc(storage);
    File* output = storage_file_alloc(storage);
    if(!input || !output) {
        if(input) storage_file_free(input);
        if(output) storage_file_free(output);
        return false;
    }
    bool ok = storage_file_open(input, live, FSAM_READ, FSOM_OPEN_EXISTING) &&
              storage_file_open(output, temp, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(!ok) {
        storage_file_close(input);
        storage_file_close(output);
        storage_file_free(input);
        storage_file_free(output);
        storage_common_remove(storage, temp);
        return false;
    }
    *input_out = input;
    *output_out = output;
    return true;
}

bool dndolphins_progression_store_features_exist(Storage* storage, uint32_t profile) {
    char path[DND_PROGRESS_PATH_LEN];
    dndolphins_progression_store_feature_path(path, sizeof(path), profile);
    return storage && storage_file_exists(storage, path);
}

static bool dndolphins_progression_store_features_create(
    Storage* storage,
    uint32_t profile,
    const DndFeature* features,
    uint8_t count) {
    if(!storage || (count && !features)) return false;
    storage_common_mkdir(storage, DND_CHARACTER_DATA_ROOT);
    char path[DND_PROGRESS_PATH_LEN];
    dndolphins_progression_store_feature_path(path, sizeof(path), profile);
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    bool opened = storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    bool ok = opened && dndolphins_progression_store_write_raw(file, DND_FEATURES_HEADER);
    for(uint8_t i = 0U; ok && i < count; ++i)
        ok = dndolphins_progression_store_write_feature(file, &features[i]);
    if(ok) ok = storage_file_sync(file);
    storage_file_close(file);
    storage_file_free(file);
    if(!ok) {
        /* The caller reaches this helper only when the Feature sidecar did not
           exist. Remove a partial file created by this failed attempt, but do
           not remove a pre-existing non-file path that blocked open(). */
        if(opened) (void)storage_common_remove(storage, path);
        return false;
    }
    return storage_file_exists(storage, path);
}

bool dndolphins_progression_store_features_count(
    Storage* storage,
    uint32_t profile,
    uint16_t* total_count) {
    if(!storage || !total_count) return false;
    *total_count = 0U;
    char path[DND_PROGRESS_PATH_LEN];
    dndolphins_progression_store_feature_path(path, sizeof(path), profile);
    if(!storage_file_exists(storage, path)) return true;
    File* file = storage_file_alloc(storage);
    if(!file || !storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(file) storage_file_free(file);
        return false;
    }
    DndProgressReader reader;
    dndolphins_progression_store_reader_init(&reader, file);
    char line[DND_PROGRESS_LINE_LEN];
    uint16_t count = 0U;
    bool ok = true;
    while(dndolphins_progression_store_read_line(&reader, line, sizeof(line))) {
        if(!line[0] || line[0] == '#' || strncmp(line, "DNDFeatures=", 12U) == 0) continue;
        DndFeature feature;
        if(!dndolphins_progression_store_parse_feature(line, &feature)) {
            ok = false;
            break;
        }
        if(count < UINT16_MAX) ++count;
    }
    storage_file_close(file);
    storage_file_free(file);
    if(ok) *total_count = count;
    return ok;
}

bool dndolphins_progression_store_features_find_name(
    Storage* storage,
    uint32_t profile,
    const char* name,
    DndFeature* feature_out,
    bool* found) {
    if(found) *found = false;
    if(feature_out) memset(feature_out, 0, sizeof(*feature_out));
    if(!storage || !name || !name[0] || !found) return false;
    char path[DND_PROGRESS_PATH_LEN];
    dndolphins_progression_store_feature_path(path, sizeof(path), profile);
    if(!storage_file_exists(storage, path)) return true;
    File* file = storage_file_alloc(storage);
    if(!file || !storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(file) storage_file_free(file);
        return false;
    }
    DndProgressReader reader;
    dndolphins_progression_store_reader_init(&reader, file);
    char line[DND_PROGRESS_LINE_LEN];
    bool ok = true;
    while(dndolphins_progression_store_read_line(&reader, line, sizeof(line))) {
        if(!line[0] || line[0] == '#' || strncmp(line, "DNDFeatures=", 12U) == 0) continue;
        DndFeature feature;
        if(!dndolphins_progression_store_parse_feature(line, &feature)) {
            ok = false;
            break;
        }
        if(strcmp(feature.name, name) == 0) {
            *found = true;
            if(feature_out) *feature_out = feature;
            break;
        }
    }
    if(storage_file_get_error(file) != FSE_OK) ok = false;
    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

bool dndolphins_progression_store_features_contains_name(
    Storage* storage,
    uint32_t profile,
    const char* name,
    bool* found) {
    return dndolphins_progression_store_features_find_name(storage, profile, name, NULL, found);
}

static bool dndolphins_progression_store_features_load_window_internal(
    Storage* storage,
    uint32_t profile,
    uint16_t start,
    DndCharacter* character,
    uint16_t* total_count,
    uint32_t page_offsets[DND_PROGRESS_PAGE_COUNT],
    uint8_t* valid_pages) {
    if(!storage || !character || !total_count) return false;
    free(character->features);
    character->features = NULL;
    character->feature_count = 0U;
    character->feature_capacity = 0U;

    const uint16_t page_index = start / DND_PROGRESS_CACHE_SIZE;
    const bool indexed = page_offsets && valid_pages;
    const bool direct = indexed && page_index < DND_PROGRESS_PAGE_COUNT &&
                        *valid_pages > page_index && *total_count >= start;
    const uint16_t known_total = *total_count;
    if(!direct) {
        *total_count = 0U;
        if(indexed) {
            memset(page_offsets, 0, sizeof(uint32_t) * DND_PROGRESS_PAGE_COUNT);
            *valid_pages = 0U;
        }
    }

    char path[DND_PROGRESS_PATH_LEN];
    dndolphins_progression_store_feature_path(path, sizeof(path), profile);
    if(!storage_file_exists(storage, path)) return true;
    File* file = storage_file_alloc(storage);
    if(!file || !storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(file) storage_file_free(file);
        return false;
    }

    uint32_t initial_offset = 0U;
    if(direct) {
        initial_offset = page_offsets[page_index];
        if(initial_offset && !storage_file_seek(file, initial_offset, true)) {
            storage_file_close(file);
            storage_file_free(file);
            return false;
        }
    }

    DndFeature* page = NULL;
    uint8_t page_count = 0U;
    uint16_t logical = direct ? start : 0U;
    const uint16_t window_end = start + DND_PROGRESS_CACHE_SIZE;
    char line[DND_PROGRESS_LINE_LEN];
    DndProgressReader reader;
    dndolphins_progression_store_reader_init_at(&reader, file, initial_offset);
    bool ok = true;
    while(logical < UINT16_MAX) {
        uint32_t line_offset = reader.raw_offset;
        if(!dndolphins_progression_store_read_line(&reader, line, sizeof(line))) break;
        if(!line[0] || line[0] == '#' || strncmp(line, "DNDFeatures=", 12U) == 0) continue;
        DndFeature feature;
        if(!dndolphins_progression_store_parse_feature(line, &feature)) {
            ok = false;
            break;
        }
        if(!direct && indexed && (logical % DND_PROGRESS_CACHE_SIZE) == 0U) {
            uint16_t discovered_page = logical / DND_PROGRESS_CACHE_SIZE;
            if(discovered_page < DND_PROGRESS_PAGE_COUNT) {
                page_offsets[discovered_page] = line_offset;
                if(*valid_pages <= discovered_page) *valid_pages = (uint8_t)(discovered_page + 1U);
            }
        }
        if(logical >= start && logical < window_end) {
            if(!page) {
                page = malloc(DND_PROGRESS_CACHE_SIZE * sizeof(DndFeature));
                if(!page) {
                    ok = false;
                    break;
                }
            }
            page[page_count++] = feature;
        }
        ++logical;
        if(direct && logical >= window_end) break;
    }
    if(storage_file_get_error(file) != FSE_OK) ok = false;
    storage_file_close(file);
    storage_file_free(file);
    if(!ok) {
        free(page);
        return false;
    }
    *total_count = direct ? known_total : logical;
    if(page_count) {
        character->features = page;
        character->feature_count = page_count;
        character->feature_capacity = DND_PROGRESS_CACHE_SIZE;
    } else {
        free(page);
    }
    return true;
}

bool dndolphins_progression_store_features_load_window(
    Storage* storage,
    uint32_t profile,
    uint16_t start,
    DndCharacter* character,
    uint16_t* total_count) {
    return dndolphins_progression_store_features_load_window_internal(
        storage, profile, start, character, total_count, NULL, NULL);
}

bool dndolphins_progression_store_features_load_window_indexed(
    Storage* storage,
    uint32_t profile,
    uint16_t start,
    DndCharacter* character,
    uint16_t* total_count,
    uint32_t page_offsets[DND_PROGRESS_PAGE_COUNT],
    uint8_t* valid_pages) {
    return dndolphins_progression_store_features_load_window_internal(
        storage, profile, start, character, total_count, page_offsets, valid_pages);
}

static bool dndolphins_progression_store_features_rewrite_window(
    Storage* storage,
    uint32_t profile,
    uint16_t start,
    const DndCharacter* replacement,
    int32_t delete_index,
    const DndFeature* append) {
    char live[DND_PROGRESS_PATH_LEN], temp[DND_PROGRESS_PATH_LEN], backup[DND_PROGRESS_PATH_LEN];
    dndolphins_progression_store_feature_path(live, sizeof(live), profile);
    if(!storage_file_exists(storage, live)) return false;
    File* input = NULL;
    File* output = NULL;
    if(!dndolphins_progression_store_open_rewrite(
           storage,
           profile,
           "feats",
           live,
           &input,
           &output,
           temp,
           sizeof(temp),
           backup,
           sizeof(backup)))
        return false;
    bool ok = dndolphins_progression_store_write_raw(output, DND_FEATURES_HEADER);
    DndProgressReader reader;
    dndolphins_progression_store_reader_init(&reader, input);
    char line[DND_PROGRESS_LINE_LEN];
    uint16_t logical = 0U;
    uint8_t replacement_written = 0U;
    while(ok && dndolphins_progression_store_read_line(&reader, line, sizeof(line))) {
        if(!line[0] || line[0] == '#' || strncmp(line, "DNDFeatures=", 12U) == 0) continue;
        DndFeature parsed;
        if(!dndolphins_progression_store_parse_feature(line, &parsed)) {
            ok = false;
            break;
        }
        if(delete_index >= 0 && logical == (uint16_t)delete_index) {
            ++logical;
            continue;
        }
        if(replacement && logical >= start &&
           logical < (uint16_t)(start + replacement->feature_count)) {
            uint8_t local = (uint8_t)(logical - start);
            ok = dndolphins_progression_store_write_feature(output, &replacement->features[local]);
            ++replacement_written;
        } else {
            ok = dndolphins_progression_store_write_feature(output, &parsed);
        }
        ++logical;
    }
    if(ok && replacement) {
        while(replacement_written < replacement->feature_count) {
            ok = dndolphins_progression_store_write_feature(
                output, &replacement->features[replacement_written++]);
            if(!ok) break;
        }
    }
    if(ok && append) ok = dndolphins_progression_store_write_feature(output, append);
    if(ok) ok = storage_file_sync(output);
    storage_file_close(input);
    storage_file_close(output);
    storage_file_free(input);
    storage_file_free(output);
    if(!ok) {
        storage_common_remove(storage, temp);
        return false;
    }
    return dndolphins_progression_store_publish(storage, temp, live, backup);
}

bool dndolphins_progression_store_features_save_window(
    Storage* storage,
    uint32_t profile,
    uint16_t start,
    const DndCharacter* character) {
    if(!storage || !character || (character->feature_count && !character->features)) return false;
    char path[DND_PROGRESS_PATH_LEN];
    dndolphins_progression_store_feature_path(path, sizeof(path), profile);
    if(!storage_file_exists(storage, path)) {
        if(start != 0U) return false;
        return dndolphins_progression_store_features_create(
            storage, profile, character->features, character->feature_count);
    }
    return dndolphins_progression_store_features_rewrite_window(
        storage, profile, start, character, -1, NULL);
}

bool dndolphins_progression_store_features_append(
    Storage* storage,
    uint32_t profile,
    const DndFeature* feature) {
    if(!storage || !feature) return false;
    char path[DND_PROGRESS_PATH_LEN];
    dndolphins_progression_store_feature_path(path, sizeof(path), profile);
    if(!storage_file_exists(storage, path))
        return dndolphins_progression_store_features_create(storage, profile, feature, 1U);
    uint16_t total = 0U;
    if(!dndolphins_progression_store_features_count(storage, profile, &total)) return false;
    return dndolphins_progression_store_features_rewrite_window(
        storage, profile, 0U, NULL, -1, feature);
}

bool dndolphins_progression_store_features_delete(
    Storage* storage,
    uint32_t profile,
    uint16_t logical_index) {
    if(!storage) return false;
    char path[DND_PROGRESS_PATH_LEN];
    dndolphins_progression_store_feature_path(path, sizeof(path), profile);
    if(!storage_file_exists(storage, path)) return false;
    return dndolphins_progression_store_features_rewrite_window(
        storage, profile, 0U, NULL, logical_index, NULL);
}

static int16_t dndolphins_progression_store_feature_max_uses(
    const DndCharacter* character,
    const DndFeature* feature) {
    if(!character) return feature->uses_max;
    if(feature->resource_formula == DndResourceProficiency) {
        uint8_t level = 0U;
        for(uint8_t i = 0U; i < character->class_count && i < DND_MAX_CLASSES; ++i)
            level += character->classes[i].level;
        if(level < 1U) level = 1U;
        return (int16_t)(2U + ((level - 1U) / 4U));
    }
    if(feature->resource_formula == DndResourceAbility &&
       feature->resource_ability < DND_ABILITY_COUNT) {
        int16_t delta = (int16_t)character->ability_scores[feature->resource_ability] - 10;
        int16_t modifier = delta >= 0 ? delta / 2 : -(((-delta) + 1) / 2);
        return modifier > 0 ? modifier : 1;
    }
    return feature->uses_max;
}

static bool
    dndolphins_progression_store_recharge_matches(uint8_t recharge, DndFeatureRechargeEvent event) {
    switch(event) {
    case DndFeatureRechargeTurn:
        return recharge == DndRechargeTurn;
    case DndFeatureRechargeEncounter:
        return recharge == DndRechargeEncounter;
    case DndFeatureRechargeShortRest:
        return recharge == DndRechargeShortOrLong;
    case DndFeatureRechargeLongRest:
        return recharge != DndRechargeManual && recharge != DndRechargeTurn &&
               recharge != DndRechargeEncounter;
    default:
        return false;
    }
}

bool dndolphins_progression_store_features_recharge(
    Storage* storage,
    uint32_t profile,
    const DndCharacter* character,
    DndFeatureRechargeEvent event) {
    if(!storage) return false;
    char live[DND_PROGRESS_PATH_LEN], temp[DND_PROGRESS_PATH_LEN], backup[DND_PROGRESS_PATH_LEN];
    dndolphins_progression_store_feature_path(live, sizeof(live), profile);
    if(!storage_file_exists(storage, live)) return true;
    File* input = NULL;
    File* output = NULL;
    if(!dndolphins_progression_store_open_rewrite(
           storage,
           profile,
           "feats",
           live,
           &input,
           &output,
           temp,
           sizeof(temp),
           backup,
           sizeof(backup)))
        return false;
    bool ok = dndolphins_progression_store_write_raw(output, DND_FEATURES_HEADER);
    DndProgressReader reader;
    dndolphins_progression_store_reader_init(&reader, input);
    char line[DND_PROGRESS_LINE_LEN];
    while(ok && dndolphins_progression_store_read_line(&reader, line, sizeof(line))) {
        if(!line[0] || line[0] == '#' || strncmp(line, "DNDFeatures=", 12U) == 0) continue;
        DndFeature feature;
        if(!dndolphins_progression_store_parse_feature(line, &feature)) {
            ok = false;
            break;
        }
        if(dndolphins_progression_store_recharge_matches(feature.recharge, event))
            feature.uses_current =
                dndolphins_progression_store_feature_max_uses(character, &feature);
        ok = dndolphins_progression_store_write_feature(output, &feature);
    }
    if(ok) ok = storage_file_sync(output);
    storage_file_close(input);
    storage_file_close(output);
    storage_file_free(input);
    storage_file_free(output);
    if(!ok) {
        storage_common_remove(storage, temp);
        return false;
    }
    return dndolphins_progression_store_publish(storage, temp, live, backup);
}

bool dndolphins_progression_store_features_remap_classes(
    Storage* storage,
    uint32_t profile,
    uint8_t removed_class) {
    if(!storage) return false;
    char live[DND_PROGRESS_PATH_LEN], temp[DND_PROGRESS_PATH_LEN], backup[DND_PROGRESS_PATH_LEN];
    dndolphins_progression_store_feature_path(live, sizeof(live), profile);
    if(!storage_file_exists(storage, live)) return true;
    File* input = NULL;
    File* output = NULL;
    if(!dndolphins_progression_store_open_rewrite(
           storage,
           profile,
           "feats",
           live,
           &input,
           &output,
           temp,
           sizeof(temp),
           backup,
           sizeof(backup)))
        return false;
    bool ok = dndolphins_progression_store_write_raw(output, DND_FEATURES_HEADER);
    DndProgressReader reader;
    dndolphins_progression_store_reader_init(&reader, input);
    char line[DND_PROGRESS_LINE_LEN];
    while(ok && dndolphins_progression_store_read_line(&reader, line, sizeof(line))) {
        if(!line[0] || line[0] == '#' || strncmp(line, "DNDFeatures=", 12U) == 0) continue;
        DndFeature feature;
        if(!dndolphins_progression_store_parse_feature(line, &feature)) {
            ok = false;
            break;
        }
        if(feature.class_index == removed_class)
            feature.class_index = 0U;
        else if(feature.class_index > removed_class)
            --feature.class_index;
        ok = dndolphins_progression_store_write_feature(output, &feature);
    }
    if(ok) ok = storage_file_sync(output);
    storage_file_close(input);
    storage_file_close(output);
    storage_file_free(input);
    storage_file_free(output);
    if(!ok) {
        storage_common_remove(storage, temp);
        return false;
    }
    return dndolphins_progression_store_publish(storage, temp, live, backup);
}

bool dndolphins_progression_store_applied_exists(
    Storage* storage,
    uint32_t profile,
    const char* stable_id) {
    if(!storage || !stable_id || !stable_id[0]) return false;
    char path[DND_PROGRESS_PATH_LEN];
    dndolphins_progression_store_applied_path(path, sizeof(path), profile);
    if(!storage_file_exists(storage, path)) return false;
    File* file = storage_file_alloc(storage);
    if(!file || !storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(file) storage_file_free(file);
        return false;
    }
    DndProgressReader reader;
    dndolphins_progression_store_reader_init(&reader, file);
    char line[DND_SHORT_LEN * 3U + 8U];
    bool found = false;
    while(dndolphins_progression_store_read_line(&reader, line, sizeof(line))) {
        if(!line[0] || line[0] == '#' || strncmp(line, "DNDAppliedGrants=", 17U) == 0) continue;
        char decoded[DND_SHORT_LEN];
        dndolphins_progression_store_decode(decoded, sizeof(decoded), line);
        if(strcmp(decoded, stable_id) == 0) {
            found = true;
            break;
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    return found;
}

bool dndolphins_progression_store_mark_applied(
    Storage* storage,
    uint32_t profile,
    const char* stable_id) {
    if(!storage || !stable_id || !stable_id[0]) return false;
    char live[DND_PROGRESS_PATH_LEN];
    dndolphins_progression_store_applied_path(live, sizeof(live), profile);
    storage_common_mkdir(storage, DND_CHARACTER_DATA_ROOT);

    char encoded[DND_SHORT_LEN * 3U + 1U];
    if(!dndolphins_progression_store_encode(encoded, sizeof(encoded), stable_id)) return false;

    if(!storage_file_exists(storage, live)) {
        File* file = storage_file_alloc(storage);
        if(!file) return false;
        bool ok = storage_file_open(file, live, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
                  dndolphins_progression_store_write_raw(file, DND_APPLIED_HEADER) &&
                  dndolphins_progression_store_write_raw(file, encoded) &&
                  dndolphins_progression_store_write_raw(file, "\n") && storage_file_sync(file);
        storage_file_close(file);
        storage_file_free(file);
        return ok && storage_file_exists(storage, live);
    }

    if(dndolphins_progression_store_applied_exists(storage, profile, stable_id)) return true;

    /* Applied-grant IDs are an audit/choice-completion sidecar; the actual
       character, Feature and Spellbook writes are authoritative. Appending one
       encoded line avoids rewriting the increasingly large marker file once for
       every accepted grant. A torn marker can at worst make that choice appear
       reviewable again; it cannot roll back the authoritative grant itself. */
    File* file = storage_file_alloc(storage);
    if(!file || !storage_file_open(file, live, FSAM_WRITE, FSOM_OPEN_EXISTING)) {
        if(file) storage_file_free(file);
        return false;
    }
    uint64_t end = storage_file_size(file);
    bool ok = end <= UINT32_MAX && storage_file_seek(file, (uint32_t)end, true) &&
              dndolphins_progression_store_write_raw(file, encoded) &&
              dndolphins_progression_store_write_raw(file, "\n") && storage_file_sync(file);
    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

bool dndolphins_progression_store_delete_sidecars(Storage* storage, uint32_t profile) {
    if(!storage) return false;
    char path[DND_PROGRESS_PATH_LEN];
    dndolphins_progression_store_feature_path(path, sizeof(path), profile);
    bool ok = !storage_file_exists(storage, path) ||
              storage_common_remove(storage, path) == FSE_OK;
    dndolphins_progression_store_applied_path(path, sizeof(path), profile);
    return (!storage_file_exists(storage, path) ||
            storage_common_remove(storage, path) == FSE_OK) &&
           ok;
}

static bool dndolphins_progression_store_copy_one(
    Storage* storage,
    const char* source,
    const char* destination) {
    if(!storage_file_exists(storage, source)) return true;

    char temporary[DND_PROGRESS_PATH_LEN];
    char backup[DND_PROGRESS_PATH_LEN];
    int tn = snprintf(temporary, sizeof(temporary), "%s.ptmp", destination);
    int bn = snprintf(backup, sizeof(backup), "%s.pbak", destination);
    if(tn <= 0 || bn <= 0 || (size_t)tn >= sizeof(temporary) || (size_t)bn >= sizeof(backup))
        return false;
    storage_common_remove(storage, temporary);

    File* input = storage_file_alloc(storage);
    File* output = storage_file_alloc(storage);
    if(!input || !output) {
        if(input) storage_file_free(input);
        if(output) storage_file_free(output);
        return false;
    }
    bool ok = storage_file_open(input, source, FSAM_READ, FSOM_OPEN_EXISTING) &&
              storage_file_open(output, temporary, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    uint8_t buffer[256];
    while(ok) {
        size_t count = storage_file_read(input, buffer, sizeof(buffer));
        if(!count) break;
        ok = storage_file_write(output, buffer, count) == count;
    }
    if(ok) ok = storage_file_get_error(input) == FSE_OK;
    if(ok) ok = storage_file_sync(output);
    storage_file_close(input);
    storage_file_close(output);
    storage_file_free(input);
    storage_file_free(output);
    if(!ok) {
        storage_common_remove(storage, temporary);
        return false;
    }
    return dndolphins_progression_store_publish(storage, temporary, destination, backup);
}

bool dndolphins_progression_store_copy_sidecars(
    Storage* storage,
    uint32_t source_profile,
    uint32_t destination_profile) {
    if(!storage) return false;
    char source[DND_PROGRESS_PATH_LEN], destination[DND_PROGRESS_PATH_LEN];
    dndolphins_progression_store_feature_path(source, sizeof(source), source_profile);
    dndolphins_progression_store_feature_path(
        destination, sizeof(destination), destination_profile);
    if(!dndolphins_progression_store_copy_one(storage, source, destination)) return false;
    dndolphins_progression_store_applied_path(source, sizeof(source), source_profile);
    dndolphins_progression_store_applied_path(
        destination, sizeof(destination), destination_profile);
    return dndolphins_progression_store_copy_one(storage, source, destination);
}
