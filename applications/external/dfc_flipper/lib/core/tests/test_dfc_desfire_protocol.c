#include "munit/munit.h"

#include "dfc_common.h"

static MunitResult test_select_application_uses_desfire_byte_order(
    const MunitParameter params[],
    void* user_data) {
    (void)params;
    (void)user_data;

    uint8_t aid[] = {0xD3, 0x49, 0x4F};
    uint8_t frame[DFC_SELECT_APPLICATION_FRAME_SIZE];
    dfc_build_select_application_frame(aid, frame);

    uint8_t expected[] = {DFC_CMD_SELECT_APPLICATION, 0x4F, 0x49, 0xD3};
    munit_assert_memory_equal(sizeof(expected), frame, expected);
    return MUNIT_OK;
}

static MunitResult
    test_select_picc_level_is_zero_aid(const MunitParameter params[], void* user_data) {
    (void)params;
    (void)user_data;

    uint8_t aid[] = {0x00, 0x00, 0x00};
    uint8_t frame[DFC_SELECT_APPLICATION_FRAME_SIZE];
    dfc_build_select_application_frame(aid, frame);

    uint8_t expected[] = {DFC_CMD_SELECT_APPLICATION, 0x00, 0x00, 0x00};
    munit_assert_memory_equal(sizeof(expected), frame, expected);
    return MUNIT_OK;
}

static MunitResult test_read_data_uses_little_endian_offset_and_length(
    const MunitParameter params[],
    void* user_data) {
    (void)params;
    (void)user_data;

    uint8_t frame[DFC_READ_DATA_FRAME_SIZE];
    dfc_build_read_data_frame(0x0F, 0x00030201, 0x00060504, frame);

    uint8_t expected[] = {DFC_CMD_READ_DATA, 0x0F, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    munit_assert_memory_equal(sizeof(expected), frame, expected);
    return MUNIT_OK;
}

static MunitResult test_read_data_zero_length_requests_remaining_file(
    const MunitParameter params[],
    void* user_data) {
    (void)params;
    (void)user_data;

    uint8_t frame[DFC_READ_DATA_FRAME_SIZE];
    dfc_build_read_data_frame(0x0F, 0, 0, frame);

    uint8_t expected[] = {DFC_CMD_READ_DATA, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    munit_assert_memory_equal(sizeof(expected), frame, expected);
    return MUNIT_OK;
}

static MunitResult test_wrapped_get_version_matches_desfire_detection_probe(
    const MunitParameter params[],
    void* user_data) {
    (void)params;
    (void)user_data;

    uint8_t frame[DFC_WRAPPED_GET_VERSION_FRAME_SIZE];
    dfc_build_wrapped_get_version_frame(frame);

    uint8_t expected[] = {DFC_ISO7816_CLA_WRAPPER, DFC_CMD_GET_VERSION, 0x00, 0x00, 0x00};
    munit_assert_memory_equal(sizeof(expected), frame, expected);
    return MUNIT_OK;
}

static MunitResult test_iso7816_wrapped_response_moves_native_status_to_trailer(
    const MunitParameter params[],
    void* user_data) {
    (void)params;
    (void)user_data;

    uint8_t native_response[] = {DFC_STATUS_OK, 0x00, 0x01, 0x12, 0x30, 0x33, 0x00, 0x00};
    uint8_t output[16];
    size_t output_len = 0;

    munit_assert_true(dfc_wrap_native_response_as_iso7816(
        native_response, sizeof(native_response), 0, output, sizeof(output), &output_len));

    uint8_t expected[] = {0x00, 0x01, 0x12, 0x30, 0x33, 0x00, 0x00, 0x91, DFC_STATUS_OK};
    munit_assert_size(output_len, ==, sizeof(expected));
    munit_assert_memory_equal(sizeof(expected), output, expected);
    return MUNIT_OK;
}

static MunitResult test_iso7816_wrapped_response_preserves_rf_prefix(
    const MunitParameter params[],
    void* user_data) {
    (void)params;
    (void)user_data;

    uint8_t native_response[] = {0x0A, DFC_STATUS_OK, 0xDE, 0xAD};
    uint8_t output[16];
    size_t output_len = 0;

    munit_assert_true(dfc_wrap_native_response_as_iso7816(
        native_response, sizeof(native_response), 1, output, sizeof(output), &output_len));

    uint8_t expected[] = {0x0A, 0xDE, 0xAD, 0x91, DFC_STATUS_OK};
    munit_assert_size(output_len, ==, sizeof(expected));
    munit_assert_memory_equal(sizeof(expected), output, expected);
    return MUNIT_OK;
}

static MunitTest tests[] = {
    {"/select-application-byte-order",
     test_select_application_uses_desfire_byte_order,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/select-picc-level-zero-aid",
     test_select_picc_level_is_zero_aid,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/read-data-little-endian-offset-and-length",
     test_read_data_uses_little_endian_offset_and_length,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/read-data-zero-length",
     test_read_data_zero_length_requests_remaining_file,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/wrapped-get-version-detection-probe",
     test_wrapped_get_version_matches_desfire_detection_probe,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/iso7816-wrapped-response-status-trailer",
     test_iso7816_wrapped_response_moves_native_status_to_trailer,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/iso7816-wrapped-response-preserves-rf-prefix",
     test_iso7816_wrapped_response_preserves_rf_prefix,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite suite = {"/dfc_desfire_protocol", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char* argv[]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
