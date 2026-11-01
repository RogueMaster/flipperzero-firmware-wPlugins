// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 ZeroNinx

#pragma once

#include "taiko_lane.h"
#include "taiko_menu.h"
#include <furi/core/string.h>
#include <gui/canvas.h>

typedef struct {
    char title[128];
    char course[24];
    uint32_t position_ms;
    uint32_t bpm_tenths;
    uint32_t hit_until;
    uint32_t target_hit_until;
    bool hit_right;
    bool hit_rim;
    bool loading;
    bool finished;
    bool paused;
    bool gogo;
    uint8_t volume;
    size_t lane_count;
    TaikoLaneNote lane_notes[TAIKO_LANE_MAX_NOTES];
    uint8_t bar_columns[TAIKO_LANE_BAR_BYTES];
    size_t roll_count;
    TaikoLaneRoll lane_rolls[TAIKO_LANE_MAX_ROLLS];
    TaikoLaneBalloon balloon;
    TaikoMenu menu;
} TaikoPlayerView;

/* text is caller-owned scratch storage, used only by the drawing thread. */
void taiko_player_view_draw(Canvas* canvas, const TaikoPlayerView* view, FuriString* text);
