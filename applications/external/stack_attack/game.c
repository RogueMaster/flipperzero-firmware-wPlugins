#include "game.h"

#include <string.h>

// Speeds in pixels per tick, measured from the demake at 15 fps.
#define CRANE_SPEED 2
#define JUMP_HEIGHT 8 // one frame at the top, then straight down
#define CRANE_OPEN  4
#define IDLE_FRONT  10

// Scoring as in stktk: 2 per box a crane drops, 10 x cranes for a cleared row.
#define SCORE_DROP       2
#define SCORE_ROW        10
#define CRANE_GAP        11 // ticks between cranes entering: 22 px at 2 px/tick
#define START_BOXES      12
#define START_MAX_HEIGHT 2
#define JUMP_MAX_ROW     3 // no jumping from a stack 3 or more boxes high
#define COLUMN_MAX       5 // cranes don't drop onto a column this high

static uint32_t rnd(Game* g) {
    uint32_t x = g->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g->rng = x;
    return x;
}

static bool overlap(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) {
    return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
}

void game_init(Game* g, uint32_t seed, uint8_t start_cranes) {
    memset(g, 0, sizeof(*g));
    g->rng = seed ? seed : 0x9e3779b9;
    g->cranes = start_cranes < 1 ? 1 : start_cranes > G_MAX_CRANES ? G_MAX_CRANES : start_cranes;
    g->spawn_timer = 15;
    g->w.face = 1;
    g->w.idle = IDLE_FRONT;

    // 12 boxes, at most 2 per column, never a full bottom row (stktk).
    int used = 0;
    for(int i = 0; i < START_BOXES; i++) {
        int c;
        for(;;) {
            c = rnd(g) % G_COLS;
            int h = g->cell[1][c] ? 2 : g->cell[0][c] ? 1 : 0;
            if(h >= START_MAX_HEIGHT) continue;
            if(h == 0 && used == G_COLS - 1) continue; // would fill the bottom row
            if(h == 0) used++;
            g->cell[h][c] = 1 + rnd(g) % G_BOX_TYPES;
            break;
        }
    }

    // The worker starts on top of the leftmost 2-high stack.
    int col = 0, h = 0;
    for(int c = 0; c < G_COLS; c++) {
        if(g->cell[1][c]) {
            col = c;
            h = 2;
            break;
        }
    }
    if(!h) h = g->cell[0][0] ? 1 : 0;
    g->w.x = g_col_x(col);
    g->w.y = G_FLOOR_Y - 16 - G_CELL * h;
}

// Settled cells: which cells does a pixel rect touch?
static bool rect_hits_cells(const Game* g, int x, int y, int w, int h) {
    int c0 = (x - G_WALL) / G_CELL, c1 = (x + w - 1 - G_WALL) / G_CELL;
    for(int c = c0; c <= c1; c++) {
        if(c < 0 || c >= G_COLS) continue;
        for(int r = 0; r < G_ROWS; r++) {
            if(g->cell[r][c] && overlap(x, y, w, h, g_col_x(c), g_row_y(r), G_CELL, G_CELL))
                return true;
        }
    }
    return false;
}

static bool rect_hits_falling(const Game* g, int x, int y, int w, int h, int skip) {
    for(int i = 0; i < g->n_falling; i++) {
        if(i == skip) continue;
        const GBox* b = &g->falling[i];
        if(overlap(x, y, w, h, b->x, b->y, G_CELL, G_CELL)) return true;
    }
    return false;
}

static bool out_of_field(int x, int y, int w, int h) {
    return x < G_WALL || x + w > G_WALL + G_COLS * G_CELL || y + h > G_FLOOR_Y || y < G_CEIL_Y;
}

// Is an 8x16 worker rect at (x, y) blocked? The box being pushed is ignored:
// the worker is always right behind it.
bool game_worker_blocked(const Game* g, int x, int y) {
    return out_of_field(x, y, 8, 16) || rect_hits_cells(g, x, y, 8, 16) ||
           rect_hits_falling(g, x, y, 8, 16, -1);
}

// A falling box stops on cells, the floor, the pushed box and the cell the pushed box
// is heading into.
static bool box_blocked(const Game* g, int x, int y, int skip) {
    if(y + G_CELL > G_FLOOR_Y) return true;
    if(rect_hits_cells(g, x, y, G_CELL, G_CELL)) return true;
    if(rect_hits_falling(g, x, y, G_CELL, G_CELL, skip)) return true;
    if(g->has_pushed) {
        const GBox* p = &g->pushed;
        if(overlap(x, y, G_CELL, G_CELL, p->x, p->y, G_CELL, G_CELL)) return true;
        int tx = p->x + g->pushed_dir * g->pushed_left;
        if(overlap(x, y, G_CELL, G_CELL, tx, p->y, G_CELL, G_CELL)) return true;
    }
    return false;
}

