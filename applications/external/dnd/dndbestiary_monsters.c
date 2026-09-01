#include "dndbestiary_monsters.h"

#include <furi.h>
#include <furi_hal_random.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MONSTER_INDEX                 APP_ASSETS_PATH("monsters/index.txt")
#define MONSTER_BLOCKS                APP_ASSETS_PATH("monsters/statblocks.txt")
#define CUSTOM_MONSTER_INDEX          APP_DATA_PATH("monsters/custom_index.txt")
#define CUSTOM_MONSTER_INDEX_TEMP     APP_DATA_PATH("monsters/custom_index.tmp")
#define CUSTOM_MONSTER_INDEX_BACKUP   APP_DATA_PATH("monsters/custom_index.bak")
#define CUSTOM_MONSTER_BLOCKS         APP_DATA_PATH("monsters/custom_statblocks.txt")
#define CUSTOM_MONSTER_BLOCKS_TEMP    APP_DATA_PATH("monsters/custom_statblocks.tmp")
#define CUSTOM_MONSTER_BLOCKS_BACKUP  APP_DATA_PATH("monsters/custom_statblocks.bak")
#define CUSTOM_MONSTER_TRANSACTION    APP_DATA_PATH("monsters/custom_transaction.txt")
#define ENABLED_MONSTER_INDEX         APP_DATA_PATH("monsters/enabled_index.txt")
#define ENABLED_MONSTER_BLOCKS        APP_DATA_PATH("monsters/enabled_statblocks.txt")
#define LEGACY_CUSTOM_MONSTER_INDEX   APP_ASSETS_PATH("monsters/custom_index.txt")
#define LEGACY_CUSTOM_MONSTER_BLOCKS  APP_ASSETS_PATH("monsters/custom_statblocks.txt")
#define DEFAULT_CUSTOM_MONSTER_INDEX  APP_ASSETS_PATH("monsters/default_custom_index.txt")
#define DEFAULT_CUSTOM_MONSTER_BLOCKS APP_ASSETS_PATH("monsters/default_custom_statblocks.txt")
#define CUSTOM_MONSTER_MIGRATION      APP_DATA_PATH("monsters/custom_migration.txt")
#define MONSTER_LINE_LEN              768U
#define MONSTER_READ_BUFFER           512U

static const uint16_t dndbestiary_monsters_budget[20][3] = {
    {50, 75, 100},       {100, 150, 200},     {150, 225, 400},      {250, 375, 500},
    {500, 750, 1100},    {600, 1000, 1400},   {750, 1300, 1700},    {1000, 1700, 2100},
    {1300, 2000, 2600},  {1600, 2300, 3100},  {1900, 2900, 4100},   {2200, 3700, 4700},
    {2600, 4200, 5400},  {2900, 4900, 6200},  {3300, 5400, 7800},   {3800, 6100, 9800},
    {4500, 7200, 11700}, {5000, 8700, 14200}, {5500, 10700, 17200}, {6400, 13200, 22000},
};

static void dndbestiary_monsters_copy(char* out, size_t size, const char* value) {
    if(!size) return;
    strncpy(out, value ? value : "", size - 1U);
    out[size - 1U] = '\0';
}

