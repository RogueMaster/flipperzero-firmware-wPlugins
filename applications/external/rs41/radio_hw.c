/*
 * radio_hw.c — CC1101 hardware abstraction for RS41 Tracker
 *
 * Internal backend : furi_hal_subghz_* + direct SPI on furi_hal_spi_bus_handle_subghz
 * External backend : direct SPI on furi_hal_spi_bus_handle_external (PA4 CS)
 *
 * CC1101 SPI protocol (all transactions wrapped in furi_hal_spi_acquire/release):
 *   Write register : CS↓ [addr & 0x3F, val] CS↑       (bit7=0 write, bit6=0 single)
 *   Read status    : CS↓ [addr | 0xC0, 0x00] CS↑ → rx[1]=value
 *   Strobe         : CS↓ [strobe_byte] CS↑
 *   Burst read FIFO: CS↓ [0xFF] then N × rx CS↑
 *
 * RSSI conversion (CC1101 datasheet, RSSI_offset ≈ 74):
 *   if raw >= 128 → dBm = (raw − 256) / 2 − 74
 *   else          → dBm =  raw        / 2 − 74
 */

#include "radio_hw.h"
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_spi.h>
#include <furi_hal_spi_config.h>
#include <furi_hal_subghz.h>

/* ── CC1101 command bytes ────────────────────────────────────────────────── */
#define CC1101_SRES   0x30u  /* software reset                   */
#define CC1101_SIDLE  0x36u  /* go to IDLE                       */
#define CC1101_SRX    0x34u  /* start RX                         */
#define CC1101_SFRX   0x3Au  /* flush RX FIFO                    */
#define CC1101_SPWD   0x39u  /* power down (SLEEP state)         */

/* CC1101 register addresses */
#define CC1101_REG_FREQ2  0x0Du
#define CC1101_REG_FREQ1  0x0Eu
#define CC1101_REG_FREQ0  0x0Fu

/* Status register read addresses (R=1 bit7, BURST=1 bit6) */
#define CC1101_ST_RXBYTES  0xFBu  /* 0x3B | 0xC0 */
#define CC1101_ST_RSSI     0xF4u  /* 0x34 | 0xC0 */
#define CC1101_FIFO_RD     0xFFu  /* 0x3F | 0xC0  — burst-read RX FIFO */

/* CC1101 crystal frequency for external module (Hz) */
#define CC1101_XOSC_HZ  26000000ULL

/* ── External CC1101 low-level helpers ───────────────────────────────────── */

static void ext_strobe(uint8_t s) {
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_external);
    furi_hal_spi_bus_trx(&furi_hal_spi_bus_handle_external, &s, NULL, 1, 50);
    furi_hal_spi_release(&furi_hal_spi_bus_handle_external);
}

static void ext_write_reg(uint8_t addr, uint8_t val) {
    /* bit7=0 (write), bit6=0 (single byte) */
    uint8_t buf[2] = {(uint8_t)(addr & 0x3Fu), val};
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_external);
    furi_hal_spi_bus_trx(&furi_hal_spi_bus_handle_external, buf, NULL, 2, 50);
    furi_hal_spi_release(&furi_hal_spi_bus_handle_external);
}

/* Read status/config register; addr must already include 0xC0 (R+BURST). */
static uint8_t ext_read_status(uint8_t addr) {
    uint8_t tx[2] = {addr, 0x00u};
    uint8_t rx[2] = {0u, 0u};
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_external);
    furi_hal_spi_bus_trx(&furi_hal_spi_bus_handle_external, tx, rx, 2, 50);
    furi_hal_spi_release(&furi_hal_spi_bus_handle_external);
    return rx[1];
}

/* ── Public API implementation ───────────────────────────────────────────── */

