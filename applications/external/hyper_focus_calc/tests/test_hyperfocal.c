#include "include/domain/hyperfocal.h"
#include "include/domain/sensor_data.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void test_coc_diag(void) {
    const float c = sensor_compute_coc_mm(36.0f, 24.0f);
    const float d = sqrtf(36.0f * 36.0f + 24.0f * 24.0f);
    assert(fabsf(c - d / 1500.0f) < 1e-4f);
}

static void test_hyperfocal_known(void) {
    /* Example: 50mm, f/8, CoC ~0.03mm full-frame style -> H in meters order of tens. */
    const float coc = 0.03f;
    const float h_mm = hyperfocal_distance_mm(50.0f, 8.0f, coc);
    const float h_m = h_mm / 1000.0f;
    assert(h_m > 10.0f && h_m < 200.0f);
}

static void test_fstop_step(void) {
    size_t i = 0;
    hyperfocal_fstop_step(&i, 1);
    assert(fabsf(hyperfocal_fstop_at_index(i) - 2.0f) < 1e-3f);
    hyperfocal_fstop_step(&i, -1);
    assert(fabsf(hyperfocal_fstop_at_index(i) - 1.4f) < 1e-3f);
}

static void test_fstop_index_of(void) {
    const size_t i = hyperfocal_fstop_index_of(5.6f);
    assert(fabsf(hyperfocal_fstop_at_index(i) - 5.6f) < 1e-3f);
}

int main(void) {
    test_coc_diag();
    test_hyperfocal_known();
    test_fstop_step();
    test_fstop_index_of();
    puts("test_hyperfocal: ok");
    return 0;
}
