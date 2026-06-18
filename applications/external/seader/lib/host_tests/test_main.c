#include "munit.h"

extern MunitSuite test_lrc_suite;
extern MunitSuite test_board_identity_suite;
extern MunitSuite test_board_power_lifecycle_suite;
extern MunitSuite test_hf_read_lifecycle_suite;
extern MunitSuite test_hf_buffer_pool_suite;
extern MunitSuite test_sam_startup_ui_suite;
extern MunitSuite test_ccid_logic_suite;
extern MunitSuite test_uart_tx_logic_suite;
extern MunitSuite test_uart_rx_logic_suite;
extern MunitSuite test_sam_key_label_suite;
extern MunitSuite test_t1_existing_suite;
extern MunitSuite test_t1_protocol_suite;
extern MunitSuite test_snmp_suite;
extern MunitSuite test_uhf_status_label_suite;
extern MunitSuite test_credential_sio_label_suite;
extern MunitSuite test_hf_read_plan_suite;
extern MunitSuite test_hf_bridge_policy_suite;
extern MunitSuite test_runtime_policy_suite;
extern MunitSuite test_wiegand_plugin_suite;
extern MunitSuite test_ui_memory_policy_suite;
extern MunitSuite test_hf_14a_session_suite;
extern MunitSuite test_hf_sam_response_view_suite;

int main(int argc, char* argv[]) {
    MunitSuite child_suites[] = {
        {"/lrc", test_lrc_suite.tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
        {"/board-identity", test_board_identity_suite.tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
        {"/board-power-lifecycle",
         test_board_power_lifecycle_suite.tests,
         NULL,
         1,
         MUNIT_SUITE_OPTION_NONE},
        {"/hf-read-lifecycle",
         test_hf_read_lifecycle_suite.tests,
         NULL,
         1,
         MUNIT_SUITE_OPTION_NONE},
        {"/hf-buffer-pool", test_hf_buffer_pool_suite.tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
        {"/sam-startup-ui",
         test_sam_startup_ui_suite.tests,
         NULL,
         1,
         MUNIT_SUITE_OPTION_NONE},
        {"/sam-key-label", test_sam_key_label_suite.tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
        {"/t1", test_t1_existing_suite.tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
        {"/t1", test_t1_protocol_suite.tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
        {"/ccid", test_ccid_logic_suite.tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
        {"/uart-tx", test_uart_tx_logic_suite.tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
        {"/uart-rx", test_uart_rx_logic_suite.tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
        {"/snmp", test_snmp_suite.tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
        {"/uhf-status-label", test_uhf_status_label_suite.tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
        {"/credential-sio-label", test_credential_sio_label_suite.tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
        {"/hf-read-plan", test_hf_read_plan_suite.tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
        {"/hf-bridge-policy", test_hf_bridge_policy_suite.tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
        {"/hf-14a-session", test_hf_14a_session_suite.tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
        {"/hf-sam-response-view",
         test_hf_sam_response_view_suite.tests,
         NULL,
         1,
         MUNIT_SUITE_OPTION_NONE},
        {"/runtime-policy", test_runtime_policy_suite.tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
        {"/wiegand-plugin", test_wiegand_plugin_suite.tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
        {"/ui-memory-policy",
         test_ui_memory_policy_suite.tests,
         NULL,
         1,
         MUNIT_SUITE_OPTION_NONE},
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
