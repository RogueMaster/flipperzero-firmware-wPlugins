#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool (*prepare_tx)(void* context, uint32_t frequency_hz);
    bool (*prepare_carrier_rx)(void* context, uint32_t frequency_hz);
    void (*set_tx_level)(void* context, bool level);
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
