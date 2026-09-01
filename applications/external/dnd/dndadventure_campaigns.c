#include "dndadventure_campaigns.h"
#include "dnd_fs.h"

#include <furi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAMPAIGN_BUNDLED_INDEX  APP_ASSETS_PATH("campaigns/index.txt")
#define CAMPAIGN_USER_INDEX     APP_DATA_PATH("campaigns/custom_index.txt")
#define CAMPAIGN_ENABLED_INDEX  APP_DATA_PATH("campaigns/enabled_index.txt")
#define CAMPAIGN_BUNDLED_SCENES APP_ASSETS_PATH("campaigns/%s/%s")
#define CAMPAIGN_USER_SCENES    APP_DATA_PATH("campaigns/custom_%s/%s")
#define CAMPAIGN_PROGRESS_DIR   APP_DATA_PATH("campaigns")
#define CAMPAIGN_LINE_LEN       512U
#define CAMPAIGN_READ_BUFFER    256U
#define CAMPAIGN_MAX_SCENES     64U
#define CAMPAIGN_HINT_STRIDE    8U
#define CAMPAIGN_HINT_MAX       8U

typedef struct {
    File* file;
    uint8_t buffer[CAMPAIGN_READ_BUFFER];
    uint16_t position;
    uint16_t count;
    uint32_t offset;
} CampaignReader;

typedef struct {
    uint32_t hints[CAMPAIGN_HINT_MAX];
    uint16_t count;
    uint8_t hint_count;
    bool valid;
} CampaignPathCache;

typedef struct {
    Storage* owner;
    CampaignPathCache bundled;
    CampaignPathCache custom;
    CampaignPathCache enabled;
} CampaignCache;

static bool dndadventure_campaigns_id_safe(const char* id);
static CampaignCache campaign_cache;

static void dndadventure_campaigns_copy(char* out, size_t size, const char* value) {
    if(!size) return;
    const char* source = value ? value : "";
    size_t length = strlen(source);
    if(length >= size) length = size - 1U;
    memcpy(out, source, length);
    out[length] = '\0';
}

static bool dndadventure_campaigns_parse_u32_span(
    const char* begin,
    const char* end,
    uint32_t maximum,
    uint32_t* output) {
    if(!begin || !end || !output || begin >= end) return false;
    uint32_t value = 0U;
    for(const char* cursor = begin; cursor < end; ++cursor) {
        if(*cursor < '0' || *cursor > '9') return false;
        uint32_t digit = (uint32_t)(*cursor - '0');
        if(value > maximum / 10U || (value == maximum / 10U && digit > maximum % 10U))
            return false;
        value = value * 10U + digit;
    }
    *output = value;
    return true;
}

static bool
    dndadventure_campaigns_parse_u32(const char* text, uint32_t maximum, uint32_t* output) {
    return text &&
           dndadventure_campaigns_parse_u32_span(text, text + strlen(text), maximum, output);
}

static void
    dndadventure_campaigns_reader_init(CampaignReader* reader, File* file, uint32_t offset) {
    memset(reader, 0, sizeof(*reader));
    reader->file = file;
    reader->offset = offset;
}

static bool dndadventure_campaigns_reader_next(CampaignReader* reader, char* value) {
    if(reader->position >= reader->count) {
        reader->count =
            (uint16_t)storage_file_read(reader->file, reader->buffer, sizeof(reader->buffer));
        reader->position = 0U;
        if(!reader->count) return false;
    }
    *value = (char)reader->buffer[reader->position++];
    ++reader->offset;
    return true;
}

static bool dndadventure_campaigns_read_line(
    CampaignReader* reader,
    char* line,
    size_t size,
    uint32_t* line_offset) {
    if(line_offset) *line_offset = reader->offset;
    size_t position = 0U;
    char value = '\0';
    bool read_any = false;
    while(dndadventure_campaigns_reader_next(reader, &value)) {
        read_any = true;
        if(value == '\r') continue;
        if(value == '\n') break;
        if(position + 1U < size) line[position++] = value;
    }
    line[position] = '\0';
    return read_any;
}

