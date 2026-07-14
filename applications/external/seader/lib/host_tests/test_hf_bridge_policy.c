#include "munit.h"

#include "hf_bridge_policy.h"

typedef struct {
    unsigned index;
    const char* calls[4];
    bool begin_result;
    int run_result;
} BridgeRecorder;

static void record_call(BridgeRecorder* recorder, const char* name) {
    if(recorder && recorder->index < (sizeof(recorder->calls) / sizeof(recorder->calls[0]))) {
        recorder->calls[recorder->index++] = name;
    }
}

static void record_set_conversation(void* context) {
    record_call(context, "set-conversation");
}

static bool record_begin_card_session(void* context) {
    BridgeRecorder* recorder = context;
    record_call(recorder, "begin-card-session");
    return recorder->begin_result;
}

static void record_set_fail(void* context) {
    record_call(context, "set-fail");
}

static int record_run_conversation(void* context) {
    BridgeRecorder* recorder = context;
    record_call(recorder, "run-conversation");
    return recorder->run_result;
}

static const SeaderHfBridgeConversationOps bridge_ops = {
    .set_conversation = record_set_conversation,
    .begin_card_session = record_begin_card_session,
    .set_fail = record_set_fail,
    .run_conversation = record_run_conversation,
};

static MunitResult test_begin_conversation_sets_stage_before_card_session(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    BridgeRecorder recorder = {
        .begin_result = true,
        .run_result = 42,
    };

    const int result = seader_hf_bridge_begin_conversation(&recorder, &bridge_ops, -1);

    munit_assert_int(result, ==, 42);
    munit_assert_uint(recorder.index, ==, 3);
    munit_assert_string_equal(recorder.calls[0], "set-conversation");
    munit_assert_string_equal(recorder.calls[1], "begin-card-session");
    munit_assert_string_equal(recorder.calls[2], "run-conversation");
    return MUNIT_OK;
}

static MunitResult test_begin_conversation_fails_without_running_conversation(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    BridgeRecorder recorder = {
        .begin_result = false,
        .run_result = 42,
    };

    const int result = seader_hf_bridge_begin_conversation(&recorder, &bridge_ops, -1);

    munit_assert_int(result, ==, -1);
    munit_assert_uint(recorder.index, ==, 3);
    munit_assert_string_equal(recorder.calls[0], "set-conversation");
    munit_assert_string_equal(recorder.calls[1], "begin-card-session");
    munit_assert_string_equal(recorder.calls[2], "set-fail");
    return MUNIT_OK;
}

static MunitResult test_rf_status_mapping(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    uint8_t bytes[2] = {0xFF, 0xFF};

    seader_hf_bridge_rf_status_bytes(SeaderHfBridgeRfStatusSuccess, bytes);
    munit_assert_uint8(bytes[0], ==, 0x00);
    munit_assert_uint8(bytes[1], ==, 0x00);

    seader_hf_bridge_rf_status_bytes(SeaderHfBridgeRfStatusTimeout, bytes);
    munit_assert_uint8(bytes[0], ==, 0x00);
    munit_assert_uint8(bytes[1], ==, 0x20);

    seader_hf_bridge_rf_status_bytes(SeaderHfBridgeRfStatusCrc, bytes);
    munit_assert_uint8(bytes[0], ==, 0x00);
    munit_assert_uint8(bytes[1], ==, 0x04);

    seader_hf_bridge_rf_status_bytes(SeaderHfBridgeRfStatusProtocol, bytes);
    munit_assert_uint8(bytes[0], ==, 0x00);
    munit_assert_uint8(bytes[1], ==, 0x04);
    return MUNIT_OK;
}

static MunitResult test_apdu_decision_discards_stale_messages(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    munit_assert_int(
        seader_hf_bridge_apdu_decision(false, false, 16U, 258U, true),
        ==,
        SeaderHfBridgeApduDecisionDiscardStale);
    munit_assert_int(
        seader_hf_bridge_apdu_decision(true, false, 16U, 258U, true),
        ==,
        SeaderHfBridgeApduDecisionQueue);
    return MUNIT_OK;
}

static MunitResult test_apdu_decision_fails_overflow(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    munit_assert_int(
        seader_hf_bridge_apdu_decision(false, true, 259U, 258U, true),
        ==,
        SeaderHfBridgeApduDecisionFailProtocol);
    munit_assert_int(
        seader_hf_bridge_apdu_decision(false, true, 16U, 258U, false),
        ==,
        SeaderHfBridgeApduDecisionFailProtocol);
    return MUNIT_OK;
}

static MunitResult test_timeout_us_to_fwt_fc(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    munit_assert_uint32(seader_hf_bridge_timeout_us_to_fwt_fc(0U), ==, 0U);
    munit_assert_uint32(seader_hf_bridge_timeout_us_to_fwt_fc(1000U), ==, 13560U);
    munit_assert_uint32(seader_hf_bridge_timeout_us_to_fwt_fc(20000U), ==, 271200U);
    return MUNIT_OK;
}

static MunitTest test_hf_bridge_policy_cases[] = {
    {(char*)"/begin-conversation-order", test_begin_conversation_sets_stage_before_card_session, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/begin-conversation-fail", test_begin_conversation_fails_without_running_conversation, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/rf-status-mapping", test_rf_status_mapping, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/apdu-discard-stale", test_apdu_decision_discards_stale_messages, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/apdu-overflow-fails", test_apdu_decision_fails_overflow, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/timeout-us-to-fwt", test_timeout_us_to_fwt_fc, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, 0, NULL},
};

MunitSuite test_hf_bridge_policy_suite = {
    "",
    test_hf_bridge_policy_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
