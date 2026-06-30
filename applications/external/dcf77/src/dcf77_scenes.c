#include "dcf77_scenes.h"

#include <datetime/datetime.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include "dcf77_gui.h"
#include "dcf77_hw.h"
#include "dcf77_logic.h"

enum {
    Dcf77LfSettingSignal,
    Dcf77LfSettingTransmit,
    Dcf77LfSettingFrequency,
    Dcf77LfSettingDefaultFrequency,
};

enum {
    Dcf77TxRatioSettingX,
    Dcf77TxRatioSettingY,
};

enum {
    Dcf77ExperimentalTimeSettingEnabled,
    Dcf77ExperimentalTimeSettingSource,
    Dcf77ExperimentalTimeSettingPreset,
    Dcf77ExperimentalTimeSettingDirection,
    Dcf77ExperimentalTimeSettingSpeed,
};

enum {
    Dcf77SubGhzSettingTransmit,
    Dcf77SubGhzSettingFskTone,
    Dcf77SubGhzSettingBand,
    Dcf77SubGhzSettingFrequency,
    Dcf77SubGhzSettingManualFrequency,
    Dcf77SubGhzSettingTimeout,
};

enum {
    Dcf77DebugSettingBaseband,
    Dcf77DebugSettingRf,
    Dcf77DebugSettingDutyCycle,
    Dcf77DebugSettingLed,
    Dcf77DebugSettingScreen,
    Dcf77DebugSettingSpeaker,
};

static const char* const dcf77_subghz_mode_labels[] = {"No", "OOK", "FSK", "100%"};
static const char* const dcf77_experimental_time_toggle_labels[] = {"No", "Yes"};
static const char* const dcf77_experimental_time_source_labels[] = {"Flipper", "Preset"};
static const char* const dcf77_experimental_time_direction_labels[] = {"Back", "Stop", "Fwd"};

static uint8_t
    dcf77_experimental_time_direction_to_index(Dcf77ExperimentalTimeDirection direction) {
    if(direction >= Dcf77ExperimentalTimeDirectionCount) {
        return 2U;
    }

    return 2U - direction;
}

static Dcf77ExperimentalTimeDirection dcf77_experimental_time_direction_from_index(uint8_t index) {
    if(index > 2U) {
        index = 2U;
    }

    return (Dcf77ExperimentalTimeDirection)(2U - index);
}

static void dcf77_lf_settings_sync(AppFSM* app_fsm) {
    variable_item_set_current_value_index(
        app_fsm->lf_signal_item, radio_clock_visible_signal_index(app_fsm->current_signal));
    variable_item_set_current_value_text(
        app_fsm->lf_signal_item, radio_clock_signal_get_label(app_fsm->current_signal));
    variable_item_set_current_value_index(
        app_fsm->lf_tx_enabled_item, app_fsm->lf_transmit_enabled ? 1 : 0);
    variable_item_set_current_value_text(
        app_fsm->lf_tx_enabled_item, app_fsm->lf_transmit_enabled ? "Yes" : "No");
    variable_item_set_current_value_index(
        app_fsm->lf_freq_item, dcf77_app_get_lf_freq_index(app_fsm->lf_freq));
    variable_item_set_current_value_text(app_fsm->lf_freq_item, app_fsm->lf_freq_text);
    with_view_model(
        variable_item_list_get_view(app_fsm->lf_settings), void* model, { UNUSED(model); }, true);
}

static void dcf77_tx_ratio_settings_sync(AppFSM* app_fsm) {
    variable_item_set_values_count(app_fsm->lf_tx_ratio_x_item, dcf77_app_get_tx_ratio_y(app_fsm));
    variable_item_set_current_value_index(app_fsm->lf_tx_ratio_x_item, app_fsm->tx_ratio_x - 1U);
    variable_item_set_current_value_text(app_fsm->lf_tx_ratio_x_item, app_fsm->tx_ratio_x_text);
    variable_item_set_values_count(app_fsm->lf_tx_ratio_y_item, (uint8_t)dcf77_tx_ratio_y_count());
    variable_item_set_current_value_index(app_fsm->lf_tx_ratio_y_item, app_fsm->tx_ratio_y_index);
    variable_item_set_current_value_text(app_fsm->lf_tx_ratio_y_item, app_fsm->tx_ratio_y_text);
    with_view_model(
        variable_item_list_get_view(app_fsm->tx_ratio_settings),
        void* model,
        { UNUSED(model); },
        true);
}

static void dcf77_experimental_time_settings_sync(AppFSM* app_fsm) {
    variable_item_set_current_value_index(
        app_fsm->experimental_time_enabled_item,
        app_fsm->experimental_time_settings.enabled ? 1U : 0U);
    variable_item_set_current_value_text(
        app_fsm->experimental_time_enabled_item,
        dcf77_experimental_time_toggle_labels
            [app_fsm->experimental_time_settings.enabled ? 1U : 0U]);
    variable_item_set_current_value_index(
        app_fsm->experimental_time_source_item, app_fsm->experimental_time_settings.source);
    variable_item_set_current_value_text(
        app_fsm->experimental_time_source_item,
        dcf77_experimental_time_source_labels[app_fsm->experimental_time_settings.source]);
    variable_item_set_current_value_index(app_fsm->experimental_preset_item, 0U);
    variable_item_set_current_value_text(
        app_fsm->experimental_preset_item, app_fsm->experimental_preset_text);
    variable_item_set_current_value_index(
        app_fsm->experimental_direction_item,
        dcf77_experimental_time_direction_to_index(app_fsm->experimental_time_settings.direction));
    variable_item_set_current_value_text(
        app_fsm->experimental_direction_item,
        dcf77_experimental_time_direction_labels[dcf77_experimental_time_direction_to_index(
            app_fsm->experimental_time_settings.direction)]);
    variable_item_set_current_value_index(
        app_fsm->experimental_speed_item, app_fsm->experimental_time_speed_index);
    variable_item_set_current_value_text(
        app_fsm->experimental_speed_item, app_fsm->experimental_speed_text);
    with_view_model(
        variable_item_list_get_view(app_fsm->experimental_time_settings_view),
        void* model,
        { UNUSED(model); },
        true);
}

static void dcf77_experimental_time_settings_apply(AppFSM* app_fsm) {
    dcf77_experimental_time_settings_sync(app_fsm);
    dcf77_app_settings_save(app_fsm);
}

