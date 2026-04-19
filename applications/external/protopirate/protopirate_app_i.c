// protopirate_app_i.c
#include "protopirate_app_i.h"

#define TAG "ProtoPirateTxRx"

void protopirate_preset_init(
    void* context,
    const char* preset_name,
    uint32_t frequency,
    uint8_t* preset_data,
    size_t preset_data_size) {
    furi_check(context);
    ProtoPirateApp* app = context;
    furi_string_set(app->txrx->preset->name, preset_name);
    app->txrx->preset->frequency = frequency;
    app->txrx->preset->data = preset_data;
    app->txrx->preset->data_size = preset_data_size;
}

// Convert full FuriHal preset name to short name used by settings
const char* preset_name_to_short(const char* preset_name) {
    if(!preset_name) return "AM650";

    // Check for full FuriHal names
    if(strstr(preset_name, "Ook650") || strstr(preset_name, "OOK650")) return "AM650";
    if(strstr(preset_name, "Ook270") || strstr(preset_name, "OOK270")) return "AM270";
    if(strstr(preset_name, "2FSKDev238") || strstr(preset_name, "Dev238")) return "FM238";
    if(strstr(preset_name, "2FSKDev12K") || strstr(preset_name, "Dev12K")) return "FM12K";
    if(strstr(preset_name, "2FSKDev476") || strstr(preset_name, "Dev476")) return "FM476";

    // Check for short names already
    if(strcmp(preset_name, "AM650") == 0) return "AM650";
    if(strcmp(preset_name, "AM270") == 0) return "AM270";
    if(strcmp(preset_name, "FM238") == 0) return "FM238";
    if(strcmp(preset_name, "FM12K") == 0) return "FM12K";
    if(strcmp(preset_name, "FM476") == 0) return "FM476";
    if(strcmp(preset_name, "FuriHalSubGhzPresetCustom") == 0) return "Custom";

    // Default fallback
    return "AM650";
}

void protopirate_get_frequency_modulation(
    ProtoPirateApp* app,
    FuriString* frequency,
    FuriString* modulation) {
    furi_check(app);
    if(frequency != NULL) {
        unsigned long mhz = (unsigned long)((app->txrx->preset->frequency / 1000000UL) % 1000UL);
        unsigned long khz = (unsigned long)((app->txrx->preset->frequency / 10000UL) % 100UL);
        furi_string_printf(frequency, "%03lu.%02lu", mhz, khz);
    }
    if(modulation != NULL) {
        furi_string_printf(modulation, "%.2s", furi_string_get_cstr(app->txrx->preset->name));
    }
}

void protopirate_begin(ProtoPirateApp* app, uint8_t* preset_data) {
    furi_check(app);
    subghz_devices_reset(app->txrx->radio_device);
    subghz_devices_idle(app->txrx->radio_device);
    subghz_devices_load_preset(app->txrx->radio_device, FuriHalSubGhzPresetCustom, preset_data);
    app->txrx->txrx_state = ProtoPirateTxRxStateIDLE;
}

uint32_t protopirate_rx(ProtoPirateApp* app, uint32_t frequency) {
    furi_check(app);
    furi_check(app->txrx);
    furi_check(app->radio_initialized);
    furi_check(app->txrx->radio_device);
    furi_check(app->txrx->worker);

    if(!subghz_devices_is_frequency_valid(app->txrx->radio_device, frequency)) {
        furi_crash("ProtoPirate: Incorrect RX frequency.");
    }
    furi_check(
        app->txrx->txrx_state != ProtoPirateTxRxStateRx &&
        app->txrx->txrx_state != ProtoPirateTxRxStateSleep);

    subghz_devices_idle(app->txrx->radio_device);
    uint32_t value = subghz_devices_set_frequency(app->txrx->radio_device, frequency);
    subghz_devices_flush_rx(app->txrx->radio_device);
    subghz_devices_set_rx(app->txrx->radio_device);

    subghz_devices_start_async_rx(
        app->txrx->radio_device, subghz_worker_rx_callback, app->txrx->worker);

    subghz_worker_start(app->txrx->worker);
    app->txrx->txrx_state = ProtoPirateTxRxStateRx;
    return value;
}

void protopirate_idle(ProtoPirateApp* app) {
    furi_check(app);
    furi_check(app->txrx->txrx_state != ProtoPirateTxRxStateSleep);
    subghz_devices_idle(app->txrx->radio_device);
    app->txrx->txrx_state = ProtoPirateTxRxStateIDLE;
}

