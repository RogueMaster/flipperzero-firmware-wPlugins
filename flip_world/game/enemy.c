// enemy.c
#include <game/enemy.h>
#include <notification/notification_messages.h>
#include <flip_storage/storage.h>
#include <game/storage.h>

static EntityContext *enemy_context_generic;
// Allocation function
static EntityContext *enemy_generic_alloc(
    SpriteID id,
    int index,
    Vector size,
    Vector start_position,
    Vector end_position,
    float move_timer, // Wait duration before moving again
    float speed,
    float attack_timer,
    float strength,
    float health,
    bool is_user,
    FuriString *username)
{
    if (!enemy_context_generic)
    {
        enemy_context_generic = malloc(sizeof(EntityContext));
    }
    if (!enemy_context_generic)
    {
        FURI_LOG_E("Game", "Failed to allocate EntityContext");
        return NULL;
    }
    enemy_context_generic->id = id;
    enemy_context_generic->index = index;
    enemy_context_generic->size = size;
    enemy_context_generic->start_position = start_position;
    enemy_context_generic->end_position = end_position;
    enemy_context_generic->move_timer = move_timer;   // Set wait duration
    enemy_context_generic->elapsed_move_timer = 0.0f; // Initialize elapsed timer
    enemy_context_generic->speed = speed;
    enemy_context_generic->attack_timer = attack_timer;
    enemy_context_generic->strength = strength;
    enemy_context_generic->health = health;
    // Initialize other fields as needed
    enemy_context_generic->sprite_right = NULL;          // sprite is assigned later
    enemy_context_generic->sprite_left = NULL;           // sprite is assigned later
    enemy_context_generic->direction = ENTITY_RIGHT;     // Default direction
    enemy_context_generic->state = ENTITY_MOVING_TO_END; // Start in IDLE state
    // Set radius based on size, for example, average of size.x and size.y divided by 2
    enemy_context_generic->radius = (size.x + size.y) / 4.0f;
    //
    enemy_context_generic->is_user = is_user;
    //
    if (username != NULL)
    {
        snprintf(enemy_context_generic->username, sizeof(enemy_context_generic->username), "%s", furi_string_get_cstr(username));
    }
    else
    {
        snprintf(enemy_context_generic->username, sizeof(enemy_context_generic->username), "SYSTEM_ENEMY");
    }
    return enemy_context_generic;
}

// Enemy start function
static void enemy_start(Entity *self, GameManager *manager, void *context)
{
    UNUSED(manager);
    if (!self || !context)
    {
        FURI_LOG_E("Game", "Enemy start: Invalid parameters");
        return;
    }
    if (!enemy_context_generic)
    {
        FURI_LOG_E("Game", "Enemy start: Enemy context not set");
        return;
    }

    EntityContext *enemy_context = (EntityContext *)context;
    // Copy fields from generic context
    enemy_context->id = enemy_context_generic->id;
    enemy_context->index = enemy_context_generic->index;
    enemy_context->size = enemy_context_generic->size;
    enemy_context->start_position = enemy_context_generic->start_position;
    enemy_context->end_position = enemy_context_generic->end_position;
    enemy_context->move_timer = enemy_context_generic->move_timer;
    enemy_context->elapsed_move_timer = enemy_context_generic->elapsed_move_timer;
    enemy_context->speed = enemy_context_generic->speed;
    enemy_context->attack_timer = enemy_context_generic->attack_timer;
    enemy_context->strength = enemy_context_generic->strength;
    enemy_context->health = enemy_context_generic->health;
    enemy_context->sprite_right = enemy_context_generic->sprite_right;
    enemy_context->sprite_left = enemy_context_generic->sprite_left;
    enemy_context->direction = enemy_context_generic->direction;
    enemy_context->state = enemy_context_generic->state;
    enemy_context->radius = enemy_context_generic->radius;
    enemy_context->is_user = enemy_context_generic->is_user;
    snprintf(enemy_context->username, sizeof(enemy_context->username), "%s", enemy_context_generic->username);

    // Set enemy's initial position based on start_position
    entity_pos_set(self, enemy_context->start_position);

    // Add collision circle based on the enemy's radius
    entity_collider_add_circle(self, enemy_context->radius);
}

