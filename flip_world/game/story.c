#include <game/story.h>
#include <game/storage.h>
#include <flip_storage/storage.h>
#include <game/level.h>

void story_draw_tutorial(Canvas *canvas, GameManager *manager)
{
    GameContext *game_context = game_manager_game_context_get(manager);
    furi_check(game_context);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 45, 12, "Tutorial");
    canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
    switch (game_context->story_step)
    {
    case 0:
        canvas_draw_str(canvas, 15, 20, "Press LEFT to move left");
        break;
    case 1:
        canvas_draw_str(canvas, 15, 20, "Press RIGHT to move right");
        break;
    case 2:
        canvas_draw_str(canvas, 15, 20, "Press UP to move up");
        break;
    case 3:
        canvas_draw_str(canvas, 15, 20, "Press DOWN to move down");
        break;
    case 4:
        canvas_draw_str(canvas, 0, 20, "Press OK + collide with an enemy to attack");
        break;
    case 5:
        canvas_draw_str(canvas, 15, 20, "Hold OK to open the menu");
        break;
    case 6:
        canvas_draw_str(canvas, 15, 20, "Press BACK to escape the menu");
        break;
    case 7:
        canvas_draw_str(canvas, 15, 20, "Hold BACK to save and exit");
        break;
    case 8:
        save_char("tutorial_done", "J You BLANKED on this one");
        game_context->story_step++;
        save_uint32("story_step", game_context->story_step);
        return;
    default:
        break;
    }
}

void story_draw(Entity *player, Canvas *canvas, GameManager *manager)
{
    if (!player || !canvas || !manager)
        return;

    // Get game context
    GameContext *game_context = game_manager_game_context_get(manager);
    furi_check(game_context);

    // Draw the tutorial or game world
    if (game_context->story_step < STORY_TUTORIAL_STEPS)
    {
        story_draw_tutorial(canvas, manager);
    }
    else
    {
        draw_background_render(canvas, manager);
    }
}

void story_update(Entity *player, GameManager *manager)
{
    UNUSED(player);
    GameContext *game_context = game_manager_game_context_get(manager);
    furi_check(game_context);
    InputState input = game_manager_input_get(manager);
    if (game_context->game_mode == GAME_MODE_STORY)
    {
        switch (game_context->story_step)
        {
        case 0:
            if (input.held & GameKeyLeft)
                game_context->story_step++;
            break;
        case 1:
            if (input.held & GameKeyRight)
                game_context->story_step++;
            break;
        case 2:
            if (input.held & GameKeyUp)
                game_context->story_step++;
            break;
        case 3:
            if (input.held & GameKeyDown)
                game_context->story_step++;
            break;
        case 5:
            if (input.held & GameKeyOk && game_context->is_menu_open)
                game_context->story_step++;
            break;
        case 6:
            if (input.held & GameKeyBack)
                game_context->story_step++;
            break;
        case 7:
            if (input.held & GameKeyBack)
                game_context->story_step++;
            break;
        }
    }
}

static void story_draw_world(Level *level, GameManager *manager, void *context)
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
    furi_string_cat_str(json_data_str, "{\"name\":\"story_world_v8\",\"author\":\"ChatGPT\",\"json_data\":[{\"i\":\"rock_medium\",\"x\":100,\"y\":100,\"a\":10,\"h\":true},{\"i\":\"rock_medium\",\"x\":400,\"y\":300,\"a\":6,\"h\":true},{\"i\":\"rock_small\",\"x\":600,\"y\":200,\"a\":8,\"h\":true},{\"i\":\"fence\",\"x\":50,\"y\":50,\"a\":10,\"h\":true},{\"i\":\"fence\",\"x\":250,\"y\":150,\"a\":12,\"h\":true},{\"i\":\"fence\",\"x\":550,\"y\":350,\"a\":12,\"h\":true},{\"i\":\"rock_large\",\"x\":400,\"y\":70,\"a\":12,\"h\":true},{\"i\":\"rock_large\",\"x\":200,\"y\":200,\"a\":6,\"h\":false},{\"i\":\"tree\",\"x\":5,\"y\":5,\"a\":45,\"h\":true},{\"i\":\"tree\",\"x\":5,\"y\":5,\"a\":20,\"h\":false},{\"i\":\"tree\",\"x\":22,\"y\":22,\"a\":44,\"h\":true},{\"i\":\"tree\",\"x\":22,\"y\":22,\"a\":20,\"h\":false},{\"i\":\"tree\",\"x\":5,\"y\":347,\"a\":45,\"h\":true},{\"i\":\"tree\",\"x\":5,\"y\":364,\"a\":45,\"h\":true},{\"i\":\"tree\",\"x\":735,\"y\":37,\"a\":18,\"h\":false},{\"i\":\"tree\",\"x\":752,\"y\":37,\"a\":18,\"h\":false}],\"enemy_data\":[{\"id\":\"cyclops\",\"index\":0,\"start_position\":{\"x\":350,\"y\":210},\"end_position\":{\"x\":390,\"y\":210},\"move_timer\":2,\"speed\":30,\"attack_timer\":0.4,\"strength\":10,\"health\":100},{\"id\":\"ogre\",\"index\":1,\"start_position\":{\"x\":200,\"y\":320},\"end_position\":{\"x\":220,\"y\":320},\"move_timer\":0.5,\"speed\":45,\"attack_timer\":0.6,\"strength\":20,\"health\":200},{\"id\":\"ghost\",\"index\":2,\"start_position\":{\"x\":100,\"y\":80},\"end_position\":{\"x\":180,\"y\":85},\"move_timer\":2.2,\"speed\":55,\"attack_timer\":0.5,\"strength\":30,\"health\":300},{\"id\":\"ogre\",\"index\":3,\"start_position\":{\"x\":400,\"y\":50},\"end_position\":{\"x\":490,\"y\":50},\"move_timer\":1.7,\"speed\":35,\"attack_timer\":1.0,\"strength\":20,\"health\":200}],\"npc_data\":[{\"id\":\"funny\",\"index\":0,\"start_position\":{\"x\":350,\"y\":180},\"end_position\":{\"x\":350,\"y\":180},\"move_timer\":0,\"speed\":0,\"message\":\"Hello there!\"}]}");
    if (!separate_world_data("story_world_v8", json_data_str))
    {
        FURI_LOG_E("Game", "Failed to separate world data");
    }
    furi_string_free(json_data_str);
    level_set_world(level, manager, "story_world_v8");
    game_context->icon_offset = 0;
    if (!game_context->imu_present)
    {
        game_context->icon_offset += ((game_context->icon_count / 10) / 15);
    }
    player_spawn(level, manager);
}

static const LevelBehaviour _story_world = {
    .alloc = NULL,
    .free = NULL,
    .start = story_draw_world,
    .stop = NULL,
    .context_size = 0,
};

const LevelBehaviour *story_world()
{
    return &_story_world;
}
