#include "dndolphins_items.h"

#include <furi.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define POCKET_D20_DEFAULT_CLASS_EQUIPMENT APP_ASSETS_PATH("equipment/default_class.txt")
#define POCKET_D20_DEFAULT_RACE_EQUIPMENT APP_ASSETS_PATH("equipment/default_race.txt")
#define POCKET_D20_DEFAULT_BACKGROUND_EQUIPMENT APP_ASSETS_PATH("equipment/default_background.txt")
#define POCKET_D20_DEFAULT_TRINKETS APP_ASSETS_PATH("equipment/trinkets.txt")

static int32_t pocket_d20_items_add_saturated(int32_t value, int32_t add) {
    int64_t total = (int64_t)value + add;
    if(total > INT32_MAX) return INT32_MAX;
    if(total < INT32_MIN) return INT32_MIN;
    return (int32_t)total;
}

bool pocket_d20_items_initialize_inventory(
    Storage* storage,
    uint32_t profile,
    PocketSaveData* data,
    bool* initialized) {
    if(initialized) *initialized = false;
    if(!storage || !data) return false;
    if(pocket_d20_storage_items_exist(storage, profile)) return true;

    PocketCharacter* character = &data->character;
    char trinket_key[4];
    uint8_t trinket_roll = pocket_d20_roll_die(100U);
    snprintf(trinket_key, sizeof(trinket_key), "%u", trinket_roll);

    PocketD20ItemSeedAsset assets[4] = {
        {.path = POCKET_D20_DEFAULT_CLASS_EQUIPMENT,
         .match = character->class_count ? character->classes[0].name : ""},
        {.path = POCKET_D20_DEFAULT_RACE_EQUIPMENT, .match = character->species},
        {.path = POCKET_D20_DEFAULT_BACKGROUND_EQUIPMENT, .match = character->background},
        {.path = POCKET_D20_DEFAULT_TRINKETS, .match = trinket_key},
    };
    int32_t currency[5] = {0, 0, 0, 0, 0};
    bool created = false;
    if(!pocket_d20_storage_create_items_from_assets(
           storage, profile, character, assets, 4U, currency, &created))
        return false;
    if(!created) return true;

    const int32_t previous_currency[5] = {
        character->currency_cp,
        character->currency_sp,
        character->currency_ep,
        character->currency_gp,
        character->currency_pp,
    };
    character->currency_cp = pocket_d20_items_add_saturated(character->currency_cp, currency[0]);
    character->currency_sp = pocket_d20_items_add_saturated(character->currency_sp, currency[1]);
    character->currency_ep = pocket_d20_items_add_saturated(character->currency_ep, currency[2]);
    character->currency_gp = pocket_d20_items_add_saturated(character->currency_gp, currency[3]);
    character->currency_pp = pocket_d20_items_add_saturated(character->currency_pp, currency[4]);

    if(!pocket_d20_storage_save_profile_updated(storage, profile, data)) {
        character->currency_cp = previous_currency[0];
        character->currency_sp = previous_currency[1];
        character->currency_ep = previous_currency[2];
        character->currency_gp = previous_currency[3];
        character->currency_pp = previous_currency[4];
        pocket_d20_storage_remove_live_items(storage, profile);
        return false;
    }
    if(initialized) *initialized = true;
    return true;
}

bool pocket_d20_items_grant_reward(
    Storage* storage,
    uint32_t profile,
    PocketCharacter* character,
    const char* name,
    const char* detail) {
    if(!storage || !character || !name || !name[0] || !strcmp(name, "-")) return false;

    uint8_t total = 0U;
    bool first_page = true;
    for(uint8_t start = 0U; first_page || start < total; start += POCKET_D20_COLLECTION_CACHE_SIZE) {
        first_page = false;
        if(!pocket_d20_storage_load_items_window(storage, profile, start, character, &total)) {
            pocket_d20_data_clear_items(character);
            return false;
        }
        for(uint8_t local = 0U; local < character->item_count; ++local) {
            if(strcmp(character->items[local].name, name)) continue;
            if(character->items[local].quantity < 999) {
                ++character->items[local].quantity;
                bool saved = pocket_d20_storage_save_items_window(storage, profile, start, character);
                pocket_d20_data_clear_items(character);
                return saved;
            }
            pocket_d20_data_clear_items(character);
            return true;
        }
        pocket_d20_data_clear_items(character);
    }

    if(total >= POCKET_D20_MAX_ITEMS) return true;
    if(!pocket_d20_storage_ensure_items_sidecar(storage, profile)) return false;
    PocketItem item;
    memset(&item, 0, sizeof(item));
    strncpy(item.name, name, sizeof(item.name) - 1U);
    item.name[sizeof(item.name) - 1U] = '\0';
    if(detail && detail[0]) {
        strncpy(item.detail, detail, sizeof(item.detail) - 1U);
        item.detail[sizeof(item.detail) - 1U] = '\0';
    }
    item.quantity = 1;
    item.container_index = -1;
    item.armor_dex_cap = -1;
    return pocket_d20_storage_append_item(storage, profile, character, &item);
}