static void dcf77_lf_settings_select_next_signal(AppFSM* app_fsm, int8_t direction) {
    int32_t value = (int32_t)radio_clock_visible_signal_index(app_fsm->current_signal) + direction;
    const int32_t count = (int32_t)radio_clock_visible_signal_count();

    if(value < 0) {
        value = count - 1;
    } else if(value >= count) {
        value = 0;
    }

    dcf77_app_set_signal(app_fsm, radio_clock_visible_signal_get((size_t)value));
    dcf77_lf_settings_sync(app_fsm);
    dcf77_app_apply_rf_settings(app_fsm);
    dcf77_app_settings_save(app_fsm);
}

static void dcf77_subghz_settings_sync(AppFSM* app_fsm) {
    variable_item_set_current_value_index(app_fsm->subghz_tx_item, app_fsm->subghz_signal_mode);
    variable_item_set_current_value_text(
        app_fsm->subghz_tx_item, dcf77_subghz_mode_labels[app_fsm->subghz_signal_mode]);
    variable_item_set_current_value_index(
        app_fsm->subghz_tone_item, app_fsm->subghz_fsk_tone_index);
    variable_item_set_current_value_text(app_fsm->subghz_tone_item, app_fsm->subghz_tone_text);
    variable_item_set_values_count(app_fsm->subghz_band_item, app_fsm->subghz_band_count);
    variable_item_set_current_value_index(app_fsm->subghz_band_item, app_fsm->subghz_band_index);
    variable_item_set_current_value_text(app_fsm->subghz_band_item, app_fsm->subghz_band_text);
    variable_item_set_current_value_index(app_fsm->subghz_frequency_item, 0);
    variable_item_set_current_value_text(
        app_fsm->subghz_frequency_item, app_fsm->subghz_frequency_step_text);
    variable_item_set_current_value_index(app_fsm->subghz_manual_item, 0);
    variable_item_set_current_value_text(app_fsm->subghz_manual_item, app_fsm->subghz_manual_text);
    variable_item_set_values_count(
        app_fsm->subghz_timeout_item, (uint8_t)dcf77_subghz_tx_timeout_count());
    variable_item_set_current_value_index(
        app_fsm->subghz_timeout_item, app_fsm->subghz_tx_timeout_index);
    variable_item_set_current_value_text(
        app_fsm->subghz_timeout_item, app_fsm->subghz_timeout_text);
    with_view_model(
        variable_item_list_get_view(app_fsm->subghz_settings),
        void* model,
        { UNUSED(model); },
        true);
}

static void dcf77_debug_settings_sync(AppFSM* app_fsm) {
    variable_item_set_current_value_index(
        app_fsm->debug_gpio_baseband_item,
        dcf77_debug_gpio_baseband_pin_index(app_fsm->gpio_baseband_pin_number));
    variable_item_set_current_value_text(
        app_fsm->debug_gpio_baseband_item, app_fsm->gpio_baseband_text);
    variable_item_set_current_value_index(
        app_fsm->debug_gpio_rf_item, dcf77_debug_gpio_rf_pin_index(app_fsm->gpio_rf_pin_number));
    variable_item_set_current_value_text(app_fsm->debug_gpio_rf_item, app_fsm->gpio_rf_text);
    variable_item_set_current_value_index(
        app_fsm->debug_gpio_duty_item,
        dcf77_debug_gpio_rf_duty_index(app_fsm->gpio_rf_duty_cycle));
    variable_item_set_current_value_text(
        app_fsm->debug_gpio_duty_item, app_fsm->gpio_rf_duty_text);
    variable_item_set_current_value_index(
        app_fsm->debug_led_item, dcf77_debug_led_color_index(app_fsm->led_color_index));
    variable_item_set_current_value_text(app_fsm->debug_led_item, app_fsm->led_text);
    variable_item_set_current_value_index(app_fsm->debug_screen_item, app_fsm->screen_mode);
    variable_item_set_current_value_text(app_fsm->debug_screen_item, app_fsm->screen_text);
    variable_item_set_current_value_index(
        app_fsm->debug_speaker_item, app_fsm->speaker_value_index);
    variable_item_set_current_value_text(app_fsm->debug_speaker_item, app_fsm->speaker_text);
    with_view_model(
        variable_item_list_get_view(app_fsm->debug_settings),
        void* model,
        { UNUSED(model); },
        true);
}

static void dcf77_subghz_settings_apply(AppFSM* app_fsm) {
    dcf77_subghz_settings_sync(app_fsm);
    dcf77_app_apply_rf_settings(app_fsm);
    dcf77_app_settings_save(app_fsm);
}

static void dcf77_debug_settings_apply(AppFSM* app_fsm) {
    dcf77_debug_settings_sync(app_fsm);
    dcf77_app_apply_rf_settings(app_fsm);
    dcf77_app_settings_save(app_fsm);
}

void dcf77_app_switch_to_menu(AppFSM* app_fsm) {
    app_fsm->screen = AppScreenMenu;
    submenu_set_selected_item(app_fsm->submenu, Dcf77MenuItemStart);
    view_dispatcher_switch_to_view(app_fsm->view_dispatcher, Dcf77ViewMenu);
}

void dcf77_app_switch_to_advanced_menu(AppFSM* app_fsm) {
    app_fsm->screen = AppScreenAdvancedMenu;
    submenu_set_selected_item(app_fsm->advanced_menu, Dcf77AdvancedMenuItemSubGhzSettings);
    view_dispatcher_switch_to_view(app_fsm->view_dispatcher, Dcf77ViewAdvancedMenu);
}

void dcf77_app_switch_to_about(AppFSM* app_fsm) {
    app_fsm->screen = AppScreenAbout;
    view_dispatcher_switch_to_view(app_fsm->view_dispatcher, Dcf77ViewAbout);
}

void dcf77_app_switch_to_egg(AppFSM* app_fsm) {
    app_fsm->screen = AppScreenEgg;
    view_dispatcher_switch_to_view(app_fsm->view_dispatcher, Dcf77ViewEgg);
}

void dcf77_app_switch_to_lf_settings(AppFSM* app_fsm) {
    app_fsm->screen = AppScreenLfSettings;
    dcf77_lf_settings_sync(app_fsm);
    view_dispatcher_switch_to_view(app_fsm->view_dispatcher, Dcf77ViewLfSettings);
}

void dcf77_app_switch_to_tx_ratio_settings(AppFSM* app_fsm) {
    app_fsm->screen = AppScreenTxRatioSettings;
    dcf77_tx_ratio_settings_sync(app_fsm);
    view_dispatcher_switch_to_view(app_fsm->view_dispatcher, Dcf77ViewTxRatioSettings);
}

