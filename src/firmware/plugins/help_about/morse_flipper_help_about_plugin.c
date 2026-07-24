/* Embedded Help/About content plugin. It imports firmware public APIs only. */
#include "morse_flipper_help_about_internal.h"

#include <flipper_application/flipper_application.h>

#include <stdio.h>
#include <stdlib.h>

#define MORSE_FLIPPER_MD_SCROLL_STEP_PX 45U
#define MORSE_FLIPPER_ABOUT_OK_FAST_MS   500U

static MorseFlipperContentResult morse_flipper_content_result(MorseFlipperContentAction action) {
    return (MorseFlipperContentResult){
        .handled = true,
        .redraw = action == MorseFlipperContentActionRedraw,
        .request_exit = action == MorseFlipperContentActionBack,
        .action = action,
    };
}

static void* morse_flipper_help_about_alloc(void) {
    return calloc(1, sizeof(MorseFlipperHelpAboutState));
}

static void morse_flipper_help_about_free(void* value) {
    MorseFlipperHelpAboutState* state = value;
    if(state == NULL) return;
    if(state->help_text != NULL) furi_string_free(state->help_text);
    free(state);
}

bool morse_flipper_help_content_enter(
    MorseFlipperHelpAboutState* state,
    const MorseFlipperContentEnterArgs* args) {
    if(state == NULL || args == NULL || args->mode > MorseFlipperContentModeAbout) return false;
    state->mode = args->mode;
    if(state->help_text == NULL) state->help_text = furi_string_alloc();
    if(state->help_text == NULL) return false;

    if(args->mode == MorseFlipperContentModeOnboarding) {
        state->onboarding_page = 0U;
        state->onboarding_md = (CwmdState){0};
        morse_flipper_onboarding_open(state);
    } else if(args->mode == MorseFlipperContentModeHelp) {
        state->help_topic = args->help_topic < 11U ? args->help_topic : 0U;
        state->help_page = 0U;
        state->help_chapter_card = false;
        state->help_md = (CwmdState){0};
        morse_flipper_help_open(state);
    } else {
        snprintf(
            state->about_body,
            sizeof(state->about_body),
            "Version: %s\nBuilt: %s\nCommit: %s\nHost: %s\n\n"
            "Morse Flipper started as a small personal project to turn the FZ into a portable CW "
            "training tool. Along the way it picked up the more ambitious goal of nudging "
            "tech-minded, RF-oriented geeks toward Morse and ham radio. It now has enough features "
            "to be dangerous on its own - if you take the dolphin seriously, anyway.",
            args->version ? args->version : "unknown",
            args->build_time ? args->build_time : "unknown",
            args->build_commit ? args->build_commit : "unknown",
            args->build_host ? args->build_host : "unknown");
        morse_flipper_about_reset(state, args->now_ms);
    }
    return true;
}

