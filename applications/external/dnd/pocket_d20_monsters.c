#include "pocket_d20_monsters.h"

#include <furi.h>
#include <furi_hal_random.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MONSTER_INDEX APP_ASSETS_PATH("monsters/index.txt")
#define MONSTER_BLOCKS APP_ASSETS_PATH("monsters/statblocks.txt")
#define CUSTOM_MONSTER_INDEX APP_ASSETS_PATH("monsters/custom_index.txt")
#define CUSTOM_MONSTER_INDEX_TEMP APP_ASSETS_PATH("monsters/custom_index.tmp")
#define CUSTOM_MONSTER_INDEX_BACKUP APP_ASSETS_PATH("monsters/custom_index.bak")
#define CUSTOM_MONSTER_BLOCKS APP_ASSETS_PATH("monsters/custom_statblocks.txt")
#define CUSTOM_MONSTER_BLOCKS_TEMP APP_ASSETS_PATH("monsters/custom_statblocks.tmp")
#define CUSTOM_MONSTER_BLOCKS_BACKUP APP_ASSETS_PATH("monsters/custom_statblocks.bak")
#define CUSTOM_MONSTER_TRANSACTION APP_ASSETS_PATH("monsters/custom_transaction.txt")
#define MONSTER_LINE_LEN 768U
#define MONSTER_READ_BUFFER 512U

static const uint16_t pocket_budget[20][3] = {
    {50,75,100},{100,150,200},{150,225,400},{250,375,500},{500,750,1100},
    {600,1000,1400},{750,1300,1700},{1000,1700,2100},{1300,2000,2600},
    {1600,2300,3100},{1900,2900,4100},{2200,3700,4700},{2600,4200,5400},
    {2900,4900,6200},{3300,5400,7800},{3800,6100,9800},{4500,7200,11700},
    {5000,8700,14200},{5500,10700,17200},{6400,13200,22000},
};

static void monster_copy(char* out, size_t size, const char* value) {
    if(!size) return;
    strncpy(out, value ? value : "", size - 1U);
    out[size - 1U] = '\0';
}

typedef struct {
    File* file;
    uint8_t buffer[MONSTER_READ_BUFFER];
    uint16_t position;
    uint16_t count;
    bool eof;
} MonsterReader;

static void monster_reader_init(MonsterReader* reader, File* file) {
    memset(reader, 0, sizeof(*reader));
    reader->file = file;
}

