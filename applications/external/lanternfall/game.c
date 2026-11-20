// Lanternfall for Flipper Zero: world, simulation, menus, saves. Pure C, no Furi.
#include "game.h"
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------- tables

const char* const WPN_NAME[4] = {"Stick", "Iron sword", "Steel sword", "Lantern blade"};
static const uint8_t WPN_ATK[4] = {0, 2, 5, 9};
static const uint16_t WPN_COST[4] = {0, 40, 140, 360};
const char* const ARM_NAME[4] = {"Tunic", "Leather", "Chain mail", "Warden plate"};
static const uint8_t ARM_DEF[4] = {0, 1, 3, 6};
static const uint16_t ARM_COST[4] = {0, 30, 120, 320};
const char* const LAMP_NAME[3] = {"Candle lamp", "Oil lamp", "Storm lantern"};
const uint8_t LAMP_R[3] = {22, 28, 36};
static const uint16_t LAMP_COST[3] = {0, 50, 160};
const char* const LOOT_NAME[L_COUNT] = {"Slime gel", "Wolf pelt", "Boar tusk", "Shade ember"};
static const uint8_t LOOT_PRICE[L_COUNT] = {2, 5, 6, 12};
const char* const MON_NAME[6] = {"", "slime", "wolf", "boar", "shade", "villager"};

typedef struct {
    uint8_t hp, atk, def, mv, xp, cmin, cmax, aggro, wind, loot, lootp;
} MonDef;
static const MonDef MON[5] = {
    {0},
    {6, 2, 0, 7, 3, 1, 3, 4, 12, L_GEL, 50}, // slime: slow hop, long wind-up
    {10, 3, 1, 3, 6, 2, 5, 7, 7, L_PELT, 40}, // wolf: fast, hunts in pairs
    {16, 4, 2, 5, 8, 3, 6, 5, 9, L_TUSK, 40}, // boar: charges in a straight line
    {14, 5, 1, 4, 12, 4, 9, 9, 8, L_EMBER, 50}, // shade: night only, drifts through trees
};

// Names and dialogue from the web game's lore.js, so the same folk live here.
static const char* const GIVEN[] = {"Mei",   "Oren",    "Tamsin", "Hollis",   "Wren",   "Isolde",
                                    "Bram",  "Juniper", "Kit",    "Ottilie",  "Rowan",  "Sable",
                                    "Edda",  "Fen",     "Lio",    "Marigold", "Corin",  "Ada",
                                    "Teo",   "Nell",    "Pip",    "Yara",     "Ansel",  "Hana",
                                    "Agnes", "Alder",   "Bryony", "Clem",     "Elowen", "Emrys",
                                    "Greer", "Hester",  "Ivo",    "Kestrel",  "Linnet", "Merrin",
                                    "Orla",  "Posy",    "Senna",  "Tobin",    "Verity", "Wynn",
                                    "Briar", "Esme",    "Lark",   "Thea"};
static const char* const TRADE[] = {
    "the baker",
    "the tea master",
    "the weaver",
    "the potter",
    "the miller",
    "the herbalist",
    "the lamplighter",
    "the farmer",
    "the carpenter",
    "the fishmonger",
    "the beekeeper",
    "the cobbler"};
static const char* const HELLO[] = {
    "Welcome to {v}.",
    "Oh, hello there.",
    "Afternoon, traveller.",
    "Good to see a new face in {v}.",
    "Mind the puddles."};
static const char* const VILLAGER[] = {
    "The lanterns round the square keep the shades away after dark. Stay near the light.",
    "Slimes get into the cabbages if you let them. Squash a few on your way past, would you?",
    "They say {v} was built around the well. It has never once run dry.",
    "Wolves hunt in packs in the deep woods. One is a nuisance; three is a problem.",
    "Boars charge when they spot you from a distance. Step aside at the last moment.",
    "If you hear thunder, count until the flash. Storms move fast across these hills.",
    "Walk far enough and you will find another village. There is always another village.",
    "Nobody remembers who lit the first lantern. We just keep them burning.",
    "Rest by the well if you are hurt. Nothing hostile sets foot inside {v}.",
    "The notice board by the well always has work for a steady hand."};
static const char* const NIGHT[] = {
    "Late to be out walking. The shades drift in from the fields after dusk.",
    "Mind the dark past the fences. Things move out there at night."};
static const char* const RAINL[] = {
    "Rain again. Good for the hydrangeas, bad for my knees.",
    "Listen to it on the roof tiles. Best sound there is."};
static const char* const PRE[] = {"Willow",   "Lantern", "Mist",   "Amber",   "Cedar",   "Heron",
                                  "Moss",     "Rain",    "Pine",   "Ember",   "Plum",    "Stone",
                                  "Reed",     "Dusk",    "Fern",   "Sorrel",  "Thistle", "Hazel",
                                  "Kettle",   "Lark",    "Alder",  "Bramble", "Copper",  "Elder",
                                  "Foxglove", "Gorse",   "Hollin", "Ivy",     "Juniper", "Linden"};
static const char* const SUF[] = {"mere",  "ford",  "by",    "wick",   "hollow", "stead", "cross",
                                  "brook", "fall",  "haven", "gate",   "dale",   "combe", "well",
                                  "field", "hythe", "leigh", "thorpe", "worth",  "bury"};
#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

// Village layout, centred on the well at (11, 9). Houses are 2x2 ('H'/'T' anchor, 'h' the rest).
static const char* const TPL[VIL_H] = {
    "...........=...........", "..Hh.......=.......Th..", "..hh..L....=....L..hh..",
    ".FFFF......=......FFFF.", ".ffff..Th..=..Hh..cccc.", ".cccc..hh..=..hh..ffff.",
    ".ffff......=......cccc.", ".FFFF..L.#####.L..FFFF.", "........##B#S##........",
    "==========#W#==========", "........#######........", ".FFFF..L.#####.L.......",
    ".ffff......=......Hh...", ".cccc..Hh..=......hh...", ".ffff..hh..=..Th.......",
    ".FFFF......=..hh..L....", "..L..Th....=...........", ".....hh....=....Hh.....",
    "...........=....hh.....",
};

// ---------------------------------------------------------------- world

uint32_t lf_hash(uint32_t seed, int32_t x, int32_t y, uint32_t salt) {
    uint32_t h = seed ^ (salt * 0x9E3779B9u);
    h ^= (uint32_t)x * 0x85EBCA6Bu;
    h = (h ^ (h >> 13)) * 0xC2B2AE35u;
    h ^= (uint32_t)y * 0x27D4EB2Fu;
    h = (h ^ (h >> 15)) * 0x165667B1u;
    return h ^ (h >> 16);
}

static int vnoise(uint32_t seed, int x, int y, int s, uint32_t salt) {
    int gx = x / s, gy = y / s;
    int fx = (x % s) * 256 / s, fy = (y % s) * 256 / s;
    fx = fx * fx * (768 - 2 * fx) >> 16; // smoothstep in 0..256
    fy = fy * fy * (768 - 2 * fy) >> 16;
    int a = lf_hash(seed, gx, gy, salt) & 255, b = lf_hash(seed, gx + 1, gy, salt) & 255;
    int c = lf_hash(seed, gx, gy + 1, salt) & 255, d = lf_hash(seed, gx + 1, gy + 1, salt) & 255;
    int top = a + (b - a) * fx / 256, bot = c + (d - c) * fx / 256;
    return top + (bot - top) * fy / 256;
}

static int height_at(uint32_t seed, int x, int y) {
    int h = (vnoise(seed, x, y, 32, 1) * 5 + vnoise(seed, x, y, 12, 2) * 3 +
             vnoise(seed, x, y, 5, 3) * 2) /
            10;
    int e = x < y ? x : y;
    if(WORLD - 1 - x < e) e = WORLD - 1 - x;
    if(WORLD - 1 - y < e) e = WORLD - 1 - y;
    if(e < 30) h -= (30 - e) * 5; // ocean all round
    int ax = x - WORLD / 2, ay = y - WORLD / 2;
    if(ax < 0) ax = -ax;
    if(ay < 0) ay = -ay;
    int d = ax > ay ? ax : ay;
    if(d < 26) h += (26 - d) * 3; // dry land round the first village
    return h;
}

uint8_t world_base(uint32_t seed, int x, int y) {
    if(x < 0 || y < 0 || x >= WORLD || y >= WORLD) return T_DEEP;
    int h = height_at(seed, x, y);
    if(h < 88) return T_DEEP;
    if(h < 103) return T_WATER;
    int r = lf_hash(seed, x, y, 9) & 1023;
    if(h < 112) return r < 14 ? T_ROCK : T_SAND;
    if(h > 196) return r < 300 ? T_ROCK : r < 420 ? T_TALL : T_GRASS;
    int m = vnoise(seed, x, y, 20, 4);
    if(m > 150) { // woods
        if(r < 430) return h > 165 ? T_PINE : T_TREE;
        if(r < 500) return T_BUSH;
        if(r < 530) return T_TALL;
        return T_GRASS;
    }
    if(m < 80) { // dry heath
        if(r < 30) return T_ROCK;
        if(r < 80) return T_FLOWER;
        if(r < 150) return T_TALL;
        return T_GRASS;
    }
    if(vnoise(seed, x, y, 6, 5) > 182) return r < 40 ? T_FLOWER : T_TALL;
    if(r < 22) return T_TREE;
    if(r < 40) return T_BUSH;
    if(r < 75) return T_FLOWER;
    return T_GRASS;
}

