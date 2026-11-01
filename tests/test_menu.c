// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 ZeroNinx

#include "taiko_menu.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    TaikoMenu menu;
    taiko_menu_init(&menu);
    assert(!menu.open);
    assert(menu.seek_mode == TaikoSeekByBars);
    assert(taiko_menu_backward_ms(&menu) == 5000);
    assert(taiko_menu_forward_ms(&menu) == 5000);
    assert(menu.don_volume == 70 && menu.ka_volume == 70);
    assert(taiko_menu_playback_tenths(&menu) == 10);
    assert(taiko_menu_scroll_tenths(&menu) == 10);
    assert(menu.frame_rate == 50);
    assert(!menu.gpio.enabled && taiko_gpio_config_valid(&menu.gpio));
    assert(menu.gpio.pulse_ms == 70);
    assert(menu.gpio.pins[TaikoGpioLeftKa] == TaikoGpioPinA7);
    assert(menu.gpio.pins[TaikoGpioLeftDon] == TaikoGpioPinA6);
    assert(menu.gpio.pins[TaikoGpioRightDon] == TaikoGpioPinA4);
    assert(menu.gpio.pins[TaikoGpioRightKa] == TaikoGpioPinB3);
    assert(taiko_menu_activate(&menu) == TaikoMenuActionNone);

    taiko_menu_open(&menu);
    assert(menu.selected == TaikoMenuReturn);
    taiko_menu_move(&menu, true);
    assert(menu.selected == TaikoMenuRestart);
    taiko_menu_move(&menu, true);
    assert(menu.selected == TaikoMenuNewSong);
    taiko_menu_move(&menu, true);
    assert(menu.selected == TaikoMenuDifficulty);
    menu.courses = (1U << TjaCourseEasy) | (1U << TjaCourseHard) | (1U << TjaCourseOni);
    taiko_menu_adjust(&menu, false);
    assert(menu.course == TjaCourseHard);
    taiko_menu_adjust(&menu, false);
    assert(menu.course == TjaCourseEasy);
    taiko_menu_adjust(&menu, false);
    assert(menu.course == TjaCourseOni);
    taiko_menu_move(&menu, true);
    assert(menu.selected == TaikoMenuPlaybackSpeed);
    for(int i = 0; i < 5; i++)
        taiko_menu_adjust(&menu, false);
    for(uint8_t speed = 5; speed <= 20; speed++) {
        assert(taiko_menu_playback_tenths(&menu) == speed);
        assert(taiko_menu_scroll_tenths(&menu) == 10);
        taiko_menu_adjust(&menu, true);
    }
    assert(taiko_menu_playback_tenths(&menu) == 5);
    taiko_menu_adjust(&menu, false);
    assert(taiko_menu_playback_tenths(&menu) == 20);
    taiko_menu_move(&menu, true);
    assert(menu.selected == TaikoMenuScrollSpeed);
    for(int i = 0; i < 5; i++)
        taiko_menu_adjust(&menu, false);
    assert(taiko_menu_scroll_tenths(&menu) == 5);
    const uint8_t scroll_expected[] = {5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
                                       17, 18, 19, 20, 21, 22, 23, 24, 25, 30, 40};
    for(size_t i = 0; i < sizeof(scroll_expected); i++) {
        assert(taiko_menu_scroll_tenths(&menu) == scroll_expected[i]);
        assert(taiko_menu_playback_tenths(&menu) == 20);
        taiko_menu_adjust(&menu, true);
    }
    assert(taiko_menu_scroll_tenths(&menu) == 5);
    taiko_menu_adjust(&menu, false);
    assert(taiko_menu_scroll_tenths(&menu) == 40);
    taiko_menu_move(&menu, true);
    assert(menu.selected == TaikoMenuFrameRate);
    assert(taiko_menu_set_value(&menu, TaikoMenuFrameRate, 60));
    taiko_menu_adjust(&menu, true);
    assert(menu.frame_rate == 15); /* Same wraparound behavior as speed settings. */
    for(unsigned rate = 15; rate <= 60; rate++) {
        assert(menu.frame_rate == rate);
        taiko_menu_adjust(&menu, true);
    }
    taiko_menu_adjust(&menu, false);
    assert(menu.frame_rate == 60);
    for(unsigned rate = 60; rate >= 15; rate--) {
        assert(menu.frame_rate == rate);
        taiko_menu_adjust(&menu, false);
    }
    assert(menu.frame_rate == 60);
    assert(taiko_menu_activate(&menu) == TaikoMenuActionNone);
    assert(menu.open && menu.frame_rate == 15);
    assert(taiko_menu_set_value(&menu, TaikoMenuFrameRate, 37));
    assert(!taiko_menu_set_value(&menu, TaikoMenuFrameRate, 14));
    assert(!taiko_menu_set_value(&menu, TaikoMenuFrameRate, 61));
    assert(!taiko_menu_set_value(&menu, TaikoMenuFrameRate, UINT32_MAX));
    assert(menu.frame_rate == 37);
    assert(taiko_menu_playback_tenths(&menu) == 20 && taiko_menu_scroll_tenths(&menu) == 40);
    taiko_menu_move(&menu, true);
    assert(menu.selected == TaikoMenuSeekMode && menu.seek_mode == TaikoSeekByBars);
    assert(taiko_menu_activate(&menu) == TaikoMenuActionNone);
    assert(menu.open && menu.seek_mode == TaikoSeekByTime);
    assert(taiko_menu_activate(&menu) == TaikoMenuActionNone);
    assert(menu.open && menu.seek_mode == TaikoSeekByBars);
    assert(taiko_menu_bar_count(&menu, false) == 1 && taiko_menu_bar_count(&menu, true) == 1);
    menu.selected = TaikoMenuBackward;
    taiko_menu_adjust(&menu, false);
    assert(taiko_menu_bar_count(&menu, false) == 10);
    taiko_menu_adjust(&menu, true);
    assert(taiko_menu_bar_count(&menu, false) == 1);
    const unsigned bars[] = {1, 3, 5, 10};
    for(size_t i = 0; i < sizeof(bars) / sizeof(bars[0]); i++) {
        assert(taiko_menu_bar_count(&menu, false) == bars[i]);
        assert(taiko_menu_bar_count(&menu, true) == 1);
        taiko_menu_adjust(&menu, true);
    }
    menu.selected = TaikoMenuForward;
    taiko_menu_adjust(&menu, true);
    assert(taiko_menu_bar_count(&menu, true) == 3);
    assert(taiko_menu_backward_ms(&menu) == 5000 && taiko_menu_forward_ms(&menu) == 5000);
    menu.selected = TaikoMenuSeekMode;
    taiko_menu_adjust(&menu, false);
    assert(menu.seek_mode == TaikoSeekByTime);
    assert(!taiko_menu_set_value(&menu, TaikoMenuSeekMode, 2));
    assert(!taiko_menu_set_bar_count(&menu, true, UINT32_MAX));
    assert(taiko_menu_bar_count(&menu, true) == 3);
    taiko_menu_move(&menu, true);
    assert(menu.selected == TaikoMenuBackward);
    taiko_menu_adjust(&menu, false);
    assert(taiko_menu_backward_ms(&menu) == 3000);
    taiko_menu_adjust(&menu, false);
    assert(taiko_menu_backward_ms(&menu) == 1000);
    taiko_menu_adjust(&menu, false);
    assert(taiko_menu_backward_ms(&menu) == 10000);
    assert(taiko_menu_forward_ms(&menu) == 5000);

    taiko_menu_move(&menu, true);
    assert(menu.selected == TaikoMenuForward);
    assert(taiko_menu_activate(&menu) == TaikoMenuActionNone);
    assert(menu.open);
    assert(taiko_menu_forward_ms(&menu) == 10000);
    taiko_menu_adjust(&menu, true);
    assert(taiko_menu_forward_ms(&menu) == 1000);
    assert(taiko_menu_backward_ms(&menu) == 10000);

    taiko_menu_move(&menu, true);
    assert(menu.selected == TaikoMenuDonVolume);
    taiko_menu_adjust(&menu, false);
    assert(menu.don_volume == 60 && menu.ka_volume == 70);
    for(int i = 0; i < 20; i++)
        taiko_menu_adjust(&menu, false);
    assert(menu.don_volume == 0);
    for(int i = 0; i < 20; i++)
        taiko_menu_adjust(&menu, true);
    assert(menu.don_volume == 100);
    taiko_menu_move(&menu, true);
    assert(menu.selected == TaikoMenuKaVolume);
    assert(taiko_menu_activate(&menu) == TaikoMenuActionNone);
    assert(menu.open && menu.ka_volume == 80 && menu.don_volume == 100);
    taiko_menu_move(&menu, true);
    assert(menu.selected == TaikoMenuGpioOutput);
    assert(!menu.gpio.enabled);
    taiko_menu_adjust(&menu, true);
    assert(menu.gpio.enabled);
    taiko_menu_adjust(&menu, false);
    assert(!menu.gpio.enabled);
    assert(taiko_menu_activate(&menu) == TaikoMenuActionNone);
    assert(menu.open && menu.gpio.enabled);
    taiko_menu_move(&menu, true);
    assert(menu.selected == TaikoMenuGpioSetting);
    assert(taiko_menu_activate(&menu) == TaikoMenuActionNone);
    assert(menu.open && menu.gpio_settings && menu.gpio_selected == TaikoGpioMenuReturn);
    taiko_menu_adjust(&menu, true);
    assert(menu.gpio_selected == TaikoGpioMenuReturn);
    taiko_menu_move(&menu, true);
    assert(menu.gpio_selected == TaikoGpioMenuLeftKa);
    taiko_menu_adjust(&menu, true);
    assert(menu.gpio.pins[TaikoGpioLeftKa] == TaikoGpioPinB2); /* Skip occupied pins. */
    taiko_menu_adjust(&menu, false);
    assert(menu.gpio.pins[TaikoGpioLeftKa] == TaikoGpioPinA7);
    taiko_menu_adjust(&menu, false);
    assert(menu.gpio.pins[TaikoGpioLeftKa] == TaikoGpioPinC0); /* Wrap backward. */
    for(unsigned output = 0; output < TaikoGpioOutputCount; output++) {
        TaikoGpioPin initial = menu.gpio.pins[output];
        unsigned visited = 0;
        for(unsigned i = 0; i < TaikoGpioPinCount - TaikoGpioOutputCount + 1; i++) {
            unsigned bit = 1U << menu.gpio.pins[output];
            assert(!(visited & bit));
            visited |= bit;
            assert(taiko_gpio_config_valid(&menu.gpio));
            taiko_gpio_config_adjust(&menu.gpio, output, true);
        }
        assert(menu.gpio.pins[output] == initial);
    }
    menu.gpio_selected = TaikoGpioMenuRightKa;
    taiko_menu_move(&menu, true);
    assert(menu.gpio_selected == TaikoGpioMenuPulseWidth);
    TaikoGpioConfig initial_gpio = menu.gpio;
    for(unsigned i = 0; i < 9; i++)
        taiko_menu_adjust(&menu, false);
    for(unsigned pulse = 25; pulse <= 100; pulse += 5) {
        assert(menu.gpio.pulse_ms == pulse);
        assert(taiko_gpio_config_valid(&menu.gpio));
        taiko_menu_adjust(&menu, true);
    }
    assert(menu.gpio.pulse_ms == 25);
    taiko_menu_adjust(&menu, false);
    assert(menu.gpio.pulse_ms == 100);
    assert(taiko_menu_activate(&menu) == TaikoMenuActionNone);
    assert(menu.open && menu.gpio_settings && menu.gpio.pulse_ms == 25);
    for(unsigned pulse = 100; pulse >= 25; pulse -= 5) {
        taiko_menu_adjust(&menu, false);
        assert(menu.gpio.pulse_ms == pulse);
    }
    for(unsigned i = 0; i < TaikoGpioOutputCount; i++)
        assert(menu.gpio.pins[i] == initial_gpio.pins[i]);
    assert(menu.gpio.enabled == initial_gpio.enabled);
    taiko_menu_move(&menu, true);
    assert(menu.gpio_selected == TaikoGpioMenuReturn);
    taiko_menu_move(&menu, false);
    assert(menu.gpio_selected == TaikoGpioMenuPulseWidth);
    assert(taiko_menu_back(&menu) == TaikoMenuActionNone);
    assert(menu.open && !menu.gpio_settings && menu.selected == TaikoMenuGpioSetting);
    assert(taiko_menu_activate(&menu) == TaikoMenuActionNone);
    assert(menu.gpio_selected == TaikoGpioMenuReturn);
    assert(taiko_menu_activate(&menu) == TaikoMenuActionNone); /* Return row stays in main menu. */
    assert(menu.open && !menu.gpio_settings);
    taiko_menu_move(&menu, true);
    assert(menu.selected == TaikoMenuRestoreDefaults);
    assert(taiko_menu_activate(&menu) == TaikoMenuActionRestoreDefaults);
    assert(menu.open && menu.selected == TaikoMenuRestoreDefaults);
    taiko_menu_move(&menu, true);
    assert(menu.selected == TaikoMenuExit);
    taiko_menu_adjust(&menu, true);
    assert(taiko_menu_forward_ms(&menu) == 1000);
    assert(menu.don_volume == 100 && menu.ka_volume == 80);
    taiko_menu_move(&menu, true);
    assert(menu.selected == TaikoMenuReturn);
    assert(taiko_menu_activate(&menu) == TaikoMenuActionReturn);
    assert(!menu.open);

    taiko_menu_open(&menu);
    assert(menu.selected == TaikoMenuReturn);
    assert(taiko_menu_backward_ms(&menu) == 10000);
    assert(taiko_menu_forward_ms(&menu) == 1000);
    taiko_menu_move(&menu, true);
    assert(taiko_menu_activate(&menu) == TaikoMenuActionRestart);
    assert(!menu.open);
    taiko_menu_open(&menu);
    taiko_menu_move(&menu, true);
    taiko_menu_move(&menu, true);
    assert(menu.selected == TaikoMenuNewSong);
    assert(taiko_menu_activate(&menu) == TaikoMenuActionNewSong);
    assert(!menu.open);
    assert(taiko_menu_backward_ms(&menu) == 10000);
    assert(taiko_menu_forward_ms(&menu) == 1000);
    assert(menu.don_volume == 100 && menu.ka_volume == 80);
    taiko_menu_open(&menu);
    assert(menu.frame_rate == 37);
    assert(!menu.gpio_settings && menu.gpio.enabled);
    assert(menu.gpio.pins[TaikoGpioLeftKa] == TaikoGpioPinC0);
    taiko_menu_move(&menu, false);
    assert(menu.selected == TaikoMenuExit);
    assert(taiko_menu_activate(&menu) == TaikoMenuActionExit);
    assert(!menu.open);
    taiko_menu_init(&menu);
    assert(menu.seek_mode == TaikoSeekByBars);
    assert(menu.frame_rate == 50);
    assert(taiko_menu_backward_ms(&menu) == 5000);
    assert(taiko_menu_forward_ms(&menu) == 5000);
    assert(menu.don_volume == 70 && menu.ka_volume == 70);
    assert(!menu.gpio.enabled && !menu.gpio_settings);
    assert(menu.gpio.pulse_ms == 70);
    assert(menu.gpio.pins[TaikoGpioLeftKa] == TaikoGpioPinA7);
    assert(taiko_menu_playback_tenths(&menu) == 10 && taiko_menu_scroll_tenths(&menu) == 10);
    puts(
        "Menu actions, difficulty, independent speeds, intervals, volume limits and defaults tests passed");
    return 0;
}