static uint8_t dndadventure_campaigns_split(char* line, char** fields, uint8_t capacity) {
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

static bool dndadventure_campaigns_parse(char* line, bool bundled, PocketCampaignSummary* output) {
    if(!line[0] || line[0] == '#') return false;
    char* fields[7];
    if(dndadventure_campaigns_split(line, fields, 7U) != 7U) return false;
    memset(output, 0, sizeof(*output));
    dndadventure_campaigns_copy(output->id, sizeof(output->id), fields[0]);
    dndadventure_campaigns_copy(output->name, sizeof(output->name), fields[1]);
    uint32_t pack_version = 0U, minimum_app = 0U, maximum_app = 0U;
    if(!dndadventure_campaigns_parse_u32(fields[2], UINT8_MAX, &pack_version) ||
       !dndadventure_campaigns_parse_u32(fields[3], UINT16_MAX, &minimum_app) ||
       !dndadventure_campaigns_parse_u32(fields[4], UINT16_MAX, &maximum_app))
        return false;
    output->pack_version = (uint8_t)pack_version;
    output->minimum_app = (uint16_t)minimum_app;
    output->maximum_app = (uint16_t)maximum_app;
    dndadventure_campaigns_copy(output->entry_scene, sizeof(output->entry_scene), fields[5]);
    dndadventure_campaigns_copy(output->scenes_file, sizeof(output->scenes_file), fields[6]);
    output->bundled = bundled ? 1U : 0U;
    return output->id[0] && output->name[0] && output->entry_scene[0] && output->scenes_file[0];
}

static void dndadventure_campaigns_path_cache_clear(CampaignPathCache* cache) {
    memset(cache, 0, sizeof(*cache));
}

void dndadventure_campaigns_cache_reset(void) {
    dndadventure_campaigns_path_cache_clear(&campaign_cache.bundled);
    dndadventure_campaigns_path_cache_clear(&campaign_cache.custom);
    dndadventure_campaigns_path_cache_clear(&campaign_cache.enabled);
    campaign_cache.owner = NULL;
}

static void dndadventure_campaigns_path_cache_record(CampaignPathCache* cache, uint32_t offset) {
    if(!cache || cache->count == UINT16_MAX) return;
    if((cache->count % CAMPAIGN_HINT_STRIDE) == 0U && cache->hint_count < CAMPAIGN_HINT_MAX)
        cache->hints[cache->hint_count++] = offset;
    ++cache->count;
}

static bool dndadventure_campaigns_path_cache_build(
    Storage* storage,
    const char* path,
    bool bundled,
    CampaignPathCache* cache) {
    dndadventure_campaigns_path_cache_clear(cache);
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    bool ok = true;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        CampaignReader reader;
        dndadventure_campaigns_reader_init(&reader, file, 0U);
        char line[CAMPAIGN_LINE_LEN];
        PocketCampaignSummary campaign;
        uint32_t line_offset = 0U;
        while(dndadventure_campaigns_read_line(&reader, line, sizeof(line), &line_offset)) {
            if(dndadventure_campaigns_parse(line, bundled, &campaign))
                dndadventure_campaigns_path_cache_record(cache, line_offset);
        }
        if(storage_file_get_error(file) != FSE_OK) ok = false;
    }
    storage_file_close(file);
    storage_file_free(file);
    if(!ok) dndadventure_campaigns_path_cache_clear(cache);
    cache->valid = ok;
    return ok;
}

static bool dndadventure_campaigns_cache_ensure(Storage* storage) {
    if(campaign_cache.owner != storage) {
        dndadventure_campaigns_cache_reset();
        campaign_cache.owner = storage;
    }
    if(!campaign_cache.bundled.valid &&
       !dndadventure_campaigns_path_cache_build(
           storage, CAMPAIGN_BUNDLED_INDEX, true, &campaign_cache.bundled))
        return false;
    if(!campaign_cache.custom.valid &&
       !dndadventure_campaigns_path_cache_build(
           storage, CAMPAIGN_USER_INDEX, false, &campaign_cache.custom))
        return false;
    if(!campaign_cache.enabled.valid &&
       !dndadventure_campaigns_path_cache_build(
           storage, CAMPAIGN_ENABLED_INDEX, false, &campaign_cache.enabled))
        return false;
    return true;
}

