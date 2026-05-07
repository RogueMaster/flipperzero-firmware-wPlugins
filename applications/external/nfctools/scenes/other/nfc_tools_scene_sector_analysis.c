#include "../../include/nfc_tools_i.h"
#include "../../include/nfc_tools_icode.h"
#include "../../include/nfc_tools_mfc.h"
#include "../../include/nfc_tools_ntag.h"
#include <nfc/nfc_poller.h>
#include <nfc/protocols/felica/felica.h>
#include <nfc/protocols/felica/felica_poller.h>
#include <nfc/protocols/mf_desfire/mf_desfire_poller.h>
#include <lib/toolbox/simple_array.h>
#include <notification/notification_messages.h>

// ── Callback FeliCa pour dump des blocs S_PAD ──────────────────────────────

typedef struct {
    NfcToolsApp*   app;
    NfcPoller*     poller;
    FuriEventFlag* done;
    bool           success;
} FelicaSectorCtx;

static NfcCommand nfc_tools_felica_sector_cb(NfcGenericEvent event, void* context) {
    FelicaSectorCtx*   ctx = context;
    FelicaPollerEvent* ev  = event.event_data;

    if(ev->type == FelicaPollerEventTypeRequestAuthContext) {
        ev->data->auth_context->skip_auth = true;
        return NfcCommandContinue;
    }

    if(ev->type == FelicaPollerEventTypeReady ||
       ev->type == FelicaPollerEventTypeIncomplete) {
        NfcToolsApp*      app    = ctx->app;
        const FelicaData* felica = (const FelicaData*)nfc_poller_get_data(ctx->poller);

        const char* wf = (felica->workflow_type == FelicaLite) ? "Lite" :
                         (felica->workflow_type == FelicaStandard) ? "Standard" : "Unknown";
        furi_string_cat_printf(
            app->info_str,
            "FeliCa %s\nBlocks: %u/%u\n\n",
            wf,
            (unsigned)felica->blocks_read,
            (unsigned)felica->blocks_total);
        furi_string_cat_printf(
            app->ndef_str,
            "FeliCa %s - ASCII\nBlocks: %u/%u\n\n",
            wf,
            (unsigned)felica->blocks_read,
            (unsigned)felica->blocks_total);

        for(uint8_t i = 0; i < 14; i++) {
            const FelicaBlock* blk = &felica->data.fs.spad[i];
            bool readable = (blk->SF1 == 0);

            furi_string_cat_printf(app->info_str, "[S%02u]", (unsigned)i);
            if(readable) {
                for(uint8_t j = 0; j < 16; j++) {
                    if(j == 4 || j == 8 || j == 12) {
                        furi_string_cat_str(app->info_str, "\n     ");
                    }
                    furi_string_cat_printf(app->info_str, "%02X ", blk->data[j]);
                }
            } else {
                furi_string_cat_str(app->info_str, " (locked)");
            }
            furi_string_cat_str(app->info_str, "\n");

            furi_string_cat_printf(app->ndef_str, "[S%02u]", (unsigned)i);
            if(readable) {
                for(uint8_t j = 0; j < 16; j++) {
                    if(j == 8) furi_string_cat_str(app->ndef_str, "\n     ");
                    char c = (char)blk->data[j];
                    furi_string_cat_printf(app->ndef_str, "%c",
                        (c >= 32 && c < 127) ? c : '.');
                }
            } else {
                furi_string_cat_str(app->ndef_str, " (locked)");
            }
            furi_string_cat_str(app->ndef_str, "\n");
        }

        ctx->success = true;
    } else {
        furi_string_set(ctx->app->info_str, "Read error\nFeliCa");
    }

    furi_event_flag_set(ctx->done, 1u);
    return NfcCommandStop;
}

// ── Callback DESFire pour dump applications / fichiers ──────────────────────

typedef struct {
    NfcToolsApp*   app;
    NfcPoller*     poller;
    FuriEventFlag* done;
    bool           success;
} DesfireSectorCtx;

