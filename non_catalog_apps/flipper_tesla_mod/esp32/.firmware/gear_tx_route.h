#pragma once
/*
 * gear_tx_route.h — which bus a generated 0x229 press may go out on
 * (Continuous AP).
 *
 * The press continues the rolling counter of the car's own 0x229, so it only
 * goes out on a bus that carries that frame: one whose counter was stored
 * within max_age_ms. Each bus keeps its own counter, fed by the 0x229 received
 * there and by the press frames sent there. When more than one bus qualifies,
 * `preferred` (GEAR_LEVER_TX_BUS_INDEX) wins. A bus that has not carried 0x229
 * recently is never picked, whatever the configuration says. With one bus this
 * is the old rule: can0, while its counter is fresh.
 *
 * Header-only, no Arduino, so test/test_esp32_core.cpp runs it on the host.
 */

#include <stdbool.h>
#include <stdint.h>

// Last 0x229 rolling counter on one bus.
struct GearCounterCache {
    uint8_t  counter;
    bool     valid;
    uint32_t ms;       // millis() when it was stored
};

static inline bool gear_counter_fresh(const GearCounterCache &c, uint32_t now_ms,
                                      uint32_t max_age_ms) {
    return c.valid && c.ms != 0u && (uint32_t)(now_ms - c.ms) <= max_age_ms;
}

// Bus index for the next press, or -1 when no bus has a fresh counter.
static inline int gear_tx_bus_index(const GearCounterCache *buses, uint8_t count,
                                    uint8_t preferred, uint32_t now_ms,
                                    uint32_t max_age_ms) {
    if (preferred < count && gear_counter_fresh(buses[preferred], now_ms, max_age_ms))
        return preferred;
    for (uint8_t i = 0; i < count; i++) {
        if (gear_counter_fresh(buses[i], now_ms, max_age_ms)) return i;
    }
    return -1;
}