static bool dndadventure_campaigns_at_index(
    Storage* storage,
    const char* path,
    bool bundled,
    const CampaignPathCache* cache,
    uint16_t index,
    PocketCampaignSummary* output) {
    if(!cache || index >= cache->count || !cache->hint_count) return false;
    uint16_t hint_slot = (uint16_t)(index / CAMPAIGN_HINT_STRIDE);
    if(hint_slot >= cache->hint_count) hint_slot = (uint16_t)(cache->hint_count - 1U);
    uint16_t logical = (uint16_t)(hint_slot * CAMPAIGN_HINT_STRIDE);
    uint32_t offset = cache->hints[hint_slot];

    File* file = storage_file_alloc(storage);
    if(!file) return false;
    bool found = false;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING) &&
       storage_file_seek(file, offset, true)) {
        CampaignReader reader;
        dndadventure_campaigns_reader_init(&reader, file, offset);
        char line[CAMPAIGN_LINE_LEN];
        while(dndadventure_campaigns_read_line(&reader, line, sizeof(line), NULL)) {
            PocketCampaignSummary campaign;
            if(!dndadventure_campaigns_parse(line, bundled, &campaign)) continue;
            if(logical == index) {
                *output = campaign;
                found = true;
                break;
            }
            if(logical == UINT16_MAX) break;
            ++logical;
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    return found;
}

uint16_t dndadventure_campaigns_count(Storage* storage) {
    if(!dndadventure_campaigns_cache_ensure(storage)) return 0U;
    uint32_t count = (uint32_t)campaign_cache.bundled.count + campaign_cache.custom.count +
                     campaign_cache.enabled.count;
    return count > UINT16_MAX ? UINT16_MAX : (uint16_t)count;
}

bool dndadventure_campaigns_at(Storage* storage, uint16_t index, PocketCampaignSummary* output) {
    if(!output || !dndadventure_campaigns_cache_ensure(storage)) return false;
    if(index < campaign_cache.bundled.count)
        return dndadventure_campaigns_at_index(
            storage, CAMPAIGN_BUNDLED_INDEX, true, &campaign_cache.bundled, index, output);
    index -= campaign_cache.bundled.count;
    if(index < campaign_cache.custom.count)
        return dndadventure_campaigns_at_index(
            storage, CAMPAIGN_USER_INDEX, false, &campaign_cache.custom, index, output);
    index -= campaign_cache.custom.count;
    return index < campaign_cache.enabled.count &&
           dndadventure_campaigns_at_index(
               storage, CAMPAIGN_ENABLED_INDEX, false, &campaign_cache.enabled, index, output);
}

static bool dndadventure_campaigns_find_path(
    Storage* storage,
    const char* path,
    bool bundled,
    const char* id,
    PocketCampaignSummary* output) {
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    bool found = false;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        CampaignReader reader;
        dndadventure_campaigns_reader_init(&reader, file, 0U);
        char line[CAMPAIGN_LINE_LEN];
        while(dndadventure_campaigns_read_line(&reader, line, sizeof(line), NULL)) {
            PocketCampaignSummary campaign;
            if(!dndadventure_campaigns_parse(line, bundled, &campaign) || strcmp(campaign.id, id))
                continue;
            *output = campaign;
            found = true;
            break;
        }
        if(storage_file_get_error(file) != FSE_OK) found = false;
        storage_file_close(file);
    }
    storage_file_free(file);
    return found;
}

bool dndadventure_campaigns_find(Storage* storage, const char* id, PocketCampaignSummary* output) {
    if(!id || !output || !dndadventure_campaigns_cache_ensure(storage)) return false;
    return dndadventure_campaigns_find_path(storage, CAMPAIGN_BUNDLED_INDEX, true, id, output) ||
           dndadventure_campaigns_find_path(storage, CAMPAIGN_USER_INDEX, false, id, output) ||
           dndadventure_campaigns_find_path(storage, CAMPAIGN_ENABLED_INDEX, false, id, output);
}

