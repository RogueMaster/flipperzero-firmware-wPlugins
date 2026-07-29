#include "mf_radio_core.h"

#include <limits.h>
#include <string.h>

typedef struct {
    uint32_t min_hz;
    uint32_t max_hz;
} MfRadioBand;

static const MfRadioBand vfo_bands[] = {
    {300000000U, 348000000U},
    {387000000U, 464000000U},
    {779000000U, 928000000U},
};

bool mf_radio_hardware_ops_valid(const MfRadioHardwareOps* ops) {
    return ops != NULL && ops->prepare_tx != NULL && ops->prepare_carrier_rx != NULL &&
           ops->set_tx_level != NULL && ops->read_carrier != NULL && ops->read_rssi_dbm != NULL &&
           ops->frequency_valid != NULL && ops->tx_allowed != NULL &&
           ops->default_frequency != NULL && ops->idle != NULL && ops->sleep != NULL;
}

bool mf_radio_frequency_in_vfo(uint32_t frequency_hz) {
    size_t i;
    for(i = 0U; i < sizeof(vfo_bands) / sizeof(vfo_bands[0]); i++) {
        if(frequency_hz >= vfo_bands[i].min_hz && frequency_hz <= vfo_bands[i].max_hz) return true;
    }
    return false;
}

uint8_t mf_radio_clamp_wpm(uint8_t wpm) {
    if(wpm < MF_RADIO_RX_WPM_MIN) return MF_RADIO_RX_WPM_MIN;
    if(wpm > MF_RADIO_RX_WPM_MAX) return MF_RADIO_RX_WPM_MAX;
    return wpm;
}

int8_t mf_radio_clamp_dbm(int8_t dbm) {
    if(dbm < -115) return -115;
    if(dbm > -50) return -50;
    return dbm;
}

static int8_t mf_radio_clamp_dbm_i16(int16_t dbm) {
    if(dbm < -115) return -115;
    if(dbm > -50) return -50;
    return (int8_t)dbm;
}

uint16_t mf_radio_wpm_to_dit_ms(uint8_t wpm) {
    wpm = mf_radio_clamp_wpm(wpm);
    return (uint16_t)(1200U / wpm);
}

uint16_t mf_radio_decoder_mark_ms(uint16_t duration_ms, uint16_t dit_ms) {
    if(duration_ms == 0U || dit_ms == 0U) return duration_ms;
    return duration_ms < ((uint32_t)dit_ms * 5U) / 2U ? dit_ms : (uint16_t)(dit_ms * 3U);
}

static bool mf_radio_frequency_valid(const MfRadioState* state, uint32_t frequency_hz) {
    return mf_radio_frequency_in_vfo(frequency_hz) &&
           state->hardware.frequency_valid(state->hardware.context, frequency_hz);
}

static void mf_radio_quiesce(MfRadioState* state) {
    bool hardware_active;
    if(state == NULL) return;
    hardware_active = state->tx_prepared || state->rx_prepared || state->snapshot.hardware_active;
    if(state->tx_prepared) state->hardware.set_tx_level(state->hardware.context, false);
    if(state->snapshot.hardware_active) state->hardware.idle(state->hardware.context);
    state->tx_prepared = false;
    state->rx_prepared = false;
    state->snapshot.hardware_active = false;
    state->snapshot.tx_active = false;
    state->snapshot.monitor_tone = false;
    state->tx_idle_at = 0U;
    if(hardware_active) state->hardware.sleep(state->hardware.context);
}

static void mf_radio_rollback_prepare(MfRadioState* state) {
    state->hardware.idle(state->hardware.context);
    state->hardware.sleep(state->hardware.context);
}

static void mf_radio_reset_decoder(MfRadioState* state, uint16_t dit_ms) {
    state->decoder_services->init(&state->decoder, dit_ms);
}

