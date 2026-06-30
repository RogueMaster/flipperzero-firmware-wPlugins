#include <furi.h>
#include <furi_hal.h>

#include <mp_flipper_modtime.h>

inline uint32_t mp_flipper_get_timestamp() {
    return furi_hal_rtc_get_timestamp();
}

inline uint32_t mp_flipper_get_tick_frequency() {
    return furi_kernel_get_tick_frequency();
}

inline uint32_t mp_flipper_get_tick() {
    return furi_get_tick();
}

inline void mp_flipper_delay_ms(uint32_t ms) {
    furi_delay_ms(ms);
}

inline void mp_flipper_delay_us(uint32_t us) {
    furi_delay_us(us);
}
