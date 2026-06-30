#include "../weebo_i.h"
#include <nfc/protocols/mf_ultralight/mf_ultralight_poller.h>

#define TAG "SceneWrite"

static uint8_t SLB[] = {0x00, 0x00, 0x0F, 0xE0};
static uint8_t CC[] = {0xf1, 0x10, 0xff, 0xee};
static uint8_t DLB[] = {0x01, 0x00, 0x0f, 0xbd};
static uint8_t CFG0[] = {0x00, 0x00, 0x00, 0x04};
static uint8_t CFG1[] = {0x5f, 0x00, 0x00, 0x00};
static uint8_t PACKRFUI[] = {0x80, 0x80, 0x00, 0x00};

enum NTAG215Pages {
    staticLockBits = 2,
    capabilityContainer = 3,
    userMemoryFirst = 4,
    userMemoryLast = 129,
    dynamicLockBits = 130,
    cfg0 = 131,
    cfg1 = 132,
    pwd = 133,
    pack = 134,
    total = 135
};

NfcCommand weebo_scene_write_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(event.protocol == NfcProtocolMfUltralight);
    Weebo* weebo = context;
    NfcCommand ret = NfcCommandContinue;

    const MfUltralightPollerEvent* mf_ultralight_event = event.event_data;
    MfUltralightPoller* poller = event.instance;

    if(mf_ultralight_event->type == MfUltralightPollerEventTypeRequestMode) {
        // no-op
    } else if(mf_ultralight_event->type == MfUltralightPollerEventTypeAuthRequest) {
        mf_ultralight_event->data->auth_context.skip_auth = true;
    } else if(mf_ultralight_event->type == MfUltralightPollerEventTypeReadSuccess) {
        nfc_device_set_data(
            weebo->nfc_device, NfcProtocolMfUltralight, nfc_poller_get_data(weebo->poller));
        const MfUltralightData* data =
            nfc_device_get_data(weebo->nfc_device, NfcProtocolMfUltralight);

        if(!mf_ultralight_is_all_data_read(data)) {
            view_dispatcher_send_custom_event(weebo->view_dispatcher, WeeboCustomEventWrongCard);
            ret = NfcCommandStop;
            return ret;
        }
        if(data->type != MfUltralightTypeNTAG215) {
            view_dispatcher_send_custom_event(weebo->view_dispatcher, WeeboCustomEventWrongCard);
            ret = NfcCommandStop;
            return ret;
        }
        view_dispatcher_send_custom_event(weebo->view_dispatcher, WeeboCustomEventCardDetected);

        uint8_t PWD[4];
        weebo_calculate_pwd(data->iso14443_3a_data->uid, PWD);

        for(size_t p = 0; p < 2; p++) {
            for(size_t i = 0; i < MF_ULTRALIGHT_PAGE_SIZE; i++) {
                weebo->figure[NFC3D_UID_OFFSET + p * MF_ULTRALIGHT_PAGE_SIZE + i] =
                    data->page[p].data[i];
            }
        }

        uint8_t modified[NTAG215_SIZE];
        nfc3d_amiibo_pack(&weebo->keys, weebo->figure, modified);

        MfUltralightError error;
        MfUltralightPage page;

        // You might think it odd that I'm doing this writing "by hand" and not using the flipper SDK, but this is for two reasons:
        // 1. The flipper SDK doesn't write beyond user memory
        // 2. I order the writes from least destructive to most destructive, so that if something goes wrong, recovery _might_ be possible
        do {
            // user data
            FURI_LOG_D(TAG, "Writing user data");
            view_dispatcher_send_custom_event(
                weebo->view_dispatcher, WeeboCustomEventWritingUserData);
            for(size_t i = userMemoryFirst; i <= userMemoryLast; i++) {
                memcpy(
                    page.data, modified + (i * MF_ULTRALIGHT_PAGE_SIZE), MF_ULTRALIGHT_PAGE_SIZE);
                error = mf_ultralight_poller_write_page(poller, i, &page);
                if(error != MfUltralightErrorNone) {
                    FURI_LOG_E(TAG, "Error writing page %zu: %d", i, error);
                    view_dispatcher_send_custom_event(
                        weebo->view_dispatcher, WeeboCustomEventWriteFailure);
                    ret = NfcCommandStop;
                    break;
                }
            }
            if(error != MfUltralightErrorNone) {
                ret = NfcCommandStop;
                break;
            }

            view_dispatcher_send_custom_event(
                weebo->view_dispatcher, WeeboCustomEventWritingConfigData);
            FURI_LOG_D(TAG, "Writing config");

            // pwd
            memcpy(page.data, PWD, sizeof(PWD));
            error = mf_ultralight_poller_write_page(poller, pwd, &page);
            if(error != MfUltralightErrorNone) {
                view_dispatcher_send_custom_event(
                    weebo->view_dispatcher, WeeboCustomEventWriteFailure);
                FURI_LOG_E(TAG, "Error writing PWD: %d", error);
                ret = NfcCommandStop;
                break;
            }

            // pack
            memcpy(page.data, PACKRFUI, sizeof(PACKRFUI));
            error = mf_ultralight_poller_write_page(poller, pack, &page);
            if(error != MfUltralightErrorNone) {
                view_dispatcher_send_custom_event(
                    weebo->view_dispatcher, WeeboCustomEventWriteFailure);
                FURI_LOG_E(TAG, "Error writing PACKRFUI: %d", error);
                ret = NfcCommandStop;
                break;
            }

            // capability container
            memcpy(page.data, CC, sizeof(CC));
            error = mf_ultralight_poller_write_page(poller, capabilityContainer, &page);
            if(error != MfUltralightErrorNone) {
                view_dispatcher_send_custom_event(
                    weebo->view_dispatcher, WeeboCustomEventWriteFailure);
                FURI_LOG_E(TAG, "Error writing CC: %d", error);
                ret = NfcCommandStop;
                break;
            }

            // cfg0
            memcpy(page.data, CFG0, sizeof(CFG0));
            error = mf_ultralight_poller_write_page(poller, cfg0, &page);
            if(error != MfUltralightErrorNone) {
                view_dispatcher_send_custom_event(
                    weebo->view_dispatcher, WeeboCustomEventWriteFailure);
                FURI_LOG_E(TAG, "Error writing CFG0: %d", error);
                ret = NfcCommandStop;
                break;
            }

            // cfg1
            memcpy(page.data, CFG1, sizeof(CFG1));
            error = mf_ultralight_poller_write_page(poller, cfg1, &page);
            if(error != MfUltralightErrorNone) {
                view_dispatcher_send_custom_event(
                    weebo->view_dispatcher, WeeboCustomEventWriteFailure);
                FURI_LOG_E(TAG, "Error writing CFG1: %d", error);
                ret = NfcCommandStop;
                break;
            }

            // dynamic lock bits
            memcpy(page.data, DLB, sizeof(DLB));
            error = mf_ultralight_poller_write_page(poller, dynamicLockBits, &page);
            if(error != MfUltralightErrorNone) {
                view_dispatcher_send_custom_event(
                    weebo->view_dispatcher, WeeboCustomEventWriteFailure);
                FURI_LOG_E(TAG, "Error writing DLB: %d", error);
                ret = NfcCommandStop;
                break;
            }

            // static lock bits
            memcpy(page.data, SLB, sizeof(SLB));
            error = mf_ultralight_poller_write_page(poller, staticLockBits, &page);
            if(error != MfUltralightErrorNone) {
                view_dispatcher_send_custom_event(
                    weebo->view_dispatcher, WeeboCustomEventWriteFailure);
                FURI_LOG_E(TAG, "Error writing SLB: %d", error);
                ret = NfcCommandStop;
                break;
            }
        } while(false);
        ret = NfcCommandStop;
        view_dispatcher_send_custom_event(weebo->view_dispatcher, WeeboCustomEventWriteSuccess);
    } else {
        FURI_LOG_D(TAG, "Unhandled event type: %d", mf_ultralight_event->type);
    }
    return ret;
}

