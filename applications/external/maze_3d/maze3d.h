#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <furi.h>
#include <input/input.h>

// 屏幕尺寸
#define SCREEN_W    128
#define SCREEN_H    64
// 渲染半分辨率列数(性能): 每列输出 2 像素宽度,减少一半光线投射
#define RENDER_COLS 64
// Framebuffer: 单色 XBM
#define FB_BYTES    (SCREEN_W * SCREEN_H / 8)

// 迷宫最大尺寸
#define MAP_MAX 31

// v6.7-beta: 跳跃参数
#define JUMP_FRAMES 28 // 跳跃总帧数 (≈0.5s)
#define JUMP_PEAK   9.0f // 跳跃峰值像素高度

// 地图格类型
typedef enum {
    CELL_EMPTY = 0,
    WALL_BRICK = 1,
    WALL_STONE = 2,
    WALL_METAL = 3,
    WALL_VINE = 4,
    WALL_WATER = 5, // v6.0 MC: 水 (不可放置, 半透明)
    WALL_GRASS = 6, // v6.0 MC: 草地 (装饰)
    WALL_WOOD = 7, // v6.0 MC: 木板 (可挖可放)
    WALL_TREE = 8, // v6.0 MC: 树 (实体, 挖掉得木)
    CELL_EXIT = 9,
    CELL_KEY = 10,
    CELL_DOOR = 11,
    CELL_TORCH = 12,
    CELL_TRAP = 13,
    CELL_POTION = 14, // 药水(捡起后入物品栏, 恢复HP)
    CELL_AMULET = 15, // 护符(捡起后入物品栏, 传送回起点)
    CELL_LOCKED_EXIT = 16, // v6.0: 锁定出口 (任务未完成前不可通过)
    WALL_SAND = 17, // v6.3 MC: 沙子 (真正沙方块, 落地有尘土粒子)
    WALL_DIRT = 18, // v6.3 MC: 土方块
    WALL_LOG = 19, // v6.3 MC: 原木 (区别于木板 WALL_WOOD)
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
    TASK_FIND_EXIT, // 找到出口
    TASK_GET_KEY, // 获得钥匙
    TASK_OPEN_DOOR, // 开门
    TASK_KILL_ENEMY, // 消灭敌人
    TASK_REACH_FLOOR, // 到达指定楼层(无尽)
    TASK_SURVIVE, // 存活 N 秒
    TASK_COUNT,
} TaskType;

#define MAX_SUBTASKS 4
typedef struct {
    TaskType type;
    int target; // 目标值
    int progress; // 当前进度
    bool done; // 是否完成
} SubTask;

typedef struct {
    bool active; // 本关是否有任务
    SubTask subs[MAX_SUBTASKS];
    int sub_count;
    bool all_done; // 全部子任务完成
    bool reward_given; // 奖励是否已发放
} Quest;

// 敌人/NPC
typedef struct {
    float x, y;
    float tx, ty; // v6.10: 移动目标位置 (平滑插值用)
    bool active;
    uint8_t type; // 0=敌人 1=NPC游客
    uint8_t cooldown; // 移动/攻击冷却
    uint8_t hp; // 敌人血量(1-4), 0=死亡
    uint8_t fire_cd; // v6.1: 远程射击冷却 (0=可射击)
    uint8_t hurt_flash; // v6.1: 受伤闪烁帧数 (>0 时反白显示)
    uint8_t moving; // v6.10: 正在移动中 (>0 时插值 tx,ty)
} Actor;

#define MAX_ACTORS 8

// v6.1: 子弹 (玩家/敌人共用, 用 owner 区分)
#define MAX_BULLETS 12
typedef struct {
    bool active;
    float x, y; // 当前位置
    float dx, dy; // 速度方向 (单位向量 * 速度)
    uint8_t life; // 剩余生命 (帧数), 0=消失
    uint8_t owner; // 0=玩家 1=敌人
} Bullet;

