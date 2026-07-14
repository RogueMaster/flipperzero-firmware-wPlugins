#include "mine_sweeper_solver.h"
#include "mine_sweeper_engine.h"
#include <furi.h>

bool check_board_with_solver(MineSweeperBoard* board) {
    furi_assert(board);

    point_deq_t deq;
    uint8_t visited[(BOARD_MAX_TILES + 7u) / 8u];

    point_deq_init(deq);
    point_visited_clear(visited, (uint16_t)board->width * board->height);

    uint16_t total_mines = board->mine_count;
    bool is_solvable = false;
    bool has_invalid_flag_deduction = false;

    Point_t pos;
    pointobj_init(pos);

    pointobj_set_point(pos, (Point){.x = 0, .y = 0});

    bfs_tile_clear_solver(board, 0, 0, &deq, visited);

    while(!is_solvable && !has_invalid_flag_deduction && point_deq_size(deq) > 0) {
        bool is_stuck = true;

        int16_t deq_size = point_deq_size(deq);

        while(deq_size-- > 0) {
            point_deq_pop_front(&pos, deq);
            const Point curr_pos = pointobj_get_point(pos);
            uint16_t curr_pos_1d = board_index(board, curr_pos.x, curr_pos.y);

            const MineSweeperCell cell = board->cells[curr_pos_1d];
            uint8_t tile_number = CELL_GET_NEIGHBORS(cell);
            uint8_t hidden_neighbors = 0;
            uint8_t flagged_neighbors = 0;

            if(!tile_number) continue;

            for(uint8_t n = 0; n < 8; ++n) {
                const int16_t dx = curr_pos.x + neighbor_offsets[n][0];
                const int16_t dy = curr_pos.y + neighbor_offsets[n][1];

                if(!board_in_bounds(board, dx, dy)) {
                    continue;
                }

                const int16_t neighbor_pos_1d = board_index(board, dx, dy);
                const MineSweeperCell neighbor_cell = board->cells[neighbor_pos_1d];

                if(CELL_IS_FLAGGED(neighbor_cell)) {
                    flagged_neighbors++;
                } else if(!CELL_IS_REVEALED(neighbor_cell)) {
                    hidden_neighbors++;
                }
            }

            if(flagged_neighbors > tile_number) {
                has_invalid_flag_deduction = true;
                break;
            }

            const uint8_t remaining_mines = tile_number - flagged_neighbors;

            if(remaining_mines == 0) {
                for(uint8_t n = 0; n < 8; ++n) {
                    const int16_t dx = curr_pos.x + neighbor_offsets[n][0];
                    const int16_t dy = curr_pos.y + neighbor_offsets[n][1];

                    if(!board_in_bounds(board, dx, dy)) continue;

                    const int16_t neighbor_pos_1d = board_index(board, dx, dy);
                    const MineSweeperCell neighbor_cell = board->cells[neighbor_pos_1d];

                    if(!CELL_IS_REVEALED(neighbor_cell) && !CELL_IS_FLAGGED(neighbor_cell)) {
                        bfs_tile_clear_solver(board, dx, dy, &deq, visited);
                    }
                }

                is_stuck = false;

            } else if(hidden_neighbors == remaining_mines) {
                for(uint8_t n = 0; n < 8; ++n) {
                    const int16_t dx = curr_pos.x + neighbor_offsets[n][0];
                    const int16_t dy = curr_pos.y + neighbor_offsets[n][1];

                    if(!board_in_bounds(board, dx, dy)) continue;

                    const int16_t neighbor_pos_1d = board_index(board, dx, dy);
                    const MineSweeperCell neighbor_cell = board->cells[neighbor_pos_1d];

                    if(!CELL_IS_REVEALED(neighbor_cell) && !CELL_IS_FLAGGED(neighbor_cell)) {
                        if(!CELL_IS_MINE(neighbor_cell) || total_mines == 0) {
                            has_invalid_flag_deduction = true;
                            break;
                        }

                        CELL_SET_FLAGGED(board->cells[neighbor_pos_1d]);
                        total_mines--;
                    }
                }

                if(has_invalid_flag_deduction) break;

                if(total_mines == 0) is_solvable = true;

                is_stuck = false;

            } else if(hidden_neighbors != 0) {
                point_deq_push_back(deq, pos);
            }
        }

        if(is_stuck) break;
    }

    point_deq_clear(deq);

    return is_solvable;
}

void bfs_tile_clear_solver(
    MineSweeperBoard* board,
    uint16_t x,
    uint16_t y,
    point_deq_t* edges,
    uint8_t* visited) {
    furi_assert(board);
    furi_assert(edges);
    furi_assert(visited);

    point_deq_t deq;
    point_deq_init(deq);

    Point_t pos;
    pointobj_init(pos);

    pointobj_set_point(pos, (Point){.x = x, .y = y});

    point_deq_push_back(deq, pos);

    while(point_deq_size(deq) > 0) {
        point_deq_pop_front(&pos, deq);
        Point curr_pos = pointobj_get_point(pos);
        uint16_t curr_pos_1d = board_index(board, curr_pos.x, curr_pos.y);
        MineSweeperCell curr_cell = board->cells[curr_pos_1d];

        if(point_visited_test(visited, curr_pos_1d) || CELL_IS_REVEALED(curr_cell) ||
           CELL_IS_FLAGGED(curr_cell)) {
            continue;
        }

        point_visited_set(visited, curr_pos_1d);

        uint8_t neighbor_bomb_count = CELL_GET_NEIGHBORS(curr_cell);

        if(CELL_IS_MINE(curr_cell)) {
            continue;
        }

        CELL_SET_REVEALED(board->cells[curr_pos_1d]);

        if(neighbor_bomb_count) {
            point_deq_push_back(*edges, pos);
            continue;
        }

        for(uint8_t n = 0; n < 8; ++n) {
            const int16_t dx = curr_pos.x + neighbor_offsets[n][0];
            const int16_t dy = curr_pos.y + neighbor_offsets[n][1];

            if(!board_in_bounds(board, dx, dy)) continue;

            const uint16_t neighbor_pos_1d = board_index(board, (uint8_t)dx, (uint8_t)dy);
            if(point_visited_test(visited, neighbor_pos_1d)) continue;

            pointobj_set_point(pos, (Point){.x = (uint8_t)dx, .y = (uint8_t)dy});

            point_deq_push_back(deq, pos);
        }
    }

    point_deq_clear(deq);
}
