#include "dndinventory_internal.h"

int16_t dndinventory_rules_carrying_capacity(const PocketCharacter* character) {
    if(character->carrying_capacity_override > 0) return character->carrying_capacity_override;
    return (int16_t)character->ability_scores[PocketAbilityStrength] * 15;
}

void dndinventory_rules_normalize_currency(PocketCharacter* character) {
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
    const PocketCharacter* character,
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
