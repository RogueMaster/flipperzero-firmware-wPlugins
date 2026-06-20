#include <string.h>

#include "munit.h"
#include "uart_tx_logic.h"

static MunitResult test_tx_frame_copy_rejects_invalid_inputs(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    SeaderUartTxFrame frame = {0};
    const uint8_t data[] = {0x01U};

    munit_assert_false(seader_uart_tx_frame_copy(NULL, data, sizeof(data), sizeof(data)));
    munit_assert_false(seader_uart_tx_frame_copy(&frame, NULL, sizeof(data), sizeof(data)));
    munit_assert_false(seader_uart_tx_frame_copy(&frame, data, 0U, sizeof(data)));
    munit_assert_false(seader_uart_tx_frame_copy(&frame, data, sizeof(data), 0U));
    return MUNIT_OK;
}

static MunitResult test_tx_frame_copy_preserves_frame_bytes(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    SeaderUartTxFrame frame = {0};
    const uint8_t data[] = {0x01U, 0x02U, 0x03U};

    munit_assert_true(seader_uart_tx_frame_copy(&frame, data, sizeof(data), sizeof(frame.data)));
    munit_assert_size(frame.len, ==, sizeof(data));
    munit_assert_memory_equal(sizeof(data), frame.data, data);
    return MUNIT_OK;
}

static MunitResult test_tx_frame_copy_is_immutable_after_source_reuse(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    SeaderUartTxFrame frame = {0};
    uint8_t scratch[] = {0xAAU, 0xBBU, 0xCCU};
    const uint8_t expected[] = {0xAAU, 0xBBU, 0xCCU};

    munit_assert_true(
        seader_uart_tx_frame_copy(&frame, scratch, sizeof(scratch), sizeof(frame.data)));
    memset(scratch, 0x11, sizeof(scratch));

    munit_assert_size(frame.len, ==, sizeof(expected));
    munit_assert_memory_equal(sizeof(expected), frame.data, expected);
    return MUNIT_OK;
}

static MunitResult test_back_to_back_copies_preserve_distinct_frames(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    SeaderUartTxFrame first = {0};
    SeaderUartTxFrame second = {0};
    uint8_t scratch[] = {0x10U, 0x20U};
    const uint8_t first_expected[] = {0x10U, 0x20U};
    const uint8_t second_expected[] = {0x30U, 0x40U};

    munit_assert_true(
        seader_uart_tx_frame_copy(&first, scratch, sizeof(scratch), sizeof(first.data)));
    scratch[0] = 0x30U;
    scratch[1] = 0x40U;
    munit_assert_true(
        seader_uart_tx_frame_copy(&second, scratch, sizeof(scratch), sizeof(second.data)));

    munit_assert_memory_equal(sizeof(first_expected), first.data, first_expected);
    munit_assert_memory_equal(sizeof(second_expected), second.data, second_expected);
    return MUNIT_OK;
}

static MunitTest test_uart_tx_logic_cases[] = {
    {(char*)"/copy/rejects-invalid",
     test_tx_frame_copy_rejects_invalid_inputs,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/copy/preserves-bytes",
     test_tx_frame_copy_preserves_frame_bytes,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/copy/immutable-after-source-reuse",
     test_tx_frame_copy_is_immutable_after_source_reuse,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/copy/back-to-back-distinct",
     test_back_to_back_copies_preserve_distinct_frames,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {NULL, NULL, NULL, NULL, 0, NULL},
};

MunitSuite test_uart_tx_logic_suite = {
    "",
    test_uart_tx_logic_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
