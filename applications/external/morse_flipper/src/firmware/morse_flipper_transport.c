/*
 * Purpose: Bridge keying events to USB keyboard, mouse, and MIDI transports.
 * Owns: PC mode state names, HID key mapping, and MIDI note output.
 * Depends on: morse_flipper_app_i.h, pc_keys.h, Flipper HID, and USB MIDI.
 * Tests: tests/test_keyboard_presets.c covers preset mapping data.
 */

#include "morse_flipper_app_i.h"

const char* morse_flipper_pc_state_name(const MorseFlipperApp* app) {
    bool up = false;

    if(app->pc_mode == MorseFlipperPcModeMidi)
        return morse_usb_midi_is_connected() ? "usb midi up" : "usb midi wait";

    if(app->pc_mode == MorseFlipperPcModeKeyboard) {
        up = furi_hal_hid_is_connected();
        return up ? "usb hid up" : "usb hid wait";
    }

    if(app->pc_mode == MorseFlipperPcModeMouse) {
        up = furi_hal_hid_is_connected();
        return up ? "usb mouse up" : "usb mouse wait";
    }

    return "usb local off";
}

uint8_t morse_flipper_nearest_tone_idx_for_midi(uint8_t midi_note) {
    uint8_t best_idx = 0U;
    uint8_t best_delta = 0xFFU;

    for(uint8_t i = 0U; i < COUNT_OF(morse_flipper_tones); i++) {
        uint8_t tone_note = morse_flipper_tones[i].midi_note;
        uint8_t delta = (tone_note > midi_note) ? (tone_note - midi_note) :
                                                  (midi_note - tone_note);
        if(delta < best_delta) {
            best_delta = delta;
            best_idx = i;
        }
    }

    return best_idx;
}

bool morse_flipper_transport_connected(const MorseFlipperApp* app) {
    if(app->pc_mode == MorseFlipperPcModeMidi) return morse_usb_midi_is_connected();

    if(app->pc_mode == MorseFlipperPcModeKeyboard || app->pc_mode == MorseFlipperPcModeMouse)
        return furi_hal_hid_is_connected();

    return false;
}

static uint8_t morse_flipper_keyboard_key_for_note(const MorseFlipperApp* app, uint8_t note) {
    switch(note) {
    case 0:
        return morse_pc_straight_preset_key(app->pc_straight_preset);
    case 1:
    case 2:
        return morse_pc_paddle_preset_key(app->pc_paddle_preset, note, false);
    default:
        return MorsePcKeyNone;
    }
}

static uint16_t morse_flipper_hid_key_for_pc_key(uint8_t key) {
    switch(key) {
    case MorsePcKeySpace:
        return HID_KEYBOARD_SPACEBAR;
    case MorsePcKeyX:
        return HID_KEYBOARD_X;
    case MorsePcKeyZ:
        return HID_KEYBOARD_Z;
    case MorsePcKeyC:
        return HID_KEYBOARD_C;
    case MorsePcKeyEnter:
        return HID_KEYBOARD_RETURN;
    case MorsePcKeyNumEnter:
        return HID_KEYPAD_ENTER;
    case MorsePcKeyOpenBracket:
        return HID_KEYBOARD_OPEN_BRACKET;
    case MorsePcKeyCloseBracket:
        return HID_KEYBOARD_CLOSE_BRACKET;
    case MorsePcKeyLeftCtrl:
        return HID_KEYBOARD_L_CTRL;
    case MorsePcKeyRightCtrl:
        return HID_KEYBOARD_R_CTRL;
    case MorsePcKeyLeftShift:
        return HID_KEYBOARD_L_SHIFT;
    case MorsePcKeyRightShift:
        return HID_KEYBOARD_R_SHIFT;
    case MorsePcKeyPeriod:
    case MorsePcKeyGreater:
        return HID_KEYBOARD_DOT;
    case MorsePcKeySlash:
        return HID_KEYBOARD_SLASH;
    case MorsePcKeyComma:
    case MorsePcKeyLess:
        return HID_KEYBOARD_COMMA;
    default:
        return HID_KEYBOARD_NONE;
    }
}

