/**
 * @file pq_cc1101.c
 * @brief CC1101 极简驱动实现.
 *
 * CC1101 SPI header byte 格式(数据手册 §10.1 Table 35):
 *   bit 7 = R/W (0=write, 1=read)
 *   bit 6 = B   (0=single, 1=burst)
 *   bit 5-0 = address
 *
 * Strobe 命令在地址 0x30-0x3D 范围,header byte = address(R/W=0, B=0).
 * 状态寄存器同样在 0x30-0x3D,但 header byte 必须把 B=1 设上才会被解释为
 * 读寄存器而不是 strobe.
 */
#include "pq_cc1101.h"

#include "pq_chip_arbiter.h"
#include "pq_config.h"

#include <furi.h>
#include <stdlib.h>

#define TAG "PqCc1101"

#define PQ_CC1101_SPI_TIMEOUT_MS 50

/* CC1101 命令字节(数据手册 §10.4 Table 38 Strobes; §29 Configuration Registers). */
#define CC1101_STROBE_SIDLE 0x36 /* Force chip to IDLE */
#define CC1101_STROBE_SPWD  0x39 /* Force chip to SLEEP — crystal off */
#define CC1101_REG_IOCFG0   0x02 /* GDO0 output config(写命令 byte = 0x02) */
#define CC1101_IOCFG_HIGHZ  0x2E /* IOCFG.GDO_CFG = 0x2E → 高阻 */

struct PqCc1101 {
    const PqBoardConfig* cfg; /* 当前未使用,占位备用. */
};

PqCc1101* pq_cc1101_alloc(const PqBoardConfig* cfg) {
    PqCc1101* dev = malloc(sizeof(*dev));
    furi_check(dev);
    dev->cfg = cfg;
    return dev;
}

void pq_cc1101_free(PqCc1101* dev) {
    if(dev == NULL) return;
    free(dev);
}

bool pq_cc1101_quiet_gd0(PqCc1101* dev) {
    furi_check(dev);

    /* SIDLE strobe — 单字节命令,确保 CC1101 不在 RX/TX/CALIBRATE 状态. */
    uint8_t cmd = CC1101_STROBE_SIDLE;
    pq_chip_spi_trx(&cmd, NULL, 1, PQ_CC1101_SPI_TIMEOUT_MS);

    /* 写 IOCFG0 = 0x2E.两字节:[addr | flags=0x00, value]. */
    uint8_t tx[2] = {CC1101_REG_IOCFG0, CC1101_IOCFG_HIGHZ};
    pq_chip_spi_trx(tx, NULL, 2, PQ_CC1101_SPI_TIMEOUT_MS);

    return true;
}

bool pq_cc1101_sleep(PqCc1101* dev) {
    furi_check(dev);
    /* SPWD strobe(0x39):IDLE → SLEEP,关 crystal.必须先 SIDLE 回 IDLE 再 SPWD,
     * 否则在 RX/TX 状态下 SPWD 行为未定义(CC1101 数据手册 §6.5). */
    uint8_t cmd = CC1101_STROBE_SIDLE;
    pq_chip_spi_trx(&cmd, NULL, 1, PQ_CC1101_SPI_TIMEOUT_MS);
    cmd = CC1101_STROBE_SPWD;
    pq_chip_spi_trx(&cmd, NULL, 1, PQ_CC1101_SPI_TIMEOUT_MS);
    return true;
}
