#include "maze3d.h"
#include <math.h>
#include <string.h>

void set_msg(int id) {
    if(id >= 0) {
        g.msg_id = id;
        g.msg_ttl = 100;
    } else {
        g.msg_id = MSG_NONE;
        g.msg_ttl = 0;
    }
}

static int level_stage(int level) {
    // v6.0: 1-10 纯迷宫 / 11-20 解谜(钥匙+门) / 21-50 战斗(手枪+敌人)
    if(level >= 21) return STAGE_COMBAT;
    if(level >= 11) return STAGE_PUZZLE;
    return STAGE_MAZE_ONLY;
}

// ---- 任务系统 ----
// 根据关卡设置任务 (只有"有剧情"的关卡才有任务)
//   level 1 (序章): 找出口
//   level 10-19 (解谜): 拿钥匙 + 开门
//   level 20+  (战斗): 消灭所有敌人
//   其他关卡: 无任务
static int combat_enemy_count(int level) {
    // v6.0: 21关起 2 敌人, 每 3 关 +1, 上限 6
    int n = 2 + (level - 21) / 3;
    return n > 6 ? 6 : n;
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
        // 1-10 纯迷宫: 找到出口
        g.quest.active = true;
        g.quest.sub_count = 1;
        g.quest.subs[0].type = TASK_FIND_EXIT;
        g.quest.subs[0].target = 1;
    } else if(stg == STAGE_PUZZLE) {
        // 11-20 解谜: 拿钥匙 + 开门, 钥匙数递进 (11关1把, 每3关+1)
        int keys = 1 + (level - 11) / 3;
        if(keys > 3) keys = 3;
        g.quest.active = true;
        g.quest.sub_count = 2;
        g.quest.subs[0].type = TASK_GET_KEY;
        g.quest.subs[0].target = keys;
        g.quest.subs[1].type = TASK_OPEN_DOOR;
        g.quest.subs[1].target = 1;
    } else if(stg == STAGE_COMBAT) {
        // 21-50 战斗: 全歼敌人 + 存活 + 找出口 (递进)
        int enemies = combat_enemy_count(level);
        g.quest.active = true;
        g.quest.sub_count = 2;
        g.quest.subs[0].type = TASK_KILL_ENEMY;
        g.quest.subs[0].target = enemies;
        g.quest.subs[1].type = TASK_FIND_EXIT;
        g.quest.subs[1].target = 1;
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
        default:
            break;
        }
        if(s->target > 0 && s->progress >= s->target) s->done = true;
    }
    // 是否全部完成
    bool all = true;
    for(int i = 0; i < g.quest.sub_count; i++) {
        if(!g.quest.subs[i].done) {
            all = false;
            break;
        }
    }
    if(all && !g.quest.all_done) {
        g.quest.all_done = true;
        // 奖励: 回满血 (一次性)
        if(!g.quest.reward_given) {
            g.quest.reward_given = true;
            g.player.health = g.player.max_health;
            sfx_play(SFX_QUEST_DONE);
            set_msg(MSG_QUESTDONE); // 任务完成 toast
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
            g.ach_total_kills++; // v6.0 成就统计
            sfx_play(SFX_ENEMY_KILL);
            ach_check(); // 触发击杀成就
        }
        return true;
    }
    return false;
}

// v6.1: 手枪射击 — 发射一颗实体子弹 (沿玩家朝向飞行, 由 bullets_update 处理碰撞).
//       立即播放枪口闪光 + 后坐力 + 震屏, 子弹命中时再结算伤害.
void player_shoot(void) {
    if(g.ammo == 0) {
        set_msg(MSG_NOAMMO);
        sfx_play(SFX_NO_AMMO);
        return;
    }
    g.ammo--;
    sfx_play(SFX_SHOOT);
    // 枪口位置: 玩家中心 + 朝向前移 0.3 格
    float px = g.player.x + g.player.dir_x * 0.3f;
    float py = g.player.y + g.player.dir_y * 0.3f;
    bullet_spawn(px, py, g.player.dir_x, g.player.dir_y, 0);
    // 视觉反馈
    g.muzzle_flash = 3; // 3 帧枪口闪光
    g.shoot_kick = 4; // 4 帧后坐力 (准星下移)
    g.screen_shake = 2; // 轻微震屏
    g.dirty = true;
}

