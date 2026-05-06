/**
 * @file pq_chip_arbiter.c
 * @brief 共享 SPI R-bus 芯片仲裁器实现 — **canonical Flipper 路径版**.
 *
 * 规范权威:PINGEQUA RF Toolkit 系统架构规范 v1.4 §5.5 + 附录 A.
 *
 * v0.2.1 重写说明:
 *   v0.2.0 试图"手工 mutex + 手工 alt-fn + 手工 SPI peripheral lifecycle"
 *   绕开 furi_hal_spi_acquire/release.结果丢了 furi_hal_bus_enable 等隐式
 *   依赖,真机崩溃.改回 canonical 路径:每次 with_* 用 furi_hal_spi_acquire
 *   选对应的 handle(NRF24 → external_extra,CC1101 → external),Activate
 *   event 驱动正确的 CSN.我们只额外管 PB2 双用引脚切换.
 *
 * 与规范 §5.5 的关系:
 *   - 满足 P10(原子切换) — acquire 持锁期间 PB2 + 一根 CSN 已正确状态
 *   - 满足 P14(CSN 互斥纯电平) — handle Activate/Deactivate 自动管 CSN
 *   - 满足 E2(同线程 acquire/release) — callback 风格架构上保证
 *   - 略偏规范"不调 furi_hal_spi_acquire" 的字面要求(E7),但 E7 真实教训
 *     是"handle 选择别错"(v0.1.4 用 external 驱 PC3 失败).只要选对 handle
 *     Activate 驱动 cs 就是对的.
 */
#include "pq_chip_arbiter.h"

#include <furi.h>
#include <furi_hal_gpio.h>
#include <furi_hal_resources.h>
#include <furi_hal_spi.h>

/* furi_hal_spi_bus_handle_external_extra (CS=PC3, NRF24) lives in
 * furi_hal_spi_config.c but is not always exported in SDK headers.
 * Forward-declare it — present in firmware binary since 0.86. */
extern FuriHalSpiBusHandle furi_hal_spi_bus_handle_external_extra;

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
static bool m_in_callback = false; /* pq_chip_spi_trx / pq_chip_nrf24_ce_set 合法性锚点 */
static const FuriHalSpiBusHandle* m_active_handle = NULL; /* 当前 acquire 的 handle */

/* ---------------------------------------------------------------------------
 * 公开 API
 * --------------------------------------------------------------------------*/

void pq_chip_arbiter_init(const PqBoardConfig* cfg) {
    UNUSED(cfg);
    furi_check(!m_initialized);

    /* 两个 handle 都做 bus_handle_init.它把 cs pin 配为 OutputPushPull HIGH
     * (spi_config.c:199-201),保证两芯片都 deselect.之后 acquire/release
     * 自动管 alt-fn pins + cs pin 电平. */
    furi_hal_spi_bus_handle_init(&furi_hal_spi_bus_handle_external);       /* PA4=CC1101 */
    furi_hal_spi_bus_handle_init(&furi_hal_spi_bus_handle_external_extra); /* PC3=NRF24 */

    m_last_owner = PqOwnerNone;
    m_in_callback = false;
    m_active_handle = NULL;
    m_initialized = true;
}

bool pq_chip_with_nrf24(PqChipCallback cb, void* ctx, uint32_t timeout_ms) {
    furi_check(m_initialized);
    UNUSED(timeout_ms); /* furi_hal_spi_acquire 内部用 FuriWaitForever — 无 timeout
                         * 概念.规范 §5.5 timeout 在此简化为 "尽快取". */

    /* 1. PB2 切到 NRF24 CE 模式(若上次不是 NRF24). */
    if(m_last_owner != PqOwnerNrf24) {
        furi_hal_gpio_init(&gpio_ext_pb2, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
        furi_hal_gpio_write(&gpio_ext_pb2, false); /* CE = standby */
    }

    /* 2. 选 external_extra(cs=PC3=NRF24).Activate 自动驱 PC3 LOW. */
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_external_extra);
    m_active_handle = &furi_hal_spi_bus_handle_external_extra;

    /* 3. Callback. */
    m_in_callback = true;
    bool ok = (cb != NULL) ? cb(ctx) : true;
    m_in_callback = false;

    /* 4. Release.Deactivate 自动驱 PC3 HIGH + 三线 Analog. */
    m_active_handle = NULL;
    furi_hal_spi_release(&furi_hal_spi_bus_handle_external_extra);

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

    /* 2. 选 external(cs=PA4=CC1101).Activate 自动驱 PA4 LOW. */
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_external);
    m_active_handle = &furi_hal_spi_bus_handle_external;

    /* 3. Callback. */
    m_in_callback = true;
    bool ok = (cb != NULL) ? cb(ctx) : true;
    m_in_callback = false;

    /* 4. Release. */
    m_active_handle = NULL;
    furi_hal_spi_release(&furi_hal_spi_bus_handle_external);

    m_last_owner = PqOwnerCc1101;
    return ok;
}