static void mf_radio_drain_decoder(MfRadioState* state) {
    const char* output = state->decoder_services->output(&state->decoder);
    if(output[0] == '\0') return;
    state->draw_services->history_append(&state->tx_history, output);
    state->decoder_services->clear_output(&state->decoder);
}

static uint16_t mf_radio_duration_u16(uint32_t duration_ms) {
    return duration_ms > UINT16_MAX ? UINT16_MAX : (uint16_t)duration_ms;
}

static void mf_radio_ticker_prune(MfRadioTicker* ticker, uint32_t now_ms) {
    while(ticker->count != 0U) {
        const MfRadioTickerMark* mark = &ticker->marks[ticker->start];
        if((uint32_t)(now_ms - mark->end_ms) <= MF_RADIO_RX_TICKER_WINDOW_MS) return;
        ticker->start = (uint8_t)((ticker->start + 1U) % MF_RADIO_RX_TICKER_CAPACITY);
        ticker->count--;
    }
}

static void mf_radio_ticker_capture(
    MfRadioTicker* ticker,
    uint16_t duration_ms,
    uint32_t end_ms,
    bool glitch) {
    uint8_t slot;
    if(duration_ms == 0U) return;
    mf_radio_ticker_prune(ticker, end_ms);
    if(ticker->count < MF_RADIO_RX_TICKER_CAPACITY) {
        slot = (uint8_t)((ticker->start + ticker->count) % MF_RADIO_RX_TICKER_CAPACITY);
        ticker->count++;
    } else {
        slot = ticker->start;
        ticker->start = (uint8_t)((ticker->start + 1U) % MF_RADIO_RX_TICKER_CAPACITY);
    }
    ticker->marks[slot] = (MfRadioTickerMark){
        .end_ms = end_ms,
        .duration_ms = duration_ms,
        .glitch = glitch,
    };
}

static void mf_radio_rx_append(MfRadioState* state, char ch) {
    size_t len;
    if(ch == '\0' || ch == '|') return;
    len = strlen(state->rx_text);
    if(len + 1U >= sizeof(state->rx_text)) {
        memmove(state->rx_text, state->rx_text + 1U, len);
        len--;
    }
    state->rx_text[len] = ch;
    state->rx_text[len + 1U] = '\0';
}

static bool mf_radio_rx_drain_decoder(MfRadioState* state) {
    const char* output = state->decoder_services->output(&state->decoder);
    size_t i;
    if(output[0] == '\0') return false;
    for(i = 0U; output[i] != '\0'; i++)
        mf_radio_rx_append(state, output[i]);
    state->decoder_services->clear_output(&state->decoder);
    return true;
}

static uint16_t mf_radio_rx_dit_ms(const MfRadioState* state) {
    uint16_t dit_ms = state->decoder_services->dit_ms(&state->decoder);
    return dit_ms == 0U ? mf_radio_wpm_to_dit_ms(state->rx_wpm_hint) : dit_ms;
}

static uint16_t mf_radio_rx_glitch_limit_ms(const MfRadioState* state) {
    uint16_t limit_ms = mf_radio_rx_dit_ms(state) / 2U;
    uint16_t floor_ms = mf_radio_wpm_to_dit_ms(MF_RADIO_RX_WPM_MAX);
    return limit_ms < floor_ms ? floor_ms : limit_ms;
}

static bool mf_radio_rx_feed_edge(MfRadioState* state, bool level, uint32_t now_ms) {
    uint32_t duration_ms;
    uint16_t duration;
    bool changed = false;
    if(level == state->rx_level) return false;
    if(state->rx_edge_at != 0U) {
        duration_ms = now_ms - state->rx_edge_at;
        duration = mf_radio_duration_u16(duration_ms);
        if(duration != 0U) {
            if(state->rx_level) {
                bool glitch = duration < mf_radio_rx_glitch_limit_ms(state);
                mf_radio_ticker_capture(&state->ticker, duration, now_ms, glitch);
                if(!glitch)
                    state->decoder_services->feed_mark(
                        &state->decoder,
                        mf_radio_decoder_mark_ms(duration, mf_radio_rx_dit_ms(state)));
            } else {
                state->decoder_services->feed_space(&state->decoder, duration);
            }
            state->rx_edges_window++;
            changed = mf_radio_rx_drain_decoder(state);
        }
    }
    state->rx_level = level;
    state->rx_edge_at = now_ms;
    state->rx_gap_flushed = level;
    return changed;
}

