#include <storage/storage.h>
#include "game.h"

#define CENTER               33
#define BASE_MULTIPLIER      1
#define BASE_PTS_GHOST       10
#define BASE_PTS_DOT         1
#define BASE_PTS_LVL_SUCCESS 5

/****** Entities: Player ******/
static const LevelBehaviour level;
static bool GameOver = false;

static const int player_size = 6;
static const int player_radius = player_size / 2;
static const int player_x_max = 127 - player_radius;
static const int player_x_min = player_radius + 1;

static const int ghost_size = 6;
static const int ghost_radius = ghost_size / 2;
static const int ghost_x_max = 128 - ghost_radius;
static const int ghost_x_min = ghost_radius;

static const int target_buffer_max = 12;
typedef struct {
    Sprite* sprite;
    float speed;
} PlayerContext;

typedef enum {
    NORMAL,
    EDIBLE,
    EATEN,
} GhostState;

typedef struct {
    Sprite* sprite;
    int num_sprites;
    float speed;
    GhostState state;
    // when true, the eaten ghost is moving right
    bool eaten_direction; // TODO: make this less hacky
    int frames;
} Ghost;

typedef struct {
    Sprite* background_top;
    Sprite* background_bottom;
    uint32_t multiplier;
} LevelContext;

static bool direction = true;

// Forward declaration of player_desc, because it's used in player_spawn function.
static const EntityDescription player_desc;

//Initial Player location upon game start:
static Vector default_player_position = {34, CENTER};
static Vector default_ghost_position = {122, CENTER};

static Vector gen_target_pos(int index) {
    return (Vector){.x = (120 * (1 - ((double)index / target_buffer_max))) + 4, .y = CENTER};
}

static const EntityDescription target_desc;
static const EntityDescription food_desc;
static bool is_level_empty(Level* level) {
    int count = level_entity_count(level, &target_desc) + level_entity_count(level, &food_desc);

    FURI_LOG_I("target_collision", "Total food + target count is %d", count);
    return count == 0;
}

// Forward declaration, so we can reset when all targets and food have been eaten
static void target_and_food_spawn(Level* level);

static void player_spawn(Level* level, GameManager* manager) {
    Entity* player = level_add_entity(level, &player_desc);

    // Set player position.
    // Depends on your game logic, it can be done in start entity function, but also can be done here.
    entity_pos_set(player, default_player_position);

    // Add collision box to player entity
    entity_collider_add_rect(player, player_size, player_size);

    // Get player context
    PlayerContext* player_context = entity_context_get(player);

    // Load player sprite
    player_context->sprite = game_manager_sprite_load(manager, "player.fxbm");
    player_context->speed = 4.0;
}

static void player_update(Entity* self, GameManager* manager, void* context) {
    UNUSED(context);

    // Get game input
    InputState input = game_manager_input_get(manager);

    // Get player position
    Vector pos = entity_pos_get(self);

    // Get player context
    PlayerContext* player_context = (PlayerContext*)context;
    // Get game context
    GameContext* game_context = game_manager_game_context_get(manager);

    char left_path[50] = "player_left_ .fxbm";
    int left_path_index = 12;
    char right_path[50] = "player_right_ .fxbm";
    int right_path_index = 13;

    if(input.pressed & GameKeyOk && GameOver) {
        GameOver = false;
        pos.x = CENTER;
        pos.y = CENTER;
        player_context->speed = 4.0;
        game_context->high_score = MAX(game_context->score, game_context->high_score);
        game_context->score = 0;

        Level* nextlevel = game_manager_add_level(manager, &level);
        game_manager_next_level_set(manager, nextlevel);
        FURI_LOG_I("player_update", "Game Over is %d", GameOver);
        FURI_LOG_I("player_update", "Player position is %d, %d", (int)pos.x, (int)pos.y);
    }
    if(input.pressed & GameKeyOk && !GameOver) {
        direction = !direction;
    }
    if(input.held & GameKeyLeft) {
        direction = false;
    }
    if(input.held & GameKeyRight) {
        direction = true;
    }
    if(!direction) {
        pos.x -= player_context->speed; //offset by one so that sprite can cycle through animation
        int frame =
            (int)pos.x % 3 + 1; //This references sprite files - out of range causes crashes
        left_path[left_path_index] = '0' + frame;
        FURI_LOG_I("left_path", "left_path is %s", left_path);
        player_context->sprite = game_manager_sprite_load(manager, left_path);
    }
    if(direction) {
        pos.x += player_context->speed;
        int frame = (int)(pos.x) % 3 + 1;
        right_path[right_path_index] = '0' + frame;
        FURI_LOG_I("right_path", "right path is %s", right_path);
        player_context->sprite = game_manager_sprite_load(manager, right_path);
    }

    // Set wraparound for player
    if(pos.x < player_x_min) {
        pos.x = player_x_max;
    }
    if(pos.x > player_x_max) {
        pos.x = player_x_min;
    }

    entity_pos_set(self, pos);

    // Control game exit
    if(input.pressed & GameKeyBack) {
        game_manager_game_stop(manager);
    }
}

