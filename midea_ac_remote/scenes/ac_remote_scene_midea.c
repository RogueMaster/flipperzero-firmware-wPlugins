#include "../ac_remote_app_i.h"

typedef enum {
    button_power,
    button_mode,
    button_temp_up,
    button_fan,
    button_temp_down,
    button_swing,
    button_turbo,
    button_led,
    button_clean,
    label_temperature,
} button_id;

typedef enum {
    command_swing,
    command_swing_vertical,
    command_turbo,
    command_led,
    command_led_change_info,
    command_clean,
    command_silent,
} command_id;

const Icon* power[2][2] = {
    [0] = {&I_on_19x20, &I_on_hover_19x20},
    [1] = {&I_off_19x20, &I_off_hover_19x20},
};
const Icon* mode[5][2] = {
    [HvacMideaModeCold] = {&I_cold_19x20, &I_cold_hover_19x20},
    [HvacMideaModeDry] = {&I_dry_19x20, &I_dry_hover_19x20},
    [HvacMideaModeFan] = {&I_fan_19x20, &I_fan_hover_19x20},
    [HvacMideaModeHeat] = {&I_heat_19x20, &I_heat_hover_19x20},
    [HvacMideaModeAuto] = {&I_auto_19x20, &I_auto_hover_19x20}};
const Icon* fan[4][2] = {
    [HvacMideaFanPower1] = {&I_fan_speed_1_19x20, &I_fan_speed_1_hover_19x20},
    [HvacMideaFanPower2] = {&I_fan_speed_2_19x20, &I_fan_speed_2_hover_19x20},
    [HvacMideaFanPower3] = {&I_fan_speed_3_19x20, &I_fan_speed_3_hover_19x20},
    [HvacMideaFanPowerAuto] = {&I_fan_speed_auto_19x20, &I_fan_speed_auto_hover_19x20}};

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
        if(!furi_string_equal(header, "AC Remote") || (version != 1)) break;
        if(!flipper_format_read_uint32(ff, "Mode", &app_state->mode, 1)) break;
        if(app_state->mode > HvacMideaModeAuto) break;
        if(!flipper_format_read_uint32(ff, "Temperature", &app_state->temperature, 1)) break;
        if(app_state->temperature > HVAC_MIDEA_TEMPERATURE_MAX) break;
        if(!flipper_format_read_uint32(ff, "Fan", &app_state->fan, 1)) break;
        if(app_state->fan > HvacMideaFanPowerAuto) break;
        if(!flipper_format_read_uint32(ff, "Power", &app_state->power, 1)) break;
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
        if(!flipper_format_write_header_cstr(ff, "AC Remote", 1)) break;
        if(!flipper_format_write_comment_cstr(ff, "")) break;
        if(!flipper_format_write_uint32(ff, "Mode", &app_state->mode, 1)) break;
        if(!flipper_format_write_uint32(ff, "Temperature", &app_state->temperature, 1)) break;
        if(!flipper_format_write_uint32(ff, "Fan", &app_state->fan, 1)) break;
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

void ac_remote_scene_universal_common_item_callback_long(void* context, uint32_t index) {
    AC_RemoteApp* ac_remote = context;
    uint32_t event = ac_remote_custom_event_pack(AC_RemoteCustomEventTypeButtonLongPressed, index);
    view_dispatcher_send_custom_event(ac_remote->view_dispatcher, event);
}

HvacMideaFanPower ac_remote_displayed_fan_power(const ACRemoteAppSettings* app_state) {
    furi_assert(app_state);

    switch(app_state->mode) {
    case HvacMideaModeDry:
    case HvacMideaModeAuto:
        return HvacMideaFanPowerAuto;
    default:
        return app_state->fan;
    }
}

void ac_remote_displayed_temperature(
    const ACRemoteAppSettings* app_state,
    char* buffer,
    size_t buffer_size) {
    if(app_state->mode == HvacMideaModeFan) {
        snprintf(buffer, buffer_size, "  ");
        return;
    }

    snprintf(buffer, buffer_size, "%ld", app_state->temperature);
}

