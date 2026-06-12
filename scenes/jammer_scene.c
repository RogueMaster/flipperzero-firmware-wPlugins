/**
 * @file jammer_scene.c
 * @brief NRF24 Jammer 实现.规范 §13 Phase 5 + §16.1/§16.2 + §17 反模式.
 *
 * 7 种预设(views/jammer_view.h JammerMode):
 *   CwCustom    : 恒载波 @ 用户自选 ch   (单频,信号发生器用途)
 *   BleAdv      : 恒载波跳 {2, 26, 80}   (BLE adv 37/38/39)
 *   ReactiveBle : RPD 反应式             (RX 监听 → 检测 → 切 TX 干扰)
 *   Wifi1       : 恒载波跳 ch 1..23      (2401..2423 MHz)
 *   Wifi6       : 恒载波跳 ch 26..48     (中心 2437)
 *   Wifi11      : 恒载波跳 ch 51..73     (中心 2462)
 *   AllBand     : 恒载波跳 0..125        (全 2.4G)
 *
 * 驱动模型(v0.5.x 重写 — 忠实对照 huuck/FlipperZeroNRFJammer):
 *   旧实现把每个 chunk 包进 pq_chip_with_nrf24(每次 acquire→cb→release),恒载波
 *   被反复"释放/重夺总线"打断 → 实测真机上起振不稳定甚至完全不发.
 *   新实现改用"独占持有总线会话"(pq_chip_nrf24_session_begin/end):OK 启动后
 *   acquire 一次、整段持有,CE 常高;worker 在紧贴循环里只连写 RF_CH 跳频,全程
 *   不释放总线 —— 与 huuck 的 while{for(ch) write(RF_CH)} 一致.
 *   恒载波起振用 huuck 的"灌 32B FIFO 后再做一次 set_tx_mode(CE LOW→HIGH 二次
 *   脉冲)"点火动作(见 jammer_start_const_carrier),旧实现只有单次 CE 上升沿.
 *
 * 线程模型(§16.1):
 *   主线程 (GUI) : jammer_input_callback 处理按键,只写 app 字段 + 调 view setter.
 *   worker 线程  : 持有总线会话 + 状态机 Idle↔Active;Active 紧贴跳频/反应循环.
 *   芯片访问     : 会话期间直接 pq_chip_spi_trx / pq_chip_nrf24_ce_set(同一线程).
 */
#include "jammer_scene.h"

#include "../core/pq_cc1101.h"
#include "../core/pq_chip_arbiter.h"
#include "../core/pq_jammer_config.h"
#include "../core/pq_jammer_log.h"
#include "../core/pq_nrf24.h"
#include "../core/pq_nrf24_regs.h"
#include "../core/pq_scene_lifecycle.h"
#include "../pingequa_app.h"
#include "../views/jammer_view.h"

#include <furi.h>
#include <input/input.h>
#include <string.h>

#define TAG                    "PqJammerScene"
#define SCENE_NAME             "jammer"
#define WORKER_STACK           2048      /* §11.1 上限 2048 */
#define ARBITER_TIMEOUT_MS     200       /* cc1101 sleep / 一次性 init_regs 用 */
#define WORKER_IDLE_YIELD_MS   50        /* idle 时让 GUI */
#define CW_TICK_YIELD_MS       20        /* 单频恒载波:载波已常发,只需周期 yield 给 GUI */
#define HOP_BATCH              128       /* 多信道:每跑 128 次 RF_CH 跳频回查停止标志
                                          * (~5ms 一次;紧贴 huuck 的无延时跳频循环) */
#define NRF24_CW_LEVEL_MAX     3         /* RF_PWR=11 → 0 dBm at die(PA 模块拉满) */

/* 会话级统计 — Scene 单例,static 安全.
 *   start_tick           : on_enter 设当前 boot ms,on_exit 算 duration
 *   reactive_jams_total  : 跨 OK 启停周期累加 RPD 反应次数(只在 BLE React 有意义) */
static struct {
    uint32_t start_tick;
    uint32_t reactive_jams_total;
} s_jam_session;

/* ---------------------------------------------------------------------------
 * Profile 表 — 每模式的引擎类型 + 信道集 + 时序参数
 * --------------------------------------------------------------------------*/

