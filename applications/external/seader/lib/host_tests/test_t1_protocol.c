#include <string.h>

#include "munit.h"
#include "t_1_host_env.h"

static void test_callback(uint32_t event, void* context) {
    (void)context;
    g_t1_host_test_state.callback_call_count++;
    g_t1_host_test_state.last_callback_event = event;
}

static Seader make_test_seader(SeaderUartBridge* uart, SeaderWorker* worker) {
    memset(uart, 0, sizeof(*uart));
    memset(worker, 0, sizeof(*worker));
    worker->uart = uart;
    worker->callback = test_callback;

    Seader seader = {.worker = worker};
    return seader;
}

static MunitResult test_recv_wtx_request_responds(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    /* ISO 7816-3 says "The interface device shall acknowledge by S(WTX response) with the same INF." */
    SeaderUartBridge uart = {0};
    SeaderWorker worker = {0};
    Seader seader = make_test_seader(&uart, &worker);

    t1_host_test_reset();
    uint8_t s_wtx_req[] = {0x00, 0xC3, 0x01, 0x02, 0x00};
    seader_add_lrc(s_wtx_req, 4);
    CCID_Message message = {.payload = s_wtx_req, .dwLength = 5};

    munit_assert_false(seader_recv_t1(&seader, &message));
    munit_assert_size(g_t1_host_test_state.xfrblock_call_count, ==, 1);
    munit_assert_uint8(g_t1_host_test_state.last_frame[1], ==, 0xE3);
    return MUNIT_OK;
}

static MunitResult test_recv_malformed_wtx_rejected(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    /* ISO 7816-3 says "INF shall be present with a single byte in an S-block adjusting IFS and WTX." */
    SeaderUartBridge uart = {0};
    SeaderWorker worker = {0};
    Seader seader = make_test_seader(&uart, &worker);

    t1_host_test_reset();
    uint8_t s_wtx_bad[] = {0x00, 0xC3, 0x00, 0x00};
    seader_add_lrc(s_wtx_bad, 3);
    CCID_Message message = {.payload = s_wtx_bad, .dwLength = 4};

    munit_assert_false(seader_recv_t1(&seader, &message));
    munit_assert_size(g_t1_host_test_state.xfrblock_call_count, ==, 1);
    munit_assert_uint8(g_t1_host_test_state.last_frame[1], ==, 0x81);
    return MUNIT_OK;
}

static MunitResult test_recv_malformed_ifs_rejected(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    /* ISO 7816-3 says "INF shall be present with a single byte in an S-block adjusting IFS and WTX." */
    SeaderUartBridge uart = {0};
    SeaderWorker worker = {0};
    Seader seader = make_test_seader(&uart, &worker);

    t1_host_test_reset();
    uint8_t s_ifs_bad[] = {0x00, 0xC1, 0x00, 0x00};
    seader_add_lrc(s_ifs_bad, 3);
    CCID_Message message = {.payload = s_ifs_bad, .dwLength = 4};

    munit_assert_false(seader_recv_t1(&seader, &message));
    munit_assert_size(g_t1_host_test_state.xfrblock_call_count, ==, 1);
    munit_assert_uint8(g_t1_host_test_state.last_frame[1], ==, 0x81);
    return MUNIT_OK;
}

static MunitResult test_recv_ifs_request_updates_ifsc(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    /* ISO 7816-3 says "The interface device assumes the new IFSC is valid as long as no other IFSC is indicated". */
    SeaderUartBridge uart = {0};
    SeaderWorker worker = {0};
    Seader seader = make_test_seader(&uart, &worker);

    t1_host_test_reset();
    uart.t1.ifsc = 0x20;
    uint8_t s_ifs_req[] = {0x00, 0xC1, 0x01, 0x40, 0x00};
    seader_add_lrc(s_ifs_req, 4);
    CCID_Message message = {.payload = s_ifs_req, .dwLength = 5};

    munit_assert_false(seader_recv_t1(&seader, &message));
    munit_assert_size(g_t1_host_test_state.xfrblock_call_count, ==, 1);
    munit_assert_uint8(g_t1_host_test_state.last_frame[3], ==, 0x40);
    return MUNIT_OK;
}

static MunitResult test_recv_ifs_response_applies_pending_ifsd(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    /* ISO 7816-3 says "The card assumes the new IFSD is valid as long as no other IFSD is indicated". */
    SeaderUartBridge uart = {0};
    SeaderWorker worker = {0};
    Seader seader = make_test_seader(&uart, &worker);

    t1_host_test_reset();
    uart.t1.ifsd = 0x10;
    uart.t1.ifsd_pending = 0x20;
    uint8_t s_ifs_res[] = {0x00, 0xE1, 0x01, 0x20, 0x00};
    seader_add_lrc(s_ifs_res, 4);
    CCID_Message message = {.payload = s_ifs_res, .dwLength = 5};

    munit_assert_false(seader_recv_t1(&seader, &message));
    munit_assert_uint8(uart.t1.ifsd, ==, 0x20);
    munit_assert_uint8(uart.t1.ifsd_pending, ==, 0x00);
    return MUNIT_OK;
}

