#include "dnd_fs.h"
#include "dnd_handoff.h"
#include "dnd_profile_ref.h"

#include <furi.h>
#include <furi_hal_random.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/number_input.h>
#include <gui/modules/text_input.h>
#include <input/input.h>
#include <storage/storage.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INIT_MAX 24U
#define INIT_NAME_LEN 32U
#define INIT_CONDITION_LEN 64U
#define INIT_PATH_LEN 128U
#define INIT_FILE_PATH APP_DATA_PATH("ch_%lu.%s")

typedef struct {
    char name[INIT_NAME_LEN];
    char conditions[INIT_CONDITION_LEN];
    int16_t hp_current;
    int16_t hp_max;
    int16_t armor_class;
    int16_t total;
    int8_t modifier;
    uint8_t roll_mode;
} InitiativeMember;

typedef enum {
    InitiativeScreenNoCharacter,
    InitiativeScreenMenu,
    InitiativeScreenRoster,
    InitiativeScreenSetup,
    InitiativeScreenCombat,
    InitiativeScreenEdit,
} InitiativeScreen;

typedef enum {
    InitiativeTextNone,
    InitiativeTextName,
    InitiativeTextConditions,
} InitiativeTextTarget;

typedef enum {
    InitiativeRollNormal,
    InitiativeRollAdvantage,
    InitiativeRollDisadvantage,
} InitiativeRollMode;

typedef enum {
    InitiativeNumberNone,
    InitiativeNumberManualRoll,
    InitiativeNumberTotal,
    InitiativeNumberModifier,
    InitiativeNumberArmorClass,
    InitiativeNumberHpCurrent,
    InitiativeNumberHpMax,
} InitiativeNumberTarget;

typedef struct {
    Gui* gui;
    Storage* storage;
    ViewDispatcher* dispatcher;
    View* view;
    TextInput* text_input;
    NumberInput* number_input;
    InitiativeMember roster[INIT_MAX];
    InitiativeMember combat[INIT_MAX];
    uint8_t roster_count;
    uint8_t combat_count;
    uint8_t active;
    uint8_t current_turn;
    uint16_t round;
    uint8_t selection;
    uint8_t scroll;
    uint8_t edit_field;
    uint8_t edit_combat;
    uint8_t edit_setup;
    uint8_t delete_armed;
    InitiativeTextTarget text_target;
    char edit_buffer[INIT_CONDITION_LEN];
    uint8_t input_member;
    InitiativeNumberTarget number_target;
    uint8_t number_combat;
    uint8_t return_to_dnd;
    uint8_t have_character;
    uint8_t roll_mode;
    char main_character_name[INIT_NAME_LEN];
    uint32_t character_id;
    InitiativeScreen screen;
    char status[32];
} InitiativeApp;

static void initiative_copy(char* out, size_t size, const char* in) {
    if(!size) return;
    strncpy(out, in ? in : "", size - 1U);
    out[size - 1U] = '\0';
}

static void initiative_redraw(InitiativeApp* app) {
    if(!app || !app->view) return;
    InitiativeApp** model = view_get_model(app->view);
    if(!model) return;
    *model = app;
    view_commit_model(app->view, true);
}

static bool initiative_parse_u32(const char* text, const char** end, uint32_t* output) {
    if(!text || *text < '0' || *text > '9') return false;
    uint32_t value = 0U;
    const char* p = text;
    while(*p >= '0' && *p <= '9') {
        uint32_t digit = (uint32_t)(*p - '0');
        if(value > (UINT32_MAX - digit) / 10U) return false;
        value = value * 10U + digit;
        ++p;
    }
    *output = value;
    if(end) *end = p;
    return true;
}

static int16_t initiative_clamp(int32_t value, int16_t low, int16_t high) {
    if(value < low) return low;
    if(value > high) return high;
    return (int16_t)value;
}

static int32_t initiative_parse_i32(const char* text) {
    if(!text) return 0;
    bool negative = *text == '-';
    if(negative || *text == '+') ++text;
    int32_t value = 0;
    while(*text >= '0' && *text <= '9') {
        int32_t digit = *text++ - '0';
        if(value > (INT32_MAX - digit) / 10) return negative ? INT32_MIN : INT32_MAX;
        value = value * 10 + digit;
    }
    return negative ? -value : value;
}

static bool initiative_path(char* out, size_t size, uint32_t id, const char* suffix) {
    int n = snprintf(out, size, INIT_FILE_PATH, (unsigned long)id, suffix);
    return n > 0 && (size_t)n < size;
}


static int8_t initiative_ability_modifier(int32_t score) {
    int32_t delta = score - 10;
    if(delta >= 0) return (int8_t)(delta / 2);
    return (int8_t)(-(((-delta) + 1) / 2));
}

static uint8_t initiative_roll_d20(InitiativeRollMode mode) {
    uint8_t first = (uint8_t)(1U + (furi_hal_random_get() % 20U));
    if(mode == InitiativeRollNormal) return first;
    uint8_t second = (uint8_t)(1U + (furi_hal_random_get() % 20U));
    return mode == InitiativeRollAdvantage ? (first > second ? first : second) :
                                             (first < second ? first : second);
}

static const char* initiative_roll_mode_name(InitiativeRollMode mode) {
    if(mode == InitiativeRollAdvantage) return "Advantage";
    if(mode == InitiativeRollDisadvantage) return "Disadvantage";
    return "Normal";
}

typedef struct {
    File* file;
    uint8_t buffer[256];
    uint16_t position;
    uint16_t count;
} InitiativeReader;

static bool initiative_read_line_checked(
    InitiativeReader* reader, char* line, size_t size, bool* overflow) {
    if(!reader || !line || size < 2U) return false;
    size_t used = 0U;
    bool consumed = false;
    bool too_long = false;
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
        if(used + 1U < size)
            line[used++] = ch;
        else
            too_long = true;
    }
    line[used] = '\0';
    if(overflow) *overflow = too_long;
    return consumed;
}

static bool initiative_read_line(InitiativeReader* reader, char* line, size_t size) {
    return initiative_read_line_checked(reader, line, size, NULL);
}

static bool initiative_parse_i32_strict(const char* text, int32_t* output) {
    if(!text || !text[0] || !output) return false;
    bool negative = false;
    if(*text == '-' || *text == '+') {
        negative = *text == '-';
        ++text;
    }
    if(*text < '0' || *text > '9') return false;
    uint32_t value = 0U;
    uint32_t maximum = negative ? (uint32_t)INT32_MAX + 1U : (uint32_t)INT32_MAX;
    while(*text >= '0' && *text <= '9') {
        uint32_t digit = (uint32_t)(*text - '0');
        if(value > (maximum - digit) / 10U) return false;
        value = value * 10U + digit;
        ++text;
    }
    if(*text) return false;
    *output = negative ? (value == (uint32_t)INT32_MAX + 1U ? INT32_MIN : -(int32_t)value) :
                         (int32_t)value;
    return true;
}

static bool initiative_indexed_key(
    const char* key,
    const char* prefix,
    const char* suffix,
    uint8_t* index) {
    size_t prefix_length = strlen(prefix);
    if(strncmp(key, prefix, prefix_length) != 0) return false;
    const char* cursor = key + prefix_length;
    const char* digits = cursor;
    uint32_t value = 0U;
    while(*cursor >= '0' && *cursor <= '9') {
        value = value * 10U + (uint32_t)(*cursor - '0');
        if(value >= INIT_MAX) return false;
        ++cursor;
    }
    if(cursor == digits || strcmp(cursor, suffix) != 0) return false;
    *index = (uint8_t)value;
    return true;
}

static bool initiative_write_named(File* file, const char* key, const char* value) {
    char line[160];
    size_t used = 0U;
    int prefix = snprintf(line, sizeof(line), "%s=", key);
    if(prefix <= 0 || (size_t)prefix >= sizeof(line)) return false;
    used = (size_t)prefix;
    for(size_t i = 0U; value && value[i]; ++i) {
        char ch = value[i];
        if(ch == '\r' || ch == '\n') ch = ' ';
        if(used + 2U >= sizeof(line)) return false;
        line[used++] = ch;
    }
    line[used++] = '\n';
    return storage_file_write(file, line, used) == used;
}

