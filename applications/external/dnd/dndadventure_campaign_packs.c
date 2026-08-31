#include "dndadventure_campaign_packs.h"
#include "dnd_fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAMPAIGN_PACK_VERSION 1U
#define CAMPAIGN_PACK_MAX_RECORDS 16U
#define CAMPAIGN_PACK_LINE_LEN 768U
#define CAMPAIGN_PACK_READ_BUFFER 256U

#define CAMPAIGN_REGISTRY             APP_DATA_PATH("packs/campaign_registry.txt")
#define CAMPAIGN_INBOX_MANIFEST       APP_DATA_PATH("packs/campaign_inbox/manifest.txt")
#define CAMPAIGN_INBOX_INDEX          APP_DATA_PATH("packs/campaign_inbox/index.txt")
#define CAMPAIGN_INBOX_CONTENT        APP_DATA_PATH("packs/campaign_inbox/scenes.txt")
#define CAMPAIGN_ENABLED_INDEX        APP_DATA_PATH("campaigns/enabled_index.txt")
#define CAMPAIGN_PACKAGED_INDEX       APP_ASSETS_PATH("campaigns/index.txt")
#define CAMPAIGN_CUSTOM_INDEX         APP_DATA_PATH("campaigns/custom_index.txt")

typedef struct {
    File* file;
    uint8_t buffer[CAMPAIGN_PACK_READ_BUFFER];
    uint16_t position;
    uint16_t count;
} CampaignPackReader;

typedef struct {
    PocketCampaignPackSummary summary;
} CampaignPackRecord;

typedef struct {
    char id[POCKET_CAMPAIGN_PACK_ID_LEN];
    char name[POCKET_CAMPAIGN_PACK_NAME_LEN];
} CampaignPackManifest;

static CampaignPackRecord* campaign_pack_records_alloc(void) {
    return calloc(CAMPAIGN_PACK_MAX_RECORDS, sizeof(CampaignPackRecord));
}

static void campaign_pack_copy(char* output, size_t size, const char* value) {
    if(!output || !size) return;
    strncpy(output, value ? value : "", size - 1U);
    output[size - 1U] = '\0';
}

static void campaign_pack_status(char* output, size_t size, const char* value) {
    campaign_pack_copy(output, size, value);
}

static bool campaign_pack_parse_u32(const char* text, uint32_t maximum, uint32_t* output) {
    if(!text || !text[0] || !output) return false;
    uint32_t value = 0U;
    for(const char* p = text; *p; ++p) {
        if(*p < '0' || *p > '9') return false;
        uint32_t digit = (uint32_t)(*p - '0');
        if(value > maximum / 10U || (value == maximum / 10U && digit > maximum % 10U))
            return false;
        value = value * 10U + digit;
    }
    *output = value;
    return true;
}