static bool mf_radio_rx_sample_level(MfRadioState* state, bool level, uint32_t now_ms) {
    uint32_t edge_ms;
    if(state->rx_candidate_samples == 0U) {
        state->rx_candidate_level = level;
        state->rx_candidate_samples = 1U;
        return false;
    }
    if(level == state->rx_candidate_level) {
        if(state->rx_candidate_samples < MF_RADIO_RX_STABLE_SAMPLES) state->rx_candidate_samples++;
    } else {
        if(state->rx_candidate_level && !state->rx_level &&
           state->rx_candidate_samples < MF_RADIO_RX_STABLE_SAMPLES)
            mf_radio_ticker_capture(
                &state->ticker,
                (uint16_t)(state->rx_candidate_samples * MF_RADIO_RX_SAMPLE_MS),
                now_ms,
                true);
        state->rx_candidate_level = level;
        state->rx_candidate_samples = 1U;
    }
    if(state->rx_candidate_samples < MF_RADIO_RX_STABLE_SAMPLES ||
       state->rx_candidate_level == state->rx_level)
        return false;
    edge_ms = now_ms - ((uint32_t)(state->rx_candidate_samples - 1U) * MF_RADIO_RX_SAMPLE_MS);
    return mf_radio_rx_feed_edge(state, state->rx_candidate_level, edge_ms);
}

static bool mf_radio_rx_flush_gap(MfRadioState* state, uint32_t now_ms) {
    uint32_t gap_ms;
    if(state->rx_level || state->rx_gap_flushed || state->rx_edge_at == 0U) return false;
    gap_ms = now_ms - state->rx_edge_at;
    if(gap_ms < ((uint32_t)mf_radio_rx_dit_ms(state) * 5U) / 2U) return false;
    state->decoder_services->feed_space(&state->decoder, mf_radio_duration_u16(gap_ms));
    state->rx_gap_flushed = true;
    return mf_radio_rx_drain_decoder(state);
}

static void mf_radio_rx_reset_cal_samples(MfRadioState* state) {
    state->rx_cal_settle_samples = 0U;
    state->rx_cal_samples = 0U;
}

static int8_t mf_radio_rx_cal_floor(MfRadioState* state) {
    uint8_t i;
    for(i = 1U; i < MF_RADIO_RX_CAL_SAMPLES; i++) {
        int8_t sample = state->rx_cal_dbm[i];
        uint8_t slot = i;
        while(slot != 0U && state->rx_cal_dbm[slot - 1U] > sample) {
            state->rx_cal_dbm[slot] = state->rx_cal_dbm[slot - 1U];
            slot--;
        }
        state->rx_cal_dbm[slot] = sample;
    }
    return (int8_t)(((int16_t)state->rx_cal_dbm[MF_RADIO_RX_CAL_SAMPLES / 2U - 1U] +
                     state->rx_cal_dbm[MF_RADIO_RX_CAL_SAMPLES / 2U]) /
                    2);
}

static int8_t mf_radio_rx_floor_threshold(int8_t floor_dbm) {
    return mf_radio_clamp_dbm_i16((int16_t)floor_dbm + MF_RADIO_RX_FLOOR_MARGIN_DB);
}

static int8_t mf_radio_rx_recovery_trigger(const MfRadioState* state) {
    return mf_radio_clamp_dbm_i16(
        (int16_t)state->rx_auto_threshold_dbm - MF_RADIO_RX_FLOOR_MARGIN_DB -
        MF_RADIO_RX_RECOVERY_DROP_DB);
}

