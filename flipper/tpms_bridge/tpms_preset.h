#pragma once

#include <cc1101_regs.h>

/* CC1101 preset for TPMS: 2-FSK, 20 kBaud, 28.56 kHz deviation, 325 kHz
 * receive bandwidth. Taken from ProtoView (custom_presets.h,
 * protoview_subghz_tpms1_fsk_async_regs), where it is proven against real
 * Renault sensors.
 *
 * Format expected by furi_hal_subghz_load_custom_preset /
 * FuriHalSubGhzPresetCustom: a flat array of "register, value" pairs, a
 * 0x00 0x00 terminator, then 8 bytes of PA table.
 */
static const uint8_t tpms_fsk_preset[] = {
    /* GDO0 outputs demodulated data asynchronously */
    CC1101_IOCFG0,
    0x0D,

    /* Frequency synthesizer: IF = 26 MHz / 2^10 * 6 = 152.34 kHz */
    CC1101_FSCTRL1,
    0x06,

    /* Packet engine: asynchronous mode, no whitening */
    CC1101_PKTCTRL0,
    0x32,
    CC1101_PKTCTRL1,
    0x04,

    /* Modem */
    CC1101_MDMCFG0,
    0x00,
    CC1101_MDMCFG1,
    0x02,
    CC1101_MDMCFG2,
    0x04, /* 2-FSK, no preamble/sync — we detect them ourselves */
    CC1101_MDMCFG3,
    0x93, /* 20 kBaud */
    CC1101_MDMCFG4,
    0x59, /* 325 kHz bandwidth */
    CC1101_DEVIATN,
    0x41, /* 28.56 kHz deviation */

    /* Auto-calibration on idle -> rx/tx transition */
    CC1101_MCSM0,
    0x18,

    /* Frequency offset compensation */
    CC1101_FOCCFG,
    0x16,

    /* AGC */
    CC1101_AGCCTRL0,
    0x91,
    CC1101_AGCCTRL1,
    0x00,
    CC1101_AGCCTRL2,
    0x07,

    /* Wake on radio */
    CC1101_WORCTRL,
    0xFB,

    /* Front end */
    CC1101_FREND0,
    0x10,
    CC1101_FREND1,
    0x56,

    /* End of register list */
    0x00,
    0x00,

    /* PA table (unused in RX) */
    0xC0,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
};
