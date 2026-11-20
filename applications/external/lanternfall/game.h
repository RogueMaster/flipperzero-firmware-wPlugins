// Lanternfall for Flipper Zero: pure game logic, no Furi dependencies, so test.c and
// tools/preview.c can run it on the host. lanternfall.c is the device shell.
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define TILE     8
#define SCR_W    128
#define SCR_H    64
#define WORLD    256 // tiles per side; the edges are ocean
#define FPS      20
#define DAY_LEN  (FPS * 300) // one day is five minutes
#define MAX_ENT  16
#define MAX_PART 24
#define MAX_FOOT 12
#define CELL     48 // village grid cell, in tiles
#define VIL_W    23
#define VIL_H    19

// Keys and event types mirror Flipper's InputKey / InputType values.
enum {
    K_UP,
    K_DOWN,
    K_RIGHT,
    K_LEFT,
    K_OK,
    K_BACK
};
enum {
    KT_PRESS,
    KT_RELEASE,
    KT_SHORT,
    KT_LONG,
    KT_REPEAT
};
enum {
    F_DOWN,
    F_UP,
    F_LEFT,
    F_RIGHT
};

enum {
    T_DEEP,
    T_WATER,
    T_SAND,
    T_GRASS,
    T_TALL,
    T_FLOWER,
    T_TREE,
    T_PINE,
    T_ROCK,
    T_BUSH,
    T_PATH,
    T_PLAZA,
    T_FARM,
    T_CROP,
    T_HOUSE,
    T_LANTERN,
    T_WELL,
    T_STALL,
    T_BOARD,
    T_FENCE,
};

enum {
    E_NONE,
    E_SLIME,
    E_WOLF,
    E_BOAR,
    E_SHADE,
    E_VILLAGER
};
enum {
    S_IDLE,
    S_WIND,
    S_CHARGE,
    S_STUN,
    S_FADE
};
enum {
    P_NONE,
    P_NUM,
    P_CRIT,
    P_SPARK,
    P_POOF,
    P_HEART,
    P_COIN,
    P_LEAF,
    P_TEXTUP
};

enum {
    UI_TITLE,
    UI_PLAY,
    UI_PAUSE,
    UI_BAG,
    UI_HERO,
    UI_MAP,
    UI_TALK,
    UI_SHOP,
    UI_BOARD,
    UI_DEAD,
    UI_SETTINGS,
};

// Settings (kept in their own file by the device, so a new journey keeps them)
enum {
    OPT_AUTO = 1 << 0, // auto-attack: swing at any creature beside you when the weapon is ready
    OPT_SOUND = 1 << 1,
    OPT_VIBRO = 1 << 2,
    OPT_LED = 1 << 3,
    OPT_LIGHT = 1 << 4, // backlight always on (off: the Flipper's own timeout)
    OPT_SHAKE = 1 << 5,
    OPT_FLASH = 1 << 6, // lightning lights up the screen
    OPT_RAIN = 1 << 7, // draw the rain
    OPT_ROAM = 1 << 8, // idle roams the island exploring (off: stays near where it began)
};
#define OPT_DEFAULT \
    (OPT_SOUND | OPT_VIBRO | OPT_LED | OPT_LIGHT | OPT_SHAKE | OPT_FLASH | OPT_RAIN)
#define OPT_ALL 0x1ff

enum {
    G_NONE,
    G_WANDER,
    G_HUNT,
    G_ROAM,
    G_BACK
};
#define IDLE_WIN  16 // path search reaches this many tiles each way
#define IDLE_WIDE 40 // and this far when that finds no way on
#define IDLE_PATH 255

enum {
    SFX_SWING = 1 << 0,
    SFX_HIT = 1 << 1,
    SFX_CRIT = 1 << 2,
    SFX_KILL = 1 << 3,
    SFX_HURT = 1 << 4,
    SFX_COIN = 1 << 5,
    SFX_LEVEL = 1 << 6,
    SFX_THUNDER = 1 << 7,
    SFX_BUY = 1 << 8,
    SFX_DENY = 1 << 9,
    SFX_QUEST = 1 << 10,
    SFX_EAT = 1 << 11,
    SFX_MENU = 1 << 12,
    SFX_DIE = 1 << 13,
    SFX_SAVE = 1 << 14,
};

enum {
    L_GEL,
    L_PELT,
    L_TUSK,
    L_EMBER,
    L_COUNT
};

typedef struct {
    uint8_t type, lvl, face, state, timer, flash, anim, mv, mvlen, steps;
    int16_t hp, maxhp, x, y; // x, y in tiles (the tile being walked into while moving)
    int8_t dx, dy;
    uint16_t seed; // villager identity
    int16_t home_x, home_y; // villager's village centre
} Ent;