static void mf_radio_rx_apply_threshold(MfRadioState* state, int8_t auto_threshold_dbm) {
    state->rx_auto_threshold_dbm = auto_threshold_dbm;
    state->snapshot.monitor_threshold_dbm =
        mf_radio_clamp_dbm_i16((int16_t)auto_threshold_dbm + state->rx_manual_threshold_offset_db);
}

static bool mf_radio_rx_take_cal_sample(MfRadioState* state, int8_t dbm) {
    if(state->rx_cal_settle_samples < MF_RADIO_RX_CAL_SETTLE_SAMPLES) {
        state->rx_cal_settle_samples++;
        return false;
    }
    state->rx_cal_dbm[state->rx_cal_samples++] = dbm;
    return state->rx_cal_samples == MF_RADIO_RX_CAL_SAMPLES;
}

bool mf_radio_core_enter(
    MfRadioState* state,
    const MfRadioEnterArgs* args,
    const MfRadioHardwareOps* hardware,
    MorseFlipperMappedFalResult* initial) {
    uint32_t frequency_hz;

    if(initial != NULL) *initial = (MorseFlipperMappedFalResult){0};
    if(state == NULL || args == NULL || args->struct_size != sizeof(MfRadioEnterArgs) ||
       args->dit_ms == 0U || !mf_radio_decoder_services_valid(args->decoder) ||
       !mf_radio_draw_services_valid(args->draw) || !mf_radio_hardware_ops_valid(hardware))
        return false;

    memset(state, 0, sizeof(*state));
    state->hardware = *hardware;
    state->decoder_services = args->decoder;
    state->draw_services = args->draw;
    frequency_hz = args->frequency_hz;
    if(!mf_radio_frequency_in_vfo(frequency_hz) ||
       !state->hardware.frequency_valid(state->hardware.context, frequency_hz)) {
        frequency_hz = state->hardware.default_frequency(state->hardware.context);
        if(!mf_radio_frequency_valid(state, frequency_hz))
            frequency_hz = MF_RADIO_DEFAULT_FREQUENCY_HZ;
        state->snapshot.frequency_dirty = true;
    }
    state->snapshot = (MfRadioSnapshot){
        .struct_size = sizeof(MfRadioSnapshot),
        .page = MfRadioPageIdle,
        .frequency_hz = frequency_hz,
        .monitor_threshold_dbm = mf_radio_clamp_dbm(args->monitor_threshold_dbm),
        .receive_audio_enabled = args->receive_audio_enabled,
        .frequency_dirty = state->snapshot.frequency_dirty,
        .tx_allowed = state->hardware.tx_allowed(state->hardware.context, frequency_hz),
    };
    state->configured_monitor_threshold_dbm = state->snapshot.monitor_threshold_dbm;
    state->rx_auto_threshold_dbm = state->snapshot.monitor_threshold_dbm;
    state->rx_wpm_hint = MF_RADIO_RX_DEFAULT_WPM;
    state->tx_dit_ms = args->dit_ms;
    mf_radio_reset_decoder(state, args->dit_ms);
    state->draw_services->history_reset(&state->tx_history);
    state->entered = true;
    if(initial != NULL) initial->redraw = true;
    return true;
}

