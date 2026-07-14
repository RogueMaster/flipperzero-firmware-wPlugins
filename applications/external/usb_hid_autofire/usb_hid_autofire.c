#include "usb_hid_autofire_i.h"

int32_t usb_hid_autofire_app(void* p) {
    UNUSED(p);
    int32_t ret = -1;
    bool gui_opened = false;
    bool view_port_added = false;
    bool usb_switched = false;

    UsbHidAutofireApp app = {
        .event_queue = NULL,
        .view_port = NULL,
        .gui = NULL,
        .dialogs = NULL,
        .click_timer = NULL,
        .ui_refresh_timer = NULL,
        .settings_save_timer = NULL,
        .usb_mode_prev = NULL,
        .active = false,
        .mouse_pressed = false,
        .ui_dirty = true,
        .settings_dirty = false,
        .show_help = false,
        .last_active_state = false,
        .autofire_delay_ms = AUTOFIRE_DELAY_DEFAULT_MS,
        .realtime_cps_x10 = 0U,
        .last_click_release_tick_ms = 0U,
        .last_click_interval_ms = 0U,
        .adjust_hold_active = false,
        .adjust_hold_key = InputKeyMAX,
        .adjust_repeat_count = 0U,
        .mode_hold_active = false,
        .mode_hold_key = InputKeyMAX,
        .ok_long_handled = false,
        .back_long_handled = false,
        .mode = AutofireModeMouseLeftClick,
        .preset = AutofirePresetCustom,
        .startup_policy = AutofireStartupPolicyPausedOnLaunch,
        .click_phase = ClickPhasePress,
    };

    app.autofire_delay_ms = usb_hid_autofire_delay_clamp(app.autofire_delay_ms);
    usb_hid_autofire_settings_load(&app);
    usb_hid_autofire_reset_cps_tracking(&app);

    app.event_queue = furi_message_queue_alloc(16, sizeof(UsbMouseEvent));
    if(!app.event_queue) {
        FURI_LOG_E(TAG, "Failed to allocate event queue");
        goto cleanup;
    }

    app.view_port = view_port_alloc();
    if(!app.view_port) {
        FURI_LOG_E(TAG, "Failed to allocate viewport");
        goto cleanup;
    }

    app.click_timer = furi_timer_alloc(usb_hid_autofire_timer_callback, FuriTimerTypeOnce, &app);
    if(!app.click_timer) {
        FURI_LOG_E(TAG, "Failed to allocate timer");
        goto cleanup;
    }

    app.ui_refresh_timer =
        furi_timer_alloc(usb_hid_autofire_ui_timer_callback, FuriTimerTypePeriodic, &app);
    if(!app.ui_refresh_timer) {
        FURI_LOG_E(TAG, "Failed to allocate UI refresh timer");
        goto cleanup;
    }

    app.settings_save_timer =
        furi_timer_alloc(usb_hid_autofire_settings_save_timer_callback, FuriTimerTypeOnce, &app);
    if(!app.settings_save_timer) {
        FURI_LOG_E(TAG, "Failed to allocate settings save timer");
        goto cleanup;
    }

    app.usb_mode_prev = furi_hal_usb_get_config();
#ifndef USB_HID_AUTOFIRE_SCREENSHOT
    furi_hal_usb_unlock();
    if(!furi_hal_usb_set_config(&usb_hid, NULL)) {
        FURI_LOG_E(TAG, "Failed to switch USB to HID mode");
        goto cleanup;
    }
    usb_switched = true;
#endif

    view_port_draw_callback_set(app.view_port, usb_hid_autofire_render_callback, &app);
    view_port_input_callback_set(app.view_port, usb_hid_autofire_input_callback, app.event_queue);

    app.gui = furi_record_open(RECORD_GUI);
    if(!app.gui) {
        FURI_LOG_E(TAG, "Failed to open GUI record");
        goto cleanup;
    }
    gui_opened = true;
    gui_add_view_port(app.gui, app.view_port, GuiLayerFullscreen);
    view_port_added = true;

    app.dialogs = furi_record_open(RECORD_DIALOGS);

    if((app.startup_policy == AutofireStartupPolicyRestoreLastState) && app.last_active_state) {
        usb_hid_autofire_start(&app);
    }

    view_port_update(app.view_port);
    app.ui_dirty = false;

    UsbMouseEvent event;
    while(1) {
        FuriStatus event_status = furi_message_queue_get(app.event_queue, &event, FuriWaitForever);
        if(event_status != FuriStatusOk) {
            continue;
        }

        if(event.type == EventTypeTick) {
            usb_hid_autofire_tick(&app);
        } else if(event.type == EventTypeUiRefresh) {
            uint32_t new_cps_x10 = usb_hid_autofire_realtime_cps_x10(&app);
            if(new_cps_x10 != app.realtime_cps_x10) {
                app.realtime_cps_x10 = new_cps_x10;
                app.ui_dirty = true;
            }
        } else if(event.type == EventTypeSettingsSave) {
            usb_hid_autofire_settings_flush_if_dirty(&app);
        } else if(event.type == EventTypeInput) {
            bool should_exit = false;
            usb_hid_autofire_handle_input_event(&app, &event.input, &should_exit);
            if(should_exit) {
                break;
            }
        }

        if(app.ui_dirty) {
            view_port_update(app.view_port);
            app.ui_dirty = false;
        }
    }

    ret = 0;

cleanup:
    usb_hid_autofire_settings_flush_if_dirty(&app);
    usb_hid_autofire_stop(&app);

#ifndef USB_HID_AUTOFIRE_SCREENSHOT
    if(usb_switched) {
        furi_hal_usb_set_config(app.usb_mode_prev, NULL);
    }
#endif

    if(view_port_added && app.gui && app.view_port) {
        gui_remove_view_port(app.gui, app.view_port);
    }

    if(app.click_timer) {
        furi_timer_stop(app.click_timer);
        furi_timer_free(app.click_timer);
    }

    if(app.ui_refresh_timer) {
        furi_timer_stop(app.ui_refresh_timer);
        furi_timer_free(app.ui_refresh_timer);
    }

    if(app.settings_save_timer) {
        furi_timer_stop(app.settings_save_timer);
        furi_timer_free(app.settings_save_timer);
    }

    if(app.view_port) {
        view_port_free(app.view_port);
    }

    if(app.event_queue) {
        furi_message_queue_free(app.event_queue);
    }

    if(gui_opened) {
        furi_record_close(RECORD_GUI);
    }
    if(app.dialogs) {
        furi_record_close(RECORD_DIALOGS);
    }

    return ret;
}
