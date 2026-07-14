#include "skeleton.h"
#include "player.h"

bool isSkeletonGrounded;
int health = 3;
int previous_health;
bool is_dead;
bool is_hurt;

int hurt_frames_to_wait = 30;
int hurt_timer = 0;

Sprite* skel_walking[8];
Sprite* skel_walking_right[8];

Sprite* skel_death[3];
Sprite* skel_death_right[3];

bool is_skel_facing_right;

int skel_walking_current_frame = 0;
int skel_walking_fps = 4;
int skel_walking_i;

int skel_walking_right_current_frame = 0;
int skel_walking_right_fps = 6;
int skel_walking_right_i;

int skel_death_current_frame = 0;
int skel_death_fps = 6;
int skel_death_i;

int skel_death_right_current_frame = 0;
int skel_death_right_fps = 8;
int skel_death_right_i;

void skeleton_sprites_load(GameManager* manager) {
    skel_walking[0] = game_manager_sprite_load(manager, "enemies/skeleton/walking_0.fxbm");
    skel_walking[1] = game_manager_sprite_load(manager, "enemies/skeleton/walking_1.fxbm");
    skel_walking[2] = game_manager_sprite_load(manager, "enemies/skeleton/walking_2.fxbm");
    skel_walking[3] = game_manager_sprite_load(manager, "enemies/skeleton/walking_3.fxbm");
    skel_walking[4] = game_manager_sprite_load(manager, "enemies/skeleton/walking_4.fxbm");
    skel_walking[5] = game_manager_sprite_load(manager, "enemies/skeleton/walking_5.fxbm");
    skel_walking[6] = game_manager_sprite_load(manager, "enemies/skeleton/walking_6.fxbm");
    skel_walking[7] = game_manager_sprite_load(manager, "enemies/skeleton/walking_7.fxbm");

    skel_death[0] = game_manager_sprite_load(manager, "enemies/skeleton/death_0.fxbm");
    skel_death[1] = game_manager_sprite_load(manager, "enemies/skeleton/death_1.fxbm");
    skel_death[2] = game_manager_sprite_load(manager, "enemies/skeleton/death_2.fxbm");

    // animations right
    skel_walking_right[0] =
        game_manager_sprite_load(manager, "enemies/skeleton/walking_right_0.fxbm");
    skel_walking_right[1] =
        game_manager_sprite_load(manager, "enemies/skeleton/walking_right_1.fxbm");
    skel_walking_right[2] =
        game_manager_sprite_load(manager, "enemies/skeleton/walking_right_2.fxbm");
    skel_walking_right[3] =
        game_manager_sprite_load(manager, "enemies/skeleton/walking_right_3.fxbm");
    skel_walking_right[4] =
        game_manager_sprite_load(manager, "enemies/skeleton/walking_right_4.fxbm");
    skel_walking_right[5] =
        game_manager_sprite_load(manager, "enemies/skeleton/walking_right_5.fxbm");
    skel_walking_right[6] =
        game_manager_sprite_load(manager, "enemies/skeleton/walking_right_6.fxbm");
    skel_walking_right[7] =
        game_manager_sprite_load(manager, "enemies/skeleton/walking_right_7.fxbm");

    skel_death_right[0] = game_manager_sprite_load(manager, "enemies/skeleton/death_right_0.fxbm");
    skel_death_right[1] = game_manager_sprite_load(manager, "enemies/skeleton/death_right_1.fxbm");
    skel_death_right[2] = game_manager_sprite_load(manager, "enemies/skeleton/death_right_2.fxbm");
}

void Skel_walking_animation_play(GameManager* manager, void* context) {
    UNUSED(manager);
    int total_frames = sizeof(skel_walking) / sizeof(skel_walking[0]);
    if(skel_walking_current_frame == total_frames) {
        skel_walking_current_frame = 0;
    }

    PlayerContext* playerContext = (PlayerContext*)context;
    playerContext->sprite = skel_walking[skel_walking_current_frame];

    skel_walking_i++;
    if(skel_walking_i >= skel_walking_fps) {
        skel_walking_current_frame++;
        skel_walking_i = 0;
    }
}

void Skel_walking_right_animation_play(GameManager* manager, void* context) {
    UNUSED(manager);
    int total_frames = sizeof(skel_walking_right) / sizeof(skel_walking_right[0]);
    if(skel_walking_right_current_frame == total_frames) {
        skel_walking_right_current_frame = 0;
    }

    PlayerContext* playerContext = (PlayerContext*)context;
    playerContext->sprite = skel_walking_right[skel_walking_right_current_frame];

    skel_walking_right_i++;
    if(skel_walking_right_i >= skel_walking_fps) {
        skel_walking_right_current_frame++;
        skel_walking_right_i = 0;
    }
}