MorseFlipperMappedFalResult
    mf_radio_core_set_page(MfRadioState* state, MfRadioPage page, uint32_t now_ms) {
    if(state == NULL || !state->entered || state->leaving || page > MfRadioPageFrequency)
        return (MorseFlipperMappedFalResult){0};

    mf_radio_quiesce(state);
    state->snapshot.page = MfRadioPageIdle;
    state->rx_level = false;
    state->rx_candidate_level = false;
    state->rx_candidate_samples = 0U;
    state->rx_gap_flushed = true;
    state->rx_calibrating = false;
    state->rx_recovery_pending = false;

    if(page == MfRadioPageTransmit) {
        state->snapshot.page = page;
        state->snapshot.tx_allowed =
            state->hardware.tx_allowed(state->hardware.context, state->snapshot.frequency_hz);
        mf_radio_reset_decoder(state, state->tx_dit_ms);
        state->draw_services->history_reset(&state->tx_history);
    } else if(page == MfRadioPageReceive) {
        state->snapshot.monitor_threshold_dbm = state->configured_monitor_threshold_dbm;
        state->rx_auto_threshold_dbm = state->configured_monitor_threshold_dbm;
        state->rx_manual_threshold_offset_db = 0;
        state->rssi_valid = false;
        state->rssi_dbm = 0;
        state->rssi_peak_dbm = 0;
        state->rssi_sum_dbm = 0;
        state->rssi_samples = 0U;
        state->rx_edges_window = 0U;
        state->rx_activity = 0U;
        state->rx_edge_at = 0U;
        state->rx_sample_next_at = now_ms;
        state->rx_view_next_at = 0U;
        state->rssi_next_at = 0U;
        state->rssi_peak_decay_at = 0U;
        state->carrier_present = false;
        state->rx_calibrating = true;
        state->rx_cal_carrier_continuous = true;
        state->rx_recovery_pending = false;
        mf_radio_rx_reset_cal_samples(state);
        mf_radio_reset_decoder(state, mf_radio_wpm_to_dit_ms(state->rx_wpm_hint));
        memset(state->rx_text, 0, sizeof(state->rx_text));
        memset(&state->ticker, 0, sizeof(state->ticker));
        if(!state->hardware.prepare_carrier_rx(
               state->hardware.context, state->snapshot.frequency_hz)) {
            mf_radio_rollback_prepare(state);
            return (MorseFlipperMappedFalResult){.handled = true, .redraw = true};
        }
        state->rx_prepared = true;
        state->snapshot.hardware_active = true;
        state->snapshot.page = page;
    } else if(page == MfRadioPageFrequency) {
        state->edit_khz = state->snapshot.frequency_hz / 1000U;
        state->edit_original_hz = state->snapshot.frequency_hz;
        state->frequency_focus = 0U;
        state->snapshot.page = page;
    }
    return (MorseFlipperMappedFalResult){.handled = true, .redraw = true};
}

MorseFlipperMappedFalResult mf_radio_core_sync_tx(
    MfRadioState* state,
    MfRadioTxInterval completed_interval,
    uint16_t duration_ms,
    bool level,
    uint32_t now_ms) {
    if(state == NULL || !state->entered || state->leaving ||
       state->snapshot.page != MfRadioPageTransmit)
        return (MorseFlipperMappedFalResult){0};

    if(completed_interval == MfRadioTxIntervalMark && duration_ms != 0U)
        state->decoder_services->feed_mark(&state->decoder, duration_ms);
    else if(completed_interval == MfRadioTxIntervalSpace && duration_ms != 0U)
        state->decoder_services->feed_space(&state->decoder, duration_ms);
    mf_radio_drain_decoder(state);

    state->snapshot.tx_allowed =
        state->hardware.tx_allowed(state->hardware.context, state->snapshot.frequency_hz);
    if(level && !state->snapshot.tx_allowed)
        return (MorseFlipperMappedFalResult){.handled = true, .redraw = true};
    if(level && !state->tx_prepared) {
        if(!state->hardware.prepare_tx(state->hardware.context, state->snapshot.frequency_hz)) {
            mf_radio_rollback_prepare(state);
            return (MorseFlipperMappedFalResult){.handled = true, .redraw = true};
        }
        state->tx_prepared = true;
        state->snapshot.hardware_active = true;
    }
    if(state->tx_prepared) state->hardware.set_tx_level(state->hardware.context, level);
    state->snapshot.tx_active = state->tx_prepared && level;
    if(state->snapshot.tx_active) {
        state->tx_idle_at = 0U;
    } else if(state->tx_prepared) {
        state->tx_idle_at = now_ms + ((uint32_t)state->tx_dit_ms * MF_RADIO_TX_TAIL_DITS);
        if(state->tx_idle_at == 0U) state->tx_idle_at = 1U;
    }
    return (MorseFlipperMappedFalResult){.handled = true, .redraw = true};
}