static bool
    base_land(uint32_t seed, int x, int y) { // same as world_base() not being water, but cheaper
    return x >= 0 && y >= 0 && x < WORLD && y < WORLD && height_at(seed, x, y) >= 103;
}

static void village_compute(uint32_t seed, int cx, int cy, Village* v);

// Villages are looked up for every tile drawn and every step taken, so keep the last few cells.
void village_in_cell(uint32_t seed, int cx, int cy, Village* v) {
    static struct {
        uint32_t seed;
        int16_t cx, cy;
        bool used;
        Village v;
    } cache[6];
    static uint8_t next;
    for(int i = 0; i < 6; i++)
        if(cache[i].used && cache[i].seed == seed && cache[i].cx == cx && cache[i].cy == cy) {
            *v = cache[i].v;
            return;
        }
    village_compute(seed, cx, cy, v);
    cache[next].seed = seed, cache[next].cx = cx, cache[next].cy = cy, cache[next].used = true;
    cache[next].v = *v;
    next = (next + 1) % 6;
}

static void village_compute(uint32_t seed, int cx, int cy, Village* v) {
    memset(v, 0, sizeof(*v));
    if(cx < 0 || cy < 0 || cx * CELL >= WORLD || cy * CELL >= WORLD) return;
    uint32_t h = lf_hash(seed, cx, cy, 77);
    v->h = h;
    v->id = (uint16_t)(cy * 8 + cx + 1);
    if(cx == WORLD / 2 / CELL &&
       cy == WORLD / 2 / CELL) { // the first village, where every journey starts
        v->ok = true;
        v->x = WORLD / 2, v->y = WORLD / 2;
        return;
    }
    if(h % 100 >= 80) return;
    // a few tries to find dry ground for the whole layout
    static const int8_t probe[9][2] = {
        {0, 0}, {-11, -9}, {11, -9}, {-11, 9}, {11, 9}, {0, -9}, {0, 9}, {-11, 0}, {11, 0}};
    for(int k = 0; k < 5; k++, h = lf_hash(seed, cx, cy, 78 + k)) {
        v->x = cx * CELL + 12 + (h >> 8) % 25;
        v->y = cy * CELL + 10 + (h >> 16) % 29;
        v->mirror = (h >> 24) & 3;
        bool dry = true;
        for(int i = 0; i < 9 && dry; i++)
            dry = base_land(seed, v->x + probe[i][0], v->y + probe[i][1]);
        if(dry) {
            v->ok = true;
            return;
        }
    }
}

bool village_at(uint32_t seed, int x, int y, Village* v) {
    if(x < 0 || y < 0 || x >= WORLD || y >= WORLD) return false;
    Village tmp;
    if(!v) v = &tmp;
    village_in_cell(seed, x / CELL, y / CELL, v);
    if(!v->ok) return false;
    int dx = x - v->x, dy = y - v->y;
    return dx >= -VIL_W / 2 && dx <= VIL_W / 2 && dy >= -VIL_H / 2 && dy <= VIL_H / 2;
}

void village_name(const Village* v, char* out, int n) {
    if(v->x == WORLD / 2 && v->y == WORLD / 2) {
        snprintf(out, n, "Lanternfall");
        return;
    }
    snprintf(out, n, "%s%s", PRE[v->h % COUNT(PRE)], SUF[(v->h >> 5) % COUNT(SUF)]);
}

static char village_char(const Village* v, int x, int y) {
    int lx = x - v->x + VIL_W / 2, ly = y - v->y + VIL_H / 2;
    if(v->mirror & 1) lx = VIL_W - 1 - lx;
    if(v->mirror & 2) ly = VIL_H - 1 - ly;
    return TPL[ly][lx];
}

uint8_t world_tile(uint32_t seed, int x, int y) {
    Village v;
    if(!village_at(seed, x, y, &v)) return world_base(seed, x, y);
    switch(village_char(&v, x, y)) {
    case '=':
        return T_PATH;
    case '#':
        return T_PLAZA;
    case 'H':
    case 'T':
    case 'h':
        return T_HOUSE;
    case 'L':
        return T_LANTERN;
    case 'W':
        return T_WELL;
    case 'S':
        return T_STALL;
    case 'B':
        return T_BOARD;
    case 'F':
        return T_FENCE;
    case 'f':
        return T_FARM;
    case 'c':
        return T_CROP;
    default: {
        uint8_t t = world_base(seed, x, y);
        return (t == T_TALL || t == T_FLOWER) ? t : T_GRASS;
    }
    }
}

int world_villages(uint32_t seed) {
    int n = 0;
    for(int cy = 0; cy * CELL < WORLD; cy++)
        for(int cx = 0; cx * CELL < WORLD; cx++) {
            Village v;
            village_in_cell(seed, cx, cy, &v);
            n += v.ok;
        }
    return n;
}

bool tile_blocks(uint8_t t) {
    switch(t) {
    case T_SAND:
    case T_GRASS:
    case T_TALL:
    case T_FLOWER:
    case T_PATH:
    case T_PLAZA:
    case T_FARM:
    case T_CROP:
        return false;
    default:
        return true;
    }
}

bool house_anchor(uint32_t seed, int x, int y) {
    return world_tile(seed, x, y) == T_HOUSE && world_tile(seed, x - 1, y) != T_HOUSE &&
           world_tile(seed, x, y - 1) != T_HOUSE;
}

// ---------------------------------------------------------------- helpers

static uint32_t rnd(Game* g) {
    g->rng ^= g->rng << 13;
    g->rng ^= g->rng >> 17;
    g->rng ^= g->rng << 5;
    return g->rng;
}
static int rndi(Game* g, int n) {
    return n > 0 ? (int)(rnd(g) % (uint32_t)n) : 0;
}

static const int8_t FDX[4] = {0, 0, -1, 1}, FDY[4] = {1, -1, 0, 0};
static int key_face(uint8_t key) {
    switch(key) {
    case K_UP:
        return F_UP;
    case K_DOWN:
        return F_DOWN;
    case K_LEFT:
        return F_LEFT;
    case K_RIGHT:
        return F_RIGHT;
    }
    return -1;
}
static int face_to(int dx, int dy) {
    int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    if(ax >= ay) return dx < 0 ? F_LEFT : F_RIGHT;
    return dy < 0 ? F_UP : F_DOWN;
}
static int iabs(int v) {
    return v < 0 ? -v : v;
}

static void toast(Game* g, const char* s) {
    snprintf(g->toast, sizeof(g->toast), "%s", s);
    g->toast_t = 45;
}

void ent_px(const Ent* e, int* x, int* y) {
    int off = e->mvlen ? e->mv * TILE / e->mvlen : 0;
    *x = e->x * TILE - e->dx * off;
    *y = e->y * TILE - e->dy * off;
}
void game_hero_px(const Game* g, int* x, int* y) {
    ent_px(&g->pl, x, y);
}

static void
    part(Game* g, uint8_t kind, int wx, int wy, int vx, int vy, uint8_t life, int16_t val) {
    Part* best = &g->parts[0];
    for(int i = 0; i < MAX_PART; i++) {
        if(g->parts[i].kind == P_NONE) {
            best = &g->parts[i];
            break;
        }
        if(g->parts[i].life < best->life) best = &g->parts[i];
    }
    *best = (Part){
        .kind = kind, .life = life, .x = wx * 4, .y = wy * 4, .vx = vx, .vy = vy, .val = val};
}

static Ent* ent_at(Game* g, int x, int y) {
    for(int i = 0; i < MAX_ENT; i++)
        if(g->ents[i].type && g->ents[i].x == x && g->ents[i].y == y) return &g->ents[i];
    return NULL;
}
static bool occupied(Game* g, int x, int y) {
    return (g->pl.x == x && g->pl.y == y) || ent_at(g, x, y);
}
static Ent* ent_free(Game* g) {
    for(int i = 0; i < MAX_ENT; i++)
        if(!g->ents[i].type) return &g->ents[i];
    return NULL;
}

int game_atk(const Game* g) {
    return 2 + g->lvl + WPN_ATK[g->wpn];
}
int game_def(const Game* g) {
    return (g->lvl - 1) / 2 + ARM_DEF[g->arm];
}
int game_xp_need(const Game* g) {
    return 8 + g->lvl * g->lvl * 6;
}

uint8_t game_darkness(const Game* g) {
    if(g->bolt && (g->opt & OPT_FLASH)) return 0;
    int c = (int)((uint32_t)g->clock * 100 / DAY_LEN), d;
    if(c < 55)
        d = 0;
    else if(c < 65)
        d = (c - 55) * 16 / 10;
    else if(c < 90)
        d = 16;
    else
        d = (100 - c) * 16 / 10;
    return (uint8_t)d;
}
bool game_is_night(const Game* g) {
    int c = (int)((uint32_t)g->clock * 100 / DAY_LEN);
    return c >= 60 && c < 95;
}

