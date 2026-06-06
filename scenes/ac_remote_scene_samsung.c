#include "../ac_remote_app_i.h"

typedef enum {
    button_power,
    button_mode,
    button_temp_up,
    button_fan,
    button_temp_down,
    button_swing,
    label_temperature,
} button_id;

const Icon* power[2][2] = {
    [0] = {&I_on_19x20, &I_on_hover_19x20},
    [1] = {&I_off_19x20, &I_off_hover_19x20},
};
const Icon* mode[5][2] = {
    [HvacSamsungModeCool] = {&I_cold_19x20, &I_cold_hover_19x20},
    [HvacSamsungModeHeat] = {&I_heat_19x20, &I_heat_hover_19x20},
    [HvacSamsungModeDry] = {&I_dry_19x20, &I_dry_hover_19x20},
    [HvacSamsungModeFan] = {&I_fan_19x20, &I_fan_hover_19x20},
    [HvacSamsungModeAuto] = {&I_auto_19x20, &I_auto_hover_19x20},
};
const Icon* fan[4][2] = {
    [HvacSamsungFanAuto] = {&I_fan_speed_auto_19x20, &I_fan_speed_auto_hover_19x20},
    [HvacSamsungFanLow] = {&I_fan_speed_1_19x20, &I_fan_speed_1_hover_19x20},
    [HvacSamsungFanMed] = {&I_fan_speed_2_19x20, &I_fan_speed_2_hover_19x20},
    [HvacSamsungFanHigh] = {&I_fan_speed_3_19x20, &I_fan_speed_3_hover_19x20},
};

char buffer[4] = {0};

bool ac_remote_load_settings(ACRemoteAppSettings* app_state) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_buffered_file_alloc(storage);
    FuriString* header = furi_string_alloc();

    uint32_t version = 0;
    bool success = false;
    do {
        if(!flipper_format_buffered_file_open_existing(ff, AC_REMOTE_APP_SETTINGS)) break;
        if(!flipper_format_read_header(ff, header, &version)) break;
        if(!furi_string_equal(header, "Samsung AC Remote") || (version != 1)) break;
        if(!flipper_format_read_uint32(ff, "Mode", &app_state->mode, 1)) break;
        if(app_state->mode > HvacSamsungModeAuto) break;
        if(!flipper_format_read_uint32(ff, "Temperature", &app_state->temperature, 1)) break;
        if(app_state->temperature < HVAC_SAMSUNG_TEMPERATURE_MIN ||
           app_state->temperature > HVAC_SAMSUNG_TEMPERATURE_MAX)
            break;
        if(!flipper_format_read_uint32(ff, "Fan", &app_state->fan, 1)) break;
        if(app_state->fan > HvacSamsungFanHigh) break;
        if(!flipper_format_read_uint32(ff, "Swing", &app_state->swing, 1)) break;
        if(app_state->swing > 1) break;
        if(!flipper_format_read_uint32(ff, "Power", &app_state->power, 1)) break;
        if(app_state->power > 1) break;
        success = true;
    } while(false);
    furi_record_close(RECORD_STORAGE);
    furi_string_free(header);
    flipper_format_free(ff);
    return success;
}

bool ac_remote_store_settings(ACRemoteAppSettings* app_state) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);

    bool success = false;
    do {
        if(!flipper_format_file_open_always(ff, AC_REMOTE_APP_SETTINGS)) break;
        if(!flipper_format_write_header_cstr(ff, "Samsung AC Remote", 1)) break;
        if(!flipper_format_write_comment_cstr(ff, "")) break;
        if(!flipper_format_write_uint32(ff, "Mode", &app_state->mode, 1)) break;
        if(!flipper_format_write_uint32(ff, "Temperature", &app_state->temperature, 1)) break;
        if(!flipper_format_write_uint32(ff, "Fan", &app_state->fan, 1)) break;
        if(!flipper_format_write_uint32(ff, "Swing", &app_state->swing, 1)) break;
        if(!flipper_format_write_uint32(ff, "Power", &app_state->power, 1)) success = true;
    } while(false);
    furi_record_close(RECORD_STORAGE);
    flipper_format_free(ff);
    return success;
}

void ac_remote_scene_universal_common_item_callback(void* context, uint32_t index) {
    AC_RemoteApp* ac_remote = context;
    uint32_t event = ac_remote_custom_event_pack(AC_RemoteCustomEventTypeButtonPressed, index);
    view_dispatcher_send_custom_event(ac_remote->view_dispatcher, event);
}