MorseFlipperMappedFalResult mf_radio_core_tick(MfRadioState* state, uint32_t now_ms) {
    if(state == NULL || !state->entered || state->leaving) return (MorseFlipperMappedFalResult){0};
    if(state->snapshot.page == MfRadioPageTransmit) {
        if(state->tx_prepared && !state->snapshot.tx_active && state->tx_idle_at != 0U &&
           (int32_t)(now_ms - state->tx_idle_at) >= 0) {
            mf_radio_quiesce(state);
            return (MorseFlipperMappedFalResult){.redraw = true};
        }
        return (MorseFlipperMappedFalResult){0};
    }
    if(state->snapshot.page != MfRadioPageReceive || !state->rx_prepared)
        return (MorseFlipperMappedFalResult){0};
    if(state->rx_sample_next_at == 0U) state->rx_sample_next_at = now_ms;
    if(now_ms < state->rx_sample_next_at) return (MorseFlipperMappedFalResult){0};
    do {
        state->rx_sample_next_at += MF_RADIO_RX_SAMPLE_MS;
    } while(now_ms >= state->rx_sample_next_at);

    {
        int8_t dbm = state->hardware.read_rssi_dbm(state->hardware.context);
        int8_t open_dbm = mf_radio_clamp_dbm(state->snapshot.monitor_threshold_dbm);
        int8_t close_dbm = mf_radio_clamp_dbm((int8_t)(open_dbm - MF_RADIO_RX_HYSTERESIS_DB));
        bool monitor = false;
        bool text_changed = false;
        state->carrier_present = state->hardware.read_carrier(state->hardware.context);
        if(state->rx_calibrating) {
            if(state->rx_cal_settle_samples >= MF_RADIO_RX_CAL_SETTLE_SAMPLES)
                state->rx_cal_carrier_continuous = state->rx_cal_carrier_continuous &&
                                                   state->carrier_present;
            if(mf_radio_rx_take_cal_sample(state, dbm)) {
                mf_radio_rx_apply_threshold(
                    state, mf_radio_rx_floor_threshold(mf_radio_rx_cal_floor(state)));
                state->rx_calibrating = false;
                state->rx_recovery_pending = state->rx_cal_carrier_continuous;
                mf_radio_rx_reset_cal_samples(state);
            }
        } else if(state->rx_recovery_pending && dbm < mf_radio_rx_recovery_trigger(state)) {
            if(state->rx_cal_settle_samples == 0U && state->rx_cal_samples == 0U) {
                state->rx_level = false;
                state->rx_candidate_level = false;
                state->rx_candidate_samples = 0U;
                state->rx_edge_at = 0U;
                state->rx_gap_flushed = true;
            }
            if(mf_radio_rx_take_cal_sample(state, dbm)) {
                int8_t recovered = mf_radio_rx_floor_threshold(mf_radio_rx_cal_floor(state));
                if(recovered < state->rx_auto_threshold_dbm)
                    mf_radio_rx_apply_threshold(state, recovered);
                state->rx_recovery_pending = false;
            }
        } else {
            if(state->rx_recovery_pending) mf_radio_rx_reset_cal_samples(state);
            monitor = dbm >=
                      ((state->rx_level || state->rx_candidate_level) ? close_dbm : open_dbm);
            text_changed = mf_radio_rx_sample_level(state, monitor, now_ms);
            text_changed = mf_radio_rx_flush_gap(state, now_ms) || text_changed;
        }
        state->rssi_sum_dbm += dbm;
        state->rssi_samples++;
        if(state->rssi_next_at == 0U) state->rssi_next_at = now_ms + MF_RADIO_RSSI_WINDOW_MS;
        if(now_ms >= state->rssi_next_at && state->rssi_samples != 0U) {
            int32_t count = state->rssi_samples;
            state->rssi_dbm = state->rssi_sum_dbm >= 0 ?
                                  (int8_t)((state->rssi_sum_dbm + count / 2) / count) :
                                  (int8_t)((state->rssi_sum_dbm - count / 2) / count);
            state->rx_activity = state->rx_edges_window;
            state->rssi_sum_dbm = 0;
            state->rssi_samples = 0U;
            state->rx_edges_window = 0U;
            state->rssi_next_at = now_ms + MF_RADIO_RSSI_WINDOW_MS;
            if(!state->rssi_valid || state->rssi_dbm > state->rssi_peak_dbm) {
                state->rssi_peak_dbm = state->rssi_dbm;
                state->rssi_peak_decay_at = now_ms + MF_RADIO_RSSI_PEAK_DECAY_MS;
            } else if(now_ms >= state->rssi_peak_decay_at && state->rssi_peak_dbm > state->rssi_dbm) {
                state->rssi_peak_dbm--;
                state->rssi_peak_decay_at = now_ms + MF_RADIO_RSSI_PEAK_DECAY_MS;
            }
            state->rssi_valid = true;
        }
        state->snapshot.monitor_tone = state->snapshot.receive_audio_enabled && state->rx_level;
        mf_radio_ticker_prune(&state->ticker, now_ms);
        if(state->rx_view_next_at == 0U || now_ms >= state->rx_view_next_at) {
            state->rx_view_next_at = now_ms + MF_RADIO_RX_VIEW_MS;
            return (MorseFlipperMappedFalResult){.redraw = true};
        }
        return (MorseFlipperMappedFalResult){.redraw = text_changed};
    }
}

