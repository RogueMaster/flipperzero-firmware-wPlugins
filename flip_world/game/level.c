#include <game/level.h>
#include <flip_storage/storage.h>
#include <game/storage.h>
#include <callback/game.h>
bool allocate_level(GameManager *manager, int index)
{
    GameContext *game_context = game_manager_game_context_get(manager);

    // open the world list from storage, then create a level for each world
    char file_path[128];
    snprintf(file_path, sizeof(file_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds/world_list.json");
    FuriString *world_list = flipper_http_load_from_file(file_path);
    if (!world_list)
    {
        FURI_LOG_E("Game", "Failed to load world list");
        game_context->levels[0] = game_manager_add_level(manager, story_world());
        game_context->level_count = 1;
        return false;
    }
    FuriString *world_name = get_json_array_value_furi("worlds", index, world_list);
    if (!world_name)
    {
        FURI_LOG_E("Game", "Failed to get world name");
        furi_string_free(world_list);
        return false;
    }
    FURI_LOG_I("Game", "Allocating level %d for world %s", index, furi_string_get_cstr(world_name));
    game_context->levels[index] = game_manager_add_level(manager, generic_level(furi_string_get_cstr(world_name), index));
    furi_string_free(world_name);
    furi_string_free(world_list);
    return true;
}
void level_set_world(Level *level, GameManager *manager, char *id)
{
    char file_path[256];
    snprintf(file_path, sizeof(file_path),
             STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds/%s/%s_json_data.json",
             id, id);

    FuriString *json_data_str = flipper_http_load_from_file(file_path);
    if (!json_data_str || furi_string_empty(json_data_str))
    {
        FURI_LOG_E("Game", "Failed to load json data from file");
        if (json_data_str)
            furi_string_free(json_data_str);
        return;
    }

    if (!is_enough_heap(20000, true))
    {
        FURI_LOG_E("Game", "level_set_world: Not enough heap memory.. ending game early.");
        GameContext *game_context = game_manager_game_context_get(manager);
        game_context->end_reason = GAME_END_MEMORY;
        game_context->ended_early = true;
        game_manager_game_stop(manager); // end game early
        furi_string_free(json_data_str);
        return;
    }

    FURI_LOG_I("Game", "Drawing world");
    if (!world_json_draw(manager, level, json_data_str))
    {
        FURI_LOG_E("Game", "Failed to draw world");
        furi_string_free(json_data_str);
    }
    else
    {
        FURI_LOG_I("Game", "Drawing enemies");
        furi_string_free(json_data_str);
        snprintf(file_path, sizeof(file_path),
                 STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds/%s/%s_enemy_data.json",
                 id, id);

        FuriString *enemy_data_str = flipper_http_load_from_file(file_path);
        if (!enemy_data_str || furi_string_empty(enemy_data_str))
        {
            FURI_LOG_E("Game", "Failed to get enemy data");
            if (enemy_data_str)
                furi_string_free(enemy_data_str);
            return;
        }

        // Loop through the array
        for (int i = 0; i < MAX_ENEMIES; i++)
        {
            FuriString *single_enemy_data = get_json_array_value_furi("enemy_data", i, enemy_data_str);
            if (!single_enemy_data || furi_string_empty(single_enemy_data))
            {
                // No more enemy elements found
                if (single_enemy_data)
                    furi_string_free(single_enemy_data);
                break;
            }

            enemy_spawn(level, manager, single_enemy_data);
            furi_string_free(single_enemy_data);
        }
        furi_string_free(enemy_data_str);

        // Draw NPCs
        FURI_LOG_I("Game", "Drawing NPCs");
        snprintf(file_path, sizeof(file_path),
                 STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds/%s/%s_npc_data.json",
                 id, id);

        FuriString *npc_data_str = flipper_http_load_from_file(file_path);
        if (!npc_data_str || furi_string_empty(npc_data_str))
        {
            FURI_LOG_E("Game", "Failed to get npc data");
            if (npc_data_str)
                furi_string_free(npc_data_str);
            return;
        }

        // Loop through the array
        for (int i = 0; i < MAX_NPCS; i++)
        {
            FuriString *single_npc_data = get_json_array_value_furi("npc_data", i, npc_data_str);
            if (!single_npc_data || furi_string_empty(single_npc_data))
            {
                // No more npc elements found
                if (single_npc_data)
                    furi_string_free(single_npc_data);
                break;
            }

            npc_spawn(level, manager, single_npc_data);
            furi_string_free(single_npc_data);
        }
        furi_string_free(npc_data_str);

        FURI_LOG_I("Game", "World drawn");
    }
}
static void level_start(Level *level, GameManager *manager, void *context)
{
    if (!manager || !level || !context)
    {
        FURI_LOG_E("Game", "Manager, level, or context is NULL");
        return;
    }
    GameContext *game_context = game_manager_game_context_get(manager);
    if (!level || !context)
    {
        FURI_LOG_E("Game", "Level, context, or manager is NULL");
        game_context->is_switching_level = false;
        return;
    }

    level_clear(level);

    LevelContext *level_context = context;
    if (!level_context)
    {
        FURI_LOG_E("Game", "Level context is NULL");
        game_context->is_switching_level = false;
        return;
    }

    // check if the world exists
    if (!world_exists(level_context->id))
    {
        FURI_LOG_E("Game", "World does not exist.. downloading now");
        FuriString *world_data = NULL;
        if (game_context->game_mode != GAME_MODE_STORY)
        {
            if (game_context->fhttp)
            {
                flipper_http_free(game_context->fhttp);
            }
            game_context->fhttp = flipper_http_alloc();
            if (!game_context->fhttp)
            {
                FURI_LOG_E("Game", "Failed to allocate FlipperHTTP");
                game_context->is_switching_level = false;
                game_context->ended_early = true;
                game_context->end_reason = GAME_END_MEMORY;
                player_spawn(level, manager);
                return;
            }
            flipper_http_websocket_stop(game_context->fhttp); // close websocket if open
            furi_delay_ms(200);
            world_data = world_fetch(game_context->fhttp, level_context->id);
            furi_delay_ms(200);
            game_start_ws(game_context->fhttp, game_ws_lobby_name); // start websocket again
        }
        else
        {
            FlipperHTTP *fhttp = flipper_http_alloc();
            if (!fhttp)
            {
                FURI_LOG_E("Game", "Failed to allocate FlipperHTTP");
                game_context->is_switching_level = false;
                game_context->ended_early = true;
                game_context->end_reason = GAME_END_MEMORY;
                player_spawn(level, manager);
                return;
            }
            world_data = world_fetch(fhttp, "pvp_world");
            flipper_http_free(fhttp);
        }
        if (!world_data)
        {
            FURI_LOG_E("Game", "Failed to fetch world data");
            // draw_town_world(manager, level);
            game_context->is_switching_level = false;
            game_context->ended_early = true;
            game_context->end_reason = GAME_END_NETWORK;
            // furi_delay_ms(1000);
            player_spawn(level, manager);
            return;
        }
        furi_string_free(world_data);

        level_set_world(level, manager, level_context->id);
        FURI_LOG_I("Game", "World set.");
        // furi_delay_ms(1000);
        game_context->is_switching_level = false;
    }
    else
    {
        FURI_LOG_I("Game", "World exists.. loading now");
        level_set_world(level, manager, level_context->id);
        FURI_LOG_I("Game", "World set.");
        // furi_delay_ms(1000);
        game_context->is_switching_level = false;
    }

    game_context->icon_offset = 0;
    if (!game_context->imu_present)
    {
        game_context->icon_offset += ((game_context->icon_count / 10) / 15);
    }
    player_spawn(level, manager);
}

static LevelContext *level_context_generic;

static LevelContext *level_generic_alloc(const char *id, int index)
{
    if (level_context_generic == NULL)
    {
        size_t heap_size = memmgr_get_free_heap();
        if (heap_size < sizeof(LevelContext))
        {
            FURI_LOG_E("Game", "Not enough heap to allocate level context");
            return NULL;
        }
        level_context_generic = malloc(sizeof(LevelContext));
    }
    snprintf(level_context_generic->id, sizeof(level_context_generic->id), "%s", id);
    level_context_generic->index = index;
    return level_context_generic;
}

static void level_generic_free()
{
    if (level_context_generic != NULL)
    {
        free(level_context_generic);
        level_context_generic = NULL;
    }
}

static void free_level(Level *level, GameManager *manager, void *context)
{
    UNUSED(level);
    UNUSED(manager);
    UNUSED(context);
    level_generic_free();
}

static void level_alloc_generic_world(Level *level, GameManager *manager, void *context)
{
    UNUSED(manager);
    UNUSED(level);
    if (!level_context_generic)
    {
        FURI_LOG_E("Game", "Generic level context not set");
        return;
    }
    if (!context)
    {
        FURI_LOG_E("Game", "Context is NULL");
        return;
    }
    LevelContext *level_context = context;
    snprintf(level_context->id, sizeof(level_context->id), "%s", level_context_generic->id);
    level_context->index = level_context_generic->index;
}

const LevelBehaviour _generic_level = {
    .alloc = level_alloc_generic_world,
    .free = free_level,
    .start = level_start,
    .stop = NULL,
    .context_size = sizeof(LevelContext),
};

const LevelBehaviour *generic_level(const char *id, int index)
{
    // free any old context before allocating a new one
    level_generic_free();
    level_context_generic = level_generic_alloc(id, index);
    return &_generic_level;
}