// v6.1: 子弹系统 — 生成子弹 (owner: 0=玩家 1=敌人)
void bullet_spawn(float x, float y, float dx, float dy, int owner) {
    for(int i = 0; i < MAX_BULLETS; i++) {
        if(!g.bullets[i].active) {
            g.bullets[i].active = true;
            g.bullets[i].x = x;
            g.bullets[i].y = y;
            // 速度: 玩家子弹 0.55 格/帧, 敌人子弹 0.35 格/帧
            float sp = (owner == 0) ? 0.55f : 0.35f;
            g.bullets[i].dx = dx * sp;
            g.bullets[i].dy = dy * sp;
            g.bullets[i].life = 30; // 约 30 帧 (~3.7s) 寿命
            g.bullets[i].owner = (uint8_t)owner;
            return;
        }
    }
}

int bullets_active_count(void) {
    int n = 0;
    for(int i = 0; i < MAX_BULLETS; i++)
        if(g.bullets[i].active) n++;
    return n;
}

// v6.1: 推进所有子弹, 检测墙/敌人/玩家碰撞
static bool blocking(uint8_t c); // 前向声明 (定义在下方 player_move 附近)
void bullets_update(void) {
    for(int i = 0; i < MAX_BULLETS; i++) {
        Bullet* b = &g.bullets[i];
        if(!b->active) continue;
        b->x += b->dx;
        b->y += b->dy;
        if(--b->life == 0) {
            b->active = false;
            continue;
        }
        // 撞墙: 消失 (基岩/砖/树等都挡子弹)
        uint8_t c = maze_get((int)b->x, (int)b->y);
        if(blocking(c)) {
            b->active = false;
            continue;
        }
        if(b->owner == 0) {
            // 玩家子弹: 检测敌人命中 (圆形碰撞, 半径 0.4)
            for(int j = 0; j < g.actor_count; j++) {
                Actor* a = &g.actors[j];
                if(!a->active || a->type != 0 || a->hp == 0) continue;
                float ddx = a->x - b->x, ddy = a->y - b->y;
                if(ddx * ddx + ddy * ddy < 0.16f) {
                    a->hp -= 2; // 手枪伤害 2
                    a->hurt_flash = 4; // 受伤反白 4 帧
                    b->active = false;
                    if(a->hp <= 0) {
                        a->active = false;
                        g.task_kill_count++;
                        g.ach_total_kills++;
                        sfx_play(SFX_ENEMY_KILL);
                        ach_check();
                    } else {
                        sfx_play(SFX_ATTACK_HIT);
                    }
                    break;
                }
            }
        } else {
            // 敌人子弹: 检测玩家命中
            float ddx = g.player.x - b->x, ddy = g.player.y - b->y;
            if(ddx * ddx + ddy * ddy < 0.16f && g.player.health > 0) {
                g.player.health -= 1;
                g.hurt_flash = 6; // 屏幕闪白
                g.screen_shake = 4;
                b->active = false;
                set_msg(MSG_HIT);
                sfx_play(SFX_DAMAGE);
                if(g.player.health <= 0) {
                    g.mode = MODE_GAME_OVER;
                    sfx_play(SFX_GAME_OVER);
                }
            }
        }
    }
}

// v6.0: 成就系统 — 检查里程碑并触发弹窗
void ach_grant(uint32_t flag, int msg_extra) {
    if(g.ach_flags & flag) return;
    g.ach_flags |= flag;
    set_msg(MSG_ACHIEVE);
    sfx_play(SFX_ACHIEVE);
    storage_save();
    (void)msg_extra;
}