static bool initiative_write_number(File* file, const char* key, int32_t value) {
    char line[64];
    int length = snprintf(line, sizeof(line), "%s=%ld\n", key, (long)value);
    return length > 0 && (size_t)length < sizeof(line) &&
           storage_file_write(file, line, (size_t)length) == (size_t)length;
}

static bool initiative_write_member_named(
    File* file,
    const char* prefix,
    uint8_t index,
    const InitiativeMember* member) {
    char key[40];
#define INIT_MEMBER_STRING(suffix, field) \
    do { snprintf(key, sizeof(key), "%s%u%s", prefix, index, suffix); if(!initiative_write_named(file, key, field)) return false; } while(false)
#define INIT_MEMBER_NUMBER(suffix, field) \
    do { snprintf(key, sizeof(key), "%s%u%s", prefix, index, suffix); if(!initiative_write_number(file, key, field)) return false; } while(false)
    INIT_MEMBER_STRING("Name", member->name);
    INIT_MEMBER_NUMBER("HpCurrent", member->hp_current);
    INIT_MEMBER_NUMBER("HpMax", member->hp_max);
    INIT_MEMBER_NUMBER("ArmorClass", member->armor_class);
    INIT_MEMBER_NUMBER("Modifier", member->modifier);
    INIT_MEMBER_NUMBER("RollMode", member->roll_mode);
    INIT_MEMBER_NUMBER("Total", member->total);
    INIT_MEMBER_STRING("Conditions", member->conditions);
#undef INIT_MEMBER_STRING
#undef INIT_MEMBER_NUMBER
    return true;
}


