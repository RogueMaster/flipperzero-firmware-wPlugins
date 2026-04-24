#include "rf_gen.h"
#include "aprs.h"

#include <furi_hal.h>
#include <cc1101_regs.h>
#include <lib/subghz/devices/cc1101_configs.h>
#include <lib/toolbox/level_duration.h>

#include <stdio.h>
#include <string.h>

bool call_split(const char* s, char* out, uint8_t* ssid, bool* has_ssid);

#define FLIPPERHAM_ASYNC_PRESET(NAME, MOD, DRATE3, DRATE4, DEV) \
    static const uint8_t NAME[] = {                             \
        CC1101_IOCFG0,                                          \
        0x0D,                                                   \
        CC1101_FSCTRL1,                                         \
        0x06,                                                   \
        CC1101_PKTCTRL0,                                        \
        0x32,                                                   \
        CC1101_PKTCTRL1,                                        \
        0x04,                                                   \
        CC1101_MDMCFG0,                                         \
        0x00,                                                   \
        CC1101_MDMCFG1,                                         \
        0x02,                                                   \
        CC1101_MDMCFG2,                                         \
        MOD,                                                    \
        CC1101_MDMCFG3,                                         \
        DRATE3,                                                 \
        CC1101_MDMCFG4,                                         \
        DRATE4,                                                 \
        CC1101_DEVIATN,                                         \
        DEV,                                                    \
        CC1101_MCSM0,                                           \
        0x18,                                                   \
        CC1101_FOCCFG,                                          \
        0x16,                                                   \
        CC1101_AGCCTRL0,                                        \
        0x91,                                                   \
        CC1101_AGCCTRL1,                                        \
        0x00,                                                   \
        CC1101_AGCCTRL2,                                        \
        0x07,                                                   \
        CC1101_WORCTRL,                                         \
        0xFB,                                                   \
        CC1101_FREND0,                                          \
        0x10,                                                   \
        CC1101_FREND1,                                          \
        0x56,                                                   \
        0,                                                      \
        0,                                                      \
        0xC0,                                                   \
        0x00,                                                   \
        0x00,                                                   \
        0x00,                                                   \
        0x00,                                                   \
        0x00,                                                   \
        0x00,                                                   \
        0x00,                                                   \
    };

static const FlipperHamPreset* flipperham_preset = &flipperham_presets[FlipperHamPresetDefault];

static bool wave_flag(FlipperHamApp* app);
static bool wave_put(FlipperHamApp* app, uint8_t bit);
static bool wave_add(FlipperHamApp* app, double value);
static LevelDuration edge_yield(void* context);
static uint16_t round_u16_even(double value);
static const char* aprs_path_pick(FlipperHamApp* app);
static void presetpick(uint8_t mod, uint8_t dev);

FLIPPERHAM_ASYNC_PRESET(flipperham_preset_2fsk_d00_async_regs, 0x04, 0x83, 0x67, 0x00)
FLIPPERHAM_ASYNC_PRESET(flipperham_preset_2fsk_d01_async_regs, 0x04, 0x83, 0x67, 0x01)
FLIPPERHAM_ASYNC_PRESET(flipperham_preset_2fsk_d02_async_regs, 0x04, 0x83, 0x67, 0x02)
FLIPPERHAM_ASYNC_PRESET(flipperham_preset_2fsk_d03_async_regs, 0x04, 0x83, 0x67, 0x03)
FLIPPERHAM_ASYNC_PRESET(flipperham_preset_2fsk_d04_async_regs, 0x04, 0x83, 0x67, 0x04)
FLIPPERHAM_ASYNC_PRESET(flipperham_preset_2fsk_d05_async_regs, 0x04, 0x83, 0x67, 0x05)
FLIPPERHAM_ASYNC_PRESET(flipperham_preset_2fsk_d06_async_regs, 0x04, 0x83, 0x67, 0x06)
FLIPPERHAM_ASYNC_PRESET(flipperham_preset_2fsk_d07_async_regs, 0x04, 0x83, 0x67, 0x07)
FLIPPERHAM_ASYNC_PRESET(flipperham_preset_2fsk_d15_async_regs, 0x04, 0x83, 0x67, 0x15)

