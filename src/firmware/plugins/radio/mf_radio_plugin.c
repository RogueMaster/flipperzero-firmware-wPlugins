#include "mf_radio_api.h"

#include <stdlib.h>

#include <flipper_application/flipper_application.h>

typedef struct {
    MfRadioSnapshot snapshot;
    bool entered;
    bool leaving;
} MfRadioState;

static void* mf_radio_alloc(void) {
    return calloc(1U, sizeof(MfRadioState));
}

static void mf_radio_free(void* state) {
    free(state);
}

static bool mf_radio_enter(
    void* context,
    const void* enter_args,
    MorseFlipperMappedFalResult* initial) {
    MfRadioState* state = context;
    const MfRadioEnterArgs* args = enter_args;

    if(initial != NULL) *initial = (MorseFlipperMappedFalResult){0};
    if(state == NULL || args == NULL || args->struct_size != sizeof(MfRadioEnterArgs) ||
       !mf_radio_decoder_services_valid(args->decoder) ||
       !mf_radio_draw_services_valid(args->draw) || args->dit_ms == 0U)
        return false;

    state->snapshot = (MfRadioSnapshot){
        .struct_size = sizeof(MfRadioSnapshot),
        .page = MfRadioPageIdle,
        .frequency_hz = args->frequency_hz,
        .monitor_threshold_dbm = args->monitor_threshold_dbm,
        .receive_audio_enabled = args->receive_audio_enabled,
    };
    state->entered = true;
    state->leaving = false;
    if(initial != NULL) initial->redraw = true;
    return true;
}

static void mf_radio_leave(void* context) {
    MfRadioState* state = context;
    if(state == NULL) return;
    state->leaving = true;
    state->snapshot.page = MfRadioPageIdle;
    state->snapshot.hardware_active = false;
    state->snapshot.tx_active = false;
    state->snapshot.monitor_tone = false;
}

static MorseFlipperMappedFalResult mf_radio_input(
    void* context,
    const InputEvent* event,
    uint32_t now_ms) {
    (void)event;
    (void)now_ms;
    MfRadioState* state = context;
    return (MorseFlipperMappedFalResult){
        .handled = state != NULL && state->entered && !state->leaving,
    };
}

static MorseFlipperMappedFalResult mf_radio_tick(void* context, uint32_t now_ms) {
    (void)context;
    (void)now_ms;
    return (MorseFlipperMappedFalResult){0};
}

static void mf_radio_draw(void* context, Canvas* canvas, uint32_t now_ms) {
    (void)context;
    (void)canvas;
    (void)now_ms;
}

static MorseFlipperMappedFalResult mf_radio_set_page(
    void* context,
    MfRadioPage page,
    uint32_t now_ms) {
    (void)now_ms;
    MfRadioState* state = context;
    if(state == NULL || !state->entered || state->leaving || page > MfRadioPageFrequency)
        return (MorseFlipperMappedFalResult){0};
    state->snapshot.page = page;
    return (MorseFlipperMappedFalResult){.handled = true, .redraw = true};
}

static MorseFlipperMappedFalResult mf_radio_sync_tx(
    void* context,
    MfRadioTxInterval completed_interval,
    uint16_t duration_ms,
    bool level,
    uint32_t now_ms) {
    (void)context;
    (void)completed_interval;
    (void)duration_ms;
    (void)level;
    (void)now_ms;
    return (MorseFlipperMappedFalResult){0};
}

static bool mf_radio_snapshot(const void* context, MfRadioSnapshot* snapshot) {
    const MfRadioState* state = context;
    if(state == NULL || snapshot == NULL || snapshot->struct_size != sizeof(MfRadioSnapshot))
        return false;
    *snapshot = state->snapshot;
    return true;
}

static const MfRadioApi radio_api = {
    .mapped =
        {
            .magic = MF_RADIO_API_MAGIC,
            .api_version = MF_RADIO_API_VERSION,
            .struct_size = sizeof(MfRadioApi),
            .alloc = mf_radio_alloc,
            .free = mf_radio_free,
            .enter = mf_radio_enter,
            .leave = mf_radio_leave,
            .input = mf_radio_input,
            .tick = mf_radio_tick,
            .draw = mf_radio_draw,
        },
    .set_page = mf_radio_set_page,
    .sync_tx = mf_radio_sync_tx,
    .snapshot = mf_radio_snapshot,
};

static const FlipperAppPluginDescriptor radio_descriptor = {
    .appid = "morse_flipper",
    .ep_api_version = MF_RADIO_API_VERSION,
    .entry_point = &radio_api,
};

const FlipperAppPluginDescriptor* morse_flipper_radio_ep(void) {
    return &radio_descriptor;
}

