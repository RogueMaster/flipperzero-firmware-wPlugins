#include "dnd_data.h"

#include <stdlib.h>
#include <string.h>

static void dnd_data_copy(char* destination, size_t size, const char* source) {
    if(size == 0U) return;
    strncpy(destination, source, size - 1U);
    destination[size - 1U] = '\0';
}

static uint8_t dnd_data_clamp_u8(uint8_t value, uint8_t maximum) {
    return value > maximum ? maximum : value;
}

static int16_t dnd_data_clamp_i16(int16_t value, int16_t minimum, int16_t maximum) {
    if(value < minimum) return minimum;
    if(value > maximum) return maximum;
    return value;
}

static uint8_t dnd_data_next_capacity(uint8_t current, uint8_t required, uint8_t maximum) {
    uint8_t capacity = current ? current : 1U;
    while(capacity < required && capacity < maximum) {
        uint16_t doubled = (uint16_t)capacity * 2U;
        capacity = doubled > maximum ? maximum : (uint8_t)doubled;
    }
    return capacity;
}

static bool dnd_data_resize_records_exact(
    void** records,
    uint8_t* capacity,
    uint8_t required,
    uint8_t maximum,
    size_t record_size) {
    if(required > maximum) return false;
    if(required == *capacity) return true;
    if(required == 0U) {
        free(*records);
        *records = NULL;
        *capacity = 0U;
        return true;
    }
    uint8_t old_capacity = *capacity;
    void* resized = realloc(*records, (size_t)required * record_size);
    if(!resized) return false;
    if(required > old_capacity)
        memset(
            (uint8_t*)resized + (size_t)old_capacity * record_size,
            0,
            (size_t)(required - old_capacity) * record_size);
    *records = resized;
    *capacity = required;
    return true;
}

static bool dnd_data_reserve_records(
    void** records,
    uint8_t* capacity,
    uint8_t required,
    uint8_t maximum,
    size_t record_size) {
    if(required <= *capacity) return true;
    if(required > maximum) return false;
    uint8_t next = dnd_data_next_capacity(*capacity, required, maximum);
    return dnd_data_resize_records_exact(records, capacity, next, maximum, record_size);
}

static bool dnd_data_resize_spell_storage(PocketCharacter* character, uint8_t next) {
    if(next > POCKET_D20_MAX_SPELLS) return false;
    if(next == character->spell_capacity) return true;
    if(next == 0U) {
        free(character->spell_storage);
        character->spell_storage = NULL;
        character->spells = NULL;
        character->spell_known = NULL;
        character->spell_always_prepared = NULL;
        character->spell_free_casts_current = NULL;
        character->spell_free_casts_max = NULL;
        character->spell_capacity = 0U;
        character->spell_count = 0U;
        return true;
    }

    const uint8_t old_capacity = character->spell_capacity;
    uint8_t count = character->spell_count;
    if(count > old_capacity) count = old_capacity;
    if(count > next) count = next;
    const size_t old_spell_bytes = (size_t)old_capacity * sizeof(PocketSpell);
    const size_t new_spell_bytes = (size_t)next * sizeof(PocketSpell);
    const size_t new_bytes = new_spell_bytes + (size_t)next * 4U;
    uint8_t* storage = character->spell_storage;

    if(old_capacity && next < old_capacity) {
        /* Compact state arrays into their smaller offsets before shrinking. If the
         * allocator elects not to release the tail, the compacted allocation is still
         * valid and the logical capacity is reduced immediately. */
        uint8_t* old_known = storage + old_spell_bytes;
        uint8_t* old_always = old_known + old_capacity;
        uint8_t* old_free_current = old_always + old_capacity;
        uint8_t* old_free_max = old_free_current + old_capacity;
        uint8_t* new_known = storage + new_spell_bytes;
        uint8_t* new_always = new_known + next;
        uint8_t* new_free_current = new_always + next;
        uint8_t* new_free_max = new_free_current + next;
        if(count) {
            memmove(new_known, old_known, count);
            memmove(new_always, old_always, count);
            memmove(new_free_current, old_free_current, count);
            memmove(new_free_max, old_free_max, count);
        }
        uint8_t* shrunk = realloc(storage, new_bytes);
        if(shrunk) storage = shrunk;
    } else {
        uint8_t* grown = realloc(storage, new_bytes);
        if(!grown) return false;
        storage = grown;

        /* Growing moves the state-array offsets to the right. Relocate the live
         * arrays from the end backwards before clearing the expanded spell region. */
        if(old_capacity && count) {
            uint8_t* old_known = storage + old_spell_bytes;
            uint8_t* old_always = old_known + old_capacity;
            uint8_t* old_free_current = old_always + old_capacity;
            uint8_t* old_free_max = old_free_current + old_capacity;
            uint8_t* new_known = storage + new_spell_bytes;
            uint8_t* new_always = new_known + next;
            uint8_t* new_free_current = new_always + next;
            uint8_t* new_free_max = new_free_current + next;
            memmove(new_free_max, old_free_max, count);
            memmove(new_free_current, old_free_current, count);
            memmove(new_always, old_always, count);
            memmove(new_known, old_known, count);
        }
    }

    PocketSpell* spells = (PocketSpell*)storage;
    uint8_t* known = storage + new_spell_bytes;
    uint8_t* always_prepared = known + next;
    uint8_t* free_current = always_prepared + next;
    uint8_t* free_max = free_current + next;

    if(next > count) {
        memset(&spells[count], 0, (size_t)(next - count) * sizeof(PocketSpell));
        memset(known + count, 0, next - count);
        memset(always_prepared + count, 0, next - count);
        memset(free_current + count, 0, next - count);
        memset(free_max + count, 0, next - count);
    }

    character->spell_storage = storage;
    character->spells = spells;
    character->spell_known = known;
    character->spell_always_prepared = always_prepared;
    character->spell_free_casts_current = free_current;
    character->spell_free_casts_max = free_max;
    character->spell_capacity = next;
    if(character->spell_count > next) character->spell_count = next;
    return true;
}

