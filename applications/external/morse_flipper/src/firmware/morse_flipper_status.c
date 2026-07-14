/*
 * Purpose: Compute status strings and input gating for active screens.
 * Owns: footer/status text, live input eligibility, and mode labels.
 * Depends on: morse_flipper_app_i.h and runtime feature state.
 * Tests: firmware build plus feature-specific host tests.
 */

#include "morse_flipper_app_i.h"

MorseFlipperInputGate morse_flipper_input_gate(const MorseFlipperApp* app) {
    MorseFlipperInputGate g = {0};

    if(app == NULL) return g;

    if(app->screen == MorseFlipperScreenRun || app->screen == MorseFlipperScreenTrace) {
        g.live = true;
    } else if(app->screen == MorseFlipperScreenSession) {
        g.live = morse_flipper_session_repeat_active(app) ||
                 morse_flipper_session_running_view(app);
    } else if(app->screen == MorseFlipperScreenRf || app->screen == MorseFlipperScreenRfRx) {
        g.live = app->rf_live_active;
    } else if(app->screen == MorseFlipperScreenStraight) {
        g.live = app->straight_wait_answer;
    } else if(app->screen == MorseFlipperScreenTxGroups) {
        g.live = app->txg_wait_answer;
    }

    if(!g.live || app->input_source != MorseFlipperInputSourceButtons) {
        g.back_exit = g.live;
        return g;
    }

    g.btn = true;
    if(app->screen == MorseFlipperScreenStraight) {
        g.btn_str = true;
        g.back_exit = true;
        return g;
    }

    if(app->screen == MorseFlipperScreenTxGroups && app->txg_sk) {
        g.btn_str = true;
        g.back_exit = true;
        return g;
    }

    if(morse_flipper_straight_like_mode(app))
        g.btn_str = true;
    else
        g.btn_pad = true;

    g.back_key = g.btn_pad;
    g.back_exit = !g.back_key;
    g.left_hint = g.back_key;
    return g;
}

bool morse_flipper_live_back_is_key(const MorseFlipperApp* app) {
    return morse_flipper_input_gate(app).back_key;
}

bool morse_flipper_live_left_hint(const MorseFlipperApp* app) {
    if(app != NULL && app->screen == MorseFlipperScreenHamRun) return true;
    return morse_flipper_input_gate(app).left_hint;
}

const char* morse_flipper_status_line(const MorseFlipperApp* app, char* buf, size_t buf_sz) {
    if(app->speaker_busy) {
        snprintf(buf, buf_sz, "speaker busy");
        return buf;
    }

    if(app->input_source == MorseFlipperInputSourceButtons) {
        if(morse_flipper_straight_like_mode(app)) {
            snprintf(buf, buf_sz, "src btn ok str");
            return buf;
        }
        if(app->handedness == MorseFlipperHandednessSwapped) {
            snprintf(buf, buf_sz, "src btn hand swp");
            return buf;
        }
        snprintf(buf, buf_sz, "src btn hand norm");
        return buf;
    }

    if(app->input_source == MorseFlipperInputSourcePaddle) {
        if(app->handedness == MorseFlipperHandednessSwapped) {
            snprintf(
                buf,
                buf_sz,
                "src %s/%s swp",
                morse_flipper_gpio_name(app->gpio_dit_idx),
                morse_flipper_gpio_name(app->gpio_dah_idx));
            return buf;
        }
        snprintf(
            buf,
            buf_sz,
            "src %s/%s norm",
            morse_flipper_gpio_name(app->gpio_dit_idx),
            morse_flipper_gpio_name(app->gpio_dah_idx));
        return buf;
    }

    snprintf(
        buf,
        buf_sz,
        "src %s straight",
        morse_flipper_gpio_name(morse_flipper_gpio_straight_idx(app)));
    return buf;
}

const char* morse_flipper_hand_name(const MorseFlipperApp* app) {
    return (app->handedness == MorseFlipperHandednessSwapped) ? "swp" : "norm";
}

uint8_t morse_flipper_ok_button_paddle(const MorseFlipperApp* app) {
    return (app->handedness == MorseFlipperHandednessSwapped) ? MorseKeyerPaddleDit :
                                                                MorseKeyerPaddleDah;
}