// v6.2: 粒子系统 (射击火花/命中爆炸/子弹尾迹/行走尘土)
#define MAX_PARTICLES 32
typedef struct {
    bool active;
    float x, y; // 世界坐标
    float vx, vy; // 速度
    uint8_t life; // 剩余帧数, 0=消失
    uint8_t type; // 0=射击火花 1=命中爆炸 2=子弹尾迹 3=行走尘土
    uint8_t size; // 粒子大小(像素半径近似)
} Particle;

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
    MODE_CAMPAIGN, // 剧情模式(原关卡模式)
    MODE_ENDLESS_VISITOR,
    MODE_ENDLESS_RUN,
    MODE_PAUSED,
    MODE_LEVEL_CLEAR,
    MODE_GAME_OVER,
    MODE_STORY, // 剧情文本展示
    MODE_INVENTORY, // 物品栏
    MODE_LEVEL_SELECT, // 层级选择
    MODE_MAP_PANEL, // 小地图面板(长按OK呼出)
    MODE_OPENING, // 开场动画
    MODE_SETTINGS, // 设置
    MODE_MC, // MC 沙盒模式 (Beta): 小空间内挖掘/放置方块
    MODE_SHOP, // v6.10: 积分商城
    MODE_SHOP_INV, // v6.11: 新道具栏 (长按 OK 呼出)
} GameMode;

