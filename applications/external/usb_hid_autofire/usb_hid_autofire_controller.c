#include "usb_hid_autofire_i.h"

void usb_hid_autofire_input_callback(InputEvent* input_event, void* ctx) {
    FuriMessageQueue* event_queue = ctx;

    UsbMouseEvent event;
    event.type = EventTypeInput;
    event.input = *input_event;
    furi_message_queue_put(event_queue, &event, 0);
}

uint32_t usb_hid_autofire_delay_clamp(uint32_t delay_ms) {
    if(delay_ms < AUTOFIRE_DELAY_MIN_MS) {
        return AUTOFIRE_DELAY_MIN_MS;
    }

    if(delay_ms > AUTOFIRE_DELAY_MAX_MS) {
        return AUTOFIRE_DELAY_MAX_MS;
    }

    return delay_ms;
}

uint32_t usb_hid_autofire_delay_decrease(uint32_t delay_ms, uint32_t step_ms) {
    if(step_ms == 0U) {
        return usb_hid_autofire_delay_clamp(delay_ms);
    }

    delay_ms = usb_hid_autofire_delay_clamp(delay_ms);
    if(delay_ms <= AUTOFIRE_DELAY_MIN_MS) {
        return AUTOFIRE_DELAY_MIN_MS;
    }

    if((delay_ms - AUTOFIRE_DELAY_MIN_MS) < step_ms) {
        return AUTOFIRE_DELAY_MIN_MS;
    }

    return delay_ms - step_ms;
}

uint32_t usb_hid_autofire_delay_increase(uint32_t delay_ms, uint32_t step_ms) {
    if(step_ms == 0U) {
        return usb_hid_autofire_delay_clamp(delay_ms);
    }

    delay_ms = usb_hid_autofire_delay_clamp(delay_ms);
    if(delay_ms >= AUTOFIRE_DELAY_MAX_MS) {
        return AUTOFIRE_DELAY_MAX_MS;
    }

    if((AUTOFIRE_DELAY_MAX_MS - delay_ms) < step_ms) {
        return AUTOFIRE_DELAY_MAX_MS;
    }

    return delay_ms + step_ms;
}

uint32_t usb_hid_autofire_accel_step_ms(uint16_t repeat_count) {
    if(repeat_count >= AUTOFIRE_REPEAT_FAST_THRESHOLD) {
        return AUTOFIRE_DELAY_ACCEL_FAST_MS;
    }
    if(repeat_count >= AUTOFIRE_REPEAT_MEDIUM_THRESHOLD) {
        return AUTOFIRE_DELAY_ACCEL_MEDIUM_MS;
    }
    return AUTOFIRE_DELAY_STEP_MS;
}

const char* usb_hid_autofire_mode_label(AutofireMode mode) {
    switch(mode) {
    case AutofireModeMouseLeftClick:
        return "Mouse Left";
    case AutofireModeMouseRightClick:
        return "Mouse Right";
    case AutofireModeKeyboardEnter:
        return "Key Enter";
    case AutofireModeKeyboardSpace:
        return "Key Space";
    default:
        return "Unknown";
    }
}

AutofireMode usb_hid_autofire_next_mode(AutofireMode mode) {
    switch(mode) {
    case AutofireModeMouseLeftClick:
        return AutofireModeMouseRightClick;
    case AutofireModeMouseRightClick:
        return AutofireModeKeyboardEnter;
    case AutofireModeKeyboardEnter:
        return AutofireModeKeyboardSpace;
    case AutofireModeKeyboardSpace:
    default:
        return AutofireModeMouseLeftClick;
    }
}

AutofireMode usb_hid_autofire_prev_mode(AutofireMode mode) {
    switch(mode) {
    case AutofireModeMouseLeftClick:
        return AutofireModeKeyboardSpace;
    case AutofireModeMouseRightClick:
        return AutofireModeMouseLeftClick;
    case AutofireModeKeyboardEnter:
        return AutofireModeMouseRightClick;
    case AutofireModeKeyboardSpace:
        return AutofireModeKeyboardEnter;
    default:
        return AutofireModeMouseLeftClick;
    }
}

const char* usb_hid_autofire_preset_label(AutofirePreset preset) {
    switch(preset) {
    case AutofirePresetCustom:
        return "Custom";
    case AutofirePresetSlow:
        return "Slow";
    case AutofirePresetMedium:
        return "Medium";
    case AutofirePresetFast:
        return "Fast";
    default:
        return "Unknown";
    }
}

uint32_t usb_hid_autofire_preset_delay_ms(AutofirePreset preset) {
    switch(preset) {
    case AutofirePresetSlow:
        return AUTOFIRE_PRESET_SLOW_MS;
    case AutofirePresetMedium:
        return AUTOFIRE_PRESET_MEDIUM_MS;
    case AutofirePresetFast:
        return AUTOFIRE_PRESET_FAST_MS;
    case AutofirePresetCustom:
    default:
        return AUTOFIRE_DELAY_DEFAULT_MS;
    }
}