static bool morse_flipper_pc_key_needs_shift(uint8_t key) {
    return key == MorsePcKeyLess || key == MorsePcKeyGreater;
}

static void morse_flipper_send_midi_note(uint8_t note, bool active) {
    uint8_t packet[4];

    packet[0] = active ? 0x09U : 0x08U;
    packet[1] = active ? 0x90U : 0x80U;
    packet[2] = note;
    packet[3] = active ? 0x7FU : 0x00U;

    morse_usb_midi_tx(packet, sizeof(packet));
}

static void morse_flipper_send_keyboard_note(MorseFlipperApp* app, uint8_t note, bool active) {
    uint8_t pc_key = morse_flipper_keyboard_key_for_note(app, note);
    uint16_t key = morse_flipper_hid_key_for_pc_key(pc_key);

    if(key == HID_KEYBOARD_NONE) return;

    if(active) {
        if(morse_flipper_pc_key_needs_shift(pc_key)) furi_hal_hid_kb_press(HID_KEYBOARD_L_SHIFT);
        furi_hal_hid_kb_press(key);
    } else {
        furi_hal_hid_kb_release(key);
        if(morse_flipper_pc_key_needs_shift(pc_key)) furi_hal_hid_kb_release(HID_KEYBOARD_L_SHIFT);
    }
}

void morse_flipper_release_mouse_buttons(void) {
    furi_hal_hid_mouse_release(HID_MOUSE_BTN_LEFT);
    furi_hal_hid_mouse_release(HID_MOUSE_BTN_RIGHT);
}

static void morse_flipper_send_mouse_note(MorseFlipperApp* app, uint8_t note, bool active) {
    uint8_t btn = morse_pc_mouse_button(note, app->mouse_invert);

    if(btn == MorsePcMouseBtnNone) return;

    if(active) {
        furi_hal_hid_mouse_press(btn);
    } else {
        furi_hal_hid_mouse_release(btn);
    }
}

void morse_flipper_send_transport_note(MorseFlipperApp* app, uint8_t note, bool active) {
    switch(app->pc_mode) {
    case MorseFlipperPcModeMidi:
        morse_flipper_send_midi_note(note, active);
        break;
    case MorseFlipperPcModeKeyboard:
        morse_flipper_send_keyboard_note(app, note, active);
        break;
    case MorseFlipperPcModeMouse:
        morse_flipper_send_mouse_note(app, note, active);
        break;
    default:
        break;
    }
}

static void morse_flipper_clear_vail_overrides(MorseFlipperApp* app) {
    app->vail_mode_active = false;
    app->vail_speed_active = false;
    app->vail_tone_active = false;
    morse_flipper_update_sidetone(app);
}

static void morse_flipper_apply_vail_speed(MorseFlipperApp* app, uint8_t value) {
    uint16_t dit_ms = (value == 0U) ? 1U : ((uint16_t)value * 2U);

    if(app->vail_speed_active && app->vail_dit_ms == dit_ms) return;

    app->vail_speed_active = true;
    app->vail_dit_ms = dit_ms;
    morse_flipper_refresh_keyer(app, furi_get_tick());
    morse_flipper_view_dirty(app);
}

static void morse_flipper_apply_vail_mode(MorseFlipperApp* app, uint8_t program) {
    uint8_t mode = morse_keyer_mode_valid(program) ? program : MorseKeyerModePassthrough;

    if(app->vail_mode_active && app->vail_keyer_mode == mode) return;

    app->vail_mode_active = true;
    app->vail_keyer_mode = mode;
    morse_flipper_refresh_keyer(app, furi_get_tick());
    morse_flipper_view_dirty(app);
}

