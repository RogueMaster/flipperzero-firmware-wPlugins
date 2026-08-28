#include "pocket_d20_storage.h"

#include <furi.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define POCKET_D20_TEXT_VERSION                  3U
#define POCKET_D20_OLDEST_SUPPORTED_TEXT_VERSION 2U
#define POCKET_D20_LINE_LEN                      768U
#define POCKET_D20_READ_BUFFER                   512U
#define POCKET_D20_DATA_DIR                      APP_DATA_PATH("profiles")
#define POCKET_D20_EXPORT_DIR                    APP_DATA_PATH("profiles/exports")
#define POCKET_D20_ARCHIVE_DIR                   APP_DATA_PATH("profiles/archive")
#define POCKET_D20_LEGACY_DATA_DIR               APP_ASSETS_PATH("profiles")

#define POCKET_D20_ACTIVE_PROFILE_PATH      APP_DATA_PATH("profiles/custom_active_profile.txt")
#define POCKET_D20_ACTIVE_PROFILE_TEMP_PATH APP_DATA_PATH("profiles/custom_active_profile.tmp")

static void pocket_d20_copy(char* destination, size_t size, const char* source) {
    if(size == 0U) return;
    strncpy(destination, source, size - 1U);
    destination[size - 1U] = '\0';
}

static bool pocket_d20_schema_supported(unsigned long schema) {
    switch(schema) {
    case 2U:
    case 3U:
        return true;
    default:
        return false;
    }
}

static bool pocket_d20_copy_file(
    Storage* storage,
    const char* source,
    const char* destination,
    const char* temporary);

static uint8_t pocket_d20_character_level(const PocketCharacter* character) {
    uint16_t total = 0U;
    for(uint8_t i = 0U; i < character->class_count; ++i)
        total += character->classes[i].level;
    if(total < 1U) total = 1U;
    return total > 255U ? 255U : (uint8_t)total;
}

