#include "partition_panic_game.h"

#include <string.h>

#define PP_FP_SHIFT        8
#define PP_FP_ONE          (1L << PP_FP_SHIFT)
#define PP_BALL_RADIUS     136
#define PP_WALL_STEP_TICKS 2

static bool pp_cell_is_solid(PPCell cell) {
    return (cell == PPCellWall) || (cell == PPCellFilled);
}

static uint32_t pp_random(PPGame* game) {
    uint32_t value = game->random_state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    game->random_state = value ? value : 0x6D2B79F5UL;
    return game->random_state;
}

static uint8_t pp_level_ball_count(uint8_t level) {
    uint8_t count = level + 1;
    return count > PP_MAX_BALLS ? PP_MAX_BALLS : count;
}

static uint32_t pp_level_seconds(uint8_t level) {
    return 90U + ((uint32_t)(level - 1U) * 15U);
}

static void pp_clear_growing_cells(PPGame* game) {
    for(uint8_t y = 1; y < PP_GRID_HEIGHT - 1; y++) {
        for(uint8_t x = 1; x < PP_GRID_WIDTH - 1; x++) {
            if(game->cells[y][x] == PPCellGrowing) {
                game->cells[y][x] = PPCellEmpty;
            }
        }
    }
    memset(&game->growing_wall, 0, sizeof(game->growing_wall));
}

static void pp_spawn_balls(PPGame* game) {
    const int32_t speed = 38 + ((int32_t)game->level * 3);

    for(uint8_t index = 0; index < game->ball_count; index++) {
        uint8_t x = 0;
        uint8_t y = 0;
        bool separated = false;

        for(uint8_t attempt = 0; attempt < 64 && !separated; attempt++) {
            x = 4U + (pp_random(game) % (PP_GRID_WIDTH - 8U));
            y = 3U + (pp_random(game) % (PP_GRID_HEIGHT - 6U));
            separated = true;
            for(uint8_t other = 0; other < index; other++) {
                int32_t dx = (int32_t)x - (game->balls[other].x >> PP_FP_SHIFT);
                int32_t dy = (int32_t)y - (game->balls[other].y >> PP_FP_SHIFT);
                if((dx * dx + dy * dy) < 16) {
                    separated = false;
                    break;
                }
            }
        }

        game->balls[index].x = ((int32_t)x << PP_FP_SHIFT) + (PP_FP_ONE / 2);
        game->balls[index].y = ((int32_t)y << PP_FP_SHIFT) + (PP_FP_ONE / 2);
        game->balls[index].vx = (pp_random(game) & 1U) ? speed : -speed;
        game->balls[index].vy = (pp_random(game) & 1U) ? speed : -speed;
    }
}

static void pp_setup_level(PPGame* game) {
    memset(game->cells, 0, sizeof(game->cells));
    memset(&game->growing_wall, 0, sizeof(game->growing_wall));

    for(uint8_t x = 0; x < PP_GRID_WIDTH; x++) {
        game->cells[0][x] = PPCellWall;
        game->cells[PP_GRID_HEIGHT - 1][x] = PPCellWall;
    }
    for(uint8_t y = 0; y < PP_GRID_HEIGHT; y++) {
        game->cells[y][0] = PPCellWall;
        game->cells[y][PP_GRID_WIDTH - 1] = PPCellWall;
    }

    game->ball_count = pp_level_ball_count(game->level);
    game->cursor_x = PP_GRID_WIDTH / 2;
    game->cursor_y = PP_GRID_HEIGHT / 2;
    game->cursor_horizontal = false;
    game->time_remaining_ticks = pp_level_seconds(game->level) * PP_TICKS_PER_SECOND;
    game->invalid_action_ticks = 0;
    pp_spawn_balls(game);
}

