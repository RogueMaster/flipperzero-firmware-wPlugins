#ifndef MINE_SWEEPER_ENGINE_H
#define MINE_SWEEPER_ENGINE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef enum {
    MineSweeperPhasePlaying = 0,
    MineSweeperPhaseWon,
    MineSweeperPhaseLost,
} MineSweeperPhase;

typedef enum {
    MineSweeperActionMove = 0,
    MineSweeperActionReveal,
    MineSweeperActionFlag,
    MineSweeperActionChord,
    MineSweeperActionNewGame,
} MineSweeperActionType;

typedef enum {
    MineSweeperMoveOutcomeNone = 0,
    MineSweeperMoveOutcomeMoved,
    MineSweeperMoveOutcomeWrapped,
    MineSweeperMoveOutcomeBlocked,
} MineSweeperMoveOutcome;

typedef enum {
    MineSweeperResultNoop,
    MineSweeperResultChanged,
    MineSweeperResultWin,
    MineSweeperResultLose,
    MineSweeperResultInvalid
} MineSweeperResult;

typedef struct {
    MineSweeperResult result;
    MineSweeperMoveOutcome move_outcome;
} MineSweeperActionResult;

/* ---- Cell Layout ----
 * bit 0 : mine
 * bit 1 : revealed
 * bit 2 : flagged
 * bits 3–6 : neighbor count (0–8)
 * bit 7 : reserved
 */
typedef uint8_t MineSweeperCell;

/* Bit masks */
#define CELL_MINE_MASK     (0x01u)
#define CELL_REVEALED_MASK (0x02u)
#define CELL_FLAG_MASK     (0x04u)

#define CELL_NEIGHBOR_SHIFT (3u)
#define CELL_NEIGHBOR_MASK  (0x0Fu << CELL_NEIGHBOR_SHIFT)

/* Board limits */
#define BOARD_MAX_WIDTH  (32u)
#define BOARD_MAX_HEIGHT (32u)
#define BOARD_MAX_TILES  (BOARD_MAX_WIDTH * BOARD_MAX_HEIGHT)

/* ---- Queries ---- */
#define CELL_IS_MINE(c)       (((c) & CELL_MINE_MASK) != 0u)
#define CELL_IS_REVEALED(c)   (((c) & CELL_REVEALED_MASK) != 0u)
#define CELL_IS_FLAGGED(c)    (((c) & CELL_FLAG_MASK) != 0u)
#define CELL_GET_NEIGHBORS(c) ((uint8_t)(((c) & CELL_NEIGHBOR_MASK) >> CELL_NEIGHBOR_SHIFT))

/* ---- Mutators ---- */
#define CELL_SET_MINE(c)   ((c) |= CELL_MINE_MASK)
#define CELL_CLEAR_MINE(c) ((c) &= (uint8_t)~CELL_MINE_MASK)

#define CELL_SET_REVEALED(c)   ((c) |= CELL_REVEALED_MASK)
#define CELL_CLEAR_REVEALED(c) ((c) &= (uint8_t)~CELL_REVEALED_MASK)

#define CELL_SET_FLAGGED(c)   ((c) |= CELL_FLAG_MASK)
#define CELL_CLEAR_FLAGGED(c) ((c) &= (uint8_t)~CELL_FLAG_MASK)

#define CELL_SET_NEIGHBORS(c, n)                                \
    do {                                                        \
        (c) &= (uint8_t)~CELL_NEIGHBOR_MASK;                    \
        (c) |= (uint8_t)(((n & 0x0Fu) << CELL_NEIGHBOR_SHIFT)); \
    } while(0)

/* ---- Board ---- */
typedef struct {
    uint8_t width;
    uint8_t height;
    uint16_t mine_count;
    MineSweeperCell cells[BOARD_MAX_TILES];
} MineSweeperBoard;

typedef struct {
    uint8_t width;
    uint8_t height;
    uint8_t difficulty;
    bool ensure_solvable;
    bool wrap_enabled;
} MineSweeperConfig;

