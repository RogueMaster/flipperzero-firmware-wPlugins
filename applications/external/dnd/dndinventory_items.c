#include "dndinventory_internal.h"

#include <furi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define POCKET_D20_DEFAULT_CLASS_EQUIPMENT      APP_ASSETS_PATH("equipment/default_class.txt")
#define POCKET_D20_DEFAULT_RACE_EQUIPMENT       APP_ASSETS_PATH("equipment/default_race.txt")
#define POCKET_D20_DEFAULT_BACKGROUND_EQUIPMENT APP_ASSETS_PATH("equipment/default_background.txt")
#define POCKET_D20_DEFAULT_TRINKETS             APP_ASSETS_PATH("equipment/trinkets.txt")

bool dndinventory_items_initialize_inventory(
    Storage* storage,
    uint32_t profile,
    DndInventoryCharacterState* character,
    bool* initialized) {
    if(initialized) *initialized = false;
    if(!storage || !character) return false;
    if(dnd_storage_items_exist(storage, profile)) {
        bool granted = false;
        if(!dnd_storage_inventory_initial_granted(storage, profile, &granted)) return false;
        if(granted) return true;
        uint8_t existing_items = 0U;
        if(!dnd_storage_visit_items(storage, profile, NULL, NULL, &existing_items)) return false;
        if(existing_items) return true;

        /* Currency-only inventory files are valid now that Inventory owns
           money. Load that authoritative balance before replacing the empty
           sidecar with the starting-equipment transaction. */
        int32_t existing_currency[5];
        bool currency_found = false;
        if(!dnd_storage_load_inventory_currency(
               storage, profile, existing_currency, &currency_found))
            return false;
        if(currency_found) {
            character->currency_cp = existing_currency[0];
            character->currency_sp = existing_currency[1];
            character->currency_ep = existing_currency[2];
            character->currency_gp = existing_currency[3];
            character->currency_pp = existing_currency[4];
        }
        if(!dnd_storage_remove_live_items(storage, profile)) return false;
    }

    PocketCharacter* owner = calloc(1U, sizeof(PocketCharacter));
    if(!owner) return false;
    strncpy(owner->name, character->name, sizeof(owner->name) - 1U);
    owner->class_count = character->class_count;
    for(uint8_t i = 0U; i < character->class_count && i < POCKET_D20_MAX_CLASSES; ++i)
        owner->classes[i] = character->classes[i];
    owner->currency_cp = character->currency_cp;
    owner->currency_sp = character->currency_sp;
    owner->currency_ep = character->currency_ep;
    owner->currency_gp = character->currency_gp;
    owner->currency_pp = character->currency_pp;

    PocketD20ItemSeedAsset assets[3] = {
        {.path = POCKET_D20_DEFAULT_CLASS_EQUIPMENT,
         .match = character->class_count ? character->classes[0].name : ""},
        {.path = POCKET_D20_DEFAULT_RACE_EQUIPMENT, .match = character->species},
        {.path = POCKET_D20_DEFAULT_BACKGROUND_EQUIPMENT, .match = character->background},
    };
    const int32_t starting_currency[5] = {
        character->currency_cp,
        character->currency_sp,
        character->currency_ep,
        character->currency_gp,
        character->currency_pp,
    };
    int32_t currency_total[5];
    memcpy(currency_total, starting_currency, sizeof(currency_total));
    bool created = false;
    bool composed = dnd_storage_create_items_from_assets(
        storage, profile, owner, assets, 3U, currency_total, &created);

    uint8_t seeded_items = 0U;
    bool granted_currency = false;
    for(uint8_t i = 0U; i < 5U; ++i)
        if(currency_total[i] != starting_currency[i]) granted_currency = true;
    bool have_seeded_equipment =
        composed && created &&
        dnd_storage_visit_items(storage, profile, NULL, NULL, &seeded_items) &&
        (seeded_items > 0U || granted_currency);
    if(!have_seeded_equipment) {
        dnd_storage_remove_live_items(storage, profile);
        memcpy(currency_total, starting_currency, sizeof(currency_total));
        char trinket_key[4];
        uint8_t trinket_roll = dnd_rules_core_roll_die(100U);
        snprintf(trinket_key, sizeof(trinket_key), "%u", trinket_roll);
        PocketD20ItemSeedAsset fallback = {
            .path = POCKET_D20_DEFAULT_TRINKETS,
            .match = trinket_key,
        };
        created = false;
        if(!dnd_storage_create_items_from_assets(
               storage, profile, owner, &fallback, 1U, currency_total, &created)) {
            free(owner);
            return false;
        }
    }
    if(!created) {
        free(owner);
        return true;
    }

    /* Items, grant marker, and the final existing+granted balance are now
       committed by the same synced sidecar write. Mirror that exact committed
       balance in RAM instead of performing a second metadata rewrite. */
    character->currency_cp = currency_total[0];
    character->currency_sp = currency_total[1];
    character->currency_ep = currency_total[2];
    character->currency_gp = currency_total[3];
    character->currency_pp = currency_total[4];
    if(initialized) *initialized = true;
    free(owner);
    return true;
}

bool dndinventory_items_regrant_inventory_once(
    Storage* storage,
    uint32_t profile,
    DndInventoryCharacterState* character,
    bool* regranted) {
    if(regranted) *regranted = false;
    if(!storage || !character || !regranted) return false;

    uint8_t grant_state = 0U;
    if(!dnd_storage_inventory_initial_grant_state(storage, profile, &grant_state)) return false;
    if(grant_state != 1U) return true;

    PocketCharacter* owner = calloc(1U, sizeof(PocketCharacter));
    if(!owner) return false;
    strncpy(owner->name, character->name, sizeof(owner->name) - 1U);
    owner->class_count = character->class_count;
    for(uint8_t i = 0U; i < character->class_count && i < POCKET_D20_MAX_CLASSES; ++i)
        owner->classes[i] = character->classes[i];
    owner->currency_cp = character->currency_cp;
    owner->currency_sp = character->currency_sp;
    owner->currency_ep = character->currency_ep;
    owner->currency_gp = character->currency_gp;
    owner->currency_pp = character->currency_pp;

    PocketD20ItemSeedAsset assets[3] = {
        {.path = POCKET_D20_DEFAULT_CLASS_EQUIPMENT,
         .match = character->class_count ? character->classes[0].name : ""},
        {.path = POCKET_D20_DEFAULT_RACE_EQUIPMENT, .match = character->species},
        {.path = POCKET_D20_DEFAULT_BACKGROUND_EQUIPMENT, .match = character->background},
    };
    char trinket_key[4];
    uint8_t trinket_roll = dnd_rules_core_roll_die(100U);
    snprintf(trinket_key, sizeof(trinket_key), "%u", trinket_roll);
    PocketD20ItemSeedAsset fallback = {
        .path = POCKET_D20_DEFAULT_TRINKETS,
        .match = trinket_key,
    };
    int32_t currency_total[5] = {0, 0, 0, 0, 0};
    bool applied = false;
    if(!dnd_storage_regrant_items_from_assets(
           storage, profile, owner, assets, 3U, &fallback, currency_total, &applied)) {
        free(owner);
        return false;
    }
    if(!applied) {
        free(owner);
        return true;
    }

    character->currency_cp = currency_total[0];
    character->currency_sp = currency_total[1];
    character->currency_ep = currency_total[2];
    character->currency_gp = currency_total[3];
    character->currency_pp = currency_total[4];
    *regranted = true;
    free(owner);
    return true;
}