void ac_remote_displayed_temperature(
    const ACRemoteAppSettings* app_state,
    char* buffer,
    size_t buffer_size) {
    if(app_state->mode == HvacSamsungModeFan) {
        snprintf(buffer, buffer_size, "  ");
        return;
    }
    snprintf(buffer, buffer_size, "%ld", app_state->temperature);
}

void ac_remote_scene_samsung_on_enter(void* context) {
    AC_RemoteApp* ac_remote = context;
    ACRemotePanel* ac_remote_panel = ac_remote->ac_remote_panel;

    if(!ac_remote_load_settings(&ac_remote->app_state)) {
        ac_remote->app_state.power = 0;
        ac_remote->app_state.mode = HvacSamsungModeCool;
        ac_remote->app_state.fan = HvacSamsungFanAuto;
        ac_remote->app_state.temperature = HVAC_SAMSUNG_TEMPERATURE_DEFAULT;
        ac_remote->app_state.swing = 0;
    }

    view_stack_add_view(ac_remote->view_stack, ac_remote_panel_get_view(ac_remote_panel));
    ac_remote_panel_reserve(ac_remote_panel, 2, 3);

    ac_remote_panel_add_item(
        ac_remote_panel,
        button_power,
        0,
        0,
        6,
        17,
        power[ac_remote->app_state.power][0],
        power[ac_remote->app_state.power][1],
        ac_remote_scene_universal_common_item_callback,
        NULL,
        context);
    ac_remote_panel_add_icon(ac_remote_panel, 5, 39, &I_power_text_21x5);
    ac_remote_panel_add_item(
        ac_remote_panel,
        button_mode,
        1,
        0,
        39,
        17,
        mode[ac_remote->app_state.mode][0],
        mode[ac_remote->app_state.mode][1],
        ac_remote_scene_universal_common_item_callback,
        NULL,
        context);
    ac_remote_panel_add_icon(ac_remote_panel, 40, 39, &I_mode_text_17x5);
    ac_remote_panel_add_icon(ac_remote_panel, 0, 59, &I_frame_30x39);
    ac_remote_panel_add_item(
        ac_remote_panel,
        button_temp_up,
        0,
        1,
        3,
        47,
        &I_tempup_24x21,
        &I_tempup_hover_24x21,
        ac_remote_scene_universal_common_item_callback,
        NULL,
        context);
    ac_remote_panel_add_item(
        ac_remote_panel,
        button_temp_down,
        0,
        2,
        3,
        89,
        &I_tempdown_24x21,
        &I_tempdown_hover_24x21,
        ac_remote_scene_universal_common_item_callback,
        NULL,
        context);
    ac_remote_panel_add_item(
        ac_remote_panel,
        button_fan,
        1,
        1,
        39,
        50,
        fan[ac_remote->app_state.fan][0],
        fan[ac_remote->app_state.fan][1],
        ac_remote_scene_universal_common_item_callback,
        NULL,
        context);
    ac_remote_panel_add_icon(ac_remote_panel, 43, 72, &I_fan_text_12x5);
    ac_remote_panel_add_item(
        ac_remote_panel,
        button_swing,
        1,
        2,
        39,
        83,
        &I_swing_19x20,
        &I_swing_hover_19x20,
        ac_remote_scene_universal_common_item_callback,
        NULL,
        context);
    ac_remote_panel_add_icon(ac_remote_panel, 38, 105, &I_swing_text_20x5);

    ac_remote_panel_add_label(ac_remote_panel, 0, 6, 11, FontPrimary, "GNUSMAS");

    ac_remote_displayed_temperature(&ac_remote->app_state, buffer, sizeof(buffer));
    ac_remote_panel_add_label(ac_remote_panel, label_temperature, 4, 82, FontKeyboard, buffer);

    view_set_orientation(view_stack_get_view(ac_remote->view_stack), ViewOrientationVertical);
    view_dispatcher_switch_to_view(ac_remote->view_dispatcher, AC_RemoteAppViewStack);
}

void ac_remote_send_state(const ACRemoteAppSettings* settings) {
    furi_assert(settings);

    HvacSamsungPacket packet = hvac_samsung_create_packet();
    hvac_samsung_set_mode(packet, settings->mode);
    hvac_samsung_set_temperature(packet, settings->temperature);
    hvac_samsung_set_fan(packet, settings->fan);
    hvac_samsung_set_swing(packet, settings->swing);
    hvac_samsung_set_power(packet, settings->power);

    hvac_samsung_send(packet);
    hvac_samsung_free_packet(packet);
}

