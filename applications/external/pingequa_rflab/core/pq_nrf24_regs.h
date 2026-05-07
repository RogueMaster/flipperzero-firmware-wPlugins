/**
 * @file nrf24_regs.h
 * @brief NRF24L01+ register and command map.
 *
 * Source: Nordic Semiconductor nRF24L01+ Product Specification v1.0, §6 / §8.
 */
#pragma once

#include <stdint.h>

/* SPI commands (§8.3 Table 19) */
#define NRF24_CMD_R_REGISTER       0x00 /* OR with 5-bit reg addr */
#define NRF24_CMD_W_REGISTER       0x20 /* OR with 5-bit reg addr */
#define NRF24_CMD_R_RX_PAYLOAD     0x61
#define NRF24_CMD_W_TX_PAYLOAD     0xA0
#define NRF24_CMD_W_TX_PAYLOAD_NOACK 0xB0 /* nRF24L01+: TX without expecting ACK,适合广谱 spam */
#define NRF24_CMD_FLUSH_TX         0xE1
#define NRF24_CMD_FLUSH_RX         0xE2
#define NRF24_CMD_REUSE_TX_PL      0xE3
#define NRF24_CMD_R_RX_PL_WID      0x60
#define NRF24_CMD_NOP              0xFF

#define NRF24_REG_MASK            0x1F

/* Register addresses (§6.4 Table 27) */
#define NRF24_REG_CONFIG          0x00
#define NRF24_REG_EN_AA           0x01
#define NRF24_REG_EN_RXADDR       0x02
#define NRF24_REG_SETUP_AW        0x03
#define NRF24_REG_SETUP_RETR      0x04
#define NRF24_REG_RF_CH           0x05
#define NRF24_REG_RF_SETUP        0x06
#define NRF24_REG_STATUS          0x07
#define NRF24_REG_OBSERVE_TX      0x08
#define NRF24_REG_RPD             0x09 /* Received Power Detector — bit 0 */
#define NRF24_REG_RX_ADDR_P0      0x0A
#define NRF24_REG_FIFO_STATUS     0x17
#define NRF24_REG_DYNPD           0x1C
#define NRF24_REG_FEATURE         0x1D

/* CONFIG bits (§6.4 Table 28) */
#define NRF24_CONFIG_PRIM_RX      (1U << 0)
#define NRF24_CONFIG_PWR_UP       (1U << 1)
#define NRF24_CONFIG_CRCO         (1U << 2)
#define NRF24_CONFIG_EN_CRC       (1U << 3)
#define NRF24_CONFIG_MASK_MAX_RT  (1U << 4)
#define NRF24_CONFIG_MASK_TX_DS   (1U << 5)
#define NRF24_CONFIG_MASK_RX_DR   (1U << 6)

/* STATUS bits */
#define NRF24_STATUS_TX_FULL      (1U << 0)
#define NRF24_STATUS_RX_P_NO_MASK 0x0E
#define NRF24_STATUS_MAX_RT       (1U << 4)
#define NRF24_STATUS_TX_DS        (1U << 5)
#define NRF24_STATUS_RX_DR        (1U << 6)
#define NRF24_STATUS_CLEAR_ALL    0x70 /* Write to clear MAX_RT/TX_DS/RX_DR */

/* RF_SETUP bits (§6.4 Table 28).  Layout: [CONT_WAVE | rsvd | RF_DR_LOW |
 * PLL_LOCK | RF_DR_HIGH | RF_PWR[1] | RF_PWR[0] | rsvd]. */
#define NRF24_RF_SETUP_CONT_WAVE  (1U << 7) /* Constant carrier output (test) */
#define NRF24_RF_SETUP_RF_DR_LOW  (1U << 5) /* 1 = 250 kbps; overrides DR_HIGH */
#define NRF24_RF_SETUP_PLL_LOCK   (1U << 4) /* Force PLL lock signal (test)   */
#define NRF24_RF_SETUP_RF_DR_HIGH (1U << 3) /* 1 = 2 Mbps, 0 = 1 Mbps         */
#define NRF24_RF_SETUP_RF_PWR_MASK 0x06     /* Bits 2:1                       */
#define NRF24_RF_SETUP_RF_PWR_0DBM 0x06     /* RF_PWR = 11 → 0 dBm at die     */

/* RF_SETUP — 2 Mbps + 0 dBm = 0x0E (RF_DR_HIGH=1, RF_PWR=11) */
#define NRF24_RF_SETUP_2M_0DBM    0x0E

/* RF_SETUP — Constant Wave + max power (Nordic AN: PLL_LOCK | CONT_WAVE |
 * RF_PWR=11).  RF_DR_HIGH=0 → narrowest bandwidth → highest peak. */
#define NRF24_RF_SETUP_CW_0DBM \
    (NRF24_RF_SETUP_CONT_WAVE | NRF24_RF_SETUP_PLL_LOCK | NRF24_RF_SETUP_RF_PWR_0DBM)

/* SETUP_AW default = 0x03 (5-byte addresses) */
#define NRF24_SETUP_AW_5BYTE      0x03

/* Channel range */
#define NRF24_CHANNEL_MIN         0
#define NRF24_CHANNEL_MAX         125
#define NRF24_CHANNEL_COUNT       126

/* Timing constants (§6.1.7 Table 16, in microseconds) */
#define NRF24_TPOR_MS             100  /* Power on reset */
#define NRF24_TPD2STBY_MS         2    /* PWR_DOWN -> Standby (worst case 4.5ms but typ 1.5ms; round up) */
#define NRF24_TSTBY2A_US          130  /* Standby -> RX active */
