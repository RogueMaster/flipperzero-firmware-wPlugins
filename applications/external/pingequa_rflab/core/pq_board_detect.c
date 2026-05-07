/**
 * @file pq_board_detect.c
 * @brief 板卡探测实现.规范 §5.1.
 *
 * 探测顺序:CC1101 先(并在同一 callback 内做 quiet_gd0,释放 PB2),NRF24 后.
 * 这样 NRF24 owner 切换时 PB2 不会与 CC1101 默认 GD0 配置打架(规范 F2).
 */
#include "pq_board_detect.h"

#include "pq_nrf24_regs.h"
#include "pq_cc1101.h"
#include "pq_chip_arbiter.h"
#include "pq_nrf24.h"

#include <furi.h>
#include <stdio.h>
#include <string.h>

#define TAG "PqBoardDetect"

/* 单 chip 临界区超时.callback 内 SPI 命令很快(~µs),100 ms 是宽松上限. */
#define PQ_DETECT_WITH_TIMEOUT_MS 100

/* CC1101 状态寄存器读取 header byte:R/W=1 + B=1 + addr.数据手册 §10.1.
 * 0x30 PARTNUM,0x31 VERSION. */
#define CC1101_HDR_RD_STATUS(addr) ((uint8_t)(0xC0 | ((addr)&0x3F)))
#define CC1101_REG_PARTNUM  0x30
#define CC1101_REG_VERSION  0x31

/* 期望值(数据手册 §29 Configuration Registers + 已知 silicon 版本). */
#define CC1101_PARTNUM_EXPECTED 0x00
#define CC1101_VERSION_OLD      0x14
#define CC1101_VERSION_NEW      0x04

/* ---------------------------------------------------------------------------
 * CC1101 探测 + quiet_gd0(同 callback 内合并)
 * --------------------------------------------------------------------------*/

typedef struct {
    PqCc1101* dev;
    uint8_t partnum;
    uint8_t version;
    bool present;
} Cc1101Ctx;

static bool cc1101_probe_and_quiet_cb(void* ctx) {
    Cc1101Ctx* c = ctx;

    /* 读 PARTNUM(2 字节事务:[header, dummy] → [status, value]). */
    uint8_t tx[2] = {CC1101_HDR_RD_STATUS(CC1101_REG_PARTNUM), 0xFF};
    uint8_t rx[2] = {0};
    pq_chip_spi_trx(tx, rx, 2, PQ_DETECT_WITH_TIMEOUT_MS);
    c->partnum = rx[1];

    /* 读 VERSION. */
    tx[0] = CC1101_HDR_RD_STATUS(CC1101_REG_VERSION);
    pq_chip_spi_trx(tx, rx, 2, PQ_DETECT_WITH_TIMEOUT_MS);
    c->version = rx[1];

    c->present =
        (c->partnum == CC1101_PARTNUM_EXPECTED) &&
        (c->version == CC1101_VERSION_OLD || c->version == CC1101_VERSION_NEW);

    /* 探测通过就立刻让 GD0 进高阻,后续 NRF24 owner 切换才不会打架. */
    if(c->present) {
        pq_cc1101_quiet_gd0(c->dev);
    }
    return true;
}

/* ---------------------------------------------------------------------------
 * NRF24 探测
 * --------------------------------------------------------------------------*/

typedef struct {
    PqNrf24* dev;
    uint8_t setup_aw;
    uint8_t status;
    uint8_t rf_ch;     /* 默认 0x02 */
    uint8_t rf_setup;  /* 默认 0x0F */
    uint8_t config;    /* 默认 0x08 */
    bool present;
} Nrf24Ctx;

