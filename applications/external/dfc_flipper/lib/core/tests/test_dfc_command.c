// Reader-side command encoder: the octets each command puts on the wire.

#include "munit/munit.h"

#include "dfc_command.h"

#include <string.h>

static size_t hex(const char* text, uint8_t* out) {
    size_t n = 0;
    for(size_t i = 0; text[i] && text[i + 1]; i += 2) {
        unsigned value = 0;
        for(size_t j = 0; j < 2; j++) {
            char c = text[i + j];
            value <<= 4;
            if(c >= '0' && c <= '9') value |= (unsigned)(c - '0');
            else if(c >= 'A' && c <= 'F') value |= (unsigned)(c - 'A' + 10);
            else if(c >= 'a' && c <= 'f') value |= (unsigned)(c - 'a' + 10);
        }
        out[n++] = (uint8_t)value;
    }
    return n;
}

static void assert_apdu(const DfcCommand* command, const char* expected_hex) {
    uint8_t expected[DFC_COMMAND_MAX_APDU];
    size_t expected_len = hex(expected_hex, expected);
    uint8_t apdu[DFC_COMMAND_MAX_APDU];
    size_t apdu_len = 0;
    munit_assert_int(dfc_command_to_apdu(command, apdu, sizeof(apdu), &apdu_len), ==, DfcCommandOk);
    munit_assert_size(apdu_len, ==, expected_len);
    munit_assert_memory_equal(apdu_len, apdu, expected);
}

static MunitResult test_simple_commands(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCommand c;
    munit_assert_int(dfc_command_get_version(&c), ==, DfcCommandOk);
    assert_apdu(&c, "9060000000");
    munit_assert_int(dfc_command_format_picc(&c), ==, DfcCommandOk);
    assert_apdu(&c, "90FC000000");
    munit_assert_int(dfc_command_get_free_memory(&c), ==, DfcCommandOk);
    assert_apdu(&c, "906E000000");
    munit_assert_int(dfc_command_notify_transaction_success(&c), ==, DfcCommandOk);
    assert_apdu(&c, "90EE000000");
    munit_assert_int(dfc_command_prepare_proximity_check(&c), ==, DfcCommandOk);
    assert_apdu(&c, "90F0000000");
    munit_assert_int(dfc_command_get_key_set_versions(&c), ==, DfcCommandOk);
    assert_apdu(&c, "90640000024080" "00");
    munit_assert_int(dfc_command_clear_record_file(&c, 2), ==, DfcCommandOk);
    assert_apdu(&c, "90EB0000010200");
    munit_assert_int(dfc_command_commit_transaction_with_option(&c, 1), ==, DfcCommandOk);
    assert_apdu(&c, "90C70000010100");
    munit_assert_int(dfc_command_roll_key_set(&c, 1), ==, DfcCommandOk);
    assert_apdu(&c, "90550000010100");
    munit_assert_int(dfc_command_initialize_key_set(&c, 1, 2), ==, DfcCommandOk);
    assert_apdu(&c, "9056000002010200");
    munit_assert_int(dfc_command_finalize_key_set(&c, 1, 2), ==, DfcCommandOk);
    assert_apdu(&c, "9057000002010200");
    return MUNIT_OK;
}

static MunitResult test_application_commands(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCommand c;
    const uint8_t aid[3] = {0x4F, 0x49, 0xD3};
    const uint8_t second[3] = {0x01, 0x02, 0x03};
    munit_assert_int(dfc_command_select_application(&c, aid, NULL), ==, DfcCommandOk);
    assert_apdu(&c, "905A0000034F49D300");
    munit_assert_int(dfc_command_select_application(&c, aid, second), ==, DfcCommandOk);
    assert_apdu(&c, "905A0000064F49D301020300");

    DfcCommandCreateApplication app = {0};
    memcpy(app.aid, aid, 3);
    app.key_settings_1 = 0x0F;
    app.key_settings_2 = 0x04;
    munit_assert_int(dfc_command_create_application(&c, &app), ==, DfcCommandOk);
    assert_apdu(&c, "90CA0000054F49D30F0400");

    app.key_settings_1 = 0x0B;
    app.has_iso_file_id = true;
    app.iso_file_id = 0xE110;
    munit_assert_int(dfc_command_create_application(&c, &app), ==, DfcCommandOk);
    assert_apdu(&c, "90CA0000074F49D30B2410E100");

    const uint8_t name[] = {0xA0, 0x00, 0x00, 0x04, 0x40, 0x00, 0x01, 0x01};
    app.df_name = name;
    app.df_name_len = sizeof(name);
    munit_assert_int(dfc_command_create_application(&c, &app), ==, DfcCommandOk);
    assert_apdu(&c, "90CA00000F4F49D30B2410E1A00000044000010100");

    // A DF name needs an ISO file identifier, and reserved identifiers are refused.
    app.has_iso_file_id = false;
    munit_assert_int(dfc_command_create_application(&c, &app), ==, DfcCommandInvalid);
    app.has_iso_file_id = true;
    app.iso_file_id = 0x3F00;
    munit_assert_int(dfc_command_create_application(&c, &app), ==, DfcCommandInvalid);

    DfcCommandCreateApplication sets = {0};
    const uint8_t aid_sets[3] = {0xE2, 0xE2, 0xE2};
    memcpy(sets.aid, aid_sets, 3);
    sets.key_settings_1 = 0x0F;
    sets.key_settings_2 = 0x82;
    sets.has_extended_settings = true;
    sets.extended_settings = 0x01;
    sets.has_key_sets = true;
    sets.key_set_version = 0x00;
    sets.key_set_count = 0x02;
    sets.key_set_max_key_size = DFC_KEY_SET_MAXIMUM_16_BYTE;
    sets.key_set_roll_key_no = 0x00;
    munit_assert_int(dfc_command_create_application(&c, &sets), ==, DfcCommandOk);
    assert_apdu(&c, "90CA00000AE2E2E20F92010002100000");
    sets.key_set_count = 1;
    munit_assert_int(dfc_command_create_application(&c, &sets), ==, DfcCommandInvalid);
    return MUNIT_OK;
}

