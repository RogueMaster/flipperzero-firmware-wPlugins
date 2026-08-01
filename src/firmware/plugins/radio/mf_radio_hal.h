#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MfRadioTxModeOok = 0,
    MfRadioTxModeCwfm,
} MfRadioTxMode;

#define MF_RADIO_CWFM_DEVIATION_HZ 2380U

typedef struct {
    uint8_t remainder;
    bool phase;
} MfRadioCwfmTiming;

typedef struct {
    uint32_t frequency_hz;
    bool data_level;
} MfRadioCwfmStaticConfig;

typedef bool (*MfRadioFrequencyPredicate)(uint32_t frequency_hz);

typedef struct {
    bool (*prepare_tx)(void* context, uint32_t frequency_hz, MfRadioTxMode mode);
    bool (*prepare_carrier_rx)(void* context, uint32_t frequency_hz);
    bool (*set_tx_level)(void* context, bool level);
    void (*stop_tx)(void* context);
    bool (*read_carrier)(void* context);
    int8_t (*read_rssi_dbm)(void* context);
    bool (*frequency_valid)(void* context, uint32_t frequency_hz);
    bool (*tx_allowed)(void* context, uint32_t frequency_hz);
    uint32_t (*default_frequency)(void* context);
    void (*idle)(void* context);
    void (*sleep)(void* context);
    void* context;
} MfRadioHardwareOps;

bool mf_radio_hardware_ops_valid(const MfRadioHardwareOps* ops);
const MfRadioHardwareOps* mf_radio_hal_ops(void);
bool mf_radio_cwfm_static_config(
    uint32_t selected_frequency_hz,
    MfRadioFrequencyPredicate frequency_valid,
    MfRadioFrequencyPredicate frequency_allowed,
    MfRadioCwfmStaticConfig* config);
void mf_radio_cwfm_timing_reset(MfRadioCwfmTiming* timing);
uint16_t mf_radio_cwfm_next_half_period(MfRadioCwfmTiming* timing, bool* level);
