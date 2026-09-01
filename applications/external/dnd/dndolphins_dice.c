#include "dndolphins_dice.h"
#include "dnd_rules.h"

uint8_t dndolphins_dice_roll_d20_mode(PocketRollMode mode) {
    uint8_t first = dnd_rules_core_roll_die(20U);
    if(mode != PocketRollAdvantage && mode != PocketRollDisadvantage) return first;
    uint8_t second = dnd_rules_core_roll_die(20U);
    if(mode == PocketRollAdvantage) return second > first ? second : first;
    return second < first ? second : first;
}

uint16_t dndolphins_dice_roll_values(
    uint8_t count,
    uint8_t sides,
    uint8_t* values,
    uint8_t capacity) {
    uint16_t total = 0U;
    for(uint8_t i = 0U; i < count; ++i) {
        uint8_t roll = dnd_rules_core_roll_die(sides);
        total += roll;
        if(values && i < capacity) values[i] = roll;
    }
    return total;
}