static bool campaign_pack_safe_id(const char* id) {
    if(!id || !id[0]) return false;
    size_t length = strlen(id);
    if(length >= POCKET_CAMPAIGN_PACK_ID_LEN) return false;
    for(size_t i = 0U; i < length; ++i) {
        char ch = id[i];
        if(!((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_')) return false;
    }
    return true;
}

static void campaign_pack_reader_init(CampaignPackReader* reader, File* file) {
    memset(reader, 0, sizeof(*reader));
    reader->file = file;
}

static bool campaign_pack_read_line(CampaignPackReader* reader, char* line, size_t size) {
    size_t used = 0U;
    bool consumed = false;
    while(true) {
        if(reader->position >= reader->count) {
            reader->count = (uint16_t)storage_file_read(reader->file, reader->buffer, sizeof(reader->buffer));
            reader->position = 0U;
            if(!reader->count) break;
        }
        char ch = (char)reader->buffer[reader->position++];
        consumed = true;
        if(ch == '\r') continue;
        if(ch == '\n') break;
        if(used + 1U < size) line[used++] = ch;
    }
    line[used] = '\0';
    return consumed;
}

static uint8_t campaign_pack_split(char* line, char** fields, uint8_t capacity) {
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

static bool campaign_pack_parse_record(char* line, CampaignPackRecord* output) {
    if(!line[0] || line[0] == '#') return false;
    char* fields[3];
    if(campaign_pack_split(line, fields, 3U) != 3U) return false;
    uint32_t enabled = 0U;
    if(!campaign_pack_parse_u32(fields[2], 1U, &enabled) || !campaign_pack_safe_id(fields[0]) || !fields[1][0])
        return false;
    memset(output, 0, sizeof(*output));
    campaign_pack_copy(output->summary.id, sizeof(output->summary.id), fields[0]);
    campaign_pack_copy(output->summary.name, sizeof(output->summary.name), fields[1]);
    output->summary.enabled = (uint8_t)enabled;
    return true;
}

static uint16_t campaign_pack_load_registry(
    Storage* storage,
    CampaignPackRecord records[CAMPAIGN_PACK_MAX_RECORDS],
    bool* valid) {
    *valid = true;
    File* file = storage_file_alloc(storage);
    if(!file) {
        *valid = false;
        return 0U;
    }
    if(!storage_file_open(file, CAMPAIGN_REGISTRY, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return 0U;
    }
    CampaignPackReader reader;
    campaign_pack_reader_init(&reader, file);
    char line[CAMPAIGN_PACK_LINE_LEN];
    uint16_t count = 0U;
    while(campaign_pack_read_line(&reader, line, sizeof(line))) {
        if(!line[0] || line[0] == '#') continue;
        CampaignPackRecord record;
        if(!campaign_pack_parse_record(line, &record) || count >= CAMPAIGN_PACK_MAX_RECORDS) {
            *valid = false;
            break;
        }
        records[count++] = record;
    }
    if(storage_file_get_error(file) != FSE_OK) *valid = false;
    storage_file_close(file);
    storage_file_free(file);
    return count;
}

static bool campaign_pack_write_registry(
    Storage* storage,
    const CampaignPackRecord* records,
    uint16_t count) {
    storage_common_mkdir(storage, APP_DATA_PATH(""));
    storage_common_mkdir(storage, APP_DATA_PATH("packs"));
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    bool ok = storage_file_open(file, CAMPAIGN_REGISTRY, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    static const char header[] = "# PocketCampaignRegistry=1\n# id|name|enabled\n";
    if(ok) ok = storage_file_write(file, header, sizeof(header) - 1U) == sizeof(header) - 1U;
    char line[128];
    for(uint16_t i = 0U; ok && i < count; ++i) {
        int length = snprintf(
            line,
            sizeof(line),
            "%s|%s|%u\n",
            records[i].summary.id,
            records[i].summary.name,
            records[i].summary.enabled ? 1U : 0U);
        ok = length > 0 && (size_t)length < sizeof(line) &&
             storage_file_write(file, line, (size_t)length) == (size_t)length;
    }
    if(ok) ok = storage_file_sync(file);
    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

static bool campaign_pack_installed_paths(
    const char* id,
    char* index,
    size_t index_size,
    char* content,
    size_t content_size) {
    if(!campaign_pack_safe_id(id)) return false;
    int a = snprintf(index, index_size, APP_DATA_PATH("packs/campaign_%s.index"), id);
    int b = snprintf(content, content_size, APP_DATA_PATH("campaigns/custom_%s/scenes.txt"), id);
    return a > 0 && (size_t)a < index_size && b > 0 && (size_t)b < content_size;
}

static bool campaign_pack_copy_file(Storage* storage, const char* source, const char* destination) {
    if(!pocket_d20_ensure_parent_dir(storage, destination)) return false;
    File* input = storage_file_alloc(storage);
    File* output = storage_file_alloc(storage);
    if(!input || !output) {
        if(input) storage_file_free(input);
        if(output) storage_file_free(output);
        return false;
    }
    bool ok = storage_file_open(input, source, FSAM_READ, FSOM_OPEN_EXISTING) &&
              storage_file_open(output, destination, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    uint8_t buffer[256];
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
    /* Never delete campaign files. A failed copy remains available for manual recovery. */
    return ok;
}

static bool campaign_pack_file_contains_id(Storage* storage, const char* path, const char* id) {
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    bool found = false;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        CampaignPackReader reader;
        campaign_pack_reader_init(&reader, file);
        char line[CAMPAIGN_PACK_LINE_LEN];
        while(campaign_pack_read_line(&reader, line, sizeof(line))) {
            if(!line[0] || line[0] == '#') continue;
            char* separator = strchr(line, '|');
            if(separator) *separator = '\0';
            if(!strcmp(line, id)) {
                found = true;
                break;
            }
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    return found;
}

static bool campaign_pack_read_manifest(Storage* storage, CampaignPackManifest* output) {
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    bool opened = storage_file_open(file, CAMPAIGN_INBOX_MANIFEST, FSAM_READ, FSOM_OPEN_EXISTING);
    CampaignPackReader reader;
    campaign_pack_reader_init(&reader, file);
    char line[CAMPAIGN_PACK_LINE_LEN];
    memset(output, 0, sizeof(*output));
    while(opened && campaign_pack_read_line(&reader, line, sizeof(line))) {
        char* value = strchr(line, '=');
        if(!value) continue;
        *value++ = '\0';
        if(!strcmp(line, "PocketPack")) {
            /* Version is informational; recognized manifest fields remain usable. */
        } else if(!strcmp(line, "Id")) {
            campaign_pack_copy(output->id, sizeof(output->id), value);
        } else if(!strcmp(line, "Name")) {
            campaign_pack_copy(output->name, sizeof(output->name), value);
        }
    }
    bool ok = opened && storage_file_get_error(file) == FSE_OK &&
              campaign_pack_safe_id(output->id) && output->name[0];
    if(opened) storage_file_close(file);
    storage_file_free(file);
    return ok;
}

static bool campaign_pack_validate_index(Storage* storage, const char* pack_id) {
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    bool ok = storage_file_open(file, CAMPAIGN_INBOX_INDEX, FSAM_READ, FSOM_OPEN_EXISTING);
    CampaignPackReader reader;
    campaign_pack_reader_init(&reader, file);
    char line[CAMPAIGN_PACK_LINE_LEN];
    uint16_t records = 0U;
    while(ok && campaign_pack_read_line(&reader, line, sizeof(line))) {
        if(!line[0] || line[0] == '#') continue;
        char* fields[7];
        uint8_t count = campaign_pack_split(line, fields, 7U);
        ok = count == 7U && !strcmp(fields[0], pack_id) && !strcmp(fields[6], "scenes.txt");
        if(ok) ++records;
    }
    if(ok) ok = storage_file_get_error(file) == FSE_OK;
    storage_file_close(file);
    storage_file_free(file);
    return ok && records == 1U;
}

static bool campaign_pack_rebuild_from_records(
    Storage* storage,
    const CampaignPackRecord* records,
    uint16_t count) {
    storage_common_mkdir(storage, APP_DATA_PATH(""));
    storage_common_mkdir(storage, APP_DATA_PATH("campaigns"));
    File* output = storage_file_alloc(storage);
    if(!output) return false;
    bool ok = storage_file_open(output, CAMPAIGN_ENABLED_INDEX, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    static const char header[] = "# CampaignPack=1\n";
    if(ok) ok = storage_file_write(output, header, sizeof(header) - 1U) == sizeof(header) - 1U;
    for(uint16_t i = 0U; ok && i < count; ++i) {
        if(!records[i].summary.enabled) continue;
        char index_path[POCKET_D20_PATH_LEN], content_path[POCKET_D20_PATH_LEN];
        if(!campaign_pack_installed_paths(
               records[i].summary.id,
               index_path,
               sizeof(index_path),
               content_path,
               sizeof(content_path))) {
            ok = false;
            break;
        }
        File* input = storage_file_alloc(storage);
        if(!input) {
            ok = false;
            break;
        }
        if(!storage_file_open(input, index_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
            storage_file_free(input);
            ok = false;
            break;
        }
        uint8_t buffer[256];
        while(ok) {
            size_t got = storage_file_read(input, buffer, sizeof(buffer));
            if(!got) break;
            ok = storage_file_write(output, buffer, got) == got;
        }
        if(ok) ok = storage_file_get_error(input) == FSE_OK;
        storage_file_close(input);
        storage_file_free(input);
    }
    if(ok) ok = storage_file_sync(output);
    storage_file_close(output);
    storage_file_free(output);
    return ok;
}

bool pocket_campaign_pack_rebuild_enabled(Storage* storage) {
    CampaignPackRecord* records = campaign_pack_records_alloc();
    if(!records) return false;
    bool valid = false;
    uint16_t count = campaign_pack_load_registry(storage, records, &valid);
    bool rebuilt = valid && campaign_pack_rebuild_from_records(storage, records, count);
    free(records);
    return rebuilt;
}

bool pocket_campaign_pack_ensure_enabled(Storage* storage) {
    if(storage_file_exists(storage, CAMPAIGN_ENABLED_INDEX)) return true;
    return pocket_campaign_pack_rebuild_enabled(storage);
}

uint16_t pocket_campaign_pack_count(Storage* storage) {
    CampaignPackRecord* records = campaign_pack_records_alloc();
    if(!records) return 0U;
    bool valid = false;
    uint16_t count = campaign_pack_load_registry(storage, records, &valid);
    free(records);
    return valid ? count : 0U;
}

bool pocket_campaign_pack_at(Storage* storage, uint16_t index, PocketCampaignPackSummary* output) {
    if(!output) return false;
    CampaignPackRecord* records = campaign_pack_records_alloc();
    if(!records) return false;
    bool valid = false;
    uint16_t count = campaign_pack_load_registry(storage, records, &valid);
    bool found = valid && index < count;
    if(found) *output = records[index].summary;
    free(records);
    return found;
}

bool pocket_campaign_pack_preview_inbox(
    Storage* storage,
    PocketCampaignPackSummary* output,
    char* status,
    size_t status_size) {
    if(output) memset(output, 0, sizeof(*output));
    CampaignPackManifest manifest;
    if(!campaign_pack_read_manifest(storage, &manifest)) {
        campaign_pack_status(status, status_size, "Inbox manifest invalid");
        return false;
    }
    if(!campaign_pack_validate_index(storage, manifest.id)) {
        campaign_pack_status(status, status_size, "Inbox index invalid");
        return false;
    }
    if(campaign_pack_file_contains_id(storage, CAMPAIGN_PACKAGED_INDEX, manifest.id) ||
       campaign_pack_file_contains_id(storage, CAMPAIGN_CUSTOM_INDEX, manifest.id) ||
       campaign_pack_file_contains_id(storage, CAMPAIGN_ENABLED_INDEX, manifest.id)) {
        campaign_pack_status(status, status_size, "Campaign ID already used");
        return false;
    }
    if(output) {
        campaign_pack_copy(output->id, sizeof(output->id), manifest.id);
        campaign_pack_copy(output->name, sizeof(output->name), manifest.name);
        output->enabled = 1U;
    }
    campaign_pack_status(status, status_size, "Hold OK to install");
    return true;
}

bool pocket_campaign_pack_install_inbox(Storage* storage, char* status, size_t status_size) {
    CampaignPackManifest manifest;
    if(!campaign_pack_read_manifest(storage, &manifest) ||
       !campaign_pack_validate_index(storage, manifest.id)) {
        campaign_pack_status(status, status_size, "Inbox pack invalid");
        return false;
    }
    if(campaign_pack_file_contains_id(storage, CAMPAIGN_PACKAGED_INDEX, manifest.id) ||
       campaign_pack_file_contains_id(storage, CAMPAIGN_CUSTOM_INDEX, manifest.id) ||
       campaign_pack_file_contains_id(storage, CAMPAIGN_ENABLED_INDEX, manifest.id)) {
        campaign_pack_status(status, status_size, "Campaign ID already used");
        return false;
    }

    CampaignPackRecord* records = campaign_pack_records_alloc();
    if(!records) {
        campaign_pack_status(status, status_size, "Pack memory low");
        return false;
    }
    bool result = false;
    bool valid = false;
    uint16_t count = campaign_pack_load_registry(storage, records, &valid);
    if(!valid || count >= CAMPAIGN_PACK_MAX_RECORDS) {
        campaign_pack_status(status, status_size, "Pack registry full/invalid");
        goto done;
    }
    for(uint16_t i = 0U; i < count; ++i) {
        if(!strcmp(records[i].summary.id, manifest.id)) {
            campaign_pack_status(status, status_size, "Pack already installed");
            goto done;
        }
    }

    char index_path[POCKET_D20_PATH_LEN], content_path[POCKET_D20_PATH_LEN];
    if(!campaign_pack_installed_paths(
           manifest.id,
           index_path,
           sizeof(index_path),
           content_path,
           sizeof(content_path)) ||
       storage_file_exists(storage, index_path) || storage_file_exists(storage, content_path)) {
        campaign_pack_status(status, status_size, "Pack path already used");
        goto done;
    }
    if(!campaign_pack_copy_file(storage, CAMPAIGN_INBOX_INDEX, index_path) ||
       !campaign_pack_copy_file(storage, CAMPAIGN_INBOX_CONTENT, content_path)) {
        /* Preserve anything already copied; Adventure never deletes campaign content. */
        campaign_pack_status(status, status_size, "Pack copy failed; files kept");
        goto done;
    }

    CampaignPackRecord* record = &records[count++];
    memset(record, 0, sizeof(*record));
    campaign_pack_copy(record->summary.id, sizeof(record->summary.id), manifest.id);
    campaign_pack_copy(record->summary.name, sizeof(record->summary.name), manifest.name);
    record->summary.enabled = 1U;
    if(!campaign_pack_write_registry(storage, records, count) ||
       !campaign_pack_rebuild_from_records(storage, records, count)) {
        campaign_pack_status(status, status_size, "Pack install write failed");
        goto done;
    }
    campaign_pack_status(status, status_size, "Pack installed/enabled");
    result = true;

done:
    free(records);
    return result;
}

bool pocket_campaign_pack_set_enabled(Storage* storage, const char* id, bool enabled) {
    CampaignPackRecord* records = campaign_pack_records_alloc();
    if(!records) return false;
    bool valid = false;
    uint16_t count = campaign_pack_load_registry(storage, records, &valid);
    bool found = false;
    uint16_t found_index = 0U;
    uint8_t previous_enabled = 0U;
    if(valid) {
        for(uint16_t i = 0U; i < count; ++i) {
            if(strcmp(records[i].summary.id, id)) continue;
            found_index = i;
            previous_enabled = records[i].summary.enabled;
            records[i].summary.enabled = enabled ? 1U : 0U;
            found = true;
            break;
        }
    }
    bool written = found && campaign_pack_write_registry(storage, records, count);
    bool rebuilt = written && campaign_pack_rebuild_from_records(storage, records, count);
    if(written && !rebuilt) {
        records[found_index].summary.enabled = previous_enabled;
        (void)campaign_pack_write_registry(storage, records, count);
        (void)campaign_pack_rebuild_from_records(storage, records, count);
    }
    free(records);
    return rebuilt;
}