void pp_game_init(PPGame* game, uint32_t seed) {
    memset(game, 0, sizeof(*game));
    game->random_state = seed ? seed : 0xA341316CUL;
    game->level = 1;
    game->lives = 3;
    game->screen = PPScreenTitle;
    pp_setup_level(game);
    game->screen = PPScreenTitle;
}

void pp_game_start(PPGame* game) {
    game->level = 1;
    game->lives = 3;
    game->score = 0;
    pp_setup_level(game);
    game->screen = PPScreenPlaying;
}

void pp_game_advance_level(PPGame* game) {
    if(game->screen != PPScreenLevelClear) return;

    if(game->level >= PP_FINAL_LEVEL) {
        game->screen = PPScreenVictory;
        return;
    }

    game->level++;
    pp_setup_level(game);
    game->screen = PPScreenPlaying;
}

void pp_game_toggle_pause(PPGame* game) {
    if(game->screen == PPScreenPlaying) {
        game->screen = PPScreenPaused;
    } else if(game->screen == PPScreenPaused) {
        game->screen = PPScreenPlaying;
    }
}

void pp_game_move_cursor(PPGame* game, int8_t dx, int8_t dy, bool horizontal) {
    if(game->screen != PPScreenPlaying) return;

    int16_t next_x = (int16_t)game->cursor_x + dx;
    int16_t next_y = (int16_t)game->cursor_y + dy;
    if(next_x < 1) next_x = 1;
    if(next_x > PP_GRID_WIDTH - 2) next_x = PP_GRID_WIDTH - 2;
    if(next_y < 1) next_y = 1;
    if(next_y > PP_GRID_HEIGHT - 2) next_y = PP_GRID_HEIGHT - 2;
    game->cursor_x = (uint8_t)next_x;
    game->cursor_y = (uint8_t)next_y;
    game->cursor_horizontal = horizontal;
}

bool pp_game_start_wall(PPGame* game) {
    if((game->screen != PPScreenPlaying) || game->growing_wall.active ||
       (game->cells[game->cursor_y][game->cursor_x] != PPCellEmpty)) {
        game->invalid_action_ticks = PP_TICKS_PER_SECOND / 2;
        return false;
    }

    PPGrowingWall* wall = &game->growing_wall;
    wall->active = true;
    wall->horizontal = game->cursor_horizontal;
    wall->origin_x = game->cursor_x;
    wall->origin_y = game->cursor_y;
    wall->negative = wall->horizontal ? wall->origin_x : wall->origin_y;
    wall->positive = wall->negative;
    wall->step_delay = 0;
    game->cells[wall->origin_y][wall->origin_x] = PPCellGrowing;
    return true;
}

static bool pp_ball_touches_cell_type(const PPGame* game, const PPBall* ball, PPCell target) {
    // Sample the ball's center and extremes so its radius can hit a growing wall.
    const int32_t sample_x[3] = {
        ball->x - PP_BALL_RADIUS,
        ball->x,
        ball->x + PP_BALL_RADIUS,
    };
    const int32_t sample_y[3] = {
        ball->y - PP_BALL_RADIUS,
        ball->y,
        ball->y + PP_BALL_RADIUS,
    };

    for(uint8_t yi = 0; yi < 3; yi++) {
        int32_t y = sample_y[yi] >> PP_FP_SHIFT;
        if((y < 0) || (y >= PP_GRID_HEIGHT)) continue;
        for(uint8_t xi = 0; xi < 3; xi++) {
            int32_t x = sample_x[xi] >> PP_FP_SHIFT;
            if((x >= 0) && (x < PP_GRID_WIDTH) && (game->cells[y][x] == target)) {
                return true;
            }
        }
    }
    return false;
}

