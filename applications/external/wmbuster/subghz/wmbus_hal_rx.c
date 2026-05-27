/*
 * Real implementation of `wmbus_hal_rx_drain` for mainline Flipper firmware.
 *
 * The official `furi_hal_subghz_*` API does not expose continuous chip-level
 * RX. We bypass it by talking to the CC1101 directly over the SubGHz SPI
 * bus that is already wired up by `furi_hal_subghz_rx()`.
 *
 * Sequence per call:
 *   1. Acquire the SubGHz SPI bus.
 *   2. Read the CC1101_STATUS_RXBYTES register twice (datasheet erratum:
 *      retry until two consecutive reads agree, ignoring overflow flag).
 *   3. If >0 bytes are available, burst-read them from the FIFO.
 *   4. If the chip overflowed, flush and re-arm RX so the slicer doesn't
 *      desync on garbage.
 *   5. Release the bus.
 *
 * This file deliberately overrides the weak default in wmbus_worker.c.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include <furi_hal.h>
#include <furi_hal_spi.h>
#include <furi_hal_subghz.h>
#include <lib/drivers/cc1101_regs.h>

static bool read_rxbytes_stable(const FuriHalSpiBusHandle* h, uint8_t* out, bool* overflow) {
    /* Erratum: read STATUS_RXBYTES twice; only trust if equal. */
    const uint8_t addr = (uint8_t)(CC1101_READ | CC1101_BURST | CC1101_STATUS_RXBYTES);
    uint8_t prev = 0xFF;
    for(int i = 0; i < 4; i++) {
        uint8_t tx[2] = { addr, 0 };
        uint8_t rx[2] = { 0, 0 };
        if(!furi_hal_spi_bus_trx(h, tx, rx, sizeof(tx), 100)) return false;
        if(i > 0 && rx[1] == prev) {
            *out = (uint8_t)(rx[1] & 0x7F);
            *overflow = (rx[1] & 0x80) != 0;
            return true;
        }
        prev = rx[1];
    }
    *out = (uint8_t)(prev & 0x7F);
    *overflow = (prev & 0x80) != 0;
    return true;
}

size_t wmbus_hal_rx_drain(uint8_t* dst, size_t cap, bool external) {
    if(!dst || cap == 0) return 0;
    const FuriHalSpiBusHandle* h = external
                                       ? &furi_hal_spi_bus_handle_external
                                       : &furi_hal_spi_bus_handle_subghz;

    furi_hal_spi_acquire(h);
    uint8_t avail = 0;
    bool overflow = false;
    if(!read_rxbytes_stable(h, &avail, &overflow)) {
        furi_hal_spi_release(h);
        return 0;
    }

    if(overflow) {
        /* Recover: idle, flush, restart RX. The chip-level worker treats
         * a chip-buffer reset on long idle as "lost frame", so we don't
         * need to signal the caller explicitly. */
        uint8_t sidle = (uint8_t)CC1101_STROBE_SIDLE;
        furi_hal_spi_bus_tx(h, &sidle, 1, 100);
        uint8_t sfrx  = (uint8_t)CC1101_STROBE_SFRX;
        furi_hal_spi_bus_tx(h, &sfrx, 1, 100);
        uint8_t srx   = (uint8_t)CC1101_STROBE_SRX;
        furi_hal_spi_bus_tx(h, &srx, 1, 100);
        furi_hal_spi_release(h);
        return 0;
    }

    if(avail == 0) {
        furi_hal_spi_release(h);
        return 0;
    }

    /* Datasheet recommends leaving 1 byte unread when streaming to avoid an
     * old-silicon RX-FIFO bug (single-byte reads can return stale data). */
    if(avail > 1) avail -= 1;
    size_t n = avail < cap ? avail : cap;

    uint8_t addr = (uint8_t)(CC1101_READ | CC1101_BURST | CC1101_FIFO);
    if(!furi_hal_spi_bus_tx(h, &addr, 1, 100)) {
        furi_hal_spi_release(h);
        return 0;
    }
    if(!furi_hal_spi_bus_rx(h, dst, n, 100)) {
        furi_hal_spi_release(h);
        return 0;
    }
    furi_hal_spi_release(h);
    return n;
}
