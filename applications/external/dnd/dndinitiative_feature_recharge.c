#include "dndinitiative_feature_recharge.h"
#include "dnd_profile_handoff.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DND_FEATURE_PATH_LEN 96U
#define DND_FEATURE_LINE_LEN 768U
#define DND_FEATURE_READ_BUFFER 256U

typedef struct {
    File* file;
    uint8_t buffer[DND_FEATURE_READ_BUFFER];
    uint16_t position;
    uint16_t count;
} DndFeatureReader;

static void dndinitiative_feature_recharge_path(char* out, size_t size, uint32_t profile) {
    snprintf(
        out,
        size,
        "%s/feats_%lu.txt",
        POCKET_D20_CHARACTER_DATA_ROOT,
        (unsigned long)profile);
}

static void dndinitiative_feature_recharge_work_path(
    char* out, size_t size, uint32_t profile, const char* suffix) {
    snprintf(
        out,
        size,
        "%s/feats_%lu.%s",
        POCKET_D20_CHARACTER_DATA_ROOT,
        (unsigned long)profile,
        suffix);
}

static void dndinitiative_feature_recharge_reader_init(DndFeatureReader* reader, File* file) {
    memset(reader, 0, sizeof(*reader));
    reader->file = file;
}

static bool dndinitiative_feature_recharge_reader_byte(DndFeatureReader* reader, char* value) {
    if(reader->position >= reader->count) {
        reader->count =
            (uint16_t)storage_file_read(reader->file, reader->buffer, sizeof(reader->buffer));
        reader->position = 0U;
        if(!reader->count) return false;
    }
    *value = (char)reader->buffer[reader->position++];
    return true;
}

static bool dndinitiative_feature_recharge_read_line(DndFeatureReader* reader, char* line, size_t size) {
    size_t used = 0U;
    char value = '\0';
    bool saw = false;
    while(dndinitiative_feature_recharge_reader_byte(reader, &value)) {
        saw = true;
        if(value == '\n') break;
        if(value == '\r') continue;
        if(used + 1U < size)
            line[used++] = value;
        else {
            while(value != '\n' && dndinitiative_feature_recharge_reader_byte(reader, &value)) {
            }
            line[0] = '\0';
            return false;
        }
    }
    line[used] = '\0';
    return saw;
}

static bool dndinitiative_feature_recharge_write_raw(File* file, const char* text) {
    size_t length = strlen(text);
    return storage_file_write(file, text, length) == length;
}

static bool dndinitiative_feature_recharge_parse_i16(const char* text, int16_t* value) {
    if(!text || !*text || !value) return false;
    char* end = NULL;
    long parsed = strtol(text, &end, 10);
    if(!end || *end || parsed < INT16_MIN || parsed > INT16_MAX) return false;
    *value = (int16_t)parsed;
    return true;
}

static bool dndinitiative_feature_recharge_parse_u8(const char* text, uint8_t maximum, uint8_t* value) {
    if(!text || !*text || !value) return false;
    char* end = NULL;
    long parsed = strtol(text, &end, 10);
    if(!end || *end || parsed < 0 || parsed > maximum) return false;
    *value = (uint8_t)parsed;
    return true;
}

static bool dndinitiative_feature_recharge_rewrite_record(
    File* output,
    char* line,
    DndFeatureFastRechargeEvent event) {
    if(strncmp(line, "F|", 2U) != 0) return false;
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
    if(strchr(fields[8], '|')) return false;

    int16_t uses_current = 0;
    int16_t uses_max = 0;
    uint8_t recharge = 0U;
    if(!dndinitiative_feature_recharge_parse_i16(fields[2], &uses_current) ||
       !dndinitiative_feature_recharge_parse_i16(fields[3], &uses_max) ||
       !dndinitiative_feature_recharge_parse_u8(fields[6], 5U, &recharge))
        return false;

    bool matches = event == DndFeatureFastRechargeTurn ? recharge == 1U : recharge == 2U;
    if(matches) uses_current = uses_max;

    char current[16];
    int current_length = snprintf(current, sizeof(current), "%d", uses_current);
    if(current_length <= 0 || (size_t)current_length >= sizeof(current)) return false;

    if(!dndinitiative_feature_recharge_write_raw(output, "F|") || !dndinitiative_feature_recharge_write_raw(output, fields[0]) ||
       !dndinitiative_feature_recharge_write_raw(output, "|") || !dndinitiative_feature_recharge_write_raw(output, fields[1]) ||
       !dndinitiative_feature_recharge_write_raw(output, "|") || !dndinitiative_feature_recharge_write_raw(output, current))
        return false;
    for(uint8_t i = 3U; i < 9U; ++i) {
        if(!dndinitiative_feature_recharge_write_raw(output, "|") || !dndinitiative_feature_recharge_write_raw(output, fields[i]))
            return false;
    }
    return dndinitiative_feature_recharge_write_raw(output, "\n");
}

static bool dndinitiative_feature_recharge_publish(
    Storage* storage, const char* temp, const char* live, const char* backup) {
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

bool dndinitiative_feature_recharge_fast_recharge(
    Storage* storage,
    uint32_t profile,
    DndFeatureFastRechargeEvent event) {
    if(!storage) return false;
    char live[DND_FEATURE_PATH_LEN], temp[DND_FEATURE_PATH_LEN], backup[DND_FEATURE_PATH_LEN];
    dndinitiative_feature_recharge_path(live, sizeof(live), profile);
    if(!storage_file_exists(storage, live)) return true;
    dndinitiative_feature_recharge_work_path(temp, sizeof(temp), profile, "tmp");
    dndinitiative_feature_recharge_work_path(backup, sizeof(backup), profile, "bak");
    storage_common_remove(storage, temp);

    File* input = storage_file_alloc(storage);
    File* output = storage_file_alloc(storage);
    if(!input || !output) {
        if(input) storage_file_free(input);
        if(output) storage_file_free(output);
        return false;
    }
    bool ok = storage_file_open(input, live, FSAM_READ, FSOM_OPEN_EXISTING) &&
              storage_file_open(output, temp, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
              dndinitiative_feature_recharge_write_raw(output, "DNDFeatures=1\n");
    DndFeatureReader reader;
    dndinitiative_feature_recharge_reader_init(&reader, input);
    char line[DND_FEATURE_LINE_LEN];
    while(ok && dndinitiative_feature_recharge_read_line(&reader, line, sizeof(line))) {
        if(!line[0] || line[0] == '#' || strncmp(line, "DNDFeatures=", 12U) == 0) continue;
        ok = dndinitiative_feature_recharge_rewrite_record(output, line, event);
    }
    if(ok) ok = storage_file_get_error(input) == FSE_OK && storage_file_sync(output);
    storage_file_close(input);
    storage_file_close(output);
    storage_file_free(input);
    storage_file_free(output);
    if(!ok) {
        storage_common_remove(storage, temp);
        return false;
    }
    return dndinitiative_feature_recharge_publish(storage, temp, live, backup);
}
