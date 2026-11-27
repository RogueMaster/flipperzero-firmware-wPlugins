#include <cassert>
#include <cstdio>
#include <cstring>
#include "loop_health.h"

// One loop() pass the way main.cpp marks it, each stage taking the given
// microseconds; returns the clock at its end.
static uint32_t
    pass(LoopHealth& h, uint32_t t, uint32_t can, uint32_t logic, uint32_t logs, uint32_t web) {
    h.begin_pass(t);
    t += can;
    h.mark(LOOP_STAGE_CAN_RX, t);
    t += logic;
    h.mark(LOOP_STAGE_LOGIC, t);
    t += 0;
    h.mark(LOOP_STAGE_CAN_INIT, t);
    t += logs;
    h.mark(LOOP_STAGE_LOGS, t);
    t += web;
    h.mark(LOOP_STAGE_WEB, t);
    return t;
}

static void test_passes() {
    LoopHealth h;
    // The first pass is only measured when the second begins.
    uint32_t t = pass(h, 1000, 300, 50, 20, 100);
    assert(h.boot.max_us == 0 && h.boot.slow == 0);
    t = pass(h, t, 200, 50, 20, 300);
    assert(h.boot.max_us == 470 && h.boot.max_stage == LOOP_STAGE_CAN_RX);
    assert(h.boot.slow == 0);

    // A page load: 400 ms in the web stage.
    t = pass(h, t, 500, 50, 20, 400000);
    t = pass(h, t, 900, 50, 20, 100);
    assert(h.boot.max_us == 400570 && h.boot.max_stage == LOOP_STAGE_WEB);
    assert(h.boot.slow == 1);

    // Time outside loop() is "other": 30 ms before the next pass starts.
    t = pass(h, t + 30000, 100, 50, 20, 100);
    assert(h.boot.slow == 2 && h.boot.max_stage == LOOP_STAGE_WEB);
    assert(h.since.slow == 2);

    // The CAN counter reset starts the second window; boot keeps its own.
    h.reset_window();
    assert(h.since.max_us == 0 && h.since.slow == 0);
    t = pass(h, t, 26000, 50, 20, 100); // a black-box flush in can rx
    t = pass(h, t, 100, 50, 20, 100);
    assert(h.since.slow == 1 && h.since.max_stage == LOOP_STAGE_CAN_RX);
    assert(h.since.max_us == 26170);
    assert(h.boot.slow == 3 && h.boot.max_us == 400570);

    // Exactly 25 ms is not slow.
    LoopHealth e;
    e.begin_pass(0);
    e.mark(LOOP_STAGE_LOGS, LOOP_SLOW_US);
    e.begin_pass(LOOP_SLOW_US);
    assert(e.boot.slow == 0 && e.boot.max_us == LOOP_SLOW_US);

    // Across the micros() wrap.
    LoopHealth w;
    w.begin_pass(0xFFFFF000u);
    w.mark(LOOP_STAGE_WEB, 0x00001000u);
    w.begin_pass(0x00001000u);
    assert(w.boot.max_us == 0x2000u && w.boot.max_stage == LOOP_STAGE_WEB);
}

static void test_names() {
    assert(strcmp(loop_stage_name(LOOP_STAGE_CAN_RX), "can rx") == 0);
    assert(strcmp(loop_stage_name(LOOP_STAGE_WEB), "web") == 0);
    assert(strcmp(loop_stage_name(LOOP_STAGE_OTHER), "other") == 0);
    assert(strcmp(loop_stage_name(LOOP_STAGE_COUNT), "?") == 0);
}

int main() {
    test_passes();
    test_names();
    puts("Loop health tests passed: pass length, slowest stage, slow count, reset window, wrap");
}