bool dnd_data_reserve_spells(PocketCharacter* character, uint8_t required) {
    if(required <= character->spell_capacity) return true;
    if(required > POCKET_D20_MAX_SPELLS) return false;
    uint8_t next =
        dnd_data_next_capacity(character->spell_capacity, required, POCKET_D20_MAX_SPELLS);
    return dnd_data_resize_spell_storage(character, next);
}

void dnd_data_clear_spells(PocketCharacter* character) {
    if(!character) return;
    dnd_data_resize_spell_storage(character, 0U);
}

bool dnd_data_reserve_features(PocketCharacter* character, uint8_t required) {
    return dnd_data_reserve_records(
        (void**)&character->features,
        &character->feature_capacity,
        required,
        POCKET_D20_MAX_FEATURES,
        sizeof(PocketFeature));
}

bool dnd_data_reserve_features_exact(PocketCharacter* character, uint8_t required) {
    return dnd_data_resize_records_exact(
        (void**)&character->features,
        &character->feature_capacity,
        required,
        POCKET_D20_MAX_FEATURES,
        sizeof(PocketFeature));
}

bool dnd_data_reserve_items(PocketCharacter* character, uint8_t required) {
    return dnd_data_reserve_records(
        (void**)&character->items,
        &character->item_capacity,
        required,
        POCKET_D20_MAX_ITEMS,
        sizeof(PocketItem));
}

void dnd_data_clear_items(PocketCharacter* character) {
    if(!character) return;
    dnd_data_resize_records_exact(
        (void**)&character->items,
        &character->item_capacity,
        0U,
        POCKET_D20_MAX_ITEMS,
        sizeof(PocketItem));
    character->item_count = 0U;
}

bool dnd_data_reserve_grants(PocketCharacter* character, uint8_t required) {
    return dnd_data_reserve_records(
        (void**)&character->grants,
        &character->grant_capacity,
        required,
        POCKET_D20_MAX_GRANTS,
        sizeof(PocketGrant));
}

bool dnd_data_reserve_grants_exact(PocketCharacter* character, uint8_t required) {
    return dnd_data_resize_records_exact(
        (void**)&character->grants,
        &character->grant_capacity,
        required,
        POCKET_D20_MAX_GRANTS,
        sizeof(PocketGrant));
}

void dnd_data_clear(PocketSaveData* data) {
    if(!data) return;
    free(data->character.spell_storage);
    free(data->character.features);
    free(data->character.items);
    free(data->character.grants);
    memset(data, 0, sizeof(*data));
}

