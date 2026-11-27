#pragma once
/*
 * blackbox_name.h — names of saved black-box events, and which one is older
 * (blackbox.cpp). Header-only and Arduino-free, so test/test_esp32_core.cpp
 * runs it on the host.
 *
 * An event is saved as ev<n>_<trigger>.log/.json, <n> one more than the
 * highest number already on the disk when blackbox_init() looked, so the
 * order survives a reboot and two events never get the same name. Nothing
 * else is written to flash for it.
 *
 * Earlier firmware named them evt_<millis>_<trigger>. millis() starts again
 * at every boot, so those names say nothing about which event is older and
 * can repeat across boots. They are still listed, downloaded and deleted,
 * and sort before every numbered event: retention drops them first.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// The event's number, 0 for a name without one (evt_<millis>_..., or any
// other file someone put in the directory).
static inline uint32_t bb_event_seq(const char *base) {
    if (!base || base[0] != 'e' || base[1] != 'v') return 0;
    const char *p = base + 2;
    uint64_t n = 0;
    for (; *p >= '0' && *p <= '9'; p++) {
        n = n * 10u + (uint32_t)(*p - '0');
        if (n > 0xFFFFFFFFu) return 0;
    }
    return (p > base + 2 && *p == '_') ? (uint32_t)n : 0;
}

// Older events give smaller values. Numbered ones go by their number; the
// old evt_<millis>_ names come before all of them and by <millis> among
// themselves, the order retention kept within one boot before.
static inline uint64_t bb_event_order(const char *base) {
    uint32_t seq = bb_event_seq(base);
    if (seq) return (uint64_t)seq << 32;
    if (!base || base[0] != 'e' || base[1] != 'v' || base[2] != 't' || base[3] != '_') return 0;
    return (uint32_t)strtoul(base + 4, nullptr, 10);
}

// Basename for event number `seq`. Zero-padded so a file manager lists them
// in order too.
static inline void bb_event_name(char *out, size_t n, uint32_t seq, const char *trigger) {
    snprintf(out, n, "ev%06lu_%s", (unsigned long)seq, trigger);
}