static bool initiative_save(InitiativeApp* app) {
    storage_common_mkdir(app->storage, APP_DATA_PATH(""));
    char path[INIT_PATH_LEN], temp[INIT_PATH_LEN], backup[INIT_PATH_LEN];
    if(!initiative_path(path, sizeof(path), app->character_id, "txt") ||
       !initiative_path(temp, sizeof(temp), app->character_id, "tmp") ||
       !initiative_path(backup, sizeof(backup), app->character_id, "bak"))
        return false;
    File* file = storage_file_alloc(app->storage);
    if(!file) return false;
    bool ok = storage_file_open(file, temp, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
              initiative_write_number(file, "DNDInitiative", 1) &&
              initiative_write_number(file, "CharacterId", (int32_t)app->character_id) &&
              initiative_write_number(file, "RollMode", app->roll_mode) &&
              initiative_write_named(file, "MainCharacterName", app->main_character_name) &&
              initiative_write_number(file, "RosterCount", app->roster_count);
    for(uint8_t i = 0U; ok && i < app->roster_count; ++i)
        ok = initiative_write_member_named(file, "Roster", i, &app->roster[i]);
    if(ok) ok = initiative_write_number(file, "Active", app->active);
    if(ok) ok = initiative_write_number(file, "Round", app->round);
    if(ok) ok = initiative_write_number(file, "CurrentTurn", app->current_turn);
    if(ok) ok = initiative_write_number(file, "CombatCount", app->combat_count);
    for(uint8_t i = 0U; ok && i < app->combat_count; ++i)
        ok = initiative_write_member_named(file, "Combat", i, &app->combat[i]);
    if(ok) ok = storage_file_write(file, "End=OK\n", 7U) == 7U && storage_file_sync(file);
    storage_file_close(file);
    storage_file_free(file);
    if(!ok) {
        storage_common_remove(app->storage, temp);
        return false;
    }
    storage_common_remove(app->storage, backup);
    if(storage_file_exists(app->storage, path) &&
       storage_common_rename(app->storage, path, backup) != FSE_OK)
        return false;
    if(storage_common_rename(app->storage, temp, path) != FSE_OK) {
        if(storage_file_exists(app->storage, backup))
            storage_common_rename(app->storage, backup, path);
        return false;
    }
    storage_common_remove(app->storage, backup);
    return true;
}


static bool initiative_apply_member_field(
    const char* key,
    const char* value,
    const char* prefix,
    InitiativeMember* list,
    uint8_t* count) {
    uint8_t index = 0U;
    InitiativeMember* member = NULL;
    int32_t number = 0;
    bool applied = false;

    if(initiative_indexed_key(key, prefix, "Name", &index)) {
        member = &list[index];
        initiative_copy(member->name, sizeof(member->name), value);
        applied = true;
    } else if(initiative_indexed_key(key, prefix, "HpCurrent", &index)) {
        member = &list[index];
        if(initiative_parse_i32_strict(value, &number)) {
            member->hp_current = initiative_clamp(number, -999, 999);
            applied = true;
        }
    } else if(initiative_indexed_key(key, prefix, "HpMax", &index)) {
        member = &list[index];
        if(initiative_parse_i32_strict(value, &number)) {
            member->hp_max = initiative_clamp(number, 0, 999);
            applied = true;
        }
    } else if(initiative_indexed_key(key, prefix, "ArmorClass", &index)) {
        member = &list[index];
        if(initiative_parse_i32_strict(value, &number)) {
            member->armor_class = initiative_clamp(number, 0, 99);
            applied = true;
        }
    } else if(initiative_indexed_key(key, prefix, "Modifier", &index)) {
        member = &list[index];
        if(initiative_parse_i32_strict(value, &number)) {
            member->modifier = (int8_t)initiative_clamp(number, -50, 50);
            applied = true;
        }
    } else if(initiative_indexed_key(key, prefix, "RollMode", &index)) {
        member = &list[index];
        if(initiative_parse_i32_strict(value, &number)) {
            member->roll_mode = (uint8_t)initiative_clamp(number, InitiativeRollNormal, InitiativeRollDisadvantage);
            applied = true;
        }
    } else if(initiative_indexed_key(key, prefix, "Total", &index)) {
        member = &list[index];
        if(initiative_parse_i32_strict(value, &number)) {
            member->total = initiative_clamp(number, -99, 199);
            applied = true;
        }
    } else if(initiative_indexed_key(key, prefix, "Conditions", &index)) {
        member = &list[index];
        initiative_copy(member->conditions, sizeof(member->conditions), value);
        applied = true;
    } else {
        return false;
    }

    if(applied && *count <= index) *count = (uint8_t)(index + 1U);
    return applied;
}

static void initiative_load(InitiativeApp* app) {
    char path[INIT_PATH_LEN];
    if(!initiative_path(path, sizeof(path), app->character_id, "txt")) return;
    File* file = storage_file_alloc(app->storage);
    if(!file || !storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(file) storage_file_free(file);
        return;
    }

    memset(app->roster, 0, sizeof(app->roster));
    memset(app->combat, 0, sizeof(app->combat));
    app->roster_count = 0U;
    app->combat_count = 0U;
    app->active = 0U;
    app->round = 1U;
    app->current_turn = 0U;

    InitiativeReader reader = {.file = file};
    char line[192];
    while(initiative_read_line(&reader, line, sizeof(line))) {
        char* value = strchr(line, '=');
        if(!value) continue;
        *value++ = '\0';
        int32_t number = 0;

        if(!strcmp(line, "MainCharacterName")) {
            initiative_copy(app->main_character_name, sizeof(app->main_character_name), value);
        } else if(!strcmp(line, "RollMode")) {
            if(initiative_parse_i32_strict(value, &number) && number >= 0 &&
               number <= (int32_t)InitiativeRollDisadvantage)
                app->roll_mode = (uint8_t)number;
        } else if(!strcmp(line, "Active")) {
            if(initiative_parse_i32_strict(value, &number)) app->active = number ? 1U : 0U;
        } else if(!strcmp(line, "Round")) {
            if(initiative_parse_i32_strict(value, &number) && number >= 1 && number <= (int32_t)UINT16_MAX)
                app->round = (uint16_t)number;
        } else if(!strcmp(line, "CurrentTurn")) {
            if(initiative_parse_i32_strict(value, &number) && number >= 0 && number < (int32_t)INIT_MAX)
                app->current_turn = (uint8_t)number;
        } else if(initiative_apply_member_field(
                      line, value, "Roster", app->roster, &app->roster_count)) {
            /* Applied by field name. */
        } else if(initiative_apply_member_field(
                      line, value, "Combat", app->combat, &app->combat_count)) {
            /* Applied by field name. */
        }
        /* DNDInitiative, CharacterId, counts, End, malformed values, and unknown
           future fields are informational and never invalidate neighboring data. */
    }

    storage_file_close(file);
    storage_file_free(file);
    if(app->current_turn >= app->combat_count) app->current_turn = 0U;
    if(!app->round) app->round = 1U;
}

static bool initiative_refresh_main_character(InitiativeApp* app) {
    if(!app || !app->storage || !app->have_character) return false;
    char path[INIT_PATH_LEN];
    if(!dnd_profile_ref_path(app->storage, app->character_id, path, sizeof(path))) return false;
    File* file = storage_file_alloc(app->storage);
    if(!file || !storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(file) storage_file_free(file);
        return false;
    }

    char name[INIT_NAME_LEN] = {0};
    int32_t dexterity = 10;
    int32_t hp_current = 1, hp_max = 1, armor_class = 10, initiative_misc = 0, exhaustion = 0;
    uint8_t total_level = 0U;
    bool has_alert = false, has_jack_of_all_trades = false;
    bool have_name = false, have_abilities = false, have_vitals = false;
    InitiativeReader reader = {.file = file};
    char line[192];
    while(initiative_read_line(&reader, line, sizeof(line))) {
        char* value = strchr(line, '=');
        if(!value) continue;
        *value++ = '\0';
        if(!strcmp(line, "Name")) {
            initiative_copy(name, sizeof(name), value);
            have_name = name[0] != '\0';
        } else if(!strcmp(line, "OriginFeat")) {
            if(!strcmp(value, "Alert")) has_alert = true;
        } else if(!strcmp(line, "AbilityScores")) {
            int32_t values[6] = {0};
            uint8_t count = 0U;
            char* cursor = value;
            while(count < 6U && *cursor) {
                char* comma = strchr(cursor, ',');
                if(comma) *comma = '\0';
                if(!initiative_parse_i32_strict(cursor, &values[count])) break;
                ++count;
                if(!comma) break;
                cursor = comma + 1U;
            }
            if(count >= 2U) { dexterity = values[1]; have_abilities = true; }
        } else if(!strncmp(line, "Class", 5U) && strstr(line, "Data")) {
            int32_t class_level = 0;
            char* comma = strchr(value, ',');
            if(comma) *comma = '\0';
            if(initiative_parse_i32_strict(value, &class_level) && class_level > 0) {
                uint16_t next = (uint16_t)total_level + (uint16_t)class_level;
                total_level = (uint8_t)(next > 20U ? 20U : next);
            }
        } else if(!strncmp(line, "Feature", 7U) && strstr(line, "Name")) {
            if(!strcmp(value, "Alert")) has_alert = true;
            else if(!strcmp(value, "Jack of All Trades")) has_jack_of_all_trades = true;
        } else if(!strcmp(line, "Vitals")) {
            int32_t values[12] = {0};
            uint8_t count = 0U;
            char* cursor = value;
            while(count < 12U && *cursor) {
                char* comma = strchr(cursor, ',');
                if(comma) *comma = '\0';
                if(!initiative_parse_i32_strict(cursor, &values[count])) break;
                ++count;
                if(!comma) break;
                cursor = comma + 1U;
            }
            if(count >= 7U) {
                hp_current = values[0]; hp_max = values[1]; armor_class = values[3];
                initiative_misc = values[5]; exhaustion = values[6]; have_vitals = true;
            }
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    if(!have_name || !have_abilities || !have_vitals) return false;

    uint8_t proficiency = total_level ? (uint8_t)(2U + (total_level - 1U) / 4U) : 2U;
    int32_t feature_bonus = has_alert ? proficiency :
                            (has_jack_of_all_trades ? (int32_t)(proficiency / 2U) : 0);
    int16_t modifier = initiative_clamp(
        (int32_t)initiative_ability_modifier(dexterity) + initiative_misc + feature_bonus -
            (2 * exhaustion),
        -50,
        50);
    InitiativeMember* member = NULL;
    bool changed = false;
    if(app->main_character_name[0]) {
        for(uint8_t i = 0U; i < app->roster_count; ++i) {
            if(!strcmp(app->roster[i].name, app->main_character_name)) { member = &app->roster[i]; break; }
        }
    }
    if(!member) {
        for(uint8_t i = 0U; i < app->roster_count; ++i) {
            if(!strcmp(app->roster[i].name, name)) { member = &app->roster[i]; break; }
        }
    }
    if(!member && app->roster_count < INIT_MAX) {
        member = &app->roster[app->roster_count++];
        memset(member, 0, sizeof(*member));
        initiative_copy(member->name, sizeof(member->name), name);
        member->roll_mode = app->roll_mode;
        changed = true;
    }
    if(!member) return false;
    char prior_main_name[INIT_NAME_LEN];
    initiative_copy(prior_main_name, sizeof(prior_main_name), app->main_character_name);
    int16_t next_hp_current = initiative_clamp(hp_current, -999, 999);
    int16_t next_hp_max = initiative_clamp(hp_max, 0, 999);
    int16_t next_ac = initiative_clamp(armor_class, 0, 99);
    if(strcmp(member->name, name) || strcmp(app->main_character_name, name) ||
       member->hp_current != next_hp_current || member->hp_max != next_hp_max ||
       member->armor_class != next_ac || member->modifier != (int8_t)modifier)
        changed = true;
    initiative_copy(member->name, sizeof(member->name), name);
    initiative_copy(app->main_character_name, sizeof(app->main_character_name), name);
    member->hp_current = next_hp_current;
    member->hp_max = next_hp_max;
    member->armor_class = next_ac;
    member->modifier = (int8_t)modifier;

    for(uint8_t i = 0U; i < app->combat_count; ++i) {
        if(!strcmp(app->combat[i].name, name) ||
           (prior_main_name[0] && !strcmp(app->combat[i].name, prior_main_name))) {
            if(strcmp(app->combat[i].name, name) || app->combat[i].modifier != (int8_t)modifier ||
               app->combat[i].hp_current != member->hp_current ||
               app->combat[i].hp_max != member->hp_max ||
               app->combat[i].armor_class != member->armor_class)
                changed = true;
            initiative_copy(app->combat[i].name, sizeof(app->combat[i].name), name);
            app->combat[i].modifier = (int8_t)modifier;
            app->combat[i].hp_current = member->hp_current;
            app->combat[i].hp_max = member->hp_max;
            app->combat[i].armor_class = member->armor_class;
            break;
        }
    }
    return changed;
}

static bool initiative_member_is_main(const InitiativeApp* app, const InitiativeMember* member) {
    return app && member && app->main_character_name[0] &&
           !strcmp(app->main_character_name, member->name);
}

static uint8_t initiative_parse_csv(char* text, int32_t* values, uint8_t capacity) {
    if(!text || !values || !capacity) return 0U;
    uint8_t count = 0U;
    char* cursor = text;
    while(count < capacity && *cursor) {
        char* comma = strchr(cursor, ',');
        if(comma) *comma = '\0';
        if(!initiative_parse_i32_strict(cursor, &values[count])) return count;
        ++count;
        if(!comma) break;
        cursor = comma + 1U;
    }
    return count;
}

static bool initiative_write_line(File* file, const char* line) {
    if(!file || !line) return false;
    size_t length = strlen(line);
    return storage_file_write(file, line, length) == length &&
           storage_file_write(file, "\n", 1U) == 1U;
}

/* Patch only the canonical fields Initiative owns. Unknown/malformed character fields
   are copied unchanged so a partial or future save remains best-effort readable. */
static bool initiative_patch_character(
    InitiativeApp* app,
    const InitiativeMember* sync_member,
    uint8_t recharge_cadence) {
    if(!app || !app->storage || !app->have_character) return false;
    char path[INIT_PATH_LEN];
    if(!dnd_profile_ref_path(app->storage, app->character_id, path, sizeof(path))) return false;
    if(!storage_file_exists(app->storage, path)) return false;

    char temp[INIT_PATH_LEN], backup[INIT_PATH_LEN];
    int tn = snprintf(temp, sizeof(temp), "%s.itmp", path);
    int bn = snprintf(backup, sizeof(backup), "%s.ibak", path);
    if(tn <= 0 || bn <= 0 || (size_t)tn >= sizeof(temp) || (size_t)bn >= sizeof(backup))
        return false;

    File* input = storage_file_alloc(app->storage);
    File* output = storage_file_alloc(app->storage);
    char* line = malloc(768U);
    char* parse = malloc(768U);
    if(!input || !output || !line || !parse) {
        if(input) storage_file_free(input);
        if(output) storage_file_free(output);
        free(line);
        free(parse);
        return false;
    }
    bool ok = storage_file_open(input, path, FSAM_READ, FSOM_OPEN_EXISTING) &&
              storage_file_open(output, temp, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    bool touched = false;
    if(ok) {
        InitiativeReader reader = {.file = input};
        bool overflow = false;
        while(ok && initiative_read_line_checked(&reader, line, 768U, &overflow)) {
            if(overflow) {
                ok = false;
                break;
            }
            char replacement[192];
            const char* out_line = line;
            initiative_copy(parse, 768U, line);
            char* value = strchr(parse, '=');
            if(value) {
                *value++ = '\0';
                if(sync_member && !strcmp(parse, "Vitals")) {
                    int32_t values[12] = {0};
                    if(initiative_parse_csv(value, values, 12U) == 12U) {
                        values[0] = sync_member->hp_current;
                        values[1] = sync_member->hp_max;
                        values[3] = sync_member->armor_class;
                        int n = snprintf(
                            replacement,
                            sizeof(replacement),
                            "Vitals=%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld",
                            (long)values[0], (long)values[1], (long)values[2], (long)values[3],
                            (long)values[4], (long)values[5], (long)values[6], (long)values[7],
                            (long)values[8], (long)values[9], (long)values[10], (long)values[11]);
                        if(n > 0 && (size_t)n < sizeof(replacement)) {
                            out_line = replacement;
                            touched = true;
                        }
                    }
                } else if(
                    recharge_cadence && !strncmp(parse, "Feature", 7U) &&
                    strlen(parse) > 11U && !strcmp(parse + strlen(parse) - 4U, "Data")) {
                    int32_t values[7] = {0};
                    if(initiative_parse_csv(value, values, 7U) == 7U &&
                       values[4] == recharge_cadence && values[0] != values[1]) {
                        values[0] = values[1];
                        int n = snprintf(
                            replacement,
                            sizeof(replacement),
                            "%s=%ld,%ld,%ld,%ld,%ld,%ld,%ld",
                            parse,
                            (long)values[0], (long)values[1], (long)values[2], (long)values[3],
                            (long)values[4], (long)values[5], (long)values[6]);
                        if(n > 0 && (size_t)n < sizeof(replacement)) {
                            out_line = replacement;
                            touched = true;
                        }
                    }
                }
            }
            ok = initiative_write_line(output, out_line);
        }
        if(ok) ok = storage_file_sync(output);
    }
    storage_file_close(input);
    storage_file_close(output);
    storage_file_free(input);
    storage_file_free(output);
    free(line);
    free(parse);

    if(!ok) {
        storage_common_remove(app->storage, temp);
        return false;
    }
    if(!touched) {
        storage_common_remove(app->storage, temp);
        return true;
    }
    storage_common_remove(app->storage, backup);
    if(storage_common_rename(app->storage, path, backup) != FSE_OK) {
        storage_common_remove(app->storage, temp);
        return false;
    }
    if(storage_common_rename(app->storage, temp, path) != FSE_OK) {
        storage_common_rename(app->storage, backup, path);
        return false;
    }
    storage_common_remove(app->storage, backup);
    return true;
}

static void initiative_sync_main_if_needed(InitiativeApp* app, InitiativeMember* member) {
    if(initiative_member_is_main(app, member)) initiative_patch_character(app, member, 0U);
}

static void initiative_swap(InitiativeApp* app, uint8_t first, uint8_t second) {
    if(!app || first >= app->combat_count || second >= app->combat_count || first == second) return;
    InitiativeMember temporary = app->combat[first];
    app->combat[first] = app->combat[second];
    app->combat[second] = temporary;
    if(app->current_turn == first)
        app->current_turn = second;
    else if(app->current_turn == second)
        app->current_turn = first;
}

static void initiative_import_args(InitiativeApp* app, const char* args) {
    const char* cursor = args;
    const char* end = NULL;
    uint32_t id = 0U;
    if(!initiative_parse_u32(cursor, &end, &id)) return;
    app->character_id = id;
    cursor = end;
    while(*cursor == ';' && app->roster_count < INIT_MAX) {
        ++cursor;
        const char* record_end = strchr(cursor, ';');
        if(!record_end) record_end = cursor + strlen(cursor);
        char record[128];
        size_t length = (size_t)(record_end - cursor);
        if(length >= sizeof(record)) length = sizeof(record) - 1U;
        memcpy(record, cursor, length);
        record[length] = '\0';
        char* a = strchr(record, ',');
        char* b = a ? strchr(a + 1U, ',') : NULL;
        char* c = b ? strchr(b + 1U, ',') : NULL;
        if(a && b && c) {
            *a++ = '\0';
            *b++ = '\0';
            *c++ = '\0';
            InitiativeMember* member = &app->roster[app->roster_count];
            memset(member, 0, sizeof(*member));
            initiative_copy(member->name, sizeof(member->name), record);
            member->hp_current = member->hp_max =
                initiative_clamp(initiative_parse_i32(a), 0, 999);
            member->armor_class = initiative_clamp(initiative_parse_i32(b), 0, 99);
            member->modifier =
                (int8_t)initiative_clamp(initiative_parse_i32(c), -50, 50);
            if(member->name[0]) ++app->roster_count;
        }
        cursor = record_end;
    }
}

static void initiative_row(Canvas* canvas, uint8_t row, bool selected, const char* text) {
    uint8_t y = (uint8_t)(13U + row * 10U);
    if(selected) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, y, 128, 10);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, (uint8_t)(y + 8U), text);
    if(selected) canvas_set_color(canvas, ColorBlack);
}

static void initiative_draw(Canvas* canvas, void* model) {
    InitiativeApp* app = *(InitiativeApp**)model;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 9, "DNDInitiative");
    if(app->screen == InitiativeScreenNoCharacter) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 24, "No DND character found.");
        canvas_draw_str(canvas, 2, 34, "Create/load one first.");
        initiative_row(canvas, 3U, app->selection == 0U, "Launch DNDolphins");
        initiative_row(canvas, 4U, app->selection == 1U, "Exit Initiative");
    } else if(app->screen == InitiativeScreenMenu) {
        char rows[7][32];
        snprintf(rows[0], sizeof(rows[0]), "Start New Combat");
        snprintf(rows[1], sizeof(rows[1]), "Resume%s", app->active ? "" : " (none)");
        snprintf(rows[2], sizeof(rows[2]), "Party Roster (%u)", app->roster_count);
        snprintf(rows[3], sizeof(rows[3]), "Edit Current Order");
        initiative_copy(rows[4], sizeof(rows[4]), "End Current Combat");
        snprintf(rows[5], sizeof(rows[5]), "Default Roll: %s", initiative_roll_mode_name((InitiativeRollMode)app->roll_mode));
        initiative_copy(rows[6], sizeof(rows[6]), "Return to DNDolphins");
        for(uint8_t row = 0U; row < 5U; ++row) {
            uint8_t i = (uint8_t)(app->scroll + row);
            if(i >= 7U) break;
            initiative_row(canvas, row, i == app->selection, rows[i]);
        }
    } else if(app->screen == InitiativeScreenRoster) {
        uint8_t total = (uint8_t)(app->roster_count + 1U);
        for(uint8_t row = 0U; row < 5U; ++row) {
            uint8_t i = (uint8_t)(app->scroll + row);
            if(i >= total) break;
            char text[48];
            if(i == app->roster_count)
                initiative_copy(text, sizeof(text), "+ New");
            else
                snprintf(text, sizeof(text), "%.18s HP%d AC%d", app->roster[i].name, app->roster[i].hp_current, app->roster[i].armor_class);
            initiative_row(canvas, row, i == app->selection, text);
        }
    } else if(app->screen == InitiativeScreenSetup) {
        uint8_t total = (uint8_t)(app->combat_count + 3U);
        for(uint8_t row = 0U; row < 5U; ++row) {
            uint8_t i = (uint8_t)(app->scroll + row);
            if(i >= total) break;
            char text[48];
            if(i == 0U) initiative_copy(text, sizeof(text), "Roll for All");
            else if(i <= app->combat_count) {
                InitiativeMember* member = &app->combat[i - 1U];
                snprintf(text, sizeof(text), "%.15s I%d HP%d AC%d", member->name, member->total, member->hp_current, member->armor_class);
            } else if(i == app->combat_count + 1U)
                initiative_copy(text, sizeof(text), "+ Temporary Member");
            else
                initiative_copy(text, sizeof(text), "Begin Combat");
            initiative_row(canvas, row, i == app->selection, text);
        }
    } else if(app->screen == InitiativeScreenCombat) {
        char header[32];
        snprintf(header, sizeof(header), "Round %u", app->round);
        canvas_draw_str(canvas, 86, 9, header);
        for(uint8_t row = 0U; row < 5U; ++row) {
            uint8_t i = (uint8_t)(app->scroll + row);
            if(i >= app->combat_count) break;
            char text[48];
            snprintf(text, sizeof(text), "%c %.10s I%d HP%d AC%d %.5s", i == app->current_turn ? '>' : ' ', app->combat[i].name, app->combat[i].total, app->combat[i].hp_current, app->combat[i].armor_class, app->combat[i].conditions);
            initiative_row(canvas, row, i == app->selection, text);
        }
    } else {
        InitiativeMember* member = app->edit_combat ? &app->combat[app->selection] : &app->roster[app->selection];
        char rows[9][48];
        uint8_t count = app->edit_combat ? 9U : 8U;
        snprintf(rows[0], sizeof(rows[0]), "Name: %.20s", member->name);
        if(app->edit_combat) {
            snprintf(rows[1], sizeof(rows[1]), "Initiative roll: %d", member->total);
            snprintf(rows[2], sizeof(rows[2]), "Modifier: %+d", member->modifier);
            snprintf(rows[3], sizeof(rows[3]), "Roll: %s", initiative_roll_mode_name((InitiativeRollMode)member->roll_mode));
            snprintf(rows[4], sizeof(rows[4]), "Armor Class: %d", member->armor_class);
            snprintf(rows[5], sizeof(rows[5]), "Current HP: %d", member->hp_current);
            snprintf(rows[6], sizeof(rows[6]), "Maximum HP: %d", member->hp_max);
            snprintf(rows[7], sizeof(rows[7]), "Conditions: %.16s", member->conditions);
            initiative_copy(rows[8], sizeof(rows[8]), app->delete_armed ? "OK again: delete" : "Delete");
        } else {
            snprintf(rows[1], sizeof(rows[1]), "Initiative mod: %+d", member->modifier);
            snprintf(rows[2], sizeof(rows[2]), "Roll: %s", initiative_roll_mode_name((InitiativeRollMode)member->roll_mode));
            snprintf(rows[3], sizeof(rows[3]), "Armor Class: %d", member->armor_class);
            snprintf(rows[4], sizeof(rows[4]), "Current HP: %d", member->hp_current);
            snprintf(rows[5], sizeof(rows[5]), "Maximum HP: %d", member->hp_max);
            snprintf(rows[6], sizeof(rows[6]), "Conditions: %.16s", member->conditions);
            initiative_copy(rows[7], sizeof(rows[7]), app->delete_armed ? "OK again: delete" : "Delete");
        }
        for(uint8_t row = 0U; row < 5U; ++row) {
            uint8_t i = (uint8_t)(app->scroll + row);
            if(i >= count) break;
            initiative_row(canvas, row, i == app->edit_field, rows[i]);
        }
    }
}

