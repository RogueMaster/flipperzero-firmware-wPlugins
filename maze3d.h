#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <furi.h>
#include <input/input.h>

// 屏幕尺寸
#define SCREEN_W 128
#define SCREEN_H 64
// Framebuffer: 单色 XBM, 每字节8像素, 128/8=16 字节/行, 64 行 = 1024 字节
#define FB_BYTES (SCREEN_W * SCREEN_H / 8)

// 迷宫最大尺寸
#define MAP_MAX 32

// 地图格类型
typedef enum {
    CELL_EMPTY = 0,
    WALL_BRICK = 1,   // 砖墙
    WALL_STONE = 2,   // 石墙
    WALL_METAL = 3,   // 金属墙
    WALL_VINE = 4,    // 藤蔓墙(后期关卡)
    CELL_EXIT = 9,    // 出口
    CELL_KEY = 10,    // 钥匙
    CELL_DOOR = 11,   // 门(需钥匙)
    CELL_TORCH = 12,  // 火把(收集得分)
    CELL_TRAP = 13,   // 陷阱
} CellType;

// 道具类型
typedef enum {
    ITEM_NONE = 0,
    ITEM_KEY = 1,
    ITEM_TORCH = 2,
} ItemType;

// 敌人/NPC 状态
typedef struct {
    float x, y;          // 位置(格子坐标)
    float dir_x, dir_y;  // 朝向
    bool active;
    int type;             // 0=敌人(攻击), 1=NPC游客(无害)
    int cooldown;         // 移动冷却(帧)
} Actor;

#define MAX_ACTORS 8

// 玩家状态
typedef struct {
    float x, y;          // 位置(格子坐标)
    float dir_x, dir_y;  // 朝向单位向量
    float plane_x, plane_y; // 摄像机平面(FOV)
    int keys;            // 持有钥匙数
    int torches;         // 持有火把数
    int health;          // 生命值(仅关卡模式20+关用)
} Player;

// 游戏模式
typedef enum {
    MODE_MENU = 0,
    MODE_CAMPAIGN,    // 模式一: 关卡闯关
    MODE_ENDLESS_VISITOR, // 无尽-游客模式
    MODE_ENDLESS_RUN,     // 无尽-无尽游玩
    MODE_PAUSED,
    MODE_LEVEL_CLEAR,
    MODE_GAME_OVER,
    MODE_WIN,
} GameMode;

// 关卡阶段(决定难度与是否含道具/敌人)
typedef enum {
    STAGE_MAZE_ONLY = 0,  // 1-10关: 纯迷宫
    STAGE_PUZZLE = 1,     // 10-20关: 道具+解谜(无敌人)
    STAGE_COMBAT = 2,     // 20+关: 道具+敌人
} LevelStage;

// 游戏状态(全局单例)
typedef struct {
    GameMode mode;
    uint8_t map[MAP_MAX * MAP_MAX]; // 地图数据
    int map_w, map_h;                // 当前地图尺寸
    Player player;
    Actor actors[MAX_ACTORS];
    int actor_count;
    int level;          // 当前关卡/层
    int campaign_cleared; // 关卡模式已通关数(存档)
    int endless_floor;    // 无尽游玩当前层(存档)
    int stage;            // 当前阶段
    bool has_exit;        // 本关是否有出口目标
    bool need_redraw;
    // 渲染用
    uint8_t fb[FB_BYTES];
    // 提示文本(临时显示)
    char message[32];
    int message_ttl;     // 剩余显示帧数
} GameState;

extern GameState g;

// ---- 模块接口 ----
// engine.c: raycasting 渲染
void engine_render(void);

// maze.c: 迷宫生成
void maze_generate(int w, int h, int level, unsigned int seed);
int  maze_cell_index(int x, int y);
uint8_t maze_get(int x, int y);
void maze_set(int x, int y, uint8_t v);

// game.c: 游戏逻辑
void game_init_campaign(int level);
void game_init_endless(int floor, bool visitor);
void game_handle_input(InputKey key, InputType type);
void game_update(void);
void game_next_level(void);
bool player_move(float dx, float dy);
void player_rotate(float angle);
void actors_update(void);
void spawn_actor(float x, float y, int type);

// storage.c: 存档
void storage_load(void);
void storage_save(void);

// textures.c: 贴图
extern const uint8_t TEXTURES[][8];
#define TEX_COUNT 4