void dcf77_app_switch_to_experimental_time_settings(AppFSM* app_fsm) {
    app_fsm->screen = AppScreenExperimentalTimeSettings;
    dcf77_experimental_time_settings_sync(app_fsm);
    view_dispatcher_switch_to_view(app_fsm->view_dispatcher, Dcf77ViewExperimentalTimeSettings);
}

void dcf77_app_switch_to_subghz_settings(AppFSM* app_fsm) {
    app_fsm->screen = AppScreenSubGhzSettings;
    dcf77_subghz_settings_sync(app_fsm);
    view_dispatcher_switch_to_view(app_fsm->view_dispatcher, Dcf77ViewSubGhzSettings);
}

void dcf77_app_switch_to_debug_settings(AppFSM* app_fsm) {
    app_fsm->screen = AppScreenDebugSettings;
    dcf77_debug_settings_sync(app_fsm);
    view_dispatcher_switch_to_view(app_fsm->view_dispatcher, Dcf77ViewDebugSettings);
}

void dcf77_app_switch_to_gpio_rf_warning(AppFSM* app_fsm, uint8_t pending_pin) {
    app_fsm->gpio_rf_pending_pin_number = pending_pin;
    app_fsm->screen = AppScreenGpioRfWarning;
    notification_message_block(app_fsm->notification, &sequence_error);
    view_dispatcher_switch_to_view(app_fsm->view_dispatcher, Dcf77ViewGpioRfWarning);
}

static void dcf77_app_switch_to_subghz_freq_input(AppFSM* app_fsm) {
    const int32_t current_khz = (int32_t)(dcf77_app_get_subghz_frequency(app_fsm) / 1000U);
    const int32_t min_khz =
        (int32_t)(app_fsm->subghz_band_starts[app_fsm->subghz_band_index] / 1000U);
    const int32_t max_khz =
        (int32_t)(app_fsm->subghz_band_ends[app_fsm->subghz_band_index] / 1000U);

    number_input_set_result_callback(
        app_fsm->subghz_freq_input,
        dcf77_subghz_freq_input_result_callback,
        app_fsm,
        current_khz,
        min_khz,
        max_khz);
    app_fsm->screen = AppScreenSubGhzFreqInput;
    view_dispatcher_switch_to_view(app_fsm->view_dispatcher, Dcf77ViewSubGhzFreqInput);
}

static void dcf77_app_switch_to_preset_time_input(AppFSM* app_fsm) {
    DateTime preset_datetime = app_fsm->experimental_time_settings.preset_datetime;

    dcf77_experimental_time_input_set(app_fsm->preset_time_input, &preset_datetime);
    app_fsm->screen = AppScreenPresetTimeInput;
    view_dispatcher_switch_to_view(app_fsm->view_dispatcher, Dcf77ViewPresetTimeInput);
}

static void dcf77_prepare_tx_minute_frames(AppFSM* app_fsm, const DateTime* rtc_snapshot) {
    DateTime current_minute;
    DateTime next_minute;

    dcf77_experimental_time_seed_runtime(
        &app_fsm->experimental_time_runtime, &app_fsm->experimental_time_settings, rtc_snapshot);
    dcf77_experimental_time_get_current_frame_minute(
        &app_fsm->experimental_time_settings,
        &app_fsm->experimental_time_runtime,
        &current_minute);
    dcf77_logic_prepare_minute(app_fsm, &current_minute, false);

    dcf77_experimental_time_get_frame_datetime(
        &app_fsm->experimental_time_settings,
        &app_fsm->experimental_time_runtime,
        app_fsm->experimental_time_runtime.frame_index + 1U,
        &next_minute);
    next_minute.second = 0U;
    dcf77_logic_prepare_minute(app_fsm, &next_minute, true);
    app_fsm->next_minute_ready = true;
    app_fsm->next_minute_prepare_pending = false;
}

void dcf77_app_start_tx(AppFSM* app_fsm) {
    if(app_fsm->tx_active) {
        return;
    }

    if(!dcf77_app_signal_can_run(app_fsm)) {
        notification_message_block(app_fsm->notification, &sequence_error);
        dcf77_app_switch_to_lf_settings(app_fsm);
        return;
    }

    if(app_fsm->current_signal == RadioClockSignalTest) {
        app_fsm->bit_number = 0;
        app_fsm->bit_value = 1;
        app_fsm->tx_frame_enabled = true;
        app_fsm->next_tx_frame_enabled = true;
        app_fsm->tx_progress_second = 0;
        app_fsm->tx_second_start_tick = furi_get_tick();
        app_fsm->tx_session_start_tick = app_fsm->tx_second_start_tick;
        app_fsm->subghz_timeout_elapsed = false;
        app_fsm->tx_active = true;
        dcf77_timing_start(app_fsm);
        dcf77_app_apply_rf_settings(app_fsm);
        app_fsm->screen = AppScreenTx;
        app_fsm->last_tx_refresh_tick = furi_get_tick();
        view_dispatcher_switch_to_view(app_fsm->view_dispatcher, Dcf77ViewTx);
        return;
    }

    DateTime dt;
    dcf77_wait_for_next_second(&dt);
    dcf77_prepare_tx_minute_frames(app_fsm, &dt);
    app_fsm->tx_frame_enabled = dcf77_app_should_send_frame(app_fsm, 0U);
    app_fsm->next_tx_frame_enabled = dcf77_app_should_send_frame(app_fsm, 1U);
    app_fsm->tx_start_second_offset =
        dcf77_experimental_time_get_start_second(&app_fsm->experimental_time_runtime);

    app_fsm->tx_active = true;
#if DEBUG_SYNC_FRAME
    dcf77_debug_sync_frame(app_fsm);
#endif
#if SYNC_ON_START
    dcf77_logic_sync_start(app_fsm, 50, true);
#else
    DateTime sync_dt = dt;
    sync_dt.second = app_fsm->tx_start_second_offset;
    dcf77_logic_sync_to_second(app_fsm, &sync_dt);
#endif
    app_fsm->tx_progress_second = app_fsm->bit_number;
    app_fsm->tx_second_start_tick = furi_get_tick();
    app_fsm->tx_session_start_tick = app_fsm->tx_second_start_tick;
    app_fsm->subghz_timeout_elapsed = false;
    dcf77_timing_start(app_fsm);
    dcf77_app_apply_rf_settings(app_fsm);
    app_fsm->screen = AppScreenTx;
    app_fsm->last_tx_refresh_tick = furi_get_tick();
    view_dispatcher_switch_to_view(app_fsm->view_dispatcher, Dcf77ViewTx);
}

