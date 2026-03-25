#include "munit.h"

#include "card_details_builder.h"

static MunitResult test_builds_type4_with_owned_optional_fields(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    const uint8_t uid[] = {0x04, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    const uint8_t ats[] = {0x75, 0x77, 0x81, 0x02};
    CardDetails_t card_details = {0};

    munit_assert_true(
        seader_card_details_build(&card_details, 0x20U, uid, sizeof(uid), ats, sizeof(ats)));
    munit_assert_size(card_details.csn.size, ==, sizeof(uid));
    munit_assert_not_null(card_details.sak);
    munit_assert_not_null(card_details.atsOrAtqbOrAtr);
    munit_assert_size(card_details.sak->size, ==, 1U);
    munit_assert_size(card_details.atsOrAtqbOrAtr->size, ==, sizeof(ats));
    munit_assert_memory_equal(sizeof(uid), card_details.csn.buf, uid);
    munit_assert_memory_equal(sizeof(ats), card_details.atsOrAtqbOrAtr->buf, ats);

    seader_card_details_reset(&card_details);
    munit_assert_ptr_null(card_details.sak);
    munit_assert_ptr_null(card_details.atsOrAtqbOrAtr);
    munit_assert_ptr_null(card_details.csn.buf);
    return MUNIT_OK;
}

static MunitResult test_builds_picopass_without_optional_fields(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    const uint8_t uid[] = {1, 2, 3, 4, 5, 6, 7, 8};
    CardDetails_t card_details = {0};

    munit_assert_true(seader_card_details_build(&card_details, 0U, uid, sizeof(uid), NULL, 0U));
    munit_assert_ptr_null(card_details.sak);
    munit_assert_ptr_null(card_details.atsOrAtqbOrAtr);

    seader_card_details_reset(&card_details);
    return MUNIT_OK;
}

static MunitResult test_builds_mfc_with_owned_sak(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    const uint8_t uid[] = {0xDE, 0xAD, 0xBE, 0xEF};
    CardDetails_t card_details = {0};

    munit_assert_true(seader_card_details_build(&card_details, 0x08U, uid, sizeof(uid), NULL, 0U));
    munit_assert_not_null(card_details.sak);
    munit_assert_size(card_details.sak->size, ==, 1U);
    munit_assert_ptr_null(card_details.atsOrAtqbOrAtr);

    seader_card_details_reset(&card_details);
    munit_assert_ptr_null(card_details.sak);
    return MUNIT_OK;
}

static MunitResult test_rejects_invalid_input(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    CardDetails_t card_details = {0};

    munit_assert_false(seader_card_details_build(&card_details, 0U, NULL, 0U, NULL, 0U));
    munit_assert_ptr_null(card_details.csn.buf);
    return MUNIT_OK;
}

static MunitTest test_card_details_builder_cases[] = {
    {(char*)"/type4-owned-optional-fields", test_builds_type4_with_owned_optional_fields, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/picopass-no-optional-fields", test_builds_picopass_without_optional_fields, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/mfc-owned-sak", test_builds_mfc_with_owned_sak, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/invalid-input", test_rejects_invalid_input, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, 0, NULL},
};

MunitSuite test_card_details_builder_suite = {
    "",
    test_card_details_builder_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