static void mark_explored(Game* g) {
    for(int y = g->pl.y - 5; y <= g->pl.y + 5; y += 4)
        for(int x = g->pl.x - 9; x <= g->pl.x + 9; x += 4) {
            if(x < 0 || y < 0 || x >= WORLD || y >= WORLD) continue;
            int i = (y / 8) * 32 + x / 8;
            g->explored[i >> 3] |= 1 << (i & 7);
        }
}

// ---------------------------------------------------------------- game

void game_new(Game* g, uint32_t seed) {
    memset(g, 0, sizeof(*g));
    g->seed = seed ? seed : 1;
    g->rng = g->seed * 2654435761u | 1;
    g->lvl = 1;
    g->pl.maxhp = g->pl.hp = 20;
    g->pl.x = g->home_x = WORLD / 2;
    g->pl.y = g->home_y = WORLD / 2 + 2;
    g->pl.face = F_DOWN;
    g->bread = 2;
    g->coins = 10;
    g->clock = DAY_LEN * 8 / 100;
    g->rain_t = FPS * 50;
    g->ui = UI_TITLE;
    g->vil_id = -1;
    g->queued = -1;
    g->dlg_pages = 1;
    g->dlg_ret = UI_PLAY;
    g->opt = OPT_DEFAULT;
    mark_explored(g);
}

static void level_check(Game* g) {
    while(g->xp >= game_xp_need(g) && g->lvl < 30) {
        g->xp -= game_xp_need(g);
        g->lvl++;
        g->pl.maxhp += 5;
        g->pl.hp = g->pl.maxhp;
        toast(g, "LEVEL UP!");
        g->sfx |= SFX_LEVEL;
    }
}

static void quest_offer(
    const Game* g,
    int vil,
    uint8_t* type,
    uint8_t* need,
    uint16_t* coins,
    uint16_t* xp) {
    uint32_t h = lf_hash(g->seed, vil, g->q_count, 31);
    int kinds = g->lvl >= 3 ? 4 : 3;
    *type = 1 + h % kinds;
    *need = 3 + (h >> 8) % 4;
    *coins = *need * (3 + 2 * g->lvl) + (*type == E_SHADE ? 10 : 0);
    *xp = *need * (2 + 2 * g->lvl);
}

static void kill(Game* g, Ent* e) {
    const MonDef* d = &MON[e->type];
    int ex, ey;
    ent_px(e, &ex, &ey);
    int coins = (d->cmin + rndi(g, d->cmax - d->cmin + 1)) * (2 + e->lvl) / 3;
    g->coins += coins;
    g->xp += d->xp * e->lvl;
    if(rndi(g, 100) < d->lootp && g->loot[d->loot] < 99) g->loot[d->loot]++;
    if(e->type == E_SLIME && rndi(g, 100) < 12 && g->bread < 9)
        g->bread++; // found in the cabbages
    g->kills[e->type]++;
    if(g->q_active && g->q_type == e->type && g->q_have < g->q_need) {
        g->q_have++;
        if(g->q_have == g->q_need) {
            toast(g, "QUEST DONE!");
            g->sfx |= SFX_QUEST;
        }
    }
    part(g, P_POOF, ex + 4, ey + 4, 0, 0, 10, 0);
    part(g, P_COIN, ex + 2, ey - 2, 0, -2, 22, coins);
    e->type = E_NONE;
    g->sfx |= SFX_KILL | SFX_COIN;
    level_check(g);
}

static bool mon_can_walk(Game* g, Ent* e, int x, int y) {
    if(x < 1 || y < 1 || x >= WORLD - 1 || y >= WORLD - 1 || occupied(g, x, y)) return false;
    uint8_t t = world_base(g->seed, x, y);
    if(e->type == E_SHADE && (t == T_TREE || t == T_PINE || t == T_BUSH || t == T_ROCK))
        t = T_GRASS;
    if(tile_blocks(t)) return false;
    return !village_at(g->seed, x, y, NULL); // nothing hostile sets foot inside a village
}

static void ent_step(Ent* e, int dx, int dy, int frames) {
    e->x += dx, e->y += dy;
    e->dx = dx, e->dy = dy;
    e->mv = e->mvlen = frames;
    e->anim++;
}

static void hit_mon(Game* g, Ent* e) {
    const MonDef* d = &MON[e->type];
    int def = d->def + (e->lvl - 1) / 3, atk = game_atk(g);
    int dmg = atk - def + rndi(g, atk / 3 + 1);
    bool crit = rndi(g, 8) == 0;
    if(crit) dmg *= 2;
    if(e->state == S_STUN) dmg += dmg / 2; // a dazed boar is an easy target
    if(dmg < 1) dmg = 1;
    e->hp -= dmg;
    e->flash = 4;
    int ex, ey;
    ent_px(e, &ex, &ey);
    part(g, crit ? P_CRIT : P_NUM, ex + 2, ey - 2, 0, -3, 18, dmg);
    part(g, P_SPARK, ex + 2, ey + 2, 0, 0, 4, 0);
    g->hitstop = crit ? 4 : 2;
    g->shake = crit ? 4 : 2;
    g->sfx |= crit ? SFX_CRIT : SFX_HIT;
    if(e->hp <= 0) {
        kill(g, e);
        return;
    }
    if(e->state == S_WIND) e->state = S_IDLE, e->timer = 8; // staggered out of the attack
    if(e->state != S_CHARGE) {
        int dx = FDX[g->pl.face], dy = FDY[g->pl.face];
        if(mon_can_walk(g, e, e->x + dx, e->y + dy)) {
            uint8_t f = e->face;
            ent_step(e, dx, dy, 3);
            e->face = f;
        }
    }
}

static void die(Game* g) {
    int lost = g->coins / 5;
    g->coins -= lost;
    g->ui = UI_DEAD;
    g->idle = false;
    snprintf(g->dlg_text, sizeof(g->dlg_text), "LOST %d COINS", lost);
    g->sfx |= SFX_DIE;
}

static void hurt_hero(Game* g, int atk, int dx, int dy) {
    if(g->inv || g->in_village || g->ui != UI_PLAY) return;
    int dmg = atk - game_def(g) + rndi(g, 2);
    if(dmg < 1) dmg = 1;
    g->pl.hp -= dmg;
    g->calm = 0;
    g->inv = 16;
    g->shake = 4;
    g->sfx |= SFX_HURT;
    int hx, hy;
    game_hero_px(g, &hx, &hy);
    part(g, P_NUM, hx + 2, hy - 2, 0, -3, 18, -dmg);
    if(g->pl.mv == 0 && (dx || dy)) { // knocked back
        int nx = g->pl.x + dx, ny = g->pl.y + dy;
        if(!tile_blocks(world_tile(g->seed, nx, ny)) && !occupied(g, nx, ny)) {
            uint8_t f = g->pl.face;
            ent_step(&g->pl, dx, dy, 3);
            g->pl.face = f;
        }
    }
    if(g->pl.hp <= 0) {
        g->pl.hp = 0;
        die(g);
    }
}

static void wake(Game* g) {
    g->pl.hp = g->pl.maxhp;
    g->pl.x = g->home_x, g->pl.y = g->home_y;
    g->pl.mv = 0;
    g->pl.face = F_DOWN;
    for(int i = 0; i < MAX_ENT; i++)
        if(g->ents[i].type != E_VILLAGER) g->ents[i].type = E_NONE;
    g->inv = 40;
    g->ui = UI_PLAY;
    g->want_save = true;
}

static bool clear_line(Game* g, Ent* e, int dx, int dy, int n) {
    for(int i = 1; i < n; i++)
        if(!mon_can_walk(g, e, e->x + dx * i, e->y + dy * i)) return false;
    return true;
}

