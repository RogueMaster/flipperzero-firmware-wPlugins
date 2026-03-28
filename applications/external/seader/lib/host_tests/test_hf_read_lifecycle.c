#include <string.h>

#include "munit.h"

#include "hf_read_lifecycle.h"

static MunitResult test_card_detect_starts_only_from_detecting_when_sam_idle(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    munit_assert_int(
        seader_hf_read_on_card_detect(SeaderHfReadStateDetecting, true),
        ==,
        SeaderHfCardSessionDecisionStartConversation);
    munit_assert_int(
        seader_hf_read_on_card_detect(SeaderHfReadStateIdle, true),
        ==,
        SeaderHfCardSessionDecisionAbort);
    munit_assert_int(
        seader_hf_read_on_card_detect(SeaderHfReadStateDetecting, false),
        ==,
        SeaderHfCardSessionDecisionAbort);
    return MUNIT_OK;
}

static MunitResult test_waiting_states_and_timeout_policy(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    munit_assert_true(seader_hf_read_is_waiting_for_progress(SeaderHfReadStateConversationStarting));
    munit_assert_true(seader_hf_read_is_waiting_for_progress(SeaderHfReadStateConversationActive));
    munit_assert_true(seader_hf_read_is_waiting_for_progress(SeaderHfReadStateFinishing));
    munit_assert_false(seader_hf_read_is_waiting_for_progress(SeaderHfReadStateDetecting));

    munit_assert_false(
        seader_hf_read_should_timeout(SeaderHfReadStateConversationActive, 2999U, 3000U));
    munit_assert_true(
        seader_hf_read_should_timeout(SeaderHfReadStateConversationActive, 3000U, 3000U));
    munit_assert_false(seader_hf_read_should_timeout(SeaderHfReadStateIdle, 99999U, 3000U));
    return MUNIT_OK;
}

static MunitResult test_failure_reason_texts_are_stable(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    munit_assert_string_equal(
        seader_hf_read_failure_reason_text(SeaderHfReadFailureReasonUnavailable),
        "HF unavailable");
    munit_assert_string_equal(
        seader_hf_read_failure_reason_text(SeaderHfReadFailureReasonSamBusy), "SAM not idle");
    munit_assert_string_equal(
        seader_hf_read_failure_reason_text(SeaderHfReadFailureReasonInternalState),
        "Read state error");
    munit_assert_string_equal(
        seader_hf_read_failure_reason_text(SeaderHfReadFailureReasonSamTimeout), "SAM timeout");
    munit_assert_string_equal(
        seader_hf_read_failure_reason_text(SeaderHfReadFailureReasonBoardMissing), "Reader lost");
    return MUNIT_OK;
}

static MunitResult test_error_texts_fit_read_error_storage(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    static const char* protected_read_timeout =
        "Protected read timed out.\nNo supported data\nor wrong key.";

    munit_assert_size(strlen(protected_read_timeout), <, 96U);
    munit_assert_size(
        strlen(seader_hf_read_failure_reason_text(SeaderHfReadFailureReasonInternalState)), <, 96U);
    return MUNIT_OK;
}

static MunitTest test_hf_read_lifecycle_cases[] = {
    {(char*)"/card-detect-gating", test_card_detect_starts_only_from_detecting_when_sam_idle, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/timeout-policy", test_waiting_states_and_timeout_policy, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/failure-text", test_failure_reason_texts_are_stable, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/failure-text-fits", test_error_texts_fit_read_error_storage, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, 0, NULL},
};

MunitSuite test_hf_read_lifecycle_suite = {
    "",
    test_hf_read_lifecycle_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
