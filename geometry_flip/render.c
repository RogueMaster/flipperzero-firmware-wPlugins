#include "render.h"
#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <storage/storage.h>
#include <toolbox/stream/file_stream.h>
#include <string.h>
#include <m-array.h>
#include <math.h>

void draw_cube(Canvas* canvas, float y) {
    if (!canvas) return;
    canvas_draw_box(canvas, CUBE_X_POS, (int)y, CUBE_SIZE, CUBE_SIZE);
    canvas_draw_dot(canvas, CUBE_X_POS + 2, (int)y + 2);
    canvas_draw_dot(canvas, CUBE_X_POS + CUBE_SIZE - 3, (int)y + 2);
}

void draw_ship(Canvas* canvas, float y, GravityDirection gravity_dir) {
    if (!canvas) return;
    int ship_y = (int)y;
    int ship_bottom = ship_y + SHIP_SIZE;
    if (gravity_dir == GravityDirectionDown) {
        canvas_draw_line(canvas, CUBE_X_POS, ship_y + SHIP_SIZE/2, CUBE_X_POS + SHIP_SIZE, ship_y);
        canvas_draw_line(canvas, CUBE_X_POS, ship_y + SHIP_SIZE/2, CUBE_X_POS + SHIP_SIZE, ship_bottom);
        canvas_draw_line(canvas, CUBE_X_POS + SHIP_SIZE, ship_y, CUBE_X_POS + SHIP_SIZE, ship_bottom);
    } else {
        canvas_draw_line(canvas, CUBE_X_POS, ship_y + SHIP_SIZE/2, CUBE_X_POS + SHIP_SIZE, ship_y);
        canvas_draw_line(canvas, CUBE_X_POS, ship_y + SHIP_SIZE/2, CUBE_X_POS + SHIP_SIZE, ship_bottom);
        canvas_draw_line(canvas, CUBE_X_POS + SHIP_SIZE, ship_y, CUBE_X_POS + SHIP_SIZE, ship_bottom);
    }
}

void draw_ball(Canvas* canvas, float y) {
    if (!canvas) return;
    canvas_draw_circle(canvas, CUBE_X_POS + BALL_SIZE/2, (int)y + BALL_SIZE/2, BALL_SIZE/2);
    canvas_draw_disc(canvas, CUBE_X_POS + BALL_SIZE/2, (int)y + BALL_SIZE/2, 1);
}

void draw_ufo(Canvas* canvas, float y) {
    if (!canvas) return;
    int ufo_y = (int)y;
    canvas_draw_disc(canvas, CUBE_X_POS + UFO_SIZE/2, ufo_y + UFO_SIZE/2, UFO_SIZE/2);
    canvas_draw_line(canvas, CUBE_X_POS, ufo_y + UFO_SIZE/2, CUBE_X_POS + UFO_SIZE, ufo_y + UFO_SIZE/2);
}

void draw_player(GeometryDashApp* app, Canvas* canvas) {
    if (!app || !canvas) return;
    switch (app->player.player_mode) {
        case PlayerModeCube:
            draw_cube(canvas, app->player.cube_y);
            break;
        case PlayerModeShip:
            draw_ship(canvas, app->player.cube_y, app->player.gravity_dir);
            break;
        case PlayerModeBall:
            draw_ball(canvas, app->player.cube_y);
            break;
        case PlayerModeUfo:
            draw_ufo(canvas, app->player.cube_y);
            break;
        default:
            break; // Обработка неизвестного режима
    }
}

void draw_vertical_special_object(Canvas* canvas, int screen_x, int base_grid_y, ObstacleType type) {
    if (!canvas) return;
    for (int i = 0; i < 3; i++) {
        int y = base_grid_y + i;
        int screen_y = GROUND_Y - (y + 1) * OBSTACLE_HEIGHT;
        canvas_draw_frame(canvas, screen_x, screen_y, OBSTACLE_WIDTH, OBSTACLE_HEIGHT);
        if (i == 1) {
            switch (type) {
                case ObstacleTypePortalShip:
                    canvas_draw_line(canvas, screen_x, screen_y, screen_x + OBSTACLE_WIDTH, screen_y + OBSTACLE_HEIGHT);
                    canvas_draw_line(canvas, screen_x + OBSTACLE_WIDTH, screen_y, screen_x, screen_y + OBSTACLE_HEIGHT);
                    break;
                case ObstacleTypePortalCube:
                    canvas_draw_disc(canvas, screen_x + OBSTACLE_WIDTH/2, screen_y + OBSTACLE_HEIGHT/2, 2);
                    break;
                case ObstacleTypePortalBall:
                    canvas_draw_circle(canvas, screen_x + OBSTACLE_WIDTH/2, screen_y + OBSTACLE_HEIGHT/2, 3);
                    break;
                case ObstacleTypePortalUfo:
                    canvas_draw_disc(canvas, screen_x + OBSTACLE_WIDTH/2, screen_y + OBSTACLE_HEIGHT/2, 3);
                    break;
                case ObstacleTypeGravityUp:
                    canvas_draw_line(canvas, screen_x + OBSTACLE_WIDTH/2, screen_y + 2, screen_x + OBSTACLE_WIDTH/2, screen_y + OBSTACLE_HEIGHT - 2);
                    canvas_draw_line(canvas, screen_x + OBSTACLE_WIDTH/2, screen_y + 2, screen_x + OBSTACLE_WIDTH/2 - 2, screen_y + 4);
                    canvas_draw_line(canvas, screen_x + OBSTACLE_WIDTH/2, screen_y + 2, screen_x + OBSTACLE_WIDTH/2 + 2, screen_y + 4);
                    break;
                case ObstacleTypeGravityDown:
                    canvas_draw_line(canvas, screen_x + OBSTACLE_WIDTH/2, screen_y + 2, screen_x + OBSTACLE_WIDTH/2, screen_y + OBSTACLE_HEIGHT - 2);
                    canvas_draw_line(canvas, screen_x + OBSTACLE_WIDTH/2, screen_y + OBSTACLE_HEIGHT - 2, screen_x + OBSTACLE_WIDTH/2 - 2, screen_y + OBSTACLE_HEIGHT - 4);
                    canvas_draw_line(canvas, screen_x + OBSTACLE_WIDTH/2, screen_y + OBSTACLE_HEIGHT - 2, screen_x + OBSTACLE_WIDTH/2 + 2, screen_y + OBSTACLE_HEIGHT - 4);
                    break;
                default:
                    break;
            }
        }
    }
}

