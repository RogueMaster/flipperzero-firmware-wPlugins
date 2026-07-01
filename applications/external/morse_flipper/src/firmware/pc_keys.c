/*
 * Purpose: Name and resolve USB keyboard/mouse key presets.
 * Owns: PC paddle and straight key preset tables.
 * Depends on: pc_keys.h.
 * Tests: tests/test_keyboard_presets.c.
 */

#include "pc_keys.h"

#include <stddef.h>

typedef struct {
    const char* name;
    uint8_t dit_key;
    uint8_t dah_key;
} MorsePcPaddlePreset;

typedef struct {
    const char* name;
    uint8_t key;
} MorsePcStraightPreset;

static const MorsePcPaddlePreset morse_pc_paddle_presets[] = {
    {"X Z", MorsePcKeyX, MorsePcKeyZ},
    {"Ctrl-s", MorsePcKeyLeftCtrl, MorsePcKeyRightCtrl},
    {"Shift-s", MorsePcKeyLeftShift, MorsePcKeyRightShift},
    {". /", MorsePcKeyPeriod, MorsePcKeySlash},
    {", .", MorsePcKeyComma, MorsePcKeyPeriod},
    {"< >", MorsePcKeyLess, MorsePcKeyGreater},
    {"[ ]", MorsePcKeyOpenBracket, MorsePcKeyCloseBracket},
};

static const MorsePcStraightPreset morse_pc_straight_presets[] = {
    {"Space", MorsePcKeySpace},
    {"X", MorsePcKeyX},
    {"Z", MorsePcKeyZ},
    {"C", MorsePcKeyC},
    {"Enter", MorsePcKeyEnter},
    {"Num enter", MorsePcKeyNumEnter},
};

static uint8_t morse_pc_clamp_paddle_idx(uint8_t idx) {
    return (idx < morse_pc_paddle_preset_count()) ? idx : 0U;
}

static uint8_t morse_pc_clamp_straight_idx(uint8_t idx) {
    return (idx < morse_pc_straight_preset_count()) ? idx : 0U;
}

uint8_t morse_pc_paddle_preset_count(void) {
    return (uint8_t)(sizeof(morse_pc_paddle_presets) / sizeof(morse_pc_paddle_presets[0]));
}

uint8_t morse_pc_straight_preset_count(void) {
    return (uint8_t)(sizeof(morse_pc_straight_presets) / sizeof(morse_pc_straight_presets[0]));
}

const char* morse_pc_paddle_preset_name(uint8_t idx) {
    return morse_pc_paddle_presets[morse_pc_clamp_paddle_idx(idx)].name;
}

const char* morse_pc_straight_preset_name(uint8_t idx) {
    return morse_pc_straight_presets[morse_pc_clamp_straight_idx(idx)].name;
}

const char* morse_pc_key_name(uint8_t key) {
    switch(key) {
    case MorsePcKeySpace:
        return "Space";
    case MorsePcKeyX:
        return "X";
    case MorsePcKeyZ:
        return "Z";
    case MorsePcKeyC:
        return "C";
    case MorsePcKeyEnter:
        return "Enter";
    case MorsePcKeyNumEnter:
        return "Num enter";
    case MorsePcKeyOpenBracket:
        return "[";
    case MorsePcKeyCloseBracket:
        return "]";
    case MorsePcKeyLeftCtrl:
        return "L Ctrl";
    case MorsePcKeyRightCtrl:
        return "R Ctrl";
    case MorsePcKeyLeftShift:
        return "L Shift";
    case MorsePcKeyRightShift:
        return "R Shift";
    case MorsePcKeyPeriod:
        return ".";
    case MorsePcKeySlash:
        return "/";
    case MorsePcKeyComma:
        return ",";
    case MorsePcKeyLess:
        return "<";
    case MorsePcKeyGreater:
        return ">";
    default:
        return "-";
    }
}

uint8_t morse_pc_straight_preset_key(uint8_t idx) {
    return morse_pc_straight_presets[morse_pc_clamp_straight_idx(idx)].key;
}

uint8_t morse_pc_paddle_preset_key(uint8_t idx, uint8_t note, bool swapped) {
    const MorsePcPaddlePreset* p = &morse_pc_paddle_presets[morse_pc_clamp_paddle_idx(idx)];
    bool dah = note == 2U;

    if(swapped) {
        dah = !dah;
    }

    return dah ? p->dah_key : p->dit_key;
}

uint8_t morse_pc_mouse_button(uint8_t note, bool inverted) {
    uint8_t btn = MorsePcMouseBtnNone;

    if(note == 2U)
        btn = MorsePcMouseBtnRight;
    else if(note == 0U || note == 1U)
        btn = MorsePcMouseBtnLeft;

    if(inverted) {
        if(btn == MorsePcMouseBtnLeft) return MorsePcMouseBtnRight;
        if(btn == MorsePcMouseBtnRight) return MorsePcMouseBtnLeft;
    }

    return btn;
}