typedef enum {
    JamEngineCw = 0,         /* CW: 寄存器配 CONT_WAVE+PLL_LOCK,FIFO 灌 0xFF */
    JamEnginePayloadSpam,    /* W_TX_PAYLOAD_NOACK 持续 spam,普通 2Mbps RF_SETUP */
    JamEngineReactive,       /* RPD 反应式:RX 监听 → 检测 → 切 TX 干扰 → 切回 RX */
} JamEngine;

typedef struct {
    JamEngine engine;
    /* 信道集合二选一表达:
     *   channels != NULL : 显式列表(BLE adv 等非连续)— 从 channels[0..count-1] 循环
     *   channels == NULL : 连续区间 [first..last] — 范围内顺序循环
     *   两者都退化(channels=NULL && first==last==0)+ engine=Cw : CwCustom 特例,
     *     worker 用 app->jammer_cw_channel,无 cursor 循环 */
    const uint8_t* channels;
    uint8_t channel_count;
    uint8_t ch_first;
    uint8_t ch_last;
    uint16_t dwell_us;       /* 每信道驻留时间(µs) */
    uint8_t chunk_size;      /* 每 callback 跑的信道数 — 控制 §11.2 callback ≤ 50ms */
} JammerProfile;

/* BLE 广告频道(NRF24 channel = MHz - 2400):
 *   BLE 37 = 2402 MHz → ch  2
 *   BLE 38 = 2426 MHz → ch 26
 *   BLE 39 = 2480 MHz → ch 80 */
static const uint8_t k_ble_adv_chs[] = {2, 26, 80};

/* WiFi 信道范围(NRF24 ch = MHz - 2400),恒载波在整段内快速跳频铺能量:
 *   WiFi 1  → ch 1..23  (2401..2423,中心 2412 ±11)
 *   WiFi 6  → ch 26..48 (2426..2448,中心 2437)
 *   WiFi 11 → ch 51..73 (2451..2473,中心 2462)
 * 用 ch_first..ch_last 连续区间表达;dwell_us/chunk_size 在恒载波驱动模型下不再
 * 参与(跳频在持有总线的紧循环里无延时连写 RF_CH,见 jammer_const_carrier_hop_batch). */

static const JammerProfile k_profiles[JammerModeCount] = {
    [JammerModeCwCustom] = {
        .engine = JamEngineCw,
        .channels = NULL, .channel_count = 0,
        .ch_first = 0, .ch_last = 0,
        .dwell_us = 0, .chunk_size = 1,
    },
    [JammerModeBleAdv] = {
        .engine = JamEngineCw,
        .channels = k_ble_adv_chs, .channel_count = 3,
        .ch_first = 0, .ch_last = 0,
        .dwell_us = 0, .chunk_size = 1,
    },
    [JammerModeReactiveBle] = {
        /* v0.4.x:RPD 反应式 BLE — 监听 + 检测 + 反应干扰.
         * 业内首创 Flipper NRF24 平台实现(参考 Brauer IEEE 7785169 概念). */
        .engine = JamEngineReactive,
        .channels = k_ble_adv_chs, .channel_count = 3,
        .ch_first = 0, .ch_last = 0,
        .dwell_us = 30000,   /* 反应循环时间上限,逻辑在 jammer_reactive_chunk_inner 内 */
        .chunk_size = 1,
    },
    [JammerModeWifi1] = {
        .engine = JamEngineCw,
        .channels = NULL, .channel_count = 0,
        .ch_first = 1, .ch_last = 23,
        .dwell_us = 0, .chunk_size = 1,
    },
    [JammerModeWifi6] = {
        .engine = JamEngineCw,
        .channels = NULL, .channel_count = 0,
        .ch_first = 26, .ch_last = 48,
        .dwell_us = 0, .chunk_size = 1,
    },
    [JammerModeWifi11] = {
        .engine = JamEngineCw,
        .channels = NULL, .channel_count = 0,
        .ch_first = 51, .ch_last = 73,
        .dwell_us = 0, .chunk_size = 1,
    },
    [JammerModeAllBand] = {
        .engine = JamEngineCw,
        .channels = NULL, .channel_count = 0,
        .ch_first = 0, .ch_last = 125,   /* 全 2.4G ISM */
        .dwell_us = 0, .chunk_size = 1,
    },
};

static const JammerProfile* profile_of(JammerMode m) {
    if(m >= JammerModeCount) return &k_profiles[JammerModeCwCustom];
    return &k_profiles[m];
}

