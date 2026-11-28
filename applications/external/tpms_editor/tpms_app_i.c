#include "tpms_app_i.h"

#define TAG "TPMS"
#include <flipper_format/flipper_format_i.h>

void tpms_preset_init(
    void* context,
    const char* preset_name,
    uint32_t frequency,
    uint8_t* preset_data,
    size_t preset_data_size) {
    furi_assert(context);
    TPMSApp* app = context;
    furi_string_set(app->txrx->preset->name, preset_name);
    app->txrx->preset->frequency = frequency;
    app->txrx->preset->data = preset_data;
    app->txrx->preset->data_size = preset_data_size;
}

bool tpms_set_preset(TPMSApp* app, const char* preset) {
    if(!strcmp(preset, "FuriHalSubGhzPresetOok270Async")) {
        furi_string_set(app->txrx->preset->name, "AM270");
    } else if(!strcmp(preset, "FuriHalSubGhzPresetOok650Async")) {
        furi_string_set(app->txrx->preset->name, "AM650");
    } else if(!strcmp(preset, "FuriHalSubGhzPreset2FSKDev238Async")) {
        furi_string_set(app->txrx->preset->name, "FM238");
    } else if(!strcmp(preset, "FuriHalSubGhzPreset2FSKDev476Async")) {
        furi_string_set(app->txrx->preset->name, "FM476");
    } else if(!strcmp(preset, "FuriHalSubGhzPresetCustom")) {
        furi_string_set(app->txrx->preset->name, "CUSTOM");
    } else {
        FURI_LOG_E(TAG, "Unknown preset");
        return false;
    }
    return true;
}

void tpms_get_frequency_modulation(TPMSApp* app, FuriString* frequency, FuriString* modulation) {
    furi_assert(app);
    if(frequency != NULL) {
        furi_string_printf(
            frequency,
            "%03ld.%02ld",
            app->txrx->preset->frequency / 1000000 % 1000,
            app->txrx->preset->frequency / 10000 % 100);
    }
    if(modulation != NULL) {
        const char* name = furi_string_get_cstr(app->txrx->preset->name);
        if(strcmp(name, "TPMS OOK") == 0) furi_string_set_str(modulation, "PO");
        else if(strcmp(name, "TPMS FSK") == 0) furi_string_set_str(modulation, "PF");
        else if(strcmp(name, "TPMS GFSK") == 0) furi_string_set_str(modulation, "PG");
        else furi_string_printf(modulation, "%.2s", name);
    }
}

void tpms_begin(TPMSApp* app, uint8_t* preset_data) {
    furi_assert(app);
    furi_assert(preset_data);
    furi_check(tpms_radio_ensure_selected(app));
    furi_check(app->txrx->radio_device);

    subghz_devices_reset(app->txrx->radio_device);
    subghz_devices_idle(app->txrx->radio_device);
    subghz_devices_load_preset(
        app->txrx->radio_device, FuriHalSubGhzPresetCustom, preset_data);
    app->txrx->txrx_state = TPMSTxRxStateIDLE;
}

uint32_t tpms_rx(TPMSApp* app, uint32_t frequency) {
    furi_assert(app);
    /* Device selection and preset loading are an atomic preparation step in
     * tpms_begin(). Do not switch radios between loading the preset and RX. */
    furi_check(app->txrx->radio_device);

    if(!subghz_devices_is_frequency_valid(app->txrx->radio_device, frequency)) {
        furi_crash("TPMS: Incorrect RX frequency.");
    }
    furi_assert(
        app->txrx->txrx_state != TPMSTxRxStateRx && app->txrx->txrx_state != TPMSTxRxStateSleep);

    subghz_devices_idle(app->txrx->radio_device);
    uint32_t value = subghz_devices_set_frequency(app->txrx->radio_device, frequency);
    subghz_devices_flush_rx(app->txrx->radio_device);

    /* The device abstraction preserves ProtoView's direct edge callback for
     * both the internal CC1101 and the external CC1101 driver. */
    subghz_devices_start_async_rx(
        app->txrx->radio_device, proto6_bridge_rx_callback, app->txrx->proto6);
    app->txrx->txrx_state = TPMSTxRxStateRx;
    return value;
}

void tpms_idle(TPMSApp* app) {
    furi_assert(app);
    furi_assert(app->txrx->txrx_state != TPMSTxRxStateSleep);
    if(app->txrx->radio_device) subghz_devices_idle(app->txrx->radio_device);
    app->txrx->txrx_state = TPMSTxRxStateIDLE;
}

void tpms_rx_end(TPMSApp* app) {
    furi_assert(app);
    furi_assert(app->txrx->txrx_state == TPMSTxRxStateRx);
    furi_check(app->txrx->radio_device);
    subghz_devices_stop_async_rx(app->txrx->radio_device);
    subghz_devices_idle(app->txrx->radio_device);
    app->txrx->txrx_state = TPMSTxRxStateIDLE;
}

void tpms_sleep(TPMSApp* app) {
    furi_assert(app);
    if(app->txrx->radio_device) subghz_devices_sleep(app->txrx->radio_device);
    app->txrx->txrx_state = TPMSTxRxStateSleep;
}

void tpms_hopper_update(TPMSApp* app) {
    furi_assert(app);
    /* Modulation hopping is deliberately disabled. ProtoView uses one exact
     * CC1101 preset at a time: TPMS FSK, TPMS OOK or TPMS GFSK. */
    app->txrx->hopper_state = TPMSHopperStateOFF;
}
