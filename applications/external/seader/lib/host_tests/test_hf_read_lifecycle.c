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
    munit_assert_string_equal(
        seader_hf_read_failure_reason_text(SeaderHfReadFailureReasonSamKeysMissing),
        "SAM missing keys");
    munit_assert_string_equal(
        seader_hf_read_failure_reason_text(SeaderHfReadFailureReasonResourceExhausted),
        "SAM exchange memory error");
    return MUNIT_OK;
}

static MunitResult test_prepare_context_clears_stale_failure(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    SeaderHfReadFailureReason failure_reason = SeaderHfReadFailureReasonSamTimeout;
    char read_error[32] = "previous timeout";

    seader_hf_read_prepare_context(&failure_reason, read_error, sizeof(read_error));

    munit_assert_int(failure_reason, ==, SeaderHfReadFailureReasonNone);
    munit_assert_char(read_error[0], ==, '\0');
    return MUNIT_OK;
}

static MunitResult test_empty_pacs2_detects_sam_keys_missing(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    munit_assert_false(seader_pacs2_indicates_sam_keys_missing(false, NULL, 0U));
    munit_assert_true(seader_pacs2_indicates_sam_keys_missing(true, NULL, 0U));
    munit_assert_true(seader_pacs2_indicates_sam_keys_missing(true, NULL, 1U));
    munit_assert_false(
        seader_pacs2_indicates_sam_keys_missing(true, (const uint8_t[]){0x00U, 0x10U}, 2U));
    return MUNIT_OK;
}

static MunitResult test_sam_keys_missing_error_texts_fit_storage(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    char label[97] = {0};

    seader_hf_read_format_sam_keys_missing_error(
        true, SeaderHfPacsMediaTypePicopass, true, false, label, sizeof(label));
    munit_assert_string_equal(
        label, "PicoPass recognized.\nUnable to read keys.\nSAM missing standard keys.");
    munit_assert_size(strlen(label), <, 96U);

    seader_hf_read_format_sam_keys_missing_error(
        true, SeaderHfPacsMediaTypeMifarePlus, true, true, label, sizeof(label));
    munit_assert_string_equal(
        label, "MIFARE Plus recognized.\nUnable to read keys.\nCheck SAM Info.");
    munit_assert_size(strlen(label), <, 96U);

    seader_hf_read_format_sam_keys_missing_error(
        false, SeaderHfPacsMediaTypeUnknown, true, false, label, sizeof(label));
    munit_assert_string_equal(
        label, "Unable to read keys.\nSAM missing standard\nkeys. Check SAM Info.");
    munit_assert_size(strlen(label), <, 96U);

    seader_hf_read_format_sam_keys_missing_error(
        false, SeaderHfPacsMediaTypeUnknown, false, false, label, sizeof(label));
    munit_assert_string_equal(label, "Unable to read keys.\nCheck SAM Info.");
    munit_assert_size(strlen(label), <, 96U);
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
    munit_assert_size(
        strlen(seader_hf_read_failure_reason_text(SeaderHfReadFailureReasonResourceExhausted)),
        <,
        96U);
    return MUNIT_OK;
}

static MunitTest test_hf_read_lifecycle_cases[] = {
    {(char*)"/card-detect-gating", test_card_detect_starts_only_from_detecting_when_sam_idle, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/timeout-policy", test_waiting_states_and_timeout_policy, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/failure-text", test_failure_reason_texts_are_stable, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/prepare-context", test_prepare_context_clears_stale_failure, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/failure-text-fits", test_error_texts_fit_read_error_storage, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/empty-pacs2", test_empty_pacs2_detects_sam_keys_missing, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/sam-keys-missing-text", test_sam_keys_missing_error_texts_fit_storage, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, 0, NULL},
};

MunitSuite test_hf_read_lifecycle_suite = {
    "",
    test_hf_read_lifecycle_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
