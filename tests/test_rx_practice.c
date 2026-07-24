#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "mf_rx_practice_core.h"

static unsigned checks;

#define CHECK(value) \
    do { \
        assert(value); \
        checks++; \
    } while(0)

static MfRxPracticeEnterArgs make_args(MfRxPracticeMode mode, uint32_t now) {
    return (MfRxPracticeEnterArgs){
        .struct_size = sizeof(MfRxPracticeEnterArgs),
        .mode = mode,
        .now_ms = now,
        .rng_seed = 1U,
        .answer_timeout_ms = 1000U,
        .result_hold_ms = 3000U,
        .dit_ms = 10U,
        .char_gap_ms = 30U,
        .physical_key_can_start = true,
    };
}

static void open_answer(MfRxPracticeState* state, uint32_t now) {
    MfRxPracticeResult result =
        mf_rx_practice_command(state, MfRxPracticeCommandStart, now);
    CHECK(result.phase == MfRxPracticePhasePlayback);
    CHECK(result.playback_mark);
    for(unsigned boundaries = 0U; state->phase == MfRxPracticePhasePlayback; boundaries++) {
        CHECK(boundaries < 100U);
        result = mf_rx_practice_tick(state, state->next_at);
        CHECK(result.handled);
    }
    CHECK(state->phase == MfRxPracticePhaseAnswer);
    CHECK(result.decoder_reset);
}

static void test_enter_validation(void) {
    MfRxPracticeState state;
    MfRxPracticeResult result;
    MfRxPracticeEnterArgs args = make_args(MfRxPracticeModeCallsigns, 0U);

    memset(&state, 0xA5, sizeof(state));
    memset(&result, 0xA5, sizeof(result));
    CHECK(!mf_rx_practice_enter(NULL, &args, &result));
    CHECK(!mf_rx_practice_enter(&state, NULL, &result));
    CHECK(state.phase == MfRxPracticePhaseIdle);
    args.struct_size--;
    CHECK(!mf_rx_practice_enter(&state, &args, &result));
    CHECK(state.session_total == 0U);
    args = make_args((MfRxPracticeMode)99, 0U);
    CHECK(!mf_rx_practice_enter(&state, &args, &result));
    args = make_args(MfRxPracticeModeCallsigns, 0U);
    args.answer_timeout_ms = INT32_MAX;
    CHECK(!mf_rx_practice_enter(&state, &args, &result));
    args = make_args(MfRxPracticeModeCallsigns, 0U);
    args.dit_ms = 0U;
    CHECK(!mf_rx_practice_enter(&state, &args, &result));
}