static void pocket_d20_filename_name(char* output, size_t size, const char* name) {
    if(size == 0U) return;
    size_t position = 0U;
    for(size_t i = 0U; name[i] && position + 1U < size; ++i) {
        char value = name[i];
        if((value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') ||
           (value >= '0' && value <= '9') || value == '-') {
            output[position++] = value;
        } else if(position && output[position - 1U] != '_') {
            output[position++] = '_';
        }
    }
    while(position && output[position - 1U] == '_')
        --position;
    if(position == 0U) {
        pocket_d20_copy(output, size, "Unnamed");
        return;
    }
    output[position] = '\0';
}

static void pocket_d20_profile_path(
    char* output,
    size_t size,
    uint32_t profile,
    const PocketCharacter* character) {
    char safe_name[POCKET_D20_NAME_LEN];
    pocket_d20_filename_name(safe_name, sizeof(safe_name), character->name);
    snprintf(
        output,
        size,
        "%s/ch_%lu_%s_%u.txt",
        POCKET_D20_DATA_DIR,
        (unsigned long)profile,
        safe_name,
        pocket_d20_character_level(character));
}

static bool pocket_d20_writef(File* file, const char* format, ...) {
    char line[POCKET_D20_LINE_LEN];
    va_list arguments;
    va_start(arguments, format);
    int length = vsnprintf(line, sizeof(line), format, arguments);
    va_end(arguments);
    if(length < 0 || (size_t)length >= sizeof(line)) return false;
    return storage_file_write(file, line, (size_t)length) == (size_t)length;
}

static uint8_t pocket_d20_hex_value(char value) {
    if(value >= '0' && value <= '9') return (uint8_t)(value - '0');
    if(value >= 'A' && value <= 'F') return (uint8_t)(value - 'A' + 10);
    if(value >= 'a' && value <= 'f') return (uint8_t)(value - 'a' + 10);
    return 0xFFU;
}

static bool pocket_d20_write_string(File* file, const char* key, const char* value) {
    char encoded[(POCKET_D20_DETAIL_LEN * 3U) + 1U];
    size_t position = 0U;
    static const char digits[] = "0123456789ABCDEF";
    for(size_t i = 0U; value[i] != '\0'; ++i) {
        uint8_t byte = (uint8_t)value[i];
        bool escape = byte == '%' || byte == '\n' || byte == '\r' || byte < 0x20U;
        if(escape) {
            if(position + 3U >= sizeof(encoded)) return false;
            encoded[position++] = '%';
            encoded[position++] = digits[byte >> 4U];
            encoded[position++] = digits[byte & 0x0FU];
        } else {
            if(position + 1U >= sizeof(encoded)) return false;
            encoded[position++] = (char)byte;
        }
    }
    encoded[position] = '\0';
    return pocket_d20_writef(file, "%s=%s\n", key, encoded);
}

static void pocket_d20_decode_string(char* destination, size_t size, const char* value) {
    size_t output = 0U;
    for(size_t input = 0U; value[input] != '\0' && output + 1U < size; ++input) {
        if(value[input] == '%' && value[input + 1U] && value[input + 2U]) {
            uint8_t high = pocket_d20_hex_value(value[input + 1U]);
            uint8_t low = pocket_d20_hex_value(value[input + 2U]);
            if(high != 0xFFU && low != 0xFFU) {
                destination[output++] = (char)((high << 4U) | low);
                input += 2U;
                continue;
            }
        }
        destination[output++] = value[input];
    }
    if(size) destination[output] = '\0';
}

static bool
    pocket_d20_write_i8_array(File* file, const char* key, const int8_t* values, size_t count) {
    char line[256];
    size_t position = (size_t)snprintf(line, sizeof(line), "%s=", key);
    for(size_t i = 0U; i < count; ++i) {
        int written =
            snprintf(line + position, sizeof(line) - position, "%s%d", i ? "," : "", values[i]);
        if(written < 0 || (size_t)written >= sizeof(line) - position) return false;
        position += (size_t)written;
    }
    line[position++] = '\n';
    return storage_file_write(file, line, position) == position;
}

static bool
    pocket_d20_write_u8_array(File* file, const char* key, const uint8_t* values, size_t count) {
    char line[256];
    size_t position = (size_t)snprintf(line, sizeof(line), "%s=", key);
    for(size_t i = 0U; i < count; ++i) {
        int written = snprintf(
            line + position,
            sizeof(line) - position,
            "%s%u",
            i ? "," : "",
            (unsigned int)values[i]);
        if(written < 0 || (size_t)written >= sizeof(line) - position) return false;
        position += (size_t)written;
    }
    line[position++] = '\n';
    return storage_file_write(file, line, position) == position;
}

typedef struct {
    File* file;
    uint8_t buffer[POCKET_D20_READ_BUFFER];
    uint16_t position;
    uint16_t count;
    bool eof;
} PocketD20Reader;

static void pocket_d20_reader_init(PocketD20Reader* reader, File* file) {
    memset(reader, 0, sizeof(*reader));
    reader->file = file;
}

static bool pocket_d20_reader_next(PocketD20Reader* reader, char* value) {
    if(reader->position >= reader->count) {
        reader->count =
            (uint16_t)storage_file_read(reader->file, reader->buffer, sizeof(reader->buffer));
        reader->position = 0U;
        if(!reader->count) {
            reader->eof = true;
            return false;
        }
    }
    *value = (char)reader->buffer[reader->position++];
    return true;
}

static bool pocket_d20_read_line(PocketD20Reader* reader, char* line, size_t size) {
    size_t position = 0U;
    char character = '\0';
    while(position + 1U < size) {
        if(!pocket_d20_reader_next(reader, &character)) break;
        if(character == '\n') break;
        if(character != '\r') line[position++] = character;
    }
    line[position] = '\0';
    return position > 0U || character == '\n';
}

static bool pocket_d20_read_value(
    PocketD20Reader* reader,
    const char* expected_key,
    char* value,
    size_t value_size) {
    char line[POCKET_D20_LINE_LEN];
    if(!pocket_d20_read_line(reader, line, sizeof(line))) return false;
    char* separator = strchr(line, '=');
    if(!separator) return false;
    *separator = '\0';
    if(strcmp(line, expected_key) != 0) return false;
    pocket_d20_copy(value, value_size, separator + 1U);
    return true;
}

static bool pocket_d20_read_string(
    PocketD20Reader* reader,
    const char* key,
    char* destination,
    size_t destination_size) {
    char value[(POCKET_D20_DETAIL_LEN * 3U) + 1U];
    if(!pocket_d20_read_value(reader, key, value, sizeof(value))) return false;
    pocket_d20_decode_string(destination, destination_size, value);
    return true;
}

static size_t pocket_d20_parse_numbers(const char* value, int32_t* numbers, size_t maximum) {
    size_t count = 0U;
    const char* cursor = value;
    while(*cursor && count < maximum) {
        char* end = NULL;
        long number = strtol(cursor, &end, 10);
        if(end == cursor) break;
        numbers[count++] = (int32_t)number;
        if(*end == ',')
            cursor = end + 1U;
        else if(*end == '\0')
            break;
        else
            return 0U;
    }
    return count;
}

static bool pocket_d20_read_numbers(
    PocketD20Reader* reader,
    const char* key,
    int32_t* numbers,
    size_t expected) {
    char value[256];
    return pocket_d20_read_value(reader, key, value, sizeof(value)) &&
           pocket_d20_parse_numbers(value, numbers, expected) == expected;
}

static bool pocket_d20_write_character(File* file, const PocketSaveData* data) {
    const PocketCharacter* c = &data->character;
    char key[48];
    if(!pocket_d20_writef(file, "PocketD20Character=%u\n", POCKET_D20_TEXT_VERSION) ||
       !pocket_d20_write_string(file, "Name", c->name) ||
       !pocket_d20_write_string(file, "Player", c->player) ||
       !pocket_d20_write_string(file, "Species", c->species) ||
       !pocket_d20_write_string(file, "Background", c->background) ||
       !pocket_d20_write_string(file, "Alignment", c->alignment) ||
       !pocket_d20_write_string(file, "OtherProficiencies", c->other_proficiencies) ||
       !pocket_d20_write_string(file, "OriginFeat", c->origin_feat) ||
       !pocket_d20_write_string(file, "ToolProficiencies", c->tool_proficiencies) ||
       !pocket_d20_write_string(file, "ArmorTraining", c->armor_training) ||
       !pocket_d20_write_string(file, "WeaponTraining", c->weapon_training) ||
       !pocket_d20_write_string(file, "Senses", c->senses) ||
       !pocket_d20_write_string(file, "AdventureCampaign", c->adventure_campaign) ||
       !pocket_d20_write_string(file, "AdventureScene", c->adventure_scene) ||
       !pocket_d20_write_string(file, "AdventureCheckpoint", c->adventure_checkpoint) ||
       !pocket_d20_writef(
           file,
           "AdventureProgress=%lu,%lu\n",
           (unsigned long)c->adventure_quest_flags,
           (unsigned long)c->adventure_achievements) ||
       !pocket_d20_writef(file, "IdentityRules=%u\n", c->size) ||
       !pocket_d20_writef(
           file,
           "Progress=%u,%lu,%u,%u\n",
           c->class_count,
           (unsigned long)c->experience,
           c->milestone_leveling,
           c->inspiration))
        return false;

    for(uint8_t i = 0U; i < c->class_count; ++i) {
        snprintf(key, sizeof(key), "Class%uName", i);
        if(!pocket_d20_write_string(file, key, c->classes[i].name)) return false;
        snprintf(key, sizeof(key), "Class%uSubclass", i);
        if(!pocket_d20_write_string(file, key, c->classes[i].subclass) ||
           !pocket_d20_writef(
               file,
               "Class%uData=%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
               i,
               c->classes[i].level,
               c->classes[i].hit_die,
               c->classes[i].hit_dice_current,
               c->classes[i].hit_dice_max,
               c->classes[i].spellcasting_mode,
               c->classes[i].spellcasting_ability,
               c->classes[i].cantrip_limit,
               c->classes[i].prepared_limit,
               c->classes[i].spellbook_size,
               c->classes[i].pact_slot_level,
               c->classes[i].pact_slots_current,
               c->classes[i].pact_slots_max,
               c->classes[i].mystic_arcanum_mask,
               c->classes[i].spell_points_current,
               c->classes[i].spell_points_max,
               0U))
            return false;
    }

    if(!pocket_d20_write_i8_array(
           file, "AbilityScores", c->ability_scores, POCKET_D20_ABILITY_COUNT) ||
       !pocket_d20_write_u8_array(
           file, "SaveProficiency", c->saving_throw_proficiency, POCKET_D20_ABILITY_COUNT) ||
       !pocket_d20_write_i8_array(
           file, "SaveMisc", c->saving_throw_misc, POCKET_D20_ABILITY_COUNT) ||
       !pocket_d20_write_u8_array(
           file, "SkillProficiency", c->skill_proficiency, POCKET_D20_SKILL_COUNT) ||
       !pocket_d20_write_i8_array(file, "SkillMisc", c->skill_misc, POCKET_D20_SKILL_COUNT) ||
       !pocket_d20_writef(
           file,
           "Vitals=%d,%d,%d,%d,%d,%d,%u,%u,%u,%u,%u,%u\n",
           c->hp_current,
           c->hp_max,
           c->hp_temporary,
           c->armor_class,
           c->speed,
           c->initiative_misc,
           c->exhaustion,
           c->death_successes,
           c->death_failures,
           c->hit_die,
           c->hit_dice_current,
           c->hit_dice_max) ||
       !pocket_d20_writef(
           file,
           "Spellcasting=%u,%d,%d,%u\n",
           c->spellcasting_ability,
           c->spell_attack_misc,
           c->spell_save_misc,
           c->arcane_recovery_used) ||
       !pocket_d20_write_u8_array(
           file, "SpellSlotsCurrent", c->spell_slots_current, POCKET_D20_SLOT_COUNT) ||
       !pocket_d20_write_u8_array(
           file, "SpellSlotsMax", c->spell_slots_max, POCKET_D20_SLOT_COUNT) ||
       !pocket_d20_writef(
           file,
           "Currency=%ld,%ld,%ld,%ld,%ld\n",
           (long)c->currency_cp,
           (long)c->currency_sp,
           (long)c->currency_ep,
           (long)c->currency_gp,
           (long)c->currency_pp) ||
       !pocket_d20_writef(file, "SpellCount=%u\n", c->spell_count))
        return false;

    for(uint8_t i = 0U; i < c->spell_count; ++i) {
        snprintf(key, sizeof(key), "Spell%uName", i);
        if(!pocket_d20_write_string(file, key, c->spells[i].name)) return false;
        snprintf(key, sizeof(key), "Spell%uDetail", i);
        if(!pocket_d20_write_string(file, key, c->spells[i].detail)) return false;
        snprintf(key, sizeof(key), "Spell%uStableId", i);
        if(!pocket_d20_write_string(file, key, c->spells[i].stable_id)) return false;
        snprintf(key, sizeof(key), "Spell%uSource", i);
        if(!pocket_d20_write_string(file, key, c->spells[i].source)) return false;
        snprintf(key, sizeof(key), "Spell%uSchool", i);
        if(!pocket_d20_write_string(file, key, c->spells[i].school)) return false;
        snprintf(key, sizeof(key), "Spell%uGrantName", i);
        if(!pocket_d20_write_string(file, key, c->spells[i].grant_name) ||
           !pocket_d20_writef(
               file,
               "Spell%uData=%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
               i,
               c->spells[i].level,
               c->spells[i].class_index,
               c->spells[i].prepared,
               c->spells[i].ritual,
               c->spell_known[i],
               c->spell_always_prepared[i],
               c->spell_free_casts_current[i],
               c->spell_free_casts_max[i],
               c->spells[i].grant_source))
            return false;
    }

    if(!pocket_d20_writef(file, "FeatureCount=%u\n", c->feature_count)) return false;
    for(uint8_t i = 0U; i < c->feature_count; ++i) {
        snprintf(key, sizeof(key), "Feature%uName", i);
        if(!pocket_d20_write_string(file, key, c->features[i].name)) return false;
        snprintf(key, sizeof(key), "Feature%uDetail", i);
        if(!pocket_d20_write_string(file, key, c->features[i].detail) ||
           !pocket_d20_writef(
               file,
               "Feature%uData=%d,%d,%u,%u,%u,%u,%u\n",
               i,
               c->features[i].uses_current,
               c->features[i].uses_max,
               c->features[i].class_index,
               c->features[i].class_level_gained,
               c->features[i].recharge,
               c->features[i].resource_formula,
               c->features[i].resource_ability))
            return false;
    }

    if(!pocket_d20_writef(file, "ItemCount=%u\n", c->item_count)) return false;
    for(uint8_t i = 0U; i < c->item_count; ++i) {
        const PocketItem* item = &c->items[i];
        snprintf(key, sizeof(key), "Item%uName", i);
        if(!pocket_d20_write_string(file, key, item->name)) return false;
        snprintf(key, sizeof(key), "Item%uDetail", i);
        if(!pocket_d20_write_string(file, key, item->detail)) return false;
        snprintf(key, sizeof(key), "Item%uAmmoGroup", i);
        if(!pocket_d20_write_string(file, key, item->ammunition_group) ||
           !pocket_d20_writef(
               file,
               "Item%uData=%d,%d,%u,%u,%u,%u,%u,%d,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d,%d,%d,%d,%d,%u,%d,%u\n",
               i,
               item->quantity,
               item->weight_tenths,
               item->equipped,
               item->attuned,
               item->is_weapon,
               item->attack_ability,
               item->proficient,
               item->magic_bonus,
               item->damage_dice,
               item->damage_die,
               item->versatile_die,
               item->use_versatile,
               item->damage_type,
               item->add_ability_damage,
               item->extra_dice,
               item->extra_die,
               item->weapon_properties,
               item->ammo_current,
               item->ammo_max,
               item->container_index,
               item->charges_current,
               item->charges_max,
               item->armor_base,
               item->armor_dex_cap,
               item->shield_bonus))
            return false;
    }

    if(!pocket_d20_writef(file, "LanguageCount=%u\n", c->language_count)) return false;
    for(uint8_t i = 0U; i < c->language_count; ++i) {
        snprintf(key, sizeof(key), "Language%u", i);
        if(!pocket_d20_write_string(file, key, c->languages[i])) return false;
    }

    if(!pocket_d20_writef(file, "JournalCount=%u\n", c->journal_count)) return false;
    for(uint8_t i = 0U; i < c->journal_count; ++i) {
        snprintf(key, sizeof(key), "Journal%uTitle", i);
        if(!pocket_d20_write_string(file, key, c->journal[i].title)) return false;
        snprintf(key, sizeof(key), "Journal%uBody", i);
        if(!pocket_d20_write_string(file, key, c->journal[i].body) ||
           !pocket_d20_writef(
               file,
               "Journal%uData=%u,%u,%u,%u\n",
               i,
               c->journal[i].category,
               c->journal[i].completed,
               c->journal[i].level_granted,
               c->journal[i].class_index))
            return false;
    }

    if(!pocket_d20_writef(file, "PartyCount=%u\n", data->party_count)) return false;
    for(uint8_t i = 0U; i < data->party_count; ++i) {
        snprintf(key, sizeof(key), "Party%uName", i);
        if(!pocket_d20_write_string(file, key, data->party[i].name) ||
           !pocket_d20_writef(
               file,
               "Party%uData=%d,%d,%d,%d\n",
               i,
               data->party[i].initiative_modifier,
               data->party[i].hp_current,
               data->party[i].hp_max,
               data->party[i].armor_class))
            return false;
    }

    if(!pocket_d20_writef(
           file,
           "InitiativeState=%u,%u,%u,%u\n",
           data->initiative.active,
           data->initiative.round,
           data->initiative.current_turn,
           data->initiative.count))
        return false;
    for(uint8_t i = 0U; i < data->initiative.count; ++i) {
        snprintf(key, sizeof(key), "Initiative%uName", i);
        if(!pocket_d20_write_string(file, key, data->initiative.entries[i].name)) return false;
        snprintf(key, sizeof(key), "Initiative%uConditions", i);
        if(!pocket_d20_write_string(file, key, data->initiative.entries[i].conditions) ||
           !pocket_d20_writef(
               file,
               "Initiative%uData=%d,%d,%u,%d,%d,%d\n",
               i,
               data->initiative.entries[i].initiative_modifier,
               data->initiative.entries[i].initiative_total,
               data->initiative.entries[i].is_player_character,
               data->initiative.entries[i].hp_current,
               data->initiative.entries[i].hp_max,
               data->initiative.entries[i].armor_class))
            return false;
    }

    if(!pocket_d20_write_string(file, "Conditions", c->conditions) ||
       !pocket_d20_write_string(file, "Concentration", c->concentration) ||
       !pocket_d20_write_string(file, "TemporaryEffects", c->temporary_effects) ||
       !pocket_d20_write_string(file, "Resistances", c->resistances) ||
       !pocket_d20_write_string(file, "Immunities", c->immunities) ||
       !pocket_d20_write_string(file, "Vulnerabilities", c->vulnerabilities) ||
       !pocket_d20_write_string(file, "MovementModes", c->movement_modes) ||
       !pocket_d20_writef(
           file,
           "CombatFlags=%u,%u,%d\n",
           c->reaction_available,
           c->encumbrance_mode,
           c->carrying_capacity_override) ||
       !pocket_d20_writef(file, "GrantCount=%u\n", c->grant_count))
        return false;

    for(uint8_t i = 0U; i < c->grant_count; ++i) {
        const PocketGrant* grant = &c->grants[i];
        snprintf(key, sizeof(key), "Grant%uStableId", i);
        if(!pocket_d20_write_string(file, key, grant->stable_id)) return false;
        snprintf(key, sizeof(key), "Grant%uSource", i);
        if(!pocket_d20_write_string(file, key, grant->source)) return false;
        snprintf(key, sizeof(key), "Grant%uOption", i);
        if(!pocket_d20_write_string(file, key, grant->option_name)) return false;
        snprintf(key, sizeof(key), "Grant%uPrerequisites", i);
        if(!pocket_d20_write_string(file, key, grant->prerequisites)) return false;
        snprintf(key, sizeof(key), "Grant%uValue", i);
        if(!pocket_d20_write_string(file, key, grant->grant_value) ||
           !pocket_d20_writef(
               file,
               "Grant%uData=%u,%u,%u,%u\n",
               i,
               grant->source_type,
               grant->class_index,
               grant->level_gained,
               grant->status))
            return false;
    }

    if(!pocket_d20_writef(file, "AttackTemplateCount=%u\n", c->attack_template_count))
        return false;
    for(uint8_t i = 0U; i < c->attack_template_count; ++i) {
        const PocketAttackTemplate* attack = &c->attack_templates[i];
        snprintf(key, sizeof(key), "AttackTemplate%uName", i);
        if(!pocket_d20_write_string(file, key, attack->name)) return false;
        snprintf(key, sizeof(key), "AttackTemplate%uMastery", i);
        if(!pocket_d20_write_string(file, key, attack->mastery)) return false;
        snprintf(key, sizeof(key), "AttackTemplate%uDamageType", i);
        if(!pocket_d20_write_string(file, key, attack->damage_type)) return false;
        snprintf(key, sizeof(key), "AttackTemplate%uRiderType", i);
        if(!pocket_d20_write_string(file, key, attack->rider_type) ||
           !pocket_d20_writef(
               file,
               "AttackTemplate%uData=%u,%u,%u,%d,%u,%u,%u,%u,%u\n",
               i,
               attack->type | (attack->recharge << 3U),
               attack->ability,
               attack->save_ability,
               attack->attack_misc,
               attack->save_dc,
               attack->damage_dice,
               attack->damage_die,
               attack->rider_dice,
               attack->rider_die))
            return false;
    }

    if(!pocket_d20_writef(file, "EncounterHistoryCount=%u\n", data->encounter_history_count))
        return false;
    for(uint8_t i = 0U; i < data->encounter_history_count; ++i) {
        const PocketEncounterHistory* history = &data->encounter_history[i];
        if(!pocket_d20_writef(
               file,
               "EncounterHistory%u=%u,%u,%u,%u,%d,%d\n",
               i,
               history->round,
               history->current_turn,
               history->kind,
               history->target,
               history->value_before,
               history->value_after))
            return false;
    }

    return pocket_d20_writef(file, "End=OK\n");
}

static bool
    pocket_d20_read_character(File* file, PocketSaveData* data, unsigned long* loaded_schema) {
    PocketD20Reader buffered_reader;
    pocket_d20_reader_init(&buffered_reader, file);
    PocketD20Reader* reader = &buffered_reader;
    PocketCharacter* c = &data->character;
    char key[48];
    char value[64];
    int32_t n[32];
    pocket_d20_data_clear(data);

    if(!pocket_d20_read_value(reader, "PocketD20Character", value, sizeof(value))) return false;
    unsigned long schema = strtoul(value, NULL, 10);
    if(schema < POCKET_D20_OLDEST_SUPPORTED_TEXT_VERSION || !pocket_d20_schema_supported(schema))
        return false;
    if(loaded_schema) *loaded_schema = schema;
    if(!pocket_d20_read_string(reader, "Name", c->name, sizeof(c->name)) ||
       !pocket_d20_read_string(reader, "Player", c->player, sizeof(c->player)) ||
       !pocket_d20_read_string(reader, "Species", c->species, sizeof(c->species)) ||
       !pocket_d20_read_string(reader, "Background", c->background, sizeof(c->background)) ||
       !pocket_d20_read_string(reader, "Alignment", c->alignment, sizeof(c->alignment)) ||
       !pocket_d20_read_string(
           reader, "OtherProficiencies", c->other_proficiencies, sizeof(c->other_proficiencies)) ||
       !pocket_d20_read_string(reader, "OriginFeat", c->origin_feat, sizeof(c->origin_feat)) ||
       !pocket_d20_read_string(
           reader, "ToolProficiencies", c->tool_proficiencies, sizeof(c->tool_proficiencies)) ||
       !pocket_d20_read_string(
           reader, "ArmorTraining", c->armor_training, sizeof(c->armor_training)) ||
       !pocket_d20_read_string(
           reader, "WeaponTraining", c->weapon_training, sizeof(c->weapon_training)) ||
       !pocket_d20_read_string(reader, "Senses", c->senses, sizeof(c->senses)) ||
       !pocket_d20_read_string(
           reader, "AdventureCampaign", c->adventure_campaign, sizeof(c->adventure_campaign)) ||
       !pocket_d20_read_string(
           reader, "AdventureScene", c->adventure_scene, sizeof(c->adventure_scene)) ||
       !pocket_d20_read_string(
           reader,
           "AdventureCheckpoint",
           c->adventure_checkpoint,
           sizeof(c->adventure_checkpoint)) ||
       !pocket_d20_read_numbers(reader, "AdventureProgress", n, 2U))
        return false;
    c->adventure_quest_flags = (uint32_t)n[0];
    c->adventure_achievements = (uint32_t)n[1];
    if(!pocket_d20_read_numbers(reader, "IdentityRules", n, 1U)) return false;
    c->size = (uint8_t)n[0];
    if(!pocket_d20_read_numbers(reader, "Progress", n, 4U)) return false;
    c->class_count = (uint8_t)n[0];
    c->experience = (uint32_t)n[1];
    c->milestone_leveling = (uint8_t)n[2];
    c->inspiration = (uint8_t)n[3];
    if(c->class_count == 0U || c->class_count > POCKET_D20_MAX_CLASSES) return false;

    for(uint8_t i = 0U; i < c->class_count; ++i) {
        snprintf(key, sizeof(key), "Class%uName", i);
        if(!pocket_d20_read_string(reader, key, c->classes[i].name, sizeof(c->classes[i].name)))
            return false;
        snprintf(key, sizeof(key), "Class%uSubclass", i);
        if(!pocket_d20_read_string(
               reader, key, c->classes[i].subclass, sizeof(c->classes[i].subclass)))
            return false;
        snprintf(key, sizeof(key), "Class%uData", i);
        if(!pocket_d20_read_numbers(reader, key, n, 16U)) return false;
        c->classes[i].level = (uint8_t)n[0];
        c->classes[i].hit_die = (uint8_t)n[1];
        c->classes[i].hit_dice_current = (uint8_t)n[2];
        c->classes[i].hit_dice_max = (uint8_t)n[3];
        c->classes[i].spellcasting_mode = (uint8_t)n[4];
        c->classes[i].spellcasting_ability = (uint8_t)n[5];
        c->classes[i].cantrip_limit = (uint8_t)n[6];
        c->classes[i].prepared_limit = (uint8_t)n[7];
        c->classes[i].spellbook_size = (uint16_t)n[8];
        c->classes[i].pact_slot_level = (uint8_t)n[9];
        c->classes[i].pact_slots_current = (uint8_t)n[10];
        c->classes[i].pact_slots_max = (uint8_t)n[11];
        c->classes[i].mystic_arcanum_mask = (uint16_t)n[12];
        c->classes[i].spell_points_current = (uint16_t)n[13];
        c->classes[i].spell_points_max = (uint16_t)n[14];
    }

    if(!pocket_d20_read_numbers(reader, "AbilityScores", n, POCKET_D20_ABILITY_COUNT))
        return false;
    for(uint8_t i = 0U; i < POCKET_D20_ABILITY_COUNT; ++i)
        c->ability_scores[i] = (int8_t)n[i];
    if(!pocket_d20_read_numbers(reader, "SaveProficiency", n, POCKET_D20_ABILITY_COUNT))
        return false;
    for(uint8_t i = 0U; i < POCKET_D20_ABILITY_COUNT; ++i)
        c->saving_throw_proficiency[i] = (uint8_t)n[i];
    if(!pocket_d20_read_numbers(reader, "SaveMisc", n, POCKET_D20_ABILITY_COUNT)) return false;
    for(uint8_t i = 0U; i < POCKET_D20_ABILITY_COUNT; ++i)
        c->saving_throw_misc[i] = (int8_t)n[i];
    if(!pocket_d20_read_numbers(reader, "SkillProficiency", n, POCKET_D20_SKILL_COUNT))
        return false;
    for(uint8_t i = 0U; i < POCKET_D20_SKILL_COUNT; ++i)
        c->skill_proficiency[i] = (uint8_t)n[i];
    if(!pocket_d20_read_numbers(reader, "SkillMisc", n, POCKET_D20_SKILL_COUNT)) return false;
    for(uint8_t i = 0U; i < POCKET_D20_SKILL_COUNT; ++i)
        c->skill_misc[i] = (int8_t)n[i];

    if(!pocket_d20_read_numbers(reader, "Vitals", n, 12U)) return false;
    c->hp_current = (int16_t)n[0];
    c->hp_max = (int16_t)n[1];
    c->hp_temporary = (int16_t)n[2];
    c->armor_class = (int16_t)n[3];
    c->speed = (int16_t)n[4];
    c->initiative_misc = (int8_t)n[5];
    c->exhaustion = (uint8_t)n[6];
    c->death_successes = (uint8_t)n[7];
    c->death_failures = (uint8_t)n[8];
    c->hit_die = (uint8_t)n[9];
    c->hit_dice_current = (uint8_t)n[10];
    c->hit_dice_max = (uint8_t)n[11];
    if(!pocket_d20_read_numbers(reader, "Spellcasting", n, 4U)) return false;
    c->spellcasting_ability = (uint8_t)n[0];
    c->spell_attack_misc = (int8_t)n[1];
    c->spell_save_misc = (int8_t)n[2];
    c->arcane_recovery_used = (uint8_t)n[3];
    if(!pocket_d20_read_numbers(reader, "SpellSlotsCurrent", n, POCKET_D20_SLOT_COUNT))
        return false;
    for(uint8_t i = 0U; i < POCKET_D20_SLOT_COUNT; ++i)
        c->spell_slots_current[i] = (uint8_t)n[i];
    if(!pocket_d20_read_numbers(reader, "SpellSlotsMax", n, POCKET_D20_SLOT_COUNT)) return false;
    for(uint8_t i = 0U; i < POCKET_D20_SLOT_COUNT; ++i)
        c->spell_slots_max[i] = (uint8_t)n[i];
    if(!pocket_d20_read_numbers(reader, "Currency", n, 5U)) return false;
    c->currency_cp = n[0];
    c->currency_sp = n[1];
    c->currency_ep = n[2];
    c->currency_gp = n[3];
    c->currency_pp = n[4];

    if(!pocket_d20_read_value(reader, "SpellCount", value, sizeof(value))) return false;
    c->spell_count = (uint8_t)strtoul(value, NULL, 10);
    if(c->spell_count > POCKET_D20_MAX_SPELLS ||
       !pocket_d20_data_reserve_spells(c, c->spell_count))
        return false;
    for(uint8_t i = 0U; i < c->spell_count; ++i) {
        snprintf(key, sizeof(key), "Spell%uName", i);
        if(!pocket_d20_read_string(reader, key, c->spells[i].name, sizeof(c->spells[i].name)))
            return false;
        snprintf(key, sizeof(key), "Spell%uDetail", i);
        if(!pocket_d20_read_string(reader, key, c->spells[i].detail, sizeof(c->spells[i].detail)))
            return false;
        snprintf(key, sizeof(key), "Spell%uStableId", i);
        if(!pocket_d20_read_string(
               reader, key, c->spells[i].stable_id, sizeof(c->spells[i].stable_id)))
            return false;
        snprintf(key, sizeof(key), "Spell%uSource", i);
        if(!pocket_d20_read_string(reader, key, c->spells[i].source, sizeof(c->spells[i].source)))
            return false;
        snprintf(key, sizeof(key), "Spell%uSchool", i);
        if(!pocket_d20_read_string(reader, key, c->spells[i].school, sizeof(c->spells[i].school)))
            return false;
        snprintf(key, sizeof(key), "Spell%uGrantName", i);
        if(!pocket_d20_read_string(
               reader, key, c->spells[i].grant_name, sizeof(c->spells[i].grant_name)))
            return false;
        snprintf(key, sizeof(key), "Spell%uData", i);
        if(!pocket_d20_read_numbers(reader, key, n, 9U)) return false;
        c->spells[i].level = (uint8_t)n[0];
        c->spells[i].class_index = (uint8_t)n[1];
        c->spells[i].prepared = (uint8_t)n[2];
        c->spells[i].ritual = (uint8_t)n[3];
        c->spell_known[i] = (uint8_t)n[4];
        c->spell_always_prepared[i] = (uint8_t)n[5];
        c->spell_free_casts_current[i] = (uint8_t)n[6];
        c->spell_free_casts_max[i] = (uint8_t)n[7];
        c->spells[i].grant_source = (uint8_t)n[8];
    }

    if(!pocket_d20_read_value(reader, "FeatureCount", value, sizeof(value))) return false;
    c->feature_count = (uint8_t)strtoul(value, NULL, 10);
    if(c->feature_count > POCKET_D20_MAX_FEATURES ||
       !pocket_d20_data_reserve_features(c, c->feature_count))
        return false;
    for(uint8_t i = 0U; i < c->feature_count; ++i) {
        snprintf(key, sizeof(key), "Feature%uName", i);
        if(!pocket_d20_read_string(reader, key, c->features[i].name, sizeof(c->features[i].name)))
            return false;
        snprintf(key, sizeof(key), "Feature%uDetail", i);
        if(!pocket_d20_read_string(
               reader, key, c->features[i].detail, sizeof(c->features[i].detail)))
            return false;
        snprintf(key, sizeof(key), "Feature%uData", i);
        if(!pocket_d20_read_numbers(reader, key, n, 7U)) return false;
        c->features[i].uses_current = (int16_t)n[0];
        c->features[i].uses_max = (int16_t)n[1];
        c->features[i].class_index = (uint8_t)n[2];
        c->features[i].class_level_gained = (uint8_t)n[3];
        c->features[i].recharge = (uint8_t)n[4];
        c->features[i].resource_formula = (uint8_t)n[5];
        c->features[i].resource_ability = (uint8_t)n[6];
    }

    if(!pocket_d20_read_value(reader, "ItemCount", value, sizeof(value))) return false;
    c->item_count = (uint8_t)strtoul(value, NULL, 10);
    if(c->item_count > POCKET_D20_MAX_ITEMS || !pocket_d20_data_reserve_items(c, c->item_count))
        return false;
    for(uint8_t i = 0U; i < c->item_count; ++i) {
        PocketItem* item = &c->items[i];
        snprintf(key, sizeof(key), "Item%uName", i);
        if(!pocket_d20_read_string(reader, key, item->name, sizeof(item->name))) return false;
        snprintf(key, sizeof(key), "Item%uDetail", i);
        if(!pocket_d20_read_string(reader, key, item->detail, sizeof(item->detail))) return false;
        snprintf(key, sizeof(key), "Item%uAmmoGroup", i);
        if(!pocket_d20_read_string(
               reader, key, item->ammunition_group, sizeof(item->ammunition_group)))
            return false;
        snprintf(key, sizeof(key), "Item%uData", i);
        if(!pocket_d20_read_numbers(reader, key, n, 25U)) return false;
        item->quantity = (int16_t)n[0];
        item->weight_tenths = (int16_t)n[1];
        item->equipped = (uint8_t)n[2];
        item->attuned = (uint8_t)n[3];
        item->is_weapon = (uint8_t)n[4];
        item->attack_ability = (uint8_t)n[5];
        item->proficient = (uint8_t)n[6];
        item->magic_bonus = (int8_t)n[7];
        item->damage_dice = (uint8_t)n[8];
        item->damage_die = (uint8_t)n[9];
        item->versatile_die = (uint8_t)n[10];
        item->use_versatile = (uint8_t)n[11];
        item->damage_type = (uint8_t)n[12];
        item->add_ability_damage = (uint8_t)n[13];
        item->extra_dice = (uint8_t)n[14];
        item->extra_die = (uint8_t)n[15];
        item->weapon_properties = (uint16_t)n[16];
        item->ammo_current = (int16_t)n[17];
        item->ammo_max = (int16_t)n[18];
        item->container_index = (int8_t)n[19];
        item->charges_current = (int16_t)n[20];
        item->charges_max = (int16_t)n[21];
        item->armor_base = (uint8_t)n[22];
        item->armor_dex_cap = (int8_t)n[23];
        item->shield_bonus = (uint8_t)n[24];
    }

    if(!pocket_d20_read_value(reader, "LanguageCount", value, sizeof(value))) return false;
    c->language_count = (uint8_t)strtoul(value, NULL, 10);
    if(c->language_count > POCKET_D20_MAX_LANGUAGES) return false;
    for(uint8_t i = 0U; i < c->language_count; ++i) {
        snprintf(key, sizeof(key), "Language%u", i);
        if(!pocket_d20_read_string(reader, key, c->languages[i], sizeof(c->languages[i])))
            return false;
    }

    if(!pocket_d20_read_value(reader, "JournalCount", value, sizeof(value))) return false;
    c->journal_count = (uint8_t)strtoul(value, NULL, 10);
    if(c->journal_count > POCKET_D20_MAX_JOURNAL ||
       !pocket_d20_data_reserve_journal(c, c->journal_count))
        return false;
    for(uint8_t i = 0U; i < c->journal_count; ++i) {
        snprintf(key, sizeof(key), "Journal%uTitle", i);
        if(!pocket_d20_read_string(reader, key, c->journal[i].title, sizeof(c->journal[i].title)))
            return false;
        snprintf(key, sizeof(key), "Journal%uBody", i);
        if(!pocket_d20_read_string(reader, key, c->journal[i].body, sizeof(c->journal[i].body)))
            return false;
        snprintf(key, sizeof(key), "Journal%uData", i);
        if(!pocket_d20_read_numbers(reader, key, n, 4U)) return false;
        c->journal[i].category = (uint8_t)n[0];
        c->journal[i].completed = (uint8_t)n[1];
        c->journal[i].level_granted = (uint8_t)n[2];
        c->journal[i].class_index = (uint8_t)n[3];
    }

    if(!pocket_d20_read_value(reader, "PartyCount", value, sizeof(value))) return false;
    data->party_count = (uint8_t)strtoul(value, NULL, 10);
    if(data->party_count > POCKET_D20_MAX_PARTY) return false;
    for(uint8_t i = 0U; i < data->party_count; ++i) {
        snprintf(key, sizeof(key), "Party%uName", i);
        if(!pocket_d20_read_string(reader, key, data->party[i].name, sizeof(data->party[i].name)))
            return false;
        snprintf(key, sizeof(key), "Party%uData", i);
        if(!pocket_d20_read_numbers(reader, key, n, 4U)) return false;
        data->party[i].initiative_modifier = (int8_t)n[0];
        data->party[i].hp_current = (int16_t)n[1];
        data->party[i].hp_max = (int16_t)n[2];
        data->party[i].armor_class = (int16_t)n[3];
    }

    if(!pocket_d20_read_numbers(reader, "InitiativeState", n, 4U)) return false;
    data->initiative.active = (uint8_t)n[0];
    data->initiative.round = (uint16_t)n[1];
    data->initiative.current_turn = (uint8_t)n[2];
    data->initiative.count = (uint8_t)n[3];
    if(data->initiative.count > POCKET_D20_MAX_INITIATIVE) return false;
    for(uint8_t i = 0U; i < data->initiative.count; ++i) {
        snprintf(key, sizeof(key), "Initiative%uName", i);
        if(!pocket_d20_read_string(
               reader,
               key,
               data->initiative.entries[i].name,
               sizeof(data->initiative.entries[i].name)))
            return false;
        snprintf(key, sizeof(key), "Initiative%uConditions", i);
        if(!pocket_d20_read_string(
               reader,
               key,
               data->initiative.entries[i].conditions,
               sizeof(data->initiative.entries[i].conditions)))
            return false;
        snprintf(key, sizeof(key), "Initiative%uData", i);
        if(!pocket_d20_read_numbers(reader, key, n, 6U)) return false;
        data->initiative.entries[i].initiative_modifier = (int8_t)n[0];
        data->initiative.entries[i].initiative_total = (int16_t)n[1];
        data->initiative.entries[i].is_player_character = (uint8_t)n[2];
        data->initiative.entries[i].hp_current = (int16_t)n[3];
        data->initiative.entries[i].hp_max = (int16_t)n[4];
        data->initiative.entries[i].armor_class = (int16_t)n[5];
    }

    if(!pocket_d20_read_string(reader, "Conditions", c->conditions, sizeof(c->conditions)) ||
       !pocket_d20_read_string(
           reader, "Concentration", c->concentration, sizeof(c->concentration)) ||
       !pocket_d20_read_string(
           reader, "TemporaryEffects", c->temporary_effects, sizeof(c->temporary_effects)) ||
       !pocket_d20_read_string(reader, "Resistances", c->resistances, sizeof(c->resistances)) ||
       !pocket_d20_read_string(reader, "Immunities", c->immunities, sizeof(c->immunities)) ||
       !pocket_d20_read_string(
           reader, "Vulnerabilities", c->vulnerabilities, sizeof(c->vulnerabilities)) ||
       !pocket_d20_read_string(
           reader, "MovementModes", c->movement_modes, sizeof(c->movement_modes)) ||
       !pocket_d20_read_numbers(reader, "CombatFlags", n, 3U))
        return false;
    c->reaction_available = (uint8_t)n[0];
    c->encumbrance_mode = (uint8_t)n[1];
    c->carrying_capacity_override = (int16_t)n[2];

    if(!pocket_d20_read_value(reader, "GrantCount", value, sizeof(value))) return false;
    c->grant_count = (uint8_t)strtoul(value, NULL, 10);
    if(c->grant_count > POCKET_D20_MAX_GRANTS ||
       !pocket_d20_data_reserve_grants(c, c->grant_count))
        return false;
    for(uint8_t i = 0U; i < c->grant_count; ++i) {
        PocketGrant* grant = &c->grants[i];
        snprintf(key, sizeof(key), "Grant%uStableId", i);
        if(!pocket_d20_read_string(reader, key, grant->stable_id, sizeof(grant->stable_id)))
            return false;
        snprintf(key, sizeof(key), "Grant%uSource", i);
        if(!pocket_d20_read_string(reader, key, grant->source, sizeof(grant->source)))
            return false;
        snprintf(key, sizeof(key), "Grant%uOption", i);
        if(!pocket_d20_read_string(reader, key, grant->option_name, sizeof(grant->option_name)))
            return false;
        snprintf(key, sizeof(key), "Grant%uPrerequisites", i);
        if(!pocket_d20_read_string(reader, key, grant->prerequisites, sizeof(grant->prerequisites)))
            return false;
        snprintf(key, sizeof(key), "Grant%uValue", i);
        if(!pocket_d20_read_string(reader, key, grant->grant_value, sizeof(grant->grant_value)))
            return false;
        snprintf(key, sizeof(key), "Grant%uData", i);
        if(!pocket_d20_read_numbers(reader, key, n, 4U)) return false;
        grant->source_type = (uint8_t)n[0];
        grant->class_index = (uint8_t)n[1];
        grant->level_gained = (uint8_t)n[2];
        grant->status = (uint8_t)n[3];
    }

    if(!pocket_d20_read_value(reader, "AttackTemplateCount", value, sizeof(value))) return false;
    c->attack_template_count = (uint8_t)strtoul(value, NULL, 10);
    if(c->attack_template_count > POCKET_D20_MAX_ATTACK_TEMPLATES) return false;
    for(uint8_t i = 0U; i < c->attack_template_count; ++i) {
        PocketAttackTemplate* attack = &c->attack_templates[i];
        snprintf(key, sizeof(key), "AttackTemplate%uName", i);
        if(!pocket_d20_read_string(reader, key, attack->name, sizeof(attack->name))) return false;
        snprintf(key, sizeof(key), "AttackTemplate%uMastery", i);
        if(!pocket_d20_read_string(reader, key, attack->mastery, sizeof(attack->mastery)))
            return false;
        snprintf(key, sizeof(key), "AttackTemplate%uDamageType", i);
        if(!pocket_d20_read_string(reader, key, attack->damage_type, sizeof(attack->damage_type)))
            return false;
        snprintf(key, sizeof(key), "AttackTemplate%uRiderType", i);
        if(!pocket_d20_read_string(reader, key, attack->rider_type, sizeof(attack->rider_type)))
            return false;
        snprintf(key, sizeof(key), "AttackTemplate%uData", i);
        if(!pocket_d20_read_numbers(reader, key, n, 9U)) return false;
        attack->type = (uint8_t)n[0] & 0x07U;
        attack->recharge = ((uint8_t)n[0] >> 3U);
        attack->ability = (uint8_t)n[1];
        attack->save_ability = (uint8_t)n[2];
        attack->attack_misc = (int8_t)n[3];
        attack->save_dc = (uint8_t)n[4];
        attack->damage_dice = (uint8_t)n[5];
        attack->damage_die = (uint8_t)n[6];
        attack->rider_dice = (uint8_t)n[7];
        attack->rider_die = (uint8_t)n[8];
    }

    if(!pocket_d20_read_value(reader, "EncounterHistoryCount", value, sizeof(value))) return false;
    data->encounter_history_count = (uint8_t)strtoul(value, NULL, 10);
    if(data->encounter_history_count > POCKET_D20_MAX_ENCOUNTER_HISTORY) return false;
    for(uint8_t i = 0U; i < data->encounter_history_count; ++i) {
        snprintf(key, sizeof(key), "EncounterHistory%u", i);
        if(!pocket_d20_read_numbers(reader, key, n, 6U)) return false;
        data->encounter_history[i].round = (uint16_t)n[0];
        data->encounter_history[i].current_turn = (uint8_t)n[1];
        data->encounter_history[i].kind = (uint8_t)n[2];
        data->encounter_history[i].target = (uint8_t)n[3];
        data->encounter_history[i].value_before = (int16_t)n[4];
        data->encounter_history[i].value_after = (int16_t)n[5];
    }

    if(!pocket_d20_read_value(reader, "End", value, sizeof(value)) || strcmp(value, "OK") != 0)
        return false;
    pocket_d20_data_sanitize(data);
    return true;
}

static bool pocket_d20_load_text_path(
    Storage* storage,
    const char* path,
    PocketSaveData* data,
    unsigned long* loaded_schema) {
    File* file = storage_file_alloc(storage);
    bool success = storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING) &&
                   pocket_d20_read_character(file, data, loaded_schema);
    storage_file_close(file);
    storage_file_free(file);
    return success;
}

