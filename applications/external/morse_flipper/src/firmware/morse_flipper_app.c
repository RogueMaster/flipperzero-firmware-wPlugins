/*
 * Purpose: Allocate, initialise, and tear down the Morse Flipper application.
 * Owns: top-level object lifetime and Flipper record/module allocation.
 * Depends on: morse_flipper_app_i.h and Flipper GUI/storage services.
 * Tests: firmware build; runtime behaviour is hardware-only.
 */

#include "morse_flipper_app_i.h"

#include <stdlib.h>

MorseFlipperApp* morse_flipper_boot(void) {
    MorseFlipperApp* app = malloc(sizeof(MorseFlipperApp));
    if(app == NULL) return NULL;

    *app = (MorseFlipperApp){
        .q = NULL,
        .view_port = NULL,
        .view_dispatcher = NULL,
        .scene_manager = NULL,
        .submenu = NULL,
        .text_input = NULL,
        .settings_list = NULL,
        .live_view = NULL,
        .gui = furi_record_open(RECORD_GUI),
        .dialogs = furi_record_open(RECORD_DIALOGS),
        .notifications = furi_record_open(RECORD_NOTIFICATION),
        .help_text = NULL,
        .exit_requested = false,
        .previous_usb_config = NULL,
        .hid_cfg =
            {
                .vid = 0x6666U,
                .pid = 0x434BU,
                .manuf = "YO3GND",
                .product = "Morse Flipper Kbd",
            },
        .tone_on = false,
        .signal_led_on = false,
        .signal_led_red = false,
        .signal_led_green = false,
        .speaker_owned = false,
        .speaker_busy = false,
        .transport_connected = false,
        .mouse_invert = false,
        .left_down = false,
        .ok_down = false,
        .back_down = false,
        .trainer_playback_active = false,
        .trainer_playback_mark = false,
        .session_started = false,
        .session_round_pending = false,
        .session_result_hold = false,
        .session_result_tone = false,
        .session_result_good = false,
        .session_start_holdoff = false,
        .about_show_next = false,
        .help_chapter_card = false,
        .midi_rx_pending = false,
        .screen = MorseFlipperScreenMenu,
        .scene = MorseFlipperSceneMenuMain,
        .pc_mode = MorseFlipperPcModeOff,
        .pc_mode_pref = MorseFlipperPcModeOff,
        .pc_paddle_preset = 0U,
        .pc_straight_preset = 0U,
        .handedness = MorseFlipperHandednessSwapped,
        .input_source = MorseFlipperInputSourceButtons,
        .audio_path = MorseFlipperAudioPathBuzzer,
        .keyer_mode = MorseKeyerModeStraight,
        .gpio_straight_idx = MorseFlipperGpioPinP7,
        .gpio_dit_idx = MorseFlipperGpioPinP7,
        .gpio_dah_idx = MorseFlipperGpioPinP5,
        .gpio_ground_idx = MorseFlipperGpioPinP3,
        .gpio_ptt_idx = MORSE_FLIPPER_GPIO_PIN_NONE,
        .gpio_edit_dit_idx = MorseFlipperGpioPinP7,
        .gpio_edit_dah_idx = MorseFlipperGpioPinP5,
        .gpio_edit_ground_idx = MorseFlipperGpioPinP3,
        .gpio_edit_ptt_idx = MORSE_FLIPPER_GPIO_PIN_NONE,
        .gpio_probe_state = MorseFlipperGpioProbeOk,
        .startup_gpio_probe_state = MorseFlipperGpioProbeOk,
        .straight_answer_timeout_s = MORSE_FLIPPER_STRAIGHT_TIMEOUT_DEFAULT_S,
        .straight_next_delay_s = MORSE_FLIPPER_STRAIGHT_NEXT_DEFAULT_S,
        .trainer_row = 0U,
        .help_card_count = 1U,
        .trainer_char_idx = 0U,
        .trainer_mark_idx = 0U,
        .session_wait_draw_s = 0xFFU,
        .about_mode = 0U,
        .about_ok_count = 0U,
        .about_social_idx = 0U,
        .about_footer_seq_i = 0U,
        .about_last_ok_ms = 0U,
        .about_social_next_ms = 0U,
        .help_md = {0},
        .about_md = {0},
        .ham =
            {
                .selected_message = 0U,
                .text_mode = MorseFlipperHamTextModeNone,
                .key_level = false,
                .macro_active = false,
                .macro_mark = false,
                .macro_char_idx = 0U,
                .macro_mark_idx = 0U,
                .macro_dir = MORSE_FLIPPER_HAM_KEYER_UNASSIGNED,
                .wpm_hold_key = MORSE_FLIPPER_HAM_WPM_HOLD_NONE,
                .macro_next_at = 0U,
                .notice_until = 0U,
                .wpm_hold_next_at = 0U,
                .text_buffer = {0},
                .macro_text = {0},
                .notice = {0},
            },
        .rf_freq_focus = 0U,
        .vail_mode_active = false,
        .vail_speed_active = false,
        .vail_tone_active = false,
        .vail_keyer_mode = MorseKeyerModeStraight,
        .vail_dit_ms = MORSE_FLIPPER_DEFAULT_DIT_MS,
        .vail_tone_idx = MORSE_FLIPPER_DEFAULT_TONE_IDX,
        .keyer = {0},
        .tone_idx = MORSE_FLIPPER_DEFAULT_TONE_IDX,
        .p2_volume_pct = 100U,
        .preview_ticks = 0U,
        .input_mask = 0U,
        .trainer_next_at = 0U,
        .straight_next_at = 0U,
        .straight_wait_started_at = 0U,
        .straight_answer_started_at = 0U,
        .straight_last_input_at = 0U,
        .straight_mark_started_at = 0U,
        .txg_wait_started_at = 0U,
        .txg_last_input_at = 0U,
        .txg_result_open_at = 0U,
        .txg_result_until = 0U,
        .txg_result_draw_s = 0xFFU,
        .run_dit_ms = 0U,
        .straight_dit_ms = MORSE_FLIPPER_DEFAULT_DIT_MS,
        .straight_session_total = 0U,
        .straight_session_good = 0U,
        .txg_session_total = 0U,
        .txg_session_good = 0U,
        .txg_session_sk = 0U,
        .txg_sum_speed = 0U,
        .txg_sum_lgap = 0U,
        .txg_sum_ratio = 0U,
        .txg_sum_accuracy = 0U,
        .txg_sum_dgap = 0U,
        .txg_sum_variance = 0U,
        .session_last_input_at = 0U,
        .session_answer_complete_at = 0U,
        .session_result_until = 0U,
        .session_next_group_at = 0U,
        .rf_tx_tail_until = 0U,
        .rf_tx_edge_at = 0U,
        .rf_rx_edge_at = 0U,
        .rf_rx_sample_next_at = 0U,
        .rf_rx_view_next_at = 0U,
        .rf_rssi_next_at = 0U,
        .rf_rssi_peak_decay_at = 0U,
        .gpio_edge_at = 0U,
        .gpio_probe_notice_until = 0U,
        .ptt_tail_until = 0U,
        .rf_edit_khz = MORSE_FLIPPER_RF_DEFAULT_FREQUENCY_KHZ,
        .rf_rssi_sum_dbm = 0,
        .paddle_sources = {0U, 0U},
        .note_sources = {0U, 0U, 0U},
        .trainer = {0},
        .ham_keyer = {0},
        .custom_sets = {0},
        .custom_sets_loaded = false,
        .straight_playback_active = false,
        .straight_playback_mark = false,
        .straight_started = false,
        .straight_wait_answer = false,
        .straight_done = false,
        .straight_key_down = false,
        .straight_cutoff_wait_release = false,
        .straight_next_draw_s = 0xFFU,
        .txg_started = false,
        .txg_wait_answer = false,
        .txg_done = false,
        .txg_sk = false,
        .txg_start_holdoff = false,
        .txg_difficulty = MorseFlipperTxgDifficultyCompetition,
        .rf_live_active = false,
        .rf_tx_level = false,
        .rf_rx_level = false,
        .rf_rx_candidate_level = false,
        .rf_tx_gap_flushed = true,
        .rf_rx_gap_flushed = true,
        .rf_carrier_present = false,
        .rf_monitor_tone = false,
        .rf_rx_audio_enabled = true,
        .ptt_level = false,
        .gpio_level = false,
        .gpio_gap_flushed = true,
        .straight_mark_idx = 0U,
        .txg_repeated_timeouts = 0U,
        .backlight_mode = MorseFlipperBacklightAuto,
        .rf_monitor_threshold_dbm = -95,
        .rf_rssi_samples = 0U,
        .rf_rx_edges_window = 0U,
        .rf_rx_activity = 0U,
        .rf_rx_candidate_samples = 0U,
        .rf_rx_wpm_hint = MORSE_FLIPPER_RF_RX_DEFAULT_WPM,
        .rf_rx_text = {0},
        .rf_tx_text = {0},
        .gpio_text = {0},
        .rf = {0},
        .rf_rx_ticker = {.marks = {{0}}, .start = 0U, .count = 0U},
        .radio = {0},
        .rf_decoder = {0},
        .tx_decoder = {0},
        .gpio_decoder = {0},
        .straight_trainer = {0},
    };

    morse_trainer_init(&app->trainer);
    morse_flipper_audio_pwm_reset(&app->audio_pwm);
    morse_flipper_rf_init(&app->rf);
    morse_flipper_radio_init(&app->radio);
    morse_flipper_ham_keyer_reset(&app->ham_keyer);
    morse_flipper_radio_set_rx_callback(&app->radio, morse_flipper_rf_rx_edge, app);
    morse_flipper_cw_decoder_init(&app->rf_decoder, morse_flipper_current_dit_ms(app));
    morse_flipper_cw_decoder_init(&app->tx_decoder, morse_flipper_current_dit_ms(app));
    morse_flipper_cw_decoder_init(&app->gpio_decoder, morse_flipper_current_dit_ms(app));
    morse_flipper_run_history_reset(&app->run_history);
    morse_flipper_straight_trainer_init(&app->straight_trainer);
    morse_flipper_tx_group_init(&app->tx_group);
    morse_trainer_set_seed(&app->trainer, furi_hal_random_get());
    morse_flipper_straight_trainer_set_seed(&app->straight_trainer, furi_hal_random_get());
    morse_flipper_tx_group_set_seed(&app->tx_group, furi_hal_random_get());
    morse_flipper_load_config(app);
    morse_flipper_apply_trainer_charset_choice(app);
    morse_flipper_cw_decoder_init(&app->rf_decoder, morse_flipper_current_dit_ms(app));
    morse_flipper_cw_decoder_init(&app->tx_decoder, morse_flipper_current_dit_ms(app));
    morse_flipper_cw_decoder_init(&app->gpio_decoder, morse_flipper_current_dit_ms(app));
    morse_keyer_init(&app->keyer, app->keyer_mode, morse_flipper_current_dit_ms(app));
    morse_flipper_gpio_init(app);
    app->startup_gpio_probe_state = morse_flipper_gpio_probe_sample_raw(app);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, morse_flipper_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, morse_flipper_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, morse_flipper_tick_callback, MORSE_FLIPPER_POLL_MS);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->scene_manager = scene_manager_alloc(&morse_flipper_scene_handlers, app);
    morse_flipper_ensure_view(app, MorseFlipperViewMenu);

    if(morse_flipper_gpio_probe_any_short(app->startup_gpio_probe_state)) {
        scene_manager_next_scene(app->scene_manager, MorseFlipperSceneStartupProbe);
    } else {
        scene_manager_next_scene(app->scene_manager, MorseFlipperSceneMenuMain);
    }
    return app;
}

