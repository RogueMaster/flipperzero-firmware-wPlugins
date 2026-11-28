#include "../tpms_app_i.h"
#include "../views/tpms_receiver.h"
#include "../protocols/proto6/custom_presets.h"
#include "../protocols/tpms_generic.h"
#include "../tpms_ford_diag.h"
#include "../tpms_save.h"
#include <string.h>

static int16_t tpms_receiver_capture_buffer[TPMS_CAPTURE_MAX_SAMPLES];

#define TPMS_RELEARN_AUTO_PROFILE_COUNT 6U
#define TPMS_RELEARN_AUTO_DWELL_MS 100U
#define TPMS_RELEARN_LED_INTERVAL_MS 500U
#define TPMS_RX_AUTO_MOD_DWELL_MS 250U
#define TPMS_RELEARN_WAKE_TRACE_MAX_AGE_MS 20000U

static const NotificationSequence subghz_sequence_rx = {
    &message_green_255,

    &message_vibro_on,
    &message_note_c6,
    &message_delay_50,
    &message_sound_off,
    &message_vibro_off,

    &message_delay_50,
    NULL,
};

static const NotificationSequence subghz_sequence_rx_locked = {
    &message_green_255,

    &message_display_backlight_on,

    &message_vibro_on,
    &message_note_c6,
    &message_delay_50,
    &message_sound_off,
    &message_vibro_off,

    &message_delay_500,

    &message_display_backlight_off,
    NULL,
};

static void tpms_scene_receiver_update_statusbar(void* context) {
    TPMSApp* app = context;
    FuriString* history_stat_str = furi_string_alloc();
    (void)tpms_history_get_text_space_left(app->txrx->history, history_stat_str);

    /* TPMS 4.5: when Autosave is ON the right-most compact field becomes an
       autosave counter. This also keeps frequency/modulation visible after
       the 12-item on-screen history is full; autosave itself is independent
       of that history limit. */
    if(app->autosave_enabled) {
        if(app->autosave_error_count > 0U) {
            furi_string_set_str(history_stat_str, "A!");
        } else if(app->autosave_saved_count < 1000U) {
            furi_string_printf(
                history_stat_str, "A%lu", (unsigned long)app->autosave_saved_count);
        } else {
            furi_string_set_str(history_stat_str, "A+");
        }
    }

    FuriString* frequency_str = furi_string_alloc();
    FuriString* modulation_str = furi_string_alloc();

    tpms_get_frequency_modulation(app, frequency_str, modulation_str);

    tpms_view_receiver_add_data_statusbar(
        app->tpms_receiver,
        furi_string_get_cstr(frequency_str),
        furi_string_get_cstr(modulation_str),
        furi_string_get_cstr(history_stat_str),
        tpms_radio_is_external(app));

    furi_string_free(frequency_str);
    furi_string_free(modulation_str);
    furi_string_free(history_stat_str);
}

