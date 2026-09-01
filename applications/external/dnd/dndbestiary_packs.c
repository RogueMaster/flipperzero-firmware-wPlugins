#include "dndbestiary_packs.h"
#include "dnd_fs.h"

#include <furi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PACK_VERSION     1U
#define PACK_MAX_RECORDS 16U
#define PACK_LINE_LEN    768U
#define PACK_READ_BUFFER 256U

#define MONSTER_REGISTRY               APP_DATA_PATH("packs/monster_registry.txt")
#define MONSTER_REGISTRY_TEMP          APP_DATA_PATH("packs/monster_registry.tmp")
#define MONSTER_REGISTRY_BACKUP        APP_DATA_PATH("packs/monster_registry.bak")
#define MONSTER_INBOX_MANIFEST         APP_DATA_PATH("packs/monster_inbox/manifest.txt")
#define MONSTER_INBOX_INDEX            APP_DATA_PATH("packs/monster_inbox/index.txt")
#define MONSTER_INBOX_CONTENT          APP_DATA_PATH("packs/monster_inbox/statblocks.txt")
#define MONSTER_ENABLED_INDEX          APP_DATA_PATH("monsters/enabled_index.txt")
#define MONSTER_ENABLED_INDEX_TEMP     APP_DATA_PATH("monsters/enabled_index.tmp")
#define MONSTER_ENABLED_INDEX_BACKUP   APP_DATA_PATH("monsters/enabled_index.bak")
#define MONSTER_ENABLED_CONTENT        APP_DATA_PATH("monsters/enabled_statblocks.txt")
#define MONSTER_ENABLED_CONTENT_TEMP   APP_DATA_PATH("monsters/enabled_statblocks.tmp")
#define MONSTER_ENABLED_CONTENT_BACKUP APP_DATA_PATH("monsters/enabled_statblocks.bak")
#define MONSTER_PACKAGED_INDEX         APP_ASSETS_PATH("monsters/index.txt")
#define MONSTER_CUSTOM_INDEX           APP_DATA_PATH("monsters/custom_index.txt")

typedef struct {
    File* file;
    uint8_t buffer[PACK_READ_BUFFER];
    uint16_t position;
    uint16_t count;
} PackReader;

typedef struct {
    PocketPackSummary summary;
} PackRecord;

typedef struct {
    char id[POCKET_PACK_ID_LEN];
    char name[POCKET_PACK_NAME_LEN];
} PackManifest;

static PackRecord* dndbestiary_packs_records_alloc(void) {
    return calloc(PACK_MAX_RECORDS, sizeof(PackRecord));
}

static void dndbestiary_packs_copy(char* output, size_t size, const char* value) {
    if(!size) return;
    strncpy(output, value ? value : "", size - 1U);
    output[size - 1U] = '\0';
}

static bool dndbestiary_packs_parse_u32(const char* text, uint32_t maximum, uint32_t* output) {
    if(!text || !text[0] || !output) return false;
    uint32_t value = 0U;
    for(const char* cursor = text; *cursor; ++cursor) {
        if(*cursor < '0' || *cursor > '9') return false;
        uint32_t digit = (uint32_t)(*cursor - '0');
        if(value > maximum / 10U || (value == maximum / 10U && digit > maximum % 10U))
            return false;
        value = value * 10U + digit;
    }
    *output = value;
    return true;
}

static void dndbestiary_packs_status(char* output, size_t size, const char* value) {
    dndbestiary_packs_copy(output, size, value);
}

static void dndbestiary_packs_reader_init(PackReader* reader, File* file) {
    memset(reader, 0, sizeof(*reader));
    reader->file = file;
}