ViewDispatcher* morse_flipper_view_dispatcher_get(MorseFlipperApp* app) {
    if(app == NULL) return NULL;
    return app->view_dispatcher;
}

void morse_flipper_ensure_view(MorseFlipperApp* app, uint8_t view) {
    if(app == NULL || app->view_dispatcher == NULL) return;

    if(view == MorseFlipperViewMenu) {
        if(app->submenu != NULL) return;
        app->submenu = submenu_alloc();
        view_dispatcher_add_view(
            app->view_dispatcher, MorseFlipperViewMenu, submenu_get_view(app->submenu));
    } else if(view == MorseFlipperViewTextInput) {
        if(app->text_input != NULL) return;
        app->text_input = text_input_alloc();
        view_dispatcher_add_view(
            app->view_dispatcher, MorseFlipperViewTextInput, text_input_get_view(app->text_input));
    } else if(view == MorseFlipperViewSettings) {
        if(app->settings_list != NULL) return;
        app->settings_list = variable_item_list_alloc();
        view_dispatcher_add_view(
            app->view_dispatcher,
            MorseFlipperViewSettings,
            variable_item_list_get_view(app->settings_list));
    } else if(view == MorseFlipperViewLive) {
        if(app->live_view != NULL) return;
        app->live_view = view_alloc();
        view_set_context(app->live_view, app);
        view_allocate_model(app->live_view, ViewModelTypeLockFree, sizeof(MorseFlipperLiveModel));
        with_view_model(
            app->live_view,
            MorseFlipperLiveModel * m,
            {
                m->app = app;
                m->bump = 0U;
            },
            false);
        view_set_draw_callback(app->live_view, morse_flipper_live_draw);
        view_set_input_callback(app->live_view, morse_flipper_live_input);
        view_dispatcher_add_view(app->view_dispatcher, MorseFlipperViewLive, app->live_view);
    }
}

