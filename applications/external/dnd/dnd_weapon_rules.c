#include "dnd_weapon_rules.h"

int8_t dnd_weapon_rules_ability(const PocketCharacter* character, const PocketItem* item) {
    int8_t strength =
        dnd_rules_core_ability_modifier(character->ability_scores[PocketAbilityStrength]);
    int8_t dexterity =
        dnd_rules_core_ability_modifier(character->ability_scores[PocketAbilityDexterity]);
    switch(item->attack_ability) {
    case PocketAttackAbilityStrength:
        return strength;
    case PocketAttackAbilityDexterity:
        return dexterity;
    case PocketAttackAbilityBest:
        return strength > dexterity ? strength : dexterity;
    default:
        if(item->weapon_properties & PocketWeaponRanged) return dexterity;
        if(item->weapon_properties & PocketWeaponFinesse)
            return strength > dexterity ? strength : dexterity;
        return strength;
    }
}

int8_t dnd_weapon_rules_attack_modifier(const PocketCharacter* character, const PocketItem* item) {
    int8_t result = dnd_weapon_rules_ability(character, item) + item->magic_bonus;
    if(item->proficient) result += dnd_rules_core_proficiency_bonus(character);
    result += dnd_rules_core_exhaustion_penalty(character);
    return result;
}