static void player_render(Entity* self, GameManager* manager, Canvas* canvas, void* context) {
    // Get player context
    PlayerContext* player = context;

    // Get player position
    Vector pos = entity_pos_get(self);

    // Draw player sprite
    // We subtract 5 from x and y, because collision box is 10x10, and we want to center sprite in it.
    canvas_draw_sprite(canvas, player->sprite, pos.x - player_radius, pos.y - player_radius);

    // Get game context
    GameContext* game_context = game_manager_game_context_get(manager);

    // Draw score
    canvas_printf(canvas, 0, 7, "Score: %lu", game_context->score);
    int width = canvas_printf_width(canvas, "High: %lu", game_context->high_score);
    canvas_printf(canvas, 128 - width, 7, "High: %lu", game_context->high_score);

    Level* level = game_manager_current_level_get(manager);
    LevelContext* level_context = level_context_get(level);
    canvas_printf(canvas, 0, 15, "Mult: %lux", level_context->multiplier);

    if(player->speed == 0) {
        canvas_printf(canvas, 0, 36, "Game Over");
        GameOver = true;
    }
}

static const EntityDescription player_desc = {
    .start = NULL, // called when entity is added to the level
    .stop = NULL, // called when entity is removed from the level
    .update = player_update, // called every frame
    .render = player_render, // called every frame, after update
    .collision = NULL, // called when entity collides with another entity
    .event = NULL, // called when entity receives an event
    .context_size =
        sizeof(PlayerContext), // size of entity context, will be automatically allocated and freed
};

/****** Entities: Ghost ******/

static const EntityDescription ghost_desc;

static void ghost_reset(Ghost* ghost, GameManager* manager) {
    ghost->sprite = game_manager_sprite_load(manager, "ghost_left_1.fxbm");
    ghost->state = NORMAL;
    ghost->num_sprites = 2;
    ghost->speed = 4.2;
    ghost->frames = -1;
}

static void ghost_edible_set(Ghost* ghost) {
    ghost->state = EDIBLE;
    ghost->speed = 1.0;
    ghost->frames = game.target_fps * 2;
}

static void ghost_eaten_set(Ghost* ghost, Entity* ghost_entity) {
    Vector pos = entity_pos_get(ghost_entity);
    if(pos.x < 64) {
        ghost->eaten_direction = true;
    } else {
        ghost->eaten_direction = false;
    }
    ghost->state = EATEN;
    ghost->speed = 6.0;
}

static void ghost_spawn(Level* level, GameManager* manager) {
    Entity* entity = level_add_entity(level, &ghost_desc);

    entity_pos_set(entity, default_ghost_position);

    // Add collision box to ghost entity
    entity_collider_add_rect(entity, ghost_size - 1, ghost_size - 1);

    // Get ghost context
    Ghost* ghost = entity_context_get(entity);

    // Load ghost sprite and set speed to 0
    ghost_reset(ghost, manager);
}