static void initiative_sort(InitiativeApp* app) {
    for(uint8_t i = 1U; i < app->combat_count; ++i) {
        InitiativeMember value = app->combat[i];
        uint8_t pos = i;
        while(pos && (app->combat[pos - 1U].total < value.total ||
                       (app->combat[pos - 1U].total == value.total &&
                        app->combat[pos - 1U].modifier < value.modifier))) {
            app->combat[pos] = app->combat[pos - 1U];
            --pos;
        }
        app->combat[pos] = value;
    }
}

static void initiative_seed_setup(InitiativeApp* app) {
    initiative_patch_character(app, NULL, 2U);
    app->combat_count = app->roster_count;
    memcpy(app->combat, app->roster, app->roster_count * sizeof(InitiativeMember));
    for(uint8_t i = 0U; i < app->combat_count; ++i) app->combat[i].total = app->combat[i].modifier;
    app->active = 0U;
    app->round = 1U;
    app->current_turn = 0U;
    app->selection = app->scroll = 0U;
    app->screen = InitiativeScreenSetup;
    initiative_save(app);
}

static void initiative_start(InitiativeApp* app) {
    initiative_sort(app);
    app->active = app->combat_count ? 1U : 0U;
    app->round = 1U;
    app->current_turn = 0U;
    app->selection = 0U;
    app->scroll = 0U;
    app->screen = InitiativeScreenCombat;
    initiative_save(app);
}

