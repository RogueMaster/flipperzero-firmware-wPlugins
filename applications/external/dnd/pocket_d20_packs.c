#include "pocket_d20_packs.h"

#include <furi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PACK_VERSION     1U
#define PACK_MAX_RECORDS 16U
#define PACK_LINE_LEN    768U
#define PACK_READ_BUFFER 512U

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

#define CAMPAIGN_REGISTRY             APP_DATA_PATH("packs/campaign_registry.txt")
#define CAMPAIGN_REGISTRY_TEMP        APP_DATA_PATH("packs/campaign_registry.tmp")
#define CAMPAIGN_REGISTRY_BACKUP      APP_DATA_PATH("packs/campaign_registry.bak")
#define CAMPAIGN_INBOX_MANIFEST       APP_DATA_PATH("packs/campaign_inbox/manifest.txt")
#define CAMPAIGN_INBOX_INDEX          APP_DATA_PATH("packs/campaign_inbox/index.txt")
#define CAMPAIGN_INBOX_CONTENT        APP_DATA_PATH("packs/campaign_inbox/scenes.txt")
#define CAMPAIGN_ENABLED_INDEX        APP_DATA_PATH("campaigns/enabled_index.txt")
#define CAMPAIGN_ENABLED_INDEX_TEMP   APP_DATA_PATH("campaigns/enabled_index.tmp")
#define CAMPAIGN_ENABLED_INDEX_BACKUP APP_DATA_PATH("campaigns/enabled_index.bak")
#define CAMPAIGN_PACKAGED_INDEX       APP_ASSETS_PATH("campaigns/index.txt")
#define CAMPAIGN_CUSTOM_INDEX         APP_DATA_PATH("campaigns/custom_index.txt")

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

static void pack_copy(char* output, size_t size, const char* value) {
    if(!size) return;
    strncpy(output, value ? value : "", size - 1U);
    output[size - 1U] = '\0';
}

static void pack_status(char* output, size_t size, const char* value) {
    pack_copy(output, size, value);
}

static void pack_reader_init(PackReader* reader, File* file) {
    memset(reader, 0, sizeof(*reader));
    reader->file = file;
}

