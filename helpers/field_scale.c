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

const char* field_proximity_word(uint8_t shown, bool saturated) {
    /* MAX means pegged: as close as this measurement can resolve, and moving
     * nearer will not change the number. Saying so is better than letting a
     * stuck needle look like a fault. */
    if(saturated) return "MAX";
    if(shown >= 70u) return "STRONG";
    if(shown >= 45u) return "CLOSE";
    if(shown >= 20u) return "NEAR";
    return "FAINT";
}

int field_trend(const uint8_t* history, uint8_t head, uint8_t len) {
    if(!history || len < 2u * SPECTER_TREND_SPAN) return 0;
    int recent = 0, older = 0;
    for(int k = 0; k < SPECTER_TREND_SPAN; k++) {
        recent += history[(head - k + 2 * (int)len) % (int)len];
    }
    for(int k = SPECTER_TREND_SPAN; k < 2 * SPECTER_TREND_SPAN; k++) {
        older += history[(head - k + 2 * (int)len) % (int)len];
    }
    int delta = (recent - older) / SPECTER_TREND_SPAN;
    if(delta >= SPECTER_TREND_DEADBAND) return 1;
    if(delta <= -SPECTER_TREND_DEADBAND) return -1;
    return 0;
}