void dnd_data_set_defaults(PocketSaveData* data) {
    memset(data, 0, sizeof(*data));
    PocketCharacter* character = &data->character;

    dnd_data_copy(character->name, sizeof(character->name), "New Hero");
    dnd_data_copy(character->player, sizeof(character->player), "Player");
    dnd_data_copy(character->species, sizeof(character->species), "Human");
    dnd_data_copy(character->background, sizeof(character->background), "Adventurer");
    dnd_data_copy(character->alignment, sizeof(character->alignment), "True Neutral");
    dnd_data_copy(character->origin_feat, sizeof(character->origin_feat), "None");
    character->size = PocketSizeMedium;
    dnd_data_copy(character->senses, sizeof(character->senses), "Normal vision");
    dnd_data_copy(character->movement_modes, sizeof(character->movement_modes), "Walk 30 ft");
    character->reaction_available = 1U;

    character->class_count = 1U;
    dnd_data_copy(character->classes[0].name, sizeof(character->classes[0].name), "Fighter");
    dnd_data_copy(character->classes[0].subclass, sizeof(character->classes[0].subclass), "None");
    character->classes[0].level = 1U;
    character->classes[0].hit_die = 10U;
    character->classes[0].hit_dice_current = 1U;
    character->classes[0].hit_dice_max = 1U;
    character->classes[0].spellcasting_mode = PocketSpellcastingNone;
    character->classes[0].spellcasting_ability = PocketAbilityIntelligence;
    character->milestone_leveling = 1U;

    /* New characters start from the standard array. Existing profiles are never
       rewritten because this path is used only for a freshly initialized save. */
    character->ability_scores[PocketAbilityStrength] = 15;
    character->ability_scores[PocketAbilityDexterity] = 14;
    character->ability_scores[PocketAbilityConstitution] = 13;
    character->ability_scores[PocketAbilityIntelligence] = 12;
    character->ability_scores[PocketAbilityWisdom] = 10;
    character->ability_scores[PocketAbilityCharisma] = 8;
    character->hp_current = 10;
    character->hp_max = 10;
    character->armor_class = 10;
    character->speed = 30;
    character->hit_die = 10U;
    character->hit_dice_current = 1U;
    character->hit_dice_max = 1U;
    character->spellcasting_ability = PocketAbilityIntelligence;

    character->language_count = 1U;
    dnd_data_copy(character->languages[0], sizeof(character->languages[0]), "Common");

    character->attack_template_count = 3U;
    PocketAttackTemplate* unarmed = &character->attack_templates[0];
    dnd_data_copy(unarmed->name, sizeof(unarmed->name), "Unarmed Strike");
    dnd_data_copy(unarmed->damage_type, sizeof(unarmed->damage_type), "Bludgeoning");
    unarmed->type = PocketAttackTemplateUnarmed;
    unarmed->ability = PocketAbilityStrength;
    unarmed->damage_dice = 1U;
    unarmed->damage_die = 1U;
    PocketAttackTemplate* spell_attack = &character->attack_templates[1];
    dnd_data_copy(spell_attack->name, sizeof(spell_attack->name), "Spell Attack");
    spell_attack->type = PocketAttackTemplateSpellAttack;
    spell_attack->ability = PocketAbilityIntelligence;
    spell_attack->damage_dice = 1U;
    spell_attack->damage_die = 10U;
    PocketAttackTemplate* saving_throw = &character->attack_templates[2];
    dnd_data_copy(saving_throw->name, sizeof(saving_throw->name), "Saving Throw Action");
    saving_throw->type = PocketAttackTemplateSavingThrow;
    saving_throw->save_ability = PocketAbilityDexterity;
    saving_throw->damage_dice = 1U;
    saving_throw->damage_die = 6U;
}