static NfcCommand nfc_tools_desfire_sector_cb(NfcGenericEvent event, void* context) {
    DesfireSectorCtx* ctx = context;
    const MfDesfirePollerEvent* ev = event.event_data;

    if(ev->type == MfDesfirePollerEventTypeReadSuccess) {
        NfcToolsApp*        app  = ctx->app;
        const MfDesfireData* d   = (const MfDesfireData*)nfc_poller_get_data(ctx->poller);

        uint32_t app_count = simple_array_get_count(d->application_ids);

        const char* type_str;
        switch(d->version.hw_major) {
        case 0x01: type_str = "EV1";    break;
        case 0x12: type_str = "EV2";    break;
        case 0x22: type_str = "EV2 XL"; break;
        case 0x33: type_str = "EV3";    break;
        default:   type_str = "MF3ICD40"; break;
        }
        furi_string_cat_printf(app->info_str, "DESFire %s\n", type_str);
        if(d->free_memory.is_present) {
            furi_string_cat_printf(
                app->info_str,
                "Free: %lu bytes\n",
                (unsigned long)d->free_memory.bytes_free);
        }
        furi_string_cat_printf(
            app->info_str, "%lu application(s)\n\n", (unsigned long)app_count);

        for(uint32_t i = 0; i < app_count; i++) {
            const MfDesfireApplicationId* aid =
                (const MfDesfireApplicationId*)simple_array_cget(d->application_ids, i);
            furi_string_cat_printf(
                app->info_str,
                "AID: %02X %02X %02X\n",
                aid->data[0],
                aid->data[1],
                aid->data[2]);

            const MfDesfireApplication* desfire_app = mf_desfire_get_application(d, aid);
            if(desfire_app) {
                uint32_t file_count = simple_array_get_count(desfire_app->file_ids);
                furi_string_cat_printf(
                    app->info_str, "  %lu file(s)\n", (unsigned long)file_count);

                for(uint32_t j = 0; j < file_count; j++) {
                    const MfDesfireFileId* fid =
                        (const MfDesfireFileId*)simple_array_cget(desfire_app->file_ids, j);
                    const MfDesfireFileSettings* fs =
                        mf_desfire_get_file_settings(desfire_app, fid);

                    if(fs) {
                        const char* ftype;
                        switch(fs->type) {
                        case MfDesfireFileTypeStandard:       ftype = "Std";  break;
                        case MfDesfireFileTypeBackup:         ftype = "Bak";  break;
                        case MfDesfireFileTypeValue:          ftype = "Val";  break;
                        case MfDesfireFileTypeLinearRecord:   ftype = "LRec"; break;
                        case MfDesfireFileTypeCyclicRecord:   ftype = "CRec"; break;
                        case MfDesfireFileTypeTransactionMac: ftype = "TMac"; break;
                        default:                              ftype = "?";    break;
                        }

                        if(fs->type == MfDesfireFileTypeStandard ||
                           fs->type == MfDesfireFileTypeBackup) {
                            furi_string_cat_printf(
                                app->info_str,
                                "  F%02X %s %lu B\n",
                                (unsigned)*fid,
                                ftype,
                                (unsigned long)fs->data.size);
                        } else if(fs->type == MfDesfireFileTypeLinearRecord ||
                                  fs->type == MfDesfireFileTypeCyclicRecord) {
                            furi_string_cat_printf(
                                app->info_str,
                                "  F%02X %s sz:%lu max:%lu\n",
                                (unsigned)*fid,
                                ftype,
                                (unsigned long)fs->record.size,
                                (unsigned long)fs->record.max);
                        } else {
                            furi_string_cat_printf(
                                app->info_str, "  F%02X %s\n", (unsigned)*fid, ftype);
                        }
                    } else {
                        furi_string_cat_printf(
                            app->info_str, "  F%02X\n", (unsigned)*fid);
                    }
                }
            }
        }

        ctx->success = true;

    } else {
        furi_string_set(ctx->app->info_str, "Read error\nDESFire");
    }

    furi_event_flag_set(ctx->done, 1u);
    return NfcCommandStop;
}

static int32_t nfc_tools_sector_worker(void* context) {
    NfcToolsApp* app = context;

    furi_string_reset(app->info_str);
    furi_string_reset(app->ndef_str);

    // ── Mifare Classic ──────────────────────────────────────────────────────
    if(app->detected_protocol == NfcProtocolMfClassic) {
        nfc_tools_mfc_dump(app);

    // ── MF Ultralight / NTAG ────────────────────────────────────────────────
    } else if(app->detected_protocol == NfcProtocolMfUltralight) {
        nfc_tools_ntag_dump(app);

    // ── ISO 15693 (ICODE SLI / SLIX / SLIX2) ────────────────────────────────
    } else if(app->detected_protocol == NfcProtocolIso15693_3 ||
              app->detected_protocol == NfcProtocolSlix) {
        bool ok = nfc_tools_icode_dump(app);
        if(!ok) {
            notification_message(app->notifications, &sequence_error);
            view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventAnalysisDone);
            return 0;
        }

    // ── DESFire EV1 / EV2 / EV3 ────────────────────────────────────────────
    } else if(app->detected_protocol == NfcProtocolMfDesfire) {

        DesfireSectorCtx desfire_ctx = {
            .app     = app,
            .done    = furi_event_flag_alloc(),
            .success = false,
        };
        desfire_ctx.poller = nfc_poller_alloc(app->nfc, NfcProtocolMfDesfire);
        nfc_poller_start(desfire_ctx.poller, nfc_tools_desfire_sector_cb, &desfire_ctx);

        furi_event_flag_wait(desfire_ctx.done, 1u, FuriFlagWaitAny, 8000);

        nfc_poller_stop(desfire_ctx.poller);
        nfc_poller_free(desfire_ctx.poller);
        furi_event_flag_free(desfire_ctx.done);

        if(!desfire_ctx.success) {
            notification_message(app->notifications, &sequence_error);
            view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventAnalysisDone);
            return 0;
        }

    // ── FeliCa (ISO 18092 / NFC-F) ──────────────────────────────────────────
    } else if(app->detected_protocol == NfcProtocolFelica) {

        FelicaSectorCtx felica_ctx = {
            .app     = app,
            .done    = furi_event_flag_alloc(),
            .success = false,
        };
        felica_ctx.poller = nfc_poller_alloc(app->nfc, NfcProtocolFelica);
        nfc_poller_start(felica_ctx.poller, nfc_tools_felica_sector_cb, &felica_ctx);

        furi_event_flag_wait(felica_ctx.done, 1u, FuriFlagWaitAny, 8000);

        nfc_poller_stop(felica_ctx.poller);
        nfc_poller_free(felica_ctx.poller);
        furi_event_flag_free(felica_ctx.done);

        if(!felica_ctx.success) {
            notification_message(app->notifications, &sequence_error);
            view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventAnalysisDone);
            return 0;
        }

    // ── Unsupported protocol ────────────────────────────────────────────────
    } else {
        furi_string_cat_printf(
            app->info_str,
            "Unsupported\nProtocol: %s\n",
            nfc_device_get_protocol_name(app->detected_protocol));
        notification_message(app->notifications, &sequence_error);
        view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventAnalysisDone);
        return 0;
    }

    notification_message(app->notifications, &sequence_success);
    view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventAnalysisDone);
    return 0;
}

