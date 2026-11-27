#pragma once
/*
 * can_counters.h — counter bookkeeping for the CAN drivers, pure so the host
 * tests can run it (test/test_can_counters.cpp).
 *
 * CanCarryCount: TWAI keeps rx_missed, bus_error and tx_failed in the driver
 * object, and twai_driver_install() starts them from 0, so every mode switch
 * and capture filter change would restart them. CanDriver promises
 * cumulative counts; this folds each install's last reading into a base.
 *
 * CanRxQueueStats: the MCP2515 receive task's queue, frames in and out of it
 * and the deepest it has been.
 */

#include <stdint.h>

struct CanCarryCount {
    uint32_t base = 0;  // sum of the finished installs
    uint32_t last = 0;  // the current install, as last read

    // Cumulative count, given what the current install reports now.
    uint32_t total(uint32_t now) {
        last = now;
        return base + now;
    }

    // Cumulative count as of the last reading.
    uint32_t value() const { return base + last; }

    // The install is going away: keep what it counted. Read it one last time
    // (total()) right before, so nothing it counted after the previous read
    // is lost.
    void restart() {
        base += last;
        last = 0;
    }
};

struct CanRxQueueStats {
    uint32_t queued     = 0;  // frames the task put on the queue
    uint32_t dropped    = 0;  // frames read from the chip with the queue full
    uint32_t high_water = 0;  // most frames waiting at once
    uint32_t overflows  = 0;  // RXnOVR events: the chip's two buffers were full

    // One frame read from the chip. @p ok: it went on the queue; @p waiting:
    // frames on the queue after the attempt.
    void note_push(bool ok, uint32_t waiting) {
        if (ok) queued++;
        else dropped++;
        if (waiting > high_water) high_water = waiting;
    }
};
