#include "munit.h"

#include "hf_release_sequence.h"

typedef struct {
    unsigned index;
    const char* calls[8];
} ReleaseRecorder;

static void record_call(ReleaseRecorder* recorder, const char* name) {
    if(recorder && recorder->index < (sizeof(recorder->calls) / sizeof(recorder->calls[0]))) {
        recorder->calls[recorder->index++] = name;
    }
}

static void record_plugin_stop(void* context) {
    record_call(context, "plugin-stop");
}

static void record_host_poller_release(void* context) {
    record_call(context, "host-poller-release");
}

static void record_picopass_release(void* context) {
    record_call(context, "picopass-release");
}

static void record_plugin_free(void* context) {
    record_call(context, "plugin-free");
}

static void record_manager_unload(void* context) {
    record_call(context, "plugin-manager-unload");
}

static void record_worker_reset(void* context) {
    record_call(context, "worker-reset");
}

static MunitResult test_release_sequence_orders_operations_and_finalizes_state(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    ReleaseRecorder recorder = {0};
    SeaderHfSessionState hf_state = SeaderHfSessionStateActive;
    SeaderModeRuntime mode_runtime = SeaderModeRuntimeHF;
    SeaderHfReleaseSequence sequence = {
        .context = &recorder,
        .hf_session_state = &hf_state,
        .mode_runtime = &mode_runtime,
        .plugin_stop = record_plugin_stop,
        .host_poller_release = record_host_poller_release,
        .host_picopass_release = record_picopass_release,
        .plugin_free = record_plugin_free,
        .plugin_manager_unload = record_manager_unload,
        .worker_reset = record_worker_reset,
    };

    seader_hf_release_sequence_run(&sequence);

    munit_assert_int(hf_state, ==, SeaderHfSessionStateUnloaded);
    munit_assert_int(mode_runtime, ==, SeaderModeRuntimeNone);
    munit_assert_uint(recorder.index, ==, 6);
    munit_assert_string_equal(recorder.calls[0], "plugin-stop");
    munit_assert_string_equal(recorder.calls[1], "host-poller-release");
    munit_assert_string_equal(recorder.calls[2], "picopass-release");
    munit_assert_string_equal(recorder.calls[3], "plugin-free");
    munit_assert_string_equal(recorder.calls[4], "plugin-manager-unload");
    munit_assert_string_equal(recorder.calls[5], "worker-reset");
    return MUNIT_OK;
}

static MunitResult test_release_sequence_tolerates_missing_callbacks(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    SeaderHfSessionState hf_state = SeaderHfSessionStateLoaded;
    SeaderModeRuntime mode_runtime = SeaderModeRuntimeHF;
    SeaderHfReleaseSequence sequence = {
        .hf_session_state = &hf_state,
        .mode_runtime = &mode_runtime,
        .worker_reset = record_worker_reset,
    };

    seader_hf_release_sequence_run(&sequence);

    munit_assert_int(hf_state, ==, SeaderHfSessionStateUnloaded);
    munit_assert_int(mode_runtime, ==, SeaderModeRuntimeNone);
    return MUNIT_OK;
}

static MunitTest test_hf_release_sequence_cases[] = {
    {(char*)"/ordering", test_release_sequence_orders_operations_and_finalizes_state, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/missing-callbacks", test_release_sequence_tolerates_missing_callbacks, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, 0, NULL},
};

MunitSuite test_hf_release_sequence_suite = {
    "",
    test_hf_release_sequence_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
