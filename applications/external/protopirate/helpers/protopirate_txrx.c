#include "protopirate_txrx.h"
#include "../protopirate_app_i.h"
#include "protopirate_protocol_plugin_host.h"
#include "protopirate_radio.h"
#include "protopirate_views.h"

#include <stdio.h>

#define TAG "ProtoPirateTxRx"

void protopirate_rx_stack_teardown_for_registry_switch(ProtoPirateApp* app) {
    furi_check(app);
    furi_check(app->txrx);

    if(app->txrx->txrx_state == ProtoPirateTxRxStateRx) {
        protopirate_rx_end(app);
    }

    if(app->txrx->receiver) {
        subghz_receiver_set_rx_callback(app->txrx->receiver, NULL, NULL);
        subghz_receiver_free(app->txrx->receiver);
        app->txrx->receiver = NULL;
    }

    if(app->txrx->worker) {
        if(subghz_worker_is_running(app->txrx->worker)) {
            subghz_worker_stop(app->txrx->worker);
        }
        subghz_worker_free(app->txrx->worker);
        app->txrx->worker = NULL;
    }

    if(app->txrx->radio_device && app->txrx->txrx_state != ProtoPirateTxRxStateTx) {
        subghz_devices_idle(app->txrx->radio_device);
        app->txrx->txrx_state = ProtoPirateTxRxStateIDLE;
    }
}

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

void protopirate_get_frequency_modulation_str(
    ProtoPirateApp* app,
    char* frequency,
    size_t frequency_size,
    char* modulation,
    size_t modulation_size) {
    furi_check(app);

    if(frequency && frequency_size > 0) {
        unsigned long mhz = (unsigned long)((app->txrx->preset->frequency / 1000000UL) % 1000UL);
        unsigned long khz = (unsigned long)((app->txrx->preset->frequency / 10000UL) % 100UL);
        snprintf(frequency, frequency_size, "%03lu.%02lu", mhz, khz);
    }

    if(modulation && modulation_size > 0) {
        snprintf(
            modulation, modulation_size, "%.2s", furi_string_get_cstr(app->txrx->preset->name));
    }
}

void protopirate_get_frequency_modulation(
    ProtoPirateApp* app,
    FuriString* frequency,
    FuriString* modulation) {
    furi_check(app);

    char frequency_buf[16] = {0};
    char modulation_buf[8] = {0};
    protopirate_get_frequency_modulation_str(
        app, frequency_buf, sizeof(frequency_buf), modulation_buf, sizeof(modulation_buf));

    if(frequency != NULL) {
        furi_string_set_str(frequency, frequency_buf);
    }
    if(modulation != NULL) {
        furi_string_set_str(modulation, modulation_buf);
    }
}

void protopirate_begin(ProtoPirateApp* app, uint8_t* preset_data) {
    furi_check(app);
    if(!app->txrx->radio_device) {
        FURI_LOG_W(TAG, "begin requested without radio device");
        app->txrx->txrx_state = ProtoPirateTxRxStateIDLE;
        return;
    }
    subghz_devices_reset(app->txrx->radio_device);
    subghz_devices_idle(app->txrx->radio_device);
    subghz_devices_load_preset(app->txrx->radio_device, FuriHalSubGhzPresetCustom, preset_data);
    app->txrx->txrx_state = ProtoPirateTxRxStateIDLE;
}

uint32_t protopirate_rx(ProtoPirateApp* app, uint32_t frequency) {
    furi_check(app);
    furi_check(app->txrx);
    if(!app->radio_initialized || !app->txrx->radio_device || !app->txrx->worker) {
        FURI_LOG_E(
            TAG,
            "RX start rejected (radio_initialized=%d, radio=%p, worker=%p)",
            app->radio_initialized,
            app->txrx->radio_device,
            app->txrx->worker);
        app->txrx->txrx_state = ProtoPirateTxRxStateIDLE;
        return 0;
    }

    if(!subghz_devices_is_frequency_valid(app->txrx->radio_device, frequency)) {
        FURI_LOG_E(TAG, "RX start rejected: invalid frequency %lu", frequency);
        if(app->notifications) {
            notification_message(app->notifications, &sequence_error);
        }
        app->txrx->txrx_state = ProtoPirateTxRxStateIDLE;
        return 0;
    }
    if(app->txrx->txrx_state == ProtoPirateTxRxStateRx ||
       app->txrx->txrx_state == ProtoPirateTxRxStateSleep) {
        FURI_LOG_W(TAG, "RX start ignored in state %d", app->txrx->txrx_state);
        return app->txrx->preset ? app->txrx->preset->frequency : 0;
    }

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
    if(app->txrx->radio_device) {
        subghz_devices_idle(app->txrx->radio_device);
    } else {
        FURI_LOG_W(TAG, "idle requested without radio device");
    }
    app->txrx->txrx_state = ProtoPirateTxRxStateIDLE;
}

