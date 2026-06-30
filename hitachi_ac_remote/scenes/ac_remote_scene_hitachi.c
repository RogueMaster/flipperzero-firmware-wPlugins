#include "../ac_remote_app_i.h"

#include <inttypes.h>

#define TAG "ACRemoteHitachi"

typedef enum {
    button_power,
    button_mode,
    button_temp_up,
    button_fan,
    button_temp_down,
    button_vane,
    button_view_sub,
    label_temperature,

    button_timer_on_h_inc,
    button_timer_on_m_inc,
    button_timer_off_h_inc,
    button_timer_off_m_inc,
    label_timer_on_h,
    label_timer_on_m,
    label_timer_off_h,
    label_timer_off_m,
    button_timer_on_h_dec,
    button_timer_on_m_dec,
    button_timer_off_h_dec,
    button_timer_off_m_dec,
    button_timer_set,
    button_timer_reset,
    button_reset_filter,
    button_settings,
    button_view_main,
} button_id;

static const Icon* POWER_ICONS[POWER_STATE_MAX][2] = {
    [PowerButtonOff] = {&I_on_19x20, &I_on_hover_19x20},
    [PowerButtonOn] = {&I_off_19x20, &I_off_hover_19x20},
};

static const Icon* MODE_BUTTON_ICONS[MODE_BUTTON_STATE_MAX][2] = {
    [ModeButtonAuto] = {&I_auto_19x20, &I_auto_hover_19x20},
    [ModeButtonHeating] = {&I_heat_19x20, &I_heat_hover_19x20},
    [ModeButtonCooling] = {&I_cold_19x20, &I_cold_hover_19x20},
    [ModeButtonDehumidifying] = {&I_dry_19x20, &I_dry_hover_19x20},
    [ModeButtonFan] = {&I_fan_speed_4_19x20, &I_fan_speed_4_hover_19x20},
};

static const Icon* FAN_SPEED_BUTTON_ICONS[FAN_SPEED_BUTTON_STATE_MAX][2] = {
    [FanSpeedButtonLow] = {&I_fan_speed_2_19x20, &I_fan_speed_2_hover_19x20},
    [FanSpeedButtonMedium] = {&I_fan_speed_3_19x20, &I_fan_speed_3_hover_19x20},
    [FanSpeedButtonHigh] = {&I_fan_speed_4_19x20, &I_fan_speed_4_hover_19x20},
};

static const Icon* VANE_BUTTON_ICONS[VANE_BUTTON_STATE_MAX][2] = {
    [VaneButtonPos0] = {&I_vane_0_19x20, &I_vane_0_hover_19x20},
    [VaneButtonPos1] = {&I_vane_1_19x20, &I_vane_1_hover_19x20},
    [VaneButtonPos2] = {&I_vane_2_19x20, &I_vane_2_hover_19x20},
    [VaneButtonPos3] = {&I_vane_3_19x20, &I_vane_3_hover_19x20},
    [VaneButtonPos4] = {&I_vane_4_19x20, &I_vane_4_hover_19x20},
    [VaneButtonPos5] = {&I_vane_5_19x20, &I_vane_5_hover_19x20},
    [VaneButtonPos6] = {&I_vane_6_19x20, &I_vane_6_hover_19x20},
    [VaneButtonAuto] = {&I_vane_auto_move_19x20, &I_vane_auto_move_hover_19x20},
};

static const Icon* TIMER_SET_BUTTON_ICONS[TIMER_STATE_COUNT][2] = {
    [TimerStateStopped] = {&I_timer_set_19x20, &I_timer_set_hover_19x20},
    [TimerStatePaused] = {&I_timer_resume_19x20, &I_timer_resume_hover_19x20},
    [TimerStateRunning] = {&I_timer_pause_19x20, &I_timer_pause_hover_19x20},
};

static const HvacHitachiControl POWER_LUT[POWER_STATE_MAX] = {
    [PowerButtonOff] = HvacHitachiControlPowerOff,
    [PowerButtonOn] = HvacHitachiControlPowerOn,
};

static const HvacHitachiMode MODE_LUT[MODE_BUTTON_STATE_MAX] = {
    [ModeButtonAuto] = HvacHitachiModeAuto,
    [ModeButtonHeating] = HvacHitachiModeHeating,
    [ModeButtonCooling] = HvacHitachiModeCooling,
    [ModeButtonDehumidifying] = HvacHitachiModeDehumidifying,
    [ModeButtonFan] = HvacHitachiModeFan,
};

static const HvacHitachiFanSpeed FAN_SPEED_LUT[FAN_SPEED_BUTTON_STATE_MAX] = {
    [FanSpeedButtonLow] = HvacHitachiFanSpeedLow,
    [FanSpeedButtonMedium] = HvacHitachiFanSpeedMedium,
    [FanSpeedButtonHigh] = HvacHitachiFanSpeedHigh,
};

