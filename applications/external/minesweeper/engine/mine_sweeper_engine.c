#include "mine_sweeper_engine.h"
#include "mine_sweeper_solver.h"
#include "mstarlib_helpers.h"

#include <furi.h>
#include <furi_hal.h>
#include <stdint.h>
#include <string.h>

static const float difficulty_multiplier[3] = {
    0.15f,
    0.17f,
    0.19f,
};

static void board_clear(MineSweeperBoard* board);
static MineSweeperResult minesweeper_engine_reveal_all_tiles(MineSweeperState* game_state);

// Bias Free Uniform Random Sample In Range [0, upper_exclusion]
static uint16_t random_uniform_u16(uint16_t upper_exclusion) {
    if(upper_exclusion <= 1u) return 0u;

    const uint32_t range = (uint32_t)upper_exclusion;

    // Compute largest multiple of range that fits in uint32_t
    // Values >= limit are rejected to avoid modulo bias
    const uint32_t limit = UINT32_MAX - (UINT32_MAX % range);

    uint32_t r = 0;
    do {
        r = furi_hal_random_get();
    } while(r >= limit);

    return (uint16_t)(r % range);
}

// Fisher-Yates Shuffle
static void board_shuffle(MineSweeperBoard* board) {
    furi_assert(board);

    uint16_t total = (uint16_t)board->width * board->height;
    if(total <= 1u) return;

    for(uint16_t i = total - 1u; i > 0u; --i) {
        // Generate j in [0, i]
        uint16_t j = random_uniform_u16(i + 1u);

        MineSweeperCell tmp = board->cells[i];
        board->cells[i] = board->cells[j];
        board->cells[j] = tmp;
    }
}

static void board_ensure_safe_start(MineSweeperBoard* board, uint8_t safe_x, uint8_t safe_y) {
    furi_assert(board);

    uint16_t total = (uint16_t)board->width * board->height;
    if(total == 0) return;

    uint16_t safe_i = board_index(board, safe_x, safe_y);
    if(!CELL_IS_MINE(board->cells[safe_i])) return;

    for(uint16_t i = 0; i < total; ++i) {
        if(i == safe_i) continue;
        if(CELL_IS_MINE(board->cells[i])) continue;

        MineSweeperCell tmp = board->cells[safe_i];
        board->cells[safe_i] = board->cells[i];
        board->cells[i] = tmp;
        return;
    }
}

static void board_clear_solver_marks(MineSweeperBoard* board) {
    furi_assert(board);

    uint16_t total = (uint16_t)board->width * board->height;
    for(uint16_t i = 0; i < total; ++i) {
        board->cells[i] &= (uint8_t) ~(CELL_REVEALED_MASK | CELL_FLAG_MASK);
    }
}

static void board_generate_candidate(MineSweeperBoard* board, uint16_t mine_count) {
    furi_assert(board);

    board_clear(board);

    for(uint16_t i = 0; i < mine_count; ++i) {
        CELL_SET_MINE(board->cells[i]);
    }

    board->mine_count = mine_count;

    board_shuffle(board);
    board_ensure_safe_start(board, 0, 0);
    board_compute_neighbor_counts(board);
}

static void board_clear(MineSweeperBoard* board) {
    furi_assert(board);

    uint16_t total = board->height * board->width;

    for(uint16_t i = 0; i < total; ++i) {
        board->cells[i] = 0;
    }
}

static bool config_is_valid(const MineSweeperConfig* config) {
    if(!config) return false;

    return config->width > 0 && config->height > 0 && config->width <= BOARD_MAX_WIDTH &&
           config->height <= BOARD_MAX_HEIGHT && config->difficulty <= 2;
}

