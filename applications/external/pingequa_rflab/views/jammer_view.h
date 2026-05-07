/**
 * @file jammer_view.h
 * @brief NRF24 Jammer 自定义 View(规范 §6.2 v1.2 + §12).
 *
 * 屏布局(128×64):
 *   y=0..7    Header — "JAMMER" + RUN/STOP 状态右对齐
 *   y=8       Divider 1
 *   y=10..20  Mode 大字:"CW" 或 "SWEEP"
 *   y=22..32  信道/范围:"Ch 042" 或 "0-125"
 *   y=34..42  频率:"2442 MHz" 或 "2400-2525 MHz"
 *   y=44..50  TX 块数 / 频段标签(BLE 37 / WiFi 6 等)
 *   y=51      Divider 2
 *   y=53..62  Footer — 输入提示
 *
 * 模型用 ViewModelTypeLocking — worker 与 GUI 线程都能安全更新.
 */
#pragma once

#include <gui/view.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * 6 种预设干扰模式(规范 §13 Phase 5,参考 huuck jam_type 与 W0rthlessS0ul
 * jam.cpp 分类做法).
 *
 * 引擎类型(jammer_scene.c profiles 表里映射):
 *   - CW          : RF_SETUP CONT_WAVE+PLL_LOCK,TX FIFO 灌 32B 0xFF
 *                   (适合广谱压制 / BLE 对 GFSK 调制有效)
 *   - PayloadSpam : RF_SETUP 普通 2 Mbps,W_TX_PAYLOAD_NOACK 0xDEADBEEF
 *                   持续刷 FIFO(适合 OFDM WiFi 对前导码窗口干扰)
 *
 * 信道范围(NRF24 ch = MHz - 2400):
 *   - CwCustom : 单 ch,用户 ←→ 选 0..125
 *   - BleAdv   : 3 ch hop {2, 26, 80} = BLE adv 37/38/39
 *   - Wifi1    : 1..23   (2401..2423 MHz,WiFi 1 中心 2412 ±11)
 *   - Wifi6    : 26..48  (2426..2448 MHz,WiFi 6 中心 2437 ±11)
 *   - Wifi11   : 51..73  (2451..2473 MHz,WiFi 11 中心 2462 ±11)
 *   - AllBand  : 0..125  (全 2.4 GHz ISM 扫)
 */
typedef enum {
    JammerModeCwCustom = 0,    /* CW,用户自选信道                            */
    JammerModeBleAdv,          /* CW,BLE adv 37/38/39 hop(盲扫)             */
    JammerModeReactiveBle,     /* RPD 反应式 BLE adv(v0.4.x,首创)           */
    JammerModeWifi1,           /* PayloadSpam,WiFi 1 ±7 MHz                 */
    JammerModeWifi6,           /* PayloadSpam,WiFi 6 ±7 MHz                 */
    JammerModeWifi11,          /* PayloadSpam,WiFi 11 ±7 MHz                */
    JammerModeAllBand,         /* CW,全频 sweep 0..125                       */
    JammerModeCount,
} JammerMode;

typedef struct JammerView JammerView;

JammerView* jammer_view_alloc(void);
void jammer_view_free(JammerView* jv);

/** 取底层 View*,用于 view_dispatcher_add_view + 注册 input_callback. */
View* jammer_view_get_view(JammerView* jv);

/* ---- 模型更新 API(任意线程可调) ---- */

/** 用户 ^/v 切模式后 GUI 线程调用. */
void jammer_view_set_mode(JammerView* jv, JammerMode mode);

/** 用户 </> 调 CW 信道后 GUI 线程调用. */
void jammer_view_set_cw_channel(JammerView* jv, uint8_t ch);

/** OK 切启停后 GUI 线程调用. */
void jammer_view_set_running(JammerView* jv, bool running);

/** Worker 每完成一个 chunk 调用 — 推送 sweep 当前信道 + 累计 chunk 计数. */
void jammer_view_update_tick(JammerView* jv, uint8_t cur_channel, uint32_t chunk_count);
