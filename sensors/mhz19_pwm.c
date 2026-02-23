#include "mhz19_pwm.h"

#include <furi.h>

// Verbatim from https://github.com/meshchaninov/flipper-zero-mh-z19
int32_t calculate_ppm(
    int32_t* prevVal,
    int32_t val,
    int32_t* th,
    int32_t* tl,
    int32_t* h,
    int32_t* l,
    SensorRange range) {
    int32_t tt = furi_get_tick();
    if(val == 1) {
        if(val != *prevVal) {
            *h = tt;
            *tl = *h - *l;
            *prevVal = val;
        }
    } else {
        if(val != *prevVal) {
            *l = tt;
            *th = *l - *h;
            *prevVal = val;
            return range * (*th - 2) / (*th + *tl - 4);
        }
    }
    return -1;
}