// Enemy render function
static void enemy_render(Entity *self, GameManager *manager, Canvas *canvas, void *context)
{
    if (!self || !context || !canvas || !manager)
        return;

    EntityContext *enemy_context = (EntityContext *)context;
    GameContext *game_context = game_manager_game_context_get(manager);

    // Get the position of the enemy
    Vector pos = entity_pos_get(self);

    // Get the camera position
    int x_pos = pos.x - draw_camera_x - enemy_context->size.x / 2;
    int y_pos = pos.y - draw_camera_y - enemy_context->size.y / 2;

    // check if position is within the screen
    if (x_pos + enemy_context->size.x < 0 || x_pos > SCREEN_WIDTH || y_pos + enemy_context->size.y < 0 || y_pos > SCREEN_HEIGHT)
        return;

    // Choose sprite based on direction
    Sprite *current_sprite = NULL;
    if (enemy_context->direction == ENTITY_LEFT)
    {
        current_sprite = enemy_context->sprite_left;
    }
    else
    {
        current_sprite = enemy_context->sprite_right;
    }

    // no enemies in story mode for now
    if (game_context->game_mode != GAME_MODE_STORY || (game_context->story_step == 4 || game_context->story_step >= STORY_TUTORIAL_STEPS))
    {
        // Draw enemy sprite relative to camera, centered on the enemy's position
        canvas_draw_sprite(
            canvas,
            current_sprite,
            pos.x - draw_camera_x - (enemy_context->size.x / 2),
            pos.y - draw_camera_y - (enemy_context->size.y / 2));

        // draw health of enemy
        char health_str[32];
        snprintf(health_str, sizeof(health_str), "%.0f", (double)enemy_context->health);
        draw_username(canvas, pos, health_str);
    }
}

static void enemy_atk_notify(GameContext *game_context, EntityContext *enemy_context, bool player_attacked)
{
    if (!game_context || !enemy_context)
    {
        FURI_LOG_E("Game", "Send attack notification: Invalid parameters");
        return;
    }

    NotificationApp *notifications = furi_record_open(RECORD_NOTIFICATION);

    const bool vibration_allowed = strstr(yes_or_no_choices[vibration_on_index], "Yes") != NULL;
    const bool sound_allowed = strstr(yes_or_no_choices[sound_on_index], "Yes") != NULL;

    if (player_attacked)
    {
        if (vibration_allowed && sound_allowed)
        {
            notification_message(notifications, &sequence_success);
        }
        else if (vibration_allowed && !sound_allowed)
        {
            notification_message(notifications, &sequence_single_vibro);
        }
        else if (!vibration_allowed && sound_allowed)
        {
            // change this to sound later
            notification_message(notifications, &sequence_blink_blue_100);
        }
        else
        {
            notification_message(notifications, &sequence_blink_blue_100);
        }
        FURI_LOG_I("Game", "Player attacked enemy '%d'!", enemy_context->id);
    }
    else
    {
        if (vibration_allowed && sound_allowed)
        {
            notification_message(notifications, &sequence_error);
        }
        else if (vibration_allowed && !sound_allowed)
        {
            notification_message(notifications, &sequence_single_vibro);
        }
        else if (!vibration_allowed && sound_allowed)
        {
            // change this to sound later
            notification_message(notifications, &sequence_blink_red_100);
        }
        else
        {
            notification_message(notifications, &sequence_blink_red_100);
        }

        FURI_LOG_I("Game", "Enemy '%d' attacked the player!", enemy_context->id);
    }

    // close the notifications
    furi_record_close(RECORD_NOTIFICATION);
}

