#include "mf_passive_api.h"

#include <stdlib.h>

#include <flipper_application/flipper_application.h>

#include "mf_passive_core.h"
#include "mf_passive_draw.h"

static void* mf_passive_alloc(void) {
    return calloc(1U, sizeof(MfPassiveState));
}
static void mf_passive_free(void* state) {
    free(state);
}
static MorseFlipperMappedFalResult mf_passive_tick_api(void* state, uint32_t now_ms) {
    MfPassiveResult result = mf_passive_tick(state, now_ms);
    return (MorseFlipperMappedFalResult){
        .handled = result.handled,
        .redraw = result.redraw,
        .request_exit = result.request_exit,
        .phase = result.phase,
        .feedback = result.feedback,
    };
}
static bool
    mf_passive_enter_api(void* state, const void* args, MorseFlipperMappedFalResult* initial) {
    MfPassiveResult result = {0};
    bool entered = mf_passive_enter(state, args, &result);
    if(initial != NULL) {
        *initial = (MorseFlipperMappedFalResult){
            .handled = result.handled,
            .redraw = result.redraw,
            .request_exit = result.request_exit,
            .phase = result.phase,
            .feedback = result.feedback,
        };
    }
    return entered;
}
static MorseFlipperMappedFalResult
    mf_passive_input_api(void* state, const InputEvent* event, uint32_t now_ms) {
    MfPassiveResult result = mf_passive_input(state, event, now_ms);
    return (MorseFlipperMappedFalResult){
        .handled = result.handled,
        .redraw = result.redraw,
        .request_exit = result.request_exit,
        .phase = result.phase,
        .feedback = result.feedback,
    };
}
static void mf_passive_draw_api(void* state, Canvas* canvas, uint32_t now_ms) {
    (void)now_ms;
    mf_passive_draw(state, canvas);
}

static const MfPassiveApi mf_passive_api = {
    .mapped =
        {
            .magic = MF_PASSIVE_API_MAGIC,
            .api_version = MF_PASSIVE_API_VERSION,
            .struct_size = sizeof(MfPassiveApi),
            .alloc = mf_passive_alloc,
            .free = mf_passive_free,
            .enter = mf_passive_enter_api,
            .leave = (void (*)(void*))mf_passive_leave,
            .input = mf_passive_input_api,
            .tick = mf_passive_tick_api,
            .draw = mf_passive_draw_api,
        },
    .enter = (bool (*)(void*, const MfPassiveEnterArgs*, MfPassiveResult*))mf_passive_enter,
    .input = (MfPassiveResult(*)(void*, const InputEvent*, uint32_t))mf_passive_input,
};

static const FlipperAppPluginDescriptor mf_passive_descriptor = {
    .appid = "morse_flipper",
    .ep_api_version = MF_PASSIVE_API_VERSION,
    .entry_point = &mf_passive_api,
};

const FlipperAppPluginDescriptor* morse_flipper_passive_listening_ep(void) {
    return &mf_passive_descriptor;
}
