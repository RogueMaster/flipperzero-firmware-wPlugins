#include "jezzball_game.h"

#include <string.h>

#define JZ_FP_SHIFT        8
#define JZ_FP_ONE          (1L << JZ_FP_SHIFT)
#define JZ_BALL_RADIUS     136
#define JZ_WALL_STEP_TICKS 2

static bool jz_cell_is_solid(JzCell cell) {
    return (cell == JzCellWall) || (cell == JzCellFilled);
}

static uint32_t jz_random(JzGame* game) {
    uint32_t value = game->random_state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    game->random_state = value ? value : 0x6D2B79F5UL;
    return game->random_state;
}

static uint8_t jz_level_ball_count(uint8_t level) {
    uint8_t count = level + 1;
    return count > JZ_MAX_BALLS ? JZ_MAX_BALLS : count;
}

static uint32_t jz_level_seconds(uint8_t level) {
    return 90U + ((uint32_t)(level - 1U) * 15U);
}

static void jz_clear_growing_cells(JzGame* game) {
    for(uint8_t y = 1; y < JZ_GRID_HEIGHT - 1; y++) {
        for(uint8_t x = 1; x < JZ_GRID_WIDTH - 1; x++) {
            if(game->cells[y][x] == JzCellGrowing) {
                game->cells[y][x] = JzCellEmpty;
            }
        }
    }
    memset(&game->growing_wall, 0, sizeof(game->growing_wall));
}

static void jz_spawn_balls(JzGame* game) {
    const int32_t speed = 38 + ((int32_t)game->level * 3);

    for(uint8_t index = 0; index < game->ball_count; index++) {
        uint8_t x = 0;
        uint8_t y = 0;
        bool separated = false;

        for(uint8_t attempt = 0; attempt < 64 && !separated; attempt++) {
            x = 4U + (jz_random(game) % (JZ_GRID_WIDTH - 8U));
            y = 3U + (jz_random(game) % (JZ_GRID_HEIGHT - 6U));
            separated = true;
            for(uint8_t other = 0; other < index; other++) {
                int32_t dx = (int32_t)x - (game->balls[other].x >> JZ_FP_SHIFT);
                int32_t dy = (int32_t)y - (game->balls[other].y >> JZ_FP_SHIFT);
                if((dx * dx + dy * dy) < 16) {
                    separated = false;
                    break;
                }
            }
        }

        game->balls[index].x = ((int32_t)x << JZ_FP_SHIFT) + (JZ_FP_ONE / 2);
        game->balls[index].y = ((int32_t)y << JZ_FP_SHIFT) + (JZ_FP_ONE / 2);
        game->balls[index].vx = (jz_random(game) & 1U) ? speed : -speed;
        game->balls[index].vy = (jz_random(game) & 1U) ? speed : -speed;
    }
}

static void jz_setup_level(JzGame* game) {
    memset(game->cells, 0, sizeof(game->cells));
    memset(&game->growing_wall, 0, sizeof(game->growing_wall));

    for(uint8_t x = 0; x < JZ_GRID_WIDTH; x++) {
        game->cells[0][x] = JzCellWall;
        game->cells[JZ_GRID_HEIGHT - 1][x] = JzCellWall;
    }
    for(uint8_t y = 0; y < JZ_GRID_HEIGHT; y++) {
        game->cells[y][0] = JzCellWall;
        game->cells[y][JZ_GRID_WIDTH - 1] = JzCellWall;
    }

    game->ball_count = jz_level_ball_count(game->level);
    game->cursor_x = JZ_GRID_WIDTH / 2;
    game->cursor_y = JZ_GRID_HEIGHT / 2;
    game->cursor_horizontal = false;
    game->time_remaining_ticks = jz_level_seconds(game->level) * JZ_TICKS_PER_SECOND;
    game->invalid_action_ticks = 0;
    jz_spawn_balls(game);
}

