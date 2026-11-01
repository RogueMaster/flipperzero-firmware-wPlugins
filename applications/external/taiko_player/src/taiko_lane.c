// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 ZeroNinx

#include "taiko_lane.h"
#include "taiko_hits.h"

#include <math.h>
#include <string.h>

void taiko_lane_collect_bars(
    const TjaChart* chart,
    uint32_t position_ms,
    uint8_t scroll_tenths,
    uint8_t columns[TAIKO_LANE_BAR_BYTES]) {
    memset(columns, 0, TAIKO_LANE_BAR_BYTES);
    if(!scroll_tenths || !isfinite(chart->min_bar_speed) || chart->min_bar_speed <= 0) return;
    float window = (128 - TAIKO_LANE_HIT_X) * 10000.0f /
                   (TAIKO_LANE_PIXELS_PER_SECOND * scroll_tenths * chart->min_bar_speed);
    int64_t span = window < UINT32_MAX ? (int64_t)window + 1 : (int64_t)UINT32_MAX;
    int64_t latest = (int64_t)position_ms + span;
    size_t low = 0, high = chart->bar_count;
    while(low < high) {
        size_t mid = low + (high - low) / 2;
        if(chart->bars[mid].time_ms < (int64_t)position_ms)
            low = mid + 1;
        else
            high = mid;
    }
    for(size_t i = low; i < chart->bar_count && chart->bars[i].time_ms <= latest; i++) {
        const TjaBar* bar = &chart->bars[i];
        if(!bar->scroll_speed) continue;
        float offset = ((int64_t)bar->time_ms - position_ms) * bar->scroll_speed * scroll_tenths *
                       (TAIKO_LANE_PIXELS_PER_SECOND / 10000.0f);
        if(!isfinite(offset) || offset <= -TAIKO_LANE_HIT_X - 1 ||
           offset >= 128 - TAIKO_LANE_HIT_X)
            continue;
        int x = TAIKO_LANE_HIT_X + (int)offset;
        if(x >= 0 && x < 128) columns[x / 8] |= 1U << (x % 8);
    }
}

size_t taiko_lane_collect(
    const TjaNote* notes,
    size_t count,
    float min_note_speed,
    uint32_t position_ms,
    uint8_t scroll_tenths,
    TaikoLaneNote* visible,
    size_t capacity) {
    if(!scroll_tenths || !capacity || !count) return 0;
    if(!isfinite(min_note_speed) || min_note_speed <= 0) return 0;
    /* Reverse-scroll notes can enter from the left too; bound their lookahead.
     * Cap before casting: very small positive HS can produce an infinite window. */
    float window = (127 + TAIKO_LANE_BIG_RADIUS - TAIKO_LANE_HIT_X + 1) * 10000.0f /
                   (TAIKO_LANE_PIXELS_PER_SECOND * scroll_tenths * min_note_speed);
    int64_t span = window < UINT32_MAX ? (int64_t)window + 1 : (int64_t)UINT32_MAX;
    const int64_t latest = (int64_t)position_ms + span;
    size_t low = 0, high = count;
    while(low < high) {
        size_t mid = low + (high - low) / 2;
        if(notes[mid].time_ms <= (int64_t)position_ms)
            low = mid + 1;
        else
            high = mid;
    }
    size_t result = 0;
    for(size_t i = low; i < count && result < capacity; i++) {
        int64_t delta = (int64_t)notes[i].time_ms - position_ms;
        if(notes[i].time_ms > latest) break;
        uint8_t kind = notes[i].kind;
        if(kind < 1 || kind > 4) continue;
        float speed = notes[i].scroll_speed ? notes[i].scroll_speed : 1;
        float offset =
            (float)delta * speed * scroll_tenths * (TAIKO_LANE_PIXELS_PER_SECOND / 10000.0f);
        int radius = kind >= 3 ? TAIKO_LANE_BIG_RADIUS : TAIKO_LANE_SMALL_RADIUS;
        if(!isfinite(offset) || offset <= -TAIKO_LANE_HIT_X - radius - 1 ||
           offset >= 127 + radius - TAIKO_LANE_HIT_X + 1)
            continue;
        int x = TAIKO_LANE_HIT_X + (int)offset;
        if(x < -radius || x > 127 + radius) continue;
        visible[result++] = (TaikoLaneNote){.x = (int16_t)x, .kind = kind};
    }
    return result;
}

static int16_t clipped_x(float offset) {
    /* Long tails may extend far beyond the display; clip before integer conversion. */
    if(offset < -256) return -256;
    if(offset > 256) return 256;
    return TAIKO_LANE_HIT_X + (int16_t)offset;
}

size_t taiko_lane_collect_rolls(
    const TjaChart* chart,
    uint32_t position_ms,
    uint8_t scroll_tenths,
    TaikoLaneRoll* visible,
    size_t capacity,
    TaikoLaneBalloon* balloon) {
    *balloon = (TaikoLaneBalloon){0};
    size_t count = 0;
    for(size_t i = 0; i < chart->roll_count; i++) {
        const TjaRoll* roll = &chart->rolls[i];
        bool is_balloon = roll->balloon_count != 0;
        if(is_balloon && (int64_t)position_ms >= roll->end_ms) continue;
        bool active = (int64_t)position_ms >= roll->time_ms;
        if(is_balloon && active) {
            balloon->remaining = taiko_balloon_remaining(chart, roll, position_ms);
            balloon->total = roll->balloon_count;
            balloon->direction = roll->scroll_speed < 0 ? -1 : 1;
            continue;
        }
        float speed = roll->scroll_speed ? roll->scroll_speed : 1;
        float scale = speed * scroll_tenths * (TAIKO_LANE_PIXELS_PER_SECOND / 10000.0f);
        float head = ((int64_t)roll->time_ms - position_ms) * scale;
        float tail = is_balloon ? head + (speed < 0 ? -18 : 18) :
                                  ((int64_t)roll->end_ms - position_ms) * scale;
        if(!isfinite(head) || !isfinite(tail)) continue;
        float left = head < tail ? head : tail;
        float right = head > tail ? head : tail;
        if(left > 127 + TAIKO_LANE_BIG_RADIUS - TAIKO_LANE_HIT_X ||
           right < -TAIKO_LANE_HIT_X - TAIKO_LANE_BIG_RADIUS)
            continue;
        if(count < capacity)
            visible[count++] = (TaikoLaneRoll){
                .x = clipped_x(head), .tail_x = clipped_x(tail), .kind = roll->kind};
    }
    return count;
}