static uint32_t mf_radio_edit_place(uint8_t focus) {
    static const uint32_t places[MF_RADIO_FREQ_DIGITS] = {100000U, 10000U, 1000U, 100U, 10U, 1U};
    return places[focus % MF_RADIO_FREQ_DIGITS];
}

static void mf_radio_change_digit(MfRadioState* state, int direction) {
    uint32_t place = mf_radio_edit_place(state->frequency_focus);
    uint32_t khz = state->edit_khz % 1000000U;
    uint8_t digit = (uint8_t)((khz / place) % 10U);
    uint8_t next = direction < 0 ? (uint8_t)((digit + 9U) % 10U) : (uint8_t)((digit + 1U) % 10U);
    state->edit_khz = (uint32_t)((int32_t)khz + ((int32_t)next - (int32_t)digit) * (int32_t)place);
}

static void mf_radio_commit_frequency(MfRadioState* state) {
    uint32_t frequency_hz = (state->edit_khz % 1000000U) * 1000U;
    if(!mf_radio_frequency_in_vfo(frequency_hz)) {
        state->edit_khz = state->edit_original_hz / 1000U;
        return;
    }
    if(!state->hardware.frequency_valid(state->hardware.context, frequency_hz)) {
        frequency_hz = state->hardware.default_frequency(state->hardware.context);
        if(!mf_radio_frequency_valid(state, frequency_hz))
            frequency_hz = MF_RADIO_DEFAULT_FREQUENCY_HZ;
    }
    if(state->snapshot.frequency_hz != frequency_hz) {
        state->snapshot.frequency_hz = frequency_hz;
        state->snapshot.frequency_dirty = true;
    }
    state->snapshot.tx_allowed =
        state->hardware.tx_allowed(state->hardware.context, state->snapshot.frequency_hz);
    state->edit_khz = state->snapshot.frequency_hz / 1000U;
}

