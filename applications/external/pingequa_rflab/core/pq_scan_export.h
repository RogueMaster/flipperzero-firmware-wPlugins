/**
 * @file pq_scan_export.h
 * @brief Scanner 数据 CSV 导出 — Scanner 屏长按 OK 触发,把 126 通道 hit 数 +
 *        元数据写到 SD 卡,事后用电脑/Excel 分析频谱.
 *
 * 文件路径:/ext/apps_data/pingequa/scans/scan_<timestamp_ms>.csv
 * 文件格式(自描述,不依赖 FlipperFormat,直接 CSV 标准):
 *   # PINGEQUA RF Lab Scanner Export
 *   # Boot ms: 1234567
 *   # Sweep count: 4567
 *   # Dwell us: 150
 *   # Peak channel: 42 (2442 MHz, 32 hits)
 *   ch,freq_mhz,hits
 *   0,2400,0
 *   1,2401,3
 *   ...
 *   125,2525,1
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PQ_SCAN_EXPORT_DIR "/ext/apps_data/pingequa/scans"

/**
 * 导出 hits[126] + 元数据到 CSV.
 *
 * @param hits         126 通道命中累加值数组(0..32)
 * @param hits_len     必须 = 126
 * @param peak_ch      最强通道 0..125
 * @param dwell_us     当前 dwell 配置
 * @param sweep_count  累计 sweep 次数
 * @param out_path     成功时填入实际写入的路径(NULL 安全跳过).缓冲区 ≥ 96 字节
 * @param out_path_sz  out_path 缓冲区大小
 *
 * @return  true = 成功;false = Storage 失败 / 磁盘满
 */
bool pq_scan_export_csv(
    const uint8_t* hits,
    uint8_t hits_len,
    uint8_t peak_ch,
    uint16_t dwell_us,
    uint16_t sweep_count,
    char* out_path,
    size_t out_path_sz);