void dcf77_app_stop_tx(AppFSM* app_fsm) {
    if(!app_fsm->tx_active) {
        return;
    }

    dcf77_timing_stop(app_fsm);
    dcf77_gpio_rf_deinit(app_fsm);
    dcf77_lf_deinit(app_fsm);
    dcf77_subghz_deinit(app_fsm);
    dcf77_output_gpio_set(app_fsm, false);
    dcf77_hw_indicator_set(app_fsm, false);
    dcf77_app_sound_set(app_fsm, false);
    app_fsm->output_state = false;
    app_fsm->output_dirty = false;
    app_fsm->next_minute_ready = false;
    app_fsm->next_minute_prepare_pending = false;
    app_fsm->tx_frame_enabled = false;
    app_fsm->next_tx_frame_enabled = false;
    app_fsm->subghz_timeout_elapsed = false;
    app_fsm->tx_active = false;
    dcf77_app_switch_to_menu(app_fsm);
}

uint32_t dcf77_tx_previous_callback(void* ctx) {
    AppFSM* app_fsm = ctx;
    notification_message_block(app_fsm->notification, &seq_c_minor);
    dcf77_app_stop_tx(app_fsm);
    return Dcf77ViewMenu;
}

uint32_t dcf77_text_previous_callback(void* ctx) {
    AppFSM* app_fsm = ctx;
    if(app_fsm->screen == AppScreenEgg) {
        dcf77_app_switch_to_about(app_fsm);
        return Dcf77ViewAbout;
    }

    dcf77_app_switch_to_menu(app_fsm);
    return Dcf77ViewMenu;
}

void dcf77_lf_transmit_change_callback(VariableItem* item) {
    UNUSED(item);
}

void dcf77_lf_frequency_change_callback(VariableItem* item) {
    UNUSED(item);
}

bool dcf77_lf_settings_input_callback(InputEvent* event, void* ctx) {
    AppFSM* app_fsm = ctx;
    uint8_t selected;

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }

    selected = variable_item_list_get_selected_item_index(app_fsm->lf_settings);

    switch(event->key) {
    case InputKeyUp:
        if(selected == 0) {
            selected = Dcf77LfSettingDefaultFrequency;
        } else {
            selected--;
        }
        variable_item_list_set_selected_item(app_fsm->lf_settings, selected);
        return true;
    case InputKeyDown:
        selected++;
        if(selected > Dcf77LfSettingDefaultFrequency) {
            selected = 0;
        }
        variable_item_list_set_selected_item(app_fsm->lf_settings, selected);
        return true;
    case InputKeyLeft:
        if(selected == Dcf77LfSettingSignal) {
            dcf77_lf_settings_select_next_signal(app_fsm, -1);
            return true;
        }
        if(selected == Dcf77LfSettingTransmit) {
            app_fsm->lf_transmit_enabled = false;
            dcf77_lf_settings_sync(app_fsm);
            dcf77_app_apply_rf_settings(app_fsm);
            dcf77_app_settings_save(app_fsm);
            return true;
        }
        if(selected == Dcf77LfSettingFrequency && app_fsm->lf_freq > LF_FREQ_MIN) {
            dcf77_app_set_signal_frequency(app_fsm, app_fsm->lf_freq - LF_FREQ_STEP);
            dcf77_lf_settings_sync(app_fsm);
            dcf77_app_apply_rf_settings(app_fsm);
            dcf77_app_settings_save(app_fsm);
            return true;
        }
        return selected == Dcf77LfSettingDefaultFrequency;
    case InputKeyRight:
        if(selected == Dcf77LfSettingSignal) {
            dcf77_lf_settings_select_next_signal(app_fsm, 1);
            return true;
        }
        if(selected == Dcf77LfSettingTransmit) {
            app_fsm->lf_transmit_enabled = true;
            dcf77_lf_settings_sync(app_fsm);
            dcf77_app_apply_rf_settings(app_fsm);
            dcf77_app_settings_save(app_fsm);
            return true;
        }
        if(selected == Dcf77LfSettingFrequency && app_fsm->lf_freq < LF_FREQ_MAX) {
            dcf77_app_set_signal_frequency(app_fsm, app_fsm->lf_freq + LF_FREQ_STEP);
            dcf77_lf_settings_sync(app_fsm);
            dcf77_app_apply_rf_settings(app_fsm);
            dcf77_app_settings_save(app_fsm);
            return true;
        }
        return selected == Dcf77LfSettingDefaultFrequency;
    case InputKeyOk:
        if(selected == Dcf77LfSettingDefaultFrequency) {
            dcf77_app_restore_default_frequency(app_fsm);
            dcf77_lf_settings_sync(app_fsm);
            dcf77_app_apply_rf_settings(app_fsm);
            dcf77_app_settings_save(app_fsm);
            return true;
        }
        return false;
    default:
        return false;
    }
}

void dcf77_lf_settings_enter_callback(void* ctx, uint32_t index) {
    AppFSM* app_fsm = ctx;

    if(index == Dcf77LfSettingDefaultFrequency) {
        dcf77_app_restore_default_frequency(app_fsm);
        dcf77_lf_settings_sync(app_fsm);
        dcf77_app_apply_rf_settings(app_fsm);
        dcf77_app_settings_save(app_fsm);
    }
}

