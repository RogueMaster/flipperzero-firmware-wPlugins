#include "mf_radio_hal.h"

#include "mf_radio_types.h"

#include <stddef.h>
#include <string.h>

#ifdef MORSE_FLIPPER_FAP
#include <cc1101_regs.h>
#include <furi_hal.h>
#include <lib/subghz/devices/cc1101_configs.h>
#endif

typedef struct {
    uint32_t selected_frequency_hz;
    bool ook_prepared;
    bool ook_running;
    bool fm_prepared;
    bool async_running;
    bool static_running;
    MfRadioCwfmTiming timing;
} MfRadioHalContext;

static MfRadioHalContext hal_context;

bool mf_radio_cwfm_static_config(
    uint32_t selected_frequency_hz,
    MfRadioFrequencyPredicate frequency_valid,
    MfRadioFrequencyPredicate frequency_allowed,
    MfRadioCwfmStaticConfig* config) {
    uint32_t candidate;
    if(frequency_valid == NULL || frequency_allowed == NULL || config == NULL ||
       !frequency_valid(selected_frequency_hz) || !frequency_allowed(selected_frequency_hz))
        return false;
    /* Move only the quiet carrier so one static FSK leg rests on the selected frequency. */
    if(selected_frequency_hz <= UINT32_MAX - MF_RADIO_CWFM_DEVIATION_HZ) {
        candidate = selected_frequency_hz + MF_RADIO_CWFM_DEVIATION_HZ;
        if(frequency_valid(candidate) && frequency_allowed(candidate)) {
            config->frequency_hz = candidate;
            config->data_level = false;
            return true;
        }
    }
    if(selected_frequency_hz >= MF_RADIO_CWFM_DEVIATION_HZ) {
        candidate = selected_frequency_hz - MF_RADIO_CWFM_DEVIATION_HZ;
        if(frequency_valid(candidate) && frequency_allowed(candidate)) {
            config->frequency_hz = candidate;
            config->data_level = true;
            return true;
        }
    }
    return false;
}

void mf_radio_cwfm_timing_reset(MfRadioCwfmTiming* timing) {
    if(timing == NULL) return;
    timing->remainder = 0U;
    timing->phase = false;
}

uint16_t mf_radio_cwfm_next_half_period(MfRadioCwfmTiming* timing, bool* level) {
    uint16_t duration = 714U;
    if(timing == NULL || level == NULL) return 0U;
    timing->remainder = (uint8_t)(timing->remainder + 2U);
    if(timing->remainder >= 7U) {
        timing->remainder = (uint8_t)(timing->remainder - 7U);
        duration++;
    }
    timing->phase = !timing->phase;
    *level = timing->phase;
    return duration;
}

#ifdef MORSE_FLIPPER_FAP
static LevelDuration hal_cwfm_yield(void* context) {
    MfRadioHalContext* hal = context;
    bool level;
    uint16_t duration = mf_radio_cwfm_next_half_period(&hal->timing, &level);
    return level_duration_make(level, duration);
}
#endif