static bool
    runtime_is_valid_for_board(const MineSweeperBoard* board, const MineSweeperRuntime* runtime) {
    if(!board || !runtime) return false;
    if(board->width == 0 || board->height == 0) return false;

    if(runtime->cursor_col >= board->width || runtime->cursor_row >= board->height) {
        return false;
    }

    uint16_t total = (uint16_t)board->width * board->height;
    if(board->mine_count > total) return false;
    uint16_t safe_total = total - board->mine_count;

    if(runtime->tiles_left > safe_total) return false;
    if(runtime->flags_left > board->mine_count) return false;
    if(runtime->mines_left > board->mine_count) return false;

    if(runtime->phase != MineSweeperPhasePlaying && runtime->phase != MineSweeperPhaseWon &&
       runtime->phase != MineSweeperPhaseLost) {
        return false;
    }

    return true;
}

static MineSweeperResult minesweeper_engine_reveal_all_tiles(MineSweeperState* game_state) {
    furi_assert(game_state);

    MineSweeperBoard* board = &game_state->board;
    uint16_t total = (uint16_t)board->width * board->height;
    bool changed = false;

    for(uint16_t i = 0; i < total; ++i) {
        if(!CELL_IS_REVEALED(board->cells[i])) {
            CELL_SET_REVEALED(board->cells[i]);
            changed = true;
        }
    }

    // tiles_left tracks unrevealed safe tiles. If every tile is now revealed, this must be zero.
    game_state->rt.tiles_left = 0;
    return changed ? MineSweeperResultChanged : MineSweeperResultNoop;
}

const int8_t neighbor_offsets[8][2] =
    {{-1, 1}, {0, 1}, {1, 1}, {1, 0}, {1, -1}, {0, -1}, {-1, -1}, {-1, 0}};

uint16_t board_index(const MineSweeperBoard* board, const uint8_t x, const uint8_t y) {
    furi_assert(board);
    return (uint16_t)y * board->width + x;
}

uint8_t board_x(const MineSweeperBoard* board, uint16_t i) {
    furi_assert(board);
    return i % board->width;
}

uint8_t board_y(const MineSweeperBoard* board, uint16_t i) {
    furi_assert(board);
    return i / board->width;
}

bool board_in_bounds(const MineSweeperBoard* board, int8_t x, int8_t y) {
    furi_assert(board);
    return (x >= 0) && (y >= 0) && (x < board->width) && (y < board->height);
}

void board_init(MineSweeperBoard* board, uint8_t width, uint8_t height) {
    furi_assert(board);
    board->width = width;
    board->height = height;
    board->mine_count = 0;

    board_clear(board);
}

void board_compute_neighbor_counts(MineSweeperBoard* board) {
    furi_assert(board);
    for(uint8_t y = 0; y < board->height; ++y) {
        for(uint8_t x = 0; x < board->width; ++x) {
            uint16_t i = board_index(board, x, y);

            if(CELL_IS_MINE(board->cells[i])) continue;

            uint8_t count = 0;

            for(uint8_t n = 0; n < 8; ++n) {
                int8_t nx = x + neighbor_offsets[n][0];
                int8_t ny = y + neighbor_offsets[n][1];

                if(board_in_bounds(board, nx, ny)) {
                    uint16_t ni = board_index(board, nx, ny);
                    if(CELL_IS_MINE(board->cells[ni])) count++;
                }
            }

            CELL_SET_NEIGHBORS(board->cells[i], count);
        }
    }
}

bool board_reveal_cell(MineSweeperBoard* board, uint8_t x, uint8_t y) {
    furi_assert(board);
    uint16_t i = board_index(board, x, y);

    if(CELL_IS_REVEALED(board->cells[i]) || CELL_IS_FLAGGED(board->cells[i])) return false;

    CELL_SET_REVEALED(board->cells[i]);
    return true;
}