bool dcf77_tx_ratio_settings_input_callback(InputEvent* event, void* ctx) {
    AppFSM* app_fsm = ctx;
    uint8_t selected;

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }

    selected = variable_item_list_get_selected_item_index(app_fsm->tx_ratio_settings);

    switch(event->key) {
    case InputKeyUp:
        selected = (selected == 0U) ? Dcf77TxRatioSettingY : selected - 1U;
        variable_item_list_set_selected_item(app_fsm->tx_ratio_settings, selected);
        return true;
    case InputKeyDown:
        selected = (selected >= Dcf77TxRatioSettingY) ? 0U : selected + 1U;
        variable_item_list_set_selected_item(app_fsm->tx_ratio_settings, selected);
        return true;
    case InputKeyLeft:
        if(selected == Dcf77TxRatioSettingX && app_fsm->tx_ratio_x > 1U) {
            dcf77_app_set_tx_ratio_x(app_fsm, app_fsm->tx_ratio_x - 1U);
            dcf77_tx_ratio_settings_sync(app_fsm);
            dcf77_app_settings_save(app_fsm);
            return true;
        }
        if(selected == Dcf77TxRatioSettingY && app_fsm->tx_ratio_y_index > 0U) {
            dcf77_app_set_tx_ratio_y_index(app_fsm, app_fsm->tx_ratio_y_index - 1U);
            dcf77_tx_ratio_settings_sync(app_fsm);
            dcf77_app_settings_save(app_fsm);
            return true;
        }
        return false;
    case InputKeyRight:
        if(selected == Dcf77TxRatioSettingX &&
           app_fsm->tx_ratio_x < dcf77_app_get_tx_ratio_y(app_fsm)) {
            dcf77_app_set_tx_ratio_x(app_fsm, app_fsm->tx_ratio_x + 1U);
            dcf77_tx_ratio_settings_sync(app_fsm);
            dcf77_app_settings_save(app_fsm);
            return true;
        }
        if(selected == Dcf77TxRatioSettingY &&
           app_fsm->tx_ratio_y_index + 1U < dcf77_tx_ratio_y_count()) {
            dcf77_app_set_tx_ratio_y_index(app_fsm, app_fsm->tx_ratio_y_index + 1U);
            dcf77_tx_ratio_settings_sync(app_fsm);
            dcf77_app_settings_save(app_fsm);
            return true;
        }
        return false;
    default:
        return false;
    }
}

bool dcf77_experimental_time_settings_input_callback(InputEvent* event, void* ctx) {
    AppFSM* app_fsm = ctx;
    uint8_t selected;

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }

    selected =
        variable_item_list_get_selected_item_index(app_fsm->experimental_time_settings_view);

    switch(event->key) {
    case InputKeyUp:
        if(selected == 0U) {
            selected = Dcf77ExperimentalTimeSettingSpeed;
        } else {
            selected--;
        }
        variable_item_list_set_selected_item(app_fsm->experimental_time_settings_view, selected);
        return true;
    case InputKeyDown:
        selected++;
        if(selected > Dcf77ExperimentalTimeSettingSpeed) {
            selected = 0U;
        }
        variable_item_list_set_selected_item(app_fsm->experimental_time_settings_view, selected);
        return true;
    case InputKeyLeft:
        if(selected == Dcf77ExperimentalTimeSettingEnabled) {
            dcf77_app_set_experimental_time_enabled(app_fsm, false);
            dcf77_experimental_time_settings_apply(app_fsm);
            return true;
        }
        if(selected == Dcf77ExperimentalTimeSettingSource &&
           app_fsm->experimental_time_settings.source > Dcf77ExperimentalTimeSourceFlipper) {
            dcf77_app_set_experimental_time_source(
                app_fsm, app_fsm->experimental_time_settings.source - 1);
            dcf77_experimental_time_settings_apply(app_fsm);
            return true;
        }
        if(selected == Dcf77ExperimentalTimeSettingDirection &&
           dcf77_experimental_time_direction_to_index(
               app_fsm->experimental_time_settings.direction) > 0U) {
            dcf77_app_set_experimental_time_direction(
                app_fsm,
                dcf77_experimental_time_direction_from_index(
                    dcf77_experimental_time_direction_to_index(
                        app_fsm->experimental_time_settings.direction) -
                    1U));
            dcf77_experimental_time_settings_apply(app_fsm);
            return true;
        }
        if(selected == Dcf77ExperimentalTimeSettingSpeed &&
           app_fsm->experimental_time_speed_index > 0U) {
            dcf77_app_set_experimental_time_speed_index(
                app_fsm, app_fsm->experimental_time_speed_index - 1U);
            dcf77_experimental_time_settings_apply(app_fsm);
            return true;
        }
        return false;
    case InputKeyRight:
        if(selected == Dcf77ExperimentalTimeSettingEnabled) {
            dcf77_app_set_experimental_time_enabled(app_fsm, true);
            dcf77_experimental_time_settings_apply(app_fsm);
            return true;
        }
        if(selected == Dcf77ExperimentalTimeSettingSource &&
           app_fsm->experimental_time_settings.source + 1U < Dcf77ExperimentalTimeSourceCount) {
            dcf77_app_set_experimental_time_source(
                app_fsm, app_fsm->experimental_time_settings.source + 1U);
            dcf77_experimental_time_settings_apply(app_fsm);
            return true;
        }
        if(selected == Dcf77ExperimentalTimeSettingDirection &&
           dcf77_experimental_time_direction_to_index(
               app_fsm->experimental_time_settings.direction) < 2U) {
            dcf77_app_set_experimental_time_direction(
                app_fsm,
                dcf77_experimental_time_direction_from_index(
                    dcf77_experimental_time_direction_to_index(
                        app_fsm->experimental_time_settings.direction) +
                    1U));
            dcf77_experimental_time_settings_apply(app_fsm);
            return true;
        }
        if(selected == Dcf77ExperimentalTimeSettingSpeed &&
           app_fsm->experimental_time_speed_index < 14U) {
            dcf77_app_set_experimental_time_speed_index(
                app_fsm, app_fsm->experimental_time_speed_index + 1U);
            dcf77_experimental_time_settings_apply(app_fsm);
            return true;
        }
        return false;
    case InputKeyOk:
        if(selected == Dcf77ExperimentalTimeSettingPreset) {
            dcf77_app_switch_to_preset_time_input(app_fsm);
            return true;
        }
        return false;
    default:
        return false;
    }
}

void dcf77_experimental_time_settings_enter_callback(void* ctx, uint32_t index) {
    AppFSM* app_fsm = ctx;

    if(index == Dcf77ExperimentalTimeSettingPreset) {
        dcf77_app_switch_to_preset_time_input(app_fsm);
    }
}

uint32_t dcf77_preset_time_input_previous_callback(void* ctx) {
    AppFSM* app_fsm = ctx;
    DateTime preset_datetime;

    if(dcf77_experimental_time_input_get(app_fsm->preset_time_input, &preset_datetime)) {
        dcf77_app_set_experimental_preset_datetime(app_fsm, &preset_datetime);
        dcf77_app_set_experimental_time_source(app_fsm, Dcf77ExperimentalTimeSourcePreset);
        dcf77_app_settings_save(app_fsm);
    }

    dcf77_experimental_time_settings_sync(app_fsm);
    app_fsm->screen = AppScreenExperimentalTimeSettings;
    return Dcf77ViewExperimentalTimeSettings;
}