static const HvacHitachiVane VANE_LUT[VANE_BUTTON_STATE_MAX] = {
    [VaneButtonPos0] = HvacHitachiVanePos0,
    [VaneButtonPos1] = HvacHitachiVanePos1,
    [VaneButtonPos2] = HvacHitachiVanePos2,
    [VaneButtonPos3] = HvacHitachiVanePos3,
    [VaneButtonPos4] = HvacHitachiVanePos4,
    [VaneButtonPos5] = HvacHitachiVanePos5,
    [VaneButtonPos6] = HvacHitachiVanePos6,
    [VaneButtonAuto] = HvacHitachiVaneAuto,
};

static const HvacHitachiSide SIDE_LUT[SETTINGS_SIDE_COUNT] = {
    [SettingsSideA] = HvacHitachiSideA,
    [SettingsSideB] = HvacHitachiSideB,
};

static uint8_t TIMER_STEP_LUT[SETTINGS_TIMER_STEP_COUNT] = {
    [SettingsTimerStep1min] = 1,
    [SettingsTimerStep2min] = 2,
    [SettingsTimerStep3min] = 3,
    [SettingsTimerStep5min] = 5,
    [SettingsTimerStep10min] = 10,
    [SettingsTimerStep15min] = 15,
    [SettingsTimerStep30min] = 30,
};

// Exactly 4095 minutes
#define TIMER_MAX_TIME_H (68u)
#define TIMER_MAX_TIME_M (15u)

#define TIMER_MAX_MIN (59u)

#define BUTTON_TIMER_X (4)
#define BUTTON_TIMER_Y (2)

typedef struct {
    const uint16_t x;
    const uint16_t y;
    const button_id button;
    const Icon* icon;
    const Icon* icon_hover;
} TimerButtonLayout;

static const TimerButtonLayout BUTTON_TIMER_LAYOUT[BUTTON_TIMER_Y][BUTTON_TIMER_X] = {
    [0][0] =
        {
            .x = 2,
            .y = 12,
            .button = button_timer_on_h_inc,
            .icon = &I_timer_inc_15x9,
            .icon_hover = &I_timer_inc_hover_15x9,
        },
    [0][1] =
        {
            .x = 16,
            .y = 12,
            .button = button_timer_on_m_inc,
            .icon = &I_timer_inc_15x9,
            .icon_hover = &I_timer_inc_hover_15x9,
        },
    [0][2] =
        {
            .x = 33,
            .y = 12,
            .button = button_timer_off_h_inc,
            .icon = &I_timer_inc_15x9,
            .icon_hover = &I_timer_inc_hover_15x9,
        },
    [0][3] =
        {
            .x = 47,
            .y = 12,
            .button = button_timer_off_m_inc,
            .icon = &I_timer_inc_15x9,
            .icon_hover = &I_timer_inc_hover_15x9,
        },
    [1][0] =
        {
            .x = 2,
            .y = 30,
            .button = button_timer_on_h_dec,
            .icon = &I_timer_dec_15x10,
            .icon_hover = &I_timer_dec_hover_15x10,
        },
    [1][1] =
        {
            .x = 16,
            .y = 30,
            .button = button_timer_on_m_dec,
            .icon = &I_timer_dec_15x10,
            .icon_hover = &I_timer_dec_hover_15x10,
        },
    [1][2] =
        {
            .x = 33,
            .y = 30,
            .button = button_timer_off_h_dec,
            .icon = &I_timer_dec_15x10,
            .icon_hover = &I_timer_dec_hover_15x10,
        },
    [1][3] =
        {
            .x = 47,
            .y = 30,
            .button = button_timer_off_m_dec,
            .icon = &I_timer_dec_15x10,
            .icon_hover = &I_timer_dec_hover_15x10,
        },
};

static inline void timer_set_minute_nocheck(ACRemoteTimerState* timer, uint16_t timer_minutes) {
    timer->minutes = timer_minutes % 60;
    timer->hours = timer_minutes / 60;
    timer->minutes_only = timer_minutes;
    snprintf(timer->minutes_display, sizeof(timer->minutes_display), "%02" PRIu8, timer->minutes);
    snprintf(timer->hours_display, sizeof(timer->hours_display), "%02" PRIu8, timer->hours);
}

static bool timer_update_from_minutes(ACRemoteTimerState* timer, uint16_t timer_minutes) {
    furi_assert(timer);
    if(timer_minutes > 0xfff) {
        timer_minutes = 0xfff;
    }
    if(timer->minutes_only != timer_minutes) {
        timer_set_minute_nocheck(timer, timer_minutes);
        return true;
    }
    return false;
}

static void timer_set_from_minutes(ACRemoteTimerState* timer, uint16_t timer_minutes) {
    furi_assert(timer);
    if(timer_minutes > 0xfff) {
        timer_minutes = 0xfff;
    }
    timer_set_minute_nocheck(timer, timer_minutes);
}

