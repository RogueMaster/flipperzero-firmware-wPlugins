#include "protopirate_radio.h"
#include "../protopirate_app_i.h"

#include <furi.h>
#include <string.h>

#define TAG "ProtoPirateRadio"

static void protopirate_radio_free_receiver(ProtoPirateApp* app) {
    furi_check(app);
    furi_check(app->txrx);

    if(!app->txrx->receiver) {
#ifndef REMOVE_LOGS
        FURI_LOG_D(TAG, "Receiver was NULL, skipping free");
#endif
        return;
    }

#ifndef REMOVE_LOGS
    FURI_LOG_D(TAG, "Freeing receiver %p", app->txrx->receiver);
#endif
    subghz_receiver_free(app->txrx->receiver);
    app->txrx->receiver = NULL;
}

static void protopirate_radio_free_environment(ProtoPirateApp* app) {
    furi_check(app);
    furi_check(app->txrx);

    if(!app->txrx->environment) {
#ifndef REMOVE_LOGS
        FURI_LOG_D(TAG, "Environment was NULL, skipping free");
#endif
        return;
    }

#ifndef REMOVE_LOGS
    FURI_LOG_D(TAG, "Freeing environment %p", app->txrx->environment);
#endif
    subghz_environment_free(app->txrx->environment);
    app->txrx->environment = NULL;
    app->txrx->protocol_registry = NULL;
}

static void protopirate_radio_end_device(ProtoPirateApp* app, bool sleep_before_end) {
    furi_check(app);
    furi_check(app->txrx);

    if(!app->txrx->radio_device) {
#ifndef REMOVE_LOGS
        FURI_LOG_D(TAG, "Radio device was NULL, skipping sleep/end");
#endif
        return;
    }

#ifndef REMOVE_LOGS
    FURI_LOG_D(
        TAG,
        "Putting radio device to %s and ending: %p",
        sleep_before_end ? "sleep" : "idle",
        app->txrx->radio_device);
#endif

    if(sleep_before_end) {
        subghz_devices_sleep(app->txrx->radio_device);
    } else {
        subghz_devices_idle(app->txrx->radio_device);
    }

    radio_device_loader_end(app->txrx->radio_device);
    app->txrx->radio_device = NULL;
}

static void protopirate_radio_reset_state(ProtoPirateApp* app) {
    furi_check(app);
    furi_check(app->txrx);

    app->txrx->protocol_registry = NULL;
    app->txrx->protocol_plugin = NULL;
    app->txrx->protocol_registry_route = ProtoPirateProtocolRegistryRouteAMDefault;
    app->txrx->txrx_state = ProtoPirateTxRxStateIDLE;
    app->radio_initialized = false;
}

static void protopirate_radio_init_cleanup(ProtoPirateApp* app, bool devices_initialized) {
    furi_check(app);
    furi_check(app->txrx);

    protopirate_radio_free_receiver(app);
    protopirate_radio_end_device(app, false);
    protopirate_radio_free_environment(app);
    protopirate_unload_protocol_plugin(app->txrx);

    if(devices_initialized) {
        subghz_devices_deinit();
    }

    protopirate_radio_reset_state(app);
}