static void initiative_move(uint8_t* selection, uint8_t count, int8_t direction) {
    if(!count) return;
    int16_t next = (int16_t)*selection + direction;
    if(next < 0) next = count - 1U;
    if(next >= count) next = 0;
    *selection = (uint8_t)next;
}

static void initiative_text_done(void* context) {
    InitiativeApp* app = context;
    InitiativeMember* list = app->edit_combat ? app->combat : app->roster;
    uint8_t count = app->edit_combat ? app->combat_count : app->roster_count;
    if(app->input_member < count) {
        InitiativeMember* member = &list[app->input_member];
        if(app->text_target == InitiativeTextName) {
            bool was_main = initiative_member_is_main(app, member);
            initiative_copy(member->name, sizeof(member->name), app->edit_buffer);
            if(was_main)
                initiative_copy(
                    app->main_character_name, sizeof(app->main_character_name), member->name);
        } else if(app->text_target == InitiativeTextConditions)
            initiative_copy(member->conditions, sizeof(member->conditions), app->edit_buffer);
    }
    app->text_target = InitiativeTextNone;
    initiative_save(app);
    view_dispatcher_switch_to_view(app->dispatcher, 0U);
    initiative_redraw(app);
}

static void initiative_begin_text(InitiativeApp* app, InitiativeTextTarget target, const char* header, const char* initial) {
    if(!app->text_input) {
        app->text_input = text_input_alloc();
        if(!app->text_input) return;
        view_dispatcher_add_view(app->dispatcher, 1U, text_input_get_view(app->text_input));
    }
    app->text_target = target;
    initiative_copy(app->edit_buffer, sizeof(app->edit_buffer), initial);
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, header);
    text_input_set_result_callback(app->text_input, initiative_text_done, app, app->edit_buffer, sizeof(app->edit_buffer), false);
    view_dispatcher_switch_to_view(app->dispatcher, 1U);
}