void jz_game_init(JzGame* game, uint32_t seed) {
    memset(game, 0, sizeof(*game));
    game->random_state = seed ? seed : 0xA341316CUL;
    game->level = 1;
    game->lives = 3;
    game->screen = JzScreenTitle;
    jz_setup_level(game);
    game->screen = JzScreenTitle;
}

void jz_game_start(JzGame* game) {
    game->level = 1;
    game->lives = 3;
    game->score = 0;
    jz_setup_level(game);
    game->screen = JzScreenPlaying;
}

void jz_game_advance_level(JzGame* game) {
    if(game->screen != JzScreenLevelClear) return;

    if(game->level >= JZ_FINAL_LEVEL) {
        game->screen = JzScreenVictory;
        return;
    }

    game->level++;
    jz_setup_level(game);
    game->screen = JzScreenPlaying;
}

void jz_game_toggle_pause(JzGame* game) {
    if(game->screen == JzScreenPlaying) {
        game->screen = JzScreenPaused;
    } else if(game->screen == JzScreenPaused) {
        game->screen = JzScreenPlaying;
    }
}

void jz_game_move_cursor(JzGame* game, int8_t dx, int8_t dy, bool horizontal) {
    if(game->screen != JzScreenPlaying) return;

    int16_t next_x = (int16_t)game->cursor_x + dx;
    int16_t next_y = (int16_t)game->cursor_y + dy;
    if(next_x < 1) next_x = 1;
    if(next_x > JZ_GRID_WIDTH - 2) next_x = JZ_GRID_WIDTH - 2;
    if(next_y < 1) next_y = 1;
    if(next_y > JZ_GRID_HEIGHT - 2) next_y = JZ_GRID_HEIGHT - 2;
    game->cursor_x = (uint8_t)next_x;
    game->cursor_y = (uint8_t)next_y;
    game->cursor_horizontal = horizontal;
}

bool jz_game_start_wall(JzGame* game) {
    if((game->screen != JzScreenPlaying) || game->growing_wall.active ||
       (game->cells[game->cursor_y][game->cursor_x] != JzCellEmpty)) {
        game->invalid_action_ticks = JZ_TICKS_PER_SECOND / 2;
        return false;
    }

    JzGrowingWall* wall = &game->growing_wall;
    wall->active = true;
    wall->horizontal = game->cursor_horizontal;
    wall->origin_x = game->cursor_x;
    wall->origin_y = game->cursor_y;
    wall->negative = wall->horizontal ? wall->origin_x : wall->origin_y;
    wall->positive = wall->negative;
    wall->step_delay = 0;
    game->cells[wall->origin_y][wall->origin_x] = JzCellGrowing;
    return true;
}

static bool jz_ball_touches_cell_type(const JzGame* game, const JzBall* ball, JzCell target) {
    const int32_t sample_x[3] = {
        ball->x - JZ_BALL_RADIUS,
        ball->x,
        ball->x + JZ_BALL_RADIUS,
    };
    const int32_t sample_y[3] = {
        ball->y - JZ_BALL_RADIUS,
        ball->y,
        ball->y + JZ_BALL_RADIUS,
    };

    for(uint8_t yi = 0; yi < 3; yi++) {
        int32_t y = sample_y[yi] >> JZ_FP_SHIFT;
        if((y < 0) || (y >= JZ_GRID_HEIGHT)) continue;
        for(uint8_t xi = 0; xi < 3; xi++) {
            int32_t x = sample_x[xi] >> JZ_FP_SHIFT;
            if((x >= 0) && (x < JZ_GRID_WIDTH) && (game->cells[y][x] == target)) {
                return true;
            }
        }
    }
    return false;
}

