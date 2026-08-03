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

// ---- 任务系统 ----
// 根据关卡设置任务 (只有"有剧情"的关卡才有任务)
//   level 1 (序章): 找出口
//   level 10-19 (解谜): 拿钥匙 + 开门
//   level 20+  (战斗): 消灭所有敌人
//   其他关卡: 无任务
static int combat_enemy_count(int level) {
    int n = 1 + (level - 20) / 4;
    return n > 4 ? 4 : n;
}

static void quest_init_for_level(int level) {
    g.quest.active = false;
    g.quest.sub_count = 0;
    g.quest.all_done = false;
    g.quest.reward_given = false;
    g.task_kill_count = 0;
    g.task_open_door = 0;
    g.task_get_key = 0;
    g.task_survive_secs = 0;
    for(int i = 0; i < MAX_SUBTASKS; i++) {
        g.quest.subs[i].type = TASK_NONE;
        g.quest.subs[i].target = 0;
        g.quest.subs[i].progress = 0;
        g.quest.subs[i].done = false;
    }

    int stg = level_stage(level);
    if(stg == STAGE_MAZE_ONLY) {
        // 普通剧情关: 找到出口
        g.quest.active = true;
        g.quest.sub_count = 1;
        g.quest.subs[0].type = TASK_FIND_EXIT;
        g.quest.subs[0].target = 1;
    } else if(stg == STAGE_PUZZLE) {
        // 解谜关: 拿钥匙 + 开门
        g.quest.active = true;
        g.quest.sub_count = 2;
        g.quest.subs[0].type = TASK_GET_KEY;
        g.quest.subs[0].target = 1;
        g.quest.subs[1].type = TASK_OPEN_DOOR;
        g.quest.subs[1].target = 1;
    } else if(stg == STAGE_COMBAT) {
        // 战斗关: 消灭所有敌人
        g.quest.active = true;
        g.quest.sub_count = 1;
        g.quest.subs[0].type = TASK_KILL_ENEMY;
        g.quest.subs[0].target = combat_enemy_count(level);
    }
}

// 更新任务进度 + 检测完成 + 发放奖励
static void quest_update(void) {
    if(!g.quest.active || g.quest.all_done) return;
    for(int i = 0; i < g.quest.sub_count; i++) {
        SubTask* s = &g.quest.subs[i];
        if(s->done) continue;
        switch(s->type) {
            case TASK_GET_KEY:
                s->progress = g.task_get_key;
                break;
            case TASK_OPEN_DOOR:
                s->progress = g.task_open_door;
                break;
            case TASK_KILL_ENEMY:
                s->progress = g.task_kill_count;
                break;
            case TASK_FIND_EXIT:
                // 由 player_move 命中 CELL_EXIT 时直接置 done
                break;
            case TASK_SURVIVE:
                s->progress = g.task_survive_secs;
                break;
            default: break;
        }
        if(s->target > 0 && s->progress >= s->target) s->done = true;
    }
    // 是否全部完成
    bool all = true;
    for(int i = 0; i < g.quest.sub_count; i++) {
        if(!g.quest.subs[i].done) { all = false; break; }
    }
    if(all && !g.quest.all_done) {
        g.quest.all_done = true;
        // 奖励: 回满血 (一次性)
        if(!g.quest.reward_given) {
            g.quest.reward_given = true;
            g.player.health = g.player.max_health;
            sfx_play(SFX_QUEST_DONE);
            set_msg(MSG_QUESTDONE);  // 任务完成 toast
        }
    }
}

