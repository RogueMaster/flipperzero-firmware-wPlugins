#include "bambu_reader.h"
#include "bambu_crypto.h"
#include "bambu_export.h"
#include "bambu_tag.h"

#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/mf_classic/mf_classic.h>
#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>

#include <stdio.h>
#include <string.h>

namespace {
void set_message(BrScanResult* result, const char* text) {
    snprintf(result->message, sizeof(result->message), "%s", text ? text : "");
}


bool all_blocks_read(const MfClassicData* data) {
    if(!data || data->type != MfClassicType1k) return false;
    for(uint8_t block = 0; block < BR_BLOCK_COUNT; ++block) {
        if(!mf_classic_is_block_read(data, block)) return false;
    }
    return true;
}

bool wait_for_uid(Nfc* nfc, volatile bool* cancel, uint8_t uid[BR_UID_SIZE]) {
    while(!*cancel) {
        NfcPoller* poller = nfc_poller_alloc(nfc, NfcProtocolIso14443_3a);
        if(!poller) return false;
        bool found = nfc_poller_detect(poller);
        if(found) {
            const Iso14443_3aData* data = static_cast<const Iso14443_3aData*>(nfc_poller_get_data(poller));
            if(data && data->uid_len == BR_UID_SIZE) {
                memcpy(uid, data->uid, BR_UID_SIZE);
                nfc_poller_free(poller);
                return true;
            }
        }
        nfc_poller_free(poller);
        furi_delay_ms(120);
    }
    return false;
}
} // namespace

int32_t br_scan_worker(void* context) {
    BrScanContext* scan = static_cast<BrScanContext*>(context);
    BrScanResult* result = scan->result;
    memset(result, 0, sizeof(*result));
    set_message(result, "Hold a Bambu tag near Flipper");
    view_dispatcher_send_custom_event(scan->dispatcher, scan->progress_event);

    Nfc* nfc = nfc_alloc();
    if(!nfc) {
        set_message(result, "Could not acquire NFC hardware");
        view_dispatcher_send_custom_event(scan->dispatcher, scan->done_event);
        return 0;
    }

    uint8_t uid[BR_UID_SIZE] = {};
    if(!wait_for_uid(nfc, scan->cancel, uid)) {
        if(!*scan->cancel) set_message(result, "No compatible 4-byte UID found");
        nfc_free(nfc);
        view_dispatcher_send_custom_event(scan->dispatcher, scan->done_event);
        return 0;
    }

    if(*scan->cancel) {
        nfc_free(nfc);
        return 0;
    }

    char uid_hex[9];
    br_format_uid(uid, uid_hex);
    snprintf(result->message, sizeof(result->message), "UID %s - deriving keys", uid_hex);
    view_dispatcher_send_custom_event(scan->dispatcher, scan->progress_event);

    MfClassicType type = MfClassicTypeMini;
    MfClassicError error = mf_classic_poller_sync_detect_type(nfc, &type);
    if(error != MfClassicErrorNone || type != MfClassicType1k) {
        set_message(result, "Tag is not MIFARE Classic 1K");
        nfc_free(nfc);
        view_dispatcher_send_custom_event(scan->dispatcher, scan->done_event);
        return 0;
    }

    MfClassicDeviceKeys keys = {};
    if(!br_derive_sector_keys(uid, &keys)) {
        set_message(result, "Key derivation failed");
        nfc_free(nfc);
        view_dispatcher_send_custom_event(scan->dispatcher, scan->done_event);
        return 0;
    }

    set_message(result, "Reading 16 sectors...");
    view_dispatcher_send_custom_event(scan->dispatcher, scan->progress_event);

    MfClassicData* data = mf_classic_alloc();
    if(!data) {
        set_message(result, "Out of memory");
        nfc_free(nfc);
        view_dispatcher_send_custom_event(scan->dispatcher, scan->done_event);
        return 0;
    }

    error = mf_classic_poller_sync_read(nfc, &keys, data);
    // Both A and B keys are UID-derived. Still accept PartialRead defensively, but only
    // treat the scan as successful when every one of the 64 blocks is present.
    if((error != MfClassicErrorNone && error != MfClassicErrorPartialRead) || !all_blocks_read(data)) {
        set_message(result, "Read incomplete - keep tag steady and retry");
        mf_classic_free(data);
        nfc_free(nfc);
        view_dispatcher_send_custom_event(scan->dispatcher, scan->done_event);
        return 0;
    }

    size_t read_uid_len = 0;
    const uint8_t* read_uid = mf_classic_get_uid(data, &read_uid_len);
    if(!read_uid || read_uid_len != BR_UID_SIZE || memcmp(read_uid, uid, BR_UID_SIZE) != 0) {
        set_message(result, "Tag changed during scan");
        mf_classic_free(data);
        nfc_free(nfc);
        view_dispatcher_send_custom_event(scan->dispatcher, scan->done_event);
        return 0;
    }

    if(!br_parse_mf(data, &result->info)) {
        set_message(result, "Read succeeded, but tag layout is unknown");
        mf_classic_free(data);
        nfc_free(nfc);
        view_dispatcher_send_custom_event(scan->dispatcher, scan->done_event);
        return 0;
    }

    if(!br_save_tag_bundle(
           scan->storage, data, &result->info, result->saved_path, sizeof(result->saved_path))) {
        set_message(result, "Read succeeded, but saving tag bundle failed");
        mf_classic_free(data);
        nfc_free(nfc);
        view_dispatcher_send_custom_event(scan->dispatcher, scan->done_event);
        return 0;
    }

    result->ok = true;
    snprintf(result->message, sizeof(result->message), "Saved %s tag bundle", result->info.uid_hex);

    mf_classic_free(data);
    nfc_free(nfc);
    view_dispatcher_send_custom_event(scan->dispatcher, scan->done_event);
    return 0;
}
