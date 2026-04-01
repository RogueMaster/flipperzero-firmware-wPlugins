#include <string.h>

#include "munit.h"
#include "uhf_status_label.h"

static MunitResult test_formats_none(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;
    char label[SEADER_UHF_STATUS_LABEL_MAX_LEN] = {0};
    seader_uhf_status_label_format(
        SeaderUhfProbeStatusSuccess, false, false, false, false, label, sizeof(label));
    munit_assert_string_equal(label, "UHF: none");
    return MUNIT_OK;
}

static MunitResult test_formats_probing_and_failed_states(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    char label[SEADER_UHF_STATUS_LABEL_MAX_LEN] = {0};
    seader_uhf_status_label_format(
        SeaderUhfProbeStatusUnknown, false, false, false, false, label, sizeof(label));
    munit_assert_string_equal(label, "UHF: probing...");

    seader_uhf_status_label_format(
        SeaderUhfProbeStatusFailed, true, true, true, true, label, sizeof(label));
    munit_assert_string_equal(label, "UHF: probe failed");
    return MUNIT_OK;
}

static MunitResult test_formats_supported_key_states(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;
    char label[SEADER_UHF_STATUS_LABEL_MAX_LEN] = {0};
    seader_uhf_status_label_format(
        SeaderUhfProbeStatusSuccess, true, false, true, true, label, sizeof(label));
    munit_assert_string_equal(label, "UHF: Monza 4QT [no key]/Higgs 3");
    return MUNIT_OK;
}

static MunitResult test_longest_supported_label_fits_buffer(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    char label[SEADER_UHF_STATUS_LABEL_MAX_LEN] = {0};
    seader_uhf_status_label_format(
        SeaderUhfProbeStatusSuccess, true, false, true, false, label, sizeof(label));
    munit_assert_string_equal(label, "UHF: Monza 4QT [no key]/Higgs 3 [no key]");
    munit_assert_size(strlen(label), <, sizeof(label));
    return MUNIT_OK;
}

static MunitResult test_handles_null_and_zero_sized_output(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    seader_uhf_status_label_format(
        SeaderUhfProbeStatusSuccess, true, true, false, false, NULL, 0U);

    char label[4] = {'X', 'Y', 'Z', 'W'};
    seader_uhf_status_label_format(
        SeaderUhfProbeStatusSuccess, true, true, false, false, label, 0U);
    munit_assert_memory_equal(sizeof(label), label, ((char[]){'X', 'Y', 'Z', 'W'}));
    return MUNIT_OK;
}

static MunitResult test_nul_terminates_single_byte_output(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    char label[1] = {'X'};
    seader_uhf_status_label_format(
        SeaderUhfProbeStatusSuccess, true, false, true, false, label, sizeof(label));
    munit_assert_char(label[0], ==, '\0');
    return MUNIT_OK;
}

static MunitResult test_truncates_safely_for_small_buffers(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    char label[8];
    memset(label, 'Z', sizeof(label));
    seader_uhf_status_label_format(
        SeaderUhfProbeStatusSuccess, true, false, true, false, label, sizeof(label));

    munit_assert_char(label[sizeof(label) - 1], ==, '\0');
    munit_assert_char(label[0], ==, 'U');
    munit_assert_char(label[1], ==, 'H');
    return MUNIT_OK;
}

static MunitResult test_small_buffer_for_none_is_safe(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    char label[4];
    memset(label, 'Q', sizeof(label));
    seader_uhf_status_label_format(
        SeaderUhfProbeStatusSuccess, false, false, false, false, label, sizeof(label));

    munit_assert_char(label[sizeof(label) - 1], ==, '\0');
    munit_assert_char(label[0], ==, 'U');
    return MUNIT_OK;
}

static MunitTest test_uhf_status_label_cases[] = {
    {(char*)"/none", test_formats_none, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/probing-failed", test_formats_probing_and_failed_states, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/supported-key-states", test_formats_supported_key_states, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/longest-fits", test_longest_supported_label_fits_buffer, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/null-zero-output", test_handles_null_and_zero_sized_output, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/single-byte-output", test_nul_terminates_single_byte_output, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/small-buffer-truncation", test_truncates_safely_for_small_buffers, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/small-buffer-none", test_small_buffer_for_none_is_safe, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, 0, NULL},
};

MunitSuite test_uhf_status_label_suite = {
    "",
    test_uhf_status_label_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