void protopirate_rx_end(ProtoPirateApp* app) {
    furi_check(app);
    furi_check(app->txrx->txrx_state == ProtoPirateTxRxStateRx);
    if(subghz_worker_is_running(app->txrx->worker)) {
        subghz_worker_stop(app->txrx->worker);
        subghz_devices_stop_async_rx(app->txrx->radio_device);
    }
    subghz_devices_idle(app->txrx->radio_device);
    app->txrx->txrx_state = ProtoPirateTxRxStateIDLE;
}

void protopirate_sleep(ProtoPirateApp* app) {
    furi_check(app);
    subghz_devices_sleep(app->txrx->radio_device);
    app->txrx->txrx_state = ProtoPirateTxRxStateSleep;
}

void protopirate_rx_stack_suspend_for_tx(ProtoPirateApp* app) {
    if(!app || !app->radio_initialized) {
        return;
    }

    if(app->txrx->txrx_state == ProtoPirateTxRxStateRx) {
        protopirate_rx_end(app);
    }

    if(app->txrx->receiver) {
        subghz_receiver_set_rx_callback(app->txrx->receiver, NULL, NULL);
        subghz_receiver_free(app->txrx->receiver);
        app->txrx->receiver = NULL;
    }

    if(app->txrx->worker) {
        subghz_worker_free(app->txrx->worker);
        app->txrx->worker = NULL;
    }

    if(app->txrx->radio_device && app->txrx->txrx_state != ProtoPirateTxRxStateTx) {
        subghz_devices_idle(app->txrx->radio_device);
        app->txrx->txrx_state = ProtoPirateTxRxStateIDLE;
    }
}

void protopirate_rx_stack_resume_after_tx(ProtoPirateApp* app) {
    if(!app || !app->radio_initialized || !app->txrx->environment) {
        return;
    }
    if(app->txrx->receiver) {
        return;
    }

    app->txrx->receiver = subghz_receiver_alloc_init(app->txrx->environment);
    if(!app->txrx->receiver) {
        FURI_LOG_E(TAG, "rx_stack_resume: subghz_receiver_alloc_init failed");
        return;
    }
    subghz_receiver_set_filter(app->txrx->receiver, SubGhzProtocolFlag_Decodable);
}

void protopirate_hopper_update(ProtoPirateApp* app) {
    furi_check(app);

    switch(app->txrx->hopper_state) {
    case ProtoPirateHopperStateOFF:
    case ProtoPirateHopperStatePause:
        return;
    case ProtoPirateHopperStateRSSITimeOut:
        if(app->txrx->hopper_timeout != 0) {
            app->txrx->hopper_timeout--;
            return;
        }
        break;
    default:
        break;
    }
    float rssi = -127.0f;
    if(app->txrx->hopper_state != ProtoPirateHopperStateRSSITimeOut) {
        rssi = subghz_devices_get_rssi(app->txrx->radio_device);

        if(rssi > -90.0f) {
            app->txrx->hopper_timeout = 10;
            app->txrx->hopper_state = ProtoPirateHopperStateRSSITimeOut;
            return;
        }
    } else {
        app->txrx->hopper_state = ProtoPirateHopperStateRunning;
    }

    if(app->txrx->hopper_idx_frequency <
       subghz_setting_get_hopper_frequency_count(app->setting) - 1) {
        app->txrx->hopper_idx_frequency++;
    } else {
        app->txrx->hopper_idx_frequency = 0;
    }

    if(app->txrx->txrx_state == ProtoPirateTxRxStateRx) {
        protopirate_rx_end(app);
    }
    if(app->txrx->txrx_state == ProtoPirateTxRxStateIDLE && app->txrx->receiver) {
        subghz_receiver_reset(app->txrx->receiver);
        app->txrx->preset->frequency =
            subghz_setting_get_hopper_frequency(app->setting, app->txrx->hopper_idx_frequency);
        protopirate_rx(app, app->txrx->preset->frequency);
    }
}

void protopirate_tx(ProtoPirateApp* app, uint32_t frequency) {
    furi_check(app);
    if(!subghz_devices_is_frequency_valid(app->txrx->radio_device, frequency)) {
        return;
    }

    furi_check(app->txrx->txrx_state == ProtoPirateTxRxStateIDLE);

    subghz_devices_idle(app->txrx->radio_device);
    subghz_devices_set_frequency(app->txrx->radio_device, frequency);
    subghz_devices_set_tx(app->txrx->radio_device);

    app->txrx->txrx_state = ProtoPirateTxRxStateTx;
}

void protopirate_tx_stop(ProtoPirateApp* app) {
    furi_check(app);
    furi_check(app->txrx->txrx_state == ProtoPirateTxRxStateTx);

    subghz_devices_idle(app->txrx->radio_device);
    app->txrx->txrx_state = ProtoPirateTxRxStateIDLE;
}
