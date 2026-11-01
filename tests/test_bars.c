// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 ZeroNinx

#include "taiko_lane.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int read_line(void* context, char* line, size_t capacity) {
    return fgets(line, capacity, context) != NULL;
}

static TjaChart parse(const char* text, TjaCourse course) {
    FILE* file = tmpfile();
    assert(file);
    fputs(text, file);
    rewind(file);
    TjaChart chart;
    const char* error;
    assert(tja_parse_course(read_line, file, course, &chart, &error));
    assert(!error);
    fclose(file);
    return chart;
}

static bool has_column(const uint8_t* columns, unsigned x) {
    return (columns[x / 8] & (1U << (x % 8))) != 0;
}

static void test_timing_and_seek(void) {
    TjaChart chart = parse(
        "BPM:120\nOFFSET:0.5\nWAVE:a.wav\nCOURSE:Easy\n#START\n1,\n#END\n"
        "COURSE:Oni\n#START\n11,\n#BARLINEOFF\n#BPMCHANGE 240\n,\n"
        "#MEASURE 3/4\n#BARLINEON\n#SCROLL 2\n#DELAY 0.25\n100,\n"
        "#BPMCHANGE 120\n1\n#BPMCHANGE 240\n1,\n#END\n",
        TjaCourseOni);
    const int times[] = {-500, 1500, 2750, 3500};
    assert(chart.bar_count == 4 && chart.end_ms == 4625);
    for(size_t i = 0; i < chart.bar_count; i++)
        assert(chart.bars[i].time_ms == times[i]);
    assert(chart.bars[0].scroll_speed == 0.5f && chart.bars[1].scroll_speed == 0);
    assert(chart.bars[2].scroll_speed == 2 && chart.bars[3].scroll_speed == 1);
    assert(chart.min_bar_speed == 0.5f);
    assert(tja_chart_seek_bars(&chart, 0, 6000, true, 1) == 1500); /* Hidden still counts. */
    assert(tja_chart_seek_bars(&chart, 1499, 6000, true, 1) == 1500);
    assert(tja_chart_seek_bars(&chart, 1500, 6000, true, 1) == 2750);
    assert(tja_chart_seek_bars(&chart, 1501, 6000, false, 1) == 1500);
    assert(tja_chart_seek_bars(&chart, 1500, 6000, false, 1) == 0);
    assert(tja_chart_seek_bars(&chart, 0, 6000, true, 3) == 3500);
    assert(tja_chart_seek_bars(&chart, 3500, 6000, true, 1) == 6000);
    assert(tja_chart_seek_bars(&chart, 6000, 6000, false, 1) == 3500);
    assert(tja_chart_seek_bars(&chart, 3500, 6000, false, 2) == 1500);
    assert(tja_chart_seek_bars(&chart, 1500, 2000, true, 1) == 2000);
    assert(tja_chart_seek_bars(&chart, 10, 6000, false, 10) == 0);
    assert(tja_chart_seek_bars(&chart, 2750, 6000, true, 0) == 2750);
    tja_chart_free(&chart);
    assert(!chart.bars && !chart.bar_count);

    chart = parse(
        "WAVE:a.wav\n#START\n1\n#BARLINEOFF\n1,\n1\n#BARLINEON\n1,\n,\n#END\n", TjaCourseOni);
    assert(chart.bar_count == 3);
    assert(chart.bars[0].scroll_speed == 0.5f && chart.bars[1].scroll_speed == 0);
    assert(chart.bars[2].scroll_speed == 0.5f);
    tja_chart_free(&chart);

    chart = parse("WAVE:a.wav\n#START\n#DELAY 1\n#END\n", TjaCourseOni);
    assert(!chart.bar_count); /* Commands alone do not create a phantom measure. */
    assert(tja_chart_seek_bars(&chart, 500, 1000, true, 1) == 1000);
    assert(tja_chart_seek_bars(&chart, 500, 1000, false, 1) == 0);
    tja_chart_free(&chart);

    chart = parse("WAVE:a.wav\n#START\n#MEASURE 0.000001/4\n,,,,\n#END\n", TjaCourseOni);
    assert(chart.bar_count == 4 && chart.bars[3].time_ms == 0);
    assert(tja_chart_seek_bars(&chart, 0, 100, true, 1) == 100);
    assert(tja_chart_seek_bars(&chart, 1, 100, false, 1) == 0);
    tja_chart_free(&chart);
}

static void test_columns(void) {
    TjaBar bars[] = {
        {.time_ms = 0, .scroll_speed = 1},
        {.time_ms = 100, .scroll_speed = -1},
        {.time_ms = 200, .scroll_speed = 0},
        {.time_ms = 500, .scroll_speed = 1},
        {.time_ms = 500, .scroll_speed = 1},
        {.time_ms = 1000, .scroll_speed = 0.5f},
        {.time_ms = 1600, .scroll_speed = 0.25f},
        {.time_ms = 2000, .scroll_speed = FLT_MAX},
    };
    TjaChart chart = {.bars = bars, .bar_count = 8, .min_bar_speed = 0.25f};
    struct {
        uint8_t before;
        uint8_t columns[TAIKO_LANE_BAR_BYTES];
        uint8_t after;
    } output = {.before = 0xaa, .after = 0x55};
    taiko_lane_collect_bars(&chart, 0, 10, output.columns);
    for(unsigned x = 0; x < 128; x++)
        assert(has_column(output.columns, x) == (x == 18 || x == 6 || x == 82 || x == 69));
    taiko_lane_collect_bars(&chart, 500, 10, output.columns);
    assert(has_column(output.columns, 18) && has_column(output.columns, 50));
    assert(!has_column(output.columns, 82));
    taiko_lane_collect_bars(&chart, 500, 20, output.columns);
    assert(has_column(output.columns, 18) && has_column(output.columns, 82));
    taiko_lane_collect_bars(&chart, UINT32_MAX, 10, output.columns);
    for(unsigned x = 0; x < 128; x++)
        assert(!has_column(output.columns, x));
    chart.min_bar_speed = FLT_MIN;
    taiko_lane_collect_bars(&chart, 0, 40, output.columns); /* Overflow-safe lookahead. */
    assert(output.before == 0xaa && output.after == 0x55);
    chart.min_bar_speed = NAN;
    taiko_lane_collect_bars(&chart, 0, 10, output.columns);
    for(unsigned x = 0; x < 128; x++)
        assert(!has_column(output.columns, x));
    chart.min_bar_speed = 1;
    taiko_lane_collect_bars(&chart, 0, 0, output.columns);
    for(unsigned x = 0; x < 128; x++)
        assert(!has_column(output.columns, x));
}

static void test_measure_limit(void) {
    FILE* file = tmpfile();
    assert(file);
    fputs("BPM:10000\nWAVE:a.wav\n#START\n", file);
    for(unsigned i = 0; i <= 8192; i++)
        fputs(",\n", file);
    fputs("#END\n", file);
    rewind(file);
    TjaChart chart;
    const char* error;
    assert(!tja_parse(read_line, file, &chart, &error));
    assert(chart.bar_count == 8192 && strcmp(error, "Chart exceeds 8192 measures") == 0);
    assert(chart.bars[8191].time_ms > chart.bars[8190].time_ms);
    tja_chart_free(&chart);
    fclose(file);
}

int main(void) {
    test_timing_and_seek();
    test_columns();
    test_measure_limit();
    puts(
        "Measure timing, hidden/empty bars, boundary seeking and barline visibility tests passed");
    return 0;
}