static void mon_think(Game* g, Ent* e) {
    const MonDef* d = &MON[e->type];
    int pdx = g->pl.x - e->x, pdy = g->pl.y - e->y, dist = iabs(pdx) + iabs(pdy);
    switch(e->state) {
    case S_FADE:
        if(--e->timer == 0) e->type = E_NONE;
        return;
    case S_STUN:
        if(e->timer == 0 || --e->timer == 0) e->state = S_IDLE;
        return;
    case S_WIND:
        e->face = face_to(pdx, pdy);
        if(e->timer == 0 || --e->timer == 0) {
            if(dist == 1) hurt_hero(g, d->atk + e->lvl - 1, pdx, pdy);
            e->state = S_IDLE;
            e->timer = 12;
        }
        return;
    case S_CHARGE:
        if(e->timer) {
            e->timer--;
            return;
        }
        if(e->x + e->dx == g->pl.x && e->y + e->dy == g->pl.y) {
            hurt_hero(g, (d->atk + e->lvl - 1) * 3 / 2, e->dx, e->dy);
            e->state = S_STUN, e->timer = 24;
        } else if(e->steps < 8 && mon_can_walk(g, e, e->x + e->dx, e->y + e->dy)) {
            ent_step(e, e->dx, e->dy, 2);
            e->steps++;
        } else {
            int ex, ey;
            ent_px(e, &ex, &ey);
            part(g, P_SPARK, ex + 2 + e->dx * 4, ey + 2 + e->dy * 4, 0, 0, 5, 0);
            e->state = S_STUN, e->timer = e->steps < 8 ? 30 : 12; // crashed into something
            g->shake = 2;
        }
        return;
    }
    if(e->timer) {
        e->timer--;
        return;
    }
    if(!g->in_village && dist <= d->aggro) {
        e->face = face_to(pdx, pdy);
        if(dist == 1) {
            e->state = S_WIND, e->timer = d->wind;
            return;
        }
        if(e->type == E_BOAR && (pdx == 0 || pdy == 0) && dist >= 2 && dist <= 6) {
            int dx = pdx ? (pdx > 0 ? 1 : -1) : 0, dy = pdy ? (pdy > 0 ? 1 : -1) : 0;
            if(clear_line(g, e, dx, dy, dist)) {
                e->state = S_CHARGE, e->timer = 10, e->steps = 0;
                e->dx = dx, e->dy = dy;
                return;
            }
        }
        int sx = pdx ? (pdx > 0 ? 1 : -1) : 0, sy = pdy ? (pdy > 0 ? 1 : -1) : 0;
        bool xfirst = iabs(pdx) > iabs(pdy) || (iabs(pdx) == iabs(pdy) && (g->tick & 1));
        int ax = xfirst ? sx : 0, ay = xfirst ? 0 : sy;
        int bx = xfirst ? 0 : sx, by = xfirst ? sy : 0;
        if((ax || ay) && mon_can_walk(g, e, e->x + ax, e->y + ay))
            ent_step(e, ax, ay, d->mv);
        else if((bx || by) && mon_can_walk(g, e, e->x + bx, e->y + by))
            ent_step(e, bx, by, d->mv);
        else
            e->timer = 4;
        if(e->mv) e->face = face_to(e->dx, e->dy);
        return;
    }
    if(rndi(g, 3) == 0) {
        int f = rndi(g, 4);
        if(mon_can_walk(g, e, e->x + FDX[f], e->y + FDY[f])) {
            ent_step(e, FDX[f], FDY[f], d->mv + d->mv / 2);
            e->face = f;
        }
    }
    e->timer = 8 + rndi(g, 30);
}

static void villager_think(Game* g, Ent* e) {
    if(e->timer) {
        e->timer--;
        return;
    }
    int f = rndi(g, 4), nx = e->x + FDX[f], ny = e->y + FDY[f];
    Village v;
    if(village_at(g->seed, nx, ny, &v) && v.x == e->home_x && v.y == e->home_y &&
       !tile_blocks(world_tile(g->seed, nx, ny)) && !occupied(g, nx, ny)) {
        ent_step(e, FDX[f], FDY[f], 8);
        e->face = f;
    }
    e->timer = 10 + rndi(g, 50);
}

static void spawn_monsters(Game* g) {
    int count = 0;
    for(int i = 0; i < MAX_ENT; i++)
        if(g->ents[i].type && g->ents[i].type != E_VILLAGER) count++;
    bool night = game_darkness(g) >= 12 && game_is_night(g);
    if(count >= (night ? 8 : 6)) return;
    int x = g->pl.x + rndi(g, 29) - 14, y = g->pl.y + rndi(g, 19) - 9;
    if(iabs(x - g->pl.x) <= 9 && iabs(y - g->pl.y) <= 5) return; // never pop in on screen
    uint8_t t = world_base(g->seed, x, y);
    if(tile_blocks(t) || village_at(g->seed, x, y, NULL) || occupied(g, x, y)) return;
    int type;
    if(night && rndi(g, 100) < 55)
        type = E_SHADE;
    else if(t == T_SAND)
        type = E_SLIME;
    else {
        int m = vnoise(g->seed, x, y, 20, 4);
        int r = rndi(g, 100);
        if(m > 140)
            type = r < 70 ? E_WOLF : E_SLIME;
        else if(m < 85)
            type = r < 60 ? E_BOAR : E_SLIME;
        else
            type = r < 35 ? E_BOAR : E_SLIME;
    }
    int ax = iabs(x - WORLD / 2), ay = iabs(y - WORLD / 2);
    int lvl = 1 + (ax > ay ? ax : ay) / 22 - rndi(g, 2);
    if(lvl < 1) lvl = 1;
    if(lvl > 9) lvl = 9;
    int pack = type == E_WOLF ? 2 : 1;
    for(int k = 0; k < pack; k++) {
        Ent* e = ent_free(g);
        if(!e) return;
        int px = x + k, py = y;
        if(k && (tile_blocks(world_base(g->seed, px, py)) || occupied(g, px, py) ||
                 village_at(g->seed, px, py, NULL)))
            return;
        const MonDef* d = &MON[type];
        *e = (Ent){.type = type, .lvl = lvl, .x = px, .y = py, .face = rndi(g, 4), .timer = 10};
        e->maxhp = e->hp = d->hp * (100 + 35 * (lvl - 1)) / 100;
    }
}

static void manage_villagers(Game* g) {
    for(int i = 0; i < MAX_ENT; i++) {
        Ent* e = &g->ents[i];
        if(e->type == E_VILLAGER &&
           (iabs(e->home_x - g->pl.x) > 30 || iabs(e->home_y - g->pl.y) > 30))
            e->type = E_NONE;
    }
    Village v;
    village_in_cell(g->seed, g->pl.x / CELL, g->pl.y / CELL, &v);
    if(!v.ok || iabs(v.x - g->pl.x) > 22 || iabs(v.y - g->pl.y) > 20) return;
    int have = 0;
    for(int i = 0; i < MAX_ENT; i++)
        if(g->ents[i].type == E_VILLAGER && g->ents[i].home_x == v.x && g->ents[i].home_y == v.y)
            have++;
    if(have >= 4) return;
    int x = v.x + rndi(g, VIL_W) - VIL_W / 2, y = v.y + rndi(g, VIL_H) - VIL_H / 2;
    uint8_t t = world_tile(g->seed, x, y);
    if((t != T_PATH && t != T_PLAZA) || occupied(g, x, y)) return;
    Ent* e = ent_free(g);
    if(!e) return;
    *e = (Ent){
        .type = E_VILLAGER, .x = x, .y = y, .home_x = v.x, .home_y = v.y, .hp = 1, .timer = 20};
    e->seed = (uint16_t)lf_hash(g->seed, v.id, have, 88);
}

static void weather(Game* g) {
    if(g->rain_t) g->rain_t--;
    if(!g->rain_t) {
        if(g->rain) {
            g->rain = 0;
            g->rain_t = FPS * (90 + rndi(g, 120));
        } else {
            g->rain = 1 + rndi(g, 3);
            g->rain_t = FPS * (40 + rndi(g, 60));
        }
    }
    if(g->rain == 3 && rndi(g, FPS * 7) == 0) {
        g->bolt = 3;
        g->thunder_in = 6 + rndi(g, 24);
    }
}

static void talk(Game* g, Ent* e) {
    uint16_t s = e->seed;
    e->face = face_to(g->pl.x - e->x, g->pl.y - e->y);
    e->timer = 80;
    Village v;
    village_in_cell(g->seed, e->home_x / CELL, e->home_y / CELL, &v);
    char vname[24];
    village_name(&v, vname, sizeof(vname));
    snprintf(
        g->dlg_name,
        sizeof(g->dlg_name),
        "%s, %s",
        GIVEN[s % COUNT(GIVEN)],
        TRADE[(s >> 6) % COUNT(TRADE)]);
    const char* parts[3] = {
        HELLO[(s >> 3) % COUNT(HELLO)],
        NULL,
        VILLAGER[(s + g->tick / (FPS * 60)) % COUNT(VILLAGER)]};
    if(game_is_night(g))
        parts[1] = NIGHT[s % COUNT(NIGHT)];
    else if(g->rain)
        parts[1] = RAINL[s % COUNT(RAINL)];
    int o = 0;
    for(int p = 0; p < 3; p++) {
        if(!parts[p]) continue;
        if(o && o < (int)sizeof(g->dlg_text) - 1) g->dlg_text[o++] = ' ';
        for(const char* c = parts[p]; *c && o < (int)sizeof(g->dlg_text) - 1; c++) {
            if(c[0] == '{' && c[1] == 'v' && c[2] == '}') {
                for(const char* n = vname; *n && o < (int)sizeof(g->dlg_text) - 1; n++)
                    g->dlg_text[o++] = *n;
                c += 2;
            } else
                g->dlg_text[o++] = *c;
        }
    }
    g->dlg_text[o] = 0;
    g->dlg_page = 0;
    g->dlg_pages = 1; // the device measures the real page count
    g->dlg_ret = UI_PLAY;
    g->ui = UI_TALK;
    g->sfx |= SFX_MENU;
}

static void drink(Game* g) {
    g->pl.hp = g->pl.maxhp;
    toast(g, "COLD CLEAR WATER");
    g->sfx |= SFX_EAT;
}

static void swing(Game* g) {
    g->swing = 6;
    g->atk_cd = 9;
    g->sfx |= SFX_SWING;
}

static void action(Game* g) {
    int tx = g->pl.x + FDX[g->pl.face], ty = g->pl.y + FDY[g->pl.face];
    Ent* e = ent_at(g, tx, ty);
    if(e && e->type == E_VILLAGER && g->pl.mv == 0) {
        talk(g, e);
        return;
    }
    if(!e && g->pl.mv == 0) {
        uint8_t t = world_tile(g->seed, tx, ty);
        if(t == T_STALL || t == T_BOARD) {
            g->ui = t == T_STALL ? UI_SHOP : UI_BOARD;
            g->cursor = 0;
            g->sfx |= SFX_MENU;
            return;
        }
        if(t == T_WELL) {
            drink(g);
            return;
        }
    }
    if(g->atk_cd) return;
    swing(g);
}

