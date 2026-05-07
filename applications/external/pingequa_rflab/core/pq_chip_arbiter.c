/**
 * @file pq_chip_arbiter.c
 * @brief 共享 SPI R-bus 芯片仲裁器实现 — API-safe 路径版.
 *
 * 规范权威:PINGEQUA RF Toolkit 系统架构规范 v1.4 §5.5 + 附录 A.
 *
 * v0.2.2 重写说明:
 *   v0.2.1 使用 furi_hal_spi_bus_handle_external_extra(CS=PC3),该 handle
 *   在新固件 API 表里没有导出 — APPCHK 报 "Symbols not resolved"。
 *   修改为只使用 furi_hal_spi_bus_handle_external(在 API 表中),手动管理
 *   两根 CS 引脚:acquire 后用 gpio_write 把 PA4 拉 HIGH、PC3 拉 LOW 切
 *   到 NRF24;CC1101 路径不变(acquire Activate 自动把 PA4 拉 LOW).
 *   SPI 总线的 enable / 时钟 / MISO-MOSI-SCK alt-fn 全部仍由 acquire/release
 *   管理,不丢 furi_hal_bus_enable 依赖(v0.2.0 崩溃根因).
 *
 * 与规范 §5.5 的关系:
 *   - 满足 P10(原子切换) — acquire 持锁期间 m_active_cs 已正确状态
 *   - 满足 P14(CSN 互斥纯电平) — init 保证 PC3 HIGH;acquire 之后才拉 LOW
 *   - 满足 E2(同线程 acquire/release) — callback 风格架构上保证
 *   - 兼容 E7 — 不再使用非 API 符号
 */
#include "pq_chip_arbiter.h"

#include <furi.h>
#include <furi_hal_gpio.h>
#include <furi_hal_resources.h>
#include <furi_hal_spi.h>

#define TAG "PqChipArbiter"

/* ---------------------------------------------------------------------------
 * 模块状态
 * --------------------------------------------------------------------------*/

typedef enum {
    PqOwnerNone = 0,
    PqOwnerNrf24,
    PqOwnerCc1101,
} PqOwner;

static PqOwner m_last_owner = PqOwnerNone;
static bool m_initialized = false;
static bool m_in_callback = false;
static const GpioPin* m_active_cs = NULL; /* 当前事务实际 CS 引脚 */

/* ---------------------------------------------------------------------------
 * 公开 API
 * --------------------------------------------------------------------------*/

void pq_chip_arbiter_init(const PqBoardConfig* cfg) {
    UNUSED(cfg);
    furi_check(!m_initialized);

    /* external handle init — 把 PA4 配为 OutputPushPull HIGH(CC1101 deselect). */
    furi_hal_spi_bus_handle_init(&furi_hal_spi_bus_handle_external);

    /* PC3(NRF24 CS)不由 SDK handle 管,手工初始化为 OutputPushPull HIGH. */
    furi_hal_gpio_write(&gpio_ext_pc3, true);
    furi_hal_gpio_init(&gpio_ext_pc3, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);

    m_last_owner = PqOwnerNone;
    m_in_callback = false;
    m_active_cs = NULL;
    m_initialized = true;
}

bool pq_chip_with_nrf24(PqChipCallback cb, void* ctx, uint32_t timeout_ms) {
    furi_check(m_initialized);
    UNUSED(timeout_ms);

    /* 1. PB2 切到 NRF24 CE 模式(若上次不是 NRF24). */
    if(m_last_owner != PqOwnerNrf24) {
        furi_hal_gpio_init(&gpio_ext_pb2, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
        furi_hal_gpio_write(&gpio_ext_pb2, false); /* CE = standby */
    }

    /* 2. acquire external:使能 SPI 总线时钟 + MISO/MOSI/SCK alt-fn.
     *    Activate 事件把 PA4(CC1101 CS)拉 LOW — 紧接着手工拉 HIGH 覆盖,
     *    再把 PC3(NRF24 CS)拉 LOW.两芯片 CS 互斥由此保证. */
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_external);
    furi_hal_gpio_write(&gpio_ext_pa4, true);  /* PA4 = CC1101 CS HIGH(覆盖 Activate) */
    furi_hal_gpio_write(&gpio_ext_pc3, false); /* PC3 = NRF24  CS LOW (select) */
    m_active_cs = &gpio_ext_pc3;

    /* 3. Callback. */
    m_in_callback = true;
    bool ok = (cb != NULL) ? cb(ctx) : true;
    m_in_callback = false;

    /* 4. Release:先把 PC3 拉 HIGH,再 release(Deactivate 把 PA4 回 HIGH/Analog). */
    furi_hal_gpio_write(&gpio_ext_pc3, true);
    m_active_cs = NULL;
    furi_hal_spi_release(&furi_hal_spi_bus_handle_external);

    m_last_owner = PqOwnerNrf24;
    return ok;
}

