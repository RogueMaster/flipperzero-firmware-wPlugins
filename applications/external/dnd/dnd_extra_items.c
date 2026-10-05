#include "dnd_extra_items.h"
#include <furi_hal_random.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t dnd_extra_items_roll(uint8_t sides) {
    return (uint8_t)(furi_hal_random_get() % sides);
}

bool dnd_extra_items_grant(Storage* storage, uint32_t profile, const DndCharacter* owner) {
    if(!storage || !owner) return false;
    if(!dnd_storage_items_exist(storage, profile)) return true;
    static const char* const accessories[] = {
        "Old Pipe & Lighter",
        "Small Bong & Lighter",
        "One Hitter & Lighter",
        "Gandalf Pipe & Lighter",
        "Puffco Peak",
        "Blue Dream Vape Pen"};
    static const char* const strains[] = {
        "Blue Dream",
        "Girl Scout Cookies",
        "Wedding Cake",
        "Sour Diesel",
        "Pineapple Express",
        "Lemon Cherry Gelato"};
    DndItem* items = calloc(3U, sizeof(DndItem));
    if(!items) return false;
    for(uint8_t i = 0U; i < 3U; ++i) {
        items[i].container_index = -1;
        items[i].quantity = 1;
    }
    uint8_t accessory = dnd_extra_items_roll(6U);
    snprintf(items[0].name, sizeof(items[0].name), "%s", accessories[accessory]);
    uint8_t count = 1U;
    if(accessory < 5U) {
        uint8_t first = dnd_extra_items_roll(6U);
        bool rosin = accessory == 4U;
        snprintf(
            items[1].name,
            sizeof(items[1].name),
            "%s %s (1 gram)",
            strains[first],
            rosin ? "Live Rosin" : "Premium Flower");
        items[1].quantity = rosin ? 2 + dnd_extra_items_roll(8U) : 5 + dnd_extra_items_roll(33U);
        count = 2U;
        if(!rosin) {
            uint8_t second = dnd_extra_items_roll(6U);
            if(second == first) second = (second + 1U) % 6U;
            snprintf(
                items[2].name,
                sizeof(items[2].name),
                "%s Premium Flower (1 gram)",
                strains[second]);
            items[2].quantity = 5 + dnd_extra_items_roll(33U);
            count = 3U;
        }
    }
    bool ok = dnd_storage_append_items(storage, profile, owner, items, count);
    free(items);
    return ok;
}