bool dndadventure_campaigns_scene_path(
    Storage* storage,
    const PocketCampaignSummary* campaign,
    char* output,
    size_t size) {
    if(!storage || !campaign || !output || size == 0U ||
       !dndadventure_campaigns_id_safe(campaign->id))
        return false;
    int length = snprintf(output, size, CAMPAIGN_USER_SCENES, campaign->id, campaign->scenes_file);
    if(length > 0 && (size_t)length < size && storage_file_exists(storage, output)) return true;
    if(campaign->bundled) {
        length =
            snprintf(output, size, CAMPAIGN_BUNDLED_SCENES, campaign->id, campaign->scenes_file);
        return length > 0 && (size_t)length < size && storage_file_exists(storage, output);
    }
    return false;
}

static bool dndadventure_campaigns_progress_path(
    char* output,
    size_t size,
    uint32_t profile_id,
    const char* campaign_id,
    const char* suffix) {
    if(!output || size == 0U || !dndadventure_campaigns_id_safe(campaign_id) || !suffix ||
       !suffix[0])
        return false;
    int length = snprintf(
        output,
        size,
        APP_DATA_PATH("campaigns/custom_progress_%08lx_%s.%s"),
        (unsigned long)profile_id,
        campaign_id,
        suffix);
    return length > 0 && (size_t)length < size;
}

static bool dndadventure_campaigns_id_safe(const char* id) {
    if(!id || !id[0]) return false;
    size_t length = strlen(id);
    if(length >= POCKET_CAMPAIGN_ID_LEN) return false;
    for(size_t i = 0U; i < length; ++i) {
        char ch = id[i];
        if(!((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') ||
             ch == '_' || ch == '-'))
            return false;
    }
    return true;
}

static bool dndadventure_campaigns_active_path(
    char* output,
    size_t size,
    uint32_t profile_id,
    const char* suffix) {
    int length = snprintf(
        output,
        size,
        APP_DATA_PATH("campaigns/active_%08lx.%s"),
        (unsigned long)profile_id,
        suffix);
    return length > 0 && (size_t)length < size;
}

bool dndadventure_campaigns_active_load(
    Storage* storage,
    uint32_t profile_id,
    char* campaign_id,
    size_t campaign_id_size) {
    if(!storage || !campaign_id || campaign_id_size == 0U) return false;
    campaign_id[0] = '\0';
    char path[POCKET_D20_PATH_LEN];
    if(!dndadventure_campaigns_active_path(path, sizeof(path), profile_id, "txt")) return false;
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return true;
    }
    CampaignReader reader;
    dndadventure_campaigns_reader_init(&reader, file, 0U);
    char line[96];
    bool found = false;
    while(dndadventure_campaigns_read_line(&reader, line, sizeof(line), NULL)) {
        char* value = strchr(line, '=');
        if(!value) continue;
        *value++ = '\0';
        if(strcmp(line, "Campaign") || !dndadventure_campaigns_id_safe(value)) continue;
        dndadventure_campaigns_copy(campaign_id, campaign_id_size, value);
        found = true;
    }
    bool io_ok = storage_file_get_error(file) == FSE_OK;
    storage_file_close(file);
    storage_file_free(file);
    return io_ok && (found || !campaign_id[0]);
}

