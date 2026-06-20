#include <string.h>

#include "munit.h"
#include "sam_key_label.h"

static MunitResult test_formats_no_sam(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;
    char label[SEADER_SAM_KEY_LABEL_MAX_LEN] = {0};

    seader_sam_key_label_format(
        false, SeaderSamKeyProbeStatusUnknown, NULL, 0U, false, false, label, sizeof(label));
    munit_assert_string_equal(label, "NO SAM");
    return MUNIT_OK;
}

static MunitResult test_formats_unknown_for_missing_value(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;
    char label[SEADER_SAM_KEY_LABEL_MAX_LEN] = {0};
    const uint8_t zeros[] = {0x00, 0x00, 0x00};

    seader_sam_key_label_format(
        true, SeaderSamKeyProbeStatusUnknown, NULL, 0U, false, false, label, sizeof(label));
    munit_assert_string_equal(label, "SAM: Key Unknown");

    seader_sam_key_label_format(
        true, SeaderSamKeyProbeStatusUnknown, zeros, sizeof(zeros), false, false, label, sizeof(label));
    munit_assert_string_equal(label, "SAM: Key Unknown");
    return MUNIT_OK;
}

static MunitResult test_formats_standard_key_for_successful_zero_value(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;
    char label[SEADER_SAM_KEY_LABEL_MAX_LEN] = {0};
    const uint8_t zero64[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    seader_sam_key_label_format(
        true,
        SeaderSamKeyProbeStatusVerifiedStandard,
        zero64,
        sizeof(zero64),
        true,
        true,
        label,
        sizeof(label));
    munit_assert_string_equal(label, "SAM: Standard Key");
    return MUNIT_OK;
}

static MunitResult test_probe_failure_never_formats_standard(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    char label[SEADER_SAM_KEY_LABEL_MAX_LEN] = {0};
    const uint8_t zero64[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    seader_sam_key_label_format(
        true, SeaderSamKeyProbeStatusProbeFailed, NULL, 0U, false, false, label, sizeof(label));
    munit_assert_string_equal(label, "SAM: Probe Failed");

    seader_sam_key_label_format(
        true,
        SeaderSamKeyProbeStatusProbeFailed,
        zero64,
        sizeof(zero64),
        false,
        false,
        label,
        sizeof(label));
    munit_assert_string_equal(label, "SAM: Probe Failed");
    return MUNIT_OK;
}

static MunitResult test_formats_ascii_ice_value(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;
    char label[SEADER_SAM_KEY_LABEL_MAX_LEN] = {0};
    const uint8_t ice[] = {'I', 'C', 'E', '1', '8', '0', '3'};

    seader_sam_key_label_format(
        true, SeaderSamKeyProbeStatusVerifiedValue, ice, sizeof(ice), true, true, label, sizeof(label));
    munit_assert_string_equal(label, "SAM: ICE1803");
    return MUNIT_OK;
}

static MunitResult test_formats_missing_standard_key(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;
    char label[SEADER_SAM_KEY_LABEL_MAX_LEN] = {0};
    const uint8_t zero64[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    seader_sam_key_label_format(
        true,
        SeaderSamKeyProbeStatusVerifiedStandard,
        zero64,
        sizeof(zero64),
        true,
        false,
        label,
        sizeof(label));
    munit_assert_string_equal(label, "MISSING STANDARD KEYS");
    munit_assert_size(strlen(label), <, SEADER_SAM_KEY_LABEL_MAX_LEN);
    return MUNIT_OK;
}

static MunitResult test_probe_status_from_snmp_result(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    const uint8_t zero64[] = {0x00, 0x00, 0x00, 0x00};
    const uint8_t ice[] = {'I', 'C', 'E'};

    munit_assert_int(
        seader_sam_key_probe_status_from_snmp_result(false, zero64, sizeof(zero64)),
        ==,
        SeaderSamKeyProbeStatusProbeFailed);
    munit_assert_int(
        seader_sam_key_probe_status_from_snmp_result(true, zero64, sizeof(zero64)),
        ==,
        SeaderSamKeyProbeStatusVerifiedStandard);
    munit_assert_int(
        seader_sam_key_probe_status_from_snmp_result(true, ice, sizeof(ice)),
        ==,
        SeaderSamKeyProbeStatusVerifiedValue);
    munit_assert_int(
        seader_sam_key_probe_status_from_snmp_result(true, NULL, 0U),
        ==,
        SeaderSamKeyProbeStatusUnknown);
    return MUNIT_OK;
}

static MunitResult test_sanitizes_non_printable_bytes(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;
    char label[SEADER_SAM_KEY_LABEL_MAX_LEN] = {0};
    const uint8_t mixed[] = {'A', 0x00, 0x1F, 'Z'};

    seader_sam_key_label_format(
        true, SeaderSamKeyProbeStatusVerifiedValue, mixed, sizeof(mixed), true, true, label, sizeof(label));
    munit_assert_string_equal(label, "SAM: A??Z");
    return MUNIT_OK;
}

static MunitTest test_sam_key_label_cases[] = {
    {(char*)"/no-sam", test_formats_no_sam, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/unknown", test_formats_unknown_for_missing_value, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/standard-key-zero64", test_formats_standard_key_for_successful_zero_value, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/missing-standard-key", test_formats_missing_standard_key, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/probe-status-from-snmp", test_probe_status_from_snmp_result, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/probe-failed", test_probe_failure_never_formats_standard, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/ascii", test_formats_ascii_ice_value, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/sanitize", test_sanitizes_non_printable_bytes, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, 0, NULL},
};

MunitSuite test_sam_key_label_suite = {
    "",
    test_sam_key_label_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
