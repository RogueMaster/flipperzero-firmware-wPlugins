#include "look_text.h"

#include "actor_state.h"
#include "inventory_state.h"
#include "item_defs.h"
#include "map_state.h"
#include "monster_defs.h"
#include "perception.h"
#include "terrain_effects.h"
#include "terrain_view.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

static void fr_append_status(char* text, size_t size, size_t* pos, const char* status, uint8_t* count) {
    if(*count >= 2 || *pos >= size) return;
    int written = snprintf(text + *pos, size - *pos, " %s.", status);
    if(written < 0) return;
    *pos = (size_t)(*pos + (size_t)written);
    (*count)++;
}

static void fr_append_effect_statuses(
    char* text,
    size_t size,
    size_t* pos,
    uint8_t effects,
    bool ward,
    uint8_t* count) {
    if((effects & FR_FX_BURNING) != 0) fr_append_status(text, size, pos, "Burning", count);
    if(ward) fr_append_status(text, size, pos, "Ward", count);
    if((effects & FR_FX_CONFUSED) != 0) fr_append_status(text, size, pos, "Confused", count);
    if((effects & FR_FX_POISONED) != 0) fr_append_status(text, size, pos, "Poisoned", count);
    if((effects & FR_FX_SLOWED) != 0) fr_append_status(text, size, pos, "Slowed", count);
    if((effects & FR_FX_STUNNED) != 0) fr_append_status(text, size, pos, "Stunned", count);
    if((effects & FR_FX_AFRAID) != 0) fr_append_status(text, size, pos, "Afraid", count);
    if((effects & FR_FX_BLIND) != 0) fr_append_status(text, size, pos, "Blind", count);
}

static const FrItem* fr_look_item_at(const FrGame* game, uint8_t x, uint8_t y) {
    for(uint8_t i = 0; i < FR_MAX_ITEMS; i++) {
        const FrItem* item = &game->items[i];
        if(item->active && item->x == x && item->y == y) return item;
    }
    return NULL;
}

const char* fr_look_text(FrGame* game, uint8_t x, uint8_t y) {
    static char text[64];
    if(x == game->player.x && y == game->player.y) {
        size_t pos = (size_t)snprintf(text, sizeof(text), "You.");
        uint8_t count = 0;
        fr_append_effect_statuses(
            text,
            sizeof(text),
            &pos,
            game->player.effects,
            game->player.fire_ward_timer > 0,
            &count);
        return text;
    }

    FrActor* actor = fr_actor_at(game, x, y);
    if(actor && fr_actor_visible_to_player(game, actor)) {
        size_t pos = (size_t)snprintf(text, sizeof(text), "%s.", fr_actor_name(actor->type));
        uint8_t count = 0;
        fr_append_effect_statuses(text, sizeof(text), &pos, actor->effects, false, &count);
        if(count == 0) snprintf(text, sizeof(text), "%s: %s", fr_actor_name(actor->type), fr_actor_flavor(actor->type));
        return text;
    }

    const FrItem* item = fr_look_item_at(game, x, y);
    if(item) {
        static FrInvSlot slot;
        slot.type = item->type;
        slot.subtype = item->subtype;
        slot.amount = item->amount;
        slot.flags = 0;
        return fr_inventory_label(game, &slot);
    }

    if(fr_terrain_fire_at(game, game->floor, x, y)) return "Ground. Burning.";
    uint8_t terrain = fr_get_terrain(game, x, y);
    if(terrain == FR_TERR_WATER) return "Water.";
    if(terrain == FR_TERR_ICE) return "Ice.";
    return fr_terrain_name(terrain);
}