typedef struct {
    uint8_t kind, life;
    int16_t x, y; // world quarter-pixels
    int8_t vx, vy; // quarter-pixels per tick
    int16_t val;
} Part;

typedef struct {
    int16_t x, y;
    uint8_t age, face;
} Foot;

typedef struct {
    bool ok;
    int16_t x, y; // centre tile (the well)
    uint8_t mirror; // bit0 horizontal, bit1 vertical
    uint16_t id;
    uint32_t h;
} Village;

typedef struct {
    uint32_t seed, rng, tick;
    uint16_t clock;

    // hero
    Ent pl;
    uint8_t lvl, wpn, arm, lamp, bread, tonic;
    uint16_t xp, coins;
    uint8_t loot[L_COUNT];
    uint16_t kills[6];
    uint8_t swing, atk_cd, inv, calm; // calm: ticks since last hurt, to 255
    int16_t home_x, home_y;
    uint32_t play_ticks;

    // quest
    uint8_t q_active, q_type, q_need, q_have;
    uint16_t q_coins, q_xp, q_count;

    Ent ents[MAX_ENT];
    Part parts[MAX_PART];
    Foot foot[MAX_FOOT];
    uint8_t foot_i;

    // weather and feel
    uint8_t rain, bolt, shake, hitstop, thunder_in;
    uint16_t rain_t;

    // ui
    uint8_t ui, cursor, toast_t, banner_t, dlg_page, dlg_pages, title_sel;
    bool has_save, quit, want_save, in_village;
    int16_t vil_id; // village the hero is standing in, -1 outside
    char toast[24], banner[24], dlg_name[32], dlg_text[288];
    uint8_t held; // bitmask of held d-pad keys (1 << K_*)
    int8_t queued; // tapped direction waiting to be walked, -1 none
    uint8_t explored[128]; // 32x32 grid of 8x8-tile chunks seen, for the map
    uint16_t sfx; // sound events raised this tick, the device plays and clears them

    // settings and idle
    uint16_t opt; // OPT_* bits
    bool opt_dirty; // the device writes the settings file and clears this
    uint8_t dlg_ret; // screen the dialogue box returns to
    bool idle, rest; // idle: the hero carries on by themselves, any d-pad or OK press takes over
    uint8_t idle_think, idle_wait, goal, path_n, path_i, stuck, no_hunt_i, no_hunt_t;
    int16_t idle_x, idle_y, goal_x, goal_y, goal_d;
    uint8_t path[IDLE_PATH]; // F_* steps
    uint8_t idle_skip[128]; // chunks roaming gave up on (water, out of reach)
} Game;

// world
uint32_t lf_hash(uint32_t seed, int32_t x, int32_t y, uint32_t salt);
uint8_t world_base(uint32_t seed, int x, int y); // terrain without villages
uint8_t world_tile(uint32_t seed, int x, int y);
void village_in_cell(uint32_t seed, int cx, int cy, Village* v);
bool village_at(uint32_t seed, int x, int y, Village* v); // is (x, y) inside a village's bounds
void village_name(const Village* v, char* out, int n);
bool tile_blocks(uint8_t t);
int world_villages(uint32_t seed);
bool house_anchor(uint32_t seed, int x, int y); // top-left tile of a 2x2 house

// game
void game_new(Game* g, uint32_t seed);
void game_tick(Game* g);
void game_key(Game* g, uint8_t key, uint8_t type);
void game_set_idle(Game* g, bool on, const char* why);
uint8_t game_darkness(const Game* g); // 0 day .. 16 night
bool game_is_night(const Game* g);
int game_atk(const Game* g);
int game_def(const Game* g);
int game_xp_need(const Game* g);
void game_hero_px(const Game* g, int* x, int* y); // hero's world pixel position
void ent_px(const Ent* e, int* x, int* y);

// menus: line i of the current list screen, returns the number of lines
int game_menu(const Game* g, int i, char* out, int n);
const char* game_menu_title(const Game* g);

// tables shared with the renderer
extern const char* const WPN_NAME[4];
extern const char* const ARM_NAME[4];
extern const char* const LAMP_NAME[3];
extern const uint8_t LAMP_R[3];
extern const char* const LOOT_NAME[L_COUNT];
extern const char* const MON_NAME[6];

// save blob
#define SAVE_MAGIC 0x31464E4C // "LNF1"
#define SAVE_SIZE  256
int game_save(const Game* g, uint8_t* buf); // returns bytes written (<= SAVE_SIZE)
bool game_load(Game* g, const uint8_t* buf, int len);
