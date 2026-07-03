#include "munit.h"
#include "uart_rx_logic.h"

#include <string.h>

static MunitResult test_rx_chunk_processes_without_artificial_delay(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    munit_assert_uint32(seader_uart_rx_inter_chunk_delay_ms(1U), ==, 0U);
    munit_assert_uint32(seader_uart_rx_inter_chunk_delay_ms(64U), ==, 0U);
    munit_assert_uint32(seader_uart_rx_inter_chunk_delay_ms(272U), ==, 0U);
    return MUNIT_OK;
}

static MunitResult test_empty_rx_chunk_does_not_delay(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    munit_assert_uint32(seader_uart_rx_inter_chunk_delay_ms(0U), ==, 0U);
    return MUNIT_OK;
}

static MunitResult test_discard_consumed_noops_when_nothing_consumed(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    uint8_t buffer[] = {0x01U, 0x02U, 0x03U};
    const uint8_t expected[] = {0x01U, 0x02U, 0x03U};

    size_t remaining = seader_uart_rx_discard_consumed(buffer, sizeof(buffer), 0U);

    munit_assert_size(remaining, ==, sizeof(expected));
    munit_assert_memory_equal(sizeof(expected), buffer, expected);
    return MUNIT_OK;
}

static MunitResult test_discard_consumed_compacts_unparsed_tail(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    uint8_t buffer[] = {0x10U, 0x20U, 0x30U, 0x40U, 0x50U};
    const uint8_t expected[] = {0x30U, 0x40U, 0x50U};

    size_t remaining = seader_uart_rx_discard_consumed(buffer, sizeof(buffer), 2U);

    munit_assert_size(remaining, ==, sizeof(expected));
    munit_assert_memory_equal(sizeof(expected), buffer, expected);
    return MUNIT_OK;
}

static MunitResult test_discard_consumed_clears_length_when_all_consumed(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    uint8_t buffer[] = {0xAAU, 0xBBU};

    size_t remaining = seader_uart_rx_discard_consumed(buffer, sizeof(buffer), sizeof(buffer));

    munit_assert_size(remaining, ==, 0U);
    return MUNIT_OK;
}

static MunitTest test_uart_rx_logic_cases[] = {
    {(char*)"/delay/positive-chunk-is-immediate",
     test_rx_chunk_processes_without_artificial_delay,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/delay/empty-chunk-is-immediate",
     test_empty_rx_chunk_does_not_delay,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/discard/noop-with-zero-consumed",
     test_discard_consumed_noops_when_nothing_consumed,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/discard/compacts-tail",
     test_discard_consumed_compacts_unparsed_tail,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/discard/all-consumed",
     test_discard_consumed_clears_length_when_all_consumed,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {NULL, NULL, NULL, NULL, 0, NULL},
};

MunitSuite test_uart_rx_logic_suite = {
    "",
    test_uart_rx_logic_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