void morse_flipper_shutdown(MorseFlipperApp* app) {
    if(app == NULL) return;

    if(app->screen == MorseFlipperScreenHamRun) {
        morse_flipper_ham_log_flush(app);
        morse_flipper_ham_stop_macro(app);
        morse_flipper_ham_gpio_release(app);
    }
    morse_flipper_clear_button_keying(app, furi_get_tick());
    morse_flipper_set_pc_mode(app, MorseFlipperPcModeOff);
    morse_flipper_radio_sync_live(
        &app->radio,
        morse_flipper_rf_frequency_hz(&app->rf),
        false,
        false,
        MorseFlipperRadioProfileOokData);
    morse_flipper_radio_set_tx_level(&app->radio, false);
    morse_flipper_radio_deinit(&app->radio);
    morse_keyer_reset(&app->keyer);
    morse_flipper_drain_keyer_events(app);
    morse_flipper_release_all_notes(app);
    morse_flipper_audio_pwm_stop(&app->audio_pwm);
    morse_flipper_tone_stop(app);
    morse_flipper_sync_signal_led(app, false);
    furi_hal_vibro_on(false);
    if(app->backlight_mode != MorseFlipperBacklightAuto && app->notifications)
        notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
    if(app->notifications) {
        notification_message(app->notifications, &sequence_reset_green);
        notification_message(app->notifications, &sequence_reset_red);
        notification_message(app->notifications, &sequence_reset_blue);
    }
    morse_flipper_save_config(app);

    morse_flipper_gpio_deinit();
    if(app->view_dispatcher) {
        if(app->text_input)
            view_dispatcher_remove_view(app->view_dispatcher, MorseFlipperViewTextInput);
        if(app->settings_list)
            view_dispatcher_remove_view(app->view_dispatcher, MorseFlipperViewSettings);
        if(app->live_view) view_dispatcher_remove_view(app->view_dispatcher, MorseFlipperViewLive);
        if(app->submenu) view_dispatcher_remove_view(app->view_dispatcher, MorseFlipperViewMenu);
    }
    if(app->help_text) furi_string_free(app->help_text);
    if(app->text_input) text_input_free(app->text_input);
    if(app->settings_list) variable_item_list_free(app->settings_list);
    if(app->live_view) view_free(app->live_view);
    if(app->submenu) submenu_free(app->submenu);
    if(app->scene_manager) scene_manager_free(app->scene_manager);
    if(app->view_dispatcher) view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    free(app);
}