static MunitResult test_recv_ifs_response_mismatch_rejected(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    /* ISO 7816-3 says the response acknowledges "with the same INF", so mismatched IFS bytes are invalid. */
    SeaderUartBridge uart = {0};
    SeaderWorker worker = {0};
    Seader seader = make_test_seader(&uart, &worker);

    t1_host_test_reset();
    uart.t1.ifsd_pending = 0x20;
    uint8_t s_ifs_res_bad[] = {0x00, 0xE1, 0x01, 0x30, 0x00};
    seader_add_lrc(s_ifs_res_bad, 4);
    CCID_Message message = {.payload = s_ifs_res_bad, .dwLength = 5};

    munit_assert_false(seader_recv_t1(&seader, &message));
    munit_assert_size(g_t1_host_test_state.send_version_call_count, ==, 0);
    munit_assert_size(g_t1_host_test_state.callback_call_count, ==, 0);
    munit_assert_size(g_t1_host_test_state.xfrblock_call_count, ==, 1);
    munit_assert_uint8(g_t1_host_test_state.last_frame[1], ==, 0x81);
    return MUNIT_OK;
}

static MunitResult test_recv_i_block_too_large_rejected(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    /* ISO 7816-3 says each piece must have length "less than or equal to IFSC or IFSD". */
    SeaderUartBridge uart = {0};
    SeaderWorker worker = {0};
    Seader seader = make_test_seader(&uart, &worker);

    t1_host_test_reset();
    uart.t1.ifsd = 2;
    uart.t1.recv_pcb = 0x00;
    uint8_t i_block[] = {0x00, 0x00, 0x03, 0xAA, 0xBB, 0xCC, 0x00};
    seader_add_lrc(i_block, 6);
    CCID_Message message = {.payload = i_block, .dwLength = 7};

    munit_assert_false(seader_recv_t1(&seader, &message));
    munit_assert_size(g_t1_host_test_state.process_call_count, ==, 0);
    munit_assert_size(g_t1_host_test_state.xfrblock_call_count, ==, 1);
    munit_assert_uint8(g_t1_host_test_state.last_frame[1], ==, 0x81);
    return MUNIT_OK;
}

static MunitResult test_recv_r_block_nack_retransmits(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    /* ISO 7816-3 says an invalid block leads to an R-block that "requests with its N(R) for the expected I-block". */
    SeaderUartBridge uart = {0};
    SeaderWorker worker = {0};
    Seader seader = make_test_seader(&uart, &worker);

    t1_host_test_reset();
    uart.t1.send_pcb = 0x00;
    uart.t1.ifsc = 4;
    uart.t1.tx_buffer = bit_buffer_alloc(8);
    bit_buffer_copy_bytes(uart.t1.tx_buffer, (const uint8_t*)"\xA0\xA1\xA2", 3);
    uart.t1.tx_buffer_offset = 2;
    uart.t1.last_tx_len = 2;
    uint8_t r_block[] = {0x00, 0x80, 0x00, 0x00};
    seader_add_lrc(r_block, 3);
    CCID_Message message = {.payload = r_block, .dwLength = 4};

    munit_assert_false(seader_recv_t1(&seader, &message));
    munit_assert_size(g_t1_host_test_state.xfrblock_call_count, ==, 1);
    munit_assert_uint8(g_t1_host_test_state.last_frame[1], ==, 0x00);
    bit_buffer_free(uart.t1.tx_buffer);
    return MUNIT_OK;
}

static MunitResult test_recv_r_block_invalid_retransmit_state_errors(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    /* ISO 7816-3 ties R-block recovery to "the expected I-block"; without a prior chunk, retransmit state is invalid. */
    SeaderUartBridge uart = {0};
    SeaderWorker worker = {0};
    Seader seader = make_test_seader(&uart, &worker);

    t1_host_test_reset();
    uart.t1.send_pcb = 0x00;
    uart.t1.tx_buffer = bit_buffer_alloc(8);
    uart.t1.tx_buffer_offset = 0;
    uart.t1.last_tx_len = 2;
    uint8_t r_block[] = {0x00, 0x91, 0x00, 0x00};
    seader_add_lrc(r_block, 3);
    CCID_Message message = {.payload = r_block, .dwLength = 4};

    munit_assert_false(seader_recv_t1(&seader, &message));
    munit_assert_size(g_t1_host_test_state.xfrblock_call_count, ==, 0);
    bit_buffer_free(uart.t1.tx_buffer);
    return MUNIT_OK;
}

static MunitTest test_t1_regression_cases[] = {
    {(char*)"/recv/wtx-request-responds",
     test_recv_wtx_request_responds,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/recv/malformed-wtx-len-zero-rejected",
     test_recv_malformed_wtx_rejected,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/recv/malformed-ifs-len-zero-rejected",
     test_recv_malformed_ifs_rejected,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/recv/ifs-request-updates-ifsc",
     test_recv_ifs_request_updates_ifsc,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/recv/ifs-response-applies-pending-ifsd",
     test_recv_ifs_response_applies_pending_ifsd,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/recv/ifs-response-mismatch-rejected",
     test_recv_ifs_response_mismatch_rejected,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/recv/i-block-too-large-rejected",
     test_recv_i_block_too_large_rejected,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/recv/r-block-nack-retransmits",
     test_recv_r_block_nack_retransmits,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/recv/r-block-invalid-retransmit-state-errors",
     test_recv_r_block_invalid_retransmit_state_errors,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {NULL, NULL, NULL, NULL, 0, NULL},
};

MunitSuite test_t1_protocol_suite = {
    "",
    test_t1_regression_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
