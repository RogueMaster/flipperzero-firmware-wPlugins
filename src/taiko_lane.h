// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 ZeroNinx

#pragma once

#include "tja_parser.h"

#define TAIKO_LANE_MAX_NOTES         64U
#define TAIKO_LANE_MAX_ROLLS         16U
#define TAIKO_LANE_HIT_X             18
#define TAIKO_LANE_PIXELS_PER_SECOND 128
#define TAIKO_LANE_SMALL_RADIUS      4
#define TAIKO_LANE_BIG_RADIUS        6
#define TAIKO_LANE_BAR_BYTES         16U

typedef struct {
    int16_t x;
    uint8_t kind;
} TaikoLaneNote;

typedef struct {
    int16_t x;
    int16_t tail_x;
    uint8_t kind;
} TaikoLaneRoll;

typedef struct {
    uint32_t remaining;
    uint32_t total;
    int8_t direction;
} TaikoLaneBalloon;

/* Collect unhit visible normal notes (1..4) with their individual HS multipliers.
 * Notes are sorted by time, but may appear out of order at different speeds.
 * min_note_speed bounds the time window without missing slower notes. */
size_t taiko_lane_collect(
    const TjaNote* notes,
    size_t count,
    float min_note_speed,
    uint32_t position_ms,
    uint8_t scroll_tenths,
    TaikoLaneNote* visible,
    size_t capacity);

size_t taiko_lane_collect_rolls(
    const TjaChart* chart,
    uint32_t position_ms,
    uint8_t scroll_tenths,
    TaikoLaneRoll* visible,
    size_t capacity,
    TaikoLaneBalloon* balloon);

/* One bit per screen column; coincident barlines cost no extra view memory. */
void taiko_lane_collect_bars(
    const TjaChart* chart,
    uint32_t position_ms,
    uint8_t scroll_tenths,
    uint8_t columns[TAIKO_LANE_BAR_BYTES]);
