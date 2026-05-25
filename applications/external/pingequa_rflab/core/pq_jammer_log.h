/**
 * @file pq_jammer_log.h
 * @brief Jammer 会话日志 — 每次退出 jammer 自动写一条 CSV 摘要,事后分析用.
 *
 * 文件路径:/ext/apps_data/pingequa/siggen/siggen_<YYYY-MM-DD>_<HHMMSS>_<mode>_<dur>s.csv
 *   - 日期在前 → qFlipper 文件列表按名排序即按时间排序
 *   - 文件名带模式短名 + 会话时长(秒),无需打开即知摘要
 *   - RTC 未设时回退 siggen_boot<tick>_<mode>_<dur>s.csv;同秒重名加 _1.._99 后缀
 *
 * 单文件格式(v0.5.2 重构,无重复段;pandas read_csv(comment='#') 可直读):
 *   # PINGEQUA RF Lab Signal Generator Session
 *   key,value
 *   datetime,2026-05-15 14:30:22
 *   mode,BLE React
 *   engine,Reactive
 *   target_freq_mhz,2402/2426/2480 (BLE adv)
 *   scene_duration_s,9.7
 *   reactive_events,15         ← 仅 BLE React 模式输出
 * 或(CW Custom 模式):
 *   ...
 *   cw_channel,42              ← 仅 CW Custom 模式输出
 *   cw_freq_mhz,2442           ← 仅 CW Custom 模式输出
 *
 * 注:scene_duration_s 是用户停留 jammer scene 的时长(含 OK 暂停期间的空闲),
 *     **不是**真实 TX 发射时长.真实 TX 时长需 worker 累计 active 态时间,v0.5.2
 *     暂未做(避免动 worker 影响稳定性).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define PQ_JAMMER_LOG_DIR "/ext/apps_data/pingequa/siggen"

/**
 * 写一条会话摘要.调用方在 jammer_scene_on_exit 自动调一次.
 *
 * @param mode_index    JammerMode 枚举值(0..6)
 * @param cw_channel    CW Custom 选定 ch(仅 CwCustom 模式被记录)
 * @param start_ms      会话开始的 furi_get_tick()(用于计算 scene_duration_s)
 * @param reactive_jams 仅 BLE React 模式有意义:累计 RPD 反应次数
 *
 * @return  true = 写盘成功
 */
bool pq_jammer_log_session(
    uint8_t mode_index,
    uint8_t cw_channel,
    uint32_t start_ms,
    uint32_t reactive_jams);
