#include "run/sprites.hpp"
#include "run/assets.hpp"
#include "run/run.hpp"
#include <math.h>

#ifndef COLOR_WHITE
#define COLOR_WHITE 0xFFFF
#endif

#ifndef COLOR_BLACK
#define COLOR_BLACK 0x0000
#endif

Sprite::Sprite(
    const char* name,
    EntityType type,
    Vector position,
    Vector endPosition,
    float move_timer,
    float speed,
    float attack_timer,
    float strength,
    float health)
    : Entity(name, type, position, Vector(0, 0), nullptr, nullptr, nullptr) {
    this->start_position = position;
    this->end_position = endPosition == Vector(-1, -1) ? position : endPosition;
    this->state = ENTITY_MOVING_TO_END;
    this->move_timer = move_timer;
    this->speed = speed;
    this->attack_timer = attack_timer;
    this->strength = strength;
    this->health = health;

    if(strcmp(name, "Cyclops") == 0) {
        size = Vector(10, 11);
        sprite = new Image(size, true, enemy_left_cyclops_10x11px);
        sprite_left = new Image(size, true, enemy_left_cyclops_10x11px);
        sprite_right = new Image(size, true, enemy_right_cyclops_10x11px);
    } else if(strcmp(name, "Ogre") == 0) {
        size = Vector(10, 13);
        sprite = new Image(size, true, enemy_left_ogre_10x13px);
        sprite_left = new Image(size, true, enemy_left_ogre_10x13px);
        sprite_right = new Image(size, true, enemy_right_ogre_10x13px);
    } else if(strcmp(name, "Ghost") == 0) {
        size = Vector(15, 15);
        sprite = new Image(size, true, enemy_left_ghost_15x15px);
        sprite_left = new Image(size, true, enemy_left_ghost_15x15px);
        sprite_right = new Image(size, true, enemy_right_ghost_15x15px);
    } else if(strcmp(name, "Funny NPC") == 0) {
        size = Vector(15, 21);
        sprite = new Image(size, true, npc_left_funny_15x21px);
        sprite_left = new Image(size, true, npc_left_funny_15x21px);
        sprite_right = new Image(size, true, npc_right_funny_15x21px);
    } else {
        // Default sprite
        sprite = nullptr;
        sprite_left = nullptr;
        sprite_right = nullptr;
    }
}

void Sprite::collision(Entity* other, Game* game) {
    if(type != ENTITY_ENEMY || other->type != ENTITY_PLAYER) {
        return; // Only handle collisions between enemies and players
    }

    // Get positions of the enemy and the player
    Vector enemy_pos = position;
    Vector player_pos = other->position;

    // Determine if the enemy is facing the player or player is facing the enemy
    bool enemy_is_facing_player = false;
    bool player_is_facing_enemy = false;

    if((direction == ENTITY_LEFT && player_pos.x < enemy_pos.x) ||
       (direction == ENTITY_RIGHT && player_pos.x > enemy_pos.x) ||
       (direction == ENTITY_UP && player_pos.y < enemy_pos.y) ||
       (direction == ENTITY_DOWN && player_pos.y > enemy_pos.y)) {
        enemy_is_facing_player = true;
    }
    if((other->direction == ENTITY_LEFT && enemy_pos.x < player_pos.x) ||
       (other->direction == ENTITY_RIGHT && enemy_pos.x > player_pos.x) ||
       (other->direction == ENTITY_UP && enemy_pos.y < player_pos.y) ||
       (other->direction == ENTITY_DOWN && enemy_pos.y > player_pos.y)) {
        player_is_facing_enemy = true;
    }

    // Handle Player Attacking Enemy (Press OK, facing enemy, and enemy not facing player)
    // we need to store the last button pressed to prevent multiple attacks
    if(player_is_facing_enemy && game->input == InputKeyOk && !enemy_is_facing_player) {
        // Reset last button
        game->input = InputKeyMAX;

        // check if enough time has passed since the last attack
        if(other->elapsed_attack_timer >= other->attack_timer) {
            // Reset player's elapsed attack timer
            other->elapsed_attack_timer = 0;
            elapsed_attack_timer = 0; // Reset enemy's attack timer to block enemy attack

            // Increase XP by the enemy's strength
            other->xp += strength;

            // Increase health by 10% of the enemy's strength
            other->health += strength * 0.1;

            // check max health
            if(other->health > 100) {
                other->health = 100;
            }

            // Decrease enemy health by player strength
            health -= other->strength;

            // check if enemy is dead
            if(health > 0) {
                state = ENTITY_ATTACKED;
                elapsed_move_timer = 0;
                position_set(old_position);
            }

            this->syncMultiplayerState(false, other);
        }
    }
    // Handle Enemy Attacking Player (enemy facing player)
    else if(enemy_is_facing_player) {
        // check if enough time has passed since the last attack
        if(elapsed_attack_timer >= attack_timer) {
            // Reset enemy's elapsed attack timer
            elapsed_attack_timer = 0;

            // Decrease player health by enemy strength
            other->health -= strength;

            // check if player is dead
            if(other->health > 0) {
                other->state = ENTITY_ATTACKED;
                other->position_set(other->old_position);
            }

            this->syncMultiplayerState(false, other);
        }
    }

    // check if player is dead
    if(other->health <= 0) {
        other->state = ENTITY_DEAD;
        other->position = other->start_position;
        other->health = other->max_health;
        other->position_set(other->start_position);
    }

    // check if enemy is dead
    if(health <= 0) {
        state = ENTITY_DEAD;
        position = Vector(-100, -100);
        health = 0;
        elapsed_move_timer = 0;
        position_set(position);
    }
}

