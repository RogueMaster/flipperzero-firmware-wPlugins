// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 ZeroNinx

#include "taiko_settings.h"
#include "taiko_mixer.h"

#include <ctype.h>
#include <string.h>

void taiko_settings_capture(TaikoSettings* settings, const TaikoMenu* menu, uint8_t volume) {
    *settings = (TaikoSettings){
        .volume = volume,
        .don_volume = menu->don_volume,
        .ka_volume = menu->ka_volume,
        .backward_ms = taiko_menu_backward_ms(menu),
        .forward_ms = taiko_menu_forward_ms(menu),
        .seek_mode = menu->seek_mode,
        .backward_bars = taiko_menu_bar_count(menu, false),
        .forward_bars = taiko_menu_bar_count(menu, true),
        .playback_tenths = taiko_menu_playback_tenths(menu),
        .scroll_tenths = taiko_menu_scroll_tenths(menu),
        .frame_rate = menu->frame_rate,
        .gpio_pulse_ms = menu->gpio.pulse_ms};
    memcpy(settings->pins, menu->gpio.pins, sizeof(settings->pins));
}

void taiko_settings_defaults(TaikoSettings* settings) {
    TaikoMenu menu;
    taiko_menu_init(&menu);
    taiko_settings_capture(settings, &menu, TAIKO_DEFAULT_VOLUME);
}

static bool valid_volume(uint32_t volume, uint32_t maximum) {
    return volume <= maximum && volume % 10 == 0;
}

bool taiko_settings_validate(TaikoSettings* settings) {
    TaikoSettings defaults;
    taiko_settings_defaults(&defaults);
    bool valid = true;
    uint32_t* volumes[] = {&settings->volume, &settings->don_volume, &settings->ka_volume};
    const uint32_t volume_limits[] = {TAIKO_MASTER_VOLUME_MAX, 100, 100};
    for(size_t i = 0; i < sizeof(volumes) / sizeof(volumes[0]); i++) {
        if(!valid_volume(*volumes[i], volume_limits[i])) {
            *volumes[i] = TAIKO_DEFAULT_VOLUME;
            valid = false;
        }
    }
    TaikoMenu menu;
    taiko_menu_init(&menu);
    const TaikoMenuItem items[] = {
        TaikoMenuBackward,
        TaikoMenuForward,
        TaikoMenuSeekMode,
        TaikoMenuPlaybackSpeed,
        TaikoMenuScrollSpeed,
        TaikoMenuFrameRate};
    uint32_t* values[] = {
        &settings->backward_ms,
        &settings->forward_ms,
        &settings->seek_mode,
        &settings->playback_tenths,
        &settings->scroll_tenths,
        &settings->frame_rate};
    const uint32_t fallback[] = {
        defaults.backward_ms,
        defaults.forward_ms,
        defaults.seek_mode,
        defaults.playback_tenths,
        defaults.scroll_tenths,
        defaults.frame_rate};
    for(size_t i = 0; i < sizeof(items) / sizeof(items[0]); i++) {
        if(!taiko_menu_set_value(&menu, items[i], *values[i])) {
            *values[i] = fallback[i];
            valid = false;
        }
    }
    if(!taiko_gpio_pulse_valid(settings->gpio_pulse_ms)) {
        settings->gpio_pulse_ms = defaults.gpio_pulse_ms;
        valid = false;
    }
    if(!taiko_menu_set_bar_count(&menu, false, settings->backward_bars)) {
        settings->backward_bars = defaults.backward_bars;
        valid = false;
    }
    if(!taiko_menu_set_bar_count(&menu, true, settings->forward_bars)) {
        settings->forward_bars = defaults.forward_bars;
        valid = false;
    }
    TaikoGpioConfig gpio = taiko_gpio_default_config();
    memcpy(gpio.pins, settings->pins, sizeof(gpio.pins));
    if(!taiko_gpio_config_valid(&gpio)) {
        memcpy(settings->pins, defaults.pins, sizeof(settings->pins));
        valid = false;
    }
    return valid;
}

void taiko_settings_apply(const TaikoSettings* settings, TaikoMenu* menu, uint8_t* volume) {
    TaikoSettings valid = *settings;
    taiko_settings_validate(&valid);
    *volume = valid.volume;
    menu->don_volume = valid.don_volume;
    menu->ka_volume = valid.ka_volume;
    taiko_menu_set_value(menu, TaikoMenuBackward, valid.backward_ms);
    taiko_menu_set_value(menu, TaikoMenuForward, valid.forward_ms);
    taiko_menu_set_value(menu, TaikoMenuSeekMode, valid.seek_mode);
    taiko_menu_set_bar_count(menu, false, valid.backward_bars);
    taiko_menu_set_bar_count(menu, true, valid.forward_bars);
    taiko_menu_set_value(menu, TaikoMenuPlaybackSpeed, valid.playback_tenths);
    taiko_menu_set_value(menu, TaikoMenuScrollSpeed, valid.scroll_tenths);
    taiko_menu_set_value(menu, TaikoMenuFrameRate, valid.frame_rate);
    menu->gpio.pulse_ms = valid.gpio_pulse_ms;
    memcpy(menu->gpio.pins, valid.pins, sizeof(menu->gpio.pins));
    menu->gpio.enabled = false;
}

bool taiko_settings_equal(const TaikoSettings* a, const TaikoSettings* b) {
    return a->volume == b->volume && a->don_volume == b->don_volume &&
           a->ka_volume == b->ka_volume && a->backward_ms == b->backward_ms &&
           a->forward_ms == b->forward_ms && a->playback_tenths == b->playback_tenths &&
           a->seek_mode == b->seek_mode && a->backward_bars == b->backward_bars &&
           a->forward_bars == b->forward_bars && a->scroll_tenths == b->scroll_tenths &&
           a->frame_rate == b->frame_rate && a->gpio_pulse_ms == b->gpio_pulse_ms &&
           memcmp(a->pins, b->pins, sizeof(a->pins)) == 0;
}

bool taiko_settings_parse_uint(const char* text, uint32_t* value) {
    while(isspace((unsigned char)*text))
        text++;
    if(!isdigit((unsigned char)*text)) return false;
    uint32_t result = 0;
    while(isdigit((unsigned char)*text)) {
        unsigned digit = *text++ - '0';
        if(result > (UINT32_MAX - digit) / 10) return false;
        result = result * 10 + digit;
    }
    while(isspace((unsigned char)*text))
        text++;
    if(*text) return false;
    *value = result;
    return true;
}

bool taiko_settings_parse_pin(const char* text, TaikoGpioPin* pin) {
    for(unsigned i = 0; i < TaikoGpioPinCount; i++) {
        if(strcmp(text, taiko_gpio_pin_name(i)) == 0) {
            *pin = i;
            return true;
        }
    }
    return false;
}