void dnd_data_sanitize(PocketSaveData* data) {
    PocketCharacter* character = &data->character;
    character->name[sizeof(character->name) - 1U] = '\0';
    character->player[sizeof(character->player) - 1U] = '\0';
    character->species[sizeof(character->species) - 1U] = '\0';
    character->background[sizeof(character->background) - 1U] = '\0';
    character->alignment[sizeof(character->alignment) - 1U] = '\0';
    if(!character->alignment[0])
        dnd_data_copy(character->alignment, sizeof(character->alignment), "True Neutral");
    character->other_proficiencies[sizeof(character->other_proficiencies) - 1U] = '\0';
    character->origin_feat[sizeof(character->origin_feat) - 1U] = '\0';
    character->tool_proficiencies[sizeof(character->tool_proficiencies) - 1U] = '\0';
    character->armor_training[sizeof(character->armor_training) - 1U] = '\0';
    character->weapon_training[sizeof(character->weapon_training) - 1U] = '\0';
    character->senses[sizeof(character->senses) - 1U] = '\0';
    character->size = dnd_data_clamp_u8(character->size, PocketSizeCount - 1U);

    character->class_count = dnd_data_clamp_u8(character->class_count, POCKET_D20_MAX_CLASSES);
    if(character->class_count == 0U) character->class_count = 1U;
    for(uint8_t i = 0U; i < POCKET_D20_MAX_CLASSES; ++i) {
        PocketClassLevel* class_level = &character->classes[i];
        class_level->name[sizeof(class_level->name) - 1U] = '\0';
        class_level->subclass[sizeof(class_level->subclass) - 1U] = '\0';
        class_level->level = dnd_data_clamp_u8(class_level->level, 20U);
        if(class_level->hit_die != 6U && class_level->hit_die != 8U &&
           class_level->hit_die != 10U && class_level->hit_die != 12U)
            class_level->hit_die = 8U;
        class_level->hit_dice_max = dnd_data_clamp_u8(class_level->hit_dice_max, 20U);
        class_level->hit_dice_current =
            dnd_data_clamp_u8(class_level->hit_dice_current, class_level->hit_dice_max);
        class_level->spellcasting_mode =
            dnd_data_clamp_u8(class_level->spellcasting_mode, PocketSpellcastingModeCount - 1U);
        class_level->spellcasting_ability =
            dnd_data_clamp_u8(class_level->spellcasting_ability, PocketAbilityCharisma);
        class_level->cantrip_limit = dnd_data_clamp_u8(class_level->cantrip_limit, 30U);
        class_level->prepared_limit = dnd_data_clamp_u8(class_level->prepared_limit, 50U);
        class_level->pact_slot_level = dnd_data_clamp_u8(class_level->pact_slot_level, 5U);
    }
    if(character->classes[0].level == 0U) character->classes[0].level = 1U;

    for(uint8_t i = 0U; i < POCKET_D20_ABILITY_COUNT; ++i) {
        character->ability_scores[i] =
            (int8_t)dnd_data_clamp_i16(character->ability_scores[i], 1, 30);
        character->saving_throw_proficiency[i] =
            dnd_data_clamp_u8(character->saving_throw_proficiency[i], PocketProficiencyProficient);
        character->saving_throw_misc[i] =
            (int8_t)dnd_data_clamp_i16(character->saving_throw_misc[i], -20, 20);
    }
    for(uint8_t i = 0U; i < POCKET_D20_SKILL_COUNT; ++i) {
        character->skill_proficiency[i] =
            dnd_data_clamp_u8(character->skill_proficiency[i], PocketProficiencyExpertise);
        character->skill_misc[i] = (int8_t)dnd_data_clamp_i16(character->skill_misc[i], -20, 20);
    }

    character->hp_max = dnd_data_clamp_i16(character->hp_max, 1, 999);
    character->hp_current = dnd_data_clamp_i16(character->hp_current, 0, 999);
    character->hp_temporary = dnd_data_clamp_i16(character->hp_temporary, 0, 999);
    character->armor_class = dnd_data_clamp_i16(character->armor_class, 0, 99);
    character->speed = dnd_data_clamp_i16(character->speed, 0, 255);
    character->initiative_misc = (int8_t)dnd_data_clamp_i16(character->initiative_misc, -20, 20);
    character->exhaustion = dnd_data_clamp_u8(character->exhaustion, 6U);
    character->death_successes = dnd_data_clamp_u8(character->death_successes, 3U);
    character->death_failures = dnd_data_clamp_u8(character->death_failures, 3U);
    if(character->hit_die != 6U && character->hit_die != 8U && character->hit_die != 10U &&
       character->hit_die != 12U)
        character->hit_die = 8U;
    character->spellcasting_ability =
        dnd_data_clamp_u8(character->spellcasting_ability, PocketAbilityCharisma);
    character->spell_attack_misc =
        (int8_t)dnd_data_clamp_i16(character->spell_attack_misc, -20, 20);
    character->spell_save_misc = (int8_t)dnd_data_clamp_i16(character->spell_save_misc, -20, 20);
    character->arcane_recovery_used = character->arcane_recovery_used ? 1U : 0U;

    /* Dynamic character collections are optional. Sanitize each collection locally
       so an absent allocation cannot dereference NULL or invalidate unrelated fields. */
    if(character->spell_storage && character->spells && character->spell_known &&
       character->spell_always_prepared && character->spell_free_casts_current &&
       character->spell_free_casts_max) {
        uint8_t spell_limit = dnd_data_clamp_u8(character->spell_capacity, POCKET_D20_MAX_SPELLS);
        character->spell_count = dnd_data_clamp_u8(character->spell_count, spell_limit);
    } else {
        character->spell_count = 0U;
    }
    if(character->features) {
        uint8_t feature_limit =
            dnd_data_clamp_u8(character->feature_capacity, POCKET_D20_MAX_FEATURES);
        character->feature_count = dnd_data_clamp_u8(character->feature_count, feature_limit);
    } else {
        character->feature_count = 0U;
    }
    if(character->items) {
        uint8_t item_limit = dnd_data_clamp_u8(character->item_capacity, POCKET_D20_MAX_ITEMS);
        character->item_count = dnd_data_clamp_u8(character->item_count, item_limit);
    } else {
        character->item_count = 0U;
    }
    if(character->grants) {
        uint8_t grant_limit = dnd_data_clamp_u8(character->grant_capacity, POCKET_D20_MAX_GRANTS);
        character->grant_count = dnd_data_clamp_u8(character->grant_count, grant_limit);
    } else {
        character->grant_count = 0U;
    }
    character->language_count =
        dnd_data_clamp_u8(character->language_count, POCKET_D20_MAX_LANGUAGES);

    for(uint8_t i = 0U; i < character->spell_count; ++i) {
        character->spells[i].name[POCKET_D20_SPELL_NAME_LEN - 1U] = '\0';
        character->spells[i].detail[POCKET_D20_DETAIL_LEN - 1U] = '\0';
        character->spells[i].level = dnd_data_clamp_u8(character->spells[i].level, 9U);
        character->spells[i].class_index =
            dnd_data_clamp_u8(character->spells[i].class_index, POCKET_D20_MAX_CLASSES - 1U);
        character->spells[i].prepared = character->spells[i].prepared ? 1U : 0U;
        character->spells[i].ritual = character->spells[i].ritual ? 1U : 0U;
        character->spells[i].stable_id[POCKET_D20_SHORT_LEN - 1U] = '\0';
        character->spells[i].source[POCKET_D20_SHORT_LEN - 1U] = '\0';
        character->spells[i].school[POCKET_D20_SHORT_LEN - 1U] = '\0';
        character->spells[i].grant_name[POCKET_D20_SHORT_LEN - 1U] = '\0';
        character->spells[i].grant_source =
            dnd_data_clamp_u8(character->spells[i].grant_source, PocketGrantSourceCount - 1U);
        character->spell_known[i] = character->spell_known[i] ? 1U : 0U;
        character->spell_always_prepared[i] = character->spell_always_prepared[i] ? 1U : 0U;
        character->spell_free_casts_max[i] =
            dnd_data_clamp_u8(character->spell_free_casts_max[i], 20U);
        character->spell_free_casts_current[i] = dnd_data_clamp_u8(
            character->spell_free_casts_current[i], character->spell_free_casts_max[i]);
    }
    for(uint8_t i = 0U; i < character->feature_count; ++i) {
        character->features[i].name[POCKET_D20_FEATURE_NAME_LEN - 1U] = '\0';
        character->features[i].detail[POCKET_D20_DETAIL_LEN - 1U] = '\0';
        character->features[i].class_index =
            dnd_data_clamp_u8(character->features[i].class_index, POCKET_D20_MAX_CLASSES - 1U);
        character->features[i].class_level_gained =
            dnd_data_clamp_u8(character->features[i].class_level_gained, 20U);
        character->features[i].recharge =
            dnd_data_clamp_u8(character->features[i].recharge, PocketRechargeCount - 1U);
        character->features[i].resource_formula = dnd_data_clamp_u8(
            character->features[i].resource_formula, PocketResourceFormulaCount - 1U);
        character->features[i].resource_ability =
            dnd_data_clamp_u8(character->features[i].resource_ability, PocketAbilityCharisma);
    }
    for(uint8_t i = 0U; i < character->item_count; ++i) {
        PocketItem* item = &character->items[i];
        item->name[POCKET_D20_ITEM_NAME_LEN - 1U] = '\0';
        item->detail[POCKET_D20_DETAIL_LEN - 1U] = '\0';
        item->attack_ability = dnd_data_clamp_u8(item->attack_ability, PocketAttackAbilityBest);
        item->damage_type = dnd_data_clamp_u8(item->damage_type, PocketDamageTypeCount - 1U);
        item->damage_dice = dnd_data_clamp_u8(item->damage_dice, 20U);
        item->extra_dice = dnd_data_clamp_u8(item->extra_dice, 20U);
        item->container_index =
            (int8_t)dnd_data_clamp_i16(item->container_index, -1, POCKET_D20_MAX_ITEMS - 1U);
        item->charges_current = dnd_data_clamp_i16(item->charges_current, 0, 999);
        item->charges_max = dnd_data_clamp_i16(item->charges_max, 0, 999);
        item->armor_dex_cap = (int8_t)dnd_data_clamp_i16(item->armor_dex_cap, -1, 9);
        item->ammunition_group[POCKET_D20_SHORT_LEN - 1U] = '\0';
    }
    for(uint8_t i = 0U; i < POCKET_D20_MAX_LANGUAGES; ++i) {
        character->languages[i][POCKET_D20_SHORT_LEN - 1U] = '\0';
    }
    character->conditions[POCKET_D20_DETAIL_LEN - 1U] = '\0';
    character->concentration[POCKET_D20_NAME_LEN - 1U] = '\0';
    character->temporary_effects[POCKET_D20_DETAIL_LEN - 1U] = '\0';
    character->resistances[POCKET_D20_DETAIL_LEN - 1U] = '\0';
    character->immunities[POCKET_D20_DETAIL_LEN - 1U] = '\0';
    character->vulnerabilities[POCKET_D20_DETAIL_LEN - 1U] = '\0';
    character->movement_modes[POCKET_D20_DETAIL_LEN - 1U] = '\0';
    character->reaction_available = character->reaction_available ? 1U : 0U;
    for(uint8_t i = 0U; i < character->grant_count; ++i) {
        PocketGrant* grant = &character->grants[i];
        grant->stable_id[POCKET_D20_SHORT_LEN - 1U] = '\0';
        grant->source[POCKET_D20_SHORT_LEN - 1U] = '\0';
        grant->option_name[POCKET_D20_NAME_LEN - 1U] = '\0';
        grant->prerequisites[POCKET_D20_NAME_LEN - 1U] = '\0';
        grant->grant_value[POCKET_D20_NAME_LEN - 1U] = '\0';
        grant->source_type = dnd_data_clamp_u8(grant->source_type, PocketGrantSourceCount - 1U);
        grant->class_index = dnd_data_clamp_u8(grant->class_index, POCKET_D20_MAX_CLASSES - 1U);
        grant->level_gained = dnd_data_clamp_u8(grant->level_gained, 20U);
        grant->status = dnd_data_clamp_u8(grant->status, PocketGrantSkipped);
    }
    character->attack_template_count =
        dnd_data_clamp_u8(character->attack_template_count, POCKET_D20_MAX_ATTACK_TEMPLATES);
    for(uint8_t i = 0U; i < character->attack_template_count; ++i) {
        PocketAttackTemplate* attack = &character->attack_templates[i];
        attack->name[POCKET_D20_NAME_LEN - 1U] = '\0';
        attack->mastery[POCKET_D20_SHORT_LEN - 1U] = '\0';
        attack->damage_type[POCKET_D20_SHORT_LEN - 1U] = '\0';
        attack->rider_type[POCKET_D20_SHORT_LEN - 1U] = '\0';
        attack->type = dnd_data_clamp_u8(attack->type, PocketAttackTemplateTypeCount - 1U);
        attack->ability = dnd_data_clamp_u8(attack->ability, PocketAbilityCharisma);
        attack->save_ability = dnd_data_clamp_u8(attack->save_ability, PocketAbilityCharisma);
        attack->damage_dice = dnd_data_clamp_u8(attack->damage_dice, 20U);
        attack->rider_dice = dnd_data_clamp_u8(attack->rider_dice, 20U);
        attack->recharge = dnd_data_clamp_u8(attack->recharge, PocketRechargeCount - 1U);
    }
}