void Sprite::drawUsername(Vector pos, Game* game) {
    char name[32];
    if(type == ENTITY_ENEMY) {
        snprintf(name, sizeof(name), "%.0f", (double)health);
    } else if(type == ENTITY_NPC) {
        snprintf(name, sizeof(name), "NPC");
    }

    // skip if enemy is out of the screen
    if(position.x + size.x < game->pos.x || position.x > game->pos.x + game->size.x ||
       position.y + size.y < game->pos.y || position.y > game->pos.y + game->size.y) {
        return;
    }

    // skip if drawing the username is out of the screen
    if(pos.x - game->pos.x - (strlen(name) * 2 + 8) < 0 ||
       pos.x - game->pos.x + (strlen(name) * 2 + 8) > game->size.x ||
       pos.y - game->pos.y - 10 < 0 || pos.y - game->pos.y > game->size.y) {
        return;
    }

    game->draw->setFont(FONT_SIZE_SMALL);

    // draw box around the name
    game->draw->fillRectangle(
        Vector(pos.x - game->pos.x - (strlen(name) * 2 - 5), pos.y - game->pos.y - 7),
        Vector(strlen(name) * 4 + 1, 8),
        COLOR_WHITE);

    // draw name over player's head
    game->draw->text(
        Vector(pos.x - game->pos.x - (strlen(name) * 2 - 4), pos.y - game->pos.y - 2),
        name,
        COLOR_BLACK);
}

void Sprite::render(Draw* canvas, Game* game) {
    UNUSED(canvas);

    if(state == ENTITY_DEAD) {
        return;
    }

    // Choose sprite based on direction
    if(direction == ENTITY_LEFT) {
        sprite = sprite_left;
    } else if(direction == ENTITY_RIGHT) {
        sprite = sprite_right;
    }

    // draw health of enemy
    drawUsername(position, game);
}

void Sprite::syncMultiplayerState(bool hostOnly, Entity* other) {
    if(!flipWorldRun || (!flipWorldRun->isPvEMode) || (hostOnly && !flipWorldRun->isLobbyHost)) {
        return;
    }

    flipWorldRun->syncMultiplayerEntity(this);
    if(other) {
        flipWorldRun->syncMultiplayerEntity(other);
    }
}

void Sprite::update(Game* game) {
    UNUSED(game);
    // check if enemy is dead
    if(state == ENTITY_DEAD) {
        return;
    }

    // float delta_time = 1.0 / game->fps;
    float delta_time = 1.0 / 60; // 60 frames per second

    // Increment the elapsed_attack_timer for the enemy
    elapsed_attack_timer += delta_time;

    switch(state) {
    case ENTITY_IDLE:
        // Increment the elapsed_move_timer
        elapsed_move_timer += delta_time;
        position_set(position);
        // Check if it's time to move again
        if(elapsed_move_timer >= move_timer) {
            // Determine the next state based on the current position
            if(fabs(position.x - start_position.x) < 1 &&
               fabs(position.y - start_position.y) < 1) {
                state = ENTITY_MOVING_TO_END;
            } else if(fabs(position.x - end_position.x) < 1 && fabs(position.y - end_position.y) < 1) {
                state = ENTITY_MOVING_TO_START;
            }
            // Reset the elapsed_move_timer
            elapsed_move_timer = 0;
        }
        break;
    case ENTITY_MOVING_TO_END:
    case ENTITY_MOVING_TO_START:
    case ENTITY_ATTACKED: {
        // determine the direction vector
        Vector direction_vector = {0, 0};

        // if attacked, change state to moving based on the direction
        if(state == ENTITY_ATTACKED) {
            state = position.x < old_position.x ? ENTITY_MOVING_TO_END : ENTITY_MOVING_TO_START;
        }

        // Determine the target position based on the current state
        Vector target_position = state == ENTITY_MOVING_TO_END ? end_position : start_position;

        // Calculate direction towards the target
        if(position.x < target_position.x) {
            direction_vector.x = 1;
            direction = ENTITY_RIGHT;
        } else if(position.x > target_position.x) {
            direction_vector.x = -1;
            direction = ENTITY_LEFT;
        } else if(position.y < target_position.y) {
            direction_vector.y = 1;
            direction = ENTITY_DOWN;
        } else if(position.y > target_position.y) {
            direction_vector.y = -1;
            direction = ENTITY_UP;
        }

        // Normalize direction vector
        float length = sqrt(
            direction_vector.x * direction_vector.x + direction_vector.y * direction_vector.y);
        if(length != 0) {
            direction_vector.x /= length;
            direction_vector.y /= length;
        }

        // Update position based on direction and speed
        Vector new_pos = position;
        new_pos.x += direction_vector.x * speed * delta_time;
        new_pos.y += direction_vector.y * speed * delta_time;

        // Clamp the position to the target to prevent overshooting
        if((direction_vector.x > 0 && new_pos.x > target_position.x) ||
           (direction_vector.x < 0 && new_pos.x < target_position.x)) {
            new_pos.x = target_position.x;
        }

        if((direction_vector.y > 0 && new_pos.y > target_position.y) ||
           (direction_vector.y < 0 && new_pos.y < target_position.y)) {
            new_pos.y = target_position.y;
        }

        // Set the new position
        position_set(new_pos);

        if(this->hasChangedPosition()) {
            this->syncMultiplayerState();
        }

        // Check if the enemy has reached or surpassed the target_position
        bool reached_x = fabs(new_pos.x - target_position.x) < 1;
        bool reached_y = fabs(new_pos.y - target_position.y) < 1;

        if(reached_x && reached_y) {
            // Set the state to idle
            state = ENTITY_IDLE;
            elapsed_move_timer = 0;
        }
    } break;
    default:
        break;
    }
}
