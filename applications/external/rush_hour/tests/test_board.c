#include "../include/domain/board.h"
#include <assert.h>
#include <stdio.h>

// Red H len2 at (2,0); a vertical blocker V len3 at (0,3).
static void setup_simple(TutuBoard *b) {
    TutuPiece p[2] = {
        {2, 0, 2, TUTU_H}, // red
        {0, 3, 3, TUTU_V}, // blocker spanning rows 0..2 at col 3
    };
    tutu_board_init(b, p, 2);
}

static void test_occupancy(void) {
    TutuBoard b;
    setup_simple(&b);
    assert(tutu_board_cell_occupied(&b, 2, 0, -1));  // red
    assert(tutu_board_cell_occupied(&b, 2, 1, -1));  // red second cell
    assert(!tutu_board_cell_occupied(&b, 2, 2, -1)); // empty
    assert(tutu_board_cell_occupied(&b, 2, 3, -1));  // blocker bottom cell
    assert(!tutu_board_cell_occupied(&b, 2, 3, 1));  // ignoring the blocker -> empty
}

static void test_move_legality(void) {
    TutuBoard b;
    setup_simple(&b);
    assert(!tutu_board_can_move(&b, TUTU_RED, -1)); // red at col0 cannot go left
    assert(tutu_board_can_move(&b, TUTU_RED, +1));  // can move right to col1..2
    assert(tutu_board_move(&b, TUTU_RED, +1));
    assert(b.pieces[TUTU_RED].c == 1);
    // now red occupies cols 1,2; col3 is the blocker -> cannot move right
    assert(!tutu_board_can_move(&b, TUTU_RED, +1));
}

static void test_vertical_move_and_bounds(void) {
    TutuBoard b;
    setup_simple(&b);
    // blocker idx1 spans rows0..2 at col3; can move down (row1..3)? cell (3,3) empty -> yes
    assert(tutu_board_can_move(&b, 1, +1));
    // cannot move up (row -1)
    assert(!tutu_board_can_move(&b, 1, -1));
}

static void test_win(void) {
    TutuBoard b;
    TutuPiece p[1] = {{2, 4, 2, TUTU_H}}; // red already at cols 4,5
    tutu_board_init(&b, p, 1);
    assert(tutu_board_won(&b));
    TutuPiece q[1] = {{2, 0, 2, TUTU_H}};
    tutu_board_init(&b, q, 1);
    assert(!tutu_board_won(&b));
}

static void test_spatial_cycle(void) {
    // pieces at (2,0),(0,3),(4,1) -> sorted by (r,c): (0,3)=idx1,(2,0)=idx0,(4,1)=idx2
    TutuPiece p[3] = {{2, 0, 2, TUTU_H}, {0, 3, 3, TUTU_V}, {4, 1, 2, TUTU_H}};
    TutuBoard b;
    tutu_board_init(&b, p, 3);
    assert(tutu_board_next_piece(&b, 1) == 0); // after (0,3) comes (2,0)
    assert(tutu_board_next_piece(&b, 0) == 2); // after (2,0) comes (4,1)
    assert(tutu_board_next_piece(&b, 2) == 1); // wraps back to (0,3)
}

static void test_known_solution_solves(void) {
    TutuPiece p[1] = {{2, 0, 2, TUTU_H}};
    TutuBoard b;
    tutu_board_init(&b, p, 1);
    for (int i = 0; i < 4; i++)
        assert(tutu_board_move(&b, TUTU_RED, +1));
    assert(tutu_board_won(&b));
}

int main(void) {
    test_occupancy();
    test_move_legality();
    test_vertical_move_and_bounds();
    test_win();
    test_spatial_cycle();
    test_known_solution_solves();
    printf("test_board: all passed\n");
    return 0;
}