static uint8_t profile_count(const JammerProfile* p) {
    if(p->channels) return p->channel_count;
    if(p->ch_last < p->ch_first) return 0;
    return (uint8_t)(p->ch_last - p->ch_first + 1);
}

static uint8_t profile_ch_at(const JammerProfile* p, uint8_t cursor) {
    if(p->channels) {
        return p->channels[cursor % p->channel_count];
    }
    uint8_t count = profile_count(p);
    if(count == 0) return p->ch_first;
    return (uint8_t)(p->ch_first + (cursor % count));
}

/* ---------------------------------------------------------------------------
 * 一次性回调(经 pq_chip_with_* 执行,on_enter 用)
 * --------------------------------------------------------------------------*/

/* on_enter 一次性:走 baseline init_regs(含 SETUP_AW writeback 校验,确认芯片
 * 仍连着;splash 探测过后插板/接触不良仍可能漏掉). */
static bool jammer_init_cb(void* ctx) {
    PqApp* app = ctx;
    if(!pq_nrf24_init_regs(app->nrf)) {
        FURI_LOG_E(TAG, "init_regs failed");
        return false;
    }
    return true;
}

/* CC1101 进 SLEEP — 进 jammer 前调一次,关 26 MHz crystal 减少邻频干扰
 * (实测影响微弱,主要为防御性,毕竟 1cm 天线间距). */
static bool cc1101_sleep_cb(void* ctx) {
    PqCc1101* cc = ctx;
    pq_cc1101_sleep(cc);
    return true;
}

/* ---------------------------------------------------------------------------
 * 恒载波点火(独占持有会话内调用)— 忠实移植 huuck nrf24.c
 * --------------------------------------------------------------------------*/

/* huuck nrf24_set_tx_mode:CE LOW → 清 STATUS → CONFIG(PRIM_RX=0, PWR_UP) → CE HIGH.
 * 关键是它末尾的 CE 上升沿;startConstCarrier 调两次,第二次在灌完 FIFO 后点火. */
static void jammer_tx_mode(PqApp* app) {
    pq_chip_nrf24_ce_set(false);
    pq_nrf24_write_reg(app->nrf, NRF24_REG_STATUS,
                       NRF24_STATUS_MAX_RT | NRF24_STATUS_TX_DS);
    uint8_t cfg = pq_nrf24_read_reg(app->nrf, NRF24_REG_CONFIG);
    cfg &= (uint8_t)~NRF24_CONFIG_PRIM_RX; /* 进 TX */
    cfg |= NRF24_CONFIG_PWR_UP;
    pq_nrf24_write_reg(app->nrf, NRF24_REG_CONFIG, cfg);
    pq_chip_nrf24_ce_set(true);
}

/* huuck nrf24_startConstCarrier 的忠实移植(read-modify-write 配 RF_SETUP/CONFIG,
 * 灌 32B 0xFF FIFO,再二次 set_tx_mode 点火).level=3 → RF_PWR=11 满功率. */
static void jammer_start_const_carrier(PqApp* app, uint8_t ch, uint8_t level) {
    PqNrf24* nrf = app->nrf;

    jammer_tx_mode(app); /* set_tx_mode #1 */

    pq_nrf24_write_reg(nrf, NRF24_REG_RF_CH, ch);

    uint8_t setup = pq_nrf24_read_reg(nrf, NRF24_REG_RF_SETUP);
    setup = (uint8_t)((setup & 0xF8) | ((level & 0x3) << 1)); /* 置 RF_PWR */
    pq_nrf24_write_reg(nrf, NRF24_REG_RF_SETUP, setup);
    setup |= NRF24_RF_SETUP_CONT_WAVE | NRF24_RF_SETUP_PLL_LOCK;
    pq_nrf24_write_reg(nrf, NRF24_REG_RF_SETUP, setup);

    pq_nrf24_write_reg(nrf, NRF24_REG_EN_AA, 0x00);

    uint8_t cfg = pq_nrf24_read_reg(nrf, NRF24_REG_CONFIG);
    cfg &= (uint8_t)~NRF24_CONFIG_EN_CRC;
    pq_nrf24_write_reg(nrf, NRF24_REG_CONFIG, cfg);

    /* 灌 FIFO 前先 flush TX:CONT_WAVE 下 dummy 包不会被真正发走,反复点火/运行中切
     * 模式会逐次累积,第 4 次起 FIFO 满(TX_FULL),后续 W_TX_PAYLOAD 被静默丢弃 →
     * 点火"踢一脚"失效,同一会话测着测着就不发了(间歇性失效根因;真机实测切几次后
     * FIFO=0x21 已 TX_FULL.sibling pq_nrf24_setup_cw 早有此 flush,jammer 路径漏了). */
    pq_nrf24_flush_tx(nrf);
    uint8_t dummy[32];
    memset(dummy, 0xFF, sizeof(dummy));
    pq_nrf24_load_tx_payload(nrf, dummy, sizeof(dummy));

    jammer_tx_mode(app); /* set_tx_mode #2 — 灌完 FIFO 后点火 */

    pq_nrf24_diag_log(nrf, "const_carrier");
}

