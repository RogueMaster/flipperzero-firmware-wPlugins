#include "about_ndef.h"

#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <notification/notification_messages.h>
#include <string.h>

bool mtools_about_ndef_start(MToolsApp* app) {
    static const uint8_t uid[7] = {0x04, 0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6};
    static const uint8_t version[8] = {0x00, 0x04, 0x04, 0x02, 0x01, 0x00, 0x0F, 0x03};
    static const char domain[] = "shop.mtoolstec.com";
    enum { NdefRecordLen = sizeof(domain) - 1 + 5 };

    app->about_ndef_data = mf_ultralight_alloc();
    if(!app->about_ndef_data) return false;
    MfUltralightData* data = app->about_ndef_data;
    memset(data->page, 0, sizeof(data->page));
    memset(data->counter, 0, sizeof(data->counter));
    memset(data->tearing_flag, MF_ULTRALIGHT_TEARING_FLAG_DEFAULT, sizeof(data->tearing_flag));
    memset(&data->signature, 0, sizeof(data->signature));
    data->type = MfUltralightTypeNTAG213;
    memcpy(&data->version, version, sizeof(version));
    data->pages_total = mf_ultralight_get_pages_total(data->type);
    data->pages_read = data->pages_total;
    data->auth_attempts = 0;
    if(!mf_ultralight_set_uid(data, uid, sizeof(uid))) {
        mtools_about_ndef_stop(app);
        return false;
    }
    const uint8_t atqa[2] = {0x44, 0x00};
    iso14443_3a_set_atqa(data->iso14443_3a_data, atqa);
    iso14443_3a_set_sak(data->iso14443_3a_data, 0x00);
    data->page[2].data[1] = 0x48;
    const uint8_t cc[4] = {0xE1, 0x10, 0x12, 0x00};
    memcpy(data->page[3].data, cc, sizeof(cc));
    data->page[mf_ultralight_get_config_page_num(data->type)].data[3] = 0xFF;

    uint8_t tlv[2 + NdefRecordLen + 1] = {
        0x03, NdefRecordLen, 0xD1, 0x01, sizeof(domain), 0x55, 0x04};
    // URI prefix 0x04 is "https://"; the remaining bytes hold the host.
    memcpy(tlv + 7, domain, sizeof(domain) - 1);
    tlv[2 + NdefRecordLen] = 0xFE;
    for(size_t i = 0; i < sizeof(tlv); ++i)
        data->page[4 + i / MF_ULTRALIGHT_PAGE_SIZE].data[i % MF_ULTRALIGHT_PAGE_SIZE] = tlv[i];

    app->about_listener = nfc_listener_alloc(app->nfc, NfcProtocolMfUltralight, data);
    if(!app->about_listener) {
        mtools_about_ndef_stop(app);
        return false;
    }
    nfc_listener_start(app->about_listener, NULL, NULL);
    notification_message(app->notifications, &sequence_blink_start_magenta);
    return true;
}

void mtools_about_ndef_stop(MToolsApp* app) {
    if(app->about_listener) {
        nfc_listener_stop(app->about_listener);
        nfc_listener_free(app->about_listener);
        app->about_listener = NULL;
    }
    if(app->about_ndef_data) {
        mf_ultralight_free(app->about_ndef_data);
        app->about_ndef_data = NULL;
    }
    notification_message(app->notifications, &sequence_blink_stop);
}