static bool pp_position_hits_solid(const PPGame* game, int32_t x, int32_t y) {
    // Four corner samples are enough for the ball's axis-aligned collision bounds.
    const int32_t left = (x - PP_BALL_RADIUS) >> PP_FP_SHIFT;
    const int32_t right = (x + PP_BALL_RADIUS) >> PP_FP_SHIFT;
    const int32_t top = (y - PP_BALL_RADIUS) >> PP_FP_SHIFT;
    const int32_t bottom = (y + PP_BALL_RADIUS) >> PP_FP_SHIFT;

    if((left < 0) || (right >= PP_GRID_WIDTH) || (top < 0) || (bottom >= PP_GRID_HEIGHT)) {
        return true;
    }

    return pp_cell_is_solid(game->cells[top][left]) || pp_cell_is_solid(game->cells[top][right]) ||
           pp_cell_is_solid(game->cells[bottom][left]) ||
           pp_cell_is_solid(game->cells[bottom][right]);
}

static void pp_move_ball(const PPGame* game, PPBall* ball) {
    int32_t next_x = ball->x + ball->vx;
    if(pp_position_hits_solid(game, next_x, ball->y)) {
        ball->vx = -ball->vx;
        next_x = ball->x + ball->vx;
        if(pp_position_hits_solid(game, next_x, ball->y)) next_x = ball->x;
    }
    ball->x = next_x;

    int32_t next_y = ball->y + ball->vy;
    if(pp_position_hits_solid(game, ball->x, next_y)) {
        ball->vy = -ball->vy;
        next_y = ball->y + ball->vy;
        if(pp_position_hits_solid(game, ball->x, next_y)) next_y = ball->y;
    }
    ball->y = next_y;
}

static bool pp_any_ball_hits_growing(const PPGame* game) {
    for(uint8_t index = 0; index < game->ball_count; index++) {
        if(pp_ball_touches_cell_type(game, &game->balls[index], PPCellGrowing)) {
            return true;
        }
    }
    return false;
}

static void pp_lose_life(PPGame* game) {
    pp_clear_growing_cells(game);
    if(game->lives > 0) game->lives--;
    if(game->lives == 0) game->screen = PPScreenGameOver;
}

static bool pp_try_extend_wall(PPGame* game, bool positive) {
    PPGrowingWall* wall = &game->growing_wall;
    int16_t next = (positive ? wall->positive + 1 : wall->negative - 1);
    uint8_t x = wall->horizontal ? (uint8_t)next : wall->origin_x;
    uint8_t y = wall->horizontal ? wall->origin_y : (uint8_t)next;

    if(game->cells[y][x] != PPCellEmpty) return false;

    game->cells[y][x] = PPCellGrowing;
    if(positive) {
        wall->positive = next;
    } else {
        wall->negative = next;
    }
    return true;
}

static uint16_t pp_captured_cell_count(const PPGame* game) {
    uint16_t count = 0;
    for(uint8_t y = 1; y < PP_GRID_HEIGHT - 1; y++) {
        for(uint8_t x = 1; x < PP_GRID_WIDTH - 1; x++) {
            if(game->cells[y][x] != PPCellEmpty && game->cells[y][x] != PPCellGrowing) {
                count++;
            }
        }
    }
    return count;
}