typedef enum {
    STAGE_MAZE_ONLY = 0,
    STAGE_PUZZLE = 1,
    STAGE_COMBAT = 2,
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
    bool dirty; // 需要重渲染
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
    uint8_t inv_sel; // 光标位置 0..ITEM_COUNT-1
    // 剧情文本
    uint8_t story_id; // 当前剧情段 id
    uint8_t story_page; // 当前页
    uint8_t story_choice; // 玩家选择 (0=A 1=B)
    GameMode story_return; // 剧情结束后回到哪个 mode
    // 层级选择
    uint8_t ls_sel; // 选中的层级 (1..)
    uint8_t ls_max; // 可选层级上限
    uint8_t ls_offset; // 滚动偏移 (首个可见层级, 1..)
    bool ls_for_campaign; // true=剧情模式选层 false=无尽模式选层
    // HUD 显示: 游戏中默认隐藏, 长按 OK 切换
    bool show_hud;
    // 平滑旋转: 目标角度(由输入设置), game_update 每帧往目标角度插值
    float turn_target; // 累积待插值旋转量(弧度)
    float move_fwd_target; // 累积待插值前进速度 (格/tick)
    float move_bwd_target; // 累积待插值后退速度
    float move_dash_target; // 待插值前冲 (OK 键)
    // v6.8: 全新操控 — 按键持续状态 (短按精确小步, 长按持续加速)
    int8_t turn_hold_dir; // 持续转向方向 (-1=左, 0=无, +1=右)
    uint16_t turn_hold_time; // 持续按下帧数 (驱动加速曲线)
    int8_t move_hold_dir; // 持续移动方向 (-1=后, 0=无, +1=前)
    uint16_t move_hold_time; // 持续按下帧数
    // 任务系统
    Quest quest;
    int task_kill_count; // 累计击杀(本关)
    int task_open_door; // 是否已开门
    int task_get_key; // 本关累计捡到的钥匙
    int task_survive_secs; // 本关存活秒数
    // 开场动画
    uint8_t opening_stage; // 0:logo渐入 1:副标题 2:结束
    uint8_t opening_tick; // 开场动画tick
    // 设置
    bool sfx_enabled; // 音效开关
    bool opening_enabled; // 开场动画开关
    bool show_debug; // 调试信息显示开关
    // 开发模式: 隐藏序列解锁, 解锁后所有关卡可玩 + 设置显示调试项
    bool dev_mode;
    // v6.3 MC 沙盒模式: 当前手持方块类型 (1..8), 挖到方块自动切换手持
    //   1=砖 2=石 3=木板 4=草 5=土 6=沙 7=原木 8=树叶
    uint8_t mc_block_type;
    uint8_t mc_mined;
    // v6.0 成就系统: 累计统计 (跨局保存)
    uint32_t ach_total_kills; // 累计击杀
    uint32_t ach_total_clears; // 累计过关
    uint32_t ach_total_mined; // 累计挖掘方块
    uint32_t ach_flags; // 里程碑位图 (见 ACH_* 枚举)
    // v6.0 战斗: 手枪弹药 (战斗关每关补给)
    uint8_t ammo;
    uint16_t ammo_regen_timer; // v6.10: 弹药自动回复计时器
    // v6.1: 子弹池 + 射击视觉
    Bullet bullets[MAX_BULLETS];
    uint8_t muzzle_flash; // 枪口闪光帧数 (>0 时屏幕中央画闪光)
    uint8_t hurt_flash; // 玩家受伤屏幕闪白帧数
    uint8_t shoot_kick; // 射击后坐力 (准星抖动)
    int16_t screen_shake; // 屏幕震屏量 (像素偏移)
    // v6.5: 无敌时间 + 回血系统
    uint16_t invincible_timer; // 受伤后无敌帧数 (>0 时免疫一切伤害)
    uint16_t regen_timer; // 回血计时器 (累计到阈值时 +1 血)
    // v6.2: 粒子系统
    Particle particles[MAX_PARTICLES];
    // 玩家移动痕迹 (上一帧位置, 用于尘土粒子触发)
    float prev_px, prev_py;
    // v6.7-beta: 跳跃 — 垂直偏移 (0=地面, +值=在空中), 帧计数
    float jump_z; // 当前像素级垂直偏移 (>0 表示离地)
    uint8_t jump_timer; // 跳跃已进行帧数, 0=不在跳跃中
    // v6.7-beta: 三次连续长按 Back 才退出 (防误触 beta 保护)
    uint8_t exit_long_cnt; // 连续 Back 长按计数 (0..3)
    uint16_t exit_long_ttl; // 计数有效剩余帧数, 归零则计数器重置
    // v6.9-beta: 开发者模式 20+ 设置 (数值档 0..N, 运行时转乘数)
    //   --- 操控控制 ---
    uint8_t cfg_turn_sens; // 转向灵敏度 (0..5) 档 1.0,1.25,1.5,1.75,2.0,2.5
    uint8_t cfg_turn_short; // 短按转角度数 (0..4) 档 5.7°,8.6°,11.5°,14.3°,17.2°
    uint8_t cfg_move_short; // 短按步幅 (0..4) 档 0.08,0.12,0.15,0.20,0.26
    uint8_t cfg_move_max; // 移动最大速度 (0..4) 档 0.024,0.030,0.042,0.055,0.072
    uint8_t cfg_turn_max; // 转向最大速度 (0..4) 档 0.030,0.038,0.050,0.065,0.085
    uint8_t cfg_jump_height; // 跳跃高度 (0..3) 档 0,6,9,12 像素
    uint8_t cfg_back_ratio; // 后退速度比 (0..3) 档 0.55,0.72,0.88,1.00
    //   --- 画面渲染 ---
    uint8_t cfg_density; // DDA 列密度 (0..2) 档 32,48,64 列
    bool cfg_fog; // 距离雾效开关
    uint8_t cfg_brightness; // 画面亮度 (0..4) 档 0.6,0.8,1.0,1.25,1.5
    bool cfg_sky_ceil; // 天空/天花板渲染开关
    bool cfg_floor_tex; // 地板纹理开关
    //   --- 音效 ---
    uint8_t cfg_sfx_vol; // 音量 (0..2) 档 0.5,1.0,1.5 倍
    bool cfg_sfx_menu; // 菜单切换音效开关
    bool cfg_sfx_combat; // 战斗/击杀/受伤音效开关
    //   --- 游戏参数 ---
    uint8_t cfg_maze_scale; // 迷宫尺寸缩放 (0..4) 档 0.6,0.8,1.0,1.2,1.5
    uint8_t cfg_hp_start; // 初始血量 (0..4) 档 8,10,12,16,20
    uint8_t cfg_regen_rate; // 回血倍率 (0..3) 档 0.5,1.0,2.0,3.0
    uint8_t cfg_ammo_mul; // 弹药倍率 (0..3) 档 0.5,1.0,2.0,3.0
    uint8_t cfg_endless_start; // 无尽起始楼层 (0..4) 档 1,10,25,50,99
    //   --- MC 沙盒 ---
    uint8_t cfg_mc_size; // MC 地图尺寸 (0..3) 档 11,15,19,23
    uint8_t cfg_mc_day_len; // MC 日夜时长 (0..3) 档 1024/512/256/128 tick
    bool cfg_mc_jump; // MC 跳跃开关
    uint8_t cfg_mc_start_sel; // MC 初始持有方块 0..7
    // v6.10: 积分系统 + 商城
    uint32_t score; // 当前积分 (击杀+10/只)
    uint8_t shop_sel; // 商城光标位置
    uint8_t auto_lock; // 自动锁定目标 (255=无)
    // v6.11: 道具库存 (10 种, 商城购买后存入此库存, 游戏中使用)
    //   0=血量药水(+5HP)  1=大血药(+10HP)  2=满血药剂(满血)
    //   3=弹药+5          4=弹药满          5=钥匙+1
    //   6=火把+3          7=护身符+1(传送回起点)
    //   8=无敌护盾(5秒)   9=双倍火力(15秒)
    uint8_t items[10];
    uint8_t shop_page; // 商城当前页 0..2 (10项分3页: 4+3+3)
    uint8_t shop_page_sel; // 商城页内光标 0..n
    uint8_t inv2_sel; // 新道具栏光标 0..9
    // v6.11: 道具增益计时器 (帧数, >0 时生效)
    uint16_t buff_shield; // 无敌护盾剩余帧
    uint16_t buff_doublefire; // 双倍火力剩余帧
} GameState;