void ach_check(void) {
    if(g.ach_total_kills >= 1) ach_grant(ACH_FIRST_BLOOD, 0);
    if(g.ach_total_kills >= 10) ach_grant(ACH_KILL_10, 0);
    if(g.ach_total_kills >= 50) ach_grant(ACH_KILL_50, 0);
    if(g.ach_total_clears >= 1) ach_grant(ACH_FIRST_CLEAR, 0);
    if(g.ach_total_clears >= 10) ach_grant(ACH_CLEAR_10, 0);
    if(g.ach_total_clears >= 25) ach_grant(ACH_CLEAR_25, 0);
    if(g.ach_total_mined >= 50) ach_grant(ACH_MINER_50, 0);
    if(g.mode == MODE_CAMPAIGN) {
        if(g.level >= 21) ach_grant(ACH_REACH_COMBAT, 0);
        if(g.level >= 35) ach_grant(ACH_REACH_LATE, 0);
    }
}

static bool walkable(uint8_t c) {
    // v6.1: 所有 WALL_* 都是实体方块, 玩家不可踏入; 仅地面道具/空格可走.
    // (WALL_WATER 也算实体阻挡 — MC 模式里水池是不能直接踩进去的方块)
    return c == CELL_EMPTY || c == CELL_KEY || c == CELL_EXIT || c == CELL_TORCH ||
           c == CELL_TRAP || c == CELL_POTION || c == CELL_AMULET || c == CELL_LOCKED_EXIT;
}
static bool blocking(uint8_t c) {
    // v6.1: 所有 WALL_* + 门都阻挡玩家移动 (草地/水/沙/木/树都是实体方块)
    return c == WALL_BRICK || c == WALL_STONE || c == WALL_METAL || c == WALL_VINE ||
           c == WALL_WATER || c == WALL_GRASS || c == WALL_WOOD || c == WALL_TREE ||
           c == CELL_DOOR;
}

static void set_dir(float angle) {
    g.player.dir_x = cosf(angle);
    g.player.dir_y = sinf(angle);
    g.player.plane_x = -g.player.dir_y * 0.66f;
    g.player.plane_y = g.player.dir_x * 0.66f;
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
            if(g.player.health <= 0) {
                g.mode = MODE_GAME_OVER;
                sfx_play(SFX_GAME_OVER);
            }
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
            g.player.x -= dx;
            g.player.y -= dy;
            return false;
        }
    } else if(here == CELL_EXIT) {
        // v6.0 修复瞬间通关: 出口在任务未完成时锁定, 不能直接通过.
        // 解谜关必须先开门, 战斗关必须先全歼敌人.
        if(g.quest.active && !g.quest.all_done) {
            // 任务未完成: 弹回 + 提示
            g.player.x -= dx;
            g.player.y -= dy;
            set_msg(MSG_LOCKED);
            sfx_play(SFX_LOCKED);
            return false;
        }
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
            g.ach_total_clears++; // v6.0 成就统计
            ach_check(); // 触发过关成就
            storage_save();
            sfx_play(SFX_LEVEL_CLEAR);
        } else if(g.mode == MODE_ENDLESS_RUN) {
            g.endless_floor++;
            storage_save();
            game_next_level();
        } else if(g.mode == MODE_ENDLESS_VISITOR) {
            set_msg(MSG_EXIT);
        }
    } else if(here == CELL_LOCKED_EXIT) {
        // v6.0 锁定出口: 视觉上存在但不可通过, 提示任务未完成
        g.player.x -= dx;
        g.player.y -= dy;
        set_msg(MSG_LOCKED);
        sfx_play(SFX_LOCKED);
        return false;
    }
    return true;
}

void player_rotate(float angle) {
    float cs = cosf(angle), sn = sinf(angle);
    float ndx = g.player.dir_x * cs - g.player.dir_y * sn;
    float ndy = g.player.dir_x * sn + g.player.dir_y * cs;
    g.player.dir_x = ndx;
    g.player.dir_y = ndy;
    float npx = g.player.plane_x * cs - g.player.plane_y * sn;
    float npy = g.player.plane_x * sn + g.player.plane_y * cs;
    g.player.plane_x = npx;
    g.player.plane_y = npy;
}