void pq_chip_nrf24_ce_set(bool high) {
    furi_check(m_initialized);
    furi_check(m_in_callback);
    furi_check(m_last_owner == PqOwnerNrf24 || m_active_handle == &furi_hal_spi_bus_handle_external_extra);
    furi_hal_gpio_write(&gpio_ext_pb2, high);
}

bool pq_chip_spi_trx(const uint8_t* tx, uint8_t* rx, size_t size, uint32_t timeout_ms) {
    furi_check(m_initialized);
    furi_check(m_in_callback);
    furi_check(m_active_handle != NULL);
    furi_check(size > 0);

    /* CSN 命令边界框定(NRF24 数据手册 §8 / huuck nrf24.c::nrf24_spi_trx):
     *
     *   开头 CSN HIGH → 1µs idle → CSN LOW → bytes → 末尾 CSN HIGH(锁存命令)
     *
     * v0.3.0 之前末尾未拉 CSN HIGH,导致 W_REGISTER / W_TX_PAYLOAD 命令仅在
     * 下一条 SPI 调用开始时才被锁存.对 setup_cw 这条 setup 序列致命:
     *   - CONFIG=PWR_UP 写后 furi_delay_ms(2) 等的是 Tpd2stby — 但 CONFIG 还
     *     没被锁存,2ms 实际上等的是空气,芯片仍在 PWR_DOWN
     *   - W_TX_PAYLOAD 写后 CE 立刻拉 HIGH — 但 W_TX_PAYLOAD 也没锁存,FIFO
     *     仍空 → 即使 PWR_UP 真生效了也不会辐射载波
     * v0.3.x 起 huuck 风格末尾 CSN HIGH 后,每条命令在返回时已锁存. */
    const GpioPin* cs = m_active_handle->cs;
    furi_hal_gpio_write(cs, true);
    furi_delay_us(1);
    furi_hal_gpio_write(cs, false);

    bool ok;
    if(tx == NULL) {
        uint8_t dummy[64];
        furi_check(size <= sizeof(dummy));
        for(size_t i = 0; i < size; i++) dummy[i] = 0xFF;
        ok = furi_hal_spi_bus_trx(m_active_handle, dummy, rx, size, timeout_ms);
    } else {
        ok = furi_hal_spi_bus_trx(m_active_handle, (uint8_t*)tx, rx, size, timeout_ms);
    }

    /* 末尾拉 CSN HIGH 锁存命令(关键修复 — 见上方说明). */
    furi_hal_gpio_write(cs, true);
    return ok;
}

void pq_chip_arbiter_force_release(void) {
    if(!m_initialized) return;

    /* 若当前持有 handle,正常 release(Deactivate 会驱 cs HIGH + 清三线). */
    if(m_active_handle) {
        furi_hal_spi_release(m_active_handle);
        m_active_handle = NULL;
    }

    /* PB2 → Analog(规范 v1.4 §2.4 退出配置).两根 CSN 由 bus_handle_deinit
     * 在 deinit 阶段统一回 Analog. */
    furi_hal_gpio_init(&gpio_ext_pb2, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

    m_in_callback = false;
    m_last_owner = PqOwnerNone;
}

void pq_chip_arbiter_deinit(void) {
    if(!m_initialized) return;

    pq_chip_arbiter_force_release();

    /* bus_handle_deinit 把 cs pin 回 Analog(spi_config.c:202-204). */
    furi_hal_spi_bus_handle_deinit(&furi_hal_spi_bus_handle_external);
    furi_hal_spi_bus_handle_deinit(&furi_hal_spi_bus_handle_external_extra);

    m_initialized = false;
}
