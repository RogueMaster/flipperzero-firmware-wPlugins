#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "mf_passive_policy.h"
#include "mf_passive_voice_pack.h"

static unsigned checks;

#define CHECK(value)   \
    do {               \
        assert(value); \
        checks++;      \
    } while(0)

static void test_defaults(void) {
    MfPassiveSettingsModel model = {0};

    mf_passive_settings_load(&model);
    CHECK(model.mode == 0U);
    CHECK(model.length == 4U);
    CHECK(model.lesson == 1U);
    CHECK(model.dit_ms == 48U);
    CHECK(model.farnsworth_wpm == 12U);
    CHECK(model.vibrate == 1U);
    CHECK(model.answer_delay_s == 3U);
    CHECK(model.repeat_after_answer == 0U);
    CHECK(model.courtesy_delay_half_s == 2U);
    CHECK(model.selected_row == 0U);
}

static void test_normalize_and_wpm(void) {
    MfPassiveSettingsModel model = {
        .mode = 2U,
        .length = 0U,
        .lesson = 255U,
        .dit_ms = 0U,
        .farnsworth_wpm = 255U,
        .vibrate = 7U,
        .answer_delay_s = 0U,
        .repeat_after_answer = 2U,
        .selected_row = 255U,
    };

    CHECK(mf_passive_settings_wpm(NULL) == 25U);
    CHECK(mf_passive_settings_wpm(&model) == 25U);
    mf_passive_settings_normalize(&model);
    CHECK(model.mode == 0U && model.length == 4U);
    CHECK(model.lesson == mf_passive_settings_lesson_count());
    CHECK(model.dit_ms == 48U && model.farnsworth_wpm == 25U);
    CHECK(model.vibrate == 1U && model.answer_delay_s == 1U);
    CHECK(model.repeat_after_answer == 1U && model.selected_row == 0U);
    model.mode = 1U;
    model.length = 0U;
    mf_passive_settings_normalize(&model);
    CHECK(model.length == 1U);
    model.length = 7U;
    mf_passive_settings_normalize(&model);
    CHECK(model.length == 7U);
    model.length = 10U;
    mf_passive_settings_normalize(&model);
    CHECK(model.length == 1U);
    model.length = 1U;
    model.mode = 0U;
    mf_passive_settings_normalize(&model);
    CHECK(model.length == 4U);
    model.mode = 1U;
    mf_passive_settings_normalize(&model);
    CHECK(model.length == 4U);
    model.dit_ms = 40U;
    CHECK(mf_passive_settings_wpm(&model) == 30U);
    model.dit_ms = 200U;
    CHECK(mf_passive_settings_wpm(&model) == 10U);
    {
        uint8_t min;
        uint8_t max;
        mf_passive_settings_length_bounds(9U, 0U, &min, &max);
        CHECK(min == 4U && max == 6U);
        CHECK(strcmp(mf_passive_settings_length_label(8U), "5-6") == 0);
    }
}

static void test_filtered_lesson_order(void) {
    const char* charset = mf_passive_settings_lesson_charset();
    char label[8];
    uint8_t token;

    CHECK(strcmp(charset, "KMURESNAPTLWI.JZFOY,VG5/Q92H38B?47C1D60X") == 0);
    CHECK(mf_passive_settings_lesson_count() == strlen(charset) - 1U);
    CHECK(mf_passive_settings_lesson_charset_len(1U) == 2U);
    CHECK(mf_passive_settings_lesson_charset_len(2U) == 3U);
    CHECK(
        mf_passive_settings_lesson_charset_len((uint8_t)mf_passive_settings_lesson_count()) ==
        strlen(charset));
    mf_passive_settings_lesson_label(1U, label, sizeof(label));
    CHECK(strcmp(label, "1 - K M") == 0);
    mf_passive_settings_lesson_label(2U, label, sizeof(label));
    CHECK(strcmp(label, "2 - U") == 0);
    mf_passive_settings_lesson_label(
        (uint8_t)mf_passive_settings_lesson_count(), label, sizeof(label));
    CHECK(strcmp(label, "39 - X") == 0);
    for(size_t i = 0U; charset[i] != '\0'; i++) {
        CHECK(mf_passive_voice_char_token(charset[i], &token));
        CHECK(token < MF_PASSIVE_VOICE_TOKEN_COUNT);
    }
}

int main(void) {
    test_defaults();
    test_normalize_and_wpm();
    test_filtered_lesson_order();
    printf("test_passive_policy: %u checks passed\n", checks);
    return 0;
}
