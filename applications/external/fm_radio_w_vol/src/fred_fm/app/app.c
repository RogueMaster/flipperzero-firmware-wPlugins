/**
 * @file app.c
 * @brief Flipper app entry, allocation, and teardown.
 */
#include "src/fred_fm/include/config.h"
#include "src/fred_fm/include/types.h"
#include "src/fred_fm/core/core.h"
#include "src/fred_fm/audio/audio.h"
#include "src/fred_fm/ui/ui.h"
#include "src/fred_fm/app/app.h"

#include <furi.h>
#include <gui/elements.h>
#include <notification/notification_messages.h>
#include "src/drivers/pam/PAM8406.h"
#ifdef ENABLE_RDS
#include "src/fred_fm/rds/rds.h"
#include "src/fred_fm/rds/capture.h"
#endif

/** Allocate GUI, views, timers; load SD state. */
FredFm* fred_fm_alloc() {
    FredFm* app = (FredFm*)malloc(sizeof(FredFm));
    if(!app) return NULL;
    memset(app, 0, sizeof(FredFm));

    pam8406_init();

    bool gui_opened = false;
    Gui* gui = furi_record_open(RECORD_GUI);
    if(!gui) goto fail;
    gui_opened = true;

    state_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!state_mutex) goto fail;

    fred_fm_presets_load();
    fred_fm_settings_load();

    app->view_dispatcher = view_dispatcher_alloc();
    if(!app->view_dispatcher) goto fail;
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);

    // Initialize submenu entries
    app->submenu = submenu_alloc();
    if(!app->submenu) goto fail;
    view_set_previous_callback(submenu_get_view(app->submenu), fred_fm_navigation_exit_callback);
    if(!fred_fm_submenu_rebuild(app)) goto fail;
    view_dispatcher_add_view(
        app->view_dispatcher, FredFmViewSubmenu, submenu_get_view(app->submenu));
    view_dispatcher_switch_to_view(app->view_dispatcher, FredFmViewSubmenu);

    // Initialize the variable item list for configuration
    app->variable_item_list_config = variable_item_list_alloc();
    if(!app->variable_item_list_config) goto fail;
    variable_item_list_reset(app->variable_item_list_config);

    // Add TEA5767 SNC toggle
    app->item_snc =
        variable_item_list_add(app->variable_item_list_config, "SNC", 2, fred_fm_snc_change, app);
    if(!app->item_snc) goto fail;
    variable_item_set_current_value_index(app->item_snc, tea_snc_enabled ? 1 : 0);
    variable_item_set_current_value_text(app->item_snc, tea_snc_enabled ? "On" : "Off");

    // Add TEA5767 de-emphasis time constant
    app->item_deemph = variable_item_list_add(
        app->variable_item_list_config, "De-emph", 2, fred_fm_deemph_change, app);
    if(!app->item_deemph) goto fail;
    variable_item_set_current_value_index(app->item_deemph, tea_deemph_75us ? 1 : 0);
    variable_item_set_current_value_text(app->item_deemph, tea_deemph_75us ? "75us" : "50us");

    // Add TEA5767 High Cut Control
    app->item_highcut = variable_item_list_add(
        app->variable_item_list_config, "HighCut", 2, fred_fm_highcut_change, app);
    if(!app->item_highcut) goto fail;
    variable_item_set_current_value_index(app->item_highcut, tea_highcut_enabled ? 1 : 0);
    variable_item_set_current_value_text(app->item_highcut, tea_highcut_enabled ? "On" : "Off");

    // Add TEA5767 Force mono
    app->item_mono = variable_item_list_add(
        app->variable_item_list_config, "Mono", 2, fred_fm_mono_change, app);
    if(!app->item_mono) goto fail;
    variable_item_set_current_value_index(app->item_mono, tea_force_mono_enabled ? 1 : 0);
    variable_item_set_current_value_text(app->item_mono, tea_force_mono_enabled ? "On" : "Off");

    // Keep backlight on while app runs
    app->item_backlight = variable_item_list_add(
        app->variable_item_list_config, "Backlight", 2, fred_fm_backlight_change, app);
    if(!app->item_backlight) goto fail;
    variable_item_set_current_value_index(app->item_backlight, backlight_keep_on ? 1 : 0);
    variable_item_set_current_value_text(app->item_backlight, backlight_keep_on ? "On" : "Off");

