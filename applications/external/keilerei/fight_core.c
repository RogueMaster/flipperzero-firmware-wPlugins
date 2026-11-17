/* KEILEREI – Spielkern. 1:1-Portierung von web/index.html (gleiche RNG, gleiche Reihenfolge). */
#include "fight_core.h"
#include <string.h>
#include <stdio.h>

#define W  FG_W
#define H  FG_H
#define OY 8

/* ---------------- KONFIGURATION (identisch zur Web-Version halten) ---------------- */
enum {
    E_HALLE,
    E_WALD,
    E_BAND,
    E_OFEN,
    E_EIS,
    E_NACHT
};
static const char* const ENV_NAME[] =
    {"WERKHALLE", "WALD", "FOERDERBAND", "BRENNOFEN", "EISLAGER", "NACHTSCHICHT"};
static const char* const ENV_TIP[] = {
    "AUFWAERMEN!",
    "BUESCHE ZERSCHLAGEN",
    "BAENDER SCHIEBEN DICH",
    "LAVA PULSIERT!",
    "RUTSCHIG!",
    "SICHT MINIMAL"};
enum {
    GRUNT,
    RUNNER,
    BRUTE,
    SHOOTER,
    BOSS
};
/* HP, Ticks pro Schritt, Schaden, Punkte, Sprite */
static const uint16_t ET[5][5] = {
    {1, 8, 1, 10, 0x6996},
    {1, 4, 1, 15, 0x9669},
    {4, 12, 2, 30, 0xF99F},
    {2, 10, 1, 25, 0x4E4A},
    {16, 9, 2, 250, 0x9FF9}};
/* Umgebung, Grunt, Renner, Brecher, Schuetze, Boss, Spawn-Intervall, Kisten-Loot % */
static const uint8_t WAVES[][8] = {
    {E_HALLE, 4, 0, 0, 0, 0, 40, 75},
    {E_WALD, 5, 2, 0, 0, 0, 36, 70},
    {E_BAND, 4, 3, 0, 1, 0, 34, 70},
    {E_OFEN, 4, 0, 1, 2, 0, 34, 65},
    {E_HALLE, 3, 2, 0, 0, 1, 40, 80},
    {E_EIS, 0, 6, 0, 2, 0, 30, 65},
    {E_NACHT, 6, 0, 2, 2, 0, 30, 65},
    {E_OFEN, 0, 5, 2, 3, 0, 28, 60},
    {E_WALD, 6, 4, 2, 2, 0, 26, 60},
    {E_NACHT, 3, 0, 2, 3, 1, 30, 75}};
#define NWAVES ((int)(sizeof(WAVES) / sizeof(WAVES[0])))
enum {
    W_FIST,
    W_CLUB,
    W_SWORD,
    W_AXE,
    W_PISTOL
};
static const char* const WP_NAME[] = {"FAUST", "KNUEPPEL", "SCHWERT", "AXT", "PISTOLE"};
static const uint8_t WP[5][4] = {
    {1, 4, 0, 0},
    {2, 5, 30, 2},
    {2, 4, 25, 0},
    {4, 8, 18, 0},
    {2, 3, 12, 0}}; /* Schaden, CD, Haltb., Rueckstoss */
static const uint8_t LOOT_CUM[6] = {20, 38, 56, 72, 84, 100};
/* ---------------------------------------------------------------------------------- */

static const int8_t DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};
enum {
    T_FLOOR,
    T_WALL,
    T_CRATE,
    T_LAVA,
    T_BELT_L,
    T_BELT_R,
    T_BUSH,
    T_ITEM = 16
};
enum {
    I_HEART = 1,
    I_SHIELD = 2
};
static const char* const ITEM_NAME[] =
    {"", "HERZ +2", "SCHILD", "KNUEPPEL", "SCHWERT", "AXT", "PISTOLE"};
static const uint16_t ITEM_SPR[] = {0, 0xAF60, 0xFF96, 0x3348, 0x1A4A, 0x6E44, 0xFC80};
#define SPR_WALL   0xFFFF
#define SPR_CRATE  0xFBDF
#define SPR_BUSH   0x6FF6
#define SPR_PLAYER 0x6F69
#define SPR_FIST   0x6FF6
#define SPR_PBUL   0x0660
#define SPR_EBUL   0x4A40

#include "font.inc"

