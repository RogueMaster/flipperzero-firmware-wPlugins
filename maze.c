#include "maze3d.h"
#include <stdlib.h>

// 简单确定性 PRNG (xorshift32), 保证可复现
static uint32_t rng_state = 1;
static void rng_seed(unsigned int s) { rng_state = s ? s : 1; }
static uint32_t rng_next(void) {
    uint32_t x = rng_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    rng_state = x;
    return x;
}

int maze_cell_index(int x, int y) { return y * MAP_MAX + x; }
uint8_t maze_get(int x, int y) {
    if(x < 0 || y < 0 || x >= g.map_w || y >= g.map_h) return WALL_BRICK;
    return g.map[y * MAP_MAX + x];
}
void maze_set(int x, int y, uint8_t v) {
    if(x < 0 || y < 0 || x >= g.map_w || y >= g.map_h) return;
    g.map[y * MAP_MAX + x] = v;
}

// 递归回溯生成完美迷宫(用显式栈避免深递归)
static void carve(int w, int h) {
    // 全部填墙
    for(int y = 0; y < h; y++)
        for(int x = 0; x < w; x++)
            g.map[y * MAP_MAX + x] = WALL_BRICK;

    // 栈
    int sx[MAP_MAX * MAP_MAX], sy[MAP_MAX * MAP_MAX];
    int top = 0;
    // 从 (1,1) 开始(奇数坐标为通道)
    sx[top] = 1; sy[top] = 1; top++;
    g.map[1 * MAP_MAX + 1] = CELL_EMPTY;

    static const int dx[4] = {2, -2, 0, 0};
    static const int dy[4] = {0, 0, 2, -2};

    while(top > 0) {
        int cx = sx[top - 1], cy = sy[top - 1];
        // 随机选一个未访问邻居
        int order[4] = {0, 1, 2, 3};
        for(int i = 3; i > 0; i--) {
            int j = rng_next() % (i + 1);
            int t = order[i]; order[i] = order[j]; order[j] = t;
        }
        int moved = 0;
        for(int k = 0; k < 4; k++) {
            int nx = cx + dx[order[k]], ny = cy + dy[order[k]];
            if(nx > 0 && ny > 0 && nx < w - 1 && ny < h - 1 &&
               g.map[ny * MAP_MAX + nx] == WALL_BRICK) {
                // 打通中间墙
                g.map[(cy + dy[order[k]] / 2) * MAP_MAX + (cx + dx[order[k]] / 2)] = CELL_EMPTY;
                g.map[ny * MAP_MAX + nx] = CELL_EMPTY;
                sx[top] = nx; sy[top] = ny; top++;
                moved = 1;
                break;
            }
        }
        if(!moved) top--; // 回溯
    }
}

// 随机打通一些墙, 增加环路(降低死路感, 难度越高打通越少)
static void add_loops(int w, int h, int count) {
    for(int i = 0; i < count; i++) {
        int x = 1 + rng_next() % (w - 2);
        int y = 1 + rng_next() % (h - 2);
        if(g.map[y * MAP_MAX + x] == WALL_BRICK) {
            // 只打通能连接两条通道的墙
            int horiz = (g.map[y * MAP_MAX + x - 1] == CELL_EMPTY && g.map[y * MAP_MAX + x + 1] == CELL_EMPTY);
            int vert = (g.map[(y - 1) * MAP_MAX + x] == CELL_EMPTY && g.map[(y + 1) * MAP_MAX + x] == CELL_EMPTY);
            if(horiz || vert) g.map[y * MAP_MAX + x] = CELL_EMPTY;
        }
    }
}

// 找一个远离起点的空格
static void find_far(int w, int h, int sx, int sy, int* ox, int* oy) {
    *ox = sx; *oy = sy;
    int best = -1;
    for(int y = 1; y < h - 1; y++) {
        for(int x = 1; x < w - 1; x++) {
            if(g.map[y * MAP_MAX + x] == CELL_EMPTY) {
                int d = abs(x - sx) + abs(y - sy);
                if(d > best) { best = d; *ox = x; *oy = y; }
            }
        }
    }
}

void maze_generate(int w, int h, int level, unsigned int seed) {
    if(w > MAP_MAX) w = MAP_MAX;
    if(h > MAP_MAX) h = MAP_MAX;
    // 保证奇数尺寸(迷宫生成要求)
    if(w % 2 == 0) w--;
    if(h % 2 == 0) h--;
    if(w < 7) w = 7;
    if(h < 7) h = 7;

    g.map_w = w; g.map_h = h;
    rng_seed(seed + level * 2654435761u);

    carve(w, h);

    // 难度越高, 环路越少(更绕) -> 难度越低打通越多
    int loops = 20 - (level > 20 ? 18 : level);
    if(loops < 2) loops = 2;
    add_loops(w, h, loops);

    // 玩家起点 (1,1)
    // 出口放在最远处
    int ex, ey;
    find_far(w, h, 1, 1, &ex, &ey);
    g.map[ey * MAP_MAX + ex] = CELL_EXIT;

    // 根据关卡阶段放置道具/门
    // 10-20关: 钥匙+门(解谜), 火把
    // 20+关: 同上 + 陷阱
    int stage = 0;
    if(level >= 20) stage = 2;
    else if(level >= 10) stage = 1;

    if(stage >= 1) {
        // 随机放钥匙(1-2把)
        int keys = 1 + (level >= 15 ? 1 : 0);
        int placed = 0, tries = 0;
        while(placed < keys && tries++ < 200) {
            int x = 1 + rng_next() % (w - 2);
            int y = 1 + rng_next() % (h - 2);
            if(g.map[y * MAP_MAX + x] == CELL_EMPTY && !(x == 1 && y == 1) && !(x == ex && y == ey)) {
                g.map[y * MAP_MAX + x] = CELL_KEY;
                placed++;
            }
        }
        // 在出口处放门(需要钥匙才能过) - 仅 15+ 关
        if(level >= 15) {
            // 出口前一格放门
            // 简化: 直接在出口旁边放门
            g.map[ey * MAP_MAX + ex] = CELL_DOOR; // 把出口变成门, 玩家有钥匙时打开
            // 再放一个真正的出口在更远处
            int ex2, ey2;
            find_far(w, h, ex, ey, &ex2, &ey2);
            g.map[ey2 * MAP_MAX + ex2] = CELL_EXIT;
        }
        // 火把(得分收集)
        int torches = 2 + level / 5;
        placed = 0; tries = 0;
        while(placed < torches && tries++ < 300) {
            int x = 1 + rng_next() % (w - 2);
            int y = 1 + rng_next() % (h - 2);
            if(g.map[y * MAP_MAX + x] == CELL_EMPTY) {
                g.map[y * MAP_MAX + x] = CELL_TORCH;
                placed++;
            }
        }
    }

    if(stage >= 2) {
        // 陷阱(踩到掉血)
        int traps = 2 + (level - 20) / 3;
        if(traps > 8) traps = 8;
        int placed = 0, tries = 0;
        while(placed < traps && tries++ < 300) {
            int x = 1 + rng_next() % (w - 2);
            int y = 1 + rng_next() % (h - 2);
            if(g.map[y * MAP_MAX + x] == CELL_EMPTY && (abs(x - 1) + abs(y - 1) > 3)) {
                g.map[y * MAP_MAX + x] = CELL_TRAP;
                placed++;
            }
        }
    }
}

// 提供给 game.c 使用的随机数(敌人/NPC 移动)
uint32_t maze_rng_next(void) { return rng_next(); }
