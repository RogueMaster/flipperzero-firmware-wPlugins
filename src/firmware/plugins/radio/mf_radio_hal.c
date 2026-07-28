#include "mf_radio_hal.h"

#include "mf_radio_types.h"

#ifdef MORSE_FLIPPER_FAP
#include <furi_hal.h>
#include <lib/subghz/devices/cc1101_configs.h>
#include <cc1101_regs.h>
#endif

static const uint8_t carrier_ook_650khz_no_autocal_regs[] = {
#ifdef MORSE_FLIPPER_FAP
    CC1101_IOCFG0,
#else
    0x02,
#endif
    0x0E,
#ifdef MORSE_FLIPPER_FAP
    CC1101_FIFOTHR,
#else
    0x03,
#endif
    0x47,
#ifdef MORSE_FLIPPER_FAP
    CC1101_PKTCTRL0,
#else
    0x08,
#endif
    0x32,
#ifdef MORSE_FLIPPER_FAP
    CC1101_FSCTRL1,
#else
    0x0B,
#endif
    0x06,
#ifdef MORSE_FLIPPER_FAP
    CC1101_MDMCFG0,
#else
    0x14,
#endif
    0x00,
#ifdef MORSE_FLIPPER_FAP
    CC1101_MDMCFG1,
#else
    0x13,
#endif
    0x00,
#ifdef MORSE_FLIPPER_FAP
    CC1101_MDMCFG2,
#else
    0x12,
#endif
    0x30,
#ifdef MORSE_FLIPPER_FAP
    CC1101_MDMCFG3,
#else
    0x11,
#endif
    0x32,
#ifdef MORSE_FLIPPER_FAP
    CC1101_MDMCFG4,
#else
    0x10,
#endif
    0x67,
#ifdef MORSE_FLIPPER_FAP
    CC1101_MCSM0,
#else
    0x18,
#endif
    0x08,
#ifdef MORSE_FLIPPER_FAP
    CC1101_FOCCFG,
#else
    0x19,
#endif
    0x18,
#ifdef MORSE_FLIPPER_FAP
    CC1101_AGCCTRL0,
#else
    0x1D,
#endif
    0x40,
#ifdef MORSE_FLIPPER_FAP
    CC1101_AGCCTRL1,
#else
    0x1C,
#endif
    0x00,
#ifdef MORSE_FLIPPER_FAP
    CC1101_AGCCTRL2,
#else
    0x1B,
#endif
    0x03,
#ifdef MORSE_FLIPPER_FAP
    CC1101_WORCTRL,
#else
    0x20,
#endif
    0xFB,
#ifdef MORSE_FLIPPER_FAP
    CC1101_FREND0,
#else
    0x22,
#endif
    0x11,
#ifdef MORSE_FLIPPER_FAP
    CC1101_FREND1,
#else
    0x21,
#endif
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

static bool hal_prepare_common(uint32_t frequency_hz, bool output) {
#ifdef MORSE_FLIPPER_FAP
    const GpioPin* data_gpio = furi_hal_subghz_get_data_gpio();
    furi_hal_subghz_reset();
    furi_hal_subghz_load_custom_preset(carrier_ook_650khz_no_autocal_regs);
    (void)furi_hal_subghz_set_frequency_and_path(frequency_hz);
    furi_hal_gpio_init(
        data_gpio,
        output ? GpioModeOutputPushPull : GpioModeInput,
        GpioPullNo,
        GpioSpeedLow);
    if(output) furi_hal_gpio_write(data_gpio, false);
    return true;
#else
    (void)frequency_hz;
    (void)output;
    return false;
#endif
}

static bool hal_prepare_tx(void* context, uint32_t frequency_hz) {
    (void)context;
#ifdef MORSE_FLIPPER_FAP
    if(!hal_prepare_common(frequency_hz, true)) return false;
    return furi_hal_subghz_tx();
#else
    return hal_prepare_common(frequency_hz, true);
#endif
}

static bool hal_prepare_carrier_rx(void* context, uint32_t frequency_hz) {
    (void)context;
#ifdef MORSE_FLIPPER_FAP
    if(!hal_prepare_common(frequency_hz, false)) return false;
    furi_hal_subghz_rx();
    return true;
#else
    return hal_prepare_common(frequency_hz, false);
#endif
}

static void hal_set_tx_level(void* context, bool level) {
    (void)context;
#ifdef MORSE_FLIPPER_FAP
    furi_hal_gpio_write(furi_hal_subghz_get_data_gpio(), level);
#else
    (void)level;
#endif
}

static bool hal_read_carrier(void* context) {
    (void)context;
#ifdef MORSE_FLIPPER_FAP
    return furi_hal_gpio_read(furi_hal_subghz_get_data_gpio());
#else
    return false;
#endif
}

static int8_t hal_read_rssi_dbm(void* context) {
    (void)context;
#ifdef MORSE_FLIPPER_FAP
    float rssi = furi_hal_subghz_get_rssi();
    return rssi >= 0.0f ? (int8_t)(rssi + 0.5f) : (int8_t)(rssi - 0.5f);
#else
    return -127;
#endif
}

static bool hal_frequency_valid(void* context, uint32_t frequency_hz) {
    (void)context;
#ifdef MORSE_FLIPPER_FAP
    return furi_hal_subghz_is_frequency_valid(frequency_hz);
#else
    return frequency_hz != 0U;
#endif
}

static bool hal_tx_allowed(void* context, uint32_t frequency_hz) {
    return hal_frequency_valid(context, frequency_hz)
#ifdef MORSE_FLIPPER_FAP
           && furi_hal_region_is_frequency_allowed(frequency_hz)
#endif
        ;
}

static uint32_t hal_default_frequency(void* context) {
    static const uint32_t candidates[] = {
        MF_RADIO_DEFAULT_FREQUENCY_HZ,
        315000000U,
        868350000U,
        915000000U,
        920500000U,
    };
    size_t i;
    (void)context;
    for(i = 0U; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        if(hal_tx_allowed(NULL, candidates[i])) return candidates[i];
    }
    return MF_RADIO_DEFAULT_FREQUENCY_HZ;
}

static void hal_idle(void* context) {
    (void)context;
#ifdef MORSE_FLIPPER_FAP
    furi_hal_subghz_idle();
#endif
}

static void hal_sleep(void* context) {
    (void)context;
#ifdef MORSE_FLIPPER_FAP
    furi_hal_subghz_sleep();
#endif
}

static const MfRadioHardwareOps hardware_ops = {
    .prepare_tx = hal_prepare_tx,
    .prepare_carrier_rx = hal_prepare_carrier_rx,
    .set_tx_level = hal_set_tx_level,
    .read_carrier = hal_read_carrier,
    .read_rssi_dbm = hal_read_rssi_dbm,
    .frequency_valid = hal_frequency_valid,
    .tx_allowed = hal_tx_allowed,
    .default_frequency = hal_default_frequency,
    .idle = hal_idle,
    .sleep = hal_sleep,
};

const MfRadioHardwareOps* mf_radio_hal_ops(void) {
    return &hardware_ops;
}