bool dndadventure_campaigns_active_save(
    Storage* storage,
    uint32_t profile_id,
    const char* campaign_id) {
    if(!storage || !campaign_id || !dndadventure_campaigns_id_safe(campaign_id)) return false;
    storage_common_mkdir(storage, APP_DATA_PATH(""));
    storage_common_mkdir(storage, CAMPAIGN_PROGRESS_DIR);
    char path[POCKET_D20_PATH_LEN];
    if(!dndadventure_campaigns_active_path(path, sizeof(path), profile_id, "txt")) return false;
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    char line[96];
    int length = snprintf(line, sizeof(line), "Campaign=%s\n", campaign_id);
    bool ok = length > 0 && (size_t)length < sizeof(line) &&
              storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
              storage_file_write(file, line, (size_t)length) == (size_t)length &&
              storage_file_sync(file);
    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

bool dndadventure_campaigns_progress_save(
    Storage* storage,
    uint32_t profile_id,
    const PocketCampaignSummary* campaign,
    const PocketCampaignProgress* progress) {
    if(!storage || !campaign || !progress) return false;
    storage_common_mkdir(storage, APP_DATA_PATH(""));
    storage_common_mkdir(storage, CAMPAIGN_PROGRESS_DIR);
    char path[POCKET_D20_PATH_LEN], line[128];
    if(!dndadventure_campaigns_progress_path(path, sizeof(path), profile_id, campaign->id, "txt"))
        return false;
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    bool ok = storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS);
#define CAMPAIGN_WRITE(...)                                                  \
    do {                                                                     \
        int length = snprintf(line, sizeof(line), __VA_ARGS__);              \
        if(length <= 0 || (size_t)length >= sizeof(line) ||                  \
           storage_file_write(file, line, (size_t)length) != (size_t)length) \
            ok = false;                                                      \
    } while(false)
    if(ok) CAMPAIGN_WRITE("CampaignProgress=1\n");
    if(ok) CAMPAIGN_WRITE("Campaign=%s\n", campaign->id);
    if(ok) CAMPAIGN_WRITE("Scene=%s\n", progress->scene);
    if(ok) CAMPAIGN_WRITE("Checkpoint=%s\n", progress->checkpoint);
    if(ok) CAMPAIGN_WRITE("QuestFlags=%lu\n", (unsigned long)progress->quest_flags);
    if(ok) CAMPAIGN_WRITE("Achievements=%lu\n", (unsigned long)progress->achievements);
#undef CAMPAIGN_WRITE
    if(ok) ok = storage_file_sync(file);
    storage_file_close(file);
    storage_file_free(file);
    if(!ok) return false;
    return dndadventure_campaigns_active_save(storage, profile_id, campaign->id);
}

bool dndadventure_campaigns_progress_load(
    Storage* storage,
    uint32_t profile_id,
    const PocketCampaignSummary* campaign,
    PocketCampaignProgress* progress) {
    memset(progress, 0, sizeof(*progress));
    dndadventure_campaigns_copy(progress->campaign, sizeof(progress->campaign), campaign->id);
    dndadventure_campaigns_copy(progress->scene, sizeof(progress->scene), campaign->entry_scene);
    dndadventure_campaigns_copy(
        progress->checkpoint, sizeof(progress->checkpoint), campaign->entry_scene);
    char path[POCKET_D20_PATH_LEN];
    if(!dndadventure_campaigns_progress_path(path, sizeof(path), profile_id, campaign->id, "txt"))
        return false;
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return true;
    }
    CampaignReader reader;
    dndadventure_campaigns_reader_init(&reader, file, 0U);
    char line[CAMPAIGN_LINE_LEN];
    while(dndadventure_campaigns_read_line(&reader, line, sizeof(line), NULL)) {
        char* value = strchr(line, '=');
        if(!value) continue;
        *value++ = '\0';
        if(!strcmp(line, "Scene"))
            dndadventure_campaigns_copy(progress->scene, sizeof(progress->scene), value);
        else if(!strcmp(line, "Checkpoint"))
            dndadventure_campaigns_copy(progress->checkpoint, sizeof(progress->checkpoint), value);
        else if(!strcmp(line, "QuestFlags")) {
            uint32_t parsed = 0U;
            if(dndadventure_campaigns_parse_u32(value, UINT32_MAX, &parsed))
                progress->quest_flags = parsed;
        } else if(!strcmp(line, "Achievements")) {
            uint32_t parsed = 0U;
            if(dndadventure_campaigns_parse_u32(value, UINT32_MAX, &parsed))
                progress->achievements = parsed;
        } else if(!strcmp(line, "Flags")) {
            /* Older Adventure progress may have packed these two values. Keep this
               one compatibility read while all new writes use independent fields. */
            const char* comma = strchr(value, ',');
            uint32_t quests = 0U, achievements = 0U;
            if(comma && !strchr(comma + 1U, ',') &&
               dndadventure_campaigns_parse_u32_span(value, comma, UINT32_MAX, &quests) &&
               dndadventure_campaigns_parse_u32(comma + 1U, UINT32_MAX, &achievements)) {
                progress->quest_flags = quests;
                progress->achievements = achievements;
            }
        }
    }
    bool io_ok = storage_file_get_error(file) == FSE_OK;
    storage_file_close(file);
    storage_file_free(file);
    return io_ok;
}

