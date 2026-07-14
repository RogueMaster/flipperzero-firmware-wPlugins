#include "game.h"
#include "player.h"
#include "skeleton.h"

// REMEBER do not use static on scripts that communicate with each other!
// what static means is that it's not letting the function be used OUTSIDE of the file!

static const EntityDescription ground_desc;

/****** ground ******/
static void ground_start(Entity* self, GameManager* manager, void* context) {
    UNUSED(manager);
    UNUSED(context);
    GameContext* game_context = game_manager_game_context_get(manager);
    game_context->ground_hight = 50;

    entity_collider_add_rect(self, 50, 128);
}

static void ground_render(Entity* self, GameManager* manager, Canvas* canvas, void* context) {
    UNUSED(manager);
    UNUSED(context);
    UNUSED(self);

    canvas_draw_box(canvas, 0, 50, 128, 16);
}

static const EntityDescription ground_desc = {
    .start = ground_start,
    .stop = NULL,
    .update = NULL,
    .render = ground_render,
    .collision = NULL,
    .event = NULL,
    .context_size = 0, // context is zero, as I will just be using the draw rect function
};

/****** Level ******/

static void level_alloc(Level* level, GameManager* manager, void* context) {
    UNUSED(manager);
    UNUSED(context);
    // add the player to the level
    player_spawn(level, manager);

    skeleton_spawn(level, manager);

    // add the ground to the level
    level_add_entity(level, &ground_desc);
}

/*
    Alloc/free is called once, when level is added/removed from the game.
    It useful if you have small amount of levels and entities, that can be allocated at once.

    Start/stop is called when level is changed to/from this level.
    It will save memory, because you can allocate entities only for current level.
*/
static const LevelBehaviour level = {
    .alloc = level_alloc, // called once, when level allocated
    .free = NULL, // called once, when level freed
    .start = NULL, // called when level is changed to this level
    .stop = NULL, // called when level is changed from this level
    .context_size = 0, // size of level context, will be automatically allocated and freed
};
/****** Game ******/

/* 
    Write here the start code for your game, for example: creating a level and so on.
    Game context is allocated (game.context_size) and passed to this function, you can use it to store your game data.
*/
static void game_start(GameManager* game_manager, void* ctx) {
    UNUSED(game_manager);

    // Do some initialization here, for example you can load score from storage.
    // For simplicity, we will just set it to 0.
    GameContext* game_context = ctx;
    game_context->score = 0;

    // Add level to the game
    game_manager_add_level(game_manager, &level);
}

/* 
    Write here the stop code for your game, for example: freeing memory, if it was allocated.
    You don't need to free level, sprites or entities, it will be done automatically.
    Also, you don't need to free game_context, it will be done automatically, after this function.
*/
static void game_stop(void* ctx) {
    GameContext* game_context = ctx;
    // Do some deinitialization here, for example you can save score to storage.
    // For simplicity, we will just print it.
    FURI_LOG_I("Game", "Your score: %lu", game_context->score);
}

/*
    Yor game configuration, do not rename this variable, but you can change it's content here.  
*/
const Game game = {
    .target_fps = 30, // target fps, game will try to keep this value
    .show_fps = false, // show fps counter on the screen
    .always_backlight = true, // keep display backlight always on
    .start = game_start, // will be called once, when game starts
    .stop = game_stop, // will be called once, when game stops
    .context_size = sizeof(GameContext), // size of game context
};
