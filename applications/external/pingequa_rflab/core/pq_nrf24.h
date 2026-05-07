/**
 * @file pq_nrf24.h
 * @brief NRF24L01+ 寄存器层驱动.规范 §5.4 + 附录 A.
 *
 * **本文件所有寄存器函数都假设调用者已在 pq_chip_with_nrf24() 的 callback
 * 内** — 即 SPI bus mutex 已持有,NRF24 CSN 已 LOW,可立即发字节命令.
 * 在 callback 外调用结果未定义(pq_chip_spi_trx 会 furi_assert 失败).
 *
 * 设计要点(规范 M15-M18):
 *   - 不调 furi_hal_spi_acquire/release
 *   - 不操作 PA4/PC3/PB2 GPIO(那是 chip_arbiter 的责任)
 *   - NRF24 CE 翻转必须经 pq_chip_nrf24_ce_set()(M16)
 *   - 本驱动只关心 SPI 字节序列
 *
 * 典型调用模式(scanner_view 风格):
 *   bool sweep_one(void* ctx) {
 *       Ctx* c = ctx;
 *       for (uint8_t ch = 0; ch < NRF24_CHANNEL_COUNT; ch++) {
 *           pq_chip_nrf24_ce_set(false);
 *           pq_nrf24_set_channel(c->dev, ch);
 *           pq_nrf24_write_reg(c->dev, NRF24_REG_CONFIG,
 *                              NRF24_CONFIG_PWR_UP | NRF24_CONFIG_PRIM_RX |
 *                              NRF24_CONFIG_MASK_MAX_RT |
 *                              NRF24_CONFIG_MASK_TX_DS |
 *                              NRF24_CONFIG_MASK_RX_DR);
 *           pq_chip_nrf24_ce_set(true);
 *           furi_delay_us(c->dwell_us);
 *           if (pq_nrf24_read_rpd(c->dev)) c->hits[ch]++;
 *           pq_chip_nrf24_ce_set(false);
 *       }
 *       return true;
 *   }
 *   pq_chip_with_nrf24(sweep_one, &ctx, 50);
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/* 前向声明:由 core/pq_config.h 完整定义. */
typedef struct PqBoardConfig PqBoardConfig;

/** 不透明驱动句柄.未来 v1.2 Jammer 模式需要内部状态时再扩展. */
typedef struct PqNrf24 PqNrf24;

/* ---------------------------------------------------------------------------
 * 生命周期
 * --------------------------------------------------------------------------*/

/**
 * 分配驱动句柄.cfg 仅保存指针(不拷贝),调用者保证 cfg 在 PqNrf24 生命
 * 期内有效.cfg 可为 NULL(此时 pin 字段使用规范默认值).
 */
PqNrf24* pq_nrf24_alloc(const PqBoardConfig* cfg);

/** 释放.传 NULL 是 no-op. */
void pq_nrf24_free(PqNrf24* dev);

/* ---------------------------------------------------------------------------
 * 寄存器层(必须在 pq_chip_with_nrf24 callback 内调用)
 * --------------------------------------------------------------------------*/

/**
 * 写一字节寄存器.cmd = W_REGISTER | (reg & 0x1F).
 * 规范 §5.4:不返回 STATUS(callback 风格不需要),需要的话单独 read.
 */
void pq_nrf24_write_reg(PqNrf24* dev, uint8_t reg, uint8_t val);

/**
 * 读一字节寄存器.cmd = R_REGISTER | (reg & 0x1F).
 * @return  寄存器内容字节(不是 STATUS).
 */
uint8_t pq_nrf24_read_reg(PqNrf24* dev, uint8_t reg);

/**
 * 写 RF_CH(自动 clamp 到 0..NRF24_CHANNEL_MAX).
 */
void pq_nrf24_set_channel(PqNrf24* dev, uint8_t ch);

/**
 * 读 RPD(Received Power Detector,寄存器 0x09 bit 0).
 * 仅在 CE 高且 RX 模式下有意义.
 * @return true = 检测到 > -64 dBm 信号(数据手册 §6.4).
 */
bool pq_nrf24_read_rpd(PqNrf24* dev);