static bool dcf77_subghz_tx_timeout_expired(const AppFSM* app_fsm, uint32_t now_tick) {
    if(!app_fsm->tx_active || app_fsm->subghz_signal_mode == SubGhzSignalModeDisabled ||
       dcf77_app_gpio_rf_enabled(app_fsm) || app_fsm->subghz_timeout_elapsed) {
        return false;
    }

    const uint32_t timeout_ms = dcf77_app_get_subghz_tx_timeout_seconds(app_fsm) * 1000U;
    return (now_tick - app_fsm->tx_session_start_tick) >= furi_ms_to_ticks(timeout_ms);
}

bool dcf77_subghz_settings_input_callback(InputEvent* event, void* ctx) {
    AppFSM* app_fsm = ctx;
    uint8_t selected;

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }

    selected = variable_item_list_get_selected_item_index(app_fsm->subghz_settings);

    switch(event->key) {
    case InputKeyUp:
        if(selected == 0) {
            selected = Dcf77SubGhzSettingTimeout;
        } else {
            selected--;
        }
        variable_item_list_set_selected_item(app_fsm->subghz_settings, selected);
        return true;
    case InputKeyDown:
        selected++;
        if(selected > Dcf77SubGhzSettingTimeout) {
            selected = 0;
        }
        variable_item_list_set_selected_item(app_fsm->subghz_settings, selected);
        return true;
    case InputKeyLeft:
        if(selected == Dcf77SubGhzSettingTransmit &&
           app_fsm->subghz_signal_mode > SubGhzSignalModeDisabled) {
            dcf77_app_set_subghz_signal_mode(
                app_fsm, (SubGhzSignalMode)(app_fsm->subghz_signal_mode - 1));
            dcf77_subghz_settings_apply(app_fsm);
            return true;
        }
        if(selected == Dcf77SubGhzSettingFskTone && app_fsm->subghz_fsk_tone_index > 0) {
            dcf77_app_set_subghz_fsk_tone(app_fsm, app_fsm->subghz_fsk_tone_index - 1U);
            dcf77_subghz_settings_apply(app_fsm);
            return true;
        }
        if(selected == Dcf77SubGhzSettingBand && app_fsm->subghz_band_index > 0) {
            dcf77_app_set_subghz_band(app_fsm, app_fsm->subghz_band_index - 1U);
            dcf77_subghz_settings_apply(app_fsm);
            return true;
        }
        if(selected == Dcf77SubGhzSettingFrequency) {
            dcf77_app_step_subghz_frequency(app_fsm, -1);
            dcf77_subghz_settings_apply(app_fsm);
            return true;
        }
        if(selected == Dcf77SubGhzSettingTimeout && app_fsm->subghz_tx_timeout_index > 0U) {
            dcf77_app_set_subghz_tx_timeout_index(app_fsm, app_fsm->subghz_tx_timeout_index - 1U);
            dcf77_subghz_settings_apply(app_fsm);
            return true;
        }
        return selected == Dcf77SubGhzSettingManualFrequency;
    case InputKeyRight:
        if(selected == Dcf77SubGhzSettingTransmit &&
           app_fsm->subghz_signal_mode < SubGhzSignalModeFskFull) {
            dcf77_app_set_subghz_signal_mode(
                app_fsm, (SubGhzSignalMode)(app_fsm->subghz_signal_mode + 1));
            dcf77_subghz_settings_apply(app_fsm);
            return true;
        }
        if(selected == Dcf77SubGhzSettingFskTone &&
           app_fsm->subghz_fsk_tone_index + 1U < dcf77_subghz_note_count()) {
            dcf77_app_set_subghz_fsk_tone(app_fsm, app_fsm->subghz_fsk_tone_index + 1U);
            dcf77_subghz_settings_apply(app_fsm);
            return true;
        }
        if(selected == Dcf77SubGhzSettingBand &&
           app_fsm->subghz_band_index + 1U < app_fsm->subghz_band_count) {
            dcf77_app_set_subghz_band(app_fsm, app_fsm->subghz_band_index + 1U);
            dcf77_subghz_settings_apply(app_fsm);
            return true;
        }
        if(selected == Dcf77SubGhzSettingFrequency) {
            dcf77_app_step_subghz_frequency(app_fsm, 1);
            dcf77_subghz_settings_apply(app_fsm);
            return true;
        }
        if(selected == Dcf77SubGhzSettingTimeout &&
           app_fsm->subghz_tx_timeout_index + 1U < dcf77_subghz_tx_timeout_count()) {
            dcf77_app_set_subghz_tx_timeout_index(app_fsm, app_fsm->subghz_tx_timeout_index + 1U);
            dcf77_subghz_settings_apply(app_fsm);
            return true;
        }
        return selected == Dcf77SubGhzSettingManualFrequency;
    case InputKeyOk:
        if(selected == Dcf77SubGhzSettingManualFrequency) {
            dcf77_app_switch_to_subghz_freq_input(app_fsm);
            return true;
        }
        return false;
    default:
        return false;
    }
}

void dcf77_subghz_settings_enter_callback(void* ctx, uint32_t index) {
    AppFSM* app_fsm = ctx;

    if(index == Dcf77SubGhzSettingManualFrequency) {
        dcf77_app_switch_to_subghz_freq_input(app_fsm);
    }
}

uint32_t dcf77_subghz_freq_input_previous_callback(void* ctx) {
    AppFSM* app_fsm = ctx;

    dcf77_app_switch_to_subghz_settings(app_fsm);
    return Dcf77ViewSubGhzSettings;
}

void dcf77_subghz_freq_input_result_callback(void* ctx, int32_t number) {
    AppFSM* app_fsm = ctx;
    dcf77_app_set_subghz_frequency(app_fsm, (uint32_t)number * 1000U);
    dcf77_subghz_settings_apply(app_fsm);

    dcf77_app_switch_to_subghz_settings(app_fsm);
}

static void dcf77_debug_settings_cancel_gpio_rf_warning(AppFSM* app_fsm) {
    app_fsm->gpio_rf_pending_pin_number = dcf77_debug_gpio_rf_default_pin();
    dcf77_app_switch_to_debug_settings(app_fsm);
}

static void dcf77_debug_settings_accept_gpio_rf_warning(AppFSM* app_fsm) {
    dcf77_app_set_gpio_rf_pin(app_fsm, app_fsm->gpio_rf_pending_pin_number);
    app_fsm->gpio_rf_pending_pin_number = dcf77_debug_gpio_rf_default_pin();
    dcf77_debug_settings_apply(app_fsm);
    dcf77_app_switch_to_debug_settings(app_fsm);
}

