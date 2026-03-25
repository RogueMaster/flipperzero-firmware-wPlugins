#include "munit.h"

extern MunitSuite test_lrc_suite;
extern MunitSuite test_ccid_logic_suite;
extern MunitSuite test_sam_key_label_suite;
extern MunitSuite test_t1_existing_suite;
extern MunitSuite test_t1_protocol_suite;
extern MunitSuite test_snmp_suite;
extern MunitSuite test_uhf_status_label_suite;
extern MunitSuite test_credential_sio_label_suite;
extern MunitSuite test_runtime_policy_suite;

int main(int argc, char* argv[]) {
    MunitSuite child_suites[] = {
        {"/lrc", test_lrc_suite.tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
        {"/sam-key-label", test_sam_key_label_suite.tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
        {"/t1", test_t1_existing_suite.tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
        {"/t1", test_t1_protocol_suite.tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
        {"/ccid", test_ccid_logic_suite.tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
        {"/snmp", test_snmp_suite.tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
        {"/uhf-status-label", test_uhf_status_label_suite.tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
        {"/credential-sio-label", test_credential_sio_label_suite.tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
        {"/runtime-policy", test_runtime_policy_suite.tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
        {NULL, NULL, NULL, 0, 0},
    };
    MunitSuite main_suite = {
        "",
        NULL,
        child_suites,
        1,
        MUNIT_SUITE_OPTION_NONE,
    };
    return munit_suite_main(&main_suite, NULL, argc, argv);
}