static void pocket_d20_work_path(char* output, size_t size, uint32_t profile, const char* kind) {
    snprintf(
        output, size, "%s/custom_%s_%lu.tmp", POCKET_D20_DATA_DIR, kind, (unsigned long)profile);
}

static void
    pocket_d20_migration_path(char* output, size_t size, uint32_t profile, const char* suffix) {
    snprintf(
        output,
        size,
        "%s/custom_migration_%lu.%s",
        POCKET_D20_DATA_DIR,
        (unsigned long)profile,
        suffix);
}

static bool pocket_d20_parse_profile_filename(const char* filename, PocketProfileEntry* entry) {
    size_t length = strlen(filename);
    if(length < 11U || strncmp(filename, "ch_", 3U) != 0 ||
       strcmp(filename + length - 4U, ".txt") != 0)
        return false;
    char* id_end = NULL;
    unsigned long id = strtoul(filename + 3U, &id_end, 10);
    if(!id_end || *id_end != '_' || id > UINT32_MAX) return false;
    const char* level_separator = filename + length - 5U;
    while(level_separator > id_end && *level_separator != '_')
        --level_separator;
    if(*level_separator != '_' || level_separator <= id_end + 1U) return false;
    char* level_end = NULL;
    unsigned long level = strtoul(level_separator + 1U, &level_end, 10);
    if(!level_end || strcmp(level_end, ".txt") != 0 || level > 255U) return false;
    entry->id = (uint32_t)id;
    entry->level = (uint8_t)level;
    size_t name_length = (size_t)(level_separator - (id_end + 1U));
    if(name_length >= sizeof(entry->name)) name_length = sizeof(entry->name) - 1U;
    memcpy(entry->name, id_end + 1U, name_length);
    entry->name[name_length] = '\0';
    for(size_t i = 0U; entry->name[i]; ++i)
        if(entry->name[i] == '_') entry->name[i] = ' ';
    return true;
}

