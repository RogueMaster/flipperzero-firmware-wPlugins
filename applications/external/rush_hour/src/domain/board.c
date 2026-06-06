#include "../../include/domain/board.h"

static bool piece_covers(const TutuPiece *p, int r, int c) {
    if (p->o == TUTU_H) {
        return r == (int)p->r && c >= (int)p->c && c < (int)p->c + (int)p->len;
    }
    return c == (int)p->c && r >= (int)p->r && r < (int)p->r + (int)p->len;
}

void tutu_board_init(TutuBoard *b, const TutuPiece *pieces, uint8_t count) {
    b->count = count;
    for (uint8_t i = 0; i < count; i++)
        b->pieces[i] = pieces[i];
    // insertion sort indices by (r, c)
    for (uint8_t i = 0; i < count; i++)
        b->order[i] = i;
    for (uint8_t i = 1; i < count; i++) {
        uint8_t key = b->order[i];
        int j = i - 1;
        while (j >= 0) {
            const TutuPiece *a = &b->pieces[b->order[j]];
            const TutuPiece *k = &b->pieces[key];
            if (a->r > k->r || (a->r == k->r && a->c > k->c)) {
                b->order[j + 1] = b->order[j];
                j--;
            } else
                break;
        }
        b->order[j + 1] = key;
    }
}

bool tutu_board_cell_occupied(const TutuBoard *b, int r, int c, int ignore) {
    for (uint8_t i = 0; i < b->count; i++) {
        if ((int)i == ignore)
            continue;
        if (piece_covers(&b->pieces[i], r, c))
            return true;
    }
    return false;
}

bool tutu_board_can_move(const TutuBoard *b, uint8_t idx, int delta) {
    const TutuPiece *p = &b->pieces[idx];
    int r, c; // the new leading cell that must be free & in-grid
    if (p->o == TUTU_H) {
        r = p->r;
        c = (delta > 0) ? (int)p->c + (int)p->len : (int)p->c - 1;
    } else {
        c = p->c;
        r = (delta > 0) ? (int)p->r + (int)p->len : (int)p->r - 1;
    }
    if (r < 0 || r >= TUTU_SIZE || c < 0 || c >= TUTU_SIZE)
        return false;
    return !tutu_board_cell_occupied(b, r, c, idx);
}

bool tutu_board_move(TutuBoard *b, uint8_t idx, int delta) {
    if (!tutu_board_can_move(b, idx, delta))
        return false;
    TutuPiece *p = &b->pieces[idx];
    if (p->o == TUTU_H)
        p->c = (uint8_t)((int)p->c + delta);
    else
        p->r = (uint8_t)((int)p->r + delta);
    return true;
}

bool tutu_board_won(const TutuBoard *b) {
    const TutuPiece *red = &b->pieces[TUTU_RED];
    return red->o == TUTU_H && red->r == TUTU_EXIT_ROW &&
           ((int)red->c + (int)red->len - 1) == TUTU_SIZE - 1;
}

uint8_t tutu_board_next_piece(const TutuBoard *b, uint8_t current) {
    uint8_t pos = 0;
    for (uint8_t i = 0; i < b->count; i++)
        if (b->order[i] == current) {
            pos = i;
            break;
        }
    return b->order[(pos + 1) % b->count];
}
