/*
 * Purpose: Name and resolve Flipper GPIO pins used by Morse inputs.
 * Owns: pin lookup tables, default pin choices, and conflict helpers.
 * Depends on: morse_flipper_gpio.h and Flipper HAL GPIO when built as FAP.
 * Tests: tests/test_gpio.c.
 */

#ifdef MORSE_FLIPPER_FAP
#include "morse_flipper_app_i.h"
#else
#include "morse_flipper_gpio.h"
#endif

static const char* const morse_flipper_gpio_names[MORSE_FLIPPER_GPIO_PIN_COUNT] = {
    "P2",
    "P3",
    "P4",
    "P5",
    "P6",
    "P7",
    "P15",
    "P16",
};

#ifdef MORSE_FLIPPER_FAP
const GpioPin* const morse_flipper_gpio_pins[MORSE_FLIPPER_GPIO_PIN_COUNT] = {
    &gpio_ext_pa7,
    &gpio_ext_pa6,
    &gpio_ext_pa4,
    &gpio_ext_pb3,
    &gpio_ext_pb2,
    &gpio_ext_pc3,
    &gpio_ext_pc1,
    &gpio_ext_pc0,
};

const GpioPin* morse_flipper_straight_pin = &gpio_ext_pa6;
const GpioPin* morse_flipper_dit_pin = &gpio_ext_pc3;
const GpioPin* morse_flipper_dah_pin = &gpio_ext_pb3;
const GpioPin* morse_flipper_ground_pin = NULL;
const GpioPin* morse_flipper_ptt_pin = NULL;
#endif

bool morse_flipper_gpio_pin_valid(uint8_t pin_idx) {
    return pin_idx < MORSE_FLIPPER_GPIO_PIN_COUNT;
}

const char* morse_flipper_gpio_name(uint8_t pin_idx) {
    if(pin_idx == MORSE_FLIPPER_GPIO_PIN_NONE) {
        return "None";
    }

    if(!morse_flipper_gpio_pin_valid(pin_idx)) {
        return "?";
    }

    return morse_flipper_gpio_names[pin_idx];
}

uint8_t morse_flipper_gpio_default_dit(void) {
    return MorseFlipperGpioPinP7;
}

uint8_t morse_flipper_gpio_default_dah(void) {
    return MorseFlipperGpioPinP5;
}

uint8_t morse_flipper_gpio_default_ground(void) {
    return MorseFlipperGpioPinP3;
}

MorseFlipperGpioRule morse_flipper_gpio_validate(uint8_t dit, uint8_t dah, uint8_t ground) {
    if(!morse_flipper_gpio_pin_valid(dit) || !morse_flipper_gpio_pin_valid(dah)) {
        return MorseFlipperGpioRuleBadIndex;
    }

    if(ground != MORSE_FLIPPER_GPIO_PIN_NONE && !morse_flipper_gpio_pin_valid(ground)) {
        return MorseFlipperGpioRuleBadIndex;
    }

    if(dit == dah) {
        return MorseFlipperGpioRulePaddlesSharePin;
    }

    if(ground != MORSE_FLIPPER_GPIO_PIN_NONE && (ground == dit || ground == dah)) {
        return MorseFlipperGpioRuleGroundShared;
    }

    return MorseFlipperGpioRuleOk;
}

MorseFlipperGpioRule
    morse_flipper_gpio_validate_with_ptt(uint8_t dit, uint8_t dah, uint8_t ground, uint8_t ptt) {
    MorseFlipperGpioRule rule = morse_flipper_gpio_validate(dit, dah, ground);

    if(rule != MorseFlipperGpioRuleOk) return rule;
    if(ptt == MORSE_FLIPPER_GPIO_PIN_NONE) return MorseFlipperGpioRuleOk;
    if(ptt != MorseFlipperGpioPinP16) return MorseFlipperGpioRuleBadIndex;
    if(ptt == dit || ptt == dah || ptt == ground) return MorseFlipperGpioRulePttShared;

    return MorseFlipperGpioRuleOk;
}

const char* morse_flipper_gpio_rule_text(MorseFlipperGpioRule rule) {
    switch(rule) {
    case MorseFlipperGpioRulePaddlesSharePin:
        return "Dit and dah must differ.";
    case MorseFlipperGpioRuleGroundShared:
        return "Virtual gnd must stay unique.";
    case MorseFlipperGpioRulePttShared:
        return "PTT/TX must stay unique.";
    case MorseFlipperGpioRuleBadIndex:
        return "That GPIO choice is not valid.";
    default:
        return "ok";
    }
}

