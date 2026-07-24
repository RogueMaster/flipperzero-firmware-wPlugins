#include "morse_flipper_rx_practice_api.h"

#include <stdlib.h>

#include <flipper_application/flipper_application.h>

#include "mf_rx_practice_core.h"
#include "mf_rx_practice_draw.h"

static void* mf_rx_alloc(void) { return calloc(1U, sizeof(MfRxPracticeState)); }
static void mf_rx_free(void* state) { free(state); }
static bool mf_rx_enter(void* state, const MfRxPracticeEnterArgs* args, MfRxPracticeResult* result) {
    return mf_rx_practice_enter(state, args, result);
}
static bool mf_rx_enter_api(void* state, const void* args, MorseFlipperMappedFalResult* initial) {
    return mf_rx_enter(state, args, initial);
}
static void mf_rx_leave(void* state) { mf_rx_practice_leave(state); }
static MfRxPracticeResult mf_rx_input(
    void* state,
    const InputEvent* event,
    bool button_paddle,
    uint32_t now_ms) {
    MfRxPracticeCommand command = MfRxPracticeCommandNone;
    if(event == NULL) return mf_rx_practice_command(state, command, now_ms);
    if(event->key == InputKeyOk && event->type == InputTypeRelease)
        command = MfRxPracticeCommandReleaseOk;
    else if(event->key == InputKeyBack && event->type == InputTypeRelease)
        command = MfRxPracticeCommandReleaseBack;
    else if(event->key == InputKeyLeft && event->type == InputTypeLong)
        command = button_paddle ? MfRxPracticeCommandBack :
                                  MfRxPracticeCommandConfirmExit;
    else if(event->type == InputTypePress) {
        if(event->key == InputKeyOk)
            command = MfRxPracticeCommandPrimaryPress;
        else if(button_paddle && event->key == InputKeyBack)
            command = MfRxPracticeCommandPaddleBackPress;
        else
            command = MfRxPracticeCommandHurry;
    } else if(event->type == InputTypeShort) {
        if(event->key == InputKeyOk)
            command = MfRxPracticeCommandConfirmExit;
        else if(event->key == InputKeyDown)
            command = MfRxPracticeCommandBackspace;
        else if(event->key == InputKeyUp)
            command = MfRxPracticeCommandClear;
        else if(event->key == InputKeyBack && !button_paddle)
            command = MfRxPracticeCommandBack;
        else
            command = MfRxPracticeCommandHurry;
    } else if(!button_paddle && event->key == InputKeyBack &&
              event->type == InputTypeLong) {
        command = MfRxPracticeCommandBack;
    }
    return mf_rx_practice_command(state, command, now_ms);
}
static MfRxPracticeResult mf_rx_command(void* state, MfRxPracticeCommand command, uint32_t now_ms) {
    return mf_rx_practice_command(state, command, now_ms);
}
static MfRxPracticeResult mf_rx_feed(void* state, const char* text, size_t len, uint32_t now_ms) {
    return mf_rx_practice_feed_text(state, text, len, now_ms);
}
static MfRxPracticeResult mf_rx_tick(void* state, uint32_t now_ms) {
    return mf_rx_practice_tick(state, now_ms);
}
static void mf_rx_draw(void* state, Canvas* canvas, uint32_t now_ms) {
    UNUSED(now_ms);
    mf_rx_practice_draw(state, canvas);
}

static const MfRxPracticeApi mf_rx_api = {
    .mapped = {
        .magic = MORSE_FLIPPER_RX_PRACTICE_API_MAGIC,
        .api_version = MORSE_FLIPPER_RX_PRACTICE_API_VERSION,
        .struct_size = sizeof(MfRxPracticeApi),
        .alloc = mf_rx_alloc,
        .free = mf_rx_free,
        .enter = mf_rx_enter_api,
        .leave = mf_rx_leave,
        .input = NULL,
        .tick = mf_rx_tick,
        .draw = mf_rx_draw,
    },
    .enter = mf_rx_enter,
    .input = mf_rx_input,
    .command = mf_rx_command,
    .feed_text = mf_rx_feed,
};

static const FlipperAppPluginDescriptor mf_rx_descriptor = {
    .appid = "morse_flipper",
    .ep_api_version = MORSE_FLIPPER_RX_PRACTICE_API_VERSION,
    .entry_point = &mf_rx_api,
};

const FlipperAppPluginDescriptor* morse_flipper_rx_practice_ep(void) { return &mf_rx_descriptor; }