static void initiative_number_done(void* context, int32_t number) {
    InitiativeApp* app = context;
    InitiativeMember* list = app->number_combat ? app->combat : app->roster;
    uint8_t count = app->number_combat ? app->combat_count : app->roster_count;
    if(app->input_member < count) {
        InitiativeMember* member = &list[app->input_member];
        switch(app->number_target) {
        case InitiativeNumberManualRoll:
            member->total = (int16_t)(initiative_clamp(number, 1, 20) + member->modifier);
            break;
        case InitiativeNumberTotal:
            member->total = initiative_clamp(number, -99, 199);
            break;
        case InitiativeNumberModifier:
            member->modifier = (int8_t)initiative_clamp(number, -50, 50);
            break;
        case InitiativeNumberArmorClass:
            member->armor_class = initiative_clamp(number, 0, 99);
            initiative_sync_main_if_needed(app, member);
            break;
        case InitiativeNumberHpCurrent:
            member->hp_current = initiative_clamp(number, -999, 999);
            initiative_sync_main_if_needed(app, member);
            break;
        case InitiativeNumberHpMax:
            member->hp_max = initiative_clamp(number, 0, 999);
            initiative_sync_main_if_needed(app, member);
            break;
        default:
            break;
        }
    }
    app->number_target = InitiativeNumberNone;
    initiative_save(app);
    view_dispatcher_switch_to_view(app->dispatcher, 0U);
    initiative_redraw(app);
}

static void initiative_begin_number(
    InitiativeApp* app,
    InitiativeNumberTarget target,
    bool combat,
    uint8_t member_index,
    const char* header,
    int32_t initial,
    int32_t minimum,
    int32_t maximum) {
    uint8_t count = combat ? app->combat_count : app->roster_count;
    if(member_index >= count) return;
    if(!app->number_input) {
        app->number_input = number_input_alloc();
        if(!app->number_input) return;
        view_dispatcher_add_view(app->dispatcher, 2U, number_input_get_view(app->number_input));
    }
    app->input_member = member_index;
    app->number_target = target;
    app->number_combat = combat ? 1U : 0U;
    number_input_set_header_text(app->number_input, header);
    number_input_set_result_callback(
        app->number_input, initiative_number_done, app, initial, minimum, maximum);
    view_dispatcher_switch_to_view(app->dispatcher, 2U);
}