// ---------------------------------------------------------------- auto-attack and idle

static void eat(Game* g, bool tonic);

static bool foe(const Ent* e) {
    return e && e->type && e->type != E_VILLAGER && e->state != S_FADE;
}

// Auto-attack (Settings), and always while idle: when the weapon is ready, swing at a creature beside the
// hero, the one in front first. Turns the hero to face it.
static bool auto_swing(Game* g) {
    if(g->pl.mv || g->swing || g->atk_cd) return false;
    for(int k = 0; k < 4; k++) {
        int f = (g->pl.face + k) & 3;
        if(!foe(ent_at(g, g->pl.x + FDX[f], g->pl.y + FDY[f]))) continue;
        g->pl.face = f;
        swing(g);
        return true;
    }
    return false;
}

// Whether the hero can stand on a tile, cached: the world never changes and world_tile() is slow on the
// Flipper (~40 us), so a path search mostly reads this instead. 2 bits a tile: 0 unknown, 1 open, 2 solid.
static uint8_t walk_cache[WORLD * WORLD / 4];
static uint32_t walk_seed;
static bool solid(uint32_t seed, int x, int y) {
    if(x < 0 || y < 0 || x >= WORLD || y >= WORLD) return true;
    if(seed != walk_seed) memset(walk_cache, 0, sizeof(walk_cache)), walk_seed = seed;
    int i = y * WORLD + x, sh = (i & 3) * 2, v = (walk_cache[i >> 2] >> sh) & 3;
    if(!v) {
        v = tile_blocks(world_tile(seed, x, y)) ? 2 : 1;
        walk_cache[i >> 2] |= v << sh;
    }
    return v == 2;
}

#define WMAX (2 * IDLE_WIDE + 1)
#define QN   2048 // ring queue: a search's frontier stays far smaller than this
static uint8_t bfs_from[WMAX * WMAX]; // 0 unseen, 1..4 reached by step F_* + 1, 5 start, 6 solid
static uint16_t bfs_q[QN];

// Breadth-first search towards (gx, gy) over the terrain within r tiles of the hero (creatures are ignored,
// they move). A goal out of the window or out of reach gets a path to the reachable tile closest to it, so
// each leg of a long trip ends strictly nearer. beside: the goal itself is solid (a creature, the well), so
// stop next to it. Fills g->path, returns how far from the goal it ends (0 = there), or -1 when no step
// gets any closer.
static int plan_r(Game* g, int gx, int gy, bool beside, int r) {
    int wn = 2 * r + 1, ox = g->pl.x - r, oy = g->pl.y - r, start = r * wn + r,
        want = beside ? 1 : 0;
    memset(bfs_from, 0, wn * wn);
    int qh = 0, qt = 0, best = start, bd = iabs(gx - g->pl.x) + iabs(gy - g->pl.y);
    bfs_from[start] = 5;
    bfs_q[qt++ % QN] = start;
    while(qh < qt) {
        int c = bfs_q[qh++ % QN], cx = c % wn, cy = c / wn;
        int d = iabs(gx - ox - cx) + iabs(gy - oy - cy);
        if(d < bd) bd = d, best = c;
        if(d <= want) break;
        for(int f = 0; f < 4 && qt - qh < QN - 1; f++) {
            int nx = cx + FDX[f], ny = cy + FDY[f], n = ny * wn + nx;
            if(nx < 0 || ny < 0 || nx >= wn || ny >= wn || bfs_from[n]) continue;
            if(solid(g->seed, ox + nx, oy + ny)) {
                bfs_from[n] = 6;
                continue;
            }
            bfs_from[n] = 1 + f;
            bfs_q[qt++ % QN] = n;
        }
    }
    g->path_n = g->path_i = 0;
    if(best == start) return bd <= want ? 0 : -1;
    int len = 0;
    for(int c = best; c != start; len++) {
        int f = bfs_from[c] - 1;
        c -= FDY[f] * wn + FDX[f];
    }
    g->path_n = len > IDLE_PATH ?
                    IDLE_PATH :
                    len; // ponytail: a longer path is cut short and planned again there
    for(int c = best, i = len; c != start;) {
        int f = bfs_from[c] - 1;
        if(--i < IDLE_PATH) g->path[i] = f;
        c -= FDY[f] * wn + FDX[f];
    }
    return bd;
}

// wide: a long trip, worth a search four times the size when the near one is boxed in by water
static int plan(Game* g, int gx, int gy, bool beside, bool wide) {
    int d = plan_r(g, gx, gy, beside, IDLE_WIN);
    if(d < 0 && wide) d = plan_r(g, gx, gy, beside, IDLE_WIDE);
    return d;
}

static int ring_lvl(int x, int y) { // the level creatures spawn at out here (see spawn_monsters)
    int ax = iabs(x - WORLD / 2), ay = iabs(y - WORLD / 2);
    return 1 + (ax > ay ? ax : ay) / 22;
}

static bool chunk_flag(const uint8_t* bits, int i) {
    return bits[i >> 3] & (1 << (i & 7));
}

// Roaming: the nearest chunk of the map not seen yet, on land, where the creatures are no more than a level
// above the hero's own. Sets the goal to a walkable tile in it.
static bool roam_pick(Game* g) {
    static const int8_t S[5][2] = {{4, 4}, {2, 2}, {5, 2}, {2, 5}, {5, 5}};
    int hx = g->pl.x / 8, hy = g->pl.y / 8;
    for(int r = 0; r < 32; r++)
        for(int cy = hy - r; cy <= hy + r; cy++)
            for(int cx = hx - r; cx <= hx + r;
                cx += (cy == hy - r || cy == hy + r || cx == hx + r) ? 1 : 2 * r) {
                if(cx < 0 || cy < 0 || cx >= 32 || cy >= 32) continue;
                int i = cy * 32 + cx;
                if(chunk_flag(g->explored, i) || chunk_flag(g->idle_skip, i)) continue;
                if(ring_lvl(cx * 8 + 4, cy * 8 + 4) > g->lvl + 1) continue;
                for(int k = 0; k < 5; k++) {
                    int x = cx * 8 + S[k][0], y = cy * 8 + S[k][1];
                    if(solid(g->seed, x, y)) continue;
                    g->goal = G_ROAM, g->goal_x = x, g->goal_y = y, g->goal_d = 9999, g->stuck = 0;
                    return true;
                }
                g->idle_skip[i >> 3] |= 1 << (i & 7); // all water or rock
            }
    return false;
}

#define IDLE_R    12 // near: how far from where idle began the hero wanders
#define IDLE_HUNT 8 // how far the hero goes after a creature

static void idle_wander(Game* g) {
    if(iabs(g->pl.x - g->idle_x) + iabs(g->pl.y - g->idle_y) > IDLE_R + 8) {
        g->goal = G_BACK, g->goal_x = g->idle_x, g->goal_y = g->idle_y;
        if(plan(g, g->goal_x, g->goal_y, false, true) < 0)
            g->idle_x = g->pl.x, g->idle_y = g->pl.y; // can't get back: start here
        return;
    }
    for(int k = 0; k < 8; k++) {
        int x = g->idle_x + rndi(g, 2 * IDLE_R + 1) - IDLE_R,
            y = g->idle_y + rndi(g, 2 * IDLE_R + 1) - IDLE_R;
        if(solid(g->seed, x, y) || (x == g->pl.x && y == g->pl.y)) continue;
        if(plan(g, x, y, false, false) < 0) continue;
        g->goal = G_WANDER, g->goal_x = x, g->goal_y = y;
        return;
    }
}