void tpms_scene_receiver_callback(TPMSCustomEvent event, void* context) {
    furi_assert(context);
    TPMSApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

/* Profile numbers: 0..2 = 315 MHz FSK/OOK/GFSK,
   3..5 = 433.92 MHz FSK/OOK/GFSK. */
static void tpms_scene_receiver_apply_relearn_profile(TPMSApp* app, uint8_t profile_index) {
    furi_assert(app);

    const uint8_t profile = profile_index % TPMS_RELEARN_AUTO_PROFILE_COUNT;
    const bool band433 = profile >= 3U;
    const uint8_t modulation = profile % 3U;
    const uint32_t frequency = band433 ? 433920000U : 315000000U;

    const char* preset_name = "TPMS FSK";
    uint8_t* preset_data = (uint8_t*)protoview_subghz_tpms1_fsk_async_regs;
    size_t preset_size = protoview_subghz_tpms1_fsk_async_regs_size;
    Proto6Mode bridge_mode = Proto6ModeFSK;

    if(modulation == 1U) {
        preset_name = "TPMS OOK";
        preset_data = (uint8_t*)protoview_subghz_tpms2_ook_async_regs;
        preset_size = protoview_subghz_tpms2_ook_async_regs_size;
        bridge_mode = Proto6ModeOOK;
    } else if(modulation == 2U) {
        preset_name = "TPMS GFSK";
        preset_data = (uint8_t*)protoview_subghz_tpms3_gfsk_async_regs;
        preset_size = protoview_subghz_tpms3_gfsk_async_regs_size;
    }

    if(app->txrx->txrx_state == TPMSTxRxStateRx) tpms_rx_end(app);
    tpms_preset_init(app, preset_name, frequency, preset_data, preset_size);
    proto6_bridge_set_mode(app->txrx->proto6, bridge_mode);
    proto6_bridge_reset(app->txrx->proto6);
    tpms_begin(app, preset_data);
    tpms_rx(app, frequency);
    /* Do not let a peak measured on the previous AUTO profile become the
       RSSI assigned to a frame decoded on this profile. */
    app->receiver_rssi_recent_peak = -127.0f;
    app->receiver_rssi_recent_start_tick = furi_get_tick();
    app->receiver_rssi_recent_valid = false;
    tpms_scene_receiver_update_statusbar(app);
}

/* TPMS 4.0: ordinary Odczyt AUTO. Unlike Relearn AUTO this never changes
   band/frequency; it rotates only FSK -> OOK -> GFSK on the selected MHz. */
static void tpms_scene_receiver_apply_normal_modulation(TPMSApp* app, uint8_t modulation) {
    furi_assert(app);
    const uint8_t mod = modulation % 3U;
    const uint32_t frequency = app->txrx->preset->frequency;

    const char* preset_name = "TPMS FSK";
    uint8_t* preset_data = (uint8_t*)protoview_subghz_tpms1_fsk_async_regs;
    size_t preset_size = protoview_subghz_tpms1_fsk_async_regs_size;
    Proto6Mode bridge_mode = Proto6ModeFSK;

    if(mod == 1U) {
        preset_name = "TPMS OOK";
        preset_data = (uint8_t*)protoview_subghz_tpms2_ook_async_regs;
        preset_size = protoview_subghz_tpms2_ook_async_regs_size;
        bridge_mode = Proto6ModeOOK;
    } else if(mod == 2U) {
        preset_name = "TPMS GFSK";
        preset_data = (uint8_t*)protoview_subghz_tpms3_gfsk_async_regs;
        preset_size = protoview_subghz_tpms3_gfsk_async_regs_size;
    }

    if(app->txrx->txrx_state == TPMSTxRxStateRx) tpms_rx_end(app);
    tpms_preset_init(app, preset_name, frequency, preset_data, preset_size);
    proto6_bridge_set_mode(app->txrx->proto6, bridge_mode);
    proto6_bridge_reset(app->txrx->proto6);
    tpms_begin(app, preset_data);
    tpms_rx(app, frequency);
    app->receiver_rssi_recent_peak = -127.0f;
    app->receiver_rssi_recent_start_tick = furi_get_tick();
    app->receiver_rssi_recent_valid = false;
    tpms_scene_receiver_update_statusbar(app);
}

static void tpms_scene_receiver_normal_auto_advance(TPMSApp* app, uint32_t now) {
    if(!app->receiver_modulation_auto || app->relearn != TPMSRelearnOff) return;
    if((int32_t)(now - app->receiver_modulation_auto_next_tick) < 0) return;

    app->receiver_modulation_auto_index =
        (uint8_t)((app->receiver_modulation_auto_index + 1U) % 3U);
    tpms_scene_receiver_apply_normal_modulation(app, app->receiver_modulation_auto_index);
    app->receiver_modulation_auto_next_tick =
        now + furi_ms_to_ticks(TPMS_RX_AUTO_MOD_DWELL_MS);
}

static bool tpms_scene_receiver_relearn_profile_allowed(const TPMSApp* app, uint8_t profile) {
    const bool band433 = profile >= 3U;
    const uint8_t profile_mod = (uint8_t)((profile % 3U) + TPMSRelearnModFSK);

    bool band_ok = app->relearn_rx_band_index == TPMSRelearnRXBandAuto;
    if(app->relearn_rx_band_index == TPMSRelearnRXBand315) band_ok = !band433;
    if(app->relearn_rx_band_index == TPMSRelearnRXBand433) band_ok = band433;

    const bool mod_ok = app->relearn_modulation_index == TPMSRelearnModAuto ||
                        app->relearn_modulation_index == profile_mod;
    return band_ok && mod_ok;
}

/* EL-50449 AUTO LF order is intentionally not the numeric profile order.
   Start with the European/default profile 433.92 FSK, then cover the paired
   315 MHz profile before moving to OOK and GFSK:
     433 FSK -> 315 FSK -> 433 OOK -> 315 OOK -> 433 GFSK -> 315 GFSK.
   Filtering by the user's band/modulation selection makes the same sequence
   work for AUTO band only, AUTO modulation only, or both AUTO. */
static uint8_t tpms_scene_receiver_next_ford_lf_profile(TPMSApp* app) {
    static const uint8_t order[TPMS_RELEARN_AUTO_PROFILE_COUNT] = {3U, 0U, 4U, 1U, 5U, 2U};
    const uint8_t start = app->relearn_ford_auto_lf_step % TPMS_RELEARN_AUTO_PROFILE_COUNT;

    for(uint8_t offset = 0U; offset < TPMS_RELEARN_AUTO_PROFILE_COUNT; offset++) {
        const uint8_t order_index =
            (uint8_t)((start + offset) % TPMS_RELEARN_AUTO_PROFILE_COUNT);
        const uint8_t candidate = order[order_index];
        if(!tpms_scene_receiver_relearn_profile_allowed(app, candidate)) continue;

        app->relearn_ford_auto_lf_step =
            (uint8_t)((order_index + 1U) % TPMS_RELEARN_AUTO_PROFILE_COUNT);
        return candidate;
    }

    /* Defensive fallback: 433.92 FSK is the safe/default manual profile. */
    app->relearn_ford_auto_lf_step = 1U;
    return 3U;
}

static void tpms_scene_receiver_prepare_relearn_rx(TPMSApp* app, bool reuse_auto_uhf_profile) {
    furi_assert(app);

    if(app->relearn_rx_band_index >= TPMSRelearnRXBandCount) {
        app->relearn_rx_band_index = TPMSRelearnRXBand433;
    }
    if(app->relearn_modulation_index >= TPMSRelearnModCount) {
        app->relearn_modulation_index = TPMSRelearnModFSK;
    }

    uint8_t first_profile = 0U;
    const bool ford_auto_selection =
        app->relearn_type == TPMSRelearnTypeFordEL50449 &&
        (app->relearn_rx_band_index == TPMSRelearnRXBandAuto ||
         app->relearn_modulation_index == TPMSRelearnModAuto);

    /* TPMS 3.8.5: never hot-switch CC1101 during the time-critical Ford LF
       waveform. AUTO chooses exactly one complete UHF profile BEFORE LF starts,
       freezes it for that whole LF stage, and the next Relearn cycle advances
       to the next allowed profile. Full RX18 AUTO rotation resumes only after
       the LF worker has stopped. */
    if(ford_auto_selection) {
        if(reuse_auto_uhf_profile && app->relearn_ford_auto_pair_uhf_valid &&
           tpms_scene_receiver_relearn_profile_allowed(
               app, app->relearn_ford_auto_pair_uhf_profile)) {
            /* AUTO Ford+VDO tries both LF wake families against the same UHF
               start profile before advancing to the next UHF candidate. */
            first_profile = app->relearn_ford_auto_pair_uhf_profile;
        } else {
            first_profile = tpms_scene_receiver_next_ford_lf_profile(app);
            app->relearn_ford_auto_pair_uhf_profile = first_profile;
            app->relearn_ford_auto_pair_uhf_valid = true;
        }
    } else {
        for(uint8_t profile = 0U; profile < TPMS_RELEARN_AUTO_PROFILE_COUNT; profile++) {
            if(tpms_scene_receiver_relearn_profile_allowed(app, profile)) {
                first_profile = profile;
                break;
            }
        }
    }

    tpms_scene_receiver_apply_relearn_profile(app, first_profile);
    app->relearn_auto_profile_index = first_profile;
    app->relearn_auto_rx = app->relearn_rx_band_index == TPMSRelearnRXBandAuto ||
                           app->relearn_modulation_index == TPMSRelearnModAuto;
    app->relearn_auto_next_tick =
        furi_get_tick() + furi_ms_to_ticks(TPMS_RELEARN_AUTO_DWELL_MS);
    tpms_view_receiver_set_auto_rx(app->tpms_receiver, app->relearn_auto_rx);
}

static void tpms_scene_receiver_relearn_auto_advance(TPMSApp* app, uint32_t now) {
    if(!app->relearn_auto_rx) return;
    if((int32_t)(now - app->relearn_auto_next_tick) < 0) return;

    /* V3.8.4: EL-50449 + AUTO band/modulation must not reconfigure CC1101
       while the LF worker/timer is generating the wake-up waveform. V3.8.3
       froze modulation only; hardware tests showed that 315 <-> 433 MHz
       switching can trigger the same furi_check. Freeze the ENTIRE UHF profile
       for the active LF burst. Once LF ends, normal RX18 AUTO rotation resumes. */
    const bool ford_lf_active =
        app->relearn_type == TPMSRelearnTypeFordEL50449 &&
        tpms_view_receiver_relearn_is_active(app->tpms_receiver);
    if(ford_lf_active) {
        app->relearn_auto_next_tick =
            now + furi_ms_to_ticks(TPMS_RELEARN_AUTO_DWELL_MS);
        return;
    }

    uint8_t next_profile = app->relearn_auto_profile_index;
    for(uint8_t step = 1U; step <= TPMS_RELEARN_AUTO_PROFILE_COUNT; step++) {
        const uint8_t candidate =
            (uint8_t)((app->relearn_auto_profile_index + step) % TPMS_RELEARN_AUTO_PROFILE_COUNT);
        if(!tpms_scene_receiver_relearn_profile_allowed(app, candidate)) continue;
        next_profile = candidate;
        break;
    }

    if(next_profile != app->relearn_auto_profile_index) {
        app->relearn_auto_profile_index = next_profile;
        tpms_scene_receiver_apply_relearn_profile(app, next_profile);
    }
    app->relearn_auto_next_tick = now + furi_ms_to_ticks(TPMS_RELEARN_AUTO_DWELL_MS);
}

static uint32_t tpms_scene_receiver_relearn_wait_ms(const TPMSApp* app) {
    if(!app) return TPMS_RELEARN_RX_WAIT_5_MS;
    if(app->relearn_rx_wait == TPMSRelearnRXWait10) return TPMS_RELEARN_RX_WAIT_10_MS;
    if(app->relearn_rx_wait == TPMSRelearnRXWait15) return TPMS_RELEARN_RX_WAIT_15_MS;
    return TPMS_RELEARN_RX_WAIT_5_MS;
}

static uint32_t tpms_scene_receiver_cw_frequency_hz(const TPMSApp* app) {
    return (app && app->relearn_cw_frequency == TPMSCWFrequency1342) ?
               TPMS_LF_CARRIER_1342_HZ :
               TPMS_LF_CARRIER_HZ;
}

static uint8_t tpms_scene_receiver_wake_trace_profile(TPMSFordLFProfile profile) {
    if(profile == TPMSFordLFProfile5A5A) return TpmsWakeProfileFord5A5A;
    if(profile == TPMSFordLFProfileVDO) return TpmsWakeProfileVDOFCA615E;
    return TpmsWakeProfileAutoFordVDO;
}

static TPMSFordLFProfile tpms_scene_receiver_select_ford_wake_profile(
    TPMSApp* app,
    bool* reuse_auto_uhf_profile) {
    furi_assert(app);
    if(reuse_auto_uhf_profile) *reuse_auto_uhf_profile = false;

    if(app->relearn_ford_profile != TPMSFordLFProfileAuto) {
        app->relearn_ford_auto_pair_uhf_valid = false;
        return app->relearn_ford_profile;
    }

    /* 4.2: one complete cycle is Ford 5A5A, the next complete cycle is
       VDO/FCA 615E. The two families are never interleaved frame-by-frame. */
    const bool vdo_cycle = (app->relearn_ford_auto_wake_step & 1U) != 0U;
    if(reuse_auto_uhf_profile) *reuse_auto_uhf_profile = vdo_cycle;
    app->relearn_ford_auto_wake_step ^= 1U;
    return vdo_cycle ? TPMSFordLFProfileVDO : TPMSFordLFProfile5A5A;
}

static void tpms_scene_receiver_start_configured_relearn(
    TPMSApp* app,
    bool user_requested) {
    if(app->relearn != TPMSRelearnOn) return;

    /* Do not truncate a running LF wake-up. Extra RIGHT/OK presses while LF
       is active are intentionally ignored; the current cycle finishes first. */
    if(tpms_view_receiver_relearn_is_active(app->tpms_receiver)) return;

    if(user_requested) app->relearn_auto_satisfied = false;
    app->relearn_repeat_waiting = false;

    /* Choose the concrete LF wake family BEFORE freezing the UHF profile.
       AUTO Ford+VDO alternates complete cycles: Ford -> RX -> VDO -> RX. */
    bool reuse_auto_uhf_profile = false;
    TPMSFordLFProfile active_ford_profile = app->relearn_ford_profile;
    if(app->relearn_type == TPMSRelearnTypeFordEL50449) {
        active_ford_profile =
            tpms_scene_receiver_select_ford_wake_profile(app, &reuse_auto_uhf_profile);
    }

    /* Re-apply only the selected UHF band/modulation set. RX18 remains the
       protocol detector for every Relearn type. */
    tpms_scene_receiver_prepare_relearn_rx(app, reuse_auto_uhf_profile);
    tpms_view_receiver_set_ford_profile(app->tpms_receiver, active_ford_profile);

    /* Manual 433/315 + manual FSK/OOK/GFSK keeps the proven 5 s EL-50449
       burst. If either UHF band or modulation is AUTO, use 2.5 s per frozen
       profile so subsequent Relearn cycles cover the other candidates sooner. */
    const bool ford_auto_selection =
        app->relearn_type == TPMSRelearnTypeFordEL50449 &&
        (app->relearn_rx_band_index == TPMSRelearnRXBandAuto ||
         app->relearn_modulation_index == TPMSRelearnModAuto);
    tpms_view_receiver_set_ford_duration(
        app->tpms_receiver,
        ford_auto_selection ? TPMS_FORD_EL50449_AUTO_DURATION_MS :
                              TPMS_FORD_EL50449_DURATION_MS);

    tpms_view_receiver_set_cw_frequency(
        app->tpms_receiver, tpms_scene_receiver_cw_frequency_hz(app));
    tpms_view_receiver_relearn_start(app->tpms_receiver, app->relearn_type);

    const uint8_t wake_profile =
        app->relearn_type == TPMSRelearnTypeFordEL50449 ?
            tpms_scene_receiver_wake_trace_profile(active_ford_profile) :
            TpmsWakeProfileNone;
    app->relearn_last_wake_profile = wake_profile;
    app->relearn_last_wake_valid = wake_profile != TpmsWakeProfileNone;
    app->relearn_last_wake_tick = furi_get_tick();

    app->relearn_cycle_was_active =
        tpms_view_receiver_relearn_is_active(app->tpms_receiver);
    app->relearn_led_green_phase = true;
    app->relearn_led_rx_on = false;
    app->relearn_led_next_tick = furi_get_tick();
}

static void tpms_scene_receiver_update_relearn_repeat(TPMSApp* app, uint32_t now) {
    const bool lf_active = tpms_view_receiver_relearn_is_active(app->tpms_receiver);

    if(app->relearn_cycle_was_active && !lf_active) {
        app->relearn_cycle_was_active = false;

        if(app->relearn == TPMSRelearnOn &&
           app->relearn_repeat_mode == TPMSRelearnRepeatAuto &&
           !app->relearn_auto_satisfied) {
            app->relearn_repeat_waiting = true;
            app->relearn_repeat_next_tick =
                now + furi_ms_to_ticks(tpms_scene_receiver_relearn_wait_ms(app));
        }
    }

    if(app->relearn_repeat_waiting &&
       app->relearn_repeat_mode == TPMSRelearnRepeatAuto &&
       !app->relearn_auto_satisfied &&
       !lf_active &&
       (int32_t)(now - app->relearn_repeat_next_tick) >= 0) {
        tpms_scene_receiver_start_configured_relearn(app, false);
    }
}

static void tpms_scene_receiver_update_led(TPMSApp* app, uint32_t now) {
    if(app->txrx->txrx_state != TPMSTxRxStateRx) return;
    if((int32_t)(now - app->relearn_led_next_tick) < 0) return;

    if(tpms_view_receiver_relearn_is_active(app->tpms_receiver)) {
        /* V3.8.1: use exclusive solid colour phases instead of the 10 ms
           blink helpers. The old helpers did not explicitly clear the other
           RGB channels, so the green phase could be effectively hidden by a
           previously lit blue channel on some firmware builds. */
        notification_message(
            app->notifications,
            app->relearn_led_green_phase ? &sequence_set_only_green_255 :
                                           &sequence_set_only_blue_255);
        app->relearn_led_green_phase = !app->relearn_led_green_phase;
        app->relearn_led_rx_on = false;
    } else {
        /* RX-only heartbeat: clearly visible blue ON / OFF phases. */
        if(app->relearn_led_rx_on) {
            notification_message(app->notifications, &sequence_reset_rgb);
        } else {
            notification_message(app->notifications, &sequence_set_only_blue_255);
        }
        app->relearn_led_rx_on = !app->relearn_led_rx_on;
        app->relearn_led_green_phase = true;
    }

    app->relearn_led_next_tick = now + furi_ms_to_ticks(TPMS_RELEARN_LED_INTERVAL_MS);
}

static bool tpms_scene_receiver_store_decoder(
    TPMSApp* app,
    SubGhzProtocolDecoderBase* decoder_base,
    float frame_rssi) {
    furi_assert(app);
    furi_assert(decoder_base);

    FuriString* str_buff = furi_string_alloc();
    bool added = false;
    bool ford_diag_ready = false;
    TPMSBlockGeneric ford_diag_generic = {0};
    uint8_t ford_diag_wake_profile = TpmsWakeProfileNone;
    bool ford_diag_wake_valid = false;

    const uint32_t store_now = furi_get_tick();
    const bool autosave_wake_valid =
        app->relearn_last_wake_valid &&
        (store_now - app->relearn_last_wake_tick) <=
            furi_ms_to_ticks(TPMS_RELEARN_WAKE_TRACE_MAX_AGE_MS);
    const uint8_t autosave_wake_profile =
        autosave_wake_valid ? app->relearn_last_wake_profile : TpmsWakeProfileNone;

    furi_mutex_acquire(app->history_mutex, FuriWaitForever);
    const TPMSHistoryStateAddKey state =
        tpms_history_add_to_history(app->txrx->history, decoder_base, app->txrx->preset);

    uint16_t affected_index = 0U;
    bool affected_valid = false;
    if(state == TPMSHistoryStateAddKeyNewDada ||
       state == TPMSHistoryStateAddKeyUpdateData ||
       state == TPMSHistoryStateAddKeyReplaceData) {
        affected_valid =
            tpms_history_get_last_affected_index(app->txrx->history, &affected_index);
        if(affected_valid) {
            (void)tpms_history_set_frame_rssi(app->txrx->history, affected_index, frame_rssi);
            const uint32_t now = furi_get_tick();
            if(app->relearn_last_wake_valid &&
               (now - app->relearn_last_wake_tick) <=
                   furi_ms_to_ticks(TPMS_RELEARN_WAKE_TRACE_MAX_AGE_MS)) {
                (void)tpms_history_set_wake_profile(
                    app->txrx->history, affected_index, app->relearn_last_wake_profile);
                ford_diag_wake_profile = app->relearn_last_wake_profile;
                ford_diag_wake_valid = true;
                /* First valid RF frame after the LF cycle consumes the trace,
                   avoiding stale attribution to later periodic transmissions. */
                app->relearn_last_wake_valid = false;
                if(app->relearn_ford_profile == TPMSFordLFProfileAuto) {
                    /* A completed wake/RF transaction starts the next AUTO
                       session again from Ford 5A5A. */
                    app->relearn_ford_auto_wake_step = 0U;
                    app->relearn_ford_auto_pair_uhf_valid = false;
                }
            }
        }

        bool capture_truncated = false;
        const size_t capture_count = proto6_bridge_copy_last_capture(
            app->txrx->proto6,
            tpms_receiver_capture_buffer,
            COUNT_OF(tpms_receiver_capture_buffer),
            &capture_truncated);
        if(capture_count > 0U && capture_count <= UINT16_MAX && affected_valid) {
            (void)tpms_history_set_raw_capture(
                app->txrx->history,
                affected_index,
                tpms_receiver_capture_buffer,
                (uint16_t)capture_count,
                capture_truncated);
        }

        /* TPMS 4.3: keep a persistent Ford state trace. This is intentionally
           RAW-first: no unconfirmed meaning is assigned to bit 0x80/0x10 or
           unusual TT values. Exact duplicate payloads are filtered by the
           logger, while real state/pressure/TT changes remain visible. */
        if(affected_valid &&
           strcmp(tpms_history_get_protocol_name(app->txrx->history, affected_index),
                  "Ford TPMS") == 0) {
            FlipperFormat* history_format =
                tpms_history_get_raw_data(app->txrx->history, affected_index);
            if(history_format &&
               tpms_block_generic_deserialize(&ford_diag_generic, history_format) ==
                   SubGhzProtocolStatusOk &&
               ford_diag_generic.raw_payload_valid &&
               ford_diag_generic.raw_payload_size >= 8U) {
                ford_diag_ready = true;
            }
        }
    }

    if(state == TPMSHistoryStateAddKeyNewDada) {
        furi_string_reset(str_buff);
        const uint16_t item_count = tpms_history_get_item(app->txrx->history);
        const uint16_t item_index = item_count - 1U;
        tpms_history_get_text_item_menu(app->txrx->history, str_buff, item_index);
        tpms_view_receiver_add_item_to_menu(
            app->tpms_receiver,
            furi_string_get_cstr(str_buff),
            tpms_history_get_type_protocol(app->txrx->history, item_index));

        tpms_scene_receiver_update_statusbar(app);
        added = true;
    } else if((state == TPMSHistoryStateAddKeyUpdateData ||
               state == TPMSHistoryStateAddKeyReplaceData) &&
              affected_valid) {
        /* Keep the list RSSI current when the same sensor transmits again. */
        furi_string_reset(str_buff);
        tpms_history_get_text_item_menu(app->txrx->history, str_buff, affected_index);
        tpms_view_receiver_update_item(
            app->tpms_receiver,
            affected_index,
            furi_string_get_cstr(str_buff),
            tpms_history_get_type_protocol(app->txrx->history, affected_index));
        if(state == TPMSHistoryStateAddKeyReplaceData) {
            tpms_scene_receiver_update_statusbar(app);
            added = true;
        }
    }

    if(state == TPMSHistoryStateAddKeyNewDada ||
       state == TPMSHistoryStateAddKeyUpdateData ||
       state == TPMSHistoryStateAddKeyReplaceData) {
        app->txrx->rx_key_state = TPMSRxKeyStateAddKey;
    }
    furi_mutex_release(app->history_mutex);

    /* Autosave serializes the decoder directly, so it continues to work even
       when the 12-entry on-screen history is full. Exact duplicate payloads
       for protocol+ID are suppressed in-session. */
    const uint32_t autosave_errors_before = app->autosave_error_count;
    const bool autosaved = tpms_autosave_process_decoder(
        app,
        decoder_base,
        frame_rssi,
        autosave_wake_profile,
        autosave_wake_valid);
    if(autosaved && autosave_wake_valid && app->relearn_last_wake_valid) {
        /* History may already be full, so let a successful Autosave consume
           the one-shot Wake trace as well. */
        app->relearn_last_wake_valid = false;
        if(app->relearn_ford_profile == TPMSFordLFProfileAuto) {
            app->relearn_ford_auto_wake_step = 0U;
            app->relearn_ford_auto_pair_uhf_valid = false;
        }
    }
    if(autosaved || app->autosave_error_count != autosave_errors_before) {
        tpms_scene_receiver_update_statusbar(app);
    }

    if(ford_diag_ready) {
        (void)tpms_ford_diag_append(
            ford_diag_generic.id,
            ford_diag_generic.pressure * 100.0f,
            ford_diag_generic.temperature,
            app->txrx->preset->frequency,
            tpms_radio_preset_from_name(furi_string_get_cstr(app->txrx->preset->name)),
            frame_rssi,
            ford_diag_wake_profile,
            ford_diag_wake_valid,
            ford_diag_generic.raw_payload,
            ford_diag_generic.raw_payload_size);
    }

    if(added) {
        notification_message(app->notifications, &sequence_blink_green_10);
        if(app->lock != TPMSLockOn) {
            notification_message(app->notifications, &subghz_sequence_rx);
        } else {
            notification_message(app->notifications, &subghz_sequence_rx_locked);
        }
    }

    furi_string_free(str_buff);
    return added;
}

void tpms_scene_receiver_on_enter(void* context) {
    TPMSApp* app = context;
    app->relearn_last_wake_valid = false;
    app->relearn_last_wake_profile = TpmsWakeProfileNone;
    tpms_ford_diag_reset_session();
    const bool relearn_autostart = app->relearn_autostart;
    app->relearn_autostart = false;

    /* Stop any previous RX session before rebuilding the list and decoder state. */
    if(app->txrx->txrx_state == TPMSTxRxStateRx) {
        tpms_rx_end(app);
    }

    FuriString* str_buff;
    str_buff = furi_string_alloc();

    if(app->txrx->rx_key_state == TPMSRxKeyStateIDLE) {
        tpms_history_reset(app->txrx->history);
        app->txrx->rx_key_state = TPMSRxKeyStateStart;
    }

    tpms_view_receiver_set_lock(app->tpms_receiver, app->lock);
    app->relearn_auto_rx = false;
    const bool normal_auto =
        app->receiver_modulation_auto && app->relearn == TPMSRelearnOff;
    tpms_view_receiver_set_auto_rx(app->tpms_receiver, normal_auto);
    app->receiver_modulation_auto_next_tick =
        furi_get_tick() + furi_ms_to_ticks(TPMS_RX_AUTO_MOD_DWELL_MS);
    tpms_view_receiver_set_relearn_enabled(
        app->tpms_receiver, app->relearn == TPMSRelearnOn);
    app->relearn_cycle_was_active = false;
    app->relearn_repeat_waiting = false;
    app->relearn_auto_satisfied = false;
    app->relearn_repeat_next_tick = 0U;
    app->relearn_ford_auto_lf_step = 0U;
    app->relearn_ford_auto_wake_step = 0U;
    app->relearn_ford_auto_pair_uhf_profile = 3U;
    app->relearn_ford_auto_pair_uhf_valid = false;
    app->relearn_led_green_phase = true;
    app->relearn_led_rx_on = false;
    app->relearn_led_next_tick = furi_get_tick();
    app->receiver_rssi_recent_peak = -127.0f;
    app->receiver_rssi_recent_start_tick = furi_get_tick();
    app->receiver_rssi_recent_valid = false;
    tpms_view_receiver_reset_rssi(app->tpms_receiver);

    //Load history to receiver
    tpms_view_receiver_exit(app->tpms_receiver);
    for(uint8_t i = 0; i < tpms_history_get_item(app->txrx->history); i++) {
        furi_string_reset(str_buff);
        tpms_history_get_text_item_menu(app->txrx->history, str_buff, i);
        tpms_view_receiver_add_item_to_menu(
            app->tpms_receiver,
            furi_string_get_cstr(str_buff),
            tpms_history_get_type_protocol(app->txrx->history, i));
        app->txrx->rx_key_state = TPMSRxKeyStateAddKey;
    }
    furi_string_free(str_buff);
    tpms_scene_receiver_update_statusbar(app);

    tpms_view_receiver_set_callback(app->tpms_receiver, tpms_scene_receiver_callback, app);

    /* The classic scanner UI is preserved. Only the decoder backend is
       replaced with ProtoView's TPMS decoder bank. */
    const char* preset_name = furi_string_get_cstr(app->txrx->preset->name);
    if(normal_auto) {
        if(!strcmp(preset_name, "TPMS OOK")) app->receiver_modulation_auto_index = 1U;
        else if(!strcmp(preset_name, "TPMS GFSK")) app->receiver_modulation_auto_index = 2U;
        else app->receiver_modulation_auto_index = 0U;
    }
    if(!strcmp(preset_name, "AM270") || !strcmp(preset_name, "AM650") ||
       !strcmp(preset_name, "TPMS OOK")) {
        proto6_bridge_set_mode(app->txrx->proto6, Proto6ModeOOK);
    } else {
        proto6_bridge_set_mode(app->txrx->proto6, Proto6ModeFSK);
    }
    proto6_bridge_reset(app->txrx->proto6);

    if((app->txrx->txrx_state == TPMSTxRxStateIDLE) ||
       (app->txrx->txrx_state == TPMSTxRxStateSleep)) {
        uint8_t* preset_data = app->txrx->preset->data;
        if(!preset_data) {
            preset_data = subghz_setting_get_preset_data_by_name(
                app->setting, furi_string_get_cstr(app->txrx->preset->name));
        }
        tpms_begin(app, preset_data);

        tpms_rx(app, app->txrx->preset->frequency);
        tpms_scene_receiver_update_statusbar(app);
    }

    tpms_view_receiver_set_idx_menu(app->tpms_receiver, app->txrx->idx_menu_chosen);
    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewReceiver);

    /* START from the Relearn configuration goes directly to scanning and
       launches the first LF wake-up only after CC1101/RX18 is already active. */
    if(relearn_autostart && app->relearn == TPMSRelearnOn) {
        tpms_scene_receiver_start_configured_relearn(app, true);
    }
}