FLIPPERHAM_ASYNC_PRESET(flipperham_preset_gfsk_d00_async_regs, 0x14, 0x83, 0x67, 0x00)
FLIPPERHAM_ASYNC_PRESET(flipperham_preset_gfsk_d01_async_regs, 0x14, 0x83, 0x67, 0x01)
FLIPPERHAM_ASYNC_PRESET(flipperham_preset_gfsk_d02_async_regs, 0x14, 0x83, 0x67, 0x02)
FLIPPERHAM_ASYNC_PRESET(flipperham_preset_gfsk_d03_async_regs, 0x14, 0x83, 0x67, 0x03)
FLIPPERHAM_ASYNC_PRESET(flipperham_preset_gfsk_d04_async_regs, 0x14, 0x83, 0x67, 0x04)
FLIPPERHAM_ASYNC_PRESET(flipperham_preset_gfsk_d05_async_regs, 0x14, 0x83, 0x67, 0x05)
FLIPPERHAM_ASYNC_PRESET(flipperham_preset_gfsk_d06_async_regs, 0x14, 0x83, 0x67, 0x06)
FLIPPERHAM_ASYNC_PRESET(flipperham_preset_gfsk_d07_async_regs, 0x14, 0x83, 0x67, 0x07)
FLIPPERHAM_ASYNC_PRESET(flipperham_preset_gfsk_d15_async_regs, 0x14, 0x83, 0x67, 0x15)

const FlipperHamModemProfile flipperham_modem_profiles[] = {
    {"300bd", 300, 1600, 1800},
    {"1200bd", 1200, 1200, 2200},
};

const FlipperHamPreset flipperham_presets[] = {
    {"2FSK 1.6", flipperham_preset_2fsk_d00_async_regs},
    {"GFSK 1.6", flipperham_preset_gfsk_d00_async_regs},
    {"2FSK 1.8", flipperham_preset_2fsk_d01_async_regs},
    {"GFSK 1.8", flipperham_preset_gfsk_d01_async_regs},
    {"2FSK 2.0", flipperham_preset_2fsk_d02_async_regs},
    {"GFSK 2.0", flipperham_preset_gfsk_d02_async_regs},
    {"2FSK 2.2", flipperham_preset_2fsk_d03_async_regs},
    {"GFSK 2.2", flipperham_preset_gfsk_d03_async_regs},
    {"2FSK 2.4", flipperham_preset_2fsk_d04_async_regs},
    {"GFSK 2.4", flipperham_preset_gfsk_d04_async_regs},
    {"2FSK 2.5", flipperham_preset_2fsk_d05_async_regs},
    {"GFSK 2.5", flipperham_preset_gfsk_d05_async_regs},
    {"2FSK 2.8", flipperham_preset_2fsk_d06_async_regs},
    {"GFSK 2.8", flipperham_preset_gfsk_d06_async_regs},
    {"2FSK 3.0", flipperham_preset_2fsk_d07_async_regs},
    {"GFSK 3.0", flipperham_preset_gfsk_d07_async_regs},
    {"2FSK 5.0", flipperham_preset_2fsk_d15_async_regs},
    {"GFSK 5.0", flipperham_preset_gfsk_d15_async_regs},
};

static const char* aprs_path_pick(FlipperHamApp* app) {
    static const char* paths[] = {
        "None", "RFONLY", "NOGATE", "WIDE1-1", "WIDE2-2", "ARISS", "APRSAT", "Custom"};

    if(!app) return NULL;
    if(app->aprs_path_index >= sizeof(paths) / sizeof(paths[0])) return NULL;
    if(app->aprs_path_index == 0) return NULL;
    if(app->aprs_path_index == 7 && app->aprs_path_edit[0]) return app->aprs_path_edit;
    if(app->aprs_path_index == 7) return NULL;

    return paths[app->aprs_path_index];
}

static void presetpick(uint8_t mod, uint8_t dev) {
    uint8_t i;

    if(mod > 1) mod = 0;
    if(dev > 8) dev = 8;

    i = (dev * 2) + mod;
    if(i >= sizeof(flipperham_presets) / sizeof(flipperham_presets[0]))
        i = FlipperHamPresetDefault;

    flipperham_preset = &flipperham_presets[i];
}

void preset_fix(FlipperHamApp* app) {
    if(app->rf_mod > 1) app->rf_mod = 0;
    if(app->rf_dev > 8) app->rf_dev = 8;
    presetpick(app->rf_mod, app->rf_dev);
}

