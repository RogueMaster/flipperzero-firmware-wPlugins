/**
 * @file pq_nrf24.c
 * @brief NRF24L01+ 寄存器层实现.规范 §5.4.
 *
 * 全部 SPI 字节走 pq_chip_spi_trx() — 由 pq_chip_arbiter 在 with_nrf24
 * callback 内统一管 mutex / CSN / PB2.本文件零 GPIO / 零 SPI HAL 直驱.
 *
 * 寄存器与命令常量见 core/nrf24_regs.h(Nordic nRF24L01+ 数据手册 v1.0
 * §6 / §8 翻译).
 */
#include "pq_nrf24.h"

#include "pq_nrf24_regs.h"
#include "pq_chip_arbiter.h"
#include "pq_config.h"

#include <furi.h>
#include <stdlib.h>
#include <string.h>

#define TAG "PqNrf24"

/* SPI 单次事务超时(ms).pq_chip_spi_trx 当前未实现 timeout(与上游
 * furi_hal_spi.c:81 同),保留参数以备 DMA 模式. */
#define PQ_NRF24_SPI_TIMEOUT_MS 50

/* ---------------------------------------------------------------------------
 * 结构体
 * --------------------------------------------------------------------------*/

struct PqNrf24 {
    const PqBoardConfig* cfg; /* 仅保存引用,不拷贝;当前 SPI 事务不读 cfg
                               * 字段(芯片仲裁器已封装 pin 选择),保留指
                               * 针为将来 v1.2 Jammer 等扩展做准备. */
};

/* ---------------------------------------------------------------------------
 * 生命周期
 * --------------------------------------------------------------------------*/

PqNrf24* pq_nrf24_alloc(const PqBoardConfig* cfg) {
    PqNrf24* dev = malloc(sizeof(*dev));
    furi_check(dev);
    dev->cfg = cfg;
    return dev;
}

void pq_nrf24_free(PqNrf24* dev) {
    if(dev == NULL) return;
    free(dev);
}

/* ---------------------------------------------------------------------------
 * 寄存器层
 * --------------------------------------------------------------------------*/

uint8_t pq_nrf24_read_reg(PqNrf24* dev, uint8_t reg) {
    furi_check(dev);
    /* R_REGISTER 是 OR 在低 5 位的命令字节.第 1 字节 = STATUS(我们丢弃),
     * 第 2 字节 = 寄存器内容. */
    uint8_t tx[2] = {(uint8_t)(NRF24_CMD_R_REGISTER | (reg & NRF24_REG_MASK)), NRF24_CMD_NOP};
    uint8_t rx[2] = {0};
    pq_chip_spi_trx(tx, rx, 2, PQ_NRF24_SPI_TIMEOUT_MS);
    return rx[1];
}

void pq_nrf24_write_reg(PqNrf24* dev, uint8_t reg, uint8_t val) {
    furi_check(dev);
    uint8_t tx[2] = {(uint8_t)(NRF24_CMD_W_REGISTER | (reg & NRF24_REG_MASK)), val};
    pq_chip_spi_trx(tx, NULL, 2, PQ_NRF24_SPI_TIMEOUT_MS);
}

void pq_nrf24_set_channel(PqNrf24* dev, uint8_t ch) {
    if(ch > NRF24_CHANNEL_MAX) ch = NRF24_CHANNEL_MAX;
    pq_nrf24_write_reg(dev, NRF24_REG_RF_CH, ch);
}

bool pq_nrf24_read_rpd(PqNrf24* dev) {
    /* RPD bit 0 = 1 → 检测到 > -64 dBm(数据手册 §6.4 Table 27). */
    return (pq_nrf24_read_reg(dev, NRF24_REG_RPD) & 0x01) != 0;
}

/* ---------------------------------------------------------------------------
 * 高层:Channel Scan 基线初始化
 * --------------------------------------------------------------------------*/