// v6.2: 粒子接口
void particles_update(void);
void particle_spawn(float x, float y, uint8_t type, int count);
int particles_active_count(void);

// v6.0 成就里程碑位图
enum {
    ACH_FIRST_BLOOD = 1u << 0, // 首次击杀
    ACH_FIRST_CLEAR = 1u << 1, // 首次过关
    ACH_KILL_10 = 1u << 2, // 累计10杀
    ACH_KILL_50 = 1u << 3, // 累计50杀
    ACH_CLEAR_10 = 1u << 4, // 累计10关
    ACH_CLEAR_25 = 1u << 5, // 累计25关
    ACH_MINER_50 = 1u << 6, // MC挖50块
    ACH_REACH_COMBAT = 1u << 7, // 到达战斗关(21)
    ACH_REACH_LATE = 1u << 8, // 到达后期关(35+)
};

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
    MSG_QUESTDONE, // 任务完成 toast
    MSG_POTION, // 拾取药水 toast
    MSG_AMULET, // 拾取护符 toast
    MSG_MINE, // MC: 挖掘 toast
    MSG_PLACE, // MC: 放置 toast
    MSG_LOCKED, // v6.0: 出口锁定 (任务未完成)
    MSG_NOAMMO, // v6.0: 弹药耗尽
    MSG_AMMO, // v6.0: 拾取弹药
    MSG_ACHIEVE, // v6.0: 成就解锁
    MSG_TASKPROG, // v6.0: 任务进度更新
    MSG_SHOP_BUY, // v6.11: 购买成功
    MSG_SHOP_FULL, // v6.11: 库存已满
    MSG_SHOP_FAIL, // v6.11: 积分不足
    MSG_ITEM_USE, // v6.11: 道具使用成功
    MSG_ITEM_NONE, // v6.11: 道具库存为空
    MSG_BUFF_SHIELD, // v6.11: 护盾激活
    MSG_BUFF_FIRE, // v6.11: 双倍火力激活
};

