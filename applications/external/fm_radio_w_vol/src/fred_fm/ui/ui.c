/**
 * @file ui.c
 * @brief Views, input handling, and periodic tick.
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
#include "src/drivers/tea5767/TEA5767.h"
#ifdef ENABLE_RDS
#include "src/fred_fm/rds/rds.h"
#include "src/fred_fm/rds/capture.h"
#endif

#include "src/fred_fm/app/app.h"

/** Back on root submenu — only sets exit flag; teardown is in @ref fred_fm_free. */
uint32_t fred_fm_navigation_exit_callback(void* context) {
    UNUSED(context);
    fred_fm_app_exiting = true;
    return VIEW_NONE;
}

/** Previous-view handler: return to root submenu. */
uint32_t fred_fm_navigation_submenu_callback(void* context) {
    UNUSED(context);
#ifdef ENABLE_RDS
    rds_constellation_view_active = false;
#endif
    return FredFmViewSubmenu;
}

void fred_fm_redraw_listen_view(View* view) {
    if(view) {
        view_get_model(view);
        view_commit_model(view, true);
    }
}

void fred_fm_redraw_constellation_view(View* view) {
    if(view) {
        view_get_model(view);
        view_commit_model(view, true);
    }
}

/** Rebuild submenu items (e.g. after RDS Debug toggle). */
bool fred_fm_submenu_rebuild(FredFm* app) {
    if(!app || !app->submenu) {
        return false;
    }

    submenu_reset(app->submenu);

    submenu_add_item(
        app->submenu, "Listen Now", FredFmSubmenuIndexListen, fred_fm_submenu_callback, app);
#ifdef ENABLE_RDS
    if(rds_debug_enabled) {
        submenu_add_item(
            app->submenu,
            "Constellation Visualizer",
            FredFmSubmenuIndexConstellation,
            fred_fm_submenu_callback,
            app);
    }
#endif
    submenu_add_item(
        app->submenu, "Config", FredFmSubmenuIndexConfigure, fred_fm_submenu_callback, app);
    submenu_add_item(
        app->submenu, "About", FredFmSubmenuIndexAbout, fred_fm_submenu_callback, app);

    return true;
}

/** Submenu item selection → switch view. */
void fred_fm_submenu_callback(void* context, uint32_t index) {
    FredFm* app = (FredFm*)context;
    switch(index) {
    case FredFmSubmenuIndexListen:
#ifdef ENABLE_RDS
        rds_constellation_view_active = false;
#endif
        view_dispatcher_switch_to_view(app->view_dispatcher, FredFmViewListen);
        break;
#ifdef ENABLE_RDS
    case FredFmSubmenuIndexConstellation:
        rds_constellation_view_active = true;
        fred_fm_rds_constellation_clear_history();
        view_dispatcher_switch_to_view(app->view_dispatcher, FredFmViewConstellation);
        break;
#endif
    case FredFmSubmenuIndexConfigure:
#ifdef ENABLE_RDS
        rds_constellation_view_active = false;
#endif
        view_dispatcher_switch_to_view(app->view_dispatcher, FredFmViewConfigure);
        break;
    case FredFmSubmenuIndexAbout:
#ifdef ENABLE_RDS
        rds_constellation_view_active = false;
#endif
        view_dispatcher_switch_to_view(app->view_dispatcher, FredFmViewAbout);
        break;
    default:
        break;
    }
}

