/**
 * @file pq_chip_arbiter.h
 * @brief 共享 SPI R-bus 芯片仲裁器(callback 风格).
 *
 * 规范权威定义:PINGEQUA RF Toolkit 系统架构规范 v1.4 §5.5 + 附录 A.
 *
 * 本模块是 PA4(CC1101 CSN) / PC3(NRF24 CSN) / PB2(GD0/CE 双用) 的
 * 唯一允许直接驱动者(规范 M15 / M16).其它 Core 驱动(pq_nrf24,
 * pq_cc1101) 必须通过 pq_chip_with_* 进入芯片访问临界区(规范 M3).
 *
 * 设计要点:
 *   - callback 风格 → 同线程 acquire/release,绕过 FreeRTOS owner-tracked
 *     mutex 跨线程的 UB(经验 E2,源:furi_hal_spi_config.c:103)
 *   - 不调 furi_hal_spi_acquire/release,init 时一次性配 SPI1 alt-fn,
 *     之后只翻 CSN 电平互斥(规范 P11/P14,经验 E7,源:
 *     furi_hal_spi_config.c:200-360)
 *   - last_owner 缓存避免冗余 PB2 重配
 *
 * 完整决策与 file:line 引用见 plan:
 *   /Users/mac/.claude/plans/users-mac-desktop-pingequa-rf-toolkit-s-abundant-storm.md
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * 前向声明.PqBoardConfig 由 core/pq_config.h 完整定义;那里必须使用带 tag 的
 * struct 形式(`struct PqBoardConfig { ... };` + typedef),否则本前向声明无效.
 */
typedef struct PqBoardConfig PqBoardConfig;

/**
 * 芯片访问回调.在 pq_chip_with_* 临界区内被调用恰一次.
 * 返回值原样作为 with_* 的返回值传递给调用方.
 *
 * 约束(规范 §5.5 关键设计 + §11.2):
 *   - 单次执行 ≤ 50ms (M11)
 *   - 严禁嵌套调用任何 pq_chip_with_*
 *   - 严禁调 furi_hal_spi_acquire/release(M18)
 *   - 严禁调 furi_hal_gpio_init(PB2/PA4/PC3) (M15;CE 改 set 用
 *     pq_chip_nrf24_ce_set)
 */
typedef bool (*PqChipCallback)(void* ctx);

/**
 * 进入 NRF24 临界区.序列(规范 p07 §2.5):
 *   PA4 HIGH(若上次是 CC1101) → PB2 = OutputPushPull/Low(NRF24 CE standby)
 *   → PC3 LOW → cb(ctx) → PC3 HIGH.
 * 整个流程持有 SPI bus mutex,callback 同线程内完成 acquire/release(E2).
 *
 * @param cb         芯片访问回调,可为 NULL(此时仅完成切换,等于无操作)
 * @param ctx        透传给 cb 的上下文
 * @param timeout_ms 等待 bus mutex 的超时(毫秒);超时返回 false
 * @return  false  = mutex 超时 OR cb 返回 false
 *          true   = mutex 取到且 cb 返回 true(或 cb == NULL)
 */
bool pq_chip_with_nrf24(PqChipCallback cb, void* ctx, uint32_t timeout_ms);

/**
 * 进入 CC1101 临界区.对称序列(规范 p07 §2.5):
 *   PC3 HIGH(若上次是 NRF24) → PB2 = Analog(复用 GD0 / TIM 输入捕获)
 *   → PA4 LOW → cb(ctx) → PA4 HIGH.
 */
bool pq_chip_with_cc1101(PqChipCallback cb, void* ctx, uint32_t timeout_ms);

/**
 * NRF24 CE 直驱(规范 §5.4 强制约束 M16).
 *
 * **仅在 pq_chip_with_nrf24 的 callback 内允许调用.** 其它语境下行为未定义
 * (debug 构建会 furi_assert 失败).pq_nrf24 的 fast-scan 路径每通道翻 CE
 * 时通过此 API 完成 — 纯 GPIO 写,不涉及 SPI.
 */
void pq_chip_nrf24_ce_set(bool high);

#include <stddef.h>

/**
 * 共享 SPI R-bus 字节传输.**仅在 pq_chip_with_* 的 callback 内有效**(其它
 * 语境下 furi_assert 失败).pq_nrf24 / pq_cc1101 寄存器层通过本函数完成
 * 所有 SPI 事务,无须接触 Flipper SPI HAL 的 handle / ownership 概念.
 *
 * 半双工 trx:tx/rx 长度同为 size,任一缓冲区可为 NULL(NULL tx 发 0xFF,
 * NULL rx 丢弃读到的字节).等价于 furi_hal_spi_bus_trx(furi_hal_spi.c:130-171)
 * 的字节轮询逻辑,但绕开它的 ownership 检查(我们用 chip_arbiter 自己的
 * mutex + CSN 互斥,不依赖 bus->current_handle 字段).
 *
 * @param tx       发送缓冲(NULL = 全发 0xFF,典型用于读寄存器后续字节)
 * @param rx       接收缓冲(NULL = 丢弃)
 * @param size     字节数(必须 > 0)
 * @param timeout_ms 兼容参数,**当前未使用**(furi_hal_spi.c:81 的 timeout
 *                 同样未实现);保留以备将来切到 DMA 模式
 * @return  实际成功完成 SPI 事务时 true.当前实现只会 furi_check 失败而非
 *          返回 false(同 furi_hal_spi_bus_trx 行为).
 */
bool pq_chip_spi_trx(const uint8_t* tx, uint8_t* rx, size_t size, uint32_t timeout_ms);

/**
 * 一次性硬件初始化.必须在任何 pq_chip_with_* 之前调用一次.
 *
 * 行为(对应 furi_hal_spi_config.c:199-229 的等价手工版,但只做一次):
 *   - 创建内部 FuriMutex(同线程 acquire/release,规范 E2)
 *   - PA4 / PC3 → OutputPushPull/PullUp/VeryHigh,写 HIGH(两芯片都 deselect)
 *   - PA6 / PA7 / PB3 → AltFn5_SPI1 / PushPull / PullDown / VeryHigh
 *   - LL_SPI_Init(bus->spi, &furi_hal_spi_preset_1edge_low_2m) + Enable
 *   - last_owner = None
 *
 * @param cfg 板配置;暂不使用其字段(全部硬连),保留参数以便后续 pq_config
 *            落地后切换到配置驱动.可传 NULL.
 */
void pq_chip_arbiter_init(const PqBoardConfig* cfg);

/**
 * 退出清理(规范 §7 退出序列).
 *
 * 行为:
 *   - 调用 pq_chip_arbiter_force_release()
 *   - PA6 / PA7 / PB3 → Analog(等价 furi_hal_spi_config.c:233-235)
 *   - LL_SPI_Disable
 *   - 释放内部 mutex
 *
 * 调用后,任何 pq_chip_with_* 行为未定义.
 */
void pq_chip_arbiter_deinit(void);

/**
 * 幂等紧急回滚.用于 app_shutdown 兜底路径(规范 §7).
 *
 * 行为(无论当前 mutex 是否持有):
 *   - 若 mutex 当前持有,释放;否则 no-op
 *   - PB2 → Analog
 *   - PA4 / PC3 → Analog(规范 v1.4 强制 §2.4 退出配置)
 *   - **不**关 OTG(系统层管,规范 §5.5)
 *
 * 调用后仍可再次 init.
 */
void pq_chip_arbiter_force_release(void);