static bool dcf77_debug_settings_step_gpio_rf(AppFSM* app_fsm, int8_t direction) {
    const uint8_t next_pin = dcf77_debug_gpio_rf_step(app_fsm->gpio_rf_pin_number, direction);

    if(next_pin == app_fsm->gpio_rf_pin_number) {
        return true;
    }

    if(app_fsm->gpio_rf_pin_number == dcf77_debug_gpio_rf_default_pin() &&
       next_pin != dcf77_debug_gpio_rf_default_pin()) {
        dcf77_app_switch_to_gpio_rf_warning(app_fsm, next_pin);
        return true;
    }

    dcf77_app_set_gpio_rf_pin(app_fsm, next_pin);
    dcf77_debug_settings_apply(app_fsm);
    return true;
}

bool dcf77_debug_settings_input_callback(InputEvent* event, void* ctx) {
    AppFSM* app_fsm = ctx;
    uint8_t selected;

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }

    selected = variable_item_list_get_selected_item_index(app_fsm->debug_settings);

    switch(event->key) {
    case InputKeyUp:
        if(selected == 0U) {
            selected = Dcf77DebugSettingSpeaker;
        } else {
            selected--;
        }
        variable_item_list_set_selected_item(app_fsm->debug_settings, selected);
        return true;
    case InputKeyDown:
        selected++;
        if(selected > Dcf77DebugSettingSpeaker) {
            selected = 0U;
        }
        variable_item_list_set_selected_item(app_fsm->debug_settings, selected);
        return true;
    case InputKeyLeft:
        if(selected == Dcf77DebugSettingBaseband) {
            const uint8_t old_pin = app_fsm->gpio_baseband_pin_number;
            dcf77_app_set_gpio_baseband_pin(
                app_fsm,
                dcf77_debug_gpio_baseband_step(
                    app_fsm->gpio_baseband_pin_number, -1, app_fsm->gpio_rf_pin_number));
            dcf77_output_gpio_reconfigure(app_fsm, old_pin);
            dcf77_debug_settings_apply(app_fsm);
            return true;
        }
        if(selected == Dcf77DebugSettingRf) {
            return dcf77_debug_settings_step_gpio_rf(app_fsm, -1);
        }
        if(selected == Dcf77DebugSettingDutyCycle) {
            dcf77_app_set_gpio_rf_duty_cycle(
                app_fsm, dcf77_debug_gpio_rf_duty_step(app_fsm->gpio_rf_duty_cycle, -1));
            dcf77_debug_settings_apply(app_fsm);
            return true;
        }
        if(selected == Dcf77DebugSettingLed) {
            const size_t led_index = dcf77_debug_led_color_index(app_fsm->led_color_index);

            if(led_index == 0U) {
                return true;
            }

            dcf77_app_set_led_color(
                app_fsm, (Dcf77LedColor)dcf77_debug_led_color_at(led_index - 1U));
            dcf77_debug_settings_apply(app_fsm);
            dcf77_hw_indicator_blink_preview(app_fsm);
            return true;
        }
        if(selected == Dcf77DebugSettingScreen && app_fsm->screen_mode > 0U) {
            dcf77_app_set_screen_mode(app_fsm, (Dcf77ScreenMode)(app_fsm->screen_mode - 1U));
            dcf77_debug_settings_apply(app_fsm);
            return true;
        }
        if(selected == Dcf77DebugSettingSpeaker && app_fsm->speaker_value_index > 0U) {
            dcf77_app_set_speaker_value_index(app_fsm, app_fsm->speaker_value_index - 1U);
            dcf77_app_sound_set(app_fsm, false);
            dcf77_debug_settings_apply(app_fsm);
            return true;
        }
        return false;
    case InputKeyRight:
        if(selected == Dcf77DebugSettingBaseband) {
            const uint8_t old_pin = app_fsm->gpio_baseband_pin_number;
            dcf77_app_set_gpio_baseband_pin(
                app_fsm,
                dcf77_debug_gpio_baseband_step(
                    app_fsm->gpio_baseband_pin_number, 1, app_fsm->gpio_rf_pin_number));
            dcf77_output_gpio_reconfigure(app_fsm, old_pin);
            dcf77_debug_settings_apply(app_fsm);
            return true;
        }
        if(selected == Dcf77DebugSettingRf) {
            return dcf77_debug_settings_step_gpio_rf(app_fsm, 1);
        }
        if(selected == Dcf77DebugSettingDutyCycle) {
            dcf77_app_set_gpio_rf_duty_cycle(
                app_fsm, dcf77_debug_gpio_rf_duty_step(app_fsm->gpio_rf_duty_cycle, 1));
            dcf77_debug_settings_apply(app_fsm);
            return true;
        }
        if(selected == Dcf77DebugSettingLed) {
            const size_t led_index = dcf77_debug_led_color_index(app_fsm->led_color_index);

            if(led_index + 1U >= dcf77_debug_led_color_count()) {
                return true;
            }

            dcf77_app_set_led_color(
                app_fsm, (Dcf77LedColor)dcf77_debug_led_color_at(led_index + 1U));
            dcf77_debug_settings_apply(app_fsm);
            dcf77_hw_indicator_blink_preview(app_fsm);
            return true;
        }
        if(selected == Dcf77DebugSettingScreen &&
           app_fsm->screen_mode + 1U < dcf77_debug_screen_mode_count()) {
            dcf77_app_set_screen_mode(app_fsm, (Dcf77ScreenMode)(app_fsm->screen_mode + 1U));
            dcf77_debug_settings_apply(app_fsm);
            return true;
        }
        if(selected == Dcf77DebugSettingSpeaker &&
           app_fsm->speaker_value_index < dcf77_subghz_note_count()) {
            dcf77_app_set_speaker_value_index(app_fsm, app_fsm->speaker_value_index + 1U);
            dcf77_debug_settings_apply(app_fsm);
            return true;
        }
        return false;
    default:
        return false;
    }
}

uint32_t dcf77_gpio_rf_warning_previous_callback(void* ctx) {
    AppFSM* app_fsm = ctx;
    dcf77_debug_settings_cancel_gpio_rf_warning(app_fsm);
    return Dcf77ViewDebugSettings;
}