/** Listen view: tune, seek, presets, mute, capture trigger. */
bool fred_fm_view_input_callback(InputEvent* event, void* context) {
    FredFm* app = (FredFm*)context;
    if(event->type == InputTypeLong && event->key == InputKeyLeft) {
        fred_fm_seek_step(false);
        fred_fm_redraw_listen_view(app ? app->listen_view : NULL);
        return true;
    } else if(event->type == InputTypeLong && event->key == InputKeyRight) {
        fred_fm_seek_step(true);
        fred_fm_redraw_listen_view(app ? app->listen_view : NULL);
        return true;
    } else if(event->type == InputTypeShort && event->key == InputKeyLeft) {
        // Use integer 10kHz math to avoid PLL quantization drift
        uint32_t fq = fred_fm_get_current_freq_10khz();
        fq = ((fq + 5) / 10) * 10;
        if(fq > 10)
            fq -= 10;
        else
            fq = 7600;
        fq = clamp_u32(fq, 7600U, 10800U);
        fred_fm_tune_nominal_freq_10khz(fq);
        fred_fm_rds_on_tuned_frequency_changed();
        fred_fm_redraw_listen_view(app ? app->listen_view : NULL);
        return true;
    } else if(event->type == InputTypeShort && event->key == InputKeyRight) {
        // Use integer 10kHz math to avoid PLL quantization drift
        uint32_t fq = fred_fm_get_current_freq_10khz();
        fq = ((fq + 5) / 10) * 10;
        fq += 10;
        fq = clamp_u32(fq, 7600U, 10800U);
        fred_fm_tune_nominal_freq_10khz(fq);
        fred_fm_rds_on_tuned_frequency_changed();
        fred_fm_redraw_listen_view(app ? app->listen_view : NULL);
        return true;
    } else if(event->type == InputTypeLong && event->key == InputKeyOk) {
        uint32_t freq_10khz = fred_fm_get_current_freq_10khz();
        int16_t offset_centihz = 0;
#ifdef ENABLE_RDS
        offset_centihz = fred_fm_rds_get_manual_offset_centihz();
#endif
        fred_fm_presets_add_or_select(freq_10khz, offset_centihz);
        fred_fm_presets_save();
        fred_fm_feedback_success();
        fred_fm_redraw_listen_view(app ? app->listen_view : NULL);
        return true;
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        fred_fm_state_lock();
        current_volume = !current_volume;
        fred_fm_state_unlock();
        fred_fm_apply_audio_output_state();
        fred_fm_settings_mark_dirty();
        fred_fm_redraw_listen_view(app ? app->listen_view : NULL);
        return true; // Event was handled
    } else if(event->type == InputTypeShort && event->key == InputKeyUp) {
        (void)fred_fm_presets_step_and_apply(true);
        fred_fm_redraw_listen_view(app ? app->listen_view : NULL);
        return true; // Event was handled
    } else if(event->type == InputTypeShort && event->key == InputKeyDown) {
        (void)fred_fm_presets_step_and_apply(false);
        fred_fm_redraw_listen_view(app ? app->listen_view : NULL);
        return true; // Event was handled
    } else if(
        (event->type == InputTypeLong || event->type == InputTypeRepeat) &&
        event->key == InputKeyUp) {
        // Volume up => reduce attenuation
        fred_fm_state_lock();
        if(pt_atten_db > 0) {
            pt_atten_db--;
            fred_fm_state_unlock();
            fred_fm_apply_audio_output_state();
            fred_fm_settings_mark_dirty();
            fred_fm_redraw_listen_view(app ? app->listen_view : NULL);
        } else {
            fred_fm_state_unlock();
        }
        return true;
    } else if(
        (event->type == InputTypeLong || event->type == InputTypeRepeat) &&
        event->key == InputKeyDown) {
        // Volume down => increase attenuation
        fred_fm_state_lock();
        if(pt_atten_db < 79) {
            pt_atten_db++;
            fred_fm_state_unlock();
            fred_fm_apply_audio_output_state();
            fred_fm_settings_mark_dirty();
            fred_fm_redraw_listen_view(app ? app->listen_view : NULL);
        } else {
            fred_fm_state_unlock();
        }
        return true;
    }

    return false; // Event was not handled
}

#ifdef ENABLE_RDS
/** Constellation view: carrier offset and preset controls. */
bool fred_fm_constellation_view_input_callback(InputEvent* event, void* context) {
    FredFm* app = (FredFm*)context;

    if(event->type == InputTypeShort && event->key == InputKeyLeft) {
        int16_t offset_centihz = fred_fm_rds_get_manual_offset_centihz();
        fred_fm_rds_set_manual_offset_centihz((int16_t)(offset_centihz - 1));
        fred_fm_redraw_constellation_view(app ? app->constellation_view : NULL);
        return true;
    } else if(event->type == InputTypeShort && event->key == InputKeyRight) {
        int16_t offset_centihz = fred_fm_rds_get_manual_offset_centihz();
        fred_fm_rds_set_manual_offset_centihz((int16_t)(offset_centihz + 1));
        fred_fm_redraw_constellation_view(app ? app->constellation_view : NULL);
        return true;
    } else if(event->type == InputTypeShort && event->key == InputKeyUp) {
        (void)fred_fm_presets_step_and_apply(true);
        fred_fm_redraw_constellation_view(app ? app->constellation_view : NULL);
        return true;
    } else if(event->type == InputTypeShort && event->key == InputKeyDown) {
        (void)fred_fm_presets_step_and_apply(false);
        fred_fm_redraw_constellation_view(app ? app->constellation_view : NULL);
        return true;
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        uint32_t freq_10khz = fred_fm_get_current_freq_10khz();
        int16_t offset_centihz = fred_fm_rds_get_manual_offset_centihz();
        fred_fm_presets_add_or_select(freq_10khz, offset_centihz);
        fred_fm_presets_save();

        fred_fm_state_lock();
        rds_constellation_saved_until_tick = furi_get_tick() + furi_ms_to_ticks(1500U);
        fred_fm_state_unlock();

        fred_fm_feedback_success();
        fred_fm_redraw_constellation_view(app ? app->constellation_view : NULL);
        return true;
    } else if(event->type == InputTypeLong && event->key == InputKeyOk) {
#if ENABLE_ADC_CAPTURE
        if(!rds_capture_active && !rds_capture_finalize_pending) {
            rds_capture_requested = true;
            fred_fm_feedback_success();
        }
#endif
        fred_fm_redraw_constellation_view(app ? app->constellation_view : NULL);
        return true;
    }

    return false;
}