static bool timer_update_from_timestamp_diff(
    ACRemoteTimerState* timer,
    uint32_t target_timestamp,
    uint32_t ts) {
    uint32_t diff_mins = 0;

    if(ts <= target_timestamp) {
        uint32_t diff_seconds = target_timestamp - ts;
        diff_mins = diff_seconds / 60;
        if(diff_seconds % 60 != 0) {
            diff_mins++;
        }
    }
    if(timer->minutes_only != diff_mins) {
        timer_set_minute_nocheck(timer, diff_mins);
        return true;
    }
    return false;
}

static void timer_set_from_timestamp_diff(
    ACRemoteTimerState* timer,
    uint32_t target_timestamp,
    uint32_t ts) {
    uint32_t diff_mins = 0;

    if(ts <= target_timestamp) {
        uint32_t diff_seconds = target_timestamp - ts;
        diff_mins = diff_seconds / 60;
        if(diff_seconds % 60 != 0) {
            diff_mins++;
        }
    }
    timer_set_minute_nocheck(timer, diff_mins);
}

static void timer_inc_hour(ACRemoteTimerState* timer) {
    uint16_t new_minutes_only = timer->minutes_only;
    if(new_minutes_only + 60 > 0xfff) {
        new_minutes_only = 0xfff;
    } else {
        new_minutes_only += 60;
    }
    timer_update_from_minutes(timer, new_minutes_only);
}

static void timer_dec_hour(ACRemoteTimerState* timer) {
    uint16_t new_minutes_only = timer->minutes_only;
    if(new_minutes_only < 60) {
        new_minutes_only = 0;
    } else {
        new_minutes_only -= 60;
    }
    timer_update_from_minutes(timer, new_minutes_only);
}

static void timer_inc_minute(ACRemoteTimerState* timer, uint8_t unit) {
    uint16_t new_minutes_only = timer->minutes_only;
    if(new_minutes_only + unit > 0xfff) {
        new_minutes_only = 0xfff;
    } else {
        new_minutes_only += unit;
    }
    timer_update_from_minutes(timer, new_minutes_only);
}

static void timer_dec_minute(ACRemoteTimerState* timer, uint8_t unit) {
    uint16_t new_minutes_only = timer->minutes_only;
    if(new_minutes_only < unit) {
        new_minutes_only = 0;
    } else {
        new_minutes_only -= unit;
    }
    timer_update_from_minutes(timer, new_minutes_only);
}

static TimerOnOffState* ac_remote_timer_selector(AC_RemoteApp* app) {
    furi_assert(app);
    switch(app->app_state.timer_state) {
    case TimerStateStopped:
        return &app->app_state.timer_preset;
    case TimerStatePaused:
        return &app->app_state.timer_pause;
    default:
        return NULL;
    }
}

static void ac_remote_timer_tick(AC_RemoteApp* ac_remote, bool init) {
    uint32_t ts = furi_hal_rtc_get_timestamp();

    bool timer_on_changed, timer_off_changed;

    if(!init) {
        timer_on_changed = timer_update_from_timestamp_diff(
            &ac_remote->transient_state.timer_on, ac_remote->app_state.timer_on_expires_at, ts);
        timer_off_changed = timer_update_from_timestamp_diff(
            &ac_remote->transient_state.timer_off, ac_remote->app_state.timer_off_expires_at, ts);
    } else {
        timer_set_from_timestamp_diff(
            &ac_remote->transient_state.timer_on, ac_remote->app_state.timer_on_expires_at, ts);
        timer_set_from_timestamp_diff(
            &ac_remote->transient_state.timer_off, ac_remote->app_state.timer_off_expires_at, ts);
        timer_on_changed = true;
        timer_off_changed = true;
    }

    bool refresh = timer_on_changed || timer_off_changed;
    bool timer_on_just_expired = timer_on_changed &&
                                 ac_remote->transient_state.timer_on.minutes_only == 0;
    bool timer_off_just_expired = timer_off_changed &&
                                  ac_remote->transient_state.timer_off.minutes_only == 0;
    if(refresh) {
        ac_remote->app_state.timer_pause.on = ac_remote->transient_state.timer_on.minutes_only;
        ac_remote->app_state.timer_pause.off = ac_remote->transient_state.timer_off.minutes_only;
        FURI_LOG_I(
            TAG,
            "ac_remote->app_state.timer_pause = {on: %" PRIu32 ", off: %" PRIu32 "}",
            ac_remote->app_state.timer_pause.on,
            ac_remote->app_state.timer_pause.off);
        ac_remote_panel_update_view(ac_remote->panel_sub);
    }
    if(timer_on_just_expired || timer_off_just_expired) {
        FURI_LOG_I(
            TAG,
            "expiring timers:%s%s",
            timer_on_just_expired ? " on" : "",
            timer_off_just_expired ? " off" : "");
        // power state should be corresponding to the timer that expires last by timestamp when
        // both are detected to expire simultaneously (offline tick)
        if(timer_on_just_expired && timer_off_just_expired) {
            if(ac_remote->app_state.timer_on_expires_at >
               ac_remote->app_state.timer_off_expires_at) {
                ac_remote->app_state.power = PowerButtonOn;
            } else {
                ac_remote->app_state.power = PowerButtonOff;
            }
        } else if(timer_on_just_expired) {
            ac_remote->app_state.power = PowerButtonOn;
        } else if(timer_off_just_expired) {
            ac_remote->app_state.power = PowerButtonOff;
        }
        ac_remote_panel_item_set_icons(
            ac_remote->panel_main,
            button_power,
            POWER_ICONS[ac_remote->app_state.power][0],
            POWER_ICONS[ac_remote->app_state.power][1]);
    }
    if(ac_remote->transient_state.timer_on.minutes_only == 0 &&
       ac_remote->transient_state.timer_off.minutes_only == 0) {
        FURI_LOG_I(TAG, "Both timers expired. Pausing...");
        ac_remote->app_state.timer_state = TimerStatePaused;
        ac_remote_panel_item_set_icons(
            ac_remote->panel_sub,
            button_timer_set,
            TIMER_SET_BUTTON_ICONS[ac_remote->app_state.timer_state][0],
            TIMER_SET_BUTTON_ICONS[ac_remote->app_state.timer_state][1]);
    }
}

