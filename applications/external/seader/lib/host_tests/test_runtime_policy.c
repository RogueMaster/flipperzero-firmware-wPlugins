#include <string.h>

#include "munit.h"
#include "runtime_policy.h"

static MunitResult test_reset_cached_sam_metadata_clears_all_fields(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    uint8_t sam_version[2] = {1U, 99U};
    char uhf_status_label[16];
    memset(uhf_status_label, 'X', sizeof(uhf_status_label));
    SeaderUhfSnmpProbe probe = {
        .stage = SeaderUhfSnmpProbeStageDone,
        .has_monza4qt = true,
        .has_higgs3 = true,
        .monza4qt_key_present = true,
        .higgs3_key_present = true,
        .ice_value_len = 7U,
    };

    seader_runtime_reset_cached_sam_metadata(
        sam_version, uhf_status_label, sizeof(uhf_status_label), &probe);

    munit_assert_uint8(sam_version[0], ==, 0U);
    munit_assert_uint8(sam_version[1], ==, 0U);
    munit_assert_char(uhf_status_label[0], ==, '\0');
    munit_assert_int(probe.stage, ==, SeaderUhfSnmpProbeStageDiscovery);
    munit_assert_false(probe.has_monza4qt);
    munit_assert_false(probe.has_higgs3);
    munit_assert_false(probe.monza4qt_key_present);
    munit_assert_false(probe.higgs3_key_present);
    munit_assert_size(probe.ice_value_len, ==, 0U);
    return MUNIT_OK;
}

static MunitResult test_begin_uhf_probe_sets_runtime_and_initializes_probe(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    SeaderModeRuntime mode_runtime = SeaderModeRuntimeNone;
    SeaderUhfSnmpProbe probe = {.stage = SeaderUhfSnmpProbeStageDone};

    munit_assert_true(seader_runtime_begin_uhf_probe(
        true, &mode_runtime, SeaderHfSessionStateUnloaded, &probe));
    munit_assert_int(mode_runtime, ==, SeaderModeRuntimeUHF);
    munit_assert_int(probe.stage, ==, SeaderUhfSnmpProbeStageDiscovery);
    return MUNIT_OK;
}

static MunitResult test_begin_uhf_probe_rejects_invalid_states(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    SeaderModeRuntime mode_runtime = SeaderModeRuntimeNone;
    SeaderUhfSnmpProbe probe = {0};

    munit_assert_false(seader_runtime_begin_uhf_probe(
        false, &mode_runtime, SeaderHfSessionStateUnloaded, &probe));
    munit_assert_int(mode_runtime, ==, SeaderModeRuntimeNone);

    munit_assert_false(seader_runtime_begin_uhf_probe(
        true, &mode_runtime, SeaderHfSessionStateActive, &probe));
    munit_assert_int(mode_runtime, ==, SeaderModeRuntimeNone);

    mode_runtime = SeaderModeRuntimeHF;
    munit_assert_false(seader_runtime_begin_uhf_probe(
        true, &mode_runtime, SeaderHfSessionStateUnloaded, &probe));
    munit_assert_int(mode_runtime, ==, SeaderModeRuntimeHF);
    return MUNIT_OK;
}

static MunitResult test_finish_uhf_probe_restores_none(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    SeaderModeRuntime mode_runtime = SeaderModeRuntimeUHF;
    seader_runtime_finish_uhf_probe(&mode_runtime);
    munit_assert_int(mode_runtime, ==, SeaderModeRuntimeNone);

    mode_runtime = SeaderModeRuntimeHF;
    seader_runtime_finish_uhf_probe(&mode_runtime);
    munit_assert_int(mode_runtime, ==, SeaderModeRuntimeHF);
    return MUNIT_OK;
}

static MunitResult test_finalize_hf_release_sets_terminal_state(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    SeaderHfSessionState hf_state = SeaderHfSessionStateTearingDown;
    SeaderModeRuntime mode_runtime = SeaderModeRuntimeHF;

    seader_runtime_finalize_hf_release(&hf_state, &mode_runtime);

    munit_assert_int(hf_state, ==, SeaderHfSessionStateUnloaded);
    munit_assert_int(mode_runtime, ==, SeaderModeRuntimeNone);
    return MUNIT_OK;
}

static MunitResult test_begin_hf_teardown_sets_state(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    SeaderHfSessionState hf_state = SeaderHfSessionStateActive;
    seader_runtime_begin_hf_teardown(&hf_state);
    munit_assert_int(hf_state, ==, SeaderHfSessionStateTearingDown);
    return MUNIT_OK;
}

static MunitTest test_runtime_policy_cases[] = {
    {(char*)"/reset-sam-metadata", test_reset_cached_sam_metadata_clears_all_fields, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/begin-uhf-probe", test_begin_uhf_probe_sets_runtime_and_initializes_probe, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/begin-uhf-probe-invalid", test_begin_uhf_probe_rejects_invalid_states, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/finish-uhf-probe", test_finish_uhf_probe_restores_none, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/begin-hf-teardown", test_begin_hf_teardown_sets_state, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/finalize-hf-release", test_finalize_hf_release_sets_terminal_state, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, 0, NULL},
};

MunitSuite test_runtime_policy_suite = {
    "",
    test_runtime_policy_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
