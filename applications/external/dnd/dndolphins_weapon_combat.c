#include "dndolphins_weapon_combat.h"

DndAttackRoll dndolphins_weapon_combat_roll_attack(
    const DndCharacter* character,
    const DndItem* item,
    DndRollMode mode) {
    DndAttackRoll result = {0};
    result.first_die = dnd_rules_core_roll_die(20U);
    result.natural_roll = result.first_die;
    if(mode == DndRollAdvantage || mode == DndRollDisadvantage) {
        result.second_die = dnd_rules_core_roll_die(20U);
        if(mode == DndRollAdvantage) {
            if(result.second_die > result.natural_roll) result.natural_roll = result.second_die;
        } else if(result.second_die < result.natural_roll) {
            result.natural_roll = result.second_die;
        }
    }
    result.modifier = dnd_weapon_rules_attack_modifier(character, item);
    result.total = (int16_t)result.natural_roll + result.modifier;
    result.critical = result.natural_roll == 20U;
    result.automatic_miss = result.natural_roll == 1U;
    return result;
}

DndDamageRoll dndolphins_weapon_combat_roll_damage(
    const DndCharacter* character,
    const DndItem* item,
    bool critical) {
    DndDamageRoll result = {0};
    uint8_t multiplier = critical ? 2U : 1U;
    uint8_t die = item->damage_die;
    if(item->use_versatile && item->versatile_die >= 2U) die = item->versatile_die;
    result.weapon_roll_count = item->damage_dice * multiplier;
    result.extra_roll_count = item->extra_dice * multiplier;
    if(result.weapon_roll_count > DNDOLPHINS_MAX_DAMAGE_ROLLS)
        result.weapon_roll_count = DNDOLPHINS_MAX_DAMAGE_ROLLS;
    if(result.extra_roll_count > DNDOLPHINS_MAX_DAMAGE_ROLLS - result.weapon_roll_count)
        result.extra_roll_count = DNDOLPHINS_MAX_DAMAGE_ROLLS - result.weapon_roll_count;
    result.weapon_total = (int16_t)dndolphins_dice_roll_values(
        result.weapon_roll_count, die, result.rolls, result.weapon_roll_count);
    result.extra_total = (int16_t)dndolphins_dice_roll_values(
        result.extra_roll_count,
        item->extra_die,
        result.rolls + result.weapon_roll_count,
        result.extra_roll_count);
    result.modifier = item->magic_bonus;
    if(item->add_ability_damage) result.modifier += dnd_weapon_rules_ability(character, item);
    result.total = result.weapon_total + result.extra_total + result.modifier;
    if(result.total < 0) result.total = 0;
    result.critical = critical;
    return result;
}

typedef struct {
    uint16_t* indices;
    uint16_t start;
    uint16_t capacity;
    uint16_t count;
} DndDolphinsWeaponIndexContext;

static bool dndolphins_weapon_combat_index_visitor(
    uint16_t logical_index,
    const DndItem* item,
    void* context) {
    DndDolphinsWeaponIndexContext* scan = context;
    if(item->is_weapon) {
        if(scan->count >= scan->start && scan->count - scan->start < scan->capacity)
            scan->indices[scan->count - scan->start] = logical_index;
        ++scan->count;
    }
    return true;
}

bool dndolphins_weapon_combat_items_collect_weapon_indices(
    Storage* storage,
    uint32_t profile,
    uint16_t start,
    uint16_t* indices,
    uint16_t capacity,
    uint16_t* count,
    uint16_t* total_count) {
    if(!storage || !count || (capacity && !indices)) return false;
    DndDolphinsWeaponIndexContext context = {
        .indices = indices,
        .start = start,
        .capacity = capacity,
        .count = 0U,
    };
    uint16_t total = 0U;
    bool success = dnd_storage_visit_items(
        storage, profile, dndolphins_weapon_combat_index_visitor, &context, &total);
    *count = context.count;
    if(total_count) *total_count = total;
    return success;
}