static void ghost_update(Entity* self, GameManager* manager, void* context) {
    UNUSED(manager);

    Ghost* ghost = context;

    // Get player entity
    Level* level = game_manager_current_level_get(manager);
    Entity* player = level_entity_get(level, &player_desc, 0);
    if(player == NULL) {
        return;
    }
    Vector player_pos = entity_pos_get(player);
    Vector ghost_pos = entity_pos_get(self);

    // Ghost mode handling
    if(ghost->state == EDIBLE) {
        if(ghost->frames == 0) {
            // Timer is up
            ghost_reset(ghost, manager);
        } else {
            // Ghost is stil edible
            ghost->frames--;
        }
    } else if(ghost->state == EATEN) {
        ghost->sprite = game_manager_sprite_load(manager, "ghost_eaten.fxbm");
    }

    // Set position based on ghost mode
    if(ghost->state == EDIBLE) {
        if(ghost_pos.x < player_pos.x) {
            ghost_pos.x -= ghost->speed;
        } else {
            ghost_pos.x += ghost->speed;
        }
    } else if(ghost->state == NORMAL) {
        if(ghost_pos.x < player_pos.x) {
            ghost_pos.x += ghost->speed;
        } else {
            ghost_pos.x -= ghost->speed;
        }
    } else {
        // When ghost is eaten
        if(ghost->eaten_direction) {
            ghost_pos.x += ghost->speed;
        } else {
            ghost_pos.x -= ghost->speed;
        }
    }

    // Ensure new ghost position is within bounds
    ghost_pos.x = CLAMP(ghost_pos.x, ghost_x_max, ghost_x_min);

    if(ghost->state == EATEN && (ghost_pos.x == ghost_x_min || ghost_pos.x == ghost_x_max)) {
        ghost_reset(ghost, manager);
    }

    entity_pos_set(self, ghost_pos);
}

static void ghost_render(Entity* self, GameManager* manager, Canvas* canvas, void* context) {
    UNUSED(manager);

    // Get ghost context
    Ghost* ghost = context;

    // Get ghost position
    Vector pos = entity_pos_get(self);

    // Draw ghost sprite
    canvas_draw_sprite(canvas, ghost->sprite, pos.x - ghost_radius, pos.y - ghost_radius);
}

static void ghost_collision(Entity* self, Entity* other, GameManager* manager, void* context) {
    UNUSED(self);
    UNUSED(context);
    UNUSED(manager);

    if(entity_description_get(other) == &player_desc) {
        Ghost* ghost = context;
        if(ghost->state == EDIBLE) {
            ghost_eaten_set(ghost, self);

            GameContext* game_context = game_manager_game_context_get(manager);
            Level* level = game_manager_current_level_get(manager);
            LevelContext* level_context = level_context_get(level);

            game_context->score += BASE_PTS_GHOST * level_context->multiplier;
            level_context->multiplier++;
        } else if(ghost->state == NORMAL) {
            PlayerContext* player_context = entity_context_get(other);
            player_context->speed = 0;
            ghost->speed = 0;
        }
    }
}

static const EntityDescription ghost_desc = {
    .start = NULL, // called when entity is added to the level
    .stop = NULL, // called when entity is removed from the level
    .update = ghost_update, // called every frame
    .render = ghost_render, // called every frame, after update
    .collision = ghost_collision, // called when entity collides with another entity
    .event = NULL, // called when entity receives an event
    .context_size =
        sizeof(Ghost), // size of entity context, will be automatically allocated and freed
};

/****** Entities: Target ******/

static void target_start(Entity* self, GameManager* manager, void* context) {
    UNUSED(context);
    UNUSED(manager);
    //entity_pos_set(self, random_pos());
    // Add collision circle to target entity
    // Circle is centered in target x and y, and it's radius is 3
    entity_collider_add_circle(self, 3);
}