bool pq_nrf24_init_regs(PqNrf24* dev) {
    furi_check(dev);

    /* CONFIG = 0:PWR_DOWN,IRQ unmasked,CRC off.调用方稍后在 sweep 循环
     * 内再写入 PRIM_RX|PWR_UP|MASK_*. */
    pq_nrf24_write_reg(dev, NRF24_REG_CONFIG, 0x00);

    /* EN_AA = 0:禁用所有 pipe 的 auto-ack(仅 RX 载波检测,无 PRX 协议层). */
    pq_nrf24_write_reg(dev, NRF24_REG_EN_AA, 0x00);

    /* SETUP_RETR = 0:不重传(我们也不发 TX). */
    pq_nrf24_write_reg(dev, NRF24_REG_SETUP_RETR, 0x00);

    /* RF_SETUP = 0x0E:2 Mbps + 0 dBm.带宽最大 → RPD 命中率最高. */
    pq_nrf24_write_reg(dev, NRF24_REG_RF_SETUP, NRF24_RF_SETUP_2M_0DBM);

    /* DYNPD / FEATURE = 0:不用动态长度. */
    pq_nrf24_write_reg(dev, NRF24_REG_DYNPD, 0x00);
    pq_nrf24_write_reg(dev, NRF24_REG_FEATURE, 0x00);

    /* STATUS:写 1 清 RX_DR / TX_DS / MAX_RT 三个 IRQ flag. */
    pq_nrf24_write_reg(dev, NRF24_REG_STATUS, NRF24_STATUS_CLEAR_ALL);

    /* SETUP_AW 写回验证 — 这是芯片"活着"的最权威信号:
     * 写入 0x03 后读回必须是 0x03,否则 SPI 链路或芯片死. */
    pq_nrf24_write_reg(dev, NRF24_REG_SETUP_AW, NRF24_SETUP_AW_5BYTE);
    uint8_t aw = pq_nrf24_read_reg(dev, NRF24_REG_SETUP_AW);
    if(aw != NRF24_SETUP_AW_5BYTE) {
        FURI_LOG_W(TAG, "SETUP_AW writeback failed: got 0x%02X want 0x%02X", aw,
                   NRF24_SETUP_AW_5BYTE);
        return false;
    }

    /* 清空 FIFO(单字节命令). */
    uint8_t cmd;
    cmd = NRF24_CMD_FLUSH_RX;
    pq_chip_spi_trx(&cmd, NULL, 1, PQ_NRF24_SPI_TIMEOUT_MS);
    cmd = NRF24_CMD_FLUSH_TX;
    pq_chip_spi_trx(&cmd, NULL, 1, PQ_NRF24_SPI_TIMEOUT_MS);

    return true;
}

/* ---------------------------------------------------------------------------
 * v1.2 Jammer 扩展
 * --------------------------------------------------------------------------*/

bool pq_nrf24_setup_cw(PqNrf24* dev, uint8_t ch) {
    furi_check(dev);
    if(ch > NRF24_CHANNEL_MAX) ch = NRF24_CHANNEL_MAX;

    /* 关协议层. */
    pq_nrf24_write_reg(dev, NRF24_REG_EN_AA, 0x00);
    pq_nrf24_write_reg(dev, NRF24_REG_EN_RXADDR, 0x00);
    pq_nrf24_write_reg(dev, NRF24_REG_SETUP_RETR, 0x00);

    pq_nrf24_write_reg(dev, NRF24_REG_RF_CH, ch);
    pq_nrf24_write_reg(dev, NRF24_REG_RF_SETUP, NRF24_RF_SETUP_CW_0DBM);

    /* CONFIG: PWR_UP=1, PRIM_RX=0, CRC 关. IRQ 全 mask 防止误触发 STATUS. */
    pq_nrf24_write_reg(
        dev,
        NRF24_REG_CONFIG,
        NRF24_CONFIG_PWR_UP | NRF24_CONFIG_MASK_MAX_RT | NRF24_CONFIG_MASK_TX_DS |
            NRF24_CONFIG_MASK_RX_DR);

    /* PWR_DOWN → Standby-I 过渡需要 Tpd2stby (datasheet §6.1.7 Table 16,
     * 典 1.5 ms,worst 4.5 ms,我们取整到 NRF24_TPD2STBY_MS = 2 ms). */
    furi_delay_ms(NRF24_TPD2STBY_MS);

    /* 关键步骤:写 32 字节 dummy 0xFF 进 TX FIFO,触发 nRF24L01+ CW 实际辐射.
     * 缺这一步只配 RF_SETUP 的 CONT_WAVE+PLL_LOCK 不会出射载波(nRF24L01+
     * 变种特有的 quirk;huuck nrf24_startConstCarrier 与 RF24 Arduino 库旧版
     * startConstCarrier 均同此做法).
     * payload 内容无关,只要 FIFO 非空,芯片就持续把 carrier 推出 PA. */
    uint8_t dummy[32];
    memset(dummy, 0xFF, sizeof(dummy));
    pq_nrf24_load_tx_payload(dev, dummy, 32);

    return true;
}

void pq_nrf24_set_channel_fast(PqNrf24* dev, uint8_t ch) {
    pq_nrf24_write_reg(dev, NRF24_REG_RF_CH, ch);
}

