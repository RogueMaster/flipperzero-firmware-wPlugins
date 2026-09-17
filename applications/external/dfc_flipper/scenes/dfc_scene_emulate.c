#include "../dfc_i.h"
#include "dfc_virtual_picc.h"
#include <dolphin/dolphin.h>

#define TAG "DfcSceneEmulate"

#define DFC_ATS_T0_TA1 0x10
#define DFC_ATS_T0_TB1 0x20
#define DFC_ATS_T0_TC1 0x40

// Unpack a flat ATS into the listener's fields. The answer is self-describing:
// its first octet is its own length and T0 says which interface bytes follow.
// Anything the credential omits stays at zero, which is what the listener treats
// as absent.
static void dfc_set_listener_ats(Iso14443_4aData* data, const uint8_t* ats, size_t ats_len) {
    data->ats_data.tl = 0;
    data->ats_data.t0 = 0;
    data->ats_data.ta_1 = 0;
    data->ats_data.tb_1 = 0;
    data->ats_data.tc_1 = 0;
    simple_array_reset(data->ats_data.t1_tk);
    if(ats_len == 0) return;

    // The length octet is authoritative; it cannot exceed what was handed over,
    // because a credential whose ATS disagrees with it is refused at load.
    size_t declared = ats[0];
    if(declared > ats_len) declared = ats_len;

    data->ats_data.tl = ats[0];
    size_t i = 1;
    if(i < declared) data->ats_data.t0 = ats[i++];
    if((data->ats_data.t0 & DFC_ATS_T0_TA1) && i < declared) data->ats_data.ta_1 = ats[i++];
    if((data->ats_data.t0 & DFC_ATS_T0_TB1) && i < declared) data->ats_data.tb_1 = ats[i++];
    if((data->ats_data.t0 & DFC_ATS_T0_TC1) && i < declared) data->ats_data.tc_1 = ats[i++];

    size_t historical_len = declared - i;
    if(historical_len > 0) {
        simple_array_init(data->ats_data.t1_tk, historical_len);
        memcpy(simple_array_get_data(data->ats_data.t1_tk), ats + i, historical_len);
    }
}

void dfc_scene_emulate_on_enter(void* context) {
    Dfc* dfc = context;
    dolphin_deed(DolphinDeedNfcEmulate);

    // Setup view
    Popup* popup = dfc->popup;
    popup_set_header(popup, "Emulating", 68, 30, AlignLeft, AlignTop);
    popup_set_icon(popup, 0, 3, &I_RFIDDolphinSend_97x61);

    if(!dfc_credential_uid_is_detectable(dfc->credential)) {
        popup_set_header(popup, "Invalid UID", 64, 24, AlignCenter, AlignTop);
        popup_set_text(popup, "Use 7 bytes\nstarting with 04", 64, 42, AlignCenter, AlignTop);
        view_dispatcher_switch_to_view(dfc->view_dispatcher, DfcViewPopup);
        return;
    }

    // One authority for anticollision, shared with the scripted sessions: the
    // credential's random ID, ATS, SAK and ATQA where it states them, the
    // built-in defaults where it does not.
    DfcVirtualPiccActivation activation;
    dfc_virtual_picc_anticollision(dfc->credential, &activation);

    nfc_device_load(dfc->nfc_device, APP_ASSETS_PATH("dfc.nfc"));
    if(!nfc_device_set_uid(dfc->nfc_device, activation.uid, activation.uid_len)) {
        popup_set_header(popup, "UID Error", 64, 24, AlignCenter, AlignTop);
        popup_set_text(popup, "Could not set UID", 64, 42, AlignCenter, AlignTop);
        view_dispatcher_switch_to_view(dfc->view_dispatcher, DfcViewPopup);
        return;
    }

    const Iso14443_4aData* data = nfc_device_get_data(dfc->nfc_device, NfcProtocolIso14443_4a);

    Iso14443_4aData* mutable_data = (Iso14443_4aData*)data;
    if(!iso14443_4a_set_uid(mutable_data, activation.uid, activation.uid_len)) {
        popup_set_header(popup, "UID Error", 64, 24, AlignCenter, AlignTop);
        popup_set_text(popup, "Could not set UID", 64, 42, AlignCenter, AlignTop);
        view_dispatcher_switch_to_view(dfc->view_dispatcher, DfcViewPopup);
        return;
    }
    FURI_LOG_I(
        TAG,
        "Emulating %u-octet NFCID1 %02X%02X%02X%02X",
        (unsigned)activation.uid_len,
        activation.uid[0],
        activation.uid[1],
        activation.uid[2],
        activation.uid[3]);

    iso14443_3a_set_atqa(mutable_data->iso14443_3a_data, activation.atqa);
    iso14443_3a_set_sak(mutable_data->iso14443_3a_data, activation.sak);

    // The listener holds the ATS as separate fields, so the answer is unpacked
    // into them: TL, T0, then whichever of TA(1), TB(1) and TC(1) T0 says are
    // present, then the historical bytes. The default keeps the bitrate
    // capability byte absent, which advertises 106 kbit/s only.
    dfc_set_listener_ats(mutable_data, activation.ats, activation.ats_len);

    furi_assert(!dfc->dfc_emulator);
    dfc->dfc_emulator = dfc_emulator_alloc(dfc->credential);
    if(!dfc->dfc_emulator) {
        popup_set_header(popup, "Out of memory", 64, 24, AlignCenter, AlignTop);
        popup_set_text(popup, "Could not start\nemulation", 64, 42, AlignCenter, AlignTop);
        view_dispatcher_switch_to_view(dfc->view_dispatcher, DfcViewPopup);
        return;
    }

    dfc->listener = nfc_listener_alloc(dfc->nfc, NfcProtocolIso14443_4a, data);
    nfc_listener_start(dfc->listener, dfc_worker_listener_callback, dfc);

    dfc_blink_start(dfc);

    view_dispatcher_switch_to_view(dfc->view_dispatcher, DfcViewPopup);
}

bool dfc_scene_emulate_on_event(void* context, SceneManagerEvent event) {
    Dfc* dfc = context;
    Popup* popup = dfc->popup;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == DfcCustomEventEmulate) {
            popup_set_header(popup, "Emulating", 68, 30, AlignLeft, AlignTop);
        } else if(event.event == DfcCustomEventAppSelected) {
            popup_set_header(popup, "App\nSelected", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == DfcCustomEventAuthenticated) {
            popup_set_header(popup, "Auth'd", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == DfcCustomEventFileRequested) {
            popup_set_header(popup, "File\nRequested", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        if(dfc->credential->dirty) {
            scene_manager_next_scene(dfc->scene_manager, DfcSceneMutationSave);
            consumed = true;
        } else if(dfc->emulate_from_blank) {
            dfc->emulate_from_blank = false;
            // No mutation occurred, so the blank card template does not need saving.
            // Fall through to default back handling (return to MainMenu).
        }
        // else: fall through to default back handling (return to SavedMenu)
    }

    return consumed;
}

void dfc_scene_emulate_on_exit(void* context) {
    Dfc* dfc = context;

    if(dfc->listener) {
        nfc_listener_stop(dfc->listener);
        nfc_listener_free(dfc->listener);
        dfc->listener = NULL;
    }

    if(dfc->dfc_emulator) {
        dfc_emulator_free(dfc->dfc_emulator);
        dfc->dfc_emulator = NULL;
    }

    // Clear view
    popup_reset(dfc->popup);

    dfc_blink_stop(dfc);
}
