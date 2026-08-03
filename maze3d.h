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
    CELL_POTION = 14,   // 药水(捡起后入物品栏, 恢复HP)
    CELL_AMULET = 15,   // 护符(捡起后入物品栏, 传送回起点)
} CellType;

// 物品栏物品类型
typedef enum {
    ITEM_KEY = 0,
    ITEM_TORCH,
    ITEM_POTION,
    ITEM_AMULET,
    ITEM_COUNT,
} ItemType;

// 任务系统
typedef enum {
    TASK_NONE = 0,
    TASK_FIND_EXIT,     // 找到出口
    TASK_GET_KEY,       // 获得钥匙
    TASK_OPEN_DOOR,     // 开门
    TASK_KILL_ENEMY,    // 消灭敌人
    TASK_REACH_FLOOR,   // 到达指定楼层(无尽)
    TASK_SURVIVE,       // 存活 N 秒
    TASK_COUNT,
} TaskType;

#define MAX_SUBTASKS 2
typedef struct {
    TaskType type;
    int target;     // 目标值
    int progress;   // 当前进度
    bool done;      // 是否完成
} SubTask;

typedef struct {
    bool active;            // 本关是否有任务
    SubTask subs[MAX_SUBTASKS];
    int  sub_count;
    bool all_done;          // 全部子任务完成
    bool reward_given;      // 奖励是否已发放
} Quest;

// 敌人/NPC
typedef struct {
    float x, y;
    bool active;
    uint8_t type;       // 0=敌人 1=NPC游客
    uint8_t cooldown;
    uint8_t hp;         // 敌人血量(1-3), 0=死亡
} Actor;

#define MAX_ACTORS 8

typedef struct {
    float x, y;
    float dir_x, dir_y;
    float plane_x, plane_y;
    int keys;
    int torches;
    int potions;
    int amulets;
    int health;
    int max_health;
} Player;

typedef enum {
    MODE_MENU = 0,
    MODE_CAMPAIGN,          // 剧情模式(原关卡模式)
    MODE_ENDLESS_VISITOR,
    MODE_ENDLESS_RUN,
    MODE_PAUSED,
    MODE_LEVEL_CLEAR,
    MODE_GAME_OVER,
    MODE_STORY,             // 剧情文本展示
    MODE_INVENTORY,         // 物品栏
    MODE_LEVEL_SELECT,      // 层级选择
    MODE_MAP_PANEL,         // 小地图面板(长按OK呼出)
    MODE_OPENING,           // 开场动画
    MODE_SETTINGS,          // 设置
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
    // 物品栏
    uint8_t inv_sel;        // 光标位置 0..ITEM_COUNT-1
    // 剧情文本
    uint8_t story_id;       // 当前剧情段 id
    uint8_t story_page;     // 当前页
    uint8_t story_choice;   // 玩家选择 (0=A 1=B)
    GameMode story_return;  // 剧情结束后回到哪个 mode
    // 层级选择
    uint8_t ls_sel;         // 选中的层级 (1..)
    uint8_t ls_max;         // 可选层级上限
    uint8_t ls_offset;      // 滚动偏移 (首个可见层级, 1..)
    bool ls_for_campaign;   // true=剧情模式选层 false=无尽模式选层
    // HUD 显示: 游戏中默认隐藏, 长按 OK 切换
    bool show_hud;
    // 平滑旋转: 目标角度(由输入设置), game_update 每帧往目标角度插值
    float turn_target;      // 累积待插值旋转量(弧度)
    float move_fwd_target;  // 累积待插值前进速度 (格/tick)
    float move_bwd_target;  // 累积待插值后退速度
    float move_dash_target; // 待插值前冲 (OK 键)
    // 任务系统
    Quest quest;
    int  task_kill_count;   // 累计击杀(本关)
    int  task_open_door;    // 是否已开门
    int  task_get_key;      // 本关累计捡到的钥匙
    int  task_survive_secs; // 本关存活秒数
    // 开场动画
    uint8_t opening_stage;   // 0:logo渐入 1:副标题 2:结束
    uint8_t opening_tick;    // 开场动画tick
    // 设置
    bool sfx_enabled;       // 音效开关
    bool opening_enabled;   // 开场动画开关
    bool show_debug;        // 调试信息显示开关
    // 开发模式: 隐藏序列解锁, 解锁后所有关卡可玩 + 设置显示调试项
    bool dev_mode;
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

// 物品栏: 使用选中物品, 返回是否消耗
bool item_use(int item_type);
// 物品栏当前持有数
int  item_count(int item_type);

// ---- 音效系统 ----
typedef enum {
    SFX_NONE = 0,
    SFX_MENU_MOVE,    // 菜单切换
    SFX_MENU_OK,      // 菜单确认
    SFX_PICK_KEY,     // 拾取钥匙
    SFX_PICK_ITEM,    // 拾取药水/火把/护符
    SFX_OPEN_DOOR,    // 开门
    SFX_NEED_KEY,     // 无钥匙(失败)
    SFX_TRAP,         // 陷阱
    SFX_DAMAGE,       // 受伤
    SFX_ATTACK_HIT,   // 命中敌人
    SFX_ENEMY_KILL,   // 击杀敌人
    SFX_QUEST_DONE,   // 任务完成
    SFX_LEVEL_CLEAR,  // 过关
    SFX_GAME_OVER,    // 阵亡
    SFX_STORY_TURN,   // 剧情翻页
    SFX_COUNT,
} SfxType;

void sfx_init(void);
void sfx_deinit(void);
void sfx_play(SfxType t);
void sfx_stop_all(void);
// 由 tick 更新 (每 120ms 被 main loop 调用即可)
void sfx_tick_update(void);
// 开场 BGM (长旋律, 独立于 SFX 通道)
void sfx_bgm_play(void);
void sfx_bgm_stop(void);
// BGM 是否仍在播放
bool sfx_bgm_playing(void);

// 设置默认值 (加载前)
void settings_defaults(void);

void storage_load(void);
void storage_save(void);

// 剧情: 返回剧情段总页数, 取指定页文本(英文), 取选项A/B文本
int  story_pages(int story_id);
const char* story_page_text(int story_id, int page);
const char* story_choice_a(int story_id);
const char* story_choice_b(int story_id);
const char* story_title(int story_id);

extern const uint8_t TEXTURES[][8];
#define TEX_COUNT 4
