#include <cassert>
#include <cstdio>
#include "can_counters.h"

// A TWAI status counter across installs: 40 missed, a mode switch, 7 more,
// a filter change, 2 more. The driver sees 40, then 0..7, then 0..2.
static void test_carry() {
    CanCarryCount c;
    assert(c.total(0) == 0);
    assert(c.total(25) == 25);
    assert(c.total(40) == 40);
    c.restart();
    assert(c.value() == 40);
    assert(c.total(0) == 40);
    assert(c.total(7) == 47);
    c.restart();
    assert(c.total(2) == 49);
    assert(c.value() == 49);

    // Uninstalled between installs: nothing new to read, the total holds.
    c.restart();
    assert(c.value() == 49);
    c.restart(); // a second restart with no read adds nothing
    assert(c.value() == 49);

    // Wraps like the counter it carries.
    CanCarryCount w;
    w.total(0xFFFFFFF0u);
    w.restart();
    assert(w.total(0x20u) == 0x10u);
}

static void test_queue_stats() {
    CanRxQueueStats q;
    q.note_push(true, 1);
    q.note_push(true, 2);
    q.note_push(true, 3);
    assert(q.queued == 3 && q.dropped == 0 && q.high_water == 3);
    // The loop drained it; the next burst does not reach the old depth.
    q.note_push(true, 1);
    assert(q.high_water == 3);
    // Full: 256 waiting, the frame is lost.
    q.note_push(false, 256);
    q.note_push(false, 256);
    assert(q.queued == 4 && q.dropped == 2 && q.high_water == 256);
}

int main() {
    test_carry();
    test_queue_stats();
    puts("CAN counter tests passed: carry across reinstalls, RX queue stats");
}