bool ac_remote_scene_samsung_on_event(void* context, SceneManagerEvent event) {
    AC_RemoteApp* ac_remote = context;
    ACRemotePanel* ac_remote_panel = ac_remote->ac_remote_panel;
    if(event.type != SceneManagerEventTypeCustom) {
        return false;
    }

    uint16_t event_type;
    int16_t event_value;
    ac_remote_custom_event_unpack(event.event, &event_type, &event_value);

    if(event_type == AC_RemoteCustomEventTypeSendSettings) {
        NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
        notification_message(notifications, &sequence_blink_white_100);
        ac_remote_send_state(&ac_remote->app_state);
        notification_message(notifications, &sequence_blink_stop);
        furi_record_close(RECORD_NOTIFICATION);
        return true;
    }

    if(event_type != AC_RemoteCustomEventTypeButtonPressed) {
        return true;
    }

    switch(event_value) {
    case button_power:
        ac_remote->app_state.power = ac_remote->app_state.power ? 0 : 1;
        ac_remote_panel_item_set_icons(
            ac_remote_panel,
            button_power,
            power[ac_remote->app_state.power][0],
            power[ac_remote->app_state.power][1]);
        break;
    case button_mode:
        ac_remote->app_state.mode++;
        if(ac_remote->app_state.mode > HvacSamsungModeAuto) {
            ac_remote->app_state.mode = HvacSamsungModeCool;
        }
        ac_remote_panel_item_set_icons(
            ac_remote_panel,
            button_mode,
            mode[ac_remote->app_state.mode][0],
            mode[ac_remote->app_state.mode][1]);

        ac_remote_displayed_temperature(&ac_remote->app_state, buffer, sizeof(buffer));
        ac_remote_panel_label_set_string(ac_remote_panel, label_temperature, buffer);

        if(!ac_remote->app_state.power) {
            return true;
        }
        break;
    case button_fan:
        ac_remote->app_state.fan++;
        if(ac_remote->app_state.fan > HvacSamsungFanHigh) {
            ac_remote->app_state.fan = HvacSamsungFanAuto;
        }
        ac_remote_panel_item_set_icons(
            ac_remote_panel,
            button_fan,
            fan[ac_remote->app_state.fan][0],
            fan[ac_remote->app_state.fan][1]);

        if(!ac_remote->app_state.power) {
            return true;
        }
        break;
    case button_temp_up:
        if(ac_remote->app_state.mode == HvacSamsungModeFan) {
            return true;
        }
        if(ac_remote->app_state.temperature < HVAC_SAMSUNG_TEMPERATURE_MAX) {
            ac_remote->app_state.temperature++;
            snprintf(buffer, sizeof(buffer), "%ld", ac_remote->app_state.temperature);
            ac_remote_panel_label_set_string(ac_remote_panel, label_temperature, buffer);
        }
        if(!ac_remote->app_state.power) {
            return true;
        }
        break;
    case button_temp_down:
        if(ac_remote->app_state.mode == HvacSamsungModeFan) {
            return true;
        }
        if(ac_remote->app_state.temperature > HVAC_SAMSUNG_TEMPERATURE_MIN) {
            ac_remote->app_state.temperature--;
            snprintf(buffer, sizeof(buffer), "%ld", ac_remote->app_state.temperature);
            ac_remote_panel_label_set_string(ac_remote_panel, label_temperature, buffer);
        }
        if(!ac_remote->app_state.power) {
            return true;
        }
        break;
    case button_swing:
        ac_remote->app_state.swing = ac_remote->app_state.swing ? 0 : 1;
        if(!ac_remote->app_state.power) {
            return true;
        }
        break;
    default:
        break;
    }

    view_dispatcher_send_custom_event(
        ac_remote->view_dispatcher,
        ac_remote_custom_event_pack(AC_RemoteCustomEventTypeSendSettings, 0));
    return true;
}

void ac_remote_scene_samsung_on_exit(void* context) {
    AC_RemoteApp* ac_remote = context;
    ACRemotePanel* ac_remote_panel = ac_remote->ac_remote_panel;
    ac_remote_store_settings(&ac_remote->app_state);
    view_stack_remove_view(ac_remote->view_stack, ac_remote_panel_get_view(ac_remote_panel));
    ac_remote_panel_reset(ac_remote_panel);
}
