#include <ctype.h>
#include <string.h>

#include "hf_sam_response_view.h"
#include "munit.h"

static size_t test_hex_to_bytes(const char* hex, uint8_t* out, size_t out_size) {
    size_t len = 0U;
    int high_nibble = -1;

    for(const char* p = hex; *p; ++p) {
        int value = -1;
        if(*p >= '0' && *p <= '9') value = *p - '0';
        else if(*p >= 'A' && *p <= 'F') value = *p - 'A' + 10;
        else if(*p >= 'a' && *p <= 'f') value = *p - 'a' + 10;
        else if(isspace((unsigned char)*p)) continue;
        else munit_error("invalid hex character");

        if(high_nibble < 0) {
            high_nibble = value;
        } else {
            if(len >= out_size) munit_error("hex output buffer too small");
            out[len++] = (uint8_t)((high_nibble << 4) | value);
            high_nibble = -1;
        }
    }

    if(high_nibble >= 0) munit_error("odd-length hex string");
    return len;
}

static void assert_nfc_send_vector(
    const char* response_hex,
    const char* expected_data_hex,
    uint16_t expected_protocol,
    uint32_t expected_timeout_us,
    const char* expected_format_hex) {
    uint8_t response[128] = {0};
    uint8_t expected_data[32] = {0};
    uint8_t expected_format[8] = {0};
    SeaderHfSamNfcSendView view = {0};
    size_t response_len = test_hex_to_bytes(response_hex, response, sizeof(response));
    size_t expected_data_len =
        test_hex_to_bytes(expected_data_hex, expected_data, sizeof(expected_data));
    size_t expected_format_len =
        test_hex_to_bytes(expected_format_hex, expected_format, sizeof(expected_format));

    munit_assert_true(
        seader_hf_sam_response_view_parse_nfc_send(response, response_len, &view));
    munit_assert_size(view.data_len, ==, expected_data_len);
    munit_assert_memory_equal(expected_data_len, view.data, expected_data);
    munit_assert_uint16(view.protocol, ==, expected_protocol);
    munit_assert_uint32(view.timeout_us, ==, expected_timeout_us);
    munit_assert_size(view.format_len, ==, expected_format_len);
    if(expected_format_len > 0U) {
        munit_assert_memory_equal(expected_format_len, view.format, expected_format);
    } else {
        munit_assert_null(view.format);
    }
}

static MunitResult test_parses_live_seos_nfc_send_vector(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;
    assert_nfc_send_vector(
        "0A140A000000A120A11E800E0A0000A4040007D2760000850100810200028203017995850306C000",
        "0A0000A4040007D2760000850100",
        0x0002U,
        96661U,
        "06C000");
    return MUNIT_OK;
}

static MunitResult test_parses_older_seos_nfc_send_vector(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;
    assert_nfc_send_vector(
        "0A140A000000A129A12780110200A404000AA000000440000101000100810200028203012E11830102840102850306C000",
        "0200A404000AA000000440000101000100",
        0x0002U,
        77329U,
        "06C000");
    return MUNIT_OK;
}

static MunitResult test_parses_live_mfc_nfc_send_vector(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;
    assert_nfc_send_vector(
        "0A140A000000A115A11380046000F57B8102000282022710850300C000",
        "6000F57B",
        0x0002U,
        10000U,
        "00C000");
    return MUNIT_OK;
}

static MunitResult test_parses_live_picopass_nfc_send_vector(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;
    assert_nfc_send_vector(
        "0A140A000000A110A10E80040C05DE6481020004820201F4",
        "0C05DE64",
        0x0004U,
        500U,
        "");
    return MUNIT_OK;
}

static MunitResult test_rejects_error_response_vector(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;
    uint8_t response[32] = {0};
    SeaderHfSamNfcSendView view = {0};
    size_t response_len =
        test_hex_to_bytes("0A4400000000BE0780013D81020015", response, sizeof(response));

    munit_assert_false(
        seader_hf_sam_response_view_parse_nfc_send(response, response_len, &view));
    return MUNIT_OK;
}

static MunitResult test_rejects_truncated_header(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;
    uint8_t response[] = {0x0A, 0x14, 0x0A, 0x00, 0x00};
    SeaderHfSamNfcSendView view = {0};

    munit_assert_false(
        seader_hf_sam_response_view_parse_nfc_send(response, sizeof(response), &view));
    return MUNIT_OK;
}

static MunitResult test_rejects_missing_timeout(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;
    uint8_t response[64] = {0};
    SeaderHfSamNfcSendView view = {0};
    size_t response_len =
        test_hex_to_bytes("0A140A000000A109A1078002600081020002", response, sizeof(response));

    munit_assert_false(
        seader_hf_sam_response_view_parse_nfc_send(response, response_len, &view));
    return MUNIT_OK;
}

static MunitResult test_rejects_malformed_nested_length(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;
    uint8_t response[64] = {0};
    SeaderHfSamNfcSendView view = {0};
    size_t response_len = test_hex_to_bytes(
        "0A140A000000A10EA11380046000F57B8102000282022710", response, sizeof(response));

    munit_assert_false(
        seader_hf_sam_response_view_parse_nfc_send(response, response_len, &view));
    return MUNIT_OK;
}

static MunitResult test_ignores_unknown_optional_tags(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;
    assert_nfc_send_vector(
        "0A140A000000A119A1178002600081020002820227108301AA8601BB850300C000",
        "6000",
        0x0002U,
        10000U,
        "00C000");
    return MUNIT_OK;
}

static MunitTest tests[] = {
    {(char*)"/live-seos-vector", test_parses_live_seos_nfc_send_vector, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/older-seos-vector", test_parses_older_seos_nfc_send_vector, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/live-mfc-vector", test_parses_live_mfc_nfc_send_vector, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/live-picopass-vector", test_parses_live_picopass_nfc_send_vector, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/reject-error-response", test_rejects_error_response_vector, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/reject-truncated-header", test_rejects_truncated_header, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/reject-missing-timeout", test_rejects_missing_timeout, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/reject-malformed-nested-length", test_rejects_malformed_nested_length, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/ignore-unknown-optional-tags", test_ignores_unknown_optional_tags, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

MunitSuite test_hf_sam_response_view_suite = {
    (char*)"",
    tests,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