static bool dndadventure_campaigns_scene_present(
    char (*ids)[POCKET_D20_SHORT_LEN],
    uint8_t count,
    const char* id) {
    if(!id[0] || !strcmp(id, "-")) return true;
    for(uint8_t i = 0U; i < count; ++i)
        if(!strcmp(ids[i], id)) return true;
    return false;
}

static void dndadventure_campaigns_note_problem(
    PocketCampaignDiagnostics* output,
    const char* id,
    const char* problem) {
    if(output->problem_id[0]) return;
    dndadventure_campaigns_copy(output->problem_id, sizeof(output->problem_id), id);
    dndadventure_campaigns_copy(output->problem, sizeof(output->problem), problem);
}

static void dndadventure_campaigns_validate_scenes(
    Storage* storage,
    const PocketCampaignSummary* campaign,
    PocketCampaignDiagnostics* output) {
    char path[POCKET_D20_PATH_LEN];
    if(!dndadventure_campaigns_scene_path(storage, campaign, path, sizeof(path))) {
        ++output->missing_scene_files;
        dndadventure_campaigns_note_problem(output, campaign->id, "Scene file missing");
        return;
    }
    char(*scene_ids)[POCKET_D20_SHORT_LEN] = calloc(CAMPAIGN_MAX_SCENES, POCKET_D20_SHORT_LEN);
    if(!scene_ids) {
        dndadventure_campaigns_note_problem(output, campaign->id, "Diagnostics memory low");
        return;
    }
    File* file = storage_file_alloc(storage);
    if(!file) {
        free(scene_ids);
        dndadventure_campaigns_note_problem(output, campaign->id, "Diagnostics memory low");
        return;
    }
    char* line = malloc(CAMPAIGN_LINE_LEN);
    if(!line) {
        storage_file_free(file);
        free(scene_ids);
        dndadventure_campaigns_note_problem(output, campaign->id, "Diagnostics memory low");
        return;
    }
    uint8_t scene_count = 0U;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        CampaignReader reader;
        dndadventure_campaigns_reader_init(&reader, file, 0U);
        while(dndadventure_campaigns_read_line(&reader, line, CAMPAIGN_LINE_LEN, NULL)) {
            char* fields[11];
            uint8_t count = dndadventure_campaigns_split(line, fields, 11U);
            if(count != 5U || strcmp(fields[0], "S")) continue;
            if(dndadventure_campaigns_scene_present(scene_ids, scene_count, fields[1])) {
                ++output->duplicate_scene_ids;
                dndadventure_campaigns_note_problem(output, campaign->id, "Duplicate scene ID");
            } else if(scene_count < CAMPAIGN_MAX_SCENES) {
                dndadventure_campaigns_copy(
                    scene_ids[scene_count++], POCKET_D20_SHORT_LEN, fields[1]);
            }
        }
    }
    storage_file_close(file);
    if(!dndadventure_campaigns_scene_present(scene_ids, scene_count, campaign->entry_scene)) {
        ++output->missing_entry_scenes;
        dndadventure_campaigns_note_problem(output, campaign->id, "Entry scene missing");
    }
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        CampaignReader reader;
        dndadventure_campaigns_reader_init(&reader, file, 0U);
        while(dndadventure_campaigns_read_line(&reader, line, CAMPAIGN_LINE_LEN, NULL)) {
            char* fields[11];
            uint8_t count = dndadventure_campaigns_split(line, fields, 11U);
            if(count != 11U || strcmp(fields[0], "C")) continue;
            if(!dndadventure_campaigns_scene_present(scene_ids, scene_count, fields[5]) ||
               !dndadventure_campaigns_scene_present(scene_ids, scene_count, fields[6])) {
                ++output->broken_links;
                dndadventure_campaigns_note_problem(output, campaign->id, "Broken scene link");
            }
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    free(line);
    free(scene_ids);
}