static void nfc_tools_sector_stop_worker(NfcToolsApp* app) {
    if(app->worker_thread) {
        furi_event_flag_set(app->worker_flags, NFC_TOOLS_WORKER_FLAG_STOP);
        furi_thread_join(app->worker_thread);
        furi_thread_free(app->worker_thread);
        furi_event_flag_free(app->worker_flags);
        app->worker_thread = NULL;
        app->worker_flags = NULL;
    }
}

// ── Bouton ASCII ──────────────────────────────────────────────────────────────

static bool s_ascii_view_active = false;

static void nfc_tools_sector_ascii_btn_cb(GuiButtonType result, InputType type, void* context) {
    NfcToolsApp* app = context;
    if(result == GuiButtonTypeRight && type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, 1);
    }
}

// ── Helper: hex result display ────────────────────────────────────────────────

static void nfc_tools_sector_show_hex(NfcToolsApp* app) {
    widget_reset(app->widget);
    widget_add_text_scroll_element(
        app->widget, 0, 0, 128, 52,
        furi_string_get_cstr(app->info_str));
    widget_add_button_element(
        app->widget,
        GuiButtonTypeRight,
        NTS_BTN_ASCII,
        nfc_tools_sector_ascii_btn_cb,
        app);
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcToolsViewWidget);
}

// ── Scene ─────────────────────────────────────────────────────────────────────

void nfc_tools_scene_sector_analysis_on_enter(void* context) {
    NfcToolsApp* app = context;

    s_ascii_view_active = false;

    popup_reset(app->popup);
    popup_set_header(app->popup, NTS_POPUP_ANALYZING, 64, 10, AlignCenter, AlignCenter);
    popup_set_text(
        app->popup, NTS_POPUP_HOLD_TAG, 64, 38, AlignCenter, AlignCenter);

    app->worker_flags = furi_event_flag_alloc();
    app->worker_thread = furi_thread_alloc_ex(
        "NfcToolsSector", 2 * 1024, nfc_tools_sector_worker, app);
    furi_thread_start(app->worker_thread);

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcToolsViewPopup);
}

bool nfc_tools_scene_sector_analysis_on_event(void* context, SceneManagerEvent event) {
    NfcToolsApp* app = context;
    bool consumed = false;

    // Return from ASCII view → go back to hex Widget without leaving the scene
    if(event.type == SceneManagerEventTypeBack && s_ascii_view_active) {
        s_ascii_view_active = false;
        nfc_tools_sector_show_hex(app);
        return true; // consumed: prevents scene pop
    }

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NfcToolsEventAnalysisDone) {
            if(furi_string_size(app->ndef_str) > 0) {
                // ASCII view available → hex Widget + ASCII button
                nfc_tools_sector_show_hex(app);
            } else {
                // No ASCII view (MF Classic, DESFire) → simple TextBox
                text_box_set_font(app->text_box, TextBoxFontText);
                text_box_set_text(app->text_box, furi_string_get_cstr(app->info_str));
                text_box_set_focus(app->text_box, TextBoxFocusStart);
                view_dispatcher_switch_to_view(app->view_dispatcher, NfcToolsViewTextBox);
            }
            consumed = true;
        } else if(event.event == 1) {
            // ASCII button → switch to ASCII TextBox within the same scene
            s_ascii_view_active = true;
            text_box_set_font(app->text_box, TextBoxFontText);
            text_box_set_text(app->text_box, furi_string_get_cstr(app->ndef_str));
            text_box_set_focus(app->text_box, TextBoxFocusStart);
            view_dispatcher_switch_to_view(app->view_dispatcher, NfcToolsViewTextBox);
            consumed = true;
        }
    }

    return consumed;
}

void nfc_tools_scene_sector_analysis_on_exit(void* context) {
    NfcToolsApp* app = context;
    nfc_tools_sector_stop_worker(app);
    popup_reset(app->popup);
    widget_reset(app->widget);
    text_box_reset(app->text_box);
}