// ---- 模块接口 ----
void engine_render(void);

void maze_generate(int w, int h, int level, unsigned int seed);
int maze_cell_index(int x, int y);
uint8_t maze_get(int x, int y);
void maze_set(int x, int y, uint8_t v);
uint32_t maze_rng_next(void);

void game_init_campaign(int level);
void game_init_endless(int floor, bool visitor);
void game_init_mc(void); // MC 沙盒模式 (Beta)
void mc_mine(void); // MC: 挖掘前方方块
void mc_place(void); // MC: 在前方放置方块
void mc_cycle_block(void); // MC: 切换手持方块
void player_shoot(void); // v6.0/v6.1: 手枪射击 (发射实体子弹)
void ach_check(void); // v6.0: 检查并触发成就里程碑
void ach_grant(uint32_t flag, int msg_extra); // v6.0: 发放成就
void game_handle_input(InputKey key, InputType type);
void game_update(void);
void game_next_level(void);
bool player_move(float dx, float dy);
void player_rotate(float angle);
void actors_update(void);
void spawn_actor(float x, float y, int type);
void set_msg(int id); // 设置中文提示
// v6.1: 子弹系统
void bullets_update(void); // 推进所有子弹, 处理碰撞
void bullet_spawn(float x, float y, float dx, float dy, int owner);
int bullets_active_count(void);

// 物品栏: 使用选中物品, 返回是否消耗
bool item_use(int item_type);
// v6.11: 商城道具 (10 种) — 名称/价格/使用
const char* shop_item_name_zh(int idx);
const char* shop_item_name_en(int idx);
const char* shop_item_desc_zh(int idx);
const char* shop_item_desc_en(int idx);
uint16_t shop_item_price(int idx);
bool shop_item_use(int idx);
// 物品栏当前持有数
int item_count(int item_type);

// ---- 音效系统 ----
typedef enum {
    SFX_NONE = 0,
    SFX_MENU_MOVE, // 菜单切换
    SFX_MENU_OK, // 菜单确认
    SFX_PICK_KEY, // 拾取钥匙
    SFX_PICK_ITEM, // 拾取药水/火把/护符
    SFX_OPEN_DOOR, // 开门
    SFX_NEED_KEY, // 无钥匙(失败)
    SFX_TRAP, // 陷阱
    SFX_DAMAGE, // 受伤
    SFX_ATTACK_HIT, // 命中敌人
    SFX_ENEMY_KILL, // 击杀敌人
    SFX_QUEST_DONE, // 任务完成
    SFX_LEVEL_CLEAR, // 过关
    SFX_GAME_OVER, // 阵亡
    SFX_STORY_TURN, // 剧情翻页
    SFX_SHOOT, // v6.0: 手枪射击
    SFX_LOCKED, // v6.0: 出口锁定/操作禁止
    SFX_ACHIEVE, // v6.0: 成就解锁
    SFX_NO_AMMO, // v6.0: 空仓
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

// v6.6: 世界预生成缓存 — 保存/加载完整迷宫到 App Data
bool world_save(int level);
bool world_load(int level);

// 剧情: 返回剧情段总页数, 取指定页文本(英文), 取选项A/B文本
int story_pages(int story_id);
const char* story_page_text(int story_id, int page);
const char* story_choice_a(int story_id);
const char* story_choice_b(int story_id);
const char* story_title(int story_id);

extern const uint8_t TEXTURES[][8];
#define TEX_COUNT \
    24 // v6.7-beta: 扩展到24槽, 保留12个空白给未来方块 (圆石/煤/铁/钻石/台阶/玻璃/工作台/熔炉/羊毛/苔石/地狱岩/末地石)