void ac_remote_scene_midea_on_enter(void* context) {
    AC_RemoteApp* ac_remote = context;
    ACRemotePanel* ac_remote_panel = ac_remote->ac_remote_panel;

    if(!ac_remote_load_settings(&ac_remote->app_state)) {
        ac_remote->app_state.power = 0;
        ac_remote->app_state.mode = HvacMideaModeCold;
        ac_remote->app_state.fan = HvacMideaFanPowerAuto;
        ac_remote->app_state.temperature = HVAC_MIDEA_TEMPERATURE_DEFAULT;
        ac_remote->app_state.silent_mode = 0;
    }

    view_stack_add_view(ac_remote->view_stack, ac_remote_panel_get_view(ac_remote_panel));
    ac_remote_panel_reserve(ac_remote_panel, 3, 4);

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
        fan[ac_remote_displayed_fan_power(&ac_remote->app_state)][0],
        fan[ac_remote_displayed_fan_power(&ac_remote->app_state)][1],
        ac_remote_scene_universal_common_item_callback,
        ac_remote_scene_universal_common_item_callback_long,
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
        ac_remote_scene_universal_common_item_callback_long,
        context);
    ac_remote_panel_add_icon(ac_remote_panel, 38, 105, &I_swing_text_20x5);
    ac_remote_panel_add_item(
        ac_remote_panel,
        button_turbo,
        0,
        3,
        1,
        115,
        &I_turbo_19x11,
        &I_turbo_hover_19x11,
        ac_remote_scene_universal_common_item_callback,
        NULL,
        context);
    ac_remote_panel_add_item(
        ac_remote_panel,
        button_led,
        1,
        3,
        22,
        115,
        &I_led_19x11,
        &I_led_hover_19x11,
        ac_remote_scene_universal_common_item_callback,
        ac_remote_scene_universal_common_item_callback_long,
        context);
    ac_remote_panel_add_item(
        ac_remote_panel,
        button_clean,
        2,
        3,
        43,
        115,
        &I_clean_19x11,
        &I_clean_hover_19x11,
        ac_remote_scene_universal_common_item_callback,
        ac_remote_scene_universal_common_item_callback_long,
        context);

    ac_remote_panel_add_label(ac_remote_panel, 0, 6, 11, FontPrimary, "AC remote");

    ac_remote_displayed_temperature(&ac_remote->app_state, buffer, sizeof(buffer));
    ac_remote_panel_add_label(ac_remote_panel, label_temperature, 4, 82, FontKeyboard, buffer);

    view_set_orientation(view_stack_get_view(ac_remote->view_stack), ViewOrientationVertical);
    view_dispatcher_switch_to_view(ac_remote->view_dispatcher, AC_RemoteAppViewStack);
}

void ac_remote_send_settings(const ACRemoteAppSettings* settings) {
    furi_assert(settings);

    HvacMideaPacket settings_packet = hvac_midea_create_packet(HvacMideaPacketSettings);
    if(!settings->power) {
        hvac_midea_set_power_off(settings_packet);
        goto send;
    }

    hvac_midea_set_temperature(settings_packet, settings->temperature);
    hvac_midea_set_fan_power(settings_packet, settings->fan);
    hvac_midea_set_mode(settings_packet, settings->mode);

send:
    hvac_midea_send(settings_packet);
    hvac_midea_free_packet(settings_packet);
}

void ac_remote_send_command(const ACRemoteAppSettings* settings, command_id command) {
    furi_assert(settings);

    HvacMideaPacket command_packet;
    if(command == command_swing) {
        // swing command sent in settings packet
        command_packet = hvac_midea_create_packet(HvacMideaPacketSettings);
        hvac_midea_set_toggle_swing(command_packet);
        goto send;
    }

    command_packet = hvac_midea_create_packet(HvacMideaPacketCommand);
    switch(command) {
    case command_swing_vertical:
        hvac_midea_set_command(command_packet, HvacMideaCommandVerticalSwing);
        break;
    case command_turbo:
        hvac_midea_set_command(command_packet, HvacMideaCommandTurbo);
        break;
    case command_led:
        hvac_midea_set_command(command_packet, HvacMideaCommandLED);
        break;
    case command_led_change_info:
        hvac_midea_set_command(command_packet, HvacMideaCommandLEDChangeInfo);
        break;
    case command_clean:
        hvac_midea_set_command(command_packet, HvacMideaCommandClean);
        break;
    case command_silent:
        hvac_midea_set_command(command_packet, HvacMideaCommandSilent);
        break;
    default:
        furi_assert(false);
        break;
    }

send:
    hvac_midea_send(command_packet);
    hvac_midea_free_packet(command_packet);
}