static bool initiative_input(InputEvent* event, void* context) {
    InitiativeApp* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat && event->type != InputTypeLong) return true;
    if(event->type == InputTypeLong && event->key == InputKeyBack &&
       app->screen != InitiativeScreenNoCharacter) {
        app->screen = InitiativeScreenMenu;
        app->selection = app->scroll = 0U;
        initiative_redraw(app);
        return true;
    }
    if(app->screen == InitiativeScreenNoCharacter) {
        if((event->type == InputTypeShort || event->type == InputTypeRepeat) &&
           (event->key == InputKeyUp || event->key == InputKeyDown))
            app->selection ^= 1U;
        else if(event->type == InputTypeShort && event->key == InputKeyOk) {
            app->return_to_dnd = app->selection == 0U ? 1U : 0U;
            view_dispatcher_stop(app->dispatcher);
            return true;
        } else if(event->type == InputTypeShort && event->key == InputKeyBack) {
            app->return_to_dnd = 0U;
            view_dispatcher_stop(app->dispatcher);
            return true;
        }
    } else if(app->screen == InitiativeScreenMenu) {
        if((event->type == InputTypeShort || event->type == InputTypeRepeat) && event->key == InputKeyUp) initiative_move(&app->selection, 7U, -1);
        else if((event->type == InputTypeShort || event->type == InputTypeRepeat) && event->key == InputKeyDown) initiative_move(&app->selection, 7U, 1);
        else if((event->type == InputTypeShort || event->type == InputTypeRepeat) &&
                app->selection == 5U && (event->key == InputKeyLeft || event->key == InputKeyRight)) {
            int8_t delta = event->key == InputKeyRight ? 1 : -1;
            int8_t mode = (int8_t)app->roll_mode + delta;
            if(mode < (int8_t)InitiativeRollNormal) mode = (int8_t)InitiativeRollDisadvantage;
            if(mode > (int8_t)InitiativeRollDisadvantage) mode = (int8_t)InitiativeRollNormal;
            app->roll_mode = (uint8_t)mode;
            initiative_save(app);
        } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
            if(app->selection == 0U) initiative_seed_setup(app);
            else if(app->selection == 1U) { if(app->active) app->screen = InitiativeScreenCombat; }
            else if(app->selection == 2U) { app->screen = InitiativeScreenRoster; app->selection = app->scroll = 0U; }
            else if(app->selection == 3U) { if(app->combat_count) { app->screen = InitiativeScreenSetup; app->selection = app->scroll = 0U; } }
            else if(app->selection == 4U) {
                app->active = 0U;
                app->combat_count = 0U;
                app->current_turn = 0U;
                app->round = 1U;
                app->selection = app->scroll = 0U;
                initiative_copy(app->status, sizeof(app->status), "Combat ended");
                initiative_save(app);
            } else if(app->selection == 5U) {
                app->roll_mode = (uint8_t)((app->roll_mode + 1U) % 3U);
                initiative_save(app);
            } else { app->return_to_dnd = 1U; view_dispatcher_stop(app->dispatcher); return true; }
        }
    } else if(app->screen == InitiativeScreenRoster) {
        uint8_t total = (uint8_t)(app->roster_count + 1U);
        if((event->type == InputTypeShort || event->type == InputTypeRepeat) && event->key == InputKeyUp) initiative_move(&app->selection, total, -1);
        else if((event->type == InputTypeShort || event->type == InputTypeRepeat) && event->key == InputKeyDown) initiative_move(&app->selection, total, 1);
        else if(event->type == InputTypeShort && event->key == InputKeyBack) { app->screen = InitiativeScreenMenu; app->selection = app->scroll = 0U; }
        else if(event->type == InputTypeShort && event->key == InputKeyOk) {
            if(app->selection == app->roster_count && app->roster_count < INIT_MAX) {
                InitiativeMember* member = &app->roster[app->roster_count++]; memset(member, 0, sizeof(*member));
                initiative_copy(member->name, sizeof(member->name), "New"); member->hp_current = member->hp_max = 1; member->armor_class = 10; member->roll_mode = app->roll_mode;
                app->selection = (uint8_t)(app->roster_count - 1U);
            }
            if(app->selection < app->roster_count) { app->screen = InitiativeScreenEdit; app->edit_combat = 0U; app->edit_setup = 0U; app->edit_field = app->scroll = 0U; app->delete_armed = 0U; }
        }
    } else if(app->screen == InitiativeScreenSetup) {
        uint8_t total = (uint8_t)(app->combat_count + 3U);
        if((event->type == InputTypeShort || event->type == InputTypeRepeat) && event->key == InputKeyUp) initiative_move(&app->selection, total, -1);
        else if((event->type == InputTypeShort || event->type == InputTypeRepeat) && event->key == InputKeyDown) initiative_move(&app->selection, total, 1);
        else if((event->type == InputTypeShort || event->type == InputTypeRepeat) && app->selection > 0U && app->selection <= app->combat_count && (event->key == InputKeyLeft || event->key == InputKeyRight)) {
            InitiativeMember* member = &app->combat[app->selection - 1U]; member->total = initiative_clamp(member->total + (event->key == InputKeyRight ? 1 : -1), -99, 199); initiative_save(app);
        } else if(event->type == InputTypeLong && app->selection > 0U && app->selection <= app->combat_count && (event->key == InputKeyLeft || event->key == InputKeyRight)) {
            uint8_t index = (uint8_t)(app->selection - 1U);
            if(event->key == InputKeyLeft && index > 0U) { initiative_swap(app, index, (uint8_t)(index - 1U)); --app->selection; initiative_save(app); }
            else if(event->key == InputKeyRight && index + 1U < app->combat_count) { initiative_swap(app, index, (uint8_t)(index + 1U)); ++app->selection; initiative_save(app); }
        } else if(event->type == InputTypeLong && event->key == InputKeyOk && app->selection > 0U && app->selection <= app->combat_count) {
            app->selection = (uint8_t)(app->selection - 1U);
            app->edit_combat = 1U;
            app->edit_setup = 1U;
            app->edit_field = app->scroll = 0U;
            app->delete_armed = 0U;
            app->screen = InitiativeScreenEdit;
        }
        else if(event->type == InputTypeShort && event->key == InputKeyBack) { app->screen = InitiativeScreenMenu; app->selection = app->scroll = 0U; }
        else if(event->type == InputTypeShort && event->key == InputKeyOk) {
            if(app->selection == 0U) { for(uint8_t i=0;i<app->combat_count;i++) app->combat[i].total=(int16_t)(initiative_roll_d20((InitiativeRollMode)app->combat[i].roll_mode)+app->combat[i].modifier); initiative_save(app); }
            else if(app->selection <= app->combat_count) { InitiativeMember* member=&app->combat[app->selection-1U]; member->total=(int16_t)(initiative_roll_d20((InitiativeRollMode)member->roll_mode)+member->modifier); initiative_save(app); }
            else if(app->selection == app->combat_count + 1U && app->combat_count < INIT_MAX) { InitiativeMember* member=&app->combat[app->combat_count++]; memset(member,0,sizeof(*member)); initiative_copy(member->name,sizeof(member->name),"Temp"); member->hp_current=member->hp_max=1; member->armor_class=10; member->roll_mode=app->roll_mode; app->input_member=(uint8_t)(app->combat_count-1U); app->edit_combat=1U; initiative_begin_text(app,InitiativeTextName,"Participant name",member->name); }
            else initiative_start(app);
        }
    } else if(app->screen == InitiativeScreenCombat) {
        if((event->type == InputTypeShort || event->type == InputTypeRepeat) && event->key == InputKeyUp) initiative_move(&app->selection, app->combat_count, -1);
        else if((event->type == InputTypeShort || event->type == InputTypeRepeat) && event->key == InputKeyDown) initiative_move(&app->selection, app->combat_count, 1);
        else if(event->type == InputTypeShort && event->key == InputKeyBack && app->combat_count) { if(app->current_turn) --app->current_turn; else if(app->round>1U){--app->round;app->current_turn=(uint8_t)(app->combat_count-1U);} app->selection=app->current_turn; initiative_save(app); }
        else if((event->type == InputTypeShort || event->type == InputTypeRepeat) && (event->key == InputKeyLeft || event->key == InputKeyRight) && app->combat_count) { InitiativeMember* m=&app->combat[app->selection]; m->hp_current=initiative_clamp(m->hp_current+(event->key==InputKeyRight?1:-1),-999,999); initiative_sync_main_if_needed(app,m); initiative_save(app); }
        else if(event->type == InputTypeLong && event->key == InputKeyOk && app->combat_count) { app->edit_combat=1U; app->edit_setup=0U; app->edit_field=app->scroll=0U; app->delete_armed=0U; app->screen=InitiativeScreenEdit; }
        else if(event->type == InputTypeLong && event->key == InputKeyUp && app->combat_count) { InitiativeMember* m=&app->combat[app->selection]; m->armor_class=initiative_clamp(m->armor_class+1,0,99); initiative_sync_main_if_needed(app,m); initiative_save(app); }
        else if(event->type == InputTypeLong && event->key == InputKeyDown && app->combat_count) { app->edit_combat=1U; app->edit_setup=0U; app->input_member=app->selection; initiative_begin_text(app,InitiativeTextConditions,"Participant conditions",app->combat[app->selection].conditions); }
        else if(event->type == InputTypeLong && event->key == InputKeyLeft && app->combat_count && app->selection>0U) { initiative_swap(app,app->selection,(uint8_t)(app->selection-1U)); --app->selection; initiative_save(app); }
        else if(event->type == InputTypeLong && event->key == InputKeyRight && app->combat_count && app->selection+1U<app->combat_count) { initiative_swap(app,app->selection,(uint8_t)(app->selection+1U)); ++app->selection; initiative_save(app); }
        else if(event->type == InputTypeShort && event->key == InputKeyOk && app->combat_count) { initiative_patch_character(app,NULL,1U); ++app->current_turn; if(app->current_turn>=app->combat_count){app->current_turn=0U;++app->round;if(!app->round)app->round=1U;} app->selection=app->current_turn; initiative_save(app); }
    } else {
        InitiativeMember* member = app->edit_combat ? &app->combat[app->selection] : &app->roster[app->selection];
        uint8_t edit_count = app->edit_combat ? 9U : 8U;
        if((event->type == InputTypeShort || event->type == InputTypeRepeat) && event->key == InputKeyUp) {
            app->delete_armed = 0U;
            initiative_move(&app->edit_field, edit_count, -1);
        } else if((event->type == InputTypeShort || event->type == InputTypeRepeat) && event->key == InputKeyDown) {
            app->delete_armed = 0U;
            initiative_move(&app->edit_field, edit_count, 1);
        } else if(event->type == InputTypeShort && event->key == InputKeyBack) {
            if(app->edit_setup) app->screen=InitiativeScreenSetup;
            else app->screen=app->edit_combat?InitiativeScreenCombat:InitiativeScreenRoster;
            app->selection=app->scroll=0U;
        } else if((event->type == InputTypeShort || event->type == InputTypeRepeat) && (event->key==InputKeyLeft||event->key==InputKeyRight)) {
            int16_t d=event->key==InputKeyRight?1:-1;
            if(app->edit_combat) {
                if(app->edit_field==1U) member->total=initiative_clamp(member->total+d,-99,199);
                else if(app->edit_field==2U) member->modifier=(int8_t)initiative_clamp(member->modifier+d,-50,50);
                else if(app->edit_field==3U) {
                    int8_t mode=(int8_t)member->roll_mode+d;
                    if(mode<(int8_t)InitiativeRollNormal) mode=(int8_t)InitiativeRollDisadvantage;
                    if(mode>(int8_t)InitiativeRollDisadvantage) mode=(int8_t)InitiativeRollNormal;
                    member->roll_mode=(uint8_t)mode;
                } else if(app->edit_field==4U) { member->armor_class=initiative_clamp(member->armor_class+d,0,99); initiative_sync_main_if_needed(app,member); }
                else if(app->edit_field==5U) { member->hp_current=initiative_clamp(member->hp_current+d,-999,999); initiative_sync_main_if_needed(app,member); }
                else if(app->edit_field==6U) { member->hp_max=initiative_clamp(member->hp_max+d,0,999); initiative_sync_main_if_needed(app,member); }
            } else {
                if(app->edit_field==1U) member->modifier=(int8_t)initiative_clamp(member->modifier+d,-50,50);
                else if(app->edit_field==2U) {
                    int8_t mode=(int8_t)member->roll_mode+d;
                    if(mode<(int8_t)InitiativeRollNormal) mode=(int8_t)InitiativeRollDisadvantage;
                    if(mode>(int8_t)InitiativeRollDisadvantage) mode=(int8_t)InitiativeRollNormal;
                    member->roll_mode=(uint8_t)mode;
                } else if(app->edit_field==3U) { member->armor_class=initiative_clamp(member->armor_class+d,0,99); initiative_sync_main_if_needed(app,member); }
                else if(app->edit_field==4U) { member->hp_current=initiative_clamp(member->hp_current+d,-999,999); initiative_sync_main_if_needed(app,member); }
                else if(app->edit_field==5U) { member->hp_max=initiative_clamp(member->hp_max+d,0,999); initiative_sync_main_if_needed(app,member); }
            }
            initiative_save(app);
        } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
            app->input_member=app->selection;
            if(app->edit_field==0U) {
                initiative_begin_text(app,InitiativeTextName,"Participant name",member->name);
            } else if(app->edit_combat) {
                if(app->edit_field==1U) initiative_begin_number(app,InitiativeNumberTotal,true,app->selection,"Initiative total",member->total,-99,199);
                else if(app->edit_field==2U) initiative_begin_number(app,InitiativeNumberModifier,true,app->selection,"Initiative modifier",member->modifier,-50,50);
                else if(app->edit_field==3U) { member->roll_mode=(uint8_t)((member->roll_mode+1U)%3U); initiative_save(app); }
                else if(app->edit_field==4U) initiative_begin_number(app,InitiativeNumberArmorClass,true,app->selection,"Armor Class",member->armor_class,0,99);
                else if(app->edit_field==5U) initiative_begin_number(app,InitiativeNumberHpCurrent,true,app->selection,"Current HP",member->hp_current,-999,999);
                else if(app->edit_field==6U) initiative_begin_number(app,InitiativeNumberHpMax,true,app->selection,"Maximum HP",member->hp_max,0,999);
                else if(app->edit_field==7U) initiative_begin_text(app,InitiativeTextConditions,"Conditions",member->conditions);
                else if(app->delete_armed) {
                    memmove(&app->combat[app->selection],&app->combat[app->selection+1U],(app->combat_count-app->selection-1U)*sizeof(*app->combat));
                    --app->combat_count;
                    if(!app->combat_count) { app->active=0U; app->current_turn=0U; }
                    else if(app->current_turn>=app->combat_count) app->current_turn=0U;
                    app->screen=app->edit_setup?InitiativeScreenSetup:InitiativeScreenCombat; app->selection=app->scroll=0U; initiative_save(app);
                } else app->delete_armed=1U;
            } else {
                if(app->edit_field==1U) initiative_begin_number(app,InitiativeNumberModifier,false,app->selection,"Initiative modifier",member->modifier,-50,50);
                else if(app->edit_field==2U) { member->roll_mode=(uint8_t)((member->roll_mode+1U)%3U); initiative_save(app); }
                else if(app->edit_field==3U) initiative_begin_number(app,InitiativeNumberArmorClass,false,app->selection,"Armor Class",member->armor_class,0,99);
                else if(app->edit_field==4U) initiative_begin_number(app,InitiativeNumberHpCurrent,false,app->selection,"Current HP",member->hp_current,-999,999);
                else if(app->edit_field==5U) initiative_begin_number(app,InitiativeNumberHpMax,false,app->selection,"Maximum HP",member->hp_max,0,999);
                else if(app->edit_field==6U) initiative_begin_text(app,InitiativeTextConditions,"Conditions",member->conditions);
                else if(app->delete_armed) {
                    memmove(&app->roster[app->selection],&app->roster[app->selection+1U],(app->roster_count-app->selection-1U)*sizeof(*app->roster));
                    --app->roster_count;
                    app->screen=InitiativeScreenRoster; app->selection=app->scroll=0U; initiative_save(app);
                } else app->delete_armed=1U;
            }
        }
    }
    if(app->screen == InitiativeScreenEdit) { if(app->edit_field < app->scroll) app->scroll=app->edit_field; if(app->edit_field>=app->scroll+5U) app->scroll=(uint8_t)(app->edit_field-4U); }
    else { if(app->selection < app->scroll) app->scroll=app->selection; if(app->selection>=app->scroll+5U) app->scroll=(uint8_t)(app->selection-4U); }
    initiative_redraw(app);
    return true;
}