uint16_t board_reveal_flood(MineSweeperBoard* board, uint8_t x, uint8_t y) {
    furi_assert(board);

    uint16_t cleared_tiles = 0;

    point_deq_t deq;
    uint8_t visited[(BOARD_MAX_TILES + 7u) / 8u];

    point_deq_init(deq);
    point_visited_clear(visited, (uint16_t)board->width * board->height);

    Point_t pos;
    pointobj_init(pos);

    pointobj_set_point(pos, (Point){.x = x, .y = y});

    point_deq_push_back(deq, pos);

    while(point_deq_size(deq) > 0) {
        point_deq_pop_front(&pos, deq);
        Point curr_pos = pointobj_get_point(pos);
        uint16_t curr_pos_1d = board_index(board, curr_pos.x, curr_pos.y);

        if(point_visited_test(visited, curr_pos_1d) ||
           CELL_IS_REVEALED(board->cells[curr_pos_1d]) ||
           CELL_IS_FLAGGED(board->cells[curr_pos_1d])) {
            continue;
        }

        board_reveal_cell(board, curr_pos.x, curr_pos.y);

        point_visited_set(visited, curr_pos_1d);

        cleared_tiles++;

        if(!CELL_GET_NEIGHBORS(board->cells[curr_pos_1d])) {
            for(uint8_t n = 0; n < 8; ++n) {
                int16_t dx = (int16_t)curr_pos.x + neighbor_offsets[n][0];
                int16_t dy = (int16_t)curr_pos.y + neighbor_offsets[n][1];

                if(board_in_bounds(board, (int8_t)dx, (int8_t)dy)) {
                    const uint16_t neighbor_pos_1d = board_index(board, (uint8_t)dx, (uint8_t)dy);
                    if(point_visited_test(visited, neighbor_pos_1d)) continue;
                    pointobj_set_point(pos, (Point){.x = (uint8_t)dx, .y = (uint8_t)dy});
                    point_deq_push_back(deq, pos);
                }
            }
        }
    }

    point_deq_clear(deq);

    return cleared_tiles;
}

void board_toggle_flag(MineSweeperBoard* board, uint8_t x, uint8_t y) {
    furi_assert(board);
    uint16_t i = board_index(board, x, y);

    if(CELL_IS_REVEALED(board->cells[i])) return;

    if(CELL_IS_FLAGGED(board->cells[i]))
        CELL_CLEAR_FLAGGED(board->cells[i]);
    else
        CELL_SET_FLAGGED(board->cells[i]);
}

static uint16_t minesweeper_engine_compute_mine_count(const MineSweeperConfig* config) {
    furi_assert(config);

    const uint16_t total_cells = (uint16_t)config->width * config->height;

    const uint8_t difficulty = config->difficulty >= 3 ? 2 : config->difficulty;
    return (uint16_t)(total_cells * difficulty_multiplier[difficulty]);
}

static void minesweeper_engine_prepare_runtime(MineSweeperState* game_state) {
    furi_assert(game_state);

    const uint16_t total_cells = (uint16_t)game_state->board.width * game_state->board.height;
    const uint16_t mine_count = game_state->board.mine_count;

    game_state->rt.tiles_left = total_cells - mine_count;
    game_state->rt.flags_left = mine_count;
    game_state->rt.mines_left = mine_count;
    game_state->rt.phase = MineSweeperPhasePlaying;
    game_state->rt.cursor_col = 0;
    game_state->rt.cursor_row = 0;
}

MineSweeperResult minesweeper_engine_generation_begin(
    MineSweeperGenerationJob* job,
    const MineSweeperConfig* config) {
    if(!job || !config_is_valid(config)) {
        return MineSweeperResultInvalid;
    }

    memset(job, 0, sizeof(*job));
    job->config = *config;
    job->status = MineSweeperGenerationStatusInProgress;
    job->start_tick = furi_get_tick();

    return MineSweeperResultChanged;
}

