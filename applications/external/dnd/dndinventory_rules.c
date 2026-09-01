#include "dndinventory_internal.h"

static uint8_t dndinventory_rules_total_level(const DndInventoryCharacterState* character) {
    if(!character) return 1U;
    uint16_t total = 0U;
    for(uint8_t i = 0U; i < character->class_count && i < POCKET_D20_MAX_CLASSES; ++i)
        total += character->classes[i].level;
    if(total < 1U) return 1U;
    return total > 20U ? 20U : (uint8_t)total;
}

int16_t dndinventory_rules_carrying_capacity(const DndInventoryCharacterState* character) {
    if(character->carrying_capacity_override > 0) return character->carrying_capacity_override;
    return (int16_t)character->ability_scores[PocketAbilityStrength] * 15;
}

void dndinventory_rules_normalize_currency(DndInventoryCharacterState* character) {
    int64_t copper = character->currency_cp + character->currency_sp * 10LL +
                     character->currency_ep * 50LL + character->currency_gp * 100LL +
                     character->currency_pp * 1000LL;
    if(copper < 0) copper = 0;
    character->currency_pp = (int32_t)(copper / 1000LL);
    copper %= 1000LL;
    character->currency_gp = (int32_t)(copper / 100LL);
    copper %= 100LL;
    character->currency_ep = (int32_t)(copper / 50LL);
    copper %= 50LL;
    character->currency_sp = (int32_t)(copper / 10LL);
    character->currency_cp = (int32_t)(copper % 10LL);
}

int16_t dndinventory_rules_calculated_armor_class(
    const DndInventoryCharacterState* character,
    const DndInventoryItemAggregate* aggregate) {
    int16_t armor = 10;
    int16_t dexterity =
        dnd_rules_core_ability_modifier(character->ability_scores[PocketAbilityDexterity]);
    if(aggregate->armor_base) {
        int16_t dexterity_part = dexterity;
        if(aggregate->armor_dex_cap >= 0 && dexterity_part > aggregate->armor_dex_cap)
            dexterity_part = aggregate->armor_dex_cap;
        armor = aggregate->armor_base + dexterity_part;
    }
    return armor + aggregate->shield_bonus;
}

int8_t dndinventory_rules_weapon_attack_modifier(
    const DndInventoryCharacterState* character,
    const PocketItem* item) {
    int8_t strength =
        dnd_rules_core_ability_modifier(character->ability_scores[PocketAbilityStrength]);
    int8_t dexterity =
        dnd_rules_core_ability_modifier(character->ability_scores[PocketAbilityDexterity]);
    int8_t ability = strength;
    switch(item->attack_ability) {
    case PocketAttackAbilityStrength:
        ability = strength;
        break;
    case PocketAttackAbilityDexterity:
        ability = dexterity;
        break;
    case PocketAttackAbilityBest:
        ability = strength > dexterity ? strength : dexterity;
        break;
    default:
        if(item->weapon_properties & PocketWeaponRanged)
            ability = dexterity;
        else if(item->weapon_properties & PocketWeaponFinesse)
            ability = strength > dexterity ? strength : dexterity;
        break;
    }
    int16_t result = ability + item->magic_bonus;
    if(item->proficient)
        result += (int16_t)(2U + (dndinventory_rules_total_level(character) - 1U) / 4U);
    if(character->exhaustion) result -= (int16_t)(2U * character->exhaustion);
    if(result < -128) result = -128;
    if(result > 127) result = 127;
    return (int8_t)result;
}