/**
 * Channel Scan 的 baseline 寄存器初始化:
 *   - CONFIG = 0(PWR_DOWN, IRQ unmasked, CRC off)
 *   - EN_AA = 0(disable auto-ack on all pipes)
 *   - SETUP_RETR = 0(no retransmit)
 *   - RF_SETUP = 0x0E(2 Mbps + 0 dBm,bandwidth 最大化便于 RPD)
 *   - DYNPD = 0,FEATURE = 0
 *   - STATUS = 0x70(清三个 IRQ flag)
 *   - SETUP_AW writeback 验证(0x03 写入后读回 == 0x03)
 *   - FLUSH_RX,FLUSH_TX
 *
 * @return false = SETUP_AW writeback 不一致(芯片不存在 / SPI 失败).
 *         调用者应展示错误屏(规范 §7).
 */
bool pq_nrf24_init_regs(PqNrf24* dev);

/* ---------------------------------------------------------------------------
 * v1.2 Jammer 扩展(同样要在 pq_chip_with_nrf24 callback 内调用)
 * --------------------------------------------------------------------------*/

/**
 * 配置成 Constant Wave 干扰发射模式(NRF24L01+ datasheet §6.4 / Nordic AN).
 *   - EN_AA / EN_RXADDR / SETUP_RETR = 0(纯 TX,无协议层)
 *   - RF_CH = ch(自动 clamp 到 0..NRF24_CHANNEL_MAX)
 *   - RF_SETUP = CONT_WAVE | PLL_LOCK | RF_PWR=11(0 dBm at die,PA 模块拉到 +20)
 *   - CONFIG = PWR_UP(PRIM_RX=0)
 *   - 内部 furi_delay_ms(NRF24_TPD2STBY_MS) 等待 PWR_DOWN→Standby
 *
 * 调用后:
 *   - 上层应 `pq_chip_nrf24_ce_set(true)` 启动发射(CE 持续 HIGH)
 *   - sweep 模式下用 `pq_nrf24_set_channel_fast` 直接换频,CE 不需要 toggle
 *     (datasheet §6.1.1:PLL re-lock 在 Tstby2a = 130 µs 内自完成)
 *
 * @return  与 init_regs 相同语义 — 这里不重做 SETUP_AW 验证(假定 init_regs
 *          已成功调过).
 */
bool pq_nrf24_setup_cw(PqNrf24* dev, uint8_t ch);

/**
 * 写 RF_CH(无 clamp,sweep 热路径专用).
 * 调用方必须保证 ch <= NRF24_CHANNEL_MAX.
 */
void pq_nrf24_set_channel_fast(PqNrf24* dev, uint8_t ch);

/**
 * 写 W_TX_PAYLOAD 命令 + 数据(填 TX FIFO).len ≤ 32(FIFO 槽容量).
 *
 * **nRF24L01+ CW 模式必需** — 仅写 RF_SETUP 的 CONT_WAVE+PLL_LOCK 不足以让芯片
 * 真正辐射载波,TX FIFO 必须有数据(huuck/FlipperZeroNRFJammer
 * lib/nrf24/nrf24.c:163-171 + RF24 Arduino 库 RF24.cpp:2010-2025 旧版均同).
 *
 * setup_cw 内部已经调本函数填 32 字节 0xFF;一般用户无需直接调用,留作扩展.
 */
void pq_nrf24_load_tx_payload(PqNrf24* dev, const uint8_t* data, uint8_t len);

/**
 * 写 W_TX_PAYLOAD_NOACK 命令 + 数据(填 TX FIFO,但不期待 ACK).
 *
 * Payload Spam 模式专用 — 持续填 FIFO,芯片 CE 持续 HIGH 时自动消费 → 持续辐射
 * 调制信号(对 OFDM WiFi 干扰更有效,huuck WiFi 模式同).
 */
void pq_nrf24_load_tx_payload_noack(PqNrf24* dev, const uint8_t* data, uint8_t len);

/**
 * 发 FLUSH_TX 单字节命令 — 清空 TX FIFO.spam 模式 setup 前调用避免残留 payload
 * 影响首次发射的频率/内容.
 */
void pq_nrf24_flush_tx(PqNrf24* dev);

/**
 * 配置成 Payload Spam TX 模式(huuck WiFi 干扰风格).与 CW 区别:
 *   - RF_SETUP 不置 CONT_WAVE/PLL_LOCK(用普通 2 Mbps + 0 dBm)
 *   - Worker 持续 W_TX_PAYLOAD_NOACK 填 FIFO + 切 RF_CH 在 ±11 MHz 范围扫
 *
 * 调用后:
 *   - 上层应 `pq_chip_nrf24_ce_set(true)` 启动连续 TX
 *   - 后续 worker 每 chunk 在 channel list 里循环写 RF_CH + 重新 load payload
 *
 * @return  与 setup_cw 相同语义.
 */
