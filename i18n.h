#pragma once

#include "settings.h"

typedef struct {
    const char* menu_tree;
    const char* menu_text;
    const char* menu_settings;
    const char* menu_about;
    const char* input_header;
    const char* hint_key;
    const char* play_playing;
    const char* play_paused;
    const char* play_done;
    const char* play_ok_pause;
    const char* play_ok_resume;
    const char* play_ok_repeat;
    const char* snd_on;
    const char* snd_off;
    const char* vib_on;
    const char* vib_off;
    const char* set_lang;
    const char* set_sound;
    const char* set_volume;
    const char* set_tone;
    const char* set_vibro;
    const char* set_led;
    const char* set_wpm;
    const char* set_dit;
    const char* set_letter_gap;
    const char* set_word_gap;
    const char* val_on;
    const char* val_off;
    const char* about;
} I18nStrings;

extern const I18nStrings i18n_strings[2]; // [0] = espanol, [1] = ingles
extern const char* const i18n_lang_names[2];

static inline const I18nStrings* i18n_get(const MorseSettings* s) {
    return &i18n_strings[s->lang & 1];
}
