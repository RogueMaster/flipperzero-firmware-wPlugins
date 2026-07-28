#include "mf_radio_core.h"

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
           ops->set_tx_level != NULL && ops->read_carrier != NULL &&
           ops->read_rssi_dbm != NULL && ops->frequency_valid != NULL &&
           ops->tx_allowed != NULL && ops->default_frequency != NULL && ops->idle != NULL &&
           ops->sleep != NULL;
}

bool mf_radio_frequency_in_vfo(uint32_t frequency_hz) {
    size_t i;
    for(i = 0U; i < sizeof(vfo_bands) / sizeof(vfo_bands[0]); i++) {
        if(frequency_hz >= vfo_bands[i].min_hz && frequency_hz <= vfo_bands[i].max_hz)
            return true;
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
    if(state == NULL) return;
    if(state->tx_prepared)
        state->hardware.set_tx_level(state->hardware.context, false);
    if(state->snapshot.hardware_active)
        state->hardware.idle(state->hardware.context);
    state->tx_prepared = false;
    state->rx_prepared = false;
    state->snapshot.hardware_active = false;
    state->snapshot.tx_active = false;
    state->snapshot.monitor_tone = false;
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
        if(!mf_radio_frequency_valid(state, frequency_hz)) frequency_hz = MF_RADIO_DEFAULT_FREQUENCY_HZ;
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
    state->rx_wpm_hint = MF_RADIO_RX_DEFAULT_WPM;
    mf_radio_reset_decoder(state, args->dit_ms);
    state->draw_services->history_reset(&state->tx_history);
    state->entered = true;
    if(initial != NULL) initial->redraw = true;
    return true;
}

MorseFlipperMappedFalResult
    mf_radio_core_set_page(MfRadioState* state, MfRadioPage page, uint32_t now_ms) {
    (void)now_ms;
    if(state == NULL || !state->entered || state->leaving || page > MfRadioPageFrequency)
        return (MorseFlipperMappedFalResult){0};

    mf_radio_quiesce(state);
    state->snapshot.page = MfRadioPageIdle;
    state->rx_level = false;
    state->rx_candidate_level = false;
    state->rx_candidate_samples = 0U;
    state->rx_gap_flushed = true;

    if(page == MfRadioPageTransmit) {
        state->snapshot.page = page;
        state->snapshot.tx_allowed = state->hardware.tx_allowed(
            state->hardware.context, state->snapshot.frequency_hz);
        mf_radio_reset_decoder(state, state->decoder_services->dit_ms(&state->decoder));
        state->draw_services->history_reset(&state->tx_history);
    } else if(page == MfRadioPageReceive) {
        mf_radio_reset_decoder(state, mf_radio_wpm_to_dit_ms(state->rx_wpm_hint));
        memset(state->rx_text, 0, sizeof(state->rx_text));
        memset(&state->ticker, 0, sizeof(state->ticker));
        if(!state->hardware.prepare_carrier_rx(
               state->hardware.context, state->snapshot.frequency_hz))
            return (MorseFlipperMappedFalResult){.handled = true, .redraw = true};
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
    (void)now_ms;
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
        if(!state->hardware.prepare_tx(state->hardware.context, state->snapshot.frequency_hz))
            return (MorseFlipperMappedFalResult){.handled = true, .redraw = true};
        state->tx_prepared = true;
        state->snapshot.hardware_active = true;
    }
    if(state->tx_prepared) state->hardware.set_tx_level(state->hardware.context, level);
    state->snapshot.tx_active = state->tx_prepared && level;
    return (MorseFlipperMappedFalResult){.handled = true, .redraw = true};
}

MorseFlipperMappedFalResult mf_radio_core_tick(MfRadioState* state, uint32_t now_ms) {
    (void)now_ms;
    if(state == NULL || !state->entered || state->leaving)
        return (MorseFlipperMappedFalResult){0};
    return (MorseFlipperMappedFalResult){0};
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