/* RPD 反应式 chunk(Reactive engine) — 监听 RPD,检测到载波 sustained jam 当前 ch.
 *
 * v0.4.x 时序修正:之前多信道 burst(ch 37→38→39)在时序上失效:
 *   BLE adv train 总时长 ~1050 µs(ch 37 + ch 38 + ch 39),我们反应延迟 ~150 µs
 *   后才开始 jam,等切到 ch 38/39 时,BLE 设备已经播完了.所以多信道 burst 大多
 *   打的是空气.
 *
 * 简化策略:检测到 cur_ch 有载波后,在 cur_ch 上 sustained CW 2.5 ms.
 *   - 第一段 ~150 µs 内 BLE 设备还在 cur_ch 上发,CW 直接对撞,CRC 损坏
 *   - 后续 2.35 ms 等下一次 BLE 周期(20-100ms 间隔)在 cur_ch 上重发,被压制
 *   - 单次反应 TX 时间从 2.1 ms → 2.5 ms,但能量集中在已确认有目标的 ch,
 *     比之前撒 3 个 ch 更有效率
 *
 * 单次反应时序:
 *   read RPD             : 14 µs
 *   CE low               : <1 µs
 *   react_to_tx          : ~14 µs SPI
 *   CE high              : <1 µs
 *   Tstby2a              : 130 µs(PA + PLL)
 *   sustained jam        : 2500 µs(覆盖当前包 + 等下一次广播的概率窗口)
 *   CE low               : <1 µs
 *   react_to_rx + RF_CH  : ~21 µs SPI
 *   CE high              : <1 µs
 *   Tstby2a              : 130 µs(进 RX active)
 *   总                    : ~2.8 ms / 反应周期 */
static bool jammer_reactive_chunk_inner(PqApp* app, const JammerProfile* p) {
    uint8_t cur_ch = profile_ch_at(p, app->jammer_sweep_cursor);

    /* 时间上限 30 ms — 满足 §11.2 callback ≤ 50 ms. */
    const uint32_t budget_ticks = (30u * furi_kernel_get_tick_frequency()) / 1000u;
    const uint32_t t_start = furi_get_tick();
    const int max_iter = 700;
    int jam_count = 0;

    for(int i = 0; i < max_iter; i++) {
        if((furi_get_tick() - t_start) >= budget_ticks) break;

        if(pq_nrf24_read_rpd(app->nrf)) {
            /* 检测到 ≥ -64 dBm 载波 — sustained jam 当前 ch. */
            pq_chip_nrf24_ce_set(false);
            pq_nrf24_react_to_tx(app->nrf);
            pq_chip_nrf24_ce_set(true);
            furi_delay_us(130);                 /* Tstby2a + PA 稳定 */
            furi_delay_us(2500);                /* sustained CW 2.5 ms — 覆盖当前包 + 等下一周期 */
            pq_chip_nrf24_ce_set(false);

            /* 切回 RX 同 ch 继续监听. */
            pq_nrf24_react_to_rx(app->nrf, cur_ch);
            pq_chip_nrf24_ce_set(true);
            furi_delay_us(130);
            jam_count++;
        } else {
            furi_delay_us(50);
        }
    }

    if(jam_count > 0) {
        FURI_LOG_I(TAG, "react ch=%u jams=%d", cur_ch, jam_count);
        s_jam_session.reactive_jams_total += (uint32_t)jam_count; /* 跨 chunk 累计写日志 */
    }
    app->jammer_chunk_count += jam_count;
    jammer_view_update_tick(app->jammer_view, cur_ch, app->jammer_chunk_count);

    /* 跳下个 BLE adv 信道. */
    app->jammer_sweep_cursor = (uint8_t)((app->jammer_sweep_cursor + 1) % p->channel_count);
    pq_nrf24_set_channel_fast(app->nrf, profile_ch_at(p, app->jammer_sweep_cursor));
    return true;
}

