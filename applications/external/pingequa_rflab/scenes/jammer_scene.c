/**
 * @file jammer_scene.c
 * @brief NRF24 Jammer 实现.规范 §13 Phase 5 + §16.1/§16.2 + §17 反模式.
 *
 * 6 种预设(views/jammer_view.h JammerMode):
 *   CwCustom : CW @ 用户自选 ch          (CW 引擎,单 ch 静态)
 *   BleAdv   : CW hop {2, 26, 80}        (CW 引擎,3 ch 循环)
 *   Wifi1    : Spam @ NRF ch 1..23       (PayloadSpam 引擎,23 ch ±11 MHz 扫)
 *   Wifi6    : Spam @ NRF ch 26..48      (同上,中心 2437 MHz)
 *   Wifi11   : Spam @ NRF ch 51..73      (同上,中心 2462 MHz)
 *   AllBand  : CW sweep 0..125           (CW 引擎,全频)
 *
 * 引擎层差异:
 *   CW          : pq_nrf24_setup_cw(RF_SETUP CONT_WAVE+PLL_LOCK + 32B 0xFF FIFO);
 *                 worker 仅每 chunk 写 RF_CH 即可换频.CE 持续 HIGH.
 *   PayloadSpam : pq_nrf24_setup_payload_spam(RF_SETUP 普通 2 Mbps);worker 每
 *                 chunk 写 RF_CH + W_TX_PAYLOAD_NOACK 32B 0xDEADBEEF×8 刷 FIFO.
 *
 * 线程模型(§16.1):
 *   主线程 (GUI) : jammer_input_callback 处理按键,直接写 app 字段 + 调 view setter.
 *   worker 线程  : 本地状态机 Idle ↔ Active;Active 时按 profile.engine 分支.
 *   芯片访问     : 全部经 pq_chip_with_nrf24 callback(M3/M11);CE 经
 *                  pq_chip_nrf24_ce_set(M15);单 callback ≤ 50 ms(§11.2 / M10).
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

#define TAG                    "PqJammerScene"
#define SCENE_NAME             "jammer"
#define WORKER_STACK           2048      /* §11.1 上限 2048 */
#define ARBITER_TIMEOUT_MS     200       /* start/stop 容忍较长 */
#define ARBITER_CHUNK_TIMEOUT_MS 100     /* steady-state */
#define WORKER_IDLE_YIELD_MS   50        /* idle 时让 GUI */
#define WORKER_TICK_YIELD_MS   5         /* sweep chunk 后 yield(留 GUI 时间)  */
#define CW_TICK_YIELD_MS       50        /* CW 模式 chunk 之间长 yield(硬件已在干扰) */

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

/* WiFi pilot-aware 信道集(Clancy 2011 IEEE 5962467 + 7.5 dB 等效干扰增益):
 *
 * 802.11g OFDM 64 子载波,pilot 在子载波索引 ±7 和 ±21,间隔 312.5 kHz:
 *   ±7 × 312.5 kHz = ±2.2 MHz 处
 *   ±21 × 312.5 kHz = ±6.6 MHz 处
 * 这 4 个 pilot 是接收机 channel estimation / phase tracking 的锚,损坏后整
 * 个 OFDM 解调失败.NRF24 1 MHz 带宽刚好覆盖 pilot ±0.5 MHz.
 *
 * 比起盲扫整个 22 MHz WiFi 带宽 15 ch,pilot-aware 把所有能量打在 4 个最致命
 * 频点,等效干扰功率 +7.5 dB(实测 BER 0.4 阈值,Clancy 2011 paper). */