#ifdef MORSE_FLIPPER_FAP
uint8_t morse_flipper_gpio_straight_idx(const MorseFlipperApp* app) {
    if(app == NULL) {
        return morse_flipper_gpio_default_dit();
    }

    return morse_flipper_gpio_pin_valid(app->gpio_dit_idx) ? app->gpio_dit_idx :
                                                             morse_flipper_gpio_default_dit();
}

void morse_flipper_gpio_sync_straight_idx(MorseFlipperApp* app) {
    if(app == NULL) return;
    app->gpio_straight_idx = morse_flipper_gpio_straight_idx(app);
}

const GpioPin* morse_flipper_gpio_pin_ptr(uint8_t pin_idx) {
    if(!morse_flipper_gpio_pin_valid(pin_idx)) {
        return morse_flipper_gpio_pins[morse_flipper_gpio_default_dit()];
    }

    return morse_flipper_gpio_pins[pin_idx];
}

static uint8_t morse_flipper_gpio_no_reserved(uint8_t pin_idx, uint8_t def) {
    return (pin_idx == MorseFlipperGpioPinP2 || pin_idx == MorseFlipperGpioPinP15) ? def : pin_idx;
}

static const GpioPin* morse_flipper_gpio_ground_pin_ptr(uint8_t pin_idx) {
    if(pin_idx == MORSE_FLIPPER_GPIO_PIN_NONE) {
        return NULL;
    }

    if(!morse_flipper_gpio_pin_valid(pin_idx)) {
        return morse_flipper_gpio_pins[morse_flipper_gpio_default_ground()];
    }

    return morse_flipper_gpio_pins[pin_idx];
}

static const GpioPin* morse_flipper_gpio_ptt_pin_ptr(const MorseFlipperApp* app) {
    uint8_t ptt;

    if(app == NULL) return NULL;

    ptt = app->gpio_ptt_idx;
    if(ptt == MORSE_FLIPPER_GPIO_PIN_NONE) return NULL;
    if(ptt != MorseFlipperGpioPinP16) return NULL;
    if(ptt == morse_flipper_gpio_straight_idx(app) || ptt == app->gpio_dit_idx ||
       ptt == app->gpio_dah_idx || ptt == app->gpio_ground_idx)
        return NULL;

    return morse_flipper_gpio_pins[ptt];
}

void morse_flipper_gpio_bind_from_app(const MorseFlipperApp* app) {
    morse_flipper_straight_pin = morse_flipper_gpio_pin_ptr(morse_flipper_gpio_no_reserved(
        morse_flipper_gpio_straight_idx(app), morse_flipper_gpio_default_dit()));
    morse_flipper_dit_pin = morse_flipper_gpio_pin_ptr(
        morse_flipper_gpio_no_reserved(app->gpio_dit_idx, morse_flipper_gpio_default_dit()));
    morse_flipper_dah_pin = morse_flipper_gpio_pin_ptr(
        morse_flipper_gpio_no_reserved(app->gpio_dah_idx, morse_flipper_gpio_default_dah()));
    morse_flipper_ground_pin = morse_flipper_gpio_ground_pin_ptr(
        morse_flipper_gpio_no_reserved(app->gpio_ground_idx, morse_flipper_gpio_default_ground()));
    morse_flipper_ptt_pin = morse_flipper_gpio_ptt_pin_ptr(app);
}