/* ---------------- Helfer ---------------- */
static uint32_t rnd(Fight* g, uint32_t n) {
    uint32_t s = g->rng;
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    g->rng = s;
    return s % n;
}
static int iabs(int a) {
    return a < 0 ? -a : a;
}
static int sgn(int a) {
    return a > 0 ? 1 : (a < 0 ? -1 : 0);
}
static int tile(const Fight* g, int x, int y) {
    return (x < 0 || y < 0 || x >= W || y >= H) ? T_WALL : g->map[y * W + x];
}
static void setT(Fight* g, int x, int y, int t) {
    g->map[y * W + x] = (uint8_t)t;
}
static int solid(int t) {
    return t == T_WALL || t == T_CRATE || t == T_BUSH;
}
static int enemyAt(const Fight* g, int x, int y) {
    for(int i = 0; i < FG_MAXE; i++)
        if(g->e[i].on && g->e[i].x == x && g->e[i].y == y) return i;
    return -1;
}
static int near(const Fight* g, int x, int y, int d) {
    return iabs(x - g->px) + iabs(y - g->py) <= d;
}
static int freeE(const Fight* g, int x, int y) {
    int t = tile(g, x, y);
    return !solid(t) && t != T_LAVA && !(x == g->px && y == g->py) && enemyAt(g, x, y) < 0;
}
static int freeP(const Fight* g, int x, int y) {
    return !solid(tile(g, x, y)) && enemyAt(g, x, y) < 0;
}
static int lavaHot(const Fight* g) {
    return g->env == E_OFEN && (g->tick % 80) >= 40;
}
static void msg(Fight* g, const char* s) {
    strncpy(g->msg, s, sizeof(g->msg) - 1);
    g->msg[sizeof(g->msg) - 1] = 0;
    g->msgT = 40;
}
static int randCell(Fight* g, int minD) {
    for(int k = 0; k < 300; k++) {
        int x = 1 + (int)rnd(g, W - 2), y = 1 + (int)rnd(g, H - 2);
        if(tile(g, x, y) == T_FLOOR && !near(g, x, y, minD)) return y * W + x;
    }
    return -1;
}
static int rollItem(Fight* g) {
    uint32_t r = rnd(g, 100);
    for(int k = 0; k < 6; k++)
        if(r < LOOT_CUM[k]) return k + 1;
    return I_HEART;
}
static void addFx(Fight* g, int x, int y) {
    if(x < 0 || y < 0 || x >= W || y >= H) return;
    for(int k = 0; k < FG_MAXFX; k++)
        if(g->fx[k].t == 0) {
            g->fx[k].x = (int8_t)x;
            g->fx[k].y = (int8_t)y;
            g->fx[k].t = 3;
            return;
        }
}