static int row_of_y(int y) {
    return (G_FLOOR_Y - G_CELL - y) / G_CELL;
}

static int col_of_x(int x) {
    return (x - G_WALL) / G_CELL;
}

static void add_falling(Game* g, int x, int y, uint8_t type) {
    if(g->n_falling >= G_MAX_FALLING) return;
    GBox* b = &g->falling[g->n_falling++];
    b->x = x;
    b->y = y;
    b->type = type;
}

static bool column_targeted(const Game* g, int col, int self) {
    for(int i = 0; i < G_MAX_CRANES; i++) {
        const GCrane* c = &g->crane[i];
        if(i != self && c->active && c->state == CraneCarry && c->target == col) return true;
    }
    return false;
}

static bool drop_slot_free(const Game* g, int col) {
    int x = g_col_x(col);
    // not right onto the worker's head either
    if(overlap(x, G_CRANE_BOX_Y, G_CELL, 2 * G_CELL, g->w.x, g->w.y, 8, 16)) return false;
    return !g->cell[COLUMN_MAX - 1][col] && !g->cell[G_BOX_ROWS - 1][col] &&
           !rect_hits_falling(g, x, G_CRANE_BOX_Y, G_CELL, G_CELL, -1) &&
           !box_blocked(g, x, G_CRANE_BOX_Y, -1);
}

static void spawn_crane(Game* g) {
    int slot = -1, active = 0;
    for(int i = 0; i < G_MAX_CRANES; i++) {
        if(g->crane[i].active)
            active++;
        else if(slot < 0)
            slot = i;
    }
    if(slot < 0 || active >= g->cranes) {
        g->spawn_timer = 1; // retry when a crane leaves
        return;
    }

    int cols[G_COLS], n = 0;
    for(int c = 0; c < G_COLS; c++)
        if(drop_slot_free(g, c) && !column_targeted(g, c, -1)) cols[n++] = c;
    if(!n) {
        g->spawn_timer = 1;
        return;
    }

    GCrane* c = &g->crane[slot];
    c->active = true;
    c->dir = (rnd(g) & 1) ? 1 : -1;
    // start off screen two cells past the walls, so 2 px steps land exactly on columns
    c->x = c->dir > 0 ? g_col_x(0) - 2 * G_CELL : g_col_x(G_COLS - 1) + 2 * G_CELL;
    c->target = cols[rnd(g) % n];
    c->type = 1 + rnd(g) % G_BOX_TYPES;
    c->state = CraneCarry;
    c->timer = 0;
    g->spawn_timer = CRANE_GAP;
}

static void retarget(Game* g, int i) {
    GCrane* c = &g->crane[i];
    for(int col = c->target + c->dir; col >= 0 && col < G_COLS; col += c->dir) {
        if(drop_slot_free(g, col) && !column_targeted(g, col, i)) {
            c->target = col;
            return;
        }
    }
    c->target = -1;
}

static void update_cranes(Game* g) {
    if(g->spawn_timer && --g->spawn_timer == 0) spawn_crane(g);

    for(int i = 0; i < G_MAX_CRANES; i++) {
        GCrane* c = &g->crane[i];
        if(!c->active) continue;

        if(c->state == CraneOpen) {
            if(--c->timer == 0) c->state = CraneLeave;
            continue;
        }

        c->x += CRANE_SPEED * c->dir;
        if(c->state == CraneCarry && c->target >= 0 && c->x == g_col_x(c->target)) {
            if(drop_slot_free(g, c->target)) {
                add_falling(g, c->x, G_CRANE_BOX_Y, c->type);
                g->score += SCORE_DROP;
                g->boxes_dropped++;
                c->state = CraneOpen;
                c->timer = CRANE_OPEN;
            } else {
                retarget(g, i);
            }
        }
        if(c->x < -20 || c->x > G_FIELD_W + 10) c->active = false;
    }
}

// Rising into a falling box smashes it with the helmet. Both close in by 1 px a tick,
// so look 2 px ahead or they would pass into each other.
static void helmet(Game* g) {
    GWorker* w = &g->w;
    for(int i = 0; i < g->n_falling;) {
        GBox* b = &g->falling[i];
        if(overlap(w->x, w->y - 2, 8, 2, b->x, b->y, G_CELL, G_CELL)) {
            g->debris_x = b->x;
            g->debris_y = b->y;
            g->debris_timer = G_DEBRIS_TICKS;
            g->events |= G_EV_BREAK;
            g->falling[i] = g->falling[--g->n_falling];
        } else {
            i++;
        }
    }
}