// Enemy collision function
static void enemy_collision(Entity *self, Entity *other, GameManager *manager, void *context)
{
    if (!self || !other || !context || !manager)
    {
        FURI_LOG_E("Game", "Enemy collision: Invalid parameters");
        return;
    }
    EntityContext *enemy_context = (EntityContext *)context;
    furi_check(enemy_context, "Enemy collision: EntityContext is NULL");
    GameContext *game_context = game_manager_game_context_get(manager);
    furi_check(game_context, "Enemy collision: GameContext is NULL");
    PlayerContext *player_context = entity_context_get(game_context->player);
    if (game_context->game_mode == GAME_MODE_STORY && game_context->story_step != 4 && game_context->story_step < STORY_TUTORIAL_STEPS)
    {
        // FURI_LOG_I("Game", "Enemy collision: No enemies in story mode");
        return;
    }
    // Check if the enemy collided with the player
    if (entity_description_get(other) == &player_desc)
    {

        // Get positions of the enemy and the player
        Vector enemy_pos = entity_pos_get(self);
        Vector player_pos = entity_pos_get(other);

        // Determine if the enemy is facing the player or player is facing the enemy
        bool enemy_is_facing_player = false;
        bool player_is_facing_enemy = false;

        // Determine if the enemy is facing the player
        if ((enemy_context->direction == ENTITY_LEFT && player_pos.x < enemy_pos.x) ||
            (enemy_context->direction == ENTITY_RIGHT && player_pos.x > enemy_pos.x) ||
            (enemy_context->direction == ENTITY_UP && player_pos.y < enemy_pos.y) ||
            (enemy_context->direction == ENTITY_DOWN && player_pos.y > enemy_pos.y))
        {
            enemy_is_facing_player = true;
        }

        // Determine if the player is facing the enemy
        if ((player_context->direction == ENTITY_LEFT && enemy_pos.x < player_pos.x) ||
            (player_context->direction == ENTITY_RIGHT && enemy_pos.x > player_pos.x) ||
            (player_context->direction == ENTITY_UP && enemy_pos.y < player_pos.y) ||
            (player_context->direction == ENTITY_DOWN && enemy_pos.y > player_pos.y))
        {
            player_is_facing_enemy = true;
        }

        // Handle Player Attacking Enemy (Press OK, facing enemy, and enemy not facing player)
        if (player_is_facing_enemy && game_context->last_button == GameKeyOk && !enemy_is_facing_player)
        {
            if (game_context->game_mode == GAME_MODE_STORY && game_context->story_step == 4)
            {
                // FURI_LOG_I("Game", "Player attacked enemy '%d'!", enemy_context->id);
                game_context->story_step++;
            }
            // Reset last button
            game_context->last_button = -1;

            if (player_context->elapsed_attack_timer >= player_context->attack_timer)
            {
                enemy_atk_notify(game_context, enemy_context, true);

                // Reset player's elapsed attack timer
                player_context->elapsed_attack_timer = 0.0f;
                enemy_context->elapsed_attack_timer = 0.0f; // Reset enemy's attack timer to block enemy attack

                // Increase XP by the enemy's strength
                player_context->xp += enemy_context->strength;

                // Increase healthy by 10% of the enemy's strength
                player_context->health += enemy_context->strength * 0.1f;
                if (player_context->health > player_context->max_health)
                {
                    player_context->health = player_context->max_health;
                }

                // Decrease enemy health by player strength
                enemy_context->health -= player_context->strength;

                if (enemy_context->health <= 0)
                {
                    enemy_context->state = ENTITY_DEAD;

                    // if pvp, end the game
                    if (game_context->game_mode == GAME_MODE_PVP)
                    {
                        player_context->health = player_context->max_health;
                        save_player_context(player_context);
                        furi_delay_ms(100);
                        game_context->end_reason = GAME_END_PVP_ENEMY_DEAD;
                        game_manager_game_stop(manager);
                        return;
                    }

                    // Reset enemy position and health
                    enemy_context->health = 100; // this needs to be set to the enemy's max health

                    // in PVE mode, enemies can respawn
                    if (game_context->game_mode != GAME_MODE_PVE)
                    {
                        // remove from game context and set in safe zone
                        game_context->enemies[enemy_context->index] = NULL;
                        game_context->enemy_count--;
                        entity_collider_remove(self);
                        entity_pos_set(self, (Vector){-100, -100});
                        return;
                    }
                }
                else
                {
                    enemy_context->state = ENTITY_ATTACKED;
                    // Vector old_pos = entity_pos_get(self);
                    //  Bounce the enemy back by X units opposite their last movement direction
                    enemy_pos.x -= player_context->dx * enemy_context->radius + game_context->icon_offset;
                    // enemy_pos.y -= player_context->dy * enemy_context->radius + game_context->icon_offset;
                    entity_pos_set(self, enemy_pos);

                    // Reset enemy's movement direction to prevent immediate re-collision
                    player_context->dx = 0;
                    player_context->dy = 0;
                }
            }
            else
            {
                FURI_LOG_I("Game", "Player attack on enemy '%d' is on cooldown: %f seconds remaining", enemy_context->id, (double)(player_context->attack_timer - player_context->elapsed_attack_timer));
            }
        }
        // Handle Enemy Attacking Player (enemy facing player)
        else if (enemy_is_facing_player)
        {
            if (enemy_context->elapsed_attack_timer >= enemy_context->attack_timer)
            {
                enemy_atk_notify(game_context, enemy_context, false);

                // Reset enemy's elapsed attack timer
                enemy_context->elapsed_attack_timer = 0.0f;

                // Decrease player health by enemy strength
                player_context->health -= enemy_context->strength;

                if (player_context->health <= 0)
                {
                    FURI_LOG_I("Game", "Player is dead.. resetting player position and health");
                    player_context->state = ENTITY_DEAD;

                    // if pvp, end the game
                    if (game_context->game_mode == GAME_MODE_PVP)
                    {
                        save_player_context(player_context);
                        furi_delay_ms(100);
                        game_context->end_reason = GAME_END_PVP_PLAYER_DEAD;
                        game_manager_game_stop(manager);
                        return;
                    }

                    // Reset player position and health
                    entity_pos_set(other, player_context->start_position);
                    player_context->health = player_context->max_health;

                    // subtract player's XP by the enemy's strength
                    player_context->xp -= enemy_context->strength;
                    if ((int)player_context->xp < 0)
                    {
                        player_context->xp = 0;
                    }
                }
                else
                {
                    FURI_LOG_I("Game", "Player took %f damage from enemy '%d'", (double)enemy_context->strength, enemy_context->id);
                    player_context->state = ENTITY_ATTACKED;

                    // Bounce the player back by X units opposite their last movement direction
                    player_pos.x -= player_context->dx * enemy_context->radius + game_context->icon_offset;
                    // player_pos.y -= player_context->dy * enemy_context->radius + game_context->icon_offset;
                    entity_pos_set(other, player_pos);

                    // Reset player's movement direction to prevent immediate re-collision
                    player_context->dx = 0;
                    player_context->dy = 0;
                }
            }
        }
        else // handle other collisions
        {
            // Set the player's old position to prevent collision
            entity_pos_set(other, player_context->old_position);
            // Reset player's movement direction to prevent immediate re-collision
            player_context->dx = 0;
            player_context->dy = 0;
        }

        if (player_context->state == ENTITY_DEAD)
        {
            // Reset player's position and health
            entity_pos_set(other, player_context->start_position);
            player_context->health = player_context->max_health;
        }
    }
    // if not player than must be an icon or npc; so push back
    else
    {
        // push enemy back
        Vector enemy_pos = entity_pos_get(self);
        switch (enemy_context->direction)
        {
        case ENTITY_LEFT:
            enemy_pos.x += (enemy_context->size.x + game_context->icon_offset);
            break;
        case ENTITY_RIGHT:
            enemy_pos.x -= (enemy_context->size.x + game_context->icon_offset);
            break;
        case ENTITY_UP:
            enemy_pos.y += (enemy_context->size.y + game_context->icon_offset);
            break;
        case ENTITY_DOWN:
            enemy_pos.y -= (enemy_context->size.y + game_context->icon_offset);
            break;
        default:
            break;
        }
        entity_pos_set(self, enemy_pos);
    }
}

