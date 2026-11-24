#include "url_nfc.h"
#include <nfc/nfc.h>
#include <nfc/nfc_listener.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <stdlib.h>
#include <string.h>

struct GtaUrlNfc {
    Nfc* nfc;
    NfcListener* listener;
    MfUltralightData* tag;
};

static NfcCommand gta_url_nfc_event(NfcGenericEvent event, void* context) {
    (void)event;
    (void)context;
    return NfcCommandContinue;
}

GtaUrlNfc* gta_url_nfc_start(const char* url, uint16_t id, GtaUrlKind kind) {
    if(kind != GtaUrlSource && kind != GtaUrlMapImage && kind != GtaUrlSource2) return NULL;
    GtaUrlNfc* session = calloc(1, sizeof(*session));
    if(!session) return NULL;
    session->tag = mf_ultralight_alloc();
    if(!session->tag) goto fail;
    MfUltralightData* tag = session->tag;
    mf_ultralight_reset(tag);
    memset(tag->page, 0, sizeof(tag->page));
    memset(&tag->signature, 0, sizeof(tag->signature));
    memset(tag->counter, 0, sizeof(tag->counter));
    memset(tag->tearing_flag, MF_ULTRALIGHT_TEARING_FLAG_DEFAULT, sizeof(tag->tearing_flag));
    tag->auth_attempts = 0;
    tag->type = MfUltralightTypeNTAG215;
    tag->pages_total = 135;
    tag->pages_read = 135;
    tag->version = (MfUltralightVersion){0x00, 0x04, 0x04, 0x02, 0x01, 0x00, 0x11, 0x03};
    // Synthetic UID is distinct per item and link type, to help phones recognize a new tag.
    const uint8_t uid[] = {0x04, 0x47, 0x54, 0x41, (uint8_t)(id >> 8), (uint8_t)id, (uint8_t)(0x81 + kind)};
    if(!mf_ultralight_set_uid(tag, uid, sizeof(uid))) goto fail;
    tag->iso14443_3a_data->atqa[0] = 0x44;
    tag->iso14443_3a_data->atqa[1] = 0x00;
    tag->iso14443_3a_data->sak = 0;
    tag->page[2].data[1] = 0x48;
    // Type 2 capability container: 496 bytes, public read, no NDEF writes advertised.
    const uint8_t cc[] = {0xE1, 0x10, 0x3E, 0x0F};
    memcpy(tag->page[3].data, cc, sizeof(cc));
    tag->page[131].data[0] = 0x04;
    tag->page[132].data[1] = 0x05;
    tag->page[130].data[3] = MF_ULTRALIGHT_TEARING_FLAG_DEFAULT;
    tag->page[131].data[3] = 0xFF; // AUTH0: no password required
    memset(tag->page[133].data, 0xFF, 4); // default PWD
    // The page array is contiguous; encode into its byte representation, not a stack buffer.
    uint8_t* memory = (uint8_t*)tag->page;
    if(!gta_url_ndef_encode(url, memory + 16, 496)) goto fail;
    session->nfc = nfc_alloc();
    if(!session->nfc) goto fail;
    session->listener = nfc_listener_alloc(session->nfc, NfcProtocolMfUltralight, tag);
    if(!session->listener) goto fail;
    nfc_listener_start(session->listener, gta_url_nfc_event, session);
    return session;
fail:
    gta_url_nfc_stop(session);
    return NULL;
}

void gta_url_nfc_stop(GtaUrlNfc* session) {
    if(!session) return;
    if(session->listener) {
        nfc_listener_stop(session->listener);
        nfc_listener_free(session->listener);
    }
    if(session->nfc) nfc_free(session->nfc);
    if(session->tag) mf_ultralight_free(session->tag);
    free(session);
}
