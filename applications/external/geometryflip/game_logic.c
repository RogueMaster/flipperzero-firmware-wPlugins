#include "game_logic.h"
#include <furi.h>
#include <math.h>

bool check_collision(GeometryDashApp* app, float scroll_offset) {
    if(!app) return false;

    // Get player dimensions based on mode
    int player_width, player_height;
    switch(app->player.player_mode) {
    case PlayerModeCube:
        player_width = CUBE_SIZE;
        player_height = CUBE_SIZE;
        break;
    case PlayerModeShip:
        player_width = SHIP_SIZE;
        player_height = SHIP_SIZE;
        break;
    case PlayerModeBall:
        player_width = BALL_SIZE;
        player_height = BALL_SIZE;
        break;
    case PlayerModeUfo:
        player_width = UFO_SIZE;
        player_height = UFO_SIZE;
        break;
    default:
        player_width = CUBE_SIZE;
        player_height = CUBE_SIZE;
        break;
    }

    int player_screen_x = CUBE_X_POS;
    int player_screen_y = (int)roundf(app->player.cube_y);
    int player_right = player_screen_x + player_width;
    int player_bottom = player_screen_y + player_height;

    // Расширен диапазон проверки препятствий для более точного обнаружения
    int start_grid_x = (int)(scroll_offset / OBSTACLE_GAP) - 4;
    int end_grid_x = start_grid_x + (SCREEN_WIDTH / OBSTACLE_GAP) + 6;

    for(int grid_x = start_grid_x; grid_x <= end_grid_x; grid_x++) {
        int obstacle_screen_x = (grid_x * OBSTACLE_GAP) - (int)scroll_offset;
        for(int j = 0; j < app->current_level.length; j++) {
            Obstacle* obstacle = &app->current_level.obstacles[j];
            if(obstacle->grid_x == grid_x && obstacle->type != ObstacleTypeNone) {
                bool is_special_object =
                    (obstacle->type == ObstacleTypePortalShip ||
                     obstacle->type == ObstacleTypePortalCube ||
                     obstacle->type == ObstacleTypePortalBall ||
                     obstacle->type == ObstacleTypePortalUfo ||
                     obstacle->type == ObstacleTypeGravityDown);
                bool collision = false;

                // Обработка специальных объектов
                if(is_special_object) {
                    for(int k = 0; k < 3; k++) {
                        int block_y = obstacle->grid_y + k;
                        int block_screen_y = GROUND_Y - (block_y + 1) * OBSTACLE_HEIGHT;
                        int block_bottom = block_screen_y + OBSTACLE_HEIGHT;
                        if(player_screen_x < obstacle_screen_x + OBSTACLE_WIDTH &&
                           player_right > obstacle_screen_x && player_screen_y < block_bottom &&
                           player_bottom > block_screen_y) {
                            collision = true;
                            break;
                        }
                    }
                    if(collision) {
                        switch(obstacle->type) {
                        case ObstacleTypePortalShip:
                            app->player.player_mode = PlayerModeShip;
                            app->player.cube_velocity_y = 0.0f;
                            app->player.is_on_ground = false;
                            app->player.gravity_dir = GravityDirectionDown;
                            return false; // No collision death
                        case ObstacleTypePortalCube:
                            app->player.player_mode = PlayerModeCube;
                            app->player.cube_velocity_y = 0.0f;
                            app->player.is_on_ground = false;
                            app->player.gravity_dir = GravityDirectionDown;
                            return false; // No collision death
                        case ObstacleTypePortalBall:
                            app->player.player_mode = PlayerModeBall;
                            app->player.cube_velocity_y = 0.0f;
                            app->player.is_on_ground = false;
                            app->player.gravity_dir = GravityDirectionDown;
                            // Сброс таймера при смене режима может быть полезен
                            app->player.ball_flip_timer = 0;
                            return false; // No collision death
                        case ObstacleTypePortalUfo:
                            app->player.player_mode = PlayerModeUfo;
                            app->player.cube_velocity_y = 0.0f;
                            app->player.is_on_ground = false;
                            app->player.gravity_dir = GravityDirectionDown;
                            return false; // No collision death
                        case ObstacleTypeGravityUp:
                            app->player.gravity_dir = GravityDirectionUp;
                            return false; // No collision death
                        case ObstacleTypeGravityDown:
                            app->player.gravity_dir = GravityDirectionDown;
                            return false; // No collision death
                        default:
                            return true; // Collision death
                        }
                    }
                }
                // Обработка обычных препятствий
                else {
                    int obstacle_screen_y = 0;
                    int obstacle_bottom = 0;
                    int obstacle_right = 0;
                    // Определяем позицию и размеры препятствия
                    if(obstacle->type == ObstacleTypeBlock) {
                        obstacle_screen_y = GROUND_Y - (obstacle->grid_y + 1) * OBSTACLE_HEIGHT;
                        obstacle_right = obstacle_screen_x + (obstacle->length * OBSTACLE_WIDTH);
                        obstacle_bottom = obstacle_screen_y + OBSTACLE_HEIGHT;
                    } else if(obstacle->type == ObstacleTypeSpikeUp) {
                        obstacle_screen_y = GROUND_Y - (obstacle->grid_y + 1) * OBSTACLE_HEIGHT;
                        obstacle_right = obstacle_screen_x + OBSTACLE_WIDTH;
                        obstacle_bottom = obstacle_screen_y + OBSTACLE_HEIGHT;
                    } else if(obstacle->type == ObstacleTypeSpikeDown) {
                        // Шип вниз рисуется немного выше, но логика столкновения такая же
                        obstacle_screen_y = GROUND_Y - (obstacle->grid_y + 1) * OBSTACLE_HEIGHT;
                        obstacle_right = obstacle_screen_x + OBSTACLE_WIDTH;
                        obstacle_bottom = obstacle_screen_y + OBSTACLE_HEIGHT;
                    } else if(
                        obstacle->type == ObstacleTypeSpikeLeft ||
                        obstacle->type == ObstacleTypeSpikeRight) {
                        obstacle_screen_y = GROUND_Y - (obstacle->grid_y + 1) * OBSTACLE_HEIGHT;
                        obstacle_right = obstacle_screen_x + OBSTACLE_WIDTH;
                        obstacle_bottom = obstacle_screen_y + OBSTACLE_HEIGHT;
                    }

                    // Проверка столкновения AABB (Axis-Aligned Bounding Box)
                    if(player_screen_x < obstacle_right && player_right > obstacle_screen_x &&
                       player_screen_y < obstacle_bottom && player_bottom > obstacle_screen_y) {
                        if(obstacle->type == ObstacleTypeBlock) {
                            // Логика для всех режимов игрока
                            int overlap_left = player_right - obstacle_screen_x;
                            int overlap_right = obstacle_right - player_screen_x;
                            int overlap_top = player_bottom - obstacle_screen_y;
                            int overlap_bottom = obstacle_bottom - player_screen_y;
                            int min_overlap =
                                MIN(MIN(overlap_left, overlap_right),
                                    MIN(overlap_top, overlap_bottom));

                            if(min_overlap == overlap_top && app->player.cube_velocity_y >= 0) {
                                // Игрок приземляется сверху на блок
                                app->player.cube_y = (float)(obstacle_screen_y - player_height);
                                app->player.cube_velocity_y = 0.0f;
                                app->player.is_on_ground = true;
                                app->player.ground_lost_time = furi_get_tick();
                                collision = false; // Не умирает
                            } else if(
                                min_overlap == overlap_bottom &&
                                app->player.cube_velocity_y <= 0) {
                                // Игрок ударяется об нижнюю часть блока
                                app->player.cube_y = (float)obstacle_bottom;

                                // Для шара меняем гравитацию, для других режимов просто останавливаем
                                if(app->player.player_mode == PlayerModeBall) {
                                    if(app->player.ball_flip_timer == 0) {
                                        app->player.gravity_dir =
                                            (app->player.gravity_dir == GravityDirectionDown) ?
                                                GravityDirectionUp :
                                                GravityDirectionDown;
                                        app->player.cube_velocity_y =
                                            (app->player.gravity_dir == GravityDirectionDown) ?
                                                BALL_FLIP_IMPULSE :
                                                -BALL_FLIP_IMPULSE;
                                        app->player.ball_flip_timer = BALL_FLIP_COOLDOWN;
                                    } else {
                                        app->player.cube_velocity_y = 0.0f;
                                    }
                                } else {
                                    app->player.cube_velocity_y = 0.0f;
                                }
                                collision = false; // Не умирает
                            } else {
                                // Боковое столкновение - умирает для всех режимов, кроме особых случаев
                                collision = true;
                            }
                        } else if(
                            obstacle->type == ObstacleTypeSpikeUp ||
                            obstacle->type == ObstacleTypeSpikeDown ||
                            obstacle->type == ObstacleTypeSpikeLeft ||
                            obstacle->type == ObstacleTypeSpikeRight) {
                            collision = true; // Любой игрок умирает при контакте со шипами
                        }

                        if(collision) {
                            return true; // Collision death
                        }
                    }
                }
            }
        }
    }

    return false; // No collision
}

void process_game_logic(GeometryDashApp* app, uint32_t dt_ms) {
    if(!app || app->state != GameStatePlaying) return; // Добавлена проверка на NULL

    float dt = dt_ms / 1000.0f;
    app->scroll_offset += SCROLL_SPEED * dt;

    // Защита от деления на ноль и проверка завершения уровня
    if(app->current_level.max_grid_x > 0 && // Добавлена проверка на положительность max_grid_x
       app->scroll_offset >= app->current_level.max_grid_x * OBSTACLE_GAP) {
        app->state = GameStateWin;
        return;
    }

    // Update player physics
    player_update(&app->player, dt_ms, app->input_pressed, app->state);

    // Check for collisions
    if(check_collision(app, app->scroll_offset)) {
        app->state = GameStateGameOver;
        return;
    }
}