static void test_playback_and_answer(void) {
    MfRxPracticeState state;
    MfRxPracticeResult result;
    MfRxPracticeEnterArgs args = make_args(MfRxPracticeModeCallsigns, 0U);
    uint8_t mark_index;

    CHECK(mf_rx_practice_enter(&state, &args, &result));
    CHECK(result.phase == MfRxPracticePhaseIdle && result.decoder_reset && result.redraw);
    result = mf_rx_practice_command(&state, MfRxPracticeCommandStart, 0U);
    CHECK(result.phase == MfRxPracticePhasePlayback && result.playback_mark);
    mark_index = state.playback_mark_index;
    result = mf_rx_practice_tick(&state, state.next_at + 5000U);
    CHECK(result.phase == MfRxPracticePhasePlayback);
    CHECK(!state.playback_mark);
    CHECK(state.playback_mark_index == mark_index + 1U ||
          state.playback_char == 1U ||
          state.phase == MfRxPracticePhaseAnswer);
    while(state.phase == MfRxPracticePhasePlayback)
        result = mf_rx_practice_tick(&state, state.next_at);
    CHECK(result.decoder_reset);

    uint32_t activity = state.answer_last_activity_ms;
    result = mf_rx_practice_feed_text(&state, " |.!?\x80", 7U, activity + 100U);
    CHECK(!result.handled && !result.redraw);
    CHECK(state.answer_last_activity_ms == activity);

    char exact[MF_CALLSIGN_MAX_LEN + 1U];
    memcpy(exact, state.target, sizeof(exact));
    for(uint8_t i = 0U; i < state.target_len; i++)
        if(exact[i] >= 'A' && exact[i] <= 'Z') exact[i] = (char)(exact[i] + ('a' - 'A'));
    result = mf_rx_practice_feed_text(&state, exact, state.target_len, activity + 200U);
    CHECK(result.phase == MfRxPracticePhaseResult);
    CHECK(result.feedback == MfRxPracticeFeedbackPass);
    CHECK(result.decoder_reset && state.session_total == 1U && state.session_passed == 1U);

    uint32_t original_deadline = state.result_deadline;
    result = mf_rx_practice_command(&state, MfRxPracticeCommandHurry, activity + 201U);
    CHECK(result.phase == MfRxPracticePhaseResult && result.handled && result.redraw);
    CHECK(state.result_deadline == activity + 1201U);
    result = mf_rx_practice_command(&state, MfRxPracticeCommandHurry, activity + 202U);
    CHECK(result.phase == MfRxPracticePhaseResult && !result.redraw);
    CHECK(state.result_deadline != original_deadline);
    result = mf_rx_practice_tick(&state, activity + 1200U);
    CHECK(result.phase == MfRxPracticePhaseResult);
    result = mf_rx_practice_tick(&state, activity + 1201U);
    CHECK(result.phase == MfRxPracticePhasePlayback);
    CHECK(result.feedback == MfRxPracticeFeedbackClear);
}

static void test_edit_and_mode_filtering(void) {
    MfRxPracticeState state;
    MfRxPracticeResult result;
    MfRxPracticeEnterArgs args = make_args(MfRxPracticeModeGroups5, 0U);

    CHECK(mf_rx_practice_enter(&state, &args, &result));
    open_answer(&state, 0U);
    uint32_t activity = state.answer_last_activity_ms;
    result = mf_rx_practice_feed_text(&state, "7 |", 3U, activity + 10U);
    CHECK(!result.handled);
    CHECK(state.answer_len == 0U && state.answer_last_activity_ms == activity);
    result = mf_rx_practice_command(&state, MfRxPracticeCommandBackspace, activity + 20U);
    CHECK(result.handled && result.decoder_reset && !result.redraw);
    CHECK(state.answer_last_activity_ms == activity);
    result = mf_rx_practice_feed_text(&state, "a", 1U, activity + 30U);
    CHECK(result.handled && result.redraw && state.answer[0] == 'A');
    CHECK(state.answer_last_activity_ms == activity + 30U);
    result = mf_rx_practice_command(&state, MfRxPracticeCommandBackspace, activity + 40U);
    CHECK(result.handled && result.redraw && result.decoder_reset);
    CHECK(state.answer_len == 0U && state.answer_last_activity_ms == activity + 40U);
    result = mf_rx_practice_feed_text(&state, "bc", 2U, activity + 50U);
    CHECK(result.handled && state.answer_len == 2U);
    result = mf_rx_practice_command(&state, MfRxPracticeCommandClear, activity + 60U);
    CHECK(result.handled && result.redraw && result.decoder_reset);
    CHECK(state.answer_len == 0U && state.answer_last_activity_ms == activity + 60U);
    result = mf_rx_practice_command(&state, MfRxPracticeCommandClear, activity + 70U);
    CHECK(result.handled && !result.redraw && result.decoder_reset);
    CHECK(state.answer_last_activity_ms == activity + 60U);
}

