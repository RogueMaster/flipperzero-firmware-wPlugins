#include "dnd_weapon_rules.h"

int8_t dnd_weapon_rules_ability(const DndCharacter* character, const DndItem* item) {
    int8_t strength =
        dnd_rules_core_ability_modifier(character->ability_scores[DndAbilityStrength]);
    int8_t dexterity =
        dnd_rules_core_ability_modifier(character->ability_scores[DndAbilityDexterity]);
    switch(item->attack_ability) {
    case DndAttackAbilityStrength:
        return strength;
    case DndAttackAbilityDexterity:
        return dexterity;
    case DndAttackAbilityBest:
        return strength > dexterity ? strength : dexterity;
    default:
        if(item->weapon_properties & DndWeaponRanged) return dexterity;
        if(item->weapon_properties & DndWeaponFinesse)
            return strength > dexterity ? strength : dexterity;
        return strength;
    }
}

int8_t dnd_weapon_rules_attack_modifier(const DndCharacter* character, const DndItem* item) {
    int8_t result = dnd_weapon_rules_ability(character, item) + item->magic_bonus;
    if(item->proficient) result += dnd_rules_core_proficiency_bonus(character);
    result += dnd_rules_core_exhaustion_penalty(character);
    return result;
}
