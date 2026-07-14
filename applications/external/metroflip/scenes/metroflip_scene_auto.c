#include "../metroflip_i.h"
#include <dolphin/dolphin.h>
#include <furi.h>
#include <bit_lib.h>
#include <lib/nfc/protocols/nfc_protocol.h>
#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>
#include <nfc/protocols/mf_classic/mf_classic.h>
#include <nfc/protocols/mf_classic/mf_classic_poller.h>
#include "keys.h"
#include "desfire.h"
#include <nfc/protocols/mf_desfire/mf_desfire_poller.h>
#include <lib/nfc/protocols/mf_desfire/mf_desfire.h>
#include <lib/nfc/protocols/iso14443_4a/iso14443_4a.h>
#include <lib/nfc/protocols/iso14443_4a/iso14443_4a_poller.h>
#include "../api/metroflip/metroflip_api.h"

/* Icon declarations (compiled from images/, not in API table) */
extern const Icon I_DolphinScan1_97x61;
extern const Icon I_DolphinScan2_97x61;
extern const Icon I_DolphinScan3_97x61;

#define TAG "Metroflip:Scene:Auto"

/* ── Scan animation ── */

typedef struct {
    uint8_t frame;
    const char* status;
} ScanAnimModel;

/* Dolphin frames: no waves -> 1 wave -> 2 waves -> all 3 waves (original) */
static const Icon* dolphin_scan_frames[] = {
    &I_DolphinScan1_97x61,
    &I_DolphinScan2_97x61,
    &I_DolphinScan3_97x61,
    &I_RFIDDolphinReceive_97x61,
};

static void scan_anim_draw(Canvas* canvas, void* model) {
    if(!model) return;
    ScanAnimModel* m = (ScanAnimModel*)model;

    canvas_set_bitmap_mode(canvas, true);
    canvas_set_color(canvas, ColorBlack);

    /* Draw dolphin with progressive waves */
    canvas_draw_icon(canvas, 0, 3, dolphin_scan_frames[m->frame % 4]);

    /* Status text with animated dots */
    canvas_set_font(canvas, FontPrimary);
    const char* dots[] = {"", ".", "..", "..."};
    char text[32];
    snprintf(text, sizeof(text), "%s%s", m->status ? m->status : "Scanning", dots[m->frame % 4]);
    canvas_draw_str(canvas, 68, 55, text);
}

static bool scan_anim_input(InputEvent* event, void* context) {
    Metroflip* app = (Metroflip*)context;
    if(event->key == InputKeyBack) {
        if(event->type == InputTypeShort) {
            /* Send as custom event so the scene transition happens
               asynchronously, after this input callback has returned. */
            view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventViewExit);
        }
        return true; /* consume all back events (press/short/release) */
    }
    return false;
}

static uint32_t scan_anim_previous(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

static NfcCommand
    metroflip_scene_detect_desfire_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(event.protocol == NfcProtocolMfDesfire);

    Metroflip* app = context;
    NfcCommand command = NfcCommandContinue;

    const MfDesfirePollerEvent* mf_desfire_event = event.event_data;
    if(mf_desfire_event->type == MfDesfirePollerEventTypeReadSuccess) {
        nfc_device_set_data(
            app->nfc_device, NfcProtocolMfDesfire, nfc_poller_get_data(app->poller));
        const MfDesfireData* data = nfc_device_get_data(app->nfc_device, NfcProtocolMfDesfire);
        furi_string_reset(app->text_box_store);
        app->card_type = desfire_type(data);

        // For T-Mobilitat Desfire cards, extract card number from ISO14443_4a historical bytes
        if(app->card_type && strstr(app->card_type, "T-mobilitat") != NULL) {
            const Iso14443_4aData* iso_data =
                nfc_device_get_data(app->nfc_device, NfcProtocolIso14443_4a);
            if(iso_data) {
                memset(app->hist_bytes, 0, sizeof(app->hist_bytes));
                app->hist_bytes_count = 0;

                uint32_t hb_count;
                const uint8_t* hb = iso14443_4a_get_historical_bytes(iso_data, &hb_count);
                if(hb && hb_count > 0) {
                    memcpy(app->hist_bytes, hb, MIN(hb_count, sizeof(app->hist_bytes)));
                    app->hist_bytes_count = MIN(hb_count, sizeof(app->hist_bytes));
                    app->card_type = "tmobilitat";
                    app->is_desfire = false;
                }
            }
        }

        view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventPollerSuccess);

        command = NfcCommandStop;
    } else if(mf_desfire_event->type == MfDesfirePollerEventTypeReadFailed) {
        /* Report the failure honestly and keep polling - entering Parse here
           would use a stale card_type from a previous scan. */
        view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventPollerFail);
        command = NfcCommandContinue;
    }

    return command;
}

