#include <game/world.h>
#include <game/storage.h>
#include <flip_storage/storage.h>
#include "game/icon.h"

static IconSpec world_get_icon_spec(const char *name)
{
    if (is_str(name, "house"))
        return (IconSpec){.id = ICON_ID_HOUSE, .icon = &I_icon_house_48x32px, .size = (Vector){48, 32}};
    else if (is_str(name, "man"))
        return (IconSpec){.id = ICON_ID_MAN, .icon = &I_icon_man_7x16, .size = (Vector){7, 16}};
    else if (is_str(name, "plant"))
        return (IconSpec){.id = ICON_ID_PLANT, .icon = &I_icon_plant_16x16, .size = (Vector){16, 16}};
    else if (is_str(name, "tree"))
        return (IconSpec){.id = ICON_ID_TREE, .icon = &I_icon_tree_16x16, .size = (Vector){16, 16}};
    else if (is_str(name, "woman"))
        return (IconSpec){.id = ICON_ID_WOMAN, .icon = &I_icon_woman_9x16, .size = (Vector){9, 16}};
    else if (is_str(name, "fence"))
        return (IconSpec){.id = ICON_ID_FENCE, .icon = &I_icon_fence_16x8px, .size = (Vector){16, 8}};
    else if (is_str(name, "fence_end"))
        return (IconSpec){.id = ICON_ID_FENCE_END, .icon = &I_icon_fence_end_16x8px, .size = (Vector){16, 8}};
    else if (is_str(name, "fence_vertical_end"))
        return (IconSpec){.id = ICON_ID_FENCE_VERTICAL_END, .icon = &I_icon_fence_vertical_end_6x8px, .size = (Vector){6, 8}};
    else if (is_str(name, "fence_vertical_start"))
        return (IconSpec){.id = ICON_ID_FENCE_VERTICAL_START, .icon = &I_icon_fence_vertical_start_6x15px, .size = (Vector){6, 15}};
    else if (is_str(name, "flower"))
        return (IconSpec){.id = ICON_ID_FLOWER, .icon = &I_icon_flower_16x16, .size = (Vector){16, 16}};
    else if (is_str(name, "lake_bottom"))
        return (IconSpec){.id = ICON_ID_LAKE_BOTTOM, .icon = &I_icon_lake_bottom_31x12px, .size = (Vector){31, 12}};
    else if (is_str(name, "lake_bottom_left"))
        return (IconSpec){.id = ICON_ID_LAKE_BOTTOM_LEFT, .icon = &I_icon_lake_bottom_left_24x22px, .size = (Vector){24, 22}};
    else if (is_str(name, "lake_bottom_right"))
        return (IconSpec){.id = ICON_ID_LAKE_BOTTOM_RIGHT, .icon = &I_icon_lake_bottom_right_24x22px, .size = (Vector){24, 22}};
    else if (is_str(name, "lake_left"))
        return (IconSpec){.id = ICON_ID_LAKE_LEFT, .icon = &I_icon_lake_left_11x31px, .size = (Vector){11, 31}};
    else if (is_str(name, "lake_right"))
        return (IconSpec){.id = ICON_ID_LAKE_RIGHT, .icon = &I_icon_lake_right_11x31, .size = (Vector){11, 31}};
    else if (is_str(name, "lake_top"))
        return (IconSpec){.id = ICON_ID_LAKE_TOP, .icon = &I_icon_lake_top_31x12px, .size = (Vector){31, 12}};
    else if (is_str(name, "lake_top_left"))
        return (IconSpec){.id = ICON_ID_LAKE_TOP_LEFT, .icon = &I_icon_lake_top_left_24x22px, .size = (Vector){24, 22}};
    else if (is_str(name, "lake_top_right"))
        return (IconSpec){.id = ICON_ID_LAKE_TOP_RIGHT, .icon = &I_icon_lake_top_right_24x22px, .size = (Vector){24, 22}};
    else if (is_str(name, "rock_large"))
        return (IconSpec){.id = ICON_ID_ROCK_LARGE, .icon = &I_icon_rock_large_18x19px, .size = (Vector){18, 19}};
    else if (is_str(name, "rock_medium"))
        return (IconSpec){.id = ICON_ID_ROCK_MEDIUM, .icon = &I_icon_rock_medium_16x14px, .size = (Vector){16, 14}};
    else if (is_str(name, "rock_small"))
        return (IconSpec){.id = ICON_ID_ROCK_SMALL, .icon = &I_icon_rock_small_10x8px, .size = (Vector){10, 8}};

    return (IconSpec){.id = -1, .icon = NULL, .size = (Vector){0, 0}};
}

