#include "hf_buffer_pool.h"
#include "munit.h"

static MunitResult test_prepare_allocates_and_resets_buffers(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    SeaderHfBufferPair pair = {0};
    const uint8_t tx_data[] = {0x01U, 0x02U};
    const uint8_t rx_data[] = {0x03U};

    munit_assert_true(seader_hf_buffer_pair_prepare(&pair, 8U, 8U, sizeof(tx_data)));
    munit_assert_not_null(pair.tx);
    munit_assert_not_null(pair.rx);

    bit_buffer_append_bytes(pair.tx, tx_data, sizeof(tx_data));
    bit_buffer_append_bytes(pair.rx, rx_data, sizeof(rx_data));
    munit_assert_size(bit_buffer_get_size_bytes(pair.tx), ==, sizeof(tx_data));
    munit_assert_size(bit_buffer_get_size_bytes(pair.rx), ==, sizeof(rx_data));

    munit_assert_true(seader_hf_buffer_pair_prepare(&pair, 8U, 8U, sizeof(tx_data)));
    munit_assert_size(bit_buffer_get_size_bytes(pair.tx), ==, 0U);
    munit_assert_size(bit_buffer_get_size_bytes(pair.rx), ==, 0U);

    seader_hf_buffer_pair_free(&pair);
    return MUNIT_OK;
}

static MunitResult test_prepare_reuses_existing_buffers(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    SeaderHfBufferPair pair = {0};
    munit_assert_true(seader_hf_buffer_pair_prepare(&pair, 8U, 8U, 1U));
    BitBuffer* first_tx = pair.tx;
    BitBuffer* first_rx = pair.rx;

    munit_assert_true(seader_hf_buffer_pair_prepare(&pair, 8U, 8U, 1U));
    munit_assert_ptr(pair.tx, ==, first_tx);
    munit_assert_ptr(pair.rx, ==, first_rx);

    seader_hf_buffer_pair_free(&pair);
    return MUNIT_OK;
}

static MunitResult test_prepare_rejects_oversized_tx(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    SeaderHfBufferPair pair = {0};
    munit_assert_false(seader_hf_buffer_pair_prepare(&pair, 8U, 8U, 9U));
    munit_assert_null(pair.tx);
    munit_assert_null(pair.rx);
    return MUNIT_OK;
}

static MunitResult test_free_clears_pair(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    SeaderHfBufferPair pair = {0};
    munit_assert_true(seader_hf_buffer_pair_prepare(&pair, 8U, 8U, 1U));

    seader_hf_buffer_pair_free(&pair);
    munit_assert_null(pair.tx);
    munit_assert_null(pair.rx);
    munit_assert_size(pair.tx_capacity, ==, 0U);
    munit_assert_size(pair.rx_capacity, ==, 0U);
    return MUNIT_OK;
}

static MunitTest test_hf_buffer_pool_cases[] = {
    {(char*)"/prepare/allocates-and-resets",
     test_prepare_allocates_and_resets_buffers,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/prepare/reuses-existing",
     test_prepare_reuses_existing_buffers,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/prepare/rejects-oversized-tx",
     test_prepare_rejects_oversized_tx,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/free/clears-pair",
     test_free_clears_pair,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {NULL, NULL, NULL, NULL, 0, NULL},
};

MunitSuite test_hf_buffer_pool_suite = {
    "",
    test_hf_buffer_pool_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