#ifdef ENABLE_RDS
    app->item_rds =
        variable_item_list_add(app->variable_item_list_config, "RDS", 2, fred_fm_rds_change, app);
    if(!app->item_rds) goto fail;
    variable_item_set_current_value_index(app->item_rds, rds_enabled ? 1 : 0);
    variable_item_set_current_value_text(app->item_rds, rds_enabled ? "On" : "Off");

    app->item_rds_debug = variable_item_list_add(
        app->variable_item_list_config, "RDS Debug", 2, fred_fm_rds_debug_change, app);
    if(!app->item_rds_debug) goto fail;
    variable_item_set_current_value_index(app->item_rds_debug, rds_debug_enabled ? 1 : 0);
    variable_item_set_current_value_text(app->item_rds_debug, rds_debug_enabled ? "On" : "Off");
#endif

    // Add volume configuration
    app->item_volume = variable_item_list_add(
        app->variable_item_list_config,
        "Volume",
        COUNT_OF(volume_values),
        fred_fm_volume_change,
        app);
    if(!app->item_volume) goto fail;
    uint8_t volume_index = 0;
    variable_item_set_current_value_index(app->item_volume, volume_index);

    app->item_pt_chip = variable_item_list_add(
        app->variable_item_list_config,
        "PT Chip",
        COUNT_OF(pt_chip_values),
        fred_fm_pt_chip_change,
        app);
    if(!app->item_pt_chip) goto fail;
    uint8_t chip_index = 0;
    for(uint8_t i = 0; i < COUNT_OF(pt_chip_values); i++) {
        if(pt_chip_values[i] == pt_chip) {
            chip_index = i;
            break;
        }
    }
    variable_item_set_current_value_index(app->item_pt_chip, chip_index);
    variable_item_set_current_value_text(app->item_pt_chip, pt_chip_names[chip_index]);

    app->item_amp_power = variable_item_list_add(
        app->variable_item_list_config,
        "Amp Power",
        COUNT_OF(amp_power_names),
        fred_fm_amp_power_change,
        app);
    if(!app->item_amp_power) goto fail;
    variable_item_set_current_value_index(app->item_amp_power, amp_power_enabled ? 1 : 0);
    variable_item_set_current_value_text(app->item_amp_power, amp_power_enabled ? "On" : "Off");

    app->item_amp_mode = variable_item_list_add(
        app->variable_item_list_config,
        "Amp Mode",
        COUNT_OF(amp_mode_names),
        fred_fm_amp_mode_change,
        app);
    if(!app->item_amp_mode) goto fail;
    variable_item_set_current_value_index(app->item_amp_mode, amp_mode_class_d ? 1 : 0);
    variable_item_set_current_value_text(app->item_amp_mode, amp_mode_class_d ? "D" : "AB");

    view_set_previous_callback(
        variable_item_list_get_view(app->variable_item_list_config),
        fred_fm_navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher,
        FredFmViewConfigure,
        variable_item_list_get_view(app->variable_item_list_config));

    // Initialize the Listen view
    app->listen_view = view_alloc();
    if(!app->listen_view) goto fail;
    view_set_draw_callback(app->listen_view, fred_fm_view_draw_callback);
    view_set_input_callback(app->listen_view, fred_fm_view_input_callback);
    view_set_previous_callback(app->listen_view, fred_fm_navigation_submenu_callback);
    view_set_context(app->listen_view, app);
    view_allocate_model(app->listen_view, ViewModelTypeLockFree, sizeof(FredFmViewModel));

    view_dispatcher_add_view(app->view_dispatcher, FredFmViewListen, app->listen_view);

#ifdef ENABLE_RDS
    app->constellation_view = view_alloc();
    if(!app->constellation_view) goto fail;
    view_set_draw_callback(app->constellation_view, fred_fm_constellation_view_draw_callback);
    view_set_input_callback(app->constellation_view, fred_fm_constellation_view_input_callback);
    view_set_previous_callback(app->constellation_view, fred_fm_navigation_submenu_callback);
    view_set_context(app->constellation_view, app);
    view_allocate_model(app->constellation_view, ViewModelTypeLockFree, sizeof(FredFmViewModel));
    view_dispatcher_add_view(
        app->view_dispatcher, FredFmViewConstellation, app->constellation_view);