bool world_json_draw(GameManager *manager, Level *level, const FuriString *json_data)
{
    if (!json_data)
    {
        FURI_LOG_E("Game", "JSON data is NULL");
        return false;
    }

    // Pass 1: Count the total number of icons.
    int total_icons = 0;
    for (int i = 0; i < MAX_WORLD_OBJECTS; i++)
    {
        FuriString *data = get_json_array_value_furi("json_data", i, json_data);
        if (!data)
            break;
        FuriString *amount = get_json_value_furi("a", data);
        if (amount)
        {
            int count = atoi(furi_string_get_cstr(amount));
            if (count < 1)
                count = 1;
            total_icons += count;
            furi_string_free(amount);
        }
        furi_string_free(data);
    }
    FURI_LOG_I("Game", "Total icons to spawn: %d", total_icons);

    // Allocate the icon group context (local instance)
    IconGroupContext igctx;
    igctx.count = total_icons;
    igctx.icons = malloc(total_icons * sizeof(IconSpec));
    if (!igctx.icons)
    {
        FURI_LOG_E("Game", "Failed to allocate icon group array for %d icons", total_icons);
        return false;
    }
    GameContext *game_context = game_manager_game_context_get(manager);
    game_context->icon_count = total_icons;

    // Pass 2: Parse the JSON to fill the icon specs.
    int spec_index = 0;
    for (int i = 0; i < MAX_WORLD_OBJECTS; i++)
    {
        FuriString *data = get_json_array_value_furi("json_data", i, json_data);
        if (!data)
            break;

        /*
        i - icon name
        x - x position
        y - y position
        a - amount
        h - horizontal (true/false)
        */

        FuriString *icon_str = get_json_value_furi("i", data);
        FuriString *x_str = get_json_value_furi("x", data);
        FuriString *y_str = get_json_value_furi("y", data);
        FuriString *amount_str = get_json_value_furi("a", data);
        FuriString *horizontal_str = get_json_value_furi("h", data);

        if (!icon_str || !x_str || !y_str || !amount_str || !horizontal_str)
        {
            FURI_LOG_E("Game", "Incomplete icon data: %s", furi_string_get_cstr(data));
            if (icon_str)
                furi_string_free(icon_str);
            if (x_str)
                furi_string_free(x_str);
            if (y_str)
                furi_string_free(y_str);
            if (amount_str)
                furi_string_free(amount_str);
            if (horizontal_str)
                furi_string_free(horizontal_str);
            furi_string_free(data);
            continue;
        }

        int count = atoi(furi_string_get_cstr(amount_str));
        if (count < 1)
            count = 1;
        float base_x = atof_furi(x_str);
        float base_y = atof_furi(y_str);
        bool is_horizontal = (furi_string_cmp(horizontal_str, "true") == 0);
        int spacing = 17;

        for (int j = 0; j < count; j++)
        {
            IconSpec spec = world_get_icon_spec(furi_string_get_cstr(icon_str));
            if (!spec.icon)
            {
                FURI_LOG_E("Game", "Icon name not recognized: %s", furi_string_get_cstr(icon_str));
                continue;
            }
            if (is_horizontal)
            {
                spec.pos.x = base_x + (j * spacing);
                spec.pos.y = base_y;
            }
            else
            {
                spec.pos.x = base_x;
                spec.pos.y = base_y + (j * spacing);
            }
            igctx.icons[spec_index++] = spec;
        }

        furi_string_free(icon_str);
        furi_string_free(x_str);
        furi_string_free(y_str);
        furi_string_free(amount_str);
        furi_string_free(horizontal_str);
        furi_string_free(data);
    }

    // Spawn one icon group entity.
    Entity *groupEntity = level_add_entity(level, &icon_desc);
    IconGroupContext *entityContext = (IconGroupContext *)entity_context_get(groupEntity);
    if (entityContext)
    {
        memcpy(entityContext, &igctx, sizeof(IconGroupContext));
    }
    else
    {
        FURI_LOG_E("Game", "Failed to get entity context for icon group");
        free(igctx.icons);
        return false;
    }

    // Set the global pointer so that player collision logic can use it.
    g_current_icon_group = entityContext;

    FURI_LOG_I("Game", "Finished loading world data");
    return true;
}

