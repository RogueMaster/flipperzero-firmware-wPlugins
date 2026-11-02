/**
 * CC1101 packet radio access for Ducobox RF. See duco_radio.h.
 */

#include "duco_radio.h"

#include <cc1101_regs.h>
#include <furi.h>
#include <furi_hal_subghz.h>

#define DUCO_RX_SETTLE_MS 10
#define DUCO_TX_SETTLE_MS 40

static void duco_radio_resync(void);
static const uint8_t duco_preset[];

void duco_radio_start_rx(uint32_t frequency) {
    // load_custom_preset() resets the chip, which also wakes it from SLEEP,
    // so there is no separate idle/shutdown step needed here.
    furi_hal_subghz_load_custom_preset(duco_preset);
    uint32_t real_frequency = furi_hal_subghz_set_frequency_and_path(frequency);
    FURI_LOG_I(
        "Ducobox",
        "RX on %lu Hz (requested %lu)",
        (unsigned long)real_frequency,
        (unsigned long)frequency);
    furi_hal_subghz_rx();
}

void duco_radio_retune(uint32_t frequency) {
    furi_hal_subghz_idle();
    // Drop any frame left over from the previous channel so a scan step
    // cannot misattribute it to the new frequency.
    furi_hal_subghz_flush_rx();
    furi_hal_subghz_set_frequency_and_path(frequency);
    furi_hal_subghz_rx();
}

int16_t duco_radio_get_rssi(void) {
    return (int16_t)furi_hal_subghz_get_rssi();
}

bool duco_radio_read_packet(uint8_t* data, uint8_t* size, bool* crc_ok) {
    if(!furi_hal_subghz_rx_pipe_not_empty()) {
        return false;
    }

    // The FIFO becomes non-empty as soon as the first byte arrives. A full
    // 32 byte frame takes about 6.7 ms at 38.4 kBaud, so wait for the packet
    // engine to finish before reading it out.
    furi_delay_ms(DUCO_RX_SETTLE_MS);
    if(!furi_hal_subghz_rx_pipe_not_empty()) {
        return false;
    }

    uint8_t length = 0;
    furi_hal_subghz_read_packet(data, &length);
    bool valid = furi_hal_subghz_is_rx_data_crc_valid();

    if(length == 0 || length > DUCO_MAX_PACKET) {
        // Corrupt length byte, usually a FIFO overflow. Resynchronise.
        duco_radio_resync();
        return false;
    }

    FURI_LOG_I("Ducobox", "Frame %u bytes crc=%s", (unsigned)length, valid ? "ok" : "bad");

    *size = length;
    *crc_ok = valid;
    return true;
}

void duco_radio_stop(void) {
    // Stay in IDLE instead of SLEEP: a sleeping CC1101 needs an extra chip
    // select edge to wake, and idle is enough for an app that exits quickly.
    furi_hal_subghz_idle();
}

bool duco_radio_send_packet(uint32_t frequency, const uint8_t* data, uint8_t size) {
    if(size == 0 || size > DUCO_MAX_PACKET) {
        return false;
    }

    furi_hal_subghz_load_custom_preset(duco_preset);
    furi_hal_subghz_set_frequency_and_path(frequency);
    furi_hal_subghz_flush_tx();
    furi_hal_subghz_write_packet(data, size);

    bool sent = furi_hal_subghz_tx();
    if(sent) {
        furi_delay_ms(DUCO_TX_SETTLE_MS);
    }
    furi_hal_subghz_idle();
    return sent;
}

/**
 * Flush the RX FIFO after a corrupt length byte. SFRX is only valid in IDLE
 * or RXFIFO_OVERFLOW, so drop to idle first.
 */
static void duco_radio_resync(void) {
    furi_hal_subghz_idle();
    furi_hal_subghz_flush_rx();
    furi_hal_subghz_rx();
}

/**
 * Duco CC1101 register set. Register addresses must stay non-zero because
 * furi_hal_subghz_load_custom_preset() stops at the first zero byte.
 *
 * PKTCTRL1 differs from the reference plugin on purpose: CRC_AUTOFLUSH and
 * APPEND_STATUS are off. Bad-CRC frames are then visible to the app instead
 * of being silently dropped, and the FIFO holds exactly length + payload.
 */
static const uint8_t duco_preset[] = {
    /* GPIO */
    CC1101_IOCFG0,
    0x2E, // high impedance
    CC1101_IOCFG1,
    0x2E, // high impedance

    /* FIFO and packet engine */
    CC1101_FIFOTHR,
    0x47,
    CC1101_PKTCTRL1,
    0x00, // no address check, no autoflush, no appended status
    CC1101_PKTCTRL0,
    0x05, // variable length, CRC on, no whitening
    CC1101_PKTLEN,
    0x20, // 32 byte maximum payload
    CC1101_ADDR,
    0x00,
    CC1101_CHANNR,
    0x00,

    /* Sync word */
    CC1101_SYNC1,
    0xD3,
    CC1101_SYNC0,
    0x91,

    /* Frequency synthesizer (overwritten by set_frequency_and_path) */
    CC1101_FSCTRL1,
    0x06,
    CC1101_FSCTRL0,
    0x00,
    CC1101_FREQ2,
    0x21,
    CC1101_FREQ1,
    0x65,
    CC1101_FREQ0,
    0xAD,

    /* Modem: GFSK, 38.3835 kBaud, 20.63 kHz deviation, ~101.6 kHz RX BW */
    CC1101_MDMCFG4,
    0xCA,
    CC1101_MDMCFG3,
    0x83,
    CC1101_MDMCFG2,
    0x13,
    CC1101_MDMCFG1,
    0x22,
    CC1101_MDMCFG0,
    0xF8,
    CC1101_DEVIATN,
    0x35,

    /* Main radio control state machine */
    CC1101_MCSM2,
    0x07,
    CC1101_MCSM1,
    0x2F,
    CC1101_MCSM0,
    0x08,

    /* Frequency offset compensation, bit sync and AGC */
    CC1101_FOCCFG,
    0x16,
    CC1101_BSCFG,
    0x6C,
    CC1101_AGCCTRL2,
    0x43,
    CC1101_AGCCTRL1,
    0x40,
    CC1101_AGCCTRL0,
    0x91,
    CC1101_WORCTRL,
    0xFB,
    CC1101_FREND1,
    0x56,
    CC1101_FREND0,
    0x10,

    /* Calibration and test */
    CC1101_FSCAL3,
    0xE9,
    CC1101_FSCAL2,
    0x2A,
    CC1101_FSCAL1,
    0x00,
    CC1101_FSCAL0,
    0x1F,
    CC1101_FSTEST,
    0x59,
    CC1101_AGCTEST,
    0x3F,
    CC1101_TEST2,
    0x81,
    CC1101_TEST1,
    0x35,
    CC1101_TEST0,
    0x09,

    /* End of register list */
    0x00,
    0x00,

    /* PATABLE: 10 dBm */
    0xC0,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
};
