#include "pocket_d20.h"

#include <stdlib.h>
#include <string.h>

static void pocket_d20_copy(char* destination, size_t size, const char* source) {
    if(size == 0U) return;
    strncpy(destination, source, size - 1U);
    destination[size - 1U] = '\0';
}

static uint8_t pocket_d20_clamp_u8(uint8_t value, uint8_t maximum) {
    return value > maximum ? maximum : value;
}

static int16_t pocket_d20_clamp_i16(int16_t value, int16_t minimum, int16_t maximum) {
    if(value < minimum) return minimum;
    if(value > maximum) return maximum;
    return value;
}

static uint8_t pocket_d20_next_capacity(uint8_t current, uint8_t required, uint8_t maximum) {
    uint8_t capacity = current ? current : 1U;
    while(capacity < required && capacity < maximum) {
        uint16_t doubled = (uint16_t)capacity * 2U;
        capacity = doubled > maximum ? maximum : (uint8_t)doubled;
    }
    return capacity;
}

static bool pocket_d20_reserve_records(
    void** records,
    uint8_t* capacity,
    uint8_t required,
    uint8_t maximum,
    size_t record_size) {
    if(required <= *capacity) return true;
    if(required > maximum) return false;
    uint8_t next = pocket_d20_next_capacity(*capacity, required, maximum);
    void* resized = realloc(*records, (size_t)next * record_size);
    if(!resized) return false;
    memset(
        (uint8_t*)resized + (size_t)(*capacity) * record_size,
        0,
        (size_t)(next - *capacity) * record_size);
    *records = resized;
    *capacity = next;
    return true;
}

bool pocket_d20_data_reserve_spells(PocketCharacter* character, uint8_t required) {
    if(required <= character->spell_capacity) return true;
    if(required > POCKET_D20_MAX_SPELLS) return false;
    uint8_t next =
        pocket_d20_next_capacity(character->spell_capacity, required, POCKET_D20_MAX_SPELLS);
    size_t spell_bytes = (size_t)next * sizeof(PocketSpell);
    size_t state_bytes = (size_t)next * 4U;
    uint8_t* storage = calloc(1U, spell_bytes + state_bytes);
    if(!storage) return false;
    PocketSpell* spells = (PocketSpell*)storage;
    uint8_t* known = storage + spell_bytes;
    uint8_t* always_prepared = known + next;
    uint8_t* free_current = always_prepared + next;
    uint8_t* free_max = free_current + next;
    if(character->spell_storage) {
        uint8_t count = character->spell_count;
        memcpy(spells, character->spells, (size_t)count * sizeof(PocketSpell));
        memcpy(known, character->spell_known, count);
        memcpy(always_prepared, character->spell_always_prepared, count);
        memcpy(free_current, character->spell_free_casts_current, count);
        memcpy(free_max, character->spell_free_casts_max, count);
        free(character->spell_storage);
    }
    character->spell_storage = storage;
    character->spells = spells;
    character->spell_known = known;
    character->spell_always_prepared = always_prepared;
    character->spell_free_casts_current = free_current;
    character->spell_free_casts_max = free_max;
    character->spell_capacity = next;
    return true;
}

bool pocket_d20_data_reserve_features(PocketCharacter* character, uint8_t required) {
    return pocket_d20_reserve_records(
        (void**)&character->features,
        &character->feature_capacity,
        required,
        POCKET_D20_MAX_FEATURES,
        sizeof(PocketFeature));
}

bool pocket_d20_data_reserve_items(PocketCharacter* character, uint8_t required) {
    return pocket_d20_reserve_records(
        (void**)&character->items,
        &character->item_capacity,
        required,
        POCKET_D20_MAX_ITEMS,
        sizeof(PocketItem));
}

bool pocket_d20_data_reserve_journal(PocketCharacter* character, uint8_t required) {
    return pocket_d20_reserve_records(
        (void**)&character->journal,
        &character->journal_capacity,
        required,
        POCKET_D20_MAX_JOURNAL,
        sizeof(PocketJournalEntry));
}

bool pocket_d20_data_reserve_grants(PocketCharacter* character, uint8_t required) {
    return pocket_d20_reserve_records(
        (void**)&character->grants,
        &character->grant_capacity,
        required,
        POCKET_D20_MAX_GRANTS,
        sizeof(PocketGrant));
}

