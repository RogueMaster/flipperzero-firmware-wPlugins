/*
 * Purpose: Classify paddle startup shorts and safety states.
 * Owns: probe state derivation and straight-key fallback rules.
 * Depends on: morse_flipper_gpio_probe.h.
 * Tests: tests/test_gpio_probe.c.
 */

#ifdef MORSE_FLIPPER_FAP
#include "morse_flipper_app_i.h"
#else
#include "morse_flipper_gpio_probe.h"
#endif

uint8_t morse_flipper_gpio_probe_paddle(bool dit_down, bool dah_down) {
    if(dit_down && dah_down) {
        return MorseFlipperGpioProbeGroundToBoth;
    }

    if(dit_down) {
        return MorseFlipperGpioProbeGroundToDit;
    }

    if(dah_down) {
        return MorseFlipperGpioProbeGroundToDah;
    }

    return MorseFlipperGpioProbeOk;
}

bool morse_flipper_gpio_probe_any_short(uint8_t state) {
    return state != MorseFlipperGpioProbeOk;
}

bool morse_flipper_gpio_probe_forces_straight(uint8_t state) {
    return state == MorseFlipperGpioProbeGroundToDah;
}

bool morse_flipper_gpio_probe_blocks(uint8_t state) {
    return state == MorseFlipperGpioProbeGroundToDit || state == MorseFlipperGpioProbeGroundToBoth;
}

#ifdef MORSE_FLIPPER_FAP
bool morse_flipper_gpio_probe_screen(const MorseFlipperApp* app) {
    if(app == NULL) return false;
    return app->screen == MorseFlipperScreenSession || app->screen == MorseFlipperScreenStraight ||
           app->screen == MorseFlipperScreenTxGroups;
}

bool morse_flipper_gpio_probe_keep_state(uint8_t screen) {
    return screen == MorseFlipperScreenSession || screen == MorseFlipperScreenStraight ||
           screen == MorseFlipperScreenTxGroups || screen == MorseFlipperScreenTxGroupsResult ||
           screen == MorseFlipperScreenTxGroupsFinal;
}

static bool morse_flipper_gpio_probe_before_start(const MorseFlipperApp* app) {
    if(app == NULL) return false;
    if(app->screen == MorseFlipperScreenSession) return !app->session_started;
    if(app->screen == MorseFlipperScreenStraight) return !app->straight_started;
    if(app->screen == MorseFlipperScreenTxGroups) return !app->txg_started;
    return false;
}

static uint8_t morse_flipper_gpio_probe_sample(const MorseFlipperApp* app) {
    if(app == NULL || app->input_source != MorseFlipperInputSourcePaddle ||
       !morse_flipper_gpio_probe_screen(app)) {
        return MorseFlipperGpioProbeOk;
    }

    return morse_flipper_gpio_probe_sample_raw((MorseFlipperApp*)app);
}

uint8_t morse_flipper_gpio_probe_sample_raw(MorseFlipperApp* app) {
    bool dit_low;
    bool dah_low;
    bool dit_follows = true;
    bool dah_follows = true;

    if(app == NULL) {
        return MorseFlipperGpioProbeOk;
    }

    furi_hal_gpio_init(morse_flipper_dit_pin, GpioModeInput, GpioPullUp, GpioSpeedLow);
    furi_hal_gpio_init(morse_flipper_dah_pin, GpioModeInput, GpioPullUp, GpioSpeedLow);
    furi_delay_us(20U);

    dit_low = !furi_hal_gpio_read(morse_flipper_dit_pin);
    dah_low = !furi_hal_gpio_read(morse_flipper_dah_pin);
    if(dit_low || dah_low || morse_flipper_ground_pin == NULL) {
        morse_flipper_gpio_apply(app);
        return morse_flipper_gpio_probe_paddle(dit_low, dah_low);
    }

    furi_hal_gpio_init(morse_flipper_ground_pin, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    for(uint8_t i = 0U; i < 3U; i++) {
        furi_hal_gpio_write(morse_flipper_ground_pin, false);
        furi_delay_us(10U);
        if(furi_hal_gpio_read(morse_flipper_dit_pin)) dit_follows = false;
        if(furi_hal_gpio_read(morse_flipper_dah_pin)) dah_follows = false;

        furi_hal_gpio_write(morse_flipper_ground_pin, true);
        furi_delay_us(10U);
        if(!furi_hal_gpio_read(morse_flipper_dit_pin)) dit_follows = false;
        if(!furi_hal_gpio_read(morse_flipper_dah_pin)) dah_follows = false;
    }

    morse_flipper_gpio_apply(app);
    return morse_flipper_gpio_probe_paddle(dit_follows, dah_follows);
}

void morse_flipper_gpio_probe_reset(MorseFlipperApp* app) {
    if(app == NULL) return;
    app->gpio_probe_state = MorseFlipperGpioProbeOk;
    app->gpio_probe_notice_until = 0U;
}

void morse_flipper_gpio_probe_prepare(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;

    if(!morse_flipper_gpio_probe_screen(app)) return;
    if(!morse_flipper_gpio_probe_before_start(app)) return;

    morse_flipper_gpio_probe_reset(app);
    app->gpio_probe_state = morse_flipper_gpio_probe_sample(app);
    if(app->gpio_probe_state == MorseFlipperGpioProbeGroundToDah) {
        app->gpio_probe_notice_until = now_ms + 1000U;
    }
}

void morse_flipper_gpio_probe_tick(MorseFlipperApp* app, uint32_t now_ms) {
    uint8_t state;

    if(app == NULL) return;

    if(app->gpio_probe_notice_until != 0U && now_ms >= app->gpio_probe_notice_until) {
        app->gpio_probe_notice_until = 0U;
        morse_flipper_view_dirty(app);
    }

    if(!morse_flipper_gpio_probe_screen(app) || !morse_flipper_gpio_probe_before_start(app) ||
       app->input_source != MorseFlipperInputSourcePaddle) {
        return;
    }

    if(!morse_flipper_gpio_probe_blocks(app->gpio_probe_state)) return;

    state = morse_flipper_gpio_probe_sample(app);
    if(state == app->gpio_probe_state) return;

    app->gpio_probe_state = state;
    if(app->gpio_probe_state == MorseFlipperGpioProbeGroundToDah) {
        app->gpio_probe_notice_until = now_ms + 1000U;
    } else {
        app->gpio_probe_notice_until = 0U;
    }
    morse_flipper_view_dirty(app);
}

bool morse_flipper_gpio_probe_notice_active(const MorseFlipperApp* app) {
    return app != NULL && morse_flipper_gpio_probe_screen(app) &&
           morse_flipper_gpio_probe_before_start(app) && app->gpio_probe_notice_until != 0U &&
           furi_get_tick() < app->gpio_probe_notice_until;
}

bool morse_flipper_gpio_probe_blocks_start(const MorseFlipperApp* app) {
    return app != NULL && app->input_source == MorseFlipperInputSourcePaddle &&
           morse_flipper_gpio_probe_screen(app) && morse_flipper_gpio_probe_before_start(app) &&
           morse_flipper_gpio_probe_blocks(app->gpio_probe_state);
}

bool morse_flipper_gpio_probe_use_straight(const MorseFlipperApp* app) {
    return app != NULL && app->input_source == MorseFlipperInputSourcePaddle &&
           morse_flipper_gpio_probe_screen(app) &&
           morse_flipper_gpio_probe_forces_straight(app->gpio_probe_state);
}
#endif
