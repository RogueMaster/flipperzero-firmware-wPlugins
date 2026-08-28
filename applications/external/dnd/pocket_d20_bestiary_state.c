#include "pocket_d20_bestiary_state.h"
#include "pocket_d20_fs.h"

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
    bool loaded = false;
    if(storage_file_open(file, PARTY_SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        /* This record is intentionally tiny; do not put the generic 512-byte reader
           plus 768-byte line buffer on the Bestiary startup stack. */
        char line[48];
        size_t count = storage_file_read(file, line, sizeof(line) - 1U);
        if(storage_file_get_error(file) == FSE_OK && count && count < sizeof(line) - 1U) {
            line[count] = '\0';
            while(count && (line[count - 1U] == '\n' || line[count - 1U] == '\r'))
                line[--count] = '\0';
            if(state_line_valid(line)) {
                char* separator = strchr(line, '|');
                if(separator) {
                    *separator++ = '\0';
                    char* end_level = NULL;
                    char* end_size = NULL;
                    unsigned long level = strtoul(line, &end_level, 10);
                    unsigned long size = strtoul(separator, &end_size, 10);
                    if(end_level && *end_level == '\0' && end_size && *end_size == '\0' &&
                       level >= 1U && level <= 20U && size >= 1U && size <= 12U) {
                        *party_level = (uint8_t)level;
                        *party_size = (uint8_t)size;
                        loaded = true;
                    }
                }
            }
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    return loaded;
}

bool pocket_bestiary_party_settings_save(Storage* storage, uint8_t party_level, uint8_t party_size) {
    if(!storage || party_level < 1U || party_level > 20U || party_size < 1U || party_size > 12U)
        return false;
    File* file = state_open_temp(storage, PARTY_SETTINGS_TEMP);
    if(!file) return false;
    char body[32];
    int length = snprintf(body, sizeof(body), "%u|%u", party_level, party_size);
    bool ok = length > 0 && (size_t)length < sizeof(body) && state_write_line(file, body) &&
              storage_file_sync(file);
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
    output->max_cr_eighths = (uint8_t)strtoul(fields[2], NULL, 10);
    output->type_filter = (uint8_t)strtoul(fields[3], NULL, 10);
    output->source_filter = (uint8_t)strtoul(fields[4], NULL, 10);
    output->environment_filter = (uint8_t)strtoul(fields[5], NULL, 10);
    output->role_filter = (uint8_t)strtoul(fields[6], NULL, 10);
    return output->name[0];
}

uint16_t pocket_bestiary_filter_count(Storage* storage) {
    if(!storage) return 0U;
    File* file = storage_file_alloc(storage);
    if(!file) return 0U;
    uint16_t count = 0U;
    if(storage_file_open(file, FILTERS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        StateReader reader;
        state_reader_init(&reader, file);
        char line[STATE_LINE_LEN];
        PocketBestiaryFilterPreset preset;
        while(count < STATE_MAX_FILTERS && state_read_line(&reader, line, sizeof(line)))
            if(state_parse_filter(line, &preset)) ++count;
    }
    storage_file_close(file);
    storage_file_free(file);
    return count;
}

bool pocket_bestiary_filter_at(
    Storage* storage,
    uint16_t wanted,
    PocketBestiaryFilterPreset* output) {
    if(!storage || !output) return false;
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    bool found = false;
    if(storage_file_open(file, FILTERS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        StateReader reader;
        state_reader_init(&reader, file);
        char line[STATE_LINE_LEN];
        uint16_t index = 0U;
        while(state_read_line(&reader, line, sizeof(line))) {
            PocketBestiaryFilterPreset preset;
            if(!state_parse_filter(line, &preset)) continue;
            if(index++ == wanted) {
                *output = preset;
                found = true;
                break;
            }
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    return found;
}

static bool state_write_filter(File* file, const PocketBestiaryFilterPreset* preset) {
    if(!file || !preset) return false;
    char name[POCKET_BESTIARY_FILTER_NAME_LEN];
    char search[POCKET_MONSTER_NAME_LEN];
    char body[STATE_LINE_LEN];
    state_safe_field(name, sizeof(name), preset->name);
    state_safe_field(search, sizeof(search), preset->search);
    int length = snprintf(
        body,
        sizeof(body),
        "%s|%s|%u|%u|%u|%u|%u",
        name,
        search,
        preset->max_cr_eighths,
        preset->type_filter,
        preset->source_filter,
        preset->environment_filter,
        preset->role_filter);
    return length > 0 && (size_t)length < sizeof(body) && state_write_line(file, body);
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
    for(uint16_t index = 0U; ok && index < count; ++index) {
        PocketBestiaryFilterPreset prior;
        if(!pocket_bestiary_filter_at(storage, index, &prior)) continue;
        if(strcmp(prior.name, preset->name)) ok = state_write_filter(output, &prior);
    }
    if(ok) ok = state_write_filter(output, preset) && storage_file_sync(output);
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
    for(uint16_t index = 0U; ok && index < count; ++index) {
        PocketBestiaryFilterPreset preset;
        if(index != wanted && pocket_bestiary_filter_at(storage, index, &preset))
            ok = state_write_filter(output, &preset);
    }
    if(ok) ok = storage_file_sync(output);
    storage_file_close(output);
    storage_file_free(output);
    return ok && state_publish(storage, FILTERS_TEMP, FILTERS_PATH);
}

static bool state_parse_u8(const char* text, uint8_t minimum, uint8_t maximum, uint8_t* output) {
    if(!text || !text[0] || !output) return false;
    char* end = NULL;
    unsigned long value = strtoul(text, &end, 10);
    if(!end || *end != '\0' || value < minimum || value > maximum) return false;
    *output = (uint8_t)value;
    return true;
}

static bool state_parse_encounter(char* line, PocketSavedEncounter* output) {
    if(!line || !output) return false;
    if(!state_line_valid(line)) return false;
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

uint16_t pocket_bestiary_encounter_count(Storage* storage) {
    if(!storage) return 0U;
    File* file = storage_file_alloc(storage);
    if(!file) return 0U;
    uint16_t count = 0U;
    if(storage_file_open(file, ENCOUNTERS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        StateReader reader;
        state_reader_init(&reader, file);
        char line[STATE_LINE_LEN];
        PocketSavedEncounter encounter;
        while(count < STATE_MAX_ENCOUNTERS && state_read_line(&reader, line, sizeof(line)))
            if(state_parse_encounter(line, &encounter)) ++count;
    }
    storage_file_close(file);
    storage_file_free(file);
    return count;
}

bool pocket_bestiary_encounter_at(Storage* storage, uint16_t wanted, PocketSavedEncounter* output) {
    if(!storage || !output) return false;
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    bool found = false;
    if(storage_file_open(file, ENCOUNTERS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        StateReader reader;
        state_reader_init(&reader, file);
        char line[STATE_LINE_LEN];
        uint16_t index = 0U;
        while(state_read_line(&reader, line, sizeof(line))) {
            PocketSavedEncounter encounter;
            if(!state_parse_encounter(line, &encounter)) continue;
            if(index++ == wanted) {
                *output = encounter;
                found = true;
                break;
            }
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    return found;
}

static bool state_write_encounter(File* file, const PocketSavedEncounter* encounter) {
    if(!file || !encounter) return false;
    char name[POCKET_BESTIARY_ENCOUNTER_NAME_LEN];
    char body[STATE_LINE_LEN];
    state_safe_field(name, sizeof(name), encounter->name);
    int length = snprintf(
        body,
        sizeof(body),
        "%s|%u|%u|%u|%u|",
        name,
        encounter->party_level,
        encounter->party_size,
        encounter->difficulty,
        encounter->count);
    if(length <= 0 || (size_t)length >= sizeof(body)) return false;
    size_t position = (size_t)length;
    for(uint8_t index = 0U; index < encounter->count; ++index) {
        length = snprintf(
            body + position,
            sizeof(body) - position,
            "%s%s:%u",
            index ? "," : "",
            encounter->monster_ids[index],
            encounter->quantities[index]);
        if(length <= 0 || (size_t)length >= sizeof(body) - position) return false;
        position += (size_t)length;
    }
    return state_write_line(file, body);
}

bool pocket_bestiary_encounter_save(Storage* storage, const PocketSavedEncounter* encounter) {
    if(!storage || !encounter || !encounter->name[0] || !encounter->count ||
       encounter->count > POCKET_MONSTER_ENCOUNTER_MAX)
        return false;
    char normalized_name[POCKET_BESTIARY_ENCOUNTER_NAME_LEN];
    state_safe_field(normalized_name, sizeof(normalized_name), encounter->name);
    if(!normalized_name[0]) return false;

    File* input = storage_file_alloc(storage);
    if(!input) return false;
    bool input_open = storage_file_open(input, ENCOUNTERS_PATH, FSAM_READ, FSOM_OPEN_EXISTING);
    File* output = state_open_temp(storage, ENCOUNTERS_TEMP);
    if(!output) {
        if(input_open) storage_file_close(input);
        storage_file_free(input);
        return false;
    }

    bool ok = true;
    bool replacing = false;
    uint16_t count = 0U;
    if(input_open) {
        StateReader reader;
        state_reader_init(&reader, input);
        char line[STATE_LINE_LEN];
        while(ok && state_read_line(&reader, line, sizeof(line))) {
            PocketSavedEncounter prior;
            if(!state_parse_encounter(line, &prior)) {
                ok = false;
                break;
            }
            if(count >= STATE_MAX_ENCOUNTERS) {
                ok = false;
                break;
            }
            ++count;
            if(!strcmp(prior.name, normalized_name)) {
                replacing = true;
                continue;
            }
            ok = state_write_encounter(output, &prior);
        }
    }
    if(input_open && storage_file_get_error(input) != FSE_OK) ok = false;
    if(!replacing && count >= STATE_MAX_ENCOUNTERS) ok = false;
    if(ok) {
        PocketSavedEncounter normalized = *encounter;
        state_copy(normalized.name, sizeof(normalized.name), normalized_name);
        ok = state_write_encounter(output, &normalized) && storage_file_sync(output);
    }

    if(input_open) storage_file_close(input);
    storage_file_free(input);
    storage_file_close(output);
    storage_file_free(output);
    if(!ok) {
        storage_common_remove(storage, ENCOUNTERS_TEMP);
        return false;
    }
    return state_publish(storage, ENCOUNTERS_TEMP, ENCOUNTERS_PATH);
}

bool pocket_bestiary_encounter_delete(Storage* storage, uint16_t wanted) {
    if(!storage) return false;
    File* input = storage_file_alloc(storage);
    if(!input) return false;
    if(!storage_file_open(input, ENCOUNTERS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(input);
        return false;
    }
    File* output = state_open_temp(storage, ENCOUNTERS_TEMP);
    if(!output) {
        storage_file_close(input);
        storage_file_free(input);
        return false;
    }

    StateReader reader;
    state_reader_init(&reader, input);
    char line[STATE_LINE_LEN];
    uint16_t index = 0U;
    bool found = false;
    bool ok = true;
    while(ok && state_read_line(&reader, line, sizeof(line))) {
        PocketSavedEncounter encounter;
        if(!state_parse_encounter(line, &encounter)) {
            ok = false;
            break;
        }
        if(index == wanted)
            found = true;
        else
            ok = state_write_encounter(output, &encounter);
        ++index;
    }
    if(storage_file_get_error(input) != FSE_OK) ok = false;
    if(ok && found) ok = storage_file_sync(output);

    storage_file_close(input);
    storage_file_free(input);
    storage_file_close(output);
    storage_file_free(output);
    if(!ok || !found) {
        storage_common_remove(storage, ENCOUNTERS_TEMP);
        return false;
    }
    return state_publish(storage, ENCOUNTERS_TEMP, ENCOUNTERS_PATH);
}

bool pocket_bestiary_encounter_rename(Storage* storage, uint16_t wanted, const char* new_name) {
    if(!storage || !new_name || !new_name[0]) return false;
    char normalized_name[POCKET_BESTIARY_ENCOUNTER_NAME_LEN];
    state_safe_field(normalized_name, sizeof(normalized_name), new_name);
    if(!normalized_name[0]) return false;
    File* input = storage_file_alloc(storage);
    if(!input) return false;
    if(!storage_file_open(input, ENCOUNTERS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(input);
        return false;
    }
    File* output = state_open_temp(storage, ENCOUNTERS_TEMP);
    if(!output) {
        storage_file_close(input);
        storage_file_free(input);
        return false;
    }

    StateReader reader;
    state_reader_init(&reader, input);
    char line[STATE_LINE_LEN];
    uint16_t index = 0U;
    bool found = false;
    bool collision = false;
    bool ok = true;
    while(ok && state_read_line(&reader, line, sizeof(line))) {
        PocketSavedEncounter encounter;
        if(!state_parse_encounter(line, &encounter)) {
            ok = false;
            break;
        }
        if(index != wanted && !strcmp(encounter.name, normalized_name)) {
            collision = true;
            ok = false;
            break;
        }
        if(index == wanted) {
            state_copy(encounter.name, sizeof(encounter.name), normalized_name);
            found = true;
        }
        ok = state_write_encounter(output, &encounter);
        ++index;
    }
    if(storage_file_get_error(input) != FSE_OK) ok = false;
    if(ok && found && !collision) ok = storage_file_sync(output);

    storage_file_close(input);
    storage_file_free(input);
    storage_file_close(output);
    storage_file_free(output);
    if(!ok || !found || collision) {
        storage_common_remove(storage, ENCOUNTERS_TEMP);
        return false;
    }
    return state_publish(storage, ENCOUNTERS_TEMP, ENCOUNTERS_PATH);
}

bool pocket_bestiary_encounter_duplicate(Storage* storage, uint16_t wanted, const char* new_name) {
    if(!storage || !new_name || !new_name[0]) return false;
    char normalized_name[POCKET_BESTIARY_ENCOUNTER_NAME_LEN];
    state_safe_field(normalized_name, sizeof(normalized_name), new_name);
    if(!normalized_name[0]) return false;
    File* input = storage_file_alloc(storage);
    if(!input) return false;
    if(!storage_file_open(input, ENCOUNTERS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(input);
        return false;
    }
    File* output = state_open_temp(storage, ENCOUNTERS_TEMP);
    if(!output) {
        storage_file_close(input);
        storage_file_free(input);
        return false;
    }

    StateReader reader;
    state_reader_init(&reader, input);
    char line[STATE_LINE_LEN];
    uint16_t index = 0U;
    uint16_t count = 0U;
    bool found = false;
    bool collision = false;
    bool ok = true;
    PocketSavedEncounter duplicate = {0};
    while(ok && state_read_line(&reader, line, sizeof(line))) {
        PocketSavedEncounter encounter;
        if(!state_parse_encounter(line, &encounter)) {
            ok = false;
            break;
        }
        if(count >= STATE_MAX_ENCOUNTERS) {
            ok = false;
            break;
        }
        if(!strcmp(encounter.name, normalized_name)) collision = true;
        if(index == wanted) {
            duplicate = encounter;
            found = true;
        }
        ok = state_write_encounter(output, &encounter);
        ++index;
        ++count;
    }
    if(storage_file_get_error(input) != FSE_OK) ok = false;
    if(ok && found && !collision && count < STATE_MAX_ENCOUNTERS) {
        state_copy(duplicate.name, sizeof(duplicate.name), normalized_name);
        ok = state_write_encounter(output, &duplicate) && storage_file_sync(output);
    } else {
        ok = false;
    }

    storage_file_close(input);
    storage_file_free(input);
    storage_file_close(output);
    storage_file_free(output);
    if(!ok) {
        storage_common_remove(storage, ENCOUNTERS_TEMP);
        return false;
    }
    return state_publish(storage, ENCOUNTERS_TEMP, ENCOUNTERS_PATH);
}

static bool state_archive_append(Storage* storage, const PocketSavedEncounter* encounter) {
    if(!storage || !encounter) return false;
    File* output = state_open_temp(storage, ENCOUNTERS_ARCHIVE_TEMP);
    if(!output) return false;
    File* input = storage_file_alloc(storage);
    if(!input) {
        storage_file_close(output);
        storage_file_free(output);
        storage_common_remove(storage, ENCOUNTERS_ARCHIVE_TEMP);
        return false;
    }
    bool input_open =
        storage_file_open(input, ENCOUNTERS_ARCHIVE_PATH, FSAM_READ, FSOM_OPEN_EXISTING);
    bool ok = true;
    if(input_open) {
        StateReader reader;
        state_reader_init(&reader, input);
        char line[STATE_LINE_LEN];
        while(ok && state_read_line(&reader, line, sizeof(line))) {
            PocketSavedEncounter archived;
            if(!state_parse_encounter(line, &archived)) {
                ok = false;
                break;
            }
            ok = state_write_encounter(output, &archived);
        }
    }
    if(input_open && storage_file_get_error(input) != FSE_OK) ok = false;
    if(ok) ok = state_write_encounter(output, encounter) && storage_file_sync(output);
    if(input_open) storage_file_close(input);
    storage_file_free(input);
    storage_file_close(output);
    storage_file_free(output);
    if(!ok) {
        storage_common_remove(storage, ENCOUNTERS_ARCHIVE_TEMP);
        return false;
    }
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