static bool dndadventure_campaigns_path_has_id_before(
    Storage* storage,
    const char* path,
    bool bundled,
    const char* id,
    uint16_t record_limit) {
    if(!storage || !path || !id || !id[0] || !record_limit) return false;
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    char* line = malloc(CAMPAIGN_LINE_LEN);
    if(!line) {
        storage_file_free(file);
        return false;
    }
    bool found = false;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        CampaignReader reader;
        dndadventure_campaigns_reader_init(&reader, file, 0U);
        uint16_t records = 0U;
        while(records < record_limit &&
              dndadventure_campaigns_read_line(&reader, line, CAMPAIGN_LINE_LEN, NULL)) {
            PocketCampaignSummary campaign;
            if(!dndadventure_campaigns_parse(line, bundled, &campaign)) continue;
            if(!strcmp(campaign.id, id)) {
                found = true;
                break;
            }
            if(records < UINT16_MAX) ++records;
        }
        storage_file_close(file);
    }
    free(line);
    storage_file_free(file);
    return found;
}

static bool dndadventure_campaigns_id_seen_before(
    Storage* storage,
    const char* current_path,
    bool current_bundled,
    uint8_t path_rank,
    uint16_t prior_records,
    const char* id) {
    /* Diagnostics is an explicit maintenance action, so prefer bounded heap over
       retaining one campaign ID per manifest. Re-scan earlier records instead.
       Rank order is bundled -> enabled -> user. */
    if(path_rank > 0U && dndadventure_campaigns_path_has_id_before(
                             storage, CAMPAIGN_BUNDLED_INDEX, true, id, UINT16_MAX))
        return true;
    if(path_rank > 1U && dndadventure_campaigns_path_has_id_before(
                             storage, CAMPAIGN_ENABLED_INDEX, false, id, UINT16_MAX))
        return true;
    return dndadventure_campaigns_path_has_id_before(
        storage, current_path, current_bundled, id, prior_records);
}

static void dndadventure_campaigns_diagnose_path(
    Storage* storage,
    const char* path,
    bool bundled,
    uint8_t path_rank,
    PocketCampaignDiagnostics* output) {
    File* file = storage_file_alloc(storage);
    if(!file) return;
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return;
    }
    char* line = malloc(CAMPAIGN_LINE_LEN);
    if(!line) {
        storage_file_close(file);
        storage_file_free(file);
        dndadventure_campaigns_note_problem(output, "diagnostics", "Diagnostics memory low");
        return;
    }
    CampaignReader reader;
    dndadventure_campaigns_reader_init(&reader, file, 0U);
    uint16_t prior_records = 0U;
    while(dndadventure_campaigns_read_line(&reader, line, CAMPAIGN_LINE_LEN, NULL)) {
        PocketCampaignSummary campaign;
        if(!dndadventure_campaigns_parse(line, bundled, &campaign)) continue;
        if(output->records < UINT16_MAX) ++output->records;
        if(campaign.pack_version != POCKET_CAMPAIGN_PACK_VERSION ||
           campaign.minimum_app > POCKET_CAMPAIGN_APP_VERSION ||
           (campaign.maximum_app && campaign.maximum_app < POCKET_CAMPAIGN_APP_VERSION)) {
            ++output->incompatible;
            dndadventure_campaigns_note_problem(output, campaign.id, "Incompatible manifest");
        }
        if(dndadventure_campaigns_id_seen_before(
               storage, path, bundled, path_rank, prior_records, campaign.id)) {
            ++output->duplicate_campaign_ids;
            dndadventure_campaigns_note_problem(output, campaign.id, "Duplicate campaign ID");
        }
        if(prior_records < UINT16_MAX) ++prior_records;
        dndadventure_campaigns_validate_scenes(storage, &campaign, output);
    }
    storage_file_close(file);
    storage_file_free(file);
    free(line);
}

void dndadventure_campaigns_diagnose(Storage* storage, PocketCampaignDiagnostics* output) {
    if(!output) return;
    memset(output, 0, sizeof(*output));
    if(!storage) {
        dndadventure_campaigns_note_problem(output, "diagnostics", "Storage unavailable");
        return;
    }
    dndadventure_campaigns_diagnose_path(storage, CAMPAIGN_BUNDLED_INDEX, true, 0U, output);
    dndadventure_campaigns_diagnose_path(storage, CAMPAIGN_ENABLED_INDEX, false, 1U, output);
    dndadventure_campaigns_diagnose_path(storage, CAMPAIGN_USER_INDEX, false, 2U, output);
}