/* 进入 Active(或运行中切模式)时,按引擎在持有的会话里做一次 setup. */
static void jammer_engine_setup(PqApp* app, JammerMode mode) {
    const JammerProfile* p = profile_of(mode);
    uint8_t ch0 = (mode == JammerModeCwCustom) ? app->jammer_cw_channel
                                               : profile_ch_at(p, 0);
    /* 重配寄存器前先 CE LOW(运行中切模式时上一引擎可能 CE 仍高). */
    pq_chip_nrf24_ce_set(false);
    if(p->engine == JamEngineReactive) {
        pq_nrf24_setup_reactive(app->nrf, ch0);
        pq_chip_nrf24_ce_set(true);
        furi_delay_us(130); /* Tstby2a 进 RX */
        pq_nrf24_diag_log(app->nrf, "reactive_setup");
    } else {
        jammer_start_const_carrier(app, ch0, NRF24_CW_LEVEL_MAX);
    }
    app->jammer_sweep_cursor = 0;
    app->jammer_cw_dirty = false;
}

/* 恒载波运行 batch — 总线持有、CE 常高全程不打断(huuck 紧贴跳频).
 *   CwCustom : 停在用户信道(随 ←→ 改频);载波由 CE 常高 + 会话持有维持,
 *              这里只需周期 yield 给 GUI.
 *   多信道   : 在信道集内无延时连写 RF_CH 跳 HOP_BATCH 次,把载波铺满整段. */
static void jammer_const_carrier_hop_batch(PqApp* app, JammerMode mode) {
    const JammerProfile* p = profile_of(mode);

    if(mode == JammerModeCwCustom) {
        pq_nrf24_set_channel_fast(app->nrf, app->jammer_cw_channel);
        app->jammer_cw_dirty = false;
        app->jammer_chunk_count++;
        jammer_view_update_tick(app->jammer_view, app->jammer_cw_channel,
                                app->jammer_chunk_count);
        furi_delay_ms(CW_TICK_YIELD_MS); /* 总线持有 + CE 高 → 载波持续;仅 yield GUI */
        return;
    }

    uint8_t total = profile_count(p);
    if(total == 0) return;

    uint8_t cursor = app->jammer_sweep_cursor;
    for(uint32_t i = 0; i < HOP_BATCH; i++) {
        pq_nrf24_set_channel_fast(app->nrf, profile_ch_at(p, cursor));
        cursor = (uint8_t)((cursor + 1) % total);
    }
    app->jammer_sweep_cursor = cursor;
    app->jammer_chunk_count++;

    uint8_t prev = (cursor == 0) ? (uint8_t)(total - 1) : (uint8_t)(cursor - 1);
    jammer_view_update_tick(app->jammer_view, profile_ch_at(p, prev),
                            app->jammer_chunk_count);
}

/* ---------------------------------------------------------------------------
 * Worker 线程 — 本地状态机
 * --------------------------------------------------------------------------*/