void draw_filled_spike(Canvas* canvas, int screen_x, int screen_y, ObstacleType type) {
    if (!canvas) return;
    switch (type) {
        case ObstacleTypeSpikeUp: {
            for (int y = screen_y; y < screen_y + OBSTACLE_HEIGHT; y++) {
                int height = y - screen_y;
                int width = (OBSTACLE_WIDTH * height) / OBSTACLE_HEIGHT;
                int left_x = screen_x + (OBSTACLE_WIDTH - width) / 2;
                canvas_draw_line(canvas, left_x, y, left_x + width, y);
            }
            break;
        }
        case ObstacleTypeSpikeDown: {
            screen_y -= OBSTACLE_HEIGHT;
            for (int y = screen_y; y < screen_y + OBSTACLE_HEIGHT; y++) {
                int height = (screen_y + OBSTACLE_HEIGHT) - y;
                int width = (OBSTACLE_WIDTH * height) / OBSTACLE_HEIGHT;
                int left_x = screen_x + (OBSTACLE_WIDTH - width) / 2;
                canvas_draw_line(canvas, left_x, y, left_x + width, y);
            }
            break;
        }
        case ObstacleTypeSpikeLeft: {
            for (int x = screen_x; x < screen_x + OBSTACLE_WIDTH; x++) {
                int width = x - screen_x;
                int height = (OBSTACLE_HEIGHT * width) / OBSTACLE_WIDTH;
                int top_y = screen_y + (OBSTACLE_HEIGHT - height) / 2;
                canvas_draw_line(canvas, x, top_y, x, top_y + height);
            }
            break;
        }
        case ObstacleTypeSpikeRight: {
            for (int x = screen_x; x < screen_x + OBSTACLE_WIDTH; x++) {
                int width = (screen_x + OBSTACLE_WIDTH) - x;
                int height = (OBSTACLE_HEIGHT * width) / OBSTACLE_WIDTH;
                int top_y = screen_y + (OBSTACLE_HEIGHT - height) / 2;
                canvas_draw_line(canvas, x, top_y, x, top_y + height);
            }
            break;
        }
        default:
            break;
    }
}

void draw_obstacle(Canvas* canvas, int screen_x, int grid_y, ObstacleType type) {
    if (!canvas) return;
    if (type == ObstacleTypeNone) return;
    bool is_special_object = (type == ObstacleTypePortalShip || type == ObstacleTypePortalCube ||
                              type == ObstacleTypePortalBall || type == ObstacleTypePortalUfo ||
                              type == ObstacleTypeGravityUp || type == ObstacleTypeGravityDown);
    if (is_special_object) {
        draw_vertical_special_object(canvas, screen_x, grid_y, type);
        return;
    }
    int screen_y = 0;
    if (type == ObstacleTypeSpikeUp) {
        screen_y = GROUND_Y - (grid_y + 1) * OBSTACLE_HEIGHT;
    } else if (type == ObstacleTypeSpikeDown) {
        screen_y = GROUND_Y - (grid_y + 1) * OBSTACLE_HEIGHT + OBSTACLE_HEIGHT;
    } else if (type == ObstacleTypeSpikeLeft || type == ObstacleTypeSpikeRight) {
         screen_y = GROUND_Y - (grid_y + 1) * OBSTACLE_HEIGHT;
    }

    switch (type) {
        case ObstacleTypeSpikeUp:
        case ObstacleTypeSpikeDown:
        case ObstacleTypeSpikeLeft:
        case ObstacleTypeSpikeRight:
            draw_filled_spike(canvas, screen_x, screen_y, type);
            break;
        default:
            break;
    }
}

