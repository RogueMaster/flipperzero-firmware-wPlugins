#include "game.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations
static const EntityDescription submarine_desc;
static const EntityDescription torpedo_desc;
static const LevelBehaviour level;

/****** Camera/Coordinate System ******/

typedef struct {
    float screen_x;
    float screen_y;
} ScreenPoint;

// Convert portrait coordinates to landscape screen coordinates
// Portrait (64x128) -> Landscape (128x64) with 90° CW rotation
static ScreenPoint portrait_to_screen(float portrait_x, float portrait_y) {
    ScreenPoint screen;
    // Rotate 90° clockwise: (x,y) -> (y, 63-x)
    screen.screen_x = portrait_y;
    screen.screen_y = 63 - portrait_x;
    return screen;
}

// Transform world coordinates to screen coordinates relative to submarine
static ScreenPoint world_to_screen(GameContext* ctx, float world_x, float world_y) {
    // Translate relative to submarine position
    float rel_x = world_x - ctx->world_x;
    float rel_y = world_y - ctx->world_y;

    // Rotate around submarine (submarine always points "up" on screen)
    float cos_h = cosf(-ctx->heading * 2 * 3.14159f); // Negative for counter-rotation
    float sin_h = sinf(-ctx->heading * 2 * 3.14159f);

    float rot_x = rel_x * cos_h - rel_y * sin_h;
    float rot_y = rel_x * sin_h + rel_y * cos_h;

    // Calculate portrait coordinates first (64x128 coordinate system)
    float portrait_x = 32 + rot_x; // Center at 32 (half of 64)
    float portrait_y = 64 + rot_y; // Center at 64 (half of 128)

    // Convert to actual screen coordinates (128x64)
    return portrait_to_screen(portrait_x, portrait_y);
}

// Transform screen coordinates back to world coordinates (for future use)
__attribute__((unused)) static void screen_to_world(
    GameContext* ctx,
    float screen_x,
    float screen_y,
    float* world_x,
    float* world_y) {
    // Translate relative to submarine screen position
    float rel_x = screen_x - ctx->screen_x;
    float rel_y = screen_y - ctx->screen_y;

    // Rotate to world coordinates
    float cos_h = cosf(ctx->heading * 2 * 3.14159f);
    float sin_h = sinf(ctx->heading * 2 * 3.14159f);

    float rot_x = rel_x * cos_h - rel_y * sin_h;
    float rot_y = rel_x * sin_h + rel_y * cos_h;

    // Translate to world coordinates
    *world_x = ctx->world_x + rot_x;
    *world_y = ctx->world_y + rot_y;
}

/****** Input Handling ******/

static void handle_input(GameManager* manager, GameContext* game_context) {
    InputState input = game_manager_input_get(manager);
    uint32_t current_time = furi_get_tick();

    // Handle back button long/short press
    if(input.pressed & GameKeyBack) {
        game_context->back_press_start = current_time;
        game_context->back_long_press = false;
    }

    if(input.held & GameKeyBack) {
        if(current_time - game_context->back_press_start > 1000) { // 1 second for long press
            if(!game_context->back_long_press) {
                // Long press detected - exit game
                game_manager_game_stop(manager);
                return;
            }
        }
    }

    if(input.released & GameKeyBack) {
        if(current_time - game_context->back_press_start < 1000) {
            // Short press - toggle mode
            game_context->mode = (game_context->mode == GAME_MODE_NAV) ? GAME_MODE_TORPEDO :
                                                                         GAME_MODE_NAV;
        }
    }
}

/****** Entities: Submarine ******/

typedef struct {
    GameContext* game_context;
} SubmarineContext;

static void submarine_start(Entity* self, GameManager* manager, void* context) {
    UNUSED(self);
    UNUSED(manager);
    SubmarineContext* sub_context = context;

    // Initialize submarine at center of screen
    entity_pos_set(self, (Vector){64, 32});

    // Add collision detection
    entity_collider_add_circle(self, 2);

    // Get game context reference
    sub_context->game_context = game_manager_game_context_get(manager);
}