void pocket_d20_data_clear(PocketSaveData* data) {
    if(!data) return;
    free(data->character.spell_storage);
    free(data->character.features);
    free(data->character.items);
    free(data->character.journal);
    free(data->character.grants);
    memset(data, 0, sizeof(*data));
}

void pocket_d20_data_set_defaults(PocketSaveData* data) {
    memset(data, 0, sizeof(*data));
    PocketCharacter* character = &data->character;

    pocket_d20_copy(character->name, sizeof(character->name), "New Hero");
    pocket_d20_copy(character->player, sizeof(character->player), "Player");
    pocket_d20_copy(character->species, sizeof(character->species), "Human");
    pocket_d20_copy(character->background, sizeof(character->background), "Adventurer");
    pocket_d20_copy(character->alignment, sizeof(character->alignment), "True Neutral");
    pocket_d20_copy(character->origin_feat, sizeof(character->origin_feat), "None");
    character->size = PocketSizeMedium;
    pocket_d20_copy(character->senses, sizeof(character->senses), "Normal vision");
    pocket_d20_copy(character->movement_modes, sizeof(character->movement_modes), "Walk 30 ft");
    pocket_d20_copy(
        character->adventure_campaign, sizeof(character->adventure_campaign), "reef_wardens");
    pocket_d20_copy(character->adventure_scene, sizeof(character->adventure_scene), "reef_gate");
    pocket_d20_copy(
        character->adventure_checkpoint, sizeof(character->adventure_checkpoint), "reef_gate");
    character->reaction_available = 1U;

    character->class_count = 1U;
    pocket_d20_copy(character->classes[0].name, sizeof(character->classes[0].name), "Fighter");
    pocket_d20_copy(
        character->classes[0].subclass, sizeof(character->classes[0].subclass), "None");
    character->classes[0].level = 1U;
    character->classes[0].hit_die = 10U;
    character->classes[0].hit_dice_current = 1U;
    character->classes[0].hit_dice_max = 1U;
    character->classes[0].spellcasting_mode = PocketSpellcastingNone;
    character->classes[0].spellcasting_ability = PocketAbilityIntelligence;
    character->milestone_leveling = 1U;

    for(uint8_t i = 0U; i < POCKET_D20_ABILITY_COUNT; ++i) {
        character->ability_scores[i] = 10;
    }
    character->ability_scores[PocketAbilityConstitution] = 12;
    character->hp_current = 10;
    character->hp_max = 10;
    character->armor_class = 10;
    character->speed = 30;
    character->hit_die = 10U;
    character->hit_dice_current = 1U;
    character->hit_dice_max = 1U;
    character->spellcasting_ability = PocketAbilityIntelligence;

    character->language_count = 1U;
    pocket_d20_copy(character->languages[0], sizeof(character->languages[0]), "Common");

    if(pocket_d20_data_reserve_items(character, 1U)) {
        character->item_count = 1U;
        PocketItem* sword = &character->items[0];
        pocket_d20_copy(sword->name, sizeof(sword->name), "Longsword");
        pocket_d20_copy(sword->detail, sizeof(sword->detail), "Versatile martial melee weapon.");
        sword->quantity = 1;
        sword->weight_tenths = 30;
        sword->equipped = 1U;
        sword->is_weapon = 1U;
        sword->attack_ability = PocketAttackAbilityAuto;
        sword->proficient = 1U;
        sword->damage_dice = 1U;
        sword->damage_die = 8U;
        sword->versatile_die = 10U;
        sword->damage_type = PocketDamageSlashing;
        sword->add_ability_damage = 1U;
        sword->container_index = -1;
        sword->armor_dex_cap = -1;
    }

    if(pocket_d20_data_reserve_journal(character, 1U)) {
        character->journal_count = 1U;
        PocketJournalEntry* note = &character->journal[0];
        pocket_d20_copy(note->title, sizeof(note->title), "Welcome");
        pocket_d20_copy(
            note->body,
            sizeof(note->body),
            "Use Journal for adventure notes, item ideas, and milestones.");
        note->category = PocketJournalQuick;
    }

    character->attack_template_count = 3U;
    PocketAttackTemplate* unarmed = &character->attack_templates[0];
    pocket_d20_copy(unarmed->name, sizeof(unarmed->name), "Unarmed Strike");
    pocket_d20_copy(unarmed->damage_type, sizeof(unarmed->damage_type), "Bludgeoning");
    unarmed->type = PocketAttackTemplateUnarmed;
    unarmed->ability = PocketAbilityStrength;
    unarmed->damage_dice = 1U;
    unarmed->damage_die = 1U;
    PocketAttackTemplate* spell_attack = &character->attack_templates[1];
    pocket_d20_copy(spell_attack->name, sizeof(spell_attack->name), "Spell Attack");
    spell_attack->type = PocketAttackTemplateSpellAttack;
    spell_attack->ability = PocketAbilityIntelligence;
    spell_attack->damage_dice = 1U;
    spell_attack->damage_die = 10U;
    PocketAttackTemplate* saving_throw = &character->attack_templates[2];
    pocket_d20_copy(saving_throw->name, sizeof(saving_throw->name), "Saving Throw Action");
    saving_throw->type = PocketAttackTemplateSavingThrow;
    saving_throw->save_ability = PocketAbilityDexterity;
    saving_throw->damage_dice = 1U;
    saving_throw->damage_die = 6U;

    data->initiative.round = 1U;
}