void draw_ground(Canvas* canvas) {
    if (!canvas) return;
    canvas_draw_line(canvas, 0, GROUND_Y, SCREEN_WIDTH, GROUND_Y);
}

void draw_ui(Canvas* canvas, GeometryDashApp* app) {
    if (!canvas || !app) return; // Добавлена проверка на NULL
    char buffer[64];
    int progress = 0; // Инициализация по умолчанию
    // Защита от деления на ноль
    if (app->current_level.max_grid_x > 0) {
        // Убедимся, что знаменатель не равен нулю
        float denominator = (float)(app->current_level.max_grid_x * OBSTACLE_GAP);
        if (denominator > 0.0f) {
             progress = (int)((app->scroll_offset / denominator) * 100);
        }
    }
    if (progress > 100) progress = 100;
    if (progress < 0) progress = 0; // Добавлена проверка на отрицательное значение
    snprintf(buffer, sizeof(buffer), "Progress: %d%%", progress);
    canvas_draw_str(canvas, 2, 10, buffer);
    if(app->state == GameStatePlaying || app->state == GameStateGameOver || app->state == GameStateWin) {
        snprintf(buffer, sizeof(buffer), "Level: %s", app->current_level.name);
        canvas_draw_str(canvas, 2, 20, buffer);
    }
}

void draw_game_scene(Canvas* canvas, GeometryDashApp* app) {
    if (!canvas || !app) return; // Добавлена проверка на NULL

    furi_assert(app);
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if (app->state == GameStateMenu) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 5, AlignCenter, AlignTop, "Geometry Dash");
        canvas_set_font(canvas, FontSecondary);
        size_t file_count = LevelFileArray_size(app->level_files);
        if (file_count == 0) {
             canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, "No levels found!");
             canvas_draw_str_aligned(canvas, 64, 45, AlignCenter, AlignTop, "Put .gflvl files in");
             canvas_draw_str_aligned(canvas, 64, 55, AlignCenter, AlignTop, GD_APP_DATA_PATH);
             return;
        }
        int start_index = MAX(0, app->selected_menu_item - 2);
        int end_index = MIN((int)file_count, start_index + 5);
        for (int i = start_index; i < end_index; i++) {
            int y_pos = 20 + (i - start_index) * 12;
            if (i == app->selected_menu_item) {
                canvas_draw_str(canvas, 2, y_pos, ">");
            }
            // Проверка указателя перед использованием
            char** filename_ptr = LevelFileArray_get(app->level_files, i);
            if(filename_ptr && *filename_ptr) {
                char* filename = *filename_ptr;
                char display_name[32];
                // Безопасное копирование имени файла
                strncpy(display_name, filename, sizeof(display_name) - 1);
                display_name[sizeof(display_name) - 1] = '\0'; // Обеспечиваем завершение строки
                char* dot = strrchr(display_name, '.');
                if(dot) *dot = '\0';
                canvas_draw_str(canvas, 15, y_pos, display_name);
            }
        }
        return;
    }

    if (app->state == GameStateGameOver) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop, "Game Over!");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignTop, "Press OK to Restart");
        canvas_draw_str_aligned(canvas, 64, 55, AlignCenter, AlignTop, "Press BACK to Menu");
        return;
    }

    if (app->state == GameStateWin) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop, "Level Complete!");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignTop, "Press OK to Restart");
        canvas_draw_str_aligned(canvas, 64, 55, AlignCenter, AlignTop, "Press BACK to Menu");
        return;
    }

    draw_ground(canvas);
    draw_player(app, canvas);
    draw_ui(canvas, app);

    int start_grid_x = (int)(app->scroll_offset / OBSTACLE_GAP) - 5;
    int end_grid_x = start_grid_x + (SCREEN_WIDTH / OBSTACLE_GAP) + 10;
    for (int grid_x = start_grid_x; grid_x <= end_grid_x; grid_x++) {
        int obstacle_screen_x = (grid_x * OBSTACLE_GAP) - (int)app->scroll_offset;
        for (int j = 0; j < app->current_level.length; j++) {
            Obstacle* obs = &app->current_level.obstacles[j];
            if (obs->grid_x == grid_x) {
                if (obs->type == ObstacleTypeBlock) {
                    int platform_width = obs->length * OBSTACLE_WIDTH;
                    int screen_y = GROUND_Y - (obs->grid_y + 1) * OBSTACLE_HEIGHT;
                    canvas_draw_box(canvas, obstacle_screen_x, screen_y, platform_width, OBSTACLE_HEIGHT);
                }
                else {
                    draw_obstacle(canvas, obstacle_screen_x, obs->grid_y, obs->type);
                }
            }
        }
    }
}

void render_callback(Canvas* canvas, void* ctx) {
    GeometryDashApp* app = (GeometryDashApp*)ctx;
    if (!app || !canvas) return; // Добавлена проверка на NULL

    draw_game_scene(canvas, app);
}