void Skel_death_animation_play(GameManager* manager, void* context, Entity* self) {
    UNUSED(manager);
    Level* level = game_manager_current_level_get(manager);

    SkeletonContext* skelContext = (SkeletonContext*)context;
    skelContext->sprite = skel_death[skel_death_current_frame];

    int total_frames = sizeof(skel_death) / sizeof(skel_death[0]);

    skel_death_i++;
    if(skel_death_i >= skel_death_fps) {
        skel_death_current_frame++;
        skel_death_i = 0;
    }

    if(skel_death_current_frame == total_frames) level_remove_entity(level, self);
}

void Skel_death_right_animation_play(GameManager* manager, void* context, Entity* self) {
    UNUSED(manager);
    Level* level = game_manager_current_level_get(manager);

    SkeletonContext* skelContext = (SkeletonContext*)context;
    skelContext->sprite = skel_death_right[skel_death_right_current_frame];

    int total_frames = sizeof(skel_death_right) / sizeof(skel_death_right[0]);

    skel_death_right_i++;
    if(skel_death_right_i >= skel_death_right_fps) {
        skel_death_right_current_frame++;
        skel_death_right_i = 0;
    }

    if(skel_death_right_current_frame == total_frames) level_remove_entity(level, self);
}

void skeleton_spawn(Level* level, GameManager* manager) {
    Entity* skeleton = level_add_entity(level, &skel_desc);
    entity_pos_set(skeleton, (Vector){30, 30});
    entity_collider_add_rect(skeleton, 15, 31);

    SkeletonContext* skeleton_context = entity_context_get(skeleton);
    skeleton_sprites_load(manager);
    skeleton_context->sprite =
        game_manager_sprite_load(manager, "enemies/skeleton/walking_0.fxbm");

    previous_health = health;
}

void skel_update(Entity* self, GameManager* manager, void* context) {
    Vector pos = entity_pos_get(self);
    SkeletonContext* skeleton_context = entity_context_get(self);
    GameContext* game_context = game_manager_game_context_get(manager);

    if((pos.y + 31) >= game_context->ground_hight && skeleton_context->Yvelocity >= 0) {
        pos.y = game_context->ground_hight - 31;
        skeleton_context->Yvelocity = 0;
        isSkeletonGrounded = true;
    } else {
        isSkeletonGrounded = false;
    }

    if(!isSkeletonGrounded) {
        skeleton_context->Yvelocity += 0.5;
        pos.y += skeleton_context->Yvelocity;
    }

    if(pos.y < 5) pos.y = 5;
    if(pos.y > 59) pos.y = 59;

    if(is_hurt) {
        if(hurt_timer > 0) {
            hurt_timer--;
        } else {
            is_hurt = false;
        }
    }

    if(!is_hurt && player != NULL) {
        Vector player_pos = entity_pos_get(player);
        PlayerContext* player_context = entity_context_get(player);

        float distance_to_player = pos.x - player_pos.x;

        if(player_context->is_hitting) {
            if(is_player_facing_right && distance_to_player > 0 &&
               distance_to_player <= player_sword_reach) {
                health -= player_context->weapon_damage;
            }
            if(!is_player_facing_right && distance_to_player < 0 &&
               -distance_to_player <= player_sword_reach) {
                health -= player_context->weapon_damage;
            }

            if(health < previous_health) {
                is_hurt = true;
                hurt_timer = hurt_frames_to_wait;
                previous_health = health;
            }
        }
    }

    if(!is_hurt && !is_dead && player != NULL) {
        Vector player_pos = entity_pos_get(player);
        if(player_pos.x > pos.x) {
            pos.x += 0.5;
            is_skel_facing_right = false;
            Skel_walking_right_animation_play(manager, context);
        }
        if(player_pos.x < pos.x) {
            pos.x -= 0.5;
            is_skel_facing_right = true;
            Skel_walking_animation_play(manager, context);
        }
    }

    entity_pos_set(self, pos);

    if(health <= 0) {
        is_dead = true;
    }

    if(is_dead) {
        if(is_skel_facing_right) Skel_death_animation_play(manager, context, self);
        if(!is_skel_facing_right) Skel_death_right_animation_play(manager, context, self);
    }

    UNUSED(manager);
    UNUSED(context);
}

void skel_render(Entity* self, GameManager* manager, Canvas* canvas, void* context) {
    SkeletonContext* skel = context;
    Vector pos = entity_pos_get(self);
    canvas_draw_sprite(canvas, skel->sprite, pos.x - 10, pos.y);
    UNUSED(manager);
}

const EntityDescription skel_desc = {
    .start = NULL,
    .stop = NULL,
    .update = skel_update,
    .render = skel_render,
    .collision = NULL,
    .event = NULL,
    .context_size = sizeof(SkeletonContext),
};