typedef struct {
    uint8_t cursor_row;
    uint8_t cursor_col;
    uint16_t mines_left;
    uint16_t flags_left;
    uint16_t tiles_left;
    uint32_t start_tick;
    MineSweeperPhase phase;
} MineSweeperRuntime;

typedef struct {
    MineSweeperBoard board;
    MineSweeperConfig config;
    MineSweeperRuntime rt;
} MineSweeperState;

typedef enum {
    MineSweeperGenerationStatusIdle = 0,
    MineSweeperGenerationStatusInProgress,
    MineSweeperGenerationStatusReady,
    MineSweeperGenerationStatusCancelled,
    MineSweeperGenerationStatusFailed,
} MineSweeperGenerationStatus;

typedef struct {
    MineSweeperConfig config;
    MineSweeperState latest_candidate;
    uint32_t attempts_total;
    uint32_t start_tick;
    bool has_latest_candidate;
    bool latest_candidate_is_solved;
    MineSweeperGenerationStatus status;
} MineSweeperGenerationJob;

typedef struct {
    MineSweeperActionType type;
    int8_t dx;
    int8_t dy;
} MineSweeperAction;

extern const int8_t neighbor_offsets[8][2];

/* ---- BOARD API ---- */
uint16_t board_index(const MineSweeperBoard* board, uint8_t x, uint8_t y);

uint8_t board_x(const MineSweeperBoard* board, uint16_t i);

uint8_t board_y(const MineSweeperBoard* board, uint16_t i);

bool board_in_bounds(const MineSweeperBoard* board, int8_t x, int8_t y);

void board_init(MineSweeperBoard* board, uint8_t width, uint8_t height);

void board_compute_neighbor_counts(MineSweeperBoard* board);

bool board_reveal_cell(MineSweeperBoard* board, uint8_t x, uint8_t y);

uint16_t board_reveal_flood(MineSweeperBoard* board, uint8_t x, uint8_t y);

void board_toggle_flag(MineSweeperBoard* board, uint8_t x, uint8_t y);

/* ---- ENGINE API ---- */

MineSweeperResult minesweeper_engine_generation_begin(
    MineSweeperGenerationJob* job,
    const MineSweeperConfig* config);

MineSweeperGenerationStatus
    minesweeper_engine_generation_step(MineSweeperGenerationJob* job, uint16_t attempt_budget);

MineSweeperGenerationStatus
    minesweeper_engine_generation_status(const MineSweeperGenerationJob* job);

MineSweeperResult minesweeper_engine_generation_finish(
    MineSweeperGenerationJob* job,
    MineSweeperState* out_state,
    bool allow_unsolved_fallback);

void minesweeper_engine_generation_cancel(MineSweeperGenerationJob* job);

void minesweeper_engine_new_game(MineSweeperState* game_state);

MineSweeperResult minesweeper_engine_reveal(MineSweeperState* game_state, uint16_t x, uint16_t y);

MineSweeperResult minesweeper_engine_chord(MineSweeperState* game_state, uint16_t x, uint16_t y);

MineSweeperResult
    minesweeper_engine_toggle_flag(MineSweeperState* game_state, uint16_t x, uint16_t y);

MineSweeperResult
    minesweeper_engine_move_cursor(MineSweeperState* game_state, int8_t dx, int8_t dy);

MineSweeperResult minesweeper_engine_move_to_closest_tile(MineSweeperState* game_state);

MineSweeperResult minesweeper_engine_reveal_all_mines(MineSweeperState* game_state);

MineSweeperResult minesweeper_engine_check_win_conditions(MineSweeperState* game_state);

MineSweeperActionResult
    minesweeper_engine_apply_action(MineSweeperState* game_state, MineSweeperAction action);

MineSweeperResult
    minesweeper_engine_set_config(MineSweeperState* game_state, const MineSweeperConfig* config);

MineSweeperResult
    minesweeper_engine_set_runtime(MineSweeperState* game_state, const MineSweeperRuntime* runtime);

MineSweeperResult minesweeper_engine_validate_state(const MineSweeperState* game_state);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // MINE_SWEEPER_ENGINE_H