// What to do next, as the web game's idle does: when badly hurt eat, or with no food sit it out until mended
// (fighting back only), fight the nearest creature the hero can take, and otherwise wander near where idle
// began, or roam off to country not seen yet.
static void idle_decide(Game* g) {
    Ent* pl = &g->pl;
    if(pl->hp * 100 < pl->maxhp * 35) {
        if(g->tonic && (pl->hp * 100 < pl->maxhp * 20 || !g->bread))
            eat(g, true);
        else if(g->bread)
            eat(g, false);
        else if(!g->rest)
            g->rest = true, toast(g, "IDLE: RESTING");
    }
    if(g->rest) {
        if(pl->hp * 100 > pl->maxhp * 85) g->rest = false;
        g->path_n = 0;
        return;
    }
    // the nearest creature in reach that is no more than a level above the hero
    Ent* best = NULL;
    int bd = IDLE_HUNT + 1;
    for(int i = 0; i < MAX_ENT; i++) {
        Ent* e = &g->ents[i];
        if(!foe(e) || e->lvl > g->lvl + 1 || (g->no_hunt_t && i == g->no_hunt_i)) continue;
        if(!(g->opt & OPT_ROAM) && iabs(e->x - g->idle_x) + iabs(e->y - g->idle_y) > IDLE_R + 4)
            continue;
        int d = iabs(e->x - pl->x) + iabs(e->y - pl->y);
        if(d < bd) bd = d, best = e;
    }
    if(best) {
        if(bd == 1) return; // auto_swing has it
        if(g->goal != G_HUNT || g->goal_x != best->x || g->goal_y != best->y ||
           g->path_i >= g->path_n) {
            g->goal = G_HUNT, g->goal_x = best->x, g->goal_y = best->y;
            if(plan(g, best->x, best->y, true, false) >= 0) return;
            g->goal = G_NONE; // across water: leave it be for a while
            g->no_hunt_i = (uint8_t)(best - g->ents), g->no_hunt_t = 100;
        } else
            return;
    }
    if(g->goal == G_HUNT) g->goal = G_NONE, g->path_n = 0;
    if(g->path_i < g->path_n) return; // still on the way
    if(g->opt & OPT_ROAM) {
        if(g->goal == G_ROAM) {
            int ci = (g->goal_y / 8) * 32 + g->goal_x / 8;
            if(chunk_flag(g->explored, ci))
                g->goal = G_NONE;
            else {
                int d = plan(g, g->goal_x, g->goal_y, false, true);
                if(d >= 0 && d < g->goal_d)
                    g->goal_d = d, g->stuck = 0;
                else if(d < 0 || ++g->stuck > 3) { // no nearer: a lake or a cliff in the way
                    g->idle_skip[ci >> 3] |= 1 << (ci & 7);
                    g->goal = G_NONE;
                }
                return;
            }
        }
        if(g->idle_wait) return;
        if(roam_pick(g)) {
            g->idle_x = g->goal_x, g->idle_y = g->goal_y;
            return;
        }
        g->idle_x = pl->x, g->idle_y = pl->y; // everything in reach is seen: potter about here
    }
    if(g->goal == G_WANDER || g->goal == G_BACK) { // arrived: look round a moment
        g->goal = G_NONE;
        g->idle_wait = 20 + rndi(g, 40);
        return;
    }
    if(!g->idle_wait) idle_wander(g);
}

static void idle_tick(Game* g) {
    if(!g->idle) return;
    if(g->idle_wait) g->idle_wait--;
    if(g->no_hunt_t) g->no_hunt_t--;
    if(auto_swing(g) || g->pl.mv || g->swing) return;
    if(g->idle_think) g->idle_think--;
    if(!g->idle_think || g->path_i >= g->path_n) {
        g->idle_think = 8;
        idle_decide(g);
        if(!g->idle) return;
    }
    if(g->path_i >= g->path_n) return;
    int f = g->path[g->path_i], nx = g->pl.x + FDX[f], ny = g->pl.y + FDY[f];
    if(ent_at(g, nx, ny)) { // a creature (auto_swing will see to it) or a villager in the way
        if(++g->stuck > 12) g->path_n = 0, g->stuck = 0;
        return;
    }
    g->queued =
        f; // hero_update takes the step this tick: the tile is open and nothing stands on it
    g->path_i++;
}

void game_set_idle(Game* g, bool on, const char* why) {
    if(on == g->idle) return;
    g->idle = on;
    g->goal = G_NONE, g->path_n = g->path_i = 0, g->idle_think = 0, g->idle_wait = 0, g->stuck = 0;
    g->idle_x = g->pl.x, g->idle_y = g->pl.y;
    g->rest = false;
    memset(g->idle_skip, 0, sizeof(g->idle_skip));
    toast(g, why ? why : on ? "IDLE: MOVE TO TAKE OVER" : "IDLE OFF");
}

static void hero_update(Game* g) {
    if(g->inv) g->inv--;
    if(g->atk_cd) g->atk_cd--;
    if(g->swing) {
        g->swing--;
        if(g->swing == 4) {
            Ent* e = ent_at(g, g->pl.x + FDX[g->pl.face], g->pl.y + FDY[g->pl.face]);
            if(e && e->type != E_VILLAGER && e->state != S_FADE) hit_mon(g, e);
        }
    }
    if(g->pl.mv) {
        g->pl.mv--;
        return;
    }
    if(g->swing) return;
    int f = g->queued;
    g->queued = -1;
    if(f < 0) {
        static const uint8_t order[4] = {K_UP, K_DOWN, K_LEFT, K_RIGHT};
        for(int i = 0; i < 4 && f < 0; i++)
            if(g->held & (1 << order[i])) f = key_face(order[i]);
    }
    if(f < 0) return;
    g->pl.face = f;
    int nx = g->pl.x + FDX[f], ny = g->pl.y + FDY[f];
    uint8_t t = world_tile(g->seed, nx, ny);
    if(tile_blocks(t) || occupied(g, nx, ny)) return;
    ent_step(&g->pl, FDX[f], FDY[f], 4);
    g->pl.face = f;
    if(t == T_SAND || t == T_FARM || (g->rain && (t == T_PATH || t == T_GRASS))) {
        g->foot[g->foot_i].x = nx, g->foot[g->foot_i].y = ny;
        g->foot[g->foot_i].age = 0, g->foot[g->foot_i].face = f;
        g->foot_i = (g->foot_i + 1) % MAX_FOOT;
    }
    if(t == T_TALL && rndi(g, 3) == 0)
        part(g, P_LEAF, nx * TILE + 2 + rndi(g, 4), ny * TILE + 2, rndi(g, 3) - 1, -2, 14, 0);
    mark_explored(g);
}

static void village_update(Game* g) {
    Village v;
    bool in = village_at(g->seed, g->pl.x, g->pl.y, &v);
    if(in && !g->in_village) {
        village_name(&v, g->banner, sizeof(g->banner));
        g->banner_t = 60;
        g->home_x = v.x;
        g->home_y = v.y + ((v.mirror & 2) ? -1 : 1);
        g->want_save = true;
    }
    g->in_village = in;
    g->vil_id = in ? v.id : -1;
    if(g->calm < 255) g->calm++;
    // as in the web game: out of a fight for five seconds, the hero mends slowly anywhere
    if(!in && g->calm >= FPS * 5 && g->pl.hp < g->pl.maxhp && g->tick % FPS == 0) g->pl.hp++;
    if(in && g->pl.hp < g->pl.maxhp && g->tick % 15 == 0) {
        g->pl.hp++;
        if(g->tick % 60 == 0) {
            int hx, hy;
            game_hero_px(g, &hx, &hy);
            part(g, P_HEART, hx + 2, hy - 4, 0, -1, 20, 0);
        }
    }
}

static void parts_update(Game* g) {
    for(int i = 0; i < MAX_PART; i++) {
        Part* p = &g->parts[i];
        if(!p->kind) continue;
        p->x += p->vx, p->y += p->vy;
        if((p->kind == P_NUM || p->kind == P_CRIT || p->kind == P_COIN) && p->vy < 0 &&
           (g->tick & 3) == 0)
            p->vy++;
        if(p->kind == P_LEAF && (g->tick & 1)) p->vy++;
        if(--p->life == 0) p->kind = P_NONE;
    }
    for(int i = 0; i < MAX_FOOT; i++)
        if(g->foot[i].age < 255) g->foot[i].age++;
}

void game_tick(Game* g) {
    g->tick++;
    if(g->toast_t) g->toast_t--;
    if(g->banner_t) g->banner_t--;
    if(g->bolt) g->bolt--;
    if(g->shake) g->shake--;
    if(g->thunder_in && --g->thunder_in == 0) g->sfx |= SFX_THUNDER;
    if(g->ui != UI_PLAY) return;
    if(g->hitstop) {
        g->hitstop--;
        return;
    }
    g->play_ticks++;
    g->clock = (g->clock + 1) % DAY_LEN;
    weather(g);
    if(g->idle)
        idle_tick(g);
    else if(g->opt & OPT_AUTO)
        auto_swing(g);
    hero_update(g);
    if(g->ui != UI_PLAY) return;
    village_update(g);
    bool day = game_darkness(g) < 8 || !game_is_night(g);
    for(int i = 0; i < MAX_ENT; i++) {
        Ent* e = &g->ents[i];
        if(!e->type) continue;
        if(e->flash) e->flash--;
        if(e->mv) {
            e->mv--;
            continue;
        }
        if(e->type == E_VILLAGER) {
            villager_think(g, e);
            continue;
        }
        if(iabs(e->x - g->pl.x) > 18 || iabs(e->y - g->pl.y) > 13) {
            e->type = E_NONE;
            continue;
        }
        if(e->type == E_SHADE && day && e->state != S_FADE) e->state = S_FADE, e->timer = 12;
        mon_think(g, e);
        if(g->ui != UI_PLAY) return;
    }
    if(g->tick % 25 == 0) spawn_monsters(g);
    if(g->tick % 20 == 0) manage_villagers(g);
    parts_update(g);
}

// ---------------------------------------------------------------- menus

static const struct {
    const char* name;
    uint16_t bit;
    const char *on, *off;
} SETTING[] = {
    {"Auto-attack", OPT_AUTO, "On", "Off"},
    {"Idle range", OPT_ROAM, "Roam", "Near"},
    {"Sound", OPT_SOUND, "On", "Off"},
    {"Vibration", OPT_VIBRO, "On", "Off"},
    {"LED", OPT_LED, "On", "Off"},
    {"Backlight", OPT_LIGHT, "Always", "Auto"},
    {"Screen shake", OPT_SHAKE, "On", "Off"},
    {"Lightning", OPT_FLASH, "Flash", "No flash"},
    {"Rain", OPT_RAIN, "Shown", "Hidden"},
};
#define N_SETTING (int)COUNT(SETTING)