AutofirePreset usb_hid_autofire_next_preset(AutofirePreset preset) {
    switch(preset) {
    case AutofirePresetCustom:
        return AutofirePresetSlow;
    case AutofirePresetSlow:
        return AutofirePresetMedium;
    case AutofirePresetMedium:
        return AutofirePresetFast;
    case AutofirePresetFast:
    default:
        return AutofirePresetSlow;
    }
}

bool usb_hid_autofire_mode_is_valid(uint32_t mode_value) {
    return mode_value <= (uint32_t)AutofireModeKeyboardSpace;
}

bool usb_hid_autofire_preset_is_valid(uint32_t preset_value) {
    return preset_value < (uint32_t)AutofirePresetCount;
}

bool usb_hid_autofire_startup_policy_is_valid(uint32_t startup_policy_value) {
    return startup_policy_value <= (uint32_t)AutofireStartupPolicyRestoreLastState;
}

bool usb_hid_autofire_set_mode(UsbHidAutofireApp* app, AutofireMode new_mode) {
    if(new_mode == app->mode) {
        return false;
    }

    if(app->mouse_pressed) {
        usb_hid_autofire_release_mode_control(app);
        app->mouse_pressed = false;
    }

    app->mode = new_mode;
    app->click_phase = ClickPhasePress;
    if(app->active) {
        furi_timer_stop(app->click_timer);
        usb_hid_autofire_schedule_next_tick(app);
    }
    usb_hid_autofire_mark_settings_dirty(app);
    app->ui_dirty = true;

    return true;
}

bool usb_hid_autofire_set_delay(
    UsbHidAutofireApp* app,
    uint32_t new_delay_ms,
    AutofirePreset new_preset) {
    new_delay_ms = usb_hid_autofire_delay_clamp(new_delay_ms);
    if((new_delay_ms == app->autofire_delay_ms) && (new_preset == app->preset)) {
        return false;
    }

    bool delay_changed = (new_delay_ms != app->autofire_delay_ms);
    app->autofire_delay_ms = new_delay_ms;
    app->preset = new_preset;
    if(app->active && delay_changed) {
        furi_timer_stop(app->click_timer);
        usb_hid_autofire_schedule_next_tick(app);
    }
    usb_hid_autofire_mark_settings_dirty(app);
    app->ui_dirty = true;

    return true;
}

void usb_hid_autofire_adjust_delay(UsbHidAutofireApp* app, InputKey key, uint32_t step_ms) {
    if(key == InputKeyLeft) {
        usb_hid_autofire_set_delay(
            app,
            usb_hid_autofire_delay_decrease(app->autofire_delay_ms, step_ms),
            AutofirePresetCustom);
    } else if(key == InputKeyRight) {
        usb_hid_autofire_set_delay(
            app,
            usb_hid_autofire_delay_increase(app->autofire_delay_ms, step_ms),
            AutofirePresetCustom);
    }
}

static bool usb_hid_autofire_confirm_high_cps_preset(
    UsbHidAutofireApp* app,
    AutofirePreset preset,
    uint32_t preset_cps_x10) {
    if(!app->dialogs) {
        return true;
    }

    bool confirmed = false;
    DialogMessage* message = dialog_message_alloc();
    if(!message) {
        return false;
    }

    char cps_str[24];
    char text[72];
    usb_hid_autofire_format_cps(cps_str, sizeof(cps_str), preset_cps_x10);
    snprintf(
        text,
        sizeof(text),
        "%s preset: %s CPS\nApply this rate?",
        usb_hid_autofire_preset_label(preset),
        cps_str);

    dialog_message_set_header(message, "High Click Rate", 64, 12, AlignCenter, AlignTop);
    dialog_message_set_text(message, text, 64, 34, AlignCenter, AlignCenter);
    dialog_message_set_buttons(message, "No", NULL, "Yes");

    // Prevent stale buffered inputs from leaking into modal/result handling.
    usb_hid_autofire_drain_event_queue(app, EVENT_DRAIN_MAX_COUNT);
    DialogMessageButton result = dialog_message_show(app->dialogs, message);
    usb_hid_autofire_drain_event_queue(app, EVENT_DRAIN_MAX_COUNT);
    confirmed = (result == DialogMessageButtonRight);
    dialog_message_free(message);

    return confirmed;
}

void usb_hid_autofire_apply_preset_request(UsbHidAutofireApp* app, AutofirePreset preset) {
    uint32_t preset_delay_ms = usb_hid_autofire_preset_delay_ms(preset);
    uint32_t preset_cps_x10 = usb_hid_autofire_config_cps_x10_for_delay(preset_delay_ms);
    bool was_active = app->active;
    bool should_apply = true;

    if(was_active) {
        usb_hid_autofire_stop(app);
    }

    if(preset_cps_x10 >= HIGH_CPS_CONFIRM_THRESHOLD_X10) {
        should_apply = usb_hid_autofire_confirm_high_cps_preset(app, preset, preset_cps_x10);
    }

    if(should_apply) {
        usb_hid_autofire_set_delay(app, preset_delay_ms, preset);
    }

    if(was_active) {
        usb_hid_autofire_start(app);
        app->ui_dirty = true;
    }
}

