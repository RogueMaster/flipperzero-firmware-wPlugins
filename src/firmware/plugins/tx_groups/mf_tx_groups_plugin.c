#include "mf_tx_groups_draw.h"

#include <flipper_application/flipper_application.h>

static MfTxGroupsState mf_tx_groups_state;

static void* mf_tx_groups_alloc(void) {
    return &mf_tx_groups_state;
}

static void mf_tx_groups_free(void* state) {
    (void)state;
}

static bool
    mf_tx_groups_enter(void* state, const void* args, MorseFlipperMappedFalResult* initial) {
    MfTxGroupsState* tx_groups_state = state;
    const MfTxGroupsEnterArgs* enter_args = args;

    if(tx_groups_state == NULL || enter_args == NULL || enter_args->draw_services.context == NULL)
        return false;
    tx_groups_state->draw_services = enter_args->draw_services;
    if(initial != NULL) *initial = (MorseFlipperMappedFalResult){0};
    return true;
}

static void mf_tx_groups_leave(void* state) {
    if(state != NULL) *(MfTxGroupsState*)state = (MfTxGroupsState){0};
}

static MorseFlipperMappedFalResult
    mf_tx_groups_input(void* state, const InputEvent* event, uint32_t now_ms) {
    (void)state;
    (void)event;
    (void)now_ms;
    return (MorseFlipperMappedFalResult){0};
}

static MorseFlipperMappedFalResult mf_tx_groups_tick(void* state, uint32_t now_ms) {
    (void)state;
    (void)now_ms;
    return (MorseFlipperMappedFalResult){0};
}

static void mf_tx_groups_draw(void* state, Canvas* canvas, uint32_t now_ms) {
    (void)now_ms;
    mf_tx_groups_draw_tx_groups(state, canvas);
}

static const MfTxGroupsApi mf_tx_groups_api = {
    .mapped =
        {
            .magic = MF_TX_GROUPS_API_MAGIC,
            .api_version = MF_TX_GROUPS_API_VERSION,
            .struct_size = sizeof(MfTxGroupsApi),
            .alloc = mf_tx_groups_alloc,
            .free = mf_tx_groups_free,
            .enter = mf_tx_groups_enter,
            .leave = mf_tx_groups_leave,
            .input = mf_tx_groups_input,
            .tick = mf_tx_groups_tick,
            .draw = mf_tx_groups_draw,
        },
    .init = morse_flipper_tx_group_init,
    .set_seed = morse_flipper_tx_group_set_seed,
    .start = morse_flipper_tx_group_start,
    .feed_mark = morse_flipper_tx_group_feed_mark,
    .feed_space = morse_flipper_tx_group_feed_space,
    .feed_text = morse_flipper_tx_group_feed_text,
    .finalize_answer_from_raw = morse_flipper_tx_group_finalize_answer_from_raw,
    .set_range = morse_flipper_tx_group_set_range,
    .score = morse_flipper_tx_group_score,
    .complete = morse_flipper_tx_group_complete,
    .marks_complete = morse_flipper_tx_group_marks_complete,
};

static const FlipperAppPluginDescriptor mf_tx_groups_descriptor = {
    .appid = "morse_flipper",
    .ep_api_version = MF_TX_GROUPS_API_VERSION,
    .entry_point = &mf_tx_groups_api,
};

const FlipperAppPluginDescriptor* morse_flipper_tx_groups_ep(void) {
    return &mf_tx_groups_descriptor;
}