static const char CONTROLS[] =
    "D-pad: walk, hold to keep going. OK: attack, talk, trade, read a notice board, drink at a well. "
    "Hold OK: eat bread. Back: this menu. Hold Back: save and quit. Idle walks, fights and mends "
    "by itself (Roam goes off to find unseen country); press OK or the d-pad to take over.";

int game_menu(const Game* g, int i, char* out, int n) {
    char tmp[2];
    if(!out) out = tmp, n = sizeof(tmp);
    switch(g->ui) {
    case UI_TITLE: {
        static const char* const a[] = {"CONTINUE", "NEW JOURNEY"};
        if(!g->has_save) {
            if(i == 0) snprintf(out, n, "BEGIN");
            return 1;
        }
        if(i >= 0 && i < 2) snprintf(out, n, "%s", a[i]);
        return 2;
    }
    case UI_PAUSE: {
        static const char* const a[] = {
            "Resume", "Go idle", "Bag", "Hero", "Map", "Settings", "Save & quit"};
        if(i >= 0 && i < 7) snprintf(out, n, "%s", i == 1 && g->idle ? "Stop idle" : a[i]);
        return 7;
    }
    case UI_SETTINGS: {
        // '\t' splits the label from the value; the device right-aligns the value
        if(i >= 0 && i < N_SETTING)
            snprintf(
                out,
                n,
                "%s\t%s",
                SETTING[i].name,
                (g->opt & SETTING[i].bit) ? SETTING[i].on : SETTING[i].off);
        if(i == N_SETTING) snprintf(out, n, "Controls");
        return N_SETTING + 1;
    }
    case UI_BAG: {
        int k = 0;
        if(i == k++) snprintf(out, n, "Bread x%d  (+8 HP)", g->bread);
        if(i == k++) snprintf(out, n, "Tonic x%d  (full HP)", g->tonic);
        for(int l = 0; l < L_COUNT; l++)
            if(i == k++) snprintf(out, n, "%s x%d", LOOT_NAME[l], g->loot[l]);
        if(i == k++) snprintf(out, n, "Wpn: %s", WPN_NAME[g->wpn]);
        if(i == k++) snprintf(out, n, "Arm: %s", ARM_NAME[g->arm]);
        if(i == k++) snprintf(out, n, "Lamp: %s", LAMP_NAME[g->lamp]);
        return k;
    }
    case UI_HERO: {
        int k = 0;
        uint32_t mins = g->clock * 24 * 60 / DAY_LEN + 6 * 60; // the day starts at dawn
        if(i == k++) snprintf(out, n, "Level %d   XP %d/%d", g->lvl, g->xp, game_xp_need(g));
        if(i == k++) snprintf(out, n, "HP %d/%d   Coins %d", g->pl.hp, g->pl.maxhp, g->coins);
        if(i == k++) snprintf(out, n, "Attack %d   Defence %d", game_atk(g), game_def(g));
        if(i == k++) snprintf(out, n, "%s, %s", WPN_NAME[g->wpn], ARM_NAME[g->arm]);
        if(i == k++) snprintf(out, n, "Lamp: %s", LAMP_NAME[g->lamp]);
        if(i == k++)
            snprintf(out, n, "Slain: %d slime %d wolf", g->kills[E_SLIME], g->kills[E_WOLF]);
        if(i == k++)
            snprintf(out, n, "       %d boar %d shade", g->kills[E_BOAR], g->kills[E_SHADE]);
        if(i == k++)
            snprintf(
                out,
                n,
                "Day %lu, %02lu:%02lu",
                (unsigned long)(g->play_ticks / DAY_LEN + 1),
                (unsigned long)(mins / 60 % 24),
                (unsigned long)(mins % 60));
        return k;
    }
    case UI_SHOP: {
        int k = 0, sell = 0;
        for(int l = 0; l < L_COUNT; l++)
            sell += g->loot[l] * LOOT_PRICE[l];
        if(i == k++) snprintf(out, n, "Bread  +8 HP        5c");
        if(i == k++) snprintf(out, n, "Tonic  full HP     25c");
        if(i == k++) {
            if(g->wpn < 3)
                snprintf(
                    out,
                    n,
                    "%s +%d  %dc",
                    WPN_NAME[g->wpn + 1],
                    WPN_ATK[g->wpn + 1] - WPN_ATK[g->wpn],
                    WPN_COST[g->wpn + 1]);
            else
                snprintf(out, n, "Weapons: sold out");
        }
        if(i == k++) {
            if(g->arm < 3)
                snprintf(
                    out,
                    n,
                    "%s +%d  %dc",
                    ARM_NAME[g->arm + 1],
                    ARM_DEF[g->arm + 1] - ARM_DEF[g->arm],
                    ARM_COST[g->arm + 1]);
            else
                snprintf(out, n, "Armour: sold out");
        }
        if(i == k++) {
            if(g->lamp < 2)
                snprintf(out, n, "%s  %dc", LAMP_NAME[g->lamp + 1], LAMP_COST[g->lamp + 1]);
            else
                snprintf(out, n, "Lamps: sold out");
        }
        if(i == k++) snprintf(out, n, "Sell all loot   +%dc", sell);
        return k;
    }
    case UI_BOARD: {
        uint8_t type, need;
        uint16_t coins, xp;
        if(!g->q_active) {
            quest_offer(g, g->vil_id, &type, &need, &coins, &xp);
            if(i == 0) snprintf(out, n, "Accept: hunt %d %ss", need, MON_NAME[type]);
            if(i == 1) snprintf(out, n, "Reward %dc, %dxp", coins, xp);
            if(i == 2) snprintf(out, n, "Leave");
            return 3;
        }
        if(g->q_have >= g->q_need) {
            if(i == 0) snprintf(out, n, "Claim %dc, %dxp", g->q_coins, g->q_xp);
            if(i == 1) snprintf(out, n, "Leave");
            return 2;
        }
        if(i == 0) snprintf(out, n, "Hunt %ss: %d/%d", MON_NAME[g->q_type], g->q_have, g->q_need);
        if(i == 1) snprintf(out, n, "Reward %dc, %dxp", g->q_coins, g->q_xp);
        if(i == 2) snprintf(out, n, "Abandon quest");
        if(i == 3) snprintf(out, n, "Leave");
        return 4;
    }
    }
    return 0;
}

const char* game_menu_title(const Game* g) {
    switch(g->ui) {
    case UI_PAUSE:
        return "Paused";
    case UI_BAG:
        return "Bag";
    case UI_HERO:
        return "Hero";
    case UI_SHOP:
        return "Merchant";
    case UI_SETTINGS:
        return "Settings";
    case UI_BOARD:
        return "Notice board";
    }
    return "";
}

static void eat(Game* g, bool tonic) {
    uint8_t* n = tonic ? &g->tonic : &g->bread;
    if(!*n || g->pl.hp >= g->pl.maxhp) {
        g->sfx |= SFX_DENY;
        return;
    }
    (*n)--;
    g->pl.hp = tonic ? g->pl.maxhp : (g->pl.hp + 8 > g->pl.maxhp ? g->pl.maxhp : g->pl.hp + 8);
    int hx, hy;
    game_hero_px(g, &hx, &hy);
    part(g, P_HEART, hx + 2, hy - 4, 0, -1, 20, 0);
    g->sfx |= SFX_EAT;
}

static bool pay(Game* g, int cost) {
    if(g->coins < cost) {
        g->sfx |= SFX_DENY;
        return false;
    }
    g->coins -= cost;
    g->sfx |= SFX_BUY;
    return true;
}

