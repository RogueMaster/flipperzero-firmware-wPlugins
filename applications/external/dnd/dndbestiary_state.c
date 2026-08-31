#include "dndbestiary_state.h"
#include "dnd_fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STATE_READ_BUFFER    512U
#define STATE_LINE_LEN       768U
#define STATE_MAX_FAVORITES  64U
#define STATE_MAX_RECENTS    20U
#define STATE_MAX_FILTERS    8U
#define STATE_MAX_ENCOUNTERS 16U

#define FAVORITES_PATH          APP_DATA_PATH("bestiary/favorites.txt")
#define FAVORITES_TEMP          APP_DATA_PATH("bestiary/favorites.tmp")
#define RECENTS_PATH            APP_DATA_PATH("bestiary/recents.txt")
#define RECENTS_TEMP            APP_DATA_PATH("bestiary/recents.tmp")
#define FILTERS_PATH            APP_DATA_PATH("bestiary/filters.txt")
#define FILTERS_TEMP            APP_DATA_PATH("bestiary/filters.tmp")
#define ENCOUNTERS_PATH         APP_DATA_PATH("bestiary/encounters.txt")
#define ENCOUNTERS_TEMP         APP_DATA_PATH("bestiary/encounters.tmp")
#define ENCOUNTERS_ARCHIVE_PATH APP_DATA_PATH("bestiary/encounters_archive.txt")
#define ENCOUNTERS_ARCHIVE_TEMP APP_DATA_PATH("bestiary/encounters_archive.tmp")
#define PARTY_SETTINGS_PATH     APP_DATA_PATH("bestiary/party_settings.txt")
#define PARTY_SETTINGS_TEMP     APP_DATA_PATH("bestiary/party_settings.tmp")

typedef struct {
    File* file;
    uint8_t buffer[STATE_READ_BUFFER];
    uint16_t position;
    uint16_t count;
} StateReader;

typedef struct {
    StateReader reader;
    char line[STATE_LINE_LEN];
} StateReadWorkspace;

static StateReadWorkspace* state_workspace_alloc(File* file) {
    StateReadWorkspace* workspace = malloc(sizeof(StateReadWorkspace));
    if(!workspace) return NULL;
    memset(workspace, 0, sizeof(*workspace));
    workspace->reader.file = file;
    return workspace;
}

static void state_workspace_free(StateReadWorkspace* workspace) {
    free(workspace);
}

static bool state_parse_u8(const char* text, uint8_t minimum, uint8_t maximum, uint8_t* output);

static void state_copy(char* output, size_t size, const char* value) {
    if(!output || !size) return;
    strncpy(output, value ? value : "", size - 1U);
    output[size - 1U] = '\0';
}

static void state_safe_field(char* output, size_t size, const char* value) {
    if(!output || !size) return;
    state_copy(output, size, value);
    for(size_t index = 0U; output[index]; ++index)
        if(output[index] == '|' || output[index] == '\r' || output[index] == '\n')
            output[index] = ' ';
}

static void state_reader_init(StateReader* reader, File* file) {
    if(!reader) return;
    memset(reader, 0, sizeof(*reader));
    reader->file = file;
}

static bool state_read_line(StateReader* reader, char* line, size_t size) {
    if(!reader || !reader->file || !line || !size) return false;
    size_t position = 0U;
    bool consumed = false;
    bool overflow = false;
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
        if(position + 1U < size)
            line[position++] = value;
        else
            overflow = true;
    }
    line[position] = '\0';
    if(overflow && size) line[0] = '\0';
    return consumed;
}

static bool state_line_valid(char* line) {
    if(!line || !line[0]) return false;
    /* Older releases appended eight hexadecimal metadata digits after the
       final '|'. Strip that legacy suffix without validating it so manually
       edited files use the same structural rules as new plain-text records. */
    char* separator = strrchr(line, '|');
    if(!separator || strlen(separator + 1U) != 8U) return true;
    for(const char* digit = separator + 1U; *digit; ++digit) {
        if(!((*digit >= '0' && *digit <= '9') || (*digit >= 'a' && *digit <= 'f') ||
             (*digit >= 'A' && *digit <= 'F')))
            return true;
    }
    *separator = '\0';
    return line[0] != '\0';
}

static bool state_write_line(File* file, const char* body) {
    if(!file || !body) return false;
    char line[STATE_LINE_LEN];
    int length = snprintf(line, sizeof(line), "%s\n", body);
    return length > 0 && (size_t)length < sizeof(line) &&
           storage_file_write(file, line, (size_t)length) == (size_t)length;
}


static bool state_write_named(File* file, const char* key, const char* value) {
    char safe[STATE_LINE_LEN];
    state_copy(safe, sizeof(safe), value ? value : "");
    for(size_t i = 0U; safe[i]; ++i)
        if(safe[i] == '\r' || safe[i] == '\n') safe[i] = ' ';
    char line[STATE_LINE_LEN];
    int length = snprintf(line, sizeof(line), "%s=%s", key, safe);
    return length > 0 && (size_t)length < sizeof(line) && state_write_line(file, line);
}