static bool monster_read_line(MonsterReader* reader, char* line, size_t size) {
    size_t position = 0U;
    bool consumed = false;
    while(!reader->eof) {
        if(reader->position >= reader->count) {
            reader->count = (uint16_t)storage_file_read(
                reader->file, reader->buffer, sizeof(reader->buffer));
            reader->position = 0U;
            if(!reader->count) {
                reader->eof = true;
                break;
            }
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

static bool monster_parse_summary(char* line, PocketMonsterSummary* output) {
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
    if(field_count < 8U) return false;
    memset(output, 0, sizeof(*output));
    monster_copy(output->id, sizeof(output->id), extended[0]);
    monster_copy(output->name, sizeof(output->name), extended[1]);
    output->cr_eighths = (uint8_t)strtoul(extended[2], NULL, 10);
    output->xp = strtoul(extended[3], NULL, 10);
    output->armor_class = (uint8_t)strtoul(extended[4], NULL, 10);
    output->hit_points = (uint16_t)strtoul(extended[5], NULL, 10);
    monster_copy(output->type, sizeof(output->type), extended[6]);
    monster_copy(output->environment, sizeof(output->environment), extended[7]);
    monster_copy(output->source, sizeof(output->source), field_count > 8U ? extended[8] : "Custom");
    monster_copy(output->role, sizeof(output->role), field_count > 9U ? extended[9] : "Any");
    return output->id[0] && output->name[0] && output->xp;
}

static uint16_t monster_count_path(Storage* storage, const char* path) {
    File* file = storage_file_alloc(storage);
    uint16_t count = 0U;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char line[MONSTER_LINE_LEN];
        PocketMonsterSummary summary;
        MonsterReader reader;
        monster_reader_init(&reader, file);
        while(monster_read_line(&reader, line, sizeof(line)))
            if(monster_parse_summary(line, &summary) && count < UINT16_MAX) ++count;
    }
    storage_file_close(file);
    storage_file_free(file);
    return count;
}

static bool monster_at_path(
    Storage* storage,
    const char* path,
    uint16_t wanted,
    PocketMonsterSummary* output) {
    File* file = storage_file_alloc(storage);
    bool found = false;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char line[MONSTER_LINE_LEN];
        uint16_t index = 0U;
        MonsterReader reader;
        monster_reader_init(&reader, file);
        while(monster_read_line(&reader, line, sizeof(line))) {
            PocketMonsterSummary summary;
            if(!monster_parse_summary(line, &summary)) continue;
            if(index++ == wanted) {
                *output = summary;
                found = true;
                break;
            }
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    return found;
}

uint32_t pocket_monster_xp_budget(
    uint8_t party_level,
    uint8_t party_size,
    PocketEncounterDifficulty difficulty) {
    if(party_level < 1U) party_level = 1U;
    if(party_level > 20U) party_level = 20U;
    if(party_size < 1U) party_size = 1U;
    if(party_size > 12U) party_size = 12U;
    if(difficulty >= PocketEncounterDifficultyCount) difficulty = PocketEncounterModerate;
    return (uint32_t)pocket_budget[party_level - 1U][difficulty] * party_size;
}

uint16_t pocket_monster_count(Storage* storage) {
    uint32_t total = monster_count_path(storage, MONSTER_INDEX);
    total += monster_count_path(storage, CUSTOM_MONSTER_INDEX);
    return total > UINT16_MAX ? UINT16_MAX : (uint16_t)total;
}

static void monster_validate_paths(
    Storage* storage,
    const char* index_path,
    const char* blocks_path,
    uint16_t* total,
    uint16_t* valid,
    uint16_t* invalid) {
    uint16_t index_total = monster_count_path(storage, index_path);
    uint16_t section_total = 0U;
    uint16_t valid_total = 0U;
    uint16_t present_fields = 0U;
    bool active = false;
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, blocks_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char line[MONSTER_LINE_LEN];
        MonsterReader reader;
        monster_reader_init(&reader, file);
        while(monster_read_line(&reader, line, sizeof(line))) {
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
            if(!strcmp(line, "SizeAlignment")) present_fields |= PocketMonsterFieldSize;
            else if(!strcmp(line, "Speed")) present_fields |= PocketMonsterFieldSpeed;
            else if(!strcmp(line, "Abilities")) present_fields |= PocketMonsterFieldAbilities;
            else if(!strcmp(line, "Senses")) present_fields |= PocketMonsterFieldSenses;
            else if(!strcmp(line, "Languages")) present_fields |= PocketMonsterFieldLanguages;
            else if(!strcmp(line, "Actions")) present_fields |= PocketMonsterFieldActions;
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

void pocket_monster_validate_pack(
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
    monster_validate_paths(
        storage,
        MONSTER_INDEX,
        MONSTER_BLOCKS,
        &bundled_total,
        &bundled_valid,
        &bundled_invalid);
    monster_validate_paths(
        storage,
        CUSTOM_MONSTER_INDEX,
        CUSTOM_MONSTER_BLOCKS,
        &custom_total,
        &custom_valid,
        &custom_invalid);
    uint32_t combined_total = (uint32_t)bundled_total + custom_total;
    uint32_t combined_valid = (uint32_t)bundled_valid + custom_valid;
    uint32_t combined_invalid = (uint32_t)bundled_invalid + custom_invalid;
    if(total) *total = combined_total > UINT16_MAX ? UINT16_MAX : (uint16_t)combined_total;
    if(valid) *valid = combined_valid > UINT16_MAX ? UINT16_MAX : (uint16_t)combined_valid;
    if(invalid)
        *invalid = combined_invalid > UINT16_MAX ? UINT16_MAX : (uint16_t)combined_invalid;
}

bool pocket_monster_at(Storage* storage, uint16_t index, PocketMonsterSummary* output) {
    uint16_t bundled_count = monster_count_path(storage, MONSTER_INDEX);
    if(index < bundled_count) return monster_at_path(storage, MONSTER_INDEX, index, output);
    return monster_at_path(storage, CUSTOM_MONSTER_INDEX, index - bundled_count, output);
}

static void monster_query_path(
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
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char line[MONSTER_LINE_LEN];
        MonsterReader reader;
        monster_reader_init(&reader, file);
        while(monster_read_line(&reader, line, sizeof(line))) {
            PocketMonsterSummary summary;
            if(!monster_parse_summary(line, &summary) ||
               (filter && !filter(&summary, context)))
                continue;
            if(*matched >= start && *loaded < capacity && output)
                output[(*loaded)++] = summary;
            if(*matched < UINT16_MAX) ++*matched;
            if(!count_all && *loaded >= capacity) break;
        }
    }
    storage_file_close(file);
    storage_file_free(file);
}

uint16_t pocket_monster_query(
    Storage* storage,
    PocketMonsterFilter filter,
    void* context,
    uint16_t start,
    PocketMonsterSummary* output,
    uint16_t capacity,
    uint16_t* total_matches) {
    uint16_t matched = 0U;
    uint16_t loaded = 0U;
    monster_query_path(
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
        monster_query_path(
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
    if(total_matches) *total_matches = matched;
    return loaded;
}

static void monster_sample_path(
    Storage* storage,
    const char* path,
    PocketMonsterFilter filter,
    void* context,
    PocketMonsterSummary* output,
    uint16_t capacity,
    uint16_t* matched) {
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char line[MONSTER_LINE_LEN];
        MonsterReader reader;
        monster_reader_init(&reader, file);
        while(monster_read_line(&reader, line, sizeof(line))) {
            PocketMonsterSummary summary;
            if(!monster_parse_summary(line, &summary) ||
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

uint16_t pocket_monster_sample(
    Storage* storage,
    PocketMonsterFilter filter,
    void* context,
    PocketMonsterSummary* output,
    uint16_t capacity,
    uint16_t* total_matches) {
    uint16_t matched = 0U;
    if(capacity && output) {
        monster_sample_path(
            storage,
            MONSTER_INDEX,
            filter,
            context,
            output,
            capacity,
            &matched);
        monster_sample_path(
            storage,
            CUSTOM_MONSTER_INDEX,
            filter,
            context,
            output,
            capacity,
            &matched);
    }
    if(total_matches) *total_matches = matched;
    return matched < capacity ? matched : capacity;
}

static bool monster_load_section(
    Storage* storage,
    const char* path,
    const char* wanted_id,
    PocketMonsterDetail* output) {
    File* file = storage_file_alloc(storage);
    bool opened = storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING);
    bool active = false;
    bool found = false;
    if(opened) {
        char line[MONSTER_LINE_LEN];
        MonsterReader reader;
        monster_reader_init(&reader, file);
        while(monster_read_line(&reader, line, sizeof(line))) {
            size_t length = strlen(line);
            if(length > 2U && line[0] == '[' && line[length - 1U] == ']') {
                line[length - 1U] = '\0';
                if(active) break;
                active = strcmp(line + 1U, wanted_id) == 0;
                if(active) found = true;
                continue;
            }
            if(!active) continue;
            char* separator = strchr(line, '=');
            if(!separator) continue;
            *separator++ = '\0';
            if(!strcmp(line, "SizeAlignment")) { monster_copy(output->size_alignment, sizeof(output->size_alignment), separator); output->present_fields |= PocketMonsterFieldSize; }
            else if(!strcmp(line, "Speed")) { monster_copy(output->speed, sizeof(output->speed), separator); output->present_fields |= PocketMonsterFieldSpeed; }
            else if(!strcmp(line, "Abilities")) { if(sscanf(separator, "%hhd,%hhd,%hhd,%hhd,%hhd,%hhd", &output->abilities[0], &output->abilities[1], &output->abilities[2], &output->abilities[3], &output->abilities[4], &output->abilities[5]) == 6) output->present_fields |= PocketMonsterFieldAbilities; }
            else if(!strcmp(line, "Skills")) monster_copy(output->skills, sizeof(output->skills), separator);
            else if(!strcmp(line, "Defenses")) monster_copy(output->defenses, sizeof(output->defenses), separator);
            else if(!strcmp(line, "Senses")) { monster_copy(output->senses, sizeof(output->senses), separator); output->present_fields |= PocketMonsterFieldSenses; }
            else if(!strcmp(line, "Languages")) { monster_copy(output->languages, sizeof(output->languages), separator); output->present_fields |= PocketMonsterFieldLanguages; }
            else if(!strcmp(line, "Traits")) monster_copy(output->traits, sizeof(output->traits), separator);
            else if(!strcmp(line, "Actions")) { monster_copy(output->actions, sizeof(output->actions), separator); output->present_fields |= PocketMonsterFieldActions; }
            else if(!strcmp(line, "Extra")) monster_copy(output->extra, sizeof(output->extra), separator);
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    return opened && found;
}

static uint8_t monster_pack_version_path(Storage* storage, const char* path, bool* present) {
    File* file = storage_file_alloc(storage);
    uint8_t version = 0U;
    *present = storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING);
    if(*present) {
        char line[MONSTER_LINE_LEN];
        MonsterReader reader;
        monster_reader_init(&reader, file);
        for(uint8_t i = 0U; i < 4U && monster_read_line(&reader, line, sizeof(line)); ++i)
            if(sscanf(line, "# MonsterPack=%hhu", &version) == 1) break;
    }
    storage_file_close(file);
    storage_file_free(file);
    return version;
}

void pocket_monster_pack_versions(
    Storage* storage,
    uint8_t* bundled_version,
    uint8_t* user_version,
    bool* user_present) {
    bool bundled_present = false;
    *bundled_version = monster_pack_version_path(storage, MONSTER_INDEX, &bundled_present);
    *user_version =
        monster_pack_version_path(storage, CUSTOM_MONSTER_INDEX, user_present);
}

bool pocket_monster_load(Storage* storage, const PocketMonsterSummary* summary, PocketMonsterDetail* output) {
    memset(output, 0, sizeof(*output));
    output->summary = *summary;
    const char* path = !strcmp(summary->source, "Custom") ?
                           CUSTOM_MONSTER_BLOCKS :
                           MONSTER_BLOCKS;
    return monster_load_section(storage, path, summary->id, output);
}

static bool monster_write(File* file, const char* text) {
    size_t size = strlen(text);
    return storage_file_write(file, text, size) == size;
}

static void monster_safe_id(char* output, size_t size, const char* name) {
    size_t position = 0U;
    for(size_t i = 0U; name[i] && position + 1U < size; ++i) {
        char value = name[i];
        if(value >= 'A' && value <= 'Z') value = (char)(value + ('a' - 'A'));
        if((value >= 'a' && value <= 'z') || (value >= '0' && value <= '9'))
            output[position++] = value;
        else if(position && output[position - 1U] != '_')
            output[position++] = '_';
    }
    while(position && output[position - 1U] == '_') --position;
    if(!position) monster_copy(output, size, "custom_monster");
    else output[position] = '\0';
}

static bool monster_exists(Storage* storage, const char* path) {
    FileInfo info;
    return storage_common_stat(storage, path, &info) == FSE_OK;
}

static bool monster_format_summary(
    const PocketMonsterSummary* summary,
    char* line,
    size_t size) {
    int length = snprintf(line, size, "%s|%s|%u|%lu|%u|%u|%s|%s|%s|%s",
        summary->id, summary->name, summary->cr_eighths, (unsigned long)summary->xp,
        summary->armor_class, summary->hit_points, summary->type, summary->environment,
        summary->source, summary->role);
    return length > 0 && (size_t)length < size;
}

static bool monster_write_block_section(File* block, const PocketMonsterDetail* detail) {
    bool ok = true;
    char line[MONSTER_LINE_LEN];
#define MONSTER_WRITE_FIELD(key, value) do { \
    int length = snprintf(line, sizeof(line), key "=%s\n", value); \
    if(length <= 0 || (size_t)length >= sizeof(line) || !monster_write(block, line)) ok = false; \
} while(false)
    int header_length = snprintf(line, sizeof(line), "[%s]\n", detail->summary.id);
    ok = header_length > 0 && (size_t)header_length < sizeof(line) && monster_write(block, line);
    if(ok) MONSTER_WRITE_FIELD("Name", detail->summary.name);
    if(ok) {
        int length = snprintf(line, sizeof(line),
            "Summary=%u,%lu,%u,%u,%s,%s,%s,%s\n", detail->summary.cr_eighths,
            (unsigned long)detail->summary.xp, detail->summary.armor_class,
            detail->summary.hit_points, detail->summary.type, detail->summary.environment,
            detail->summary.source, detail->summary.role);
        ok = length > 0 && (size_t)length < sizeof(line) && monster_write(block, line);
    }
    if(ok) MONSTER_WRITE_FIELD("SizeAlignment", detail->size_alignment);
    if(ok) MONSTER_WRITE_FIELD("Speed", detail->speed);
    if(ok) {
        int length = snprintf(line, sizeof(line), "Abilities=%d,%d,%d,%d,%d,%d\n",
            detail->abilities[0], detail->abilities[1], detail->abilities[2],
            detail->abilities[3], detail->abilities[4], detail->abilities[5]);
        ok = length > 0 && (size_t)length < sizeof(line) && monster_write(block, line);
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

static bool monster_rewrite_index(
    Storage* storage,
    const PocketMonsterSummary* replacement,
    const char* remove_id) {
    File* output = storage_file_alloc(storage);
    bool ok = storage_file_open(
                  output, CUSTOM_MONSTER_INDEX_TEMP, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
        monster_write(output, "# MonsterPack=1\n# id|name|CR eighths|XP|AC|HP|type|environment|source|role\n");
    bool replaced = false;
    File* input = storage_file_alloc(storage);
    if(storage_file_open(input, CUSTOM_MONSTER_INDEX, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char line[MONSTER_LINE_LEN];
        MonsterReader reader;
        monster_reader_init(&reader, input);
        while(ok && monster_read_line(&reader, line, sizeof(line))) {
            PocketMonsterSummary current;
            if(!monster_parse_summary(line, &current)) continue;
            if(remove_id && !strcmp(current.id, remove_id)) continue;
            if(replacement && !strcmp(current.id, replacement->id)) {
                char formatted[384];
                ok = monster_format_summary(replacement, formatted, sizeof(formatted)) &&
                     monster_write(output, formatted) && monster_write(output, "\n");
                replaced = true;
            } else {
                ok = monster_write(output, line) && monster_write(output, "\n");
            }
        }
    }
    storage_file_close(input);
    storage_file_free(input);
    if(ok && replacement && !replaced) {
        char formatted[384];
        ok = monster_format_summary(replacement, formatted, sizeof(formatted)) &&
             monster_write(output, formatted) && monster_write(output, "\n");
    }
    storage_file_close(output);
    storage_file_free(output);
    if(!ok) {
        storage_common_remove(storage, CUSTOM_MONSTER_INDEX_TEMP);
        return false;
    }
    return true;
}

static bool monster_write_transaction(Storage* storage, const char* action, const char* value) {
    File* file = storage_file_alloc(storage);
    bool ok = storage_file_open(
                  file, CUSTOM_MONSTER_TRANSACTION, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
        monster_write(file, action) && monster_write(file, "|") &&
        monster_write(file, value) && monster_write(file, "\n");
    storage_file_close(file);
    storage_file_free(file);
    if(!ok) storage_common_remove(storage, CUSTOM_MONSTER_TRANSACTION);
    return ok;
}

static bool monster_rewrite_blocks(
    Storage* storage,
    const PocketMonsterDetail* replacement,
    const char* remove_id) {
    File* output = storage_file_alloc(storage);
    bool ok = storage_file_open(
        output, CUSTOM_MONSTER_BLOCKS_TEMP, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    File* input = storage_file_alloc(storage);
    bool skip = false;
    if(ok &&
       storage_file_open(input, CUSTOM_MONSTER_BLOCKS, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char line[MONSTER_LINE_LEN];
        MonsterReader reader;
        monster_reader_init(&reader, input);
        while(ok && monster_read_line(&reader, line, sizeof(line))) {
            size_t length = strlen(line);
            if(length > 2U && line[0] == '[' && line[length - 1U] == ']') {
                line[length - 1U] = '\0';
                skip = remove_id && !strcmp(line + 1U, remove_id);
                line[length - 1U] = ']';
            }
            if(!skip) ok = monster_write(output, line) && monster_write(output, "\n");
        }
    }
    storage_file_close(input);
    storage_file_free(input);
    if(ok && replacement)
        ok = monster_write(output, "\n") && monster_write_block_section(output, replacement);
    storage_file_close(output);
    storage_file_free(output);
    if(!ok) storage_common_remove(storage, CUSTOM_MONSTER_BLOCKS_TEMP);
    return ok;
}

static bool monster_publish_pair(Storage* storage) {
    storage_common_remove(storage, CUSTOM_MONSTER_INDEX_BACKUP);
    storage_common_remove(storage, CUSTOM_MONSTER_BLOCKS_BACKUP);
    bool index_existed = monster_exists(storage, CUSTOM_MONSTER_INDEX);
    bool blocks_existed = monster_exists(storage, CUSTOM_MONSTER_BLOCKS);
    bool index_backed_up = !index_existed ||
        storage_common_rename(
            storage, CUSTOM_MONSTER_INDEX, CUSTOM_MONSTER_INDEX_BACKUP) == FSE_OK;
    bool blocks_backed_up = !blocks_existed ||
        storage_common_rename(
            storage, CUSTOM_MONSTER_BLOCKS, CUSTOM_MONSTER_BLOCKS_BACKUP) == FSE_OK;
    bool index_published = false;
    bool blocks_published = false;
    if(index_backed_up && blocks_backed_up) {
        blocks_published =
            storage_common_rename(
                storage, CUSTOM_MONSTER_BLOCKS_TEMP, CUSTOM_MONSTER_BLOCKS) == FSE_OK;
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
        storage_common_rename(
            storage, CUSTOM_MONSTER_INDEX_BACKUP, CUSTOM_MONSTER_INDEX);
    if(blocks_existed && blocks_backed_up)
        storage_common_rename(
            storage, CUSTOM_MONSTER_BLOCKS_BACKUP, CUSTOM_MONSTER_BLOCKS);
    storage_common_remove(storage, CUSTOM_MONSTER_INDEX_TEMP);
    storage_common_remove(storage, CUSTOM_MONSTER_BLOCKS_TEMP);
    return false;
}

static void monster_sanitize_summary(PocketMonsterSummary* summary) {
    char* fields[] = {summary->name, summary->type, summary->environment,
                      summary->source, summary->role};
    for(size_t field = 0U; field < sizeof(fields) / sizeof(fields[0]); ++field)
        for(char* p = fields[field]; *p; ++p)
            if(*p == '|' || *p == '\n' || *p == '\r') *p = '-';
}

static bool monster_save_custom_common(
    Storage* storage,
    PocketMonsterDetail* detail,
    bool preserve_id) {
    storage_common_mkdir(storage, APP_ASSETS_PATH("monsters"));
    if(!preserve_id || !detail->summary.id[0]) {
        char base[20];
        monster_safe_id(base, sizeof(base), detail->summary.name);
        snprintf(detail->summary.id, sizeof(detail->summary.id), "%s_%04lx", base,
                 (unsigned long)(furi_hal_random_get() & 0xFFFFU));
    }
    monster_copy(detail->summary.source, sizeof(detail->summary.source), "Custom");
    if(!detail->summary.role[0])
        monster_copy(detail->summary.role, sizeof(detail->summary.role), "Skirmisher");
    monster_sanitize_summary(&detail->summary);
    bool ok = monster_write_transaction(storage, "UPSERT", detail->summary.id) &&
              monster_rewrite_index(storage, &detail->summary, detail->summary.id) &&
              monster_rewrite_blocks(storage, detail, detail->summary.id) &&
              monster_publish_pair(storage);
    if(ok) {
        storage_common_remove(storage, CUSTOM_MONSTER_TRANSACTION);
    } else {
        storage_common_remove(storage, CUSTOM_MONSTER_INDEX_TEMP);
        storage_common_remove(storage, CUSTOM_MONSTER_BLOCKS_TEMP);
        storage_common_remove(storage, CUSTOM_MONSTER_TRANSACTION);
    }
    return ok;
}

bool pocket_monster_save_custom(Storage* storage, PocketMonsterDetail* detail) {
    return monster_save_custom_common(storage, detail, false);
}

bool pocket_monster_update_custom(Storage* storage, PocketMonsterDetail* detail) {
    return detail->summary.id[0] && !strcmp(detail->summary.source, "Custom") &&
           monster_save_custom_common(storage, detail, true);
}

bool pocket_monster_delete_custom(Storage* storage, const PocketMonsterSummary* summary) {
    if(!summary || strcmp(summary->source, "Custom")) return false;
    bool ok = monster_write_transaction(storage, "DELETE", summary->id) &&
              monster_rewrite_index(storage, NULL, summary->id) &&
              monster_rewrite_blocks(storage, NULL, summary->id) &&
              monster_publish_pair(storage);
    storage_common_remove(storage, CUSTOM_MONSTER_TRANSACTION);
    return ok;
}

bool pocket_monster_recover_user_pack(
    Storage* storage,
    uint16_t* recovered,
    uint16_t* rolled_back) {
    *recovered = 0U;
    *rolled_back = 0U;
    bool pending = monster_exists(storage, CUSTOM_MONSTER_TRANSACTION);
    bool index_backup = monster_exists(storage, CUSTOM_MONSTER_INDEX_BACKUP);
    bool blocks_backup = monster_exists(storage, CUSTOM_MONSTER_BLOCKS_BACKUP);
    if(!pending && !index_backup && !blocks_backup) return true;
    bool publish_complete = pending && monster_exists(storage, CUSTOM_MONSTER_INDEX) &&
        monster_exists(storage, CUSTOM_MONSTER_BLOCKS) &&
        !monster_exists(storage, CUSTOM_MONSTER_INDEX_TEMP) &&
        !monster_exists(storage, CUSTOM_MONSTER_BLOCKS_TEMP);
    if(publish_complete) {
        storage_common_remove(storage, CUSTOM_MONSTER_INDEX_BACKUP);
        storage_common_remove(storage, CUSTOM_MONSTER_BLOCKS_BACKUP);
        storage_common_remove(storage, CUSTOM_MONSTER_TRANSACTION);
        *recovered = 1U;
        return true;
    }
    if(index_backup) {
        storage_common_remove(storage, CUSTOM_MONSTER_INDEX);
        if(storage_common_rename(
               storage, CUSTOM_MONSTER_INDEX_BACKUP, CUSTOM_MONSTER_INDEX) == FSE_OK)
            ++*rolled_back;
    }
    if(blocks_backup) {
        storage_common_remove(storage, CUSTOM_MONSTER_BLOCKS);
        if(storage_common_rename(
               storage, CUSTOM_MONSTER_BLOCKS_BACKUP, CUSTOM_MONSTER_BLOCKS) == FSE_OK)
            ++*rolled_back;
    }
    if(!index_backup && !monster_exists(storage, CUSTOM_MONSTER_INDEX))
        storage_common_remove(storage, CUSTOM_MONSTER_BLOCKS);
    if(!blocks_backup && !monster_exists(storage, CUSTOM_MONSTER_BLOCKS))
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

static bool monster_generate_filter(
    const PocketMonsterSummary* candidate,
    void* context) {
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

static int8_t monster_plan_find(
    const MonsterEncounterPlan* plan,
    uint8_t candidate_index) {
    for(uint8_t i = 0U; i < plan->count; ++i)
        if(plan->candidates[i] == candidate_index) return (int8_t)i;
    return -1;
}

static bool monster_plan_add(
    MonsterEncounterPlan* plan,
    uint8_t candidate_index,
    uint32_t xp,
    bool allow_repeats,
    uint8_t maximum_creatures,
    uint8_t maximum_types) {
    if(plan->creature_count >= maximum_creatures) return false;
    int8_t existing = monster_plan_find(plan, candidate_index);
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

bool pocket_monster_generate(
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
    output->budget = pocket_monster_xp_budget(party_level, party_size, difficulty);
    enum { MonsterCandidateWindow = 16U };
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
    uint16_t candidate_count = pocket_monster_sample(
        storage,
        monster_generate_filter,
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
            monster_plan_add(
                &plan,
                candidate_index,
                candidate->xp,
                allow_repeats,
                maximum_creatures,
                maximum_types);
        }
        if(plan.spent > best.spent) best = plan;
    }
    uint32_t lower_budget = difficulty > PocketEncounterLow ?
        pocket_monster_xp_budget(
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