void spawn_actor(float x, float y, int type) {
    if(g.actor_count >= MAX_ACTORS) return;
    Actor* a = &g.actors[g.actor_count++];
    a->x = x;
    a->y = y;
    a->active = true;
    a->type = (uint8_t)type;
    a->cooldown = 0;
    // v6.0: 敌人血量随关卡递进 (21-27关 hp=2, 28-34关 hp=3, 35+关 hp=4)
    // 手枪伤害=2, 高级关需要2发; 近战冲刺伤害=1
    if(type == 0) {
        int hp = 2;
        if(g.level >= 35)
            hp = 4;
        else if(g.level >= 28)
            hp = 3;
        a->hp = (uint8_t)hp;
    } else {
        a->hp = 0;
    }
    a->fire_cd = (uint8_t)(20 + (maze_rng_next() & 31)); // v6.1: 随机初始射击冷却
    a->hurt_flash = 0;
}

// v6.1: 敌人 AI — 移动 + 视线内远程射击玩家
void actors_update(void) {
    for(int i = 0; i < g.actor_count; i++) {
        Actor* a = &g.actors[i];
        if(!a->active) continue;
        if(a->hurt_flash > 0) a->hurt_flash--; // 受伤闪烁递减
        if(a->cooldown > 0) {
            a->cooldown--;
        } else {
            a->cooldown = (a->type == 0) ? (uint8_t)22 : (uint8_t)34;
            // 移动: 优先朝玩家方向走 (简单贪心), 走不通则随机
            float dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            int r = maze_rng_next() & 3;
            // 30% 概率朝玩家走, 70% 随机游走 (避免太聪明卡死玩家)
            bool chase = (a->type == 0) && ((maze_rng_next() & 3) < 2);
            if(chase) {
                float ddx = g.player.x - a->x, ddy = g.player.y - a->y;
                // 选主导轴
                if(fabsf(ddx) > fabsf(ddy)) {
                    dirs[0][0] = (ddx > 0) ? 1 : -1;
                    dirs[0][1] = 0;
                    dirs[1][0] = 0;
                    dirs[1][1] = (ddy > 0) ? 1 : -1;
                } else {
                    dirs[0][0] = 0;
                    dirs[0][1] = (ddy > 0) ? 1 : -1;
                    dirs[1][0] = (ddx > 0) ? 1 : -1;
                    dirs[1][1] = 0;
                }
                r = 0;
            }
            for(int k = 0; k < 4; k++) {
                int idx = (r + k) & 3;
                float nx = a->x + dirs[idx][0];
                float ny = a->y + dirs[idx][1];
                if(walkable(maze_get((int)nx, (int)ny))) {
                    a->x = nx;
                    a->y = ny;
                    break;
                }
            }
        }
        // 近战接触伤害 (原有逻辑保留)
        if(a->type == 0) {
            float ddx = a->x - g.player.x, ddy = a->y - g.player.y;
            if(ddx * ddx + ddy * ddy < 0.6f && g.player.health > 0) {
                g.player.health -= 1;
                g.hurt_flash = 6;
                g.screen_shake = 4;
                set_msg(MSG_HIT);
                sfx_play(SFX_DAMAGE);
                if(g.player.health <= 0) {
                    g.mode = MODE_GAME_OVER;
                    sfx_play(SFX_GAME_OVER);
                }
            }
        }
        // v6.1: 战斗关敌人远程射击 — 视线内且 2~6 格距离时开火
        if(a->type == 0 && a->hp > 0 && g.stage == STAGE_COMBAT) {
            if(a->fire_cd > 0)
                a->fire_cd--;
            else {
                float ddx = g.player.x - a->x, ddy = g.player.y - a->y;
                float dist2 = ddx * ddx + ddy * ddy;
                if(dist2 > 4.0f && dist2 < 36.0f) {
                    // 视线检测: 沿方向 DDA 看是否被墙挡住
                    float dist = sqrtf(dist2);
                    float nx = ddx / dist, ny = ddy / dist;
                    bool blocked = false;
                    for(float s = 0.5f; s < dist; s += 0.5f) {
                        if(blocking(maze_get((int)(a->x + nx * s), (int)(a->y + ny * s)))) {
                            blocked = true;
                            break;
                        }
                    }
                    if(!blocked) {
                        bullet_spawn(a->x, a->y, nx, ny, 1);
                        sfx_play(SFX_SHOOT); // 复用射击音 (敌人射击)
                        a->fire_cd = (uint8_t)(40 + (g.level * 2)); // 高关卡射更慢
                    } else {
                        a->fire_cd = 8; // 视线被挡, 短暂等待
                    }
                } else {
                    a->fire_cd = 10;
                }
            }
        }
    }
}

