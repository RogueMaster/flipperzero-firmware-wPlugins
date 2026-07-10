#include "include/domain/speed_ramp.h"

#include <assert.h>
#include <stdio.h>

static void test_stage_thresholds(void) {
    // Each threshold belongs to the next (higher) stage.
    assert(speed_ramp_stage(0) == 1);
    assert(speed_ramp_stage(SPEED_RAMP_STAGE1_UNTIL - 1) == 1);
    assert(speed_ramp_stage(SPEED_RAMP_STAGE1_UNTIL) == 2);
    assert(speed_ramp_stage(SPEED_RAMP_STAGE2_UNTIL - 1) == 2);
    assert(speed_ramp_stage(SPEED_RAMP_STAGE2_UNTIL) == SPEED_RAMP_MAX_STAGE);
    assert(speed_ramp_stage(SPEED_RAMP_STAGE2_UNTIL + 10000) == SPEED_RAMP_MAX_STAGE);
}

static void test_zero_and_negative_distance_is_stage_one(void) {
    // Distance never goes negative in the sim, but the mapping must still be defined there.
    assert(speed_ramp_stage(-1) == 1);
    assert(speed_ramp_stage(-100000) == 1);
}

static void test_monotonic_and_in_range(void) {
    uint8_t previous = 1;
    for (int32_t d = 0; d <= SPEED_RAMP_STAGE2_UNTIL + 50; d++) {
        uint8_t stage = speed_ramp_stage(d);
        assert(stage >= 1 && stage <= SPEED_RAMP_MAX_STAGE);
        assert(stage >= previous);
        previous = stage;
    }
}

int main(void) {
    test_stage_thresholds();
    printf("test_stage_thresholds: PASS\n");

    test_zero_and_negative_distance_is_stage_one();
    printf("test_zero_and_negative_distance_is_stage_one: PASS\n");

    test_monotonic_and_in_range();
    printf("test_monotonic_and_in_range: PASS\n");

    return 0;
}
