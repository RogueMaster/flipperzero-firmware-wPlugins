#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "plugins/radio/mf_radio_api.h"

#define APP_ASSETS_PATH(path) path
#define FuriWaitForever       0U

typedef struct {
    int unused;
} FuriMutex;

typedef enum {
    MorseFlipperPluginOwnerNone = 0,
    MorseFlipperPluginOwnerRadio = 7,
} MorseFlipperPluginOwner;

typedef enum {
    MorseFlipperPluginErrorNone = 0,
    MorseFlipperPluginErrorLoad = 2,
} MorseFlipperPluginError;

typedef struct {
    FuriMutex* mutex;
    void* manager;
    const void* api;
    void* state;
    uint8_t owner;
    uint8_t error;
} MorseFlipperPluginSlot;

typedef struct {
    uint8_t owner;
    bool active;
} MorseFlipperPluginSnapshot;

enum {
    MorseFlipperScreenRf = 5,
};

typedef struct MorseFlipperApp {
    MorseFlipperPluginSlot plugin_slot;
    MfRadioDrawServices radio_draw_services;
    uint32_t rf_frequency_hz;
    int8_t rf_monitor_threshold_dbm;
    bool rf_rx_audio_enabled;
    bool radio_load_error;
    bool radio_tx_allowed;
    bool radio_tx_active;
    bool radio_monitor_tone;
    uint8_t screen;
} MorseFlipperApp;

void furi_mutex_acquire(FuriMutex* mutex, uint32_t timeout);
void furi_mutex_release(FuriMutex* mutex);
uint16_t morse_flipper_current_dit_ms(const MorseFlipperApp* app);
const MfRadioDecoderServices* morse_flipper_radio_decoder_services(void);
void morse_flipper_run_history_reset(MorseFlipperRunHistory* history);
void morse_flipper_run_history_append(MorseFlipperRunHistory* history, const char* text);
void morse_flipper_draw_tx_history_supplied(
    void* context,
    Canvas* canvas,
    const MorseFlipperRunHistory* history,
    uint8_t preview,
    bool preview_extendable,
    const char* frequency_line);
void morse_flipper_draw_radio_rx_text(
    void* context,
    Canvas* canvas,
    const char* text,
    uint8_t preview,
    bool preview_extendable);
bool morse_flipper_plugin_runtime_open_mapped_locked(
    MorseFlipperApp* app,
    MorseFlipperPluginOwner owner,
    uint8_t mode,
    const char* path,
    uint32_t api_version,
    uint32_t api_magic,
    uint32_t minimum_api_size,
    const void* enter_args,
    MorseFlipperMappedFalResult* initial);
void morse_flipper_plugin_runtime_release_claim_locked(
    MorseFlipperApp* app,
    MorseFlipperPluginOwner owner);
void morse_flipper_plugin_runtime_detach_locked(
    MorseFlipperApp* app,
    MorseFlipperPluginOwner owner);
bool morse_flipper_plugin_runtime_snapshot(
    const MorseFlipperApp* app,
    MorseFlipperPluginSnapshot* snapshot);
void morse_flipper_update_sidetone(MorseFlipperApp* app);
void morse_flipper_sync_ptt(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_save_config(const MorseFlipperApp* app);
void morse_flipper_view_dirty(MorseFlipperApp* app);
void morse_flipper_release_all_notes(MorseFlipperApp* app);
void morse_flipper_handle_active_keying_event(MorseFlipperApp* app, const InputEvent* event);
void morse_flipper_scene_back(MorseFlipperApp* app);
void morse_flipper_draw_plugin_unavailable(Canvas* canvas);

bool morse_flipper_radio_host_open(MorseFlipperApp* app, uint32_t now_ms);
bool morse_flipper_radio_host_active(const MorseFlipperApp* app);
bool morse_flipper_radio_host_set_page(MorseFlipperApp* app, MfRadioPage page, uint32_t now_ms);
bool morse_flipper_radio_host_input(MorseFlipperApp* app, const InputEvent* event, uint32_t now_ms);
void morse_flipper_radio_host_tick(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_radio_host_sync_tx(
    MorseFlipperApp* app,
    MfRadioTxInterval completed_interval,
    uint16_t duration_ms,
    bool level,
    uint32_t now_ms);
void morse_flipper_radio_host_draw(MorseFlipperApp* app, Canvas* canvas, uint32_t now_ms);
void morse_flipper_radio_host_close(MorseFlipperApp* app, uint32_t now_ms);
