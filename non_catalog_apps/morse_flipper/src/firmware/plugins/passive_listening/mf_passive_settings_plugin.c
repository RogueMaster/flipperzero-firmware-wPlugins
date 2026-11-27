#include "mf_passive_api.h"

#include <stdlib.h>

#include <flipper_application/flipper_application.h>

#include "mf_passive_settings.h"

static void* mf_passive_settings_alloc(void) {
    return calloc(1U, sizeof(MfPassiveSettingsState));
}

static void mf_passive_settings_free(void* state) {
    free(state);
}

static void mf_passive_settings_leave_api(void* state) {
    mf_passive_settings_leave(state);
}

static bool mf_passive_settings_enter_api(
    void* state,
    const void* args,
    MorseFlipperMappedFalResult* initial) {
    MfPassiveResult result = {0};
    bool entered = mf_passive_settings_enter(state, args, &result);
    if(initial != NULL) {
        *initial = (MorseFlipperMappedFalResult){
            .handled = result.handled,
            .redraw = result.redraw,
        };
    }
    return entered;
}

static MorseFlipperMappedFalResult
    mf_passive_settings_input(void* state, const InputEvent* event, uint32_t now_ms) {
    UNUSED(state);
    UNUSED(event);
    UNUSED(now_ms);
    return (MorseFlipperMappedFalResult){.handled = true};
}

static MorseFlipperMappedFalResult mf_passive_settings_tick(void* state, uint32_t now_ms) {
    UNUSED(state);
    UNUSED(now_ms);
    return (MorseFlipperMappedFalResult){.handled = true};
}

static void mf_passive_settings_draw(void* state, Canvas* canvas, uint32_t now_ms) {
    UNUSED(state);
    UNUSED(canvas);
    UNUSED(now_ms);
}

static const MfPassiveApi mf_passive_settings_api = {
    .mapped =
        {
            .magic = MF_PASSIVE_SETTINGS_API_MAGIC,
            .api_version = MF_PASSIVE_API_VERSION,
            .struct_size = sizeof(MfPassiveApi),
            .alloc = mf_passive_settings_alloc,
            .free = mf_passive_settings_free,
            .enter = mf_passive_settings_enter_api,
            .leave = mf_passive_settings_leave_api,
            .input = mf_passive_settings_input,
            .tick = mf_passive_settings_tick,
            .draw = mf_passive_settings_draw,
        },
    .enter = NULL,
    .input = NULL,
};

static const FlipperAppPluginDescriptor mf_passive_settings_descriptor = {
    .appid = "morse_flipper",
    .ep_api_version = MF_PASSIVE_API_VERSION,
    .entry_point = &mf_passive_settings_api,
};

const FlipperAppPluginDescriptor* morse_flipper_passive_settings_ep(void) {
    return &mf_passive_settings_descriptor;
}
