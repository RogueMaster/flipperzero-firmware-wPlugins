#include "maze3d.h"
#include <furi_hal_speaker.h>
#include <furi_hal.h>
#include <string.h>

// ---- 简易音序器 ----
// 单个音符: 频率 + 时长(tick数, 1 tick ≈ 120ms)
typedef struct {
    uint16_t freq;    // Hz, 0 = 静音间隔
    uint8_t  ticks;   // 持续 tick 数
} Note;

#define MAX_SEQ_NOTES 8
typedef struct {
    Note notes[MAX_SEQ_NOTES];
    uint8_t count;
} Seq;

// 预设 SFX 谱 (使用常见音符频率近似)
#define C5  523
#define D5  587
#define E5  659
#define F5  698
#define G5  784
#define A5  880
#define B5  988
#define C6  1046
#define D6  1175
#define E6  1318
#define F6  1397
#define G6  1568
#define A6  1760
#define REST 0

static const Seq s_seqs[SFX_COUNT] = {
    [SFX_NONE]       = {{}, 0},
    // 菜单切换: 短高音
    [SFX_MENU_MOVE]  = {{{C6,1}}, 1},
    // 菜单确认: 两短音
    [SFX_MENU_OK]    = {{{E6,1},{G6,1}}, 2},
    // 拾取钥匙: 上行琶音
    [SFX_PICK_KEY]   = {{{C5,1},{E5,1},{G5,1},{C6,1}}, 4},
    // 拾取物品: 单中音
    [SFX_PICK_ITEM]  = {{{E5,1},{G5,1}}, 2},
    // 开门: 低沉两音
    [SFX_OPEN_DOOR]  = {{{C5,2},{G5,2}}, 2},
    // 无钥匙: 失败低降
    [SFX_NEED_KEY]   = {{{E5,1},{C5,2}}, 2},
    // 陷阱: 短刺耳
    [SFX_TRAP]       = {{{A5,1},{G5,1},{F5,1}}, 3},
    // 受伤: 短促降
    [SFX_DAMAGE]     = {{{G5,1},{C5,2}}, 2},
    // 命中: 敲击
    [SFX_ATTACK_HIT] = {{{D6,1}}, 1},
    // 击杀敌人: 下行
    [SFX_ENEMY_KILL] = {{{E6,1},{C6,1},{G5,2}}, 3},
    // 任务完成: 小三和弦
    [SFX_QUEST_DONE] = {{{C5,1},{E5,1},{G5,1},{C6,2}}, 4},
    // 过关: C大调上行
    [SFX_LEVEL_CLEAR]= {{{C5,1},{D5,1},{E5,1},{G5,1},{C6,3}}, 5},
    // 阵亡: 下行小调
    [SFX_GAME_OVER]  = {{{A5,1},{F5,1},{D5,2},{C5,3}}, 4},
    // 剧情翻页
    [SFX_STORY_TURN] = {{{G5,1}}, 1},
};

// ---- 开场 BGM: 嗨皮欢快旋律 (C大调, 独立长序列) ----
// 总时长 ~38 tick (~4.5s), 配合4阶段动画
#define BGM_NOTES 40
static const Note s_bgm[BGM_NOTES] = {
    // 第1句: 上行琶音开场 (0-7)
    {C5,1},{E5,1},{G5,1},{C6,1},
    {E6,1},{G6,1},{E6,1},{C6,1},
    // 第2句: 跳跃下行 (8-15)
    {G5,1},{C6,1},{E6,1},{G6,1},
    {A6,2},{G6,1},{E6,1},
    // 第3句: 欢快回旋 (16-23)
    {C6,1},{G5,1},{E5,1},{G5,1},
    {C6,1},{E6,1},{G6,1},{REST,1},
    // 第4句: 下行渐收 (24-31)
    {E6,1},{D6,1},{C6,1},{G5,1},
    {E5,1},{G5,1},{C6,1},{REST,1},
    // 第5句: 终曲和弦 (32-39)
    {G5,1},{C6,1},{E6,1},{G6,1},
    {C6,1},{E6,1},{G6,2},{C6,3},
};

// 当前 SFX 播放状态
static uint8_t  s_cur_sfx = SFX_NONE;
static uint8_t  s_note_idx = 0;
static uint8_t  s_note_ticks_left = 0;
// BGM 播放状态 (独立通道, 优先级高于 SFX)
static bool     s_bgm_active = false;
static uint8_t  s_bgm_idx = 0;
static uint8_t  s_bgm_ticks_left = 0;
static volatile bool s_speaker_on = false;

