#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "mf_rx_practice_core.h"

static MfRxPracticeEnterArgs args(MfRxPracticeMode mode, uint32_t now) {
    return (MfRxPracticeEnterArgs){
        .struct_size = sizeof(MfRxPracticeEnterArgs),
        .mode = mode,
        .now_ms = now,
        .rng_seed = 1U,
        .answer_timeout_ms = 1000U,
        .result_hold_ms = 300U,
        .dit_ms = 10U,
        .char_gap_ms = 30U,
        .physical_key_can_start = true,
    };
}

int main(void) {
    MfRxPracticeState state;
    MfRxPracticeResult result;
    MfRxPracticeEnterArgs enter_args;
    unsigned checks = 0U;
#define CHECK(value) \
    do { \
        assert(value); \
        checks++; \
    } while(0)
    enter_args = args(MfRxPracticeModeCallsigns, 0U);
    CHECK(mf_rx_practice_enter(&state, &enter_args, &result));
    CHECK(result.phase == MfRxPracticePhaseIdle && result.decoder_reset);
    result = mf_rx_practice_command(&state, MfRxPracticeCommandStart, 0U);
    CHECK(result.phase == MfRxPracticePhasePlayback && result.playback_mark);
    for(uint32_t now = 10U; state.phase == MfRxPracticePhasePlayback; now += 10U) {
        result = mf_rx_practice_tick(&state, now);
        CHECK(now < 10000U);
    }
    CHECK(state.phase == MfRxPracticePhaseAnswer);
    result = mf_rx_practice_feed_text(&state, state.target, state.target_len, 500U);
    CHECK(result.phase == MfRxPracticePhaseResult && result.feedback == MfRxPracticeFeedbackPass);
    result = mf_rx_practice_command(&state, MfRxPracticeCommandHurry, 501U);
    CHECK(result.phase == MfRxPracticePhasePlayback);
    enter_args = args(MfRxPracticeModeGroups5, UINT32_MAX - 50U);
    CHECK(mf_rx_practice_enter(&state, &enter_args, &result));
    result = mf_rx_practice_command(&state, MfRxPracticeCommandStart, UINT32_MAX - 50U);
    CHECK(result.phase == MfRxPracticePhasePlayback);
    result = mf_rx_practice_command(&state, MfRxPracticeCommandBack, 0U);
    CHECK(result.phase == MfRxPracticePhaseFinal && !result.request_exit);
    result = mf_rx_practice_command(&state, MfRxPracticeCommandConfirmExit, 0U);
    CHECK(result.request_exit);
    CHECK(sizeof(state) <= 256U);
    printf("test_rx_practice: %u checks passed; state=%u bytes\n", checks, (unsigned)sizeof(state));
    return 0;
}