bool pq_chip_with_cc1101(PqChipCallback cb, void* ctx, uint32_t timeout_ms) {
    furi_check(m_initialized);
    UNUSED(timeout_ms);

    /* 1. PB2 切回 Analog(让 CC1101 GD0/TIM 输入捕获自由配置). */
    if(m_last_owner != PqOwnerCc1101) {
        furi_hal_gpio_init(&gpio_ext_pb2, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    }

    /* 2. acquire external:Activate 自动把 PA4(CC1101 CS)拉 LOW. */
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_external);
    m_active_cs = &gpio_ext_pa4;

    /* 3. Callback. */
    m_in_callback = true;
    bool ok = (cb != NULL) ? cb(ctx) : true;
    m_in_callback = false;

    /* 4. Release. */
    m_active_cs = NULL;
    furi_hal_spi_release(&furi_hal_spi_bus_handle_external);

    m_last_owner = PqOwnerCc1101;
    return ok;
}

void pq_chip_nrf24_ce_set(bool high) {
    furi_check(m_initialized);
    furi_check(m_in_callback);
    furi_check(m_active_cs == &gpio_ext_pc3);
    furi_hal_gpio_write(&gpio_ext_pb2, high);
}

bool pq_chip_spi_trx(const uint8_t* tx, uint8_t* rx, size_t size, uint32_t timeout_ms) {
    furi_check(m_initialized);
    furi_check(m_in_callback);
    furi_check(m_active_cs != NULL);
    furi_check(size > 0);

    /* CSN 命令边界框定:每条命令前 CSN HIGH → 1µs idle → CSN LOW.
     * 确保上一条命令在 SPI 引脚空闲期被锁存(NRF24 §8 / CC1101 要求相同). */
    furi_hal_gpio_write(m_active_cs, true);
    furi_delay_us(1);
    furi_hal_gpio_write(m_active_cs, false);

    bool ok;
    if(tx == NULL) {
        uint8_t dummy[64];
        furi_check(size <= sizeof(dummy));
        for(size_t i = 0; i < size; i++) dummy[i] = 0xFF;
        ok = furi_hal_spi_bus_trx(&furi_hal_spi_bus_handle_external, dummy, rx, size, timeout_ms);
    } else {
        ok = furi_hal_spi_bus_trx(
            &furi_hal_spi_bus_handle_external, (uint8_t*)tx, rx, size, timeout_ms);
    }

    /* 末尾拉 CSN HIGH 锁存命令. */
    furi_hal_gpio_write(m_active_cs, true);
    return ok;
}

void pq_chip_arbiter_force_release(void) {
    if(!m_initialized) return;

    /* 若当前持有 handle,release(Deactivate 驱 PA4 HIGH + 清三线).
     * 同时确保 PC3(NRF24 CS)回 HIGH. */
    if(m_active_cs != NULL) {
        furi_hal_gpio_write(&gpio_ext_pc3, true);
        m_active_cs = NULL;
        furi_hal_spi_release(&furi_hal_spi_bus_handle_external);
    }

    furi_hal_gpio_init(&gpio_ext_pb2, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

    m_in_callback = false;
    m_last_owner = PqOwnerNone;
}

void pq_chip_arbiter_deinit(void) {
    if(!m_initialized) return;

    pq_chip_arbiter_force_release();

    /* bus_handle_deinit 把 PA4 回 Analog. */
    furi_hal_spi_bus_handle_deinit(&furi_hal_spi_bus_handle_external);

    /* PC3 手工回 Analog. */
    furi_hal_gpio_write(&gpio_ext_pc3, true);
    furi_hal_gpio_init(&gpio_ext_pc3, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

    m_initialized = false;
}
