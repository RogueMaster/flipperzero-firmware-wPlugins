#ifndef MINE_SWEEPER_SOLVER_H
#define MINE_SWEEPER_SOLVER_H

#include "mstarlib_helpers.h"
#include "mine_sweeper_engine.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

bool check_board_with_solver(MineSweeperBoard* board);

void bfs_tile_clear_solver(
    MineSweeperBoard* board,
    uint16_t x,
    uint16_t y,
    point_deq_t* edges,
    uint8_t* visited);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // MINE_SWEEPER_SOLVER_H
