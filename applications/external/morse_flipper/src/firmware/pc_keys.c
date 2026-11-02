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
    {"X Z azerty (FR)", MorsePcKeyX, MorsePcKeyW},
    {"X Z qwertz (DE)", MorsePcKeyX, MorsePcKeyY},
};

static const MorsePcStraightPreset morse_pc_straight_presets[] = {
    {"Space", MorsePcKeySpace},
    {"X", MorsePcKeyX},
    {"Z", MorsePcKeyZ},
    {"C", MorsePcKeyC},
    {"Enter", MorsePcKeyEnter},
    {"Num enter", MorsePcKeyNumEnter},
    {"Z azerty (FR)", MorsePcKeyW},
    {"Z qwertz (DE)", MorsePcKeyY},
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
    case MorsePcKeyW:
        return "W";
    case MorsePcKeyY:
        return "Y";
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

/* USB HID keyboard usage ids (US layout). Low byte of the HID keycode. */
#define MORSE_HID_USAGE_A      0x04U
#define MORSE_HID_USAGE_1      0x1EU
#define MORSE_HID_USAGE_0      0x27U
#define MORSE_HID_USAGE_SPACE  0x2CU
#define MORSE_HID_USAGE_MINUS  0x2DU
#define MORSE_HID_USAGE_EQUAL  0x2EU
#define MORSE_HID_USAGE_SEMI   0x33U
#define MORSE_HID_USAGE_QUOTE  0x34U
#define MORSE_HID_USAGE_COMMA  0x36U
#define MORSE_HID_USAGE_DOT    0x37U
#define MORSE_HID_USAGE_SLASH  0x38U

uint8_t morse_pc_hid_usage_for_char(char c, bool* needs_shift) {
    uint8_t usage = 0U;
    bool shift = false;

    if(c >= 'a' && c <= 'z') {
        usage = (uint8_t)(MORSE_HID_USAGE_A + (uint8_t)(c - 'a'));
    } else if(c >= 'A' && c <= 'Z') {
        /* Morse is case-less; type lowercase (no shift) for readable text. */
        usage = (uint8_t)(MORSE_HID_USAGE_A + (uint8_t)(c - 'A'));
    } else if(c >= '1' && c <= '9') {
        usage = (uint8_t)(MORSE_HID_USAGE_1 + (uint8_t)(c - '1'));
    } else {
        switch(c) {
        case '0':
            usage = MORSE_HID_USAGE_0;
            break;
        case ' ':
            usage = MORSE_HID_USAGE_SPACE;
            break;
        case '-':
            usage = MORSE_HID_USAGE_MINUS;
            break;
        case '=':
            usage = MORSE_HID_USAGE_EQUAL;
            break;
        case ';':
            usage = MORSE_HID_USAGE_SEMI;
            break;
        case '\'':
            usage = MORSE_HID_USAGE_QUOTE;
            break;
        case ',':
            usage = MORSE_HID_USAGE_COMMA;
            break;
        case '.':
            usage = MORSE_HID_USAGE_DOT;
            break;
        case '/':
            usage = MORSE_HID_USAGE_SLASH;
            break;
        /* Shifted symbols the CW decoder can emit. */
        case '!':
            usage = MORSE_HID_USAGE_1; /* shift + 1 */
            shift = true;
            break;
        case '@':
            usage = (uint8_t)(MORSE_HID_USAGE_1 + 1U); /* shift + 2 */
            shift = true;
            break;
        case '&':
            usage = (uint8_t)(MORSE_HID_USAGE_1 + 6U); /* shift + 7 */
            shift = true;
            break;
        case '(':
            usage = (uint8_t)(MORSE_HID_USAGE_1 + 8U); /* shift + 9 */
            shift = true;
            break;
        case ')':
            usage = MORSE_HID_USAGE_0; /* shift + 0 */
            shift = true;
            break;
        case '+':
            usage = MORSE_HID_USAGE_EQUAL; /* shift + = */
            shift = true;
            break;
        case ':':
            usage = MORSE_HID_USAGE_SEMI; /* shift + ; */
            shift = true;
            break;
        case '"':
            usage = MORSE_HID_USAGE_QUOTE; /* shift + ' */
            shift = true;
            break;
        case '?':
            usage = MORSE_HID_USAGE_SLASH; /* shift + / */
            shift = true;
            break;
        case '_':
            usage = MORSE_HID_USAGE_MINUS; /* shift + - */
            shift = true;
            break;
        default:
            usage = 0U;
            break;
        }
    }

    if(needs_shift != NULL) *needs_shift = (usage != 0U) && shift;
    return usage;
}