/* ---------------- Level ---------------- */
static void pillar(Fight* g, int x, int y) {
    if(near(g, x, y, 4) || near(g, x + 1, y + 1, 4)) return;
    if(tile(g, x, y) || tile(g, x + 1, y) || tile(g, x, y + 1) || tile(g, x + 1, y + 1)) return;
    setT(g, x, y, T_WALL);
    setT(g, x + 1, y, T_WALL);
    setT(g, x, y + 1, T_WALL);
    setT(g, x + 1, y + 1, T_WALL);
}
static void genLevel(Fight* g) {
    uint8_t* m = g->map;
    int x, y, k, env = g->env;
    for(k = 0; k < W * H; k++)
        m[k] = T_FLOOR;
    for(x = 0; x < W; x++) {
        m[x] = T_WALL;
        m[(H - 1) * W + x] = T_WALL;
    }
    for(y = 0; y < H; y++) {
        m[y * W] = T_WALL;
        m[y * W + W - 1] = T_WALL;
    }
    g->px = W >> 1;
    g->py = H >> 1;
    g->pd = 1;
    if(env == E_BAND)
        for(x = 3; x < W - 3; x++) {
            setT(g, x, 4, T_BELT_R);
            setT(g, x, H - 5, T_BELT_L);
        }
    if(env == E_HALLE || env == E_NACHT || env == E_BAND || env == E_OFEN) {
        int n = env == E_HALLE ? 6 : (env == E_NACHT ? 5 : 2);
        for(k = 0; k < n; k++) {
            int px = 2 + (int)rnd(g, W - 5);
            int py = 2 + (int)rnd(g, H - 5);
            pillar(g, px, py);
        }
    }
    if(env == E_WALD)
        for(y = 1; y < H - 1; y++)
            for(x = 1; x < W - 1; x++)
                if(!near(g, x, y, 3) && rnd(g, 100) < 16) setT(g, x, y, T_BUSH);
    if(env == E_OFEN)
        for(k = 0; k < 5; k++) {
            int lx = 1 + (int)rnd(g, W - 5);
            int ly = 1 + (int)rnd(g, H - 3);
            int lw = 2 + (int)rnd(g, 3);
            int lh = 1 + (int)rnd(g, 2);
            for(y = ly; y < ly + lh && y < H - 1; y++)
                for(x = lx; x < lx + lw && x < W - 1; x++)
                    if(!near(g, x, y, 3) && tile(g, x, y) == T_FLOOR) setT(g, x, y, T_LAVA);
        }
    if(env == E_EIS)
        for(k = 0; k < 9; k++) {
            int c = randCell(g, 3);
            if(c >= 0) m[c] = T_WALL;
        }
    /* Unerreichbare Taschen zumauern */
    uint8_t seen[W * H];
    int16_t stack[W * H];
    int sp = 0;
    memset(seen, 0, sizeof(seen));
    stack[sp++] = (int16_t)(g->py * W + g->px);
    seen[g->py * W + g->px] = 1;
    while(sp > 0) {
        int p = stack[--sp], cx = p % W, cy = p / W;
        for(int d = 0; d < 4; d++) {
            int nx = cx + DX[d], ny = cy + DY[d];
            if(nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
            int q = ny * W + nx;
            if(seen[q] || m[q] == T_WALL) continue;
            seen[q] = 1;
            stack[sp++] = (int16_t)q;
        }
    }
    for(k = 0; k < W * H; k++)
        if(!seen[k]) m[k] = T_WALL;
    int nc = 5 + (g->wave >> 1);
    if(nc > 14) nc = 14;
    for(k = 0; k < nc; k++) {
        int c = randCell(g, 3);
        if(c >= 0) m[c] = T_CRATE;
    }
}

/* ---------------- Ablauf ---------------- */
static void nextWave(Fight* g) {
    g->wave++;
    const uint8_t* c = WAVES[(g->wave - 1) % NWAVES];
    int loop = (g->wave - 1) / NWAVES, k;
    g->env = c[0];
    g->total = 0;
    g->spawned = 0;
    g->alive = 0;
    for(int t = 0; t < 5; t++) {
        k = c[1 + t];
        if(t != BOSS) k += (k * loop) >> 1;
        g->left[t] = k;
        g->total += k;
    }
    g->iv = c[6] - loop * 4;
    if(g->iv < 12) g->iv = 12;
    g->loot = c[7];
    g->spawnT = 20;
    for(k = 0; k < FG_MAXE; k++)
        g->e[k].on = 0;
    for(k = 0; k < FG_MAXB; k++)
        g->b[k].on = 0;
    for(k = 0; k < FG_MAXFX; k++)
        g->fx[k].t = 0;
    genLevel(g);
    g->mcd = 0;
    g->acd = 0;
    g->slide = 0;
    g->inv = 0;
    g->msgT = 0;
    g->st = S_INTRO;
    g->stT = 60;
}
static void newGame(Fight* g) {
    g->score = 0;
    g->wave = 0;
    g->hp = 6;
    g->wp = W_FIST;
    g->dur = 0;
    g->sh = 0;
    g->newHi = 0;
    nextWave(g);
}
static void hurt(Fight* g, int dmg) {
    if(g->inv > 0 || g->st != S_PLAY) return;
    if(g->sh > 0) {
        g->sh--;
        g->inv = 12;
        g->evt |= EV_HIT;
        msg(g, "GEBLOCKT");
        return;
    }
    g->hp -= dmg;
    g->inv = 24;
    g->evt |= EV_HURT;
    if(g->hp <= 0) {
        g->hp = 0;
        g->st = S_OVER;
        g->stT = 40;
        g->evt |= EV_OVER;
        if(g->score > g->hi) {
            g->hi = g->score;
            g->newHi = 1;
        }
    }
}
static void pickup(Fight* g, int it) {
    if(it == I_HEART) {
        g->hp += 2;
        if(g->hp > 6) g->hp = 6;
    } else if(it == I_SHIELD)
        g->sh = 5;
    else {
        int w = it - 2;
        if(g->wp == w) {
            g->dur += WP[w][2];
            if(g->dur > 99) g->dur = 99;
        } else {
            g->wp = w;
            g->dur = WP[w][2];
        }
    }
    msg(g, ITEM_NAME[it]);
    g->evt |= EV_PICK;
    g->score += 5;
}
static int movePlayer(Fight* g, int d) {
    int nx = g->px + DX[d], ny = g->py + DY[d];
    if(!freeP(g, nx, ny)) return 0;
    g->px = nx;
    g->py = ny;
    int t = tile(g, nx, ny);
    if(t >= T_ITEM) {
        pickup(g, t - T_ITEM);
        setT(g, nx, ny, T_FLOOR);
    }
    return 1;
}
static void breakCrate(Fight* g, int x, int y) {
    setT(g, x, y, rnd(g, 100) < (uint32_t)g->loot ? T_ITEM + rollItem(g) : T_FLOOR);
    addFx(g, x, y);
    g->evt |= EV_HIT;
}
static void dmgEnemy(Fight* g, int i, int dmg) {
    FEnemy* e = &g->e[i];
    e->hp -= dmg;
    e->fl = 6;
    if(e->acd < 8) e->acd = 8;
    if(e->hp <= 0) {
        e->on = 0;
        g->alive--;
        g->score += ET[e->t][3];
        g->evt |= EV_KILL;
        addFx(g, e->x, e->y);
        uint32_t chance = e->t == BOSS ? 100 : (e->t == BRUTE ? 30 : 7);
        if(tile(g, e->x, e->y) == T_FLOOR && rnd(g, 100) < chance)
            setT(g, e->x, e->y, T_ITEM + rollItem(g));
    } else
        g->evt |= EV_HIT;
}
static void wear(Fight* g) {
    if(g->wp == W_FIST) return;
    g->dur--;
    if(g->dur <= 0) {
        char b[24];
        snprintf(b, sizeof(b), "%s KAPUTT", WP_NAME[g->wp]);
        msg(g, b);
        g->wp = W_FIST;
        g->dur = 0;
    }
}
static int hitCell(Fight* g, int x, int y, int w, int d) {
    int i = enemyAt(g, x, y);
    if(i >= 0) {
        dmgEnemy(g, i, WP[w][0]);
        FEnemy* e = &g->e[i];
        if(e->on && WP[w][3] > 0) {
            for(int k = 0; k < WP[w][3]; k++) {
                int nx = e->x + DX[d], ny = e->y + DY[d];
                if(freeE(g, nx, ny)) {
                    e->x = (int8_t)nx;
                    e->y = (int8_t)ny;
                } else
                    break;
            }
            if(e->cd < 10) e->cd = 10;
        }
        return 1;
    }
    int t = tile(g, x, y);
    if(t == T_CRATE) {
        breakCrate(g, x, y);
        return 1;
    }
    if(t == T_BUSH) {
        setT(g, x, y, T_FLOOR);
        addFx(g, x, y);
        return 1;
    }
    return 0;
}
static void moveBullet(Fight* g, FBullet* b) {
    int nx = b->x + DX[b->d], ny = b->y + DY[b->d], t = tile(g, nx, ny);
    if(solid(t)) {
        if(t == T_CRATE) breakCrate(g, nx, ny);
        b->on = 0;
        return;
    }
    b->x = (int8_t)nx;
    b->y = (int8_t)ny;
    if(b->own == 0) {
        int i = enemyAt(g, nx, ny);
        if(i >= 0) {
            dmgEnemy(g, i, b->dmg);
            b->on = 0;
        }
    } else if(nx == g->px && ny == g->py) {
        hurt(g, b->dmg);
        b->on = 0;
    }
}
static void fire(Fight* g, int x, int y, int d, int own, int dmg) {
    for(int k = 0; k < FG_MAXB; k++) {
        FBullet* b = &g->b[k];
        if(!b->on) {
            b->on = 1;
            b->x = (int8_t)x;
            b->y = (int8_t)y;
            b->d = (uint8_t)d;
            b->own = (uint8_t)own;
            b->dmg = (uint8_t)dmg;
            moveBullet(g, b);
            return;
        }
    }
}
static void attack(Fight* g) {
    int w = g->wp, d = g->pd, hit = 0;
    g->acd = WP[w][1];
    if(w == W_PISTOL) {
        fire(g, g->px, g->py, d, 0, WP[w][0]);
        g->evt |= EV_SHOT;
        wear(g);
        return;
    }
    int fx = g->px + DX[d], fy = g->py + DY[d];
    addFx(g, fx, fy);
    if(hitCell(g, fx, fy, w, d)) hit = 1;
    if(w == W_SWORD) {
        int sd = (d + 1) & 3;
        for(int k = -1; k <= 1; k += 2) {
            int sx = fx + DX[sd] * k, sy = fy + DY[sd] * k;
            addFx(g, sx, sy);
            if(hitCell(g, sx, sy, w, d)) hit = 1;
        }
    }
    if(w == W_AXE && !solid(tile(g, fx, fy))) {
        int ax = fx + DX[d], ay = fy + DY[d];
        addFx(g, ax, ay);
        if(hitCell(g, ax, ay, w, d)) hit = 1;
    }
    if(hit && w != W_FIST) wear(g);
}
static int clearLine(const Fight* g, int x, int y, int dx, int dy) {
    int sx = sgn(dx), sy = sgn(dy), n = iabs(dx) + iabs(dy) - 1;
    while(n-- > 0) {
        x += sx;
        y += sy;
        if(solid(tile(g, x, y))) return 0;
    }
    return 1;
}
static int tryStep(Fight* g, FEnemy* e, int ox, int oy) {
    if((ox || oy) && freeE(g, e->x + ox, e->y + oy)) {
        e->x += ox;
        e->y += oy;
        return 1;
    }
    return 0;
}
static void stepE(Fight* g, FEnemy* e, int sx, int sy, int hFirst) {
    if(rnd(g, 6) == 0) hFirst = !hFirst;
    if(hFirst) {
        if(tryStep(g, e, sx, 0) || tryStep(g, e, 0, sy)) return;
    } else {
        if(tryStep(g, e, 0, sy) || tryStep(g, e, sx, 0)) return;
    }
    int d = (int)rnd(g, 4);
    tryStep(g, e, DX[d], DY[d]);
}
static void updEnemies(Fight* g) {
    for(int i = 0; i < FG_MAXE; i++) {
        FEnemy* e = &g->e[i];
        if(!e->on) continue;
        if(e->fl > 0) e->fl--;
        if(e->acd > 0) e->acd--;
        const uint16_t* T = ET[e->t];
        int dx = g->px - e->x, dy = g->py - e->y, adx = iabs(dx), ady = iabs(dy), md = adx + ady;
        if(md == 1) {
            if(!e->adj) {
                e->adj = 1;
                if(e->acd < 8) e->acd = 8;
            } else if(e->acd == 0) {
                hurt(g, T[2]);
                e->acd = 24;
                addFx(g, g->px, g->py);
            }
            continue;
        }
        e->adj = 0;
        if(e->t == SHOOTER && e->acd == 0 && (dx == 0 || dy == 0) && md <= 12 &&
           clearLine(g, e->x, e->y, dx, dy)) {
            fire(g, e->x, e->y, dy < 0 ? 0 : (dx > 0 ? 1 : (dy > 0 ? 2 : 3)), 1, 1);
            e->acd = 40;
            g->evt |= EV_SHOT;
            continue;
        }
        if(e->t == BOSS && e->acd == 0 && md <= 10) {
            for(int d = 0; d < 4; d++)
                fire(g, e->x, e->y, d, 1, 1);
            e->acd = 50;
            g->evt |= EV_SHOT;
        }
        if(e->cd > 0) {
            e->cd--;
            continue;
        }
        e->cd = (uint8_t)T[1];
        if(e->t == SHOOTER) {
            if(md < 4)
                stepE(g, e, -sgn(dx), -sgn(dy), adx >= ady);
            else if(dx == 0 || dy == 0)
                stepE(g, e, sgn(dx), sgn(dy), adx > ady);
            else if(adx < ady)
                stepE(g, e, sgn(dx), 0, 1);
            else
                stepE(g, e, 0, sgn(dy), 0);
        } else
            stepE(g, e, sgn(dx), sgn(dy), adx > ady);
    }
}
static void updBullets(Fight* g) {
    for(int k = 0; k < FG_MAXB; k++) {
        FBullet* b = &g->b[k];
        if(b->on && (b->own == 0 || (g->tick & 1))) moveBullet(g, b);
    }
}
static void belts(Fight* g) {
    int t = tile(g, g->px, g->py);
    if(t == T_BELT_R || t == T_BELT_L) movePlayer(g, t == T_BELT_R ? 1 : 3);
    for(int k = 0; k < FG_MAXE; k++) {
        FEnemy* e = &g->e[k];
        if(!e->on) continue;
        int t2 = tile(g, e->x, e->y);
        if(t2 == T_BELT_R || t2 == T_BELT_L) {
            int d = t2 == T_BELT_R ? 1 : 3;
            if(freeE(g, e->x + DX[d], e->y)) e->x += DX[d];
        }
    }
}
static void spawn(Fight* g) {
    if(g->spawned >= g->total) return;
    g->spawnT--;
    if(g->spawnT > 0) return;
    g->spawnT = g->iv;
    int maxAlive = 4 + (g->wave >> 1);
    if(maxAlive > FG_MAXE) maxAlive = FG_MAXE;
    if(g->alive >= maxAlive) return;
    int rest = g->total - g->spawned, r = (int)rnd(g, (uint32_t)rest), t, k;
    for(t = 0; t < 5; t++) {
        if(r < g->left[t]) break;
        r -= g->left[t];
    }
    if(t == BOSS && g->spawned < (g->total >> 1) && rest > g->left[BOSS])
        for(t = 0; t < 4; t++)
            if(g->left[t] > 0) break;
    int c = -1;
    for(k = 0; k < 200; k++) {
        int x = 1 + (int)rnd(g, W - 2), y = 1 + (int)rnd(g, H - 2);
        if(freeE(g, x, y) && tile(g, x, y) < T_ITEM && !near(g, x, y, 7)) {
            c = y * W + x;
            break;
        }
    }
    if(c < 0) {
        g->spawnT = 5;
        return;
    }
    for(k = 0; k < FG_MAXE; k++) {
        FEnemy* e = &g->e[k];
        if(e->on) continue;
        e->on = 1;
        e->t = (uint8_t)t;
        e->x = (int8_t)(c % W);
        e->y = (int8_t)(c / W);
        e->hp =
            (int16_t)(t == BOSS ? ET[BOSS][0] + g->wave * 2 : ET[t][0] + (g->wave - 1) / NWAVES);
        e->cd = 12;
        e->acd = 10;
        e->fl = 12;
        e->adj = 0;
        break;
    }
    g->left[t]--;
    g->spawned++;
    g->alive++;
}
static void playStep(Fight* g, uint8_t held, uint8_t pr) {
    int k, d = -1;
    if(g->inv > 0) g->inv--;
    if(g->mcd > 0) g->mcd--;
    if(g->acd > 0) g->acd--;
    if(g->msgT > 0) g->msgT--;
    for(k = 0; k < FG_MAXFX; k++)
        if(g->fx[k].t > 0) g->fx[k].t--;
    for(k = 0; k < 4; k++)
        if(pr & (1 << k)) d = k;
    if(d < 0)
        for(k = 0; k < 4; k++)
            if(held & (1 << k)) d = k;
    if(d >= 0) {
        if((pr & (1 << d)) || g->mcd == 0) {
            g->pd = d;
            g->slide = (movePlayer(g, d) && g->env == E_EIS) ? 2 : 0;
            g->mcd = 3;
        }
    } else if(g->slide > 0 && g->mcd == 0) {
        g->mcd = 3;
        if(movePlayer(g, g->pd))
            g->slide--;
        else
            g->slide = 0;
    }
    if(((pr | held) & K_A) && g->acd == 0) attack(g);
    if(g->env == E_BAND && g->tick % 6 == 0) belts(g);
    if(lavaHot(g) && tile(g, g->px, g->py) == T_LAVA) hurt(g, 1);
    updBullets(g);
    updEnemies(g);
    spawn(g);
    if(g->st == S_PLAY && g->spawned >= g->total && g->alive == 0) {
        int bonus = 50 * g->wave + 10 * g->hp;
        char b[24];
        snprintf(b, sizeof(b), "BONUS +%d", bonus);
        g->score += bonus;
        msg(g, b);
        g->st = S_CLEAR;
        g->stT = 50;
        g->evt |= EV_WAVE;
    }
}

void fight_init(Fight* g, uint32_t seed, uint32_t hiscore) {
    memset(g, 0, sizeof(*g));
    g->rng = seed ? seed : 1;
    g->hi = hiscore;
    g->st = S_TITLE;
    g->hp = 5;
    g->pd = 1;
    g->iv = 40;
    g->loot = 70;
}
void fight_step(Fight* g, uint8_t held, uint8_t pr) {
    g->tick++;
    g->evt = 0;
    switch(g->st) {
    case S_TITLE:
        if(pr & K_A) newGame(g);
        break;
    case S_INTRO:
        g->stT--;
        if(g->stT <= 0 || ((pr & K_A) && g->stT < 45)) g->st = S_PLAY;
        break;
    case S_PLAY:
        if(pr & K_B) {
            g->st = S_PAUSE;
            break;
        }
        playStep(g, held, pr);
        break;
    case S_PAUSE:
        if(pr & (K_A | K_B)) g->st = S_PLAY;
        break;
    case S_CLEAR:
        if(g->msgT > 0) g->msgT--;
        g->stT--;
        if(g->stT <= 0) nextWave(g);
        break;
    case S_OVER:
        if(g->stT > 0)
            g->stT--;
        else if(pr & K_A)
            g->st = S_TITLE;
        break;
    }
}

/* ---------------- Rendering ---------------- */
static uint8_t* FB;
static void pset(int x, int y, int v) {
    if(x < 0 || y < 0 || x >= 128 || y >= 64) return;
    uint8_t* p = &FB[y * 16 + (x >> 3)];
    uint8_t m = (uint8_t)(1u << (x & 7));
    if(v == 2)
        *p ^= m;
    else if(v)
        *p |= m;
    else
        *p &= (uint8_t)~m;
}
static void spr(int x, int y, uint16_t bits, int mode) {
    for(int r = 0; r < 4; r++)
        for(int c = 0; c < 4; c++)
            if((bits >> (15 - (r * 4 + c))) & 1) pset(x + c, y + r, mode);
}
static void cell(int cx, int cy, uint16_t bits) {
    spr(cx * 4, OY + cy * 4, bits, 1);
}
static void rect(int x0, int y0, int x1, int y1, int v) {
    for(int y = y0; y <= y1; y++)
        for(int x = x0; x <= x1; x++)
            pset(x, y, v);
}
static void text(int x, int y, const char* s, int sc) {
    for(; *s; s++) {
        const char* f = strchr(FCH, *s);
        if(f && *s) {
            uint16_t gl = FBITS[f - FCH];
            for(int r = 0; r < 5; r++)
                for(int c = 0; c < 3; c++)
                    if((gl >> (14 - (r * 3 + c))) & 1)
                        rect(x + c * sc, y + r * sc, x + c * sc + sc - 1, y + r * sc + sc - 1, 1);
        }
        x += 4 * sc;
    }
}
static void ctext(int y, const char* s, int sc) {
    int n = (int)strlen(s);
    text((128 - (n * 4 * sc - sc)) >> 1, y, s, sc);
}
static void box(int y0, int y1) {
    rect(10, y0, 117, y1, 0);
    rect(10, y0, 117, y0, 1);
    rect(10, y1, 117, y1, 1);
    rect(10, y0, 10, y1, 1);
    rect(117, y0, 117, y1, 1);
}
static uint16_t beltSpr(const Fight* g, int right) {
    int p = (g->tick >> 1) & 3;
    static const uint8_t RR[4] = {8, 4, 8, 0}, RL[4] = {1, 2, 1, 0};
    uint16_t s = 0;
    for(int r = 0; r < 4; r++) {
        int n = right ? RR[r] : RL[r];
        n = right ? ((n >> p) | (n << (4 - p))) & 15 : ((n << p) | (n >> (4 - p))) & 15;
        s = (uint16_t)((s << 4) | n);
    }
    return s;
}
static void hud(const Fight* g) {
    char b[16];
    int k, boss = -1;
    for(k = 0; k < g->hp; k++)
        text(k * 4, 1, "*", 1);
    spr(26, 1, g->wp == W_FIST ? SPR_FIST : ITEM_SPR[g->wp + 2], 1);
    if(g->wp == W_FIST)
        text(31, 1, "--", 1);
    else {
        snprintf(b, sizeof(b), "%02d", g->dur);
        text(31, 1, b, 1);
    }
    spr(42, 1, ITEM_SPR[I_SHIELD], 1);
    snprintf(b, sizeof(b), "%d", g->sh);
    text(47, 1, b, 1);
    snprintf(b, sizeof(b), "W%02d", g->wave);
    text(55, 1, b, 1);
    for(k = 0; k < FG_MAXE; k++)
        if(g->e[k].on && g->e[k].t == BOSS) boss = k;
    if(boss >= 0) {
        text(70, 1, "B", 1);
        int bw = g->e[boss].hp;
        if(bw > 28) bw = 28;
        rect(74, 2, 74 + bw - 1, 4, 1);
    } else {
        snprintf(b, sizeof(b), "E%02d", g->total - g->spawned + g->alive);
        text(70, 1, b, 1);
    }
    snprintf(b, sizeof(b), "%06lu", (unsigned long)(g->score % 1000000));
    text(104, 1, b, 1);
    rect(0, 7, 127, 7, 1);
}
static int vis(const Fight* g, int dark, int x, int y) {
    int dx = x - g->px, dy = y - g->py;
    return !dark || dx * dx + dy * dy <= 20;
}
static void drawGame(const Fight* g) {
    hud(g);
    int dark = g->env == E_NACHT && (g->tick % 160) >= 3, x, y, k, t;
    int ph = g->tick % 80, hot = ph >= 40, warn = ph >= 30 && ph < 40;
    for(y = 0; y < H; y++)
        for(x = 0; x < W; x++) {
            if(!vis(g, dark, x, y)) continue;
            t = g->map[y * W + x];
            if(t == T_WALL)
                cell(x, y, SPR_WALL);
            else if(t == T_CRATE)
                cell(x, y, SPR_CRATE);
            else if(t == T_BUSH)
                cell(x, y, SPR_BUSH);
            else if(t == T_LAVA)
                cell(x, y, hot ? 0xA5A5 : ((warn && (g->tick & 2)) ? 0x5A5A : 0x8020));
            else if(t == T_BELT_R || t == T_BELT_L)
                cell(x, y, beltSpr(g, t == T_BELT_R));
            else if(t >= T_ITEM && ((g->tick >> 2) & 1))
                cell(x, y, ITEM_SPR[t - T_ITEM]);
        }
    for(k = 0; k < FG_MAXB; k++) {
        const FBullet* b = &g->b[k];
        if(b->on && vis(g, dark, b->x, b->y)) cell(b->x, b->y, b->own ? SPR_EBUL : SPR_PBUL);
    }
    for(k = 0; k < FG_MAXE; k++) {
        const FEnemy* e = &g->e[k];
        if(e->on && vis(g, dark, e->x, e->y) && !(e->fl & 1)) cell(e->x, e->y, ET[e->t][4]);
    }
    if(!(g->inv > 0 && (g->tick & 2))) {
        int sx = g->px * 4, sy = OY + g->py * 4;
        spr(sx, sy, SPR_PLAYER, 1);
        if(g->pd == 0) {
            pset(sx + 1, sy - 1, 1);
            pset(sx + 2, sy - 1, 1);
        } else if(g->pd == 1) {
            pset(sx + 4, sy + 1, 1);
            pset(sx + 4, sy + 2, 1);
        } else if(g->pd == 2) {
            pset(sx + 1, sy + 4, 1);
            pset(sx + 2, sy + 4, 1);
        } else {
            pset(sx - 1, sy + 1, 1);
            pset(sx - 1, sy + 2, 1);
        }
    }
    for(k = 0; k < FG_MAXFX; k++) {
        const FFx* f = &g->fx[k];
        if(f->t > 0 && vis(g, dark, f->x, f->y)) spr(f->x * 4, OY + f->y * 4, 0xFFFF, 2);
    }
    if(g->msgT > 0 && g->st == S_PLAY) {
        int w = (int)strlen(g->msg) * 4 + 1, x0 = (128 - w) >> 1;
        rect(x0 - 2, 56, x0 + w, 63, 0);
        ctext(58, g->msg, 1);
    }
}
void fight_render(const Fight* g, uint8_t* fb) {
    char b[32];
    FB = fb;
    memset(fb, 0, FG_FB_SIZE);
    if(g->st == S_TITLE) {
        ctext(3, "KEILEREI", 2);
        ctext(16, "LCD BRAWLER", 1);
        int t = (int)(g->tick % 150);
        spr(t - 8, 26, SPR_PLAYER, 1);
        spr(t - 22, 26, ET[GRUNT][4], 1);
        spr(t - 30, 26, ET[RUNNER][4], 1);
        spr(t - 44, 26, ET[BRUTE][4], 1);
        rect(0, 31, 127, 31, 1);
        if((g->tick >> 3) & 1) ctext(37, "OK = START", 1);
        snprintf(b, sizeof(b), "HI %06lu", (unsigned long)g->hi);
        ctext(47, b, 1);
        ctext(57, "WELLEN KISTEN WAFFEN", 1);
        return;
    }
    drawGame(g);
    if(g->st == S_INTRO) {
        box(14, 50);
        snprintf(b, sizeof(b), "WELLE %d", g->wave);
        ctext(18, b, 2);
        ctext(31, ENV_NAME[g->env], 1);
        ctext(39, g->left[BOSS] > 0 ? "BOSS-WELLE!" : ENV_TIP[g->env], 1);
    } else if(g->st == S_PAUSE) {
        box(20, 44);
        ctext(24, "PAUSE", 2);
        ctext(37, "OK = WEITER", 1);
    } else if(g->st == S_CLEAR) {
        box(18, 44);
        ctext(22, "WELLE GESCHAFFT!", 1);
        ctext(32, g->msg, 1);
    } else if(g->st == S_OVER) {
        box(10, 54);
        ctext(14, "K.O.!", 2);
        snprintf(b, sizeof(b), "SCORE %06lu", (unsigned long)g->score);
        ctext(28, b, 1);
        if(g->newHi)
            ctext(36, "NEUER REKORD!", 1);
        else {
            snprintf(b, sizeof(b), "HI %06lu", (unsigned long)g->hi);
            ctext(36, b, 1);
        }
        if(g->stT == 0 && ((g->tick >> 3) & 1)) ctext(46, "OK = MENUE", 1);
    }
}