static void submarine_update(Entity* self, GameManager* manager, void* context) {
    SubmarineContext* sub_context = context;
    GameContext* game_context = sub_context->game_context;

    // Handle input first
    handle_input(manager, game_context);

    InputState input = game_manager_input_get(manager);

    // Handle movement controls (rotated 90° CCW for portrait mode)
    // Physical UP -> turn right, DOWN -> turn left, LEFT -> accelerate, RIGHT -> decelerate
    if(input.held & GameKeyUp) { // Physical up = turn right
        game_context->heading += game_context->turn_rate;
        if(game_context->heading >= 1.0f) game_context->heading -= 1.0f;
    }
    if(input.held & GameKeyDown) { // Physical down = turn left
        game_context->heading -= game_context->turn_rate;
        if(game_context->heading < 0) game_context->heading += 1.0f;
    }
    if(input.pressed & GameKeyLeft) { // Physical left = accelerate (discrete)
        if(game_context->velocity < 0.001f) {
            game_context->velocity = 0.033f; // Stop -> Slow
        } else if(game_context->velocity < 0.034f) {
            game_context->velocity = 0.066f; // Slow -> Medium
        } else if(game_context->velocity < 0.067f) {
            game_context->velocity = 0.1f; // Medium -> Fast
        }
        // Already at max, do nothing
    }
    if(input.pressed & GameKeyRight) { // Physical right = decelerate (discrete)
        if(game_context->velocity > 0.067f) {
            game_context->velocity = 0.066f; // Fast -> Medium
        } else if(game_context->velocity > 0.034f) {
            game_context->velocity = 0.033f; // Medium -> Slow
        } else if(game_context->velocity > 0.001f) {
            game_context->velocity = 0.0f; // Slow -> Stop
        }
        // Already stopped, do nothing
    }

    // Handle OK button (ping or fire)
    if(input.pressed & GameKeyOk) {
        if(game_context->mode == GAME_MODE_NAV) {
            // Start ping
            if(!game_context->ping_active) {
                game_context->ping_active = true;
                game_context->ping_x = game_context->world_x;
                game_context->ping_y = game_context->world_y;
                game_context->ping_radius = 0;
                game_context->ping_timer = furi_get_tick();
            }
        } else {
            // Fire torpedo
            if(game_context->torpedo_count < game_context->max_torpedoes) {
                Level* current_level = game_manager_current_level_get(manager);
                Entity* torpedo = level_add_entity(current_level, &torpedo_desc);
                if(torpedo) {
                    // Set torpedo initial world position and screen position
                    entity_pos_set(
                        torpedo, (Vector){game_context->screen_x, game_context->screen_y});
                    game_context->torpedo_count++;
                }
            }
        }
    }

    // Update ping with terrain detection
    if(game_context->ping_active) {
        uint32_t current_time = furi_get_tick();
        if(current_time - game_context->ping_timer > 50) { // Update every 50ms
            game_context->ping_radius += 2;
            game_context->ping_timer = current_time;

            // Perform raycasting to detect terrain
            if(game_context->terrain && game_context->sonar_chart) {
                for(float angle = 0; angle < 2 * 3.14159f; angle += 0.1f) {
                    int ray_x =
                        (int)(game_context->ping_x + cosf(angle) * game_context->ping_radius);
                    int ray_y =
                        (int)(game_context->ping_y + sinf(angle) * game_context->ping_radius);

                    if(ray_x >= 0 && ray_x < game_context->chart_width && ray_y >= 0 &&
                       ray_y < game_context->chart_height) {
                        // Mark as discovered in sonar chart
                        int chart_idx = ray_y * game_context->chart_width + ray_x;
                        game_context->sonar_chart[chart_idx] = true;
                    }
                }
            }

            if(game_context->ping_radius > 40) { // Max ping radius (reduced for screen size)
                game_context->ping_active = false;
            }
        }
    }

    // Update submarine world position
    // Adjust heading so 0 = forward (negative Y), matching screen orientation
    float movement_heading = (game_context->heading - 0.25f) * 2 * 3.14159f;
    float dx = game_context->velocity * cosf(movement_heading);
    float dy = game_context->velocity * sinf(movement_heading);

    float new_world_x = game_context->world_x + dx;
    float new_world_y = game_context->world_y + dy;

    // Check terrain collision in world coordinates
    if(game_context->terrain &&
       terrain_check_collision(game_context->terrain, (int)new_world_x, (int)new_world_y)) {
        // Stop submarine if hitting terrain
        game_context->velocity = 0;
    } else {
        game_context->world_x = new_world_x;
        game_context->world_y = new_world_y;
    }

    // Submarine screen position is always centered (no boundary checks needed)
    // Update entity position to screen center
    entity_pos_set(self, (Vector){game_context->screen_x, game_context->screen_y});
}