void pq_nrf24_load_tx_payload(PqNrf24* dev, const uint8_t* data, uint8_t len) {
    furi_check(dev);
    furi_check(len <= 32);
    /* W_TX_PAYLOAD 是单字节命令,后跟最多 32 字节数据(NRF24 数据手册 §8.3
     * Table 19).一次 SPI 事务发完(33 字节). */
    uint8_t tx[33];
    tx[0] = NRF24_CMD_W_TX_PAYLOAD;
    if(data != NULL && len > 0) {
        memcpy(&tx[1], data, len);
    }
    pq_chip_spi_trx(tx, NULL, (size_t)len + 1, PQ_NRF24_SPI_TIMEOUT_MS);
}

void pq_nrf24_load_tx_payload_noack(PqNrf24* dev, const uint8_t* data, uint8_t len) {
    furi_check(dev);
    furi_check(len <= 32);
    uint8_t tx[33];
    tx[0] = NRF24_CMD_W_TX_PAYLOAD_NOACK;
    if(data != NULL && len > 0) {
        memcpy(&tx[1], data, len);
    }
    pq_chip_spi_trx(tx, NULL, (size_t)len + 1, PQ_NRF24_SPI_TIMEOUT_MS);
}

void pq_nrf24_flush_tx(PqNrf24* dev) {
    furi_check(dev);
    uint8_t cmd = NRF24_CMD_FLUSH_TX;
    pq_chip_spi_trx(&cmd, NULL, 1, PQ_NRF24_SPI_TIMEOUT_MS);
}

bool pq_nrf24_setup_payload_spam(PqNrf24* dev, uint8_t ch) {
    furi_check(dev);
    if(ch > NRF24_CHANNEL_MAX) ch = NRF24_CHANNEL_MAX;

    /* 关协议层(无 ACK / 无重传 / 不开 RX). */
    pq_nrf24_write_reg(dev, NRF24_REG_EN_AA, 0x00);
    pq_nrf24_write_reg(dev, NRF24_REG_EN_RXADDR, 0x00);
    pq_nrf24_write_reg(dev, NRF24_REG_SETUP_RETR, 0x00);

    pq_nrf24_write_reg(dev, NRF24_REG_RF_CH, ch);

    /* 普通 2 Mbps + 0 dBm,**不**置 CONT_WAVE/PLL_LOCK(那俩是 CW 专用). */
    pq_nrf24_write_reg(dev, NRF24_REG_RF_SETUP, NRF24_RF_SETUP_2M_0DBM);

    /* CONFIG: PWR_UP=1, PRIM_RX=0(TX 模式), CRC 关, IRQ 全 mask 防 STATUS 误触发. */
    pq_nrf24_write_reg(
        dev,
        NRF24_REG_CONFIG,
        NRF24_CONFIG_PWR_UP | NRF24_CONFIG_MASK_MAX_RT | NRF24_CONFIG_MASK_TX_DS |
            NRF24_CONFIG_MASK_RX_DR);

    /* 清 TX FIFO(可能有 Scanner 模式残留). */
    pq_nrf24_flush_tx(dev);

    /* Tpd2stby. */
    furi_delay_ms(NRF24_TPD2STBY_MS);

    /* 预填一次 payload(让 CE HIGH 后立即有数据可发).
     * 32 字节 0xDEADBEEF × 8 — 模仿 huuck jammer.c WiFi 模式 ping_packet. */
    static const uint8_t spam_payload[32] = {
        0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE,
        0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD,
        0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF};
    pq_nrf24_load_tx_payload_noack(dev, spam_payload, sizeof(spam_payload));

    return true;
}

/* ---------------------------------------------------------------------------
 * v0.4.x:RPD 反应式干扰
 * --------------------------------------------------------------------------*/