static void rise(Game* g) {
    GWorker* w = &g->w;
    helmet(g);
    if(w->vtimer == 0) {
        w->vstate = WFalling;
    } else if(!game_worker_blocked(g, w->x, w->y - 1)) {
        w->y--;
        w->vtimer--;
    } else {
        w->vstate = WFalling;
    }
}

static void update_worker_vertical(Game* g, uint8_t pressed) {
    GWorker* w = &g->w;
    switch(w->vstate) {
    case WGround:
        if((pressed & G_KEY_JUMP) && (G_FLOOR_Y - 16 - w->y) / G_CELL < JUMP_MAX_ROW) {
            w->vstate = WRising;
            w->vtimer = JUMP_HEIGHT;
            w->idle = 0;
            g->events |= G_EV_JUMP;
            rise(g);
        } else if(!game_worker_blocked(g, w->x, w->y + 1)) {
            w->vstate = WFalling;
            w->y++;
        }
        break;
    case WRising:
        rise(g);
        break;
    case WFalling:
        if(!game_worker_blocked(g, w->x, w->y + 1)) {
            w->y++;
        } else {
            w->vstate = WGround;
            g->events |= G_EV_WLAND;
        }
        break;
    }
}

// Worker aligned on a column and a row, facing a single free box: start pushing it.
static bool try_push(Game* g, int dir) {
    GWorker* w = &g->w;
    if(g->has_pushed) return false;
    if((w->x - G_WALL) % G_CELL) return false;
    int feet = G_FLOOR_Y - (w->y + 16);
    if(feet < 0 || feet % G_CELL) return false;
    int r = feet / G_CELL;
    int c = col_of_x(w->x);
    int tc = c + dir, bc = c + 2 * dir;
    if(r >= G_BOX_ROWS || tc < 0 || tc >= G_COLS || bc < 0 || bc >= G_COLS) return false;
    if(!g->cell[r][tc] || (g->clear_mask[r] & (1u << tc))) return false;
    if(g->cell[r + 1][tc] || g->cell[r][bc]) return false;
    if(rect_hits_falling(g, g_col_x(bc), g_row_y(r), G_CELL, G_CELL, -1)) return false;
    if(rect_hits_falling(g, g_col_x(tc), w->y, G_CELL, G_CELL, -1)) return false;

    g->pushed.x = g_col_x(tc);
    g->pushed.y = g_row_y(r);
    g->pushed.type = g->cell[r][tc];
    g->cell[r][tc] = 0;
    g->has_pushed = true;
    g->pushed_stall = 0;
    g->pushed_dir = dir;
    g->pushed_left = G_CELL;
    w->move_dir = dir;
    w->move_left = G_CELL;
    g->events |= G_EV_PUSH;
    return true;
}

// A box still falling in the next column, level with the worker's legs, can be
// pushed too (seen in the demake at 1:12). It slides without falling, then falls on.
static bool try_push_falling(Game* g, int dir) {
    GWorker* w = &g->w;
    if(g->has_pushed || (w->x - G_WALL) % G_CELL) return false;
    int bx = w->x + G_CELL * dir;
    for(int i = 0; i < g->n_falling; i++) {
        GBox* b = &g->falling[i];
        // waist height or lower, as in stktk and both pushes seen in the demake
        if(b->x != bx || !overlap(bx, b->y, G_CELL, G_CELL, bx, w->y + 8, G_CELL, 8)) continue;
        int tx = bx + G_CELL * dir;
        if(out_of_field(tx, b->y, G_CELL, G_CELL)) return false;
        if(rect_hits_cells(g, tx, b->y, G_CELL, G_CELL)) return false;
        if(rect_hits_falling(g, tx, b->y, G_CELL, G_CELL, i)) return false;
        g->pushed = *b;
        g->falling[i] = g->falling[--g->n_falling];
        g->has_pushed = true;
        g->pushed_stall = 1; // one-step stall seen on the C45, then it falls while sliding
        g->pushed_dir = dir;
        g->pushed_left = G_CELL;
        w->move_dir = dir;
        w->move_left = G_CELL;
        g->events |= G_EV_PUSH;
        return true;
    }
    return false;
}