static bool dndbestiary_monsters_parse_u32(const char* text, uint32_t maximum, uint32_t* output) {
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

static bool dndbestiary_monsters_parse_i8(const char* text, int8_t* output) {
    if(!text || !text[0] || !output) return false;
    bool negative = false;
    if(*text == '-') {
        negative = true;
        ++text;
        if(!text[0]) return false;
    }
    uint16_t maximum = negative ? 128U : 127U;
    uint16_t value = 0U;
    for(const char* cursor = text; *cursor; ++cursor) {
        if(*cursor < '0' || *cursor > '9') return false;
        uint8_t digit = (uint8_t)(*cursor - '0');
        if(value > maximum / 10U || (value == maximum / 10U && digit > maximum % 10U))
            return false;
        value = (uint16_t)(value * 10U + digit);
    }
    *output = negative ? (value == 128U ? INT8_MIN : (int8_t) - (int16_t)value) : (int8_t)value;
    return true;
}

static bool dndbestiary_monsters_parse_abilities(const char* text, int8_t abilities[6]) {
    if(!text || !abilities) return false;
    const char* cursor = text;
    for(uint8_t index = 0U; index < 6U; ++index) {
        const char* separator = strchr(cursor, ',');
        const char* end = separator ? separator : cursor + strlen(cursor);
        if(end <= cursor || (index < 5U && !separator) || (index == 5U && separator)) return false;
        uint32_t value = 0U;
        for(const char* digit_cursor = cursor; digit_cursor < end; ++digit_cursor) {
            if(*digit_cursor < '0' || *digit_cursor > '9') return false;
            uint8_t digit = (uint8_t)(*digit_cursor - '0');
            if(value > 3U || (value == 3U && digit > 0U)) return false;
            value = value * 10U + digit;
        }
        if(value < 1U || value > 30U) return false;
        abilities[index] = (int8_t)value;
        cursor = separator ? separator + 1U : end;
    }
    return *cursor == '\0';
}
void dndbestiary_monsters_analyze_composition(
    const PocketMonsterEncounter* encounter,
    uint8_t party_size,
    PocketEncounterComposition* output) {
    if(!output) return;
    memset(output, 0, sizeof(*output));
    if(!encounter) return;

    for(uint8_t index = 0U; index < encounter->count; ++index) {
        uint16_t quantity = encounter->quantities[index];
        if(UINT16_MAX - output->total_creatures < quantity)
            output->total_creatures = UINT16_MAX;
        else
            output->total_creatures += quantity;

        const char* role = encounter->monsters[index].role;
        uint16_t* counter = NULL;
        if(!strcmp(role, "Leader"))
            counter = &output->leaders;
        else if(!strcmp(role, "Artillery"))
            counter = &output->artillery;
        else if(!strcmp(role, "Minion")) {
            counter = &output->minions;
            if(UINT16_MAX - output->frontline < quantity)
                output->frontline = UINT16_MAX;
            else
                output->frontline += quantity;
        } else if(!strcmp(role, "Brute") || !strcmp(role, "Skirmisher")) {
            if(UINT16_MAX - output->frontline < quantity)
                output->frontline = UINT16_MAX;
            else
                output->frontline += quantity;
        }
        if(counter) {
            if(UINT16_MAX - *counter < quantity)
                *counter = UINT16_MAX;
            else
                *counter += quantity;
        }
    }

    uint16_t non_leaders = 0U;
    if(output->total_creatures >= output->leaders)
        non_leaders = (uint16_t)(output->total_creatures - output->leaders);
    if(output->leaders && non_leaders < (uint16_t)output->leaders * 2U)
        output->warning_flags |= PocketEncounterWarningUnsupportedLeader;
    if(output->artillery && !output->frontline)
        output->warning_flags |= PocketEncounterWarningExposedArtillery;
    if(output->minions > party_size && output->total_creatures &&
       (uint32_t)output->minions * 3U >= (uint32_t)output->total_creatures * 2U)
        output->warning_flags |= PocketEncounterWarningMinionDensity;
}

typedef struct {
    File* file;
    uint8_t buffer[MONSTER_READ_BUFFER];
    uint16_t position;
    uint16_t count;
    uint32_t offset;
    bool eof;
} MonsterReader;

typedef struct {
    uint32_t id_hash;
    uint32_t offset;
} MonsterOffsetHint;

#define MONSTER_SPARSE_STRIDE 32U
#define MONSTER_SPARSE_MAX    16U
#define MONSTER_RECENT_MAX    8U

typedef struct {
    /* One checkpoint per 32 valid index records. For the bundled ~340-monster
       catalog this needs only eleven offsets instead of a 512-entry table. */
    uint32_t sparse_offsets[MONSTER_SPARSE_MAX];
    uint16_t index_count;
    uint8_t sparse_count;
    MonsterOffsetHint recent_index[MONSTER_RECENT_MAX];
    uint8_t recent_index_count;
    uint8_t recent_index_next;
    MonsterOffsetHint recent_blocks[MONSTER_RECENT_MAX];
    uint8_t recent_block_count;
    uint8_t recent_block_next;
    bool index_valid;
} MonsterPathCache;

typedef struct {
    Storage* owner;
    MonsterPathCache bundled;
    MonsterPathCache custom;
    MonsterPathCache enabled;
} MonsterCache;

static MonsterCache monster_cache;

static void
    dndbestiary_monsters_reader_init_at(MonsterReader* reader, File* file, uint32_t offset) {
    memset(reader, 0, sizeof(*reader));
    reader->file = file;
    reader->offset = offset;
}

static void dndbestiary_monsters_reader_init(MonsterReader* reader, File* file) {
    dndbestiary_monsters_reader_init_at(reader, file, 0U);
}

static bool dndbestiary_monsters_read_line_at(
    MonsterReader* reader,
    char* line,
    size_t size,
    uint32_t* line_offset) {
    if(line_offset) *line_offset = reader->offset;
    size_t position = 0U;
    bool consumed = false;
    while(!reader->eof) {
        if(reader->position >= reader->count) {
            reader->count =
                (uint16_t)storage_file_read(reader->file, reader->buffer, sizeof(reader->buffer));
            reader->position = 0U;
            if(!reader->count) {
                reader->eof = true;
                break;
            }
        }
        char value = (char)reader->buffer[reader->position++];
        ++reader->offset;
        consumed = true;
        if(value == '\r') continue;
        if(value == '\n') break;
        if(position + 1U < size) line[position++] = value;
    }
    line[position] = '\0';
    return consumed;
}

static bool dndbestiary_monsters_read_line(MonsterReader* reader, char* line, size_t size) {
    return dndbestiary_monsters_read_line_at(reader, line, size, NULL);
}

static bool dndbestiary_monsters_parse_summary(char* line, PocketMonsterSummary* output) {
    if(!line[0] || line[0] == '#') return false;
    char* cursor = line;
    char* extended[10] = {0};
    uint8_t field_count = 0U;
    while(field_count < 10U) {
        extended[field_count++] = cursor;
        char* separator = strchr(cursor, '|');
        if(!separator) break;
        *separator = '\0';
        cursor = separator + 1U;
    }
    if(field_count != 10U) return false;
    memset(output, 0, sizeof(*output));
    dndbestiary_monsters_copy(output->id, sizeof(output->id), extended[0]);
    dndbestiary_monsters_copy(output->name, sizeof(output->name), extended[1]);
    uint32_t cr = 0U, xp = 0U, armor_class = 0U, hit_points = 0U;
    if(!dndbestiary_monsters_parse_u32(extended[2], UINT8_MAX, &cr) ||
       !dndbestiary_monsters_parse_u32(extended[3], UINT32_MAX, &xp) ||
       !dndbestiary_monsters_parse_u32(extended[4], UINT8_MAX, &armor_class) ||
       !dndbestiary_monsters_parse_u32(extended[5], UINT16_MAX, &hit_points))
        return false;
    output->cr_eighths = (uint8_t)cr;
    output->xp = xp;
    output->armor_class = (uint8_t)armor_class;
    output->hit_points = (uint16_t)hit_points;
    dndbestiary_monsters_copy(output->type, sizeof(output->type), extended[6]);
    dndbestiary_monsters_copy(output->environment, sizeof(output->environment), extended[7]);
    dndbestiary_monsters_copy(output->source, sizeof(output->source), extended[8]);
    dndbestiary_monsters_copy(output->role, sizeof(output->role), extended[9]);
    return output->id[0] && output->name[0] && output->xp && output->type[0] &&
           output->environment[0] && output->source[0] && output->role[0];
}

static uint32_t dndbestiary_monsters_id_hash(const char* id) {
    uint32_t hash = 2166136261UL;
    while(*id) {
        hash ^= (uint8_t)*id++;
        hash *= 16777619UL;
    }
    return hash;
}

static void dndbestiary_monsters_path_cache_clear(MonsterPathCache* cache) {
    memset(cache, 0, sizeof(*cache));
}

void dndbestiary_monsters_cache_reset(void) {
    dndbestiary_monsters_path_cache_clear(&monster_cache.bundled);
    dndbestiary_monsters_path_cache_clear(&monster_cache.custom);
    dndbestiary_monsters_path_cache_clear(&monster_cache.enabled);
    monster_cache.owner = NULL;
}

static void dndbestiary_monsters_custom_cache_reset(void) {
    dndbestiary_monsters_path_cache_clear(&monster_cache.custom);
}

static void dndbestiary_monsters_cache_prepare_owner(Storage* storage) {
    if(monster_cache.owner == storage) return;
    dndbestiary_monsters_cache_reset();
    monster_cache.owner = storage;
}

static void dndbestiary_monsters_hint_remember(
    MonsterOffsetHint* hints,
    uint8_t* count,
    uint8_t* next,
    uint32_t id_hash,
    uint32_t offset) {
    for(uint8_t i = 0U; i < *count; ++i) {
        if(hints[i].id_hash == id_hash && hints[i].offset == offset) return;
    }
    uint8_t slot = *count < MONSTER_RECENT_MAX ? (*count)++ : *next;
    hints[slot].id_hash = id_hash;
    hints[slot].offset = offset;
    *next = (uint8_t)((slot + 1U) % MONSTER_RECENT_MAX);
}

static void dndbestiary_monsters_index_hint_remember(
    MonsterPathCache* cache,
    uint32_t hash,
    uint32_t offset) {
    dndbestiary_monsters_hint_remember(
        cache->recent_index, &cache->recent_index_count, &cache->recent_index_next, hash, offset);
}

static void dndbestiary_monsters_block_hint_remember(
    MonsterPathCache* cache,
    uint32_t hash,
    uint32_t offset) {
    dndbestiary_monsters_hint_remember(
        cache->recent_blocks, &cache->recent_block_count, &cache->recent_block_next, hash, offset);
}

static bool dndbestiary_monsters_build_index_cache(
    Storage* storage,
    const char* path,
    MonsterPathCache* cache) {
    cache->index_count = 0U;
    cache->sparse_count = 0U;
    cache->recent_index_count = 0U;
    cache->recent_index_next = 0U;
    File* file = storage_file_alloc(storage);
    if(!file) {
        cache->index_valid = false;
        return false;
    }
    bool opened = storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING);
    bool ok = opened;
    if(opened) {
        char line[MONSTER_LINE_LEN];
        PocketMonsterSummary summary;
        MonsterReader reader;
        dndbestiary_monsters_reader_init(&reader, file);
        uint32_t line_offset = 0U;
        while(dndbestiary_monsters_read_line_at(&reader, line, sizeof(line), &line_offset)) {
            if(!dndbestiary_monsters_parse_summary(line, &summary)) continue;
            if((cache->index_count % MONSTER_SPARSE_STRIDE) == 0U &&
               cache->sparse_count < MONSTER_SPARSE_MAX)
                cache->sparse_offsets[cache->sparse_count++] = line_offset;
            if(cache->index_count < UINT16_MAX) ++cache->index_count;
        }
        if(storage_file_get_error(file) != FSE_OK) ok = false;
    } else {
        FileInfo info;
        bool exists = storage_common_stat(storage, path, &info) == FSE_OK;
        ok = !exists && strcmp(path, MONSTER_INDEX) != 0;
    }
    storage_file_close(file);
    storage_file_free(file);
    if(!ok) {
        cache->index_count = 0U;
        cache->sparse_count = 0U;
        cache->recent_index_count = 0U;
    }
    cache->index_valid = ok;
    return ok;
}

static bool dndbestiary_monsters_cache_ensure(Storage* storage) {
    dndbestiary_monsters_cache_prepare_owner(storage);
    if(!monster_cache.bundled.index_valid &&
       !dndbestiary_monsters_build_index_cache(storage, MONSTER_INDEX, &monster_cache.bundled))
        return false;
    if(!monster_cache.custom.index_valid &&
       !dndbestiary_monsters_build_index_cache(
           storage, CUSTOM_MONSTER_INDEX, &monster_cache.custom))
        return false;
    if(!monster_cache.enabled.index_valid &&
       !dndbestiary_monsters_build_index_cache(
           storage, ENABLED_MONSTER_INDEX, &monster_cache.enabled))
        return false;
    return true;
}

static void dndbestiary_monsters_sparse_start(
    const MonsterPathCache* cache,
    uint16_t wanted,
    uint32_t* offset,
    uint16_t* ordinal) {
    *offset = 0U;
    *ordinal = 0U;
    if(!cache->sparse_count) return;
    uint16_t checkpoint = (uint16_t)(wanted / MONSTER_SPARSE_STRIDE);
    if(checkpoint >= cache->sparse_count) checkpoint = (uint16_t)(cache->sparse_count - 1U);
    *offset = cache->sparse_offsets[checkpoint];
    *ordinal = (uint16_t)(checkpoint * MONSTER_SPARSE_STRIDE);
}

static uint16_t dndbestiary_monsters_count_path(Storage* storage, const char* path) {
    File* file = storage_file_alloc(storage);
    if(!file) return 0U;
    uint16_t count = 0U;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char line[MONSTER_LINE_LEN];
        PocketMonsterSummary summary;
        MonsterReader reader;
        dndbestiary_monsters_reader_init(&reader, file);
        while(dndbestiary_monsters_read_line(&reader, line, sizeof(line)))
            if(dndbestiary_monsters_parse_summary(line, &summary) && count < UINT16_MAX) ++count;
    }
    storage_file_close(file);
    storage_file_free(file);
    return count;
}

static bool dndbestiary_monsters_at_offset(
    Storage* storage,
    const char* path,
    uint32_t offset,
    PocketMonsterSummary* output) {
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    bool found = false;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING) &&
       storage_file_seek(file, offset, true)) {
        char line[MONSTER_LINE_LEN];
        MonsterReader reader;
        dndbestiary_monsters_reader_init_at(&reader, file, offset);
        found = dndbestiary_monsters_read_line(&reader, line, sizeof(line)) &&
                dndbestiary_monsters_parse_summary(line, output);
    }
    storage_file_close(file);
    storage_file_free(file);
    return found;
}