bool tpms_scene_receiver_on_event(void* context, SceneManagerEvent event) {
    TPMSApp* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case TPMSCustomEventViewReceiverBack:
            // Stop CC1101 Rx
            if(app->txrx->txrx_state == TPMSTxRxStateRx) {
                tpms_rx_end(app);
                tpms_sleep(app);
            };
            app->txrx->hopper_state = TPMSHopperStateOFF;
            app->relearn_auto_rx = false;
            tpms_view_receiver_set_auto_rx(app->tpms_receiver, false);
            app->txrx->idx_menu_chosen = 0;
            app->txrx->rx_key_state = TPMSRxKeyStateIDLE;
            if(scene_manager_has_previous_scene(app->scene_manager, TPMSSceneStart)) {
                consumed = scene_manager_search_and_switch_to_previous_scene(
                    app->scene_manager, TPMSSceneStart);
            } else {
                scene_manager_next_scene(app->scene_manager, TPMSSceneStart);
            }
            break;
        case TPMSCustomEventViewReceiverOK:
            app->relearn_auto_rx = false;
            tpms_view_receiver_set_auto_rx(app->tpms_receiver, false);
            app->txrx->idx_menu_chosen = tpms_view_receiver_get_idx_menu(app->tpms_receiver);
            if(app->txrx->txrx_state == TPMSTxRxStateRx) tpms_rx_end(app);
            scene_manager_next_scene(app->scene_manager, TPMSSceneReceiverInfo);
            consumed = true;
            break;
        case TPMSCustomEventViewReceiverConfig:
            app->relearn_auto_rx = false;
            tpms_view_receiver_set_auto_rx(app->tpms_receiver, false);
            app->txrx->idx_menu_chosen = tpms_view_receiver_get_idx_menu(app->tpms_receiver);
            if(app->txrx->txrx_state == TPMSTxRxStateRx) tpms_rx_end(app);
            scene_manager_next_scene(app->scene_manager, TPMSSceneReceiverConfig);
            consumed = true;
            break;
        case TPMSCustomEventViewReceiverRelearn:
            tpms_scene_receiver_start_configured_relearn(app, true);
            consumed = true;
            break;
        case TPMSCustomEventViewReceiverOffDisplay:
            notification_message(app->notifications, &sequence_display_backlight_off);
            consumed = true;
            break;
        case TPMSCustomEventViewReceiverUnlock:
            app->lock = TPMSLockOff;
            consumed = true;
            break;
        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        const uint32_t now = furi_get_tick();

        /* Live RSSI is sampled before decoder/Auto profile changes. Keep a
           short 500 ms peak window so a decoded TPMS frame gets a useful RSSI
           even though ProtoView intentionally waits ~50 ms of quiet before
           declaring a short burst complete. */
        const float rssi = tpms_radio_get_rssi(app);
        tpms_view_receiver_set_rssi(app->tpms_receiver, rssi);
        if((now - app->receiver_rssi_recent_start_tick) >= furi_ms_to_ticks(500U)) {
            app->receiver_rssi_recent_start_tick = now;
            app->receiver_rssi_recent_peak = rssi;
            app->receiver_rssi_recent_valid = rssi > -127.0f && rssi <= 20.0f;
        } else if(rssi > -127.0f && rssi <= 20.0f &&
                  (!app->receiver_rssi_recent_valid || rssi > app->receiver_rssi_recent_peak)) {
            app->receiver_rssi_recent_peak = rssi;
            app->receiver_rssi_recent_valid = true;
        }

        SubGhzProtocolDecoderBase* proto6_result = proto6_bridge_scan(app->txrx->proto6);
        if(proto6_result) {
            const float frame_rssi = app->receiver_rssi_recent_valid ?
                                         app->receiver_rssi_recent_peak :
                                         rssi;
            const bool added = tpms_scene_receiver_store_decoder(app, proto6_result, frame_rssi);
            app->receiver_rssi_recent_start_tick = now;
            app->receiver_rssi_recent_peak = rssi;
            app->receiver_rssi_recent_valid = rssi > -127.0f && rssi <= 20.0f;
            if(added && app->relearn_repeat_mode == TPMSRelearnRepeatAuto) {
                /* A valid RX18 result satisfies AUTO Relearn. Do not cut an
                   LF cycle that is already transmitting; simply prevent the
                   next scheduled wake-up. */
                app->relearn_auto_satisfied = true;
                app->relearn_repeat_waiting = false;
            }
        }

        /* TPMS 4.0 normal Odczyt AUTO rotates only FSK/OOK/GFSK on the
           currently selected frequency. Relearn AUTO remains independent. */
        tpms_scene_receiver_normal_auto_advance(app, now);

        /* Relearn AUTO RX rotates only through the configured band/modulation
           combinations while the ordinary RX18 decoder remains active. */
        tpms_scene_receiver_relearn_auto_advance(app, now);
        tpms_scene_receiver_update_relearn_repeat(app, now);
        tpms_scene_receiver_update_led(app, now);

    }
    return consumed;
}

void tpms_scene_receiver_on_exit(void* context) {
    TPMSApp* app = context;
    tpms_view_receiver_relearn_stop(app->tpms_receiver);
    tpms_view_receiver_set_auto_rx(app->tpms_receiver, false);
    tpms_view_receiver_set_relearn_enabled(app->tpms_receiver, false);
    app->relearn_cycle_was_active = false;
    app->relearn_repeat_waiting = false;
    app->relearn_auto_satisfied = false;
    app->relearn_last_wake_valid = false;
    app->relearn_last_wake_profile = TpmsWakeProfileNone;
    notification_message(app->notifications, &sequence_reset_rgb);
    /* Never leave a live callback attached to a hidden receiver view. */
    proto6_bridge_reset(app->txrx->proto6);
}
