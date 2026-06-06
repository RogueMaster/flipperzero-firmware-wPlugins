#pragma once
#include <stdbool.h>
#include <stdint.h>

#define TUTU_SIZE 6
#define TUTU_EXIT_ROW 2
#define TUTU_MAX_PIECES 16
#define TUTU_RED 0

typedef enum { TUTU_H = 0, TUTU_V = 1 } TutuOrient;

typedef struct {
    uint8_t r;   // top row, 0..5
    uint8_t c;   // left col, 0..5
    uint8_t len; // 2 or 3
    uint8_t o;   // TutuOrient
} TutuPiece;

typedef struct {
    TutuPiece pieces[TUTU_MAX_PIECES];
    uint8_t count;
    uint8_t order[TUTU_MAX_PIECES]; // piece indices sorted by (r,c) for spatial cycling
} TutuBoard;

// Initialize a board from a piece array; computes the spatial cycle order.
void tutu_board_init(TutuBoard *b, const TutuPiece *pieces, uint8_t count);

// True if cell (r,c) is covered by any piece other than `ignore` (-1 = ignore none).
bool tutu_board_cell_occupied(const TutuBoard *b, int r, int c, int ignore);

// Can piece `idx` slide by `delta` (-1 or +1) one cell along its axis?
bool tutu_board_can_move(const TutuBoard *b, uint8_t idx, int delta);

// Apply the move if legal; return true if it happened.
bool tutu_board_move(TutuBoard *b, uint8_t idx, int delta);

// True when the red car (piece 0) has reached the exit at the right edge.
bool tutu_board_won(const TutuBoard *b);

// Next piece index in spatial cycle order after `current` (wraps).
uint8_t tutu_board_next_piece(const TutuBoard *b, uint8_t current);