uint32_t dndbestiary_monsters_xp_budget(
    uint8_t party_level,
    uint8_t party_size,
    PocketEncounterDifficulty difficulty) {
    if(party_level < 1U) party_level = 1U;
    if(party_level > 20U) party_level = 20U;
    if(party_size < 1U) party_size = 1U;
    if(party_size > 12U) party_size = 12U;
    if(difficulty >= PocketEncounterDifficultyCount) difficulty = PocketEncounterModerate;
    return (uint32_t)dndbestiary_monsters_budget[party_level - 1U][difficulty] * party_size;
}

static void dndbestiary_monsters_validate_paths(
    Storage* storage,
    const char* index_path,
    const char* blocks_path,
    uint16_t* total,
    uint16_t* valid,
    uint16_t* invalid) {
    uint16_t index_total = dndbestiary_monsters_count_path(storage, index_path);
    uint16_t section_total = 0U;
    uint16_t valid_total = 0U;
    uint16_t present_fields = 0U;
    bool active = false;
    File* file = storage_file_alloc(storage);
    if(!file) {
        if(total) *total = index_total;
        if(invalid) *invalid = index_total;
        return;
    }
    if(storage_file_open(file, blocks_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char line[MONSTER_LINE_LEN];
        MonsterReader reader;
        dndbestiary_monsters_reader_init(&reader, file);
        while(dndbestiary_monsters_read_line(&reader, line, sizeof(line))) {
            size_t length = strlen(line);
            if(length > 2U && line[0] == '[' && line[length - 1U] == ']') {
                if(active) {
                    ++section_total;
                    if((present_fields & PocketMonsterRequiredFields) ==
                       PocketMonsterRequiredFields)
                        ++valid_total;
                }
                active = true;
                present_fields = 0U;
                continue;
            }
            if(!active) continue;
            char* separator = strchr(line, '=');
            if(!separator) continue;
            *separator = '\0';
            if(!strcmp(line, "SizeAlignment"))
                present_fields |= PocketMonsterFieldSize;
            else if(!strcmp(line, "Speed"))
                present_fields |= PocketMonsterFieldSpeed;
            else if(!strcmp(line, "Abilities"))
                present_fields |= PocketMonsterFieldAbilities;
            else if(!strcmp(line, "Senses"))
                present_fields |= PocketMonsterFieldSenses;
            else if(!strcmp(line, "Languages"))
                present_fields |= PocketMonsterFieldLanguages;
            else if(!strcmp(line, "Actions"))
                present_fields |= PocketMonsterFieldActions;
        }
        if(active) {
            ++section_total;
            if((present_fields & PocketMonsterRequiredFields) == PocketMonsterRequiredFields)
                ++valid_total;
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    uint16_t record_total = index_total > section_total ? index_total : section_total;
    if(valid_total > index_total) valid_total = index_total;
    uint16_t invalid_total = record_total - valid_total;
    if(total) *total = record_total;
    if(valid) *valid = valid_total;
    if(invalid) *invalid = invalid_total;
}

void dndbestiary_monsters_validate_pack(
    Storage* storage,
    uint16_t* total,
    uint16_t* valid,
    uint16_t* invalid) {
    uint16_t bundled_total = 0U;
    uint16_t bundled_valid = 0U;
    uint16_t bundled_invalid = 0U;
    uint16_t custom_total = 0U;
    uint16_t custom_valid = 0U;
    uint16_t custom_invalid = 0U;
    dndbestiary_monsters_validate_paths(
        storage, MONSTER_INDEX, MONSTER_BLOCKS, &bundled_total, &bundled_valid, &bundled_invalid);
    dndbestiary_monsters_validate_paths(
        storage,
        CUSTOM_MONSTER_INDEX,
        CUSTOM_MONSTER_BLOCKS,
        &custom_total,
        &custom_valid,
        &custom_invalid);
    uint16_t enabled_total = 0U;
    uint16_t enabled_valid = 0U;
    uint16_t enabled_invalid = 0U;
    dndbestiary_monsters_validate_paths(
        storage,
        ENABLED_MONSTER_INDEX,
        ENABLED_MONSTER_BLOCKS,
        &enabled_total,
        &enabled_valid,
        &enabled_invalid);
    uint32_t combined_total = (uint32_t)bundled_total + custom_total + enabled_total;
    uint32_t combined_valid = (uint32_t)bundled_valid + custom_valid + enabled_valid;
    uint32_t combined_invalid = (uint32_t)bundled_invalid + custom_invalid + enabled_invalid;
    if(total) *total = combined_total > UINT16_MAX ? UINT16_MAX : (uint16_t)combined_total;
    if(valid) *valid = combined_valid > UINT16_MAX ? UINT16_MAX : (uint16_t)combined_valid;
    if(invalid) *invalid = combined_invalid > UINT16_MAX ? UINT16_MAX : (uint16_t)combined_invalid;
}

static bool dndbestiary_monsters_find_path(
    Storage* storage,
    const char* path,
    MonsterPathCache* cache,
    const char* id,
    PocketMonsterSummary* output) {
    uint32_t hash = dndbestiary_monsters_id_hash(id);
    for(uint8_t i = 0U; i < cache->recent_index_count; ++i) {
        if(cache->recent_index[i].id_hash != hash) continue;
        if(dndbestiary_monsters_at_offset(storage, path, cache->recent_index[i].offset, output) &&
           !strcmp(output->id, id))
            return true;
    }

    File* file = storage_file_alloc(storage);
    if(!file) return false;
    bool found = false;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char line[MONSTER_LINE_LEN];
        MonsterReader reader;
        dndbestiary_monsters_reader_init(&reader, file);
        uint32_t line_offset = 0U;
        while(dndbestiary_monsters_read_line_at(&reader, line, sizeof(line), &line_offset)) {
            PocketMonsterSummary summary;
            if(!dndbestiary_monsters_parse_summary(line, &summary) || strcmp(summary.id, id))
                continue;
            *output = summary;
            dndbestiary_monsters_index_hint_remember(cache, hash, line_offset);
            found = true;
            break;
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    return found;
}

bool dndbestiary_monsters_find(Storage* storage, const char* id, PocketMonsterSummary* output) {
    if(!id || !output || !dndbestiary_monsters_cache_ensure(storage)) return false;
    if(dndbestiary_monsters_find_path(storage, MONSTER_INDEX, &monster_cache.bundled, id, output))
        return true;
    if(dndbestiary_monsters_find_path(
           storage, CUSTOM_MONSTER_INDEX, &monster_cache.custom, id, output))
        return true;
    return dndbestiary_monsters_find_path(
        storage, ENABLED_MONSTER_INDEX, &monster_cache.enabled, id, output);
}

static bool dndbestiary_monsters_initiative_modifier_path(
    Storage* storage,
    const char* path,
    const char* wanted_id,
    int8_t* modifier) {
    if(!storage || !path || !wanted_id || !wanted_id[0] || !modifier) return false;
    File* file = storage_file_alloc(storage);
    if(!file) return false;

    bool found = false;
    bool fallback_valid = false;
    int8_t fallback_modifier = 0;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char line[96];
        MonsterReader reader;
        dndbestiary_monsters_reader_init(&reader, file);
        bool active = false;
        while(dndbestiary_monsters_read_line(&reader, line, sizeof(line))) {
            size_t length = strlen(line);
            if(length > 2U && line[0] == '[' && line[length - 1U] == ']') {
                line[length - 1U] = '\0';
                if(active) break;
                active = !strcmp(line + 1U, wanted_id);
                continue;
            }
            if(!active) continue;
            if(!strncmp(line, "Initiative=", 11U)) {
                int8_t explicit_modifier = 0;
                if(dndbestiary_monsters_parse_i8(line + 11U, &explicit_modifier)) {
                    *modifier = explicit_modifier;
                    found = true;
                }
                break;
            }
            if(!strncmp(line, "Abilities=", 10U)) {
                int8_t abilities[6];
                if(dndbestiary_monsters_parse_abilities(line + 10U, abilities)) {
                    bool all_tens = true;
                    for(uint8_t index = 0U; index < 6U; ++index)
                        if(abilities[index] != 10) all_tens = false;
                    if(!all_tens) {
                        int16_t delta = (int16_t)abilities[1] - 10;
                        fallback_modifier = delta >= 0 ? (int8_t)(delta / 2) :
                                                         (int8_t) - ((1 - delta) / 2);
                        fallback_valid = true;
                    }
                }
            }
        }
        if(storage_file_get_error(file) != FSE_OK) {
            found = false;
            fallback_valid = false;
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    if(!found && fallback_valid) {
        *modifier = fallback_modifier;
        found = true;
    }
    return found;
}

bool dndbestiary_monsters_initiative_modifier(
    Storage* storage,
    const PocketMonsterSummary* summary,
    int8_t* modifier) {
    if(!modifier) return false;

    /* Initiative handoff must never be blocked just because a monster lacks
       trustworthy initiative metadata. Default to +0, then replace it only
       when an explicit Initiative= value or a usable Dexterity score exists. */
    *modifier = 0;
    if(!storage || !summary || !summary->id[0]) return true;

    if(!strcmp(summary->source, "Custom Pack")) {
        dndbestiary_monsters_initiative_modifier_path(
            storage, ENABLED_MONSTER_BLOCKS, summary->id, modifier);
        return true;
    }
    if(strcmp(summary->source, "Custom")) {
        dndbestiary_monsters_initiative_modifier_path(
            storage, MONSTER_BLOCKS, summary->id, modifier);
        return true;
    }
    if(dndbestiary_monsters_initiative_modifier_path(
           storage, CUSTOM_MONSTER_BLOCKS, summary->id, modifier))
        return true;
    dndbestiary_monsters_initiative_modifier_path(storage, MONSTER_BLOCKS, summary->id, modifier);
    return true;
}

static void dndbestiary_monsters_query_path(
    Storage* storage,
    const char* path,
    PocketMonsterFilter filter,
    void* context,
    uint16_t start,
    PocketMonsterSummary* output,
    uint16_t capacity,
    uint16_t* matched,
    uint16_t* loaded,
    bool count_all) {
    File* file = storage_file_alloc(storage);
    if(!file) return;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char line[MONSTER_LINE_LEN];
        MonsterReader reader;
        dndbestiary_monsters_reader_init(&reader, file);
        while(dndbestiary_monsters_read_line(&reader, line, sizeof(line))) {
            PocketMonsterSummary summary;
            if(!dndbestiary_monsters_parse_summary(line, &summary) ||
               (filter && !filter(&summary, context)))
                continue;
            if(*matched >= start && *loaded < capacity && output) output[(*loaded)++] = summary;
            if(*matched < UINT16_MAX) ++*matched;
            if(!count_all && *loaded >= capacity) break;
        }
    }
    storage_file_close(file);
    storage_file_free(file);
}

static void dndbestiary_monsters_query_sparse_path(
    Storage* storage,
    const char* path,
    MonsterPathCache* cache,
    uint16_t start,
    PocketMonsterSummary* output,
    uint16_t capacity,
    uint16_t* loaded) {
    if(start >= cache->index_count || !output || *loaded >= capacity) return;
    File* file = storage_file_alloc(storage);
    if(!file) return;
    uint32_t offset = 0U;
    uint16_t ordinal = 0U;
    dndbestiary_monsters_sparse_start(cache, start, &offset, &ordinal);
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING) &&
       (!offset || storage_file_seek(file, offset, true))) {
        char line[MONSTER_LINE_LEN];
        MonsterReader reader;
        dndbestiary_monsters_reader_init_at(&reader, file, offset);
        uint32_t line_offset = offset;
        while(*loaded < capacity &&
              dndbestiary_monsters_read_line_at(&reader, line, sizeof(line), &line_offset)) {
            PocketMonsterSummary summary;
            if(!dndbestiary_monsters_parse_summary(line, &summary)) continue;
            if(ordinal++ < start) continue;
            output[(*loaded)++] = summary;
            dndbestiary_monsters_index_hint_remember(
                cache, dndbestiary_monsters_id_hash(summary.id), line_offset);
        }
    }
    storage_file_close(file);
    storage_file_free(file);
}

uint16_t dndbestiary_monsters_query(
    Storage* storage,
    PocketMonsterFilter filter,
    void* context,
    uint16_t start,
    PocketMonsterSummary* output,
    uint16_t capacity,
    uint16_t* total_matches) {
    if(!filter && dndbestiary_monsters_cache_ensure(storage)) {
        uint32_t combined = (uint32_t)monster_cache.bundled.index_count +
                            monster_cache.custom.index_count + monster_cache.enabled.index_count;
        if(total_matches) *total_matches = combined > UINT16_MAX ? UINT16_MAX : (uint16_t)combined;
        uint16_t loaded = 0U;
        if(start < monster_cache.bundled.index_count) {
            dndbestiary_monsters_query_sparse_path(
                storage, MONSTER_INDEX, &monster_cache.bundled, start, output, capacity, &loaded);
            if(loaded < capacity)
                dndbestiary_monsters_query_sparse_path(
                    storage,
                    CUSTOM_MONSTER_INDEX,
                    &monster_cache.custom,
                    0U,
                    output,
                    capacity,
                    &loaded);
            if(loaded < capacity)
                dndbestiary_monsters_query_sparse_path(
                    storage,
                    ENABLED_MONSTER_INDEX,
                    &monster_cache.enabled,
                    0U,
                    output,
                    capacity,
                    &loaded);
        } else {
            uint16_t custom_start = (uint16_t)(start - monster_cache.bundled.index_count);
            if(custom_start < monster_cache.custom.index_count) {
                dndbestiary_monsters_query_sparse_path(
                    storage,
                    CUSTOM_MONSTER_INDEX,
                    &monster_cache.custom,
                    custom_start,
                    output,
                    capacity,
                    &loaded);
                if(loaded < capacity)
                    dndbestiary_monsters_query_sparse_path(
                        storage,
                        ENABLED_MONSTER_INDEX,
                        &monster_cache.enabled,
                        0U,
                        output,
                        capacity,
                        &loaded);
            } else {
                dndbestiary_monsters_query_sparse_path(
                    storage,
                    ENABLED_MONSTER_INDEX,
                    &monster_cache.enabled,
                    (uint16_t)(custom_start - monster_cache.custom.index_count),
                    output,
                    capacity,
                    &loaded);
            }
        }
        return loaded;
    }
    uint16_t matched = 0U;
    uint16_t loaded = 0U;
    dndbestiary_monsters_query_path(
        storage,
        MONSTER_INDEX,
        filter,
        context,
        start,
        output,
        capacity,
        &matched,
        &loaded,
        total_matches != NULL);
    if(total_matches || loaded < capacity)
        dndbestiary_monsters_query_path(
            storage,
            CUSTOM_MONSTER_INDEX,
            filter,
            context,
            start,
            output,
            capacity,
            &matched,
            &loaded,
            total_matches != NULL);
    if(total_matches || loaded < capacity)
        dndbestiary_monsters_query_path(
            storage,
            ENABLED_MONSTER_INDEX,
            filter,
            context,
            start,
            output,
            capacity,
            &matched,
            &loaded,
            total_matches != NULL);
    if(total_matches) *total_matches = matched;
    return loaded;
}

static void dndbestiary_monsters_sample_path(
    Storage* storage,
    const char* path,
    PocketMonsterFilter filter,
    void* context,
    PocketMonsterSummary* output,
    uint16_t capacity,
    uint16_t* matched) {
    File* file = storage_file_alloc(storage);
    if(!file) return;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char line[MONSTER_LINE_LEN];
        MonsterReader reader;
        dndbestiary_monsters_reader_init(&reader, file);
        while(dndbestiary_monsters_read_line(&reader, line, sizeof(line))) {
            PocketMonsterSummary summary;
            if(!dndbestiary_monsters_parse_summary(line, &summary) ||
               (filter && !filter(&summary, context)))
                continue;
            uint32_t seen = (uint32_t)(*matched) + 1U;
            if(*matched < capacity)
                output[*matched] = summary;
            else {
                uint32_t replacement = furi_hal_random_get() % seen;
                if(replacement < capacity) output[replacement] = summary;
            }
            if(*matched < UINT16_MAX) ++*matched;
        }
    }
    storage_file_close(file);
    storage_file_free(file);
}

uint16_t dndbestiary_monsters_sample(
    Storage* storage,
    PocketMonsterFilter filter,
    void* context,
    PocketMonsterSummary* output,
    uint16_t capacity,
    uint16_t* total_matches) {
    uint16_t matched = 0U;
    if(capacity && output) {
        dndbestiary_monsters_sample_path(
            storage, MONSTER_INDEX, filter, context, output, capacity, &matched);
        dndbestiary_monsters_sample_path(
            storage, ENABLED_MONSTER_INDEX, filter, context, output, capacity, &matched);
        dndbestiary_monsters_sample_path(
            storage, CUSTOM_MONSTER_INDEX, filter, context, output, capacity, &matched);
    }
    if(total_matches) *total_matches = matched;
    return matched < capacity ? matched : capacity;
}

static void dndbestiary_monsters_apply_detail_line(char* line, PocketMonsterDetail* output) {
    char* separator = strchr(line, '=');
    if(!separator) return;
    *separator++ = '\0';
    if(!strcmp(line, "SizeAlignment")) {
        dndbestiary_monsters_copy(
            output->size_alignment, sizeof(output->size_alignment), separator);
        output->present_fields |= PocketMonsterFieldSize;
    } else if(!strcmp(line, "Speed")) {
        dndbestiary_monsters_copy(output->speed, sizeof(output->speed), separator);
        output->present_fields |= PocketMonsterFieldSpeed;
    } else if(!strcmp(line, "Abilities")) {
        if(dndbestiary_monsters_parse_abilities(separator, output->abilities))
            output->present_fields |= PocketMonsterFieldAbilities;
    } else if(!strcmp(line, "Initiative")) {
        if(dndbestiary_monsters_parse_i8(separator, &output->initiative_modifier)) {
            output->initiative_present = 1U;
            output->present_fields |= PocketMonsterFieldInitiative;
        }
    } else if(!strcmp(line, "Skills"))
        dndbestiary_monsters_copy(output->skills, sizeof(output->skills), separator);
    else if(!strcmp(line, "Defenses"))
        dndbestiary_monsters_copy(output->defenses, sizeof(output->defenses), separator);
    else if(!strcmp(line, "Senses")) {
        dndbestiary_monsters_copy(output->senses, sizeof(output->senses), separator);
        output->present_fields |= PocketMonsterFieldSenses;
    } else if(!strcmp(line, "Languages")) {
        dndbestiary_monsters_copy(output->languages, sizeof(output->languages), separator);
        output->present_fields |= PocketMonsterFieldLanguages;
    } else if(!strcmp(line, "Traits"))
        dndbestiary_monsters_copy(output->traits, sizeof(output->traits), separator);
    else if(!strcmp(line, "Actions")) {
        dndbestiary_monsters_copy(output->actions, sizeof(output->actions), separator);
        output->present_fields |= PocketMonsterFieldActions;
    } else if(!strcmp(line, "Extra"))
        dndbestiary_monsters_copy(output->extra, sizeof(output->extra), separator);
}

static bool dndbestiary_monsters_load_section_at(
    Storage* storage,
    const char* path,
    uint32_t offset,
    const char* wanted_id,
    PocketMonsterDetail* output) {
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    bool found = false;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING) &&
       storage_file_seek(file, offset, true)) {
        char line[MONSTER_LINE_LEN];
        MonsterReader reader;
        dndbestiary_monsters_reader_init_at(&reader, file, offset);
        if(dndbestiary_monsters_read_line(&reader, line, sizeof(line))) {
            size_t length = strlen(line);
            if(length > 2U && line[0] == '[' && line[length - 1U] == ']') {
                line[length - 1U] = '\0';
                found = !strcmp(line + 1U, wanted_id);
            }
        }
        while(found && dndbestiary_monsters_read_line(&reader, line, sizeof(line))) {
            size_t length = strlen(line);
            if(length > 2U && line[0] == '[' && line[length - 1U] == ']') break;
            dndbestiary_monsters_apply_detail_line(line, output);
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    return found;
}

static bool dndbestiary_monsters_load_section_streamed(
    Storage* storage,
    const char* path,
    MonsterPathCache* cache,
    const char* wanted_id,
    PocketMonsterDetail* output) {
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    bool opened = storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING);
    bool active = false;
    bool found = false;
    if(opened) {
        char line[MONSTER_LINE_LEN];
        MonsterReader reader;
        dndbestiary_monsters_reader_init(&reader, file);
        uint32_t line_offset = 0U;
        while(dndbestiary_monsters_read_line_at(&reader, line, sizeof(line), &line_offset)) {
            size_t length = strlen(line);
            if(length > 2U && line[0] == '[' && line[length - 1U] == ']') {
                line[length - 1U] = '\0';
                if(active) break;
                active = !strcmp(line + 1U, wanted_id);
                if(active) {
                    found = true;
                    dndbestiary_monsters_block_hint_remember(
                        cache, dndbestiary_monsters_id_hash(wanted_id), line_offset);
                }
                continue;
            }
            if(active) dndbestiary_monsters_apply_detail_line(line, output);
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    return opened && found;
}

static bool dndbestiary_monsters_load_section(
    Storage* storage,
    const char* path,
    MonsterPathCache* cache,
    const char* wanted_id,
    PocketMonsterDetail* output) {
    dndbestiary_monsters_cache_prepare_owner(storage);
    uint32_t hash = dndbestiary_monsters_id_hash(wanted_id);
    for(uint8_t i = 0U; i < cache->recent_block_count; ++i) {
        if(cache->recent_blocks[i].id_hash != hash) continue;
        if(dndbestiary_monsters_load_section_at(
               storage, path, cache->recent_blocks[i].offset, wanted_id, output))
            return true;
    }
    return dndbestiary_monsters_load_section_streamed(storage, path, cache, wanted_id, output);
}

static uint8_t
    dndbestiary_monsters_pack_version_path(Storage* storage, const char* path, bool* present) {
    File* file = storage_file_alloc(storage);
    uint8_t version = 0U;
    if(!present) return 0U;
    *present = false;
    if(!file) return 0U;
    *present = storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING);
    if(*present) {
        char line[MONSTER_LINE_LEN];
        MonsterReader reader;
        dndbestiary_monsters_reader_init(&reader, file);
        for(uint8_t i = 0U; i < 4U && dndbestiary_monsters_read_line(&reader, line, sizeof(line));
            ++i) {
            static const char prefix[] = "# MonsterPack=";
            if(!strncmp(line, prefix, sizeof(prefix) - 1U)) {
                uint32_t parsed = 0U;
                if(dndbestiary_monsters_parse_u32(line + sizeof(prefix) - 1U, UINT8_MAX, &parsed))
                    version = (uint8_t)parsed;
                break;
            }
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    return version;
}

void dndbestiary_monsters_pack_versions(
    Storage* storage,
    uint8_t* bundled_version,
    uint8_t* user_version,
    bool* user_present) {
    bool bundled_present = false;
    *bundled_version =
        dndbestiary_monsters_pack_version_path(storage, MONSTER_INDEX, &bundled_present);
    *user_version =
        dndbestiary_monsters_pack_version_path(storage, CUSTOM_MONSTER_INDEX, user_present);
}

bool dndbestiary_monsters_load(
    Storage* storage,
    const PocketMonsterSummary* summary,
    PocketMonsterDetail* output) {
    memset(output, 0, sizeof(*output));
    output->summary = *summary;
    if(!strcmp(summary->source, "Custom Pack"))
        return dndbestiary_monsters_load_section(
            storage, ENABLED_MONSTER_BLOCKS, &monster_cache.enabled, summary->id, output);
    if(strcmp(summary->source, "Custom"))
        return dndbestiary_monsters_load_section(
            storage, MONSTER_BLOCKS, &monster_cache.bundled, summary->id, output);
    if(dndbestiary_monsters_load_section(
           storage, CUSTOM_MONSTER_BLOCKS, &monster_cache.custom, summary->id, output))
        return true;
    memset(output, 0, sizeof(*output));
    output->summary = *summary;
    return dndbestiary_monsters_load_section(
        storage, MONSTER_BLOCKS, &monster_cache.bundled, summary->id, output);
}

static bool dndbestiary_monsters_write(File* file, const char* text) {
    size_t size = strlen(text);
    return storage_file_write(file, text, size) == size;
}

static void dndbestiary_monsters_safe_id(char* output, size_t size, const char* name) {
    size_t position = 0U;
    for(size_t i = 0U; name[i] && position + 1U < size; ++i) {
        char value = name[i];
        if(value >= 'A' && value <= 'Z') value = (char)(value + ('a' - 'A'));
        if((value >= 'a' && value <= 'z') || (value >= '0' && value <= '9'))
            output[position++] = value;
        else if(position && output[position - 1U] != '_')
            output[position++] = '_';
    }
    while(position && output[position - 1U] == '_')
        --position;
    if(!position)
        dndbestiary_monsters_copy(output, size, "custom_monster");
    else
        output[position] = '\0';
}

static bool dndbestiary_monsters_exists(Storage* storage, const char* path) {
    FileInfo info;
    return storage_common_stat(storage, path, &info) == FSE_OK;
}

static bool dndbestiary_monsters_remove_if_present(Storage* storage, const char* path) {
    return !dndbestiary_monsters_exists(storage, path) ||
           storage_common_remove(storage, path) == FSE_OK;
}

static bool dndbestiary_monsters_copy_storage_file(
    Storage* storage,
    const char* source,
    const char* temporary,
    const char* destination) {
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
    if(ok) ok = storage_file_get_error(input) == FSE_OK && storage_file_sync(output);
    storage_file_close(input);
    storage_file_close(output);
    storage_file_free(input);
    storage_file_free(output);
    if(!ok || storage_common_rename(storage, temporary, destination) != FSE_OK) {
        storage_common_remove(storage, temporary);
        return false;
    }
    return true;
}

bool dndbestiary_monsters_seed_default_custom(Storage* storage, uint16_t* copied_files) {
    furi_assert(storage);
    dndbestiary_monsters_custom_cache_reset();
    if(copied_files) *copied_files = 0U;

    /* Existing user data always wins. A partial custom pair is also left untouched so
       recovery or manual repair can preserve it rather than silently replacing it. */
    if(dndbestiary_monsters_exists(storage, CUSTOM_MONSTER_INDEX) ||
       dndbestiary_monsters_exists(storage, CUSTOM_MONSTER_BLOCKS))
        return true;

    if(!dndbestiary_monsters_exists(storage, DEFAULT_CUSTOM_MONSTER_INDEX) ||
       !dndbestiary_monsters_exists(storage, DEFAULT_CUSTOM_MONSTER_BLOCKS))
        return false;

    storage_common_mkdir(storage, APP_DATA_PATH(""));
    storage_common_mkdir(storage, APP_DATA_PATH("monsters"));

    const char* seed_index = APP_DATA_PATH("monsters/custom_index.seed");
    const char* seed_blocks = APP_DATA_PATH("monsters/custom_statblocks.seed");
    storage_common_remove(storage, seed_index);
    storage_common_remove(storage, seed_blocks);

    bool blocks_copied = dndbestiary_monsters_copy_storage_file(
        storage, DEFAULT_CUSTOM_MONSTER_BLOCKS, seed_blocks, CUSTOM_MONSTER_BLOCKS);
    bool index_copied =
        blocks_copied &&
        dndbestiary_monsters_copy_storage_file(
            storage, DEFAULT_CUSTOM_MONSTER_INDEX, seed_index, CUSTOM_MONSTER_INDEX);

    if(!blocks_copied || !index_copied) {
        storage_common_remove(storage, CUSTOM_MONSTER_INDEX);
        storage_common_remove(storage, CUSTOM_MONSTER_BLOCKS);
        storage_common_remove(storage, seed_index);
        storage_common_remove(storage, seed_blocks);
        dndbestiary_monsters_custom_cache_reset();
        return false;
    }

    if(copied_files) *copied_files = 2U;
    dndbestiary_monsters_custom_cache_reset();
    return true;
}

bool dndbestiary_monsters_migrate_legacy_custom(Storage* storage, uint16_t* copied_files) {
    furi_assert(storage);
    dndbestiary_monsters_custom_cache_reset();
    if(copied_files) *copied_files = 0U;
    bool data_index = dndbestiary_monsters_exists(storage, CUSTOM_MONSTER_INDEX);
    bool data_blocks = dndbestiary_monsters_exists(storage, CUSTOM_MONSTER_BLOCKS);
    bool pending = dndbestiary_monsters_exists(storage, CUSTOM_MONSTER_MIGRATION);
    if(data_index && data_blocks) {
        if(pending) storage_common_remove(storage, CUSTOM_MONSTER_MIGRATION);
        return dndbestiary_monsters_remove_if_present(storage, LEGACY_CUSTOM_MONSTER_INDEX) &&
               dndbestiary_monsters_remove_if_present(storage, LEGACY_CUSTOM_MONSTER_BLOCKS);
    }
    if((data_index || data_blocks) && !pending) return false;
    if(!dndbestiary_monsters_exists(storage, LEGACY_CUSTOM_MONSTER_INDEX) &&
       !dndbestiary_monsters_exists(storage, LEGACY_CUSTOM_MONSTER_BLOCKS))
        return true;
    if(!dndbestiary_monsters_exists(storage, LEGACY_CUSTOM_MONSTER_INDEX) ||
       !dndbestiary_monsters_exists(storage, LEGACY_CUSTOM_MONSTER_BLOCKS))
        return false;
    storage_common_mkdir(storage, APP_DATA_PATH(""));
    storage_common_mkdir(storage, APP_DATA_PATH("monsters"));
    if(!pending) {
        File* marker = storage_file_alloc(storage);
        if(!marker) return false;
        bool marked =
            storage_file_open(marker, CUSTOM_MONSTER_MIGRATION, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
            storage_file_write(marker, "MIGRATE\n", 8U) == 8U && storage_file_sync(marker);
        storage_file_close(marker);
        storage_file_free(marker);
        if(!marked) {
            storage_common_remove(storage, CUSTOM_MONSTER_MIGRATION);
            return false;
        }
    }
    const char* migration_index = APP_DATA_PATH("monsters/custom_index.migrate");
    const char* migration_blocks = APP_DATA_PATH("monsters/custom_statblocks.migrate");
    storage_common_remove(storage, migration_index);
    storage_common_remove(storage, migration_blocks);
    bool blocks_copied =
        data_blocks ||
        dndbestiary_monsters_copy_storage_file(
            storage, LEGACY_CUSTOM_MONSTER_BLOCKS, migration_blocks, CUSTOM_MONSTER_BLOCKS);
    bool index_copied =
        data_index ||
        (blocks_copied &&
         dndbestiary_monsters_copy_storage_file(
             storage, LEGACY_CUSTOM_MONSTER_INDEX, migration_index, CUSTOM_MONSTER_INDEX));
    if(!blocks_copied || !index_copied) {
        /* Roll back only files created by this migration attempt.  Existing data may
           be the surviving half of an interrupted earlier attempt and must be kept. */
        if(!data_index) storage_common_remove(storage, CUSTOM_MONSTER_INDEX);
        if(!data_blocks) storage_common_remove(storage, CUSTOM_MONSTER_BLOCKS);
        storage_common_remove(storage, migration_index);
        storage_common_remove(storage, migration_blocks);
        /* Keep the migration marker so the next launch retries safely. */
        return false;
    }
    storage_common_remove(storage, CUSTOM_MONSTER_MIGRATION);
    if(!dndbestiary_monsters_remove_if_present(storage, LEGACY_CUSTOM_MONSTER_INDEX) ||
       !dndbestiary_monsters_remove_if_present(storage, LEGACY_CUSTOM_MONSTER_BLOCKS))
        return false;
    if(copied_files) *copied_files = (uint16_t)((data_index ? 0U : 1U) + (data_blocks ? 0U : 1U));
    return true;
}

static bool dndbestiary_monsters_format_summary(
    const PocketMonsterSummary* summary,
    char* line,
    size_t size) {
    int length = snprintf(
        line,
        size,
        "%s|%s|%u|%lu|%u|%u|%s|%s|%s|%s",
        summary->id,
        summary->name,
        summary->cr_eighths,
        (unsigned long)summary->xp,
        summary->armor_class,
        summary->hit_points,
        summary->type,
        summary->environment,
        summary->source,
        summary->role);
    return length > 0 && (size_t)length < size;
}

static bool
    dndbestiary_monsters_write_block_section(File* block, const PocketMonsterDetail* detail) {
    bool ok = true;
    char line[MONSTER_LINE_LEN];
#define MONSTER_WRITE_FIELD(key, value)                                \
    do {                                                               \
        int length = snprintf(line, sizeof(line), key "=%s\n", value); \
        if(length <= 0 || (size_t)length >= sizeof(line) ||            \
           !dndbestiary_monsters_write(block, line))                   \
            ok = false;                                                \
    } while(false)
    int header_length = snprintf(line, sizeof(line), "[%s]\n", detail->summary.id);
    ok = header_length > 0 && (size_t)header_length < sizeof(line) &&
         dndbestiary_monsters_write(block, line);
    if(ok) MONSTER_WRITE_FIELD("Name", detail->summary.name);
    if(ok) {
        int length = snprintf(
            line,
            sizeof(line),
            "Summary=%u,%lu,%u,%u,%s,%s,%s,%s\n",
            detail->summary.cr_eighths,
            (unsigned long)detail->summary.xp,
            detail->summary.armor_class,
            detail->summary.hit_points,
            detail->summary.type,
            detail->summary.environment,
            detail->summary.source,
            detail->summary.role);
        ok = length > 0 && (size_t)length < sizeof(line) &&
             dndbestiary_monsters_write(block, line);
    }
    if(ok) MONSTER_WRITE_FIELD("SizeAlignment", detail->size_alignment);
    if(ok) MONSTER_WRITE_FIELD("Speed", detail->speed);
    if(ok) {
        int length = snprintf(
            line,
            sizeof(line),
            "Abilities=%d,%d,%d,%d,%d,%d\n",
            detail->abilities[0],
            detail->abilities[1],
            detail->abilities[2],
            detail->abilities[3],
            detail->abilities[4],
            detail->abilities[5]);
        ok = length > 0 && (size_t)length < sizeof(line) &&
             dndbestiary_monsters_write(block, line);
    }
    if(ok) {
        int8_t initiative = detail->initiative_modifier;
        if(!detail->initiative_present) {
            int16_t delta = (int16_t)detail->abilities[1] - 10;
            initiative = delta >= 0 ? (int8_t)(delta / 2) : (int8_t) - ((1 - delta) / 2);
        }
        int length = snprintf(line, sizeof(line), "Initiative=%d\n", initiative);
        ok = length > 0 && (size_t)length < sizeof(line) &&
             dndbestiary_monsters_write(block, line);
    }
    if(ok) MONSTER_WRITE_FIELD("Skills", detail->skills);
    if(ok) MONSTER_WRITE_FIELD("Defenses", detail->defenses);
    if(ok) MONSTER_WRITE_FIELD("Senses", detail->senses);
    if(ok) MONSTER_WRITE_FIELD("Languages", detail->languages);
    if(ok) MONSTER_WRITE_FIELD("Traits", detail->traits);
    if(ok) MONSTER_WRITE_FIELD("Actions", detail->actions);
    if(ok) MONSTER_WRITE_FIELD("Extra", detail->extra);
#undef MONSTER_WRITE_FIELD
    return ok;
}

static bool dndbestiary_monsters_rewrite_index(
    Storage* storage,
    const PocketMonsterSummary* replacement,
    const char* remove_id) {
    File* output = storage_file_alloc(storage);
    if(!output) return false;
    bool ok =
        storage_file_open(output, CUSTOM_MONSTER_INDEX_TEMP, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
        dndbestiary_monsters_write(
            output,
            "# MonsterPack=1\n# id|name|CR eighths|XP|AC|HP|type|environment|source|role\n");
    bool replaced = false;
    File* input = storage_file_alloc(storage);
    if(!input) {
        storage_file_close(output);
        storage_file_free(output);
        storage_common_remove(storage, CUSTOM_MONSTER_INDEX_TEMP);
        return false;
    }
    if(storage_file_open(input, CUSTOM_MONSTER_INDEX, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char line[MONSTER_LINE_LEN];
        MonsterReader reader;
        dndbestiary_monsters_reader_init(&reader, input);
        while(ok && dndbestiary_monsters_read_line(&reader, line, sizeof(line))) {
            PocketMonsterSummary current;
            if(!dndbestiary_monsters_parse_summary(line, &current)) continue;
            if(remove_id && !strcmp(current.id, remove_id)) continue;
            if(replacement && !strcmp(current.id, replacement->id)) {
                char formatted[384];
                ok = dndbestiary_monsters_format_summary(
                         replacement, formatted, sizeof(formatted)) &&
                     dndbestiary_monsters_write(output, formatted) &&
                     dndbestiary_monsters_write(output, "\n");
                replaced = true;
            } else {
                ok = dndbestiary_monsters_write(output, line) &&
                     dndbestiary_monsters_write(output, "\n");
            }
        }
    }
    storage_file_close(input);
    storage_file_free(input);
    if(ok && replacement && !replaced) {
        char formatted[384];
        ok = dndbestiary_monsters_format_summary(replacement, formatted, sizeof(formatted)) &&
             dndbestiary_monsters_write(output, formatted) &&
             dndbestiary_monsters_write(output, "\n");
    }
    storage_file_close(output);
    storage_file_free(output);
    if(!ok) {
        storage_common_remove(storage, CUSTOM_MONSTER_INDEX_TEMP);
        return false;
    }
    return true;
}

static bool dndbestiary_monsters_write_transaction(
    Storage* storage,
    const char* action,
    const char* value) {
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    bool ok =
        storage_file_open(file, CUSTOM_MONSTER_TRANSACTION, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
        dndbestiary_monsters_write(file, action) && dndbestiary_monsters_write(file, "|") &&
        dndbestiary_monsters_write(file, value) && dndbestiary_monsters_write(file, "\n");
    storage_file_close(file);
    storage_file_free(file);
    if(!ok) storage_common_remove(storage, CUSTOM_MONSTER_TRANSACTION);
    return ok;
}

static bool dndbestiary_monsters_rewrite_blocks(
    Storage* storage,
    const PocketMonsterDetail* replacement,
    const char* remove_id) {
    File* output = storage_file_alloc(storage);
    File* input = storage_file_alloc(storage);
    if(!output || !input) {
        if(output) storage_file_free(output);
        if(input) storage_file_free(input);
        storage_common_remove(storage, CUSTOM_MONSTER_BLOCKS_TEMP);
        return false;
    }
    bool ok =
        storage_file_open(output, CUSTOM_MONSTER_BLOCKS_TEMP, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    bool skip = false;
    if(ok && storage_file_open(input, CUSTOM_MONSTER_BLOCKS, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char line[MONSTER_LINE_LEN];
        MonsterReader reader;
        dndbestiary_monsters_reader_init(&reader, input);
        while(ok && dndbestiary_monsters_read_line(&reader, line, sizeof(line))) {
            size_t length = strlen(line);
            if(length > 2U && line[0] == '[' && line[length - 1U] == ']') {
                line[length - 1U] = '\0';
                skip = remove_id && !strcmp(line + 1U, remove_id);
                line[length - 1U] = ']';
            }
            if(!skip)
                ok = dndbestiary_monsters_write(output, line) &&
                     dndbestiary_monsters_write(output, "\n");
        }
    }
    storage_file_close(input);
    storage_file_free(input);
    if(ok && replacement)
        ok = dndbestiary_monsters_write(output, "\n") &&
             dndbestiary_monsters_write_block_section(output, replacement);
    storage_file_close(output);
    storage_file_free(output);
    if(!ok) storage_common_remove(storage, CUSTOM_MONSTER_BLOCKS_TEMP);
    return ok;
}

static bool dndbestiary_monsters_publish_pair(Storage* storage) {
    storage_common_remove(storage, CUSTOM_MONSTER_INDEX_BACKUP);
    storage_common_remove(storage, CUSTOM_MONSTER_BLOCKS_BACKUP);
    bool index_existed = dndbestiary_monsters_exists(storage, CUSTOM_MONSTER_INDEX);
    bool blocks_existed = dndbestiary_monsters_exists(storage, CUSTOM_MONSTER_BLOCKS);
    bool index_backed_up =
        !index_existed ||
        storage_common_rename(storage, CUSTOM_MONSTER_INDEX, CUSTOM_MONSTER_INDEX_BACKUP) ==
            FSE_OK;
    bool blocks_backed_up =
        !blocks_existed ||
        storage_common_rename(storage, CUSTOM_MONSTER_BLOCKS, CUSTOM_MONSTER_BLOCKS_BACKUP) ==
            FSE_OK;
    bool index_published = false;
    bool blocks_published = false;
    if(index_backed_up && blocks_backed_up) {
        blocks_published =
            storage_common_rename(storage, CUSTOM_MONSTER_BLOCKS_TEMP, CUSTOM_MONSTER_BLOCKS) ==
            FSE_OK;
        index_published = blocks_published &&
                          storage_common_rename(
                              storage, CUSTOM_MONSTER_INDEX_TEMP, CUSTOM_MONSTER_INDEX) == FSE_OK;
    }
    if(index_published && blocks_published) {
        storage_common_remove(storage, CUSTOM_MONSTER_INDEX_BACKUP);
        storage_common_remove(storage, CUSTOM_MONSTER_BLOCKS_BACKUP);
        return true;
    }
    if(index_published) storage_common_remove(storage, CUSTOM_MONSTER_INDEX);
    if(blocks_published) storage_common_remove(storage, CUSTOM_MONSTER_BLOCKS);
    if(index_existed && index_backed_up)
        storage_common_rename(storage, CUSTOM_MONSTER_INDEX_BACKUP, CUSTOM_MONSTER_INDEX);
    if(blocks_existed && blocks_backed_up)
        storage_common_rename(storage, CUSTOM_MONSTER_BLOCKS_BACKUP, CUSTOM_MONSTER_BLOCKS);
    storage_common_remove(storage, CUSTOM_MONSTER_INDEX_TEMP);
    storage_common_remove(storage, CUSTOM_MONSTER_BLOCKS_TEMP);
    return false;
}

static void dndbestiary_monsters_sanitize_summary(PocketMonsterSummary* summary) {
    char* fields[] = {
        summary->name, summary->type, summary->environment, summary->source, summary->role};
    for(size_t field = 0U; field < sizeof(fields) / sizeof(fields[0]); ++field)
        for(char* p = fields[field]; *p; ++p)
            if(*p == '|' || *p == '\n' || *p == '\r') *p = '-';
}

static bool dndbestiary_monsters_save_custom_common(
    Storage* storage,
    PocketMonsterDetail* detail,
    bool preserve_id) {
    dndbestiary_monsters_custom_cache_reset();
    storage_common_mkdir(storage, APP_DATA_PATH(""));
    storage_common_mkdir(storage, APP_DATA_PATH("monsters"));
    if(!preserve_id || !detail->summary.id[0]) {
        char base[20];
        dndbestiary_monsters_safe_id(base, sizeof(base), detail->summary.name);
        snprintf(
            detail->summary.id,
            sizeof(detail->summary.id),
            "%s_%04lx",
            base,
            (unsigned long)(furi_hal_random_get() & 0xFFFFU));
    }
    dndbestiary_monsters_copy(detail->summary.source, sizeof(detail->summary.source), "Custom");
    if(!detail->summary.role[0])
        dndbestiary_monsters_copy(
            detail->summary.role, sizeof(detail->summary.role), "Skirmisher");
    dndbestiary_monsters_sanitize_summary(&detail->summary);
    bool ok = dndbestiary_monsters_write_transaction(storage, "UPSERT", detail->summary.id) &&
              dndbestiary_monsters_rewrite_index(storage, &detail->summary, detail->summary.id) &&
              dndbestiary_monsters_rewrite_blocks(storage, detail, detail->summary.id) &&
              dndbestiary_monsters_publish_pair(storage);
    if(ok) {
        storage_common_remove(storage, CUSTOM_MONSTER_TRANSACTION);
    } else {
        storage_common_remove(storage, CUSTOM_MONSTER_INDEX_TEMP);
        storage_common_remove(storage, CUSTOM_MONSTER_BLOCKS_TEMP);
        storage_common_remove(storage, CUSTOM_MONSTER_TRANSACTION);
    }
    return ok;
}

bool dndbestiary_monsters_save_custom(Storage* storage, PocketMonsterDetail* detail) {
    return dndbestiary_monsters_save_custom_common(storage, detail, false);
}

bool dndbestiary_monsters_update_custom(Storage* storage, PocketMonsterDetail* detail) {
    return detail->summary.id[0] && !strcmp(detail->summary.source, "Custom") &&
           dndbestiary_monsters_save_custom_common(storage, detail, true);
}

bool dndbestiary_monsters_delete_custom(Storage* storage, const PocketMonsterSummary* summary) {
    if(!summary || strcmp(summary->source, "Custom")) return false;
    dndbestiary_monsters_custom_cache_reset();
    bool ok = dndbestiary_monsters_write_transaction(storage, "DELETE", summary->id) &&
              dndbestiary_monsters_rewrite_index(storage, NULL, summary->id) &&
              dndbestiary_monsters_rewrite_blocks(storage, NULL, summary->id) &&
              dndbestiary_monsters_publish_pair(storage);
    storage_common_remove(storage, CUSTOM_MONSTER_TRANSACTION);
    return ok;
}

bool dndbestiary_monsters_recover_user_pack(
    Storage* storage,
    uint16_t* recovered,
    uint16_t* rolled_back) {
    dndbestiary_monsters_custom_cache_reset();
    *recovered = 0U;
    *rolled_back = 0U;
    bool pending = dndbestiary_monsters_exists(storage, CUSTOM_MONSTER_TRANSACTION);
    bool index_backup = dndbestiary_monsters_exists(storage, CUSTOM_MONSTER_INDEX_BACKUP);
    bool blocks_backup = dndbestiary_monsters_exists(storage, CUSTOM_MONSTER_BLOCKS_BACKUP);
    if(!pending && !index_backup && !blocks_backup) return true;
    bool publish_complete = pending &&
                            dndbestiary_monsters_exists(storage, CUSTOM_MONSTER_INDEX) &&
                            dndbestiary_monsters_exists(storage, CUSTOM_MONSTER_BLOCKS) &&
                            !dndbestiary_monsters_exists(storage, CUSTOM_MONSTER_INDEX_TEMP) &&
                            !dndbestiary_monsters_exists(storage, CUSTOM_MONSTER_BLOCKS_TEMP);
    if(publish_complete) {
        storage_common_remove(storage, CUSTOM_MONSTER_INDEX_BACKUP);
        storage_common_remove(storage, CUSTOM_MONSTER_BLOCKS_BACKUP);
        storage_common_remove(storage, CUSTOM_MONSTER_TRANSACTION);
        *recovered = 1U;
        return true;
    }
    if(index_backup) {
        storage_common_remove(storage, CUSTOM_MONSTER_INDEX);
        if(storage_common_rename(storage, CUSTOM_MONSTER_INDEX_BACKUP, CUSTOM_MONSTER_INDEX) ==
           FSE_OK)
            ++*rolled_back;
    }
    if(blocks_backup) {
        storage_common_remove(storage, CUSTOM_MONSTER_BLOCKS);
        if(storage_common_rename(storage, CUSTOM_MONSTER_BLOCKS_BACKUP, CUSTOM_MONSTER_BLOCKS) ==
           FSE_OK)
            ++*rolled_back;
    }
    if(!index_backup && !dndbestiary_monsters_exists(storage, CUSTOM_MONSTER_INDEX))
        storage_common_remove(storage, CUSTOM_MONSTER_BLOCKS);
    if(!blocks_backup && !dndbestiary_monsters_exists(storage, CUSTOM_MONSTER_BLOCKS))
        storage_common_remove(storage, CUSTOM_MONSTER_INDEX);
    storage_common_remove(storage, CUSTOM_MONSTER_INDEX_TEMP);
    storage_common_remove(storage, CUSTOM_MONSTER_BLOCKS_TEMP);
    storage_common_remove(storage, CUSTOM_MONSTER_TRANSACTION);
    return true;
}

typedef struct {
    uint32_t budget;
    uint8_t party_level;
    PocketEncounterTemplate template_kind;
    const char* environment;
} MonsterGenerateFilter;

static bool
    dndbestiary_monsters_generate_filter(const PocketMonsterSummary* candidate, void* context) {
    const MonsterGenerateFilter* filter = context;
    if(candidate->xp > filter->budget) return false;
    if(filter->environment && strcmp(filter->environment, "Any") &&
       strcmp(candidate->environment, filter->environment))
        return false;
    if(filter->template_kind == PocketEncounterHorde &&
       candidate->cr_eighths > (uint8_t)(filter->party_level * 4U))
        return false;
    if(filter->template_kind == PocketEncounterElite &&
       candidate->cr_eighths < (uint8_t)(filter->party_level * 4U))
        return false;
    return true;
}

typedef struct {
    uint8_t candidates[POCKET_MONSTER_ENCOUNTER_MAX];
    uint8_t quantities[POCKET_MONSTER_ENCOUNTER_MAX];
    uint8_t count;
    uint8_t creature_count;
    uint32_t spent;
} MonsterEncounterPlan;

static int8_t
    dndbestiary_monsters_plan_find(const MonsterEncounterPlan* plan, uint8_t candidate_index) {
    for(uint8_t i = 0U; i < plan->count; ++i)
        if(plan->candidates[i] == candidate_index) return (int8_t)i;
    return -1;
}

static bool dndbestiary_monsters_plan_add(
    MonsterEncounterPlan* plan,
    uint8_t candidate_index,
    uint32_t xp,
    bool allow_repeats,
    uint8_t maximum_creatures,
    uint8_t maximum_types) {
    if(plan->creature_count >= maximum_creatures) return false;
    int8_t existing = dndbestiary_monsters_plan_find(plan, candidate_index);
    if(existing >= 0) {
        if(!allow_repeats || plan->quantities[(uint8_t)existing] == UINT8_MAX) return false;
        ++plan->quantities[(uint8_t)existing];
    } else {
        if(plan->count >= maximum_types || plan->count >= POCKET_MONSTER_ENCOUNTER_MAX)
            return false;
        plan->candidates[plan->count] = candidate_index;
        plan->quantities[plan->count++] = 1U;
    }
    ++plan->creature_count;
    plan->spent += xp;
    return true;
}

bool dndbestiary_monsters_generate(
    Storage* storage,
    uint8_t party_level,
    uint8_t party_size,
    PocketEncounterDifficulty difficulty,
    const char* environment,
    bool allow_repeats,
    PocketEncounterTemplate template_kind,
    const char* preferred_role,
    PocketMonsterEncounter* output) {
    memset(output, 0, sizeof(*output));
    output->budget = dndbestiary_monsters_xp_budget(party_level, party_size, difficulty);
    enum {
        MonsterCandidateWindow = 16U
    };
    PocketMonsterSummary* candidates =
        malloc(MonsterCandidateWindow * sizeof(PocketMonsterSummary));
    if(!candidates) return false;
    MonsterGenerateFilter filter = {
        .budget = output->budget,
        .party_level = party_level,
        .template_kind = template_kind,
        .environment = environment,
    };
    uint16_t total_eligible = 0U;
    uint16_t candidate_count = dndbestiary_monsters_sample(
        storage,
        dndbestiary_monsters_generate_filter,
        &filter,
        candidates,
        MonsterCandidateWindow,
        &total_eligible);
    if(!candidate_count || !total_eligible) {
        free(candidates);
        return false;
    }
    uint8_t maximum_creatures = party_size * 2U;
    if(maximum_creatures < 1U) maximum_creatures = 1U;
    uint8_t maximum_types = template_kind == PocketEncounterElite ? 2U : 3U;
    MonsterEncounterPlan best = {0};
    for(uint8_t trial = 0U; trial < 48U && best.spent < output->budget; ++trial) {
        MonsterEncounterPlan plan = {0};
        for(uint16_t attempt = 0U; attempt < 160U && plan.spent < output->budget; ++attempt) {
            uint8_t candidate_index = (uint8_t)(furi_hal_random_get() % candidate_count);
            const PocketMonsterSummary* candidate = &candidates[candidate_index];
            if(candidate->xp > output->budget - plan.spent) continue;
            if(preferred_role && strcmp(preferred_role, "Any") &&
               strcmp(candidate->role, preferred_role) && (furi_hal_random_get() % 4U))
                continue;
            dndbestiary_monsters_plan_add(
                &plan,
                candidate_index,
                candidate->xp,
                allow_repeats,
                maximum_creatures,
                maximum_types);
        }
        if(plan.spent > best.spent) best = plan;
    }
    uint32_t lower_budget =
        difficulty > PocketEncounterLow ?
            dndbestiary_monsters_xp_budget(
                party_level, party_size, (PocketEncounterDifficulty)(difficulty - 1U)) :
            0U;
    if(!best.count || (difficulty > PocketEncounterLow && best.spent <= lower_budget)) {
        free(candidates);
        return false;
    }
    output->count = best.count;
    output->spent = best.spent;
    for(uint8_t i = 0U; i < best.count; ++i) {
        output->monsters[i] = candidates[best.candidates[i]];
        output->quantities[i] = best.quantities[i];
    }
    free(candidates);
    return output->count > 0U;
}

void dndbestiary_monsters_simulate(
    PocketMonsterEncounter* encounter,
    uint8_t party_level,
    uint8_t party_size,
    PocketEncounterSimulation* output) {
    memset(output, 0, sizeof(*output));
    for(uint8_t index = 0U; index < encounter->count; ++index) {
        uint32_t record_xp = encounter->monsters[index].xp * encounter->quantities[index];
        if(UINT32_MAX - output->spent < record_xp)
            output->spent = UINT32_MAX;
        else
            output->spent += record_xp;
    }
    output->low_budget =
        dndbestiary_monsters_xp_budget(party_level, party_size, PocketEncounterLow);
    output->moderate_budget =
        dndbestiary_monsters_xp_budget(party_level, party_size, PocketEncounterModerate);
    output->high_budget =
        dndbestiary_monsters_xp_budget(party_level, party_size, PocketEncounterHigh);
    output->classification = PocketEncounterLow;
    if(output->spent > output->low_budget) output->classification = PocketEncounterModerate;
    if(output->spent > output->moderate_budget) output->classification = PocketEncounterHigh;
    encounter->spent = output->spent;
    encounter->budget = output->classification == PocketEncounterLow ? output->low_budget :
                        output->classification == PocketEncounterModerate ?
                                                                       output->moderate_budget :
                                                                       output->high_budget;
}
