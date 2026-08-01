#include "maze3d.h"
#include <math.h>
#include <string.h>

extern uint32_t maze_rng_next(void);

// ---- 工具 ----
static void set_message(const char* msg) {
    strncpy(g.message, msg, sizeof(g.message) - 1);
    g.message[sizeof(g.message) - 1] = 0;
    g.message_ttl = 90; // 约1.5秒(60fps)
}

static int level_stage(int level) {
    if(level >= 20) return STAGE_COMBAT;
    if(level >= 10) return STAGE_PUZZLE;
    return STAGE_MAZE_ONLY;
}

// 判断格子可通行
static bool walkable(uint8_t c) {
    return c == CELL_EMPTY || c == CELL_KEY || c == CELL_EXIT ||
           c == CELL_TORCH || c == CELL_TRAP;
}
static bool blocking(uint8_t c) {
    return c == WALL_BRICK || c == WALL_STONE || c == WALL_METAL ||
           c == WALL_VINE || c == CELL_DOOR;
}

// ---- 玩家朝向初始化 ----
static void set_dir(float angle) {
    g.player.dir_x = cosf(angle);
    g.player.dir_y = sinf(angle);
    // 摄像机平面 = 朝向旋转90度 * tan(FOV/2), FOV~66度 => 0.66
    g.player.plane_x = -g.player.dir_y * 0.66f;
    g.player.plane_y = g.player.dir_x * 0.66f;
}

// ---- 碰撞移动 ----
bool player_move(float dx, float dy) {
    float nx = g.player.x + dx;
    float ny = g.player.y + dy;
    float pad = 0.2f;
    // X 方向
    int mx = (int)(nx + (dx > 0 ? pad : -pad));
    int my = (int)g.player.y;
    if(!blocking(maze_get(mx, my))) g.player.x = nx;
    // Y 方向
    mx = (int)g.player.x;
    my = (int)(ny + (dy > 0 ? pad : -pad));
    if(!blocking(maze_get(mx, my))) g.player.y = ny;

    // 拾取/触发当前格
    int cx = (int)g.player.x, cy = (int)g.player.y;
    uint8_t here = maze_get(cx, cy);
    if(here == CELL_KEY) {
        g.player.keys++;
        maze_set(cx, cy, CELL_EMPTY);
        set_message("Got Key!");
    } else if(here == CELL_TORCH) {
        g.player.torches++;
        maze_set(cx, cy, CELL_EMPTY);
        set_message("Torch +1");
    } else if(here == CELL_TRAP) {
        if(g.stage == STAGE_COMBAT && g.player.health > 0) {
            g.player.health -= 1;
            set_message("Trap! -1 HP");
        }
        maze_set(cx, cy, CELL_EMPTY);
    } else if(here == CELL_DOOR) {
        // 门: 需要钥匙
        if(g.player.keys > 0) {
            g.player.keys--;
            maze_set(cx, cy, CELL_EMPTY);
            set_message("Door opened");
        } else {
            set_message("Need Key");
            // 退回
            g.player.x -= dx; g.player.y -= dy;
            return false;
        }
    } else if(here == CELL_EXIT) {
        // 到达出口 -> 通关
        if(g.mode == MODE_CAMPAIGN) {
            g.mode = MODE_LEVEL_CLEAR;
            if(g.level > g.campaign_cleared) g.campaign_cleared = g.level;
            storage_save();
        } else if(g.mode == MODE_ENDLESS_RUN) {
            g.endless_floor++;
            storage_save();
            game_next_level();
        } else if(g.mode == MODE_ENDLESS_VISITOR) {
            set_message("Visitor: no exit goal");
        }
    }
    return true;
}

void player_rotate(float angle) {
    float cs = cosf(angle), sn = sinf(angle);
    float ndx = g.player.dir_x * cs - g.player.dir_y * sn;
    float ndy = g.player.dir_x * sn + g.player.dir_y * cs;
    g.player.dir_x = ndx; g.player.dir_y = ndy;
    float npx = g.player.plane_x * cs - g.player.plane_y * sn;
    float npy = g.player.plane_x * sn + g.player.plane_y * cs;
    g.player.plane_x = npx; g.player.plane_y = npy;
}

// ---- 敌人/NPC ----
void spawn_actor(float x, float y, int type) {
    if(g.actor_count >= MAX_ACTORS) return;
    Actor* a = &g.actors[g.actor_count++];
    a->x = x; a->y = y;
    a->dir_x = 1; a->dir_y = 0;
    a->active = true;
    a->type = type;
    a->cooldown = 0;
}

void actors_update(void) {
    for(int i = 0; i < g.actor_count; i++) {
        Actor* a = &g.actors[i];
        if(!a->active) continue;
        if(a->cooldown > 0) { a->cooldown--; continue; }
        a->cooldown = (a->type == 0) ? 18 : 30; // 敌人较快, NPC较慢

        // 简单AI: 随机游走, 不穿墙
        float dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
        int r = maze_rng_next() & 3;
        for(int k = 0; k < 4; k++) {
            int idx = (r + k) & 3;
            float nx = a->x + dirs[idx][0];
            float ny = a->y + dirs[idx][1];
            if(walkable(maze_get((int)nx, (int)ny))) {
                a->x = nx; a->y = ny;
                a->dir_x = dirs[idx][0]; a->dir_y = dirs[idx][1];
                break;
            }
        }
        // 敌人: 若靠近玩家则攻击
        if(a->type == 0) {
            float ddx = a->x - g.player.x, ddy = a->y - g.player.y;
            if(ddx * ddx + ddy * ddy < 0.6f && g.player.health > 0) {
                g.player.health -= 1;
                set_message("Hit! -1 HP");
                if(g.player.health <= 0) {
                    g.mode = MODE_GAME_OVER;
                }
            }
        }
    }
}