#endif

    app->widget_about = widget_alloc();
    if(!app->widget_about) goto fail;
    widget_add_text_scroll_element(
        app->widget_about,
        0,
        0,
        128,
        64,
        "FReD FM\nVersion: " FRED_FM_UI_VERSION
        "\n---\nFlipper Radio Experimental Decoder\nFM + RDS Radio Board\nby pchmielewski1\n\n"
        "Left/Right (short) = Tune -/+ 0.1MHz\n"
        "Left/Right (hold) = Seek next/prev\n"
        "OK (short) = Mute audio\n"
        "OK (hold) = Save preset\n"
        "Up/Down (short) = Preset next/prev\n"
        "Up/Down (hold) = Volume\n\n"
        "Constellation view:\n"
        "Left/Right = Carrier offset -/+ 0.01Hz\n"
        "Up/Down = Preset next/prev\n"
        "OK = Save preset + offset\n"
        "OK (hold) = Dump RAW/meta\n\n"
        "Band: 76.0-108.0MHz\n\n"
#ifdef ENABLE_RDS
        "Config: SNC / De-emph / HighCut / Mono / Backlight / RDS / RDS Debug / Volume / PT / Amp\n"
#else
        "Config: SNC / De-emph / HighCut / Mono / Backlight / Volume / PT / Amp\n"
#endif
        "RDS offset is manual and saved per preset");
    view_set_previous_callback(
        widget_get_view(app->widget_about), fred_fm_navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, FredFmViewAbout, widget_get_view(app->widget_about));

    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    if(!app->notifications) goto fail;

    if(!fred_fm_submenu_rebuild(app)) goto fail;

#ifdef ENABLE_RDS
    fred_fm_rds_pipeline_start();
#endif

    // Apply backlight policy after loading settings
    fred_fm_apply_backlight(app->notifications);

    // Refresh config UI based on loaded settings
    if(app->item_snc) {
        variable_item_set_current_value_index(app->item_snc, tea_snc_enabled ? 1 : 0);
        variable_item_set_current_value_text(app->item_snc, tea_snc_enabled ? "On" : "Off");
    }
    if(app->item_volume) {
        variable_item_set_current_value_index(app->item_volume, current_volume ? 1 : 0);
        variable_item_set_current_value_text(
            app->item_volume, current_volume ? "Muted" : "Un-Muted");
    }
    if(app->item_pt_chip) {
        uint8_t chip_index = 0;
        for(uint8_t i = 0; i < COUNT_OF(pt_chip_values); i++) {
            if(pt_chip_values[i] == pt_chip) {
                chip_index = i;
                break;
            }
        }
        variable_item_set_current_value_index(app->item_pt_chip, chip_index);
        variable_item_set_current_value_text(app->item_pt_chip, pt_chip_names[chip_index]);
    }
    if(app->item_amp_power) {
        variable_item_set_current_value_index(app->item_amp_power, amp_power_enabled ? 1 : 0);
        variable_item_set_current_value_text(
            app->item_amp_power, amp_power_enabled ? "On" : "Off");
    }
    if(app->item_amp_mode) {
        variable_item_set_current_value_index(app->item_amp_mode, amp_mode_class_d ? 1 : 0);
        variable_item_set_current_value_text(app->item_amp_mode, amp_mode_class_d ? "D" : "AB");
    }
    if(app->item_deemph) {
        variable_item_set_current_value_index(app->item_deemph, tea_deemph_75us ? 1 : 0);
        variable_item_set_current_value_text(app->item_deemph, tea_deemph_75us ? "75us" : "50us");
    }
    if(app->item_highcut) {
        variable_item_set_current_value_index(app->item_highcut, tea_highcut_enabled ? 1 : 0);
        variable_item_set_current_value_text(
            app->item_highcut, tea_highcut_enabled ? "On" : "Off");
    }
    if(app->item_mono) {
        variable_item_set_current_value_index(app->item_mono, tea_force_mono_enabled ? 1 : 0);
        variable_item_set_current_value_text(
            app->item_mono, tea_force_mono_enabled ? "On" : "Off");
    }
    if(app->item_backlight) {
        variable_item_set_current_value_index(app->item_backlight, backlight_keep_on ? 1 : 0);
        variable_item_set_current_value_text(
            app->item_backlight, backlight_keep_on ? "On" : "Off");
    }
#ifdef ENABLE_RDS
    if(app->item_rds) {
        variable_item_set_current_value_index(app->item_rds, rds_enabled ? 1 : 0);
        variable_item_set_current_value_text(app->item_rds, rds_enabled ? "On" : "Off");
    }
    if(app->item_rds_debug) {
        variable_item_set_current_value_index(app->item_rds_debug, rds_debug_enabled ? 1 : 0);
        variable_item_set_current_value_text(
            app->item_rds_debug, rds_debug_enabled ? "On" : "Off");
    }
