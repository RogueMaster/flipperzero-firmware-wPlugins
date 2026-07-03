#pragma once
#include "board.h"

typedef struct {
    uint8_t count;
    TutuPiece pieces[TUTU_MAX_PIECES];
    uint8_t optimal_moves;
} TutuLevel;
