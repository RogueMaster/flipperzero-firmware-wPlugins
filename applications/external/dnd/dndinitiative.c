#include "dnd_fs.h"
#include "dnd_profile_handoff.h"
#include "dndinitiative_feature_recharge.h"

#include <furi.h>
#include <furi_hal.h>
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

#define INIT_MAX           24U
#define INIT_NAME_LEN      32U
#define INIT_CONDITION_LEN 64U
#define INIT_PATH_LEN      128U
#define INIT_FILE_PATH     APP_DATA_PATH("ch_%lu.%s")
#define INIT_HISTORY_ROOT  APP_DATA_PATH("history")

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
    InitiativeScreenEndCombat,
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

static void dndinitiative_focus_combat_member(InitiativeApp* app, uint8_t index);

static void dndinitiative_copy(char* out, size_t size, const char* in) {
    if(!size) return;
    strncpy(out, in ? in : "", size - 1U);
    out[size - 1U] = '\0';
}

static void dndinitiative_redraw(InitiativeApp* app) {
    if(!app || !app->view) return;
    InitiativeApp** model = view_get_model(app->view);
    if(!model) return;
    *model = app;
    view_commit_model(app->view, true);
}

static bool dndinitiative_parse_u32(const char* text, const char** end, uint32_t* output) {
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

static int16_t dndinitiative_clamp(int32_t value, int16_t low, int16_t high) {
    if(value < low) return low;
    if(value > high) return high;
    return (int16_t)value;
}

static int32_t dndinitiative_parse_i32(const char* text) {
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

static bool dndinitiative_path(char* out, size_t size, uint32_t id, const char* suffix) {
    int n = snprintf(out, size, INIT_FILE_PATH, (unsigned long)id, suffix);
    return n > 0 && (size_t)n < size;
}

static int8_t dndinitiative_ability_modifier(int32_t score) {
    int32_t delta = score - 10;
    if(delta >= 0) return (int8_t)(delta / 2);
    return (int8_t)(-(((-delta) + 1) / 2));
}

static uint8_t dndinitiative_roll_d20(InitiativeRollMode mode) {
    uint8_t first = (uint8_t)(1U + (furi_hal_random_get() % 20U));
    if(mode == InitiativeRollNormal) return first;
    uint8_t second = (uint8_t)(1U + (furi_hal_random_get() % 20U));
    return mode == InitiativeRollAdvantage ? (first > second ? first : second) :
                                             (first < second ? first : second);
}

static const char* dndinitiative_roll_mode_name(InitiativeRollMode mode) {
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

static bool dndinitiative_read_line_checked(
    InitiativeReader* reader,
    char* line,
    size_t size,
    bool* overflow) {
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

static bool dndinitiative_read_line(InitiativeReader* reader, char* line, size_t size) {
    return dndinitiative_read_line_checked(reader, line, size, NULL);
}

static bool dndinitiative_parse_i32_strict(const char* text, int32_t* output) {
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

static bool dndinitiative_indexed_key(
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

static bool dndinitiative_write_named(File* file, const char* key, const char* value) {
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

static bool dndinitiative_write_number(File* file, const char* key, int32_t value) {
    char line[64];
    int length = snprintf(line, sizeof(line), "%s=%ld\n", key, (long)value);
    return length > 0 && (size_t)length < sizeof(line) &&
           storage_file_write(file, line, (size_t)length) == (size_t)length;
}

static bool dndinitiative_write_member_named(
    File* file,
    const char* prefix,
    uint8_t index,
    const InitiativeMember* member) {
    char key[40];
#define INIT_MEMBER_STRING(suffix, field)                              \
    do {                                                               \
        snprintf(key, sizeof(key), "%s%u%s", prefix, index, suffix);   \
        if(!dndinitiative_write_named(file, key, field)) return false; \
    } while(false)
#define INIT_MEMBER_NUMBER(suffix, field)                               \
    do {                                                                \
        snprintf(key, sizeof(key), "%s%u%s", prefix, index, suffix);    \
        if(!dndinitiative_write_number(file, key, field)) return false; \
    } while(false)
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

static bool dndinitiative_save(InitiativeApp* app) {
    storage_common_mkdir(app->storage, APP_DATA_PATH(""));
    char path[INIT_PATH_LEN], temp[INIT_PATH_LEN], backup[INIT_PATH_LEN];
    if(!dndinitiative_path(path, sizeof(path), app->character_id, "txt") ||
       !dndinitiative_path(temp, sizeof(temp), app->character_id, "tmp") ||
       !dndinitiative_path(backup, sizeof(backup), app->character_id, "bak"))
        return false;
    File* file = storage_file_alloc(app->storage);
    if(!file) return false;
    bool ok = storage_file_open(file, temp, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
              dndinitiative_write_number(file, "DNDInitiative", 1) &&
              dndinitiative_write_number(file, "CharacterId", (int32_t)app->character_id) &&
              dndinitiative_write_number(file, "RollMode", app->roll_mode) &&
              dndinitiative_write_named(file, "MainCharacterName", app->main_character_name) &&
              dndinitiative_write_number(file, "RosterCount", app->roster_count);
    for(uint8_t i = 0U; ok && i < app->roster_count; ++i)
        ok = dndinitiative_write_member_named(file, "Roster", i, &app->roster[i]);
    if(ok) ok = dndinitiative_write_number(file, "Active", app->active);
    if(ok) ok = dndinitiative_write_number(file, "Round", app->round);
    if(ok) ok = dndinitiative_write_number(file, "CurrentTurn", app->current_turn);
    if(ok) ok = dndinitiative_write_number(file, "CombatCount", app->combat_count);
    for(uint8_t i = 0U; ok && i < app->combat_count; ++i)
        ok = dndinitiative_write_member_named(file, "Combat", i, &app->combat[i]);
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

static bool dndinitiative_apply_member_field(
    const char* key,
    const char* value,
    const char* prefix,
    InitiativeMember* list,
    uint8_t* count) {
    uint8_t index = 0U;
    InitiativeMember* member = NULL;
    int32_t number = 0;
    bool applied = false;

    if(dndinitiative_indexed_key(key, prefix, "Name", &index)) {
        member = &list[index];
        dndinitiative_copy(member->name, sizeof(member->name), value);
        applied = true;
    } else if(dndinitiative_indexed_key(key, prefix, "HpCurrent", &index)) {
        member = &list[index];
        if(dndinitiative_parse_i32_strict(value, &number)) {
            member->hp_current = dndinitiative_clamp(number, -999, 999);
            applied = true;
        }
    } else if(dndinitiative_indexed_key(key, prefix, "HpMax", &index)) {
        member = &list[index];
        if(dndinitiative_parse_i32_strict(value, &number)) {
            member->hp_max = dndinitiative_clamp(number, 0, 999);
            applied = true;
        }
    } else if(dndinitiative_indexed_key(key, prefix, "ArmorClass", &index)) {
        member = &list[index];
        if(dndinitiative_parse_i32_strict(value, &number)) {
            member->armor_class = dndinitiative_clamp(number, 0, 99);
            applied = true;
        }
    } else if(dndinitiative_indexed_key(key, prefix, "Modifier", &index)) {
        member = &list[index];
        if(dndinitiative_parse_i32_strict(value, &number)) {
            member->modifier = (int8_t)dndinitiative_clamp(number, -50, 50);
            applied = true;
        }
    } else if(dndinitiative_indexed_key(key, prefix, "RollMode", &index)) {
        member = &list[index];
        if(dndinitiative_parse_i32_strict(value, &number)) {
            member->roll_mode = (uint8_t)dndinitiative_clamp(
                number, InitiativeRollNormal, InitiativeRollDisadvantage);
            applied = true;
        }
    } else if(dndinitiative_indexed_key(key, prefix, "Total", &index)) {
        member = &list[index];
        if(dndinitiative_parse_i32_strict(value, &number)) {
            member->total = dndinitiative_clamp(number, -99, 199);
            applied = true;
        }
    } else if(dndinitiative_indexed_key(key, prefix, "Conditions", &index)) {
        member = &list[index];
        dndinitiative_copy(member->conditions, sizeof(member->conditions), value);
        applied = true;
    } else {
        return false;
    }

    if(applied && *count <= index) *count = (uint8_t)(index + 1U);
    return applied;
}

static void dndinitiative_load(InitiativeApp* app) {
    char path[INIT_PATH_LEN];
    if(!dndinitiative_path(path, sizeof(path), app->character_id, "txt")) return;
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
    while(dndinitiative_read_line(&reader, line, sizeof(line))) {
        char* value = strchr(line, '=');
        if(!value) continue;
        *value++ = '\0';
        int32_t number = 0;

        if(!strcmp(line, "MainCharacterName")) {
            dndinitiative_copy(app->main_character_name, sizeof(app->main_character_name), value);
        } else if(!strcmp(line, "RollMode")) {
            if(dndinitiative_parse_i32_strict(value, &number) && number >= 0 &&
               number <= (int32_t)InitiativeRollDisadvantage)
                app->roll_mode = (uint8_t)number;
        } else if(!strcmp(line, "Active")) {
            if(dndinitiative_parse_i32_strict(value, &number)) app->active = number ? 1U : 0U;
        } else if(!strcmp(line, "Round")) {
            if(dndinitiative_parse_i32_strict(value, &number) && number >= 1 &&
               number <= (int32_t)UINT16_MAX)
                app->round = (uint16_t)number;
        } else if(!strcmp(line, "CurrentTurn")) {
            if(dndinitiative_parse_i32_strict(value, &number) && number >= 0 &&
               number < (int32_t)INIT_MAX)
                app->current_turn = (uint8_t)number;
        } else if(dndinitiative_apply_member_field(
                      line, value, "Roster", app->roster, &app->roster_count)) {
            /* Applied by field name. */
        } else if(dndinitiative_apply_member_field(
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

static bool dndinitiative_refresh_main_character(InitiativeApp* app) {
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
    while(dndinitiative_read_line(&reader, line, sizeof(line))) {
        char* value = strchr(line, '=');
        if(!value) continue;
        *value++ = '\0';
        if(!strcmp(line, "Name")) {
            dndinitiative_copy(name, sizeof(name), value);
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
                if(!dndinitiative_parse_i32_strict(cursor, &values[count])) break;
                ++count;
                if(!comma) break;
                cursor = comma + 1U;
            }
            if(count >= 2U) {
                dexterity = values[1];
                have_abilities = true;
            }
        } else if(!strncmp(line, "Class", 5U) && strstr(line, "Data")) {
            int32_t class_level = 0;
            char* comma = strchr(value, ',');
            if(comma) *comma = '\0';
            if(dndinitiative_parse_i32_strict(value, &class_level) && class_level > 0) {
                uint16_t next = (uint16_t)total_level + (uint16_t)class_level;
                total_level = (uint8_t)(next > 20U ? 20U : next);
            }
        } else if(!strncmp(line, "Feature", 7U) && strstr(line, "Name")) {
            if(!strcmp(value, "Alert"))
                has_alert = true;
            else if(!strcmp(value, "Jack of All Trades"))
                has_jack_of_all_trades = true;
        } else if(!strcmp(line, "Vitals")) {
            int32_t values[12] = {0};
            uint8_t count = 0U;
            char* cursor = value;
            while(count < 12U && *cursor) {
                char* comma = strchr(cursor, ',');
                if(comma) *comma = '\0';
                if(!dndinitiative_parse_i32_strict(cursor, &values[count])) break;
                ++count;
                if(!comma) break;
                cursor = comma + 1U;
            }
            if(count >= 7U) {
                hp_current = values[0];
                hp_max = values[1];
                armor_class = values[3];
                initiative_misc = values[5];
                exhaustion = values[6];
                have_vitals = true;
            }
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    if(!have_name || !have_abilities || !have_vitals) return false;

    uint8_t proficiency = total_level ? (uint8_t)(2U + (total_level - 1U) / 4U) : 2U;
    int32_t feature_bonus = has_alert ? proficiency :
                                        (has_jack_of_all_trades ? (int32_t)(proficiency / 2U) : 0);
    int16_t modifier = dndinitiative_clamp(
        (int32_t)dndinitiative_ability_modifier(dexterity) + initiative_misc + feature_bonus -
            (2 * exhaustion),
        -50,
        50);
    InitiativeMember* member = NULL;
    bool changed = false;
    if(app->main_character_name[0]) {
        for(uint8_t i = 0U; i < app->roster_count; ++i) {
            if(!strcmp(app->roster[i].name, app->main_character_name)) {
                member = &app->roster[i];
                break;
            }
        }
    }
    if(!member) {
        for(uint8_t i = 0U; i < app->roster_count; ++i) {
            if(!strcmp(app->roster[i].name, name)) {
                member = &app->roster[i];
                break;
            }
        }
    }
    if(!member && app->roster_count < INIT_MAX) {
        member = &app->roster[app->roster_count++];
        memset(member, 0, sizeof(*member));
        dndinitiative_copy(member->name, sizeof(member->name), name);
        member->roll_mode = app->roll_mode;
        changed = true;
    }
    if(!member) return false;
    char prior_main_name[INIT_NAME_LEN];
    dndinitiative_copy(prior_main_name, sizeof(prior_main_name), app->main_character_name);
    int16_t next_hp_current = dndinitiative_clamp(hp_current, -999, 999);
    int16_t next_hp_max = dndinitiative_clamp(hp_max, 0, 999);
    int16_t next_ac = dndinitiative_clamp(armor_class, 0, 99);
    if(strcmp(member->name, name) || strcmp(app->main_character_name, name) ||
       member->hp_current != next_hp_current || member->hp_max != next_hp_max ||
       member->armor_class != next_ac || member->modifier != (int8_t)modifier)
        changed = true;
    dndinitiative_copy(member->name, sizeof(member->name), name);
    dndinitiative_copy(app->main_character_name, sizeof(app->main_character_name), name);
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
            dndinitiative_copy(app->combat[i].name, sizeof(app->combat[i].name), name);
            app->combat[i].modifier = (int8_t)modifier;
            app->combat[i].hp_current = member->hp_current;
            app->combat[i].hp_max = member->hp_max;
            app->combat[i].armor_class = member->armor_class;
            break;
        }
    }
    return changed;
}

static bool
    dndinitiative_member_is_main(const InitiativeApp* app, const InitiativeMember* member) {
    return app && member && app->main_character_name[0] &&
           !strcmp(app->main_character_name, member->name);
}

static uint8_t dndinitiative_parse_csv(char* text, int32_t* values, uint8_t capacity) {
    if(!text || !values || !capacity) return 0U;
    uint8_t count = 0U;
    char* cursor = text;
    while(count < capacity && *cursor) {
        char* comma = strchr(cursor, ',');
        if(comma) *comma = '\0';
        if(!dndinitiative_parse_i32_strict(cursor, &values[count])) return count;
        ++count;
        if(!comma) break;
        cursor = comma + 1U;
    }
    return count;
}

static bool dndinitiative_write_line(File* file, const char* line) {
    if(!file || !line) return false;
    size_t length = strlen(line);
    return storage_file_write(file, line, length) == length &&
           storage_file_write(file, "\n", 1U) == 1U;
}

/* Patch only the canonical fields Initiative owns. Unknown/malformed character fields
   are copied unchanged so a partial or future save remains best-effort readable. */
static bool dndinitiative_patch_character(
    InitiativeApp* app,
    const InitiativeMember* sync_member,
    uint8_t recharge_cadence) {
    if(!app || !app->storage || !app->have_character) return false;
    if(!sync_member && recharge_cadence) {
        DndFeatureFastRechargeEvent event =
            recharge_cadence == 1U ? DndFeatureFastRechargeTurn : DndFeatureFastRechargeEncounter;
        return dndinitiative_feature_recharge_fast_recharge(
            app->storage, app->character_id, event);
    }
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
        while(ok && dndinitiative_read_line_checked(&reader, line, 768U, &overflow)) {
            if(overflow) {
                ok = false;
                break;
            }
            char replacement[192];
            const char* out_line = line;
            dndinitiative_copy(parse, 768U, line);
            char* value = strchr(parse, '=');
            if(value) {
                *value++ = '\0';
                if(sync_member && !strcmp(parse, "Vitals")) {
                    int32_t values[12] = {0};
                    if(dndinitiative_parse_csv(value, values, 12U) == 12U) {
                        values[0] = sync_member->hp_current;
                        values[1] = sync_member->hp_max;
                        values[3] = sync_member->armor_class;
                        int n = snprintf(
                            replacement,
                            sizeof(replacement),
                            "Vitals=%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld",
                            (long)values[0],
                            (long)values[1],
                            (long)values[2],
                            (long)values[3],
                            (long)values[4],
                            (long)values[5],
                            (long)values[6],
                            (long)values[7],
                            (long)values[8],
                            (long)values[9],
                            (long)values[10],
                            (long)values[11]);
                        if(n > 0 && (size_t)n < sizeof(replacement)) {
                            out_line = replacement;
                            touched = true;
                        }
                    }
                }
            }
            ok = dndinitiative_write_line(output, out_line);
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

static void dndinitiative_sync_main_if_needed(InitiativeApp* app, InitiativeMember* member) {
    if(dndinitiative_member_is_main(app, member)) dndinitiative_patch_character(app, member, 0U);
}

static void dndinitiative_swap(InitiativeApp* app, uint8_t first, uint8_t second) {
    if(!app || first >= app->combat_count || second >= app->combat_count || first == second)
        return;
    InitiativeMember temporary = app->combat[first];
    app->combat[first] = app->combat[second];
    app->combat[second] = temporary;
    if(app->current_turn == first)
        app->current_turn = second;
    else if(app->current_turn == second)
        app->current_turn = first;
}

static void dndinitiative_import_args(InitiativeApp* app, const char* args) {
    const char* cursor = args;
    const char* end = NULL;
    uint32_t transport_profile = 0U;
    if(!dndinitiative_parse_u32(cursor, &end, &transport_profile)) return;
    (void)transport_profile;
    /* The leading ID remains in Bestiary's transfer format for compatibility,
       but persisted Active= is authoritative for Initiative profile selection. */
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
            dndinitiative_copy(member->name, sizeof(member->name), record);
            member->hp_current = member->hp_max =
                dndinitiative_clamp(dndinitiative_parse_i32(a), 0, 999);
            member->armor_class = dndinitiative_clamp(dndinitiative_parse_i32(b), 0, 99);
            member->modifier = (int8_t)dndinitiative_clamp(dndinitiative_parse_i32(c), -50, 50);
            if(member->name[0]) ++app->roster_count;
        }
        cursor = record_end;
    }
}

static bool
    dndinitiative_member_is_party(const InitiativeApp* app, const InitiativeMember* member) {
    if(!app || !member || !member->name[0]) return false;
    if(app->main_character_name[0] && !strcmp(member->name, app->main_character_name)) return true;
    for(uint8_t i = 0U; i < app->roster_count; ++i) {
        if(app->roster[i].name[0] && !strcmp(member->name, app->roster[i].name)) return true;
    }
    return false;
}

static void dndinitiative_history_field(char* out, size_t size, const char* in) {
    if(!out || !size) return;
    size_t used = 0U;
    if(in) {
        while(*in && used + 1U < size) {
            char ch = *in++;
            out[used++] = (ch == '|' || ch == '\r' || ch == '\n') ? ' ' : ch;
        }
    }
    out[used] = '\0';
}

static bool dndinitiative_history_write_raw(File* file, const char* text) {
    if(!file || !text) return false;
    size_t length = strlen(text);
    return storage_file_write(file, text, length) == length;
}

static bool
    dndinitiative_history_write_member(File* file, char kind, const InitiativeMember* member) {
    if(!file || !member) return false;
    char name[INIT_NAME_LEN];
    char conditions[INIT_CONDITION_LEN];
    dndinitiative_history_field(name, sizeof(name), member->name);
    dndinitiative_history_field(conditions, sizeof(conditions), member->conditions);
    char line[192];
    int length = snprintf(
        line,
        sizeof(line),
        "%c|%s|%d|%d|%d|%s\n",
        kind,
        name,
        member->hp_current,
        member->hp_max,
        member->armor_class,
        conditions);
    return length > 0 && (size_t)length < sizeof(line) &&
           storage_file_write(file, line, (size_t)length) == (size_t)length;
}

static bool dndinitiative_save_completed_history(InitiativeApp* app) {
    if(!app || !app->storage || !app->combat_count || app->character_id == UINT32_MAX)
        return false;
    if(!dnd_fs_ensure_directory(app->storage, INIT_HISTORY_ROOT)) return false;
    DateTime now;
    furi_hal_rtc_get_datetime(&now);
    char final_path[INIT_PATH_LEN];
    char temp_path[INIT_PATH_LEN];
    char filename[64];
    bool path_ready = false;
    for(uint8_t suffix = 0U; suffix < 100U; ++suffix) {
        int length = snprintf(
            filename,
            sizeof(filename),
            "ch_%lu_%04u%02u%02u_%02u%02u%02u_%02u.txt",
            (unsigned long)app->character_id,
            (unsigned)now.year,
            (unsigned)now.month,
            (unsigned)now.day,
            (unsigned)now.hour,
            (unsigned)now.minute,
            (unsigned)now.second,
            (unsigned)suffix);
        if(length <= 0 || (size_t)length >= sizeof(filename) ||
           !dnd_fs_child_path(final_path, sizeof(final_path), INIT_HISTORY_ROOT, NULL, filename))
            return false;
        if(!storage_file_exists(app->storage, final_path)) {
            path_ready = true;
            break;
        }
    }
    if(!path_ready) return false;
    int temp_length = snprintf(temp_path, sizeof(temp_path), "%s.tmp", final_path);
    if(temp_length <= 0 || (size_t)temp_length >= sizeof(temp_path)) return false;
    storage_common_remove(app->storage, temp_path);
    File* file = storage_file_alloc(app->storage);
    if(!file) return false;
    bool ok = storage_file_open(file, temp_path, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    char header[192];
    if(ok) ok = dndinitiative_history_write_raw(file, "DNDInitiativeHistory=1\n");
    if(ok) {
        int length = snprintf(
            header,
            sizeof(header),
            "Profile=%lu\nEnded=%04u-%02u-%02u %02u:%02u:%02u\nRounds=%u\n",
            (unsigned long)app->character_id,
            (unsigned)now.year,
            (unsigned)now.month,
            (unsigned)now.day,
            (unsigned)now.hour,
            (unsigned)now.minute,
            (unsigned)now.second,
            (unsigned)app->round);
        ok = length > 0 && (size_t)length < sizeof(header) &&
             storage_file_write(file, header, (size_t)length) == (size_t)length;
    }
    for(uint8_t i = 0U; ok && i < app->combat_count; ++i) {
        const InitiativeMember* member = &app->combat[i];
        bool party = dndinitiative_member_is_party(app, member);
        if(!party && member->hp_current <= 0) continue;
        ok = dndinitiative_history_write_member(file, party ? 'P' : 'O', member);
    }
    if(ok) ok = storage_file_sync(file);
    storage_file_close(file);
    storage_file_free(file);
    if(!ok) {
        storage_common_remove(app->storage, temp_path);
        return false;
    }
    if(storage_common_rename(app->storage, temp_path, final_path) != FSE_OK) {
        storage_common_remove(app->storage, temp_path);
        return false;
    }
    return true;
}

static void dndinitiative_end_combat(InitiativeApp* app, const char* status) {
    if(!app) return;
    app->active = 0U;
    app->combat_count = 0U;
    app->current_turn = 0U;
    app->round = 1U;
    app->screen = InitiativeScreenMenu;
    app->selection = app->scroll = 0U;
    dndinitiative_copy(app->status, sizeof(app->status), status);
    dndinitiative_save(app);
}

static void dndinitiative_row(Canvas* canvas, uint8_t row, bool selected, const char* text) {
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

static void dndinitiative_draw(Canvas* canvas, void* model) {
    InitiativeApp* app = *(InitiativeApp**)model;
    canvas_clear(canvas);

    /* Initiative uses the same dark title bar as the other standalone DND FAPs.
       Character ID remains main-menu-only; active combat uses that right-side
       header space for the round counter instead. */
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 10);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, "DNDInitiative");
    if(app->screen == InitiativeScreenMenu && app->character_id != UINT32_MAX) {
        char profile_id[16];
        snprintf(profile_id, sizeof(profile_id), "[%lu]", (unsigned long)app->character_id);
        uint16_t profile_width = canvas_string_width(canvas, profile_id);
        uint8_t profile_x = profile_width < 125U ? (uint8_t)(126U - profile_width) : 1U;
        canvas_draw_str(canvas, profile_x, 8, profile_id);
    } else if(app->screen == InitiativeScreenCombat) {
        char round_text[24];
        snprintf(
            round_text,
            sizeof(round_text),
            "R%u T%u/%u",
            app->round,
            app->combat_count ? (uint8_t)(app->current_turn + 1U) : 0U,
            app->combat_count);
        uint16_t round_width = canvas_string_width(canvas, round_text);
        uint8_t round_x = round_width < 125U ? (uint8_t)(126U - round_width) : 1U;
        canvas_draw_str(canvas, round_x, 8, round_text);
    }
    canvas_set_color(canvas, ColorBlack);
    if(app->screen == InitiativeScreenNoCharacter) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 24, "No DND character found.");
        canvas_draw_str(canvas, 2, 34, "Create/load one first.");
        dndinitiative_row(canvas, 3U, app->selection == 0U, "Launch DNDolphins");
        dndinitiative_row(canvas, 4U, app->selection == 1U, "Exit Initiative");
    } else if(app->screen == InitiativeScreenMenu) {
        char rows[6][32];
        snprintf(rows[0], sizeof(rows[0]), "Start New Combat");
        snprintf(rows[1], sizeof(rows[1]), "Resume%s", app->active ? "" : " (none)");
        snprintf(rows[2], sizeof(rows[2]), "Party Roster (%u)", app->roster_count);
        snprintf(rows[3], sizeof(rows[3]), "Edit Current Order");
        dndinitiative_copy(rows[4], sizeof(rows[4]), "End Current Combat");
        snprintf(
            rows[5],
            sizeof(rows[5]),
            "Default Roll: %s",
            dndinitiative_roll_mode_name((InitiativeRollMode)app->roll_mode));
        for(uint8_t row = 0U; row < 5U; ++row) {
            uint8_t i = (uint8_t)(app->scroll + row);
            if(i >= 6U) break;
            dndinitiative_row(canvas, row, i == app->selection, rows[i]);
        }
    } else if(app->screen == InitiativeScreenRoster) {
        uint8_t total = (uint8_t)(app->roster_count + 1U);
        for(uint8_t row = 0U; row < 5U; ++row) {
            uint8_t i = (uint8_t)(app->scroll + row);
            if(i >= total) break;
            char text[48];
            if(i == app->roster_count)
                dndinitiative_copy(text, sizeof(text), "+ New");
            else
                snprintf(
                    text,
                    sizeof(text),
                    "%.18s HP%d AC%d",
                    app->roster[i].name,
                    app->roster[i].hp_current,
                    app->roster[i].armor_class);
            dndinitiative_row(canvas, row, i == app->selection, text);
        }
    } else if(app->screen == InitiativeScreenSetup) {
        uint8_t total = (uint8_t)(app->combat_count + 3U);
        for(uint8_t row = 0U; row < 5U; ++row) {
            uint8_t i = (uint8_t)(app->scroll + row);
            if(i >= total) break;
            char text[48];
            if(i == 0U)
                dndinitiative_copy(text, sizeof(text), "Roll for All");
            else if(i <= app->combat_count) {
                InitiativeMember* member = &app->combat[i - 1U];
                snprintf(
                    text,
                    sizeof(text),
                    "%.15s I%d HP%d AC%d",
                    member->name,
                    member->total,
                    member->hp_current,
                    member->armor_class);
            } else if(i == app->combat_count + 1U)
                dndinitiative_copy(text, sizeof(text), "+ Temporary Member");
            else
                dndinitiative_copy(text, sizeof(text), "Begin Combat");
            dndinitiative_row(canvas, row, i == app->selection, text);
        }
    } else if(app->screen == InitiativeScreenCombat) {
        for(uint8_t row = 0U; row < 5U; ++row) {
            uint8_t i = (uint8_t)(app->scroll + row);
            if(i >= app->combat_count) break;
            char text[48];
            snprintf(
                text,
                sizeof(text),
                "%c %.10s I%d HP%d AC%d %.5s",
                i == app->current_turn ? '>' : ' ',
                app->combat[i].name,
                app->combat[i].total,
                app->combat[i].hp_current,
                app->combat[i].armor_class,
                app->combat[i].conditions);
            dndinitiative_row(canvas, row, i == app->selection, text);
        }
    } else if(app->screen == InitiativeScreenEndCombat) {
        static const char* const choices[] = {
            "End + Save History",
            "End Without History",
            "Cancel",
        };
        for(uint8_t row = 0U; row < 3U; ++row)
            dndinitiative_row(canvas, row, row == app->selection, choices[row]);
    } else {
        InitiativeMember* member = app->edit_combat ? &app->combat[app->selection] :
                                                      &app->roster[app->selection];
        char rows[9][48];
        uint8_t count = app->edit_combat ? 9U : 8U;
        snprintf(rows[0], sizeof(rows[0]), "Name: %.20s", member->name);
        if(app->edit_combat) {
            snprintf(rows[1], sizeof(rows[1]), "Initiative roll: %d", member->total);
            snprintf(rows[2], sizeof(rows[2]), "Modifier: %+d", member->modifier);
            snprintf(
                rows[3],
                sizeof(rows[3]),
                "Roll: %s",
                dndinitiative_roll_mode_name((InitiativeRollMode)member->roll_mode));
            snprintf(rows[4], sizeof(rows[4]), "Armor Class: %d", member->armor_class);
            snprintf(rows[5], sizeof(rows[5]), "Current HP: %d", member->hp_current);
            snprintf(rows[6], sizeof(rows[6]), "Maximum HP: %d", member->hp_max);
            snprintf(rows[7], sizeof(rows[7]), "Conditions: %.16s", member->conditions);
            dndinitiative_copy(
                rows[8], sizeof(rows[8]), app->delete_armed ? "OK again: delete" : "Delete");
        } else {
            snprintf(rows[1], sizeof(rows[1]), "Initiative mod: %+d", member->modifier);
            snprintf(
                rows[2],
                sizeof(rows[2]),
                "Roll: %s",
                dndinitiative_roll_mode_name((InitiativeRollMode)member->roll_mode));
            snprintf(rows[3], sizeof(rows[3]), "Armor Class: %d", member->armor_class);
            snprintf(rows[4], sizeof(rows[4]), "Current HP: %d", member->hp_current);
            snprintf(rows[5], sizeof(rows[5]), "Maximum HP: %d", member->hp_max);
            snprintf(rows[6], sizeof(rows[6]), "Conditions: %.16s", member->conditions);
            dndinitiative_copy(
                rows[7], sizeof(rows[7]), app->delete_armed ? "OK again: delete" : "Delete");
        }
        for(uint8_t row = 0U; row < 5U; ++row) {
            uint8_t i = (uint8_t)(app->scroll + row);
            if(i >= count) break;
            dndinitiative_row(canvas, row, i == app->edit_field, rows[i]);
        }
    }
}

static void dndinitiative_sort(InitiativeApp* app) {
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

static void dndinitiative_seed_setup(InitiativeApp* app) {
    dndinitiative_patch_character(app, NULL, 2U);
    app->combat_count = app->roster_count;
    memcpy(app->combat, app->roster, app->roster_count * sizeof(InitiativeMember));
    for(uint8_t i = 0U; i < app->combat_count; ++i)
        app->combat[i].total = app->combat[i].modifier;
    app->active = 0U;
    app->round = 1U;
    app->current_turn = 0U;
    app->selection = app->scroll = 0U;
    app->screen = InitiativeScreenSetup;
    dndinitiative_save(app);
}

static void dndinitiative_start(InitiativeApp* app) {
    dndinitiative_sort(app);
    app->active = app->combat_count ? 1U : 0U;
    app->round = 1U;
    app->current_turn = 0U;
    app->screen = InitiativeScreenCombat;
    dndinitiative_focus_combat_member(app, app->current_turn);
    dndinitiative_save(app);
}

static void dndinitiative_move(uint8_t* selection, uint8_t count, int8_t direction) {
    if(!count) return;
    int16_t next = (int16_t)*selection + direction;
    if(next < 0) next = count - 1U;
    if(next >= count) next = 0;
    *selection = (uint8_t)next;
}

static void dndinitiative_keep_visible(uint8_t selection, uint8_t count, uint8_t* scroll) {
    if(!scroll || !count) {
        if(scroll) *scroll = 0U;
        return;
    }
    uint8_t maximum_scroll = count > 5U ? (uint8_t)(count - 5U) : 0U;
    if(selection < *scroll)
        *scroll = selection;
    else if(selection >= (uint8_t)(*scroll + 5U))
        *scroll = (uint8_t)(selection - 4U);
    if(*scroll > maximum_scroll) *scroll = maximum_scroll;
}

static void dndinitiative_focus_combat_member(InitiativeApp* app, uint8_t index) {
    if(!app || !app->combat_count) {
        if(app) app->selection = app->scroll = 0U;
        return;
    }
    if(index >= app->combat_count) index = (uint8_t)(app->combat_count - 1U);
    app->selection = index;
    uint8_t maximum_scroll = app->combat_count > 5U ? (uint8_t)(app->combat_count - 5U) : 0U;
    uint8_t centered = index > 2U ? (uint8_t)(index - 2U) : 0U;
    if(centered > maximum_scroll) centered = maximum_scroll;
    app->scroll = centered;
}

static void dndinitiative_text_done(void* context) {
    InitiativeApp* app = context;
    InitiativeMember* list = app->edit_combat ? app->combat : app->roster;
    uint8_t count = app->edit_combat ? app->combat_count : app->roster_count;
    if(app->input_member < count) {
        InitiativeMember* member = &list[app->input_member];
        if(app->text_target == InitiativeTextName) {
            bool was_main = dndinitiative_member_is_main(app, member);
            dndinitiative_copy(member->name, sizeof(member->name), app->edit_buffer);
            if(was_main)
                dndinitiative_copy(
                    app->main_character_name, sizeof(app->main_character_name), member->name);
        } else if(app->text_target == InitiativeTextConditions)
            dndinitiative_copy(member->conditions, sizeof(member->conditions), app->edit_buffer);
    }
    app->text_target = InitiativeTextNone;
    dndinitiative_save(app);
    view_dispatcher_switch_to_view(app->dispatcher, 0U);
    dndinitiative_redraw(app);
}

static void dndinitiative_begin_text(
    InitiativeApp* app,
    InitiativeTextTarget target,
    const char* header,
    const char* initial) {
    if(!app->text_input) {
        app->text_input = text_input_alloc();
        if(!app->text_input) return;
        view_dispatcher_add_view(app->dispatcher, 1U, text_input_get_view(app->text_input));
    }
    app->text_target = target;
    dndinitiative_copy(app->edit_buffer, sizeof(app->edit_buffer), initial);
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, header);
    text_input_set_result_callback(
        app->text_input,
        dndinitiative_text_done,
        app,
        app->edit_buffer,
        sizeof(app->edit_buffer),
        false);
    view_dispatcher_switch_to_view(app->dispatcher, 1U);
}

static void dndinitiative_number_done(void* context, int32_t number) {
    InitiativeApp* app = context;
    InitiativeMember* list = app->number_combat ? app->combat : app->roster;
    uint8_t count = app->number_combat ? app->combat_count : app->roster_count;
    if(app->input_member < count) {
        InitiativeMember* member = &list[app->input_member];
        switch(app->number_target) {
        case InitiativeNumberManualRoll:
            member->total = (int16_t)(dndinitiative_clamp(number, 1, 20) + member->modifier);
            break;
        case InitiativeNumberTotal:
            member->total = dndinitiative_clamp(number, -99, 199);
            break;
        case InitiativeNumberModifier:
            member->modifier = (int8_t)dndinitiative_clamp(number, -50, 50);
            break;
        case InitiativeNumberArmorClass:
            member->armor_class = dndinitiative_clamp(number, 0, 99);
            dndinitiative_sync_main_if_needed(app, member);
            break;
        case InitiativeNumberHpCurrent:
            member->hp_current = dndinitiative_clamp(number, -999, 999);
            dndinitiative_sync_main_if_needed(app, member);
            break;
        case InitiativeNumberHpMax:
            member->hp_max = dndinitiative_clamp(number, 0, 999);
            dndinitiative_sync_main_if_needed(app, member);
            break;
        default:
            break;
        }
    }
    app->number_target = InitiativeNumberNone;
    dndinitiative_save(app);
    view_dispatcher_switch_to_view(app->dispatcher, 0U);
    dndinitiative_redraw(app);
}

static void dndinitiative_begin_number(
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
        app->number_input, dndinitiative_number_done, app, initial, minimum, maximum);
    view_dispatcher_switch_to_view(app->dispatcher, 2U);
}

static void dndinitiative_previous_turn(InitiativeApp* app) {
    if(!app || !app->combat_count) return;
    if(app->current_turn) {
        --app->current_turn;
    } else if(app->round > 1U) {
        --app->round;
        app->current_turn = (uint8_t)(app->combat_count - 1U);
    }
    dndinitiative_focus_combat_member(app, app->current_turn);
    dndinitiative_save(app);
}

static bool dndinitiative_input(InputEvent* event, void* context) {
    InitiativeApp* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat &&
       event->type != InputTypeLong)
        return true;
    if(event->type == InputTypeLong && event->key == InputKeyBack) {
        app->return_to_dnd = 0U;
        view_dispatcher_stop(app->dispatcher);
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
            app->return_to_dnd = 1U;
            view_dispatcher_stop(app->dispatcher);
            return true;
        }
    } else if(app->screen == InitiativeScreenMenu) {
        if(event->type == InputTypeShort && event->key == InputKeyBack) {
            app->return_to_dnd = 1U;
            view_dispatcher_stop(app->dispatcher);
            return true;
        } else if(
            (event->type == InputTypeShort || event->type == InputTypeRepeat) &&
            event->key == InputKeyUp) {
            dndinitiative_move(&app->selection, 6U, -1);
            dndinitiative_keep_visible(app->selection, 6U, &app->scroll);
        } else if(
            (event->type == InputTypeShort || event->type == InputTypeRepeat) &&
            event->key == InputKeyDown) {
            dndinitiative_move(&app->selection, 6U, 1);
            dndinitiative_keep_visible(app->selection, 6U, &app->scroll);
        } else if(
            (event->type == InputTypeShort || event->type == InputTypeRepeat) &&
            app->selection == 5U && (event->key == InputKeyLeft || event->key == InputKeyRight)) {
            int8_t delta = event->key == InputKeyRight ? 1 : -1;
            int8_t mode = (int8_t)app->roll_mode + delta;
            if(mode < (int8_t)InitiativeRollNormal) mode = (int8_t)InitiativeRollDisadvantage;
            if(mode > (int8_t)InitiativeRollDisadvantage) mode = (int8_t)InitiativeRollNormal;
            app->roll_mode = (uint8_t)mode;
            dndinitiative_save(app);
        } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
            if(app->selection == 0U)
                dndinitiative_seed_setup(app);
            else if(app->selection == 1U) {
                if(app->active) {
                    app->screen = InitiativeScreenCombat;
                    dndinitiative_focus_combat_member(app, app->current_turn);
                }
            } else if(app->selection == 2U) {
                app->screen = InitiativeScreenRoster;
                app->selection = app->scroll = 0U;
            } else if(app->selection == 3U) {
                if(app->combat_count) {
                    app->screen = InitiativeScreenSetup;
                    app->selection = app->scroll = 0U;
                }
            } else if(app->selection == 4U) {
                if(app->combat_count) {
                    app->screen = InitiativeScreenEndCombat;
                    app->selection = app->scroll = 0U;
                } else {
                    dndinitiative_copy(app->status, sizeof(app->status), "No current combat");
                }
            } else if(app->selection == 5U) {
                app->roll_mode = (uint8_t)((app->roll_mode + 1U) % 3U);
                dndinitiative_save(app);
            }
        }
    } else if(app->screen == InitiativeScreenEndCombat) {
        if((event->type == InputTypeShort || event->type == InputTypeRepeat) &&
           event->key == InputKeyUp)
            dndinitiative_move(&app->selection, 3U, -1);
        else if(
            (event->type == InputTypeShort || event->type == InputTypeRepeat) &&
            event->key == InputKeyDown)
            dndinitiative_move(&app->selection, 3U, 1);
        else if(event->type == InputTypeShort && event->key == InputKeyBack) {
            app->screen = InitiativeScreenMenu;
            app->selection = 4U;
            app->scroll = 0U;
        } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
            if(app->selection == 0U) {
                if(dndinitiative_save_completed_history(app))
                    dndinitiative_end_combat(app, "History saved");
                else
                    dndinitiative_copy(app->status, sizeof(app->status), "History save failed");
            } else if(app->selection == 1U) {
                dndinitiative_end_combat(app, "Combat ended");
            } else {
                app->screen = InitiativeScreenMenu;
                app->selection = 4U;
                app->scroll = 0U;
            }
        }
    } else if(app->screen == InitiativeScreenRoster) {
        uint8_t total = (uint8_t)(app->roster_count + 1U);
        if((event->type == InputTypeShort || event->type == InputTypeRepeat) &&
           event->key == InputKeyUp) {
            dndinitiative_move(&app->selection, total, -1);
            dndinitiative_keep_visible(app->selection, total, &app->scroll);
        } else if(
            (event->type == InputTypeShort || event->type == InputTypeRepeat) &&
            event->key == InputKeyDown) {
            dndinitiative_move(&app->selection, total, 1);
            dndinitiative_keep_visible(app->selection, total, &app->scroll);
        } else if(event->type == InputTypeShort && event->key == InputKeyBack) {
            app->screen = InitiativeScreenMenu;
            app->selection = app->scroll = 0U;
        } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
            if(app->selection == app->roster_count && app->roster_count < INIT_MAX) {
                InitiativeMember* member = &app->roster[app->roster_count++];
                memset(member, 0, sizeof(*member));
                dndinitiative_copy(member->name, sizeof(member->name), "New");
                member->hp_current = member->hp_max = 1;
                member->armor_class = 10;
                member->roll_mode = app->roll_mode;
                app->selection = (uint8_t)(app->roster_count - 1U);
            }
            if(app->selection < app->roster_count) {
                app->screen = InitiativeScreenEdit;
                app->edit_combat = 0U;
                app->edit_setup = 0U;
                app->edit_field = app->scroll = 0U;
                app->delete_armed = 0U;
            }
        }
    } else if(app->screen == InitiativeScreenSetup) {
        uint8_t total = (uint8_t)(app->combat_count + 3U);
        if((event->type == InputTypeShort || event->type == InputTypeRepeat) &&
           event->key == InputKeyUp) {
            dndinitiative_move(&app->selection, total, -1);
            dndinitiative_keep_visible(app->selection, total, &app->scroll);
        } else if(
            (event->type == InputTypeShort || event->type == InputTypeRepeat) &&
            event->key == InputKeyDown) {
            dndinitiative_move(&app->selection, total, 1);
            dndinitiative_keep_visible(app->selection, total, &app->scroll);
        } else if(
            (event->type == InputTypeShort || event->type == InputTypeRepeat) &&
            app->selection > 0U && app->selection <= app->combat_count &&
            (event->key == InputKeyLeft || event->key == InputKeyRight)) {
            InitiativeMember* member = &app->combat[app->selection - 1U];
            member->total = dndinitiative_clamp(
                member->total + (event->key == InputKeyRight ? 1 : -1), -99, 199);
            dndinitiative_save(app);
        } else if(
            event->type == InputTypeLong && event->key == InputKeyUp && app->selection > 0U &&
            app->selection <= app->combat_count) {
            InitiativeMember* member = &app->combat[app->selection - 1U];
            member->armor_class = dndinitiative_clamp(member->armor_class + 1, 0, 99);
            dndinitiative_sync_main_if_needed(app, member);
            dndinitiative_save(app);
        } else if(
            event->type == InputTypeLong && app->selection > 0U &&
            app->selection <= app->combat_count &&
            (event->key == InputKeyLeft || event->key == InputKeyRight)) {
            uint8_t index = (uint8_t)(app->selection - 1U);
            if(event->key == InputKeyLeft && index > 0U) {
                dndinitiative_swap(app, index, (uint8_t)(index - 1U));
                --app->selection;
                dndinitiative_save(app);
            } else if(event->key == InputKeyRight && index + 1U < app->combat_count) {
                dndinitiative_swap(app, index, (uint8_t)(index + 1U));
                ++app->selection;
                dndinitiative_save(app);
            }
        } else if(
            event->type == InputTypeLong && event->key == InputKeyOk && app->selection > 0U &&
            app->selection <= app->combat_count) {
            app->selection = (uint8_t)(app->selection - 1U);
            app->edit_combat = 1U;
            app->edit_setup = 1U;
            app->edit_field = app->scroll = 0U;
            app->delete_armed = 0U;
            app->screen = InitiativeScreenEdit;
        } else if(event->type == InputTypeShort && event->key == InputKeyBack) {
            app->screen = InitiativeScreenMenu;
            app->selection = app->scroll = 0U;
        } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
            if(app->selection == 0U) {
                for(uint8_t i = 0; i < app->combat_count; i++)
                    app->combat[i].total =
                        (int16_t)(dndinitiative_roll_d20(
                                      (InitiativeRollMode)app->combat[i].roll_mode) +
                                  app->combat[i].modifier);
                dndinitiative_save(app);
            } else if(app->selection <= app->combat_count) {
                InitiativeMember* member = &app->combat[app->selection - 1U];
                member->total =
                    (int16_t)(dndinitiative_roll_d20((InitiativeRollMode)member->roll_mode) +
                              member->modifier);
                dndinitiative_save(app);
            } else if(app->selection == app->combat_count + 1U && app->combat_count < INIT_MAX) {
                InitiativeMember* member = &app->combat[app->combat_count++];
                memset(member, 0, sizeof(*member));
                dndinitiative_copy(member->name, sizeof(member->name), "Temp");
                member->hp_current = member->hp_max = 1;
                member->armor_class = 10;
                member->roll_mode = app->roll_mode;
                app->input_member = (uint8_t)(app->combat_count - 1U);
                app->edit_combat = 1U;
                dndinitiative_begin_text(
                    app, InitiativeTextName, "Participant name", member->name);
            } else
                dndinitiative_start(app);
        }
    } else if(app->screen == InitiativeScreenCombat) {
        if((event->type == InputTypeShort || event->type == InputTypeRepeat) &&
           event->key == InputKeyUp) {
            dndinitiative_move(&app->selection, app->combat_count, -1);
            dndinitiative_keep_visible(app->selection, app->combat_count, &app->scroll);
        } else if(
            (event->type == InputTypeShort || event->type == InputTypeRepeat) &&
            event->key == InputKeyDown) {
            dndinitiative_move(&app->selection, app->combat_count, 1);
            dndinitiative_keep_visible(app->selection, app->combat_count, &app->scroll);
        } else if(event->type == InputTypeShort && event->key == InputKeyBack) {
            app->screen = InitiativeScreenMenu;
            app->selection = app->scroll = 0U;
        } else if(event->type == InputTypeLong && event->key == InputKeyUp && app->combat_count) {
            dndinitiative_previous_turn(app);
        } else if(
            (event->type == InputTypeShort || event->type == InputTypeRepeat) &&
            (event->key == InputKeyLeft || event->key == InputKeyRight) && app->combat_count) {
            InitiativeMember* m = &app->combat[app->selection];
            m->hp_current = dndinitiative_clamp(
                m->hp_current + (event->key == InputKeyRight ? 1 : -1), -999, 999);
            dndinitiative_sync_main_if_needed(app, m);
            dndinitiative_save(app);
        } else if(event->type == InputTypeLong && event->key == InputKeyOk && app->combat_count) {
            app->edit_combat = 1U;
            app->edit_setup = 0U;
            app->edit_field = app->scroll = 0U;
            app->delete_armed = 0U;
            app->screen = InitiativeScreenEdit;
        } else if(event->type == InputTypeLong && event->key == InputKeyDown && app->combat_count) {
            app->edit_combat = 1U;
            app->edit_setup = 0U;
            app->input_member = app->selection;
            dndinitiative_begin_text(
                app,
                InitiativeTextConditions,
                "Participant conditions",
                app->combat[app->selection].conditions);
        } else if(
            event->type == InputTypeLong && event->key == InputKeyLeft && app->combat_count &&
            app->selection > 0U) {
            dndinitiative_swap(app, app->selection, (uint8_t)(app->selection - 1U));
            --app->selection;
            dndinitiative_keep_visible(app->selection, app->combat_count, &app->scroll);
            dndinitiative_save(app);
        } else if(
            event->type == InputTypeLong && event->key == InputKeyRight && app->combat_count &&
            app->selection + 1U < app->combat_count) {
            dndinitiative_swap(app, app->selection, (uint8_t)(app->selection + 1U));
            ++app->selection;
            dndinitiative_keep_visible(app->selection, app->combat_count, &app->scroll);
            dndinitiative_save(app);
        } else if(event->type == InputTypeShort && event->key == InputKeyOk && app->combat_count) {
            dndinitiative_patch_character(app, NULL, 1U);
            ++app->current_turn;
            if(app->current_turn >= app->combat_count) {
                app->current_turn = 0U;
                ++app->round;
                if(!app->round) app->round = 1U;
            }
            dndinitiative_focus_combat_member(app, app->current_turn);
            dndinitiative_save(app);
        }
    } else {
        InitiativeMember* member = app->edit_combat ? &app->combat[app->selection] :
                                                      &app->roster[app->selection];
        uint8_t edit_count = app->edit_combat ? 9U : 8U;
        if((event->type == InputTypeShort || event->type == InputTypeRepeat) &&
           event->key == InputKeyUp) {
            app->delete_armed = 0U;
            dndinitiative_move(&app->edit_field, edit_count, -1);
            dndinitiative_keep_visible(app->edit_field, edit_count, &app->scroll);
        } else if(
            (event->type == InputTypeShort || event->type == InputTypeRepeat) &&
            event->key == InputKeyDown) {
            app->delete_armed = 0U;
            dndinitiative_move(&app->edit_field, edit_count, 1);
            dndinitiative_keep_visible(app->edit_field, edit_count, &app->scroll);
        } else if(event->type == InputTypeShort && event->key == InputKeyBack) {
            uint8_t edited_member = app->selection;
            if(app->edit_setup) {
                app->screen = InitiativeScreenSetup;
                app->selection = (uint8_t)(edited_member + 1U);
                app->scroll = 0U;
                dndinitiative_keep_visible(
                    app->selection, (uint8_t)(app->combat_count + 3U), &app->scroll);
            } else if(app->edit_combat) {
                app->screen = InitiativeScreenCombat;
                dndinitiative_focus_combat_member(app, edited_member);
            } else {
                app->screen = InitiativeScreenRoster;
                app->selection = edited_member;
                app->scroll = 0U;
                dndinitiative_keep_visible(
                    app->selection, (uint8_t)(app->roster_count + 1U), &app->scroll);
            }
        } else if(
            (event->type == InputTypeShort || event->type == InputTypeRepeat) &&
            (event->key == InputKeyLeft || event->key == InputKeyRight)) {
            int16_t d = event->key == InputKeyRight ? 1 : -1;
            if(app->edit_combat) {
                if(app->edit_field == 1U)
                    member->total = dndinitiative_clamp(member->total + d, -99, 199);
                else if(app->edit_field == 2U)
                    member->modifier = (int8_t)dndinitiative_clamp(member->modifier + d, -50, 50);
                else if(app->edit_field == 3U) {
                    int8_t mode = (int8_t)member->roll_mode + d;
                    if(mode < (int8_t)InitiativeRollNormal)
                        mode = (int8_t)InitiativeRollDisadvantage;
                    if(mode > (int8_t)InitiativeRollDisadvantage)
                        mode = (int8_t)InitiativeRollNormal;
                    member->roll_mode = (uint8_t)mode;
                } else if(app->edit_field == 4U) {
                    member->armor_class = dndinitiative_clamp(member->armor_class + d, 0, 99);
                    dndinitiative_sync_main_if_needed(app, member);
                } else if(app->edit_field == 5U) {
                    member->hp_current = dndinitiative_clamp(member->hp_current + d, -999, 999);
                    dndinitiative_sync_main_if_needed(app, member);
                } else if(app->edit_field == 6U) {
                    member->hp_max = dndinitiative_clamp(member->hp_max + d, 0, 999);
                    dndinitiative_sync_main_if_needed(app, member);
                }
            } else {
                if(app->edit_field == 1U)
                    member->modifier = (int8_t)dndinitiative_clamp(member->modifier + d, -50, 50);
                else if(app->edit_field == 2U) {
                    int8_t mode = (int8_t)member->roll_mode + d;
                    if(mode < (int8_t)InitiativeRollNormal)
                        mode = (int8_t)InitiativeRollDisadvantage;
                    if(mode > (int8_t)InitiativeRollDisadvantage)
                        mode = (int8_t)InitiativeRollNormal;
                    member->roll_mode = (uint8_t)mode;
                } else if(app->edit_field == 3U) {
                    member->armor_class = dndinitiative_clamp(member->armor_class + d, 0, 99);
                    dndinitiative_sync_main_if_needed(app, member);
                } else if(app->edit_field == 4U) {
                    member->hp_current = dndinitiative_clamp(member->hp_current + d, -999, 999);
                    dndinitiative_sync_main_if_needed(app, member);
                } else if(app->edit_field == 5U) {
                    member->hp_max = dndinitiative_clamp(member->hp_max + d, 0, 999);
                    dndinitiative_sync_main_if_needed(app, member);
                }
            }
            dndinitiative_save(app);
        } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
            app->input_member = app->selection;
            if(app->edit_field == 0U) {
                dndinitiative_begin_text(
                    app, InitiativeTextName, "Participant name", member->name);
            } else if(app->edit_combat) {
                if(app->edit_field == 1U)
                    dndinitiative_begin_number(
                        app,
                        InitiativeNumberTotal,
                        true,
                        app->selection,
                        "Initiative total",
                        member->total,
                        -99,
                        199);
                else if(app->edit_field == 2U)
                    dndinitiative_begin_number(
                        app,
                        InitiativeNumberModifier,
                        true,
                        app->selection,
                        "Initiative modifier",
                        member->modifier,
                        -50,
                        50);
                else if(app->edit_field == 3U) {
                    member->roll_mode = (uint8_t)((member->roll_mode + 1U) % 3U);
                    dndinitiative_save(app);
                } else if(app->edit_field == 4U)
                    dndinitiative_begin_number(
                        app,
                        InitiativeNumberArmorClass,
                        true,
                        app->selection,
                        "Armor Class",
                        member->armor_class,
                        0,
                        99);
                else if(app->edit_field == 5U)
                    dndinitiative_begin_number(
                        app,
                        InitiativeNumberHpCurrent,
                        true,
                        app->selection,
                        "Current HP",
                        member->hp_current,
                        -999,
                        999);
                else if(app->edit_field == 6U)
                    dndinitiative_begin_number(
                        app,
                        InitiativeNumberHpMax,
                        true,
                        app->selection,
                        "Maximum HP",
                        member->hp_max,
                        0,
                        999);
                else if(app->edit_field == 7U)
                    dndinitiative_begin_text(
                        app, InitiativeTextConditions, "Conditions", member->conditions);
                else if(app->delete_armed) {
                    memmove(
                        &app->combat[app->selection],
                        &app->combat[app->selection + 1U],
                        (app->combat_count - app->selection - 1U) * sizeof(*app->combat));
                    --app->combat_count;
                    if(!app->combat_count) {
                        app->active = 0U;
                        app->current_turn = 0U;
                    } else if(app->current_turn >= app->combat_count)
                        app->current_turn = 0U;
                    if(app->edit_setup) {
                        app->screen = InitiativeScreenSetup;
                        app->selection = app->combat_count ? 1U : 0U;
                        app->scroll = 0U;
                    } else {
                        app->screen = InitiativeScreenCombat;
                        dndinitiative_focus_combat_member(app, app->current_turn);
                    }
                    dndinitiative_save(app);
                } else
                    app->delete_armed = 1U;
            } else {
                if(app->edit_field == 1U)
                    dndinitiative_begin_number(
                        app,
                        InitiativeNumberModifier,
                        false,
                        app->selection,
                        "Initiative modifier",
                        member->modifier,
                        -50,
                        50);
                else if(app->edit_field == 2U) {
                    member->roll_mode = (uint8_t)((member->roll_mode + 1U) % 3U);
                    dndinitiative_save(app);
                } else if(app->edit_field == 3U)
                    dndinitiative_begin_number(
                        app,
                        InitiativeNumberArmorClass,
                        false,
                        app->selection,
                        "Armor Class",
                        member->armor_class,
                        0,
                        99);
                else if(app->edit_field == 4U)
                    dndinitiative_begin_number(
                        app,
                        InitiativeNumberHpCurrent,
                        false,
                        app->selection,
                        "Current HP",
                        member->hp_current,
                        -999,
                        999);
                else if(app->edit_field == 5U)
                    dndinitiative_begin_number(
                        app,
                        InitiativeNumberHpMax,
                        false,
                        app->selection,
                        "Maximum HP",
                        member->hp_max,
                        0,
                        999);
                else if(app->edit_field == 6U)
                    dndinitiative_begin_text(
                        app, InitiativeTextConditions, "Conditions", member->conditions);
                else if(app->delete_armed) {
                    memmove(
                        &app->roster[app->selection],
                        &app->roster[app->selection + 1U],
                        (app->roster_count - app->selection - 1U) * sizeof(*app->roster));
                    --app->roster_count;
                    app->screen = InitiativeScreenRoster;
                    app->selection = app->scroll = 0U;
                    dndinitiative_save(app);
                } else
                    app->delete_armed = 1U;
            }
        }
    }
    if(app->screen == InitiativeScreenEdit) {
        if(app->edit_field < app->scroll) app->scroll = app->edit_field;
        if(app->edit_field >= app->scroll + 5U) app->scroll = (uint8_t)(app->edit_field - 4U);
    } else {
        if(app->selection < app->scroll) app->scroll = app->selection;
        if(app->selection >= app->scroll + 5U) app->scroll = (uint8_t)(app->selection - 4U);
    }
    dndinitiative_redraw(app);
    return true;
}

static bool dndinitiative_navigation(void* context) {
    InitiativeApp* app = context;
    if(app->screen == InitiativeScreenNoCharacter) {
        app->return_to_dnd = 1U;
        view_dispatcher_stop(app->dispatcher);
    } else if(app->screen == InitiativeScreenMenu) {
        app->return_to_dnd = 1U;
        view_dispatcher_stop(app->dispatcher);
    } else {
        app->screen = InitiativeScreenMenu;
        app->selection = app->scroll = 0U;
        dndinitiative_redraw(app);
    }
    return true;
}

static InitiativeApp* dndinitiative_alloc(const char* args) {
    InitiativeApp* app = calloc(1U, sizeof(InitiativeApp));
    if(!app) return NULL;
    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    if(!app->gui || !app->storage) goto fail;
    if(!dnd_profile_ref_active_id(app->storage, &app->character_id)) app->character_id = 0U;
    app->have_character = dnd_profile_ref_exists(app->storage, app->character_id) ? 1U : 0U;
    if(app->have_character) {
        dndinitiative_load(app);
        if(args) dndinitiative_import_args(app, args);
        bool refreshed_main = dndinitiative_refresh_main_character(app);
        if(refreshed_main || (args && strchr(args, ';'))) dndinitiative_save(app);
        app->screen = InitiativeScreenMenu;
    } else {
        app->selection = app->scroll = 0U;
        app->screen = InitiativeScreenNoCharacter;
    }
    app->dispatcher = view_dispatcher_alloc();
    app->view = view_alloc();
    if(!app->dispatcher || !app->view) goto fail;
    view_dispatcher_set_event_callback_context(app->dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->dispatcher, dndinitiative_navigation);
    view_allocate_model(app->view, ViewModelTypeLockFree, sizeof(InitiativeApp*));
    InitiativeApp** model = view_get_model(app->view);
    if(!model) goto fail;
    *model = app;
    view_commit_model(app->view, false);
    view_set_context(app->view, app);
    view_set_draw_callback(app->view, dndinitiative_draw);
    view_set_input_callback(app->view, dndinitiative_input);
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

static void dndinitiative_free(InitiativeApp* app) {
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
    InitiativeApp* app = dndinitiative_alloc(context);
    if(!app) return -1;
    view_dispatcher_switch_to_view(app->dispatcher, 0U);
    view_dispatcher_run(app->dispatcher);
    bool return_to_dnd = app->return_to_dnd;
    dndinitiative_free(app);
    if(return_to_dnd)
        (void)dnd_handoff_launch_if_present(
            DNDOLPHINS_FAP_PATH, POCKET_D20_RETURN_FOCUS_INITIATIVE);
    return 0;
}