static bool jz_position_hits_solid(const JzGame* game, int32_t x, int32_t y) {
    const int32_t left = (x - JZ_BALL_RADIUS) >> JZ_FP_SHIFT;
    const int32_t right = (x + JZ_BALL_RADIUS) >> JZ_FP_SHIFT;
    const int32_t top = (y - JZ_BALL_RADIUS) >> JZ_FP_SHIFT;
    const int32_t bottom = (y + JZ_BALL_RADIUS) >> JZ_FP_SHIFT;

    if((left < 0) || (right >= JZ_GRID_WIDTH) || (top < 0) || (bottom >= JZ_GRID_HEIGHT)) {
        return true;
    }

    return jz_cell_is_solid(game->cells[top][left]) || jz_cell_is_solid(game->cells[top][right]) ||
           jz_cell_is_solid(game->cells[bottom][left]) ||
           jz_cell_is_solid(game->cells[bottom][right]);
}

static void jz_move_ball(const JzGame* game, JzBall* ball) {
    int32_t next_x = ball->x + ball->vx;
    if(jz_position_hits_solid(game, next_x, ball->y)) {
        ball->vx = -ball->vx;
        next_x = ball->x + ball->vx;
        if(jz_position_hits_solid(game, next_x, ball->y)) next_x = ball->x;
    }
    ball->x = next_x;

    int32_t next_y = ball->y + ball->vy;
    if(jz_position_hits_solid(game, ball->x, next_y)) {
        ball->vy = -ball->vy;
        next_y = ball->y + ball->vy;
        if(jz_position_hits_solid(game, ball->x, next_y)) next_y = ball->y;
    }
    ball->y = next_y;
}

static bool jz_any_ball_hits_growing(const JzGame* game) {
    for(uint8_t index = 0; index < game->ball_count; index++) {
        if(jz_ball_touches_cell_type(game, &game->balls[index], JzCellGrowing)) {
            return true;
        }
    }
    return false;
}

static void jz_lose_life(JzGame* game) {
    jz_clear_growing_cells(game);
    if(game->lives > 0) game->lives--;
    if(game->lives == 0) game->screen = JzScreenGameOver;
}

static bool jz_try_extend_wall(JzGame* game, bool positive) {
    JzGrowingWall* wall = &game->growing_wall;
    int16_t next = (positive ? wall->positive + 1 : wall->negative - 1);
    uint8_t x = wall->horizontal ? (uint8_t)next : wall->origin_x;
    uint8_t y = wall->horizontal ? wall->origin_y : (uint8_t)next;

    if(game->cells[y][x] != JzCellEmpty) return false;

    game->cells[y][x] = JzCellGrowing;
    if(positive) {
        wall->positive = next;
    } else {
        wall->negative = next;
    }
    return true;
}

static uint16_t jz_captured_cell_count(const JzGame* game) {
    uint16_t count = 0;
    for(uint8_t y = 1; y < JZ_GRID_HEIGHT - 1; y++) {
        for(uint8_t x = 1; x < JZ_GRID_WIDTH - 1; x++) {
            if(game->cells[y][x] != JzCellEmpty && game->cells[y][x] != JzCellGrowing) {
                count++;
            }
        }
    }
    return count;
}