static void place_player_and_actors(int level, bool visitor) {
    g.player.x = 1.5f;
    g.player.y = 1.5f;
    set_dir(0.0f);
    g.player.keys = 0;
    g.player.torches = 0;
    g.player.potions = 0;
    g.player.amulets = 0;
    g.actor_count = 0;

    // 起始 HP/物品: 剧情模式由开场选择决定; 无尽/游客固定
    if(g.mode == MODE_CAMPAIGN) {
        if(g.story_choice == 0) { // A) Warrior
            g.player.max_health = 7;
            g.player.health = 7;
        } else if(g.story_choice == 1) { // B) Seeker
            g.player.max_health = 4;
            g.player.health = 4;
            g.player.torches = 1;
        } else { // 未选(直接进高层级重玩)
            g.player.max_health = 5;
            g.player.health = 5;
        }
    } else {
        g.player.max_health = 5;
        g.player.health = 5;
    }

    if(g.mode == MODE_CAMPAIGN && g.stage == STAGE_COMBAT) {
        // v6.0: 战斗关给手枪弹药 (敌人数 + 3, 留容错), 敌人血量随关卡递进
        int enemies = combat_enemy_count(level);
        g.ammo = (uint8_t)(enemies + 3);
        int placed = 0, tries = 0;
        while(placed < enemies && tries++ < 300) {
            int x = 2 + maze_rng_next() % (g.map_w - 4);
            int y = 2 + maze_rng_next() % (g.map_h - 4);
            if(walkable(maze_get(x, y)) && (abs(x - 1) + abs(y - 1) > 5)) {
                spawn_actor(x + 0.5f, y + 0.5f, 0);
                placed++;
            }
        }
    } else {
        g.ammo = 0;
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
    // v6.0: 迷宫尺寸随关卡递进, 上限 25 防卡顿; 50 关上限
    int sz = 7 + level;
    if(sz > 25) sz = 25;
    maze_generate(sz, sz, level, 0xABCDEF01u);
    place_player_and_actors(level, false);
    quest_init_for_level(level);
    if(g.stage == STAGE_COMBAT)
        set_msg(MSG_CARE);
    else if(g.stage == STAGE_PUZZLE)
        set_msg(MSG_PUZZLE);
    else
        set_msg(MSG_FINDEXIT);
    ach_check(); // v6.0: 触发进度里程碑成就
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
    if(sz < 9) sz = 9;
    // 用 floor 做种子散列 (避免连续 floor 生成相似地图)
    unsigned seed = 0x12345678u ^ (unsigned)floor * 2654435761u ^ (unsigned)floor * 1013904242u;
    maze_generate(sz, sz, floor + 100, seed);
    place_player_and_actors(floor + 100, visitor);
    set_msg(visitor ? MSG_VISITOR : MSG_RUN);
    g.dirty = true;
}

void game_next_level(void) {
    if(g.mode == MODE_CAMPAIGN)
        game_init_campaign(g.level + 1);
    else if(g.mode == MODE_ENDLESS_RUN)
        game_init_endless(g.endless_floor, false);
}

// ---- MC 沙盒模式 (v6.0 扩展) ----
// v6.0: 手持方块类型 mc_block_type: 1=砖 2=石 3=木 4=草 5=沙 6=树叶
// (金属/水/树不可放置; 挖树得木, 挖草得草, 挖沙得沙)
#define MC_BLOCK_COUNT 6
static uint8_t mc_block_to_wall(uint8_t bt) {
    switch(bt) {
    case 1:
        return WALL_BRICK;
    case 2:
        return WALL_STONE;
    case 3:
        return WALL_WOOD;
    case 4:
        return WALL_GRASS;
    case 5:
        return WALL_VINE; // 沙用藤蔓纹理近似
    case 6:
        return WALL_TREE; // 树叶用树纹理
    default:
        return WALL_BRICK;
    }
}

// 玩家正前方的相邻格 (按主导方向取 4 邻域, 类似 MC 的十字挖掘)
static void mc_target_cell(int* tx, int* ty) {
    int px = (int)g.player.x;
    int py = (int)g.player.y;
    if(fabsf(g.player.dir_x) >= fabsf(g.player.dir_y))
        *tx = px + (g.player.dir_x > 0 ? 1 : -1), *ty = py;
    else
        *tx = px, *ty = py + (g.player.dir_y > 0 ? 1 : -1);
}

void game_init_mc(void) {
    g.mode = MODE_MC;
    g.stage = STAGE_MAZE_ONLY;
    g.has_exit = false;
    g.exit_found = false;
    g.tick = 0;
    g.show_hud = false;
    g.turn_target = 0.0f;
    g.move_fwd_target = 0.0f;
    g.move_bwd_target = 0.0f;
    g.move_dash_target = 0.0f;
    g.actor_count = 0;
    g.player.keys = 0;
    g.player.torches = 0;
    g.player.potions = 0;
    g.player.amulets = 0;
    g.player.max_health = 5;
    g.player.health = 5;
    g.quest.active = false;
    g.quest.sub_count = 0;
    g.quest.all_done = false;
    g.ammo = 0;

    // v6.1: 15x15 大空间, 以空地为主 (玩家可自由走动),
    //        随机点缀方块 (草/树/木/沙/水) — 不再"几乎全是实体"卡死玩家.
    const int sz = 15;
    g.map_w = sz;
    g.map_h = sz;
    // 外圈基岩(不可挖), 内部: 默认空地, 随机撒方块
    for(int y = 0; y < sz; y++) {
        for(int x = 0; x < sz; x++) {
            uint8_t c;
            if(x == 0 || y == 0 || x == sz - 1 || y == sz - 1) {
                c = WALL_METAL; // 基岩边界
            } else {
                // 用关卡种子派生 PRNG, 保证地形可重现又多样
                uint32_t h = maze_rng_next() & 15;
                // v6.1: 约 30% 格子放方块 (草/树/沙/水/木混合), 70% 留空可走
                if(h < 4)
                    c = WALL_GRASS;
                else if(h < 7)
                    c = WALL_TREE;
                else if(h < 9)
                    c = WALL_WOOD;
                else if(h < 11)
                    c = WALL_VINE; // 沙地用藤蔓纹理
                else if(h < 12)
                    c = WALL_WATER;
                else
                    c = CELL_EMPTY;
            }
            g.map[(uint16_t)y * MAP_MAX + x] = c;
        }
    }
    // 玩家出生点周围 3x3 强制清空 (保证有起步空间)
    int mx = sz / 2, my = sz / 2;
    for(int dy = -1; dy <= 1; dy++)
        for(int dx = -1; dx <= 1; dx++)
            g.map[(uint16_t)(my + dy) * MAP_MAX + (mx + dx)] = CELL_EMPTY;
    g.player.x = mx + 0.5f;
    g.player.y = my + 0.5f;
    set_dir(0.0f);

    g.mc_block_type = 3; // v6.0: 默认手持木板
    g.mc_mined = 0;
    set_msg(MSG_MINE);
    g.dirty = true;
}

void mc_mine(void) {
    int tx, ty;
    mc_target_cell(&tx, &ty);
    uint8_t c = maze_get(tx, ty);
    bool is_border = (tx == 0 || ty == 0 || tx == g.map_w - 1 || ty == g.map_h - 1);
    // v6.0: 可挖草地/树/木/砖/藤蔓/沙; 水和基岩边界不可挖
    if(is_border || c == WALL_WATER || c == WALL_METAL) {
        sfx_play(SFX_NEED_KEY); // 不可挖提示
        return;
    }
    if(c == WALL_GRASS || c == WALL_TREE || c == WALL_WOOD || c == WALL_BRICK || c == WALL_STONE ||
       c == WALL_VINE) {
        maze_set(tx, ty, CELL_EMPTY);
        g.mc_mined++;
        g.ach_total_mined++; // v6.0 成就统计
        set_msg(MSG_MINE);
        sfx_play(SFX_PICK_ITEM);
        ach_check(); // 触发挖掘成就
    } else {
        sfx_play(SFX_NEED_KEY);
    }
}

void mc_place(void) {
    int tx, ty;
    mc_target_cell(&tx, &ty);
    // 目标格恒为玩家相邻格, 不会封死玩家自身所在格
    if(maze_get(tx, ty) == CELL_EMPTY) {
        maze_set(tx, ty, mc_block_to_wall(g.mc_block_type));
        set_msg(MSG_PLACE);
        sfx_play(SFX_OPEN_DOOR);
    } else {
        sfx_play(SFX_NEED_KEY);
    }
}

// ---- 物品栏 ----
int item_count(int item_type) {
    switch(item_type) {
    case ITEM_KEY:
        return g.player.keys;
    case ITEM_TORCH:
        return g.player.torches;
    case ITEM_POTION:
        return g.player.potions;
    case ITEM_AMULET:
        return g.player.amulets;
    default:
        return 0;
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
            g.player.x = 1.5f;
            g.player.y = 1.5f;
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
    const float speed = 0.28f;
    const float slow = 0.18f;
    const float dash = 0.42f;
    const float turn = 0.26f;

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
        // v6.0: 战斗关 OK = 手枪射击; 其他关 OK = 冲刺
        if(g.stage == STAGE_COMBAT) {
            player_shoot();
        } else {
            g.move_dash_target += dash;
        }
        break;
    default:
        break;
    }
    g.dirty = true;
}

void game_update(void) {
    // 平滑插值: 每帧施加 turn_target 的 40% (剩余 60% 累积到下帧),
    // 这样"按左右键"不会立刻转一个大角度,而是分几帧平滑转到目标朝向.
    // 如果 turn_target 过大(连续按多次),就每帧 40% 分步转.
    if(g.turn_target != 0.0f) {
        float step = g.turn_target * 0.45f;
        if(fabsf(step) < 0.01f) {
            step = g.turn_target;
            g.turn_target = 0.0f;
        } else {
            g.turn_target -= step;
        }
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
    bullets_update(); // v6.1: 推进所有飞行中的子弹
    // v6.1: 视觉帧计数递减 (枪口闪光/受伤闪白/后坐力/震屏)
    if(g.muzzle_flash > 0) g.muzzle_flash--;
    if(g.hurt_flash > 0) g.hurt_flash--;
    if(g.shoot_kick > 0) g.shoot_kick--;
    if(g.screen_shake > 0)
        g.screen_shake--;
    else if(g.screen_shake < 0)
        g.screen_shake++;
    // 任务进度更新 + 完成检测/奖励
    quest_update();
    if(g.msg_ttl > 0) {
        g.msg_ttl--;
        if(g.msg_ttl == 0) g.msg_id = MSG_NONE;
    }
    // 每帧都dirty: 有闪烁和敌人移动
    g.dirty = true;
}