#endif

    // Give PT controllers time to settle after power-on before touching I2C.
    furi_delay_ms(200);
    (void)fred_fm_pt_refresh_state(true);
    fred_fm_apply_audio_output_state();

    // Start periodic background tick (I2C hot-plug, debounced saves)
    app->tick_timer = furi_timer_alloc(fred_fm_tick_callback, FuriTimerTypePeriodic, app);
    if(!app->tick_timer) goto fail;
    furi_timer_start(app->tick_timer, furi_ms_to_ticks(100));

#ifdef ENABLE_RDS
    fred_fm_rds_dsp_worker_start();
    app->rds_adc_timer =
        furi_timer_alloc(fred_fm_rds_adc_timer_callback, FuriTimerTypePeriodic, app);
    if(!app->rds_adc_timer) goto fail;
    rds_adc_timer_handle = app->rds_adc_timer;
    if(fred_fm_rds_pipeline_enabled()) {
        fred_fm_rds_apply_runtime_state(true);
    }
#endif

    return app;

fail:
    fred_fm_audio_shutdown();

    if(app) {
        if(app->tick_timer) {
            furi_timer_stop(app->tick_timer);
            furi_timer_free(app->tick_timer);
            app->tick_timer = NULL;
        }
#ifdef ENABLE_RDS
        if(app->rds_adc_timer) {
            furi_timer_stop(app->rds_adc_timer);
            furi_timer_free(app->rds_adc_timer);
            app->rds_adc_timer = NULL;
        }
        rds_adc_timer_running = false;
        rds_adc_timer_handle = NULL;
        fred_fm_rds_capture_stop();
        fred_fm_rds_capture_writer_stop();
        fred_fm_rds_adc_stop();
#endif
        if(app->notifications) {
            notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
            furi_record_close(RECORD_NOTIFICATION);
            app->notifications = NULL;
        }
        if(app->widget_about) {
            if(app->view_dispatcher)
                view_dispatcher_remove_view(app->view_dispatcher, FredFmViewAbout);
            widget_free(app->widget_about);
            app->widget_about = NULL;
        }
        if(app->listen_view) {
            if(app->view_dispatcher)
                view_dispatcher_remove_view(app->view_dispatcher, FredFmViewListen);
            view_free(app->listen_view);
            app->listen_view = NULL;
        }
#ifdef ENABLE_RDS
        if(app->constellation_view) {
            if(app->view_dispatcher)
                view_dispatcher_remove_view(app->view_dispatcher, FredFmViewConstellation);
            view_free(app->constellation_view);
            app->constellation_view = NULL;
        }
#endif
        if(app->variable_item_list_config) {
            if(app->view_dispatcher)
                view_dispatcher_remove_view(app->view_dispatcher, FredFmViewConfigure);
            variable_item_list_free(app->variable_item_list_config);
            app->variable_item_list_config = NULL;
        }
        if(app->submenu) {
            if(app->view_dispatcher)
                view_dispatcher_remove_view(app->view_dispatcher, FredFmViewSubmenu);
            submenu_free(app->submenu);
            app->submenu = NULL;
        }
        if(app->view_dispatcher) {
            view_dispatcher_free(app->view_dispatcher);
            app->view_dispatcher = NULL;
        }
    }

    if(state_mutex) {
        furi_mutex_free(state_mutex);
        state_mutex = NULL;
    }

    if(gui_opened) {
        furi_record_close(RECORD_GUI);
    }
    free(app);
    return NULL;
}

/**
 * Single shutdown path after @c view_dispatcher_run() returns.
 * Order: stop timers (do not free — late callbacks use @c fred_fm_app_exiting),
 * join RDS workers, stop ADC/DMA, mute hardware, tear down GUI.
 */