void protopirate_rx_end(ProtoPirateApp* app) {
    furi_check(app);
    if(!app->txrx || app->txrx->txrx_state != ProtoPirateTxRxStateRx) {
        return;
    }

    if(app->txrx->worker && subghz_worker_is_running(app->txrx->worker)) {
        subghz_worker_stop(app->txrx->worker);
    }

    if(app->txrx->radio_device) {
        subghz_devices_stop_async_rx(app->txrx->radio_device);
        subghz_devices_idle(app->txrx->radio_device);
    }

    app->txrx->txrx_state = ProtoPirateTxRxStateIDLE;
}

void protopirate_sleep(ProtoPirateApp* app) {
    furi_check(app);
    subghz_devices_sleep(app->txrx->radio_device);
    app->txrx->txrx_state = ProtoPirateTxRxStateSleep;
}

void protopirate_release_shared_radio_state(ProtoPirateApp* app) {
    furi_check(app);
    furi_check(app->txrx);

    if(app->protopirate_receiver) {
        protopirate_view_receiver_reset_menu(app->protopirate_receiver);
    }

    protopirate_radio_deinit(app);
}

void protopirate_rx_stack_suspend_for_tx(ProtoPirateApp* app) {
    if(!app || !app->radio_initialized) {
        return;
    }

    if(app->txrx->txrx_state == ProtoPirateTxRxStateRx) {
        protopirate_rx_end(app);
    }

    if(app->txrx->worker && subghz_worker_is_running(app->txrx->worker)) {
        subghz_worker_stop(app->txrx->worker);
    }

    if(app->txrx->receiver) {
        subghz_receiver_set_rx_callback(app->txrx->receiver, NULL, NULL);
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

    if(!protopirate_refresh_protocol_registry(app, true)) {
        FURI_LOG_E(TAG, "rx_stack_resume: failed to restore RX stack");
    }
}

bool protopirate_hopper_update(ProtoPirateApp* app) {
    furi_check(app);

    switch(app->txrx->hopper_state) {
    case ProtoPirateHopperStateOFF:
    case ProtoPirateHopperStatePause:
        return false;
    case ProtoPirateHopperStateRSSITimeOut:
        if(app->txrx->hopper_timeout != 0) {
            app->txrx->hopper_timeout--;
            return false;
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
            return false;
        }
    } else {
        app->txrx->hopper_state = ProtoPirateHopperStateRunning;
    }

    const size_t hopper_count = subghz_setting_get_hopper_frequency_count(app->setting);
    if(hopper_count == 0) {
        app->txrx->hopper_state = ProtoPirateHopperStateOFF;
        app->txrx->hopper_idx_frequency = 0;
        return false;
    }
    if(app->txrx->hopper_idx_frequency < hopper_count - 1) {
        app->txrx->hopper_idx_frequency++;
    } else {
        app->txrx->hopper_idx_frequency = 0;
    }

    if(app->txrx->txrx_state == ProtoPirateTxRxStateRx) {
        protopirate_rx_end(app);
    }
    if(app->txrx->txrx_state == ProtoPirateTxRxStateIDLE) {
        app->txrx->preset->frequency =
            subghz_setting_get_hopper_frequency(app->setting, app->txrx->hopper_idx_frequency);
        if(!protopirate_refresh_protocol_registry(app, true) || !app->txrx->receiver) {
            FURI_LOG_E(TAG, "Failed to refresh registry while hopping");
            return false;
        }
        return true;
    }

    return false;
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
