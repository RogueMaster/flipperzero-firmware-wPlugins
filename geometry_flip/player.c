#include "player.h"
#include <furi.h>
#include <math.h>

void player_init(PlayerState* player) {
    if (!player) return;
    
    player->player_mode = PlayerModeCube;
    player->gravity_dir = GravityDirectionDown;
    player->cube_y = GROUND_Y - CUBE_SIZE;
    player->cube_velocity_y = 0.0f;
    player->is_on_ground = true;
    player->ground_lost_time = furi_get_tick();
    player->ball_flip_timer = 0;
}

void player_reset(PlayerState* player) {
    if (!player) return;
    
    player->player_mode = PlayerModeCube;
    player->gravity_dir = GravityDirectionDown;
    player->cube_y = GROUND_Y - CUBE_SIZE;
    player->cube_velocity_y = 0.0f;
    player->is_on_ground = true;
    player->ground_lost_time = furi_get_tick();
    player->ball_flip_timer = 0;
}

void player_update(PlayerState* player, uint32_t dt_ms, bool input_pressed, GameState state) {
    if (!player || state != GameStatePlaying) return;

    float dt = dt_ms / 1000.0f;
    player->is_on_ground = false;

    // Update ball flip timer
    if (player->ball_flip_timer > 0) {
        if (dt_ms >= player->ball_flip_timer) {
            player->ball_flip_timer = 0;
        } else {
            player->ball_flip_timer -= dt_ms;
        }
    }

    switch (player->player_mode) {
        case PlayerModeCube:
            player->cube_velocity_y += CUBE_GRAVITY * dt;
            player->cube_y += player->cube_velocity_y * dt;

            if (player->cube_y > GROUND_Y - CUBE_SIZE) {
                player->cube_y = GROUND_Y - CUBE_SIZE;
                player->cube_velocity_y = 0.0f;
                player->is_on_ground = true;
                player->ground_lost_time = furi_get_tick();
            }
            if (player->cube_y < 0) {
                player->cube_y = 0;
                player->cube_velocity_y = 0.0f;
                player->is_on_ground = true;
                player->ground_lost_time = furi_get_tick();
            }
            break;

        case PlayerModeShip:
            // Плавное ускорение вместо мгновенного изменения скорости
            // Only apply continuous flight when button is pressed, but limit the maximum velocity
            if (input_pressed) {
                float target_velocity = (player->gravity_dir == GravityDirectionDown) ?
                    SHIP_FLY_FORCE :
                    -SHIP_FLY_FORCE;
                // Плавное приближение к целевой скорости
                if (fabsf(player->cube_velocity_y - target_velocity) > 1.0f) {
                    player->cube_velocity_y += (target_velocity - player->cube_velocity_y) * SHIP_ACCELERATION * dt;
                } else {
                    player->cube_velocity_y = target_velocity;
                }
            } else {
                // Используем гравитацию для определения целевой скорости
                float target_velocity = (player->gravity_dir == GravityDirectionDown) ?
                    SHIP_GRAVITY :
                    -SHIP_GRAVITY;
                if (fabsf(player->cube_velocity_y - target_velocity) > 1.0f) {
                    player->cube_velocity_y += (target_velocity - player->cube_velocity_y) * SHIP_ACCELERATION * dt;
                } else {
                    player->cube_velocity_y = target_velocity;
                }
            }
            // Ограничение максимальной скорости
            if (player->gravity_dir == GravityDirectionDown) {
                if (player->cube_velocity_y > SHIP_MAX_FALLSPEED) player->cube_velocity_y = SHIP_MAX_FALLSPEED;
                if (player->cube_velocity_y < SHIP_MAX_RISESPEED) player->cube_velocity_y = SHIP_MAX_RISESPEED;
            } else {
                if (player->cube_velocity_y < -SHIP_MAX_FALLSPEED) player->cube_velocity_y = -SHIP_MAX_FALLSPEED;
                if (player->cube_velocity_y > -SHIP_MAX_RISESPEED) player->cube_velocity_y = -SHIP_MAX_RISESPEED;
            }
            player->cube_y += player->cube_velocity_y * dt;
            // Ограничение по вертикали
            if (player->cube_y > GROUND_Y - SHIP_SIZE) {
                player->cube_y = GROUND_Y - SHIP_SIZE;
                player->cube_velocity_y = 0.0f;
                player->is_on_ground = true;
                player->ground_lost_time = furi_get_tick();
            }
            if (player->cube_y < 0) {
                player->cube_y = 0;
                player->cube_velocity_y = 0.0f;
                player->is_on_ground = true;
                player->ground_lost_time = furi_get_tick();
            }
            break;

        case PlayerModeBall:
            // Применяем гравитацию
            float ball_gravity = (player->gravity_dir == GravityDirectionDown) ? BALL_GRAVITY : -BALL_GRAVITY;
            player->cube_velocity_y += ball_gravity * dt;
            player->cube_y += player->cube_velocity_y * dt;

            // --- НОВАЯ логика для шара - гравитация меняется ТОЛЬКО по нажатию кнопки ---
            // Убираем автоматическое изменение гравитации при касании пола/потолка
            // Теперь шар будет отскакивать от пола и потолка, как обычный мяч

            // Проверяем касание пола
            if (player->cube_y >= GROUND_Y - BALL_SIZE) {
                player->cube_y = GROUND_Y - BALL_SIZE; // Точное позиционирование
                player->cube_velocity_y = fabsf(player->cube_velocity_y) * 0.8f; // Отскок с потерей энергии
                player->is_on_ground = true;
                player->ground_lost_time = furi_get_tick();
            }
            // Проверяем касание потолка
            else if (player->cube_y <= 0) {
                player->cube_y = 0; // Точное позиционирование
                player->cube_velocity_y = -fabsf(player->cube_velocity_y) * 0.8f; // Отскок с потерей энергии
            }
            // --- Конец новой логики для шара ---
            break;

        case PlayerModeUfo:
            player->cube_velocity_y += UFO_GRAVITY * dt;
            player->cube_y += player->cube_velocity_y * dt;
            if (player->cube_y > GROUND_Y - UFO_SIZE) {
                player->cube_y = GROUND_Y - UFO_SIZE;
                player->cube_velocity_y = 0.0f;
                player->is_on_ground = true;
                player->ground_lost_time = furi_get_tick();
            } else {
                player->is_on_ground = false;
            }
            if (player->cube_y < 0) {
                player->cube_y = 0;
                player->cube_velocity_y = 0.0f;
                player->is_on_ground = true;
                player->ground_lost_time = furi_get_tick();
            }
            break;

        default:
            break;
    }
}