void pocket_d20_data_sanitize(PocketSaveData* data) {
    PocketCharacter* character = &data->character;

    character->name[sizeof(character->name) - 1U] = '\0';
    character->player[sizeof(character->player) - 1U] = '\0';
    character->species[sizeof(character->species) - 1U] = '\0';
    character->background[sizeof(character->background) - 1U] = '\0';
    character->alignment[sizeof(character->alignment) - 1U] = '\0';
    if(!character->alignment[0])
        pocket_d20_copy(character->alignment, sizeof(character->alignment), "True Neutral");
    character->other_proficiencies[sizeof(character->other_proficiencies) - 1U] = '\0';
    character->origin_feat[sizeof(character->origin_feat) - 1U] = '\0';
    character->tool_proficiencies[sizeof(character->tool_proficiencies) - 1U] = '\0';
    character->armor_training[sizeof(character->armor_training) - 1U] = '\0';
    character->weapon_training[sizeof(character->weapon_training) - 1U] = '\0';
    character->senses[sizeof(character->senses) - 1U] = '\0';
    character->adventure_campaign[sizeof(character->adventure_campaign) - 1U] = '\0';
    character->adventure_scene[sizeof(character->adventure_scene) - 1U] = '\0';
    character->adventure_checkpoint[sizeof(character->adventure_checkpoint) - 1U] = '\0';
    if(!character->adventure_campaign[0])
        pocket_d20_copy(
            character->adventure_campaign, sizeof(character->adventure_campaign), "reef_wardens");
    if(!character->adventure_scene[0])
        pocket_d20_copy(
            character->adventure_scene, sizeof(character->adventure_scene), "reef_gate");
    if(!character->adventure_checkpoint[0])
        pocket_d20_copy(
            character->adventure_checkpoint,
            sizeof(character->adventure_checkpoint),
            character->adventure_scene);
    character->size = pocket_d20_clamp_u8(character->size, PocketSizeCount - 1U);

    character->class_count = pocket_d20_clamp_u8(character->class_count, POCKET_D20_MAX_CLASSES);
    if(character->class_count == 0U) character->class_count = 1U;
    for(uint8_t i = 0U; i < POCKET_D20_MAX_CLASSES; ++i) {
        PocketClassLevel* class_level = &character->classes[i];
        class_level->name[sizeof(class_level->name) - 1U] = '\0';
        class_level->subclass[sizeof(class_level->subclass) - 1U] = '\0';
        class_level->level = pocket_d20_clamp_u8(class_level->level, 20U);
        if(class_level->hit_die != 6U && class_level->hit_die != 8U &&
           class_level->hit_die != 10U && class_level->hit_die != 12U)
            class_level->hit_die = 8U;
        class_level->hit_dice_max = pocket_d20_clamp_u8(class_level->hit_dice_max, 20U);
        class_level->hit_dice_current =
            pocket_d20_clamp_u8(class_level->hit_dice_current, class_level->hit_dice_max);
        class_level->spellcasting_mode =
            pocket_d20_clamp_u8(class_level->spellcasting_mode, PocketSpellcastingModeCount - 1U);
        class_level->spellcasting_ability =
            pocket_d20_clamp_u8(class_level->spellcasting_ability, PocketAbilityCharisma);
        class_level->cantrip_limit = pocket_d20_clamp_u8(class_level->cantrip_limit, 30U);
        class_level->prepared_limit = pocket_d20_clamp_u8(class_level->prepared_limit, 50U);
        class_level->pact_slot_level = pocket_d20_clamp_u8(class_level->pact_slot_level, 5U);
    }
    if(character->classes[0].level == 0U) character->classes[0].level = 1U;

    for(uint8_t i = 0U; i < POCKET_D20_ABILITY_COUNT; ++i) {
        character->ability_scores[i] =
            (int8_t)pocket_d20_clamp_i16(character->ability_scores[i], 1, 30);
        character->saving_throw_proficiency[i] = pocket_d20_clamp_u8(
            character->saving_throw_proficiency[i], PocketProficiencyProficient);
        character->saving_throw_misc[i] =
            (int8_t)pocket_d20_clamp_i16(character->saving_throw_misc[i], -20, 20);
    }
    for(uint8_t i = 0U; i < POCKET_D20_SKILL_COUNT; ++i) {
        character->skill_proficiency[i] =
            pocket_d20_clamp_u8(character->skill_proficiency[i], PocketProficiencyExpertise);
        character->skill_misc[i] = (int8_t)pocket_d20_clamp_i16(character->skill_misc[i], -20, 20);
    }

    character->hp_max = pocket_d20_clamp_i16(character->hp_max, 1, 999);
    character->hp_current = pocket_d20_clamp_i16(character->hp_current, 0, 999);
    character->hp_temporary = pocket_d20_clamp_i16(character->hp_temporary, 0, 999);
    character->armor_class = pocket_d20_clamp_i16(character->armor_class, 0, 99);
    character->speed = pocket_d20_clamp_i16(character->speed, 0, 255);
    character->initiative_misc = (int8_t)pocket_d20_clamp_i16(character->initiative_misc, -20, 20);
    character->exhaustion = pocket_d20_clamp_u8(character->exhaustion, 6U);
    character->death_successes = pocket_d20_clamp_u8(character->death_successes, 3U);
    character->death_failures = pocket_d20_clamp_u8(character->death_failures, 3U);
    if(character->hit_die != 6U && character->hit_die != 8U && character->hit_die != 10U &&
       character->hit_die != 12U)
        character->hit_die = 8U;
    character->spellcasting_ability =
        pocket_d20_clamp_u8(character->spellcasting_ability, PocketAbilityCharisma);
    character->spell_attack_misc =
        (int8_t)pocket_d20_clamp_i16(character->spell_attack_misc, -20, 20);
    character->spell_save_misc = (int8_t)pocket_d20_clamp_i16(character->spell_save_misc, -20, 20);
    character->arcane_recovery_used = character->arcane_recovery_used ? 1U : 0U;

    character->spell_count = pocket_d20_clamp_u8(character->spell_count, POCKET_D20_MAX_SPELLS);
    character->feature_count =
        pocket_d20_clamp_u8(character->feature_count, POCKET_D20_MAX_FEATURES);
    character->item_count = pocket_d20_clamp_u8(character->item_count, POCKET_D20_MAX_ITEMS);
    character->language_count =
        pocket_d20_clamp_u8(character->language_count, POCKET_D20_MAX_LANGUAGES);
    character->journal_count =
        pocket_d20_clamp_u8(character->journal_count, POCKET_D20_MAX_JOURNAL);

    for(uint8_t i = 0U; i < character->spell_count; ++i) {
        character->spells[i].name[POCKET_D20_NAME_LEN - 1U] = '\0';
        character->spells[i].detail[POCKET_D20_DETAIL_LEN - 1U] = '\0';
        character->spells[i].level = pocket_d20_clamp_u8(character->spells[i].level, 9U);
        character->spells[i].class_index =
            pocket_d20_clamp_u8(character->spells[i].class_index, POCKET_D20_MAX_CLASSES - 1U);
        character->spells[i].prepared = character->spells[i].prepared ? 1U : 0U;
        character->spells[i].ritual = character->spells[i].ritual ? 1U : 0U;
        character->spells[i].stable_id[POCKET_D20_SHORT_LEN - 1U] = '\0';
        character->spells[i].source[POCKET_D20_SHORT_LEN - 1U] = '\0';
        character->spells[i].school[POCKET_D20_SHORT_LEN - 1U] = '\0';
        character->spells[i].grant_name[POCKET_D20_SHORT_LEN - 1U] = '\0';
        character->spells[i].grant_source =
            pocket_d20_clamp_u8(character->spells[i].grant_source, PocketGrantSourceCount - 1U);
        character->spell_known[i] = character->spell_known[i] ? 1U : 0U;
        character->spell_always_prepared[i] = character->spell_always_prepared[i] ? 1U : 0U;
        character->spell_free_casts_max[i] =
            pocket_d20_clamp_u8(character->spell_free_casts_max[i], 20U);
        character->spell_free_casts_current[i] = pocket_d20_clamp_u8(
            character->spell_free_casts_current[i], character->spell_free_casts_max[i]);
    }
    for(uint8_t i = 0U; i < character->feature_count; ++i) {
        character->features[i].name[POCKET_D20_NAME_LEN - 1U] = '\0';
        character->features[i].detail[POCKET_D20_DETAIL_LEN - 1U] = '\0';
        character->features[i].class_index =
            pocket_d20_clamp_u8(character->features[i].class_index, POCKET_D20_MAX_CLASSES - 1U);
        character->features[i].class_level_gained =
            pocket_d20_clamp_u8(character->features[i].class_level_gained, 20U);
        character->features[i].recharge =
            pocket_d20_clamp_u8(character->features[i].recharge, PocketRechargeCount - 1U);
        character->features[i].resource_formula = pocket_d20_clamp_u8(
            character->features[i].resource_formula, PocketResourceFormulaCount - 1U);
        character->features[i].resource_ability =
            pocket_d20_clamp_u8(character->features[i].resource_ability, PocketAbilityCharisma);
    }
    for(uint8_t i = 0U; i < character->item_count; ++i) {
        PocketItem* item = &character->items[i];
        item->name[POCKET_D20_NAME_LEN - 1U] = '\0';
        item->detail[POCKET_D20_DETAIL_LEN - 1U] = '\0';
        item->attack_ability = pocket_d20_clamp_u8(item->attack_ability, PocketAttackAbilityBest);
        item->damage_type = pocket_d20_clamp_u8(item->damage_type, PocketDamageTypeCount - 1U);
        item->damage_dice = pocket_d20_clamp_u8(item->damage_dice, 20U);
        item->extra_dice = pocket_d20_clamp_u8(item->extra_dice, 20U);
        item->container_index =
            (int8_t)pocket_d20_clamp_i16(item->container_index, -1, POCKET_D20_MAX_ITEMS - 1U);
        item->charges_current = pocket_d20_clamp_i16(item->charges_current, 0, 999);
        item->charges_max = pocket_d20_clamp_i16(item->charges_max, 0, 999);
        item->armor_dex_cap = (int8_t)pocket_d20_clamp_i16(item->armor_dex_cap, -1, 9);
        item->ammunition_group[POCKET_D20_SHORT_LEN - 1U] = '\0';
    }
    for(uint8_t i = 0U; i < POCKET_D20_MAX_LANGUAGES; ++i) {
        character->languages[i][POCKET_D20_SHORT_LEN - 1U] = '\0';
    }
    for(uint8_t i = 0U; i < character->journal_count; ++i) {
        PocketJournalEntry* entry = &character->journal[i];
        entry->title[POCKET_D20_NAME_LEN - 1U] = '\0';
        entry->body[POCKET_D20_DETAIL_LEN - 1U] = '\0';
        entry->category = pocket_d20_clamp_u8(entry->category, PocketJournalCategoryCount - 1U);
        entry->class_index = pocket_d20_clamp_u8(entry->class_index, POCKET_D20_MAX_CLASSES - 1U);
    }

    character->conditions[POCKET_D20_DETAIL_LEN - 1U] = '\0';
    character->concentration[POCKET_D20_NAME_LEN - 1U] = '\0';
    character->temporary_effects[POCKET_D20_DETAIL_LEN - 1U] = '\0';
    character->resistances[POCKET_D20_DETAIL_LEN - 1U] = '\0';
    character->immunities[POCKET_D20_DETAIL_LEN - 1U] = '\0';
    character->vulnerabilities[POCKET_D20_DETAIL_LEN - 1U] = '\0';
    character->movement_modes[POCKET_D20_DETAIL_LEN - 1U] = '\0';
    character->reaction_available = character->reaction_available ? 1U : 0U;
    character->grant_count = pocket_d20_clamp_u8(character->grant_count, POCKET_D20_MAX_GRANTS);
    for(uint8_t i = 0U; i < character->grant_count; ++i) {
        PocketGrant* grant = &character->grants[i];
        grant->stable_id[POCKET_D20_SHORT_LEN - 1U] = '\0';
        grant->source[POCKET_D20_SHORT_LEN - 1U] = '\0';
        grant->option_name[POCKET_D20_NAME_LEN - 1U] = '\0';
        grant->prerequisites[POCKET_D20_NAME_LEN - 1U] = '\0';
        grant->grant_value[POCKET_D20_NAME_LEN - 1U] = '\0';
        grant->source_type = pocket_d20_clamp_u8(grant->source_type, PocketGrantSourceCount - 1U);
        grant->class_index = pocket_d20_clamp_u8(grant->class_index, POCKET_D20_MAX_CLASSES - 1U);
        grant->level_gained = pocket_d20_clamp_u8(grant->level_gained, 20U);
        grant->status = pocket_d20_clamp_u8(grant->status, PocketGrantSkipped);
    }
    character->attack_template_count =
        pocket_d20_clamp_u8(character->attack_template_count, POCKET_D20_MAX_ATTACK_TEMPLATES);
    for(uint8_t i = 0U; i < character->attack_template_count; ++i) {
        PocketAttackTemplate* attack = &character->attack_templates[i];
        attack->name[POCKET_D20_NAME_LEN - 1U] = '\0';
        attack->mastery[POCKET_D20_SHORT_LEN - 1U] = '\0';
        attack->damage_type[POCKET_D20_SHORT_LEN - 1U] = '\0';
        attack->rider_type[POCKET_D20_SHORT_LEN - 1U] = '\0';
        attack->type = pocket_d20_clamp_u8(attack->type, PocketAttackTemplateTypeCount - 1U);
        attack->ability = pocket_d20_clamp_u8(attack->ability, PocketAbilityCharisma);
        attack->save_ability = pocket_d20_clamp_u8(attack->save_ability, PocketAbilityCharisma);
        attack->damage_dice = pocket_d20_clamp_u8(attack->damage_dice, 20U);
        attack->rider_dice = pocket_d20_clamp_u8(attack->rider_dice, 20U);
        attack->recharge = pocket_d20_clamp_u8(attack->recharge, PocketRechargeCount - 1U);
    }

    data->party_count = pocket_d20_clamp_u8(data->party_count, POCKET_D20_MAX_PARTY);
    for(uint8_t i = 0U; i < POCKET_D20_MAX_PARTY; ++i) {
        data->party[i].name[POCKET_D20_SHORT_LEN - 1U] = '\0';
    }
    data->initiative.count =
        pocket_d20_clamp_u8(data->initiative.count, POCKET_D20_MAX_INITIATIVE);
    if(data->initiative.round == 0U) data->initiative.round = 1U;
    if(data->initiative.current_turn >= data->initiative.count) data->initiative.current_turn = 0U;
    for(uint8_t i = 0U; i < POCKET_D20_MAX_INITIATIVE; ++i) {
        data->initiative.entries[i].name[POCKET_D20_SHORT_LEN - 1U] = '\0';
        data->initiative.entries[i].conditions[POCKET_D20_SHORT_LEN - 1U] = '\0';
    }
    data->encounter_history_count =
        pocket_d20_clamp_u8(data->encounter_history_count, POCKET_D20_MAX_ENCOUNTER_HISTORY);
    for(uint8_t i = 0U; i < data->encounter_history_count; ++i) {
        data->encounter_history[i].kind =
            pocket_d20_clamp_u8(data->encounter_history[i].kind, PocketHistoryKindCount - 1U);
    }
}
