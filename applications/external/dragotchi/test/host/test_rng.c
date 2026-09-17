#include "tests.h"
#include "test_util.h"
#include "rng_control.h"
#include "random_generator.h"
void run_rng_tests(void) {
    rng_seed(1234);
    uint32_t a = random_uniform(0, 100);
    rng_seed(1234);
    uint32_t b = random_uniform(0, 100);
    CHECK(a == b);
    CHECK(a < 100);
}