static bool pack_read_line(PackReader* reader, char* line, size_t size) {
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

static bool pack_safe_id(const char* id) {
    if(!id[0]) return false;
    for(size_t index = 0U; id[index]; ++index)
        if(!((id[index] >= 'a' && id[index] <= 'z') || (id[index] >= '0' && id[index] <= '9') ||
             id[index] == '_'))
            return false;
    return true;
}

static const char* pack_registry(PocketPackKind kind) {
    return kind == PocketPackMonster ? MONSTER_REGISTRY : CAMPAIGN_REGISTRY;
}

static const char* pack_registry_temp(PocketPackKind kind) {
    return kind == PocketPackMonster ? MONSTER_REGISTRY_TEMP : CAMPAIGN_REGISTRY_TEMP;
}

static const char* pack_registry_backup(PocketPackKind kind) {
    return kind == PocketPackMonster ? MONSTER_REGISTRY_BACKUP : CAMPAIGN_REGISTRY_BACKUP;
}

static void pack_installed_paths(
    PocketPackKind kind,
    const char* id,
    char* index,
    size_t index_size,
    char* content,
    size_t content_size) {
    if(kind == PocketPackMonster) {
        snprintf(index, index_size, APP_DATA_PATH("packs/monster_%s.index"), id);
        snprintf(content, content_size, APP_DATA_PATH("packs/monster_%s.blocks"), id);
    } else {
        snprintf(index, index_size, APP_DATA_PATH("packs/campaign_%s.index"), id);
        snprintf(content, content_size, APP_DATA_PATH("campaigns/custom_%s/scenes.txt"), id);
    }
}

static uint8_t pack_split(char* line, char** fields, uint8_t capacity) {
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

static bool pack_parse_record(char* line, PackRecord* output) {
    if(!line[0] || line[0] == '#') return false;
    char* fields[5];
    uint8_t count = pack_split(line, fields, 5U);
    if(count != 3U && count != 5U) return false;
    memset(output, 0, sizeof(*output));
    pack_copy(output->summary.id, sizeof(output->summary.id), fields[0]);
    pack_copy(output->summary.name, sizeof(output->summary.name), fields[1]);
    output->summary.enabled = (uint8_t)strtoul(fields[2], NULL, 10) ? 1U : 0U;
    return pack_safe_id(output->summary.id) && output->summary.name[0];
}

static uint16_t pack_load_registry(
    Storage* storage,
    PocketPackKind kind,
    PackRecord records[PACK_MAX_RECORDS],
    bool* valid) {
    *valid = true;
    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, pack_registry(kind), FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return 0U;
    }
    PackReader reader;
    pack_reader_init(&reader, file);
    char line[PACK_LINE_LEN];
    uint16_t count = 0U;
    while(pack_read_line(&reader, line, sizeof(line))) {
        /* Older registries may have one trailing key/value metadata record. */
        if(!strchr(line, '|') && strchr(line, '=')) {
            continue;
        }
        PackRecord record;
        if(pack_parse_record(line, &record)) {
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

static bool pack_write_registry(
    Storage* storage,
    PocketPackKind kind,
    const PackRecord* records,
    uint16_t count) {
    storage_common_mkdir(storage, APP_DATA_PATH(""));
    storage_common_mkdir(storage, APP_DATA_PATH("packs"));
    const char* temporary = pack_registry_temp(kind);
    storage_common_remove(storage, temporary);
    File* file = storage_file_alloc(storage);
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
    const char* registry = pack_registry(kind);
    const char* backup = pack_registry_backup(kind);
    storage_common_remove(storage, backup);
    bool had_registry = storage_common_rename(storage, registry, backup) == FSE_OK;
    if(storage_common_rename(storage, temporary, registry) == FSE_OK) return true;
    if(had_registry) storage_common_rename(storage, backup, registry);
    storage_common_remove(storage, temporary);
    return false;
}

static bool pack_copy_file(
    Storage* storage,
    const char* source,
    const char* destination,
    const char* temporary) {
    storage_common_remove(storage, temporary);
    File* input = storage_file_alloc(storage);
    File* output = storage_file_alloc(storage);
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
    storage_common_remove(storage, destination);
    if(storage_common_rename(storage, temporary, destination) == FSE_OK) return true;
    storage_common_remove(storage, temporary);
    return false;
}

static bool pack_files_match(Storage* storage, const char* left, const char* right) {
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

static bool
    pack_append_file(Storage* storage, const char* source, File* output, bool skip_comments) {
    File* input = storage_file_alloc(storage);
    bool ok = storage_file_open(input, source, FSAM_READ, FSOM_OPEN_EXISTING);
    PackReader reader;
    pack_reader_init(&reader, input);
    char line[PACK_LINE_LEN];
    while(ok && pack_read_line(&reader, line, sizeof(line))) {
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

static bool pack_publish_one(
    Storage* storage,
    const char* temporary,
    const char* destination,
    const char* backup) {
    storage_common_remove(storage, backup);
    bool had_destination = storage_common_rename(storage, destination, backup) == FSE_OK;
    if(storage_common_rename(storage, temporary, destination) == FSE_OK) return true;
    if(had_destination) storage_common_rename(storage, backup, destination);
    storage_common_remove(storage, temporary);
    return false;
}

static bool
    pack_build_enabled_monsters(Storage* storage, const PackRecord* records, uint16_t count) {
    storage_common_mkdir(storage, APP_DATA_PATH("monsters"));
    storage_common_remove(storage, MONSTER_ENABLED_INDEX_TEMP);
    storage_common_remove(storage, MONSTER_ENABLED_CONTENT_TEMP);
    File* index = storage_file_alloc(storage);
    File* content = storage_file_alloc(storage);
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
        char index_path[160], content_path[160];
        pack_installed_paths(
            PocketPackMonster,
            records[record].summary.id,
            index_path,
            sizeof(index_path),
            content_path,
            sizeof(content_path));
        ok = pack_append_file(storage, index_path, index, true) &&
             pack_append_file(storage, content_path, content, true);
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
    if(!pack_publish_one(
           storage,
           MONSTER_ENABLED_INDEX_TEMP,
           MONSTER_ENABLED_INDEX,
           MONSTER_ENABLED_INDEX_BACKUP)) {
        storage_common_remove(storage, MONSTER_ENABLED_CONTENT_TEMP);
        return false;
    }
    if(pack_publish_one(
           storage,
           MONSTER_ENABLED_CONTENT_TEMP,
           MONSTER_ENABLED_CONTENT,
           MONSTER_ENABLED_CONTENT_BACKUP)) {
        storage_common_remove(storage, MONSTER_ENABLED_INDEX_BACKUP);
        storage_common_remove(storage, MONSTER_ENABLED_CONTENT_BACKUP);
        return true;
    }
    storage_common_remove(storage, MONSTER_ENABLED_INDEX);
    storage_common_rename(storage, MONSTER_ENABLED_INDEX_BACKUP, MONSTER_ENABLED_INDEX);
    return false;
}

static bool
    pack_build_enabled_campaigns(Storage* storage, const PackRecord* records, uint16_t count) {
    storage_common_mkdir(storage, APP_DATA_PATH("campaigns"));
    storage_common_remove(storage, CAMPAIGN_ENABLED_INDEX_TEMP);
    File* output = storage_file_alloc(storage);
    bool ok =
        storage_file_open(output, CAMPAIGN_ENABLED_INDEX_TEMP, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    static const char header[] = "# CampaignPack=1\n";
    if(ok) ok = storage_file_write(output, header, sizeof(header) - 1U) == sizeof(header) - 1U;
    for(uint16_t record = 0U; ok && record < count; ++record) {
        if(!records[record].summary.enabled) continue;
        char index_path[160], content_path[160];
        pack_installed_paths(
            PocketPackCampaign,
            records[record].summary.id,
            index_path,
            sizeof(index_path),
            content_path,
            sizeof(content_path));
        ok = pack_append_file(storage, index_path, output, true);
    }
    if(ok) ok = storage_file_sync(output);
    storage_file_close(output);
    storage_file_free(output);
    if(!ok) {
        storage_common_remove(storage, CAMPAIGN_ENABLED_INDEX_TEMP);
        return false;
    }
    bool published = pack_publish_one(
        storage,
        CAMPAIGN_ENABLED_INDEX_TEMP,
        CAMPAIGN_ENABLED_INDEX,
        CAMPAIGN_ENABLED_INDEX_BACKUP);
    if(published) storage_common_remove(storage, CAMPAIGN_ENABLED_INDEX_BACKUP);
    return published;
}

bool pocket_pack_rebuild_enabled(Storage* storage, PocketPackKind kind) {
    PackRecord records[PACK_MAX_RECORDS];
    bool valid = false;
    const char* registry = pack_registry(kind);
    const char* backup = pack_registry_backup(kind);
    if(!storage_file_exists(storage, registry) && storage_file_exists(storage, backup))
        storage_common_rename(storage, backup, registry);
    uint16_t count = pack_load_registry(storage, kind, records, &valid);
    if(!valid && storage_file_exists(storage, backup)) {
        storage_common_remove(storage, registry);
        if(storage_common_rename(storage, backup, registry) == FSE_OK)
            count = pack_load_registry(storage, kind, records, &valid);
    }
    if(!valid) return false;
    bool rebuilt = kind == PocketPackMonster ?
                       pack_build_enabled_monsters(storage, records, count) :
                       pack_build_enabled_campaigns(storage, records, count);
    if(rebuilt) storage_common_remove(storage, backup);
    return rebuilt;
}

uint16_t pocket_pack_count(Storage* storage, PocketPackKind kind) {
    PackRecord records[PACK_MAX_RECORDS];
    bool valid = false;
    return pack_load_registry(storage, kind, records, &valid);
}

bool pocket_pack_at(
    Storage* storage,
    PocketPackKind kind,
    uint16_t index,
    PocketPackSummary* output) {
    PackRecord records[PACK_MAX_RECORDS];
    bool valid = false;
    uint16_t count = pack_load_registry(storage, kind, records, &valid);
    if(!valid || index >= count || !output) return false;
    *output = records[index].summary;
    return true;
}

static bool pack_read_manifest(Storage* storage, PocketPackKind kind, PackManifest* output) {
    const char* path = kind == PocketPackMonster ? MONSTER_INBOX_MANIFEST :
                                                   CAMPAIGN_INBOX_MANIFEST;
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    bool opened = storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING);
    PackReader reader;
    pack_reader_init(&reader, file);
    char line[PACK_LINE_LEN];
    memset(output, 0, sizeof(*output));
    bool version = false;
    while(opened && pack_read_line(&reader, line, sizeof(line))) {
        char* value = strchr(line, '=');
        if(!value) continue;
        *value++ = '\0';
        if(!strcmp(line, "PocketPack"))
            version = strtoul(value, NULL, 10) == PACK_VERSION;
        else if(!strcmp(line, "Id"))
            pack_copy(output->id, sizeof(output->id), value);
        else if(!strcmp(line, "Name"))
            pack_copy(output->name, sizeof(output->name), value);
        /* Unknown legacy metadata fields are ignored. */
    }
    bool io_ok = opened && storage_file_get_error(file) == FSE_OK;
    if(opened) storage_file_close(file);
    storage_file_free(file);
    return io_ok && version && pack_safe_id(output->id) && output->name[0];
}

static bool pack_validate_index(
    Storage* storage,
    PocketPackKind kind,
    const char* path,
    const char* pack_id) {
    File* file = storage_file_alloc(storage);
    bool ok = storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING);
    PackReader reader;
    pack_reader_init(&reader, file);
    char line[PACK_LINE_LEN];
    uint16_t records = 0U;
    while(ok && pack_read_line(&reader, line, sizeof(line))) {
        if(!line[0] || line[0] == '#') continue;
        char original[PACK_LINE_LEN];
        pack_copy(original, sizeof(original), line);
        char* fields[10];
        uint8_t count = pack_split(line, fields, 10U);
        if(kind == PocketPackCampaign) {
            ok = count == 7U && !strcmp(fields[0], pack_id) && !strcmp(fields[6], "scenes.txt");
        } else {
            ok = count == 10U && !strcmp(fields[8], "Custom Pack");
        }
        if(ok && records < UINT16_MAX) ++records;
        (void)original;
    }
    storage_file_close(file);
    storage_file_free(file);
    return ok && records > 0U && (kind == PocketPackMonster || records == 1U);
}

static bool pack_index_contains_id(Storage* storage, const char* path, const char* id) {
    File* file = storage_file_alloc(storage);
    bool found = false;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        PackReader reader;
        pack_reader_init(&reader, file);
        char line[PACK_LINE_LEN];
        while(pack_read_line(&reader, line, sizeof(line))) {
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

static bool pack_unique_record_ids(Storage* storage, PocketPackKind kind, const char* new_index) {
    enum {
        PackMaximumIds = 96U
    };
    char(*ids)[POCKET_PACK_ID_LEN] = calloc(PackMaximumIds, POCKET_PACK_ID_LEN);
    if(!ids) return false;
    const char* packaged = kind == PocketPackMonster ? MONSTER_PACKAGED_INDEX :
                                                       CAMPAIGN_PACKAGED_INDEX;
    const char* custom = kind == PocketPackMonster ? MONSTER_CUSTOM_INDEX : CAMPAIGN_CUSTOM_INDEX;
    const char* enabled = kind == PocketPackMonster ? MONSTER_ENABLED_INDEX :
                                                      CAMPAIGN_ENABLED_INDEX;
    File* file = storage_file_alloc(storage);
    bool ok = storage_file_open(file, new_index, FSAM_READ, FSOM_OPEN_EXISTING);
    PackReader reader;
    pack_reader_init(&reader, file);
    char line[PACK_LINE_LEN];
    uint16_t count = 0U;
    while(ok && pack_read_line(&reader, line, sizeof(line))) {
        if(!line[0] || line[0] == '#') continue;
        char* separator = strchr(line, '|');
        if(separator) *separator = '\0';
        if(!pack_safe_id(line) || count >= PackMaximumIds ||
           pack_index_contains_id(storage, packaged, line) ||
           pack_index_contains_id(storage, custom, line) ||
           pack_index_contains_id(storage, enabled, line)) {
            ok = false;
            break;
        }
        for(uint16_t prior = 0U; prior < count; ++prior)
            if(!strcmp(ids[prior], line)) {
                ok = false;
                break;
            }
        if(ok) pack_copy(ids[count++], POCKET_PACK_ID_LEN, line);
    }
    storage_file_close(file);
    storage_file_free(file);
    free(ids);
    return ok && count > 0U;
}

static bool
    pack_publish_install_file(Storage* storage, const char* source, const char* destination) {
    char temporary[192];
    int length = snprintf(temporary, sizeof(temporary), "%s.install", destination);
    if(length <= 0 || (size_t)length >= sizeof(temporary) ||
       storage_file_exists(storage, destination) ||
       !pack_copy_file(storage, source, temporary, APP_DATA_PATH("packs/install.tmp")))
        return false;
    if(!pack_files_match(storage, source, temporary) ||
       storage_common_rename(storage, temporary, destination) != FSE_OK) {
        storage_common_remove(storage, temporary);
        return false;
    }
    return true;
}

bool pocket_pack_install_inbox(
    Storage* storage,
    PocketPackKind kind,
    char* status,
    size_t status_size) {
    PackManifest manifest;
    if(!pack_read_manifest(storage, kind, &manifest)) {
        pack_status(status, status_size, "Inbox manifest invalid");
        return false;
    }
    const char* inbox_index = kind == PocketPackMonster ? MONSTER_INBOX_INDEX :
                                                          CAMPAIGN_INBOX_INDEX;
    const char* inbox_content = kind == PocketPackMonster ? MONSTER_INBOX_CONTENT :
                                                            CAMPAIGN_INBOX_CONTENT;
    if(!pack_validate_index(storage, kind, inbox_index, manifest.id) ||
       !pack_unique_record_ids(storage, kind, inbox_index)) {
        pack_status(status, status_size, "Pack format failed");
        return false;
    }
    PackRecord records[PACK_MAX_RECORDS];
    bool valid = false;
    uint16_t count = pack_load_registry(storage, kind, records, &valid);
    if(!valid || count >= PACK_MAX_RECORDS) {
        pack_status(status, status_size, "Pack registry unavailable");
        return false;
    }
    for(uint16_t index = 0U; index < count; ++index)
        if(!strcmp(records[index].summary.id, manifest.id)) {
            pack_status(status, status_size, "Pack ID already installed");
            return false;
        }
    storage_common_mkdir(storage, APP_DATA_PATH(""));
    storage_common_mkdir(storage, APP_DATA_PATH("packs"));
    if(kind == PocketPackCampaign) {
        storage_common_mkdir(storage, APP_DATA_PATH("campaigns"));
        char directory[128];
        snprintf(directory, sizeof(directory), APP_DATA_PATH("campaigns/custom_%s"), manifest.id);
        storage_common_mkdir(storage, directory);
    }
    char installed_index[160], installed_content[160];
    pack_installed_paths(
        kind,
        manifest.id,
        installed_index,
        sizeof(installed_index),
        installed_content,
        sizeof(installed_content));
    if(storage_file_exists(storage, installed_index) ||
       storage_file_exists(storage, installed_content)) {
        pack_status(status, status_size, "Installed pack files already exist");
        return false;
    }
    if(!pack_publish_install_file(storage, inbox_index, installed_index) ||
       !pack_publish_install_file(storage, inbox_content, installed_content)) {
        storage_common_remove(storage, installed_index);
        storage_common_remove(storage, installed_content);
        pack_status(status, status_size, "Pack install publish failed");
        return false;
    }
    PackRecord* record = &records[count++];
    memset(record, 0, sizeof(*record));
    pack_copy(record->summary.id, sizeof(record->summary.id), manifest.id);
    pack_copy(record->summary.name, sizeof(record->summary.name), manifest.name);
    record->summary.enabled = 1U;
    if(!pack_write_registry(storage, kind, records, count) ||
       !(kind == PocketPackMonster ? pack_build_enabled_monsters(storage, records, count) :
                                     pack_build_enabled_campaigns(storage, records, count))) {
        storage_common_remove(storage, installed_index);
        storage_common_remove(storage, installed_content);
        storage_common_remove(storage, pack_registry(kind));
        storage_common_rename(storage, pack_registry_backup(kind), pack_registry(kind));
        pack_status(status, status_size, "Pack transaction rolled back");
        return false;
    }
    storage_common_remove(storage, pack_registry_backup(kind));
    pack_status(status, status_size, "Pack installed and enabled");
    return true;
}

bool pocket_pack_set_enabled(Storage* storage, PocketPackKind kind, const char* id, bool enabled) {
    PackRecord records[PACK_MAX_RECORDS];
    bool valid = false;
    uint16_t count = pack_load_registry(storage, kind, records, &valid);
    if(!valid) return false;
    bool found = false;
    for(uint16_t index = 0U; index < count; ++index) {
        if(strcmp(records[index].summary.id, id)) continue;
        records[index].summary.enabled = enabled ? 1U : 0U;
        found = true;
        break;
    }
    if(!found || !pack_write_registry(storage, kind, records, count)) return false;
    bool rebuilt = kind == PocketPackMonster ?
                       pack_build_enabled_monsters(storage, records, count) :
                       pack_build_enabled_campaigns(storage, records, count);
    if(rebuilt) {
        storage_common_remove(storage, pack_registry_backup(kind));
        return true;
    }
    storage_common_remove(storage, pack_registry(kind));
    storage_common_rename(storage, pack_registry_backup(kind), pack_registry(kind));
    return false;
}
