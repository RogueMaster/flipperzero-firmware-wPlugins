/**
 * @file pq_cc1101.h
 * @brief CC1101 极简驱动(Phase 1.0 仅 quiet_gd0).规范 §5.4(简短).
 *
 * Phase 1.0 不做完整 CC1101 操作,只为启动时把 CC1101 的 GD0 引脚释放掉
 * (避免 CC1101 默认配置驱 GD0 = PB2,与 NRF24 CE 抢线).未来 v1.2 Jammer
 * 等需要时再扩展.
 *
 * 所有函数 MUST 在 pq_chip_with_cc1101() callback 内调用(同 pq_nrf24).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct PqBoardConfig PqBoardConfig;
typedef struct PqCc1101 PqCc1101;

/** 分配 / 释放. */
PqCc1101* pq_cc1101_alloc(const PqBoardConfig* cfg);
void pq_cc1101_free(PqCc1101* dev);

/**
 * 让 CC1101 释放 GD0 引脚:
 *   1. 发 SIDLE strobe(0x36) — 强制 IDLE 状态
 *   2. 写 IOCFG0(0x02) = 0x2E(HIGHZ)— GD0 进高阻
 *
 * 启动序列(规范 §7)中调一次即可.之后 CC1101 就不再驱 PB2,
 * NRF24 可以安全地把 PB2 配为 CE 输出.
 *
 * @return  当前实现总返回 true(SPI 写入成功无法独立验证;若后续 NRF24
 *          probe 通过即可推断 PB2 被让出来了).
 */
bool pq_cc1101_quiet_gd0(PqCc1101* dev);

/**
 * 让 CC1101 进 SLEEP(SPWD strobe 0x39) — 比 IDLE 更深,crystal 关.
 *
 * 用于 NRF24 干扰场景下消除 CC1101 26 MHz 振荡器谐波(尽管 2.4 GHz 衰减极
 * 严重,聊胜于无).SLEEP 后下一次 SIDLE 唤醒需 240 µs(数据手册 §6.5).
 *
 * MUST 在 pq_chip_with_cc1101() callback 内调用.
 */
bool pq_cc1101_sleep(PqCc1101* dev);