static bool dndbestiary_packs_read_line(PackReader* reader, char* line, size_t size) {
    size_t position = 0U;
    bool consumed = false;
    while(true) {
        if(reader->position >= reader->count) {
            reader->count =
                (uint16_t)storage_file_read(reader->file, reader->buffer, sizeof(reader->buffer));
            reader->position = 0U;
            if(!reader->count) break;
        }
        char value = (char)reader->buffer[reader->position++];
        consumed = true;
        if(value == '\r') continue;
        if(value == '\n') break;
        if(position + 1U < size) line[position++] = value;
    }
    line[position] = '\0';
    return consumed;
}

static bool dndbestiary_packs_safe_id(const char* id) {
    if(!id[0]) return false;
    for(size_t index = 0U; id[index]; ++index)
        if(!((id[index] >= 'a' && id[index] <= 'z') || (id[index] >= '0' && id[index] <= '9') ||
             id[index] == '_'))
            return false;
    return true;
}

static bool dndbestiary_packs_installed_paths(
    const char* id,
    char* index,
    size_t index_size,
    char* content,
    size_t content_size) {
    if(!dndbestiary_packs_safe_id(id) || !index || !content || index_size == 0U ||
       content_size == 0U)
        return false;
    int index_length = snprintf(index, index_size, APP_DATA_PATH("packs/monster_%s.index"), id);
    int content_length =
        snprintf(content, content_size, APP_DATA_PATH("packs/monster_%s.blocks"), id);
    return index_length > 0 && (size_t)index_length < index_size && content_length > 0 &&
           (size_t)content_length < content_size;
}

static uint8_t dndbestiary_packs_split(char* line, char** fields, uint8_t capacity) {
    uint8_t count = 0U;
    char* cursor = line;
    while(count < capacity) {
        fields[count++] = cursor;
        char* separator = strchr(cursor, '|');
        if(!separator) break;
        *separator = '\0';
        cursor = separator + 1U;
    }
    return count;
}

static bool dndbestiary_packs_parse_record(char* line, PackRecord* output) {
    if(!line[0] || line[0] == '#') return false;
    char* fields[5];
    uint8_t count = dndbestiary_packs_split(line, fields, 5U);
    if(count != 3U && count != 5U) return false;
    memset(output, 0, sizeof(*output));
    dndbestiary_packs_copy(output->summary.id, sizeof(output->summary.id), fields[0]);
    dndbestiary_packs_copy(output->summary.name, sizeof(output->summary.name), fields[1]);
    uint32_t enabled = 0U;
    if(!dndbestiary_packs_parse_u32(fields[2], 1U, &enabled)) return false;
    output->summary.enabled = (uint8_t)enabled;
    return dndbestiary_packs_safe_id(output->summary.id) && output->summary.name[0];
}

