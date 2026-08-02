#include "maze3d.h"
#include <math.h>
#include <string.h>

void set_msg(int id) {
    if(id >= 0) { g.msg_id = id; g.msg_ttl = 100; }
    else { g.msg_id = MSG_NONE; g.msg_ttl = 0; }
}

static int level_stage(int level) {
    if(level >= 20) return STAGE_COMBAT;
    if(level >= 10) return STAGE_PUZZLE;
    return STAGE_MAZE_ONLY;
}

static bool walkable(uint8_t c) {
    return c == CELL_EMPTY || c == CELL_KEY || c == CELL_EXIT ||
           c == CELL_TORCH || c == CELL_TRAP ||
           c == CELL_POTION || c == CELL_AMULET;
}
static bool blocking(uint8_t c) {
    return c == WALL_BRICK || c == WALL_STONE || c == WALL_METAL ||
           c == WALL_VINE || c == CELL_DOOR;
}

static void set_dir(float angle) {
    g.player.dir_x = cosf(angle);
    g.player.dir_y = sinf(angle);
    g.player.plane_x = -g.player.dir_y * 0.66f;
    g.player.plane_y =  g.player.dir_x * 0.66f;
}

bool player_move(float dx, float dy) {
    float nx = g.player.x + dx;
    float ny = g.player.y + dy;
    const float pad = 0.2f;
    int mx = (int)(nx + (dx > 0 ? pad : -pad));
    int my = (int)g.player.y;
    if(!blocking(maze_get(mx, my))) g.player.x = nx;
    mx = (int)g.player.x;
    my = (int)(ny + (dy > 0 ? pad : -pad));
    if(!blocking(maze_get(mx, my))) g.player.y = ny;

    int cx = (int)g.player.x, cy = (int)g.player.y;
    uint8_t here = maze_get(cx, cy);
    if(here == CELL_KEY) {
        g.player.keys++;
        maze_set(cx, cy, CELL_EMPTY);
        set_msg(MSG_KEY);
    } else if(here == CELL_TORCH) {
        g.player.torches++;
        maze_set(cx, cy, CELL_EMPTY);
        set_msg(MSG_TORCH);
    } else if(here == CELL_POTION) {
        g.player.potions++;
        maze_set(cx, cy, CELL_EMPTY);
        set_msg(MSG_TORCH); // 复用"获得"提示
    } else if(here == CELL_AMULET) {
        g.player.amulets++;
        maze_set(cx, cy, CELL_EMPTY);
        set_msg(MSG_KEY);
    } else if(here == CELL_TRAP) {
        if(g.stage == STAGE_COMBAT && g.player.health > 0) {
            g.player.health -= 1;
            set_msg(MSG_TRAP);
        }
        maze_set(cx, cy, CELL_EMPTY);
    } else if(here == CELL_DOOR) {
        if(g.player.keys > 0) {
            g.player.keys--;
            maze_set(cx, cy, CELL_EMPTY);
            set_msg(MSG_DOOR);
        } else {
            set_msg(MSG_NEEDKEY);
            g.player.x -= dx; g.player.y -= dy;
            return false;
        }
    } else if(here == CELL_EXIT) {
        if(g.mode == MODE_CAMPAIGN) {
            g.mode = MODE_LEVEL_CLEAR;
            if(g.level > g.campaign_cleared) g.campaign_cleared = g.level;
            storage_save();
        } else if(g.mode == MODE_ENDLESS_RUN) {
            g.endless_floor++;
            storage_save();
            game_next_level();
        } else if(g.mode == MODE_ENDLESS_VISITOR) {
            set_msg(MSG_EXIT);
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

void spawn_actor(float x, float y, int type) {
    if(g.actor_count >= MAX_ACTORS) return;
    Actor* a = &g.actors[g.actor_count++];
    a->x = x; a->y = y; a->active = true; a->type = (uint8_t)type; a->cooldown = 0;
}

void actors_update(void) {
    for(int i = 0; i < g.actor_count; i++) {
        Actor* a = &g.actors[i];
        if(!a->active) continue;
        if(a->cooldown > 0) { a->cooldown--; continue; }
        a->cooldown = (a->type == 0) ? (uint8_t)22 : (uint8_t)34;
        float dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
        int r = maze_rng_next() & 3;
        for(int k = 0; k < 4; k++) {
            int idx = (r + k) & 3;
            float nx = a->x + dirs[idx][0];
            float ny = a->y + dirs[idx][1];
            if(walkable(maze_get((int)nx, (int)ny))) {
                a->x = nx; a->y = ny;
                break;
            }
        }
        if(a->type == 0) {
            float ddx = a->x - g.player.x, ddy = a->y - g.player.y;
            if(ddx*ddx + ddy*ddy < 0.6f && g.player.health > 0) {
                g.player.health -= 1;
                set_msg(MSG_HIT);
                if(g.player.health <= 0) g.mode = MODE_GAME_OVER;
            }
        }
    }
}

static void place_player_and_actors(int level, bool visitor) {
    g.player.x = 1.5f; g.player.y = 1.5f;
    set_dir(0.0f);
    g.player.keys = 0;
    g.player.torches = 0;
    g.player.potions = 0;
    g.player.amulets = 0;
    g.actor_count = 0;

    // 起始 HP/物品: 剧情模式由开场选择决定; 无尽/游客固定
    if(g.mode == MODE_CAMPAIGN) {
        if(g.story_choice == 0) {        // A) Warrior
            g.player.max_health = 7;
            g.player.health = 7;
        } else if(g.story_choice == 1) { // B) Seeker
            g.player.max_health = 4;
            g.player.health = 4;
            g.player.torches = 1;
        } else {                          // 未选(直接进高层级重玩)
            g.player.max_health = 5;
            g.player.health = 5;
        }
    } else {
        g.player.max_health = 5;
        g.player.health = 5;
    }

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
    g.exit_found = true;
    g.tick = 0;
    g.show_hud = false;
    int sz = 7 + level;
    if(sz > 23) sz = 23;     // 上限保护, 防止大迷宫卡顿
    maze_generate(sz, sz, level, 0xABCDEF01u);
    place_player_and_actors(level, false);
    if(g.stage == STAGE_COMBAT) set_msg(MSG_CARE);
    else if(g.stage == STAGE_PUZZLE) set_msg(MSG_PUZZLE);
    else set_msg(MSG_FINDEXIT);
    g.dirty = true;
}

void game_init_endless(int floor, bool visitor) {
    g.mode = visitor ? MODE_ENDLESS_VISITOR : MODE_ENDLESS_RUN;
    g.level = floor;
    g.endless_floor = floor;
    g.stage = STAGE_MAZE_ONLY;
    g.has_exit = true;
    g.exit_found = true;     // 始终显示出口罗盘, 避免渲染异常
    g.tick = 0;
    g.show_hud = false;
    int sz = 9 + (floor > 12 ? 12 : floor);
    if(sz > 21) sz = 21;     // 严格上限, 防卡死
    maze_generate(sz, sz, floor + 100, 0x12345678u + (unsigned)floor * 31u);
    place_player_and_actors(floor + 100, visitor);
    set_msg(visitor ? MSG_VISITOR : MSG_RUN);
    g.dirty = true;
}

void game_next_level(void) {
    if(g.mode == MODE_CAMPAIGN) game_init_campaign(g.level + 1);
    else if(g.mode == MODE_ENDLESS_RUN) game_init_endless(g.endless_floor, false);
}

// ---- 物品栏 ----
int item_count(int item_type) {
    switch(item_type) {
        case ITEM_KEY:    return g.player.keys;
        case ITEM_TORCH:  return g.player.torches;
        case ITEM_POTION: return g.player.potions;
        case ITEM_AMULET: return g.player.amulets;
        default: return 0;
    }
}

bool item_use(int item_type) {
    switch(item_type) {
        case ITEM_KEY:
            // 钥匙不能主动使用, 自动开门
            return false;
        case ITEM_TORCH:
            if(g.player.torches > 0) {
                g.player.torches--;
                set_msg(MSG_TORCH);
                return true;
            }
            return false;
        case ITEM_POTION:
            if(g.player.potions > 0 && g.player.health < g.player.max_health) {
                g.player.potions--;
                g.player.health = g.player.max_health;
                set_msg(MSG_KEY); // 复用提示
                return true;
            }
            return false;
        case ITEM_AMULET:
            if(g.player.amulets > 0) {
                g.player.amulets--;
                g.player.x = 1.5f; g.player.y = 1.5f;
                set_msg(MSG_EXIT);
                return true;
            }
            return false;
        default:
            return false;
    }
}

void game_handle_input(InputKey key, InputType type) {
    if(type != InputTypeShort && type != InputTypeRepeat) return;

    float speed  = 0.28f;
    float slow   = 0.18f;
    float dash   = 0.45f;
    float turn   = 0.29f;

    switch(key) {
        case InputKeyUp:
            player_move(g.player.dir_x * speed, g.player.dir_y * speed);
            break;
        case InputKeyDown:
            player_move(-g.player.dir_x * slow, -g.player.dir_y * slow);
            break;
        case InputKeyLeft:
            player_rotate(-turn);
            break;
        case InputKeyRight:
            player_rotate(turn);
            break;
        case InputKeyOk:
            // 确认键: 前冲一步
            player_move(g.player.dir_x * dash, g.player.dir_y * dash);
            break;
        default: break;
    }
    g.dirty = true;
}

void game_update(void) {
    g.tick++;
    actors_update();
    if(g.msg_ttl > 0) {
        g.msg_ttl--;
        if(g.msg_ttl == 0) g.msg_id = MSG_NONE;
    }
    // 每帧都dirty: 有闪烁和敌人移动
    g.dirty = true;
}