static MunitResult test_file_commands(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCommand c;
    DfcCommandDataFile standard = {0x0F, false, 0, 0x01, 0x1230, 0x21};
    munit_assert_int(dfc_command_create_standard_data_file(&c, &standard), ==, DfcCommandOk);
    assert_apdu(&c, "90CD0000070F01301221000000");
    DfcCommandDataFile backup = {1, false, 0, 3, 0x1234, 0x1020};
    munit_assert_int(dfc_command_create_backup_data_file(&c, &backup), ==, DfcCommandOk);
    assert_apdu(&c, "90CB0000070103341220100000");

    DfcCommandRecordFile linear = {2, false, 0, 1, 0x4321, 8, 12, false};
    munit_assert_int(dfc_command_create_record_file(&c, &linear), ==, DfcCommandOk);
    assert_apdu(&c, "90C100000A020121430800000C000000");
    DfcCommandRecordFile cyclic = {3, false, 0, 0, 0xEEEE, 4, 2, true};
    munit_assert_int(dfc_command_create_record_file(&c, &cyclic), ==, DfcCommandOk);
    assert_apdu(&c, "90C000000A0300EEEE04000002000000");

    DfcCommandTransactionMacFile tmac = {4, 0, 0x0FF0, 2, {0}, 1};
    uint8_t key[16];
    hex("00112233445566778899AABBCCDDEEFF", key);
    memcpy(tmac.key, key, sizeof(key));
    munit_assert_int(dfc_command_create_transaction_mac_file(&c, &tmac), ==, DfcCommandOk);
    assert_apdu(&c, "90CE0000160400F00F0200112233445566778899AABBCCDDEEFF0100");

    DfcCommandValueFile value = {5, 0x03, 0x0000, -10, 1000, 100, 1};
    munit_assert_int(dfc_command_create_value_file(&c, &value), ==, DfcCommandOk);
    assert_apdu(&c, "90CC00001105030000" "F6FFFFFF" "E8030000" "64000000" "0100");
    value.value = 2000;
    munit_assert_int(dfc_command_create_value_file(&c, &value), ==, DfcCommandInvalid);

    munit_assert_int(dfc_command_read_data(&c, 0x0F, 0, 3), ==, DfcCommandOk);
    assert_apdu(&c, "90BD0000070F00000003000000");
    const uint8_t payload[] = {0x01, 0x02, 0x03};
    munit_assert_int(dfc_command_write_data(&c, 0x0F, 0, payload, sizeof(payload)), ==, DfcCommandOk);
    assert_apdu(&c, "903D00000A0F00000003000001020300");
    munit_assert_int(dfc_command_read_records(&c, 2, 1, 3, false), ==, DfcCommandOk);
    assert_apdu(&c, "90BB0000070201000003000000");
    const uint8_t record[] = {0xDE, 0xAD, 0xBE, 0xEF};
    munit_assert_int(dfc_command_write_record(&c, 2, 0, record, sizeof(record), false), ==, DfcCommandOk);
    assert_apdu(&c, "903B00000B02000000040000DEADBEEF00");
    const uint8_t update[] = {0xA1, 0xA2, 0xA3};
    munit_assert_int(dfc_command_update_record(&c, 2, 1, 2, update, sizeof(update), false), ==, DfcCommandOk);
    assert_apdu(&c, "90DB00000D02010000020000030000A1A2A300");
    munit_assert_int(dfc_command_read_data(&c, 1, DFC_COMMAND_UINT24_MAX + 1, 1), ==, DfcCommandInvalid);
    return MUNIT_OK;
}