static void target_render(Entity* self, GameManager* manager, Canvas* canvas, void* context) {
    UNUSED(context);
    UNUSED(manager);

    // Get target position
    Vector pos = entity_pos_get(self);

    // Draw target
    canvas_draw_disc(canvas, pos.x, pos.y, 1);
}

static void target_collision(Entity* self, Entity* other, GameManager* manager, void* context) {
    UNUSED(context);
    // Check if target collided with player
    if(entity_description_get(other) == &player_desc) {
        GameContext* game_context = game_manager_game_context_get(manager);
        Level* level = game_manager_current_level_get(manager);
        LevelContext* level_context = level_context_get(level);

        Vector pos = entity_pos_get(self);
        FURI_LOG_I("target_collision", "Removing target at %d, %d", (int)pos.x, (int)pos.y);
        level_remove_entity(level, self);

        if(is_level_empty(level)) {
            game_context->score += BASE_PTS_LVL_SUCCESS * level_context->multiplier;
            level_context->multiplier++;

            target_and_food_spawn(level);
        } else {
            game_context->score += BASE_PTS_DOT * level_context->multiplier;
        }
    }
}

static const EntityDescription target_desc = {
    .start = target_start, // called when entity is added to the level
    .stop = NULL, // called when entity is removed from the level
    .update = NULL, // called every frame
    .render = target_render, // called every frame, after update
    .collision = target_collision, // called when entity collides with another entity
    .event = NULL, // called when entity receives an event
    .context_size = 0, // size of entity context, will be automatically allocated and freed
};

static void target_spawn(Level* level, Vector pos) {
    Entity* target = level_add_entity(level, &target_desc);
    FURI_LOG_I("before pos_set", "Spawning target at %d, %d", (int)pos.x, (int)pos.y);
    entity_pos_set(target, pos);
}

/****** Entities: Big Dot ******/

static void food_start();
static void food_render();
static void food_collision();

static void food_spawn(Level* level, Vector pos) {
    Entity* food = level_add_entity(level, &food_desc);
    FURI_LOG_I("food_spawn", "Spawning food at %d, %d", (int)pos.x, (int)pos.y);
    entity_pos_set(food, pos);
}

static void food_start(Entity* self, GameManager* manager, void* context) {
    UNUSED(context);
    UNUSED(manager);

    entity_collider_add_circle(self, 3);
}

static void food_render(Entity* self, GameManager* manager, Canvas* canvas, void* context) {
    UNUSED(context);
    UNUSED(manager);

    // Get target position
    Vector pos = entity_pos_get(self);

    // Draw target
    canvas_draw_disc(canvas, pos.x, pos.y, 2);
}

static void food_collision(Entity* self, Entity* other, GameManager* manager, void* context) {
    UNUSED(context);

    // Check if target collided with player
    if(entity_description_get(other) == &player_desc) {
        GameContext* game_context = game_manager_game_context_get(manager);
        Level* level = game_manager_current_level_get(manager);
        LevelContext* level_context = level_context_get(level);

        Vector pos = entity_pos_get(self);
        FURI_LOG_I("food_collision", "Removing target at %d, %d", (int)pos.x, (int)pos.y);
        level_remove_entity(level, self);

        Entity* ghost_entity = level_entity_get(level, &ghost_desc, 0);
        Ghost* ghost = entity_context_get(ghost_entity);
        if(ghost->state != EATEN) {
            ghost_edible_set(ghost);
        }

        if(is_level_empty(level)) {
            game_context->score += BASE_PTS_LVL_SUCCESS * level_context->multiplier;
            level_context->multiplier++;

            target_and_food_spawn(level);
        } else {
            game_context->score += BASE_PTS_DOT * level_context->multiplier;
        }
    }
}

static const EntityDescription food_desc = {
    .start = food_start, // called when entity is added to the level
    .stop = NULL, // called when entity is removed from the level
    .update = NULL, // called every frame
    .render = food_render, // called every frame, after update
    .collision = food_collision, // called when entity collides with another entity
    .event = NULL, // called when entity receives an event
    .context_size = 0, // size of entity context, will be automatically allocated and freed
};

/****** Level ******/