/** Draw RDS constellation debug plot. */
void fred_fm_constellation_view_draw_callback(Canvas* canvas, void* model) {
    (void)model;

    char freq_display[20];
    char offset_display[24];
    char status_display[32];
    char rds_ps_local[RDS_PS_LEN + 1U] = {0};

    uint32_t local_freq_10khz = 0U;
    int16_t local_offset_centihz = 0;
    uint8_t local_hist_count = 0U;
    uint8_t local_hist_write_index = 0U;
    bool local_saved_active = false;
    bool local_status_visible = false;
#if ENABLE_ADC_CAPTURE
    bool local_capture_active = false;
    bool local_capture_finalize_pending = false;
    bool local_capture_complete = false;
    bool local_capture_error = false;
    bool local_capture_partial = false;
    uint32_t local_capture_captured_blocks = 0U;
    uint32_t local_capture_written_blocks = 0U;
    uint32_t local_capture_target_blocks = 0U;
#endif

    fred_fm_state_lock();
    local_freq_10khz = tea_nominal_freq_10khz;
    local_offset_centihz = rds_carrier_offset_centihz;
    memcpy(rds_ps_local, rds_ps_display, sizeof(rds_ps_local));
    local_saved_active = (rds_constellation_saved_until_tick > furi_get_tick());
#if ENABLE_ADC_CAPTURE
    local_capture_active = rds_capture_active;
    local_capture_finalize_pending = rds_capture_finalize_pending;
    local_capture_complete = rds_capture_complete;
    local_capture_error = rds_capture_error;
    local_capture_captured_blocks = rds_capture_captured_blocks;
    local_capture_written_blocks = rds_capture_written_blocks;
    local_capture_target_blocks = rds_capture_target_blocks;
    local_capture_partial = !local_capture_active && !local_capture_finalize_pending &&
                            !local_capture_complete && (local_capture_written_blocks > 0U);
#endif
    fred_fm_state_unlock();

    FURI_CRITICAL_ENTER();
    local_hist_count = rds_constellation_history_count;
    local_hist_write_index = rds_constellation_history_write_index;
    for(uint8_t i = 0U; i < local_hist_count; i++) {
        uint32_t idx = (uint32_t)(local_hist_write_index + RDS_CONSTELLATION_HISTORY_LEN -
                                  local_hist_count + i) %
                       RDS_CONSTELLATION_HISTORY_LEN;
        rds_constellation_i_snapshot[i] = rds_constellation_i_history[idx];
        rds_constellation_q_snapshot[i] = rds_constellation_q_history[idx];
    }
    FURI_CRITICAL_EXIT();

    canvas_set_font(canvas, FontSecondary);

    snprintf(
        freq_display,
        sizeof(freq_display),
        "%lu.%lu",
        (unsigned long)(local_freq_10khz / 100U),
        (unsigned long)((local_freq_10khz / 10U) % 10U));
    canvas_draw_str(canvas, 1, 8, freq_display);

    if(rds_ps_local[0] != '\0') {
        uint8_t ps_width = canvas_string_width(canvas, rds_ps_local);
        uint8_t ps_x = (uint8_t)((canvas_width(canvas) - ps_width) / 2U);
        canvas_draw_str(canvas, ps_x, 8, rds_ps_local);
    }

    uint16_t local_offset_abs =
        (uint16_t)((local_offset_centihz < 0) ? -local_offset_centihz : local_offset_centihz);
    snprintf(
        offset_display,
        sizeof(offset_display),
        "dF %c%u.%02u",
        (local_offset_centihz < 0) ? '-' : '+',
        (unsigned)(local_offset_abs / 100U),
        (unsigned)(local_offset_abs % 100U));
    uint8_t offset_width = canvas_string_width(canvas, offset_display);
    canvas_draw_str(
        canvas, (uint8_t)(canvas_width(canvas) - offset_width - 1U), 8, offset_display);

    const int32_t plot_radius_left = RDS_CONSTELLATION_CENTER_X - RDS_CONSTELLATION_PLOT_LEFT - 1;
    const int32_t plot_radius_right =
        RDS_CONSTELLATION_PLOT_RIGHT - RDS_CONSTELLATION_CENTER_X - 1;
    const int32_t plot_radius_top = RDS_CONSTELLATION_CENTER_Y - RDS_CONSTELLATION_PLOT_TOP - 1;
    const int32_t plot_radius_bottom =
        RDS_CONSTELLATION_PLOT_BOTTOM - RDS_CONSTELLATION_CENTER_Y - 1;
    const int32_t plot_radius_x = (plot_radius_left < plot_radius_right) ? plot_radius_left :
                                                                           plot_radius_right;
    const int32_t plot_radius_y = (plot_radius_top < plot_radius_bottom) ? plot_radius_top :
                                                                           plot_radius_bottom;
    const int32_t plot_radius = (plot_radius_x < plot_radius_y) ? plot_radius_x : plot_radius_y;

    int32_t max_abs = 1;
    for(uint8_t i = 0U; i < local_hist_count; i++) {
        int32_t i_abs = (rds_constellation_i_snapshot[i] < 0) ? -rds_constellation_i_snapshot[i] :
                                                                rds_constellation_i_snapshot[i];
        int32_t q_abs = (rds_constellation_q_snapshot[i] < 0) ? -rds_constellation_q_snapshot[i] :
                                                                rds_constellation_q_snapshot[i];
        if(i_abs > max_abs) max_abs = i_abs;
        if(q_abs > max_abs) max_abs = q_abs;
    }

    for(uint8_t i = 0U; i < local_hist_count; i++) {
        int32_t px = RDS_CONSTELLATION_CENTER_X +
                     (int32_t)(((int64_t)rds_constellation_i_snapshot[i] * plot_radius) / max_abs);
        int32_t py = RDS_CONSTELLATION_CENTER_Y -
                     (int32_t)(((int64_t)rds_constellation_q_snapshot[i] * plot_radius) / max_abs);

        if(px < RDS_CONSTELLATION_PLOT_LEFT) px = RDS_CONSTELLATION_PLOT_LEFT;
        if(px > RDS_CONSTELLATION_PLOT_RIGHT) px = RDS_CONSTELLATION_PLOT_RIGHT;
        if(py < RDS_CONSTELLATION_PLOT_TOP) py = RDS_CONSTELLATION_PLOT_TOP;
        if(py > RDS_CONSTELLATION_PLOT_BOTTOM) py = RDS_CONSTELLATION_PLOT_BOTTOM;

        canvas_draw_box(canvas, (uint8_t)px, (uint8_t)py, 1, 1);
    }

#if ENABLE_ADC_CAPTURE
    if(local_capture_active) {
        uint32_t pct = (local_capture_target_blocks > 0U) ?
                           (local_capture_captured_blocks * 100U) / local_capture_target_blocks :
                           0U;
        snprintf(status_display, sizeof(status_display), "REC %lu%%", (unsigned long)pct);
        local_status_visible = true;
    } else if(local_capture_finalize_pending) {
        snprintf(status_display, sizeof(status_display), "REC WR");
        local_status_visible = true;
    } else if(local_capture_complete) {
        snprintf(status_display, sizeof(status_display), "REC OK");
        local_status_visible = true;
    } else if(local_capture_partial) {
        snprintf(status_display, sizeof(status_display), "REC PART");
        local_status_visible = true;
    } else if(local_capture_error) {
        snprintf(status_display, sizeof(status_display), "REC ERR");
        local_status_visible = true;
    } else
#endif
        if(local_saved_active) {
        snprintf(status_display, sizeof(status_display), "SAVE OK");
        local_status_visible = true;
    }

    if(local_status_visible) {
        canvas_draw_str(canvas, 1, 63, status_display);
    }
}
#endif

