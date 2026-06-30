#include "flipper_wedge_rfid.h"
#include <lfrfid/protocols/lfrfid_protocols.h>

#define TAG "FlipperWedgeRfid"

struct FlipperWedgeRfid {
    LFRFIDWorker* worker;
    ProtocolDict* dict;

    bool scanning;

    FlipperWedgeRfidCallback callback;
    void* callback_context;

    FlipperWedgeRfidData last_data;
};

static void flipper_wedge_rfid_worker_callback(LFRFIDWorkerReadResult result, ProtocolId protocol, void* context) {
    furi_assert(context);
    FlipperWedgeRfid* instance = context;

    if(result == LFRFIDWorkerReadDone) {
        // Get protocol data
        size_t data_size = protocol_dict_get_data_size(instance->dict, protocol);
        if(data_size > FLIPPER_WEDGE_RFID_UID_MAX_LEN) {
            data_size = FLIPPER_WEDGE_RFID_UID_MAX_LEN;
        }

        uint8_t* data = malloc(protocol_dict_get_data_size(instance->dict, protocol));
        protocol_dict_get_data(instance->dict, protocol, data, protocol_dict_get_data_size(instance->dict, protocol));

        // Copy to our data structure
        instance->last_data.uid_len = data_size;
        memcpy(instance->last_data.uid, data, data_size);

        // Get protocol name
        const char* name = protocol_dict_get_name(instance->dict, protocol);
        if(name) {
            snprintf(instance->last_data.protocol_name, sizeof(instance->last_data.protocol_name), "%s", name);
        } else {
            instance->last_data.protocol_name[0] = '\0';
        }

        free(data);

        FURI_LOG_I(TAG, "RFID tag read: %s, len: %d", instance->last_data.protocol_name, instance->last_data.uid_len);

        // Notify callback
        if(instance->callback) {
            instance->callback(&instance->last_data, instance->callback_context);
        }
    }
}

FlipperWedgeRfid* flipper_wedge_rfid_alloc(void) {
    FlipperWedgeRfid* instance = malloc(sizeof(FlipperWedgeRfid));

    instance->dict = protocol_dict_alloc(lfrfid_protocols, LFRFIDProtocolMax);
    instance->worker = lfrfid_worker_alloc(instance->dict);

    instance->scanning = false;
    instance->callback = NULL;
    instance->callback_context = NULL;

    memset(&instance->last_data, 0, sizeof(FlipperWedgeRfidData));

    FURI_LOG_I(TAG, "RFID reader allocated");

    return instance;
}

void flipper_wedge_rfid_free(FlipperWedgeRfid* instance) {
    furi_assert(instance);

    flipper_wedge_rfid_stop(instance);

    if(instance->worker) {
        lfrfid_worker_free(instance->worker);
        instance->worker = NULL;
    }

    if(instance->dict) {
        protocol_dict_free(instance->dict);
        instance->dict = NULL;
    }

    free(instance);
    FURI_LOG_I(TAG, "RFID reader freed");
}

void flipper_wedge_rfid_set_callback(
    FlipperWedgeRfid* instance,
    FlipperWedgeRfidCallback callback,
    void* context) {
    furi_assert(instance);
    instance->callback = callback;
    instance->callback_context = context;
}

void flipper_wedge_rfid_start(FlipperWedgeRfid* instance) {
    furi_assert(instance);

    if(instance->scanning) {
        FURI_LOG_W(TAG, "Already scanning");
        return;
    }

    lfrfid_worker_start_thread(instance->worker);
    lfrfid_worker_read_start(instance->worker, LFRFIDWorkerReadTypeAuto, flipper_wedge_rfid_worker_callback, instance);

    instance->scanning = true;
    FURI_LOG_I(TAG, "RFID scanning started");
}

void flipper_wedge_rfid_stop(FlipperWedgeRfid* instance) {
    furi_assert(instance);

    if(!instance->scanning) {
        return;
    }

    lfrfid_worker_stop(instance->worker);
    lfrfid_worker_stop_thread(instance->worker);

    instance->scanning = false;
    FURI_LOG_I(TAG, "RFID scanning stopped");
}

bool flipper_wedge_rfid_is_scanning(FlipperWedgeRfid* instance) {
    furi_assert(instance);
    return instance->scanning;
}
