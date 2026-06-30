#include <string.h>

#include "munit.h"
#include "t_1_host_env.h"

static size_t test_hex_to_bytes(const char* hex, uint8_t* out, size_t out_size) {
    size_t len = 0U;
    int high_nibble = -1;

    for(const char* p = hex; *p; ++p) {
        int value = -1;
        if(*p >= '0' && *p <= '9') value = *p - '0';
        else if(*p >= 'A' && *p <= 'F') value = *p - 'A' + 10;
        else if(*p >= 'a' && *p <= 'f') value = *p - 'a' + 10;
        else continue;

        if(high_nibble < 0) {
            high_nibble = value;
        } else {
            munit_assert_size(len, <, out_size);
            out[len++] = (uint8_t)((high_nibble << 4) | value);
            high_nibble = -1;
        }
    }

    munit_assert_int(high_nibble, ==, -1);
    return len;
}

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

static MunitResult test_recv_live_uhf_config_chained_blocks(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    static const char* expected_apdu_hex =
        "0A4400000000BD81FB8A81F8308200F40201033082000E02010002010004020000020201010482003A30820036041B2B0601040181E438010103050F8C9088CDA2A8D885C0B298D7FF7F020105020103040D2B0601040181E438010104080F040004003082009D041B2B0601040181E438010103050F8C9088CDA2A8D885C0B298D7FF7F040D2B0601040181E438010104080FA282006D020100020100020100308200603082005C0606030107030B00045204E2003412112B0601040181E438010102012201010101112B0601040181E43801010201220101020104E2801105112B0601040181E438010102011E01010101112B0601040181E438010102011E010102019000";

    SeaderUartBridge uart = {0};
    SeaderWorker worker = {0};
    Seader seader = make_test_seader(&uart, &worker);
    uint8_t first_block[300] = {0};
    uint8_t final_block[64] = {0};
    uint8_t expected_apdu[300] = {0};
    CCID_Message first_message = {0};
    CCID_Message final_message = {0};
    size_t expected_apdu_len = 0U;
    const size_t first_inf_len = 0xECU;
    const size_t final_inf_len = 0x1AU;

    t1_host_test_reset();
    uart.t1.ifsd = 0xFE;
    uart.t1.recv_pcb = 0x00;

    expected_apdu_len =
        test_hex_to_bytes(expected_apdu_hex, expected_apdu, sizeof(expected_apdu));
    munit_assert_size(expected_apdu_len, ==, first_inf_len + final_inf_len);

    first_block[0] = 0x00;
    first_block[1] = 0x20;
    first_block[2] = (uint8_t)first_inf_len;
    memcpy(first_block + 3, expected_apdu, first_inf_len);
    seader_add_lrc(first_block, 3 + first_inf_len);

    final_block[0] = 0x00;
    final_block[1] = 0x40;
    final_block[2] = (uint8_t)final_inf_len;
    memcpy(final_block + 3, expected_apdu + first_inf_len, final_inf_len);
    seader_add_lrc(final_block, 3 + final_inf_len);

    first_message.payload = first_block;
    first_message.dwLength = 3 + first_inf_len + 1;
    munit_assert_false(seader_recv_t1(&seader, &first_message));
    munit_assert_size(g_t1_host_test_state.xfrblock_call_count, ==, 1);
    munit_assert_uint8(g_t1_host_test_state.last_frame[0], ==, 0x00);
    munit_assert_uint8(g_t1_host_test_state.last_frame[1], ==, 0x90);
    munit_assert_uint8(g_t1_host_test_state.last_frame[2], ==, 0x00);

    final_message.payload = final_block;
    final_message.dwLength = 3 + final_inf_len + 1;
    munit_assert_true(seader_recv_t1(&seader, &final_message));
    munit_assert_size(g_t1_host_test_state.process_call_count, ==, 1);
    munit_assert_size(g_t1_host_test_state.last_apdu_len, ==, expected_apdu_len);
    munit_assert_memory_equal(
        expected_apdu_len, g_t1_host_test_state.last_apdu, expected_apdu);
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
    {(char*)"/recv/live-uhf-config-chained-blocks",
     test_recv_live_uhf_config_chained_blocks,
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
