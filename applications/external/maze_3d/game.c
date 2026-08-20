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
        // 命中 (v6.10.2: 近战也扣1血+闪烁)
        if(a->hp > 0) a->hp--;
        a->hurt_flash = 15;
        set_msg(MSG_HIT);
        sfx_play(SFX_ATTACK_HIT);
        if(a->hp == 0) {
            a->active = false;
            g.task_kill_count++;
            g.ach_total_kills++;
            g.score += 10; // v6.10: 击杀+10积分
            sfx_play(SFX_ENEMY_KILL);
            ach_check();
            particle_spawn(a->x, a->y, 1, 10);
        }
        return true;
    }
    return false;
}

// v6.2: 手枪射击 — 发射实体子弹 + 射击火花粒子.
//       MC 模式没有弹药也能调用: 播放"空枪"反馈.
void player_shoot(void) {
    if(g.ammo == 0) {
        set_msg(MSG_NOAMMO);
        sfx_play(SFX_NO_AMMO);
        return;
    }
    g.ammo--;
    sfx_play(SFX_SHOOT);
    // v6.11: 双倍火力 — 每次射击消耗 1 子弹但发射 2 发
    int shots = (g.buff_doublefire > 0) ? 2 : 1;
    // v6.10: 自动锁定 — 找画面中最近的敌人, 子弹偏向它
    float bdx = g.player.dir_x, bdy = g.player.dir_y;
    g.auto_lock = 255;
    float best_score = 999.0f;
    for(int i = 0; i < g.actor_count; i++) {
        Actor* a = &g.actors[i];
        if(!a->active || a->type != 0 || a->hp == 0) continue;
        float ex = a->x - g.player.x, ey = a->y - g.player.y;
        float fwd = ex * g.player.dir_x + ey * g.player.dir_y; // 前方投影
        if(fwd < 0.5f || fwd > 8.0f) continue; // 必须在前方 0.5~8 格
        float side = fabsf(ex * (-g.player.dir_y) + ey * g.player.dir_x); // 侧向偏移
        if(side > 2.0f) continue; // 视锥宽度 2 格
        // 综合评分: 距离越近 + 越居中 = 越优先
        float sc = fwd + side * 2.0f;
        if(sc < best_score) {
            best_score = sc;
            g.auto_lock = (uint8_t)i;
            // 子弹方向偏转向敌人 (最多偏 30°)
            float dist = sqrtf(ex * ex + ey * ey);
            float tx = ex / dist, ty = ey / dist;
            // 混合: 70% 原方向 + 30% 目标方向
            bdx = g.player.dir_x * 0.7f + tx * 0.3f;
            bdy = g.player.dir_y * 0.7f + ty * 0.3f;
            float bl = sqrtf(bdx * bdx + bdy * bdy);
            if(bl > 0.01f) {
                bdx /= bl;
                bdy /= bl;
            }
        }
    }
    // 枪口位置
    float px = g.player.x + g.player.dir_x * 0.3f;
    float py = g.player.y + g.player.dir_y * 0.3f;
    // v6.11: 双倍火力 — 第二发子弹带散射偏移
    for(int s = 0; s < shots; s++) {
        float sx = bdx, sy = bdy;
        if(shots > 1 && s == 1) {
            // 第二发: 横向偏 0.2 弧度 (约 11°)
            float c = cosf(0.2f), sn = sinf(0.2f);
            sx = bdx * c - bdy * sn;
            sy = bdx * sn + bdy * c;
        }
        bullet_spawn(px, py, sx, sy, 0);
    }
    particle_spawn(px, py, 0, 5);
    g.muzzle_flash = 3;
    g.shoot_kick = 4;
    g.screen_shake = 2;
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

// v6.1: 推进所有子弹, 检测墙/敌人/玩家碰撞 + 生成尾迹/爆炸粒子
static bool blocking(uint8_t c); // 前向声明 (定义在下方 player_move 附近)
void bullets_update(void) {
    for(int i = 0; i < MAX_BULLETS; i++) {
        Bullet* b = &g.bullets[i];
        if(!b->active) continue;
        b->x += b->dx;
        b->y += b->dy;
        // v6.2: 子弹尾迹粒子 (每帧 1 粒)
        particle_spawn(b->x, b->y, 2, 1);
        if(--b->life == 0) {
            b->active = false;
            continue;
        }
        // 撞墙: 消失 + 爆炸粒子
        uint8_t c = maze_get((int)b->x, (int)b->y);
        if(blocking(c)) {
            // v6.2: 撞墙爆炸 (3粒)
            particle_spawn(b->x, b->y, 1, 3);
            b->active = false;
            continue;
        }
        if(b->owner == 0) {
            // 玩家子弹: 检测敌人命中 (圆形碰撞, 半径 0.4)
            for(int j = 0; j < g.actor_count; j++) {
                Actor* a = &g.actors[j];
                if(!a->active || a->type != 0 || a->hp == 0) continue;
                float ddx = a->x - b->x, ddy = a->y - b->y;
                if(ddx * ddx + ddy * ddy < 0.25f) { // v6.10.2: 碰撞半径增大 (0.5格)
                    a->hp -= 1; // v6.10.2: 每发伤害1, 3枪必死
                    a->hurt_flash = 15; // v6.10.2: 闪烁15帧
                    b->active = false;
                    particle_spawn(b->x, b->y, 1, 6);
                    if(a->hp <= 0) {
                        a->active = false;
                        g.task_kill_count++;
                        g.ach_total_kills++;
                        g.score += 10; // v6.10: 击杀 +10 积分
                        sfx_play(SFX_ENEMY_KILL);
                        ach_check();
                        particle_spawn(a->x, a->y, 1, 10);
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
                // v6.5: 无敌期内不掉血
                if(g.invincible_timer > 0) {
                    b->active = false;
                    continue;
                }
                g.player.health -= 1;
                g.hurt_flash = 6; // 屏幕闪白
                g.screen_shake = 4;
                g.invincible_timer = 120; // v6.5: 2秒无敌 (60fps * 2s)
                g.regen_timer = 0; // 受伤后重置回血计时
                b->active = false;
                particle_spawn(b->x, b->y, 1, 4);
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
    // v6.4: 所有 WALL_* + 门都阻挡玩家移动
    return c == WALL_BRICK || c == WALL_STONE || c == WALL_METAL || c == WALL_VINE ||
           c == WALL_WATER || c == WALL_GRASS || c == WALL_WOOD || c == WALL_TREE ||
           c == CELL_DOOR || c == WALL_SAND || c == WALL_DIRT || c == WALL_LOG;
}

static void set_dir(float angle) {
    g.player.dir_x = cosf(angle);
    g.player.dir_y = sinf(angle);
    g.player.plane_x = -g.player.dir_y * 0.66f;
    g.player.plane_y = g.player.dir_x * 0.66f;
}

// v6.2: 粒子系统
void particles_update(void) {
    for(int i = 0; i < MAX_PARTICLES; i++) {
        Particle* p = &g.particles[i];
        if(!p->active) continue;
        p->x += p->vx;
        p->y += p->vy;
        // 摩擦: 逐渐减速
        p->vx *= 0.92f;
        p->vy *= 0.92f;
        if(--p->life == 0) p->active = false;
    }
}

void particle_spawn(float x, float y, uint8_t type, int count) {
    for(int k = 0; k < count; k++) {
        for(int i = 0; i < MAX_PARTICLES; i++) {
            if(!g.particles[i].active) {
                g.particles[i].active = true;
                g.particles[i].x = x;
                g.particles[i].y = y;
                g.particles[i].type = type;
                // 不同粒子类型的速度/生命/大小分布
                if(type == 0) { // 射击火花: 朝前喷射, 短寿命, 很小
                    float ang = atan2f(g.player.dir_y, g.player.dir_x);
                    ang += ((float)(maze_rng_next() & 31) - 15.5f) * 0.04f; // ±0.6 rad 散射
                    float sp = 0.25f + ((float)(maze_rng_next() & 7)) * 0.02f;
                    g.particles[i].vx = cosf(ang) * sp;
                    g.particles[i].vy = sinf(ang) * sp;
                    g.particles[i].life = 6 + (maze_rng_next() & 3);
                    g.particles[i].size = 1;
                } else if(type == 1) { // 命中爆炸: 四周发散, 中等寿命
                    float ang = (float)(maze_rng_next() & 255) * 0.02454f; // / 2pi 近似
                    float sp = 0.18f + ((float)(maze_rng_next() & 7)) * 0.015f;
                    g.particles[i].vx = cosf(ang) * sp;
                    g.particles[i].vy = sinf(ang) * sp;
                    g.particles[i].life = 8 + (maze_rng_next() & 7);
                    g.particles[i].size = 2;
                } else if(type == 2) { // 子弹尾迹: 与子弹相反方向, 极短寿命
                    float spd = 0.06f;
                    float ang = (float)(maze_rng_next() & 255) * 0.02454f;
                    g.particles[i].vx = cosf(ang) * spd;
                    g.particles[i].vy = sinf(ang) * spd;
                    g.particles[i].life = 3 + (maze_rng_next() & 1);
                    g.particles[i].size = 1;
                } else { // 行走尘土: 向上+后飘散, 慢
                    g.particles[i].vx = (float)((int)(maze_rng_next() & 3) - 1) * 0.04f;
                    g.particles[i].vy = (float)((int)(maze_rng_next() & 3) - 1) * 0.04f;
                    g.particles[i].life = 10 + (maze_rng_next() & 7);
                    g.particles[i].size = 1;
                }
                break; // 每个槽只放一次
            }
        }
    }
}

int particles_active_count(void) {
    int n = 0;
    for(int i = 0; i < MAX_PARTICLES; i++)
        if(g.particles[i].active) n++;
    return n;
}

bool player_move(float dx, float dy) {
    float nx = g.player.x + dx;
    float ny = g.player.y + dy;
    const float pad = 0.2f;
    // v6.10.1: 移除 mc_hop 穿墙 — 改为正常碰撞检测, 跳跃纯视觉
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
        // v6.10: 纯迷宫关 (1-10) 拿钥匙即通关
        if(g.stage == STAGE_MAZE_ONLY && g.mode == MODE_CAMPAIGN) {
            g.mode = MODE_LEVEL_CLEAR;
            if(g.level > g.campaign_cleared) g.campaign_cleared = g.level;
            g.ach_total_clears++;
            ach_check();
            storage_save();
            sfx_play(SFX_LEVEL_CLEAR);
            return true;
        }
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
        if(g.stage == STAGE_COMBAT && g.player.health > 0 && g.invincible_timer == 0) {
            g.player.health -= 1;
            g.invincible_timer = 120; // v6.5: 2秒无敌
            g.regen_timer = 0;
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
        // v6.4 修复: CELL_EXIT 是普通出口 (1-10 纯迷宫关 / 无尽模式),
        //   直接放行. 任务 FIND_EXIT 在此标记完成.
        //   (只有 CELL_LOCKED_EXIT 才需要检查任务完成)
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
        } else if(g.mode == MODE_RAP) {
            // v6.12-beta RAP: 找到出口 → 过关弹框, 之后下一层
            g.mode = MODE_LEVEL_CLEAR;
            g.ach_total_clears++;
            sfx_play(SFX_LEVEL_CLEAR);
            storage_save();
        }
    } else if(here == CELL_LOCKED_EXIT) {
        // v6.4 修复: 任务完成后锁定出口自动放行 (之前总是弹回, 导致无法通关)
        if(g.quest.active && !g.quest.all_done) {
            // 任务未完成: 弹回 + 提示
            g.player.x -= dx;
            g.player.y -= dy;
            set_msg(MSG_LOCKED);
            sfx_play(SFX_LOCKED);
            return false;
        }
        // 任务完成: 当作普通出口处理
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
            g.ach_total_clears++;
            ach_check();
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
    a->tx = x;
    a->ty = y;
    a->active = true;
    a->type = (uint8_t)type;
    a->cooldown = 0;
    // v6.10.2: 统一敌人血量=3, 每发子弹伤害=1, 3枪必死
    if(type == 0) {
        a->hp = 3;
    } else {
        a->hp = 0;
    }
    a->fire_cd = (uint8_t)(20 + (maze_rng_next() & 31));
    a->hurt_flash = 0;
    a->moving = 0;
}

// v6.10: 敌人 AI — 平滑插值移动 + 视线内远程射击 + 看不到玩家时自动接近
void actors_update(void) {
    for(int i = 0; i < g.actor_count; i++) {
        Actor* a = &g.actors[i];
        if(!a->active) continue;
        // v6.10.2: 快死时(hp=1)持续闪烁, 不递减
        if(a->hurt_flash > 0 && a->hp > 1)
            a->hurt_flash--;
        else if(a->hp == 1)
            a->hurt_flash = 1; // 濒死持续闪烁

        // v6.10.2: 平滑移动插值 — 12帧极慢插值 (每帧 1/12 距离)
        if(a->moving > 0) {
            float dx = a->tx - a->x, dy = a->ty - a->y;
            a->x += dx * 0.083f;
            a->y += dy * 0.083f;
            a->moving--;
            if(a->moving == 0) {
                a->x = a->tx;
                a->y = a->ty;
            }
        }

        if(a->cooldown > 0) {
            a->cooldown--;
        } else if(a->moving == 0) {
            // v6.10.2: 敌人极慢移动 + 随机间隔 (80-180帧 = 1.3-3秒一动)
            a->cooldown = (a->type == 0) ? (uint8_t)(80 + (maze_rng_next() & 100)) :
                                           (uint8_t)(120 + (maze_rng_next() & 120));
            float dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            int r = maze_rng_next() & 3;

            // v6.10: 检测是否有视线 — 看不到玩家时 100% 追击, 看到时 60% 追击
            bool has_los = false;
            if(a->type == 0) {
                float ddx = g.player.x - a->x, ddy = g.player.y - a->y;
                float dist2 = ddx * ddx + ddy * ddy;
                if(dist2 < 64.0f) {
                    float dist = sqrtf(dist2);
                    float nx = ddx / dist, ny = ddy / dist;
                    has_los = true;
                    for(float s = 0.5f; s < dist; s += 0.5f) {
                        if(blocking(maze_get((int)(a->x + nx * s), (int)(a->y + ny * s)))) {
                            has_los = false;
                            break;
                        }
                    }
                }
            }
            // 看不到玩家 → 100% 朝玩家走; 看到玩家 → 60% 追击 40% 随机
            bool chase = (a->type == 0) && (has_los ? ((maze_rng_next() & 3) < 2) : true);
            if(chase) {
                float ddx = g.player.x - a->x, ddy = g.player.y - a->y;
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
                    // v6.10: 不直接传送, 设置目标位置 + 插值帧数
                    a->tx = nx;
                    a->ty = ny;
                    a->moving = 12; // 12 帧插值完成 (极慢极平滑)
                    break;
                }
            }
        }
        // 近战接触伤害
        if(a->type == 0) {
            float ddx = a->x - g.player.x, ddy = a->y - g.player.y;
            if(ddx * ddx + ddy * ddy < 0.6f && g.player.health > 0 && g.invincible_timer == 0) {
                g.player.health -= 1;
                g.hurt_flash = 6;
                g.screen_shake = 4;
                g.invincible_timer = 120;
                g.regen_timer = 0;
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
                        sfx_play(SFX_SHOOT);
                        int cd = 80 - (g.level > 20 ? (g.level - 20) : 0);
                        if(cd < 40) cd = 40;
                        a->fire_cd = (uint8_t)cd;
                    } else {
                        a->fire_cd = 8;
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

    // v6.5: 血量上限 = 10 + 关卡递进 (每5关+2, 上限20)
    //   1-4关: 10血, 5-9关: 12血, 10-14关: 14血, ..., 25+关: 20血上限
    int base_hp = 10;
    if(g.mode == MODE_CAMPAIGN) {
        int bonus = (g.level / 5) * 2;
        if(bonus > 10) bonus = 10;
        base_hp += bonus;
    }
    g.player.max_health = base_hp;
    g.player.health = base_hp;
    g.invincible_timer = 0;
    g.regen_timer = 0;

    if(g.mode == MODE_CAMPAIGN && g.stage == STAGE_COMBAT) {
        // v6.9: cfg_ammo_mul (0..3) → 0.5x/1x/2x/3x
        static const float amm[] = {0.5f, 1.0f, 2.0f, 3.0f};
        float amul = amm[(g.cfg_ammo_mul < 4) ? g.cfg_ammo_mul : 1];
        int enemies = combat_enemy_count(level);
        g.ammo = (uint8_t)((float)(enemies + 3) * amul);
        g.ammo_regen_timer = 0; // v6.10: 弹药回复计时器
        int placed = 0, tries = 0;
        // v6.10: 敌人刷新距离玩家至少 8 格曼哈顿距离 (原 5 太近)
        int px = (int)g.player.x, py = (int)g.player.y;
        while(placed < enemies && tries++ < 300) {
            int x = 2 + maze_rng_next() % (g.map_w - 4);
            int y = 2 + maze_rng_next() % (g.map_h - 4);
            if(walkable(maze_get(x, y)) && (abs(x - px) + abs(y - py) > 8)) {
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
    g.turn_hold_dir = 0;
    g.turn_hold_time = 0;
    g.move_hold_dir = 0;
    g.move_hold_time = 0;
    // v6.9: cfg_hp_start: 0..4 → 8/10/12/16/20 HP (+2 per 5 levels, max 20 不变)
    static const int hp_start[] = {8, 10, 12, 16, 20};
    int basehp = hp_start[(g.cfg_hp_start < 5) ? g.cfg_hp_start : 1];
    int add = level / 5 * 2; // 每 5 关 +2 HP (跟原逻辑相同, 基数不同)
    int maxhp = basehp + add;
    if(maxhp > 20) maxhp = 20;
    g.player.max_health = maxhp;
    g.player.health = maxhp;
    // v6.9: cfg_maze_scale 0..4 → 0.6x/0.8x/1.0x/1.2x/1.5x
    static const float mscale[] = {0.6f, 0.8f, 1.0f, 1.2f, 1.5f};
    float ms = mscale[(g.cfg_maze_scale < 5) ? g.cfg_maze_scale : 2];
    // v6.0: 迷宫尺寸随关卡递进, 上限 25 防卡顿; 50 关上限
    int sz = (int)((float)(7 + level) * ms + 0.5f);
    if(sz < 5) sz = 5;
    if(sz > 25) sz = 25;
    // v6.6: 先尝试加载缓存, 未命中则生成并保存
    if(!world_load(level)) {
        maze_generate(sz, sz, level, 0xABCDEF01u);
        world_save(level);
    }
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
    g.turn_hold_dir = 0;
    g.turn_hold_time = 0;
    g.move_hold_dir = 0;
    g.move_hold_time = 0;
    // 尺寸严格上限 19 (19x19=361 格, DDA 步数最多 19 列 × 64 = 1216 次循环)
    int sz = 9 + (floor > 10 ? 10 : floor);
    if(sz > 19) sz = 19;
    if(sz < 9) sz = 9;
    // v6.6: 传 floor (不是 floor+100) 避免 maze_generate 误走战斗分支
    // 用种子散列避免连续 floor 生成相似地图
    // v6.6: 先尝试加载缓存, 未命中则生成并保存
    unsigned seed = 0x12345678u ^ (unsigned)floor * 2654435761u ^ (unsigned)floor * 1013904242u;
    // 无尽模式用负数 level 区分缓存文件名
    int cache_id = -(floor + 1000);
    if(!world_load(cache_id)) {
        maze_generate(sz, sz, floor, seed);
        world_save(cache_id);
    }
    place_player_and_actors(floor, visitor);
    set_msg(visitor ? MSG_VISITOR : MSG_RUN);
    g.dirty = true;
}

void game_next_level(void) {
    if(g.mode == MODE_CAMPAIGN)
        game_init_campaign(g.level + 1);
    else if(g.mode == MODE_ENDLESS_RUN)
        game_init_endless(g.endless_floor, false);
}

// ---- MC 沙盒模式 (v6.3 完整材质版) ----
// v6.3: 手持方块类型 mc_block_type: 1=砖 2=石 3=木板 4=草 5=土 6=沙 7=原木
// 8=树叶 挖到方块自动切换手持 (MC 生存模式核心玩法); 对空地长按 OK = 循环切手持
#define MC_BLOCK_COUNT 8
static uint8_t mc_block_to_wall(uint8_t bt) {
    switch(bt) {
    case 1:
        return WALL_BRICK;
    case 2:
        return WALL_STONE;
    case 3:
        return WALL_WOOD; // 木板
    case 4:
        return WALL_GRASS; // 草方块
    case 5:
        return WALL_DIRT; // 土方块
    case 6:
        return WALL_SAND; // 沙子
    case 7:
        return WALL_LOG; // 原木
    case 8:
        return WALL_TREE; // 树叶 (用树叶纹理)
    default:
        return WALL_BRICK;
    }
}

// v6.3: WALL_* → 手持方块 ID (挖掘时自动切换). 不可挖的返回 0.
static uint8_t mc_wall_to_block(uint8_t wall) {
    switch(wall) {
    case WALL_BRICK:
        return 1;
    case WALL_STONE:
        return 2;
    case WALL_WOOD:
        return 3;
    case WALL_GRASS:
        return 4;
    case WALL_DIRT:
        return 5;
    case WALL_SAND:
        return 6;
    case WALL_LOG:
        return 7;
    case WALL_TREE:
        return 8;
    default:
        return 0; // 水/金属/藤蔓不可得
    }
}

// v6.4: 循环切换手持方块 (Back 短按调用)
void mc_cycle_block(void) {
    g.mc_block_type = (g.mc_block_type >= MC_BLOCK_COUNT) ? 1 : (g.mc_block_type + 1);
    sfx_play(SFX_MENU_MOVE);
    set_msg(MSG_PLACE);
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
    g.turn_hold_dir = 0;
    g.turn_hold_time = 0;
    g.move_hold_dir = 0;
    g.move_hold_time = 0;
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

    // v6.4: 纯平地 — 整个地图全空 (无墙/无边界/无随机方块),
    //   玩家在开阔平地上自由搭建, 天空+日升月落
    // v6.9: cfg_mc_size 0..3 → 11,15,19,23
    static const int mcsizes[] = {11, 15, 19, 23};
    const int sz = mcsizes[(g.cfg_mc_size < 4) ? g.cfg_mc_size : 1];
    g.map_w = sz;
    g.map_h = sz;
    for(int y = 0; y < sz; y++) {
        for(int x = 0; x < sz; x++) {
            g.map[(uint16_t)y * MAP_MAX + x] = CELL_EMPTY;
        }
    }
    g.player.x = sz / 2 + 0.5f;
    g.player.y = sz / 2 + 0.5f;
    set_dir(0.0f);

    // v6.9: cfg_mc_start_sel 0..7 → 8 种方块
    static const uint8_t mc_default_sel[] = {1, 2, 3, 4, 5, 6, 7, 8};
    g.mc_block_type = mc_default_sel[(g.cfg_mc_start_sel < 8) ? g.cfg_mc_start_sel : 0];
    g.mc_mined = 0;
    set_msg(MSG_MINE);
    g.dirty = true;
}

// v6.12-beta: RAP 单键挑战模式初始化
//   - 基于无尽模式: 小迷宫 13x13, 纯找出口
//   - rap_active=true, rap_action=0 (从右转+扫视开始)
//   - 视角全部归零 (启动时平视)
void game_init_rap(void) {
    g.mode = MODE_RAP;
    g.rap_active = true;
    g.rap_action = 0; // 0=TurnR 1=Fwd 2=TurnL 3=Dash
    g.view_pitch_target = 0.0f;
    g.view_pitch = 0.0f;
    g.view_yaw_sweep_target = 0.0f;
    g.view_yaw_sweep = 0.0f;

    g.level = 1;
    g.endless_floor = 1;
    g.stage = STAGE_MAZE_ONLY; // 纯迷宫, 不搞战斗/解谜 (单键玩就纯走路)
    g.has_exit = true;
    g.exit_found = true;
    g.tick = 0;
    g.show_hud = false;
    // 清零所有插值 target
    g.turn_target = 0.0f;
    g.move_fwd_target = 0.0f;
    g.move_bwd_target = 0.0f;
    g.move_dash_target = 0.0f;
    g.turn_hold_dir = 0;
    g.turn_hold_time = 0;
    g.move_hold_dir = 0;
    g.move_hold_time = 0;
    g.jump_timer = 0;
    g.jump_z = 0.0f;
    g.invincible_timer = 0;
    g.regen_timer = 0;

    // RAP: 固定小迷宫 13x13 — 尺寸合适, 找出口不太难也不太简单
    int sz = 13;
    g.map_w = sz;
    g.map_h = sz;
    // 种子: 每局 RAP 开始都不同, 但稳定可复现
    unsigned seed = 0x5A5A0000u ^ (unsigned)furi_get_tick();
    maze_generate(sz, sz, 1, seed);
    // 放置玩家 + 给 10 血 (死亡=游戏结束, 但纯迷宫没有陷阱)
    g.player.x = 1.5f;
    g.player.y = 1.5f;
    g.map_w = sz;
    g.map_h = sz;
    // 重设方向 (东)
    {
        float angle = 0.0f;
        g.player.dir_x = cosf(angle);
        g.player.dir_y = sinf(angle);
        g.player.plane_x = -g.player.dir_y * 0.66f;
        g.player.plane_y = g.player.dir_x * 0.66f;
    }
    g.player.keys = 0;
    g.player.torches = 0;
    g.player.potions = 0;
    g.player.amulets = 0;
    g.actor_count = 0;
    g.ammo = 0;
    g.player.max_health = 10;
    g.player.health = 10;
    quest_init_for_level(1); // 任务: 找出口
    set_msg(MSG_RUN); // "无尽挑战" 提示位图 (凑合用)
    g.dirty = true;
}

// v6.4: mc_mine — OK 长按挖掘前方方块, 挖到后手持自动切换为该方块
void mc_mine(void) {
    int tx, ty;
    mc_target_cell(&tx, &ty);
    uint8_t c = maze_get(tx, ty);
    // 空地/水: 不可挖
    if(c == CELL_EMPTY || c == WALL_WATER) {
        sfx_play(SFX_NEED_KEY);
        return;
    }
    uint8_t got = mc_wall_to_block(c);
    if(got == 0) {
        sfx_play(SFX_NEED_KEY);
        return;
    }
    // 挖掘成功: 清除方块 + 手持切换 + 粒子 + 音效
    maze_set(tx, ty, CELL_EMPTY);
    g.mc_mined++;
    g.ach_total_mined++;
    g.mc_block_type = got; // v6.3: 挖到什么拿什么
    // 挖掘碎屑粒子 (沙/土多撒一些)
    float px = tx + 0.5f, py = ty + 0.5f;
    int cnt = (c == WALL_SAND || c == WALL_DIRT) ? 6 : 3;
    particle_spawn(px, py, 3, cnt);
    set_msg(MSG_MINE);
    sfx_play(SFX_PICK_ITEM);
    ach_check();
}

// v6.3: mc_place — 在前方放置手持方块. 放沙子时额外尘土粒子 (落地效果)
void mc_place(void) {
    int tx, ty;
    mc_target_cell(&tx, &ty);
    if(maze_get(tx, ty) == CELL_EMPTY) {
        uint8_t wall = mc_block_to_wall(g.mc_block_type);
        maze_set(tx, ty, wall);
        // v6.3: 沙子放置 → 落地尘土粒子爆发
        if(wall == WALL_SAND || wall == WALL_DIRT) {
            float px = tx + 0.5f, py = ty + 0.5f;
            particle_spawn(px, py, 3, 6); // 落地尘土
            particle_spawn(px, py, 0, 3); // 少量火花点缀
        }
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

// v6.11: 10 种道具的名称 (中/英) + 价格 + 使用逻辑
//   索引 0..9 对应 g.items[0..9]
const char* shop_item_name_zh(int idx) {
    static const char* names[] = {
        "血量药水",
        "大血药",
        "满血药剂",
        "弹药+5",
        "弹药满",
        "钥匙+1",
        "火把+3",
        "护身符",
        "无敌护盾",
        "双倍火力",
    };
    return (idx >= 0 && idx < 10) ? names[idx] : "?";
}
const char* shop_item_name_en(int idx) {
    static const char* names[] = {
        "Potion S",
        "Potion L",
        "Full Heal",
        "Ammo+5",
        "Ammo Max",
        "Key+1",
        "Torch+3",
        "Amulet",
        "Shield",
        "2x Fire",
    };
    return (idx >= 0 && idx < 10) ? names[idx] : "?";
}
const char* shop_item_desc_zh(int idx) {
    static const char* desc[] = {
        "+5 HP",
        "+10 HP",
        "回满血",
        "+5 子弹",
        "子弹填满",
        "+1 钥匙",
        "+3 火把",
        "回起点",
        "5秒无敌",
        "15秒双倍火力",
    };
    return (idx >= 0 && idx < 10) ? desc[idx] : "";
}
const char* shop_item_desc_en(int idx) {
    static const char* desc[] = {
        "+5 HP",
        "+10 HP",
        "Full HP",
        "+5 Bullets",
        "Ammo Full",
        "+1 Key",
        "+3 Torches",
        "Warp Start",
        "Invincible 5s",
        "2x Fire 15s",
    };
    return (idx >= 0 && idx < 10) ? desc[idx] : "";
}
uint16_t shop_item_price(int idx) {
    static const uint16_t prices[] = {
        30,
        50,
        80, // 血药
        20,
        80,
        20, // 弹药/钥匙
        15,
        70,
        100,
        120, // 火把/护身符/护盾/双倍
    };
    return (idx >= 0 && idx < 10) ? prices[idx] : 0;
}

// v6.11: 使用道具 (从新道具栏调用), 返回 true=成功消耗
bool shop_item_use(int idx) {
    if(idx < 0 || idx >= 10) return false;
    if(g.items[idx] == 0) {
        set_msg(MSG_ITEM_NONE);
        sfx_play(SFX_NO_AMMO);
        return false;
    }
    bool ok = false;
    switch(idx) {
    case 0: // 血量药水 +5HP
        if(g.player.health < g.player.max_health) {
            g.player.health += 5;
            if(g.player.health > g.player.max_health) g.player.health = g.player.max_health;
            ok = true;
        }
        break;
    case 1: // 大血药 +10HP
        if(g.player.health < g.player.max_health) {
            g.player.health += 10;
            if(g.player.health > g.player.max_health) g.player.health = g.player.max_health;
            ok = true;
        }
        break;
    case 2: // 满血药剂
        if(g.player.health < g.player.max_health) {
            g.player.health = g.player.max_health;
            ok = true;
        }
        break;
    case 3: // 弹药+5
        if(g.ammo < 99) {
            g.ammo = (g.ammo + 5 > 99) ? 99 : g.ammo + 5;
            ok = true;
        }
        break;
    case 4: // 弹药满
        if(g.ammo < 99) {
            g.ammo = 99;
            ok = true;
        }
        break;
    case 5: // 钥匙+1
        g.player.keys++;
        ok = true;
        break;
    case 6: // 火把+3
        g.player.torches += 3;
        ok = true;
        break;
    case 7: // 护身符: 传送回起点 (1.5, 1.5)
        g.player.x = 1.5f;
        g.player.y = 1.5f;
        ok = true;
        break;
    case 8: // 无敌护盾 5秒 (300帧)
        g.buff_shield = 300;
        g.invincible_timer = 300;
        set_msg(MSG_BUFF_SHIELD);
        ok = true;
        break;
    case 9: // 双倍火力 15秒 (900帧)
        g.buff_doublefire = 900;
        set_msg(MSG_BUFF_FIRE);
        ok = true;
        break;
    }
    if(ok) {
        g.items[idx]--;
        set_msg(MSG_ITEM_USE);
        sfx_play(SFX_PICK_ITEM);
        g.dirty = true;
        storage_save();
    } else {
        set_msg(MSG_ITEM_NONE);
        sfx_play(SFX_NO_AMMO);
    }
    return ok;
}

void game_handle_input(InputKey key, InputType type) {
    // v6.9: 操控全部由 cfg_* 设置驱动 — 开发者模式可实时调节

    // ---- 左右转向 (全局, 所有模式) ----
    if(key == InputKeyLeft || key == InputKeyRight) {
        int8_t dir = (key == InputKeyLeft) ? -1 : +1;
        // cfg_turn_short: 0..4 → 弧度表 (对应 5.7°/8.6°/11.5°/14.3°/17.2°)
        static const float short_deg[] = {0.10f, 0.15f, 0.20f, 0.25f, 0.30f};
        float sh = short_deg[(g.cfg_turn_short < 5) ? g.cfg_turn_short : 2];
        if(type == InputTypePress) {
            g.turn_hold_dir = dir;
            g.turn_hold_time = 0;
        } else if(type == InputTypeShort) {
            g.turn_target += dir * sh;
            g.turn_hold_dir = 0;
        } else if(type == InputTypeRelease) {
            g.turn_hold_dir = 0;
        }
        g.dirty = true;
        return;
    }

    // ---- 前后移动 (全局) ----
    if(key == InputKeyUp || key == InputKeyDown) {
        int8_t dir = (key == InputKeyUp) ? +1 : -1;
        // cfg_move_short: 0..4 → 前进步幅表 (0.08/0.12/0.15/0.20/0.26)
        static const float fwd_step[] = {0.08f, 0.12f, 0.15f, 0.20f, 0.26f};
        static const float bak_ratio[] = {0.55f, 0.72f, 0.88f, 1.00f}; // cfg_back_ratio
        float fs = fwd_step[(g.cfg_move_short < 5) ? g.cfg_move_short : 2];
        float br = bak_ratio[(g.cfg_back_ratio < 4) ? g.cfg_back_ratio : 1];
        if(type == InputTypePress) {
            g.move_hold_dir = dir;
            g.move_hold_time = 0;
        } else if(type == InputTypeShort) {
            if(dir > 0)
                g.move_fwd_target += fs;
            else
                g.move_bwd_target += fs * br;
            g.move_hold_dir = 0;
        } else if(type == InputTypeRelease) {
            g.move_hold_dir = 0;
        }
        g.dirty = true;
        return;
    }

    // ---- OK 键: 战斗关=射击, 其他=冲刺 ----
    if(key == InputKeyOk) {
        if(type == InputTypeShort || type == InputTypeRepeat) {
            if(g.stage == STAGE_COMBAT) {
                player_shoot();
            } else {
                g.move_dash_target += 0.42f;
            }
        }
        g.dirty = true;
        return;
    }
}

void game_update(void) {
    // v6.7-beta: 退出长按窗口计时 — 超时则计数器清零
    if(g.exit_long_ttl > 0) {
        g.exit_long_ttl--;
        if(g.exit_long_ttl == 0) g.exit_long_cnt = 0;
    }

    // v6.10: 弹药自动回复 — 战斗关每 40 帧 (~0.6s) +1 弹药, 上限 30
    if(g.stage == STAGE_COMBAT && g.ammo < 30) {
        g.ammo_regen_timer++;
        if(g.ammo_regen_timer >= 40) {
            g.ammo_regen_timer = 0;
            g.ammo++;
            g.dirty = true;
        }
    }

    // v6.10.2: 跳跃峰值再次降低 — MC模式上限3px, 普通模式上限6px
    float JUMP_PEAK_LOCAL;
    {
        static const float peaks[] = {0.0f, 3.0f, 5.0f, 6.0f};
        JUMP_PEAK_LOCAL = peaks[(g.cfg_jump_height < 4) ? g.cfg_jump_height : 2];
        // MC 模式额外限制: 最高 3px (防止卡墙崩溃)
        if(g.mode == MODE_MC && JUMP_PEAK_LOCAL > 3.0f) JUMP_PEAK_LOCAL = 3.0f;
    }
    // v6.9: cfg_regen_rate 回血倍率 (0=0.5x, 1=1.0x, 2=2.0x, 3=3.0x)
    //   倍率越高, 阈值越低 → 回血越快
    static const float regen_mul[] = {0.5f, 1.0f, 2.0f, 3.0f};
    float rm = regen_mul[(g.cfg_regen_rate < 4) ? g.cfg_regen_rate : 1];
    uint16_t regen_threshold = (uint16_t)(60.0f / rm);
    if(regen_threshold < 5) regen_threshold = 5;
    if(regen_threshold > 240) regen_threshold = 240;

    // v6.7-beta: 跳跃抛物线 — 基于半余弦曲线 jump_z
    if(g.jump_timer > 0 && JUMP_PEAK_LOCAL > 0.1f) {
        float t_norm = (float)g.jump_timer / (float)JUMP_FRAMES;
        float sine_approx = t_norm * (1.0f - t_norm) * 4.0f;
        g.jump_z = sine_approx * JUMP_PEAK_LOCAL;
        g.jump_timer++;
        if(g.jump_timer >= JUMP_FRAMES) {
            g.jump_timer = 0;
            g.jump_z = 0.0f;
            // 落地瞬间 2 粒尘土
            float bx = g.player.x - g.player.dir_x * 0.35f;
            float by = g.player.y - g.player.dir_y * 0.35f;
            particle_spawn(bx, by, 3, 2);
        }
    } else if(JUMP_PEAK_LOCAL <= 0.1f) {
        // 跳跃被关闭
        g.jump_timer = 0;
        g.jump_z = 0.0f;
    }
    // v6.9: 转向灵敏度 + 长按加速 — cfg_turn_sens
    // 0..5: 1.0x/1.25x/1.5x/1.75x/2.0x/2.5x
    static const float t_sens[] = {1.0f, 1.25f, 1.5f, 1.75f, 2.0f, 2.5f};
    float ts = t_sens[(g.cfg_turn_sens < 6) ? g.cfg_turn_sens : 2];
    // cfg_turn_max 0..4: 0.030,0.038,0.050,0.065,0.085
    static const float tmax_vals[] = {0.030f, 0.038f, 0.050f, 0.065f, 0.085f};
    float tmax = tmax_vals[(g.cfg_turn_max < 5) ? g.cfg_turn_max : 2];
    if(g.turn_hold_dir != 0) {
        g.turn_hold_time++;
        // 基础速度由 t_sens 调整: base = 0.008 * ts, 每 8 帧加 0.003 * ts
        float base = 0.008f * ts;
        float inc = 0.003f * ts;
        float v = base + (float)(g.turn_hold_time / 8) * inc;
        if(v > tmax) v = tmax;
        g.turn_target += (float)g.turn_hold_dir * v;
    }
    // v6.9: 长按移动加速 cfg_move_max 0..4: 0.024,0.030,0.042,0.055,0.072
    static const float mmax_vals[] = {0.024f, 0.030f, 0.042f, 0.055f, 0.072f};
    float mmax = mmax_vals[(g.cfg_move_max < 5) ? g.cfg_move_max : 2];
    // 后退比
    static const float bak_r[] = {0.55f, 0.72f, 0.88f, 1.00f};
    float backr = bak_r[(g.cfg_back_ratio < 4) ? g.cfg_back_ratio : 1];
    if(g.move_hold_dir != 0) {
        g.move_hold_time++;
        float base = 0.007f * ts; // 移动基础速度也跟 turn_sens 挂钩 (更快时都更快)
        float inc = 0.004f * ts;
        float v = base + (float)(g.move_hold_time / 6) * inc;
        if(v > mmax) v = mmax;
        if(g.move_hold_dir > 0)
            g.move_fwd_target += v;
        else
            g.move_bwd_target += v * backr;
    }

    // 平滑插值
    if(g.turn_target != 0.0f) {
        float step = g.turn_target * 0.55f;
        if(fabsf(step) < 0.008f) {
            step = g.turn_target;
            g.turn_target = 0.0f;
        } else {
            g.turn_target -= step;
        }
        player_rotate(step);
    }

    if(g.move_dash_target != 0.0f) {
        if(g.stage == STAGE_COMBAT && player_attack()) {
            // 命中敌人, 不移动
        } else {
            float s = g.move_dash_target;
            player_move(g.player.dir_x * s, g.player.dir_y * s);
        }
        g.move_dash_target = 0.0f;
    }
    if(g.move_fwd_target != 0.0f) {
        float s = g.move_fwd_target > 0.55f ? 0.55f : g.move_fwd_target;
        player_move(g.player.dir_x * s, g.player.dir_y * s);
        g.move_fwd_target -= s;
        if(g.move_fwd_target < 0.0f) g.move_fwd_target = 0.0f;
    }
    if(g.move_bwd_target != 0.0f) {
        float s = g.move_bwd_target > 0.38f ? 0.38f : g.move_bwd_target;
        player_move(-g.player.dir_x * s, -g.player.dir_y * s);
        g.move_bwd_target -= s;
        if(g.move_bwd_target < 0.0f) g.move_bwd_target = 0.0f;
    }

    // v6.12-beta: 视角插值 (俯仰 pitch + 左右扫视 yaw sweep)
    //   所有模式都支持 — 让游戏画面有呼吸感, 单键模式节奏更强
    {
        float lerp_rate = g.rap_active ? 0.38f : 0.25f; // RAP 模式更快回弹, 更嗨
        float dp = g.view_pitch_target - g.view_pitch;
        if(fabsf(dp) > 0.002f) {
            float step = dp * lerp_rate;
            if(fabsf(step) < 0.003f) {
                g.view_pitch = g.view_pitch_target;
            } else {
                g.view_pitch += step;
            }
        } else {
            g.view_pitch = g.view_pitch_target;
        }
        float dyw = g.view_yaw_sweep_target - g.view_yaw_sweep;
        if(fabsf(dyw) > 0.002f) {
            float step = dyw * lerp_rate;
            if(fabsf(step) < 0.003f) {
                g.view_yaw_sweep = g.view_yaw_sweep_target;
            } else {
                g.view_yaw_sweep += step;
            }
        } else {
            g.view_yaw_sweep = g.view_yaw_sweep_target;
        }
        // RAP 模式: 每帧加一点随机扫视抖动(像 Rapper 一样不停晃头), 让画面更有动感
        if(g.rap_active && (g.tick & 3) == 0) {
            float wob = ((float)((int)(maze_rng_next() & 31) - 16)) * 0.002f;
            g.view_yaw_sweep += wob;
            float wob_p = ((float)((int)(maze_rng_next() & 15) - 8)) * 0.002f;
            g.view_pitch += wob_p;
        }
    }

    actors_update();
    bullets_update(); // v6.1: 推进所有飞行中的子弹
    particles_update(); // v6.2: 粒子更新 (摩擦 + 寿命递减)
    // v6.2: 玩家移动尘土粒子 (根据上一帧和当前帧距离)
    {
        float dpx = g.player.x - g.prev_px;
        float dpy = g.player.y - g.prev_py;
        if(dpx * dpx + dpy * dpy > 0.015f && (g.tick & 1)) {
            // 脚下往后撒 2 粒尘土
            float bx = g.player.x - g.player.dir_x * 0.35f;
            float by = g.player.y - g.player.dir_y * 0.35f;
            particle_spawn(bx, by, 3, 2);
        }
        g.prev_px = g.player.x;
        g.prev_py = g.player.y;
    }
    // v6.1: 视觉帧计数递减 (枪口闪光/受伤闪白/后坐力/震屏)
    if(g.muzzle_flash > 0) g.muzzle_flash--;
    if(g.hurt_flash > 0) g.hurt_flash--;
    if(g.shoot_kick > 0) g.shoot_kick--;
    if(g.screen_shake > 0)
        g.screen_shake--;
    else if(g.screen_shake < 0)
        g.screen_shake++;
    // v6.5: 无敌时间递减
    if(g.invincible_timer > 0) g.invincible_timer--;
    // v6.11: 增益计时器递减 (护盾/双倍火力)
    if(g.buff_shield > 0) g.buff_shield--;
    if(g.buff_doublefire > 0) g.buff_doublefire--;
    // v6.5: 每秒回血 1 点 (60 tick = 1秒), 无敌期结束后才开始回血
    // v6.9: 阈值由 cfg_regen_rate 控制 (regen_threshold)
    if(g.invincible_timer == 0 && g.player.health < g.player.max_health) {
        g.regen_timer++;
        if(g.regen_timer >= regen_threshold) {
            g.regen_timer = 0;
            g.player.health++;
        }
    }
    // 任务进度更新 + 完成检测/奖励
    quest_update();
    if(g.msg_ttl > 0) {
        g.msg_ttl--;
        if(g.msg_ttl == 0) g.msg_id = MSG_NONE;
    }
    // 每帧都dirty: 有闪烁和敌人移动
    g.dirty = true;
}
