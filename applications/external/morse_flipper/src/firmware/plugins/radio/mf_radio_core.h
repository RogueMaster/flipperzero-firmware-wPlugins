#pragma once

#include "mf_radio_api.h"
#include "mf_radio_hal.h"
#include "../common/mf_radio_tx_session.h"

#define MF_RADIO_FREQ_DIGITS           6U
#define MF_RADIO_RX_DEFAULT_WPM        15U
#define MF_RADIO_RX_WPM_MIN            2U
#define MF_RADIO_RX_WPM_MAX            25U
#define MF_RADIO_RX_SAMPLE_MS          8U
#define MF_RADIO_RX_STABLE_SAMPLES     4U
#define MF_RADIO_RX_VIEW_MS            32U
#define MF_RADIO_RX_TICKER_WINDOW_MS   4000U
#define MF_RADIO_RX_TICKER_CAPACITY    64U
#define MF_RADIO_RX_HYSTERESIS_DB      1
#define MF_RADIO_RX_AUTO_WPM_SAMPLES   3U
#define MF_RADIO_RSSI_WINDOW_MS        160U
#define MF_RADIO_RSSI_PEAK_DECAY_MS    240U
/* Ignore eight startup samples, then use a 16-sample median plus a 6 dB margin. */
#define MF_RADIO_RX_CAL_SETTLE_SAMPLES 8U
#define MF_RADIO_RX_CAL_SAMPLES        16U
#define MF_RADIO_RX_FLOOR_MARGIN_DB    6
#define MF_RADIO_RX_RECOVERY_DROP_DB   3
#define MF_RADIO_TX_TAIL_DITS          2U
#define MF_RADIO_CWFM_HANG_DITS        7U
#define MF_RADIO_CWFM_HANG_MIN_MS      500U

typedef struct {
    uint32_t end_ms;
    uint16_t duration_ms;
    bool glitch;
} MfRadioTickerMark;

typedef struct {
    MfRadioTickerMark marks[MF_RADIO_RX_TICKER_CAPACITY];
    uint8_t start;
    uint8_t count;
} MfRadioTicker;

typedef struct {
    MfRadioSnapshot snapshot;
    MfRadioHardwareOps hardware;
    MfRadioTxSession tx_session;
    const MfRadioDecoderServices* decoder_services;
    const MfRadioDrawServices* draw_services;
    MorseFlipperCwDecoder decoder;
    MorseFlipperRunHistory tx_history;
    MfRadioTicker ticker;
    uint32_t edit_khz;
    uint32_t edit_original_hz;
    uint32_t tx_idle_at;
    uint32_t rx_edge_at;
    uint32_t rx_sample_next_at;
    uint32_t rx_view_next_at;
    uint32_t rssi_next_at;
    uint32_t rssi_peak_decay_at;
    int32_t rssi_sum_dbm;
    uint16_t rssi_samples;
    uint16_t rx_edges_window;
    uint16_t rx_activity;
    uint16_t tx_dit_ms;
    MfRadioTxMode tx_mode;
    uint8_t frequency_focus;
    uint8_t rx_candidate_samples;
    uint8_t rx_wpm_hint;
    uint8_t rx_cal_settle_samples;
    uint8_t rx_cal_samples;
    int8_t rx_cal_dbm[MF_RADIO_RX_CAL_SAMPLES];
    int8_t configured_monitor_threshold_dbm;
    int8_t rx_auto_threshold_dbm;
    int8_t rx_manual_threshold_offset_db;
    int8_t rssi_dbm;
    int8_t rssi_peak_dbm;
    bool entered;
    bool leaving;
    bool tx_prepared;
    bool tx_idle_pending;
    bool rx_prepared;
    bool rx_level;
    bool rx_candidate_level;
    bool rx_gap_flushed;
    bool rssi_valid;
    bool carrier_present;
    bool rx_calibrating;
    bool rx_cal_carrier_continuous;
    bool rx_recovery_pending;
    char rx_text[64];
} MfRadioState;

bool mf_radio_core_enter(
    MfRadioState* state,
    const MfRadioEnterArgs* args,
    const MfRadioHardwareOps* hardware,
    MorseFlipperMappedFalResult* initial);
MorseFlipperMappedFalResult
    mf_radio_core_set_page(MfRadioState* state, MfRadioPage page, uint32_t now_ms);
MorseFlipperMappedFalResult mf_radio_core_sync_tx(
    MfRadioState* state,
    MfRadioTxInterval completed_interval,
    uint16_t duration_ms,
    bool level,
    uint32_t now_ms);
MorseFlipperMappedFalResult mf_radio_core_tick(MfRadioState* state, uint32_t now_ms);
MorseFlipperMappedFalResult
    mf_radio_core_input(MfRadioState* state, const InputEvent* event, uint32_t now_ms);
void mf_radio_core_leave(MfRadioState* state);
bool mf_radio_core_snapshot(const MfRadioState* state, MfRadioSnapshot* snapshot);

bool mf_radio_frequency_in_vfo(uint32_t frequency_hz);
uint8_t mf_radio_clamp_wpm(uint8_t wpm);
int8_t mf_radio_clamp_dbm(int8_t dbm);
uint16_t mf_radio_wpm_to_dit_ms(uint8_t wpm);
uint16_t mf_radio_decoder_mark_ms(uint16_t duration_ms, uint16_t dit_ms);
