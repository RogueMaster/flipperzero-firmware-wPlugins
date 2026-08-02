#include <assert.h>
#include <stdio.h>

#include <flipper_application/flipper_application.h>

#include "mf_rx_practice_core.h"
#include "morse_flipper_rx_practice_api.h"

static unsigned checks;

#define CHECK(value)   \
    do {               \
        assert(value); \
        checks++;      \
    } while(0)

void mf_rx_practice_draw(const MfRxPracticeState* state, Canvas* canvas) {
    (void)state;
    (void)canvas;
}

const FlipperAppPluginDescriptor* morse_flipper_rx_practice_ep(void);

static MfRxPracticeEnterArgs make_args(MfRxPracticeDrawSnapshot* snapshot) {
    return (MfRxPracticeEnterArgs){
        .struct_size = sizeof(MfRxPracticeEnterArgs),
        .rng_seed = 1U,
        .answer_timeout_ms = 1000U,
        .result_hold_ms = 3000U,
        .dit_ms = 10U,
        .char_gap_ms = 30U,
        .min_length = 4U,
        .max_length = 6U,
        .button_paddle = true,
        .draw_snapshot = snapshot,
    };
}

static MfRxPracticeResult
    send(const MfRxPracticeApi* api, void* state, InputKey key, InputType type) {
    InputEvent event = {.key = key, .type = type};
    return api->input(state, &event, 100U);
}

int main(void) {
    const FlipperAppPluginDescriptor* descriptor = morse_flipper_rx_practice_ep();
    const MfRxPracticeApi* api = descriptor->entry_point;
    MfRxPracticeDrawSnapshot snapshot = {0};
    MfRxPracticeEnterArgs args = make_args(&snapshot);
    MfRxPracticeResult result;
    void* opaque = api->mapped.alloc();
    MfRxPracticeState* state = opaque;
    const InputType back_types[] = {
        InputTypePress,
        InputTypeRelease,
        InputTypeShort,
        InputTypeLong,
        InputTypeRepeat,
    };

    CHECK(opaque != NULL);
    CHECK(descriptor->ep_api_version == MORSE_FLIPPER_RX_PRACTICE_API_VERSION);
    CHECK(api->enter(opaque, &args, &result));
    for(MfRxPracticePhase phase = MfRxPracticePhaseIdle; phase <= MfRxPracticePhaseFinal;
        phase++) {
        state->phase = phase;
        state->result_deadline = 5000U;
        for(size_t i = 0U; i < sizeof(back_types) / sizeof(back_types[0]); i++) {
            result = send(api, opaque, InputKeyBack, back_types[i]);
            CHECK(!result.request_exit);
            CHECK(state->phase == phase);
            CHECK(state->result_deadline == 5000U);
        }

        result = send(api, opaque, InputKeyLeft, InputTypeLong);
        CHECK(result.handled && result.request_exit);
        CHECK(state->phase == phase);
    }

    CHECK(api->enter(opaque, &args, &result));
    result = send(api, opaque, InputKeyOk, InputTypePress);
    CHECK(result.handled && state->phase == MfRxPracticePhasePlayback);

    api->mapped.free(opaque);
    printf("test_rx_practice_input: %u checks passed\n", checks);
    return 0;
}