MineSweeperGenerationStatus
    minesweeper_engine_generation_step(MineSweeperGenerationJob* job, uint16_t attempt_budget) {
    if(!job) {
        return MineSweeperGenerationStatusFailed;
    }

    if(job->status != MineSweeperGenerationStatusInProgress || attempt_budget == 0) {
        return job->status;
    }

    const uint16_t mine_count = minesweeper_engine_compute_mine_count(&job->config);

    for(uint16_t i = 0; i < attempt_budget; ++i) {
        memset(&job->latest_candidate, 0, sizeof(job->latest_candidate));
        job->latest_candidate.config = job->config;
        job->latest_candidate_is_solved = false;

        board_init(&job->latest_candidate.board, job->config.width, job->config.height);
        board_generate_candidate(&job->latest_candidate.board, mine_count);
        minesweeper_engine_prepare_runtime(&job->latest_candidate);

        job->has_latest_candidate = true;
        job->attempts_total++;

        if(!job->config.ensure_solvable) {
            job->status = MineSweeperGenerationStatusReady;
            break;
        }

        const bool is_solvable = check_board_with_solver(&job->latest_candidate.board);
        board_clear_solver_marks(&job->latest_candidate.board);
        job->latest_candidate_is_solved = is_solvable;

        if(is_solvable) {
            job->status = MineSweeperGenerationStatusReady;
            break;
        }
    }

    return job->status;
}

MineSweeperGenerationStatus
    minesweeper_engine_generation_status(const MineSweeperGenerationJob* job) {
    if(!job) {
        return MineSweeperGenerationStatusFailed;
    }

    return job->status;
}

MineSweeperResult minesweeper_engine_generation_finish(
    MineSweeperGenerationJob* job,
    MineSweeperState* out_state,
    bool allow_unsolved_fallback) {
    if(!job || !out_state) {
        return MineSweeperResultInvalid;
    }

    if(job->status == MineSweeperGenerationStatusCancelled ||
       job->status == MineSweeperGenerationStatusFailed ||
       job->status == MineSweeperGenerationStatusIdle) {
        return MineSweeperResultInvalid;
    }

    if(!job->has_latest_candidate) {
        return MineSweeperResultInvalid;
    }

    const bool candidate_allowed = job->latest_candidate_is_solved ||
                                   !job->config.ensure_solvable || allow_unsolved_fallback;
    if(!candidate_allowed) {
        return MineSweeperResultInvalid;
    }

    *out_state = job->latest_candidate;
    minesweeper_engine_prepare_runtime(out_state);
    out_state->rt.start_tick = furi_get_tick();
    job->status = MineSweeperGenerationStatusReady;

    return MineSweeperResultChanged;
}

void minesweeper_engine_generation_cancel(MineSweeperGenerationJob* job) {
    if(!job) {
        return;
    }

    job->status = MineSweeperGenerationStatusCancelled;
}

void minesweeper_engine_new_game(MineSweeperState* game_state) {
    furi_assert(game_state);

    const uint16_t number_mines = minesweeper_engine_compute_mine_count(&game_state->config);
    bool is_solvable = false;

    do {
        board_generate_candidate(&game_state->board, number_mines);

        if(!game_state->config.ensure_solvable) {
            break;
        }

        is_solvable = check_board_with_solver(&game_state->board);
        board_clear_solver_marks(&game_state->board);
    } while(!is_solvable);

    minesweeper_engine_prepare_runtime(game_state);
    game_state->rt.start_tick = furi_get_tick();
}

MineSweeperResult minesweeper_engine_reveal(MineSweeperState* game_state, uint16_t x, uint16_t y) {
    furi_assert(game_state);

    MineSweeperBoard* board = &game_state->board;

    uint16_t cursor_pos_1d = board_index(board, x, y);
    MineSweeperCell tile = board->cells[cursor_pos_1d];

    // Revealing a flagged (or already revealed) tile is a no-op.
    if(CELL_IS_FLAGGED(tile) || CELL_IS_REVEALED(tile)) {
        return MineSweeperResultNoop;
    }

    if(CELL_IS_MINE(tile)) {
        minesweeper_engine_reveal_all_mines(game_state);
        return MineSweeperResultLose;
    }

    uint16_t revealed_delta = board_reveal_flood(board, x, y);
    if(revealed_delta == 0) {
        return MineSweeperResultNoop;
    }

    game_state->rt.tiles_left -= revealed_delta;

    MineSweeperResult result = minesweeper_engine_check_win_conditions(game_state);
    return result == MineSweeperResultWin ? MineSweeperResultWin : MineSweeperResultChanged;
}