static bool enemy_is_game_enemy(GameManager *manager, const char *username)
{
    GameContext *game_context = game_manager_game_context_get(manager);
    if (game_context)
    {
        for (int i = 0; i < MAX_ENEMIES; i++)
        {
            if (!game_context->enemies[i])
                break;
            EntityContext *enemy_context = entity_context_get(game_context->enemies[i]);
            if (enemy_context && is_str(enemy_context->username, username))
            {
                return true;
            }
        }
    }
    return false;
}

static void enemy_pvp_position(GameManager *manager, EntityContext *enemy, Entity *self)
{
    if (!manager || !enemy || !self)
    {
        FURI_LOG_E("Game", "enemy_pvp_position: Invalid parameters");
        return;
    }

    GameContext *game_context = game_manager_game_context_get(manager);
    if (!game_context->fhttp->last_response || strlen(game_context->fhttp->last_response) == 0)
    {
        return;
    }

    // parse the response and set the enemy position
    /* expected response:
    {
        "u": "JBlanked",
        "xp": 37743,
        "h": 207,
        "ehr": 0.7,
        "eat": 127.5,
        "d": 2,
        "s": 1,
        "sp": {
            "x": 381.0,
            "y": 192.0
        }
    }
    */

    // match username
    char *u = get_json_value("u", game_context->fhttp->last_response);
    if (!u)
    {
        FURI_LOG_E("Game", "enemy_pvp_position: Failed to get username");
        return;
    }
    // check if the username matches
    if (!is_str(u, enemy->username))
    {
        if (strlen(u) == 0 || enemy_is_game_enemy(manager, u))
        {
            free(u);
            return;
        }

        PlayerContext *player_context = entity_context_get(game_context->player);
        if (is_str(player_context->username, u))
        {
            free(u);
            return;
        }

        char *h = get_json_value("h", game_context->fhttp->last_response);
        if (!h)
        {
            free(u);
            return;
        }
        FuriString *enemy_data = furi_string_alloc();
        furi_string_printf(
            enemy_data,
            "{\"enemy_data\":[{\"id\":\"sword\",\"is_user\":\"true\",\"username\":\"%s\","
            "\"index\":0,\"start_position\":{\"x\":350,\"y\":210},\"end_position\":{\"x\":350,\"y\":210},"
            "\"move_timer\":1,\"speed\":1,\"attack_timer\":\"0.1\",\"strength\":\"100\",\"health\":%f}]}",
            u,
            (double)atoi(h) // h is an int
        );
        free(h);                                                                             // free health
        enemy_spawn(game_context->levels[game_context->current_level], manager, enemy_data); // add the enemy to the game context
        FURI_LOG_I("Game", "enemy_pvp_position: Added enemy '%s' to the game context", u);
        free(u);
        furi_string_free(enemy_data); // free enemy data
        return;
    }

    free(u); // free username

    // we need the health, elapsed attack timer, direction, xp, and position
    char *h = get_json_value("h", game_context->fhttp->last_response);
    char *eat = get_json_value("eat", game_context->fhttp->last_response);
    char *d = get_json_value("d", game_context->fhttp->last_response);
    char *xp = get_json_value("xp", game_context->fhttp->last_response);
    char *sp = get_json_value("sp", game_context->fhttp->last_response);
    char *x = get_json_value("x", sp);
    char *y = get_json_value("y", sp);

    if (!h || !eat || !d || !sp || !x || !y || !xp)
    {
        if (h)
            free(h);
        if (eat)
            free(eat);
        if (d)
            free(d);
        if (sp)
            free(sp);
        if (x)
            free(x);
        if (y)
            free(y);
        if (xp)
            free(xp);
        return;
    }

    // set enemy info
    enemy->health = (float)atoi(h); // h is an int
    if (enemy->health <= 0)
    {
        enemy->health = 0;
        enemy->state = ENTITY_DEAD;
        entity_pos_set(self, (Vector){-100, -100});
        free(h);
        free(eat);
        free(d);
        free(sp);
        free(x);
        free(y);
        free(xp);
        PlayerContext *player_context = entity_context_get(game_context->player);
        if (player_context)
        {
            save_player_context(player_context);
            furi_delay_ms(100);
            game_manager_game_stop(manager);
        }
        return;
    }

    enemy->elapsed_attack_timer = atof_(eat);

    switch (atoi(d))
    {
    case 0:
        enemy->direction = ENTITY_LEFT;
        break;
    case 1:
        enemy->direction = ENTITY_RIGHT;
        break;
    case 2:
        enemy->direction = ENTITY_UP;
        break;
    case 3:
        enemy->direction = ENTITY_DOWN;
        break;
    default:
        enemy->direction = ENTITY_RIGHT;
        break;
    }

    enemy->xp = (atoi)(xp); // xp is an int
    enemy->level = player_level_iterative_get(enemy->xp);

    Vector new_pos = (Vector){
        .x = atof_(x),
        .y = atof_(y),
    };

    // set enemy position
    entity_pos_set(self, new_pos);

    // free the strings
    free(h);
    free(eat);
    free(d);
    free(sp);
    free(x);
    free(y);
    free(xp);
}