static void update_worker_horizontal(Game* g, uint8_t held) {
    GWorker* w = &g->w;
    int want = 0;
    if((held & G_KEY_LEFT) && !(held & G_KEY_RIGHT)) want = -1;
    if((held & G_KEY_RIGHT) && !(held & G_KEY_LEFT)) want = 1;

    if(w->move_left == 0 && want) {
        w->face = want;
        int off = (w->x - G_WALL) % G_CELL;
        if(off) {
            // between columns: finish the step towards the wanted side
            w->move_dir = want;
            w->move_left = want > 0 ? G_CELL - off : off;
        } else if(!game_worker_blocked(g, w->x + want, w->y)) {
            w->move_dir = want;
            w->move_left = G_CELL;
        } else if(!try_push(g, want)) {
            try_push_falling(g, want);
        }
    }

    if(w->move_left) {
        if(!game_worker_blocked(g, w->x + w->move_dir, w->y)) {
            w->x += w->move_dir;
            w->move_left--;
            w->anim++;
            w->idle = 0;
        } else {
            w->move_left = 0;
        }
    } else if(w->vstate == WGround) {
        if(w->idle < 0xffff) w->idle++;
    }
}

static void check_rows(Game* g) {
    int k = 0;
    for(int r = 0; r < G_BOX_ROWS; r++) {
        if(g->clear_age[r]) continue;
        bool full = true;
        for(int c = 0; c < G_COLS && full; c++)
            full = g->cell[r][c] != 0;
        if(full) {
            g->clear_age[r] = 1;
            g->clear_mask[r] = (1u << G_COLS) - 1;
            k++;
        }
    }
    if(k) {
        g->rows_cleared += k;
        for(int i = 0; i < k; i++) {
            g->score += SCORE_ROW * g->cranes;
            if(g->cranes < G_MAX_CRANES) g->cranes++;
        }
        g->events |= G_EV_CLEAR;
    }
}

static void land(Game* g, const GBox* b) {
    int r = row_of_y(b->y), c = col_of_x(b->x);
    if(r < 0 || r >= G_ROWS || c < 0 || c >= G_COLS) return;
    g->cell[r][c] = b->type;
    g->events |= G_EV_LAND;
}

static void update_pushed(Game* g) {
    if(!g->has_pushed) return;
    GBox* p = &g->pushed;
    p->x += g->pushed_dir;
    // keeps falling while it slides, riding over anything in the way
    if(g->pushed_stall) {
        g->pushed_stall--;
    } else if(
        p->y + G_CELL < G_FLOOR_Y && !rect_hits_cells(g, p->x, p->y + 1, G_CELL, G_CELL) &&
        !rect_hits_falling(g, p->x, p->y + 1, G_CELL, G_CELL, -1)) {
        p->y++;
    }
    if(--g->pushed_left == 0) {
        g->has_pushed = false;
        add_falling(g, g->pushed.x, g->pushed.y, g->pushed.type);
    }
}

static void update_falling(Game* g) {
    for(int i = 0; i < g->n_falling;) {
        GBox* b = &g->falling[i];
        if(box_blocked(g, b->x, b->y + 1, i)) {
            GBox done = *b;
            g->falling[i] = g->falling[--g->n_falling];
            land(g, &done);
        } else {
            b->y++;
            i++;
        }
    }
}

// Settled boxes with nothing under them start falling.
static void apply_gravity(Game* g) {
    for(int r = 1; r < G_ROWS; r++) {
        for(int c = 0; c < G_COLS; c++) {
            if(!g->cell[r][c] || g->cell[r - 1][c]) continue;
            if(g->clear_mask[r] & (1u << c)) continue; // exploding cells stay put
            // the cell below may still be taken by a falling or pushed box
            if(box_blocked(g, g_col_x(c), g_row_y(r - 1), -1)) continue;
            add_falling(g, g_col_x(c), g_row_y(r), g->cell[r][c]);
            g->cell[r][c] = 0;
        }
    }
}

static void update_clearing(Game* g) {
    for(int r = 0; r < G_ROWS; r++) {
        if(!g->clear_age[r]) continue;
        g->clear_age[r]++;
        for(int c = 0; c < G_COLS; c++) {
            uint16_t bit = 1u << c;
            if((g->clear_mask[r] & bit) &&
               g->clear_age[r] - 1 - g_clear_delay(c) >= G_CLEAR_CELL_TICKS) {
                g->cell[r][c] = 0;
                g->clear_mask[r] &= ~bit;
            }
        }
        if(!g->clear_mask[r]) g->clear_age[r] = 0;
    }
}

void game_tick(Game* g, uint8_t held, uint8_t pressed) {
    g->events = 0;
    if(g->over) return;
    g->tick++;

    update_clearing(g);
    if(g->debris_timer) g->debris_timer--;
    update_cranes(g);
    update_worker_vertical(g, pressed);
    update_worker_horizontal(g, held);
    update_pushed(g);
    update_falling(g);
    apply_gravity(g);
    check_rows(g);

    if(rect_hits_falling(g, g->w.x, g->w.y, 8, 16, -1)) {
        g->over = true;
        g->events |= G_EV_DEATH;
    }
}