int16_t pocket_d20_carrying_capacity(const PocketCharacter* character) {
    if(character->carrying_capacity_override > 0) return character->carrying_capacity_override;
    return (int16_t)character->ability_scores[PocketAbilityStrength] * 15;
}

void pocket_d20_normalize_currency(PocketCharacter* character) {
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

int8_t pocket_d20_weapon_ability(const PocketCharacter* character, const PocketItem* item) {
    int8_t strength =
        pocket_d20_ability_modifier(character->ability_scores[PocketAbilityStrength]);
    int8_t dexterity =
        pocket_d20_ability_modifier(character->ability_scores[PocketAbilityDexterity]);
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

int8_t
    pocket_d20_weapon_attack_modifier(const PocketCharacter* character, const PocketItem* item) {
    int8_t result = pocket_d20_weapon_ability(character, item) + item->magic_bonus;
    if(item->proficient) result += pocket_d20_proficiency_bonus(character);
    result += pocket_d20_exhaustion_penalty(character);
    return result;
}

PocketAttackRoll pocket_d20_roll_attack(
    const PocketCharacter* character,
    const PocketItem* item,
    PocketRollMode mode) {
    PocketAttackRoll result = {0};
    result.first_die = pocket_d20_roll_die(20U);
    result.natural_roll = result.first_die;
    if(mode == PocketRollAdvantage || mode == PocketRollDisadvantage) {
        result.second_die = pocket_d20_roll_die(20U);
        if(mode == PocketRollAdvantage) {
            if(result.second_die > result.natural_roll) result.natural_roll = result.second_die;
        } else if(result.second_die < result.natural_roll) {
            result.natural_roll = result.second_die;
        }
    }
    result.modifier = pocket_d20_weapon_attack_modifier(character, item);
    result.total = (int16_t)result.natural_roll + result.modifier;
    result.critical = result.natural_roll == 20U;
    result.automatic_miss = result.natural_roll == 1U;
    return result;
}

PocketDamageRoll pocket_d20_roll_damage(
    const PocketCharacter* character,
    const PocketItem* item,
    bool critical) {
    PocketDamageRoll result = {0};
    uint8_t multiplier = critical ? 2U : 1U;
    uint8_t die = item->damage_die;
    if(item->use_versatile && item->versatile_die >= 2U) die = item->versatile_die;
    result.weapon_roll_count = item->damage_dice * multiplier;
    result.extra_roll_count = item->extra_dice * multiplier;
    if(result.weapon_roll_count > POCKET_D20_MAX_DAMAGE_ROLLS)
        result.weapon_roll_count = POCKET_D20_MAX_DAMAGE_ROLLS;
    if(result.extra_roll_count > POCKET_D20_MAX_DAMAGE_ROLLS - result.weapon_roll_count)
        result.extra_roll_count = POCKET_D20_MAX_DAMAGE_ROLLS - result.weapon_roll_count;
    result.weapon_total = (int16_t)pocket_d20_roll_dice_values(
        result.weapon_roll_count, die, result.rolls, result.weapon_roll_count);
    result.extra_total = (int16_t)pocket_d20_roll_dice_values(
        result.extra_roll_count,
        item->extra_die,
        result.rolls + result.weapon_roll_count,
        result.extra_roll_count);
    result.modifier = item->magic_bonus;
    if(item->add_ability_damage) result.modifier += pocket_d20_weapon_ability(character, item);
    result.total = result.weapon_total + result.extra_total + result.modifier;
    if(result.total < 0) result.total = 0;
    result.critical = critical;
    return result;
}

int16_t pocket_d20_items_calculated_armor_class(
    const PocketCharacter* character,
    const PocketD20ItemAggregate* aggregate) {
    int16_t armor = 10;
    int16_t dexterity =
        pocket_d20_ability_modifier(character->ability_scores[PocketAbilityDexterity]);
    if(aggregate->armor_base) {
        int16_t dexterity_part = dexterity;
        if(aggregate->armor_dex_cap >= 0 && dexterity_part > aggregate->armor_dex_cap)
            dexterity_part = aggregate->armor_dex_cap;
        armor = aggregate->armor_base + dexterity_part;
    }
    return armor + aggregate->shield_bonus;
}

typedef struct {
    uint8_t* indices;
    uint8_t capacity;
    uint8_t count;
} PocketD20WeaponIndexContext;

static bool pocket_d20_weapon_index_visitor(
    uint8_t logical_index, const PocketItem* item, void* context) {
    PocketD20WeaponIndexContext* scan = context;
    if(item->is_weapon && scan->count < scan->capacity)
        scan->indices[scan->count++] = logical_index;
    return true;
}

bool pocket_d20_items_collect_weapon_indices(
    Storage* storage,
    uint32_t profile,
    uint8_t* indices,
    uint8_t capacity,
    uint8_t* count,
    uint8_t* total_count) {
    if(!storage || !indices || !count) return false;
    PocketD20WeaponIndexContext context = {
        .indices = indices,
        .capacity = capacity,
        .count = 0U,
    };
    uint8_t total = 0U;
    bool success = pocket_d20_storage_visit_items(
        storage, profile, pocket_d20_weapon_index_visitor, &context, &total);
    *count = context.count;
    if(total_count) *total_count = total;
    return success;
}
