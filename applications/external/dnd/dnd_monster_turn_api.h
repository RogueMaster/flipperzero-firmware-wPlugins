#pragma once
#include "dnd_plugin_loader.h"
#include "dndbestiary_monsters.h"
#define DND_MONSTER_TURN_API_ID      "dnd_monster_turn"
#define DND_MONSTER_TURN_API_VERSION 1U
#define DND_MONSTER_TURN_INITIATIVE_PATH \
    "/ext/apps_data/dndinitiative/plugins/dnd_monster_turn.fal"
#define DND_MONSTER_TURN_BESTIARY_PATH "/ext/apps_data/dndbestiary/plugins/dnd_monster_turn.fal"
typedef struct {
    uint32_t size;
    DndPluginUiResult (*run)(
        ViewDispatcher* dispatcher,
        Storage* storage,
        const DndMonsterDetail* detail,
        const char* name,
        bool allow_homebrew,
        bool start_with_attacks,
        bool loading_on_return);
} DndMonsterTurnApi;
