// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Cafe com Solda / Cristiano Santana

#include "../chaos_id_app.h"
#include "chaos_id_scene.h"
#include "../storage/scan_log.h"

#include <string.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller_sync.h>

#define LF_PHASE_DURATION_MS 4000
#define HF_PHASE_DURATION_MS 2500

// =========================================================================
// LF (125 kHz) - app-lifetime worker, start/stop por scan
// =========================================================================

static void lf_read_callback(LFRFIDWorkerReadResult result, ProtocolId protocol, void* context) {
    ChaosIdApp* app = context;

    if(result != LFRFIDWorkerReadDone) return;
    if(app->lf_done) return;
    app->lf_done = true;

    const size_t proto_index = (size_t)protocol;
    const char* proto_name = protocol_dict_get_name(app->lf_dict, proto_index);

    FuriString* tmp = furi_string_alloc();

    size_t data_size = protocol_dict_get_data_size(app->lf_dict, proto_index);
    if(data_size > 0 && data_size <= 16) {
        uint8_t raw[16];
        protocol_dict_get_data(app->lf_dict, proto_index, raw, data_size);
        char* p = app->uid_buffer;
        size_t remaining = sizeof(app->uid_buffer);
        for(size_t i = 0; i < data_size && remaining > 3; i++) {
            int n = snprintf(p, remaining, "%02X ", raw[i]);
            if(n <= 0 || (size_t)n >= remaining) break;
            p += n;
            remaining -= n;
        }
        if(p > app->uid_buffer && *(p - 1) == ' ')
            *(p - 1) = '\0';
        else
            *p = '\0';
    } else {
        app->uid_buffer[0] = '\0';
    }

    furi_string_reset(tmp);
    protocol_dict_render_brief_data(app->lf_dict, tmp, proto_index);
    strncpy(app->card_data_buffer, furi_string_get_cstr(tmp), sizeof(app->card_data_buffer) - 1);
    app->card_data_buffer[sizeof(app->card_data_buffer) - 1] = '\0';

    FURI_LOG_I(
        "ChaosID",
        "LF Done: id=%ld name='%s' uid='%s' data='%s'",
        (long)protocol,
        proto_name ? proto_name : "(null)",
        app->uid_buffer,
        app->card_data_buffer);

    furi_string_free(tmp);

    app->last_result = cards_db_find_by_lfrfid_name(proto_name);

    if(app->last_result) {
        view_dispatcher_send_custom_event(app->view_dispatcher, ChaosIdCustomEventCardFound);
    } else {
        FURI_LOG_W("ChaosID", "LF detectado mas nao mapeado: '%s'", proto_name);
        view_dispatcher_send_custom_event(app->view_dispatcher, ChaosIdCustomEventScanFailed);
    }
}

static void start_phase_lf(ChaosIdApp* app) {
    app->scan_phase = 0;
    app->lf_done = false;

    popup_reset(app->popup);
    popup_set_header(app->popup, "Aproxime o cartao", 64, 10, AlignCenter, AlignTop);
    popup_set_text(app->popup, "Lendo LF (125 kHz)...", 64, 35, AlignCenter, AlignTop);

    // lf_dict e lf_worker ja alocados em chaos_id_app_alloc (app-lifetime)
    lfrfid_worker_start_thread(app->lf_worker);
    lfrfid_worker_read_start(app->lf_worker, LFRFIDWorkerReadTypeAuto, lf_read_callback, app);

    furi_timer_start(app->scan_timer, furi_ms_to_ticks(LF_PHASE_DURATION_MS));
}

static void stop_lf_worker(ChaosIdApp* app) {
    if(app->lf_worker) {
        lfrfid_worker_stop(app->lf_worker);
        lfrfid_worker_stop_thread(app->lf_worker);
        // NAO frees - worker e dict sao app-lifetime
    }
}

// =========================================================================
// HF (13.56 MHz) - Nfc app-lifetime, scanner por scan
// =========================================================================

static void hf_scanner_callback(NfcScannerEvent event, void* context) {
    ChaosIdApp* app = context;

    if(event.type != NfcScannerEventTypeDetected) return;
    if(event.data.protocol_num == 0) return;
    if(app->hf_done) return;
    app->hf_done = true;

    app->detected_protocol = event.data.protocols[0];

    FURI_LOG_I(
        "ChaosID",
        "HF scanner: num=%u proto[0]=%u",
        (unsigned)event.data.protocol_num,
        (unsigned)event.data.protocols[0]);

    view_dispatcher_send_custom_event(app->view_dispatcher, ChaosIdCustomEventHfDetected);
}

static void start_phase_hf(ChaosIdApp* app) {
    app->scan_phase = 1;
    app->hf_done = false;

    popup_reset(app->popup);
    popup_set_header(app->popup, "Aproxime o cartao", 64, 10, AlignCenter, AlignTop);
    popup_set_text(app->popup, "Lendo HF (13.56 MHz)...", 64, 35, AlignCenter, AlignTop);

    // app->nfc ja alocado em chaos_id_app_alloc (app-lifetime)
    app->nfc_scanner = nfc_scanner_alloc(app->nfc);
    nfc_scanner_start(app->nfc_scanner, hf_scanner_callback, app);

    furi_timer_start(app->scan_timer, furi_ms_to_ticks(HF_PHASE_DURATION_MS));
}

static void stop_hf_scanner(ChaosIdApp* app) {
    if(app->nfc_scanner) {
        nfc_scanner_stop(app->nfc_scanner);
        nfc_scanner_free(app->nfc_scanner);
        app->nfc_scanner = NULL;
    }
    // NAO freia app->nfc - eh app-lifetime
}

// =========================================================================
// Timer + scene callbacks
// =========================================================================