MineSweeperResult minesweeper_engine_chord(MineSweeperState* game_state, uint16_t x, uint16_t y) {
    furi_assert(game_state);

    MineSweeperBoard* board = &game_state->board;

    uint16_t cursor_pos_1d = board_index(board, x, y);
    uint8_t tile_num = CELL_GET_NEIGHBORS(board->cells[cursor_pos_1d]);

    if(!CELL_IS_REVEALED(board->cells[cursor_pos_1d]) || tile_num == 0) {
        return MineSweeperResultNoop;
    }

    uint8_t flagged_neighbors = 0;

    for(uint8_t n = 0; n < 8; ++n) {
        int16_t dx = (int16_t)x + neighbor_offsets[n][0];
        int16_t dy = (int16_t)y + neighbor_offsets[n][1];

        if(board_in_bounds(board, (int8_t)dx, (int8_t)dy)) {
            uint16_t neighbor_pos_1d = board_index(board, (uint8_t)dx, (uint8_t)dy);
            if(CELL_IS_FLAGGED(board->cells[neighbor_pos_1d])) {
                flagged_neighbors++;
            }
        }
    }

    // chord only if flagged neighbor count == tile number
    if(flagged_neighbors == tile_num) {
        uint16_t revealed_delta_total = 0;

        for(uint8_t n = 0; n < 8; ++n) {
            int16_t dx = (int16_t)x + neighbor_offsets[n][0];
            int16_t dy = (int16_t)y + neighbor_offsets[n][1];

            if(board_in_bounds(board, (int8_t)dx, (int8_t)dy)) {
                uint16_t neighbor_pos_1d = board_index(board, (uint8_t)dx, (uint8_t)dy);
                if(!CELL_IS_REVEALED(board->cells[neighbor_pos_1d]) &&
                   !CELL_IS_FLAGGED(board->cells[neighbor_pos_1d])) {
                    if(CELL_IS_MINE(board->cells[neighbor_pos_1d])) {
                        minesweeper_engine_reveal_all_mines(game_state);
                        return MineSweeperResultLose;
                    }
                    uint16_t revealed_delta = board_reveal_flood(board, (uint8_t)dx, (uint8_t)dy);
                    game_state->rt.tiles_left -= revealed_delta;
                    revealed_delta_total += revealed_delta;
                }
            }
        }

        if(revealed_delta_total == 0) {
            return MineSweeperResultNoop;
        }

        MineSweeperResult result = minesweeper_engine_check_win_conditions(game_state);
        return result == MineSweeperResultWin ? MineSweeperResultWin : MineSweeperResultChanged;
    }

    return MineSweeperResultNoop;
}

MineSweeperResult minesweeper_engine_check_win_conditions(MineSweeperState* game_state) {
    furi_assert(game_state);

    if(game_state->rt.tiles_left == 0 && game_state->rt.flags_left == game_state->rt.mines_left) {
        game_state->rt.phase = MineSweeperPhaseWon;
        minesweeper_engine_reveal_all_tiles(game_state);
        return MineSweeperResultWin;
    }

    return MineSweeperResultChanged;
}

MineSweeperResult
    minesweeper_engine_toggle_flag(MineSweeperState* game_state, uint16_t x, uint16_t y) {
    furi_assert(game_state);

    if(game_state->rt.phase != MineSweeperPhasePlaying) {
        return MineSweeperResultNoop;
    }

    MineSweeperBoard* board = &game_state->board;
    uint16_t tile_pos_1d = board_index(board, x, y);
    MineSweeperCell tile = board->cells[tile_pos_1d];

    if(CELL_IS_REVEALED(tile)) {
        return MineSweeperResultNoop;
    }

    bool was_flagged = CELL_IS_FLAGGED(tile);
    bool is_mine = CELL_IS_MINE(tile);

    if(!was_flagged && game_state->rt.flags_left == 0) {
        return MineSweeperResultNoop;
    }

    board_toggle_flag(board, x, y);

    if(was_flagged) {
        game_state->rt.flags_left++;
        if(is_mine) {
            game_state->rt.mines_left++;
        }
    } else {
        game_state->rt.flags_left--;
        if(is_mine) {
            game_state->rt.mines_left--;
        }
    }

    return minesweeper_engine_check_win_conditions(game_state);
}

