// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 ZeroNinx

#include "tja_parser.h"
#include "taiko_memory.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t free_heap, largest_block;
static unsigned fail_bar_at;

size_t memmgr_get_free_heap(void) {
    return free_heap;
}

size_t memmgr_heap_get_max_free_block(void) {
    return largest_block;
}

static int read_line(void* context, char* line, size_t capacity) {
    if(!fgets(line, capacity, context)) return 0;
    if(fail_bar_at && strcmp(line, ",\n") == 0 && !--fail_bar_at) largest_block = 32;
    return 1;
}

static FILE* large_chart(void) {
    FILE* file = tmpfile();
    assert(file);
    fputs("BPM:120\nWAVE:silence.wav\n#START\n", file);
    for(unsigned measure = 0; measure < 24; measure++) {
        for(unsigned note = 0; note < 64; note++)
            fputc('1' + note % 4, file);
        fputs(",\n", file);
    }
    fputs("#END\n", file);
    rewind(file);
    return file;
}

int main(void) {
    /* The shared preflight rejects both exhausted and fragmented heaps, while
     * allowing allocations exactly on the reserve / allocator-overhead limit. */
    free_heap = 1088;
    largest_block = 96;
    assert(taiko_memory_available(64, 1024));
    void* allocation = taiko_alloc(64, 1024);
    assert(allocation);
    free(allocation);
    assert(!taiko_alloc(65, 1024));
    largest_block = 95;
    assert(!taiko_alloc(64, 1024));
    largest_block = 31;
    assert(!taiko_alloc(1, 0));
    assert(!taiko_alloc(0, 0));
    free_heap = 0;
    largest_block = 100000;
    assert(!taiko_alloc(1, 0));
    free_heap = (size_t)-1;
    largest_block = (size_t)-1;
    assert(!taiko_memory_available(64, (size_t)-1));

    const char* error;
    TjaChart chart;
    FILE* file = large_chart();
    /* Total exhaustion is rejected before allocating parser scratch space. */
    free_heap = 0;
    largest_block = 100000;
    assert(!tja_parse(read_line, file, &chart, &error));
    assert(strcmp(error, "Not enough memory") == 0 && !chart.notes);
    tja_chart_free(&chart);

    rewind(file);
    free_heap = 4096;
    assert(!tja_parse(read_line, file, &chart, &error));
    tja_chart_free(&chart);
    rewind(file);
    uint8_t courses;
    assert(!tja_list_courses(read_line, file, &courses, &error));
    assert(strcmp(error, "Not enough memory") == 0);

    /* Plenty of total memory does not imply a large enough contiguous block.
     * Failure while growing the array must preserve its old allocation. */
    free_heap = 100000;
    largest_block = 16000;
    rewind(file);
    assert(!tja_parse(read_line, file, &chart, &error));
    assert(strcmp(error, "Not enough memory") == 0);
    assert(chart.note_count == 1024 && chart.notes);
    for(size_t i = 0; i < chart.note_count; i++)
        assert(chart.notes[i].kind == 1 + i % 4);
    tja_chart_free(&chart);

    /* Retrying after releasing a chart succeeds, without losing old records
     * as the arrays grow. Repeat to catch leaks under the sanitizers. */
    largest_block = 100000;
    for(unsigned attempt = 0; attempt < 5; attempt++) {
        rewind(file);
        assert(tja_parse(read_line, file, &chart, &error));
        assert(!error && chart.note_count == 1536);
        for(size_t i = 0; i < chart.note_count; i++)
            assert(chart.notes[i].kind == 1 + i % 4);
        tja_chart_free(&chart);
    }
    fclose(file);

    /* Fail the initial bar allocation and both growth steps after consuming
     * parser scratch space. Old bar records remain owned and safe to free. */
    file = tmpfile();
    assert(file);
    fputs("BPM:120\nWAVE:a.wav\n#START\n", file);
    for(unsigned i = 0; i < 65; i++)
        fputs(",\n", file);
    fputs("#END\n", file);
    const unsigned failures[] = {1, 33, 65};
    for(unsigned i = 0; i < sizeof(failures) / sizeof(failures[0]); i++) {
        rewind(file);
        free_heap = largest_block = 100000;
        fail_bar_at = failures[i];
        assert(!tja_parse(read_line, file, &chart, &error));
        assert(strcmp(error, "Not enough memory") == 0);
        assert(chart.bar_count == failures[i] - 1);
        for(size_t j = 0; j < chart.bar_count; j++)
            assert(chart.bars[j].time_ms == (int32_t)j * 2000);
        tja_chart_free(&chart);
        assert(!chart.bars && !chart.bar_count);
    }
    rewind(file);
    free_heap = largest_block = 100000;
    assert(tja_parse(read_line, file, &chart, &error));
    assert(chart.bar_count == 65);
    tja_chart_free(&chart);
    fclose(file);

    file = tmpfile();
    assert(file);
    fputs("BPM:300\nWAVE:a.wav\n#START\n5008,\n#END\n", file);
    rewind(file);
    assert(tja_parse(read_line, file, &chart, &error));
    fclose(file);
    TjaRoll* rolls = chart.rolls;
    TjaHitRun* runs = chart.hit_runs;
    const uint32_t count = chart.rolls[0].hit_count;
    free_heap = 0;
    assert(tja_chart_set_roll_speed(&chart, 10, &error));
    assert(!tja_chart_set_roll_speed(&chart, 20, &error));
    assert(strcmp(error, "Not enough memory") == 0);
    assert(chart.rolls == rolls && chart.hit_runs == runs && chart.roll_speed_tenths == 10);
    assert(chart.rolls[0].hit_count == count);
    free_heap = 100000;
    assert(tja_chart_set_roll_speed(&chart, 20, &error));
    assert(chart.rolls[0].hit_count == 10);
    tja_chart_free(&chart);
    puts(
        "Chart allocation exhaustion, fragmentation, retry and speed-change rollback tests passed");
    return 0;
}
