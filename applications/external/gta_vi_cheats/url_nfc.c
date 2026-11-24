#include "url_nfc.h"
#include <furi.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_listener.h>
#include <nfc/helpers/nfc_data_generator.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <notification/notification_messages.h>
#include <stdlib.h>
#include <string.h>

#define TAG                       "GTA NFC"
#define GTA_NTAG215_NDEF_CAPACITY 496u

static const char* last_error;

const char* gta_url_nfc_error(void) {
    return last_error;
}

struct GtaUrlNfc {
    Nfc* nfc;
    NfcListener* listener;
    NotificationApp* notifications;
};

GtaUrlNfc* gta_url_nfc_start(const char* url, uint16_t id, GtaUrlKind kind) {
    last_error = "Invalid URL or type";
    FURI_LOG_I(
        TAG,
        "Start id=%u kind=%d len=%u",
        (unsigned)id,
        (int)kind,
        (unsigned)(url ? strlen(url) : 0));
    if(!url || (kind != GtaUrlSource && kind != GtaUrlMapImage && kind != GtaUrlSource2)) {
        FURI_LOG_E(TAG, "%s", last_error);
        return NULL;
    }

    last_error = "Session allocation failed";
    GtaUrlNfc* session = calloc(1, sizeof(*session));
    if(!session) {
        FURI_LOG_E(TAG, "%s", last_error);
        return NULL;
    }
    NfcDevice* device = nfc_device_alloc();
    MfUltralightData* tag = NULL;
    last_error = "Tag allocation failed";
    if(!device) goto fail;

    // Use the same firmware NTAG215 generator as NFC URL (y-dejong/nfcurl).
    // Retain its UID, anticollision parameters, capability container and config.
    nfc_data_generator_fill_data(NfcDataGeneratorTypeNTAG215, device);
    last_error = "Tag data unavailable";
    const MfUltralightData* generated = nfc_device_get_data(device, NfcProtocolMfUltralight);
    if(!generated) goto fail;
    // Ask the firmware for the tag name, rather than reading ABI-sensitive
    // fields from MfUltralightData. NFC URL trusts the native tag generator.
    last_error = "Wrong NTAG215 type";
    if(strcmp(mf_ultralight_get_device_name(generated, NfcDeviceNameTypeFull), "NTAG215") != 0)
        goto fail;

    last_error = "Tag copy allocation failed";
    tag = mf_ultralight_alloc();
    if(!tag) goto fail;
    mf_ultralight_copy(tag, generated);

    // The firmware locates page 131 using its own structure layout. NTAG215
    // pages are contiguous, four bytes each; the NDEF area starts at page 4.
    // This avoids using application-compiled offsets for page[] or page counts.
    MfUltralightConfigPages* config = NULL;
    last_error = "NTAG215 config unavailable";
    if(!mf_ultralight_get_config_page(tag, &config) || !config) goto fail;
    uint8_t* memory = (uint8_t*)((uintptr_t)config - (131u - 4u) * 4u);
    const uint8_t* cc = memory - 4;
    last_error = "NTAG215 data layout invalid";
    if(cc[0] != 0xE1 || cc[1] != 0x10 || cc[2] != 0x3E || cc[3] != 0x00) goto fail;

    last_error = "NDEF encoding failed";
    const size_t encoded = gta_url_ndef_encode(url, memory, GTA_NTAG215_NDEF_CAPACITY);
    if(!encoded) goto fail;

    // Same tag preparation lifecycle as NFC URL: commit the edited tag to
    // the NFC device, then use the device's data for listener allocation.
    nfc_device_set_data(device, NfcProtocolMfUltralight, tag);
    mf_ultralight_free(tag);
    tag = NULL;
    last_error = "Tag data unavailable";
    generated = nfc_device_get_data(device, NfcProtocolMfUltralight);
    if(!generated) goto fail;

    last_error = "NFC allocation failed";
    session->nfc = nfc_alloc();
    if(!session->nfc) goto fail;
    last_error = "Listener allocation failed";
    session->listener = nfc_listener_alloc(session->nfc, NfcProtocolMfUltralight, generated);
    if(!session->listener) goto fail;
    // The native listener owns a copy of the NFC device data.
    nfc_device_free(device);
    device = NULL;

    // As in NFC URL, allow the native protocol listener to handle all frames.
    nfc_listener_start(session->listener, NULL, NULL);
    session->notifications = furi_record_open(RECORD_NOTIFICATION);
    notification_message(session->notifications, &sequence_blink_start_magenta);
    FURI_LOG_I(TAG, "NTAG215 listener started (%u NDEF bytes)", (unsigned)encoded);
    last_error = NULL;
    return session;

fail:
    FURI_LOG_E(TAG, "%s", last_error);
    if(tag) mf_ultralight_free(tag);
    if(device) nfc_device_free(device);
    gta_url_nfc_stop(session);
    return NULL;
}

void gta_url_nfc_stop(GtaUrlNfc* session) {
    if(!session) return;
    if(session->listener) {
        nfc_listener_stop(session->listener);
        nfc_listener_free(session->listener);
    }
    if(session->notifications) {
        notification_message(session->notifications, &sequence_blink_stop);
        furi_record_close(RECORD_NOTIFICATION);
    }
    if(session->nfc) nfc_free(session->nfc);
    free(session);
}