static void submarine_render(Entity* self, GameManager* manager, Canvas* canvas, void* context) {
    UNUSED(self);
    UNUSED(manager);
    SubmarineContext* sub_context = context;
    GameContext* game_context = sub_context->game_context;

    // Draw terrain - transform world coordinates to screen
    if(game_context->terrain && game_context->sonar_chart) {
        // Sample terrain around submarine's world position
        int sample_radius = 80; // How far to sample around submarine

        for(int world_y = (int)game_context->world_y - sample_radius;
            world_y <= (int)game_context->world_y + sample_radius;
            world_y++) {
            for(int world_x = (int)game_context->world_x - sample_radius;
                world_x <= (int)game_context->world_x + sample_radius;
                world_x++) {
                // Check if this world coordinate is in bounds and discovered
                if(world_x >= 0 && world_x < game_context->chart_width && world_y >= 0 &&
                   world_y < game_context->chart_height) {
                    int chart_idx = world_y * game_context->chart_width + world_x;
                    if(game_context->sonar_chart[chart_idx] &&
                       terrain_check_collision(game_context->terrain, world_x, world_y)) {
                        // Transform world coordinates to screen
                        ScreenPoint screen = world_to_screen(game_context, world_x, world_y);

                        // Only draw if on screen (landscape screen)
                        if(screen.screen_x >= 0 && screen.screen_x < 128 && screen.screen_y >= 0 &&
                           screen.screen_y < 64) {
                            canvas_draw_dot(canvas, screen.screen_x, screen.screen_y);
                        }
                    }
                }
            }
        }
    }

    // Draw submarine (always centered and pointing up in portrait)
    ScreenPoint sub_screen = portrait_to_screen(32, 64);
    canvas_draw_disc(canvas, sub_screen.screen_x, sub_screen.screen_y, 2);

    // Draw heading indicator (always pointing up in portrait)
    ScreenPoint head_screen = portrait_to_screen(32, 64 - 8); // Point up in portrait
    canvas_draw_line(
        canvas,
        sub_screen.screen_x,
        sub_screen.screen_y,
        head_screen.screen_x,
        head_screen.screen_y);

    // Draw velocity vector or torpedo targeting
    if(game_context->mode == GAME_MODE_NAV && game_context->velocity > 0.01f) {
        // Navigation mode: show velocity vector (always pointing up in portrait)
        ScreenPoint vel_screen = portrait_to_screen(32, 64 - 8 - game_context->velocity * 20);
        canvas_draw_line(
            canvas,
            head_screen.screen_x,
            head_screen.screen_y,
            vel_screen.screen_x,
            vel_screen.screen_y);
    } else if(game_context->mode == GAME_MODE_TORPEDO) {
        // Torpedo mode: show targeting cone (symmetric around up direction in portrait)
        float range = 30.0f;
        float cone_offset = 8.0f; // pixels offset for cone width

        ScreenPoint target1_screen = portrait_to_screen(32 - cone_offset, 64 - range);
        ScreenPoint target2_screen = portrait_to_screen(32 + cone_offset, 64 - range);

        canvas_draw_line(
            canvas,
            sub_screen.screen_x,
            sub_screen.screen_y,
            target1_screen.screen_x,
            target1_screen.screen_y);
        canvas_draw_line(
            canvas,
            sub_screen.screen_x,
            sub_screen.screen_y,
            target2_screen.screen_x,
            target2_screen.screen_y);
    }

    // Draw ping (transform ping center to screen)
    if(game_context->ping_active) {
        ScreenPoint ping_screen =
            world_to_screen(game_context, game_context->ping_x, game_context->ping_y);
        canvas_draw_circle(
            canvas, ping_screen.screen_x, ping_screen.screen_y, game_context->ping_radius);
    }

    // Draw velocity bars (bottom left in portrait)
    int vel_bars = 0;
    if(game_context->velocity > 0.066f)
        vel_bars = 3; // Fast
    else if(game_context->velocity > 0.033f)
        vel_bars = 2; // Medium
    else if(game_context->velocity > 0.001f)
        vel_bars = 1; // Slow

    for(int i = 0; i < 3; i++) {
        ScreenPoint bar_screen = portrait_to_screen(2 + (i * 4), 120);
        if(i < vel_bars) {
            canvas_draw_box(canvas, bar_screen.screen_x, bar_screen.screen_y, 3, 6); // Filled bar
        } else {
            canvas_draw_frame(
                canvas, bar_screen.screen_x, bar_screen.screen_y, 3, 6); // Empty frame
        }
    }

    // Draw mode indicator boxes
    if(game_context->mode == GAME_MODE_NAV) {
        // Box around velocity bars for nav mode
        ScreenPoint box_screen = portrait_to_screen(0, 114);
        canvas_draw_frame(canvas, box_screen.screen_x, box_screen.screen_y, 16, 12);
    }

    // Draw torpedo indicators (bottom right in portrait)
    for(int i = 0; i < game_context->max_torpedoes; i++) {
        int torp_portrait_x = 40 + (i % 4) * 6; // 4 columns
        int torp_portrait_y = 118 + (i / 4) * 8; // 2 rows
        ScreenPoint torp_screen = portrait_to_screen(torp_portrait_x, torp_portrait_y);

        if(i < game_context->torpedo_count) {
            // Draw filled torpedo (fired)
            canvas_draw_disc(canvas, torp_screen.screen_x, torp_screen.screen_y, 1);
            ScreenPoint torp_tail_screen =
                portrait_to_screen(torp_portrait_x + 2, torp_portrait_y);
            canvas_draw_line(
                canvas,
                torp_tail_screen.screen_x,
                torp_tail_screen.screen_y,
                torp_tail_screen.screen_x + 1,
                torp_tail_screen.screen_y);
        } else {
            // Draw outline torpedo (available)
            canvas_draw_circle(canvas, torp_screen.screen_x, torp_screen.screen_y, 1);
            ScreenPoint torp_tail_screen =
                portrait_to_screen(torp_portrait_x + 2, torp_portrait_y);
            canvas_draw_dot(canvas, torp_tail_screen.screen_x, torp_tail_screen.screen_y);
            canvas_draw_dot(canvas, torp_tail_screen.screen_x + 1, torp_tail_screen.screen_y);
        }
    }

    // Draw torpedo mode indicator box
    if(game_context->mode == GAME_MODE_TORPEDO) {
        // Box around torpedo indicators for torpedo mode
        ScreenPoint torp_box_screen = portrait_to_screen(38, 116);
        canvas_draw_frame(canvas, torp_box_screen.screen_x, torp_box_screen.screen_y, 26, 18);
    }
}

