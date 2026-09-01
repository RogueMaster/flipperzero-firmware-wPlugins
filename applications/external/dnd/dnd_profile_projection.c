#include "dnd_profile_projection.h"

#include "dnd_storage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DND_PROJECTION_PATH_LEN 192U
#define DND_PROJECTION_LINE_LEN ((POCKET_D20_DETAIL_LEN * 3U) + 64U)

typedef struct {
    File* file;
    uint8_t buffer[256];
    uint16_t position;
    uint16_t count;
} DndProjectionReader;

static void dnd_projection_reader_init(DndProjectionReader* reader, File* file) {
    memset(reader, 0, sizeof(*reader));
    reader->file = file;
}

static bool dnd_projection_read_line(DndProjectionReader* reader, char* line, size_t size) {
    if(!reader || !reader->file || !line || size < 2U) return false;
    size_t used = 0U;
    bool consumed = false;
    while(true) {
        if(reader->position >= reader->count) {
            reader->count =
                (uint16_t)storage_file_read(reader->file, reader->buffer, sizeof(reader->buffer));
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

static bool dnd_projection_parse_i32(const char* start, const char* end, int32_t* out) {
    if(!start || !end || !out || start >= end) return false;
    bool negative = false;
    if(*start == '+' || *start == '-') {
        negative = *start == '-';
        ++start;
    }
    if(start >= end) return false;
    int64_t value = 0;
    for(const char* p = start; p < end; ++p) {
        if(*p < '0' || *p > '9') return false;
        value = value * 10 + (*p - '0');
        if(value > 2147483648LL) return false;
    }
    if(negative) value = -value;
    if(value < INT32_MIN || value > INT32_MAX) return false;
    *out = (int32_t)value;
    return true;
}

static size_t dnd_projection_parse_numbers(const char* value, int32_t* numbers, size_t maximum) {
    if(!value || !numbers || !maximum) return 0U;
    size_t count = 0U;
    const char* cursor = value;
    while(count < maximum) {
        const char* separator = strchr(cursor, ',');
        const char* end = separator ? separator : cursor + strlen(cursor);
        if(!dnd_projection_parse_i32(cursor, end, &numbers[count])) return 0U;
        ++count;
        if(!separator) return count;
        cursor = separator + 1U;
        if(!cursor[0]) return 0U;
    }
    return cursor[0] ? 0U : count;
}

static bool dnd_projection_indexed_key(
    const char* key,
    const char* prefix,
    const char* suffix,
    uint8_t maximum,
    uint8_t* index) {
    if(!key || !prefix || !suffix || !index) return false;
    size_t prefix_len = strlen(prefix);
    if(strncmp(key, prefix, prefix_len)) return false;
    const char* p = key + prefix_len;
    if(*p < '0' || *p > '9') return false;
    uint32_t value = 0U;
    while(*p >= '0' && *p <= '9') {
        value = value * 10U + (uint32_t)(*p - '0');
        if(value >= maximum) return false;
        ++p;
    }
    if(strcmp(p, suffix)) return false;
    *index = (uint8_t)value;
    return true;
}

static int dnd_projection_hex(char value) {
    if(value >= '0' && value <= '9') return value - '0';
    if(value >= 'A' && value <= 'F') return value - 'A' + 10;
    if(value >= 'a' && value <= 'f') return value - 'a' + 10;
    return -1;
}

static void dnd_projection_decode_string(char* out, size_t size, const char* value) {
    if(!out || !size) return;
    size_t used = 0U;
    for(size_t i = 0U; value && value[i] && used + 1U < size; ++i) {
        if(value[i] == '%' && value[i + 1U] && value[i + 2U]) {
            int hi = dnd_projection_hex(value[i + 1U]);
            int lo = dnd_projection_hex(value[i + 2U]);
            if(hi >= 0 && lo >= 0) {
                out[used++] = (char)((hi << 4) | lo);
                i += 2U;
                continue;
            }
        }
        out[used++] = value[i];
    }
    out[used] = '\0';
}

static bool dnd_projection_open(
    Storage* storage,
    uint32_t profile,
    File** file_out,
    char* path,
    size_t path_size) {
    if(!storage || !file_out || !path || !path_size) return false;
    if(!dnd_storage_find_profile_path(storage, profile, path, path_size)) return false;
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return false;
    }
    *file_out = file;
    return true;
}

static bool dnd_projection_load_common(
    Storage* storage,
    uint32_t profile,
    DndInventoryProfileProjection* inventory,
    DndSpellbookProfileProjection* spellbook,
    DndAdventureProfileProjection* adventure) {
    char path[DND_PROJECTION_PATH_LEN];
    File* file = NULL;
    if(!dnd_projection_open(storage, profile, &file, path, sizeof(path))) return false;
    if(inventory) memset(inventory, 0, sizeof(*inventory));
    if(spellbook) memset(spellbook, 0, sizeof(*spellbook));
    if(adventure) memset(adventure, 0, sizeof(*adventure));

    DndProjectionReader reader;
    dnd_projection_reader_init(&reader, file);
    char* line = malloc(DND_PROJECTION_LINE_LEN);
    if(!line) {
        storage_file_close(file);
        storage_file_free(file);
        return false;
    }
    int32_t n[20];
    bool recognized = false;
    while(dnd_projection_read_line(&reader, line, DND_PROJECTION_LINE_LEN)) {
        char* value = strchr(line, '=');
        if(!value) continue;
        *value++ = '\0';
        const char* key = line;
        uint8_t index = 0U;
        if(!strcmp(key, "Name")) {
            if(inventory)
                dnd_projection_decode_string(inventory->name, sizeof(inventory->name), value);
            if(spellbook)
                dnd_projection_decode_string(spellbook->name, sizeof(spellbook->name), value);
            if(adventure)
                dnd_projection_decode_string(adventure->name, sizeof(adventure->name), value);
            recognized = true;
            continue;
        }
        if(inventory && !strcmp(key, "Species")) {
            dnd_projection_decode_string(inventory->species, sizeof(inventory->species), value);
            recognized = true;
            continue;
        }
        if(inventory && !strcmp(key, "Background")) {
            dnd_projection_decode_string(
                inventory->background, sizeof(inventory->background), value);
            recognized = true;
            continue;
        }
        if(!strcmp(key, "Progress")) {
            size_t count = dnd_projection_parse_numbers(value, n, 4U);
            if(count) {
                uint8_t class_count =
                    n[0] > 0 && n[0] <= (int32_t)POCKET_D20_MAX_CLASSES ? (uint8_t)n[0] : 0U;
                if(inventory) inventory->class_count = class_count;
                if(spellbook) spellbook->class_count = class_count;
                if(adventure) adventure->class_count = class_count;
                recognized = true;
            }
            continue;
        }
        if(dnd_projection_indexed_key(key, "Class", "Name", POCKET_D20_MAX_CLASSES, &index)) {
            if(inventory)
                dnd_projection_decode_string(
                    inventory->classes[index].name, sizeof(inventory->classes[index].name), value);
            if(spellbook)
                dnd_projection_decode_string(
                    spellbook->classes[index].name, sizeof(spellbook->classes[index].name), value);
            if(inventory && inventory->class_count <= index) inventory->class_count = index + 1U;
            if(spellbook && spellbook->class_count <= index) spellbook->class_count = index + 1U;
            if(adventure && adventure->class_count <= index) adventure->class_count = index + 1U;
            recognized = true;
            continue;
        }
        if(dnd_projection_indexed_key(key, "Class", "Subclass", POCKET_D20_MAX_CLASSES, &index)) {
            if(inventory)
                dnd_projection_decode_string(
                    inventory->classes[index].subclass,
                    sizeof(inventory->classes[index].subclass),
                    value);
            if(spellbook)
                dnd_projection_decode_string(
                    spellbook->classes[index].subclass,
                    sizeof(spellbook->classes[index].subclass),
                    value);
            recognized = true;
            continue;
        }
        if(dnd_projection_indexed_key(key, "Class", "Data", POCKET_D20_MAX_CLASSES, &index)) {
            size_t count = dnd_projection_parse_numbers(value, n, 16U);
            if(count) {
                PocketClassLevel* targets[2] = {
                    inventory ? &inventory->classes[index] : NULL,
                    spellbook ? &spellbook->classes[index] : NULL,
                };
                for(uint8_t t = 0U; t < 2U; ++t) {
                    PocketClassLevel* cl = targets[t];
                    if(!cl) continue;
                    if(count >= 1U) cl->level = (uint8_t)n[0];
                    if(count >= 2U) cl->hit_die = (uint8_t)n[1];
                    if(count >= 3U) cl->hit_dice_current = (uint8_t)n[2];
                    if(count >= 4U) cl->hit_dice_max = (uint8_t)n[3];
                    if(count >= 5U) cl->spellcasting_mode = (uint8_t)n[4];
                    if(count >= 6U) cl->spellcasting_ability = (uint8_t)n[5];
                    if(count >= 7U) cl->cantrip_limit = (uint8_t)n[6];
                    if(count >= 8U) cl->prepared_limit = (uint8_t)n[7];
                    if(count >= 9U) cl->spellbook_size = (uint16_t)n[8];
                    if(count >= 10U) cl->pact_slot_level = (uint8_t)n[9];
                    if(count >= 11U) cl->pact_slots_current = (uint8_t)n[10];
                    if(count >= 12U) cl->pact_slots_max = (uint8_t)n[11];
                    if(count >= 13U) cl->mystic_arcanum_mask = (uint16_t)n[12];
                    if(count >= 14U) cl->spell_points_current = (uint16_t)n[13];
                    if(count >= 15U) cl->spell_points_max = (uint16_t)n[14];
                }
                if(adventure && count >= 1U) adventure->class_levels[index] = (uint8_t)n[0];
                recognized = true;
            }
            continue;
        }
        if((inventory || adventure) && !strcmp(key, "AbilityScores")) {
            size_t count = dnd_projection_parse_numbers(value, n, POCKET_D20_ABILITY_COUNT);
            for(size_t i = 0U; i < count; ++i) {
                if(inventory) inventory->ability_scores[i] = (int8_t)n[i];
                if(adventure) adventure->ability_scores[i] = (int8_t)n[i];
            }
            if(count) recognized = true;
            continue;
        }
        if(adventure && !strcmp(key, "SkillProficiency")) {
            size_t count = dnd_projection_parse_numbers(value, n, POCKET_D20_SKILL_COUNT);
            for(size_t i = 0U; i < count; ++i)
                adventure->skill_proficiency[i] = (uint8_t)n[i];
            if(count) recognized = true;
            continue;
        }
        if(adventure && !strcmp(key, "SkillMisc")) {
            size_t count = dnd_projection_parse_numbers(value, n, POCKET_D20_SKILL_COUNT);
            for(size_t i = 0U; i < count; ++i)
                adventure->skill_misc[i] = (int8_t)n[i];
            if(count) recognized = true;
            continue;
        }
        if(inventory && !strcmp(key, "Vitals")) {
            size_t count = dnd_projection_parse_numbers(value, n, 12U);
            if(count >= 4U) inventory->armor_class = (int16_t)n[3];
            if(count >= 7U) inventory->exhaustion = (uint8_t)n[6];
            if(count) recognized = true;
            continue;
        }
        if(inventory && !strcmp(key, "CombatFlags")) {
            size_t count = dnd_projection_parse_numbers(value, n, 3U);
            if(count >= 2U) inventory->encumbrance_mode = n[1] ? 1U : 0U;
            if(count >= 3U) inventory->carrying_capacity_override = (int16_t)n[2];
            if(count) recognized = true;
            continue;
        }
    }
    bool ok = storage_file_get_error(file) == FSE_OK && recognized;
    free(line);
    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

static bool dnd_projection_restore_backup(Storage* storage, uint32_t profile) {
    PocketSaveData* recovery = calloc(1U, sizeof(PocketSaveData));
    if(!recovery) return false;
    bool restored = dnd_storage_restore_backup(storage, profile, recovery);
    dnd_data_clear(recovery);
    free(recovery);
    return restored;
}

bool dnd_profile_projection_load_inventory(
    Storage* storage,
    uint32_t profile,
    DndInventoryProfileProjection* projection) {
    if(!projection) return false;
    if(dnd_projection_load_common(storage, profile, projection, NULL, NULL)) return true;
    return dnd_projection_restore_backup(storage, profile) &&
           dnd_projection_load_common(storage, profile, projection, NULL, NULL);
}

bool dnd_profile_projection_load_spellbook(
    Storage* storage,
    uint32_t profile,
    DndSpellbookProfileProjection* projection) {
    if(!projection) return false;
    if(dnd_projection_load_common(storage, profile, NULL, projection, NULL)) return true;
    return dnd_projection_restore_backup(storage, profile) &&
           dnd_projection_load_common(storage, profile, NULL, projection, NULL);
}

bool dnd_profile_projection_load_adventure(
    Storage* storage,
    uint32_t profile,
    DndAdventureProfileProjection* projection) {
    if(!projection) return false;
    if(dnd_projection_load_common(storage, profile, NULL, NULL, projection)) return true;
    return dnd_projection_restore_backup(storage, profile) &&
           dnd_projection_load_common(storage, profile, NULL, NULL, projection);
}

static bool dnd_projection_write_line(File* file, const char* line) {
    size_t len = strlen(line);
    return storage_file_write(file, line, len) == len && storage_file_write(file, "\n", 1U) == 1U;
}

static bool dnd_projection_publish_temp(
    Storage* storage,
    const char* temporary,
    const char* destination,
    const char* backup) {
    if(!storage || !temporary || !destination || !backup) return false;
    bool had_destination = storage_file_exists(storage, destination);
    if(had_destination) {
        if(storage_file_exists(storage, backup) &&
           storage_common_remove(storage, backup) != FSE_OK)
            return false;
        if(storage_common_rename(storage, destination, backup) != FSE_OK) return false;
    }
    if(storage_common_rename(storage, temporary, destination) == FSE_OK) {
        if(had_destination) storage_common_remove(storage, backup);
        return true;
    }
    if(had_destination) storage_common_rename(storage, backup, destination);
    storage_common_remove(storage, temporary);
    return false;
}

bool dnd_profile_projection_save_inventory_owned(
    Storage* storage,
    uint32_t profile,
    const DndInventoryProfileProjection* projection) {
    if(!storage || !projection) return false;
    char live[DND_PROJECTION_PATH_LEN];
    if(!dnd_storage_find_profile_path(storage, profile, live, sizeof(live))) return false;
    char temp[DND_PROJECTION_PATH_LEN];
    char backup[DND_PROJECTION_PATH_LEN];
    int written = snprintf(temp, sizeof(temp), "%s.proj", live);
    if(written <= 0 || (size_t)written >= sizeof(temp)) return false;
    written = snprintf(backup, sizeof(backup), "%s.proj.bak", live);
    if(written <= 0 || (size_t)written >= sizeof(backup)) return false;
    File* input = storage_file_alloc(storage);
    File* output = storage_file_alloc(storage);
    if(!input || !output) {
        if(input) storage_file_free(input);
        if(output) storage_file_free(output);
        return false;
    }
    bool ok = storage_file_open(input, live, FSAM_READ, FSOM_OPEN_EXISTING) &&
              storage_file_open(output, temp, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    DndProjectionReader reader;
    dnd_projection_reader_init(&reader, input);
    char* line = malloc(DND_PROJECTION_LINE_LEN);
    int32_t n[16];
    bool saw_vitals = false, saw_flags = false;
    if(!line) ok = false;
    while(ok && dnd_projection_read_line(&reader, line, DND_PROJECTION_LINE_LEN)) {
        if(!strncmp(line, "Vitals=", 7U) &&
           dnd_projection_parse_numbers(line + 7U, n, 12U) == 12U) {
            n[3] = projection->armor_class;
            char patched[256];
            snprintf(
                patched,
                sizeof(patched),
                "Vitals=%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld",
                (long)n[0],
                (long)n[1],
                (long)n[2],
                (long)n[3],
                (long)n[4],
                (long)n[5],
                (long)n[6],
                (long)n[7],
                (long)n[8],
                (long)n[9],
                (long)n[10],
                (long)n[11]);
            ok = dnd_projection_write_line(output, patched);
            saw_vitals = true;
        } else if(
            !strncmp(line, "CombatFlags=", 12U) &&
            dnd_projection_parse_numbers(line + 12U, n, 3U) == 3U) {
            char patched[96];
            snprintf(
                patched,
                sizeof(patched),
                "CombatFlags=%ld,%u,%d",
                (long)n[0],
                projection->encumbrance_mode ? 1U : 0U,
                projection->carrying_capacity_override);
            ok = dnd_projection_write_line(output, patched);
            saw_flags = true;
        } else {
            ok = dnd_projection_write_line(output, line);
        }
    }
    if(ok)
        ok = storage_file_get_error(input) == FSE_OK && saw_vitals && saw_flags &&
             storage_file_sync(output);
    free(line);
    storage_file_close(input);
    storage_file_close(output);
    storage_file_free(input);
    storage_file_free(output);
    if(ok)
        ok = dnd_projection_publish_temp(storage, temp, live, backup);
    else
        storage_common_remove(storage, temp);
    return ok;
}