bool protopirate_radio_init(ProtoPirateApp* app) {
    furi_check(app);
    furi_check(app->txrx);

    FURI_LOG_I(TAG, "=== protopirate_radio_init called ===");
#ifndef REMOVE_LOGS
    FURI_LOG_D(TAG, "State: radio_initialized=%d", app->radio_initialized);
#endif

    if(app->radio_initialized) {
        const bool radio_ready = (app->txrx->environment != NULL) &&
                                 (app->txrx->radio_device != NULL);
        if(radio_ready) {
#ifndef REMOVE_LOGS
            FURI_LOG_D(TAG, "Radio already initialized, returning true");
#endif
            return true;
        }

        FURI_LOG_W(
            TAG,
            "Radio marked initialized but resources missing (env=%p device=%p), repairing",
            app->txrx->environment,
            app->txrx->radio_device);
        protopirate_radio_deinit(app);
    }

    FURI_LOG_I(TAG, "Fresh radio init - allocating all components");

    app->txrx->environment = subghz_environment_alloc();
    if(!app->txrx->environment) {
        FURI_LOG_E(TAG, "Failed to allocate environment!");
        protopirate_radio_init_cleanup(app, false);
        return false;
    }

    app->txrx->protocol_registry = NULL;

    if(!protopirate_refresh_protocol_registry(app, false)) {
        FURI_LOG_E(TAG, "Failed to configure protocol registry");
        protopirate_radio_init_cleanup(app, false);
        return false;
    }

    subghz_environment_load_keystore(app->txrx->environment, PROTOPIRATE_KEYSTORE_DIR_NAME);

    subghz_devices_init();
#ifndef REMOVE_LOGS
    FURI_LOG_D(TAG, "SubGhz devices initialized");
#endif

    app->txrx->radio_device = radio_device_loader_set(NULL, SubGhzRadioDeviceTypeExternalCC1101);

    if(!app->txrx->radio_device) {
        FURI_LOG_W(TAG, "External CC1101 not found, trying internal radio");
        app->txrx->radio_device = radio_device_loader_set(NULL, SubGhzRadioDeviceTypeInternal);
    }

    if(!app->txrx->radio_device) {
        FURI_LOG_E(TAG, "Failed to initialize any radio device!");
        protopirate_radio_init_cleanup(app, true);
        return false;
    }
#ifndef REMOVE_LOGS
    const char* device_name = subghz_devices_get_name(app->txrx->radio_device);
    bool is_external = device_name && strstr(device_name, "ext");
    FURI_LOG_I(
        TAG,
        "Radio device initialized: %s (%s)",
        device_name ? device_name : "unknown",
        is_external ? "external" : "internal");
#endif
    subghz_devices_reset(app->txrx->radio_device);
    subghz_devices_idle(app->txrx->radio_device);

    app->radio_initialized = true;

#ifndef REMOVE_LOGS
    FURI_LOG_D(TAG, "Final state: radio_initialized=%d", app->radio_initialized);
#endif

    return true;
}

void protopirate_radio_deinit(ProtoPirateApp* app) {
    furi_check(app);
    furi_check(app->txrx);

    FURI_LOG_I(TAG, "=== protopirate_radio_deinit called ===");
#ifndef REMOVE_LOGS
    FURI_LOG_D(TAG, "State: radio_initialized=%d", app->radio_initialized);
    FURI_LOG_D(
        TAG,
        "Pointers: worker=%p, environment=%p, receiver=%p, history=%p, radio_device=%p",
        app->txrx->worker,
        app->txrx->environment,
        app->txrx->receiver,
        app->txrx->history,
        app->txrx->radio_device);
#endif

    bool has_radio_resources = app->radio_initialized || app->txrx->worker ||
                               app->txrx->environment || app->txrx->receiver ||
                               app->txrx->history || app->txrx->radio_device ||
                               app->txrx->protocol_plugin_manager || app->txrx->plugin_resolver ||
                               app->txrx->protocol_plugin;
    if(!has_radio_resources) {
#ifndef REMOVE_LOGS
        FURI_LOG_D(TAG, "Radio resources were not initialized, returning");
#endif
        return;
    }

    bool devices_initialized = app->radio_initialized || (app->txrx->radio_device != NULL);

    if(app->txrx->worker && app->txrx->txrx_state == ProtoPirateTxRxStateRx) {
#ifndef REMOVE_LOGS
        FURI_LOG_D(TAG, "Stopping active RX, state=%d", app->txrx->txrx_state);
#endif
        subghz_worker_stop(app->txrx->worker);
        if(app->txrx->radio_device) {
            subghz_devices_stop_async_rx(app->txrx->radio_device);
        }
    }

    protopirate_radio_end_device(app, true);

    if(devices_initialized) {
#ifndef REMOVE_LOGS
        FURI_LOG_D(TAG, "Calling subghz_devices_deinit");
#endif
        subghz_devices_deinit();
    }

    protopirate_radio_free_receiver(app);
    protopirate_radio_free_environment(app);
    protopirate_unload_protocol_plugin(app->txrx);

    if(app->txrx->history) {
#ifndef REMOVE_LOGS
        FURI_LOG_D(TAG, "Freeing history %p", app->txrx->history);
#endif
        protopirate_history_free(app->txrx->history);
        app->txrx->history = NULL;
    } else {
#ifndef REMOVE_LOGS
        FURI_LOG_D(TAG, "History was NULL, skipping free");
#endif
    }

    if(app->txrx->worker) {
#ifndef REMOVE_LOGS
        FURI_LOG_D(TAG, "Freeing worker %p", app->txrx->worker);
#endif
        subghz_worker_free(app->txrx->worker);
        app->txrx->worker = NULL;
    } else {
#ifndef REMOVE_LOGS
        FURI_LOG_D(TAG, "Worker was NULL, skipping free");
#endif
    }

    protopirate_radio_reset_state(app);

#ifndef REMOVE_LOGS
    FURI_LOG_D(TAG, "Final state: radio_initialized=%d", app->radio_initialized);
#endif
}