bool ac_remote_scene_midea_on_event(void* context, SceneManagerEvent event) {
    AC_RemoteApp* ac_remote = context;
    SceneManager* scene_manager = ac_remote->scene_manager;
    ACRemotePanel* ac_remote_panel = ac_remote->ac_remote_panel;
    UNUSED(scene_manager);
    if(event.type != SceneManagerEventTypeCustom) {
        return false;
    }

    uint16_t event_type;
    int16_t event_value;
    ac_remote_custom_event_unpack(event.event, &event_type, &event_value);
    if(event_type == AC_RemoteCustomEventTypeSendSettings) {
        NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
        notification_message(notifications, &sequence_blink_white_100);
        ac_remote_send_settings(&ac_remote->app_state);
        notification_message(notifications, &sequence_blink_stop);
        return true;
    }

    if(event_type == AC_RemoteCustomEventTypeSendCommand) {
        NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
        notification_message(notifications, &sequence_blink_white_100);
        ac_remote_send_command(&ac_remote->app_state, event_value);
        notification_message(notifications, &sequence_blink_stop);
        return true;
    }

    if(event_type == AC_RemoteCustomEventTypeButtonLongPressed) {
        switch(event_value) {
        case button_swing:
            // ignore when power off
            if(!ac_remote->app_state.power) {
                break;
            }

            view_dispatcher_send_custom_event(
                ac_remote->view_dispatcher,
                ac_remote_custom_event_pack(
                    AC_RemoteCustomEventTypeSendCommand, command_swing_vertical));
            break;
        case button_led:
            // ignore when power off
            if(!ac_remote->app_state.power) {
                break;
            }

            view_dispatcher_send_custom_event(
                ac_remote->view_dispatcher,
                ac_remote_custom_event_pack(
                    AC_RemoteCustomEventTypeSendCommand, command_led_change_info));
            break;
        case button_fan:
            // ignore when power off
            if(!ac_remote->app_state.power) {
                break;
            }

            // toggle silent mode
            ac_remote->app_state.silent_mode = ac_remote->app_state.silent_mode ? 0 : 1;
            if(ac_remote->app_state.silent_mode) {
                ac_remote_panel_item_set_icons(
                    ac_remote_panel, button_fan, &I_fan_silent_19x20, &I_fan_silent_hover_19x20);

                view_dispatcher_send_custom_event(
                    ac_remote->view_dispatcher,
                    ac_remote_custom_event_pack(
                        AC_RemoteCustomEventTypeSendCommand, command_silent));

                break;
            }

            // if silent mode turned off, resend settings again
            ac_remote_panel_item_set_icons(
                ac_remote_panel,
                button_fan,
                fan[ac_remote_displayed_fan_power(&ac_remote->app_state)][0],
                fan[ac_remote_displayed_fan_power(&ac_remote->app_state)][1]);

            view_dispatcher_send_custom_event(
                ac_remote->view_dispatcher,
                ac_remote_custom_event_pack(AC_RemoteCustomEventTypeSendSettings, 0));
            break;
        default:
            break;
        }

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
        // reset possible "silent"
        ac_remote->app_state.silent_mode = 0;
        ac_remote_panel_item_set_icons(
            ac_remote_panel,
            button_fan,
            fan[ac_remote_displayed_fan_power(&ac_remote->app_state)][0],
            fan[ac_remote_displayed_fan_power(&ac_remote->app_state)][1]);
        break;
    case button_mode:
        ac_remote->app_state.mode++;
        if(ac_remote->app_state.mode > HvacMideaModeAuto) {
            ac_remote->app_state.mode = HvacMideaModeCold;
        }
        ac_remote->app_state.silent_mode = 0;
        ac_remote_panel_item_set_icons(
            ac_remote_panel,
            button_mode,
            mode[ac_remote->app_state.mode][0],
            mode[ac_remote->app_state.mode][1]);

        // for "auto" and "dry" mode set displayed fan power to "auto"
        ac_remote_panel_item_set_icons(
            ac_remote_panel,
            button_fan,
            fan[ac_remote_displayed_fan_power(&ac_remote->app_state)][0],
            fan[ac_remote_displayed_fan_power(&ac_remote->app_state)][1]);

        // for "fan" mode lock temperature to blank
        ac_remote_displayed_temperature(&ac_remote->app_state, buffer, sizeof(buffer));
        ac_remote_panel_label_set_string(ac_remote_panel, label_temperature, buffer);

        if(!ac_remote->app_state.power) {
            return true;
        }

        break;
    case button_fan:
        // do not adjust if in "auto" or "dry" mode
        if(ac_remote->app_state.mode == HvacMideaModeAuto ||
           ac_remote->app_state.mode == HvacMideaModeDry) {
            return true;
        }

        ac_remote->app_state.fan++;
        if(ac_remote->app_state.fan > HvacMideaFanPowerAuto) {
            ac_remote->app_state.fan = HvacMideaModeCold;
        }

        ac_remote->app_state.silent_mode = 0; // force reset silent mode
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
        // do not adjust in "fan" mode
        if(ac_remote->app_state.mode == HvacMideaModeFan) {
            return true;
        }

        if(ac_remote->app_state.temperature < HVAC_MIDEA_TEMPERATURE_MAX) {
            ac_remote->app_state.temperature++;
            snprintf(buffer, sizeof(buffer), "%ld", ac_remote->app_state.temperature);
            ac_remote_panel_label_set_string(ac_remote_panel, label_temperature, buffer);
        }

        if(!ac_remote->app_state.power) {
            return true;
        }

        break;
    case button_temp_down:
        // do not adjust in "fan" mode
        if(ac_remote->app_state.mode == HvacMideaModeFan) {
            return true;
        }

        if(ac_remote->app_state.temperature > HVAC_MIDEA_TEMPERATURE_MIN) {
            ac_remote->app_state.temperature--;
            snprintf(buffer, sizeof(buffer), "%ld", ac_remote->app_state.temperature);
            ac_remote_panel_label_set_string(ac_remote_panel, label_temperature, buffer);
        }

        if(!ac_remote->app_state.power) {
            return true;
        }

        break;
    case button_swing:
        // ignore when power off
        if(!ac_remote->app_state.power) {
            return true;
        }

        view_dispatcher_send_custom_event(
            ac_remote->view_dispatcher,
            ac_remote_custom_event_pack(AC_RemoteCustomEventTypeSendCommand, command_swing));

        return true;
    case button_turbo:
        // ignore when power off
        if(!ac_remote->app_state.power) {
            return true;
        }

        // reset possible "silent"
        ac_remote->app_state.silent_mode = 0;
        ac_remote_panel_item_set_icons(
            ac_remote_panel,
            button_fan,
            fan[ac_remote_displayed_fan_power(&ac_remote->app_state)][0],
            fan[ac_remote_displayed_fan_power(&ac_remote->app_state)][1]);

        view_dispatcher_send_custom_event(
            ac_remote->view_dispatcher,
            ac_remote_custom_event_pack(AC_RemoteCustomEventTypeSendCommand, command_turbo));

        return true;
    case button_led:
        // ignore when power off
        if(!ac_remote->app_state.power) {
            return true;
        }

        view_dispatcher_send_custom_event(
            ac_remote->view_dispatcher,
            ac_remote_custom_event_pack(AC_RemoteCustomEventTypeSendCommand, command_led));

        return true;
    case button_clean:
        // ignore when power off
        if(!ac_remote->app_state.power) {
            return true;
        }

        // reset possible "silent"
        ac_remote->app_state.silent_mode = 0;
        ac_remote_panel_item_set_icons(
            ac_remote_panel,
            button_fan,
            fan[ac_remote_displayed_fan_power(&ac_remote->app_state)][0],
            fan[ac_remote_displayed_fan_power(&ac_remote->app_state)][1]);

        // mimic original behaviour when remote tracks power off
        ac_remote->app_state.power = 0;
        ac_remote_panel_item_set_icons(
            ac_remote_panel,
            button_power,
            power[ac_remote->app_state.power][0],
            power[ac_remote->app_state.power][1]);

        view_dispatcher_send_custom_event(
            ac_remote->view_dispatcher,
            ac_remote_custom_event_pack(AC_RemoteCustomEventTypeSendCommand, command_clean));

        return true;
    default:
        break;
    }

    view_dispatcher_send_custom_event(
        ac_remote->view_dispatcher,
        ac_remote_custom_event_pack(AC_RemoteCustomEventTypeSendSettings, 0));
    return true;
}

void ac_remote_scene_midea_on_exit(void* context) {
    AC_RemoteApp* ac_remote = context;
    ACRemotePanel* ac_remote_panel = ac_remote->ac_remote_panel;
    ac_remote_store_settings(&ac_remote->app_state);
    view_stack_remove_view(ac_remote->view_stack, ac_remote_panel_get_view(ac_remote_panel));
    ac_remote_panel_reset(ac_remote_panel);
}