/** Config: shared mute/unmute (PT + amp). */
void fred_fm_volume_change(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= COUNT_OF(volume_values)) {
        index = 0;
        variable_item_set_current_value_index(item, index);
    }
    variable_item_set_current_value_text(item, volume_names[index]);

    if(index < COUNT_OF(volume_values)) {
        current_volume = (volume_values[index] != 0);
        fred_fm_apply_audio_output_state();
        fred_fm_settings_mark_dirty();
    }
}

/** Config: select PT2257 vs PT2259-S. */
void fred_fm_pt_chip_change(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= COUNT_OF(pt_chip_values)) {
        index = 0;
        variable_item_set_current_value_index(item, index);
    }

    pt_chip = pt_chip_values[index];
    variable_item_set_current_value_text(item, pt_chip_names[index]);

    (void)fred_fm_pt_refresh_state(true);
    fred_fm_apply_audio_output_state();
    fred_fm_settings_mark_dirty();
}

/** 250 ms timer: I2C hot-plug, debounced SD saves, RDS housekeeping. */
void fred_fm_tick_callback(void* context) {
    if(!context || fred_fm_app_exiting) return;
    FredFm* app = (FredFm*)context;
    uint32_t now = furi_get_tick();

    // PT hot-plug (~500 ms)
    static uint32_t last_pt_check = 0;
    if((now - last_pt_check) > furi_ms_to_ticks(500)) {
        fred_fm_state_lock();
        bool was_ready = pt_ready_cached;
        fred_fm_state_unlock();

        bool ready = fred_fm_pt_refresh_state(false);

        if(ready && !was_ready) {
            fred_fm_apply_audio_output_state();
        }
        last_pt_check = now;
    }

    // Debounced settings save (every ~2 s when dirty)
    static uint32_t last_settings_save = 0;
    if(settings_dirty && ((now - last_settings_save) > furi_ms_to_ticks(2000))) {
        fred_fm_settings_save();
        last_settings_save = now;
    }

#ifdef ENABLE_RDS
    if(fred_fm_rds_pipeline_enabled()) {
        fred_fm_rds_process_events();
    }
#endif

    // Debounced presets save (every ~2 s when dirty)
    static uint32_t last_presets_save = 0;
    if(presets_dirty && ((now - last_presets_save) > furi_ms_to_ticks(2000))) {
        fred_fm_presets_save();
        last_presets_save = now;
    }

    // TEA5767 heartbeat for hot-unplug (~2 s)
    static uint32_t last_tea_poll = 0U;
    if((now - last_tea_poll) >= furi_ms_to_ticks(2000U)) {
        last_tea_poll = now;
        uint8_t tea_buf[5];
        struct RADIO_INFO info;
        if(tea5767_get_radio_info(tea_buf, &info)) {
            fred_fm_state_lock();
            bool was_ready = tea_i2c_ready;
            tea_info_cached = info;
            tea_info_valid = true;
            tea_i2c_ready = true;
            tea_i2c_failure_count = 0U;
            tea_info_read_count++;
            fred_fm_state_unlock();

            if(!was_ready && fred_fm_rds_pipeline_enabled()) {
                fred_fm_rds_pipeline_start();
                // Start timer so worker can begin processing
                if(rds_adc_timer_handle && !rds_adc_timer_running) {
                    fred_fm_rds_timer_start();
                }
            }
        } else {
            bool stop_rds = false;
            fred_fm_state_lock();
            if(tea_i2c_failure_count < TEA_I2C_RDS_FAILURE_LIMIT) {
                tea_i2c_failure_count++;
            }
            if(tea_i2c_failure_count >= TEA_I2C_RDS_FAILURE_LIMIT) {
                stop_rds = tea_i2c_ready;
                tea_info_valid = false;
                tea_i2c_ready = false;
            }
            fred_fm_state_unlock();

            if(stop_rds) {
                fred_fm_rds_apply_runtime_state(false);
            }
        }
        UNUSED(now);
    }

    // Trigger a redraw so the Listen view picks up fresh data
    if(app->listen_view) {
        fred_fm_redraw_listen_view(app->listen_view);
    }
#ifdef ENABLE_RDS
    if(rds_constellation_view_active && app->constellation_view) {
        fred_fm_redraw_constellation_view(app->constellation_view);
    }
#endif
}

