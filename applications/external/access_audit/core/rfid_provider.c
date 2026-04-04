#include <furi.h>
#include <lib/lfrfid/lfrfid_worker.h>
#include <lib/lfrfid/protocols/lfrfid_protocols.h>
#include <toolbox/protocols/protocol_dict.h>
#include <string.h>

#include "rfid_provider.h"

/* -------------------------------------------------------------------------
 * Card type mapping
 * -------------------------------------------------------------------------
 */

static CardType lfrfid_protocol_to_card_type(LFRFIDProtocol protocol) {
    switch(protocol) {
    case LFRFIDProtocolEM4100:
    case LFRFIDProtocolEM410032:
    case LFRFIDProtocolEM410016:
    case LFRFIDProtocolElectra:
        return CardTypeEm4100Like;
    case LFRFIDProtocolH10301:
        return CardTypeHidProxLike;
    case LFRFIDProtocolHidGeneric:
    case LFRFIDProtocolHidExGeneric:
        return CardTypeHidGeneric;
    case LFRFIDProtocolIndala26:
        return CardTypeIndala;
    default:
        return CardTypeRfid125;
    }
}

/* -------------------------------------------------------------------------
 * Provider struct
 * -------------------------------------------------------------------------
 */

struct RfidProvider {
    ProtocolDict* dict;
    LFRFIDWorker* worker;
    FuriMutex* mutex;
    bool done;
    AccessObservation pending;
};

/* -------------------------------------------------------------------------
 * Worker callback  (runs on LFRFID worker thread)
 * -------------------------------------------------------------------------
 */

static void rfid_worker_callback(
    LFRFIDWorkerReadResult result,
    ProtocolId protocol,
    void* context) {
    RfidProvider* p = context;

    if(result != LFRFIDWorkerReadDone) return;

    furi_mutex_acquire(p->mutex, FuriWaitForever);

    p->pending = (AccessObservation){0};
    p->pending.tech = TechTypeRfid125;
    p->pending.card_type = lfrfid_protocol_to_card_type((LFRFIDProtocol)protocol);
    p->pending.metadata_complete = true;

    /* Pull the raw data bytes and use them as the UID. */
    size_t data_size = protocol_dict_get_data_size(p->dict, (size_t)protocol);
    if(data_size > 0) {
        size_t copy_size =
            data_size <= sizeof(p->pending.uid) ? data_size : sizeof(p->pending.uid);
        protocol_dict_get_data(p->dict, (size_t)protocol, p->pending.uid, copy_size);
        p->pending.uid_present = true;
        p->pending.uid_len = copy_size;
    }

    p->done = true;

    furi_mutex_release(p->mutex);
}

/* -------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------
 */

RfidProvider* rfid_provider_alloc(void) {
    RfidProvider* p = malloc(sizeof(RfidProvider));
    if(!p) return NULL;

    p->dict = protocol_dict_alloc(lfrfid_protocols, LFRFIDProtocolMax);
    p->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    p->done = false;
    p->pending = (AccessObservation){0};

    if(!p->dict || !p->mutex) {
        if(p->dict) protocol_dict_free(p->dict);
        if(p->mutex) furi_mutex_free(p->mutex);
        free(p);
        return NULL;
    }

    p->worker = lfrfid_worker_alloc(p->dict);
    if(!p->worker) {
        protocol_dict_free(p->dict);
        furi_mutex_free(p->mutex);
        free(p);
        return NULL;
    }

    lfrfid_worker_start_thread(p->worker);
    return p;
}

void rfid_provider_free(RfidProvider* provider) {
    if(!provider) return;
    rfid_provider_stop(provider);
    lfrfid_worker_stop_thread(provider->worker);
    lfrfid_worker_free(provider->worker);
    protocol_dict_free(provider->dict);
    furi_mutex_free(provider->mutex);
    free(provider);
}

void rfid_provider_start(RfidProvider* provider) {
    if(!provider) return;
    furi_mutex_acquire(provider->mutex, FuriWaitForever);
    provider->done = false;
    furi_mutex_release(provider->mutex);
    lfrfid_worker_read_start(
        provider->worker, LFRFIDWorkerReadTypeAuto, rfid_worker_callback, provider);
}

void rfid_provider_stop(RfidProvider* provider) {
    if(!provider) return;
    lfrfid_worker_stop(provider->worker);
}

bool rfid_provider_poll(RfidProvider* provider, AccessObservation* out) {
    if(!provider || !out) return false;

    furi_mutex_acquire(provider->mutex, FuriWaitForever);
    bool ready = provider->done;
    AccessObservation result = provider->pending;
    if(ready) provider->done = false;
    furi_mutex_release(provider->mutex);

    if(ready) {
        *out = result;
        return true;
    }
    return false;
}
