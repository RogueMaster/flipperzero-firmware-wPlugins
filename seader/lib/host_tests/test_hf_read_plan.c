#include "munit.h"

#include "seader_hf_read_plan.h"

static MunitResult test_selected_type_starts_immediately(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    const SeaderCredentialType detected_types[] = {
        SeaderCredentialType14A,
        SeaderCredentialTypeMifareClassic,
    };
    const SeaderHfReadPlan plan = seader_hf_read_plan_build(
        SeaderCredentialTypePicopass, detected_types, 2U);

    munit_assert_int(plan.decision, ==, SeaderHfReadDecisionStartRead);
    munit_assert_int(plan.type_to_read, ==, SeaderCredentialTypePicopass);
    munit_assert_size(plan.detected_type_count, ==, 0U);
    return MUNIT_OK;
}

static MunitResult test_no_detected_types_continues_polling(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    const SeaderHfReadPlan plan = seader_hf_read_plan_build(SeaderCredentialTypeNone, NULL, 0U);

    munit_assert_int(plan.decision, ==, SeaderHfReadDecisionContinuePolling);
    munit_assert_int(plan.type_to_read, ==, SeaderCredentialTypeNone);
    munit_assert_size(plan.detected_type_count, ==, 0U);
    return MUNIT_OK;
}

static MunitResult test_single_detected_type_starts_read(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    const SeaderCredentialType detected_types[] = {SeaderCredentialType14A};
    const SeaderHfReadPlan plan =
        seader_hf_read_plan_build(SeaderCredentialTypeNone, detected_types, 1U);

    munit_assert_int(plan.decision, ==, SeaderHfReadDecisionStartRead);
    munit_assert_int(plan.type_to_read, ==, SeaderCredentialType14A);
    munit_assert_size(plan.detected_type_count, ==, 1U);
    munit_assert_int(plan.detected_types[0], ==, SeaderCredentialType14A);
    return MUNIT_OK;
}

static MunitResult test_multiple_detected_types_requests_selection(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    const SeaderCredentialType detected_types[] = {
        SeaderCredentialType14A,
        SeaderCredentialTypeMifareClassic,
        SeaderCredentialTypePicopass,
    };
    const SeaderHfReadPlan plan =
        seader_hf_read_plan_build(SeaderCredentialTypeNone, detected_types, 3U);

    munit_assert_int(plan.decision, ==, SeaderHfReadDecisionSelectType);
    munit_assert_int(plan.type_to_read, ==, SeaderCredentialTypeNone);
    munit_assert_size(plan.detected_type_count, ==, 3U);
    munit_assert_int(plan.detected_types[0], ==, SeaderCredentialType14A);
    munit_assert_int(plan.detected_types[1], ==, SeaderCredentialTypeMifareClassic);
    munit_assert_int(plan.detected_types[2], ==, SeaderCredentialTypePicopass);
    return MUNIT_OK;
}

static MunitResult test_detected_types_are_deduped_and_clamped(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    const SeaderCredentialType detected_types[] = {
        SeaderCredentialType14A,
        SeaderCredentialTypeMifareClassic,
        SeaderCredentialType14A,
        SeaderCredentialTypeNone,
        SeaderCredentialTypePicopass,
        SeaderCredentialTypeConfig,
    };
    const SeaderHfReadPlan plan =
        seader_hf_read_plan_build(SeaderCredentialTypeNone, detected_types, 6U);

    munit_assert_int(plan.decision, ==, SeaderHfReadDecisionSelectType);
    munit_assert_size(plan.detected_type_count, ==, 3U);
    munit_assert_int(plan.detected_types[0], ==, SeaderCredentialType14A);
    munit_assert_int(plan.detected_types[1], ==, SeaderCredentialTypeMifareClassic);
    munit_assert_int(plan.detected_types[2], ==, SeaderCredentialTypePicopass);
    return MUNIT_OK;
}

static MunitTest test_hf_read_plan_cases[] = {
    {(char*)"/selected-type", test_selected_type_starts_immediately, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/none-detected", test_no_detected_types_continues_polling, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/single-detected", test_single_detected_type_starts_read, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/multiple-detected", test_multiple_detected_types_requests_selection, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/dedupe-and-clamp", test_detected_types_are_deduped_and_clamped, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, 0, NULL},
};

MunitSuite test_hf_read_plan_suite = {
    "",
    test_hf_read_plan_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
