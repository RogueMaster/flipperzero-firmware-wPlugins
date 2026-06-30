#include "munit.h"

extern MunitSuite test_hf_release_sequence_suite;

int main(int argc, char* argv[]) {
    MunitSuite main_suite = {
        "/runtime-integration",
        test_hf_release_sequence_suite.tests,
        NULL,
        1,
        MUNIT_SUITE_OPTION_NONE,
    };

    return munit_suite_main(&main_suite, NULL, argc, argv);
}
