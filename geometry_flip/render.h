#pragma once
#include "constants.h"
#include "player.h"

// Function prototypes for rendering
void render_callback(Canvas* canvas, void* ctx);
void draw_cube(Canvas* canvas, float y);
void draw_ship(Canvas* canvas, float y, GravityDirection gravity_dir);
void draw_ball(Canvas* canvas, float y);
void draw_ufo(Canvas* canvas, float y);
void draw_vertical_special_object(Canvas* canvas, int screen_x, int base_grid_y, ObstacleType type);
void draw_filled_spike(Canvas* canvas, int screen_x, int screen_y, ObstacleType type);
void draw_obstacle(Canvas* canvas, int screen_x, int grid_y, ObstacleType type);
void draw_ground(Canvas* canvas);
void draw_player(GeometryDashApp* app, Canvas* canvas);
void draw_ui(Canvas* canvas, GeometryDashApp* app);
void draw_game_scene(Canvas* canvas, GeometryDashApp* app);