void morse_flipper_gpio_reset_candidates(void) {
    for(size_t i = 0; i < COUNT_OF(morse_flipper_gpio_pins); i++) {
        furi_hal_gpio_init(morse_flipper_gpio_pins[i], GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    }
}

void morse_flipper_gpio_apply(MorseFlipperApp* app) {
    const GpioPin* ham_key_pin = morse_flipper_gpio_pins[MorseFlipperGpioPinP15];

    morse_flipper_gpio_bind_from_app(app);
    morse_flipper_gpio_reset_candidates();

    furi_hal_gpio_init(morse_flipper_dit_pin, GpioModeInput, GpioPullUp, GpioSpeedLow);
    furi_hal_gpio_init(morse_flipper_dah_pin, GpioModeInput, GpioPullUp, GpioSpeedLow);
    furi_hal_gpio_init(ham_key_pin, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_write(ham_key_pin, false);
    if(morse_flipper_ground_pin != NULL) {
        furi_hal_gpio_init(
            morse_flipper_ground_pin, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
        furi_hal_gpio_write(morse_flipper_ground_pin, false);
    }
    if(morse_flipper_ptt_pin != NULL) {
        furi_hal_gpio_init(
            morse_flipper_ptt_pin, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
        furi_hal_gpio_write(morse_flipper_ptt_pin, false);
    }

    if(app != NULL) {
        app->ham.key_level = false;
        app->ptt_level = false;
        app->ptt_tail_until = 0U;
    }
}

void morse_flipper_gpio_alert(MorseFlipperApp* app, MorseFlipperGpioRule rule) {
    DialogMessage* msg;

    if(app == NULL || app->dialogs == NULL || rule == MorseFlipperGpioRuleOk) {
        return;
    }

    msg = dialog_message_alloc();
    dialog_message_set_header(msg, "GPIO conflict", 64, 10, AlignCenter, AlignTop);
    dialog_message_set_text(
        msg, morse_flipper_gpio_rule_text(rule), 64, 30, AlignCenter, AlignTop);
    dialog_message_set_buttons(msg, NULL, "OK", NULL);
    dialog_message_show(app->dialogs, msg);
    dialog_message_free(msg);
}

bool morse_flipper_gpio_try_apply(
    MorseFlipperApp* app,
    uint8_t dit,
    uint8_t dah,
    uint8_t ground,
    uint8_t ptt,
    MorseFlipperGpioRule* out_rule) {
    MorseFlipperGpioRule rule = morse_flipper_gpio_validate_with_ptt(dit, dah, ground, ptt);

    if(out_rule != NULL) {
        *out_rule = rule;
    }

    if(rule != MorseFlipperGpioRuleOk) {
        return false;
    }

    app->gpio_dit_idx = dit;
    app->gpio_dah_idx = dah;
    app->gpio_ground_idx = ground;
    app->gpio_ptt_idx = ptt == MorseFlipperGpioPinP16 ? MorseFlipperGpioPinP16 :
                                                        MORSE_FLIPPER_GPIO_PIN_NONE;
    morse_flipper_gpio_sync_straight_idx(app);
    morse_flipper_gpio_apply(app);
    morse_flipper_poll(app);
    morse_flipper_view_dirty(app);
    morse_flipper_save_config(app);
    return true;
}

void morse_flipper_sync_ptt(MorseFlipperApp* app, uint32_t now_ms) {
    bool tx_active;
    bool want_high;
    bool want_key;
    const GpioPin* ham_key_pin = morse_flipper_gpio_pins[MorseFlipperGpioPinP15];
    const GpioPin* ham_ptt_pin = morse_flipper_gpio_pins[MorseFlipperGpioPinP16];

    if(app == NULL) return;

    if(app->screen == MorseFlipperScreenHamRun) {
        tx_active = (app->note_sources[0] != 0U) || (app->note_sources[1] != 0U) ||
                    (app->note_sources[2] != 0U);
        want_key = app->ham_keyer.break_in_enabled && tx_active;
        if(app->ham_keyer.break_in_enabled && tx_active)
            app->ptt_tail_until = now_ms + ((uint32_t)morse_flipper_current_dit_ms(app) * 7U);
        want_high = app->ham_keyer.break_in_enabled &&
                    (tx_active || (app->ptt_tail_until != 0U && now_ms < app->ptt_tail_until));
        if(!want_high) app->ptt_tail_until = 0U;

        if(app->ham.key_level != want_key) {
            furi_hal_gpio_write(ham_key_pin, want_key);
            app->ham.key_level = want_key;
        }
        if(app->ptt_level != want_high) {
            furi_hal_gpio_write(ham_ptt_pin, want_high);
            app->ptt_level = want_high;
        }
        return;
    }

    if(app->screen != MorseFlipperScreenRun) {
        app->ptt_tail_until = 0U;
        want_key = false;
        want_high = false;
    } else {
        want_key = (app->note_sources[0] != 0U) || (app->note_sources[1] != 0U) ||
                   (app->note_sources[2] != 0U);
        tx_active = want_key || (app->preview_ticks > 0U);
        if(morse_flipper_ptt_pin != NULL && tx_active)
            app->ptt_tail_until = now_ms + ((uint32_t)morse_flipper_current_dit_ms(app) * 7U);
        want_high = morse_flipper_ptt_pin != NULL &&
                    (tx_active || (app->ptt_tail_until != 0U && now_ms < app->ptt_tail_until));
        if(!want_high) app->ptt_tail_until = 0U;
    }

    if(app->ham.key_level != want_key) {
        furi_hal_gpio_write(ham_key_pin, want_key);
        app->ham.key_level = want_key;
    }
    if(app->ptt_level != want_high) {
        if(morse_flipper_ptt_pin != NULL) furi_hal_gpio_write(morse_flipper_ptt_pin, want_high);
        app->ptt_level = want_high;
    }
}
#endif