/** Main Listen screen (frequency, RDS PS, signal bars). */
void fred_fm_view_draw_callback(Canvas* canvas, void* model) {
    (void)model; // Mark model as unused

    char title_display[24];
    char frequency_display[64];
    char signal_display[64];
    char tuning_display[64];
    char audio_display[48];
    uint8_t title_x;
#ifdef ENABLE_RDS
    char rds_ps_local[RDS_PS_LEN + 1U];
    bool local_rds_enabled;
    bool local_rds_debug_enabled;
    RdsSyncState local_rds_sync;
#endif

    fred_fm_state_lock();
    uint8_t local_pt_atten = pt_atten_db;
    bool local_muted = current_volume;
    struct RADIO_INFO info = tea_info_cached;
    bool info_valid = tea_info_valid;
    uint32_t nominal_freq_10khz = tea_nominal_freq_10khz;
#ifdef ENABLE_RDS
    local_rds_enabled = rds_enabled;
    local_rds_debug_enabled = rds_debug_enabled;
    local_rds_sync = rds_sync_display;
    memcpy(rds_ps_local, rds_ps_display, sizeof(rds_ps_local));
#endif
    fred_fm_state_unlock();

#ifdef ENABLE_RDS
    if(rds_ps_local[0] != '\0') {
        snprintf(title_display, sizeof(title_display), "%.*s", (int)RDS_PS_LEN, rds_ps_local);
    } else
#endif
    {
        snprintf(title_display, sizeof(title_display), "Radio FM");
    }

    canvas_set_font(canvas, FontPrimary);
    title_x = (uint8_t)((canvas_width(canvas) - canvas_string_width(canvas, title_display)) / 2U);
    canvas_draw_str(canvas, title_x, 10, title_display);

    // Draw button prompts
    canvas_set_font(canvas, FontSecondary);
    elements_button_left(canvas, "-0.1");
    elements_button_right(canvas, "+0.1");
    elements_button_center(canvas, "Mute");
    elements_button_top_left(canvas, " Pre");
    elements_button_top_right(canvas, "Pre ");

    if(info_valid) {
        snprintf(
            frequency_display,
            sizeof(frequency_display),
            "F: %.1f MHz",
            (double)(((float)fred_fm_get_current_freq_10khz()) / 100.0f));
        canvas_draw_str(canvas, 10, 21, frequency_display);

        snprintf(
            signal_display,
            sizeof(signal_display),
            "RSSI:%d %s",
            info.signalLevel,
            info.signalQuality);
        canvas_draw_str(canvas, 10, 41, signal_display);

        if(local_muted) {
            snprintf(
                audio_display, sizeof(audio_display), "A:MT V:-%udB", (unsigned)local_pt_atten);
        } else {
            snprintf(
                audio_display,
                sizeof(audio_display),
                "A:%s V:-%udB",
                info.stereo ? "ST" : "MO",
                (unsigned)local_pt_atten);
        }
        canvas_draw_str(canvas, 10, 31, audio_display);

#ifdef ENABLE_RDS
        if(local_rds_debug_enabled) {
            snprintf(
                tuning_display,
                sizeof(tuning_display),
                "IF:%02u E:%+ld R:%s",
                (unsigned)info.ifCounter,
                (long)((int32_t)info.ifCounter - (int32_t)TEA_IF_COUNT_TARGET),
                (local_rds_enabled || local_rds_debug_enabled) ?
                    fred_fm_rds_sync_short_text(local_rds_sync) :
                    "off");
        } else {
#endif
            snprintf(
                tuning_display,
                sizeof(tuning_display),
                "IF:%02u E:%+ld",
                (unsigned)info.ifCounter,
                (long)((int32_t)info.ifCounter - (int32_t)TEA_IF_COUNT_TARGET));
#ifdef ENABLE_RDS
        }
#endif
        if(nominal_freq_10khz >= 7600U && nominal_freq_10khz <= 10800U) {
            canvas_draw_str(canvas, 10, 51, tuning_display);
        }
    } else {
        snprintf(frequency_display, sizeof(frequency_display), "TEA5767 Not Detected");
        canvas_draw_str(canvas, 10, 21, frequency_display);

        snprintf(signal_display, sizeof(signal_display), "Pin 15 = SDA | Pin 16 = SCL");
        canvas_draw_str(canvas, 10, 41, signal_display);
    }
}
