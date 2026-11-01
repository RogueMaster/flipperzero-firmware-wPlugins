// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 ZeroNinx

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "tja_parser.h"
#include "taiko_gpio_config.h"

#define TAIKO_MIN_FRAME_RATE     15U
#define TAIKO_MAX_FRAME_RATE     60U
#define TAIKO_DEFAULT_FRAME_RATE 50U

typedef enum {
    TaikoSeekByTime,
    TaikoSeekByBars,
} TaikoSeekMode;

typedef enum {
    TaikoMenuReturn,
    TaikoMenuRestart,
    TaikoMenuNewSong,
    TaikoMenuDifficulty,
    TaikoMenuPlaybackSpeed,
    TaikoMenuScrollSpeed,
    TaikoMenuFrameRate,
    TaikoMenuSeekMode,
    TaikoMenuBackward,
    TaikoMenuForward,
    TaikoMenuDonVolume,
    TaikoMenuKaVolume,
    TaikoMenuGpioOutput,
    TaikoMenuGpioSetting,
    TaikoMenuRestoreDefaults,
    TaikoMenuExit,
    TaikoMenuItemCount,
} TaikoMenuItem;

typedef enum {
    TaikoGpioMenuReturn,
    TaikoGpioMenuLeftKa,
    TaikoGpioMenuLeftDon,
    TaikoGpioMenuRightDon,
    TaikoGpioMenuRightKa,
    TaikoGpioMenuPulseWidth,
    TaikoGpioMenuItemCount,
} TaikoGpioMenuItem;

typedef enum {
    TaikoMenuActionNone,
    TaikoMenuActionReturn,
    TaikoMenuActionRestart,
    TaikoMenuActionNewSong,
    TaikoMenuActionRestoreDefaults,
    TaikoMenuActionExit,
} TaikoMenuAction;

typedef struct {
    bool open;
    TaikoMenuItem selected;
    bool gpio_settings;
    TaikoGpioMenuItem gpio_selected;
    TaikoGpioConfig gpio;
    uint8_t backward_index;
    uint8_t forward_index;
    TaikoSeekMode seek_mode;
    uint8_t backward_bar_index;
    uint8_t forward_bar_index;
    uint8_t don_volume;
    uint8_t ka_volume;
    uint8_t courses;
    TjaCourse course;
    uint8_t playback_index;
    uint8_t scroll_index;
    uint8_t frame_rate;
} TaikoMenu;

void taiko_menu_init(TaikoMenu* menu);
void taiko_menu_open(TaikoMenu* menu);
void taiko_menu_move(TaikoMenu* menu, bool down);
void taiko_menu_adjust(TaikoMenu* menu, bool forward);
TaikoMenuAction taiko_menu_activate(TaikoMenu* menu);
TaikoMenuAction taiko_menu_back(TaikoMenu* menu);
uint32_t taiko_menu_backward_ms(const TaikoMenu* menu);
uint32_t taiko_menu_forward_ms(const TaikoMenu* menu);
uint32_t taiko_menu_bar_count(const TaikoMenu* menu, bool forward);
bool taiko_menu_set_bar_count(TaikoMenu* menu, bool forward, uint32_t value);
uint8_t taiko_menu_playback_tenths(const TaikoMenu* menu);
uint8_t taiko_menu_scroll_tenths(const TaikoMenu* menu);
/* Select by the persisted value rather than the menu table index. */
bool taiko_menu_set_value(TaikoMenu* menu, TaikoMenuItem item, uint32_t value);
