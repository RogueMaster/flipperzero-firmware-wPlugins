#include "munit.h"

#include "sam_startup_ui.h"

static MunitResult test_startup_stage_strings(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    munit_assert_string_equal(
        seader_startup_stage_header(SeaderStartupStageCheckingSam), "Checking SAM");
    munit_assert_string_equal(
        seader_startup_stage_text(SeaderStartupStageRetryingBoard), "Power cycle\nand retry");
    return MUNIT_OK;
}

static MunitResult test_board_detail_strings(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;

    munit_assert_string_equal(
        seader_board_status_detail_title(SeaderBoardStatusFaultPostEnable), "Board Fault");
    munit_assert_string_equal(
        seader_board_status_detail_body(SeaderBoardStatusNoResponse, true),
        "Board powered,\nno SAM after retry");
    munit_assert_string_equal(
        seader_board_status_detail_body(SeaderBoardStatusPowerLost, false),
        "USB/5V removed\nboard unpowered");
    munit_assert_string_equal(
        seader_board_status_detail_hint(SeaderBoardStatusUnknown), "Insert supported SAM");
    munit_assert_string_equal(
        seader_board_status_detail_hint(SeaderBoardStatusPowerLost), "Reconnect power");
    return MUNIT_OK;
}

static MunitResult test_atr_summary_formats_and_truncates(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    const uint8_t atr[] = {0x3B, 0x7F, 0x96, 0x00, 0x00, 0x80, 0x31};
    char summary[48];

    seader_format_atr_summary(atr, sizeof(atr), summary, sizeof(summary));
    munit_assert_string_equal(summary, "ATR: 3B 7F 96 00 00 80...");

    seader_format_atr_summary(NULL, 0U, summary, sizeof(summary));
    munit_assert_string_equal(summary, "ATR: unavailable");
    return MUNIT_OK;
}

static MunitTest test_sam_startup_ui_cases[] = {
    {(char*)"/startup-stage-strings",
     test_startup_stage_strings,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/board-detail-strings",
     test_board_detail_strings,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/atr-summary",
     test_atr_summary_formats_and_truncates,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

MunitSuite test_sam_startup_ui_suite = {
    "/sam-startup-ui",
    test_sam_startup_ui_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
