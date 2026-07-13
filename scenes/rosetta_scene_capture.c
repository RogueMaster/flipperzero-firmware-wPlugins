#include "../rosetta_i.h"
#include <stdio.h>

/* The capture scene fronts one of two views depending on the protocol:
 *  - Mifare / 1-Wire  -> capture_view (present a tag, then annotated fields)
 *  - OOK & PSK        -> scope_view   (a live Sub-GHz RF envelope)
 * It owns starting/stopping the matching live reader and, on each UI tick,
 * pumping the reader's state into the view. */

static const char* onewire_family_name(uint8_t family) {
    switch(family) {
    case 0x01:
        return "DS1990A key";
    case 0x02:
        return "DS1991 mem";
    case 0x04:
        return "DS1994 clock";
    case 0x05:
        return "DS2405 switch";
    case 0x06:
        return "DS1993 4Kb";
    case 0x08:
        return "DS1992 1Kb";
    case 0x0A:
        return "DS1995 16Kb";
    case 0x0C:
        return "DS1996 64Kb";
    case 0x10:
        return "DS1920 temp";
    case 0x81:
        return "DS1420 serial";
    case 0x91:
        return "DS1425 temp";
    default:
        return "unknown";
    }
}

/* ----------------------------------------------------- annotation builders */

static void build_nfc_annot(const NfcReading* r, CaptureAnnot* a) {
    memset(a, 0, sizeof(*a));

    snprintf(
        a->lines[a->nline++], CAPTURE_LINE_LEN, "Tech: %.18s", r->tech[0] ? r->tech : "ISO14443");

    if(r->uid_len) {
        char uid[24];
        int off = 0;
        for(uint8_t i = 0; i < r->uid_len && off < (int)sizeof(uid) - 3; i++) {
            off += snprintf(uid + off, sizeof(uid) - off, "%02X", r->uid[i]);
        }
        snprintf(a->lines[a->nline++], CAPTURE_LINE_LEN, "UID: %.20s", uid);
    }

    snprintf(
        a->lines[a->nline++],
        CAPTURE_LINE_LEN,
        "SAK %02X  ATQA %02X%02X",
        r->sak,
        r->atqa[1],
        r->atqa[0]);

    snprintf(
        a->lines[a->nline++],
        CAPTURE_LINE_LEN,
        "UID is %d bytes long",
        (int)r->uid_len);

    /* Tie the read straight back into the walkthrough. */
    if(r->uid_len == 4) {
        snprintf(a->verdict, CAPTURE_VERDICT_LEN, "4-byte UID: cloneable");
        a->verdict_kind = CaptureVerdictBad;
    } else {
        snprintf(a->verdict, CAPTURE_VERDICT_LEN, "Next: 3-pass Crypto1");
        a->verdict_kind = CaptureVerdictNeutral;
    }
}

static void build_onewire_annot(const OneWireReading* r, CaptureAnnot* a) {
    memset(a, 0, sizeof(*a));

    uint8_t family = r->rom[0];
    snprintf(
        a->lines[a->nline++],
        CAPTURE_LINE_LEN,
        "Family %02X: %s",
        family,
        onewire_family_name(family));

    /* 48-bit serial = bytes 1..6, printed MSB-first for readability. */
    snprintf(
        a->lines[a->nline++],
        CAPTURE_LINE_LEN,
        "SN %02X%02X%02X%02X%02X%02X",
        r->rom[6],
        r->rom[5],
        r->rom[4],
        r->rom[3],
        r->rom[2],
        r->rom[1]);

    snprintf(
        a->lines[a->nline++],
        CAPTURE_LINE_LEN,
        "CRC %02X  calc %02X",
        r->rom[7],
        r->crc_calc);

    if(r->crc_ok) {
        snprintf(a->verdict, CAPTURE_VERDICT_LEN, "CRC valid: genuine ROM");
        a->verdict_kind = CaptureVerdictGood;
    } else {
        snprintf(a->verdict, CAPTURE_VERDICT_LEN, "CRC mismatch: reread");
        a->verdict_kind = CaptureVerdictBad;
    }
}

/* --------------------------------------------------------- rescan bridge */

static void rosetta_capture_rescan_cb(void* ctx) {
    RosettaApp* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, RosettaCustomEventCaptureRescan);
}

static void capture_begin(RosettaApp* app) {
    /* scene state acts as a "result consumed" latch: 0 = still listening,
     * 1 = a result is on screen (so the tick stops re-firing it). */
    scene_manager_set_scene_state(app->scene_manager, RosettaSceneCapture, 0);
    if(app->protocol == ProtocolMifare) {
        capture_view_reset(app->capture_view, "Reading NFC");
        nfc_reader_start(app->nfc);
    } else { // ProtocolOneWire
        capture_view_reset(app->capture_view, "Touch iButton");
        onewire_reader_start(app->onewire);
    }
}

/* ------------------------------------------------------------- scene glue */

void rosetta_scene_capture_on_enter(void* context) {
    RosettaApp* app = context;

    if(app->protocol == ProtocolModulation) {
        rf_scope_set_freq_index(app->rf, app->rf_freq_index);
        scope_view_reset(app->scope_view);
        rf_scope_start(app->rf);
        view_dispatcher_switch_to_view(app->view_dispatcher, RosettaViewScope);
        return;
    }

    capture_view_set_rescan_cb(app->capture_view, rosetta_capture_rescan_cb, app);
    capture_begin(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, RosettaViewCapture);
}

bool rosetta_scene_capture_on_event(void* context, SceneManagerEvent event) {
    RosettaApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeTick) {
        if(app->protocol == ProtocolModulation) {
            RfSnapshot snap;
            rf_scope_get(app->rf, &snap);
            scope_view_set_snapshot(app->scope_view, &snap);
        } else {
            capture_view_tick(app->capture_view);

            /* Only latch the first result until the user asks to re-scan;
             * the reader keeps state == Ready after we stop it, so without
             * this gate the tick would re-fire the result every frame. */
            bool consumed_result =
                scene_manager_get_scene_state(app->scene_manager, RosettaSceneCapture);

            if(!consumed_result && app->protocol == ProtocolMifare) {
                if(nfc_reader_state(app->nfc) == NfcReaderReady) {
                    NfcReading r;
                    if(nfc_reader_get(app->nfc, &r)) {
                        CaptureAnnot a;
                        build_nfc_annot(&r, &a);
                        capture_view_set_result(app->capture_view, &a);
                        rosetta_notify_capture(app, a.verdict_kind != CaptureVerdictBad);
                    }
                    nfc_reader_stop(app->nfc);
                    scene_manager_set_scene_state(app->scene_manager, RosettaSceneCapture, 1);
                }
            } else if(!consumed_result) { // ProtocolOneWire
                if(onewire_reader_state(app->onewire) == OneWireReady) {
                    OneWireReading r;
                    if(onewire_reader_get(app->onewire, &r)) {
                        CaptureAnnot a;
                        build_onewire_annot(&r, &a);
                        capture_view_set_result(app->capture_view, &a);
                        rosetta_notify_capture(app, r.crc_ok);
                    }
                    onewire_reader_stop(app->onewire);
                    scene_manager_set_scene_state(app->scene_manager, RosettaSceneCapture, 1);
                }
            }
        }
        consumed = true;
    } else if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == RosettaCustomEventCaptureRescan) {
            capture_begin(app);
            consumed = true;
        }
    }
    return consumed;
}

void rosetta_scene_capture_on_exit(void* context) {
    RosettaApp* app = context;
    nfc_reader_stop(app->nfc);
    onewire_reader_stop(app->onewire);
    rf_scope_stop(app->rf);
}