static MorseFlipperContentResult morse_flipper_content_help_input(
    MorseFlipperHelpAboutState* state,
    const InputEvent* event) {
    bool forward;
    CwmdState* markdown = &state->help_md;

    if(event->key == InputKeyBack &&
       (event->type == InputTypeShort || event->type == InputTypeLong))
        return morse_flipper_content_result(MorseFlipperContentActionBack);

    if(event->key == InputKeyLeft &&
       (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        if(!state->help_chapter_card && state->help_page > 0U) {
            state->help_page--;
            state->help_md = (CwmdState){0};
            morse_flipper_help_open(state);
            return morse_flipper_content_result(MorseFlipperContentActionRedraw);
        }
        return morse_flipper_content_result(MorseFlipperContentActionNone);
    }

    if(!((event->key == InputKeyUp || event->key == InputKeyDown || event->key == InputKeyRight) &&
         (event->type == InputTypeShort || event->type == InputTypeRepeat)))
        return morse_flipper_content_result(MorseFlipperContentActionNone);

    forward = event->key != InputKeyUp;
    if(forward && state->help_chapter_card) {
        morse_flipper_help_enter_chapter(state);
        return morse_flipper_content_result(MorseFlipperContentActionRedraw);
    }
    int16_t old_target = markdown->target_scroll_px;
    cwmd_scroll_step(markdown, forward ? 1 : -1, markdown->max_scroll_px, MORSE_FLIPPER_MD_SCROLL_STEP_PX);
    if(markdown->target_scroll_px != old_target)
        return morse_flipper_content_result(MorseFlipperContentActionRedraw);
    if(forward && markdown->scroll_px >= markdown->max_scroll_px && old_target >= markdown->max_scroll_px) {
        if(state->help_page + 1U < morse_flipper_help_card_count(state)) {
            state->help_page++;
            state->help_md = (CwmdState){0};
            morse_flipper_help_open(state);
        } else if(morse_flipper_help_show_next_chapter(state)) {
            MorseFlipperContentResult result = morse_flipper_content_result(MorseFlipperContentActionRedraw);
            result.help_topic_changed = true;
            result.help_topic = state->help_topic;
            return result;
        }
        return morse_flipper_content_result(MorseFlipperContentActionRedraw);
    }
    return morse_flipper_content_result(MorseFlipperContentActionNone);
}

static MorseFlipperContentResult morse_flipper_content_onboarding_input(
    MorseFlipperHelpAboutState* state,
    const InputEvent* event) {
    if((event->key == InputKeyOk || event->key == InputKeyBack) &&
       (event->type == InputTypeShort || event->type == InputTypeLong))
        return morse_flipper_content_result(MorseFlipperContentActionFinishOnboarding);
    if(event->key == InputKeyLeft &&
       (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        if(state->onboarding_page > 0U) {
            state->onboarding_page--;
            state->onboarding_md = (CwmdState){0};
            morse_flipper_onboarding_open(state);
            return morse_flipper_content_result(MorseFlipperContentActionRedraw);
        }
    } else if(event->key == InputKeyRight &&
              (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        if(state->onboarding_page + 1U >= state->onboarding_card_count)
            return morse_flipper_content_result(MorseFlipperContentActionFinishOnboarding);
        state->onboarding_page++;
        state->onboarding_md = (CwmdState){0};
        morse_flipper_onboarding_open(state);
        return morse_flipper_content_result(MorseFlipperContentActionRedraw);
    } else if((event->key == InputKeyUp || event->key == InputKeyDown) &&
              (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        int16_t old_target = state->onboarding_md.target_scroll_px;
        cwmd_scroll_step(&state->onboarding_md, event->key == InputKeyDown ? 1 : -1,
                         state->onboarding_md.max_scroll_px, MORSE_FLIPPER_MD_SCROLL_STEP_PX);
        if(old_target != state->onboarding_md.target_scroll_px)
            return morse_flipper_content_result(MorseFlipperContentActionRedraw);
    }
    return morse_flipper_content_result(MorseFlipperContentActionNone);
}

static MorseFlipperContentResult morse_flipper_content_about_input(
    MorseFlipperHelpAboutState* state,
    const InputEvent* event,
    uint32_t now_ms) {
    if((event->key == InputKeyBack || event->key == InputKeyLeft) &&
       (event->type == InputTypeShort || event->type == InputTypeLong))
        return morse_flipper_content_result(MorseFlipperContentActionBack);
    if(state->about_mode == 0U && event->type == InputTypeShort) {
        if(state->about_show_next && (event->key == InputKeyOk || event->key == InputKeyRight)) {
            state->about_mode = 1U;
            state->about_md = (CwmdState){0};
            state->about_ok_count = 0U;
            state->about_last_ok_ms = 0U;
            return morse_flipper_content_result(MorseFlipperContentActionRedraw);
        }
    } else if(state->about_mode == 1U && event->key == InputKeyOk && event->type == InputTypeShort) {
        state->about_ok_count = state->about_last_ok_ms != 0U &&
                                        now_ms - state->about_last_ok_ms <= MORSE_FLIPPER_ABOUT_OK_FAST_MS ?
                                    state->about_ok_count + 1U :
                                    1U;
        state->about_last_ok_ms = now_ms;
        if(state->about_ok_count >= 3U) return morse_flipper_content_result(MorseFlipperContentActionOpenTrace);
        int16_t old_target = state->about_md.target_scroll_px;
        cwmd_scroll_step(&state->about_md, 1, state->about_md.max_scroll_px, MORSE_FLIPPER_MD_SCROLL_STEP_PX);
        if(old_target != state->about_md.target_scroll_px)
            return morse_flipper_content_result(MorseFlipperContentActionRedraw);
    } else if(
        state->about_mode == 1U && (event->key == InputKeyUp || event->key == InputKeyDown) &&
        (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        state->about_ok_count = 0U;
        state->about_last_ok_ms = 0U;
        int16_t old_target = state->about_md.target_scroll_px;
        cwmd_scroll_step(
            &state->about_md,
            event->key == InputKeyDown ? 1 : -1,
            state->about_md.max_scroll_px,
            MORSE_FLIPPER_MD_SCROLL_STEP_PX);
        if(old_target != state->about_md.target_scroll_px)
            return morse_flipper_content_result(MorseFlipperContentActionRedraw);
    }
    return morse_flipper_content_result(MorseFlipperContentActionNone);
}

MorseFlipperContentResult morse_flipper_help_content_input(
    MorseFlipperHelpAboutState* state,
    const InputEvent* event,
    uint32_t now_ms) {
    if(state == NULL || event == NULL) return morse_flipper_content_result(MorseFlipperContentActionNone);
    if(state->mode == MorseFlipperContentModeOnboarding) return morse_flipper_content_onboarding_input(state, event);
    if(state->mode == MorseFlipperContentModeHelp) return morse_flipper_content_help_input(state, event);
    return morse_flipper_content_about_input(state, event, now_ms);
}

bool morse_flipper_help_content_tick(MorseFlipperHelpAboutState* state, uint32_t now_ms) {
    if(state == NULL) return false;
    if(state->mode == MorseFlipperContentModeAbout) {
        if(state->about_mode == 0U) return morse_flipper_tick_about(state, now_ms);
        return cwmd_scroll_tick(&state->about_md);
    }
    if(state->mode == MorseFlipperContentModeOnboarding) return cwmd_scroll_tick(&state->onboarding_md);
    return cwmd_scroll_tick(&state->help_md);
}

void morse_flipper_help_content_draw(MorseFlipperHelpAboutState* state, Canvas* canvas) {
    if(state == NULL || canvas == NULL) return;
    if(state->mode == MorseFlipperContentModeOnboarding) morse_flipper_draw_onboarding(canvas, state);
    else if(state->mode == MorseFlipperContentModeHelp) morse_flipper_draw_help(canvas, state);
    else morse_flipper_draw_about(canvas, state);
}

static bool morse_flipper_help_about_enter(void* state, const MorseFlipperContentEnterArgs* args) {
    return morse_flipper_help_content_enter(state, args);
}
static MorseFlipperContentResult morse_flipper_help_about_input(
    void* state,
    const InputEvent* event,
    uint32_t now_ms);
static bool morse_flipper_help_about_enter_api(
    void* state,
    const void* args,
    MorseFlipperMappedFalResult* initial) {
    bool entered = morse_flipper_help_about_enter(state, args);
    if(initial != NULL)
        *initial = (MorseFlipperMappedFalResult){.handled = entered, .redraw = entered};
    return entered;
}
static MorseFlipperMappedFalResult morse_flipper_help_about_input_api(
    void* state,
    const InputEvent* event,
    uint32_t now_ms) {
    MorseFlipperContentResult result = morse_flipper_help_about_input(state, event, now_ms);
    return (MorseFlipperMappedFalResult){
        .handled = result.handled,
        .redraw = result.redraw,
        .request_exit = result.request_exit,
    };
}
static void morse_flipper_help_about_leave(void* state) { UNUSED(state); }
static MorseFlipperContentResult morse_flipper_help_about_input(void* state, const InputEvent* event, uint32_t now_ms) {
    return morse_flipper_help_content_input(state, event, now_ms);
}
static MorseFlipperMappedFalResult morse_flipper_help_about_tick(void* state, uint32_t now_ms) {
    return (MorseFlipperMappedFalResult){
        .handled = true,
        .redraw = morse_flipper_help_content_tick(state, now_ms),
    };
}
static void morse_flipper_help_about_draw(void* state, Canvas* canvas, uint32_t now_ms) {
    UNUSED(now_ms);
    morse_flipper_help_content_draw(state, canvas);
}

static const MorseFlipperHelpAboutApi morse_flipper_help_about_api = {
    .mapped = {
        .magic = MORSE_FLIPPER_HELP_ABOUT_API_MAGIC,
        .api_version = MORSE_FLIPPER_HELP_ABOUT_API_VERSION,
        .struct_size = sizeof(MorseFlipperHelpAboutApi),
        .alloc = morse_flipper_help_about_alloc,
        .free = morse_flipper_help_about_free,
        .enter = morse_flipper_help_about_enter_api,
        .leave = morse_flipper_help_about_leave,
        .input = morse_flipper_help_about_input_api,
        .tick = morse_flipper_help_about_tick,
        .draw = morse_flipper_help_about_draw,
    },
    .enter = morse_flipper_help_about_enter,
    .input = morse_flipper_help_about_input,
};

static const FlipperAppPluginDescriptor morse_flipper_help_about_descriptor = {
    .appid = "morse_flipper",
    .ep_api_version = MORSE_FLIPPER_HELP_ABOUT_API_VERSION,
    .entry_point = &morse_flipper_help_about_api,
};

const FlipperAppPluginDescriptor* morse_flipper_help_about_ep(void) {
    return &morse_flipper_help_about_descriptor;
}