static const EntityDescription submarine_desc = {
    .start = submarine_start,
    .stop = NULL,
    .update = submarine_update,
    .render = submarine_render,
    .collision = NULL,
    .event = NULL,
    .context_size = sizeof(SubmarineContext),
};

/****** Entities: Torpedo ******/

typedef struct {
    float world_x;
    float world_y;
    float heading;
    float speed;
    GameContext* game_context;
} TorpedoContext;

static void torpedo_start(Entity* self, GameManager* manager, void* context) {
    UNUSED(self);
    TorpedoContext* torp_context = context;
    GameContext* game_context = game_manager_game_context_get(manager);

    // Initialize torpedo with submarine's current world position and heading
    torp_context->world_x = game_context->world_x;
    torp_context->world_y = game_context->world_y;
    torp_context->heading = game_context->heading;
    torp_context->speed = 0.15f; // Faster than submarine max speed
    torp_context->game_context = game_context;

    // Add collision detection
    entity_collider_add_circle(self, 1);
}

static void torpedo_update(Entity* self, GameManager* manager, void* context) {
    UNUSED(manager);
    TorpedoContext* torp_context = context;

    // Move torpedo in world coordinates
    // Use same heading adjustment as submarine movement
    float movement_heading = (torp_context->heading - 0.25f) * 2 * 3.14159f;
    float dx = torp_context->speed * cosf(movement_heading);
    float dy = torp_context->speed * sinf(movement_heading);

    torp_context->world_x += dx;
    torp_context->world_y += dy;

    // Check terrain collision in world coordinates
    if(torp_context->game_context->terrain && terrain_check_collision(
                                                  torp_context->game_context->terrain,
                                                  (int)torp_context->world_x,
                                                  (int)torp_context->world_y)) {
        // Torpedo hit terrain - remove it
        Level* current_level = game_manager_current_level_get(manager);
        torp_context->game_context->torpedo_count--;
        level_remove_entity(current_level, self);
        return;
    }

    // Check if torpedo is too far from submarine (replace screen boundary check)
    float dist_x = torp_context->world_x - torp_context->game_context->world_x;
    float dist_y = torp_context->world_y - torp_context->game_context->world_y;
    float distance_squared = dist_x * dist_x + dist_y * dist_y;

    if(distance_squared > 100 * 100) { // Max range of 100 units
        // Torpedo went too far - remove it
        Level* current_level = game_manager_current_level_get(manager);
        torp_context->game_context->torpedo_count--;
        level_remove_entity(current_level, self);
        return;
    }

    // Transform torpedo world position to screen position
    ScreenPoint screen =
        world_to_screen(torp_context->game_context, torp_context->world_x, torp_context->world_y);
    entity_pos_set(self, (Vector){screen.screen_x, screen.screen_y});
}

