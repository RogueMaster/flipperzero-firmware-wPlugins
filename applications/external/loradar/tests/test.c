#include "test.h"
#include "unit_tests/unit_tests.h"

int32_t run_lora_tests(void* context) {
    UNUSED(context);
    run_unit_tests();
    return 0;
}
