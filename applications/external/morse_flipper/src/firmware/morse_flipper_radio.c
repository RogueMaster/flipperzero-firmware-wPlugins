/*
 * Purpose: Control the CC1101 radio backend for Morse RF modes.
 * Owns: radio profile setup, TX/RX state, edge callbacks, and RSSI reads.
 * Depends on: morse_flipper_radio.h and Flipper Sub-GHz HAL in FAP builds.
 * Tests: tests/test_rf.c covers host-side fallback paths.
 */

#include "morse_flipper_radio.h"

#include <string.h>

#define MORSE_FLIPPER_RADIO_RX_RING      64u
#define MORSE_FLIPPER_RADIO_RX_DRAIN_MAX 8u

#ifdef MORSE_FLIPPER_FAP
#include <furi_hal.h>
#include <lib/subghz/devices/cc1101_configs.h>
#include <cc1101_regs.h>

static const uint8_t morse_flipper_ook_270khz_no_autocal_regs[] = {
    0x02, 0x0D, 0x03, 0x47, 0x08, 0x32, 0x0B, 0x06, 0x14, 0x00, 0x13, 0x00, 0x12, 0x30, 0x11,
    0x32, 0x10, 0x67, 0x18, 0x08, 0x19, 0x18, 0x1D, 0x40, 0x1C, 0x00, 0x1B, 0x03, 0x20, 0xFB,
    0x22, 0x11, 0x21, 0xB6, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t morse_flipper_carrier_ook_650khz_no_autocal_regs[] = {
    CC1101_IOCFG0,
    0x0E,
    CC1101_FIFOTHR,
    0x47,
    CC1101_PKTCTRL0,
    0x32,
    CC1101_FSCTRL1,
    0x06,
    CC1101_MDMCFG0,
    0x00,
    CC1101_MDMCFG1,
    0x00,
    CC1101_MDMCFG2,
    0x30,
    CC1101_MDMCFG3,
    0x32,
    CC1101_MDMCFG4,
    0x67,
    CC1101_MCSM0,
    0x08,
    CC1101_FOCCFG,
    0x18,
    CC1101_AGCCTRL0,
    0x40,
    CC1101_AGCCTRL1,
    0x00,
    CC1101_AGCCTRL2,
    0x03,
    CC1101_WORCTRL,
    0xFB,
    CC1101_FREND0,
    0x11,
    CC1101_FREND1,
    0xB6,
    0x00,
    0x00,
    0x00,
    0xC0,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
};

static void radio_rx_capture(bool level, uint32_t duration, void* context) {
    MorseFlipperRadio* radio = context;
    uint8_t next;
    uint16_t ms;

    if(!radio || !duration) return;

    ms = duration > 65535u ? 65535u : (uint16_t)duration;
    next = (uint8_t)((radio->rx_wr + 1u) % MORSE_FLIPPER_RADIO_RX_RING);
    if(next == radio->rx_rd)
        radio->rx_rd = (uint8_t)((radio->rx_rd + 1u) % MORSE_FLIPPER_RADIO_RX_RING);

    radio->rx_mark[radio->rx_wr] = level;
    radio->rx_ms[radio->rx_wr] = ms;
    radio->rx_wr = next;
}

static void radio_apply_frequency(MorseFlipperRadio* radio) {
    if(!radio || !radio->freq_hz) return;
    radio->freq_hz = furi_hal_subghz_set_frequency_and_path(radio->freq_hz);
    radio->tuned_hz = radio->freq_hz;
}

static void radio_prepare_preset(MorseFlipperRadio* radio) {
    const uint8_t* preset;

    if(!radio || radio->ready) return;

    preset = radio->profile == MorseFlipperRadioProfileCarrierSense ?
                 morse_flipper_carrier_ook_650khz_no_autocal_regs :
                 morse_flipper_ook_270khz_no_autocal_regs;

    furi_hal_subghz_reset();
    furi_hal_subghz_load_custom_preset(preset);
    radio->ready = true;
    radio->tuned_hz = 0;
}

static void radio_prepare_rx(MorseFlipperRadio* radio) {
    const GpioPin* data_gpio;

    if(!radio) return;

    radio_prepare_preset(radio);
    data_gpio = furi_hal_subghz_get_data_gpio();
    if(radio->tuned_hz != radio->freq_hz) radio_apply_frequency(radio);
    furi_hal_gpio_init(data_gpio, GpioModeInput, GpioPullNo, GpioSpeedLow);
    if(radio->profile == MorseFlipperRadioProfileCarrierSense) {
        furi_hal_subghz_rx();
    }
}

static bool radio_prepare_tx(MorseFlipperRadio* radio) {
    const GpioPin* data_gpio;

    if(!radio) return false;

    radio_prepare_preset(radio);

    data_gpio = furi_hal_subghz_get_data_gpio();
    if(radio->tuned_hz != radio->freq_hz) radio_apply_frequency(radio);
    furi_hal_gpio_init(data_gpio, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_write(data_gpio, false);
    radio->tx_level = false;
    return furi_hal_subghz_tx();
}
#endif

static void radio_stop_rx(MorseFlipperRadio* radio) {
    if(!radio || !radio->rx_on) return;

#ifdef MORSE_FLIPPER_FAP
    if(radio->profile == MorseFlipperRadioProfileCarrierSense) {
        furi_hal_subghz_idle();
    } else {
        furi_hal_subghz_stop_async_rx();
        furi_hal_subghz_idle();
    }
#endif

    radio->rx_on = false;
    radio->rx_rd = radio->rx_wr;
}

static void radio_stop_tx(MorseFlipperRadio* radio) {
    if(!radio) return;

#ifdef MORSE_FLIPPER_FAP
    if(radio->tx_on) {
        furi_hal_gpio_write(furi_hal_subghz_get_data_gpio(), false);
        furi_hal_subghz_idle();
    }
#endif

    radio->tx_on = false;
    radio->tx_level = false;
}

void morse_flipper_radio_init(MorseFlipperRadio* radio) {
    if(!radio) return;
    memset(radio, 0, sizeof(*radio));
    radio->profile = MorseFlipperRadioProfileOokData;
}

void morse_flipper_radio_deinit(MorseFlipperRadio* radio) {
    if(!radio) return;

#ifdef MORSE_FLIPPER_FAP
    radio_stop_rx(radio);
    radio_stop_tx(radio);
    if(radio->ready) furi_hal_subghz_sleep();
#endif

    radio->ready = false;
    radio->tx_on = false;
    radio->rx_on = false;
    radio->freq_hz = 0;
    radio->tuned_hz = 0;
}

void morse_flipper_radio_set_rx_callback(
    MorseFlipperRadio* radio,
    void (*cb)(void* ctx, bool level, uint16_t duration_ms),
    void* ctx) {
    if(!radio) return;
    radio->rx_cb = cb;
    radio->rx_ctx = ctx;
}

void morse_flipper_radio_sync_live(
    MorseFlipperRadio* radio,
    uint32_t freq_hz,
    bool active,
    bool tx_on,
    MorseFlipperRadioProfile profile) {
    if(!radio) return;

#ifdef MORSE_FLIPPER_FAP
    if(radio->profile != profile) {
        radio_stop_rx(radio);
        radio_stop_tx(radio);
        radio->ready = false;
        radio->tuned_hz = 0;
        radio->profile = profile;
    }

    if(freq_hz && radio->freq_hz != freq_hz) {
        radio_stop_rx(radio);
        radio_stop_tx(radio);
        radio->freq_hz = freq_hz;
        radio->tuned_hz = 0;
    }

    if(!active) {
        radio_stop_rx(radio);
        radio_stop_tx(radio);
        return;
    }

    if(tx_on) {
        radio_stop_rx(radio);
        if(radio->tx_on) return;
        if(!radio->freq_hz && freq_hz) radio->freq_hz = freq_hz;
        radio->tx_on = radio_prepare_tx(radio);
        return;
    }

    radio_stop_tx(radio);
    if(!radio->rx_on) {
        if(!radio->freq_hz && freq_hz) radio->freq_hz = freq_hz;
        radio_prepare_rx(radio);
        if(radio->profile != MorseFlipperRadioProfileCarrierSense) {
            furi_hal_subghz_flush_rx();
            furi_hal_subghz_start_async_rx(radio_rx_capture, radio);
        }
        radio->rx_on = true;
    }
#else
    (void)freq_hz;
    (void)active;
    (void)profile;
    radio->tx_on = tx_on;
#endif
}

void morse_flipper_radio_drain_rx(MorseFlipperRadio* radio) {
    uint8_t n = 0;

    if(!radio || !radio->rx_cb) return;

    while(radio->rx_rd != radio->rx_wr && n < MORSE_FLIPPER_RADIO_RX_DRAIN_MAX) {
        radio->rx_cb(radio->rx_ctx, radio->rx_mark[radio->rx_rd], radio->rx_ms[radio->rx_rd]);
        radio->rx_rd = (uint8_t)((radio->rx_rd + 1u) % MORSE_FLIPPER_RADIO_RX_RING);
        n++;
    }
}

void morse_flipper_radio_set_tx_level(MorseFlipperRadio* radio, bool level) {
    if(!radio || !radio->tx_on) return;
    if(radio->tx_level == level) return;

#ifdef MORSE_FLIPPER_FAP
    furi_hal_gpio_write(furi_hal_subghz_get_data_gpio(), level);
#else
    (void)level;
#endif
    radio->tx_level = level;
}

bool morse_flipper_radio_read_rx_level(const MorseFlipperRadio* radio) {
    if(!radio || !radio->rx_on) return false;

#ifdef MORSE_FLIPPER_FAP
    return furi_hal_gpio_read(furi_hal_subghz_get_data_gpio());
#else
    return false;
#endif
}