// 玩家冲刺(OK键)时攻击前方敌人: 命中则敌人扣血, 不前进
// 返回 true 表示命中了敌人 (应取消本次冲刺移动)
static bool player_attack(void) {
    if(g.actor_count == 0) return false;
    float px = g.player.x, py = g.player.y;
    float dx = g.player.dir_x, dy = g.player.dir_y;
    for(int i = 0; i < g.actor_count; i++) {
        Actor* a = &g.actors[i];
        if(!a->active || a->type != 0 || a->hp == 0) continue;
        float ex = a->x - px, ey = a->y - py;
        // 前方距离 (沿朝向投影)
        float fwd = ex * dx + ey * dy;
        if(fwd < 0.2f || fwd > 1.2f) continue;
        // 横向偏移 (垂直于朝向)
        float side = fabsf(ex * (-dy) + ey * dx);
        if(side > 0.6f) continue;
        // 命中
        if(a->hp > 0) a->hp--;
        set_msg(MSG_HIT);
        sfx_play(SFX_ATTACK_HIT);
        if(a->hp == 0) {
            a->active = false;
            g.task_kill_count++;
            sfx_play(SFX_ENEMY_KILL);
        }
        return true;
    }
    return false;
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
        g.task_get_key++;
        maze_set(cx, cy, CELL_EMPTY);
        set_msg(MSG_KEY);
        sfx_play(SFX_PICK_KEY);
    } else if(here == CELL_TORCH) {
        g.player.torches++;
        maze_set(cx, cy, CELL_EMPTY);
        set_msg(MSG_TORCH);
        sfx_play(SFX_PICK_ITEM);
    } else if(here == CELL_POTION) {
        g.player.potions++;
        maze_set(cx, cy, CELL_EMPTY);
        set_msg(MSG_POTION);
        sfx_play(SFX_PICK_ITEM);
    } else if(here == CELL_AMULET) {
        g.player.amulets++;
        maze_set(cx, cy, CELL_EMPTY);
        set_msg(MSG_AMULET);
        sfx_play(SFX_PICK_ITEM);
    } else if(here == CELL_TRAP) {
        if(g.stage == STAGE_COMBAT && g.player.health > 0) {
            g.player.health -= 1;
            set_msg(MSG_TRAP);
            sfx_play(SFX_TRAP);
            if(g.player.health <= 0) { g.mode = MODE_GAME_OVER; sfx_play(SFX_GAME_OVER); }
        }
        maze_set(cx, cy, CELL_EMPTY);
    } else if(here == CELL_DOOR) {
        if(g.player.keys > 0) {
            g.player.keys--;
            g.task_open_door = 1;
            maze_set(cx, cy, CELL_EMPTY);
            set_msg(MSG_DOOR);
            sfx_play(SFX_OPEN_DOOR);
        } else {
            set_msg(MSG_NEEDKEY);
            sfx_play(SFX_NEED_KEY);
            g.player.x -= dx; g.player.y -= dy;
            return false;
        }
    } else if(here == CELL_EXIT) {
        // 命中出口: 标记 FIND_EXIT 子任务完成, 并即时结算奖励
        // (mode 即将变为 LEVEL_CLEAR, game_update 不再运行, 故此处直接结算)
        if(g.quest.active) {
            for(int i = 0; i < g.quest.sub_count; i++) {
                if(g.quest.subs[i].type == TASK_FIND_EXIT) {
                    g.quest.subs[i].progress = 1;
                    g.quest.subs[i].done = true;
                }
            }
            quest_update();
        }
        if(g.mode == MODE_CAMPAIGN) {
            g.mode = MODE_LEVEL_CLEAR;
            if(g.level > g.campaign_cleared) g.campaign_cleared = g.level;
            storage_save();
            sfx_play(SFX_LEVEL_CLEAR);
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
    // 敌人血量 2 (需冲刺两次击杀); NPC 游客无血量
    a->hp = (type == 0) ? 2 : 0;
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
                sfx_play(SFX_DAMAGE);
                if(g.player.health <= 0) { g.mode = MODE_GAME_OVER; sfx_play(SFX_GAME_OVER); }
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
    g.turn_target = 0.0f;
    g.move_fwd_target = 0.0f;
    g.move_bwd_target = 0.0f;
    g.move_dash_target = 0.0f;
    int sz = 7 + level;
    if(sz > 23) sz = 23;     // 上限保护, 防止大迷宫卡顿
    maze_generate(sz, sz, level, 0xABCDEF01u);
    place_player_and_actors(level, false);
    quest_init_for_level(level);
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
    g.exit_found = true;
    g.tick = 0;
    g.show_hud = false;
    // 清零平滑插值目标(避免旧累积值)
    g.turn_target = 0.0f;
    g.move_fwd_target = 0.0f;
    g.move_bwd_target = 0.0f;
    g.move_dash_target = 0.0f;
    // 尺寸严格上限 19 (19x19=361 格, DDA 步数最多 19 列 × 64 = 1216 次循环)
    int sz = 9 + (floor > 10 ? 10 : floor);
    if(sz > 19) sz = 19;
    if(sz < 9)  sz = 9;
    // 用 floor 做种子散列 (避免连续 floor 生成相似地图)
    unsigned seed = 0x12345678u ^ (unsigned)floor * 2654435761u ^ (unsigned)floor * 1013904242u;
    maze_generate(sz, sz, floor + 100, seed);
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

    // 平滑移动: 输入只设置"目标"值,game_update 每帧逐步插值施加,
    // 从而达到"转角和移动更加平滑"的效果
    const float speed  = 0.28f;
    const float slow   = 0.18f;
    const float dash   = 0.42f;
    const float turn   = 0.26f;

    switch(key) {
        case InputKeyUp:
            g.move_fwd_target += speed;
            break;
        case InputKeyDown:
            g.move_bwd_target += slow;
            break;
        case InputKeyLeft:
            g.turn_target -= turn;
            break;
        case InputKeyRight:
            g.turn_target += turn;
            break;
        case InputKeyOk:
            g.move_dash_target += dash;
            break;
        default: break;
    }
    g.dirty = true;
}

void game_update(void) {
    // 平滑插值: 每帧施加 turn_target 的 40% (剩余 60% 累积到下帧),
    // 这样"按左右键"不会立刻转一个大角度,而是分几帧平滑转到目标朝向.
    // 如果 turn_target 过大(连续按多次),就每帧 40% 分步转.
    if(g.turn_target != 0.0f) {
        float step = g.turn_target * 0.45f;
        if(fabsf(step) < 0.01f) { step = g.turn_target; g.turn_target = 0.0f; }
        else                    { g.turn_target -= step; }
        player_rotate(step);
    }

    // 平滑移动: 每帧施加 move_target 的一部分, 与插值转弯配合
    if(g.move_dash_target != 0.0f) {
        // 冲刺(OK键): 若前方有敌人则攻击, 命中则取消本次冲刺
        if(g.stage == STAGE_COMBAT && player_attack()) {
            // 命中敌人, 不移动
        } else {
            float s = g.move_dash_target;
            player_move(g.player.dir_x * s, g.player.dir_y * s);
        }
        g.move_dash_target = 0.0f;
    }
    if(g.move_fwd_target != 0.0f) {
        float s = g.move_fwd_target > 0.5f ? 0.5f : g.move_fwd_target;
        player_move(g.player.dir_x * s, g.player.dir_y * s);
        g.move_fwd_target -= s;
        if(g.move_fwd_target < 0.0f) g.move_fwd_target = 0.0f;
    }
    if(g.move_bwd_target != 0.0f) {
        float s = g.move_bwd_target > 0.3f ? 0.3f : g.move_bwd_target;
        player_move(-g.player.dir_x * s, -g.player.dir_y * s);
        g.move_bwd_target -= s;
        if(g.move_bwd_target < 0.0f) g.move_bwd_target = 0.0f;
    }

    actors_update();
    // 任务进度更新 + 完成检测/奖励
    quest_update();
    if(g.msg_ttl > 0) {
        g.msg_ttl--;
        if(g.msg_ttl == 0) g.msg_id = MSG_NONE;
    }
    // 每帧都dirty: 有闪烁和敌人移动
    g.dirty = true;
}
