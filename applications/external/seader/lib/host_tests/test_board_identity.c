#include "munit.h"

#include "board_identity.h"

static MunitResult test_classifies_strap_states(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    munit_assert_int(
        seader_board_classify(false, false, false), ==, SeaderBoardClassNone);
    munit_assert_int(
        seader_board_classify(false, true, false), ==, SeaderBoardClassSamOnly);
    munit_assert_int(
        seader_board_classify(false, false, true), ==, SeaderBoardClassSamOnly);
    munit_assert_int(
        seader_board_classify(true, false, false), ==, SeaderBoardClassUhfCarrier);
    munit_assert_int(
        seader_board_classify(true, true, true), ==, SeaderBoardClassUhfCarrier);
    return MUNIT_OK;
}

static MunitResult test_uhf_support_policy(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    munit_assert_false(seader_board_class_supports_uhf(SeaderBoardClassUnknown));
    munit_assert_false(seader_board_class_supports_uhf(SeaderBoardClassNone));
    munit_assert_false(seader_board_class_supports_uhf(SeaderBoardClassSamOnly));
    munit_assert_true(seader_board_class_supports_uhf(SeaderBoardClassUhfCarrier));
    return MUNIT_OK;
}

static MunitTest test_board_identity_cases[] = {
    {(char*)"/strap-classifier", test_classifies_strap_states, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/uhf-support-policy", test_uhf_support_policy, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, 0, NULL},
};

MunitSuite test_board_identity_suite = {
    "",
    test_board_identity_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};