void fred_fm_free(FredFm* app) {
    if(!app) return;

    FURI_LOG_I(TAG, "free: enter (unified shutdown)");

    fred_fm_app_exiting = true;

    /* Stop only — furi_timer_free() can race with queued callbacks. */
#ifdef ENABLE_RDS
    if(app->rds_adc_timer) {
        furi_timer_stop(app->rds_adc_timer);
        app->rds_adc_timer = NULL;
    }
    rds_adc_timer_handle = NULL;
    rds_adc_timer_running = false;
#endif

    if(app->tick_timer) {
        furi_timer_stop(app->tick_timer);
        app->tick_timer = NULL;
    }
    FURI_LOG_I(TAG, "free: timers stopped (not freed)");

    furi_delay_ms(200);
    furi_delay_ms(50);
    FURI_LOG_I(TAG, "free: timer callback drain complete");

#ifdef ENABLE_RDS
    if(rds_dsp_worker_thread) {
        FuriThreadId dsp_wid = rds_dsp_worker_thread_id;
        rds_dsp_worker_thread_id = NULL;
        if(dsp_wid) {
            furi_thread_flags_set(dsp_wid, RDS_DSP_WORKER_FLAG_STOP);
        }
        furi_thread_join(rds_dsp_worker_thread);
        furi_thread_free(rds_dsp_worker_thread);
        rds_dsp_worker_thread = NULL;
    }

    fred_fm_rds_capture_writer_stop();
    FURI_LOG_I(TAG, "free: DSP worker + capture writer stopped");

    rds_pipeline_stopping = true;
    furi_delay_ms(20);
    fred_fm_rds_capture_stop();
    fred_fm_rds_adc_stop();
    FURI_LOG_I(TAG, "free: RDS pipeline stopped");
#endif

    FURI_LOG_I(TAG, "free: entering audio shutdown");
    fred_fm_audio_shutdown();
    FURI_LOG_I(TAG, "free: audio shutdown done");

    if(app && app->notifications) {
        FURI_LOG_I(TAG, "free: restoring backlight");
        notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
        furi_record_close(RECORD_NOTIFICATION);
        app->notifications = NULL;
        FURI_LOG_I(TAG, "free: notification closed");
    }

    FURI_LOG_I(TAG, "free: starting GUI teardown");
    if(app && app->widget_about) {
        FURI_LOG_I(TAG, "free: removing About view");
        if(app->view_dispatcher)
            view_dispatcher_remove_view(app->view_dispatcher, FredFmViewAbout);
        widget_free(app->widget_about);
        app->widget_about = NULL;
        FURI_LOG_I(TAG, "free: About view freed");
    }
    if(app && app->listen_view) {
        FURI_LOG_I(TAG, "free: removing Listen view");
        if(app->view_dispatcher)
            view_dispatcher_remove_view(app->view_dispatcher, FredFmViewListen);
        view_free(app->listen_view);
        app->listen_view = NULL;
        FURI_LOG_I(TAG, "free: Listen view freed");
    }
#ifdef ENABLE_RDS
    if(app && app->constellation_view) {
        FURI_LOG_I(TAG, "free: removing Constellation view");
        if(app->view_dispatcher)
            view_dispatcher_remove_view(app->view_dispatcher, FredFmViewConstellation);
        view_free(app->constellation_view);
        app->constellation_view = NULL;
        FURI_LOG_I(TAG, "free: Constellation view freed");
    }
#endif
    if(app && app->variable_item_list_config) {
        FURI_LOG_I(TAG, "free: removing Config view");
        if(app->view_dispatcher)
            view_dispatcher_remove_view(app->view_dispatcher, FredFmViewConfigure);
        variable_item_list_free(app->variable_item_list_config);
        app->variable_item_list_config = NULL;
        FURI_LOG_I(TAG, "free: Config view freed");
    }
    if(app && app->submenu) {
        FURI_LOG_I(TAG, "free: removing Submenu view");
        if(app->view_dispatcher)
            view_dispatcher_remove_view(app->view_dispatcher, FredFmViewSubmenu);
        submenu_free(app->submenu);
        app->submenu = NULL;
        FURI_LOG_I(TAG, "free: Submenu freed");
    }
    if(app && app->view_dispatcher) {
        FURI_LOG_I(TAG, "free: freeing view dispatcher");
        view_dispatcher_free(app->view_dispatcher);
        app->view_dispatcher = NULL;
        FURI_LOG_I(TAG, "free: view dispatcher freed");
    }
    if(app) {
        furi_record_close(RECORD_GUI);
        FURI_LOG_I(TAG, "free: GUI record closed");
    }

    free(app);
    FURI_LOG_I(TAG, "free: done");
}

/** Flipper app entry — runs view dispatcher then @ref fred_fm_free. */
int32_t fred_fm_app(void* p) {
    UNUSED(p);

    FredFm* app = fred_fm_alloc();
    if(!app) {
        FURI_LOG_E(TAG, "App allocation failed");
        return -1;
    }
    view_dispatcher_run(app->view_dispatcher);

    fred_fm_free(app);
    return 0;
}