static void speaker_start(uint32_t freq) {
    if(!g.sfx_enabled) return;
    if(!furi_hal_speaker_is_mine()) return;
    furi_hal_speaker_start(freq, 0.35f);
    s_speaker_on = true;
}
static void speaker_stop_now(void) {
    if(s_speaker_on) {
        furi_hal_speaker_stop();
        s_speaker_on = false;
    }
}

void sfx_init(void) {
    s_cur_sfx = SFX_NONE;
    s_note_idx = 0;
    s_note_ticks_left = 0;
    s_bgm_active = false;
    s_bgm_idx = 0;
    s_bgm_ticks_left = 0;
    s_speaker_on = false;
}

void sfx_deinit(void) {
    speaker_stop_now();
    s_cur_sfx = SFX_NONE;
    s_bgm_active = false;
}

void sfx_stop_all(void) {
    speaker_stop_now();
    s_cur_sfx = SFX_NONE;
    s_note_idx = 0;
    s_note_ticks_left = 0;
    s_bgm_active = false;
    s_bgm_idx = 0;
    s_bgm_ticks_left = 0;
}

// 开场 BGM 控制
void sfx_bgm_play(void) {
    if(!g.sfx_enabled) return;
    s_bgm_active = true;
    s_bgm_idx = 0;
    s_bgm_ticks_left = 0;
    // 中断当前 SFX
    s_cur_sfx = SFX_NONE;
    s_note_ticks_left = 0;
}

void sfx_bgm_stop(void) {
    s_bgm_active = false;
    s_bgm_idx = 0;
    s_bgm_ticks_left = 0;
    speaker_stop_now();
}

bool sfx_bgm_playing(void) {
    return s_bgm_active;
}

void sfx_play(SfxType t) {
    if(!g.sfx_enabled) return;
    if(t <= SFX_NONE || t >= SFX_COUNT) return;
    // BGM 播放时, SFX 不打断 (BGM 优先)
    if(s_bgm_active) return;
    const Seq* s = &s_seqs[t];
    if(s->count == 0) return;
    // 中断当前SFX, 立即开新
    s_cur_sfx = t;
    s_note_idx = 0;
    s_note_ticks_left = 0;
    sfx_tick_update();
}

// 每 120ms 调一次 (主循环 UPDATE_MS)
void sfx_tick_update(void) {
    // BGM 通道优先
    if(s_bgm_active) {
        if(s_bgm_ticks_left == 0) {
            speaker_stop_now();
            if(s_bgm_idx >= BGM_NOTES) {
                s_bgm_active = false;
                return;
            }
            const Note* n = &s_bgm[s_bgm_idx];
            s_bgm_ticks_left = n->ticks;
            s_bgm_idx++;
            if(n->freq > 0) speaker_start(n->freq);
            if(s_bgm_ticks_left > 0) s_bgm_ticks_left--;
        } else {
            s_bgm_ticks_left--;
            if(s_bgm_ticks_left == 0) speaker_stop_now();
        }
        return; // BGM 播放时不处理 SFX
    }

    // SFX 通道
    if(s_cur_sfx == SFX_NONE) return;
    const Seq* s = &s_seqs[s_cur_sfx];
    if(s->count == 0) { s_cur_sfx = SFX_NONE; return; }

    if(s_note_ticks_left == 0) {
        // 当前音符结束: 先静音
        speaker_stop_now();
        if(s_note_idx >= s->count) {
            s_cur_sfx = SFX_NONE;
            return;
        }
        // 下一个音符
        const Note* n = &s->notes[s_note_idx];
        s_note_ticks_left = n->ticks;
        s_note_idx++;
        if(n->freq > 0) {
            speaker_start(n->freq);
        }
        // 持续 tick 减一 (这个 tick 已经算播放了 1 tick)
        if(s_note_ticks_left > 0) s_note_ticks_left--;
    } else {
        s_note_ticks_left--;
        if(s_note_ticks_left == 0) {
            // 本tick结束后静音, 下tick切下一个
            speaker_stop_now();
        }
    }
}

void settings_defaults(void) {
    g.sfx_enabled = true;
    g.opening_enabled = true;
}