// Enemy update function
static void enemy_update(Entity *self, GameManager *manager, void *context)
{
    if (!self || !context || !manager)
        return;

    EntityContext *enemy_context = (EntityContext *)context;
    if (!enemy_context || enemy_context->state == ENTITY_DEAD)
    {
        return;
    }

    GameContext *game_context = game_manager_game_context_get(manager);
    if (!game_context)
    {
        FURI_LOG_E("Game", "Enemy update: Failed to get GameContext");
        return;
    }

    const float delta_time = 1.0f / game_context->fps;

    if (game_context->game_mode == GAME_MODE_PVP)
    {
        // update enemy position
        enemy_pvp_position(manager, enemy_context, self);
    }
    else
    {
        // update enemy position for pve mode
        if (game_context->game_mode == GAME_MODE_PVE)
        {
            enemy_pvp_position(manager, enemy_context, self);
        }

        // Increment the elapsed_attack_timer for the enemy
        enemy_context->elapsed_attack_timer += delta_time;

        switch (enemy_context->state)
        {
        case ENTITY_IDLE:
            // Increment the elapsed_move_timer
            enemy_context->elapsed_move_timer += delta_time;

            // Check if it's time to move again
            if (enemy_context->elapsed_move_timer >= enemy_context->move_timer)
            {
                // Determine the next state based on the current position
                Vector current_pos = entity_pos_get(self);
                if (fabs(current_pos.x - enemy_context->start_position.x) < (double)1.0 &&
                    fabs(current_pos.y - enemy_context->start_position.y) < (double)1.0)
                {
                    enemy_context->state = ENTITY_MOVING_TO_END;
                }
                else
                {
                    enemy_context->state = ENTITY_MOVING_TO_START;
                }
                enemy_context->elapsed_move_timer = 0.0f;
            }

            break;

        case ENTITY_MOVING_TO_END:
        case ENTITY_MOVING_TO_START:
        case ENTITY_ATTACKED:
        {
            // Get current position
            Vector current_pos = entity_pos_get(self);
            if (enemy_context->state == ENTITY_ATTACKED)
            {
                // set direction again
                enemy_context->state = enemy_context->direction == ENTITY_LEFT ? ENTITY_MOVING_TO_START : ENTITY_MOVING_TO_END;
            }

            // Determine the target position based on the current state
            Vector target_position = (enemy_context->state == ENTITY_MOVING_TO_END) ? enemy_context->end_position : enemy_context->start_position;
            Vector direction_vector = {0, 0};

            // Calculate direction towards the target
            if (current_pos.x < target_position.x)
            {
                direction_vector.x = 1.0f;
                enemy_context->direction = ENTITY_RIGHT;
            }
            else if (current_pos.x > target_position.x)
            {
                direction_vector.x = -1.0f;
                enemy_context->direction = ENTITY_LEFT;
            }

            if (current_pos.y < target_position.y)
            {
                direction_vector.y = 1.0f;
                enemy_context->direction = ENTITY_DOWN;
            }
            else if (current_pos.y > target_position.y)
            {
                direction_vector.y = -1.0f;
                enemy_context->direction = ENTITY_UP;
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
            new_pos.x += direction_vector.x * enemy_context->speed * delta_time;
            new_pos.y += direction_vector.y * enemy_context->speed * delta_time;

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

            // Set the new position
            entity_pos_set(self, new_pos);

            // Check if the enemy has reached or surpassed the target_position
            bool reached_x = fabs(new_pos.x - target_position.x) < (double)1.0;
            bool reached_y = fabs(new_pos.y - target_position.y) < (double)1.0;

            // If reached the target position on both axes, transition to IDLE
            if (reached_x && reached_y)
            {
                enemy_context->state = ENTITY_IDLE;
                enemy_context->elapsed_move_timer = 0.0f;
            }
        }
        break;

        default:
            break;
        }
    }
}

// Free function for the entity
static void enemy_free(Entity *self, GameManager *manager, void *context)
{
    UNUSED(self);
    UNUSED(manager);
    UNUSED(context);
    if (enemy_context_generic)
    {
        free(enemy_context_generic);
        enemy_context_generic = NULL;
    }
}

// Enemy behavior structure
static const EntityDescription _generic_enemy = {
    .start = enemy_start,
    .stop = enemy_free,
    .update = enemy_update,
    .render = enemy_render,
    .collision = enemy_collision,
    .event = NULL,
    .context_size = sizeof(EntityContext),
};

// Enemy function to return the entity description
static const EntityDescription *enemy(
    GameManager *manager,
    const char *id,
    int index,
    Vector start_position,
    Vector end_position,
    float move_timer, // Wait duration before moving again
    float speed,
    float attack_timer,
    float strength,
    float health,
    bool is_user,
    FuriString *username)

{
    SpriteContext *sprite_context = sprite_context_get(id);
    if (!sprite_context)
    {
        FURI_LOG_E("Game", "Failed to get SpriteContext");
        return NULL;
    }

    // Allocate a new EntityContext with provided parameters
    enemy_context_generic = enemy_generic_alloc(
        sprite_context->id,
        index,
        (Vector){sprite_context->width, sprite_context->height},
        start_position,
        end_position,
        move_timer, // Set wait duration
        speed,
        attack_timer,
        strength,
        health,
        is_user, username);
    if (!enemy_context_generic)
    {
        FURI_LOG_E("Game", "Failed to allocate EntityContext");
        return NULL;
    }

    // assign sprites to the context
    enemy_context_generic->sprite_right = game_manager_sprite_load(manager, sprite_context->right_file_name);
    enemy_context_generic->sprite_left = game_manager_sprite_load(manager, sprite_context->left_file_name);

    // Set initial direction based on start and end positions
    if (start_position.x < end_position.x)
    {
        enemy_context_generic->direction = ENTITY_RIGHT;
    }
    else
    {
        enemy_context_generic->direction = ENTITY_LEFT;
    }

    // Set initial state based on movement
    if (start_position.x != end_position.x || start_position.y != end_position.y)
    {
        enemy_context_generic->state = ENTITY_MOVING_TO_END;
    }
    else
    {
        enemy_context_generic->state = ENTITY_IDLE;
    }
    free(sprite_context);
    return &_generic_enemy;
}

void enemy_spawn(Level *level, GameManager *manager, FuriString *json)
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
    FuriString *attack_timer = get_json_value_furi("attack_timer", json);
    FuriString *strength = get_json_value_furi("strength", json);
    FuriString *health = get_json_value_furi("health", json);
    //

    if (!id || !_index || !start_position || !start_position_x || !start_position_y || !end_position || !end_position_x || !end_position_y || !move_timer || !speed || !attack_timer || !strength || !health)
    {
        FURI_LOG_E("Game", "Failed to parse JSON values");
        return;
    }

    FuriString *is_user = get_json_value_furi("is_user", json);
    bool is_user_value = false;
    if (is_user)
    {
        is_user_value = strstr(furi_string_get_cstr(is_user), "true") != NULL;
    }

    FuriString *username = get_json_value_furi("username", json);
    // no need to check for username, it is optional

    GameContext *game_context = game_manager_game_context_get(manager);
    if (game_context && game_context->enemy_count < MAX_ENEMIES && !game_context->enemies[game_context->enemy_count])
    {
        game_context->enemies[game_context->enemy_count] = level_add_entity(level, enemy(
                                                                                       manager,
                                                                                       furi_string_get_cstr(id),
                                                                                       atoi(furi_string_get_cstr(_index)),
                                                                                       (Vector){atof_furi(start_position_x), atof_furi(start_position_y)},
                                                                                       (Vector){atof_furi(end_position_x), atof_furi(end_position_y)},
                                                                                       atof_furi(move_timer),
                                                                                       atof_furi(speed),
                                                                                       atof_furi(attack_timer),
                                                                                       atof_furi(strength),
                                                                                       atof_furi(health),
                                                                                       is_user_value, username));
        game_context->enemy_count++;
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
    furi_string_free(attack_timer);
    furi_string_free(strength);
    furi_string_free(health);
    if (is_user)
    {
        furi_string_free(is_user);
    }
    if (username)
    {
        furi_string_free(username);
    }
}