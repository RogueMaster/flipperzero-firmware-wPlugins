#include "munit.h"
#include "uhf_status_label.h"

static MunitResult test_formats_none(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;
    char label[SEADER_UHF_STATUS_LABEL_MAX_LEN] = {0};
    seader_uhf_status_label_format(false, false, false, false, label, sizeof(label));
    munit_assert_string_equal(label, "UHF: none");
    return MUNIT_OK;
}

static MunitResult test_formats_supported_key_states(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;
    char label[SEADER_UHF_STATUS_LABEL_MAX_LEN] = {0};
    seader_uhf_status_label_format(true, false, true, true, label, sizeof(label));
    munit_assert_string_equal(label, "UHF: Monza 4QT [no key]/Higgs 3");
    return MUNIT_OK;
}

static MunitTest test_uhf_status_label_cases[] = {
    {(char*)"/none", test_formats_none, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/supported-key-states", test_formats_supported_key_states, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, 0, NULL},
};

MunitSuite test_uhf_status_label_suite = {
    "",
    test_uhf_status_label_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