void player_handle_input(PlayerState* player, bool input_pressed) {
    if (!player) return;

    // This function is mainly for handling special input for different player modes
    if (input_pressed) {
        switch (player->player_mode) {
            case PlayerModeCube:
                // Only jump if on ground or within coyote time
                {
                    uint32_t current_time = furi_get_tick();
                    uint32_t time_since_ground_lost = current_time - player->ground_lost_time;
                    bool can_jump = player->is_on_ground || (!player->is_on_ground && time_since_ground_lost < COYOTE_TIME_MS);

                    if (can_jump) {
                        player->cube_velocity_y = CUBE_JUMP_VEL;
                    }
                }
                break;
            case PlayerModeShip:
                // For ship mode, we don't apply a jump in player_handle_input
                // because ship mode is controlled by continuous input in player_update
                // So we don't do anything here to avoid conflict with continuous flight
                break;
            case PlayerModeBall:
                // For ball mode, pressing button flips gravity (if not in cooldown)
                // To reduce input delay perception, we'll handle input differently
                if (player->ball_flip_timer == 0) {
                    // Change gravity direction
                    player->gravity_dir = (player->gravity_dir == GravityDirectionDown) ?
                        GravityDirectionUp : GravityDirectionDown;
                    // Apply velocity in the direction of the NEW gravity
                    player->cube_velocity_y = (player->gravity_dir == GravityDirectionDown) ?
                        BALL_JUMP_VEL : -BALL_JUMP_VEL;
                    player->ball_flip_timer = BALL_FLIP_COOLDOWN;
                }
                // Note: We don't prevent input during cooldown in the input handler
                // The cooldown is handled in the physics to prevent multiple flips per press
                break;
            case PlayerModeUfo:
                // For ufo mode, jumping is always possible
                player->cube_velocity_y = UFO_JUMP_VEL;
                break;
            default:
                break;
        }
    } else {
        // Handle input release - only relevant for ship mode
        if (player->player_mode == PlayerModeShip) {
            // For ship mode, when button is released, it should follow gravity
            // This is already handled in the player_update function
        }
    }
    // Ship mode handles continuous input in the update function
}