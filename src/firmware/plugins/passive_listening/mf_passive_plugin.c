#include "mf_passive_api.h"

#include <stdlib.h>

#include <flipper_application/flipper_application.h>

#include "mf_passive_core.h"
#include "mf_passive_draw.h"

static void* mf_passive_alloc(void) { return calloc(1U, sizeof(MfPassiveState)); }
static void mf_passive_free(void* state) { free(state); }
static void mf_passive_draw_api(const void* state, Canvas* canvas) { mf_passive_draw(state, canvas); }

static const MfPassiveApi mf_passive_api = {
    .magic = MF_PASSIVE_API_MAGIC,
    .api_version = MF_PASSIVE_API_VERSION,
    .struct_size = sizeof(MfPassiveApi),
    .alloc = mf_passive_alloc,
    .free = mf_passive_free,
    .enter = (bool (*)(void*, const MfPassiveEnterArgs*, MfPassiveResult*))mf_passive_enter,
    .leave = (void (*)(void*))mf_passive_leave,
    .input = (MfPassiveResult (*)(void*, const InputEvent*, uint32_t))mf_passive_input,
    .tick = (MfPassiveResult (*)(void*, uint32_t))mf_passive_tick,
    .draw = mf_passive_draw_api,
};

static const FlipperAppPluginDescriptor mf_passive_descriptor = {
    .appid = "morse_flipper",
    .ep_api_version = MF_PASSIVE_API_VERSION,
    .entry_point = &mf_passive_api,
};

const FlipperAppPluginDescriptor* morse_flipper_passive_listening_ep(void) {
    return &mf_passive_descriptor;
}
