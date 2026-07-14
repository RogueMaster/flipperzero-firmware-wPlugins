#include "munit.h"

#include "ui_memory_policy.h"

static MunitResult test_keeps_submenu_in_normal_ui_phase(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    munit_assert_false(seader_ui_memory_should_release_submenu(SeaderUiMemoryPhaseNormal));
    return MUNIT_OK;
}

static MunitResult test_releases_submenu_during_hf_read(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    munit_assert_true(seader_ui_memory_should_release_submenu(SeaderUiMemoryPhaseHfReadActive));
    return MUNIT_OK;
}

static MunitResult test_keeps_inactive_lazy_views_in_normal_ui_phase(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    munit_assert_false(
        seader_ui_memory_should_release_inactive_lazy_views(SeaderUiMemoryPhaseNormal));
    return MUNIT_OK;
}

static MunitResult test_releases_inactive_lazy_views_during_hf_read(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    munit_assert_true(
        seader_ui_memory_should_release_inactive_lazy_views(SeaderUiMemoryPhaseHfReadActive));
    return MUNIT_OK;
}

static MunitTest test_ui_memory_policy_cases[] = {
    {(char*)"/normal-keeps-submenu",
     test_keeps_submenu_in_normal_ui_phase,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/hf-read-releases-submenu",
     test_releases_submenu_during_hf_read,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/normal-keeps-inactive-lazy-views",
     test_keeps_inactive_lazy_views_in_normal_ui_phase,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/hf-read-releases-inactive-lazy-views",
     test_releases_inactive_lazy_views_during_hf_read,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {NULL, NULL, NULL, NULL, 0, NULL},
};

MunitSuite test_ui_memory_policy_suite = {
    "",
    test_ui_memory_policy_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
