// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "../recon_app_i.h"
#include "../helpers/recon_nfc.h"
#include "../helpers/recon_report.h"

#include <math.h>

typedef enum {
    NfcCustomLog = 300,
    NfcCustomDeepCheck,
} NfcCustomEvent;

/** True when the best detected protocol is MIFARE Classic (deep check applies). */
static bool recon_scene_nfc_is_mfc(FuriString* title) {
    // Matches the firmware's nfc_device_get_protocol_name() spelling exactly.
    return furi_string_equal_str(title, "Mifare Classic");
}

/** Format a UID byte array as space-less uppercase hex into buf. */
static void recon_scene_nfc_uid_hex(const uint8_t* uid, uint8_t len, char* buf, size_t buf_len) {
    size_t j = 0;
    for(uint8_t i = 0; i < len && j + 2 < buf_len; i++) {
        j += snprintf(buf + j, buf_len - j, "%02X", uid[i]);
    }
    if(buf_len) buf[j] = '\0';
}

static void recon_scene_nfc_button_cb(GuiButtonType type, InputType input, void* context) {
    ReconApp* app = context;
    if(input != InputTypeShort) return;
    if(type == GuiButtonTypeCenter) {
        view_dispatcher_send_custom_event(app->view_dispatcher, NfcCustomLog);
    } else if(type == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, NfcCustomDeepCheck);
    }
}

static void recon_scene_nfc_render(
    ReconApp* app,
    bool detected,
    FuriString* title,
    FuriString* grade,
    FuriString* detail) {
    Widget* widget = app->widget;
    widget_reset(widget);

    if(!detected) {
        widget_add_string_multiline_element(
            widget,
            64,
            28,
            AlignCenter,
            AlignCenter,
            FontSecondary,
            "Present a card to the\nback of the Flipper...");
        return;
    }

    bool is_mfc = recon_scene_nfc_is_mfc(title);

    // While a deep check runs, show a transient status banner in place of detail.
    if(is_mfc && recon_nfc_deep_check_busy(app->nfc)) {
        widget_add_string_element(
            widget, 0, 2, AlignLeft, AlignTop, FontPrimary, furi_string_get_cstr(title));
        widget_add_string_multiline_element(
            widget, 64, 34, AlignCenter, AlignCenter, FontSecondary, "Checking default keys...");
        return;
    }

    // If a deep check completed for this MIFARE Classic card, surface the UID;
    // the grade + detail already carry the upgraded verdict from recon_nfc_get.
    ReconMfcResult mfc;
    bool have_mfc = is_mfc && recon_nfc_deep_check_get(app->nfc, &mfc) && mfc.valid;

    widget_add_string_element(
        widget, 0, 2, AlignLeft, AlignTop, FontPrimary, furi_string_get_cstr(title));
    widget_add_string_element(
        widget, 127, 2, AlignRight, AlignTop, FontPrimary, furi_string_get_cstr(grade));

    if(have_mfc) {
        char uid_hex[24];
        recon_scene_nfc_uid_hex(mfc.uid, mfc.uid_len, uid_hex, sizeof(uid_hex));
        char line[48];
        snprintf(
            line,
            sizeof(line),
            "UID %s%s",
            uid_hex[0] ? uid_hex : "?",
            mfc.aborted ? " (partial)" : "");
        widget_add_string_element(widget, 0, 14, AlignLeft, AlignTop, FontSecondary, line);
        widget_add_text_scroll_element(widget, 0, 26, 128, 20, furi_string_get_cstr(detail));
    } else {
        widget_add_text_scroll_element(widget, 0, 16, 128, 30, furi_string_get_cstr(detail));
    }

    widget_add_button_element(widget, GuiButtonTypeCenter, "Log", recon_scene_nfc_button_cb, app);
    // Keep "Deep" available even after a result exists, so a newly presented card
    // can always be re-checked instead of inheriting the previous card's verdict.
    if(is_mfc) {
        widget_add_button_element(
            widget, GuiButtonTypeRight, "Deep", recon_scene_nfc_button_cb, app);
    }
}

void recon_scene_nfc_on_enter(void* context) {
    ReconApp* app = context;
    app->nfc = recon_nfc_alloc(app);
    recon_nfc_start(app->nfc);
    app->text_store[0] = '\0';

    FuriString* dummy = furi_string_alloc();
    recon_scene_nfc_render(app, false, dummy, dummy, dummy);
    furi_string_free(dummy);

    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewWidget);
}