bool pq_nrf24_setup_reactive(PqNrf24* dev, uint8_t ch) {
    furi_check(dev);
    if(ch > NRF24_CHANNEL_MAX) ch = NRF24_CHANNEL_MAX;

    /* 协议层关. */
    pq_nrf24_write_reg(dev, NRF24_REG_EN_AA, 0x00);
    pq_nrf24_write_reg(dev, NRF24_REG_EN_RXADDR, 0x01); /* 启用 pipe 0(RPD 工作不严格依赖) */
    pq_nrf24_write_reg(dev, NRF24_REG_SETUP_RETR, 0x00);

    pq_nrf24_write_reg(dev, NRF24_REG_RF_CH, ch);

    /* RX:普通 2 Mbps + 0 dBm,带宽 ±1 MHz 适合捕捉 BLE 1Mbps GFSK. */
    pq_nrf24_write_reg(dev, NRF24_REG_RF_SETUP, NRF24_RF_SETUP_2M_0DBM);

    /* CONFIG: PWR_UP=1, PRIM_RX=1(进 RX), CRC 关, IRQ 全 mask. */
    pq_nrf24_write_reg(
        dev,
        NRF24_REG_CONFIG,
        NRF24_CONFIG_PWR_UP | NRF24_CONFIG_PRIM_RX | NRF24_CONFIG_MASK_MAX_RT |
            NRF24_CONFIG_MASK_TX_DS | NRF24_CONFIG_MASK_RX_DR);

    /* Tpd2stby. */
    furi_delay_ms(NRF24_TPD2STBY_MS);

    /* 预填 TX FIFO — 32 字节 0xFF.等会反应切 TX 时,FIFO 已就位,
     * 不必再写 W_TX_PAYLOAD,反应时间从 ~150 µs 缩到 ~50 µs. */
    pq_nrf24_flush_tx(dev);
    uint8_t dummy[32];
    memset(dummy, 0xFF, sizeof(dummy));
    pq_nrf24_load_tx_payload(dev, dummy, sizeof(dummy));

    return true;
}

void pq_nrf24_react_to_tx(PqNrf24* dev) {
    furi_check(dev);
    /* 切 CW 模式 — 仅写 RF_SETUP + CONFIG 两个寄存器,~14 µs SPI. */
    pq_nrf24_write_reg(dev, NRF24_REG_RF_SETUP, NRF24_RF_SETUP_CW_0DBM);
    pq_nrf24_write_reg(
        dev,
        NRF24_REG_CONFIG,
        NRF24_CONFIG_PWR_UP | NRF24_CONFIG_MASK_MAX_RT | NRF24_CONFIG_MASK_TX_DS |
            NRF24_CONFIG_MASK_RX_DR);
    /* TX FIFO 在 setup_reactive 已预填,无需重写. */
}

void pq_nrf24_react_to_rx(PqNrf24* dev, uint8_t ch) {
    furi_check(dev);
    if(ch > NRF24_CHANNEL_MAX) ch = NRF24_CHANNEL_MAX;
    /* 切回 RX — 普通 RF_SETUP + CONFIG 加 PRIM_RX. */
    pq_nrf24_write_reg(dev, NRF24_REG_RF_SETUP, NRF24_RF_SETUP_2M_0DBM);
    pq_nrf24_write_reg(dev, NRF24_REG_RF_CH, ch);
    pq_nrf24_write_reg(
        dev,
        NRF24_REG_CONFIG,
        NRF24_CONFIG_PWR_UP | NRF24_CONFIG_PRIM_RX | NRF24_CONFIG_MASK_MAX_RT |
            NRF24_CONFIG_MASK_TX_DS | NRF24_CONFIG_MASK_RX_DR);
}

void pq_nrf24_diag_log(PqNrf24* dev, const char* tag) {
    furi_check(dev);
    uint8_t cfg = pq_nrf24_read_reg(dev, NRF24_REG_CONFIG);
    uint8_t rf_setup = pq_nrf24_read_reg(dev, NRF24_REG_RF_SETUP);
    uint8_t rf_ch = pq_nrf24_read_reg(dev, NRF24_REG_RF_CH);
    uint8_t status = pq_nrf24_read_reg(dev, NRF24_REG_STATUS);
    uint8_t fifo = pq_nrf24_read_reg(dev, NRF24_REG_FIFO_STATUS);
    uint8_t en_aa = pq_nrf24_read_reg(dev, NRF24_REG_EN_AA);

    /* 预期 CW: CFG=0x72 (PWR_UP|MASK_ALL_IRQ, EN_CRC=0, PRIM_RX=0)
     *           RF_SETUP=0x96 (CONT_WAVE|PLL_LOCK|RF_PWR=11)
     *           EN_AA=0
     *           FIFO_STATUS bit 5 (TX_FULL) 写 W_TX_PAYLOAD 后置 1 */
    FURI_LOG_I(TAG,
               "%s diag: CFG=0x%02X RF_SETUP=0x%02X RF_CH=%u "
               "STATUS=0x%02X FIFO=0x%02X EN_AA=0x%02X",
               tag, cfg, rf_setup, rf_ch, status, fifo, en_aa);
}

void pq_nrf24_power_down(PqNrf24* dev) {
    furi_check(dev);
    /* CONFIG bit PWR_UP = 0,其他 IRQ 仍 mask 防触发. */
    pq_nrf24_write_reg(
        dev,
        NRF24_REG_CONFIG,
        NRF24_CONFIG_MASK_MAX_RT | NRF24_CONFIG_MASK_TX_DS | NRF24_CONFIG_MASK_RX_DR);
}
