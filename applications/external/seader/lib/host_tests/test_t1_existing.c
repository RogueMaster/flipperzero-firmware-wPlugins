#include <string.h>

#include "munit.h"
#include "t_1_host_env.h"

/* The production worker reports SAM-present through a callback; the harness just records it. */
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

static bool last_frame_prefix_matches(const uint8_t* expected, size_t len) {
    return g_t1_host_test_state.last_frame_len >= len &&
           memcmp(g_t1_host_test_state.last_frame, expected, len) == 0;
}

static bool last_apdu_matches(const uint8_t* expected, size_t len) {
    return g_t1_host_test_state.last_apdu_len == len &&
           memcmp(g_t1_host_test_state.last_apdu, expected, len) == 0;
}

static MunitResult test_send_ifs_request(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    SeaderUartBridge uart = {0};
    SeaderWorker worker = {0};
    Seader seader = make_test_seader(&uart, &worker);

    t1_host_test_reset();
    uart.t1.ifsc = 0x20;
    uart.t1.nad = 0x00;
    uart.t1.send_pcb = SEADER_T1_PCB_SEQUENCE_BIT;

    seader_t_1_set_IFSD(&seader);
    munit_assert_size(g_t1_host_test_state.xfrblock_call_count, ==, 1);
    munit_assert_size(g_t1_host_test_state.last_frame_len, ==, 5);
    munit_assert_true(last_frame_prefix_matches((const uint8_t*)"\x00\xC1\x01\x20", 4));
    munit_assert_true(
        seader_validate_lrc(g_t1_host_test_state.last_frame, g_t1_host_test_state.last_frame_len));
    return MUNIT_OK;
}

static MunitResult test_send_single_block(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    SeaderUartBridge uart = {0};
    SeaderWorker worker = {0};
    make_test_seader(&uart, &worker);

    t1_host_test_reset();
    uart.t1.ifsc = 0x20;
    uart.t1.send_pcb = SEADER_T1_PCB_SEQUENCE_BIT;
    uint8_t apdu_short[] = {0xDE, 0xAD, 0xBE};

    seader_send_t1(&uart, apdu_short, sizeof(apdu_short));
    munit_assert_size(g_t1_host_test_state.xfrblock_call_count, ==, 1);
    munit_assert_size(g_t1_host_test_state.last_frame_len, ==, 7);
    munit_assert_true(last_frame_prefix_matches((const uint8_t*)"\x00\x00\x03", 3));
    munit_assert_memory_equal(sizeof(apdu_short), g_t1_host_test_state.last_frame + 3, apdu_short);
    munit_assert_uint8(uart.t1.send_pcb, ==, 0x00);
    return MUNIT_OK;
}

static MunitResult test_send_scratchpad_block(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    SeaderUartBridge uart = {0};
    SeaderWorker worker = {0};
    make_test_seader(&uart, &worker);

    t1_host_test_reset();
    uart.t1.ifsc = 0x20;
    uart.t1.send_pcb = SEADER_T1_PCB_SEQUENCE_BIT;
    uint8_t* scratchpad_apdu = uart.tx_buf + 3;
    scratchpad_apdu[0] = 0x01;
    scratchpad_apdu[1] = 0x02;

    seader_send_t1(&uart, scratchpad_apdu, 2);
    munit_assert_size(g_t1_host_test_state.xfrblock_call_count, ==, 1);
    munit_assert_true(last_frame_prefix_matches((const uint8_t*)"\x00\x00\x02\x01\x02", 5));
    return MUNIT_OK;
}

static MunitResult test_send_chained_block(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    SeaderUartBridge uart = {0};
    SeaderWorker worker = {0};
    make_test_seader(&uart, &worker);

    t1_host_test_reset();
    uart.t1.send_pcb = SEADER_T1_PCB_SEQUENCE_BIT;
    uart.t1.ifsc = 2;
    uint8_t apdu_long[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};

    seader_send_t1(&uart, apdu_long, sizeof(apdu_long));
    munit_assert_size(g_t1_host_test_state.xfrblock_call_count, ==, 1);
    munit_assert_uint8(
        g_t1_host_test_state.last_frame[1], ==, (SEADER_T1_PCB_I_BLOCK_MORE | 0x00));
    munit_assert_uint8(g_t1_host_test_state.last_frame[2], ==, 2);
    munit_assert_not_null(uart.t1.tx_buffer);
    munit_assert_size(uart.t1.tx_buffer_offset, ==, 2);

    bit_buffer_free(uart.t1.tx_buffer);
    return MUNIT_OK;
}

static MunitResult test_recv_ifs_response(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    SeaderUartBridge uart = {0};
    SeaderWorker worker = {0};
    Seader seader = make_test_seader(&uart, &worker);

    t1_host_test_reset();
    uint8_t ifs_response[] = {0x00, 0xE1, 0x01, 0x20, 0x00};
    seader_add_lrc(ifs_response, 4);
    CCID_Message ifs_message = {.payload = ifs_response, .dwLength = 5};

    munit_assert_false(seader_recv_t1(&seader, &ifs_message));
    munit_assert_size(g_t1_host_test_state.send_version_call_count, ==, 1);
    munit_assert_size(g_t1_host_test_state.callback_call_count, ==, 1);
    munit_assert_uint32(g_t1_host_test_state.last_callback_event, ==, SeaderWorkerEventSamPresent);
    return MUNIT_OK;
}

