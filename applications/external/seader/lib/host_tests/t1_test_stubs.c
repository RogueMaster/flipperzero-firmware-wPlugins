#include "t_1_host_env.h"

T1HostTestState g_t1_host_test_state = {0};

void t1_host_test_reset(void) {
    /* Default to success so tests opt into failure only when needed. */
    memset(&g_t1_host_test_state, 0, sizeof(g_t1_host_test_state));
    g_t1_host_test_state.process_return_value = true;
}

void seader_ccid_XfrBlock(SeaderUartBridge* seader_uart, uint8_t* data, size_t len) {
    (void)seader_uart;
    /* Record the exact frame the production code asked CCID to transmit. */
    g_t1_host_test_state.xfrblock_call_count++;
    g_t1_host_test_state.last_frame_len = len;
    memcpy(g_t1_host_test_state.last_frame, data, len);
}

bool seader_worker_process_sam_message(Seader* seader, uint8_t* apdu, uint32_t len) {
    (void)seader;
    /* Capture the final APDU bytes that would be handed to the worker layer. */
    g_t1_host_test_state.process_call_count++;
    g_t1_host_test_state.last_apdu_len = len;
    memcpy(g_t1_host_test_state.last_apdu, apdu, len);
    return g_t1_host_test_state.process_return_value;
}

void seader_worker_send_version(Seader* seader) {
    (void)seader;
    g_t1_host_test_state.send_version_call_count++;
}

void seader_abort_active_read_with_reason(
    Seader* seader,
    SeaderHfReadFailureReason reason,
    const char* detail) {
    g_t1_host_test_state.abort_call_count++;
    seader->hf_read_failure_reason = reason;
    if(detail && detail[0] != '\0') {
        strncpy(seader->read_error, detail, sizeof(seader->read_error) - 1U);
    } else {
        strncpy(
            seader->read_error,
            seader_hf_read_failure_reason_text(reason),
            sizeof(seader->read_error) - 1U);
    }
}