static const uint8_t k_wifi1_pilots[]  = {5, 10, 14, 19};   /* 2405/2410/2414/2419 MHz */
static const uint8_t k_wifi6_pilots[]  = {30, 35, 39, 44};  /* 2430/2435/2439/2444 MHz */
static const uint8_t k_wifi11_pilots[] = {55, 60, 64, 69};  /* 2455/2460/2464/2469 MHz */

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
        .dwell_us = 5000,    /* 5 ms / ch — 3 个 ch × 5 ms = 15ms chunk */
        .chunk_size = 3,
    },
    [JammerModeReactiveBle] = {
        /* v0.4.x:RPD 反应式 BLE — 监听 + 检测 + 反应干扰.
         * 业内首创 Flipper NRF24 平台实现(参考 Brauer IEEE 7785169 概念). */
        .engine = JamEngineReactive,
        .channels = k_ble_adv_chs, .channel_count = 3,
        .ch_first = 0, .ch_last = 0,
        .dwell_us = 30000,   /* 30 ms / ch — 监听 + 反应循环上限,然后跳下个 BLE ch */
        .chunk_size = 1,     /* 一次只在一个 ch 反应,下个 chunk 跳下个 ch */
    },
    [JammerModeWifi1] = {
        /* v0.4.x:CW (max +20 dBm 连续) + 单 pilot/chunk + 长 dwell.
         * 每 chunk 在 1 个 OFDM pilot 频点 sustained CW 25 ms,下个 chunk 跳下个 pilot.
         * 每 pilot 4 个 chunk 周期内有 25% TX 占空(高于之前 payload spam 4-pilot 滚的 14%),
         * 且 CW 能量集中在 1 MHz 带宽内,对 OFDM 单子载波损坏率最高. */
        .engine = JamEngineCw,
        .channels = k_wifi1_pilots, .channel_count = 4,
        .ch_first = 0, .ch_last = 0,
        .dwell_us = 25000,               /* 1 pilot × 25 ms / chunk = 25 ms callback (§11.2 上限内) */
        .chunk_size = 1,                 /* 每 chunk 只打 1 个 pilot,下次跳下个 */
    },
    [JammerModeWifi6] = {
        .engine = JamEngineCw,
        .channels = k_wifi6_pilots, .channel_count = 4,
        .ch_first = 0, .ch_last = 0,
        .dwell_us = 25000,
        .chunk_size = 1,
    },
    [JammerModeWifi11] = {
        .engine = JamEngineCw,
        .channels = k_wifi11_pilots, .channel_count = 4,
        .ch_first = 0, .ch_last = 0,
        .dwell_us = 25000,
        .chunk_size = 1,
    },
    [JammerModeAllBand] = {
        .engine = JamEngineCw,
        .channels = NULL, .channel_count = 0,
        .ch_first = 0, .ch_last = 125,
        .dwell_us = 2000,                /* 2 ms / ch CW dwell */
        .chunk_size = 16,                /* 16 × 2ms = 32ms < 50ms 上限 */
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

/* 32 字节 0xDEADBEEF × 8 — Payload Spam 引擎喂 FIFO 用. */
static const uint8_t k_spam_payload[32] = {
    0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE,
    0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD,
    0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF,
};

/* ---------------------------------------------------------------------------
 * 芯片操作回调(必须在 pq_chip_with_nrf24 内执行)
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

/* 进入 Active:按 profile.engine 分发 setup,然后 CE high + Tstby2a wait. */
static bool jammer_start_cb(void* ctx) {
    PqApp* app = ctx;
    const JammerProfile* p = profile_of(app->jammer_mode);

    /* 起始确保 CE LOW(防 setup 期间发射). */
    pq_chip_nrf24_ce_set(false);

    /* 起点信道:CwCustom 用用户值,其他用 profile 第一信道. */
    uint8_t initial_ch = (app->jammer_mode == JammerModeCwCustom)
                             ? app->jammer_cw_channel
                             : profile_ch_at(p, 0);

    bool ok;
    switch(p->engine) {
    case JamEngineCw:
        ok = pq_nrf24_setup_cw(app->nrf, initial_ch);
        break;
    case JamEnginePayloadSpam:
        ok = pq_nrf24_setup_payload_spam(app->nrf, initial_ch);
        break;
    case JamEngineReactive:
        ok = pq_nrf24_setup_reactive(app->nrf, initial_ch);
        break;
    default:
        ok = false;
        break;
    }
    if(!ok) {
        FURI_LOG_E(TAG, "setup failed engine=%d mode=%d", (int)p->engine,
                   (int)app->jammer_mode);
        return false;
    }

    /* CE HIGH 启动 TX. */
    pq_chip_nrf24_ce_set(true);

    /* Tstby2a = 130 µs(datasheet §6.1.7 Table 16):CE 拉高后到 PLL/PA 完全锁定. */
    furi_delay_us(130);

    /* 诊断:回读寄存器验证 SPI 写入真生效 — 用 ufbt cli + log 命令看输出. */
    pq_nrf24_diag_log(app->nrf, "start_cb");

    /* 重置 sweep 起点 + 清 dirty. */
    app->jammer_sweep_cursor = 0;
    app->jammer_cw_dirty = false;
    return true;
}

/* 退出 Active:CE low + PWR_DOWN. */
static bool jammer_stop_cb(void* ctx) {
    PqApp* app = ctx;
    pq_chip_nrf24_ce_set(false);
    pq_nrf24_power_down(app->nrf);
    return true;
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

/* Steady-state chunk — 按 profile 分发:
 *   CwCustom         : 仅 dirty 时写 RF_CH(用户改了 ch);callback < 1 ms
 *   其他 CW 模式     : profile.chunk_size 个 ch,每 ch 写 RF_CH + dwell_us
 *   PayloadSpam 模式 : 同上 + 每 ch 额外 W_TX_PAYLOAD_NOACK 32B 0xDEADBEEF×8
 *   Reactive 模式    : 转入 jammer_reactive_chunk_inner */
static bool jammer_chunk_cb(void* ctx) {
    PqApp* app = ctx;
    const JammerProfile* p = profile_of(app->jammer_mode);

    /* Reactive engine:专门走 RPD 监听+反应循环. */
    if(p->engine == JamEngineReactive) {
        return jammer_reactive_chunk_inner(app, p);
    }

    /* CwCustom 特例:静态 CW,用户调 ch 才需重写 RF_CH. */
    if(app->jammer_mode == JammerModeCwCustom) {
        if(app->jammer_cw_dirty) {
            pq_nrf24_set_channel_fast(app->nrf, app->jammer_cw_channel);
            app->jammer_cw_dirty = false;
        }
        return true;
    }

    /* 多信道模式:跑 chunk_size 个 ch. */
    uint8_t total = profile_count(p);
    if(total == 0) return true;

    uint8_t cursor = app->jammer_sweep_cursor;
    for(int i = 0; i < p->chunk_size; i++) {
        uint8_t ch = profile_ch_at(p, cursor);
        pq_nrf24_set_channel_fast(app->nrf, ch);

        /* PayloadSpam:每 ch 重新刷 FIFO(huuck WiFi 模式同).
         * CW 模式:FIFO 在 setup_cw 阶段已填,worker 不重写. */
        if(p->engine == JamEnginePayloadSpam) {
            pq_nrf24_load_tx_payload_noack(app->nrf, k_spam_payload, sizeof(k_spam_payload));
        }

        if(p->dwell_us > 0) furi_delay_us(p->dwell_us);

        cursor = (uint8_t)((cursor + 1) % total);
    }
    app->jammer_sweep_cursor = cursor;
    return true;
}

/* ---------------------------------------------------------------------------
 * Worker 线程 — 本地状态机
 * --------------------------------------------------------------------------*/

typedef enum {
    JamLocalIdle,   /* CE low,PWR_DOWN — 等待 user 按 OK */
    JamLocalActive, /* CE high,正在干扰 */
} JamLocalState;

static int32_t jammer_worker_func(void* ctx) {
    PqApp* app = ctx;
    JamLocalState local = JamLocalIdle;
    uint32_t chunk_n = 0;

    while(!app->jammer_stop_requested) {
        const bool want_run = app->jammer_running;

        /* 状态转换:Idle → Active. */
        if(want_run && local == JamLocalIdle) {
            if(pq_chip_with_nrf24(jammer_start_cb, app, ARBITER_TIMEOUT_MS)) {
                local = JamLocalActive;
                chunk_n = 0;
                FURI_LOG_I(TAG, "jam start mode=%d ch=%u",
                           (int)app->jammer_mode, app->jammer_cw_channel);
            } else {
                /* 启动失败 → 强制回 stopped,提示 GUI. */
                FURI_LOG_W(TAG, "start_cb timeout/fail");
                app->jammer_running = false;
                jammer_view_set_running(app->jammer_view, false);
                furi_delay_ms(WORKER_IDLE_YIELD_MS);
            }
            continue;
        }

        /* 状态转换:Active → Idle. */
        if(!want_run && local == JamLocalActive) {
            pq_chip_with_nrf24(jammer_stop_cb, app, ARBITER_TIMEOUT_MS);
            local = JamLocalIdle;
            FURI_LOG_I(TAG, "jam stop chunks=%lu", (unsigned long)chunk_n);
            continue;
        }

        /* Idle 等待. */
        if(local == JamLocalIdle) {
            furi_delay_ms(WORKER_IDLE_YIELD_MS);
            continue;
        }

        /* Active steady-state:跑一个 chunk. */
        if(!pq_chip_with_nrf24(jammer_chunk_cb, app, ARBITER_CHUNK_TIMEOUT_MS)) {
            FURI_LOG_W(TAG, "chunk_cb timeout, retry");
            furi_delay_ms(WORKER_IDLE_YIELD_MS);
            continue;
        }
        chunk_n++;
        app->jammer_chunk_count = chunk_n;

        /* 推送 view tick — 多信道模式上报当前 cursor 命中位置;CwCustom 上报 user ch. */
        const JammerProfile* p = profile_of(app->jammer_mode);
        uint8_t cur_ch;
        if(app->jammer_mode == JammerModeCwCustom) {
            cur_ch = app->jammer_cw_channel;
        } else {
            uint8_t total = profile_count(p);
            uint8_t prev_cursor =
                (app->jammer_sweep_cursor == 0)
                    ? (uint8_t)(total - 1)
                    : (uint8_t)(app->jammer_sweep_cursor - 1); /* 上一刻 */
            cur_ch = profile_ch_at(p, prev_cursor);
        }
        jammer_view_update_tick(app->jammer_view, cur_ch, chunk_n);

        /* Yield(R6:每 100 ms 检查停止信号):
         *   CwCustom : chunk 几乎瞬时 → 长 yield 不影响硬件持续发射
         *   多信道   : chunk 已含 dwell_us×chunk_size 的 TX 时间 → 短 yield 维持高占空比 */
        furi_delay_ms((app->jammer_mode == JammerModeCwCustom) ? CW_TICK_YIELD_MS
                                                               : WORKER_TICK_YIELD_MS);
    }

    /* 退出兜底:若仍 Active,做一次清理. */
    if(local == JamLocalActive) {
        pq_chip_with_nrf24(jammer_stop_cb, app, ARBITER_TIMEOUT_MS * 2);
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

    /* 一次性 baseline init(SETUP_AW 验证).start_cb 之后再做 CW 专属配置. */
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

    /* 4. 写本次会话日志(/ext/apps_data/pingequa/jammer/session_<ts>.csv). */
    pq_jammer_log_session(
        (uint8_t)app->jammer_mode,
        app->jammer_cw_channel,
        s_jam_session.start_tick,
        app->jammer_chunk_count,
        s_jam_session.reactive_jams_total);

    /* 5. View 是 app-owned,scene 不释放. */

    pq_scene_exit(SCENE_NAME);
}
