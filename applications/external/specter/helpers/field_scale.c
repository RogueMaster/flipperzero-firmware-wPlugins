#include "field_scale.h"

uint8_t field_scale_apply(uint8_t raw_duty, uint8_t full_scale) {
    if(full_scale == 0u) full_scale = SPECTER_SCALE_RAW;
    if(raw_duty >= full_scale) return 100u;

    /* Rounded rather than truncated: at these small integer ranges truncation
     * visibly costs a percent, and the meter should not read low. */
    uint32_t scaled = ((uint32_t)raw_duty * 100u + full_scale / 2u) / full_scale;
    return (uint8_t)(scaled > 100u ? 100u : scaled);
}

bool field_scale_is_saturated(uint8_t raw_duty, uint8_t full_scale) {
    if(full_scale == 0u) full_scale = SPECTER_SCALE_RAW;
    return raw_duty >= full_scale;
}