static int32_t jammer_worker_func(void* ctx) {
    PqApp* app = ctx;
    bool active = false;                      /* 独占持有会话是否已开(= 正在干扰) */
    JammerMode active_mode = JammerModeCount; /* 无效哨兵,迫使首帧 setup */

    while(!app->jammer_stop_requested) {
        const bool want_run = app->jammer_running;

        /* Idle → Active:开独占会话(acquire 一次,整段持有)+ 按模式点火. */
        if(want_run && !active) {
            pq_chip_nrf24_session_begin();
            active_mode = app->jammer_mode;
            jammer_engine_setup(app, active_mode);
            active = true;
            FURI_LOG_I(TAG, "jam start (held session) mode=%d ch=%u",
                       (int)active_mode, app->jammer_cw_channel);
            continue;
        }

        /* Active → Idle:停发 + 关会话(release 总线). */
        if(!want_run && active) {
            pq_chip_nrf24_ce_set(false);
            pq_nrf24_power_down(app->nrf);
            pq_chip_nrf24_session_end();
            active = false;
            FURI_LOG_I(TAG, "jam stop chunks=%lu",
                       (unsigned long)app->jammer_chunk_count);
            continue;
        }

        /* Idle 等待. */
        if(!active) {
            furi_delay_ms(WORKER_IDLE_YIELD_MS);
            continue;
        }

        /* 运行中实时切模式(↑↓ 改 app->jammer_mode):会话内重新 setup,不必停发. */
        if(app->jammer_mode != active_mode) {
            active_mode = app->jammer_mode;
            jammer_engine_setup(app, active_mode);
            continue;
        }

        /* Active 稳态:跑一个 batch(总线持有,CE 常高全程不打断). */
        const JammerProfile* p = profile_of(active_mode);
        if(p->engine == JamEngineReactive) {
            jammer_reactive_chunk_inner(app, p);
        } else {
            jammer_const_carrier_hop_batch(app, active_mode);
        }
    }

    /* 退出兜底:若仍持有会话,停发 + 释放. */
    if(active) {
        pq_chip_nrf24_ce_set(false);
        pq_nrf24_power_down(app->nrf);
        pq_chip_nrf24_session_end();
    }
    return 0;
}

/* ---------------------------------------------------------------------------
 * Input 回调 — GUI 线程
 * --------------------------------------------------------------------------*/

static bool jammer_input_callback(InputEvent* event, void* ctx) {
    PqApp* app = ctx;

    const bool is_short = (event->type == InputTypeShort);
    const bool is_long_or_rep =
        (event->type == InputTypeLong) || (event->type == InputTypeRepeat);
    if(!is_short && !is_long_or_rep) return false;

    switch(event->key) {
    case InputKeyUp:
    case InputKeyDown:
        /* 6 模式循环:↑ 前进 / ↓ 后退(只接受短按,避免长按多次切). */
        if(is_short) {
            JammerMode new_mode;
            if(event->key == InputKeyUp) {
                new_mode = (JammerMode)((app->jammer_mode + 1) % JammerModeCount);
            } else {
                new_mode = (app->jammer_mode == 0)
                               ? (JammerMode)(JammerModeCount - 1)
                               : (JammerMode)(app->jammer_mode - 1);
            }
            app->jammer_mode = new_mode;
            app->jammer_sweep_cursor = 0;
            app->jammer_cw_dirty = true;
            jammer_view_set_mode(app->jammer_view, new_mode);
        }
        return true;

    case InputKeyLeft: {
        /* 仅 CwCustom 可调信道;其他模式信道由 profile 锁定. */
        if(app->jammer_mode != JammerModeCwCustom) return true;
        uint8_t step = is_long_or_rep ? 5 : 1;
        app->jammer_cw_channel =
            (app->jammer_cw_channel >= step) ? (app->jammer_cw_channel - step) : 0;
        app->jammer_cw_dirty = true;
        jammer_view_set_cw_channel(app->jammer_view, app->jammer_cw_channel);
        return true;
    }

    case InputKeyRight: {
        if(app->jammer_mode != JammerModeCwCustom) return true;
        uint8_t step = is_long_or_rep ? 5 : 1;
        uint16_t v = (uint16_t)app->jammer_cw_channel + step;
        app->jammer_cw_channel = (v > NRF24_CHANNEL_MAX) ? NRF24_CHANNEL_MAX : (uint8_t)v;
        app->jammer_cw_dirty = true;
        jammer_view_set_cw_channel(app->jammer_view, app->jammer_cw_channel);
        return true;
    }

    case InputKeyOk:
        if(is_short) {
            app->jammer_running = !app->jammer_running;
            jammer_view_set_running(app->jammer_view, app->jammer_running);
        }
        return true;

    case InputKeyBack:
        /* 不在此 stop dispatcher;返回 false 让 SceneManager 走 nav → previous_scene
         * (规范 §6.1).on_exit 会 join worker + 清理. */
        if(is_short) {
            app->jammer_stop_requested = true; /* 提前发停信号,缩短 join 等待 */
            app->jammer_running = false;       /* 让 worker 转 Idle */
        }
        return false;

    default:
        return false;
    }
}

/* ---------------------------------------------------------------------------
 * Scene 接口
 * --------------------------------------------------------------------------*/

