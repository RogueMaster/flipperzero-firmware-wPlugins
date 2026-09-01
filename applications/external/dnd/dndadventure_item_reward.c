#include "dndadventure_item_reward.h"

#include <string.h>

bool dndadventure_item_reward_grant_reward(
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
        if(!dnd_storage_load_items_window(storage, profile, start, character, &total)) {
            dnd_data_clear_items(character);
            return false;
        }
        for(uint8_t local = 0U; local < character->item_count; ++local) {
            if(strcmp(character->items[local].name, name)) continue;
            if(character->items[local].quantity < 999) {
                ++character->items[local].quantity;
                bool saved = dnd_storage_save_items_window(storage, profile, start, character);
                dnd_data_clear_items(character);
                return saved;
            }
            dnd_data_clear_items(character);
            return true;
        }
        dnd_data_clear_items(character);
    }

    if(total >= POCKET_D20_MAX_ITEMS) return true;
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
    return dnd_storage_append_item(storage, profile, character, &item);
}
