#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <core/string.h>
#include <lib/subghz/subghz_setting.h>

typedef struct ProtoPirateApp ProtoPirateApp;

typedef struct ProtoPirateApp ProtoPirateApp;
typedef struct ProtoPirateTxRx ProtoPirateTxRx;

void protopirate_rx_stack_teardown_for_registry_switch(ProtoPirateApp* app);

void protopirate_preset_init(
    void* context,
    const char* preset_name,
    uint32_t frequency,
    uint8_t* preset_data,
    size_t preset_data_size);

void protopirate_get_frequency_modulation(
    ProtoPirateApp* app,
    FuriString* frequency,
    size_t frequency_size,
    FuriString* modulation,
    size_t modulation_size);

void protopirate_get_frequency_modulation_str(
    ProtoPirateApp* app,
    char* frequency,
    size_t frequency_size,
    char* modulation,
    size_t modulation_size);

void protopirate_begin(ProtoPirateApp* app, uint8_t* preset_data);
uint32_t protopirate_rx(ProtoPirateApp* app, uint32_t frequency);
void protopirate_idle(ProtoPirateApp* app);
void protopirate_rx_end(ProtoPirateApp* app);
void protopirate_sleep(ProtoPirateApp* app);
bool protopirate_hopper_update(ProtoPirateApp* app);
void protopirate_tx(ProtoPirateApp* app, uint32_t frequency);
void protopirate_tx_stop(ProtoPirateApp* app);
void protopirate_release_shared_radio_state(ProtoPirateApp* app);
void protopirate_rx_stack_suspend_for_tx(ProtoPirateApp* app);
void protopirate_rx_stack_resume_after_tx(ProtoPirateApp* app);

const char* subghz_txrx_radio_device_get_name(ProtoPirateTxRx* instance);
bool subghz_txrx_radio_device_is_frequency_valid(ProtoPirateTxRx* instance, uint32_t frequency);