static void test_timeout_wrap_and_saturation(void) {
    MfRxPracticeState state;
    MfRxPracticeResult result;
    MfRxPracticeEnterArgs args = make_args(MfRxPracticeModeGroups5, UINT32_MAX - 50U);

    args.answer_timeout_ms = 100U;
    CHECK(mf_rx_practice_enter(&state, &args, &result));
    state.phase = MfRxPracticePhaseAnswer;
    state.target_len = 5U;
    memcpy(state.target, "ABCDE", 6U);
    state.answer_last_activity_ms = UINT32_MAX - 50U;
    result = mf_rx_practice_tick(&state, 48U);
    CHECK(result.phase == MfRxPracticePhaseAnswer);
    result = mf_rx_practice_tick(&state, 49U);
    CHECK(result.phase == MfRxPracticePhaseResult);
    CHECK(result.feedback == MfRxPracticeFeedbackTimeout);

    state.phase = MfRxPracticePhaseAnswer;
    state.answer_len = 0U;
    state.answer[0] = '\0';
    state.session_total = UINT16_MAX;
    state.session_passed = 123U;
    result = mf_rx_practice_feed_text(&state, state.target, state.target_len, 100U);
    CHECK(result.feedback == MfRxPracticeFeedbackPass);
    CHECK(state.session_total == UINT16_MAX && state.session_passed == 123U);
}

static void test_back_and_reenter(void) {
    MfRxPracticeState state;
    MfRxPracticeResult result;
    MfRxPracticeEnterArgs args = make_args(MfRxPracticeModeCallsigns, 0U);

    CHECK(mf_rx_practice_enter(&state, &args, &result));
    result = mf_rx_practice_command(&state, MfRxPracticeCommandBack, 0U);
    CHECK(result.request_exit && result.decoder_reset);
    CHECK(mf_rx_practice_enter(&state, &args, &result));
    open_answer(&state, 0U);
    result = mf_rx_practice_command(&state, MfRxPracticeCommandBack, 0U);
    CHECK(result.phase == MfRxPracticePhaseFinal && !result.request_exit);
    CHECK(result.feedback == MfRxPracticeFeedbackClear && result.decoder_reset);
    result = mf_rx_practice_command(&state, MfRxPracticeCommandConfirmExit, 0U);
    CHECK(result.request_exit);
    mf_rx_practice_leave(&state);
    CHECK(state.session_total == 0U && state.target[0] == '\0');
}

static void test_press_commands(void) {
    MfRxPracticeState state;
    MfRxPracticeResult result;
    MfRxPracticeEnterArgs args = make_args(MfRxPracticeModeGroups5, 100U);

    CHECK(mf_rx_practice_enter(&state, &args, &result));
    result = mf_rx_practice_command(&state, MfRxPracticeCommandPrimaryPress, 100U);
    CHECK(result.handled && result.phase == MfRxPracticePhasePlayback);

    CHECK(mf_rx_practice_enter(&state, &args, &result));
    result = mf_rx_practice_command(&state, MfRxPracticeCommandPaddleBackPress, 100U);
    CHECK(result.handled && result.phase == MfRxPracticePhasePlayback);

    state.phase = MfRxPracticePhaseResult;
    state.result_deadline = 5000U;
    result = mf_rx_practice_command(&state, MfRxPracticeCommandPrimaryPress, 200U);
    CHECK(result.handled && result.redraw && state.result_deadline == 1200U);
    state.phase = MfRxPracticePhaseFinal;
    result = mf_rx_practice_command(&state, MfRxPracticeCommandPaddleBackPress, 300U);
    CHECK(result.handled && result.request_exit);
}

int main(void) {
    test_enter_validation();
    test_playback_and_answer();
    test_edit_and_mode_filtering();
    test_timeout_wrap_and_saturation();
    test_back_and_reenter();
    test_press_commands();
    CHECK(sizeof(MfRxPracticeState) <= 256U);
    printf(
        "test_rx_practice: %u checks passed; state=%u bytes\n",
        checks,
        (unsigned)sizeof(MfRxPracticeState));
    return 0;
}
