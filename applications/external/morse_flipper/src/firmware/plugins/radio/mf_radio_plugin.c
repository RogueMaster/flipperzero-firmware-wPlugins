#include "mf_radio_api.h"

#include <stdlib.h>

#include <flipper_application/flipper_application.h>

#include "mf_radio_core.h"
#include "mf_radio_draw.h"
#include "mf_radio_hal.h"

static void* mf_radio_alloc(void) {
    return calloc(1U, sizeof(MfRadioState));
}

static void mf_radio_free(void* state) {
    free(state);
}

static bool
    mf_radio_enter_api(void* state, const void* enter_args, MorseFlipperMappedFalResult* initial) {
    return mf_radio_core_enter(state, enter_args, mf_radio_hal_ops(), initial);
}

static void mf_radio_leave_api(void* state) {
    mf_radio_core_leave(state);
}

static MorseFlipperMappedFalResult
    mf_radio_input_api(void* state, const InputEvent* event, uint32_t now_ms) {
    return mf_radio_core_input(state, event, now_ms);
}

static MorseFlipperMappedFalResult mf_radio_tick_api(void* state, uint32_t now_ms) {
    return mf_radio_core_tick(state, now_ms);
}

static void mf_radio_draw_api(void* state, Canvas* canvas, uint32_t now_ms) {
    mf_radio_draw(state, canvas, now_ms);
}

static MorseFlipperMappedFalResult mf_radio_command_api(
    void* state,
    uint32_t command,
    const void* input,
    void* output,
    uint32_t now_ms) {
    MorseFlipperMappedFalResult result = {0};
    if(command == MfRadioCommandSetPage && input != NULL) {
        result = mf_radio_core_set_page(state, *(const MfRadioPage*)input, now_ms);
    } else if(command == MfRadioCommandSyncTx && input != NULL) {
        const MfRadioSyncTxCommand* sync = input;
        result = mf_radio_core_sync_tx(
            state, sync->completed_interval, sync->duration_ms, sync->level, now_ms);
    }
    if(output != NULL) (void)mf_radio_core_snapshot(state, output);
    return result;
}

static const MfRadioApi radio_api = {
    .fal =
        {
            .mapped =
                {
                    .magic = MF_RADIO_API_MAGIC,
                    .api_version = MF_RADIO_API_VERSION,
                    .struct_size = sizeof(MfRadioApi),
                    .alloc = mf_radio_alloc,
                    .free = mf_radio_free,
                    .enter = mf_radio_enter_api,
                    .leave = mf_radio_leave_api,
                    .input = mf_radio_input_api,
                    .tick = mf_radio_tick_api,
                    .draw = mf_radio_draw_api,
                },
            .command = mf_radio_command_api,
        },
};

static const FlipperAppPluginDescriptor radio_descriptor = {
    .appid = "morse_flipper",
    .ep_api_version = MF_RADIO_API_VERSION,
    .entry_point = &radio_api,
};

const FlipperAppPluginDescriptor* morse_flipper_radio_ep(void) {
    return &radio_descriptor;
}
