#include "munit.h"

#include <furi.h>

int seader_test_wiegand_format_count(uint8_t bit_length, uint64_t bits);
void seader_test_wiegand_format_description(
    uint8_t bit_length,
    uint64_t bits,
    size_t index,
    FuriString* description);

static const uint8_t multi_match_bit_length = 37U;
static const uint64_t multi_match_bits = 0x00100003ULL;

static MunitResult test_multi_match_count_and_descriptions(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    FuriString* description = furi_string_alloc();
    munit_assert_int(seader_test_wiegand_format_count(multi_match_bit_length, multi_match_bits), ==, 2);

    seader_test_wiegand_format_description(
        multi_match_bit_length, multi_match_bits, 0U, description);
    munit_assert_string_equal(furi_string_get_cstr(description), "H10302\nCN: 524289\n");

    seader_test_wiegand_format_description(
        multi_match_bit_length, multi_match_bits, 1U, description);
    munit_assert_string_equal(furi_string_get_cstr(description), "H10304\nFC: 1 CN: 1\n");

    furi_string_free(description);
    return MUNIT_OK;
}

static MunitResult test_out_of_range_index_returns_empty_string(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    FuriString* description = furi_string_alloc();
    furi_string_set_str(description, "stale");

    seader_test_wiegand_format_description(
        multi_match_bit_length, multi_match_bits, 2U, description);

    munit_assert_size(furi_string_size(description), ==, 0U);
    furi_string_free(description);
    return MUNIT_OK;
}

static MunitResult test_reused_description_buffer_stays_per_index(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    FuriString* description = furi_string_alloc();
    FuriString* combined = furi_string_alloc();
    const size_t count = (size_t)seader_test_wiegand_format_count(multi_match_bit_length, multi_match_bits);

    for(size_t i = 0; i < count; i++) {
        furi_string_reset(description);
        seader_test_wiegand_format_description(
            multi_match_bit_length, multi_match_bits, i, description);
        if(furi_string_size(description) > 0U) {
            furi_string_cat_printf(combined, "%s\n", furi_string_get_cstr(description));
        }
    }

    munit_assert_string_equal(
        furi_string_get_cstr(combined),
        "H10302\nCN: 524289\n\nH10304\nFC: 1 CN: 1\n\n");

    furi_string_free(combined);
    furi_string_free(description);
    return MUNIT_OK;
}

static MunitTest test_wiegand_plugin_cases[] = {
    {(char*)"/multi-match", test_multi_match_count_and_descriptions, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/out-of-range", test_out_of_range_index_returns_empty_string, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/reused-buffer", test_reused_description_buffer_stays_per_index, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

MunitSuite test_wiegand_plugin_suite = {
    "/wiegand-plugin",
    test_wiegand_plugin_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