static void morse_flipper_apply_vail_tone(MorseFlipperApp* app, uint8_t midi_note) {
    uint8_t tone_idx = morse_flipper_nearest_tone_idx_for_midi(midi_note);

    if(app->vail_tone_active && app->vail_tone_idx == tone_idx) return;

    app->vail_tone_active = true;
    app->vail_tone_idx = tone_idx;
    morse_flipper_update_sidetone(app);

    morse_flipper_view_dirty(app);
}

void morse_flipper_resync_transport_notes(MorseFlipperApp* app) {
    for(uint8_t note = 0U; note < COUNT_OF(app->note_sources); note++) {
        if(app->note_sources[note] != 0U) morse_flipper_send_transport_note(app, note, true);
    }
}

void morse_flipper_midi_rx_ready(void* context) {
    MorseFlipperApp* app = context;

    if(app == NULL) return;
    app->midi_rx_pending = true;
}

void morse_flipper_set_pc_mode(MorseFlipperApp* app, uint8_t mode) {
    const char* prod = "Morse Flipper Kbd";

    if(mode > MorseFlipperPcModeMidi) mode = MorseFlipperPcModeOff;
    if(app->pc_mode == mode) return;

    morse_flipper_release_all_notes(app);

    if(app->pc_mode == MorseFlipperPcModeKeyboard) {
        furi_hal_hid_kb_release_all();
    } else if(app->pc_mode == MorseFlipperPcModeMouse) {
        morse_flipper_release_mouse_buttons();
    }

    if(mode == MorseFlipperPcModeOff) {
        morse_usb_midi_set_rx_callback(NULL);
        morse_usb_midi_set_context(NULL);
        if(app->previous_usb_config != NULL) {
            furi_check(furi_hal_usb_set_config(app->previous_usb_config, NULL));
            app->previous_usb_config = NULL;
        }
        app->pc_mode = MorseFlipperPcModeOff;
        app->transport_connected = false;
        app->midi_rx_pending = false;
        morse_flipper_clear_vail_overrides(app);
        morse_flipper_refresh_keyer(app, furi_get_tick());
        return;
    }

    if(app->previous_usb_config == NULL) app->previous_usb_config = furi_hal_usb_get_config();

    if(mode == MorseFlipperPcModeMidi) {
        morse_usb_midi_set_context(app);
        morse_usb_midi_set_rx_callback(morse_flipper_midi_rx_ready);
        furi_check(furi_hal_usb_set_config(NULL, NULL));
        furi_delay_ms(150U);
        furi_check(furi_hal_usb_set_config(&morse_usb_midi_interface, NULL));
    } else {
        if(mode == MorseFlipperPcModeMouse) prod = "Morse Flipper Mouse";
        snprintf(app->hid_cfg.product, sizeof(app->hid_cfg.product), "%s", prod);
        morse_usb_midi_set_rx_callback(NULL);
        morse_usb_midi_set_context(NULL);
        app->midi_rx_pending = false;
        furi_check(furi_hal_usb_set_config(NULL, NULL));
        furi_delay_ms(150U);
        furi_check(furi_hal_usb_set_config(&usb_hid, &app->hid_cfg));
        morse_flipper_clear_vail_overrides(app);
    }

    app->pc_mode = mode;
    app->transport_connected = false;
    morse_flipper_refresh_keyer(app, furi_get_tick());
}

void morse_flipper_handle_midi_rx(MorseFlipperApp* app) {
    uint8_t buffer[64];
    size_t size = morse_usb_midi_rx(buffer, sizeof(buffer));

    for(size_t offset = 0; offset + 3U < size; offset += 4U) {
        uint8_t status = buffer[offset + 1U] & 0xF0U;

        if(status == 0xB0U) {
            uint8_t control = buffer[offset + 2U];
            uint8_t value = buffer[offset + 3U];

            if(control == 1U) {
                morse_flipper_apply_vail_speed(app, value);
            } else if(control == 2U) {
                morse_flipper_apply_vail_tone(app, value);
            }
        } else if(status == 0xC0U) {
            morse_flipper_apply_vail_mode(app, buffer[offset + 2U]);
        }
    }
}
