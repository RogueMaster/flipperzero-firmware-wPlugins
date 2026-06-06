#include "../include/data/levels.h"
#include "../include/domain/board.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    assert(tutu_levels_count() == 100);
    int prev = -1;
    for(uint16_t i = 0; i < tutu_levels_count(); i++) {
        const TutuLevel* l = tutu_levels_get(i);
        assert(l != NULL);
        assert(l->count >= 1 && l->count <= TUTU_MAX_PIECES);
        // piece 0 is the red car: horizontal, length 2, on the exit row
        assert(l->pieces[TUTU_RED].o == TUTU_H);
        assert(l->pieces[TUTU_RED].len == 2);
        assert(l->pieces[TUTU_RED].r == TUTU_EXIT_ROW);
        // all pieces in-grid
        for(uint8_t k = 0; k < l->count; k++) {
            const TutuPiece* p = &l->pieces[k];
            assert(p->len == 2 || p->len == 3);
            if(p->o == TUTU_H)
                assert((int)p->c + (int)p->len <= TUTU_SIZE && p->r < TUTU_SIZE);
            else
                assert((int)p->r + (int)p->len <= TUTU_SIZE && p->c < TUTU_SIZE);
        }
        // build a board (smoke check that init is consistent)
        TutuBoard b;
        tutu_board_init(&b, l->pieces, l->count);
        assert(!tutu_board_won(&b)); // a starting position must not be already solved
        // ascending curve
        assert((int)l->optimal_moves >= prev);
        prev = l->optimal_moves;
    }
    printf("test_levels_data: all passed (100 levels)\n");
    return 0;
}