MineSweeperResult
    minesweeper_engine_move_cursor(MineSweeperState* game_state, int8_t dx, int8_t dy) {
    furi_assert(game_state);

    MineSweeperBoard* board = &game_state->board;
    if(board->width == 0 || board->height == 0) {
        return MineSweeperResultInvalid;
    }

    int16_t next_col = (int16_t)game_state->rt.cursor_col + dx;
    int16_t next_row = (int16_t)game_state->rt.cursor_row + dy;

    if(game_state->config.wrap_enabled) {
        next_col = (next_col % board->width + board->width) % board->width;
        next_row = (next_row % board->height + board->height) % board->height;
    } else {
        if(next_col < 0) {
            next_col = 0;
        } else if(next_col >= board->width) {
            next_col = board->width - 1;
        }

        if(next_row < 0) {
            next_row = 0;
        } else if(next_row >= board->height) {
            next_row = board->height - 1;
        }
    }

    if(game_state->rt.cursor_col == (uint8_t)next_col &&
       game_state->rt.cursor_row == (uint8_t)next_row) {
        return MineSweeperResultNoop;
    }

    game_state->rt.cursor_col = (uint8_t)next_col;
    game_state->rt.cursor_row = (uint8_t)next_row;
    return MineSweeperResultChanged;
}

MineSweeperResult minesweeper_engine_move_to_closest_tile(MineSweeperState* game_state) {
    furi_assert(game_state);

    MineSweeperBoard* board = &game_state->board;
    if(board->width == 0 || board->height == 0) {
        return MineSweeperResultInvalid;
    }

    uint16_t curr_pos_1d =
        board_index(&game_state->board, game_state->rt.cursor_col, game_state->rt.cursor_row);

    if(!CELL_IS_REVEALED(game_state->board.cells[curr_pos_1d])) {
        return MineSweeperResultNoop;
    }

    Point result = (Point){.x = 0, .y = 0};

    point_deq_t deq2;
    point_deq_init(deq2);

    // Init dequeue and visited bitset used for BFS traversal
    point_deq_t deq;
    uint8_t visited[(BOARD_MAX_TILES + 7u) / 8u];

    point_deq_init(deq);
    point_visited_clear(visited, (uint16_t)board->width * board->height);

    // Point_t pos will be used to keep track of the current point
    Point_t pos;
    pointobj_init(pos);

    // Starting position is current position of the model
    Point start_pos = (Point){.x = game_state->rt.cursor_col, .y = game_state->rt.cursor_row};
    pointobj_set_point(pos, start_pos);

    point_deq_push_back(deq, pos);

    bool is_target_tile_found = false;

    while(point_deq_size(deq) > 0) {
        point_deq_pop_front(&pos, deq);
        Point curr_pos = pointobj_get_point(pos);
        uint16_t curr_pos_1d = board_index(&game_state->board, curr_pos.x, curr_pos.y);

        if(point_visited_test(visited, curr_pos_1d)) {
            continue;
        }

        point_visited_set(visited, curr_pos_1d);

        // Do not continue if we have found candidate tiles and this tile is revealed.
        if(is_target_tile_found && CELL_IS_REVEALED(game_state->board.cells[curr_pos_1d])) {
            continue;
        }

        if(!CELL_IS_REVEALED(game_state->board.cells[curr_pos_1d]) &&
           !CELL_IS_FLAGGED(game_state->board.cells[curr_pos_1d])) {
            is_target_tile_found = true;
            pointobj_set_point(pos, curr_pos);
            point_deq_push_back(deq2, pos);
            continue;
        }

        for(uint8_t n = 0; n < 8; ++n) {
            int16_t dx = curr_pos.x + neighbor_offsets[n][0];
            int16_t dy = curr_pos.y + neighbor_offsets[n][1];

            if(!board_in_bounds(&game_state->board, dx, dy)) continue;

            Point neighbor = (Point){.x = dx, .y = dy};
            pointobj_set_point(pos, neighbor);
            point_deq_push_back(deq, pos);
        }
    }

    point_deq_clear(deq);

    if(!is_target_tile_found) {
        point_deq_clear(deq2);
        return MineSweeperResultNoop;
    }

    double min_distance = INT_MAX;

    while(point_deq_size(deq2) > 0) {
        point_deq_pop_front(&pos, deq2);
        Point curr_pos = pointobj_get_point(pos);
        int x_abs = abs(curr_pos.x - start_pos.x);
        int y_abs = abs(curr_pos.y - start_pos.y);
        double distance = sqrt(x_abs * x_abs + y_abs * y_abs);

        if(distance < min_distance) {
            result = curr_pos;
            min_distance = distance;
        } else if(distance == min_distance && (furi_hal_random_get() % 2) == 0) {
            result = curr_pos;
            min_distance = distance;
        }
    }

    point_deq_clear(deq2);

    game_state->rt.cursor_col = result.x;
    game_state->rt.cursor_row = result.y;

    return MineSweeperResultChanged;
}

