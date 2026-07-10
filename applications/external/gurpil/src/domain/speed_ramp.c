#include "include/domain/speed_ramp.h"

uint8_t speed_ramp_stage(int32_t distance_units) {
    if (distance_units < SPEED_RAMP_STAGE1_UNTIL) {
        return 1;
    }
    if (distance_units < SPEED_RAMP_STAGE2_UNTIL) {
        return 2;
    }
    return SPEED_RAMP_MAX_STAGE;
}
