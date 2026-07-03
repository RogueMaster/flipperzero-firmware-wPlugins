#include "hf_14a_session.h"
#include "munit.h"

static MunitResult test_build_ats_empty_when_tl_has_no_ats(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    const SeaderHf14aAtsSource source = {.tl = 1U};
    uint8_t ats[8] = {0xffU};
    size_t ats_len = 99U;

    munit_assert_true(seader_hf_14a_build_ats(&source, ats, sizeof(ats), &ats_len));
    munit_assert_size(ats_len, ==, 0U);
    return MUNIT_OK;
}

static MunitResult test_build_ats_packs_declared_interface_bytes(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    const SeaderHf14aAtsSource source = {
        .tl = 4U,
        .t0 = SEADER_HF_14A_ATS_T0_TA1 | SEADER_HF_14A_ATS_T0_TC1 | 0x05U,
        .ta_1 = 0x11U,
        .tb_1 = 0x22U,
        .tc_1 = 0x33U,
    };
    uint8_t ats[8] = {0};
    size_t ats_len = 0U;

    munit_assert_true(seader_hf_14a_build_ats(&source, ats, sizeof(ats), &ats_len));
    munit_assert_size(ats_len, ==, 3U);
    munit_assert_uint8(ats[0], ==, source.t0);
    munit_assert_uint8(ats[1], ==, source.ta_1);
    munit_assert_uint8(ats[2], ==, source.tc_1);
    return MUNIT_OK;
}

static MunitResult test_build_ats_appends_historical_bytes(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    const uint8_t historical[] = {0xaaU, 0xbbU, 0xccU};
    const SeaderHf14aAtsSource source = {
        .tl = 5U,
        .t0 = SEADER_HF_14A_ATS_T0_TB1,
        .tb_1 = 0x44U,
        .t1_tk = historical,
        .t1_tk_size = sizeof(historical),
    };
    uint8_t ats[8] = {0};
    size_t ats_len = 0U;

    munit_assert_true(seader_hf_14a_build_ats(&source, ats, sizeof(ats), &ats_len));
    munit_assert_size(ats_len, ==, 5U);
    munit_assert_uint8(ats[0], ==, source.t0);
    munit_assert_uint8(ats[1], ==, source.tb_1);
    munit_assert_memory_equal(sizeof(historical), ats + 2U, historical);
    return MUNIT_OK;
}

static MunitResult test_build_ats_rejects_oversized_output(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    const uint8_t historical[] = {0x01U, 0x02U, 0x03U};
    const SeaderHf14aAtsSource source = {
        .tl = 5U,
        .t0 = SEADER_HF_14A_ATS_T0_TA1,
        .ta_1 = 0x11U,
        .t1_tk = historical,
        .t1_tk_size = sizeof(historical),
    };
    uint8_t ats[4] = {0};
    size_t ats_len = 99U;

    munit_assert_false(seader_hf_14a_build_ats(&source, ats, sizeof(ats), &ats_len));
    munit_assert_size(ats_len, ==, 0U);
    return MUNIT_OK;
}

static MunitTest test_hf_14a_session_cases[] = {
    {(char*)"/build-ats/no-ats",
     test_build_ats_empty_when_tl_has_no_ats,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/build-ats/interface-bytes",
     test_build_ats_packs_declared_interface_bytes,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/build-ats/historical-bytes",
     test_build_ats_appends_historical_bytes,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/build-ats/oversized",
     test_build_ats_rejects_oversized_output,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {NULL, NULL, NULL, NULL, 0, NULL},
};

MunitSuite test_hf_14a_session_suite = {
    "",
    test_hf_14a_session_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
