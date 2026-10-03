#include "morse_flipper_icr_api.h"

#include <flipper_application/flipper_application.h>

void* morse_flipper_icr_runtime_alloc(void);
void morse_flipper_icr_runtime_free(void* state);
bool morse_flipper_icr_runtime_enter(
    void* state,
    const MorseFlipperIcrEnterArgs* args,
    MorseFlipperIcrResult* initial);
void morse_flipper_icr_runtime_leave(void* state);
MorseFlipperIcrResult
    morse_flipper_icr_runtime_input(void* state, const InputEvent* event, uint32_t now_ms);
MorseFlipperIcrResult morse_flipper_icr_runtime_tick(void* state, uint32_t now_ms);
void morse_flipper_icr_runtime_draw(void* state, Canvas* canvas, uint32_t now_ms);

static bool morse_flipper_icr_enter_api(
    void* state,
    const void* args,
    MorseFlipperMappedFalResult* initial) {
    return morse_flipper_icr_runtime_enter(state, args, initial);
}

static const MorseFlipperIcrApi morse_flipper_icr_api = {
    .mapped =
        {
            .magic = MORSE_FLIPPER_ICR_API_MAGIC,
            .api_version = MORSE_FLIPPER_ICR_API_VERSION,
            .struct_size = sizeof(MorseFlipperIcrApi),
            .alloc = morse_flipper_icr_runtime_alloc,
            .free = morse_flipper_icr_runtime_free,
            .enter = morse_flipper_icr_enter_api,
            .leave = morse_flipper_icr_runtime_leave,
            .input = morse_flipper_icr_runtime_input,
            .tick = morse_flipper_icr_runtime_tick,
            .draw = morse_flipper_icr_runtime_draw,
        },
    .enter = morse_flipper_icr_runtime_enter,
    .input = morse_flipper_icr_runtime_input,
};

static const FlipperAppPluginDescriptor morse_flipper_icr_descriptor = {
    .appid = "morse_flipper",
    .ep_api_version = MORSE_FLIPPER_ICR_API_VERSION,
    .entry_point = &morse_flipper_icr_api,
};

const FlipperAppPluginDescriptor* morse_flipper_icr_ep(void) {
    return &morse_flipper_icr_descriptor;
}
