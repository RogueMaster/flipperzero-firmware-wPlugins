#include "dndadventure_item_reward.h"

#include <stdlib.h>
#include <string.h>

bool dndadventure_item_reward_grant_reward(
    Storage* storage,
    uint32_t profile,
    const DndAdventureProfileProjection* projection,
    const char* name,
    const char* detail) {
    if(!storage || !projection || !name || !name[0] || !strcmp(name, "-")) return false;

    /* Collection storage still accepts the canonical PocketCharacter owner shape.
       Adventure creates it only for this bounded I/O operation; it is never part
       of resident app state. */
    PocketCharacter* character = calloc(1U, sizeof(PocketCharacter));
    if(!character) return false;
    strncpy(character->name, projection->name, sizeof(character->name) - 1U);
    character->class_count = projection->class_count;
    for(uint8_t i = 0U; i < projection->class_count && i < POCKET_D20_MAX_CLASSES; ++i)
        character->classes[i].level = projection->class_levels[i];

    bool result = false;
    uint8_t total = 0U;
    bool first_page = true;
    for(uint8_t start = 0U; first_page || start < total; start += POCKET_D20_COLLECTION_CACHE_SIZE) {
        first_page = false;
        if(!dnd_storage_load_items_window(storage, profile, start, character, &total)) goto done;
        for(uint8_t local = 0U; local < character->item_count; ++local) {
            if(strcmp(character->items[local].name, name)) continue;
            if(character->items[local].quantity < 999) {
                ++character->items[local].quantity;
                result = dnd_storage_save_items_window(storage, profile, start, character);
            } else {
                result = true;
            }
            goto done;
        }
        dnd_data_clear_items(character);
    }

    if(total >= POCKET_D20_MAX_ITEMS) {
        result = true;
        goto done;
    }
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
    result = dnd_storage_append_item(storage, profile, character, &item);

done:
    dnd_data_clear_items(character);
    free(character);
    return result;
}