static void torpedo_render(Entity* self, GameManager* manager, Canvas* canvas, void* context) {
    UNUSED(manager);
    UNUSED(context);
    Vector pos = entity_pos_get(self);

    // Draw torpedo as small filled circle
    canvas_draw_disc(canvas, pos.x, pos.y, 1);
}

static void torpedo_stop(Entity* self, GameManager* manager, void* context) {
    UNUSED(self);
    UNUSED(manager);
    TorpedoContext* torp_context = context;

    // Decrement torpedo count when torpedo is destroyed
    if(torp_context->game_context->torpedo_count > 0) {
        torp_context->game_context->torpedo_count--;
    }
}

static const EntityDescription torpedo_desc = {
    .start = torpedo_start,
    .stop = torpedo_stop,
    .update = torpedo_update,
    .render = torpedo_render,
    .collision = NULL,
    .event = NULL,
    .context_size = sizeof(TorpedoContext),
};

/****** Level ******/

static void level_alloc(Level* level, GameManager* manager, void* context) {
    UNUSED(context);
    UNUSED(manager);

    // Add submarine entity to the level
    level_add_entity(level, &submarine_desc);
}

static const LevelBehaviour level = {
    .alloc = level_alloc,
    .free = NULL,
    .start = NULL,
    .stop = NULL,
    .context_size = 0,
};

/****** Game ******/

