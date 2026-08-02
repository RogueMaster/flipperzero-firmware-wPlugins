#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../../morse_flipper_cw_decoder.h"
#include "../../morse_flipper_mapped_fal.h"
#include "../../morse_flipper_run_history.h"
#include "mf_radio_types.h"

#define MF_RADIO_API_MAGIC   0x4D465246UL
#define MF_RADIO_API_VERSION 3U

typedef struct {
    uint32_t struct_size;
    void (*init)(MorseFlipperCwDecoder* decoder, uint16_t starting_dit_ms);
    void (*feed_mark)(MorseFlipperCwDecoder* decoder, uint16_t ms);
    void (*feed_space)(MorseFlipperCwDecoder* decoder, uint16_t ms);
    uint16_t (*dit_ms)(const MorseFlipperCwDecoder* decoder);
    const char* (*output)(const MorseFlipperCwDecoder* decoder);
    void (*clear_output)(MorseFlipperCwDecoder* decoder);
    uint8_t (*preview)(const MorseFlipperCwDecoder* decoder);
    bool (*preview_extendable)(const MorseFlipperCwDecoder* decoder);
} MfRadioDecoderServices;

typedef struct {
    uint32_t struct_size;
    void* context;
    void (*history_reset)(MorseFlipperRunHistory* history);
    void (*history_append)(MorseFlipperRunHistory* history, const char* text);
    void (*draw_tx_history)(
        void* context,
        Canvas* canvas,
        const MorseFlipperRunHistory* history,
        uint8_t preview,
        bool preview_extendable,
        const char* frequency_line);
    void (*draw_rx_text)(
        void* context,
        Canvas* canvas,
        const char* text,
        uint8_t preview,
        bool preview_extendable);
} MfRadioDrawServices;

typedef struct {
    uint32_t struct_size;
    uint32_t now_ms;
    uint32_t frequency_hz;
    uint16_t dit_ms;
    int8_t monitor_threshold_dbm;
    bool receive_audio_enabled;
    const MfRadioDecoderServices* decoder;
    const MfRadioDrawServices* draw;
} MfRadioEnterArgs;

typedef struct {
    uint32_t struct_size;
    MfRadioPage page;
    uint32_t frequency_hz;
    int8_t monitor_threshold_dbm;
    bool receive_audio_enabled;
    bool frequency_dirty;
    bool tx_allowed;
    bool hardware_active;
    bool tx_active;
    bool monitor_tone;
} MfRadioSnapshot;

typedef struct {
    MfRadioTxInterval completed_interval;
    uint16_t duration_ms;
    bool level;
} MfRadioSyncTxCommand;

typedef enum {
    MfRadioCommandSnapshot = 0,
    MfRadioCommandSetPage,
    MfRadioCommandSyncTx,
} MfRadioCommand;

typedef struct {
    MorseFlipperCommandFalApi fal;
} MfRadioApi;

static inline bool mf_radio_decoder_services_valid(const MfRadioDecoderServices* services) {
    return services != NULL && services->struct_size == sizeof(MfRadioDecoderServices) &&
           services->init != NULL && services->feed_mark != NULL && services->feed_space != NULL &&
           services->dit_ms != NULL && services->output != NULL &&
           services->clear_output != NULL && services->preview != NULL &&
           services->preview_extendable != NULL;
}

static inline bool mf_radio_draw_services_valid(const MfRadioDrawServices* services) {
    return services != NULL && services->struct_size == sizeof(MfRadioDrawServices) &&
           services->history_reset != NULL && services->history_append != NULL &&
           services->draw_tx_history != NULL && services->draw_rx_text != NULL;
}

static inline bool mf_radio_api_valid(const MfRadioApi* api) {
    const MorseFlipperMappedFalApi* mapped = api != NULL ? &api->fal.mapped : NULL;
    return mapped != NULL && mapped->magic == MF_RADIO_API_MAGIC &&
           mapped->api_version == MF_RADIO_API_VERSION &&
           mapped->struct_size == sizeof(MfRadioApi) && mapped->alloc != NULL &&
           mapped->free != NULL && mapped->enter != NULL && mapped->leave != NULL &&
           mapped->input != NULL && mapped->tick != NULL && mapped->draw != NULL &&
           api->fal.command != NULL;
}
