#include "usb_hid_autofire_i.h"

void usb_hid_autofire_timer_callback(void* ctx) {
    UsbHidAutofireApp* app = ctx;
    UsbMouseEvent event = {.type = EventTypeTick};
    furi_message_queue_put(app->event_queue, &event, 0);
}

void usb_hid_autofire_ui_timer_callback(void* ctx) {
    UsbHidAutofireApp* app = ctx;
    UsbMouseEvent event = {.type = EventTypeUiRefresh};
    furi_message_queue_put(app->event_queue, &event, 0);
}

void usb_hid_autofire_format_cps(char* out, size_t out_size, uint32_t cps_x10) {
    snprintf(
        out, out_size, "%lu.%lu", (unsigned long)(cps_x10 / 10U), (unsigned long)(cps_x10 % 10U));
}

uint32_t usb_hid_autofire_config_cps_x10_for_delay(uint32_t delay_ms) {
    uint32_t half_delay_ms = delay_ms / 2U;
    if(half_delay_ms == 0U) {
        half_delay_ms = 1U;
    }

    uint32_t cycle_ms = half_delay_ms * 2U;
    return (10000U + (cycle_ms / 2U)) / cycle_ms;
}

static uint32_t usb_hid_autofire_effective_cycle_ms(const UsbHidAutofireApp* app) {
    uint32_t half_delay_ms = app->autofire_delay_ms / 2U;
    if(half_delay_ms == 0U) {
        half_delay_ms = 1U;
    }
    return half_delay_ms * 2U;
}

void usb_hid_autofire_reset_cps_tracking(UsbHidAutofireApp* app) {
    app->realtime_cps_x10 = 0U;
    app->last_click_release_tick_ms = 0U;
    app->last_click_interval_ms = usb_hid_autofire_effective_cycle_ms(app);
}

static void usb_hid_autofire_record_click_release(UsbHidAutofireApp* app) {
    uint32_t now_ms = furi_get_tick();
    if(app->last_click_release_tick_ms != 0U) {
        uint32_t interval_ms = now_ms - app->last_click_release_tick_ms;
        if(interval_ms == 0U) {
            interval_ms = 1U;
        }
        app->last_click_interval_ms = interval_ms;
    }
    app->last_click_release_tick_ms = now_ms;
}

uint32_t usb_hid_autofire_realtime_cps_x10(const UsbHidAutofireApp* app) {
    if(!app->active || (app->last_click_release_tick_ms == 0U) ||
       (app->last_click_interval_ms == 0U)) {
        return 0U;
    }

    uint32_t elapsed_ms = furi_get_tick() - app->last_click_release_tick_ms;
    uint32_t effective_interval_ms = app->last_click_interval_ms;
    if(elapsed_ms > effective_interval_ms) {
        effective_interval_ms = elapsed_ms;
    }
    if(effective_interval_ms == 0U) {
        effective_interval_ms = 1U;
    }

    return (10000U + (effective_interval_ms / 2U)) / effective_interval_ms;
}

static uint32_t usb_hid_autofire_half_delay_ticks(const UsbHidAutofireApp* app) {
    uint32_t half_delay_ms = app->autofire_delay_ms / 2;
    if(half_delay_ms == 0) {
        half_delay_ms = 1;
    }
    return furi_ms_to_ticks(half_delay_ms);
}

void usb_hid_autofire_schedule_next_tick(UsbHidAutofireApp* app) {
    furi_timer_start(app->click_timer, usb_hid_autofire_half_delay_ticks(app));
}

void usb_hid_autofire_drain_event_queue(UsbHidAutofireApp* app, uint8_t max_count) {
    UsbMouseEvent event;
    for(uint8_t i = 0; i < max_count; i++) {
        if(furi_message_queue_get(app->event_queue, &event, 0) != FuriStatusOk) {
            break;
        }
    }
}

void usb_hid_autofire_press_mode_control(UsbHidAutofireApp* app) {
    switch(app->mode) {
    case AutofireModeMouseLeftClick:
        furi_hal_hid_mouse_press(HID_MOUSE_BTN_LEFT);
        break;
    case AutofireModeMouseRightClick:
        furi_hal_hid_mouse_press(HID_MOUSE_BTN_RIGHT);
        break;
    case AutofireModeKeyboardEnter:
        furi_hal_hid_kb_press(HID_KEYBOARD_RETURN);
        break;
    case AutofireModeKeyboardSpace:
        furi_hal_hid_kb_press(HID_KEYBOARD_SPACEBAR);
        break;
    default:
        break;
    }
}

void usb_hid_autofire_release_mode_control(UsbHidAutofireApp* app) {
    switch(app->mode) {
    case AutofireModeMouseLeftClick:
        furi_hal_hid_mouse_release(HID_MOUSE_BTN_LEFT);
        break;
    case AutofireModeMouseRightClick:
        furi_hal_hid_mouse_release(HID_MOUSE_BTN_RIGHT);
        break;
    case AutofireModeKeyboardEnter:
        furi_hal_hid_kb_release(HID_KEYBOARD_RETURN);
        break;
    case AutofireModeKeyboardSpace:
        furi_hal_hid_kb_release(HID_KEYBOARD_SPACEBAR);
        break;
    default:
        break;
    }
}

void usb_hid_autofire_start(UsbHidAutofireApp* app) {
    app->active = true;
    app->click_phase = ClickPhasePress;
    usb_hid_autofire_reset_cps_tracking(app);
    furi_timer_start(app->ui_refresh_timer, furi_ms_to_ticks(UI_REFRESH_PERIOD_MS));
    usb_hid_autofire_tick(app);
}

void usb_hid_autofire_stop(UsbHidAutofireApp* app) {
    app->active = false;
    app->click_phase = ClickPhasePress;
    if(app->click_timer) {
        furi_timer_stop(app->click_timer);
    }
    if(app->ui_refresh_timer) {
        furi_timer_stop(app->ui_refresh_timer);
    }

    if(app->mouse_pressed) {
        usb_hid_autofire_release_mode_control(app);
        app->mouse_pressed = false;
    }
    furi_hal_hid_kb_release_all();

    usb_hid_autofire_reset_cps_tracking(app);
    app->adjust_hold_active = false;
    app->adjust_hold_key = InputKeyMAX;
    app->adjust_repeat_count = 0U;
    app->mode_hold_active = false;
    app->mode_hold_key = InputKeyMAX;
    app->ok_long_handled = false;
    app->back_long_handled = false;
}

void usb_hid_autofire_tick(UsbHidAutofireApp* app) {
    if(!app->active) {
        return;
    }

    if(app->click_phase == ClickPhasePress) {
        usb_hid_autofire_press_mode_control(app);
        app->mouse_pressed = true;
        app->click_phase = ClickPhaseRelease;
    } else {
        usb_hid_autofire_release_mode_control(app);
        app->mouse_pressed = false;
        usb_hid_autofire_record_click_release(app);
        app->click_phase = ClickPhasePress;
    }

    usb_hid_autofire_schedule_next_tick(app);
}