MineSweeperResult minesweeper_engine_reveal_all_mines(MineSweeperState* game_state) {
    furi_assert(game_state);

    game_state->rt.phase = MineSweeperPhaseLost;
    return minesweeper_engine_reveal_all_tiles(game_state);
}

static MineSweeperMoveOutcome minesweeper_engine_classify_move_outcome(
    const MineSweeperState* game_state,
    uint8_t prev_col,
    uint8_t prev_row,
    int8_t dx,
    int8_t dy,
    MineSweeperResult move_result) {
    furi_assert(game_state);

    if(move_result != MineSweeperResultChanged && move_result != MineSweeperResultNoop) {
        return MineSweeperMoveOutcomeNone;
    }

    const MineSweeperBoard* board = &game_state->board;
    if(board->width == 0 || board->height == 0) {
        return MineSweeperMoveOutcomeNone;
    }

    int16_t raw_next_col = (int16_t)prev_col + dx;
    int16_t raw_next_row = (int16_t)prev_row + dy;
    bool attempted_oob = raw_next_col < 0 || raw_next_col >= board->width || raw_next_row < 0 ||
                         raw_next_row >= board->height;

    if(game_state->config.wrap_enabled) {
        if(move_result == MineSweeperResultChanged && attempted_oob) {
            return MineSweeperMoveOutcomeWrapped;
        } else if(move_result == MineSweeperResultChanged) {
            return MineSweeperMoveOutcomeMoved;
        }
        return MineSweeperMoveOutcomeNone;
    }

    if(move_result == MineSweeperResultNoop && attempted_oob) {
        return MineSweeperMoveOutcomeBlocked;
    } else if(move_result == MineSweeperResultChanged) {
        return MineSweeperMoveOutcomeMoved;
    }

    return MineSweeperMoveOutcomeNone;
}

