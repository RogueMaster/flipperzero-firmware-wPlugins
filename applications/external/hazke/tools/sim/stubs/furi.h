#pragma once
// Desktop stand-ins for the few Furi calls the game headers make, so the
// simulator can run the flight code and renderer on a Mac.
#include <stdint.h>
#include <stddef.h>

namespace SimClock {
inline uint32_t ms = 0;
}
inline uint32_t furi_get_tick() {
    return SimClock::ms;
}
inline void furi_delay_ms(uint32_t) {
}

struct FuriThread {
    int unused;
};
typedef int32_t (*FuriThreadCallback)(void*);
inline FuriThread* furi_thread_alloc_ex(const char*, uint32_t, FuriThreadCallback, void*) {
    static FuriThread t;
    return &t;
}
inline void furi_thread_start(FuriThread*) {
}
inline void furi_thread_join(FuriThread*) {
}
inline void furi_thread_free(FuriThread*) {
}