static void jz_capture_regions_without_balls(JzGame* game) {
    memset(game->reachable, 0, sizeof(game->reachable));
    uint16_t head = 0;
    uint16_t tail = 0;

    for(uint8_t index = 0; index < game->ball_count; index++) {
        uint8_t x = (uint8_t)(game->balls[index].x >> JZ_FP_SHIFT);
        uint8_t y = (uint8_t)(game->balls[index].y >> JZ_FP_SHIFT);
        if((game->cells[y][x] == JzCellEmpty) && !game->reachable[y][x]) {
            game->reachable[y][x] = true;
            game->flood_queue[tail++] = ((uint16_t)y * JZ_GRID_WIDTH) + x;
        }
    }

    while(head < tail) {
        uint16_t packed = game->flood_queue[head++];
        uint8_t x = packed % JZ_GRID_WIDTH;
        uint8_t y = packed / JZ_GRID_WIDTH;
        const int8_t dx[4] = {-1, 1, 0, 0};
        const int8_t dy[4] = {0, 0, -1, 1};

        for(uint8_t direction = 0; direction < 4; direction++) {
            uint8_t next_x = (uint8_t)((int16_t)x + dx[direction]);
            uint8_t next_y = (uint8_t)((int16_t)y + dy[direction]);
            if((game->cells[next_y][next_x] == JzCellEmpty) && !game->reachable[next_y][next_x]) {
                game->reachable[next_y][next_x] = true;
                game->flood_queue[tail++] = ((uint16_t)next_y * JZ_GRID_WIDTH) + next_x;
            }
        }
    }

    for(uint8_t y = 1; y < JZ_GRID_HEIGHT - 1; y++) {
        for(uint8_t x = 1; x < JZ_GRID_WIDTH - 1; x++) {
            if((game->cells[y][x] == JzCellEmpty) && !game->reachable[y][x]) {
                game->cells[y][x] = JzCellFilled;
            }
        }
    }
}

static void jz_finish_wall(JzGame* game) {
    const uint16_t captured_before = jz_captured_cell_count(game);

    for(uint8_t y = 1; y < JZ_GRID_HEIGHT - 1; y++) {
        for(uint8_t x = 1; x < JZ_GRID_WIDTH - 1; x++) {
            if(game->cells[y][x] == JzCellGrowing) game->cells[y][x] = JzCellWall;
        }
    }
    memset(&game->growing_wall, 0, sizeof(game->growing_wall));
    jz_capture_regions_without_balls(game);

    const uint16_t captured_after = jz_captured_cell_count(game);
    game->score += ((uint32_t)(captured_after - captured_before) * 5U) + 50U;

    if(jz_game_captured_percent(game) >= JZ_TARGET_PERCENT) {
        game->score += jz_game_seconds_remaining(game) * 10U;
        game->score += (uint32_t)game->lives * 100U;
        game->screen = JzScreenLevelClear;
    }
}

static void jz_update_growing_wall(JzGame* game) {
    JzGrowingWall* wall = &game->growing_wall;
    if(!wall->active) return;
    if(++wall->step_delay < JZ_WALL_STEP_TICKS) return;
    wall->step_delay = 0;

    if(!wall->negative_done && !jz_try_extend_wall(game, false)) {
        wall->negative_done = true;
    }
    if(!wall->positive_done && !jz_try_extend_wall(game, true)) {
        wall->positive_done = true;
    }
    if(wall->negative_done && wall->positive_done) jz_finish_wall(game);
}

void jz_game_tick(JzGame* game) {
    if(game->screen != JzScreenPlaying) return;
    if(game->invalid_action_ticks > 0) game->invalid_action_ticks--;

    if(game->time_remaining_ticks > 0) game->time_remaining_ticks--;
    if(game->time_remaining_ticks == 0) {
        jz_clear_growing_cells(game);
        game->screen = JzScreenGameOver;
        return;
    }

    jz_update_growing_wall(game);
    if(jz_any_ball_hits_growing(game)) {
        jz_lose_life(game);
        return;
    }

    for(uint8_t index = 0; index < game->ball_count; index++) {
        jz_move_ball(game, &game->balls[index]);
    }
    if(jz_any_ball_hits_growing(game)) jz_lose_life(game);
}

uint8_t jz_game_captured_percent(const JzGame* game) {
    const uint16_t interior_cells = (JZ_GRID_WIDTH - 2) * (JZ_GRID_HEIGHT - 2);
    return (uint8_t)(((uint32_t)jz_captured_cell_count(game) * 100U) / interior_cells);
}

uint32_t jz_game_seconds_remaining(const JzGame* game) {
    return (game->time_remaining_ticks + JZ_TICKS_PER_SECOND - 1U) / JZ_TICKS_PER_SECOND;
}