uint32_t tx_freq_get(FlipperHamApp* app) {
    if(app->tx_freq_index < FREQ_N)
        if(app->freq_used[app->tx_freq_index])
            if(app->freq[app->tx_freq_index]) return app->freq[app->tx_freq_index];

    return CARRIER_HZ;
}

static bool wave_add(FlipperHamApp* app, double value) {
    uint16_t pulse;

    if(!app->wave) return false;
    if(app->wave_len >= WAVE_N) return false;

    value += app->wave_carry;
    pulse = round_u16_even(value);
    app->wave_carry = value - pulse;
    if(!pulse) return true;

    app->wave[app->wave_len++] = pulse;
    return true;
}

static uint16_t round_u16_even(double value) {
    uint16_t whole;
    double frac;

    whole = (uint16_t)value;
    frac = value - whole;

    if(frac > (double)0.5f)
        whole++;
    else if(frac >= (double)0.5f - (double)0.000001f)
        if(whole & 1) whole++;

    return whole;
}

static bool wave_put(FlipperHamApp* app, uint8_t bit) {
    const FlipperHamModemProfile* profile;
    double bit_us;
    double half_us;
    double accum_us;

    profile = &flipperham_modem_profiles[app->encoding_index];
    bit_us = 1000000.0 / profile->baud;

    if(bit == 0) {
        app->wave_is_mark = !app->wave_is_mark;
    }

    if(app->wave_is_mark)
        half_us = 1000000.0 / (2.0 * profile->mark_hz);
    else
        half_us = 1000000.0 / (2.0 * profile->space_hz);

    if(app->wave_prev_h <= (double)0.000001f) {
        app->wave_prev_h = half_us;
        app->wave_pending = 0;
        app->wave_osc_remain = half_us;
    } else if(
        half_us < app->wave_prev_h - (double)0.000001f ||
        half_us > app->wave_prev_h + (double)0.000001f) {
        /* Flush the partial old half-cycle before changing tone. */
        if(app->wave_pending > (double)0.000001f) {
            if(!wave_add(app, app->wave_pending)) return false;
        }

        app->wave_prev_h = half_us;
        app->wave_pending = 0;
        app->wave_osc_remain = half_us;
    }

    accum_us = app->wave_pending + bit_us;

    while(accum_us >= app->wave_prev_h - (double)0.000001f) {
        if(!wave_add(app, app->wave_prev_h)) return false;
        accum_us -= app->wave_prev_h;
    }

    app->wave_pending = accum_us;
    app->wave_osc_remain = app->wave_prev_h - accum_us;

    return true;
}

static bool wave_flag(FlipperHamApp* app) {
    static const uint8_t flag[] = {0, 1, 1, 1, 1, 1, 1, 0};
    uint8_t i;

    for(i = 0; i < sizeof(flag); i++) {
        if(!wave_put(app, flag[i])) return false;
    }

    return true;
}

