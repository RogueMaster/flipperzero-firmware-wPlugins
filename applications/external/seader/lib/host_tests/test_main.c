#include "munit.h"

extern MunitSuite test_lrc_suite;
extern MunitSuite test_t1_existing_suite;

int main(int argc, char* argv[]) {
    MunitSuite child_suites[] = {
        {"/lrc", test_lrc_suite.tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
        {"/t1", test_t1_existing_suite.tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
        {NULL, NULL, NULL, 0, 0},
    };
    MunitSuite main_suite = {
        "",
        NULL,
        child_suites,
        1,
        MUNIT_SUITE_OPTION_NONE,
    };
    return munit_suite_main(&main_suite, NULL, argc, argv);
}
