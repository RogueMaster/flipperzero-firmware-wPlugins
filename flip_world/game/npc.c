#include <game/npc.h>
static EntityContext *npc_context_generic;

// Allocation function
static EntityContext *npc_generic_alloc(
    SpriteID id,
    int index,
    Vector size,
    Vector start_position,
    Vector end_position,
    float move_timer, // Wait duration before moving again
    float speed,
    const char *message)
{
    if (!npc_context_generic)
    {
        npc_context_generic = malloc(sizeof(EntityContext));
    }
    if (!npc_context_generic)
    {
        FURI_LOG_E("Game", "Failed to allocate EntityContext");
        return NULL;
    }
    npc_context_generic->id = id;
    npc_context_generic->index = index;
    npc_context_generic->size = size;
    npc_context_generic->start_position = start_position;
    npc_context_generic->end_position = end_position;
    npc_context_generic->move_timer = move_timer;   // Set wait duration
    npc_context_generic->elapsed_move_timer = 0.0f; // Initialize elapsed timer
    npc_context_generic->speed = speed;
    snprintf(npc_context_generic->message, sizeof(npc_context_generic->message), "%s", message);
    // Initialize other fields as needed
    npc_context_generic->sprite_right = NULL;          // sprite is assigned later
    npc_context_generic->sprite_left = NULL;           // sprite is assigned later
    npc_context_generic->direction = ENTITY_RIGHT;     // Default direction
    npc_context_generic->state = ENTITY_MOVING_TO_END; // Start in IDLE state
    // Set radius based on size, for example, average of size.x and size.y divided by 2
    npc_context_generic->radius = (size.x + size.y) / 4.0f;
    return npc_context_generic;
}

// NPC start function
static void npc_start(Entity *self, GameManager *manager, void *context)
{
    UNUSED(manager);
    if (!self || !context)
    {
        FURI_LOG_E("Game", "Enemy start: Invalid parameters");
        return;
    }
    if (!npc_context_generic)
    {
        FURI_LOG_E("Game", "NPC start: NPC context not set");
        return;
    }

    EntityContext *npc_context = (EntityContext *)context;
    // Copy fields from generic context
    npc_context->id = npc_context_generic->id;
    snprintf(npc_context->message, sizeof(npc_context->message), "%s", npc_context_generic->message);
    npc_context->index = npc_context_generic->index;
    npc_context->size = npc_context_generic->size;
    npc_context->start_position = npc_context_generic->start_position;
    npc_context->end_position = npc_context_generic->end_position;
    npc_context->move_timer = npc_context_generic->move_timer;
    npc_context->elapsed_move_timer = npc_context_generic->elapsed_move_timer;
    npc_context->speed = npc_context_generic->speed;
    npc_context->sprite_right = npc_context_generic->sprite_right;
    npc_context->sprite_left = npc_context_generic->sprite_left;
    npc_context->direction = npc_context_generic->direction;
    npc_context->state = npc_context_generic->state;
    npc_context->radius = npc_context_generic->radius;

    // Set NPC's initial position based on start_position
    entity_pos_set(self, npc_context->start_position);

    // Add collision circle based on the NPC's radius
    entity_collider_add_circle(self, npc_context->radius);
}

// NPC render function
static void npc_render(Entity *self, GameManager *manager, Canvas *canvas, void *context)
{
    if (!self || !context || !canvas || !manager)
        return;

    EntityContext *npc_context = (EntityContext *)context;
    GameContext *game_context = game_manager_game_context_get(manager);

    // Get the position of the NPC
    Vector pos = entity_pos_get(self);

    int x_pos = pos.x - draw_camera_x - npc_context->size.x / 2;
    int y_pos = pos.y - draw_camera_y - npc_context->size.y / 2;

    // check if position is within the screen
    if (x_pos + npc_context->size.x < 0 || x_pos > SCREEN_WIDTH || y_pos + npc_context->size.y < 0 || y_pos > SCREEN_HEIGHT)
        return;

    // Choose sprite based on direction
    Sprite *current_sprite = NULL;
    if (npc_context->direction == ENTITY_LEFT)
    {
        current_sprite = npc_context->sprite_left;
    }
    else
    {
        current_sprite = npc_context->sprite_right;
    }
    // no NPCs in story mode for now
    if (game_context->game_mode != GAME_MODE_STORY || game_context->story_step >= STORY_TUTORIAL_STEPS)
    {
        // Draw NPC sprite relative to camera, centered on the NPC's position
        canvas_draw_sprite(
            canvas,
            current_sprite,
            pos.x - draw_camera_x - (npc_context->size.x / 2),
            pos.y - draw_camera_y - (npc_context->size.y / 2));
    }
}