void radio_hw_init(RadioSource src, const uint8_t* preset) {
    if(src == RadioInternal) {
        furi_hal_subghz_reset();
        furi_hal_subghz_flush_rx();
        furi_hal_subghz_load_custom_preset(preset);
    } else {
        /* Reset external CC1101 */
        ext_strobe(CC1101_SRES);
        furi_delay_ms(2);

        /* Write {reg, val} pairs until terminator {0x00, 0x00} */
        for(size_t i = 0; ; i += 2) {
            uint8_t reg = preset[i];
            uint8_t val = preset[i + 1u];
            if(reg == 0x00u && val == 0x00u) {
                /* PA table: 8 bytes follow the terminator.
                 * Burst write to PATABLE (addr 0x3E | 0x40 burst = 0x7E). */
                uint8_t cmd = 0x7Eu;
                furi_hal_spi_acquire(&furi_hal_spi_bus_handle_external);
                furi_hal_spi_bus_trx(&furi_hal_spi_bus_handle_external, &cmd, NULL, 1, 50);
                furi_hal_spi_bus_tx(
                    &furi_hal_spi_bus_handle_external,
                    (const uint8_t*)&preset[i + 2u], 8, 50);
                furi_hal_spi_release(&furi_hal_spi_bus_handle_external);
                break;
            }
            ext_write_reg(reg, val);
        }
    }
}

void radio_hw_set_frequency(RadioSource src, uint32_t freq_hz) {
    if(src == RadioInternal) {
        furi_hal_subghz_set_frequency_and_path(freq_hz);
    } else {
        /* FREQ_REG = freq_hz * 2^16 / f_xosc (26 MHz) */
        uint32_t freq_reg = (uint32_t)(((uint64_t)freq_hz << 16) / CC1101_XOSC_HZ);
        ext_write_reg(CC1101_REG_FREQ2, (uint8_t)((freq_reg >> 16) & 0xFFu));
        ext_write_reg(CC1101_REG_FREQ1, (uint8_t)((freq_reg >>  8) & 0xFFu));
        ext_write_reg(CC1101_REG_FREQ0, (uint8_t)( freq_reg        & 0xFFu));
    }
}

void radio_hw_rx(RadioSource src) {
    if(src == RadioInternal) {
        furi_hal_subghz_rx();
    } else {
        ext_strobe(CC1101_SRX);
    }
}

void radio_hw_idle(RadioSource src) {
    if(src == RadioInternal) {
        furi_hal_subghz_idle();
    } else {
        ext_strobe(CC1101_SIDLE);
        furi_delay_ms(1);
    }
}

void radio_hw_sleep(RadioSource src) {
    if(src == RadioInternal) {
        furi_hal_subghz_idle();
        furi_hal_subghz_sleep();
    } else {
        ext_strobe(CC1101_SIDLE);
        furi_delay_ms(1);
        ext_strobe(CC1101_SPWD);
    }
}

void radio_hw_flush_rx(RadioSource src) {
    if(src == RadioInternal) {
        furi_hal_subghz_flush_rx();
    } else {
        ext_strobe(CC1101_SFRX);
    }
}

uint8_t radio_hw_rxbytes(RadioSource src) {
    const FuriHalSpiBusHandle* h = (src == RadioInternal)
                                   ? &furi_hal_spi_bus_handle_subghz
                                   : &furi_hal_spi_bus_handle_external;
    uint8_t tx[2] = {CC1101_ST_RXBYTES, 0x00u};
    uint8_t rx[2] = {0u, 0u};
    furi_hal_spi_acquire(h);
    furi_hal_spi_bus_trx(h, tx, rx, 2, 50);
    furi_hal_spi_release(h);
    return rx[1];
}

float radio_hw_get_rssi(RadioSource src) {
    if(src == RadioInternal) {
        return furi_hal_subghz_get_rssi();
    }
    uint8_t raw      = ext_read_status(CC1101_ST_RSSI);
    int     rssi_dec = (raw >= 128u) ? (int)raw - 256 : (int)raw;
    return (float)rssi_dec / 2.0f - 74.0f;
}

void radio_hw_read_fifo(RadioSource src, uint8_t* dst, uint8_t n) {
    if(n == 0) return;
    const FuriHalSpiBusHandle* h = (src == RadioInternal)
                                   ? &furi_hal_spi_bus_handle_subghz
                                   : &furi_hal_spi_bus_handle_external;
    uint8_t cmd    = CC1101_FIFO_RD;
    uint8_t status = 0u;
    furi_hal_spi_acquire(h);
    furi_hal_spi_bus_trx(h, &cmd, &status, 1, 50);
    furi_hal_spi_bus_rx(h, dst, n, 500);
    furi_hal_spi_release(h);
}