uint8_t morse_flipper_back_button_paddle(const MorseFlipperApp* app) {
    return (app->handedness == MorseFlipperHandednessSwapped) ? MorseKeyerPaddleDah :
                                                                MorseKeyerPaddleDit;
}

const char* morse_flipper_run_hint(const MorseFlipperApp* app, char* buf, size_t buf_sz) {
    if(app->input_source == MorseFlipperInputSourceButtons) {
        snprintf(
            buf,
            buf_sz,
            "%s",
            morse_flipper_live_back_is_key(app) ? "Left clear; hold L exit" :
                                                  "Bk exit; Left clear");
        return buf;
    }

    snprintf(buf, buf_sz, "Bk exit; Left clear");
    return buf;
}

const char* morse_flipper_run_mode_line(const MorseFlipperApp* app, char* buf, size_t buf_sz) {
    const char* input;
    const char* keyer;

    if(app == NULL || buf == NULL || buf_sz == 0U) return "---";

    input = morse_flipper_run_input_name(app);
    keyer = morse_flipper_run_keyer_name(app);
    if(morse_flipper_straight_like_mode(app) || strcmp(keyer, "---") == 0) {
        snprintf(buf, buf_sz, "%u wpm  %s", (unsigned)morse_flipper_current_wpm(app), input);
    } else {
        snprintf(
            buf, buf_sz, "%u wpm  %s  %s", (unsigned)morse_flipper_current_wpm(app), input, keyer);
    }

    return buf;
}

const char* morse_flipper_run_input_name(const MorseFlipperApp* app) {
    if(app == NULL) return "---";
    if(app->input_source == MorseFlipperInputSourceButtons) return "buttons";
    if(app->input_source == MorseFlipperInputSourcePaddle) return "paddles";
    return "straight";
}

const char* morse_flipper_run_keyer_name(const MorseFlipperApp* app) {
    uint8_t mode;

    if(app == NULL) return "---";
    if(morse_flipper_straight_like_mode(app)) return "---";

    mode = morse_flipper_current_keyer_mode(app);
    switch(mode) {
    case MorseKeyerModeBug:
        return "bug";
    case MorseKeyerModeElBug:
        return "elbug";
    case MorseKeyerModeSingleDot:
        return "single-dot";
    case MorseKeyerModeUltimatic:
        return "ultimatic";
    case MorseKeyerModePlainIambic:
        return "plain";
    case MorseKeyerModeIambicA:
        return "elekey-a";
    case MorseKeyerModeIambicB:
        return "elekey-b";
    case MorseKeyerModeKeyahead:
        return "keyahead";
    default:
        return "---";
    }
}

const char* morse_flipper_run_usb_name(const MorseFlipperApp* app) {
    if(app == NULL) return "USB off";

    switch(app->pc_mode) {
    case MorseFlipperPcModeMidi:
        return "MIDI";
    case MorseFlipperPcModeKeyboard:
        return "Kbd";
    case MorseFlipperPcModeMouse:
        return "Mouse";
    default:
        return "USB off";
    }
}

const char* morse_flipper_trace_hint(const MorseFlipperApp* app, char* buf, size_t buf_sz) {
    if(app->input_source == MorseFlipperInputSourceButtons) {
        snprintf(
            buf,
            buf_sz,
            "%s",
            morse_flipper_live_back_is_key(app) ? "key live hold L" : "OK str Bk back");
        return buf;
    }

    snprintf(buf, buf_sz, "gpio live  Bk back");
    return buf;
}

const char* morse_flipper_source_short_name(const MorseFlipperApp* app, char* buf, size_t buf_sz) {
    if(app->input_source == MorseFlipperInputSourceButtons) {
        snprintf(buf, buf_sz, "btn");
        return buf;
    }

    if(app->input_source == MorseFlipperInputSourcePaddle) {
        snprintf(
            buf,
            buf_sz,
            "%s/%s",
            morse_flipper_gpio_name(app->gpio_dit_idx),
            morse_flipper_gpio_name(app->gpio_dah_idx));
        return buf;
    }

    snprintf(buf, buf_sz, "%s", morse_flipper_gpio_name(morse_flipper_gpio_straight_idx(app)));
    return buf;
}

bool morse_flipper_session_left_exit_active(const MorseFlipperApp* app) {
    return morse_flipper_live_left_hint(app);
}