// NPC collision function
static void npc_collision(Entity *self, Entity *other, GameManager *manager, void *context)
{
    if (!self || !other || !context || !manager)
    {
        FURI_LOG_E("Game", "NPC collision: Invalid parameters");
        return;
    }

    // Check if the NPC collided with the player
    if (entity_description_get(other) == &player_desc)
    {
        // Retrieve NPC context
        EntityContext *npc_context = (EntityContext *)context;
        GameContext *game_context = game_manager_game_context_get(manager);
        PlayerContext *player_context = entity_context_get(game_context->player);
        furi_check(npc_context);
        furi_check(game_context);
        furi_check(player_context);

        // Get positions of the NPC and the player
        Vector npc_pos = entity_pos_get(self);
        Vector player_pos = entity_pos_get(other);

        // Determine if the NPC is facing the player or player is facing the NPC
        bool player_is_facing_npc = false;

        // Determine if the player is facing the NPC
        if ((player_context->direction == ENTITY_LEFT && npc_pos.x < player_pos.x) ||
            (player_context->direction == ENTITY_RIGHT && npc_pos.x > player_pos.x) ||
            (player_context->direction == ENTITY_UP && npc_pos.y < player_pos.y) ||
            (player_context->direction == ENTITY_DOWN && npc_pos.y > player_pos.y))
        {
            player_is_facing_npc = true;
        }

        // bounce the player back to where it came from
        // Set the player's old position to prevent collision
        entity_pos_set(other, player_context->old_position);
        // Reset player's movement direction to prevent immediate re-collision
        player_context->dx = 0;
        player_context->dy = 0;

        // Press OK and facing NPC
        if (player_is_facing_npc && game_context->last_button == GameKeyOk)
        {
            // show the NPC dialog on the game menu
            game_context->menu_screen = GAME_MENU_NPC;
            game_context->menu_selection = 0;
            snprintf(game_context->message, sizeof(game_context->message), "%s", npc_context->message);
            game_context->is_menu_open = true;
        }
    }
}

// NPC update function
static void npc_update(Entity *self, GameManager *manager, void *context)
{
    if (!self || !context || !manager)
        return;

    EntityContext *npc_context = (EntityContext *)context;
    if (!npc_context || npc_context->state == ENTITY_DEAD)
    {
        return;
    }

    GameContext *game_context = game_manager_game_context_get(manager);
    if (!game_context)
    {
        FURI_LOG_E("Game", "NPC update: Failed to get GameContext");
        return;
    }

    float delta_time = 1.0f / game_context->fps;

    switch (npc_context->state)
    {
    case ENTITY_IDLE:
        // Increment the elapsed_move_timer
        npc_context->elapsed_move_timer += delta_time;

        // Check if it's time to move again
        if (npc_context->elapsed_move_timer >= npc_context->move_timer)
        {
            // Determine the next state based on the current position
            Vector current_pos = entity_pos_get(self);
            if (fabs(current_pos.x - npc_context->start_position.x) < (double)1.0 &&
                fabs(current_pos.y - npc_context->start_position.y) < (double)1.0)
            {
                npc_context->state = ENTITY_MOVING_TO_END;
            }
            else
            {
                npc_context->state = ENTITY_MOVING_TO_START;
            }
            npc_context->elapsed_move_timer = 0.0f;
        }
        break;

    case ENTITY_MOVING_TO_END:
    case ENTITY_MOVING_TO_START:
    {
        // Determine the target position based on the current state
        Vector target_position = (npc_context->state == ENTITY_MOVING_TO_END) ? npc_context->end_position : npc_context->start_position;

        // Get current position
        Vector current_pos = entity_pos_get(self);
        Vector direction_vector = {0, 0};

        // Calculate direction towards the target
        if (current_pos.x < target_position.x)
        {
            direction_vector.x = 1.0f;
            npc_context->direction = ENTITY_RIGHT;
        }
        else if (current_pos.x > target_position.x)
        {
            direction_vector.x = -1.0f;
            npc_context->direction = ENTITY_LEFT;
        }

        if (current_pos.y < target_position.y)
        {
            direction_vector.y = 1.0f;
            npc_context->direction = ENTITY_DOWN;
        }
        else if (current_pos.y > target_position.y)
        {
            direction_vector.y = -1.0f;
            npc_context->direction = ENTITY_UP;
        }

        // Normalize direction vector
        float length = sqrt(direction_vector.x * direction_vector.x + direction_vector.y * direction_vector.y);
        if (length != 0)
        {
            direction_vector.x /= length;
            direction_vector.y /= length;
        }

        // Update position based on direction and speed
        Vector new_pos = current_pos;
        new_pos.x += direction_vector.x * npc_context->speed * delta_time;
        new_pos.y += direction_vector.y * npc_context->speed * delta_time;

        // Clamp the position to the target to prevent overshooting
        if ((direction_vector.x > 0.0f && new_pos.x > target_position.x) ||
            (direction_vector.x < 0.0f && new_pos.x < target_position.x))
        {
            new_pos.x = target_position.x;
        }

        if ((direction_vector.y > 0.0f && new_pos.y > target_position.y) ||
            (direction_vector.y < 0.0f && new_pos.y < target_position.y))
        {
            new_pos.y = target_position.y;
        }

        entity_pos_set(self, new_pos);

        // Check if the nPC has reached or surpassed the target_position
        bool reached_x = fabs(new_pos.x - target_position.x) < (double)1.0;
        bool reached_y = fabs(new_pos.y - target_position.y) < (double)1.0;

        // If reached the target position on both axes, transition to IDLE
        if (reached_x && reached_y)
        {
            npc_context->state = ENTITY_IDLE;
            npc_context->elapsed_move_timer = 0.0f;
        }
    }
    break;

    default:
        break;
    }
}

