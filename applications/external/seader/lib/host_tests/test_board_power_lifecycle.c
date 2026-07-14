#include "munit.h"

#include "board_power_lifecycle.h"

static MunitResult test_acquire_plan_when_otg_is_off(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    SeaderBoardPowerAcquirePlan plan = seader_board_power_plan_acquire(false);
    munit_assert_true(plan.should_enable_otg);
    munit_assert_true(plan.owns_otg);
    return MUNIT_OK;
}

static MunitResult test_acquire_plan_when_otg_is_already_on(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    SeaderBoardPowerAcquirePlan plan = seader_board_power_plan_acquire(true);
    munit_assert_false(plan.should_enable_otg);
    munit_assert_false(plan.owns_otg);
    return MUNIT_OK;
}

static MunitResult test_disable_owned_otg_only_when_still_enabled(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    munit_assert_true(seader_board_should_disable_owned_otg(true, true));
    munit_assert_false(seader_board_should_disable_owned_otg(true, false));
    munit_assert_false(seader_board_should_disable_owned_otg(false, true));
    return MUNIT_OK;
}

static MunitResult test_power_available_when_usb_vbus_is_already_present(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    munit_assert_true(seader_board_power_is_available(false, 5000U));
    munit_assert_true(seader_board_power_is_available(true, 0U));
    munit_assert_false(seader_board_power_is_available(false, 4400U));
    return MUNIT_OK;
}

static MunitResult test_power_unavailable_when_neither_otg_nor_vbus_is_present(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    munit_assert_false(seader_board_power_is_available(false, 0U));
    return MUNIT_OK;
}

static MunitResult test_runtime_power_state_uses_handoff_grace(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    munit_assert_int(
        seader_board_runtime_power_state(false, false, 5000U, false, false, 0U, 2000U),
        ==,
        SeaderBoardRuntimePowerStateHealthy);
    munit_assert_int(
        seader_board_runtime_power_state(true, false, 0U, false, false, 0U, 2000U),
        ==,
        SeaderBoardRuntimePowerStateGracePending);
    munit_assert_int(
        seader_board_runtime_power_state(true, false, 0U, false, true, 1500U, 2000U),
        ==,
        SeaderBoardRuntimePowerStateGracePending);
    munit_assert_int(
        seader_board_runtime_power_state(true, false, 0U, false, true, 2000U, 2000U),
        ==,
        SeaderBoardRuntimePowerStateLost);
    munit_assert_int(
        seader_board_runtime_power_state(true, true, 0U, false, true, 2000U, 2000U),
        ==,
        SeaderBoardRuntimePowerStateHealthy);
    return MUNIT_OK;
}

static MunitResult test_runtime_power_state_fault_is_immediate(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    munit_assert_int(
        seader_board_runtime_power_state(true, false, 0U, true, false, 0U, 2000U),
        ==,
        SeaderBoardRuntimePowerStateLost);
    return MUNIT_OK;
}

static MunitResult test_runtime_event_action_honors_grace_and_autorecover_rules(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    munit_assert_int(
        seader_board_runtime_event_action(
            SeaderBoardRuntimePowerStateGracePending, true, false),
        ==,
        SeaderBoardRuntimeEventActionWait);
    munit_assert_int(
        seader_board_runtime_event_action(SeaderBoardRuntimePowerStateLost, true, false),
        ==,
        SeaderBoardRuntimeEventActionAutoRecover);
    munit_assert_int(
        seader_board_runtime_event_action(SeaderBoardRuntimePowerStateLost, false, false),
        ==,
        SeaderBoardRuntimeEventActionBoardPowerLost);
    munit_assert_int(
        seader_board_runtime_event_action(SeaderBoardRuntimePowerStateLost, true, true),
        ==,
        SeaderBoardRuntimeEventActionNone);
    return MUNIT_OK;
}

static MunitResult test_status_requires_power_cycle(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    munit_assert_true(seader_board_status_requires_power_cycle(SeaderBoardStatusFaultPreEnable));
    munit_assert_true(seader_board_status_requires_power_cycle(SeaderBoardStatusFaultPostEnable));
    munit_assert_true(seader_board_status_requires_power_cycle(SeaderBoardStatusNoResponse));
    munit_assert_true(seader_board_status_requires_power_cycle(SeaderBoardStatusPowerLost));
    munit_assert_true(seader_board_status_requires_power_cycle(SeaderBoardStatusRetryRequested));
    munit_assert_false(seader_board_status_requires_power_cycle(SeaderBoardStatusReady));
    return MUNIT_OK;
}

static MunitResult test_status_on_sam_missing(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    munit_assert_int(
        seader_board_status_on_sam_missing(SeaderBoardStatusPowerReadyPendingValidation),
        ==,
        SeaderBoardStatusNoResponse);
    munit_assert_int(
        seader_board_status_on_sam_missing(SeaderBoardStatusFaultPostEnable),
        ==,
        SeaderBoardStatusFaultPostEnable);
    return MUNIT_OK;
}

static MunitResult test_status_labels(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    munit_assert_string_equal(
        seader_board_status_label(SeaderBoardStatusFaultPreEnable), "Board Fault");
    munit_assert_string_equal(
        seader_board_status_label(SeaderBoardStatusNoResponse), "Board No Response");
    munit_assert_string_equal(seader_board_status_label(SeaderBoardStatusPowerLost), "Power Lost");
    munit_assert_string_equal(seader_board_status_label(SeaderBoardStatusReady), "Board Ready");
    return MUNIT_OK;
}

static MunitTest test_board_power_lifecycle_cases[] = {
    {(char*)"/acquire-otg-off",
     test_acquire_plan_when_otg_is_off,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/acquire-otg-on",
     test_acquire_plan_when_otg_is_already_on,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/disable-owned-otg",
     test_disable_owned_otg_only_when_still_enabled,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/power-available-with-vbus",
     test_power_available_when_usb_vbus_is_already_present,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/power-unavailable-with-no-rail",
     test_power_unavailable_when_neither_otg_nor_vbus_is_present,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/runtime-power-state-grace",
     test_runtime_power_state_uses_handoff_grace,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/runtime-power-state-fault",
     test_runtime_power_state_fault_is_immediate,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/runtime-event-action",
     test_runtime_event_action_honors_grace_and_autorecover_rules,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/status-requires-power-cycle",
     test_status_requires_power_cycle,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/status-on-sam-missing",
     test_status_on_sam_missing,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/status-labels", test_status_labels, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

MunitSuite test_board_power_lifecycle_suite = {
    "/board-power-lifecycle",
    test_board_power_lifecycle_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
