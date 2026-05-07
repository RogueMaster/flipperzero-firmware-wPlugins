/**
 * @file pq_jammer_log.h
 * @brief Jammer 会话日志 — 每次退出 jammer 自动写一条 CSV 摘要,事后分析用.
 *
 * 文件路径:/ext/apps_data/pingequa/jammer/session_<boot_ms>.csv
 *
 * 单文件格式(自描述,Excel/Python pandas 可读):
 *   # PINGEQUA RF Lab Jammer Session
 *   # Mode: BLE React
 *   # Mode index: 2
 *   # CW channel: 42
 *   # Start boot ms: 1234567
 *   # End boot ms: 1253217
 *   # Duration ms: 18650
 *   # Total chunks (last run): 477
 *   # Total reactive jams (BLE React only): 1432
 *   key,value
 *   mode_index,2
 *   mode_name,BLE React
 *   cw_channel,42
 *   duration_ms,18650
 *   chunks,477
 *   reactive_jams,1432
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define PQ_JAMMER_LOG_DIR "/ext/apps_data/pingequa/jammer"

/**
 * 写一条会话摘要.调用方在 jammer_scene_on_exit 自动调一次.
 *
 * @param mode_index    JammerMode 枚举值(0..6)
 * @param cw_channel    CW Custom 选定 ch(其他模式忽略,但仍记录)
 * @param start_ms      会话开始的 furi_get_tick()
 * @param chunks        worker 运行总 chunk 数
 * @param reactive_jams BLE React 模式下累计 RPD 反应次数(其他模式 = chunks)
 *
 * @return  true = 写盘成功
 */
bool pq_jammer_log_session(
    uint8_t mode_index,
    uint8_t cw_channel,
    uint32_t start_ms,
    uint32_t chunks,
    uint32_t reactive_jams);