static bool state_write_named_u32(File* file, const char* key, uint32_t value) {
    char line[64];
    int length = snprintf(line, sizeof(line), "%s=%lu", key, (unsigned long)value);
    return length > 0 && (size_t)length < sizeof(line) && state_write_line(file, line);
}

static bool state_indexed_key(
    const char* key,
    const char* prefix,
    const char* suffix,
    uint8_t maximum,
    uint8_t* index) {
    if(!key || !prefix || !suffix || !index) return false;
    size_t prefix_length = strlen(prefix);
    if(strncmp(key, prefix, prefix_length) != 0) return false;
    const char* cursor = key + prefix_length;
    const char* digits = cursor;
    uint32_t value = 0U;
    while(*cursor >= '0' && *cursor <= '9') {
        value = value * 10U + (uint32_t)(*cursor - '0');
        if(value >= maximum) return false;
        ++cursor;
    }
    if(cursor == digits || strcmp(cursor, suffix) != 0) return false;
    *index = (uint8_t)value;
    return true;
}

static bool state_publish(Storage* storage, const char* temporary, const char* path) {
    if(!storage || !temporary || !path) return false;
    char backup[160];
    int length = snprintf(backup, sizeof(backup), "%s.bak", path);
    if(length <= 0 || (size_t)length >= sizeof(backup)) return false;
    storage_common_remove(storage, backup);
    bool had_file = storage_common_rename(storage, path, backup) == FSE_OK;
    if(storage_common_rename(storage, temporary, path) == FSE_OK) {
        storage_common_remove(storage, backup);
        return true;
    }
    if(had_file) storage_common_rename(storage, backup, path);
    storage_common_remove(storage, temporary);
    return false;
}