bool pq_nrf24_setup_payload_spam(PqNrf24* dev, uint8_t ch);

/**
 * 配置成 RPD 反应式监听模式(v0.4.x — Flipper Zero NRF24 平台首创).
 *
 * 工作流程:
 *   1. setup_reactive(ch) — RX 模式 + 预填 TX FIFO 32B 0xFF
 *   2. CE high → 130 µs → 进 RX 监听
 *   3. 轮询 read_rpd():每 ~50 µs 一次
 *   4. RPD=1 检测到载波(目标 BLE 设备开始发包):
 *      a. CE low
 *      b. react_to_tx() 切 CW 模式(写 RF_SETUP + CONFIG)
 *      c. CE high → 130 µs → 进 TX(PA 锁定 +20 dBm)
 *      d. 持续 ~300 µs CW 干扰脉冲(刚好压到 BLE adv 包中段)
 *      e. CE low
 *      f. react_to_rx(ch) 切回 RX 监听
 *   5. RPD=0 → 50 µs 等下一次轮询
 *
 * 反应延迟分析(NRF24L01+ datasheet §6.1.7 + §6.4):
 *   - 检测延迟:RPD 40 µs(从 carrier 出现到位置位)+ 50 µs 轮询周期 = ~90 µs
 *   - 反应延迟:CE 切换 + 寄存器写 + Tstby2a = ~150 µs
 *   - 总反应:~240 µs(从目标包开始到我们 TX)
 *   - BLE adv 包长 80-328 µs(payload 0-31 字节),典 150-256 µs
 *   - 中段命中率高,即使本次包错过,3 ch 跳频中下一信道大概率截杀
 *
 * 等效干扰增益:盲扫 4% 命中率 → 反应式 ~70-100% 命中率 = +12-14 dB.
 *
 * MUST 在 pq_chip_with_nrf24 callback 内调用.
 */
bool pq_nrf24_setup_reactive(PqNrf24* dev, uint8_t ch);

/**
 * RX → TX/CW 快速切换(setup_reactive 之后用).调用前 CE=LOW.
 *   - RF_SETUP = CONT_WAVE | PLL_LOCK | RF_PWR=11(同 setup_cw)
 *   - CONFIG: 清 PRIM_RX 位(进 TX),保留 PWR_UP/IRQ mask
 *   - TX FIFO 已在 setup_reactive 阶段预填,无需重新 load
 *
 * 调用后:上层 CE=HIGH + 等 130 µs Tstby2a 即开始 +20 dBm CW 输出.
 */
void pq_nrf24_react_to_tx(PqNrf24* dev);

/**
 * TX/CW → RX 快速切换 + 换信道.调用前 CE=LOW.
 *   - RF_SETUP = 普通 2 Mbps 0 dBm(关 CW)
 *   - CONFIG: 置 PRIM_RX 位(进 RX),保留 PWR_UP/IRQ mask
 *   - RF_CH 写入新信道(支持 BLE 3 adv 信道间跳频)
 *
 * 调用后:上层 CE=HIGH + 等 130 µs Tstby2a 重新进入 RX 监听.
 */
void pq_nrf24_react_to_rx(PqNrf24* dev, uint8_t ch);

/**
 * 退出 TX 状态:CE 拉低之后调 — CONFIG = 0x08(PWR_DOWN,IRQ unmasked).
 *
 * 注意(规范 §17.2 反模式 26):写 PWR_DOWN 不会让芯片真断电(VCC 仍上),
 * 但能停止载波.scene_on_exit 必须先 `pq_chip_nrf24_ce_set(false)` 再调本函数.
 */
void pq_nrf24_power_down(PqNrf24* dev);

/**
 * 诊断:回读 CONFIG / RF_SETUP / RF_CH / STATUS / FIFO_STATUS,FURI_LOG_I 输出.
 *
 * setup_cw / setup_payload_spam 后调一次 → 用 `ufbt cli` 接串口看实际寄存器值,
 * 验证 SPI 写入真正生效(预期值见 setup_cw / setup_payload_spam 注释).
 *
 * 必须在 pq_chip_with_nrf24 callback 内调用.
 */
void pq_nrf24_diag_log(PqNrf24* dev, const char* tag);