static bool nrf24_probe_cb(void* ctx) {
    Nrf24Ctx* c = ctx;

    /* 先读默认值寄存器(PoR 后 NRF24 应有这些非零默认值). */
    c->rf_ch = pq_nrf24_read_reg(c->dev, NRF24_REG_RF_CH);       /* 默认 0x02 */
    c->rf_setup = pq_nrf24_read_reg(c->dev, NRF24_REG_RF_SETUP); /* 默认 0x0F */
    c->config = pq_nrf24_read_reg(c->dev, NRF24_REG_CONFIG);     /* 默认 0x08 */

    /* 写 SETUP_AW = 0x03,读回. */
    pq_nrf24_write_reg(c->dev, NRF24_REG_SETUP_AW, NRF24_SETUP_AW_5BYTE);
    c->setup_aw = pq_nrf24_read_reg(c->dev, NRF24_REG_SETUP_AW);

    c->status = pq_nrf24_read_reg(c->dev, NRF24_REG_STATUS);

    bool aw_ok = (c->setup_aw == NRF24_SETUP_AW_5BYTE);
    bool status_ok = (c->status != 0x00) && (c->status != 0xFF);
    bool not_cc1101 = ((c->status & 0xF0) != 0x80);

    c->present = aw_ok && status_ok && not_cc1101;
    return true;
}

/* ---------------------------------------------------------------------------
 * 公开 API
 * --------------------------------------------------------------------------*/

PqDetectResult pq_board_detect(void) {
    PqDetectResult r = {0}; /* zero-init: type=NOT_FOUND, both absent, hint="" */
    uint32_t t0 = furi_get_tick();

    /* --- CC1101 --- */
    PqCc1101* cc = pq_cc1101_alloc(NULL);
    Cc1101Ctx cc_ctx = {.dev = cc};
    bool cc_callback_ok =
        pq_chip_with_cc1101(cc1101_probe_and_quiet_cb, &cc_ctx, PQ_DETECT_WITH_TIMEOUT_MS);
    r.cc1101_present = cc_callback_ok && cc_ctx.present;
    pq_cc1101_free(cc);

    /* --- NRF24 --- */
    PqNrf24* nrf = pq_nrf24_alloc(NULL);
    Nrf24Ctx nrf_ctx = {.dev = nrf};
    bool nrf_callback_ok =
        pq_chip_with_nrf24(nrf24_probe_cb, &nrf_ctx, PQ_DETECT_WITH_TIMEOUT_MS);
    r.nrf24_present = nrf_callback_ok && nrf_ctx.present;
    pq_nrf24_free(nrf);

    r.t_detect_duration_ms = furi_get_tick() - t0;

    if(r.nrf24_present && r.cc1101_present) {
        r.type = PQ_BOARD_PINGEQUA_2IN1;
        FURI_LOG_I(
            TAG,
            "Detect OK in %lu ms (CC1101 ver=0x%02X, NRF24 STATUS=0x%02X)",
            r.t_detect_duration_ms,
            cc_ctx.version,
            nrf_ctx.status);
    } else {
        r.type = PQ_BOARD_NOT_FOUND;
        /* 失败时把 NRF24 多个寄存器 dump 进 hint,用于现场诊断:
         *   - 全 0 → 芯片没响应(电源 / PC3 wiring)
         *   - 全 FF → MISO 浮空(handle 没启动)
         *   - RF=02 SET=0F CFG=08 → 寄存器对默认值,SPI 通,但 SETUP_AW 写不进 */
        if(r.cc1101_present && !r.nrf24_present) {
            snprintf(
                r.failure_hint,
                sizeof(r.failure_hint),
                "NRF24 FAIL\nRF=%02X SET=%02X CFG=%02X\nAW=%02X ST=%02X",
                nrf_ctx.rf_ch,
                nrf_ctx.rf_setup,
                nrf_ctx.config,
                nrf_ctx.setup_aw,
                nrf_ctx.status);
        } else {
            snprintf(
                r.failure_hint,
                sizeof(r.failure_hint),
                "CC1101 %s (PN=%02X V=%02X)\nNRF24 %s (AW=%02X ST=%02X)",
                r.cc1101_present ? "OK" : "FAIL",
                cc_ctx.partnum,
                cc_ctx.version,
                r.nrf24_present ? "OK" : "FAIL",
                nrf_ctx.setup_aw,
                nrf_ctx.status);
        }
        FURI_LOG_W(TAG, "Detect FAIL: %s", r.failure_hint);
    }

    return r;
}