static void world_draw_pvp(Level *level, GameManager *manager, void *context)
{
    UNUSED(context);
    if (!manager || !level)
    {
        FURI_LOG_E("Game", "Manager or level is NULL");
        return;
    }
    GameContext *game_context = game_manager_game_context_get(manager);
    level_clear(level);
    FuriString *json_data_str = furi_string_alloc();
    furi_string_cat_str(json_data_str, "{\"name\":\"pvp_world\",\"author\":\"ChatGPT\",\"json_data\":[{\"i\":\"rock_medium\",\"x\":100,\"y\":100,\"a\":10,\"h\":true},{\"i\":\"rock_medium\",\"x\":400,\"y\":300,\"a\":6,\"h\":true},{\"i\":\"rock_small\",\"x\":600,\"y\":200,\"a\":8,\"h\":true},{\"i\":\"fence\",\"x\":50,\"y\":50,\"a\":10,\"h\":true},{\"i\":\"fence\",\"x\":250,\"y\":150,\"a\":12,\"h\":true},{\"i\":\"fence\",\"x\":550,\"y\":350,\"a\":12,\"h\":true},{\"i\":\"rock_large\",\"x\":400,\"y\":70,\"a\":12,\"h\":true},{\"i\":\"rock_large\",\"x\":200,\"y\":200,\"a\":6,\"h\":false},{\"i\":\"tree\",\"x\":5,\"y\":5,\"a\":45,\"h\":true},{\"i\":\"tree\",\"x\":5,\"y\":5,\"a\":20,\"h\":false},{\"i\":\"tree\",\"x\":22,\"y\":22,\"a\":44,\"h\":true},{\"i\":\"tree\",\"x\":22,\"y\":22,\"a\":20,\"h\":false},{\"i\":\"tree\",\"x\":5,\"y\":347,\"a\":45,\"h\":true},{\"i\":\"tree\",\"x\":5,\"y\":364,\"a\":45,\"h\":true},{\"i\":\"tree\",\"x\":735,\"y\":37,\"a\":18,\"h\":false},{\"i\":\"tree\",\"x\":752,\"y\":37,\"a\":18,\"h\":false}]}");
    if (!separate_world_data("pvp_world", json_data_str))
    {
        FURI_LOG_E("Game", "Failed to separate world data");
    }
    furi_string_free(json_data_str);
    level_set_world(level, manager, "pvp_world");
    game_context->is_switching_level = false;
    game_context->icon_offset = 0;
    if (!game_context->imu_present)
    {
        game_context->icon_offset += ((game_context->icon_count / 10) / 15);
    }
    player_spawn(level, manager);
}

static const LevelBehaviour _world_pvp = {
    .alloc = NULL,
    .free = NULL,
    .start = world_draw_pvp,
    .stop = NULL,
    .context_size = 0,
};

const LevelBehaviour *world_pvp()
{
    return &_world_pvp;
}

FuriString *world_fetch(FlipperHTTP *fhttp, const char *name)
{
    if (!name)
    {
        FURI_LOG_E("Game", "World name is NULL");
        return NULL;
    }

    if (!fhttp)
    {
        FURI_LOG_E("Game", "world_fetch: FlipperHTTP is NULL");
        return NULL;
    }

    char url[256];
    snprintf(url, sizeof(url), "https://www.jblanked.com/flipper/api/world/v8/get/world/%s/", name);
    snprintf(fhttp->file_path, sizeof(fhttp->file_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds/%s.json", name);

    fhttp->save_received_data = true;
    fhttp->state = IDLE;
    if (!flipper_http_request(fhttp, GET, url, "{\"Content-Type\": \"application/json\"}", NULL))
    {
        FURI_LOG_E("Game", "world_fetch: Failed to send HTTP request");

        return NULL;
    }
    fhttp->state = RECEIVING;
    while (fhttp->state != IDLE)
    {
        furi_delay_ms(100);
    }

    FuriString *returned_data = load_furi_world(name);
    if (!returned_data)
    {
        FURI_LOG_E("Game", "Failed to load world data from file");
        return NULL;
    }
    if (!separate_world_data((char *)name, returned_data))
    {
        FURI_LOG_E("Game", "Failed to separate world data");
        furi_string_free(returned_data);
        return NULL;
    }
    return returned_data;
}