bool recon_scene_nfc_on_event(void* context, SceneManagerEvent event) {
    ReconApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeTick) {
        FuriString* title = furi_string_alloc();
        FuriString* grade = furi_string_alloc();
        FuriString* detail = furi_string_alloc();
        bool detected = recon_nfc_get(app->nfc, title, grade, detail);

        // Re-render only when the displayed card/grade/deep-check state changes
        // (avoids flicker). The deep-check busy flag is part of the signature so
        // the "Checking..." banner appears and clears promptly.
        char sig[64];
        if(detected) {
            snprintf(
                sig,
                sizeof(sig),
                "%s|%s|%d",
                furi_string_get_cstr(title),
                furi_string_get_cstr(grade),
                recon_nfc_deep_check_busy(app->nfc) ? 1 : 0);
        } else {
            sig[0] = '\0';
        }
        if(strncmp(sig, app->text_store, sizeof(sig)) != 0) {
            strncpy(app->text_store, sig, RECON_TEXT_STORE - 1);
            app->text_store[RECON_TEXT_STORE - 1] = '\0';
            recon_scene_nfc_render(app, detected, title, grade, detail);
        }

        furi_string_free(title);
        furi_string_free(grade);
        furi_string_free(detail);
        consumed = true;
    } else if(event.type == SceneManagerEventTypeCustom && event.event == NfcCustomLog) {
        FuriString* title = furi_string_alloc();
        FuriString* grade = furi_string_alloc();
        FuriString* detail = furi_string_alloc();
        if(recon_nfc_get(app->nfc, title, grade, detail)) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            bool fix = app->gps_valid;
            float lat = app->gps_lat, lon = app->gps_lon;
            furi_mutex_release(app->mutex);

            // Add UID hex + default-keyed sector count when a deep check ran on a
            // MIFARE Classic card; non-Classic cards leave those fields blank.
            char uid_hex[24];
            char sectors[8];
            uid_hex[0] = '\0';
            sectors[0] = '\0';
            ReconMfcResult mfc;
            if(recon_scene_nfc_is_mfc(title) && recon_nfc_deep_check_get(app->nfc, &mfc) &&
               mfc.valid) {
                recon_scene_nfc_uid_hex(mfc.uid, mfc.uid_len, uid_hex, sizeof(uid_hex));
                snprintf(sectors, sizeof(sectors), "%u", mfc.default_keyed);
            }

            FuriString* line = furi_string_alloc();
            if(fix) {
                furi_string_printf(
                    line,
                    "%s,%s,%s,%s,%.6f,%.6f",
                    furi_string_get_cstr(title),
                    furi_string_get_cstr(grade),
                    uid_hex,
                    sectors,
                    (double)lat,
                    (double)lon);
            } else {
                furi_string_printf(
                    line,
                    "%s,%s,%s,%s,,",
                    furi_string_get_cstr(title),
                    furi_string_get_cstr(grade),
                    uid_hex,
                    sectors);
            }
            bool ok = recon_report_append_nfc(app, furi_string_get_cstr(line));
            furi_string_free(line);

            if(app->settings.sound) {
                notification_message(app->notifications, ok ? &sequence_success : &sequence_error);
            }
        }
        furi_string_free(title);
        furi_string_free(grade);
        furi_string_free(detail);
        consumed = true;
    } else if(event.type == SceneManagerEventTypeCustom && event.event == NfcCustomDeepCheck) {
        // Only meaningful for a MIFARE Classic card; the button is hidden
        // otherwise, but guard anyway.
        FuriString* title = furi_string_alloc();
        if(recon_nfc_get(app->nfc, title, NULL, NULL) && recon_scene_nfc_is_mfc(title)) {
            recon_nfc_deep_check_start(app->nfc);
            // Force the next tick to re-render with the busy banner.
            app->text_store[0] = '\0';
        }
        furi_string_free(title);
        consumed = true;
    }
    return consumed;
}

void recon_scene_nfc_on_exit(void* context) {
    ReconApp* app = context;
    if(app->nfc) {
        recon_nfc_stop(app->nfc);
        recon_nfc_free(app->nfc);
        app->nfc = NULL;
    }
    widget_reset(app->widget);
}
