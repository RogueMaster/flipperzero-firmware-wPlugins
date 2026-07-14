#include "player.h"
#include "player_animations.h"

bool isGrounded;
bool isJumping;

bool is_moving;
bool can_move = true;

bool is_player_facing_right;

float player_sword_reach = 37;

Entity* player = NULL;

void player_spawn(Level* level, GameManager* manager) {
    player = level_add_entity(level, &player_desc);

    entity_pos_set(player, (Vector){15, 24});

    // Box is centered in player x and y
    entity_collider_add_rect(player, 15, 24);

    PlayerContext* player_context = entity_context_get(player);

    player_context->sprite = game_manager_sprite_load(manager, "other/player.fxbm");

    player_context->weapon_damage = 1;

    Jumping_animations_load(manager);

    Idle_animation_load(manager);
    Walking_animation_load(manager);
    Swinging_sword_animation_load(manager);

    Idle_animation_right_load(manager);
    Walking_right_animation_load(manager);
    Swinging_sword_right_animation_load(manager);
}

void player_movement(Entity* self, GameManager* manager, void* context) {
    PlayerContext* playerContext = (PlayerContext*)context;
    GameContext* game_context = game_manager_game_context_get(manager);
    InputState input = game_manager_input_get(manager);

    Vector pos = entity_pos_get(self);

    // isGrounded check
    if((pos.y + 12) >= game_context->ground_hight && playerContext->Yvelocity >= 0) {
        pos.y = game_context->ground_hight - 12;
        playerContext->Yvelocity = 0;
        isGrounded = true;
    } else {
        isGrounded = false;
    }

    // gravity
    if(!isGrounded) {
        playerContext->Yvelocity += 0.5;
        pos.y += playerContext->Yvelocity;
    }

    can_move = !playerContext->is_swinging_sword;

    if(can_move) {
        if(input.pressed & GameKeyOk) {
            playerContext->is_swinging_sword = true;
        }

        if(input.held & GameKeyLeft) pos.x -= 1;
        if(input.held & GameKeyRight) pos.x += 1;

        if((input.held & GameKeyLeft || input.held & GameKeyRight) && can_move)
            is_moving = true;
        else
            is_moving = false;

        if(input.held & GameKeyLeft) {
            is_player_facing_right = false;
        }

        if(input.held & GameKeyRight) {
            is_player_facing_right = true;
        }

        // jump
        if(input.pressed & GameKeyUp && isGrounded) {
            playerContext->Yvelocity = -3.5;
        }
    } else {
        is_moving = false;
    }

    pos.x = CLAMP(pos.x, 123, 5);
    pos.y = CLAMP(pos.y, 59, 5);

    if(pos.y < 5) pos.y = 5;
    if(pos.y > 59) pos.y = 59;

    entity_pos_set(self, pos);
}

void player_collision(Entity* self, Entity* other, GameManager* manager, void* context) {
    UNUSED(context);
    UNUSED(self);
    UNUSED(manager);
    UNUSED(other);
    /*
    if(entity_description_get(other) == &ground_desc) {
        return;
    }
    */
}

void player_render(Entity* self, GameManager* manager, Canvas* canvas, void* context) {
    // Get player context
    PlayerContext* player = context;

    // Get player position
    Vector pos = entity_pos_get(self);

    // Draw player sprite
    canvas_draw_sprite(canvas, player->sprite, pos.x - 38, pos.y - 40);

    // Get game context
    GameContext* game_context = game_manager_game_context_get(manager);

    // Draw score
    canvas_printf(canvas, 0, 7, "Score: %lu", game_context->score);
}

void Animations(GameManager* manager, void* context) {
    PlayerContext* playerContext = (PlayerContext*)context;

    // swinging sword animation logic
    if(playerContext->is_swinging_sword) {
        if(is_player_facing_right)
            Swinging_sword_animation_right_play(manager, context);
        else
            Swinging_sword_animation_play(manager, context);
    }

    if(!playerContext->is_swinging_sword) {
        // idle / walk / jump animations
        if(!is_moving && isGrounded && !is_player_facing_right) {
            Idle_animation_play(manager, context);
        }

        if(!is_moving && isGrounded && is_player_facing_right) {
            Idle_animation_right_play(manager, context);
        }

        if(is_moving && isGrounded && !is_player_facing_right) {
            Walking_animation_play(manager, context);
        }

        if(is_moving && isGrounded && is_player_facing_right) {
            Walking_right_animation_play(manager, context);
        }

        if(playerContext->Yvelocity > 0 && !isGrounded) {
            if(!is_player_facing_right) {
                playerContext->sprite = jumping[3];
            } else {
                playerContext->sprite = jumping[1];
            }
        }

        if(playerContext->Yvelocity < 0 && !isGrounded) {
            if(!is_player_facing_right) {
                playerContext->sprite = jumping[0];
            } else {
                playerContext->sprite = jumping[4];
            }
        }
    }

    UNUSED(manager);
}

void player_update(Entity* self, GameManager* manager, void* context) {
    PlayerContext* playerContext = (PlayerContext*)context;
    GameContext* game_context = game_manager_game_context_get(manager);
    InputState input = game_manager_input_get(manager);
    UNUSED(game_context);
    UNUSED(playerContext);

    // control game exit
    if(input.pressed & GameKeyBack) {
        game_manager_game_stop(manager);
    }

    player_movement(self, manager, context);
    Animations(manager, context);
}

const EntityDescription player_desc = {
    .start = NULL, // called when entity is added to the level
    .stop = NULL, // called when entity is //*d from the level
    .update = player_update, // called every frame
    .render = player_render, // called every frame, after update
    .collision = player_collision, // called when entity collides with another entity
    .event = NULL, // called when entity receives an event
    .context_size =
        sizeof(PlayerContext), // size of entity context, will be automatically allocated and freed
};
