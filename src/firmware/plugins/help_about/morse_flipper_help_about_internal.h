#pragma once

#include "cw_markdown_widget.h"
#include "morse_flipper_help_about_api.h"

#include <storage/storage.h>
#include <furi_hal.h>
#include <furi_hal_rtc.h>

typedef struct {
    FuriString* help_text;
    MorseFlipperContentMode mode;
    uint8_t help_topic;
    uint8_t help_page;
    uint8_t help_card_count;
    uint8_t onboarding_page;
    uint8_t onboarding_card_count;
    uint8_t about_mode;
    uint8_t about_ok_count;
    uint8_t about_social_idx;
    uint8_t about_footer_seq_i;
    bool help_chapter_card;
    bool about_show_next;
    uint32_t about_last_ok_ms;
    uint32_t about_social_next_ms;
    CwmdState onboarding_md;
    CwmdState help_md;
    CwmdState about_md;
    char about_body[512];
} MorseFlipperHelpAboutState;

bool morse_flipper_help_content_enter(
    MorseFlipperHelpAboutState* state,
    const MorseFlipperContentEnterArgs* args);
MorseFlipperContentResult morse_flipper_help_content_input(
    MorseFlipperHelpAboutState* state,
    const InputEvent* event,
    uint32_t now_ms);
bool morse_flipper_help_content_tick(MorseFlipperHelpAboutState* state, uint32_t now_ms);
void morse_flipper_help_content_draw(MorseFlipperHelpAboutState* state, Canvas* canvas);

void morse_flipper_help_open(MorseFlipperHelpAboutState* state);
void morse_flipper_onboarding_open(MorseFlipperHelpAboutState* state);
void morse_flipper_help_enter_chapter(MorseFlipperHelpAboutState* state);
bool morse_flipper_help_show_next_chapter(MorseFlipperHelpAboutState* state);
uint8_t morse_flipper_help_card_count(const MorseFlipperHelpAboutState* state);
void morse_flipper_draw_onboarding(Canvas* canvas, MorseFlipperHelpAboutState* state);
void morse_flipper_draw_help(Canvas* canvas, MorseFlipperHelpAboutState* state);
void morse_flipper_about_reset(MorseFlipperHelpAboutState* state, uint32_t now_ms);
bool morse_flipper_tick_about(MorseFlipperHelpAboutState* state, uint32_t now_ms);
void morse_flipper_draw_about(Canvas* canvas, MorseFlipperHelpAboutState* state);