void jammer_scene_on_enter(void* context) {
    PqApp* app = context;
    pq_scene_enter(SCENE_NAME);

    /* 加载上次保存的偏好 (jammer.conf).文件不存在 / 字段越界 → defaults
     * (CwCustom @ ch 42 ~2442 MHz).v0.5.0 起不再每次进 jammer 重选模式. */
    PqJammerState saved;
    pq_jammer_config_load(&saved);

    app->jammer_running = false;
    app->jammer_stop_requested = false;
    app->jammer_mode = (JammerMode)saved.mode;
    app->jammer_cw_channel = saved.cw_channel;
    app->jammer_cw_dirty = true;
    app->jammer_sweep_cursor = 0;
    app->jammer_chunk_count = 0;

    /* 复位会话统计 — 退出时写日志用. */
    s_jam_session.start_tick = furi_get_tick();
    s_jam_session.reactive_jams_total = 0;

    /* CC1101 进 SLEEP — 1cm 天线间距下,关 CC1101 26 MHz crystal 减少邻频干扰
     * (即便 sub-GHz 谐波在 2.4 GHz 极弱,仍是稳妥防御).每次进 jammer 都做. */
    PqCc1101* cc_sleep = pq_cc1101_alloc(NULL);
    pq_chip_with_cc1101(cc1101_sleep_cb, cc_sleep, ARBITER_TIMEOUT_MS);
    pq_cc1101_free(cc_sleep);

    /* Alloc nrf24 driver(scene 拥有生命期;exit 时 free). */
    app->nrf = pq_nrf24_alloc(&app->cfg);

    /* 一次性 baseline init(SETUP_AW 验证芯片活着).真正的恒载波点火在 worker
     * 开独占会话后做(jammer_engine_setup → jammer_start_const_carrier). */
    if(!pq_chip_with_nrf24(jammer_init_cb, app, ARBITER_TIMEOUT_MS)) {
        FURI_LOG_E(TAG, "init_regs failed; → error scene");
        scene_manager_next_scene(app->scene_manager, PqSceneError);
        return;
    }

    /* 注册 view 的输入回调 + 切到 Jammer view. */
    View* v = jammer_view_get_view(app->jammer_view);
    view_set_context(v, app);
    view_set_input_callback(v, jammer_input_callback);
    view_dispatcher_switch_to_view(app->view_dispatcher, PqViewJammer);

    /* 初值同步给 view. */
    jammer_view_set_mode(app->jammer_view, app->jammer_mode);
    jammer_view_set_cw_channel(app->jammer_view, app->jammer_cw_channel);
    jammer_view_set_running(app->jammer_view, app->jammer_running);

    /* Worker. */
    app->jammer_thread =
        furi_thread_alloc_ex("PqJamWorker", WORKER_STACK, jammer_worker_func, app);
    furi_thread_set_priority(app->jammer_thread, FuriThreadPriorityLow);
    furi_thread_start(app->jammer_thread);
}

bool jammer_scene_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    /* 输入由 view 的 input_callback 直接消化. */
    return false;
}

void jammer_scene_on_exit(void* context) {
    PqApp* app = context;

    /* 1. 通知 worker 停 + join. */
    if(app->jammer_thread) {
        app->jammer_stop_requested = true;
        app->jammer_running = false;
        furi_thread_join(app->jammer_thread);
        furi_thread_free(app->jammer_thread);
        app->jammer_thread = NULL;
    }

    /* 2. 释放 nrf24 driver. */
    if(app->nrf) {
        pq_nrf24_free(app->nrf);
        app->nrf = NULL;
    }

    /* 3. 保存当前 mode + cw_channel(下次进 jammer 直接落地到上次状态). */
    PqJammerState st = {
        .mode = (uint8_t)app->jammer_mode,
        .cw_channel = app->jammer_cw_channel,
    };
    pq_jammer_config_save(&st);

    /* 4. 写本次会话日志(v0.5.2 重构:有意义字段集,按模式条件输出). */
    pq_jammer_log_session(
        (uint8_t)app->jammer_mode,
        app->jammer_cw_channel,
        s_jam_session.start_tick,
        s_jam_session.reactive_jams_total);

    /* 5. View 是 app-owned,scene 不释放. */

    pq_scene_exit(SCENE_NAME);
}