MineSweeperActionResult
    minesweeper_engine_apply_action(MineSweeperState* game_state, MineSweeperAction action) {
    furi_assert(game_state);

    MineSweeperActionResult detailed = {
        .result = MineSweeperResultInvalid,
        .move_outcome = MineSweeperMoveOutcomeNone,
    };

    if(action.type != MineSweeperActionNewGame && action.type != MineSweeperActionMove &&
       game_state->rt.phase != MineSweeperPhasePlaying) {
        detailed.result = MineSweeperResultNoop;
        return detailed;
    }

    uint16_t curr_pos_1d =
        board_index(&game_state->board, game_state->rt.cursor_col, game_state->rt.cursor_row);
    MineSweeperCell curr_cell = game_state->board.cells[curr_pos_1d];

    switch(action.type) {
    case MineSweeperActionMove: {
        uint8_t prev_col = game_state->rt.cursor_col;
        uint8_t prev_row = game_state->rt.cursor_row;
        detailed.result = minesweeper_engine_move_cursor(game_state, action.dx, action.dy);
        detailed.move_outcome = minesweeper_engine_classify_move_outcome(
            game_state, prev_col, prev_row, action.dx, action.dy, detailed.result);
        return detailed;
    }

    case MineSweeperActionReveal:
        detailed.result = minesweeper_engine_reveal(
            game_state, game_state->rt.cursor_col, game_state->rt.cursor_row);
        return detailed;

    case MineSweeperActionFlag:
        // Flag action toggles flag on unrevealed tiles and
        // moves to closest tiles on revealed
        detailed.result =
            CELL_IS_REVEALED(curr_cell) ?
                minesweeper_engine_move_to_closest_tile(game_state) :
                minesweeper_engine_toggle_flag(
                    game_state, game_state->rt.cursor_col, game_state->rt.cursor_row);
        return detailed;

    case MineSweeperActionChord:
        detailed.result = minesweeper_engine_chord(
            game_state, game_state->rt.cursor_col, game_state->rt.cursor_row);
        return detailed;

    case MineSweeperActionNewGame:
        minesweeper_engine_new_game(game_state);
        detailed.result = MineSweeperResultChanged;
        return detailed;

    default:
        detailed.result = MineSweeperResultInvalid;
        return detailed;
    }
}

MineSweeperResult
    minesweeper_engine_set_config(MineSweeperState* game_state, const MineSweeperConfig* config) {
    furi_assert(game_state);

    if(!config_is_valid(config)) {
        return MineSweeperResultInvalid;
    }

    game_state->config = *config;
    game_state->board.width = config->width;
    game_state->board.height = config->height;

    return MineSweeperResultChanged;
}

MineSweeperResult minesweeper_engine_set_runtime(
    MineSweeperState* game_state,
    const MineSweeperRuntime* runtime) {
    furi_assert(game_state);

    if(!runtime_is_valid_for_board(&game_state->board, runtime)) {
        return MineSweeperResultInvalid;
    }

    game_state->rt = *runtime;
    return MineSweeperResultChanged;
}

MineSweeperResult minesweeper_engine_validate_state(const MineSweeperState* game_state) {
    furi_assert(game_state);

    const MineSweeperBoard* board = &game_state->board;
    const MineSweeperRuntime* runtime = &game_state->rt;
    const MineSweeperConfig* config = &game_state->config;

    if(!config_is_valid(config)) {
        return MineSweeperResultInvalid;
    }

    if(board->width != config->width || board->height != config->height) {
        return MineSweeperResultInvalid;
    }

    uint16_t total = (uint16_t)board->width * board->height;
    if(total == 0 || total > BOARD_MAX_TILES) {
        return MineSweeperResultInvalid;
    }

    if(board->mine_count > total) {
        return MineSweeperResultInvalid;
    }

    uint16_t mine_count_actual = 0;
    uint16_t revealed_safe_tiles = 0;

    for(uint16_t i = 0; i < total; ++i) {
        MineSweeperCell cell = board->cells[i];

        if(CELL_IS_MINE(cell)) {
            mine_count_actual++;
        } else if(CELL_IS_REVEALED(cell)) {
            revealed_safe_tiles++;
        }
    }

    if(mine_count_actual != board->mine_count) {
        return MineSweeperResultInvalid;
    }

    if(!runtime_is_valid_for_board(board, runtime)) {
        return MineSweeperResultInvalid;
    }

    uint16_t safe_total = total - board->mine_count;
    if((uint16_t)(runtime->tiles_left + revealed_safe_tiles) != safe_total) {
        return MineSweeperResultInvalid;
    }

    if(runtime->phase == MineSweeperPhaseWon &&
       !(runtime->tiles_left == 0 && runtime->flags_left == runtime->mines_left)) {
        return MineSweeperResultInvalid;
    }

    return MineSweeperResultNoop;
}