void ac_remote_scene_universal_common_item_callback(
    void* context,
    InputType event_type,
    uint32_t index) {
    AC_RemoteApp* ac_remote = context;
    if(event_type == InputTypeShort) {
        uint32_t event =
            ac_remote_custom_event_pack(AC_RemoteCustomEventTypeButtonSelected, index);
        view_dispatcher_send_custom_event(ac_remote->view_dispatcher, event);
    } else if(event_type == InputTypeLong) {
        uint32_t event =
            ac_remote_custom_event_pack(AC_RemoteCustomEventTypeButtonLongPress, index);
        view_dispatcher_send_custom_event(ac_remote->view_dispatcher, event);
    }
}

void ac_remote_scene_hitachi_on_enter(void* context) {
    FURI_LOG_I(TAG, "Entering scene...");

    AC_RemoteApp* ac_remote = context;
    ACRemotePanel* panel_main = ac_remote->panel_main;
    ACRemotePanel* panel_sub = ac_remote->panel_sub;
    ac_remote->protocol = hvac_hitachi_init();

    ac_remote_panel_reserve(panel_main, 2, 4);

    ac_remote_panel_add_item(
        panel_main,
        button_power,
        0,
        0,
        6,
        17,
        POWER_ICONS[ac_remote->app_state.power][0],
        POWER_ICONS[ac_remote->app_state.power][1],
        ac_remote_scene_universal_common_item_callback,
        context);
    ac_remote_panel_add_icon(panel_main, 5, 39, &I_power_text_21x5);
    ac_remote_panel_add_item(
        panel_main,
        button_mode,
        1,
        0,
        39,
        17,
        MODE_BUTTON_ICONS[ac_remote->app_state.mode][0],
        MODE_BUTTON_ICONS[ac_remote->app_state.mode][1],
        ac_remote_scene_universal_common_item_callback,
        context);
    ac_remote_panel_add_icon(panel_main, 40, 39, &I_mode_text_17x5);
    ac_remote_panel_add_icon(panel_main, 0, 59, &I_frame_30x39);
    ac_remote_panel_add_item(
        panel_main,
        button_temp_up,
        0,
        1,
        3,
        47,
        &I_tempup_24x21,
        &I_tempup_hover_24x21,
        ac_remote_scene_universal_common_item_callback,
        context);
    ac_remote_panel_add_item(
        panel_main,
        button_temp_down,
        0,
        2,
        3,
        89,
        &I_tempdown_24x21,
        &I_tempdown_hover_24x21,
        ac_remote_scene_universal_common_item_callback,
        context);
    ac_remote_panel_add_item(
        panel_main,
        button_fan,
        1,
        1,
        39,
        50,
        FAN_SPEED_BUTTON_ICONS[ac_remote->app_state.fan][0],
        FAN_SPEED_BUTTON_ICONS[ac_remote->app_state.fan][1],
        ac_remote_scene_universal_common_item_callback,
        context);
    ac_remote_panel_add_icon(panel_main, 43, 72, &I_fan_text_11x5);
    ac_remote_panel_add_item(
        panel_main,
        button_vane,
        1,
        2,
        39,
        83,
        VANE_BUTTON_ICONS[ac_remote->app_state.vane][0],
        VANE_BUTTON_ICONS[ac_remote->app_state.vane][1],
        ac_remote_scene_universal_common_item_callback,
        context);
    ac_remote_panel_add_icon(panel_main, 37, 105, &I_louver_text_23x5);
    ac_remote_panel_add_item(
        panel_main,
        button_view_sub,
        0,
        3,
        6,
        116,
        &I_timer_52x10,
        &I_timer_hover_52x10,
        ac_remote_scene_universal_common_item_callback,
        context);

    ac_remote_panel_add_label(panel_main, 0, 6, 11, FontPrimary, "AC remote");

    snprintf(
        ac_remote->transient_state.temp_display,
        sizeof(ac_remote->transient_state.temp_display),
        "%" PRIu32,
        ac_remote->app_state.temperature);
    ac_remote_panel_add_label(
        panel_main,
        label_temperature,
        4,
        82,
        FontKeyboard,
        ac_remote->transient_state.temp_display);

    view_set_orientation(ac_remote_panel_get_view(panel_main), ViewOrientationVertical);

    ac_remote_panel_reserve(panel_sub, 4, 5);

    // Timer control
    ac_remote_panel_add_icon(panel_sub, 0, 4, &I_timer_frame_64x73);
    for(uint16_t yy = 0; yy < BUTTON_TIMER_Y; yy++) {
        for(uint16_t xx = 0; xx < BUTTON_TIMER_X; xx++) {
            const TimerButtonLayout* layout = &BUTTON_TIMER_LAYOUT[yy][xx];
            ac_remote_panel_add_item(
                panel_sub,
                layout->button,
                xx,
                yy,
                layout->x,
                layout->y,
                layout->icon,
                layout->icon_hover,
                ac_remote_scene_universal_common_item_callback,
                context);
        }
    }

    if(ac_remote->app_state.timer_state == TimerStateRunning) {
        ac_remote_timer_tick(ac_remote, true);
    } else {
        const TimerOnOffState* timer_state = ac_remote_timer_selector(ac_remote);
        if(timer_state != NULL) {
            timer_set_from_minutes(&ac_remote->transient_state.timer_on, timer_state->on);
            timer_set_from_minutes(&ac_remote->transient_state.timer_off, timer_state->off);
        } else {
            timer_set_from_minutes(&ac_remote->transient_state.timer_on, 0);
            timer_set_from_minutes(&ac_remote->transient_state.timer_off, 0);
        }
    }

    ac_remote_panel_add_label(
        panel_sub,
        label_timer_on_h,
        4,
        29,
        FontKeyboard,
        ac_remote->transient_state.timer_on.hours_display);
    ac_remote_panel_add_label(
        panel_sub,
        label_timer_on_m,
        18,
        29,
        FontKeyboard,
        ac_remote->transient_state.timer_on.minutes_display);
    ac_remote_panel_add_label(
        panel_sub,
        label_timer_off_h,
        35,
        29,
        FontKeyboard,
        ac_remote->transient_state.timer_off.hours_display);
    ac_remote_panel_add_label(
        panel_sub,
        label_timer_off_m,
        49,
        29,
        FontKeyboard,
        ac_remote->transient_state.timer_off.minutes_display);

    ac_remote_panel_add_item(
        panel_sub,
        button_timer_set,
        0,
        2,
        7,
        50,
        TIMER_SET_BUTTON_ICONS[ac_remote->app_state.timer_state][0],
        TIMER_SET_BUTTON_ICONS[ac_remote->app_state.timer_state][1],
        ac_remote_scene_universal_common_item_callback,
        context);
    ac_remote_panel_add_item(
        panel_sub,
        button_timer_reset,
        1,
        2,
        38,
        50,
        &I_timer_reset_19x20,
        &I_timer_reset_hover_19x20,
        ac_remote_scene_universal_common_item_callback,
        context);

    // Misc buttons
    ac_remote_panel_add_item(
        panel_sub,
        button_reset_filter,
        0,
        3,
        7,
        82,
        &I_reset_filter_19x20,
        &I_reset_filter_hover_19x20,
        ac_remote_scene_universal_common_item_callback,
        context);
    ac_remote_panel_add_icon(panel_sub, 5, 105, &I_reset_filter_text_23x5);

    ac_remote_panel_add_item(
        panel_sub,
        button_settings,
        1,
        3,
        38,
        82,
        &I_settings_19x20,
        &I_settings_hover_19x20,
        ac_remote_scene_universal_common_item_callback,
        context);
    ac_remote_panel_add_icon(panel_sub, 36, 105, &I_settings_text_23x5);

    // Back button
    ac_remote_panel_add_item(
        panel_sub,
        button_view_main,
        0,
        4,
        6,
        116,
        &I_back_52x10,
        &I_back_hover_52x10,
        ac_remote_scene_universal_common_item_callback,
        context);

    view_set_orientation(ac_remote_panel_get_view(panel_sub), ViewOrientationVertical);

    view_dispatcher_switch_to_view(ac_remote->view_dispatcher, AC_RemoteAppViewMain);
}