static void menu_select(Game* g) {
    int c = g->cursor;
    switch(g->ui) {
    case UI_TITLE:
        if(g->has_save && c == 0) {
            g->ui = UI_PLAY;
        } else {
            bool had = g->has_save;
            uint16_t opt = g->opt;
            uint32_t seed = rnd(g) ^ g->tick;
            for(int k = 0; k < 8 && world_villages(seed) < 6; k++)
                seed = rnd(g); // a well-settled island
            game_new(g, seed);
            g->has_save = had;
            g->opt = opt;
            g->ui = UI_PLAY;
            g->want_save = true;
        }
        g->banner_t = 60;
        snprintf(g->banner, sizeof(g->banner), "Lanternfall");
        g->in_village = true;
        break;
    case UI_PAUSE:
        if(c == 0)
            g->ui = UI_PLAY;
        else if(c == 1)
            game_set_idle(g, !g->idle, NULL), g->ui = UI_PLAY;
        else if(c == 2)
            g->ui = UI_BAG;
        else if(c == 3)
            g->ui = UI_HERO;
        else if(c == 4)
            g->ui = UI_MAP;
        else if(c == 5)
            g->ui = UI_SETTINGS;
        else {
            g->want_save = true;
            g->quit = true;
        }
        if(c >= 2 && c <= 5) g->cursor = 0;
        break;
    case UI_SETTINGS:
        if(c < N_SETTING) {
            g->opt ^= SETTING[c].bit;
            g->opt_dirty = true;
            g->sfx |= SFX_MENU;
        } else {
            snprintf(g->dlg_name, sizeof(g->dlg_name), "Controls");
            snprintf(g->dlg_text, sizeof(g->dlg_text), "%s", CONTROLS);
            g->dlg_page = 0, g->dlg_pages = 1, g->dlg_ret = UI_SETTINGS;
            g->ui = UI_TALK;
        }
        break;
    case UI_BAG:
        if(c == 0)
            eat(g, false);
        else if(c == 1)
            eat(g, true);
        break;
    case UI_SHOP:
        if(c == 0 && g->bread < 9 && pay(g, 5))
            g->bread++;
        else if(c == 1 && g->tonic < 9 && pay(g, 25))
            g->tonic++;
        else if(c == 2 && g->wpn < 3 && pay(g, WPN_COST[g->wpn + 1]))
            g->wpn++;
        else if(c == 3 && g->arm < 3 && pay(g, ARM_COST[g->arm + 1]))
            g->arm++;
        else if(c == 4 && g->lamp < 2 && pay(g, LAMP_COST[g->lamp + 1]))
            g->lamp++;
        else if(c == 5) {
            int sell = 0;
            for(int l = 0; l < L_COUNT; l++)
                sell += g->loot[l] * LOOT_PRICE[l], g->loot[l] = 0;
            if(sell)
                g->coins += sell, g->sfx |= SFX_COIN;
            else
                g->sfx |= SFX_DENY;
        } else if(c <= 4)
            g->sfx |= SFX_DENY;
        if(g->sfx & SFX_BUY) g->want_save = true;
        break;
    case UI_BOARD:
        if(!g->q_active) {
            if(c == 0) {
                quest_offer(g, g->vil_id, &g->q_type, &g->q_need, &g->q_coins, &g->q_xp);
                g->q_active = 1, g->q_have = 0;
                g->sfx |= SFX_QUEST;
                g->ui = UI_PLAY;
            } else if(c == 2)
                g->ui = UI_PLAY;
        } else if(g->q_have >= g->q_need) {
            if(c == 0) {
                g->coins += g->q_coins;
                g->xp += g->q_xp;
                g->q_active = 0;
                g->q_count++;
                g->sfx |= SFX_COIN | SFX_QUEST;
                toast(g, "REWARD CLAIMED");
                level_check(g);
                g->want_save = true;
            }
            g->ui = UI_PLAY;
        } else {
            if(c == 2) g->q_active = 0, g->q_count++;
            if(c >= 2) g->ui = UI_PLAY;
        }
        break;
    }
}

void game_key(Game* g, uint8_t key, uint8_t type) {
    if(key <= K_LEFT) {
        if(type == KT_PRESS) g->held |= 1 << key;
        if(type == KT_RELEASE) g->held &= ~(1 << key);
    }
    if(key == K_BACK && type == KT_LONG && g->ui != UI_TITLE) {
        g->want_save = true;
        g->quit = true;
        return;
    }
    bool press = type == KT_PRESS, nav = press || type == KT_REPEAT;
    switch(g->ui) {
    case UI_PLAY:
        if(g->idle && (key <= K_LEFT || key == K_OK) && press)
            game_set_idle(g, false, NULL); // you take over
        if(key <= K_LEFT && press)
            g->queued = key_face(key);
        else if(key == K_OK && press)
            action(g);
        else if(key == K_OK && type == KT_LONG)
            eat(g, false);
        else if(key == K_BACK && press)
            g->ui = UI_PAUSE, g->cursor = 0, g->sfx |= SFX_MENU;
        return;
    case UI_TALK:
        if(key == K_OK && press) {
            if(g->dlg_page + 1 < g->dlg_pages)
                g->dlg_page++;
            else
                g->ui = g->dlg_ret;
        } else if(key == K_BACK && press)
            g->ui = g->dlg_ret;
        return;
    case UI_DEAD:
        if(key == K_OK && press) wake(g);
        return;
    case UI_MAP:
        if((key == K_OK || key == K_BACK) && press) g->ui = UI_PAUSE, g->cursor = 4;
        return;
    }
    int n = game_menu(g, -1, NULL, 0);
    if(n <= 0) return;
    if((key == K_UP || key == K_DOWN) && nav) {
        g->cursor = (g->cursor + (key == K_UP ? n - 1 : 1)) % n;
        g->sfx |= SFX_MENU;
    } else if(key == K_OK && press) {
        menu_select(g);
    } else if(
        g->ui == UI_SETTINGS && (key == K_LEFT || key == K_RIGHT) && press &&
        g->cursor < N_SETTING) {
        menu_select(g);
    } else if(key == K_BACK && press) {
        if(g->ui == UI_BAG || g->ui == UI_HERO || g->ui == UI_SETTINGS)
            g->cursor = g->ui == UI_BAG ? 2 : g->ui == UI_HERO ? 3 : 5, g->ui = UI_PAUSE;
        else if(g->ui != UI_TITLE)
            g->ui = UI_PLAY;
        else
            g->quit = true;
    }
}

// ---------------------------------------------------------------- saves

#define PUT(v, sz)                                \
    do {                                          \
        uint32_t _v = (uint32_t)(v);              \
        for(int _i = 0; _i < (sz); _i++)          \
            buf[o++] = (uint8_t)(_v >> (8 * _i)); \
    } while(0)
#define GET(dst, sz)                              \
    do {                                          \
        uint32_t _v = 0;                          \
        for(int _i = 0; _i < (sz); _i++)          \
            _v |= (uint32_t)buf[o++] << (8 * _i); \
        dst = _v;                                 \
    } while(0)

int game_save(const Game* g, uint8_t* buf) {
    int o = 0;
    PUT(SAVE_MAGIC, 4);
    PUT(g->seed, 4);
    PUT(g->clock, 2);
    PUT(g->play_ticks, 4);
    PUT(g->pl.x, 2);
    PUT(g->pl.y, 2);
    PUT(g->pl.face, 1);
    PUT(g->pl.hp, 2);
    PUT(g->pl.maxhp, 2);
    PUT(g->lvl, 1);
    PUT(g->wpn, 1);
    PUT(g->arm, 1);
    PUT(g->lamp, 1);
    PUT(g->bread, 1);
    PUT(g->tonic, 1);
    PUT(g->xp, 2);
    PUT(g->coins, 2);
    for(int i = 0; i < L_COUNT; i++)
        PUT(g->loot[i], 1);
    for(int i = 0; i < 6; i++)
        PUT(g->kills[i], 2);
    PUT(g->home_x, 2);
    PUT(g->home_y, 2);
    PUT(g->q_active, 1);
    PUT(g->q_type, 1);
    PUT(g->q_need, 1);
    PUT(g->q_have, 1);
    PUT(g->q_coins, 2);
    PUT(g->q_xp, 2);
    PUT(g->q_count, 2);
    PUT(g->rain, 1);
    PUT(g->rain_t, 2);
    for(int i = 0; i < 128; i++)
        PUT(g->explored[i], 1);
    uint16_t sum = 0;
    for(int i = 0; i < o; i++)
        sum = sum * 31 + buf[i];
    PUT(sum, 2);
    return o;
}

bool game_load(Game* g, const uint8_t* buf, int len) {
    int o = 0;
    uint32_t magic;
    if(len < 8) return false;
    GET(magic, 4);
    if(magic != SAVE_MAGIC) return false;
    Game* t = g; // fill a fresh game, then copy fields
    uint32_t seed;
    GET(seed, 4);
    game_new(t, seed); // settings are not part of the save: the device sets g->opt after loading
    GET(t->clock, 2);
    GET(t->play_ticks, 4);
    GET(t->pl.x, 2);
    GET(t->pl.y, 2);
    GET(t->pl.face, 1);
    GET(t->pl.hp, 2);
    GET(t->pl.maxhp, 2);
    GET(t->lvl, 1);
    GET(t->wpn, 1);
    GET(t->arm, 1);
    GET(t->lamp, 1);
    GET(t->bread, 1);
    GET(t->tonic, 1);
    GET(t->xp, 2);
    GET(t->coins, 2);
    for(int i = 0; i < L_COUNT; i++)
        GET(t->loot[i], 1);
    for(int i = 0; i < 6; i++)
        GET(t->kills[i], 2);
    GET(t->home_x, 2);
    GET(t->home_y, 2);
    GET(t->q_active, 1);
    GET(t->q_type, 1);
    GET(t->q_need, 1);
    GET(t->q_have, 1);
    GET(t->q_coins, 2);
    GET(t->q_xp, 2);
    GET(t->q_count, 2);
    GET(t->rain, 1);
    GET(t->rain_t, 2);
    for(int i = 0; i < 128; i++)
        GET(t->explored[i], 1);
    if(o + 2 > len) return false;
    uint16_t sum = 0, want;
    for(int i = 0; i < o; i++)
        sum = sum * 31 + buf[i];
    GET(want, 2);
    if(sum != want || t->wpn > 3 || t->arm > 3 || t->lamp > 2 || t->lvl < 1 ||
       t->clock >= DAY_LEN || t->pl.x < 1 || t->pl.y < 1 || t->pl.x >= WORLD - 1 ||
       t->pl.y >= WORLD - 1 || t->pl.face > 3 || t->pl.maxhp < 1) {
        game_new(g, 1);
        return false;
    }
    if(t->pl.hp < 1) t->pl.hp = 1;
    t->has_save = true;
    return true;
}