static MunitResult test_recv_single_i_block(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    SeaderUartBridge uart = {0};
    SeaderWorker worker = {0};
    Seader seader = make_test_seader(&uart, &worker);

    t1_host_test_reset();
    uart.t1.recv_pcb = 0x00;
    uint8_t i_block[] = {0x00, 0x00, 0x03, 0x11, 0x22, 0x33, 0x00};
    seader_add_lrc(i_block, 6);
    CCID_Message i_message = {.payload = i_block, .dwLength = 7};

    munit_assert_true(seader_recv_t1(&seader, &i_message));
    munit_assert_size(g_t1_host_test_state.process_call_count, ==, 1);
    munit_assert_true(last_apdu_matches((const uint8_t*)"\x11\x22\x33", 3));
    munit_assert_uint8(uart.t1.recv_pcb, ==, 0x40);
    return MUNIT_OK;
}

static MunitResult test_recv_chained_i_blocks(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    SeaderUartBridge uart = {0};
    SeaderWorker worker = {0};
    Seader seader = make_test_seader(&uart, &worker);

    t1_host_test_reset();
    uart.t1.recv_pcb = 0x00;
    uint8_t i_more[] = {0x00, 0x20, 0x03, 0x44, 0x55, 0x66, 0x00};
    seader_add_lrc(i_more, 6);
    CCID_Message more_message = {.payload = i_more, .dwLength = 7};

    munit_assert_false(seader_recv_t1(&seader, &more_message));
    munit_assert_size(g_t1_host_test_state.xfrblock_call_count, ==, 1);
    munit_assert_not_null(uart.t1.rx_buffer);
    munit_assert_uint8(uart.t1.recv_pcb, ==, 0x40);

    t1_host_test_reset();
    uint8_t i_final[] = {0x00, 0x40, 0x02, 0x77, 0x88, 0x00};
    seader_add_lrc(i_final, 5);
    CCID_Message final_message = {.payload = i_final, .dwLength = 6};

    munit_assert_true(seader_recv_t1(&seader, &final_message));
    munit_assert_size(g_t1_host_test_state.process_call_count, ==, 1);
    munit_assert_true(last_apdu_matches((const uint8_t*)"\x44\x55\x66\x77\x88", 5));
    munit_assert_null(uart.t1.rx_buffer);
    return MUNIT_OK;
}

static MunitResult test_recv_ifs_request(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    SeaderUartBridge uart = {0};
    SeaderWorker worker = {0};
    Seader seader = make_test_seader(&uart, &worker);

    t1_host_test_reset();
    uart.t1.ifsc = 0x33;
    uint8_t ifs_request[] = {0x00, 0xC1, 0x01, 0x20, 0x00};
    seader_add_lrc(ifs_request, 4);
    CCID_Message ifs_request_message = {.payload = ifs_request, .dwLength = 5};

    munit_assert_false(seader_recv_t1(&seader, &ifs_request_message));
    munit_assert_size(g_t1_host_test_state.xfrblock_call_count, ==, 1);
    munit_assert_true(last_frame_prefix_matches((const uint8_t*)"\x00\xE1\x01\x33", 4));
    return MUNIT_OK;
}

static MunitResult test_recv_r_block_resends_buffer(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    SeaderUartBridge uart = {0};
    SeaderWorker worker = {0};
    Seader seader = make_test_seader(&uart, &worker);

    t1_host_test_reset();
    uart.t1.send_pcb = 0x00;
    uart.t1.tx_buffer = bit_buffer_alloc(8);
    bit_buffer_copy_bytes(uart.t1.tx_buffer, (const uint8_t*)"\xA0\xA1\xA2", 3);
    uart.t1.tx_buffer_offset = 0;
    uart.t1.ifsc = 4;
    uint8_t r_block[] = {0x00, 0x90, 0x00, 0x00};
    seader_add_lrc(r_block, 3);
    CCID_Message r_message = {.payload = r_block, .dwLength = 4};

    munit_assert_false(seader_recv_t1(&seader, &r_message));
    munit_assert_size(g_t1_host_test_state.xfrblock_call_count, ==, 1);
    munit_assert_not_null(uart.t1.tx_buffer);
    bit_buffer_free(uart.t1.tx_buffer);
    return MUNIT_OK;
}

static MunitResult test_reset_clears_state(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    SeaderUartBridge uart = {0};
    uart.t1.nad = 0x77;
    uart.t1.send_pcb = 0x00;
    uart.t1.recv_pcb = 0x40;
    uart.t1.tx_buffer = bit_buffer_alloc(8);
    uart.t1.rx_buffer = bit_buffer_alloc(8);
    uart.t1.tx_buffer_offset = 3;

    seader_t_1_reset(&uart);
    munit_assert_uint8(uart.t1.nad, ==, 0x00);
    munit_assert_uint8(uart.t1.send_pcb, ==, SEADER_T1_PCB_SEQUENCE_BIT);
    munit_assert_uint8(uart.t1.recv_pcb, ==, 0x00);
    munit_assert_null(uart.t1.tx_buffer);
    munit_assert_null(uart.t1.rx_buffer);
    munit_assert_size(uart.t1.tx_buffer_offset, ==, 0);
    return MUNIT_OK;
}

static MunitTest test_t1_cases[] = {
    {(char*)"/send/ifs-request", test_send_ifs_request, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/send/single-block", test_send_single_block, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/send/scratchpad-block",
     test_send_scratchpad_block,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/send/chained-block",
     test_send_chained_block,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/recv/ifs-response", test_recv_ifs_response, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/recv/single-i-block",
     test_recv_single_i_block,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/recv/chained-i-blocks",
     test_recv_chained_i_blocks,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/recv/ifs-request", test_recv_ifs_request, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/recv/r-block-resends-buffer",
     test_recv_r_block_resends_buffer,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/reset/clears-state",
     test_reset_clears_state,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {NULL, NULL, NULL, NULL, 0, NULL},
};

MunitSuite test_t1_existing_suite = {
    "",
    test_t1_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
