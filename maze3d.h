#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <furi.h>
#include <input/input.h>

// 屏幕尺寸
#define SCREEN_W 128
#define SCREEN_H 64
// 渲染半分辨率列数(性能): 每列输出 2 像素宽度,减少一半光线投射
#define RENDER_COLS 64
// Framebuffer: 单色 XBM
#define FB_BYTES (SCREEN_W * SCREEN_H / 8)

// 迷宫最大尺寸
#define MAP_MAX 31

// 地图格类型
typedef enum {
    CELL_EMPTY = 0,
    WALL_BRICK = 1,
    WALL_STONE = 2,
    WALL_METAL = 3,
    WALL_VINE  = 4,
    CELL_EXIT  = 9,
    CELL_KEY   = 10,
    CELL_DOOR  = 11,
    CELL_TORCH = 12,
    CELL_TRAP  = 13,
} CellType;

// 敌人/NPC
typedef struct {
    float x, y;
    bool active;
    uint8_t type;       // 0=敌人 1=NPC游客
    uint8_t cooldown;
} Actor;

#define MAX_ACTORS 8

typedef struct {
    float x, y;
    float dir_x, dir_y;
    float plane_x, plane_y;
    int keys;
    int torches;
    int health;
} Player;

typedef enum {
    MODE_MENU = 0,
    MODE_CAMPAIGN,
    MODE_ENDLESS_VISITOR,
    MODE_ENDLESS_RUN,
    MODE_PAUSED,
    MODE_LEVEL_CLEAR,
    MODE_GAME_OVER,
} GameMode;

typedef enum {
    STAGE_MAZE_ONLY = 0,
    STAGE_PUZZLE    = 1,
    STAGE_COMBAT    = 2,
} LevelStage;

typedef struct {
    GameMode mode;
    uint8_t map[MAP_MAX * MAP_MAX];
    int map_w, map_h;
    Player player;
    Actor actors[MAX_ACTORS];
    int actor_count;
    int level;
    int campaign_cleared;
    int endless_floor;
    int stage;
    bool has_exit;
    bool dirty;   // 需要重渲染
    uint8_t fb[FB_BYTES];
    // 出口坐标(每关生成后缓存,避免每帧扫描)
    int exit_x, exit_y;
    bool exit_found;
    // 闪烁帧计数
    uint8_t tick;
    // 中文消息索引(>=0 指向 msg_id 到中文位图)
    int msg_id;
    uint8_t msg_ttl;
    // 语言: 0=中文 (XBM 位图), 1=English (canvas_draw_str)
    uint8_t lang;
} GameState;

extern GameState g;

// 中文消息 id (对应 zh_chars.h 位图 msg_*)
enum {
    MSG_NONE = -1,
    MSG_KEY = 0,
    MSG_TORCH,
    MSG_TRAP,
    MSG_DOOR,
    MSG_NEEDKEY,
    MSG_FINDEXIT,
    MSG_CARE,
    MSG_PUZZLE,
    MSG_VISITOR,
    MSG_RUN,
    MSG_HIT,
    MSG_EXIT,
};

// ---- 模块接口 ----
void engine_render(void);

void maze_generate(int w, int h, int level, unsigned int seed);
int  maze_cell_index(int x, int y);
uint8_t maze_get(int x, int y);
void maze_set(int x, int y, uint8_t v);
uint32_t maze_rng_next(void);

void game_init_campaign(int level);
void game_init_endless(int floor, bool visitor);
void game_handle_input(InputKey key, InputType type);
void game_update(void);
void game_next_level(void);
bool player_move(float dx, float dy);
void player_rotate(float angle);
void actors_update(void);
void spawn_actor(float x, float y, int type);
void set_msg(int id); // 设置中文提示

void storage_load(void);
void storage_save(void);

extern const uint8_t TEXTURES[][8];
#define TEX_COUNT 4
