#include "munit.h"
#include "credential_sio_label.h"

static MunitResult test_returns_false_without_sio(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    char label[16] = "unchanged";

    munit_assert_false(
        seader_sio_label_format(false, false, 0U, label, sizeof(label)));
    munit_assert_string_equal(label, "");
    return MUNIT_OK;
}

static MunitResult test_formats_generic_sio_for_desfire(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    char label[16] = {0};

    munit_assert_true(
        seader_sio_label_format(true, false, 0U, label, sizeof(label)));
    munit_assert_string_equal(label, "+SIO");
    return MUNIT_OK;
}

static MunitResult test_formats_sr_and_se_for_picopass(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    char label[16] = {0};

    munit_assert_true(
        seader_sio_label_format(true, true, 6U, label, sizeof(label)));
    munit_assert_string_equal(label, "+SIO(SE)");

    munit_assert_true(
        seader_sio_label_format(true, true, 10U, label, sizeof(label)));
    munit_assert_string_equal(label, "+SIO(SR)");
    return MUNIT_OK;
}

static MunitResult test_formats_unknown_picopass_layout(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    char label[16] = {0};

    munit_assert_true(
        seader_sio_label_format(true, true, 0U, label, sizeof(label)));
    munit_assert_string_equal(label, "+SIO(?)");
    return MUNIT_OK;
}

static MunitTest test_credential_sio_label_cases[] = {
    {(char*)"/no-sio", test_returns_false_without_sio, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/desfire-generic", test_formats_generic_sio_for_desfire, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/picopass-sr-se", test_formats_sr_and_se_for_picopass, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/picopass-unknown", test_formats_unknown_picopass_layout, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, 0, NULL},
};

MunitSuite test_credential_sio_label_suite = {
    "",
    test_credential_sio_label_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