bool ac_remote_scene_hitachi_on_event(void* context, SceneManagerEvent event) {
    AC_RemoteApp* ac_remote = context;
    ACRemoteAppSettings* app_state = &ac_remote->app_state;
    SceneManager* scene_manager = ac_remote->scene_manager;
    ACRemotePanel* panel_main = ac_remote->panel_main;
    ACRemotePanel* panel_sub = ac_remote->panel_sub;
    UNUSED(scene_manager);
    bool consumed = false;
    if(event.type == SceneManagerEventTypeTick) {
        if(ac_remote->app_state.timer_state == TimerStateRunning) {
            ac_remote_timer_tick(ac_remote, false);
        }
    } else if(event.type == SceneManagerEventTypeCustom) {
        uint16_t event_type;
        int16_t event_value;
        uint32_t next_event;
        ac_remote_custom_event_unpack(event.event, &event_type, &event_value);
        if(event_type == AC_RemoteCustomEventTypeSendCommand) {
            NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
            notification_message(notifications, &sequence_blink_magenta_100);
            hvac_hitachi_switch_side(ac_remote->protocol, SIDE_LUT[ac_remote->app_state.side]);
            hvac_hitachi_build_samples(ac_remote->protocol);
            hvac_hitachi_send(ac_remote->protocol);
            notification_message(notifications, &sequence_blink_stop);
        } else if(event_type == AC_RemoteCustomEventTypeCallSettings) {
            scene_manager_next_scene(ac_remote->scene_manager, AC_RemoteSceneSettings);
        } else if(event_type == AC_RemoteCustomEventTypeSwitchPanel) {
            if(event_value == AC_RemoteAppViewMain || event_value == AC_RemoteAppViewSub) {
                view_dispatcher_switch_to_view(ac_remote->view_dispatcher, event_value);
            }
        } else if(event_type == AC_RemoteCustomEventTypeButtonSelected) {
            bool send_on_power_off = false, has_ir_code = true;
            hvac_hitachi_reset(ac_remote->protocol);
            switch(event_value) {
            case button_power:
                app_state->power++;
                if(app_state->power >= POWER_STATE_MAX) {
                    app_state->power = PowerButtonOff;
                }
                hvac_hitachi_set_power(
                    ac_remote->protocol,
                    app_state->temperature,
                    FAN_SPEED_LUT[app_state->fan],
                    MODE_LUT[app_state->mode],
                    VANE_LUT[app_state->vane],
                    POWER_LUT[app_state->power]);
                ac_remote_panel_item_set_icons(
                    panel_main,
                    button_power,
                    POWER_ICONS[app_state->power][0],
                    POWER_ICONS[app_state->power][1]);
                send_on_power_off = true;
                break;
            case button_mode:
                app_state->mode++;
                if(app_state->mode >= MODE_BUTTON_STATE_MAX) {
                    app_state->mode = app_state->allow_auto ? ModeButtonAuto : ModeButtonHeating;
                }
                hvac_hitachi_set_mode(
                    ac_remote->protocol,
                    app_state->temperature,
                    FAN_SPEED_LUT[app_state->fan],
                    MODE_LUT[app_state->mode],
                    VANE_LUT[app_state->vane]);
                ac_remote_panel_item_set_icons(
                    panel_main,
                    button_mode,
                    MODE_BUTTON_ICONS[app_state->mode][0],
                    MODE_BUTTON_ICONS[app_state->mode][1]);
                break;
            case button_fan:
                app_state->fan++;
                if(app_state->fan >= FAN_SPEED_BUTTON_STATE_MAX) {
                    app_state->fan = FanSpeedButtonLow;
                }
                hvac_hitachi_set_fan_speed_mode(
                    ac_remote->protocol, FAN_SPEED_LUT[app_state->fan], MODE_LUT[app_state->mode]);
                ac_remote_panel_item_set_icons(
                    panel_main,
                    button_fan,
                    FAN_SPEED_BUTTON_ICONS[app_state->fan][0],
                    FAN_SPEED_BUTTON_ICONS[app_state->fan][1]);
                break;
            case button_vane:
                app_state->vane++;
                if(app_state->vane >= VANE_BUTTON_STATE_MAX) {
                    app_state->vane = VaneButtonPos0;
                }
                hvac_hitachi_set_vane(ac_remote->protocol, VANE_LUT[app_state->vane]);
                ac_remote_panel_item_set_icons(
                    panel_main,
                    button_vane,
                    VANE_BUTTON_ICONS[app_state->vane][0],
                    VANE_BUTTON_ICONS[app_state->vane][1]);
                break;
            case button_temp_up:
                if(app_state->temperature < HVAC_HITACHI_TEMPERATURE_MAX) {
                    app_state->temperature++;
                }
                hvac_hitachi_set_temperature(ac_remote->protocol, app_state->temperature, true);
                snprintf(
                    ac_remote->transient_state.temp_display,
                    sizeof(ac_remote->transient_state.temp_display),
                    "%" PRIu32,
                    app_state->temperature);
                ac_remote_panel_update_view(panel_main);
                break;
            case button_temp_down:
                if(app_state->temperature > HVAC_HITACHI_TEMPERATURE_MIN) {
                    app_state->temperature--;
                }
                hvac_hitachi_set_temperature(ac_remote->protocol, app_state->temperature, false);
                snprintf(
                    ac_remote->transient_state.temp_display,
                    sizeof(ac_remote->transient_state.temp_display),
                    "%" PRIu32,
                    app_state->temperature);
                ac_remote_panel_update_view(panel_main);
                break;
            case button_view_sub:
                next_event = ac_remote_custom_event_pack(
                    AC_RemoteCustomEventTypeSwitchPanel, AC_RemoteAppViewSub);
                view_dispatcher_send_custom_event(ac_remote->view_dispatcher, next_event);
                has_ir_code = false;
                break;
            case button_view_main:
                next_event = ac_remote_custom_event_pack(
                    AC_RemoteCustomEventTypeSwitchPanel, AC_RemoteAppViewMain);
                view_dispatcher_send_custom_event(ac_remote->view_dispatcher, next_event);
                has_ir_code = false;
                break;
            case button_timer_on_h_inc: {
                TimerOnOffState* timer = ac_remote_timer_selector(ac_remote);
                if(timer != NULL) {
                    timer_inc_hour(&ac_remote->transient_state.timer_on);
                    timer->on = ac_remote->transient_state.timer_on.minutes_only;
                    ac_remote_panel_update_view(panel_sub);
                }
                has_ir_code = false;
                break;
            }
            case button_timer_on_h_dec: {
                TimerOnOffState* timer = ac_remote_timer_selector(ac_remote);
                if(timer != NULL) {
                    timer_dec_hour(&ac_remote->transient_state.timer_on);
                    timer->on = ac_remote->transient_state.timer_on.minutes_only;
                    ac_remote_panel_update_view(panel_sub);
                }
                has_ir_code = false;
                break;
            }
            case button_timer_on_m_inc: {
                TimerOnOffState* timer = ac_remote_timer_selector(ac_remote);
                if(timer != NULL) {
                    timer_inc_minute(
                        &ac_remote->transient_state.timer_on,
                        TIMER_STEP_LUT[ac_remote->app_state.timer_step]);
                    timer->on = ac_remote->transient_state.timer_on.minutes_only;
                    ac_remote_panel_update_view(panel_sub);
                }
                has_ir_code = false;
                break;
            }
            case button_timer_on_m_dec: {
                TimerOnOffState* timer = ac_remote_timer_selector(ac_remote);
                if(timer != NULL) {
                    timer_dec_minute(
                        &ac_remote->transient_state.timer_on,
                        TIMER_STEP_LUT[ac_remote->app_state.timer_step]);
                    timer->on = ac_remote->transient_state.timer_on.minutes_only;
                    ac_remote_panel_update_view(panel_sub);
                }
                has_ir_code = false;
                break;
            }
            case button_timer_off_h_inc: {
                TimerOnOffState* timer = ac_remote_timer_selector(ac_remote);
                if(timer != NULL) {
                    timer_inc_hour(&ac_remote->transient_state.timer_off);
                    timer->off = ac_remote->transient_state.timer_off.minutes_only;
                    ac_remote_panel_update_view(panel_sub);
                }
                has_ir_code = false;
                break;
            }
            case button_timer_off_h_dec: {
                TimerOnOffState* timer = ac_remote_timer_selector(ac_remote);
                if(timer != NULL) {
                    timer_dec_hour(&ac_remote->transient_state.timer_off);
                    timer->off = ac_remote->transient_state.timer_off.minutes_only;
                    ac_remote_panel_update_view(panel_sub);
                }
                has_ir_code = false;
                break;
            }
            case button_timer_off_m_inc: {
                TimerOnOffState* timer = ac_remote_timer_selector(ac_remote);
                if(timer != NULL) {
                    timer_inc_minute(
                        &ac_remote->transient_state.timer_off,
                        TIMER_STEP_LUT[ac_remote->app_state.timer_step]);
                    timer->off = ac_remote->transient_state.timer_off.minutes_only;
                    ac_remote_panel_update_view(panel_sub);
                }
                has_ir_code = false;
                break;
            }
            case button_timer_off_m_dec: {
                TimerOnOffState* timer = ac_remote_timer_selector(ac_remote);
                if(timer != NULL) {
                    timer_dec_minute(
                        &ac_remote->transient_state.timer_off,
                        TIMER_STEP_LUT[ac_remote->app_state.timer_step]);
                    timer->off = ac_remote->transient_state.timer_off.minutes_only;
                    ac_remote_panel_update_view(panel_sub);
                }
                has_ir_code = false;
                break;
            }
            case button_timer_set:
                switch(app_state->timer_state) {
                case TimerStateRunning:
                    app_state->timer_state = TimerStatePaused;
                    hvac_hitachi_reset_timer(ac_remote->protocol);
                    break;
                case TimerStateStopped:
                    app_state->timer_pause = app_state->timer_preset;
                    // Intentional fallthrough
                case TimerStatePaused:
                default: {
                    app_state->timer_state = TimerStateRunning;
                    uint32_t current_timestamp = furi_hal_rtc_get_timestamp();
                    app_state->timer_on_expires_at =
                        current_timestamp + app_state->timer_pause.on * 60;
                    app_state->timer_off_expires_at =
                        current_timestamp + app_state->timer_pause.off * 60;
                    hvac_hitachi_set_timer(
                        ac_remote->protocol,
                        app_state->temperature,
                        app_state->timer_pause.off,
                        app_state->timer_pause.on,
                        FAN_SPEED_LUT[app_state->fan],
                        MODE_LUT[app_state->mode],
                        VANE_LUT[app_state->vane]);
                    break;
                }
                }
                ac_remote_panel_item_set_icons(
                    panel_sub,
                    button_timer_set,
                    TIMER_SET_BUTTON_ICONS[app_state->timer_state][0],
                    TIMER_SET_BUTTON_ICONS[app_state->timer_state][1]);
                send_on_power_off = true;
                break;
            case button_timer_reset:
                hvac_hitachi_reset_timer(ac_remote->protocol);
                if(app_state->timer_state != TimerStateStopped) {
                    app_state->timer_state = TimerStateStopped;
                    ac_remote_panel_item_set_icons(
                        panel_sub,
                        button_timer_set,
                        TIMER_SET_BUTTON_ICONS[app_state->timer_state][0],
                        TIMER_SET_BUTTON_ICONS[app_state->timer_state][1]);
                    const TimerOnOffState* timer_state = ac_remote_timer_selector(ac_remote);
                    if(timer_state != NULL) {
                        timer_set_from_minutes(
                            &ac_remote->transient_state.timer_on, timer_state->on);
                        timer_set_from_minutes(
                            &ac_remote->transient_state.timer_off, timer_state->off);
                    }
                }
                send_on_power_off = true;
                break;
            case button_reset_filter:
                hvac_hitachi_reset_filter(ac_remote->protocol);
                send_on_power_off = true;
                break;
            case button_settings:
                next_event = ac_remote_custom_event_pack(AC_RemoteCustomEventTypeCallSettings, 0);
                view_dispatcher_send_custom_event(ac_remote->view_dispatcher, next_event);
                has_ir_code = false;
                break;
            default:
                has_ir_code = false;
                break;
            }
            if(has_ir_code && (send_on_power_off || app_state->power == PowerButtonOn)) {
                next_event = ac_remote_custom_event_pack(AC_RemoteCustomEventTypeSendCommand, 0);
                view_dispatcher_send_custom_event(ac_remote->view_dispatcher, next_event);
            }
        } else if(event_type == AC_RemoteCustomEventTypeButtonLongPress) {
            switch(event_value) {
            case button_timer_set:
                if(app_state->timer_state != TimerStateStopped) {
                    break;
                }
                hvac_hitachi_reset(ac_remote->protocol);

                timer_set_from_minutes(&ac_remote->transient_state.timer_on, 0);
                if(ac_remote->transient_state.timer_off.minutes_only == 0) {
                    timer_set_from_minutes(&ac_remote->transient_state.timer_off, 120);
                }

                app_state->timer_pause.on = ac_remote->transient_state.timer_on.minutes_only;
                app_state->timer_pause.off = ac_remote->transient_state.timer_off.minutes_only;

                uint32_t current_timestamp = furi_hal_rtc_get_timestamp();
                app_state->timer_on_expires_at =
                    current_timestamp + app_state->timer_pause.on * 60;
                app_state->timer_off_expires_at =
                    current_timestamp + app_state->timer_pause.off * 60;

                hvac_hitachi_test_mode(
                    ac_remote->protocol,
                    app_state->temperature,
                    MODE_LUT[app_state->mode],
                    app_state->timer_pause.off);

                app_state->timer_state = TimerStateRunning;
                app_state->power = PowerButtonOn;

                ac_remote_panel_item_set_icons(
                    panel_sub,
                    button_timer_set,
                    TIMER_SET_BUTTON_ICONS[app_state->timer_state][0],
                    TIMER_SET_BUTTON_ICONS[app_state->timer_state][1]);

                ac_remote_panel_item_set_icons(
                    panel_main,
                    button_power,
                    POWER_ICONS[app_state->power][0],
                    POWER_ICONS[app_state->power][1]);

                next_event = ac_remote_custom_event_pack(AC_RemoteCustomEventTypeSendCommand, 0);
                view_dispatcher_send_custom_event(ac_remote->view_dispatcher, next_event);
                break;
            }
        }
        consumed = true;
    }
    return consumed;
}

void ac_remote_scene_hitachi_on_exit(void* context) {
    FURI_LOG_I(TAG, "Exiting scene...");

    AC_RemoteApp* ac_remote = context;
    ACRemotePanel* panel_main = ac_remote->panel_main;
    ACRemotePanel* panel_sub = ac_remote->panel_sub;
    hvac_hitachi_deinit(ac_remote->protocol);
    ac_remote_panel_reset(panel_main);
    ac_remote_panel_reset(panel_sub);
}