static void scan_timer_callback(void* context) {
    ChaosIdApp* app = context;
    if(app->scan_phase == 0) {
        view_dispatcher_send_custom_event(app->view_dispatcher, ChaosIdCustomEventPhaseLfDone);
    } else if(app->scan_phase == 1) {
        view_dispatcher_send_custom_event(app->view_dispatcher, ChaosIdCustomEventPhaseHfDone);
    }
}

void chaos_id_scene_scanning_on_enter(void* context) {
    ChaosIdApp* app = context;
    app->last_result = NULL;
    app->lf_done = false;
    app->nfc_scanner = NULL;
    app->hf_done = false;
    app->uid_buffer[0] = '\0';
    app->card_data_buffer[0] = '\0';
    app->scan_timer = furi_timer_alloc(scan_timer_callback, FuriTimerTypeOnce, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ChaosIdViewScanning);
    start_phase_lf(app);
}

bool chaos_id_scene_scanning_on_event(void* context, SceneManagerEvent event) {
    ChaosIdApp* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case ChaosIdCustomEventPhaseLfDone:
            if(app->lf_done) {
                consumed = true;
                break;
            }
            furi_timer_stop(app->scan_timer);
            stop_lf_worker(app);
            start_phase_hf(app);
            consumed = true;
            break;

        case ChaosIdCustomEventHfDetected: {
            furi_timer_stop(app->scan_timer);
            if(app->nfc_scanner) {
                nfc_scanner_stop(app->nfc_scanner);
                nfc_scanner_free(app->nfc_scanner);
                app->nfc_scanner = NULL;
            }

            Iso14443_3aData* data = iso14443_3a_alloc();
            Iso14443_3aError err = iso14443_3a_poller_sync_read(app->nfc, data);

            if(err == Iso14443_3aErrorNone) {
                char* p = app->uid_buffer;
                size_t remaining = sizeof(app->uid_buffer);
                for(size_t i = 0; i < data->uid_len && remaining > 3; i++) {
                    int n = snprintf(p, remaining, "%02X ", data->uid[i]);
                    if(n <= 0 || (size_t)n >= remaining) break;
                    p += n;
                    remaining -= n;
                }
                if(p > app->uid_buffer && *(p - 1) == ' ')
                    *(p - 1) = '\0';
                else
                    *p = '\0';

                snprintf(
                    app->card_data_buffer,
                    sizeof(app->card_data_buffer),
                    "ATQA: %02X%02X SAK: %02X",
                    data->atqa[1],
                    data->atqa[0],
                    data->sak);

                FURI_LOG_I(
                    "ChaosID",
                    "HF Activated: UID=%s ATQA=%02X%02X SAK=%02X",
                    app->uid_buffer,
                    data->atqa[1],
                    data->atqa[0],
                    data->sak);
            } else {
                app->uid_buffer[0] = '\0';
                snprintf(
                    app->card_data_buffer,
                    sizeof(app->card_data_buffer),
                    "(activation err=%d)",
                    (int)err);
                FURI_LOG_W("ChaosID", "HF sync_read falhou: err=%d", (int)err);
            }
            iso14443_3a_free(data);

            // NAO frees app->nfc - eh app-lifetime

            const char* proto_name = nfc_device_get_protocol_name(app->detected_protocol);
            FURI_LOG_I("ChaosID", "HF protocol_name: '%s'", proto_name ? proto_name : "(null)");
            app->last_result = cards_db_find_by_nfc_name(proto_name);

            if(app->last_result) {
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, ChaosIdCustomEventCardFound);
            } else {
                FURI_LOG_W("ChaosID", "HF nao mapeado no DB: '%s'", proto_name);
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, ChaosIdCustomEventScanFailed);
            }
            consumed = true;
            break;
        }

        case ChaosIdCustomEventPhaseHfDone:
            if(app->hf_done) {
                consumed = true;
                break;
            }
            furi_timer_stop(app->scan_timer);
            stop_hf_scanner(app);
            view_dispatcher_send_custom_event(app->view_dispatcher, ChaosIdCustomEventScanFailed);
            consumed = true;
            break;

        case ChaosIdCustomEventCardFound:
            furi_timer_stop(app->scan_timer);
            stop_lf_worker(app);
            stop_hf_scanner(app);

            if(app->last_result) {
                ScanLogEntry entry = {
                    .freq_label = card_frequency_label(app->last_result->frequency),
                    .protocol = app->last_result->protocol,
                    .uid = app->uid_buffer,
                    .data = app->card_data_buffer,
                    .risk_label = card_risk_label(app->last_result->risk),
                };
                scan_log_append(app->storage, &entry);
            }

            scene_manager_next_scene(app->scene_manager, ChaosIdSceneResult);
            consumed = true;
            break;

        case ChaosIdCustomEventScanFailed:
            furi_timer_stop(app->scan_timer);
            stop_lf_worker(app);
            stop_hf_scanner(app);
            popup_reset(app->popup);
            popup_set_header(app->popup, "Nada detectado", 64, 12, AlignCenter, AlignTop);
            popup_set_text(
                app->popup, "Reposicione o cartao\ne tente de novo", 64, 32, AlignCenter, AlignTop);
            popup_set_timeout(app->popup, 1500);
            popup_enable_timeout(app->popup);
            consumed = true;
            break;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }
    return consumed;
}

void chaos_id_scene_scanning_on_exit(void* context) {
    ChaosIdApp* app = context;
    if(app->scan_timer) {
        furi_timer_stop(app->scan_timer);
        furi_timer_free(app->scan_timer);
        app->scan_timer = NULL;
    }
    stop_lf_worker(app);
    stop_hf_scanner(app);
    popup_reset(app->popup);
}
