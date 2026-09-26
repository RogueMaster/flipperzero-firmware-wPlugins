#include "maze3d.h"
#include <stdlib.h>

// Xorshift PRNG
static uint32_t rng_state = 1;
static void rng_seed(unsigned int s) {
    rng_state = s ? s : 1;
}
uint32_t maze_rng_next(void) {
    uint32_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x;
    return x;
}

int maze_cell_index(int x, int y) {
    return y * MAP_MAX + x;
}
uint8_t maze_get(int x, int y) {
    if((unsigned)x >= (unsigned)g.map_w || (unsigned)y >= (unsigned)g.map_h) return WALL_BRICK;
    return g.map[(uint16_t)y * MAP_MAX + x];
}
void maze_set(int x, int y, uint8_t v) {
    if((unsigned)x >= (unsigned)g.map_w || (unsigned)y >= (unsigned)g.map_h) return;
    g.map[(uint16_t)y * MAP_MAX + x] = v;
}

// Recursive-backtracker maze carving (iterative, stack in static storage to
// avoid blowing the 8KB application stack).
static void carve(int w, int h) {
    for(int y = 0; y < h; y++)
        for(int x = 0; x < w; x++)
            g.map[(uint16_t)y * MAP_MAX + x] = WALL_BRICK;

    static int sx[MAP_MAX * MAP_MAX], sy[MAP_MAX * MAP_MAX];
    int top = 0;
    sx[top] = 1;
    sy[top] = 1;
    top++;
    g.map[1 * MAP_MAX + 1] = CELL_EMPTY;

    static const int dx[4] = {2, -2, 0, 0};
    static const int dy[4] = {0, 0, 2, -2};

    while(top > 0) {
        int cx = sx[top - 1], cy = sy[top - 1];
        int order[4] = {0, 1, 2, 3};
        for(int i = 3; i > 0; i--) {
            int j = maze_rng_next() % (i + 1);
            int t = order[i];
            order[i] = order[j];
            order[j] = t;
        }
        int moved = 0;
        for(int k = 0; k < 4; k++) {
            int idx = order[k];
            int nx = cx + dx[idx], ny = cy + dy[idx];
            if(nx > 0 && ny > 0 && nx < w - 1 && ny < h - 1 &&
               g.map[(uint16_t)ny * MAP_MAX + nx] == WALL_BRICK) {
                int wx = cx + dx[idx] / 2, wy = cy + dy[idx] / 2;
                g.map[(uint16_t)wy * MAP_MAX + wx] = CELL_EMPTY;
                g.map[(uint16_t)ny * MAP_MAX + nx] = CELL_EMPTY;
                sx[top] = nx;
                sy[top] = ny;
                top++;
                moved = 1;
                break;
            }
        }
        if(!moved) top--;
    }
}

// Add a few loops so the maze is not a strict tree (more interesting to walk).
static void add_loops(int w, int h, int count) {
    for(int i = 0; i < count; i++) {
        int x = 1 + maze_rng_next() % (w - 2);
        int y = 1 + maze_rng_next() % (h - 2);
        if(g.map[(uint16_t)y * MAP_MAX + x] == WALL_BRICK) {
            int horiz =
                (g.map[(uint16_t)y * MAP_MAX + x - 1] == CELL_EMPTY &&
                 g.map[(uint16_t)y * MAP_MAX + x + 1] == CELL_EMPTY);
            int vert =
                (g.map[(uint16_t)(y - 1) * MAP_MAX + x] == CELL_EMPTY &&
                 g.map[(uint16_t)(y + 1) * MAP_MAX + x] == CELL_EMPTY);
            if(horiz || vert) g.map[(uint16_t)y * MAP_MAX + x] = CELL_EMPTY;
        }
    }
}

// BFS from start to find the farthest reachable empty cell (the exit).
static int bfs_dist[MAP_MAX * MAP_MAX];
static void find_far(int w, int h, int sxx, int syy, int* ox, int* oy) {
    static int qx[MAP_MAX * MAP_MAX], qy[MAP_MAX * MAP_MAX];
    int head = 0, tail = 0;
    for(int i = 0; i < MAP_MAX * MAP_MAX; i++)
        bfs_dist[i] = -1;
    qx[tail] = sxx;
    qy[tail] = syy;
    tail++;
    bfs_dist[syy * MAP_MAX + sxx] = 0;

    *ox = sxx;
    *oy = syy;
    int best = 0;

    while(head < tail) {
        int cx = qx[head], cy = qy[head];
        head++;
        int cd = bfs_dist[cy * MAP_MAX + cx];
        if(cd > best) {
            best = cd;
            *ox = cx;
            *oy = cy;
        }
        static const int dx[4] = {1, -1, 0, 0};
        static const int dy[4] = {0, 0, 1, -1};
        for(int k = 0; k < 4; k++) {
            int nx = cx + dx[k], ny = cy + dy[k];
            if(nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
            int idx = ny * MAP_MAX + nx;
            if(bfs_dist[idx] != -1) continue;
            if(g.map[(uint16_t)idx] == WALL_BRICK) continue;
            bfs_dist[idx] = cd + 1;
            qx[tail] = nx;
            qy[tail] = ny;
            tail++;
        }
    }
}

// Generate a pure maze: carve, add loops, place exit at the farthest cell.
void maze_generate(int w, int h, int level, unsigned int seed) {
    if(w > MAP_MAX) w = MAP_MAX;
    if(h > MAP_MAX) h = MAP_MAX;
    if(w % 2 == 0) w--;
    if(h % 2 == 0) h--;
    if(w < 7) w = 7;
    if(h < 7) h = 7;
    g.map_w = w;
    g.map_h = h;

    rng_seed(seed + level * 2654435761u);
    carve(w, h);

    int loops = 8 + level / 2;
    if(loops > 30) loops = 30;
    add_loops(w, h, loops);

    // Exit at the farthest reachable cell from the start.
    int ex, ey;
    find_far(w, h, 1, 1, &ex, &ey);
    g.map[(uint16_t)ey * MAP_MAX + ex] = CELL_EXIT;
    g.exit_x = ex;
    g.exit_y = ey;
    g.exit_found = true;
}