#ifdef MORSE_FLIPPER_FAP
static const uint8_t tx_ook_270khz_no_autocal_regs[] = {
    0x02, 0x0D, 0x03, 0x47, 0x08, 0x32, 0x0B, 0x06, 0x14, 0x00, 0x13, 0x00, 0x12, 0x30, 0x11,
    0x32, 0x10, 0x67, 0x18, 0x08, 0x19, 0x18, 0x1D, 0x40, 0x1C, 0x00, 0x1B, 0x03, 0x20, 0xFB,
    0x22, 0x11, 0x21, 0xB6, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

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
#endif

static bool hal_prepare_common(uint32_t frequency_hz, bool output) {
#ifdef MORSE_FLIPPER_FAP
    const GpioPin* data_gpio = furi_hal_subghz_get_data_gpio();
    const uint8_t* preset = output ? tx_ook_270khz_no_autocal_regs :
                                     carrier_ook_650khz_no_autocal_regs;
    furi_hal_subghz_load_custom_preset(preset);
    (void)furi_hal_subghz_set_frequency_and_path(frequency_hz);
    furi_hal_gpio_init(
        data_gpio, output ? GpioModeOutputPushPull : GpioModeInput, GpioPullNo, GpioSpeedLow);
    if(output) furi_hal_gpio_write(data_gpio, false);
    return true;
#else
    (void)frequency_hz;
    (void)output;
    return false;
#endif
}

static bool hal_prepare_tx(void* context, uint32_t frequency_hz, MfRadioTxMode mode) {
    MfRadioHalContext* hal = context;
    memset(hal, 0, sizeof(*hal));
#ifdef MORSE_FLIPPER_FAP
    if(mode == MfRadioTxModeCwfm) {
        const GpioPin* data_gpio = furi_hal_subghz_get_data_gpio();
        furi_hal_subghz_load_custom_preset(subghz_device_cc1101_preset_2fsk_dev2_38khz_async_regs);
        (void)furi_hal_subghz_set_frequency_and_path(frequency_hz);
        furi_hal_gpio_init(data_gpio, GpioModeInput, GpioPullNo, GpioSpeedLow);
        hal->selected_frequency_hz = frequency_hz;
        hal->fm_prepared = true;
        return true;
    }
    if(!hal_prepare_common(frequency_hz, true)) return false;
    hal->ook_prepared = true;
    return true;
#else
    (void)mode;
    return hal_prepare_common(frequency_hz, true);
#endif
}

static bool hal_prepare_carrier_rx(void* context, uint32_t frequency_hz) {
    MfRadioHalContext* hal = context;
    memset(hal, 0, sizeof(*hal));
#ifdef MORSE_FLIPPER_FAP
    if(!hal_prepare_common(frequency_hz, false)) return false;
    furi_hal_subghz_rx();
    return true;
#else
    return hal_prepare_common(frequency_hz, false);
#endif
}

static bool hal_set_tx_level(void* context, bool level) {
    MfRadioHalContext* hal = context;
#ifdef MORSE_FLIPPER_FAP
    const GpioPin* data_gpio = furi_hal_subghz_get_data_gpio();
    if(hal->fm_prepared) {
        if(level) {
            if(hal->async_running) return true;
            if(hal->static_running) {
                furi_hal_subghz_idle();
                (void)furi_hal_subghz_set_frequency_and_path(hal->selected_frequency_hz);
                hal->static_running = false;
            }
            furi_hal_gpio_init(data_gpio, GpioModeInput, GpioPullNo, GpioSpeedLow);
            mf_radio_cwfm_timing_reset(&hal->timing);
            if(!furi_hal_subghz_start_async_tx(hal_cwfm_yield, hal)) return false;
            hal->async_running = true;
            return true;
        }
        if(hal->static_running) return true;
        if(hal->async_running) {
            furi_hal_subghz_stop_async_tx();
            hal->async_running = false;
            mf_radio_cwfm_timing_reset(&hal->timing);
        }
        MfRadioCwfmStaticConfig static_config;
        if(!mf_radio_cwfm_static_config(
               hal->selected_frequency_hz,
               furi_hal_subghz_is_frequency_valid,
               furi_hal_region_is_frequency_allowed,
               &static_config))
            return false;
        (void)furi_hal_subghz_set_frequency_and_path(static_config.frequency_hz);
        furi_hal_gpio_init(data_gpio, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
        furi_hal_gpio_write(data_gpio, static_config.data_level);
        if(!furi_hal_subghz_tx()) return false;
        hal->static_running = true;
        return true;
    }
    if(!hal->ook_prepared) return false;
    if(level && !hal->ook_running) {
        if(!furi_hal_subghz_tx()) return false;
        hal->ook_running = true;
    }
    furi_hal_gpio_write(data_gpio, level);
#else
    (void)hal;
    (void)level;
#endif
    return true;
}

static void hal_stop_tx(void* context) {
    MfRadioHalContext* hal = context;
#ifdef MORSE_FLIPPER_FAP
    const GpioPin* data_gpio = furi_hal_subghz_get_data_gpio();
    if(hal->async_running) furi_hal_subghz_stop_async_tx();
    furi_hal_gpio_write(data_gpio, false);
    furi_hal_gpio_init(data_gpio, GpioModeInput, GpioPullNo, GpioSpeedLow);
#endif
    memset(hal, 0, sizeof(*hal));
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

#ifdef MORSE_FLIPPER_FAP
typedef struct {
    uint32_t min_hz;
    uint32_t max_hz;
} HalRadioBand;

static const HalRadioBand hal_vfo_bands[] = {
    {300000000U, 348000000U},
    {387000000U, 464000000U},
    {779000000U, 928000000U},
};

static bool hal_region_wide_open(void) {
    const char* name = furi_hal_region_get_name();
    const FuriHalRegion* region = furi_hal_region_get();
    if(name != NULL && strcmp(name, "00") == 0) return true;
    if(region != NULL && region->bands_count == 1U && region->bands[0].start == 0U &&
       region->bands[0].end >= 1000000000U)
        return true;
    return furi_hal_region_is_frequency_allowed(hal_vfo_bands[0].min_hz) &&
           furi_hal_region_is_frequency_allowed(hal_vfo_bands[1].min_hz) &&
           furi_hal_region_is_frequency_allowed(hal_vfo_bands[2].min_hz);
}

static uint32_t hal_region_band_default(void) {
    const FuriHalRegion* region = furi_hal_region_get();
    size_t region_i;
    size_t vfo_i;
    if(region == NULL) return MF_RADIO_DEFAULT_FREQUENCY_HZ;
    for(region_i = 0U; region_i < region->bands_count; region_i++) {
        for(vfo_i = 0U; vfo_i < sizeof(hal_vfo_bands) / sizeof(hal_vfo_bands[0]); vfo_i++) {
            uint32_t region_min_khz = (region->bands[region_i].start + 999U) / 1000U;
            uint32_t region_max_khz = region->bands[region_i].end / 1000U;
            uint32_t vfo_min_khz = hal_vfo_bands[vfo_i].min_hz / 1000U;
            uint32_t vfo_max_khz = hal_vfo_bands[vfo_i].max_hz / 1000U;
            uint32_t min_khz = region_min_khz > vfo_min_khz ? region_min_khz : vfo_min_khz;
            uint32_t max_khz = region_max_khz < vfo_max_khz ? region_max_khz : vfo_max_khz;
            uint32_t candidate;
            if(min_khz > max_khz) continue;
            candidate = (min_khz + ((max_khz - min_khz) / 2U)) * 1000U;
            if(hal_tx_allowed(NULL, candidate)) return candidate;
        }
    }
    return MF_RADIO_DEFAULT_FREQUENCY_HZ;
}
#endif

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
#ifdef MORSE_FLIPPER_FAP
    if(hal_region_wide_open()) return MF_RADIO_DEFAULT_FREQUENCY_HZ;
#endif
    for(i = 0U; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        if(hal_tx_allowed(NULL, candidates[i])) return candidates[i];
    }
#ifdef MORSE_FLIPPER_FAP
    return hal_region_band_default();
#else
    return MF_RADIO_DEFAULT_FREQUENCY_HZ;
#endif
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
    .stop_tx = hal_stop_tx,
    .read_carrier = hal_read_carrier,
    .read_rssi_dbm = hal_read_rssi_dbm,
    .frequency_valid = hal_frequency_valid,
    .tx_allowed = hal_tx_allowed,
    .default_frequency = hal_default_frequency,
    .idle = hal_idle,
    .sleep = hal_sleep,
    .context = &hal_context,
};

const MfRadioHardwareOps* mf_radio_hal_ops(void) {
    return &hardware_ops;
}