static MunitResult test_value_and_key_commands(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCommand c;
    munit_assert_int(dfc_command_credit(&c, 4, 25), ==, DfcCommandOk);
    assert_apdu(&c, "900C0000050419000000" "00");
    munit_assert_int(dfc_command_debit(&c, 4, -1), ==, DfcCommandOk);
    assert_apdu(&c, "90DC00000504FFFFFFFF00");
    uint8_t reader_id[16];
    hex("00112233445566778899AABBCCDDEEFF", reader_id);
    munit_assert_int(dfc_command_commit_reader_id(&c, reader_id), ==, DfcCommandOk);
    assert_apdu(&c, "90C800001000112233445566778899AABBCCDDEEFF00");

    const uint8_t cryptogram[] = {0xAA, 0xBB, 0xCC};
    munit_assert_int(dfc_command_change_key_ev2(&c, 1, 2, cryptogram, sizeof(cryptogram)), ==, DfcCommandOk);
    assert_apdu(&c, "90C60000050102AABBCC00");
    munit_assert_int(dfc_command_change_key(&c, 1, cryptogram, 0), ==, DfcCommandInvalid);

    const uint8_t config[] = {0x01, 0x02};
    munit_assert_int(dfc_command_set_configuration(&c, 0, config, sizeof(config)), ==, DfcCommandOk);
    assert_apdu(&c, "905C00000300010200");
    const uint8_t check[] = {0x01, 0x02, 0x03};
    munit_assert_int(dfc_command_proximity_check(&c, check, sizeof(check)), ==, DfcCommandOk);
    assert_apdu(&c, "90F200000301020300");
    const uint8_t verify[] = {0xAA, 0xBB};
    munit_assert_int(dfc_command_verify_proximity_check(&c, verify, sizeof(verify)), ==, DfcCommandOk);
    assert_apdu(&c, "90FD000002AABB00");
    return MUNIT_OK;
}

static MunitResult test_authentication_commands(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCommand c;
    munit_assert_int(dfc_command_authenticate(&c, DFC_CMD_AUTHENTICATE_AES, 1), ==, DfcCommandOk);
    assert_apdu(&c, "90AA0000010100");
    munit_assert_int(dfc_command_authenticate(&c, 0x42, 1), ==, DfcCommandInvalid);
    const uint8_t caps[6] = {0};
    munit_assert_int(dfc_command_authenticate_ev2_first(&c, 0, caps, sizeof(caps)), ==, DfcCommandOk);
    assert_apdu(&c, "9071000008000600000000000000");
    munit_assert_int(dfc_command_authenticate_ev2_first(&c, 0, NULL, 0), ==, DfcCommandOk);
    assert_apdu(&c, "907100000200" "0000");
    munit_assert_int(dfc_command_authenticate_ev2_first(&c, 0, caps, 3), ==, DfcCommandInvalid);
    munit_assert_int(dfc_command_authenticate_ev2_non_first(&c, 1), ==, DfcCommandOk);
    assert_apdu(&c, "90770000010100");
    return MUNIT_OK;
}

static MunitResult test_framing_and_bounds(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCommand c;
    uint8_t big[DFC_COMMAND_MAX_DATA + 1];
    memset(big, 0x5A, sizeof(big));
    munit_assert_int(dfc_command_encode_raw(&c, 0x3D, big, DFC_COMMAND_MAX_DATA), ==, DfcCommandOk);
    munit_assert_int(dfc_command_encode_raw(&c, 0x3D, big, sizeof(big)), ==, DfcCommandTooLong);
    munit_assert_int(dfc_command_write_data(&c, 1, 0, big, DFC_COMMAND_MAX_DATA - 7), ==, DfcCommandOk);
    munit_assert_int(dfc_command_write_data(&c, 1, 0, big, DFC_COMMAND_MAX_DATA - 6), ==, DfcCommandTooLong);

    munit_assert_int(dfc_command_read_data(&c, 0x0F, 0, 3), ==, DfcCommandOk);
    uint8_t frame[16];
    size_t frame_len = 0;
    munit_assert_int(dfc_command_to_frame(&c, frame, sizeof(frame), &frame_len), ==, DfcCommandOk);
    uint8_t expected[16];
    size_t expected_len = hex("BD0F00000003000000", expected);
    munit_assert_size(frame_len, ==, expected_len - 1);
    munit_assert_memory_equal(frame_len, frame, expected);
    munit_assert_int(dfc_command_to_apdu(&c, frame, 5, &frame_len), ==, DfcCommandTooLong);
    return MUNIT_OK;
}

static MunitTest tests[] = {
    {"/simple", test_simple_commands, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/applications", test_application_commands, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/files", test_file_commands, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/values-and-keys", test_value_and_key_commands, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/authentication", test_authentication_commands, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/framing-and-bounds", test_framing_and_bounds, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite suite = {"/dfc_command", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char* argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