void usb_hid_autofire_handle_delay_input(UsbHidAutofireApp* app, const InputEvent* input) {
    if((input->key != InputKeyLeft) && (input->key != InputKeyRight)) {
        return;
    }

    switch(input->type) {
    case InputTypeShort:
        usb_hid_autofire_adjust_delay(app, input->key, AUTOFIRE_DELAY_STEP_MS);
        break;

    case InputTypeLong:
        app->adjust_hold_active = true;
        app->adjust_hold_key = input->key;
        app->adjust_repeat_count = 0U;
        usb_hid_autofire_adjust_delay(app, input->key, AUTOFIRE_DELAY_ACCEL_MEDIUM_MS);
        break;

    case InputTypeRepeat:
        if((!app->adjust_hold_active) || (app->adjust_hold_key != input->key)) {
            app->adjust_hold_active = true;
            app->adjust_hold_key = input->key;
            app->adjust_repeat_count = 0U;
        }
        if(app->adjust_repeat_count < 0xFFFFU) {
            app->adjust_repeat_count++;
        }
        usb_hid_autofire_adjust_delay(
            app, input->key, usb_hid_autofire_accel_step_ms(app->adjust_repeat_count));
        break;

    case InputTypeRelease:
        if(app->adjust_hold_active && (app->adjust_hold_key == input->key)) {
            app->adjust_hold_active = false;
            app->adjust_hold_key = InputKeyMAX;
            app->adjust_repeat_count = 0U;
        }
        break;

    default:
        break;
    }
}

void usb_hid_autofire_handle_mode_input(UsbHidAutofireApp* app, const InputEvent* input) {
    if((input->key != InputKeyUp) && (input->key != InputKeyDown)) {
        return;
    }

    switch(input->type) {
    case InputTypeShort:
    case InputTypeLong:
    case InputTypeRepeat: {
        if((input->type == InputTypeLong) || (input->type == InputTypeRepeat)) {
            if((!app->mode_hold_active) || (app->mode_hold_key != input->key)) {
                app->mode_hold_active = true;
                app->mode_hold_key = input->key;
            }
        }

        AutofireMode next_mode = (input->key == InputKeyUp) ?
                                     usb_hid_autofire_prev_mode(app->mode) :
                                     usb_hid_autofire_next_mode(app->mode);
        usb_hid_autofire_set_mode(app, next_mode);
        break;
    }

    case InputTypeRelease:
        if(app->mode_hold_active && (app->mode_hold_key == input->key)) {
            app->mode_hold_active = false;
            app->mode_hold_key = InputKeyMAX;
        }
        break;

    default:
        break;
    }
}

bool usb_hid_autofire_handle_input_event(
    UsbHidAutofireApp* app,
    const InputEvent* input,
    bool* should_exit) {
    furi_check(should_exit);
    *should_exit = false;

    if(input->key == InputKeyBack) {
        if(input->type == InputTypeLong) {
            app->show_help = true;
            app->back_long_handled = true;
            app->ui_dirty = true;
            return true;
        }

        if(input->type == InputTypeRelease) {
            if(app->back_long_handled) {
                app->back_long_handled = false;
                return true;
            }
            if(app->show_help) {
                app->show_help = false;
                app->ui_dirty = true;
                return true;
            }
            *should_exit = true;
            return true;
        }
    }

    if(app->show_help) {
        return true;
    }

    if((input->key == InputKeyUp) || (input->key == InputKeyDown)) {
        usb_hid_autofire_handle_mode_input(app, input);
        return true;
    }

    if((input->key == InputKeyLeft) || (input->key == InputKeyRight)) {
        usb_hid_autofire_handle_delay_input(app, input);
        return true;
    } else if(input->key == InputKeyOk) {
        if(input->type == InputTypeLong) {
            AutofirePreset next_preset = usb_hid_autofire_next_preset(app->preset);
            usb_hid_autofire_apply_preset_request(app, next_preset);
            app->ok_long_handled = true;
        } else if(input->type == InputTypeRelease) {
            if(app->ok_long_handled) {
                app->ok_long_handled = false;
            } else if(app->active) {
                usb_hid_autofire_stop(app);
                app->last_active_state = false;
                usb_hid_autofire_mark_settings_dirty(app);
            } else {
                usb_hid_autofire_start(app);
                app->last_active_state = true;
                usb_hid_autofire_mark_settings_dirty(app);
            }
            app->ui_dirty = true;
        }
    }

    return true;
}