static void game_start(GameManager* game_manager, void* ctx) {
    GameContext* game_context = ctx;

    // Initialize terrain system first
    game_context->terrain = terrain_manager_alloc(12345, 0.5f); // seed=12345, elevation=0.5

    // Initialize sonar chart (same size as screen)
    game_context->chart_width = 128;
    game_context->chart_height = 64;
    size_t chart_size = game_context->chart_width * game_context->chart_height;
    game_context->sonar_chart = malloc(chart_size * sizeof(bool));
    if(game_context->sonar_chart) {
        memset(game_context->sonar_chart, 0, chart_size * sizeof(bool));
    }

    // Set submarine screen position (always center) - landscape screen
    game_context->screen_x = 64; // Center of 128px screen width
    game_context->screen_y = 32; // Center of 64px screen height

    // Find a safe starting world position in water
    game_context->world_x = 64;
    game_context->world_y = 32;

    // Search more thoroughly for water if starting position is in terrain
    if(game_context->terrain) {
        bool found_water = false;

        // First check if default position has enough open water around it
        bool default_has_open_water = true;
        for(int dy = -5; dy <= 5 && default_has_open_water; dy++) {
            for(int dx = -5; dx <= 5 && default_has_open_water; dx++) {
                int check_x = (int)game_context->world_x + dx;
                int check_y = (int)game_context->world_y + dy;
                if(terrain_check_collision(game_context->terrain, check_x, check_y)) {
                    default_has_open_water = false;
                }
            }
        }

        if(default_has_open_water) {
            found_water = true;
        }

        // If not, search in expanding circles for a good water area
        if(!found_water) {
            for(int radius = 10; radius <= 50 && !found_water; radius += 5) {
                for(int angle = 0; angle < 36 && !found_water; angle++) {
                    float test_angle = angle * (2.0f * 3.14159f / 36.0f);
                    int test_x = (int)(game_context->world_x + cosf(test_angle) * radius);
                    int test_y = (int)(game_context->world_y + sinf(test_angle) * radius);

                    // Keep within terrain bounds with bigger margin
                    if(test_x >= 15 && test_x < game_context->chart_width - 15 && test_y >= 15 &&
                       test_y < game_context->chart_height - 15) {
                        // Check for open water in a 10x10 area around this position
                        bool has_open_water = true;
                        for(int dy = -5; dy <= 5 && has_open_water; dy++) {
                            for(int dx = -5; dx <= 5 && has_open_water; dx++) {
                                int check_x = test_x + dx;
                                int check_y = test_y + dy;
                                if(terrain_check_collision(
                                       game_context->terrain, check_x, check_y)) {
                                    has_open_water = false;
                                }
                            }
                        }

                        if(has_open_water) {
                            game_context->world_x = test_x;
                            game_context->world_y = test_y;
                            found_water = true;
                        }
                    }
                }
            }
        }

        // No initial sonar coverage - start with blank map
        // Player must use sonar to discover terrain
    }
    game_context->velocity = 0;
    game_context->heading = 0;
    game_context->mode = GAME_MODE_NAV;

    game_context->torpedo_count = 0;
    game_context->max_torpedoes = 6;

    game_context->ping_active = false;
    game_context->ping_radius = 0;

    game_context->back_press_start = 0;
    game_context->back_long_press = false;

    // Game settings
    game_context->max_velocity = 0.1f;
    game_context->turn_rate = 0.002f;
    game_context->acceleration = 0.002f;

    // Add level to the game
    game_manager_add_level(game_manager, &level);
}

static void game_stop(void* ctx) {
    GameContext* game_context = ctx;

    // Clean up terrain system
    if(game_context->terrain) {
        terrain_manager_free(game_context->terrain);
    }

    // Clean up sonar chart
    if(game_context->sonar_chart) {
        free(game_context->sonar_chart);
    }
}

const Game game = {
    .target_fps = 30,
    .show_fps = false,
    .always_backlight = true,
    .start = game_start,
    .stop = game_stop,
    .context_size = sizeof(GameContext),
};