MorseFlipperMappedFalResult
    mf_radio_core_input(MfRadioState* state, const InputEvent* event, uint32_t now_ms) {
    (void)now_ms;
    if(state == NULL || event == NULL || !state->entered || state->leaving)
        return (MorseFlipperMappedFalResult){0};
    if(state->snapshot.page != MfRadioPageTransmit && event->key == InputKeyBack &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        if(state->snapshot.page == MfRadioPageFrequency) mf_radio_commit_frequency(state);
        return (MorseFlipperMappedFalResult){
            .handled = true,
            .redraw = state->snapshot.frequency_dirty,
            .request_exit = true,
        };
    }
    if(state->snapshot.page == MfRadioPageTransmit) {
        if(event->key == InputKeyLeft && event->type == InputTypeShort) {
            mf_radio_reset_decoder(state, state->decoder_services->dit_ms(&state->decoder));
            state->draw_services->history_reset(&state->tx_history);
            return (MorseFlipperMappedFalResult){.handled = true, .redraw = true};
        }
        return (MorseFlipperMappedFalResult){0};
    }
    if(state->snapshot.page == MfRadioPageReceive) {
        if(event->key == InputKeyOk && event->type == InputTypeShort) {
            state->snapshot.receive_audio_enabled = !state->snapshot.receive_audio_enabled;
        } else if(
            (event->key == InputKeyLeft || event->key == InputKeyRight) &&
            (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
            int direction = event->key == InputKeyLeft ? -1 : 1;
            int8_t previous = state->snapshot.monitor_threshold_dbm;
            state->snapshot.monitor_threshold_dbm =
                mf_radio_clamp_dbm_i16((int16_t)previous + direction);
            state->rx_manual_threshold_offset_db +=
                state->snapshot.monitor_threshold_dbm - previous;
        } else if(
            (event->key == InputKeyUp || event->key == InputKeyDown) &&
            (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
            int next = state->rx_wpm_hint + (event->key == InputKeyUp ? 1 : -1);
            state->rx_wpm_hint = mf_radio_clamp_wpm((uint8_t)(next < 0 ? 0 : next));
            state->rx_level = false;
            state->rx_candidate_level = false;
            state->rx_candidate_samples = 0U;
            state->rx_edge_at = 0U;
            state->rx_gap_flushed = true;
            state->snapshot.monitor_tone = false;
            mf_radio_reset_decoder(state, mf_radio_wpm_to_dit_ms(state->rx_wpm_hint));
        }
        state->snapshot.monitor_tone = state->snapshot.receive_audio_enabled && state->rx_level;
        return (MorseFlipperMappedFalResult){.handled = true, .redraw = true};
    }
    if(state->snapshot.page == MfRadioPageFrequency &&
       (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        if(event->key == InputKeyLeft)
            state->frequency_focus = state->frequency_focus == 0U ?
                                         MF_RADIO_FREQ_DIGITS - 1U :
                                         (uint8_t)(state->frequency_focus - 1U);
        else if(event->key == InputKeyRight)
            state->frequency_focus =
                (uint8_t)((state->frequency_focus + 1U) % MF_RADIO_FREQ_DIGITS);
        else if(event->key == InputKeyUp)
            mf_radio_change_digit(state, 1);
        else if(event->key == InputKeyDown)
            mf_radio_change_digit(state, -1);
        return (MorseFlipperMappedFalResult){.handled = true, .redraw = true};
    }
    return (MorseFlipperMappedFalResult){.handled = true};
}

void mf_radio_core_leave(MfRadioState* state) {
    if(state == NULL || !state->entered || state->leaving) return;
    state->leaving = true;
    mf_radio_quiesce(state);
    state->snapshot.page = MfRadioPageIdle;
}

bool mf_radio_core_snapshot(const MfRadioState* state, MfRadioSnapshot* snapshot) {
    if(state == NULL || snapshot == NULL || snapshot->struct_size != sizeof(MfRadioSnapshot))
        return false;
    *snapshot = state->snapshot;
    return true;
}