static NfcCommand
    metroflip_scene_detect_iso14443_4a_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(event.protocol == NfcProtocolIso14443_4a);

    Metroflip* app = context;
    const Iso14443_4aPollerEvent* iso14443_4a_event = event.event_data;

    if(iso14443_4a_event->type == Iso14443_4aPollerEventTypeReady) {
        nfc_device_set_data(
            app->nfc_device, NfcProtocolIso14443_4a, nfc_poller_get_data(app->poller));

        const Iso14443_4aData* data = nfc_device_get_data(app->nfc_device, NfcProtocolIso14443_4a);

        // Clear stale historical bytes
        memset(app->hist_bytes, 0, sizeof(app->hist_bytes));
        app->hist_bytes_count = 0;

        uint32_t hist_bytes_count;
        const uint8_t* hist_bytes = iso14443_4a_get_historical_bytes(data, &hist_bytes_count);
        if(hist_bytes && hist_bytes_count > 0) {
            memcpy(app->hist_bytes, hist_bytes, MIN(hist_bytes_count, sizeof(app->hist_bytes)));
            app->hist_bytes_count = MIN(hist_bytes_count, sizeof(app->hist_bytes));
        }

        // Determine card type from historical bytes
        if(app->hist_bytes_count >= 2 && app->hist_bytes[0] == 0x2A &&
           app->hist_bytes[1] == 0x26) {
            app->card_type = "tmobilitat";
        } else if(
            app->hist_bytes_count >= 2 && app->hist_bytes[0] == 0x04 &&
            app->hist_bytes[1] == 0x09) {
            app->card_type = "tmoney";
        } else {
            app->card_type = "atr";
        }

        view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventPollerSuccess);
        return NfcCommandStop;
    }

    return NfcCommandContinue;
}

void metroflip_scene_detect_scan_callback(NfcScannerEvent event, void* context) {
    furi_assert(context);
    Metroflip* app = context;

    if(event.type == NfcScannerEventTypeDetected) {
        view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventCardDetected);
        if(event.data.protocols && event.data.protocol_num > 0) {
            nfc_detected_protocols_set(
                app->detected_protocols, event.data.protocols, event.data.protocol_num);
        } else {
            /* Store a real one-element list so the PollerDetect handler can
               read index 0 (the old code passed a casted enum as the pointer
               with count 0, which tripped the idx<num assert downstream). */
            const NfcProtocol invalid_protocol = NfcProtocolInvalid;
            nfc_detected_protocols_set(app->detected_protocols, &invalid_protocol, 1);
        }
        view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventPollerDetect);
    }
}

void metroflip_scene_auto_on_enter(void* context) {
    Metroflip* app = context;
    dolphin_deed(DolphinDeedNfcRead);

    app->sec_num = 0;
    /* app->poller may be a stale pointer left over from a previous plugin's
       poller (freed in its on_exit). This scene owns the slot from here on. */
    app->poller = NULL;

    /* Configure scan animation view (allocated in metroflip_alloc) */
    view_set_draw_callback(app->scan_anim, scan_anim_draw);
    view_set_input_callback(app->scan_anim, scan_anim_input);
    view_set_previous_callback(app->scan_anim, scan_anim_previous);

    if(!view_get_model(app->scan_anim)) {
        view_allocate_model(app->scan_anim, ViewModelTypeLockFree, sizeof(ScanAnimModel));
    }
    with_view_model(
        app->scan_anim,
        ScanAnimModel * m,
        {
            m->frame = 0;
            m->status = "Scanning";
        },
        false);

    /* Start NFC scanner */
    view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewLoading);
    app->scanner = nfc_scanner_alloc(app->nfc);
    nfc_scanner_start(app->scanner, metroflip_scene_detect_scan_callback, app);

    metroflip_app_blink_start(app);
}

