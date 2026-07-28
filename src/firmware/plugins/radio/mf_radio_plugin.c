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

static bool mf_radio_enter_api(
    void* state,
    const void* enter_args,
    MorseFlipperMappedFalResult* initial) {
    return mf_radio_core_enter(state, enter_args, mf_radio_hal_ops(), initial);
}

static void mf_radio_leave_api(void* state) {
    mf_radio_core_leave(state);
}

static MorseFlipperMappedFalResult mf_radio_input_api(
    void* state,
    const InputEvent* event,
    uint32_t now_ms) {
    return mf_radio_core_input(state, event, now_ms);
}

static MorseFlipperMappedFalResult mf_radio_tick_api(void* state, uint32_t now_ms) {
    return mf_radio_core_tick(state, now_ms);
}

static void mf_radio_draw_api(void* state, Canvas* canvas, uint32_t now_ms) {
    mf_radio_draw(state, canvas, now_ms);
}

static MorseFlipperMappedFalResult mf_radio_set_page_api(
    void* state,
    MfRadioPage page,
    uint32_t now_ms) {
    return mf_radio_core_set_page(state, page, now_ms);
}

static MorseFlipperMappedFalResult mf_radio_sync_tx_api(
    void* state,
    MfRadioTxInterval completed_interval,
    uint16_t duration_ms,
    bool level,
    uint32_t now_ms) {
    return mf_radio_core_sync_tx(
        state, completed_interval, duration_ms, level, now_ms);
}

static bool mf_radio_snapshot_api(const void* state, MfRadioSnapshot* snapshot) {
    return mf_radio_core_snapshot(state, snapshot);
}

static const MfRadioApi radio_api = {
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
    .set_page = mf_radio_set_page_api,
    .sync_tx = mf_radio_sync_tx_api,
    .snapshot = mf_radio_snapshot_api,
};

static const FlipperAppPluginDescriptor radio_descriptor = {
    .appid = "morse_flipper",
    .ep_api_version = MF_RADIO_API_VERSION,
    .entry_point = &radio_api,
};

const FlipperAppPluginDescriptor* morse_flipper_radio_ep(void) {
    return &radio_descriptor;
}
