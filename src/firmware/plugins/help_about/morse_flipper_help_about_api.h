#pragma once

#include <gui/canvas.h>
#include <input/input.h>

#include <stdbool.h>
#include <stdint.h>

#define MORSE_FLIPPER_HELP_ABOUT_API_VERSION 1U
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
    MorseFlipperContentAction action;
    bool redraw;
    bool help_topic_changed;
    uint8_t help_topic;
} MorseFlipperContentResult;

typedef struct {
    uint32_t magic;
    uint32_t api_version;
    uint32_t struct_size;
    void* (*alloc)(void);
    void (*free)(void* state);
    bool (*enter)(void* state, const MorseFlipperContentEnterArgs* args);
    void (*leave)(void* state);
    MorseFlipperContentResult (*input)(void* state, const InputEvent* event, uint32_t now_ms);
    bool (*tick)(void* state, uint32_t now_ms);
    void (*draw)(void* state, Canvas* canvas);
} MorseFlipperHelpAboutApi;