static bool initiative_navigation(void* context) {
    InitiativeApp* app = context;
    if(app->screen == InitiativeScreenNoCharacter) {
        app->return_to_dnd = 0U;
        view_dispatcher_stop(app->dispatcher);
    } else if(app->screen == InitiativeScreenMenu) {
        app->return_to_dnd = 1U;
        view_dispatcher_stop(app->dispatcher);
    } else {
        app->screen = InitiativeScreenMenu;
        app->selection = app->scroll = 0U;
        initiative_redraw(app);
    }
    return true;
}

static InitiativeApp* initiative_alloc(const char* args) {
    InitiativeApp* app = calloc(1U, sizeof(InitiativeApp));
    if(!app) return NULL;
    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    if(!app->gui || !app->storage) goto fail;
    const char* end = NULL;
    bool explicit_profile = args && initiative_parse_u32(args, &end, &app->character_id);
    char profile_path[INIT_PATH_LEN];
    if(explicit_profile) {
        app->have_character =
            dnd_profile_ref_path(app->storage, app->character_id, profile_path, sizeof(profile_path)) ? 1U : 0U;
        if(!app->have_character)
            app->have_character =
                dnd_profile_ref_active(app->storage, &app->character_id) ? 1U : 0U;
    } else {
        app->have_character =
            dnd_profile_ref_active(app->storage, &app->character_id) ? 1U : 0U;
    }
    if(app->have_character) {
        initiative_load(app);
        if(args) initiative_import_args(app, args);
        bool refreshed_main = initiative_refresh_main_character(app);
        if(refreshed_main || (args && strchr(args, ';'))) initiative_save(app);
        app->screen = InitiativeScreenMenu;
    } else {
        app->selection = app->scroll = 0U;
        app->screen = InitiativeScreenNoCharacter;
    }
    app->dispatcher = view_dispatcher_alloc();
    app->view = view_alloc();
    if(!app->dispatcher || !app->view) goto fail;
    view_dispatcher_set_event_callback_context(app->dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->dispatcher, initiative_navigation);
    view_allocate_model(app->view, ViewModelTypeLockFree, sizeof(InitiativeApp*));
    InitiativeApp** model = view_get_model(app->view);
    if(!model) goto fail;
    *model = app;
    view_commit_model(app->view, false);
    view_set_context(app->view, app);
    view_set_draw_callback(app->view, initiative_draw);
    view_set_input_callback(app->view, initiative_input);
    view_dispatcher_add_view(app->dispatcher, 0U, app->view);
    view_dispatcher_attach_to_gui(app->dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    return app;
fail:
    if(app->text_input) text_input_free(app->text_input);
    if(app->number_input) number_input_free(app->number_input);
    if(app->view) view_free(app->view);
    if(app->dispatcher) view_dispatcher_free(app->dispatcher);
    if(app->storage) furi_record_close(RECORD_STORAGE);
    if(app->gui) furi_record_close(RECORD_GUI);
    free(app);
    return NULL;
}

static void initiative_free(InitiativeApp* app) {
    if(!app) return;
    if(app->dispatcher && app->text_input) view_dispatcher_remove_view(app->dispatcher, 1U);
    if(app->dispatcher && app->number_input) view_dispatcher_remove_view(app->dispatcher, 2U);
    if(app->dispatcher && app->view) view_dispatcher_remove_view(app->dispatcher, 0U);
    if(app->text_input) text_input_free(app->text_input);
    if(app->number_input) number_input_free(app->number_input);
    if(app->view) view_free(app->view);
    if(app->dispatcher) view_dispatcher_free(app->dispatcher);
    if(app->storage) furi_record_close(RECORD_STORAGE);
    if(app->gui) furi_record_close(RECORD_GUI);
    free(app);
}

int32_t dndinitiative_app(void* context) {
    InitiativeApp* app = initiative_alloc(context);
    if(!app) return -1;
    view_dispatcher_switch_to_view(app->dispatcher, 0U);
    view_dispatcher_run(app->dispatcher);
    bool return_to_dnd = app->return_to_dnd;
    initiative_free(app);
    if(return_to_dnd) {
        if(!dnd_handoff_launch(DNDOLPHINS_FAP_PATH, NULL)) return -1;
    }
    return 0;
}
