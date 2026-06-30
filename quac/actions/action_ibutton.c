// Methods for iButton transmission

#include <furi.h>
#include <furi_hal.h>

#include <ibutton/ibutton_key.h>
#include <ibutton/ibutton_worker.h>
#include <ibutton/ibutton_protocols.h>

#include "action_i.h"
#include "quac.h"

void action_ibutton_tx(void* context, const FuriString* action_path, FuriString* error) {
    App* app = context;
    const char* cpath = furi_string_get_cstr(action_path);

    FURI_LOG_I(TAG, "iButton: Tx %s", cpath);

    iButtonProtocols* protocols = ibutton_protocols_alloc();
    iButtonKey* key = ibutton_key_alloc(ibutton_protocols_get_max_data_size(protocols));

    const bool success = ibutton_protocols_load(protocols, key, cpath);
    if(!success) {
        FURI_LOG_E(TAG, "Error loading iButton file %s", cpath);
        ACTION_SET_ERROR("Error loading %s", cpath);
    } else {
        FURI_LOG_I(TAG, "iButton: Starting...");
        iButtonWorker* worker = ibutton_worker_alloc(protocols);
        ibutton_worker_start_thread(worker);

        ibutton_worker_emulate_start(worker, key);

        int16_t time_ms = app->settings.ibutton_duration;
        const int16_t interval_ms = 100;
        while(time_ms > 0) {
            furi_delay_ms(interval_ms);
            time_ms -= interval_ms;
        }

        FURI_LOG_I(TAG, "iButton: Done");
        ibutton_worker_stop(worker);
        ibutton_worker_stop_thread(worker);
        ibutton_worker_free(worker);
    }

    ibutton_key_free(key);
    ibutton_protocols_free(protocols);
}
