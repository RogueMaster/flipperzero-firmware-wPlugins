/*
 * Purpose: Connect live RF edge callbacks to app UI and decoder state.
 * Owns: RF live carrier counters, RX timing capture, and decoded text updates.
 * Depends on: morse_flipper_app_i.h, radio callbacks, and CW decoder state.
 * Tests: tests/test_decoder_rf_integration.c covers host RF decoding.
 */

#include "morse_flipper_app_i.h"

#include <string.h>

static uint16_t morse_flipper_rf_rx_duration_u16(uint32_t duration_ms) {
    return duration_ms > 65535U ? 65535U : (uint16_t)duration_ms;
}

static uint16_t morse_flipper_rf_rx_min_dit_ms(void) {
    return morse_flipper_wpm_to_dit_ms(MORSE_FLIPPER_RF_RX_WPM_MAX);
}

static uint16_t morse_flipper_rf_rx_glitch_limit_ms(const MorseFlipperApp* app) {
    uint16_t limit_ms;
    uint16_t dit_ms;

    if(app == NULL) return morse_flipper_rf_rx_min_dit_ms();

    dit_ms = morse_flipper_cw_decoder_dit_ms(&app->rf_decoder);
    if(dit_ms == 0U) {
        uint8_t hint_wpm = app->rf_rx_wpm_hint == 0U ? MORSE_FLIPPER_RF_RX_DEFAULT_WPM :
                                                       app->rf_rx_wpm_hint;
        dit_ms = morse_flipper_wpm_to_dit_ms(morse_flipper_rf_clamp_wpm(hint_wpm));
    }

    limit_ms = dit_ms / 2U;
    if(limit_ms < morse_flipper_rf_rx_min_dit_ms()) return morse_flipper_rf_rx_min_dit_ms();
    return limit_ms;
}

static uint16_t morse_flipper_rf_rx_dit_ms(const MorseFlipperApp* app) {
    uint16_t dit_ms;
    uint8_t hint_wpm;

    if(app == NULL) return morse_flipper_wpm_to_dit_ms(MORSE_FLIPPER_RF_RX_DEFAULT_WPM);

    dit_ms = morse_flipper_cw_decoder_dit_ms(&app->rf_decoder);
    if(dit_ms != 0U) return dit_ms;

    hint_wpm = app->rf_rx_wpm_hint == 0U ? MORSE_FLIPPER_RF_RX_DEFAULT_WPM : app->rf_rx_wpm_hint;
    return morse_flipper_wpm_to_dit_ms(morse_flipper_rf_clamp_wpm(hint_wpm));
}

static bool morse_flipper_rf_rx_rssi_monitor(const MorseFlipperApp* app, int8_t dbm) {
    int8_t open_dbm;
    int8_t close_dbm;

    if(app == NULL) return false;

    /* Hysteresis keeps weak OOK from chattering at the threshold. */
    open_dbm = morse_flipper_rf_clamp_dbm(app->rf_monitor_threshold_dbm);
    close_dbm = morse_flipper_rf_clamp_dbm((int8_t)(open_dbm - MORSE_FLIPPER_RF_RX_HYSTERESIS_DB));

    if(app->rf_rx_level || app->rf_rx_candidate_level) {
        return dbm >= close_dbm;
    }

    return dbm >= open_dbm;
}

static void morse_flipper_rf_rx_append_char(MorseFlipperApp* app, char ch) {
    size_t len;

    if(app == NULL || ch == '\0' || ch == '|') return;

    len = strlen(app->rf_rx_text);
    if(len + 1U >= sizeof(app->rf_rx_text)) {
        memmove(app->rf_rx_text, app->rf_rx_text + 1U, len);
        len--;
    }

    app->rf_rx_text[len] = ch;
    app->rf_rx_text[len + 1U] = '\0';
}

static bool morse_flipper_rf_rx_drain_decoder(MorseFlipperApp* app) {
    const char* output;

    if(app == NULL) return false;

    output = morse_flipper_cw_decoder_output(&app->rf_decoder);
    if(output[0] == '\0') return false;

    for(size_t i = 0U; output[i] != '\0'; i++) {
        morse_flipper_rf_rx_append_char(app, output[i]);
    }
    morse_flipper_cw_decoder_clear_output(&app->rf_decoder);
    return true;
}