static bool
    pocket_d20_find_profile_path(Storage* storage, uint32_t profile, char* output, size_t size) {
    File* directory = storage_file_alloc(storage);
    if(!storage_dir_open(directory, POCKET_D20_DATA_DIR)) {
        storage_file_free(directory);
        return false;
    }
    FileInfo info;
    char filename[128];
    bool found = false;
    while(storage_dir_read(directory, &info, filename, sizeof(filename))) {
        PocketProfileEntry entry;
        if(!file_info_is_dir(&info) && pocket_d20_parse_profile_filename(filename, &entry) &&
           entry.id == profile) {
            size_t prefix_length = strlen(POCKET_D20_DATA_DIR);
            size_t filename_length = strlen(filename);
            if(prefix_length + filename_length + 2U > size) continue;
            memcpy(output, POCKET_D20_DATA_DIR, prefix_length);
            output[prefix_length] = '/';
            memcpy(output + prefix_length + 1U, filename, filename_length + 1U);
            found = true;
            break;
        }
    }
    storage_dir_close(directory);
    storage_file_free(directory);
    return found;
}

static bool pocket_d20_storage_save_profile_internal(
    Storage* storage,
    uint32_t profile,
    const PocketSaveData* data,
    const char* known_old_path) {
    furi_assert(storage);
    furi_assert(data);
    storage_common_mkdir(storage, POCKET_D20_DATA_DIR);
    char old_path[128] = {0};
    char new_path[128];
    char temp_path[96];
    char backup_path[96];
    bool had_save = false;
    if(known_old_path && storage_file_exists(storage, known_old_path)) {
        pocket_d20_copy(old_path, sizeof(old_path), known_old_path);
        had_save = true;
    } else {
        had_save = pocket_d20_find_profile_path(storage, profile, old_path, sizeof(old_path));
    }
    pocket_d20_profile_path(new_path, sizeof(new_path), profile, &data->character);
    pocket_d20_work_path(temp_path, sizeof(temp_path), profile, "write");
    pocket_d20_work_path(backup_path, sizeof(backup_path), profile, "backup");
    File* file = storage_file_alloc(storage);
    bool written = storage_file_open(file, temp_path, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
                   pocket_d20_write_character(file, data) && storage_file_sync(file);
    storage_file_close(file);
    storage_file_free(file);
    if(!written) {
        storage_common_remove(storage, temp_path);
        return false;
    }
    if(had_save) {
        storage_common_remove(storage, backup_path);
        if(storage_common_rename(storage, old_path, backup_path) != FSE_OK) {
            storage_common_remove(storage, temp_path);
            return false;
        }
    }
    if(storage_common_rename(storage, temp_path, new_path) != FSE_OK) {
        if(had_save) storage_common_rename(storage, backup_path, old_path);
        return false;
    }
    return true;
}

bool pocket_d20_storage_save_profile(
    Storage* storage,
    uint32_t profile,
    const PocketSaveData* data) {
    return pocket_d20_storage_save_profile_internal(storage, profile, data, NULL);
}

bool pocket_d20_storage_save_profile_known(
    Storage* storage,
    const PocketProfileEntry* current_entry,
    const PocketSaveData* data) {
    furi_assert(current_entry);
    char safe_name[POCKET_D20_NAME_LEN];
    char current_path[128];
    pocket_d20_filename_name(safe_name, sizeof(safe_name), current_entry->name);
    snprintf(
        current_path,
        sizeof(current_path),
        "%s/ch_%lu_%s_%u.txt",
        POCKET_D20_DATA_DIR,
        (unsigned long)current_entry->id,
        safe_name,
        current_entry->level);
    return pocket_d20_storage_save_profile_internal(
        storage, current_entry->id, data, current_path);
}

static bool pocket_d20_files_match(Storage* storage, const char* left, const char* right) {
    File* left_file = storage_file_alloc(storage);
    File* right_file = storage_file_alloc(storage);
    bool left_open = left_file &&
                     storage_file_open(left_file, left, FSAM_READ, FSOM_OPEN_EXISTING);
    bool right_open = right_file && left_open &&
                      storage_file_open(right_file, right, FSAM_READ, FSOM_OPEN_EXISTING);
    bool equal = left_open && right_open;
    uint8_t left_buffer[256];
    uint8_t right_buffer[256];
    while(equal) {
        size_t left_count = storage_file_read(left_file, left_buffer, sizeof(left_buffer));
        size_t right_count = storage_file_read(right_file, right_buffer, sizeof(right_buffer));
        if(left_count != right_count ||
           (left_count && memcmp(left_buffer, right_buffer, left_count))) {
            equal = false;
            break;
        }
        if(!left_count) break;
    }
    if(left_file) {
        if(left_open && storage_file_get_error(left_file) != FSE_OK) equal = false;
        if(left_open) storage_file_close(left_file);
        storage_file_free(left_file);
    }
    if(right_file) {
        if(right_open && storage_file_get_error(right_file) != FSE_OK) equal = false;
        if(right_open) storage_file_close(right_file);
        storage_file_free(right_file);
    }
    return equal;
}

static bool pocket_d20_prepare_migration_snapshot(
    Storage* storage,
    uint32_t profile,
    const char* primary_path) {
    char snapshot_path[112];
    char previous_path[112];
    char temporary_path[96];
    pocket_d20_migration_path(snapshot_path, sizeof(snapshot_path), profile, "rollback");
    pocket_d20_migration_path(previous_path, sizeof(previous_path), profile, "previous");
    pocket_d20_work_path(temporary_path, sizeof(temporary_path), profile, "migration");
    if(storage_file_exists(storage, snapshot_path) &&
       pocket_d20_files_match(storage, primary_path, snapshot_path))
        return true;

    storage_common_remove(storage, temporary_path);
    File* input = storage_file_alloc(storage);
    File* output = storage_file_alloc(storage);
    bool copied = storage_file_open(input, primary_path, FSAM_READ, FSOM_OPEN_EXISTING) &&
                  storage_file_open(output, temporary_path, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    uint8_t buffer[POCKET_D20_READ_BUFFER];
    while(copied) {
        size_t count = storage_file_read(input, buffer, sizeof(buffer));
        if(!count) break;
        copied = storage_file_write(output, buffer, count) == count;
    }
    if(copied) copied = storage_file_get_error(input) == FSE_OK && storage_file_sync(output);
    storage_file_close(input);
    storage_file_close(output);
    storage_file_free(input);
    storage_file_free(output);
    unsigned long snapshot_schema = 0U;
    PocketSaveData* verify = malloc(sizeof(PocketSaveData));
    if(!verify) {
        storage_common_remove(storage, temporary_path);
        return false;
    }
    pocket_d20_data_set_defaults(verify);
    bool verified = copied &&
                    pocket_d20_load_text_path(storage, temporary_path, verify, &snapshot_schema) &&
                    snapshot_schema < POCKET_D20_TEXT_VERSION;
    pocket_d20_data_clear(verify);
    free(verify);
    if(!verified) {
        storage_common_remove(storage, temporary_path);
        return false;
    }

    bool had_snapshot = storage_file_exists(storage, snapshot_path);
    if(had_snapshot) {
        storage_common_remove(storage, previous_path);
        if(storage_common_rename(storage, snapshot_path, previous_path) != FSE_OK) {
            storage_common_remove(storage, temporary_path);
            return false;
        }
    }
    if(storage_common_rename(storage, temporary_path, snapshot_path) == FSE_OK) return true;
    if(had_snapshot) storage_common_rename(storage, previous_path, snapshot_path);
    storage_common_remove(storage, temporary_path);
    return false;
}

static bool pocket_d20_restore_migration_snapshot(
    Storage* storage,
    uint32_t profile,
    const char* primary_path) {
    char snapshot_path[112];
    char temporary_path[96];
    pocket_d20_migration_path(snapshot_path, sizeof(snapshot_path), profile, "rollback");
    pocket_d20_work_path(temporary_path, sizeof(temporary_path), profile, "restore");
    if(!storage_file_exists(storage, snapshot_path)) return false;
    return pocket_d20_copy_file(storage, snapshot_path, primary_path, temporary_path) &&
           pocket_d20_files_match(storage, snapshot_path, primary_path);
}

static bool pocket_d20_upgrade_profile_schema(
    Storage* storage,
    uint32_t profile,
    const char* primary_path,
    PocketSaveData* data,
    unsigned long loaded_schema) {
    if(loaded_schema == POCKET_D20_TEXT_VERSION) return true;
    if(loaded_schema < POCKET_D20_OLDEST_SUPPORTED_TEXT_VERSION ||
       loaded_schema > POCKET_D20_TEXT_VERSION)
        return false;
    if(!pocket_d20_prepare_migration_snapshot(storage, profile, primary_path)) return false;
    if(!pocket_d20_storage_save_profile_internal(storage, profile, data, primary_path)) {
        pocket_d20_restore_migration_snapshot(storage, profile, primary_path);
        return false;
    }

    char upgraded_path[128];
    unsigned long upgraded_schema = 0U;
    PocketSaveData* verify = malloc(sizeof(PocketSaveData));
    if(!verify) {
        pocket_d20_restore_migration_snapshot(storage, profile, primary_path);
        return false;
    }
    pocket_d20_data_set_defaults(verify);
    bool valid =
        pocket_d20_find_profile_path(storage, profile, upgraded_path, sizeof(upgraded_path)) &&
        pocket_d20_load_text_path(storage, upgraded_path, verify, &upgraded_schema) &&
        upgraded_schema == POCKET_D20_TEXT_VERSION;
    pocket_d20_data_clear(verify);
    free(verify);
    if(valid) return true;

    if(pocket_d20_find_profile_path(storage, profile, upgraded_path, sizeof(upgraded_path)))
        storage_common_remove(storage, upgraded_path);
    pocket_d20_restore_migration_snapshot(storage, profile, primary_path);
    return false;
}

bool pocket_d20_storage_load_profile(
    Storage* storage,
    uint32_t profile,
    PocketSaveData* data,
    bool* recovered_backup) {
    furi_assert(storage);
    furi_assert(data);
    if(recovered_backup) *recovered_backup = false;
    char path[128];
    char backup_path[96];
    if(pocket_d20_find_profile_path(storage, profile, path, sizeof(path))) {
        unsigned long loaded_schema = 0U;
        if(pocket_d20_load_text_path(storage, path, data, &loaded_schema))
            return pocket_d20_upgrade_profile_schema(storage, profile, path, data, loaded_schema);
        pocket_d20_data_clear(data);
        pocket_d20_data_set_defaults(data);
        return false;
    }
    pocket_d20_work_path(backup_path, sizeof(backup_path), profile, "backup");
    if(pocket_d20_load_text_path(storage, backup_path, data, NULL)) {
        if(recovered_backup) *recovered_backup = true;
        return true;
    }
    pocket_d20_data_clear(data);
    pocket_d20_data_set_defaults(data);
    return false;
}

static bool pocket_d20_remove_if_present(Storage* storage, const char* path) {
    return !storage_file_exists(storage, path) || storage_common_remove(storage, path) == FSE_OK;
}

bool pocket_d20_storage_delete_profile(Storage* storage, uint32_t profile) {
    char path[128];
    char temp_path[96];
    char backup_path[96];
    char migration_path[112];
    char previous_path[112];
    bool found = pocket_d20_find_profile_path(storage, profile, path, sizeof(path));
    pocket_d20_work_path(temp_path, sizeof(temp_path), profile, "write");
    pocket_d20_work_path(backup_path, sizeof(backup_path), profile, "backup");
    pocket_d20_migration_path(migration_path, sizeof(migration_path), profile, "rollback");
    pocket_d20_migration_path(previous_path, sizeof(previous_path), profile, "previous");
    return (!found || pocket_d20_remove_if_present(storage, path)) &&
           pocket_d20_remove_if_present(storage, temp_path) &&
           pocket_d20_remove_if_present(storage, backup_path) &&
           pocket_d20_remove_if_present(storage, migration_path) &&
           pocket_d20_remove_if_present(storage, previous_path);
}

static bool pocket_d20_copy_file(
    Storage* storage,
    const char* source,
    const char* destination,
    const char* temporary) {
    File* input = storage_file_alloc(storage);
    File* output = storage_file_alloc(storage);
    bool success = storage_file_open(input, source, FSAM_READ, FSOM_OPEN_EXISTING) &&
                   storage_file_open(output, temporary, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    uint8_t buffer[256];
    while(success) {
        size_t count = storage_file_read(input, buffer, sizeof(buffer));
        if(!count) break;
        success = storage_file_write(output, buffer, count) == count;
    }
    if(success) success = storage_file_get_error(input) == FSE_OK && storage_file_sync(output);
    storage_file_close(input);
    storage_file_close(output);
    storage_file_free(input);
    storage_file_free(output);
    if(!success) {
        storage_common_remove(storage, temporary);
        return false;
    }
    storage_common_remove(storage, destination);
    if(storage_common_rename(storage, temporary, destination) != FSE_OK) {
        storage_common_remove(storage, temporary);
        return false;
    }
    return true;
}

static bool pocket_d20_relocate_file_without_replace(
    Storage* storage,
    const char* source,
    const char* destination) {
    if(storage_file_exists(storage, destination))
        return storage_common_remove(storage, source) == FSE_OK;
    char temporary[224];
    int temporary_length = snprintf(temporary, sizeof(temporary), "%s.migrate.tmp", destination);
    if(temporary_length < 0 || (size_t)temporary_length >= sizeof(temporary)) return false;
    storage_common_remove(storage, temporary);
    File* input = storage_file_alloc(storage);
    File* output = storage_file_alloc(storage);
    bool success = storage_file_open(input, source, FSAM_READ, FSOM_OPEN_EXISTING) &&
                   storage_file_open(output, temporary, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    uint8_t buffer[256];
    while(success) {
        size_t count = storage_file_read(input, buffer, sizeof(buffer));
        if(!count) break;
        success = storage_file_write(output, buffer, count) == count;
    }
    if(success) success = storage_file_get_error(input) == FSE_OK && storage_file_sync(output);
    storage_file_close(input);
    storage_file_close(output);
    storage_file_free(input);
    storage_file_free(output);
    if(!success) {
        storage_common_remove(storage, temporary);
        return false;
    }
    if(storage_file_exists(storage, destination)) {
        storage_common_remove(storage, temporary);
        return storage_common_remove(storage, source) == FSE_OK;
    }
    if(storage_common_rename(storage, temporary, destination) != FSE_OK) {
        storage_common_remove(storage, temporary);
        return false;
    }
    return storage_common_remove(storage, source) == FSE_OK;
}

static bool pocket_d20_migrate_directory(
    Storage* storage,
    const char* legacy_directory,
    const char* data_directory,
    uint8_t depth,
    uint16_t* copied_files) {
    File* directory = storage_file_alloc(storage);
    if(!storage_dir_open(directory, legacy_directory)) {
        storage_file_free(directory);
        return true;
    }
    storage_common_mkdir(storage, data_directory);
    FileInfo info;
    char filename[128];
    bool success = true;
    while(storage_dir_read(directory, &info, filename, sizeof(filename))) {
        if(!filename[0] || !strcmp(filename, ".") || !strcmp(filename, "..") ||
           strchr(filename, '/') || strchr(filename, '\\'))
            continue;
        char source[192];
        char destination[192];
        int source_length = snprintf(source, sizeof(source), "%s/%s", legacy_directory, filename);
        int destination_length =
            snprintf(destination, sizeof(destination), "%s/%s", data_directory, filename);
        if(source_length < 0 || (size_t)source_length >= sizeof(source) ||
           destination_length < 0 || (size_t)destination_length >= sizeof(destination)) {
            success = false;
            continue;
        }
        if(file_info_is_dir(&info)) {
            if(depth >= 2U || !pocket_d20_migrate_directory(
                                  storage, source, destination, depth + 1U, copied_files))
                success = false;
            continue;
        }
        if(depth == 0U) {
            PocketProfileEntry legacy_entry;
            char existing[128];
            if(pocket_d20_parse_profile_filename(filename, &legacy_entry) &&
               pocket_d20_find_profile_path(storage, legacy_entry.id, existing, sizeof(existing))) {
                if(storage_common_remove(storage, source) != FSE_OK) success = false;
                continue;
            }
        }
        bool existed = storage_file_exists(storage, destination);
        if(!pocket_d20_relocate_file_without_replace(storage, source, destination)) {
            success = false;
        } else if(!existed && copied_files && *copied_files < UINT16_MAX) {
            ++*copied_files;
        }
    }
    storage_dir_close(directory);
    storage_file_free(directory);
    if(success && storage_common_remove(storage, legacy_directory) != FSE_OK) success = false;
    return success;
}

bool pocket_d20_storage_migrate_legacy_profiles(Storage* storage, uint16_t* copied_files) {
    furi_assert(storage);
    if(copied_files) *copied_files = 0U;
    storage_common_mkdir(storage, APP_DATA_PATH(""));
    storage_common_mkdir(storage, POCKET_D20_DATA_DIR);
    return pocket_d20_migrate_directory(
        storage, POCKET_D20_LEGACY_DATA_DIR, POCKET_D20_DATA_DIR, 0U, copied_files);
}

bool pocket_d20_storage_duplicate_profile(Storage* storage, uint32_t source, uint32_t destination) {
    char source_path[128];
    if(!pocket_d20_find_profile_path(storage, source, source_path, sizeof(source_path)))
        return false;
    const char* filename = strrchr(source_path, '/');
    filename = filename ? filename + 1U : source_path;
    PocketProfileEntry entry;
    if(!pocket_d20_parse_profile_filename(filename, &entry)) return false;
    char safe_name[POCKET_D20_NAME_LEN];
    pocket_d20_filename_name(safe_name, sizeof(safe_name), entry.name);
    char destination_path[128];
    char temporary[96];
    snprintf(
        destination_path,
        sizeof(destination_path),
        "%s/ch_%lu_%s_%u.txt",
        POCKET_D20_DATA_DIR,
        (unsigned long)destination,
        safe_name,
        entry.level);
    pocket_d20_work_path(temporary, sizeof(temporary), destination, "duplicate");
    return pocket_d20_copy_file(storage, source_path, destination_path, temporary);
}

bool pocket_d20_storage_export_profile(Storage* storage, uint32_t profile) {
    char source_path[128];
    if(!pocket_d20_find_profile_path(storage, profile, source_path, sizeof(source_path)))
        return false;
    storage_common_mkdir(storage, POCKET_D20_EXPORT_DIR);
    const char* filename = strrchr(source_path, '/');
    filename = filename ? filename + 1U : source_path;
    char destination[160];
    char temporary[160];
    snprintf(destination, sizeof(destination), "%s/export_%s", POCKET_D20_EXPORT_DIR, filename);
    snprintf(
        temporary,
        sizeof(temporary),
        "%s/export_%lu.tmp",
        POCKET_D20_EXPORT_DIR,
        (unsigned long)profile);
    return pocket_d20_copy_file(storage, source_path, destination, temporary);
}

bool pocket_d20_storage_archive_profile(Storage* storage, uint32_t profile) {
    char source_path[128];
    if(!pocket_d20_find_profile_path(storage, profile, source_path, sizeof(source_path)))
        return false;
    storage_common_mkdir(storage, POCKET_D20_ARCHIVE_DIR);
    const char* filename = strrchr(source_path, '/');
    filename = filename ? filename + 1U : source_path;
    char destination[160];
    snprintf(destination, sizeof(destination), "%s/%s", POCKET_D20_ARCHIVE_DIR, filename);
    if(storage_file_exists(storage, destination)) return false;
    return storage_common_rename(storage, source_path, destination) == FSE_OK;
}

bool pocket_d20_storage_verify_profile(Storage* storage, uint32_t profile) {
    char path[128];
    PocketSaveData* data = calloc(1U, sizeof(PocketSaveData));
    if(!data) return false;
    bool valid = pocket_d20_find_profile_path(storage, profile, path, sizeof(path)) &&
                 pocket_d20_load_text_path(storage, path, data, NULL);
    pocket_d20_data_clear(data);
    free(data);
    return valid;
}

bool pocket_d20_storage_restore_backup(Storage* storage, uint32_t profile, PocketSaveData* data) {
    char backup_path[96];
    char primary_path[128];
    char rejected_path[96];
    pocket_d20_work_path(backup_path, sizeof(backup_path), profile, "backup");
    if(!pocket_d20_load_text_path(storage, backup_path, data, NULL)) return false;
    bool had_primary =
        pocket_d20_find_profile_path(storage, profile, primary_path, sizeof(primary_path));
    pocket_d20_work_path(rejected_path, sizeof(rejected_path), profile, "rejected");
    storage_common_remove(storage, rejected_path);
    if(had_primary && storage_common_rename(storage, primary_path, rejected_path) != FSE_OK)
        return false;
    bool restored = pocket_d20_storage_save_profile(storage, profile, data);
    if(restored) {
        storage_common_remove(storage, rejected_path);
        return true;
    }
    if(had_primary) storage_common_rename(storage, rejected_path, primary_path);
    return false;
}

bool pocket_d20_storage_rollback_migration(
    Storage* storage,
    uint32_t profile,
    PocketSaveData* data) {
    furi_assert(storage);
    furi_assert(data);
    char snapshot_path[112];
    char primary_path[128];
    char temporary_path[96];
    char forward_path[112];
    pocket_d20_migration_path(snapshot_path, sizeof(snapshot_path), profile, "rollback");
    pocket_d20_migration_path(forward_path, sizeof(forward_path), profile, "forward");
    pocket_d20_work_path(temporary_path, sizeof(temporary_path), profile, "rollback");
    unsigned long schema = 0U;
    PocketSaveData* restored = malloc(sizeof(PocketSaveData));
    if(!restored) return false;
    pocket_d20_data_set_defaults(restored);
    if(!pocket_d20_load_text_path(storage, snapshot_path, restored, &schema) ||
       schema >= POCKET_D20_TEXT_VERSION ||
       !pocket_d20_find_profile_path(storage, profile, primary_path, sizeof(primary_path))) {
        pocket_d20_data_clear(restored);
        free(restored);
        return false;
    }

    storage_common_remove(storage, temporary_path);
    if(!pocket_d20_copy_file(storage, snapshot_path, temporary_path, forward_path)) {
        pocket_d20_data_clear(restored);
        free(restored);
        return false;
    }
    storage_common_remove(storage, forward_path);
    if(storage_common_rename(storage, primary_path, forward_path) != FSE_OK) {
        storage_common_remove(storage, temporary_path);
        pocket_d20_data_clear(restored);
        free(restored);
        return false;
    }
    if(storage_common_rename(storage, temporary_path, primary_path) == FSE_OK) {
        pocket_d20_data_clear(data);
        *data = *restored;
        free(restored);
        return true;
    }
    storage_common_rename(storage, forward_path, primary_path);
    storage_common_remove(storage, temporary_path);
    pocket_d20_data_clear(restored);
    free(restored);
    return false;
}

bool pocket_d20_storage_import_first(Storage* storage, uint32_t destination, PocketSaveData* data) {
    File* directory = storage_file_alloc(storage);
    if(!storage_dir_open(directory, POCKET_D20_EXPORT_DIR)) {
        storage_file_free(directory);
        return false;
    }
    FileInfo info;
    char filename[128];
    bool imported = false;
    while(storage_dir_read(directory, &info, filename, sizeof(filename))) {
        size_t length = strlen(filename);
        if(file_info_is_dir(&info) || length < 5U || strcmp(filename + length - 4U, ".txt") != 0)
            continue;
        char path[192];
        snprintf(path, sizeof(path), "%s/%s", POCKET_D20_EXPORT_DIR, filename);
        if(pocket_d20_load_text_path(storage, path, data, NULL) &&
           pocket_d20_storage_save_profile(storage, destination, data)) {
            imported = true;
            break;
        }
    }
    storage_dir_close(directory);
    storage_file_free(directory);
    return imported;
}

void pocket_d20_profiles_set_defaults(PocketProfileState* profiles) {
    memset(profiles, 0, sizeof(*profiles));
}

void pocket_d20_profiles_free(PocketProfileState* profiles) {
    if(!profiles) return;
    free(profiles->entries);
    pocket_d20_profiles_set_defaults(profiles);
}

static bool
    pocket_d20_profiles_append(PocketProfileState* profiles, const PocketProfileEntry* entry) {
    if(profiles->count == UINT16_MAX) return false;
    if(profiles->count == profiles->capacity) {
        uint16_t next_capacity = profiles->capacity ? (uint16_t)(profiles->capacity * 2U) : 8U;
        if(next_capacity < profiles->capacity || next_capacity > UINT16_MAX / 2U)
            next_capacity = UINT16_MAX;
        PocketProfileEntry* resized =
            realloc(profiles->entries, (size_t)next_capacity * sizeof(PocketProfileEntry));
        if(!resized) return false;
        profiles->entries = resized;
        profiles->capacity = next_capacity;
    }
    profiles->entries[profiles->count++] = *entry;
    return true;
}

bool pocket_d20_profiles_refresh(Storage* storage, PocketProfileState* profiles) {
    furi_assert(storage);
    furi_assert(profiles);
    storage_common_mkdir(storage, POCKET_D20_DATA_DIR);
    profiles->count = 0U;
    profiles->character_file_seen = 0U;
    profiles->scan_succeeded = 0U;
    File* directory = storage_file_alloc(storage);
    if(!directory || !storage_dir_open(directory, POCKET_D20_DATA_DIR)) {
        if(directory) storage_file_free(directory);
        return false;
    }
    FileInfo info;
    char filename[128];
    bool success = true;
    while(storage_dir_read(directory, &info, filename, sizeof(filename))) {
        if(file_info_is_dir(&info)) continue;

        size_t length = strlen(filename);
        bool character_related = (length >= 8U && !strncmp(filename, "ch_", 3U) &&
                                  !strcmp(filename + length - 4U, ".txt")) ||
                                 !strncmp(filename, "custom_backup_", 14U) ||
                                 !strncmp(filename, "custom_migration_", 17U) ||
                                 !strncmp(filename, "custom_rejected_", 16U) ||
                                 !strncmp(filename, "custom_write_", 13U);
        if(character_related) profiles->character_file_seen = 1U;

        PocketProfileEntry entry;
        if(!pocket_d20_parse_profile_filename(filename, &entry)) continue;
        profiles->character_file_seen = 1U;
        if(!pocket_d20_profiles_append(profiles, &entry)) {
            success = false;
            break;
        }
    }
    storage_dir_close(directory);
    storage_file_free(directory);
    profiles->scan_succeeded = success ? 1U : 0U;
    for(uint16_t i = 1U; i < profiles->count; ++i) {
        PocketProfileEntry current = profiles->entries[i];
        uint16_t position = i;
        while(position > 0U && profiles->entries[position - 1U].id > current.id) {
            profiles->entries[position] = profiles->entries[position - 1U];
            --position;
        }
        profiles->entries[position] = current;
    }
    return success;
}

uint32_t pocket_d20_profiles_next_id(const PocketProfileState* profiles) {
    uint32_t maximum = 0U;
    bool any = false;
    for(uint16_t i = 0U; i < profiles->count; ++i) {
        if(!any || profiles->entries[i].id > maximum) maximum = profiles->entries[i].id;
        any = true;
    }
    return any && maximum < UINT32_MAX ? maximum + 1U : any ? UINT32_MAX : 0U;
}

bool pocket_d20_profiles_load(Storage* storage, PocketProfileState* profiles) {
    furi_assert(storage);
    furi_assert(profiles);
    pocket_d20_profiles_set_defaults(profiles);
    char value[64];
    File* file = storage_file_alloc(storage);
    bool metadata_loaded =
        storage_file_open(file, POCKET_D20_ACTIVE_PROFILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING);
    PocketD20Reader metadata_reader;
    pocket_d20_reader_init(&metadata_reader, file);
    metadata_loaded = metadata_loaded &&
                      pocket_d20_read_value(&metadata_reader, "Active", value, sizeof(value));
    if(metadata_loaded) profiles->active_profile = (uint32_t)strtoul(value, NULL, 10);
    storage_file_close(file);
    storage_file_free(file);
    bool scanned = pocket_d20_profiles_refresh(storage, profiles);
    bool active_found = false;
    for(uint16_t i = 0U; i < profiles->count; ++i)
        if(profiles->entries[i].id == profiles->active_profile) active_found = true;
    if(!active_found) {
        char backup_path[96];
        pocket_d20_work_path(backup_path, sizeof(backup_path), profiles->active_profile, "backup");
        active_found = storage_file_exists(storage, backup_path);
    }
    if(!active_found && profiles->count) profiles->active_profile = profiles->entries[0].id;
    return scanned && metadata_loaded;
}

bool pocket_d20_profiles_save(Storage* storage, const PocketProfileState* profiles) {
    furi_assert(storage);
    furi_assert(profiles);
    storage_common_mkdir(storage, POCKET_D20_DATA_DIR);
    File* file = storage_file_alloc(storage);
    bool written =
        storage_file_open(
            file, POCKET_D20_ACTIVE_PROFILE_TEMP_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
        pocket_d20_writef(file, "Active=%lu\n", (unsigned long)profiles->active_profile) &&
        storage_file_sync(file);
    storage_file_close(file);
    storage_file_free(file);
    if(!written) {
        storage_common_remove(storage, POCKET_D20_ACTIVE_PROFILE_TEMP_PATH);
        return false;
    }
    storage_common_remove(storage, POCKET_D20_ACTIVE_PROFILE_PATH);
    return storage_common_rename(
               storage, POCKET_D20_ACTIVE_PROFILE_TEMP_PATH, POCKET_D20_ACTIVE_PROFILE_PATH) ==
           FSE_OK;
}