bool dcf77_gpio_rf_warning_input_callback(InputEvent* event, void* ctx) {
    AppFSM* app_fsm = ctx;

    if(event->type != InputTypePress && event->type != InputTypeShort) {
        return false;
    }

    switch(event->key) {
    case InputKeyLeft:
    case InputKeyBack:
        dcf77_debug_settings_cancel_gpio_rf_warning(app_fsm);
        return true;
    case InputKeyRight:
    case InputKeyOk:
        dcf77_debug_settings_accept_gpio_rf_warning(app_fsm);
        return true;
    default:
        return false;
    }
}

void dcf77_egg_button_callback(GuiButtonType result, InputType type, void* ctx) {
    AppFSM* app_fsm = ctx;
    if(type != InputTypePress) {
        return;
    }

    if(result == GuiButtonTypeLeft) {
        dcf77_app_switch_to_about(app_fsm);
    }
}

bool dcf77_egg_input_callback(InputEvent* event, void* ctx) {
    AppFSM* app_fsm = ctx;
    if(event->type == InputTypePress &&
       (event->key == InputKeyLeft || event->key == InputKeyBack)) {
        dcf77_app_switch_to_about(app_fsm);
        return true;
    }

    return false;
}

bool dcf77_tx_input_callback(InputEvent* event, void* ctx) {
    AppFSM* app_fsm = ctx;
    if(event->type != InputTypePress) {
        return false;
    }

    switch(event->key) {
    case InputKeyDown:
    case InputKeyRight:
    case InputKeyLeft:
    case InputKeyOk:
        return true;
    case InputKeyBack:
        notification_message_block(app_fsm->notification, &seq_c_minor);
        dcf77_app_stop_tx(app_fsm);
        return true;
    default:
        return false;
    }
}

void dcf77_menu_callback(void* ctx, uint32_t index) {
    AppFSM* app_fsm = ctx;

    switch(index) {
    case Dcf77MenuItemStart:
        dcf77_app_start_tx(app_fsm);
        break;
    case Dcf77MenuItemLfSettings:
        dcf77_app_switch_to_lf_settings(app_fsm);
        break;
    case Dcf77MenuItemAdvanced:
        dcf77_app_switch_to_advanced_menu(app_fsm);
        break;
    case Dcf77MenuItemAbout:
        dcf77_app_switch_to_about(app_fsm);
        break;
    default:
        break;
    }
}

void dcf77_advanced_menu_callback(void* ctx, uint32_t index) {
    AppFSM* app_fsm = ctx;

    switch(index) {
    case Dcf77AdvancedMenuItemSubGhzSettings:
        dcf77_app_switch_to_subghz_settings(app_fsm);
        break;
    case Dcf77AdvancedMenuItemDebugSettings:
        dcf77_app_switch_to_debug_settings(app_fsm);
        break;
    case Dcf77AdvancedMenuItemExperimentalTimeSettings:
        dcf77_app_switch_to_experimental_time_settings(app_fsm);
        break;
    case Dcf77AdvancedMenuItemTxRatioSettings:
        dcf77_app_switch_to_tx_ratio_settings(app_fsm);
        break;
    default:
        break;
    }
}

bool dcf77_navigation_callback(void* ctx) {
    AppFSM* app_fsm = ctx;

    if(app_fsm->screen == AppScreenTx) {
        dcf77_app_stop_tx(app_fsm);
        return true;
    }

    if(app_fsm->screen == AppScreenAdvancedMenu) {
        dcf77_app_switch_to_menu(app_fsm);
        return true;
    }

    if(app_fsm->screen == AppScreenLfSettings) {
        if(app_fsm->screen == AppScreenLfSettings && !dcf77_app_signal_can_run(app_fsm)) {
            notification_message_block(app_fsm->notification, &sequence_error);
            return true;
        }
        dcf77_app_switch_to_menu(app_fsm);
        return true;
    }

    if(app_fsm->screen == AppScreenExperimentalTimeSettings ||
       app_fsm->screen == AppScreenTxRatioSettings || app_fsm->screen == AppScreenSubGhzSettings ||
       app_fsm->screen == AppScreenDebugSettings) {
        dcf77_app_switch_to_advanced_menu(app_fsm);
        return true;
    }

    if(app_fsm->screen == AppScreenSubGhzFreqInput) {
        dcf77_app_switch_to_subghz_settings(app_fsm);
        return true;
    }

    if(app_fsm->screen == AppScreenPresetTimeInput) {
        dcf77_preset_time_input_previous_callback(app_fsm);
        return true;
    }

    view_dispatcher_stop(app_fsm->view_dispatcher);
    return true;
}

void dcf77_tick_callback(void* ctx) {
    AppFSM* app_fsm = ctx;
    const uint32_t now = furi_get_tick();

    if(app_fsm->screen == AppScreenStartup && now >= app_fsm->startup_deadline_tick) {
        dcf77_app_switch_to_menu(app_fsm);
    }

    if(dcf77_subghz_tx_timeout_expired(app_fsm, now)) {
        app_fsm->subghz_timeout_elapsed = true;
        dcf77_app_apply_rf_settings(app_fsm);
    }

    if(app_fsm->tx_active && app_fsm->output_dirty) {
        const bool output = app_fsm->output_state;
        app_fsm->output_dirty = false;
        dcf77_hw_indicator_set(app_fsm, output);
        dcf77_app_sound_set(app_fsm, app_fsm->speaker_value_index != 0 && output);
        dcf77_gpio_rf_sync_output(app_fsm);
        dcf77_subghz_sync_output(app_fsm);
    }

    if(app_fsm->tx_active && app_fsm->next_minute_prepare_pending) {
        dcf77_prepare_pending_minute(app_fsm);
    }

    if(app_fsm->tx_active) {
        if(app_fsm->current_signal == RadioClockSignalTest) {
            DateTime dt;
            furi_hal_rtc_get_datetime(&dt);
            if(dt.second != app_fsm->tx_progress_second) {
                app_fsm->tx_progress_second = dt.second;
                app_fsm->tx_second_start_tick = now;
            }
        } else if(app_fsm->bit_number != app_fsm->tx_progress_second) {
            app_fsm->tx_progress_second = app_fsm->bit_number;
            app_fsm->tx_second_start_tick = now;
        }
    }

    if(app_fsm->screen == AppScreenTx && now - app_fsm->last_tx_refresh_tick >= 500U) {
        app_fsm->last_tx_refresh_tick = now;
        with_view_model(
            app_fsm->tx_view, Dcf77AppViewModel * tx_model, { UNUSED(tx_model); }, true);
    }
}
