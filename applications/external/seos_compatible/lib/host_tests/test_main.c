#include "munit.h"

extern MunitSuite test_allocation_suite;
extern MunitSuite test_credential_file_suite;
extern MunitSuite test_tlv_suite;
extern MunitSuite test_iso14443_4_suite;
extern MunitSuite test_cmac_suite;
extern MunitSuite test_kdf_suite;
extern MunitSuite test_secure_messaging_suite;
extern MunitSuite test_large_messages_suite;
extern MunitSuite test_protocol_suite;
extern MunitSuite test_reader_parse_suite;
extern MunitSuite test_sio_collect_suite;
extern MunitSuite test_sm_command_suite;
extern MunitSuite test_write_response_suite;
extern MunitSuite test_ble_policy_suite;
extern MunitSuite test_ble_framing_suite;
extern MunitSuite test_session_vectors_suite;
extern MunitSuite test_emulated_card_suite;
extern MunitSuite test_select_adf_suite;

int main(int argc, char* argv[]) {
    MunitSuite child_suites[] = {
        {(char*)"/allocation",
         test_allocation_suite.tests,
         NULL,
         1,
         MUNIT_SUITE_OPTION_NONE},
        {(char*)"/credential-file",
         test_credential_file_suite.tests,
         NULL,
         1,
         MUNIT_SUITE_OPTION_NONE},
        {(char*)"/tlv", test_tlv_suite.tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
        {(char*)"/iso14443-4",
         test_iso14443_4_suite.tests,
         NULL,
         1,
         MUNIT_SUITE_OPTION_NONE},
        {(char*)"/cmac", test_cmac_suite.tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
        {(char*)"/kdf", test_kdf_suite.tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
        {(char*)"/secure-messaging",
         test_secure_messaging_suite.tests,
         NULL,
         1,
         MUNIT_SUITE_OPTION_NONE},
        {(char*)"/large-messages",
         test_large_messages_suite.tests,
         NULL,
         1,
         MUNIT_SUITE_OPTION_NONE},
        {(char*)"/protocol", test_protocol_suite.tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
        {(char*)"/reader-parse",
         test_reader_parse_suite.tests,
         NULL,
         1,
         MUNIT_SUITE_OPTION_NONE},
        {(char*)"/sio-collect",
         test_sio_collect_suite.tests,
         NULL,
         1,
         MUNIT_SUITE_OPTION_NONE},
        {(char*)"/sm-command", test_sm_command_suite.tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
        {(char*)"/write-response",
         test_write_response_suite.tests,
         NULL,
         1,
         MUNIT_SUITE_OPTION_NONE},
        {(char*)"/ble-policy", test_ble_policy_suite.tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
        {(char*)"/ble-framing", test_ble_framing_suite.tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
        {(char*)"/session-vectors",
         test_session_vectors_suite.tests,
         NULL,
         1,
         MUNIT_SUITE_OPTION_NONE},
        {(char*)"/emulated-card", test_emulated_card_suite.tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
        {(char*)"/select-adf", test_select_adf_suite.tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
        {NULL, NULL, NULL, 0, MUNIT_SUITE_OPTION_NONE},
    };
    MunitSuite main_suite = {
        (char*)"",
        NULL,
        child_suites,
        1,
        MUNIT_SUITE_OPTION_NONE,
    };
    return munit_suite_main(&main_suite, NULL, argc, argv);
}