// ---- 关卡初始化 ----
static void place_player_and_actors(int level, bool visitor) {
    g.player.x = 1.5f; g.player.y = 1.5f;
    set_dir(0.0f); // 朝东
    g.player.keys = 0;
    g.player.torches = 0;
    g.player.health = (g.stage >= STAGE_COMBAT) ? 5 : 1;
    g.actor_count = 0;

    // 关卡模式20+关: 放敌人
    if(g.mode == MODE_CAMPAIGN && g.stage == STAGE_COMBAT) {
        int enemies = 1 + (level - 20) / 4;
        if(enemies > 4) enemies = 4;
        int placed = 0, tries = 0;
        while(placed < enemies && tries++ < 200) {
            int x = 2 + maze_rng_next() % (g.map_w - 4);
            int y = 2 + maze_rng_next() % (g.map_h - 4);
            if(walkable(maze_get(x, y)) && (abs(x - 1) + abs(y - 1) > 5)) {
                spawn_actor(x + 0.5f, y + 0.5f, 0);
                placed++;
            }
        }
    }
    // 无尽游客模式: 放 NPC
    if(visitor) {
        int npcs = 3;
        for(int i = 0; i < npcs; i++) {
            int tries = 0;
            while(tries++ < 100) {
                int x = 2 + maze_rng_next() % (g.map_w - 4);
                int y = 2 + maze_rng_next() % (g.map_h - 4);
                if(walkable(maze_get(x, y))) {
                    spawn_actor(x + 0.5f, y + 0.5f, 1);
                    break;
                }
            }
        }
    }
}

void game_init_campaign(int level) {
    g.mode = MODE_CAMPAIGN;
    g.level = level;
    g.stage = level_stage(level);
    g.has_exit = true;
    // 尺寸随关卡增长: 7 -> 31
    int sz = 7 + level;
    if(sz > 31) sz = 31;
    maze_generate(sz, sz, level, 0xABCDEF01u);
    place_player_and_actors(level, false);
    set_message(g.stage == STAGE_COMBAT ? "Beware enemies!" :
                g.stage == STAGE_PUZZLE ? "Find keys & exit" : "Find the exit");
}

void game_init_endless(int floor, bool visitor) {
    if(visitor) {
        g.mode = MODE_ENDLESS_VISITOR;
    } else {
        g.mode = MODE_ENDLESS_RUN;
    }
    g.level = floor;
    g.endless_floor = floor;
    g.stage = STAGE_MAZE_ONLY;
    g.has_exit = !visitor; // 游客模式无出口目标
    // 尺寸随层数增长
    int sz = 9 + floor;
    if(sz > 31) sz = 31;
    // 每层不同贴图主调: 通过关卡号传入让迷宫生成变化
    maze_generate(sz, sz, floor + 100, 0x12345678u + floor * 31u);
    place_player_and_actors(floor + 100, visitor);
    set_message(visitor ? "Visitor mode" : "Endless run");
}

void game_next_level(void) {
    if(g.mode == MODE_CAMPAIGN) {
        game_init_campaign(g.level + 1);
    } else if(g.mode == MODE_ENDLESS_RUN) {
        game_init_endless(g.endless_floor, false);
    }
}

// ---- 输入 ----
void game_handle_input(InputKey key, InputType type) {
    if(type != InputTypeShort && type != InputTypeRepeat) return;

    if(g.mode == MODE_PAUSED) {
        if(key == InputKeyBack) { g.mode = MODE_CAMPAIGN; } // 简化: 恢复(用菜单状态会更严谨, 此处保留)
        return;
    }
    if(g.mode != MODE_CAMPAIGN && g.mode != MODE_ENDLESS_RUN &&
       g.mode != MODE_ENDLESS_VISITOR) return;

    float speed = 0.25f;
    float turn = 0.26f; // 约15度
    switch(key) {
        case InputKeyUp: {
            float dx = g.player.dir_x * speed, dy = g.player.dir_y * speed;
            player_move(dx, dy);
            break;
        }
        case InputKeyDown: {
            float dx = -g.player.dir_x * speed, dy = -g.player.dir_y * speed;
            player_move(dx, dy);
            break;
        }
        case InputKeyLeft:
            player_rotate(-turn);
            break;
        case InputKeyRight:
            player_rotate(turn);
            break;
        case InputKeyOk:
            // 确认键: 互动(开门已在移动里处理)/ 提示
            set_message("OK - move to exit");
            break;
        default: break;
    }
    g.need_redraw = true;
}

// ---- 每帧更新 ----
void game_update(void) {
    if(g.mode == MODE_CAMPAIGN || g.mode == MODE_ENDLESS_RUN ||
       g.mode == MODE_ENDLESS_VISITOR) {
        actors_update();
    }
    if(g.message_ttl > 0) g.message_ttl--;
    g.need_redraw = true;
}