// Free function for the entity
static void npc_free(Entity *self, GameManager *manager, void *context)
{
    UNUSED(self);
    UNUSED(manager);
    UNUSED(context);
    if (npc_context_generic)
    {
        free(npc_context_generic);
        npc_context_generic = NULL;
    }
}

// NPC Behavior structure
static const EntityDescription _generic_npc = {
    .start = npc_start,
    .stop = npc_free,
    .update = npc_update,
    .render = npc_render,
    .collision = npc_collision,
    .event = NULL,
    .context_size = sizeof(EntityContext),
};

// Spawn function to return the entity description
static const EntityDescription *npc(
    GameManager *manager,
    const char *id,
    int index,
    Vector start_position,
    Vector end_position,
    float move_timer, // Wait duration before moving again
    float speed,
    const char *message)
{
    SpriteContext *sprite_context = sprite_context_get(id);
    if (!sprite_context)
    {
        FURI_LOG_E("Game", "Failed to get SpriteContext");
        return NULL;
    }

    // Allocate a new EntityContext with provided parameters
    npc_context_generic = npc_generic_alloc(
        sprite_context->id,
        index,
        (Vector){sprite_context->width, sprite_context->height},
        start_position,
        end_position,
        move_timer,
        speed,
        message);
    if (!npc_context_generic)
    {
        FURI_LOG_E("Game", "Failed to allocate EntityContext");
        return NULL;
    }

    // assign sprites to the context
    npc_context_generic->sprite_right = game_manager_sprite_load(manager, sprite_context->right_file_name);
    npc_context_generic->sprite_left = game_manager_sprite_load(manager, sprite_context->left_file_name);

    // Set initial direction based on start and end positions
    if (start_position.x < end_position.x)
    {
        npc_context_generic->direction = ENTITY_RIGHT;
    }
    else
    {
        npc_context_generic->direction = ENTITY_LEFT;
    }

    // Set initial state based on movement
    if (start_position.x != end_position.x || start_position.y != end_position.y)
    {
        npc_context_generic->state = ENTITY_MOVING_TO_END;
    }
    else
    {
        npc_context_generic->state = ENTITY_IDLE;
    }
    free(sprite_context);
    return &_generic_npc;
}

void npc_spawn(Level *level, GameManager *manager, FuriString *json)
{
    if (!level || !manager || !json)
    {
        FURI_LOG_E("Game", "Level, GameManager, or JSON is NULL");
        return;
    }

    FuriString *id = get_json_value_furi("id", json);
    FuriString *_index = get_json_value_furi("index", json);
    //
    FuriString *start_position = get_json_value_furi("start_position", json);
    FuriString *start_position_x = get_json_value_furi("x", start_position);
    FuriString *start_position_y = get_json_value_furi("y", start_position);
    //
    FuriString *end_position = get_json_value_furi("end_position", json);
    FuriString *end_position_x = get_json_value_furi("x", end_position);
    FuriString *end_position_y = get_json_value_furi("y", end_position);
    //
    FuriString *move_timer = get_json_value_furi("move_timer", json);
    FuriString *speed = get_json_value_furi("speed", json);
    //
    FuriString *message = get_json_value_furi("message", json);

    if (!id || !_index || !start_position || !start_position_x || !start_position_y || !end_position || !end_position_x || !end_position_y || !move_timer || !speed || !message)
    {
        FURI_LOG_E("Game", "Failed to get JSON values");
        return;
    }

    GameContext *game_context = game_manager_game_context_get(manager);
    if (game_context && game_context->npc_count < MAX_NPCS && !game_context->npcs[game_context->npc_count])
    {
        game_context->npcs[game_context->npc_count] = level_add_entity(level, npc(
                                                                                  manager,
                                                                                  furi_string_get_cstr(id),
                                                                                  atoi(furi_string_get_cstr(_index)),
                                                                                  (Vector){atof_furi(start_position_x), atof_furi(start_position_y)},
                                                                                  (Vector){atof_furi(end_position_x), atof_furi(end_position_y)},
                                                                                  atof_furi(move_timer),
                                                                                  atof_furi(speed),
                                                                                  furi_string_get_cstr(message)));
        game_context->npc_count++;
    }

    furi_string_free(id);
    furi_string_free(_index);
    furi_string_free(start_position);
    furi_string_free(start_position_x);
    furi_string_free(start_position_y);
    furi_string_free(end_position);
    furi_string_free(end_position_x);
    furi_string_free(end_position_y);
    furi_string_free(move_timer);
    furi_string_free(speed);
    furi_string_free(message);
}