void weebo_scene_write_on_enter(void* context) {
    Weebo* weebo = context;
    Popup* popup = weebo->popup;

    popup_set_header(popup, "Present NTAG215", 58, 28, AlignCenter, AlignCenter);

    weebo->poller = nfc_poller_alloc(weebo->nfc, NfcProtocolMfUltralight);
    nfc_poller_start(weebo->poller, weebo_scene_write_poller_callback, weebo);

    weebo_blink_start(weebo);

    view_dispatcher_switch_to_view(weebo->view_dispatcher, WeeboViewPopup);
}

bool weebo_scene_write_on_event(void* context, SceneManagerEvent event) {
    Weebo* weebo = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(weebo->scene_manager, WeeboSceneWrite, event.event);
        if(event.event == WeeboCustomEventCardDetected) {
            popup_set_text(weebo->popup, "Card detected", 64, 36, AlignCenter, AlignTop);
            consumed = true;
        } else if(event.event == WeeboCustomEventWritingUserData) {
            popup_set_text(weebo->popup, "Writing user data", 64, 36, AlignCenter, AlignTop);
            consumed = true;
        } else if(event.event == WeeboCustomEventWritingConfigData) {
            popup_set_text(weebo->popup, "Writing config data", 64, 36, AlignCenter, AlignTop);
            consumed = true;
        } else if(event.event == WeeboCustomEventWriteSuccess) {
            popup_set_text(weebo->popup, "Write success", 64, 36, AlignCenter, AlignTop);
            consumed = true;
            scene_manager_next_scene(weebo->scene_manager, WeeboSceneWriteCardSuccess);
        } else if(event.event == WeeboCustomEventWrongCard) {
            popup_set_text(weebo->popup, "Wrong card", 64, 36, AlignCenter, AlignTop);
            consumed = true;
        } else if(event.event == WeeboCustomEventWriteFailure) {
            popup_set_text(weebo->popup, "Write failure", 64, 36, AlignCenter, AlignTop);
            consumed = true;
        }
    }

    return consumed;
}

void weebo_scene_write_on_exit(void* context) {
    Weebo* weebo = context;

    if(weebo->poller) {
        nfc_poller_stop(weebo->poller);
        nfc_poller_free(weebo->poller);
        weebo->poller = NULL;
    }

    popup_reset(weebo->popup);
    weebo_blink_stop(weebo);
}