void txstart(FlipperHamApp* app) {
    char message[96];
    char dst[CALL_LEN];
    const FlipperHamModemProfile* p;
    const char* path;
    const char* src;
    uint16_t i;
    uint16_t n;
    uint8_t src_ssid;
    uint8_t ssid;
    bool has_ssid;

    app->tx_done = false;
    app->tx_ok = false;
    app->wave_i = 0;
    app->level = true;
    app->wave_len = 0;
    app->wave_carry = 0;
    app->wave_pending = 0;
    app->wave_osc_remain = 0;
    app->wave_prev_h = 0;
    app->pre_b = 0;
    app->pre_h = 0;
    app->pre_c = 0;
    app->pre_a = 0;
    app->pre_us = 0;
    app->pre_k = 0;
    app->wave_is_mark = true;

    if(!app->pkt) return;
    if(!app->wave) return;
    if(app->tx_msg_index >= TXT_N) return;
    p = &flipperham_modem_profiles[app->encoding_index];

    if(app->tx_type == 0) {
        if(!app->bulletin_used[app->tx_msg_index]) return;
        if(!app->bulletin[app->tx_msg_index][0]) return;
        if(!aprs_bulletin(
               message, sizeof(message), app->tx_msg_index, app->bulletin[app->tx_msg_index]))
            return;
    } else if(app->tx_type == 1) {
        if(!app->status_used[app->tx_msg_index]) return;
        if(!app->status[app->tx_msg_index][0]) return;
        if(!aprs_status(message, sizeof(message), app->status[app->tx_msg_index])) return;
    } else if(app->tx_type == 3) {
        if(!app->pos_used[app->tx_msg_index]) return;
        if(!app->pos_name[app->tx_msg_index][0]) return;
        if(!app->pos_lat[app->tx_msg_index][0]) return;
        if(!app->pos_lon[app->tx_msg_index][0]) return;
        if(!aprs_pos(
               message,
               sizeof(message),
               app->pos_name[app->tx_msg_index],
               app->pos_lat[app->tx_msg_index],
               app->pos_lon[app->tx_msg_index]))
            return;
    } else {
        if(app->dst_call_index >= CALL_N) return;
        if(!app->message_used[app->tx_msg_index]) return;
        if(!app->message[app->tx_msg_index][0]) return;
        if(!app->calls_used[app->dst_call_index]) return;
        if(!app->calls[app->dst_call_index][0]) return;

        if(!call_split(app->calls[app->dst_call_index], dst, &ssid, &has_ssid)) return;
        if(!has_ssid) ssid = app->dst_ssid;
        if(!aprs_message(message, sizeof(message), dst, ssid, app->message[app->tx_msg_index]))
            return;
    }

    src = MY_CALL;
    src_ssid = 0;
    path = aprs_path_pick(app);
    if(app->ham_ok)
        if(app->ham_n)
            if(app->ham_index < app->ham_n) {
                if(app->ham_calls[app->ham_index][0]) src = app->ham_calls[app->ham_index];
                if(app->ham_has_ssid[app->ham_index]) src_ssid = app->ham_ssid[app->ham_index];
            }

    if(app->debug_tx)
        presetpick(app->dbg_mod, app->dbg_dev);
    else
        presetpick(app->rf_mod, app->rf_dev);

    if(!aprs_packet(app->pkt, src, src_ssid, MY_TOCALL, 0, message, path)) return;

    if(app->leadin_ms) {
        n = (app->leadin_ms * p->baud + 500) / 1000;
        if(!n) n = 1;
        for(i = 0; i < n; i++)
            if(!wave_put(app, 1)) return;
    }

    if(app->preamble_ms) {
        n = (app->preamble_ms * p->baud + 4000) / 8000;
        if(!n) n = 1;
        for(i = 0; i < n; i++)
            if(!wave_flag(app)) return;
    }

    if(!wave_flag(app)) return;

    for(i = 8; i + 8 < app->pkt->stuffed_len; i++) {
        if(!wave_put(app, app->pkt->stuffed[i])) return;
    }

    for(i = 0; i < 3; i++) {
        if(!wave_flag(app)) return;
    }

    if(app->wave_pending > (double)0.000001f)
        if(!wave_add(app, app->wave_pending)) return;
    app->wave_pending = 0;
    app->wave_osc_remain = 0;
    app->wave_prev_h = 0;

    if(!app->wave_len)
        app->tx_done = true;
    else
        app->tx_ok = true;
}

static LevelDuration edge_yield(void* context) {
    FlipperHamApp* app = context;
    LevelDuration ld;
    uint16_t half_us;

    if(app->tx_done) return level_duration_reset();

    if(app->wave_i >= app->wave_len) {
        app->tx_done = true;
        return level_duration_reset();
    }

    half_us = app->wave[app->wave_i];

    ld = level_duration_make(app->level, half_us);

    app->level = !app->level;
    app->wave_i++;

    if(app->wave_i >= app->wave_len) app->tx_done = true;

    return ld;
}

void flipperham_radio_start(FlipperHamApp* app) {
    furi_hal_subghz_reset();
    furi_hal_subghz_idle();
    furi_hal_subghz_load_custom_preset(flipperham_preset->regs);
    furi_hal_subghz_set_frequency_and_path(tx_freq_get(app));
    furi_hal_subghz_flush_tx();

    if(!furi_hal_subghz_tx()) {
        app->tx_allowed = false;
        app->tx_done = true;
        return;
    }

    app->tx_allowed = furi_hal_subghz_start_async_tx(edge_yield, app);
    app->tx_started = app->tx_allowed;
    if(!app->tx_allowed) app->tx_done = true;
}

void flipperham_radio_stop(FlipperHamApp* app) {
    if(app->tx_started) furi_hal_subghz_stop_async_tx();

    app->tx_started = false;
    furi_hal_subghz_sleep();
}
