#include "munit.h"

extern MunitSuite test_card_details_builder_suite;

int main(int argc, char* argv[]) {
    MunitSuite main_suite = {
        "/card-details",
        test_card_details_builder_suite.tests,
        NULL,
        1,
        MUNIT_SUITE_OPTION_NONE,
    };

    return munit_suite_main(&main_suite, NULL, argc, argv);
}
