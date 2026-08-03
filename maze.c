#include "maze3d.h"
#include <stdlib.h>

// Xorshift PRNG
static uint32_t rng_state = 1;
static void rng_seed(unsigned int s) { rng_state = s ? s : 1; }
uint32_t maze_rng_next(void) {
    uint32_t x = rng_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    rng_state = x;
    return x;
}

int maze_cell_index(int x, int y) { return y * MAP_MAX + x; }
uint8_t maze_get(int x, int y) {
    if((unsigned)x >= (unsigned)g.map_w || (unsigned)y >= (unsigned)g.map_h) return WALL_BRICK;
    return g.map[(uint16_t)y * MAP_MAX + x];
}
void maze_set(int x, int y, uint8_t v) {
    if((unsigned)x >= (unsigned)g.map_w || (unsigned)y >= (unsigned)g.map_h) return;
    g.map[(uint16_t)y * MAP_MAX + x] = v;
}

static void carve(int w, int h) {
    for(int y = 0; y < h; y++)
        for(int x = 0; x < w; x++)
            g.map[(uint16_t)y * MAP_MAX + x] = WALL_BRICK;

    // NOTE: sx/sy 放在静态存储(BSS)而非栈上.
    // 之前是 int sx[961], sy[961] 局部数组 = 7688 字节, 而应用栈只有 8KB,
    // 叠加 maze_generate/game_init_*/engine_render 的栈帧后直接栈溢出崩溃
    // (无尽模式每次进下一关都触发, "一玩就死机"的真正根因).
    // 单线程应用使用 static 安全.
    static int sx[MAP_MAX * MAP_MAX], sy[MAP_MAX * MAP_MAX];
    int top = 0;
    sx[top] = 1; sy[top] = 1; top++;
    g.map[1 * MAP_MAX + 1] = CELL_EMPTY;

    static const int dx[4] = {2, -2, 0, 0};
    static const int dy[4] = {0, 0, 2, -2};

    while(top > 0) {
        int cx = sx[top - 1], cy = sy[top - 1];
        int order[4] = {0, 1, 2, 3};
        for(int i = 3; i > 0; i--) {
            int j = maze_rng_next() % (i + 1);
            int t = order[i]; order[i] = order[j]; order[j] = t;
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
                sx[top] = nx; sy[top] = ny; top++;
                moved = 1;
                break;
            }
        }
        if(!moved) top--;
    }
}

static void add_loops(int w, int h, int count) {
    for(int i = 0; i < count; i++) {
        int x = 1 + maze_rng_next() % (w - 2);
        int y = 1 + maze_rng_next() % (h - 2);
        if(g.map[(uint16_t)y * MAP_MAX + x] == WALL_BRICK) {
            int horiz = (g.map[(uint16_t)y * MAP_MAX + x - 1] == CELL_EMPTY && g.map[(uint16_t)y * MAP_MAX + x + 1] == CELL_EMPTY);
            int vert  = (g.map[(uint16_t)(y - 1) * MAP_MAX + x] == CELL_EMPTY && g.map[(uint16_t)(y + 1) * MAP_MAX + x] == CELL_EMPTY);
            if(horiz || vert) g.map[(uint16_t)y * MAP_MAX + x] = CELL_EMPTY;
        }
    }
}

static void find_far(int w, int h, int sxx, int syy, int* ox, int* oy) {
    *ox = sxx; *oy = syy;
    int best = -1;
    for(int y = 1; y < h - 1; y++) {
        for(int x = 1; x < w - 1; x++) {
            if(g.map[(uint16_t)y * MAP_MAX + x] == CELL_EMPTY) {
                int d = abs(x - sxx) + abs(y - syy);
                if(d > best) { best = d; *ox = x; *oy = y; }
            }
        }
    }
}

static void update_wall_textures(int w, int h, int level) {
    // v6.0: 随着关卡切换主贴图 (砖/石/金属/藤蔓 循环, 不动 MC 方块)
    int base = (level / 5) % 4;
    // 给每个墙格按坐标决定纹理变体(用位置哈希)
    for(int y = 0; y < h; y++) {
        for(int x = 0; x < w; x++) {
            if(g.map[(uint16_t)y * MAP_MAX + x] == WALL_BRICK) {
                int hh = (x * 73856093u ^ y * 19349663u) & 7;
                switch(base) {
                    case 0: g.map[(uint16_t)y * MAP_MAX + x] = (hh < 6) ? WALL_BRICK : WALL_STONE; break;
                    case 1: g.map[(uint16_t)y * MAP_MAX + x] = (hh < 5) ? WALL_STONE : WALL_METAL; break;
                    case 2: g.map[(uint16_t)y * MAP_MAX + x] = (hh < 5) ? WALL_METAL : WALL_VINE; break;
                    case 3: g.map[(uint16_t)y * MAP_MAX + x] = (hh < 4) ? WALL_VINE : WALL_BRICK; break;
                }
            }
        }
    }
}