static uint16_t dndbestiary_packs_load_registry(
    Storage* storage,
    PackRecord records[PACK_MAX_RECORDS],
    bool* valid) {
    *valid = true;
    File* file = storage_file_alloc(storage);
    if(!file) {
        *valid = false;
        return 0U;
    }
    if(!storage_file_open(file, MONSTER_REGISTRY, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return 0U;
    }
    PackReader reader;
    dndbestiary_packs_reader_init(&reader, file);
    char line[PACK_LINE_LEN];
    uint16_t count = 0U;
    while(dndbestiary_packs_read_line(&reader, line, sizeof(line))) {
        /* Older registries may have one trailing key/value metadata record. */
        if(!strchr(line, '|') && strchr(line, '=')) {
            continue;
        }
        PackRecord record;
        if(dndbestiary_packs_parse_record(line, &record)) {
            if(count >= PACK_MAX_RECORDS) {
                *valid = false;
                break;
            }
            records[count++] = record;
        } else if(line[0] != '#') {
            *valid = false;
            break;
        }
    }
    if(storage_file_get_error(file) != FSE_OK) *valid = false;
    storage_file_close(file);
    storage_file_free(file);
    return *valid ? count : 0U;
}

static bool
    dndbestiary_packs_write_registry(Storage* storage, const PackRecord* records, uint16_t count) {
    storage_common_mkdir(storage, APP_DATA_PATH(""));
    storage_common_mkdir(storage, APP_DATA_PATH("packs"));
    const char* temporary = MONSTER_REGISTRY_TEMP;
    storage_common_remove(storage, temporary);
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    bool ok = storage_file_open(file, temporary, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    char line[PACK_LINE_LEN];
    int length = snprintf(line, sizeof(line), "# PocketPackRegistry=%u\n", PACK_VERSION);
    if(ok && length > 0 && (size_t)length < sizeof(line)) {
        ok = storage_file_write(file, line, (size_t)length) == (size_t)length;
    } else {
        ok = false;
    }
    for(uint16_t index = 0U; ok && index < count; ++index) {
        length = snprintf(
            line,
            sizeof(line),
            "%s|%s|%u\n",
            records[index].summary.id,
            records[index].summary.name,
            records[index].summary.enabled);
        if(length <= 0 || (size_t)length >= sizeof(line)) {
            ok = false;
            break;
        }
        ok = storage_file_write(file, line, (size_t)length) == (size_t)length;
    }
    if(ok) ok = storage_file_sync(file);
    storage_file_close(file);
    storage_file_free(file);
    if(!ok) {
        storage_common_remove(storage, temporary);
        return false;
    }
    const char* registry = MONSTER_REGISTRY;
    const char* backup = MONSTER_REGISTRY_BACKUP;
    storage_common_remove(storage, backup);
    bool had_registry = storage_common_rename(storage, registry, backup) == FSE_OK;
    if(storage_common_rename(storage, temporary, registry) == FSE_OK) {
        return true;
    }
    if(had_registry) storage_common_rename(storage, backup, registry);
    storage_common_remove(storage, temporary);
    return false;
}

static bool dndbestiary_packs_copy_file(
    Storage* storage,
    const char* source,
    const char* destination,
    const char* temporary) {
    if(!storage || !source || !destination || !temporary) return false;
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
    uint8_t buffer[PACK_READ_BUFFER];
    while(ok) {
        size_t count = storage_file_read(input, buffer, sizeof(buffer));
        if(!count) break;
        ok = storage_file_write(output, buffer, count) == count;
    }
    if(ok) ok = storage_file_get_error(input) == FSE_OK && storage_file_sync(output);
    storage_file_close(input);
    storage_file_close(output);
    storage_file_free(input);
    storage_file_free(output);
    if(!ok) {
        storage_common_remove(storage, temporary);
        return false;
    }
    char backup[POCKET_D20_LONG_PATH_LEN];
    int length = snprintf(backup, sizeof(backup), "%s.publish.bak", destination);
    if(length < 0 || (size_t)length >= sizeof(backup)) {
        storage_common_remove(storage, temporary);
        return false;
    }
    if(storage_file_exists(storage, backup) && storage_common_remove(storage, backup) != FSE_OK) {
        storage_common_remove(storage, temporary);
        return false;
    }
    bool had_destination = storage_file_exists(storage, destination);
    if(had_destination && storage_common_rename(storage, destination, backup) != FSE_OK) {
        storage_common_remove(storage, temporary);
        return false;
    }
    if(storage_common_rename(storage, temporary, destination) == FSE_OK) {
        if(had_destination) storage_common_remove(storage, backup);
        return true;
    }
    if(had_destination) storage_common_rename(storage, backup, destination);
    storage_common_remove(storage, temporary);
    return false;
}

static bool dndbestiary_packs_files_match(Storage* storage, const char* left, const char* right) {
    File* left_file = storage_file_alloc(storage);
    File* right_file = storage_file_alloc(storage);
    bool left_open = left_file &&
                     storage_file_open(left_file, left, FSAM_READ, FSOM_OPEN_EXISTING);
    bool right_open = right_file && left_open &&
                      storage_file_open(right_file, right, FSAM_READ, FSOM_OPEN_EXISTING);
    bool equal = left_open && right_open;
    uint8_t left_buffer[PACK_READ_BUFFER];
    uint8_t right_buffer[PACK_READ_BUFFER];
    while(equal) {
        size_t left_count = storage_file_read(left_file, left_buffer, sizeof(left_buffer));
        size_t right_count = storage_file_read(right_file, right_buffer, sizeof(right_buffer));
        if(left_count != right_count ||
           (left_count && memcmp(left_buffer, right_buffer, left_count))) {
            equal = false;
            break;
        }
        if(!left_count) break;
    }
    if(left_file) {
        if(left_open && storage_file_get_error(left_file) != FSE_OK) equal = false;
        if(left_open) storage_file_close(left_file);
        storage_file_free(left_file);
    }
    if(right_file) {
        if(right_open && storage_file_get_error(right_file) != FSE_OK) equal = false;
        if(right_open) storage_file_close(right_file);
        storage_file_free(right_file);
    }
    return equal;
}

static bool dndbestiary_packs_append_file(
    Storage* storage,
    const char* source,
    File* output,
    bool skip_comments) {
    File* input = storage_file_alloc(storage);
    if(!input) return false;
    bool ok = storage_file_open(input, source, FSAM_READ, FSOM_OPEN_EXISTING);
    PackReader reader;
    dndbestiary_packs_reader_init(&reader, input);
    char line[PACK_LINE_LEN];
    while(ok && dndbestiary_packs_read_line(&reader, line, sizeof(line))) {
        if(skip_comments && (!line[0] || line[0] == '#')) continue;
        size_t length = strlen(line);
        ok = storage_file_write(output, line, length) == length &&
             storage_file_write(output, "\n", 1U) == 1U;
    }
    if(ok) ok = storage_file_get_error(input) == FSE_OK;
    storage_file_close(input);
    storage_file_free(input);
    return ok;
}

static bool dndbestiary_packs_build_enabled_monsters(
    Storage* storage,
    const PackRecord* records,
    uint16_t count) {
    storage_common_mkdir(storage, APP_DATA_PATH("monsters"));
    storage_common_remove(storage, MONSTER_ENABLED_INDEX_TEMP);
    storage_common_remove(storage, MONSTER_ENABLED_CONTENT_TEMP);
    File* index = storage_file_alloc(storage);
    File* content = storage_file_alloc(storage);
    if(!index || !content) {
        if(index) storage_file_free(index);
        if(content) storage_file_free(content);
        return false;
    }
    bool ok =
        storage_file_open(index, MONSTER_ENABLED_INDEX_TEMP, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
        storage_file_open(content, MONSTER_ENABLED_CONTENT_TEMP, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    static const char index_header[] = "# MonsterPack=1\n";
    static const char content_header[] = "# MonsterStatBlocks=1\n";
    if(ok)
        ok = storage_file_write(index, index_header, sizeof(index_header) - 1U) ==
                 sizeof(index_header) - 1U &&
             storage_file_write(content, content_header, sizeof(content_header) - 1U) ==
                 sizeof(content_header) - 1U;
    for(uint16_t record = 0U; ok && record < count; ++record) {
        if(!records[record].summary.enabled) continue;
        char index_path[POCKET_D20_PATH_LEN], content_path[POCKET_D20_PATH_LEN];
        ok = dndbestiary_packs_installed_paths(
                 records[record].summary.id,
                 index_path,
                 sizeof(index_path),
                 content_path,
                 sizeof(content_path)) &&
             dndbestiary_packs_append_file(storage, index_path, index, true) &&
             dndbestiary_packs_append_file(storage, content_path, content, true);
    }
    if(ok) ok = storage_file_sync(index) && storage_file_sync(content);
    storage_file_close(index);
    storage_file_close(content);
    storage_file_free(index);
    storage_file_free(content);
    if(!ok) {
        storage_common_remove(storage, MONSTER_ENABLED_INDEX_TEMP);
        storage_common_remove(storage, MONSTER_ENABLED_CONTENT_TEMP);
        return false;
    }
    bool had_index = storage_file_exists(storage, MONSTER_ENABLED_INDEX);
    bool had_content = storage_file_exists(storage, MONSTER_ENABLED_CONTENT);
    if((storage_file_exists(storage, MONSTER_ENABLED_INDEX_BACKUP) &&
        storage_common_remove(storage, MONSTER_ENABLED_INDEX_BACKUP) != FSE_OK) ||
       (storage_file_exists(storage, MONSTER_ENABLED_CONTENT_BACKUP) &&
        storage_common_remove(storage, MONSTER_ENABLED_CONTENT_BACKUP) != FSE_OK)) {
        storage_common_remove(storage, MONSTER_ENABLED_INDEX_TEMP);
        storage_common_remove(storage, MONSTER_ENABLED_CONTENT_TEMP);
        return false;
    }
    if(had_index && storage_common_rename(
                        storage, MONSTER_ENABLED_INDEX, MONSTER_ENABLED_INDEX_BACKUP) != FSE_OK) {
        storage_common_remove(storage, MONSTER_ENABLED_INDEX_TEMP);
        storage_common_remove(storage, MONSTER_ENABLED_CONTENT_TEMP);
        return false;
    }
    if(had_content &&
       storage_common_rename(storage, MONSTER_ENABLED_CONTENT, MONSTER_ENABLED_CONTENT_BACKUP) !=
           FSE_OK) {
        if(had_index)
            storage_common_rename(storage, MONSTER_ENABLED_INDEX_BACKUP, MONSTER_ENABLED_INDEX);
        storage_common_remove(storage, MONSTER_ENABLED_INDEX_TEMP);
        storage_common_remove(storage, MONSTER_ENABLED_CONTENT_TEMP);
        return false;
    }
    bool index_published =
        storage_common_rename(storage, MONSTER_ENABLED_INDEX_TEMP, MONSTER_ENABLED_INDEX) ==
        FSE_OK;
    bool content_published =
        index_published &&
        storage_common_rename(storage, MONSTER_ENABLED_CONTENT_TEMP, MONSTER_ENABLED_CONTENT) ==
            FSE_OK;
    if(index_published && content_published) {
        storage_common_remove(storage, MONSTER_ENABLED_INDEX_BACKUP);
        storage_common_remove(storage, MONSTER_ENABLED_CONTENT_BACKUP);
        return true;
    }
    if(index_published) storage_common_remove(storage, MONSTER_ENABLED_INDEX);
    if(content_published) storage_common_remove(storage, MONSTER_ENABLED_CONTENT);
    if(had_index)
        storage_common_rename(storage, MONSTER_ENABLED_INDEX_BACKUP, MONSTER_ENABLED_INDEX);
    if(had_content)
        storage_common_rename(storage, MONSTER_ENABLED_CONTENT_BACKUP, MONSTER_ENABLED_CONTENT);
    storage_common_remove(storage, MONSTER_ENABLED_INDEX_TEMP);
    storage_common_remove(storage, MONSTER_ENABLED_CONTENT_TEMP);
    return false;
}

bool dndbestiary_packs_rebuild_enabled(Storage* storage) {
    PackRecord* records = dndbestiary_packs_records_alloc();
    if(!records) return false;
    bool valid = false;
    if(!storage_file_exists(storage, MONSTER_REGISTRY) &&
       storage_file_exists(storage, MONSTER_REGISTRY_BACKUP))
        storage_common_rename(storage, MONSTER_REGISTRY_BACKUP, MONSTER_REGISTRY);
    uint16_t count = dndbestiary_packs_load_registry(storage, records, &valid);
    if(!valid && storage_file_exists(storage, MONSTER_REGISTRY_BACKUP)) {
        storage_common_remove(storage, MONSTER_REGISTRY);
        if(storage_common_rename(storage, MONSTER_REGISTRY_BACKUP, MONSTER_REGISTRY) == FSE_OK)
            count = dndbestiary_packs_load_registry(storage, records, &valid);
    }
    bool rebuilt = valid && dndbestiary_packs_build_enabled_monsters(storage, records, count);
    free(records);
    if(rebuilt) storage_common_remove(storage, MONSTER_REGISTRY_BACKUP);
    return rebuilt;
}

static bool
    dndbestiary_packs_file_has_header(Storage* storage, const char* path, const char* header) {
    if(!storage || !path || !header || !storage_file_exists(storage, path)) return false;
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    bool opened = storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING);
    size_t length = strlen(header);
    char buffer[24];
    bool valid = false;
    if(opened && length < sizeof(buffer)) {
        size_t read = storage_file_read(file, buffer, length);
        valid = read == length && memcmp(buffer, header, length) == 0;
    }
    if(opened) storage_file_close(file);
    storage_file_free(file);
    return valid;
}

bool dndbestiary_packs_ensure_enabled(Storage* storage) {
    if(dndbestiary_packs_file_has_header(storage, MONSTER_ENABLED_INDEX, "# MonsterPack=1\n") &&
       storage_file_exists(storage, MONSTER_ENABLED_CONTENT))
        return true;
    return dndbestiary_packs_rebuild_enabled(storage);
}

uint16_t dndbestiary_packs_count(Storage* storage) {
    PackRecord* records = dndbestiary_packs_records_alloc();
    if(!records) return 0U;
    bool valid = false;
    uint16_t count = dndbestiary_packs_load_registry(storage, records, &valid);
    free(records);
    return valid ? count : 0U;
}

bool dndbestiary_packs_at(Storage* storage, uint16_t index, PocketPackSummary* output) {
    if(!output) return false;
    PackRecord* records = dndbestiary_packs_records_alloc();
    if(!records) return false;
    bool valid = false;
    uint16_t count = dndbestiary_packs_load_registry(storage, records, &valid);
    bool found = valid && index < count;
    if(found) *output = records[index].summary;
    free(records);
    return found;
}

static bool dndbestiary_packs_read_manifest(Storage* storage, PackManifest* output) {
    const char* path = MONSTER_INBOX_MANIFEST;
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    bool opened = storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING);
    PackReader reader;
    dndbestiary_packs_reader_init(&reader, file);
    char line[PACK_LINE_LEN];
    memset(output, 0, sizeof(*output));
    while(opened && dndbestiary_packs_read_line(&reader, line, sizeof(line))) {
        char* value = strchr(line, '=');
        if(!value) continue;
        *value++ = '\0';
        if(!strcmp(line, "PocketPack")) {
            /* Version is informational; recognized manifest fields remain usable. */
        } else if(!strcmp(line, "Id"))
            dndbestiary_packs_copy(output->id, sizeof(output->id), value);
        else if(!strcmp(line, "Name"))
            dndbestiary_packs_copy(output->name, sizeof(output->name), value);
        /* Unknown legacy metadata fields are ignored. */
    }
    bool io_ok = opened && storage_file_get_error(file) == FSE_OK;
    if(opened) storage_file_close(file);
    storage_file_free(file);
    return io_ok && dndbestiary_packs_safe_id(output->id) && output->name[0];
}

static bool dndbestiary_packs_validate_index(Storage* storage, const char* path) {
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    bool ok = storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING);
    PackReader reader;
    dndbestiary_packs_reader_init(&reader, file);
    char line[PACK_LINE_LEN];
    uint16_t records = 0U;
    while(ok && dndbestiary_packs_read_line(&reader, line, sizeof(line))) {
        if(!line[0] || line[0] == '#') continue;
        char* fields[10];
        uint8_t count = dndbestiary_packs_split(line, fields, 10U);
        ok = count == 10U && !strcmp(fields[8], "Custom Pack");
        if(ok && records < UINT16_MAX) ++records;
    }
    if(ok) ok = storage_file_get_error(file) == FSE_OK;
    storage_file_close(file);
    storage_file_free(file);
    return ok && records > 0U;
}

static bool
    dndbestiary_packs_index_contains_id(Storage* storage, const char* path, const char* id) {
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    bool found = false;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        PackReader reader;
        dndbestiary_packs_reader_init(&reader, file);
        char line[PACK_LINE_LEN];
        while(dndbestiary_packs_read_line(&reader, line, sizeof(line))) {
            if(!line[0] || line[0] == '#') continue;
            char* separator = strchr(line, '|');
            if(separator) *separator = '\0';
            if(!strcmp(line, id)) {
                found = true;
                break;
            }
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    return found;
}

static bool dndbestiary_packs_unique_record_ids(Storage* storage, const char* new_index) {
    enum {
        PackMaximumIds = 96U
    };
    char(*ids)[POCKET_PACK_ID_LEN] = calloc(PackMaximumIds, POCKET_PACK_ID_LEN);
    if(!ids) return false;
    const char* packaged = MONSTER_PACKAGED_INDEX;
    const char* custom = MONSTER_CUSTOM_INDEX;
    const char* enabled = MONSTER_ENABLED_INDEX;
    File* file = storage_file_alloc(storage);
    if(!file) {
        free(ids);
        return false;
    }
    bool ok = storage_file_open(file, new_index, FSAM_READ, FSOM_OPEN_EXISTING);
    PackReader reader;
    dndbestiary_packs_reader_init(&reader, file);
    char line[PACK_LINE_LEN];
    uint16_t count = 0U;
    while(ok && dndbestiary_packs_read_line(&reader, line, sizeof(line))) {
        if(!line[0] || line[0] == '#') continue;
        char* separator = strchr(line, '|');
        if(separator) *separator = '\0';
        if(!dndbestiary_packs_safe_id(line) || count >= PackMaximumIds ||
           dndbestiary_packs_index_contains_id(storage, packaged, line) ||
           dndbestiary_packs_index_contains_id(storage, custom, line) ||
           dndbestiary_packs_index_contains_id(storage, enabled, line)) {
            ok = false;
            break;
        }
        for(uint16_t prior = 0U; prior < count; ++prior)
            if(!strcmp(ids[prior], line)) {
                ok = false;
                break;
            }
        if(ok) dndbestiary_packs_copy(ids[count++], POCKET_PACK_ID_LEN, line);
    }
    storage_file_close(file);
    storage_file_free(file);
    free(ids);
    return ok && count > 0U;
}

static bool dndbestiary_packs_publish_install_file(
    Storage* storage,
    const char* source,
    const char* destination) {
    char temporary[POCKET_D20_LONG_PATH_LEN];
    int length = snprintf(temporary, sizeof(temporary), "%s.install", destination);
    if(length <= 0 || (size_t)length >= sizeof(temporary) ||
       storage_file_exists(storage, destination) ||
       !dndbestiary_packs_copy_file(storage, source, temporary, APP_DATA_PATH("packs/install.tmp")))
        return false;
    if(!dndbestiary_packs_files_match(storage, source, temporary) ||
       storage_common_rename(storage, temporary, destination) != FSE_OK) {
        storage_common_remove(storage, temporary);
        return false;
    }
    return true;
}

bool dndbestiary_packs_install_inbox(Storage* storage, char* status, size_t status_size) {
    PackManifest manifest;
    if(!dndbestiary_packs_read_manifest(storage, &manifest)) {
        dndbestiary_packs_status(status, status_size, "Inbox manifest invalid");
        return false;
    }
    const char* inbox_index = MONSTER_INBOX_INDEX;
    const char* inbox_content = MONSTER_INBOX_CONTENT;
    if(!dndbestiary_packs_validate_index(storage, inbox_index) ||
       !dndbestiary_packs_unique_record_ids(storage, inbox_index)) {
        dndbestiary_packs_status(status, status_size, "Pack format failed");
        return false;
    }

    PackRecord* records = dndbestiary_packs_records_alloc();
    if(!records) {
        dndbestiary_packs_status(status, status_size, "Pack memory low");
        return false;
    }
    bool result = false;
    bool valid = false;
    uint16_t count = dndbestiary_packs_load_registry(storage, records, &valid);
    if(!valid || count >= PACK_MAX_RECORDS) {
        dndbestiary_packs_status(status, status_size, "Pack registry unavailable");
        goto done;
    }
    for(uint16_t record_index = 0U; record_index < count; ++record_index) {
        if(strcmp(records[record_index].summary.id, manifest.id)) continue;
        dndbestiary_packs_status(status, status_size, "Pack ID already installed");
        goto done;
    }

    storage_common_mkdir(storage, APP_DATA_PATH(""));
    storage_common_mkdir(storage, APP_DATA_PATH("packs"));
    char installed_index[POCKET_D20_PATH_LEN];
    char installed_content[POCKET_D20_PATH_LEN];
    if(!dndbestiary_packs_installed_paths(
           manifest.id,
           installed_index,
           sizeof(installed_index),
           installed_content,
           sizeof(installed_content))) {
        dndbestiary_packs_status(status, status_size, "Pack path too long");
        goto done;
    }
    if(storage_file_exists(storage, installed_index) ||
       storage_file_exists(storage, installed_content)) {
        dndbestiary_packs_status(status, status_size, "Installed pack files already exist");
        goto done;
    }
    if(!dndbestiary_packs_publish_install_file(storage, inbox_index, installed_index) ||
       !dndbestiary_packs_publish_install_file(storage, inbox_content, installed_content)) {
        storage_common_remove(storage, installed_index);
        storage_common_remove(storage, installed_content);
        dndbestiary_packs_status(status, status_size, "Pack install publish failed");
        goto done;
    }

    PackRecord* record = &records[count++];
    memset(record, 0, sizeof(*record));
    dndbestiary_packs_copy(record->summary.id, sizeof(record->summary.id), manifest.id);
    dndbestiary_packs_copy(record->summary.name, sizeof(record->summary.name), manifest.name);
    record->summary.enabled = 1U;
    if(!dndbestiary_packs_write_registry(storage, records, count) ||
       !dndbestiary_packs_build_enabled_monsters(storage, records, count)) {
        storage_common_remove(storage, installed_index);
        storage_common_remove(storage, installed_content);
        storage_common_remove(storage, MONSTER_REGISTRY);
        storage_common_rename(storage, MONSTER_REGISTRY_BACKUP, MONSTER_REGISTRY);
        dndbestiary_packs_status(status, status_size, "Pack transaction rolled back");
        goto done;
    }
    storage_common_remove(storage, MONSTER_REGISTRY_BACKUP);
    dndbestiary_packs_status(status, status_size, "Pack installed and enabled");
    result = true;

done:
    free(records);
    return result;
}

bool dndbestiary_packs_set_enabled(Storage* storage, const char* id, bool enabled) {
    PackRecord* records = dndbestiary_packs_records_alloc();
    if(!records) return false;
    bool valid = false;
    uint16_t count = dndbestiary_packs_load_registry(storage, records, &valid);
    bool found = false;
    if(valid) {
        for(uint16_t index = 0U; index < count; ++index) {
            if(strcmp(records[index].summary.id, id)) continue;
            records[index].summary.enabled = enabled ? 1U : 0U;
            found = true;
            break;
        }
    }
    bool written = found && dndbestiary_packs_write_registry(storage, records, count);
    bool rebuilt = written && dndbestiary_packs_build_enabled_monsters(storage, records, count);
    free(records);
    if(rebuilt) {
        storage_common_remove(storage, MONSTER_REGISTRY_BACKUP);
        return true;
    }
    if(written) {
        storage_common_remove(storage, MONSTER_REGISTRY);
        storage_common_rename(storage, MONSTER_REGISTRY_BACKUP, MONSTER_REGISTRY);
    }
    return false;
}