static void pp_capture_regions_without_balls(PPGame* game) {
    // Flood outward from every ball; any empty cell not reached is safe to capture.
    memset(game->reachable, 0, sizeof(game->reachable));
    uint16_t head = 0;
    uint16_t tail = 0;

    for(uint8_t index = 0; index < game->ball_count; index++) {
        uint8_t x = (uint8_t)(game->balls[index].x >> PP_FP_SHIFT);
        uint8_t y = (uint8_t)(game->balls[index].y >> PP_FP_SHIFT);
        if((game->cells[y][x] == PPCellEmpty) && !game->reachable[y][x]) {
            game->reachable[y][x] = true;
            game->flood_queue[tail++] = ((uint16_t)y * PP_GRID_WIDTH) + x;
        }
    }

    while(head < tail) {
        uint16_t packed = game->flood_queue[head++];
        uint8_t x = packed % PP_GRID_WIDTH;
        uint8_t y = packed / PP_GRID_WIDTH;
        const int8_t dx[4] = {-1, 1, 0, 0};
        const int8_t dy[4] = {0, 0, -1, 1};

        for(uint8_t direction = 0; direction < 4; direction++) {
            uint8_t next_x = (uint8_t)((int16_t)x + dx[direction]);
            uint8_t next_y = (uint8_t)((int16_t)y + dy[direction]);
            if((game->cells[next_y][next_x] == PPCellEmpty) && !game->reachable[next_y][next_x]) {
                game->reachable[next_y][next_x] = true;
                game->flood_queue[tail++] = ((uint16_t)next_y * PP_GRID_WIDTH) + next_x;
            }
        }
    }

    for(uint8_t y = 1; y < PP_GRID_HEIGHT - 1; y++) {
        for(uint8_t x = 1; x < PP_GRID_WIDTH - 1; x++) {
            if((game->cells[y][x] == PPCellEmpty) && !game->reachable[y][x]) {
                game->cells[y][x] = PPCellFilled;
            }
        }
    }
}

static void pp_finish_wall(PPGame* game) {
    // Commit the divider before filling enclosed regions so it blocks the flood.
    const uint16_t captured_before = pp_captured_cell_count(game);

    for(uint8_t y = 1; y < PP_GRID_HEIGHT - 1; y++) {
        for(uint8_t x = 1; x < PP_GRID_WIDTH - 1; x++) {
            if(game->cells[y][x] == PPCellGrowing) game->cells[y][x] = PPCellWall;
        }
    }
    memset(&game->growing_wall, 0, sizeof(game->growing_wall));
    pp_capture_regions_without_balls(game);

    const uint16_t captured_after = pp_captured_cell_count(game);
    game->score += ((uint32_t)(captured_after - captured_before) * 5U) + 50U;

    if(pp_game_captured_percent(game) >= PP_TARGET_PERCENT) {
        game->score += pp_game_seconds_remaining(game) * 10U;
        game->score += (uint32_t)game->lives * 100U;
        game->screen = PPScreenLevelClear;
    }
}

static void pp_update_growing_wall(PPGame* game) {
    PPGrowingWall* wall = &game->growing_wall;
    if(!wall->active) return;
    if(++wall->step_delay < PP_WALL_STEP_TICKS) return;
    wall->step_delay = 0;

    if(!wall->negative_done && !pp_try_extend_wall(game, false)) {
        wall->negative_done = true;
    }
    if(!wall->positive_done && !pp_try_extend_wall(game, true)) {
        wall->positive_done = true;
    }
    if(wall->negative_done && wall->positive_done) pp_finish_wall(game);
}

void pp_game_tick(PPGame* game) {
    if(game->screen != PPScreenPlaying) return;
    if(game->invalid_action_ticks > 0) game->invalid_action_ticks--;

    if(game->time_remaining_ticks > 0) game->time_remaining_ticks--;
    if(game->time_remaining_ticks == 0) {
        pp_clear_growing_cells(game);
        game->screen = PPScreenGameOver;
        return;
    }

    pp_update_growing_wall(game);
    if(pp_any_ball_hits_growing(game)) {
        pp_lose_life(game);
        return;
    }

    for(uint8_t index = 0; index < game->ball_count; index++) {
        pp_move_ball(game, &game->balls[index]);
    }
    if(pp_any_ball_hits_growing(game)) pp_lose_life(game);
}

uint8_t pp_game_captured_percent(const PPGame* game) {
    const uint16_t interior_cells = (PP_GRID_WIDTH - 2) * (PP_GRID_HEIGHT - 2);
    return (uint8_t)(((uint32_t)pp_captured_cell_count(game) * 100U) / interior_cells);
}

uint32_t pp_game_seconds_remaining(const PPGame* game) {
    return (game->time_remaining_ticks + PP_TICKS_PER_SECOND - 1U) / PP_TICKS_PER_SECOND;
}