static File* state_open_temp(Storage* storage, const char* temporary) {
    if(!storage || !temporary) return NULL;
    if(!pocket_d20_ensure_parent_dir(storage, temporary)) return NULL;
    storage_common_remove(storage, temporary);
    File* file = storage_file_alloc(storage);
    if(!file) return NULL;
    if(!storage_file_open(file, temporary, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_free(file);
        return NULL;
    }
    return file;
}

bool pocket_bestiary_party_settings_load(
    Storage* storage,
    uint8_t* party_level,
    uint8_t* party_size) {
    if(!storage || !party_level || !party_size) return false;
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    if(!storage_file_open(file, PARTY_SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return false;
    }
    StateReader reader;
    state_reader_init(&reader, file);
    char line[96];
    bool loaded = false;
    while(state_read_line(&reader, line, sizeof(line))) {
        char* value = strchr(line, '=');
        if(!value) continue;
        *value++ = '\0';
        uint8_t parsed = 0U;
        if(!strcmp(line, "PartyLevel") && state_parse_u8(value, 1U, 20U, &parsed)) {
            *party_level = parsed;
            loaded = true;
        } else if(!strcmp(line, "PartySize") && state_parse_u8(value, 1U, 12U, &parsed)) {
            *party_size = parsed;
            loaded = true;
        }
    }
    bool io_ok = storage_file_get_error(file) == FSE_OK;
    storage_file_close(file);
    storage_file_free(file);
    return io_ok && loaded;
}

bool pocket_bestiary_party_settings_save(Storage* storage, uint8_t party_level, uint8_t party_size) {
    if(!storage || party_level < 1U || party_level > 20U || party_size < 1U || party_size > 12U)
        return false;
    File* file = state_open_temp(storage, PARTY_SETTINGS_TEMP);
    if(!file) return false;
    bool ok = state_write_named_u32(file, "PartyLevel", party_level) &&
              state_write_named_u32(file, "PartySize", party_size) && storage_file_sync(file);
    storage_file_close(file);
    storage_file_free(file);
    if(!ok) {
        storage_common_remove(storage, PARTY_SETTINGS_TEMP);
        return false;
    }
    return state_publish(storage, PARTY_SETTINGS_TEMP, PARTY_SETTINGS_PATH);
}

static uint16_t state_id_count(Storage* storage, const char* path, uint16_t maximum) {
    if(!storage || !path) return 0U;
    File* file = storage_file_alloc(storage);
    if(!file) return 0U;
    uint16_t count = 0U;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        StateReader reader;
        state_reader_init(&reader, file);
        char line[STATE_LINE_LEN];
        while(count < maximum && state_read_line(&reader, line, sizeof(line)))
            if(state_line_valid(line) && line[0]) ++count;
    }
    storage_file_close(file);
    storage_file_free(file);
    return count;
}

static bool
    state_id_at(Storage* storage, const char* path, uint16_t wanted, char* id, size_t size) {
    if(!storage || !path || !id || !size) return false;
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    bool found = false;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        StateReader reader;
        state_reader_init(&reader, file);
        char line[STATE_LINE_LEN];
        uint16_t current = 0U;
        while(state_read_line(&reader, line, sizeof(line))) {
            if(!state_line_valid(line) || !line[0]) continue;
            if(current++ == wanted) {
                state_copy(id, size, line);
                found = true;
                break;
            }
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    return found;
}

bool pocket_bestiary_favorite_contains(Storage* storage, const char* id) {
    if(!storage || !id || !id[0]) return false;
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    bool found = false;
    if(storage_file_open(file, FAVORITES_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        StateReader reader;
        state_reader_init(&reader, file);
        char line[STATE_LINE_LEN];
        while(state_read_line(&reader, line, sizeof(line))) {
            if(state_line_valid(line) && !strcmp(line, id)) {
                found = true;
                break;
            }
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    return found;
}

bool pocket_bestiary_favorite_toggle(Storage* storage, const char* id, bool* now_favorite) {
    if(!storage || !id || !id[0]) return false;
    bool remove = pocket_bestiary_favorite_contains(storage, id);
    uint16_t count = pocket_bestiary_favorite_count(storage);
    if(!remove && count >= STATE_MAX_FAVORITES) return false;
    File* output = state_open_temp(storage, FAVORITES_TEMP);
    if(!output) return false;
    File* input = storage_file_alloc(storage);
    if(!input) {
        storage_file_close(output);
        storage_file_free(output);
        storage_common_remove(storage, FAVORITES_TEMP);
        return false;
    }
    bool ok = true;
    if(storage_file_open(input, FAVORITES_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        StateReader reader;
        state_reader_init(&reader, input);
        char line[STATE_LINE_LEN];
        while(ok && state_read_line(&reader, line, sizeof(line))) {
            if(!state_line_valid(line) || !line[0] || (remove && !strcmp(line, id))) continue;
            ok = state_write_line(output, line);
        }
    }
    storage_file_close(input);
    storage_file_free(input);
    if(ok && !remove) ok = state_write_line(output, id);
    if(ok) ok = storage_file_sync(output);
    storage_file_close(output);
    storage_file_free(output);
    if(!ok || !state_publish(storage, FAVORITES_TEMP, FAVORITES_PATH)) return false;
    if(now_favorite) *now_favorite = !remove;
    return true;
}

uint16_t pocket_bestiary_favorite_count(Storage* storage) {
    return state_id_count(storage, FAVORITES_PATH, STATE_MAX_FAVORITES);
}

bool pocket_bestiary_favorite_at(Storage* storage, uint16_t index, char* id, size_t size) {
    return state_id_at(storage, FAVORITES_PATH, index, id, size);
}

bool pocket_bestiary_recent_add(Storage* storage, const char* id) {
    if(!storage || !id || !id[0]) return false;
    char ids[STATE_MAX_RECENTS][POCKET_MONSTER_ID_LEN];
    uint16_t count = 0U;
    state_copy(ids[count++], sizeof(ids[0]), id);
    File* input = storage_file_alloc(storage);
    if(!input) return false;
    if(storage_file_open(input, RECENTS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        StateReader reader;
        state_reader_init(&reader, input);
        char line[STATE_LINE_LEN];
        while(count < STATE_MAX_RECENTS && state_read_line(&reader, line, sizeof(line))) {
            if(state_line_valid(line) && line[0] && strcmp(line, id))
                state_copy(ids[count++], sizeof(ids[0]), line);
        }
    }
    storage_file_close(input);
    storage_file_free(input);
    File* output = state_open_temp(storage, RECENTS_TEMP);
    if(!output) return false;
    bool ok = true;
    for(uint16_t index = 0U; ok && index < count; ++index)
        ok = state_write_line(output, ids[index]);
    if(ok) ok = storage_file_sync(output);
    storage_file_close(output);
    storage_file_free(output);
    return ok && state_publish(storage, RECENTS_TEMP, RECENTS_PATH);
}

uint16_t pocket_bestiary_recent_count(Storage* storage) {
    return state_id_count(storage, RECENTS_PATH, STATE_MAX_RECENTS);
}

bool pocket_bestiary_recent_at(Storage* storage, uint16_t index, char* id, size_t size) {
    return state_id_at(storage, RECENTS_PATH, index, id, size);
}

static bool state_parse_filter(char* line, PocketBestiaryFilterPreset* output) {
    /* Legacy compact rows remain readable; new writes use indexed named fields. */
    if(!state_line_valid(line)) return false;
    char* fields[7];
    uint8_t count = 0U;
    char* cursor = line;
    while(count < 7U) {
        fields[count++] = cursor;
        char* separator = strchr(cursor, '|');
        if(!separator) break;
        *separator = '\0';
        cursor = separator + 1U;
    }
    if(count != 7U) return false;
    memset(output, 0, sizeof(*output));
    state_copy(output->name, sizeof(output->name), fields[0]);
    state_copy(output->search, sizeof(output->search), fields[1]);
    if(!state_parse_u8(fields[2], 0U, UINT8_MAX, &output->max_cr_eighths) ||
       !state_parse_u8(fields[3], 0U, UINT8_MAX, &output->type_filter) ||
       !state_parse_u8(fields[4], 0U, UINT8_MAX, &output->source_filter) ||
       !state_parse_u8(fields[5], 0U, UINT8_MAX, &output->environment_filter) ||
       !state_parse_u8(fields[6], 0U, UINT8_MAX, &output->role_filter))
        return false;
    return output->name[0];
}

static uint16_t state_load_filters(
    Storage* storage,
    PocketBestiaryFilterPreset output[STATE_MAX_FILTERS]) {
    memset(output, 0, sizeof(PocketBestiaryFilterPreset) * STATE_MAX_FILTERS);
    File* file = storage_file_alloc(storage);
    if(!file) return 0U;
    if(!storage_file_open(file, FILTERS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return 0U;
    }
    StateReader reader;
    state_reader_init(&reader, file);
    char line[STATE_LINE_LEN];
    bool named_seen = false;
    uint8_t legacy_next = 0U;
    while(state_read_line(&reader, line, sizeof(line))) {
        char original[STATE_LINE_LEN];
        state_copy(original, sizeof(original), line);
        char* value = strchr(line, '=');
        if(!value) {
            if(named_seen || legacy_next >= STATE_MAX_FILTERS) continue;
            PocketBestiaryFilterPreset legacy;
            if(state_parse_filter(original, &legacy)) output[legacy_next++] = legacy;
            continue;
        }
        *value++ = '\0';
        uint8_t index = 0U;
#define FILTER_STRING(suffix, field) \
        if(state_indexed_key(line, "Filter", suffix, STATE_MAX_FILTERS, &index)) { state_copy(output[index].field, sizeof(output[index].field), value); named_seen = true; continue; }
#define FILTER_U8(suffix, field) \
        if(state_indexed_key(line, "Filter", suffix, STATE_MAX_FILTERS, &index)) { uint8_t parsed = 0U; if(state_parse_u8(value, 0U, UINT8_MAX, &parsed)) output[index].field = parsed; named_seen = true; continue; }
        FILTER_STRING("Name", name)
        FILTER_STRING("Search", search)
        FILTER_U8("MaxCrEighths", max_cr_eighths)
        FILTER_U8("Type", type_filter)
        FILTER_U8("Source", source_filter)
        FILTER_U8("Environment", environment_filter)
        FILTER_U8("Role", role_filter)
#undef FILTER_STRING
#undef FILTER_U8
    }
    storage_file_close(file);
    storage_file_free(file);
    if(named_seen) {
        uint8_t write = 0U;
        for(uint8_t read = 0U; read < STATE_MAX_FILTERS; ++read)
            if(output[read].name[0]) {
                if(write != read) output[write] = output[read];
                ++write;
            }
        for(uint8_t i = write; i < STATE_MAX_FILTERS; ++i) memset(&output[i], 0, sizeof(output[i]));
        return write;
    }
    return legacy_next;
}

uint16_t pocket_bestiary_filter_count(Storage* storage) {
    if(!storage) return 0U;
    PocketBestiaryFilterPreset* filters = calloc(STATE_MAX_FILTERS, sizeof(PocketBestiaryFilterPreset));
    if(!filters) return 0U;
    uint16_t count = state_load_filters(storage, filters);
    free(filters);
    return count;
}

bool pocket_bestiary_filter_at(
    Storage* storage,
    uint16_t wanted,
    PocketBestiaryFilterPreset* output) {
    if(!storage || !output || wanted >= STATE_MAX_FILTERS) return false;
    PocketBestiaryFilterPreset* filters = calloc(STATE_MAX_FILTERS, sizeof(PocketBestiaryFilterPreset));
    if(!filters) return false;
    uint16_t count = state_load_filters(storage, filters);
    bool found = wanted < count;
    if(found) *output = filters[wanted];
    free(filters);
    return found;
}

static bool state_write_filter(
    File* file,
    uint8_t index,
    const PocketBestiaryFilterPreset* preset) {
    if(!file || !preset || index >= STATE_MAX_FILTERS) return false;
    char key[48];
#define FILTER_WRITE_STRING(suffix, field) \
    do { snprintf(key, sizeof(key), "Filter%u%s", index, suffix); if(!state_write_named(file, key, preset->field)) return false; } while(false)
#define FILTER_WRITE_U8(suffix, field) \
    do { snprintf(key, sizeof(key), "Filter%u%s", index, suffix); if(!state_write_named_u32(file, key, preset->field)) return false; } while(false)
    FILTER_WRITE_STRING("Name", name);
    FILTER_WRITE_STRING("Search", search);
    FILTER_WRITE_U8("MaxCrEighths", max_cr_eighths);
    FILTER_WRITE_U8("Type", type_filter);
    FILTER_WRITE_U8("Source", source_filter);
    FILTER_WRITE_U8("Environment", environment_filter);
    FILTER_WRITE_U8("Role", role_filter);
#undef FILTER_WRITE_STRING
#undef FILTER_WRITE_U8
    return true;
}

bool pocket_bestiary_filter_save(Storage* storage, const PocketBestiaryFilterPreset* preset) {
    if(!storage || !preset || !preset->name[0]) return false;
    uint16_t count = pocket_bestiary_filter_count(storage);
    bool replacing = false;
    for(uint16_t index = 0U; index < count; ++index) {
        PocketBestiaryFilterPreset prior;
        if(pocket_bestiary_filter_at(storage, index, &prior) &&
           !strcmp(prior.name, preset->name)) {
            replacing = true;
            break;
        }
    }
    if(!replacing && count >= STATE_MAX_FILTERS) return false;
    File* output = state_open_temp(storage, FILTERS_TEMP);
    if(!output) return false;
    bool ok = true;
    uint8_t output_index = 0U;
    for(uint16_t index = 0U; ok && index < count; ++index) {
        PocketBestiaryFilterPreset prior;
        if(!pocket_bestiary_filter_at(storage, index, &prior)) continue;
        if(strcmp(prior.name, preset->name)) ok = state_write_filter(output, output_index++, &prior);
    }
    if(ok) ok = state_write_filter(output, output_index, preset) && storage_file_sync(output);
    storage_file_close(output);
    storage_file_free(output);
    return ok && state_publish(storage, FILTERS_TEMP, FILTERS_PATH);
}

bool pocket_bestiary_filter_delete(Storage* storage, uint16_t wanted) {
    if(!storage) return false;
    uint16_t count = pocket_bestiary_filter_count(storage);
    if(wanted >= count) return false;
    File* output = state_open_temp(storage, FILTERS_TEMP);
    if(!output) return false;
    bool ok = true;
    uint8_t output_index = 0U;
    for(uint16_t index = 0U; ok && index < count; ++index) {
        PocketBestiaryFilterPreset preset;
        if(index != wanted && pocket_bestiary_filter_at(storage, index, &preset))
            ok = state_write_filter(output, output_index++, &preset);
    }
    if(ok) ok = storage_file_sync(output);
    storage_file_close(output);
    storage_file_free(output);
    return ok && state_publish(storage, FILTERS_TEMP, FILTERS_PATH);
}

static bool state_parse_u8(const char* text, uint8_t minimum, uint8_t maximum, uint8_t* output) {
    if(!text || !text[0] || !output) return false;
    uint16_t value = 0U;
    for(const char* cursor = text; *cursor; ++cursor) {
        if(*cursor < '0' || *cursor > '9') return false;
        uint8_t digit = (uint8_t)(*cursor - '0');
        if(value > (uint16_t)(maximum / 10U) ||
           (value == (uint16_t)(maximum / 10U) && digit > (uint8_t)(maximum % 10U)))
            return false;
        value = (uint16_t)(value * 10U + digit);
    }
    if(value < minimum || value > maximum) return false;
    *output = (uint8_t)value;
    return true;
}

static bool state_parse_encounter(char* line, PocketSavedEncounter* output) {
    /* Legacy compact rows remain readable; new writes use indexed named fields. */
    if(!line || !output || !state_line_valid(line)) return false;
    char* fields[6];
    uint8_t field_count = 0U;
    char* cursor = line;
    while(field_count < 6U) {
        fields[field_count++] = cursor;
        char* separator = strchr(cursor, '|');
        if(!separator) break;
        *separator = '\0';
        cursor = separator + 1U;
    }
    if(field_count != 6U) return false;
    memset(output, 0, sizeof(*output));
    state_copy(output->name, sizeof(output->name), fields[0]);
    if(!state_parse_u8(fields[1], 1U, 20U, &output->party_level) ||
       !state_parse_u8(fields[2], 1U, 12U, &output->party_size) ||
       !state_parse_u8(fields[3], 0U, PocketEncounterDifficultyCount - 1U, &output->difficulty) ||
       !state_parse_u8(fields[4], 1U, POCKET_MONSTER_ENCOUNTER_MAX, &output->count))
        return false;
    cursor = fields[5];
    for(uint8_t index = 0U; index < output->count; ++index) {
        char* quantity = strchr(cursor, ':');
        if(!quantity) return false;
        *quantity++ = '\0';
        char* next = strchr(quantity, ',');
        if(next) *next++ = '\0';
        state_copy(output->monster_ids[index], sizeof(output->monster_ids[index]), cursor);
        if(!output->monster_ids[index][0] ||
           !state_parse_u8(quantity, 1U, UINT8_MAX, &output->quantities[index]))
            return false;
        cursor = next;
        if(index + 1U < output->count && !cursor) return false;
    }
    return output->name[0] != '\0' && !cursor;
}

static bool state_encounter_named_mask(Storage* storage, const char* path, uint16_t* mask) {
    *mask = 0U;
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return true;
    }
    StateReadWorkspace* workspace = state_workspace_alloc(file);
    if(!workspace) {
        storage_file_close(file);
        storage_file_free(file);
        return false;
    }
    while(state_read_line(&workspace->reader, workspace->line, sizeof(workspace->line))) {
        char* value = strchr(workspace->line, '=');
        if(!value) continue;
        *value = '\0';
        uint8_t index = 0U;
        if(state_indexed_key(workspace->line, "Encounter", "Name", STATE_MAX_ENCOUNTERS, &index))
            *mask |= (uint16_t)(1U << index);
    }
    bool ok = storage_file_get_error(file) == FSE_OK;
    state_workspace_free(workspace);
    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

static uint16_t state_encounter_count_path(Storage* storage, const char* path) {
    uint16_t mask = 0U;
    if(!state_encounter_named_mask(storage, path, &mask)) return 0U;
    if(mask) {
        uint16_t count = 0U;
        for(uint8_t i = 0U; i < STATE_MAX_ENCOUNTERS; ++i)
            if(mask & (uint16_t)(1U << i)) ++count;
        return count;
    }
    File* file = storage_file_alloc(storage);
    if(!file) return 0U;
    uint16_t count = 0U;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        StateReadWorkspace* workspace = state_workspace_alloc(file);
        if(workspace) {
            PocketSavedEncounter encounter;
            while(count < STATE_MAX_ENCOUNTERS &&
                  state_read_line(&workspace->reader, workspace->line, sizeof(workspace->line)))
                if(state_parse_encounter(workspace->line, &encounter)) ++count;
            state_workspace_free(workspace);
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    return count;
}

static bool state_encounter_at_path(
    Storage* storage,
    const char* path,
    uint16_t wanted,
    PocketSavedEncounter* output) {
    uint16_t mask = 0U;
    if(!state_encounter_named_mask(storage, path, &mask)) return false;
    if(!mask) {
        File* file = storage_file_alloc(storage);
        if(!file) return false;
        bool found = false;
        if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
            StateReadWorkspace* workspace = state_workspace_alloc(file);
            if(workspace) {
                uint16_t ordinal = 0U;
                while(state_read_line(
                    &workspace->reader, workspace->line, sizeof(workspace->line))) {
                    PocketSavedEncounter encounter;
                    if(!state_parse_encounter(workspace->line, &encounter)) continue;
                    if(ordinal++ == wanted) {
                        *output = encounter;
                        found = true;
                        break;
                    }
                }
                state_workspace_free(workspace);
            }
        }
        storage_file_close(file);
        storage_file_free(file);
        return found;
    }
    uint8_t actual = UINT8_MAX;
    uint16_t ordinal = 0U;
    for(uint8_t i = 0U; i < STATE_MAX_ENCOUNTERS; ++i) {
        if(!(mask & (uint16_t)(1U << i))) continue;
        if(ordinal++ == wanted) {
            actual = i;
            break;
        }
    }
    if(actual == UINT8_MAX) return false;
    memset(output, 0, sizeof(*output));
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return false;
    }
    StateReadWorkspace* workspace = state_workspace_alloc(file);
    if(!workspace) {
        storage_file_close(file);
        storage_file_free(file);
        return false;
    }
    while(state_read_line(&workspace->reader, workspace->line, sizeof(workspace->line))) {
        char* value = strchr(workspace->line, '=');
        if(!value) continue;
        *value++ = '\0';
        uint8_t index = 0U;
        uint8_t parsed = 0U;
#define ENC_STRING(suffix, field) \
        if(state_indexed_key(workspace->line, "Encounter", suffix, STATE_MAX_ENCOUNTERS, &index) && index == actual) { state_copy(output->field, sizeof(output->field), value); continue; }
#define ENC_U8(suffix, field, minv, maxv) \
        if(state_indexed_key(workspace->line, "Encounter", suffix, STATE_MAX_ENCOUNTERS, &index) && index == actual) { if(state_parse_u8(value, minv, maxv, &parsed)) output->field = parsed; continue; }
        ENC_STRING("Name", name)
        ENC_U8("PartyLevel", party_level, 1U, 20U)
        ENC_U8("PartySize", party_size, 1U, 12U)
        ENC_U8("Difficulty", difficulty, 0U, PocketEncounterDifficultyCount - 1U)
        ENC_U8("Count", count, 0U, POCKET_MONSTER_ENCOUNTER_MAX)
#undef ENC_STRING
#undef ENC_U8
        for(uint8_t monster = 0U; monster < POCKET_MONSTER_ENCOUNTER_MAX; ++monster) {
            char suffix[32];
            snprintf(suffix, sizeof(suffix), "Monster%uId", monster);
            if(state_indexed_key(
                   workspace->line,
                   "Encounter",
                   suffix,
                   STATE_MAX_ENCOUNTERS,
                   &index) &&
               index == actual) {
                state_copy(output->monster_ids[monster], sizeof(output->monster_ids[monster]), value);
                if(output->count <= monster) output->count = (uint8_t)(monster + 1U);
                break;
            }
            snprintf(suffix, sizeof(suffix), "Monster%uQuantity", monster);
            if(state_indexed_key(
                   workspace->line,
                   "Encounter",
                   suffix,
                   STATE_MAX_ENCOUNTERS,
                   &index) &&
               index == actual) {
                if(state_parse_u8(value, 1U, UINT8_MAX, &parsed))
                    output->quantities[monster] = parsed;
                if(output->count <= monster) output->count = (uint8_t)(monster + 1U);
                break;
            }
        }
    }
    bool ok = storage_file_get_error(file) == FSE_OK && output->name[0];
    state_workspace_free(workspace);
    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

uint16_t pocket_bestiary_encounter_count(Storage* storage) {
    return storage ? state_encounter_count_path(storage, ENCOUNTERS_PATH) : 0U;
}

bool pocket_bestiary_encounter_at(Storage* storage, uint16_t wanted, PocketSavedEncounter* output) {
    return storage && output && state_encounter_at_path(storage, ENCOUNTERS_PATH, wanted, output);
}

static bool state_write_encounter(
    File* file,
    uint8_t index,
    const PocketSavedEncounter* encounter) {
    if(!file || !encounter || index >= STATE_MAX_ENCOUNTERS) return false;
    char key[64];
#define ENC_WRITE_STRING(suffix, value) \
    do { snprintf(key, sizeof(key), "Encounter%u%s", index, suffix); if(!state_write_named(file, key, value)) return false; } while(false)
#define ENC_WRITE_U8(suffix, value) \
    do { snprintf(key, sizeof(key), "Encounter%u%s", index, suffix); if(!state_write_named_u32(file, key, value)) return false; } while(false)
    ENC_WRITE_STRING("Name", encounter->name);
    ENC_WRITE_U8("PartyLevel", encounter->party_level);
    ENC_WRITE_U8("PartySize", encounter->party_size);
    ENC_WRITE_U8("Difficulty", encounter->difficulty);
    ENC_WRITE_U8("Count", encounter->count);
    for(uint8_t monster = 0U; monster < encounter->count && monster < POCKET_MONSTER_ENCOUNTER_MAX; ++monster) {
        char suffix[32];
        snprintf(suffix, sizeof(suffix), "Monster%uId", monster);
        ENC_WRITE_STRING(suffix, encounter->monster_ids[monster]);
        snprintf(suffix, sizeof(suffix), "Monster%uQuantity", monster);
        ENC_WRITE_U8(suffix, encounter->quantities[monster]);
    }
#undef ENC_WRITE_STRING
#undef ENC_WRITE_U8
    return true;
}

bool pocket_bestiary_encounter_save(Storage* storage, const PocketSavedEncounter* encounter) {
    if(!storage || !encounter || !encounter->name[0] || !encounter->count ||
       encounter->count > POCKET_MONSTER_ENCOUNTER_MAX)
        return false;
    char normalized_name[POCKET_BESTIARY_ENCOUNTER_NAME_LEN];
    state_safe_field(normalized_name, sizeof(normalized_name), encounter->name);
    if(!normalized_name[0]) return false;
    uint16_t count = pocket_bestiary_encounter_count(storage);
    File* output = state_open_temp(storage, ENCOUNTERS_TEMP);
    if(!output) return false;
    bool ok = true;
    bool replacing = false;
    uint8_t out_index = 0U;
    for(uint16_t index = 0U; ok && index < count; ++index) {
        PocketSavedEncounter prior;
        if(!pocket_bestiary_encounter_at(storage, index, &prior)) continue;
        if(!strcmp(prior.name, normalized_name)) { replacing = true; continue; }
        ok = state_write_encounter(output, out_index++, &prior);
    }
    if(!replacing && count >= STATE_MAX_ENCOUNTERS) ok = false;
    if(ok) {
        PocketSavedEncounter normalized = *encounter;
        state_copy(normalized.name, sizeof(normalized.name), normalized_name);
        ok = state_write_encounter(output, out_index, &normalized) && storage_file_sync(output);
    }
    storage_file_close(output);
    storage_file_free(output);
    if(!ok) { storage_common_remove(storage, ENCOUNTERS_TEMP); return false; }
    return state_publish(storage, ENCOUNTERS_TEMP, ENCOUNTERS_PATH);
}

bool pocket_bestiary_encounter_delete(Storage* storage, uint16_t wanted) {
    if(!storage) return false;
    uint16_t count = pocket_bestiary_encounter_count(storage);
    if(wanted >= count) return false;
    File* output = state_open_temp(storage, ENCOUNTERS_TEMP);
    if(!output) return false;
    bool ok = true;
    uint8_t out_index = 0U;
    for(uint16_t index = 0U; ok && index < count; ++index) {
        if(index == wanted) continue;
        PocketSavedEncounter encounter;
        if(pocket_bestiary_encounter_at(storage, index, &encounter))
            ok = state_write_encounter(output, out_index++, &encounter);
    }
    if(ok) ok = storage_file_sync(output);
    storage_file_close(output);
    storage_file_free(output);
    if(!ok) { storage_common_remove(storage, ENCOUNTERS_TEMP); return false; }
    return state_publish(storage, ENCOUNTERS_TEMP, ENCOUNTERS_PATH);
}

bool pocket_bestiary_encounter_rename(Storage* storage, uint16_t wanted, const char* new_name) {
    if(!storage || !new_name || !new_name[0]) return false;
    char normalized_name[POCKET_BESTIARY_ENCOUNTER_NAME_LEN];
    state_safe_field(normalized_name, sizeof(normalized_name), new_name);
    uint16_t count = pocket_bestiary_encounter_count(storage);
    if(!normalized_name[0] || wanted >= count) return false;
    File* output = state_open_temp(storage, ENCOUNTERS_TEMP);
    if(!output) return false;
    bool ok = true;
    for(uint16_t index = 0U; ok && index < count; ++index) {
        PocketSavedEncounter encounter;
        if(!pocket_bestiary_encounter_at(storage, index, &encounter)) continue;
        if(index != wanted && !strcmp(encounter.name, normalized_name)) { ok = false; break; }
        if(index == wanted) state_copy(encounter.name, sizeof(encounter.name), normalized_name);
        ok = state_write_encounter(output, (uint8_t)index, &encounter);
    }
    if(ok) ok = storage_file_sync(output);
    storage_file_close(output);
    storage_file_free(output);
    if(!ok) { storage_common_remove(storage, ENCOUNTERS_TEMP); return false; }
    return state_publish(storage, ENCOUNTERS_TEMP, ENCOUNTERS_PATH);
}

bool pocket_bestiary_encounter_duplicate(Storage* storage, uint16_t wanted, const char* new_name) {
    if(!storage || !new_name || !new_name[0]) return false;
    char normalized_name[POCKET_BESTIARY_ENCOUNTER_NAME_LEN];
    state_safe_field(normalized_name, sizeof(normalized_name), new_name);
    uint16_t count = pocket_bestiary_encounter_count(storage);
    if(!normalized_name[0] || wanted >= count || count >= STATE_MAX_ENCOUNTERS) return false;
    PocketSavedEncounter duplicate;
    if(!pocket_bestiary_encounter_at(storage, wanted, &duplicate)) return false;
    for(uint16_t index = 0U; index < count; ++index) {
        PocketSavedEncounter existing;
        if(pocket_bestiary_encounter_at(storage, index, &existing) && !strcmp(existing.name, normalized_name))
            return false;
    }
    File* output = state_open_temp(storage, ENCOUNTERS_TEMP);
    if(!output) return false;
    bool ok = true;
    for(uint16_t index = 0U; ok && index < count; ++index) {
        PocketSavedEncounter existing;
        if(pocket_bestiary_encounter_at(storage, index, &existing))
            ok = state_write_encounter(output, (uint8_t)index, &existing);
    }
    if(ok) {
        state_copy(duplicate.name, sizeof(duplicate.name), normalized_name);
        ok = state_write_encounter(output, (uint8_t)count, &duplicate) && storage_file_sync(output);
    }
    storage_file_close(output);
    storage_file_free(output);
    if(!ok) { storage_common_remove(storage, ENCOUNTERS_TEMP); return false; }
    return state_publish(storage, ENCOUNTERS_TEMP, ENCOUNTERS_PATH);
}

static bool state_archive_append(Storage* storage, const PocketSavedEncounter* encounter) {
    if(!storage || !encounter) return false;
    uint16_t count = state_encounter_count_path(storage, ENCOUNTERS_ARCHIVE_PATH);
    if(count >= STATE_MAX_ENCOUNTERS) return false;
    File* output = state_open_temp(storage, ENCOUNTERS_ARCHIVE_TEMP);
    if(!output) return false;
    bool ok = true;
    for(uint16_t index = 0U; ok && index < count; ++index) {
        PocketSavedEncounter archived;
        if(state_encounter_at_path(storage, ENCOUNTERS_ARCHIVE_PATH, index, &archived))
            ok = state_write_encounter(output, (uint8_t)index, &archived);
    }
    if(ok) ok = state_write_encounter(output, (uint8_t)count, encounter) && storage_file_sync(output);
    storage_file_close(output);
    storage_file_free(output);
    if(!ok) { storage_common_remove(storage, ENCOUNTERS_ARCHIVE_TEMP); return false; }
    return state_publish(storage, ENCOUNTERS_ARCHIVE_TEMP, ENCOUNTERS_ARCHIVE_PATH);
}

bool pocket_bestiary_encounter_archive(Storage* storage, uint16_t wanted) {
    if(!storage) return false;
    PocketSavedEncounter encounter;
    if(!pocket_bestiary_encounter_at(storage, wanted, &encounter)) return false;
    /* Publish the archive copy first. If active removal fails, the encounter remains
       in both places rather than being lost. */
    if(!state_archive_append(storage, &encounter)) return false;
    return pocket_bestiary_encounter_delete(storage, wanted);
}