bool metroflip_scene_auto_on_event(void* context, SceneManagerEvent event) {
    Metroflip* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MetroflipCustomEventTick) {
            /* Animate scan view */
            if(app->scan_anim && view_get_model(app->scan_anim)) {
                with_view_model(
                    app->scan_anim, ScanAnimModel * m, { m->frame = (m->frame + 1) % 4; }, true);
            }
            consumed = true;
        } else if(event.event == MetroflipCustomEventCardDetected) {
            /* Update scan animation text */
            if(app->scan_anim) {
                with_view_model(
                    app->scan_anim, ScanAnimModel * m, { m->status = "Card found"; }, true);
            }
            consumed = true;
        } else if(event.event == MetroflipCustomEventPollerSuccess) {
            /* Stop animation, switch to popup for status */
            Popup* popup = app->popup;
            popup_set_icon(popup, 0, 3, &I_RFIDDolphinReceive_97x61);
            popup_set_header(popup, "Reading\ncomplete!\nParsing...", 68, 30, AlignLeft, AlignTop);
            view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewPopup);
            nfc_poller_stop(app->poller);
            nfc_poller_free(app->poller);
            app->poller = NULL;
            scene_manager_next_scene(app->scene_manager, MetroflipSceneParse);
            consumed = true;
        } else if(event.event == MetroflipCustomEventCardLost) {
            if(app->scan_anim) {
                with_view_model(
                    app->scan_anim, ScanAnimModel * m, { m->status = "Card lost"; }, true);
            }
            consumed = true;
        } else if(event.event == MetroflipCustomEventWrongCard) {
            if(app->scan_anim) {
                with_view_model(
                    app->scan_anim, ScanAnimModel * m, { m->status = "Unsupported"; }, true);
            }
            consumed = true;
        } else if(event.event == MetroflipCustomEventPollerFail) {
            /* The detect poller keeps retrying; tell the user to re-seat the
               card. (The scan anim text only shows if we're still on it.) */
            popup_set_header(
                app->popup, "Read failed.\nHold card\nsteady...", 68, 30, AlignLeft, AlignTop);
            if(app->scan_anim) {
                with_view_model(
                    app->scan_anim, ScanAnimModel * m, { m->status = "Read failed"; }, true);
            }
            consumed = true;
        } else if(event.event == MetroflipCustomEventPollerDetect) {
            nfc_scanner_stop(app->scanner);
            nfc_scanner_free(app->scanner);
            app->auto_mode = true;
            /* Stop animation, switch to popup for protocol-specific status */
            Popup* popup = app->popup;
            popup_set_icon(popup, 0, 3, &I_RFIDDolphinReceive_97x61);
            view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewPopup);
            NfcProtocol proto = nfc_detected_protocols_get_protocol(app->detected_protocols, 0);
            FURI_LOG_I(TAG, "proto: %d", proto);
            if(proto == NfcProtocolMfClassic) {
                popup_set_header(
                    popup,
                    "MIFARE Classic\ndetected.\nReading keys...",
                    68,
                    30,
                    AlignLeft,
                    AlignTop);
                MfClassicData* mfc_data = mf_classic_alloc();
                app->data_loaded = false;
                CardType card_type = determine_card_type(app->nfc, mfc_data, app->data_loaded);
                mf_classic_free(mfc_data);
                app->mfc_card_type = card_type;
                switch(card_type) {
                case CARD_TYPE_METROMONEY:
                    app->card_type = "metromoney";
                    FURI_LOG_I(TAG, "Detected: Metromoney");
                    popup_set_header(popup, "MetroMoney\ndetected!", 68, 30, AlignLeft, AlignTop);
                    break;
                case CARD_TYPE_CHARLIECARD:
                    app->card_type = "charliecard";
                    FURI_LOG_I(TAG, "Detected: CharlieCard");
                    popup_set_header(popup, "CharlieCard\ndetected!", 68, 30, AlignLeft, AlignTop);
                    break;
                case CARD_TYPE_SMARTRIDER:
                    app->card_type = "smartrider";
                    FURI_LOG_I(TAG, "Detected: SmartRider");
                    popup_set_header(popup, "SmartRider\ndetected!", 68, 30, AlignLeft, AlignTop);
                    break;
                case CARD_TYPE_TROIKA:
                    app->card_type = "troika";
                    FURI_LOG_I(TAG, "Detected: Troika");
                    popup_set_header(popup, "Troika\ndetected!", 68, 30, AlignLeft, AlignTop);
                    break;
                case CARD_TYPE_RENFE_SUM10:
                    app->card_type = "renfe_sum10";
                    FURI_LOG_I(TAG, "Detected: RENFE Suma 10");
                    popup_set_header(
                        popup, "RENFE Suma 10\ndetected!", 68, 30, AlignLeft, AlignTop);
                    break;
                case CARD_TYPE_RENFE_REGULAR:
                    app->card_type = "renfe_regular";
                    FURI_LOG_I(TAG, "Detected: RENFE Regular");
                    popup_set_header(
                        popup, "RENFE Regular\ndetected!", 68, 30, AlignLeft, AlignTop);
                    break;
                case CARD_TYPE_UNKNOWN:
                    app->card_type = "Unknown Card";
                    popup_set_header(
                        popup, "MIFARE Classic\nUnknown card", 58, 31, AlignLeft, AlignTop);
                    break;
                default:
                    app->card_type = "Unknown Card";
                    FURI_LOG_I(TAG, "Detected: Unknown card type");
                    popup_set_header(
                        popup, "MIFARE Classic\nUnknown card", 58, 31, AlignLeft, AlignTop);
                    break;
                }
                app->is_desfire = false;
                scene_manager_next_scene(app->scene_manager, MetroflipSceneParse);
                consumed = true;
            } else if(proto == NfcProtocolIso14443_4b) {
                popup_set_header(
                    popup, "ISO14443-4B\nCalypso card\ndetected!", 68, 30, AlignLeft, AlignTop);
                app->card_type = "calypso";
                app->is_desfire = false;
                scene_manager_next_scene(app->scene_manager, MetroflipSceneParse);
                consumed = true;
            } else if(proto == NfcProtocolFelica) {
                popup_set_header(
                    popup, "FeliCa card\ndetected!\nReading...", 68, 30, AlignLeft, AlignTop);
                app->card_type = "suica";
                app->is_desfire = false;
                scene_manager_next_scene(app->scene_manager, MetroflipSceneParse);
                consumed = true;
            } else if(proto == NfcProtocolMfDesfire) {
                popup_set_header(
                    popup, "DESFire card\ndetected!\nReading AIDs...", 68, 30, AlignLeft, AlignTop);
                app->is_desfire = true;
                app->poller = nfc_poller_alloc(app->nfc, NfcProtocolMfDesfire);
                nfc_poller_start(app->poller, metroflip_scene_detect_desfire_poller_callback, app);
                consumed = true;
            } else if(proto == NfcProtocolIso14443_4a) {
                popup_set_header(
                    popup, "ISO14443-4A\ndetected!\nReading ATR...", 68, 30, AlignLeft, AlignTop);
                app->is_desfire = false;
                app->poller = nfc_poller_alloc(app->nfc, NfcProtocolIso14443_4a);
                nfc_poller_start(
                    app->poller, metroflip_scene_detect_iso14443_4a_poller_callback, app);
                consumed = true;
            } else if(proto == NfcProtocolSt25tb) {
                popup_set_header(popup, "ST25TB card\ndetected!", 68, 30, AlignLeft, AlignTop);
                FURI_LOG_I(TAG, "Protocol is ST25TB");
                app->card_type = "intertic";
                app->is_desfire = false;
                scene_manager_next_scene(app->scene_manager, MetroflipSceneParse);
                consumed = true;
            } else if(proto == NfcProtocolMfUltralight) {
                popup_set_header(popup, "MIFARE UL\ndetected!", 68, 30, AlignLeft, AlignTop);
                FURI_LOG_I(TAG, "Protocol is MfUl");
                app->card_type = "trt";
                app->is_desfire = false;
                scene_manager_next_scene(app->scene_manager, MetroflipSceneParse);
                consumed = true;
            } else if(proto == NfcProtocolInvalid) {
                app->card_type = "Unknown Card";
                popup_set_header(popup, "Unknown\nprotocol", 58, 31, AlignLeft, AlignTop);
                scene_manager_next_scene(app->scene_manager, MetroflipSceneParse);
                consumed = true;
            } else {
                app->card_type = "Unknown Card";
                popup_set_header(popup, "Unsupported\nprotocol", 68, 30, AlignLeft, AlignTop);
                scene_manager_next_scene(app->scene_manager, MetroflipSceneParse);
                consumed = true;
            }
        } else if(event.event == MetroflipCustomEventViewExit) {
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, MetroflipSceneStart);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, MetroflipSceneStart);
        consumed = true;
    }

    return consumed;
}

void metroflip_scene_auto_on_exit(void* context) {
    Metroflip* app = context;
    if(!app->auto_mode) {
        nfc_scanner_stop(app->scanner);
        nfc_scanner_free(app->scanner);
    }
    app->auto_mode = false;

    /* If the user backed out while the DESFire/ISO14443-4A detect poller was
       still running, stop it here - otherwise it would keep polling (and
       posting events) after this scene is gone. On the success path the
       poller was already freed and NULLed by the PollerSuccess handler. */
    if(app->poller) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
        app->poller = NULL;
    }

    popup_reset(app->popup);

    metroflip_app_blink_stop(app);
}