static void target_and_food_spawn(Level* level) {
    int food_index = rand() % target_buffer_max;

    for(int i = target_buffer_max; i > 0; i--) {
        Vector pos = gen_target_pos(i);
        if(i == food_index) {
            food_spawn(level, pos);
        } else {
            target_spawn(level, pos);
        }
    }
}
/******* Background ********/

static void background_render(Entity* self, GameManager* manager, Canvas* canvas, void* context) {
    UNUSED(context);
    UNUSED(manager);
    UNUSED(self);

    // Draw target
    LevelContext* level_context = level_context_get(game_manager_current_level_get(manager));
    canvas_draw_sprite(canvas, level_context->background_top, 0, 8);
    canvas_draw_sprite(canvas, level_context->background_bottom, 0, 40);
}

static const EntityDescription background_desc = {
    .start = NULL, // called when entity is added to the level
    .stop = NULL, // called when entity is removed from the level
    .update = NULL, // called every frame
    .render = background_render, // called every frame, after update
    .collision = NULL, // called when entity collides with another entity
    .event = NULL, // called when entity receives an event
    .context_size = 0, // size of entity context, will be automatically allocated and freed
};
/**End Background render */

static void level_alloc(Level* level, GameManager* manager, void* context) {
    UNUSED(manager);
    UNUSED(context);

    LevelContext* level_context = level_context_get(level);
    level_context->background_top = game_manager_sprite_load(manager, "background_top.fxbm");
    level_context->background_bottom = game_manager_sprite_load(manager, "background_bottom.fxbm");
    level_context->multiplier = BASE_MULTIPLIER;
    level_add_entity(level, &background_desc);

    // Add player entity to the level
    player_spawn(level, manager);
    // Add ghost entity to the level
    ghost_spawn(level, manager);
    // Add target and food entities to the level
    target_and_food_spawn(level);
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
    .context_size =
        sizeof(LevelContext), // size of level context, will be automatically allocated and freed
};

/****** Game ******/

static uint32_t read_high_score() {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(!storage_file_open(file, APP_DATA_PATH("high_score.txt"), FSAM_READ, FSOM_OPEN_ALWAYS)) {
        FURI_LOG_E("read_high_score", "Failed to open file");
        return 0;
    }

    char saved_score_str[12];
    size_t read_size = storage_file_read(file, saved_score_str, 12);
    FURI_LOG_I("read_high_score", "bytes read: %u", read_size);

    FURI_LOG_I("read_high_score", "Saved high score: %s", saved_score_str);

    storage_file_close(file);

    // Deallocate file
    storage_file_free(file);

    // Close storage
    furi_record_close(RECORD_STORAGE);

    return atoi(saved_score_str);
}

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
    game_context->high_score = read_high_score();

    // Add level to the game
    game_manager_add_level(game_manager, &level);
}

static void persist_high_score(GameContext* game_context) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(!storage_file_open(file, APP_DATA_PATH("high_score.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FURI_LOG_E("save_high_score", "Failed to open file");
    }

    FuriString* score_furi_str = furi_string_alloc();
    furi_string_printf(score_furi_str, "%lu", MAX(game_context->score, game_context->high_score));
    const char* score_str = furi_string_get_cstr(score_furi_str);
    if(!storage_file_write(file, score_str, strlen(score_str))) {
        FURI_LOG_E("save_high_score", "Failed to write to file");
    }
    storage_file_close(file);

    // Deallocate file
    storage_file_free(file);

    // Close storage
    furi_record_close(RECORD_STORAGE);
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
    persist_high_score(game_context);
}

/*
    Yor game configuration, do not rename this variable, but you can change it's content here.  
*/
const Game game = {
    .target_fps = 10, // target fps, game will try to keep this value
    .show_fps = false, // show fps counter on the screen
    .always_backlight = true, // keep display backlight always on
    .start = game_start, // will be called once, when game starts
    .stop = game_stop, // will be called once, when game stops
    .context_size = sizeof(GameContext), // size of game context
};