static bool morse_flipper_rf_rx_feed_carrier(MorseFlipperApp* app, bool level, uint32_t now_ms) {
    uint32_t duration_ms;
    uint16_t duration;
    bool glitch;
    bool text_changed = false;

    if(app == NULL) return false;
    if(level == app->rf_rx_level) return false;

    /* Convert carrier edges into decoder timing; tiny high pulses are shown, not decoded. */
    if(app->rf_rx_edge_at != 0U) {
        duration_ms = now_ms - app->rf_rx_edge_at;
        if(duration_ms != 0U) {
            duration = morse_flipper_rf_rx_duration_u16(duration_ms);
            if(app->rf_rx_level) {
                glitch = duration < morse_flipper_rf_rx_glitch_limit_ms(app);
                if(glitch) {
                    morse_flipper_rf_ticker_capture_glitch(&app->rf_rx_ticker, duration, now_ms);
                } else {
                    morse_flipper_rf_capture_rx_timing(&app->rf, true, duration);
                    morse_flipper_rf_ticker_capture(&app->rf_rx_ticker, true, duration, now_ms);
                    morse_flipper_cw_decoder_feed_mark(
                        &app->rf_decoder,
                        morse_flipper_rf_decoder_mark_ms(
                            duration, morse_flipper_rf_rx_dit_ms(app)));
                }
            } else {
                morse_flipper_rf_capture_rx_timing(&app->rf, false, duration);
                morse_flipper_cw_decoder_feed_space(&app->rf_decoder, duration);
            }
            app->rf_rx_edges_window++;
            text_changed = morse_flipper_rf_rx_drain_decoder(app);
        }
    }

    app->rf_rx_level = level;
    app->rf_rx_edge_at = now_ms;
    app->rf_rx_gap_flushed = level;
    return text_changed;
}

static bool morse_flipper_rf_rx_sample_due(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return false;

    if(app->rf_rx_sample_next_at == 0U) {
        app->rf_rx_sample_next_at = now_ms;
    }

    if(now_ms < app->rf_rx_sample_next_at) return false;

    do {
        app->rf_rx_sample_next_at += MORSE_FLIPPER_RF_RX_SAMPLE_MS;
    } while(now_ms >= app->rf_rx_sample_next_at);

    return true;
}

static bool morse_flipper_rf_rx_view_due(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return false;

    if(app->rf_rx_view_next_at == 0U) {
        app->rf_rx_view_next_at = now_ms + MORSE_FLIPPER_RF_RX_VIEW_MS;
        return true;
    }

    if(now_ms < app->rf_rx_view_next_at) return false;

    do {
        app->rf_rx_view_next_at += MORSE_FLIPPER_RF_RX_VIEW_MS;
    } while(now_ms >= app->rf_rx_view_next_at);

    return true;
}

static bool morse_flipper_rf_rx_sample_monitor(MorseFlipperApp* app, bool level, uint32_t now_ms) {
    uint32_t edge_ms;
    uint16_t glitch_ms;

    if(app == NULL) return false;

    /* Candidate state must survive several samples before becoming a carrier edge. */
    if(app->rf_rx_candidate_samples == 0U) {
        app->rf_rx_candidate_level = level;
        app->rf_rx_candidate_samples = 1U;
        return false;
    }

    if(level == app->rf_rx_candidate_level) {
        if(app->rf_rx_candidate_samples < MORSE_FLIPPER_RF_RX_STABLE_SAMPLES) {
            app->rf_rx_candidate_samples++;
        }
    } else {
        if(app->rf_rx_candidate_level && !app->rf_rx_level &&
           app->rf_rx_candidate_samples < MORSE_FLIPPER_RF_RX_STABLE_SAMPLES) {
            glitch_ms = (uint16_t)(app->rf_rx_candidate_samples * MORSE_FLIPPER_RF_RX_SAMPLE_MS);
            morse_flipper_rf_ticker_capture_glitch(&app->rf_rx_ticker, glitch_ms, now_ms);
        }
        app->rf_rx_candidate_level = level;
        app->rf_rx_candidate_samples = 1U;
    }

    if(app->rf_rx_candidate_samples < MORSE_FLIPPER_RF_RX_STABLE_SAMPLES ||
       app->rf_rx_candidate_level == app->rf_rx_level) {
        return false;
    }

    edge_ms =
        now_ms - ((uint32_t)(app->rf_rx_candidate_samples - 1U) * MORSE_FLIPPER_RF_RX_SAMPLE_MS);
    return morse_flipper_rf_rx_feed_carrier(app, app->rf_rx_candidate_level, edge_ms);
}

