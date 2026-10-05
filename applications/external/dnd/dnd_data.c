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

static bool dnd_data_resize_spell_storage(DndCharacter* character, uint8_t next) {
    if(next > DND_RESIDENT_RECORD_LIMIT) return false;
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
    const size_t old_spell_bytes = (size_t)old_capacity * sizeof(DndSpell);
    const size_t new_spell_bytes = (size_t)next * sizeof(DndSpell);
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

    DndSpell* spells = (DndSpell*)storage;
    uint8_t* known = storage + new_spell_bytes;
    uint8_t* always_prepared = known + next;
    uint8_t* free_current = always_prepared + next;
    uint8_t* free_max = free_current + next;

    if(next > count) {
        memset(&spells[count], 0, (size_t)(next - count) * sizeof(DndSpell));
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

bool dnd_data_reserve_spells(DndCharacter* character, uint8_t required) {
    if(required <= character->spell_capacity) return true;
    if(required > DND_RESIDENT_RECORD_LIMIT) return false;
    uint8_t next =
        dnd_data_next_capacity(character->spell_capacity, required, DND_RESIDENT_RECORD_LIMIT);
    return dnd_data_resize_spell_storage(character, next);
}

void dnd_data_clear_spells(DndCharacter* character) {
    if(!character) return;
    dnd_data_resize_spell_storage(character, 0U);
}

bool dnd_data_reserve_features(DndCharacter* character, uint8_t required) {
    return dnd_data_reserve_records(
        (void**)&character->features,
        &character->feature_capacity,
        required,
        DND_RESIDENT_RECORD_LIMIT,
        sizeof(DndFeature));
}

bool dnd_data_reserve_features_exact(DndCharacter* character, uint8_t required) {
    return dnd_data_resize_records_exact(
        (void**)&character->features,
        &character->feature_capacity,
        required,
        DND_RESIDENT_RECORD_LIMIT,
        sizeof(DndFeature));
}

bool dnd_data_reserve_items(DndCharacter* character, uint8_t required) {
    return dnd_data_reserve_records(
        (void**)&character->items,
        &character->item_capacity,
        required,
        DND_RESIDENT_RECORD_LIMIT,
        sizeof(DndItem));
}

void dnd_data_clear_items(DndCharacter* character) {
    if(!character) return;
    dnd_data_resize_records_exact(
        (void**)&character->items,
        &character->item_capacity,
        0U,
        DND_RESIDENT_RECORD_LIMIT,
        sizeof(DndItem));
    character->item_count = 0U;
}

bool dnd_data_reserve_grants(DndCharacter* character, uint8_t required) {
    return dnd_data_reserve_records(
        (void**)&character->grants,
        &character->grant_capacity,
        required,
        DND_MAX_GRANTS,
        sizeof(DndGrant));
}

bool dnd_data_reserve_grants_exact(DndCharacter* character, uint8_t required) {
    return dnd_data_resize_records_exact(
        (void**)&character->grants,
        &character->grant_capacity,
        required,
        DND_MAX_GRANTS,
        sizeof(DndGrant));
}

void dnd_data_clear(DndSaveData* data) {
    if(!data) return;
    free(data->character.spell_storage);
    free(data->character.features);
    free(data->character.items);
    free(data->character.grants);
    memset(data, 0, sizeof(*data));
}

void dnd_data_set_defaults(DndSaveData* data) {
    memset(data, 0, sizeof(*data));
    DndCharacter* character = &data->character;

    dnd_data_copy(character->name, sizeof(character->name), "New Hero");
    dnd_data_copy(character->player, sizeof(character->player), "Player");
    dnd_data_copy(character->species, sizeof(character->species), "Human");
    dnd_data_copy(character->background, sizeof(character->background), "Adventurer");
    dnd_data_copy(character->alignment, sizeof(character->alignment), "True Neutral");
    dnd_data_copy(character->origin_feat, sizeof(character->origin_feat), "None");
    character->size = DndSizeMedium;
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
    character->classes[0].spellcasting_mode = DndSpellcastingNone;
    character->classes[0].spellcasting_ability = DndAbilityIntelligence;
    character->milestone_leveling = 1U;

    /* New characters start from the standard array. Existing profiles are never
       rewritten because this path is used only for a freshly initialized save. */
    character->ability_scores[DndAbilityStrength] = 15;
    character->ability_scores[DndAbilityDexterity] = 14;
    character->ability_scores[DndAbilityConstitution] = 13;
    character->ability_scores[DndAbilityIntelligence] = 12;
    character->ability_scores[DndAbilityWisdom] = 10;
    character->ability_scores[DndAbilityCharisma] = 8;
    character->hp_current = 10;
    character->hp_max = 10;
    character->armor_class = 10;
    character->speed = 30;
    character->hit_die = 10U;
    character->hit_dice_current = 1U;
    character->hit_dice_max = 1U;
    character->spellcasting_ability = DndAbilityIntelligence;
    /* Saving-throw proficiencies are class grants. Fresh characters leave them
       unset until Grant Initial Traits presents each proficiency for approval. */

    character->attack_template_count = 5U;
    DndAttackTemplate* unarmed = &character->attack_templates[0];
    dnd_data_copy(unarmed->name, sizeof(unarmed->name), "Unarmed Strike");
    dnd_data_copy(unarmed->damage_type, sizeof(unarmed->damage_type), "Bludgeoning");
    unarmed->type = DndAttackTemplateUnarmed;
    unarmed->ability = DndAbilityStrength;
    unarmed->damage_dice = 1U;
    unarmed->damage_die = 1U;

    DndAttackTemplate* grapple = &character->attack_templates[1];
    dnd_data_copy(grapple->name, sizeof(grapple->name), "Grapple");
    grapple->type = DndAttackTemplateGrapple;
    grapple->ability = DndAbilityStrength;
    grapple->save_ability = DndAbilityStrength;

    DndAttackTemplate* shove = &character->attack_templates[2];
    dnd_data_copy(shove->name, sizeof(shove->name), "Shove");
    shove->type = DndAttackTemplateShove;
    shove->ability = DndAbilityStrength;
    shove->save_ability = DndAbilityStrength;

    DndAttackTemplate* spell_attack = &character->attack_templates[3];
    dnd_data_copy(spell_attack->name, sizeof(spell_attack->name), "Spell Attack");
    spell_attack->type = DndAttackTemplateSpellAttack;
    spell_attack->ability = DndAbilityIntelligence;
    spell_attack->damage_dice = 1U;
    spell_attack->damage_die = 10U;
    DndAttackTemplate* saving_throw = &character->attack_templates[4];
    dnd_data_copy(saving_throw->name, sizeof(saving_throw->name), "Saving Throw Action");
    saving_throw->type = DndAttackTemplateSavingThrow;
    saving_throw->save_ability = DndAbilityDexterity;
    saving_throw->damage_dice = 1U;
    saving_throw->damage_die = 6U;
}

void dnd_data_sanitize(DndSaveData* data) {
    DndCharacter* character = &data->character;
    character->name[sizeof(character->name) - 1U] = '\0';
    character->player[sizeof(character->player) - 1U] = '\0';
    character->species[sizeof(character->species) - 1U] = '\0';
    character->background[sizeof(character->background) - 1U] = '\0';
    character->alignment[sizeof(character->alignment) - 1U] = '\0';
    if(!character->alignment[0])
        dnd_data_copy(character->alignment, sizeof(character->alignment), "True Neutral");
    character->origin_feat[sizeof(character->origin_feat) - 1U] = '\0';
    character->senses[sizeof(character->senses) - 1U] = '\0';
    character->size = dnd_data_clamp_u8(character->size, DndSizeCount - 1U);

    character->class_count = dnd_data_clamp_u8(character->class_count, DND_MAX_CLASSES);
    if(character->class_count == 0U) character->class_count = 1U;
    for(uint8_t i = 0U; i < DND_MAX_CLASSES; ++i) {
        DndClassLevel* class_level = &character->classes[i];
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
            dnd_data_clamp_u8(class_level->spellcasting_mode, DndSpellcastingModeCount - 1U);
        class_level->spellcasting_ability =
            dnd_data_clamp_u8(class_level->spellcasting_ability, DndAbilityCharisma);
        class_level->cantrip_limit = dnd_data_clamp_u8(class_level->cantrip_limit, 30U);
        class_level->prepared_limit = dnd_data_clamp_u8(class_level->prepared_limit, 50U);
        class_level->pact_slot_level = dnd_data_clamp_u8(class_level->pact_slot_level, 5U);
    }
    if(character->classes[0].level == 0U) character->classes[0].level = 1U;

    for(uint8_t i = 0U; i < DND_ABILITY_COUNT; ++i) {
        character->ability_scores[i] =
            (int8_t)dnd_data_clamp_i16(character->ability_scores[i], 1, 30);
        character->saving_throw_proficiency[i] =
            dnd_data_clamp_u8(character->saving_throw_proficiency[i], DndProficiencyProficient);
        character->saving_throw_misc[i] =
            (int8_t)dnd_data_clamp_i16(character->saving_throw_misc[i], -20, 20);
    }
    for(uint8_t i = 0U; i < DND_SKILL_COUNT; ++i) {
        character->skill_proficiency[i] =
            dnd_data_clamp_u8(character->skill_proficiency[i], DndProficiencyExpertise);
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
        dnd_data_clamp_u8(character->spellcasting_ability, DndAbilityCharisma);
    character->spell_attack_misc =
        (int8_t)dnd_data_clamp_i16(character->spell_attack_misc, -20, 20);
    character->spell_save_misc = (int8_t)dnd_data_clamp_i16(character->spell_save_misc, -20, 20);
    character->arcane_recovery_used = character->arcane_recovery_used ? 1U : 0U;

    /* Dynamic character collections are optional. Sanitize each collection locally
       so an absent allocation cannot dereference NULL or invalidate unrelated fields. */
    if(character->spell_storage && character->spells && character->spell_known &&
       character->spell_always_prepared && character->spell_free_casts_current &&
       character->spell_free_casts_max) {
        uint8_t spell_limit =
            dnd_data_clamp_u8(character->spell_capacity, DND_RESIDENT_RECORD_LIMIT);
        character->spell_count = dnd_data_clamp_u8(character->spell_count, spell_limit);
    } else {
        character->spell_count = 0U;
    }
    if(character->features) {
        uint8_t feature_limit =
            dnd_data_clamp_u8(character->feature_capacity, DND_RESIDENT_RECORD_LIMIT);
        character->feature_count = dnd_data_clamp_u8(character->feature_count, feature_limit);
    } else {
        character->feature_count = 0U;
    }
    if(character->items) {
        uint8_t item_limit =
            dnd_data_clamp_u8(character->item_capacity, DND_RESIDENT_RECORD_LIMIT);
        character->item_count = dnd_data_clamp_u8(character->item_count, item_limit);
    } else {
        character->item_count = 0U;
    }
    if(character->grants) {
        uint8_t grant_limit = dnd_data_clamp_u8(character->grant_capacity, DND_MAX_GRANTS);
        character->grant_count = dnd_data_clamp_u8(character->grant_count, grant_limit);
    } else {
        character->grant_count = 0U;
    }

    for(uint8_t i = 0U; i < character->spell_count; ++i) {
        character->spells[i].name[DND_SPELL_NAME_LEN - 1U] = '\0';
        character->spells[i].detail[DND_DETAIL_LEN - 1U] = '\0';
        character->spells[i].level = dnd_data_clamp_u8(character->spells[i].level, 9U);
        character->spells[i].class_index =
            dnd_data_clamp_u8(character->spells[i].class_index, DND_MAX_CLASSES - 1U);
        character->spells[i].prepared = character->spells[i].prepared ? 1U : 0U;
        character->spells[i].ritual = character->spells[i].ritual ? 1U : 0U;
        character->spells[i].stable_id[DND_SHORT_LEN - 1U] = '\0';
        character->spells[i].source[DND_SHORT_LEN - 1U] = '\0';
        character->spells[i].school[DND_SHORT_LEN - 1U] = '\0';
        character->spells[i].grant_name[DND_SHORT_LEN - 1U] = '\0';
        character->spells[i].grant_source =
            dnd_data_clamp_u8(character->spells[i].grant_source, DndGrantSourceCount - 1U);
        character->spell_known[i] = character->spell_known[i] ? 1U : 0U;
        character->spell_always_prepared[i] = character->spell_always_prepared[i] ? 1U : 0U;
        character->spell_free_casts_max[i] =
            dnd_data_clamp_u8(character->spell_free_casts_max[i], 20U);
        character->spell_free_casts_current[i] = dnd_data_clamp_u8(
            character->spell_free_casts_current[i], character->spell_free_casts_max[i]);
    }
    for(uint8_t i = 0U; i < character->feature_count; ++i) {
        character->features[i].name[DND_FEATURE_NAME_LEN - 1U] = '\0';
        character->features[i].detail[DND_DETAIL_LEN - 1U] = '\0';
        character->features[i].class_index =
            dnd_data_clamp_u8(character->features[i].class_index, DND_MAX_CLASSES - 1U);
        character->features[i].class_level_gained =
            dnd_data_clamp_u8(character->features[i].class_level_gained, 20U);
        character->features[i].recharge =
            dnd_data_clamp_u8(character->features[i].recharge, DndRechargeCount - 1U);
        character->features[i].resource_formula = dnd_data_clamp_u8(
            character->features[i].resource_formula, DndResourceFormulaCount - 1U);
        character->features[i].resource_ability =
            dnd_data_clamp_u8(character->features[i].resource_ability, DndAbilityCharisma);
    }
    for(uint8_t i = 0U; i < character->item_count; ++i) {
        DndItem* item = &character->items[i];
        item->name[DND_ITEM_NAME_LEN - 1U] = '\0';
        item->detail[DND_DETAIL_LEN - 1U] = '\0';
        item->attack_ability = dnd_data_clamp_u8(item->attack_ability, DndAttackAbilityBest);
        item->damage_type = dnd_data_clamp_u8(item->damage_type, DndDamageTypeCount - 1U);
        item->damage_dice = dnd_data_clamp_u8(item->damage_dice, 20U);
        item->extra_dice = dnd_data_clamp_u8(item->extra_dice, 20U);
        if(item->container_index < -1 || item->container_index >= UINT16_MAX)
            item->container_index = -1;
        item->charges_current = dnd_data_clamp_i16(item->charges_current, 0, 999);
        item->charges_max = dnd_data_clamp_i16(item->charges_max, 0, 999);
        item->armor_dex_cap = (int8_t)dnd_data_clamp_i16(item->armor_dex_cap, -1, 9);
        item->ammunition_group[DND_SHORT_LEN - 1U] = '\0';
    }
    character->conditions[DND_DETAIL_LEN - 1U] = '\0';
    character->concentration[DND_NAME_LEN - 1U] = '\0';
    character->temporary_effects[DND_DETAIL_LEN - 1U] = '\0';
    character->resistances[DND_DETAIL_LEN - 1U] = '\0';
    character->immunities[DND_DETAIL_LEN - 1U] = '\0';
    character->vulnerabilities[DND_DETAIL_LEN - 1U] = '\0';
    character->movement_modes[DND_DETAIL_LEN - 1U] = '\0';
    character->reaction_available = character->reaction_available ? 1U : 0U;
    for(uint8_t i = 0U; i < character->grant_count; ++i) {
        DndGrant* grant = &character->grants[i];
        grant->stable_id[DND_SHORT_LEN - 1U] = '\0';
        grant->source[DND_SHORT_LEN - 1U] = '\0';
        grant->option_name[DND_NAME_LEN - 1U] = '\0';
        grant->prerequisites[DND_NAME_LEN - 1U] = '\0';
        grant->grant_value[DND_NAME_LEN - 1U] = '\0';
        grant->source_type = dnd_data_clamp_u8(grant->source_type, DndGrantSourceCount - 1U);
        grant->class_index = dnd_data_clamp_u8(grant->class_index, DND_MAX_CLASSES - 1U);
        grant->level_gained = dnd_data_clamp_u8(grant->level_gained, 20U);
        grant->status = dnd_data_clamp_u8(grant->status, DndGrantSkipped);
    }
    character->attack_template_count =
        dnd_data_clamp_u8(character->attack_template_count, DND_MAX_ATTACK_TEMPLATES);

    /* Grapple and Shove are independent attack templates. Older profiles used
       synthetic rows derived from Unarmed Strike; append real templates when
       room exists without renumbering or deleting any user template. */
    bool have_grapple = false;
    bool have_shove = false;
    for(uint8_t i = 0U; i < character->attack_template_count; ++i) {
        have_grapple = have_grapple ||
                       character->attack_templates[i].type == DndAttackTemplateGrapple;
        have_shove = have_shove || character->attack_templates[i].type == DndAttackTemplateShove;
    }
    if(!have_grapple && character->attack_template_count < DND_MAX_ATTACK_TEMPLATES) {
        DndAttackTemplate* grapple =
            &character->attack_templates[character->attack_template_count++];
        memset(grapple, 0, sizeof(*grapple));
        dnd_data_copy(grapple->name, sizeof(grapple->name), "Grapple");
        grapple->type = DndAttackTemplateGrapple;
        grapple->ability = DndAbilityStrength;
        grapple->save_ability = DndAbilityStrength;
    }
    if(!have_shove && character->attack_template_count < DND_MAX_ATTACK_TEMPLATES) {
        DndAttackTemplate* shove =
            &character->attack_templates[character->attack_template_count++];
        memset(shove, 0, sizeof(*shove));
        dnd_data_copy(shove->name, sizeof(shove->name), "Shove");
        shove->type = DndAttackTemplateShove;
        shove->ability = DndAbilityStrength;
        shove->save_ability = DndAbilityStrength;
    }

    for(uint8_t i = 0U; i < character->attack_template_count; ++i) {
        DndAttackTemplate* attack = &character->attack_templates[i];
        attack->name[DND_NAME_LEN - 1U] = '\0';
        attack->mastery[DND_SHORT_LEN - 1U] = '\0';
        attack->damage_type[DND_SHORT_LEN - 1U] = '\0';
        attack->rider_type[DND_SHORT_LEN - 1U] = '\0';
        attack->type = dnd_data_clamp_u8(attack->type, DndAttackTemplateTypeCount - 1U);
        attack->ability = dnd_data_clamp_u8(attack->ability, DndAbilityCharisma);
        attack->save_ability = dnd_data_clamp_u8(attack->save_ability, DndAbilityCharisma);
        attack->damage_dice = dnd_data_clamp_u8(attack->damage_dice, 20U);
        attack->rider_dice = dnd_data_clamp_u8(attack->rider_dice, 20U);
        attack->recharge = dnd_data_clamp_u8(attack->recharge, DndRechargeCount - 1U);
    }
}
