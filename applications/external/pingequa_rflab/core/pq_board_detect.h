/**
 * @file pq_board_detect.h
 * @brief 启动时板卡探测.规范 §5.1.
 *
 * 通过 chip_arbiter 切换到两个芯片各自的临界区,读 ID/校验寄存器:
 *   - CC1101: PARTNUM (0x30) == 0x00 且 VERSION (0x31) ∈ {0x14, 0x04}
 *   - nRF24:  写 SETUP_AW = 0x03 后读回 == 0x03,且不是 0xFF/0x00/CC1101 pattern
 *
 * 探测时序前提(由 splash_scene 在调本函数前完成):
 *   - OTG 5V 已启用
 *   - LDO 已稳定 (≥ 250 ms)
 *   - NRF24 PoR 已过 (≥ 150 ms)
 *
 * 总超时:500ms(规范).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "pq_config.h" /* PqBoardType */

/**
 * 探测结果.zero-init 等价 "type=NOT_FOUND, 两 chip 都不在,fail hint 空".
 *
 * - failure_hint 在 type == PQ_BOARD_PINGEQUA_2IN1 时**未定义**(可能是空字符串
 *   或残留),调用方应只在 NOT_FOUND 时读它.
 * - t_detect_duration_ms 总测耗时,用于性能预算监控(规范 §11.2 启动到 scanner
 *   ≤ 1.5s,板卡探测预算 ~500ms).
 */
typedef struct {
    PqBoardType type;
    bool nrf24_present;
    bool cc1101_present;
    uint32_t t_detect_duration_ms;
    char failure_hint[64]; /* error_scene 直接显示 */
} PqDetectResult;

/**
 * 同步探测两颗芯片.内部已通过 pq_chip_with_cc1101 / pq_chip_with_nrf24 切换
 * SPI 所有权,无须调用方提前 init.
 *
 * 必须先调过 pq_chip_arbiter_init() — 否则 furi_check 失败.
 *
 * @return 复合结果(zero-init 安全,可直接 = pq_board_detect()).
 */
PqDetectResult pq_board_detect(void);