static bool morse_flipper_rf_rx_flush_idle_gap(MorseFlipperApp* app, uint32_t now_ms) {
    uint32_t gap_ms;

    if(app == NULL || app->rf_rx_level || app->rf_rx_gap_flushed || app->rf_rx_edge_at == 0U)
        return false;

    /* A long quiet gap may be the only signal that the last symbol is complete. */
    gap_ms = now_ms - app->rf_rx_edge_at;
    if(gap_ms < (morse_flipper_cw_decoder_dit_ms(&app->rf_decoder) * 5U) / 2U) return false;

    morse_flipper_cw_decoder_feed_space(
        &app->rf_decoder, morse_flipper_rf_rx_duration_u16(gap_ms));
    app->rf_rx_gap_flushed = true;
    return morse_flipper_rf_rx_drain_decoder(app);
}

void morse_flipper_rf_rx_edge(void* ctx, bool level, uint16_t duration_ms) {
    MorseFlipperApp* app = ctx;

    if(app == NULL || duration_ms == 0U) return;
    if(app->screen == MorseFlipperScreenRfRx) return;
    app->rf_rx_edges_window++;
    UNUSED(level);
}

void morse_flipper_tick_live_rf(MorseFlipperApp* app, uint32_t now_ms) {
    float rssi;
    int8_t dbm;
    bool old_valid;
    int8_t old_dbm;
    int8_t old_peak_dbm;
    uint16_t old_activity;
    bool old_carrier;
    bool old_monitor_tone;
    size_t old_ticker_count;
    bool ticker_scroll;
    bool ticker_changed;
    bool text_changed;
    bool carrier_now;
    bool monitor_now;
    int32_t avg_sum;
    uint16_t avg_samples;

    if(app == NULL) return;

    if(!app->rf_live_active) {
        morse_flipper_radio_sync_live(
            &app->radio,
            morse_flipper_rf_frequency_hz(&app->rf),
            false,
            false,
            MorseFlipperRadioProfileOokData);
        morse_flipper_radio_set_tx_level(&app->radio, false);
        return;
    }

    if(app->screen == MorseFlipperScreenRfRx) {
        old_valid = app->rf_rssi_valid;
        old_dbm = app->rf_rssi_dbm;
        old_peak_dbm = app->rf_rssi_peak_dbm;
        old_activity = app->rf_rx_activity;
        old_carrier = app->rf_carrier_present;
        old_monitor_tone = app->rf_monitor_tone;
        old_ticker_count = morse_flipper_rf_ticker_count(&app->rf_rx_ticker);
        morse_flipper_rf_ticker_prune(&app->rf_rx_ticker, now_ms);
        ticker_changed = old_ticker_count != morse_flipper_rf_ticker_count(&app->rf_rx_ticker);
        ticker_scroll = false;
        text_changed = false;
        morse_flipper_radio_sync_live(
            &app->radio,
            morse_flipper_rf_frequency_hz(&app->rf),
            true,
            false,
            MorseFlipperRadioProfileCarrierSense);
        if(!morse_flipper_rf_rx_sample_due(app, now_ms)) return;

        rssi = furi_hal_subghz_get_rssi();
        dbm = morse_flipper_rssi_dbm_round(rssi);
        carrier_now = morse_flipper_radio_read_rx_level(&app->radio);
        monitor_now = morse_flipper_rf_rx_rssi_monitor(app, dbm);
        text_changed = morse_flipper_rf_rx_sample_monitor(app, monitor_now, now_ms);
        text_changed = morse_flipper_rf_rx_flush_idle_gap(app, now_ms) || text_changed;
        app->rf_carrier_present = carrier_now;
        app->rf_rssi_sum_dbm += dbm;
        app->rf_rssi_samples++;
        ticker_scroll = app->rf_rx_level ||
                        morse_flipper_rf_ticker_count(&app->rf_rx_ticker) != 0U;

        if(app->rf_rssi_next_at == 0U)
            app->rf_rssi_next_at = now_ms + MORSE_FLIPPER_RF_RSSI_WINDOW_MS;
        if(now_ms >= app->rf_rssi_next_at && app->rf_rssi_samples != 0U) {
            avg_sum = app->rf_rssi_sum_dbm;
            avg_samples = app->rf_rssi_samples;

            if(avg_sum >= 0) {
                app->rf_rssi_dbm =
                    (int8_t)((avg_sum + ((int32_t)avg_samples / 2)) / (int32_t)avg_samples);
            } else {
                app->rf_rssi_dbm =
                    (int8_t)((avg_sum - ((int32_t)avg_samples / 2)) / (int32_t)avg_samples);
            }
            app->rf_rx_activity = app->rf_rx_edges_window;
            app->rf_rssi_sum_dbm = 0;
            app->rf_rssi_samples = 0U;
            app->rf_rx_edges_window = 0U;
            app->rf_rssi_next_at = now_ms + MORSE_FLIPPER_RF_RSSI_WINDOW_MS;

            if(!app->rf_rssi_valid || app->rf_rssi_dbm > app->rf_rssi_peak_dbm) {
                app->rf_rssi_peak_dbm = app->rf_rssi_dbm;
                app->rf_rssi_peak_decay_at = now_ms + MORSE_FLIPPER_RF_RSSI_PEAK_DECAY_MS;
            } else if(
                now_ms >= app->rf_rssi_peak_decay_at && app->rf_rssi_peak_dbm > app->rf_rssi_dbm) {
                app->rf_rssi_peak_dbm--;
                app->rf_rssi_peak_decay_at = now_ms + MORSE_FLIPPER_RF_RSSI_PEAK_DECAY_MS;
            }
            app->rf_rssi_valid = true;
        }

        app->rf_monitor_tone = app->rf_rx_audio_enabled && app->rf_rx_level;
        if(app->rf_rx_level || morse_flipper_rf_ticker_count(&app->rf_rx_ticker) != 0U) {
            ticker_scroll = morse_flipper_rf_rx_view_due(app, now_ms);
        }

        if(!old_valid || old_dbm != app->rf_rssi_dbm || old_peak_dbm != app->rf_rssi_peak_dbm ||
           old_activity != app->rf_rx_activity || old_carrier != app->rf_carrier_present ||
           old_monitor_tone != app->rf_monitor_tone || ticker_changed || ticker_scroll ||
           text_changed) {
            if(old_monitor_tone != app->rf_monitor_tone) {
                morse_flipper_update_sidetone(app);
            }
            morse_flipper_view_dirty(app);
        }
        return;
    }

    if(!app->radio.tx_on && morse_flipper_any_active_notes(app) &&
       morse_flipper_rf_tx_allowed_khz(morse_flipper_rf_frequency_khz(&app->rf))) {
        morse_flipper_radio_sync_live(
            &app->radio,
            morse_flipper_rf_frequency_hz(&app->rf),
            true,
            true,
            MorseFlipperRadioProfileOokData);
        morse_flipper_radio_set_tx_level(&app->radio, morse_flipper_any_active_notes(app));
    } else if(app->radio.tx_on && !morse_flipper_any_active_notes(app)) {
        morse_flipper_radio_set_tx_level(&app->radio, false);
        if(app->rf_tx_tail_until != 0U && now_ms >= app->rf_tx_tail_until) {
            morse_flipper_radio_sync_live(
                &app->radio,
                morse_flipper_rf_frequency_hz(&app->rf),
                false,
                false,
                MorseFlipperRadioProfileOokData);
            app->rf_tx_tail_until = 0U;
        }
    }
}