void maze_generate(int w, int h, int level, unsigned int seed) {
    if(w > MAP_MAX) w = MAP_MAX;
    if(h > MAP_MAX) h = MAP_MAX;
    if(w % 2 == 0) w--;
    if(h % 2 == 0) h--;
    if(w < 7) w = 7;
    if(h < 7) h = 7;
    g.map_w = w; g.map_h = h;

    rng_seed(seed + level * 2654435761u);
    carve(w, h);

    int loops = 22 - (level > 20 ? 17 : level);
    if(loops < 2) loops = 2;
    add_loops(w, h, loops);

    // 出口(放在最远处
    int ex, ey;
    find_far(w, h, 1, 1, &ex, &ey);
    g.map[(uint16_t)ey * MAP_MAX + ex] = CELL_EXIT;
    g.exit_x = ex; g.exit_y = ey; g.exit_found = true;

    int stage = 0;
    if(level >= 21) stage = 2;       // v6.0: 21+ 战斗
    else if(level >= 11) stage = 1;  // v6.0: 11-20 解谜

    if(stage >= 1) {
        // v6.0: 解谜关 — 出口前必须有一扇门挡住 (任务完成前出口锁定)
        // 在出口格放 CELL_LOCKED_EXIT, 旁边放 CELL_DOOR (需钥匙)
        // 任务完成后 game 逻辑会把它当 CELL_EXIT 处理 (quest.all_done 解锁)
        g.map[(uint16_t)ey * MAP_MAX + ex] = CELL_LOCKED_EXIT;
        // 钥匙数递进: 11关1把, 每3关+1, 上限3
        int keys = 1 + (level - 11) / 3;
        if(keys > 3) keys = 3;
        int placed = 0, tries = 0;
        while(placed < keys && tries++ < 300) {
            int x = 1 + maze_rng_next() % (w - 2);
            int y = 1 + maze_rng_next() % (h - 2);
            uint8_t c = g.map[(uint16_t)y * MAP_MAX + x];
            if(c == CELL_EMPTY && !(x == 1 && y == 1) && !(x == ex && y == ey)) {
                g.map[(uint16_t)y * MAP_MAX + x] = CELL_KEY;
                placed++;
            }
        }
        // 门: 放在通往出口的走廊上 (简化: 在出口邻格放门)
        {
            int dxs[4] = {1,-1,0,0}, dys[4] = {0,0,1,-1};
            for(int k = 0; k < 4; k++) {
                int nx = ex + dxs[k], ny = ey + dys[k];
                if(nx > 0 && ny > 0 && nx < w-1 && ny < h-1 &&
                   g.map[(uint16_t)ny * MAP_MAX + nx] == CELL_EMPTY) {
                    g.map[(uint16_t)ny * MAP_MAX + nx] = CELL_DOOR;
                    break;
                }
            }
        }
        int torches = 2 + level / 5;
        placed = 0; tries = 0;
        while(placed < torches && tries++ < 300) {
            int x = 1 + maze_rng_next() % (w - 2);
            int y = 1 + maze_rng_next() % (h - 2);
            if(g.map[(uint16_t)y * MAP_MAX + x] == CELL_EMPTY) {
                g.map[(uint16_t)y * MAP_MAX + x] = CELL_TORCH;
                placed++;
            }
        }
        // 药水: 解谜关开始出现
        int potions = 1 + (level - 11) / 4;
        if(potions > 3) potions = 3;
        placed = 0; tries = 0;
        while(placed < potions && tries++ < 300) {
            int x = 1 + maze_rng_next() % (w - 2);
            int y = 1 + maze_rng_next() % (h - 2);
            if(g.map[(uint16_t)y * MAP_MAX + x] == CELL_EMPTY && !(x == 1 && y == 1)) {
                g.map[(uint16_t)y * MAP_MAX + x] = CELL_POTION;
                placed++;
            }
        }
    }
    if(stage >= 2) {
        // v6.0: 战斗关 — 出口锁定 (全歼敌人后解锁), 放陷阱和护符
        g.map[(uint16_t)ey * MAP_MAX + ex] = CELL_LOCKED_EXIT;
        int traps = 2 + (level - 21) / 3;
        if(traps > 8) traps = 8;
        int placed = 0, tries = 0;
        while(placed < traps && tries++ < 300) {
            int x = 1 + maze_rng_next() % (w - 2);
            int y = 1 + maze_rng_next() % (h - 2);
            if(g.map[(uint16_t)y * MAP_MAX + x] == CELL_EMPTY && (abs(x - 1) + abs(y - 1) > 3)) {
                g.map[(uint16_t)y * MAP_MAX + x] = CELL_TRAP;
                placed++;
            }
        }
        int amulets = 1 + (level - 21) / 8;
        if(amulets > 2) amulets = 2;
        placed = 0; tries = 0;
        while(placed < amulets && tries++ < 300) {
            int x = 1 + maze_rng_next() % (w - 2);
            int y = 1 + maze_rng_next() % (h - 2);
            if(g.map[(uint16_t)y * MAP_MAX + x] == CELL_EMPTY && (abs(x - 1) + abs(y - 1) > 4)) {
                g.map[(uint16_t)y * MAP_MAX + x] = CELL_AMULET;
                placed++;
            }
        }
    }

    update_wall_textures(w, h, level);
}
