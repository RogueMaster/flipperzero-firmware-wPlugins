#pragma once

#include <gui/canvas.h>
#include <input/input.h>

#include <stdbool.h>
#include <stdint.h>

#include "../../morse_flipper_mapped_fal.h"

#define MORSE_FLIPPER_HELP_ABOUT_API_VERSION 2U
#define MORSE_FLIPPER_HELP_ABOUT_API_MAGIC 0x4D464841UL

typedef enum {
    MorseFlipperContentModeOnboarding = 0,
    MorseFlipperContentModeHelp,
    MorseFlipperContentModeAbout,
} MorseFlipperContentMode;

typedef enum {
    MorseFlipperContentActionNone = 0,
    MorseFlipperContentActionRedraw,
    MorseFlipperContentActionBack,
    MorseFlipperContentActionFinishOnboarding,
    MorseFlipperContentActionOpenTrace,
} MorseFlipperContentAction;

typedef struct {
    MorseFlipperContentMode mode;
    uint8_t help_topic;
    uint32_t now_ms;
    const char* version;
    const char* build_time;
    const char* build_commit;
    const char* build_host;
} MorseFlipperContentEnterArgs;

typedef struct {
    bool handled;
    bool redraw;
    bool request_exit;
    MorseFlipperContentAction action;
    bool help_topic_changed;
    uint8_t help_topic;
} MorseFlipperContentResult;

typedef struct {
    MorseFlipperMappedFalApi mapped;
    bool (*enter)(void* state, const MorseFlipperContentEnterArgs* args);
    MorseFlipperContentResult (*input)(void* state, const InputEvent* event, uint32_t now_ms);
} MorseFlipperHelpAboutApi;
