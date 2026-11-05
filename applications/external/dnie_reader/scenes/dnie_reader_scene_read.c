#include "../dnie_reader.h"
#include "dnie_reader_scene.h"
#include "../dnie_sounds.h"

/*
 * Lectura del DNIe:
 *  A. Activar el chip (ISO14443-4B) y leer EF.CardAccess (publico).
 *  B. PACE completo con el CAN -> KSenc/KSmac (canal seguro).
 *  C. Secure Messaging: leer DG1 (MRZ) y parsearlo.
 *  D. EF.SOD + DG14: hashes firmados y datos de Chip Authentication.
 *  E. DG2 (foto) a la SD con su hash; al terminar se extrae la imagen.
 *  F. Chip Authentication: el chip demuestra que tiene la clave privada de DG14.
 */

static const char* const DNIE_STEP_NAMES[] = {
    "Reading EF.CardAccess",
    "1/4 Encrypted nonce",
    "2/4 Mapping (ECC)",
    "3/4 ECDH (ECC)",
    "4/4 Tokens",
    "Reading DG1 (encrypted)",
    "Checking SOD/DG14",
    "Photo (DG2)",
    "Authenticating chip",
};
#define DNIE_READ_STEPS 8
#define DNIE_STEP_PHOTO 7 // muestra el porcentaje

static void dnie_scene_read_draw(DnieApp* app) {
    Widget* widget = app->widget;
    uint8_t step = app->pace_progress;
    if(step > DNIE_READ_STEPS) step = DNIE_READ_STEPS;

    widget_reset(widget);
    widget_add_string_element(
        widget,
        64,
        2,
        AlignCenter,
        AlignTop,
        FontPrimary,
        step == 0 ? (app->foreign_id ? "Hold the document" :
                     app->passport   ? "Hold the passport" :
                                       "Hold the ID") :
                    (step <= DNIE_PACE_STEPS ? "PACE in progress" : "Secure channel OK"));
    widget_add_string_multiline_element(
        widget,
        64,
        18,
        AlignCenter,
        AlignTop,
        FontSecondary,
        step ?
            "Don't move it,\nthis may take a\nfew seconds" :
            (app->passport && !app->foreign_id ? "(photo page)\nto the back of\nthe Flipper" :
                                                 "to the back of the\nFlipper; don't\nmove it"));
    if(step == DNIE_STEP_PHOTO) {
        char line[32];
        snprintf(line, sizeof(line), "%s: %u%%", DNIE_STEP_NAMES[step], app->photo_percent);
        widget_add_string_element(widget, 64, 54, AlignCenter, AlignTop, FontSecondary, line);
    } else {
        widget_add_string_element(
            widget, 64, 54, AlignCenter, AlignTop, FontSecondary, DNIE_STEP_NAMES[step]);
    }
}

// Hilo del poller -> GUI: solo mediante custom events
static void dnie_scene_read_progress(uint8_t step, void* context) {
    DnieApp* app = context;
    app->pace_progress = step;
    view_dispatcher_send_custom_event(app->view_dispatcher, DnieCustomEventProgress);
}

static void dnie_scene_read_photo_progress(uint8_t percent, void* context) {
    DnieApp* app = context;
    app->photo_percent = percent;
    view_dispatcher_send_custom_event(app->view_dispatcher, DnieCustomEventProgress);
}

// OJO: este callback corre en el hilo del poller, no en el de la GUI.
// Solo hablamos con la GUI mediante custom events.
static NfcCommand dnie_scene_read_poller_callback(NfcGenericEvent event, void* context) {
    DnieApp* app = context;
    bool ready;
    if(event.protocol == NfcProtocolIso14443_4a) {
        const Iso14443_4aPollerEvent* e = event.event_data;
        ready = e->type == Iso14443_4aPollerEventTypeReady;
    } else {
        const Iso14443_4bPollerEvent* e = event.event_data;
        ready = e->type == Iso14443_4bPollerEventTypeReady;
    }
    if(!ready) return NfcCommandContinue; // aun no hay documento en el campo

    DnieTransport transport = {.protocol = event.protocol, .poller = event.instance};
    DnieTransport* tr = &transport;
    char mrz_info[25];
    if(app->passport)
        dnie_bac_mrz_info(app->pass_number, app->pass_birth, app->pass_expiry, mrz_info);

    bool ok = false;
    FURI_LOG_I(
        "DnieRead",
        "Inicio: heap libre %u B, bloque max %u B",
        memmgr_get_free_heap(),
        memmgr_heap_get_max_free_block());
    do {
        // Paso A: EF.CardAccess (publico). En pasaportes antiguos no existe: BAC.
        app->read_result = dnie_card_read_card_access(
            tr, app->card_access, sizeof(app->card_access), &app->card_access_len, &app->last_sw);
        if(app->read_result == DnieCardErrorTransport) break;
        bool pace = false;
        if(app->read_result == DnieCardOk) {
            app->pace_info_count = dnie_pace_info_parse(
                app->card_access, app->card_access_len, app->pace_infos, DNIE_PACE_INFO_MAX);
            pace = dnie_pace_choose(app->pace_infos, app->pace_info_count, &app->pace.info);
        }
        if(!app->passport && !pace) {
            if(app->read_result == DnieCardOk) app->pace_result = DniePaceErrorUnsupported;
            break;
        }
        app->read_result = DnieCardOk; // para el pasaporte, sin CardAccess seguimos con BAC

        DnieSm sm;
        if(pace) {
            // Paso B: PACE con el CAN (DNIe) o con la MRZ (pasaporte)
            DniePacePassword pw;
            if(app->passport) {
                dnie_bac_pace_password(mrz_info, &pw);
            } else {
                pw.ref = DNIE_PACE_PW_CAN;
                pw.len = strlen(app->can);
                memcpy(pw.secret, app->can, pw.len);
            }
            app->pace_result =
                dnie_pace_establish(tr, &pw, &app->pace, dnie_scene_read_progress, app);
            memset(&pw, 0, sizeof(pw));
            if(app->pace_result != DniePaceOk) break;
            dnie_sm_init(&sm, app->pace.ks_enc, app->pace.ks_mac);
            // Las claves de sesion ya estan en sm: fuera de app->pace
            memset(app->pace.ks_enc, 0, sizeof(app->pace.ks_enc));
            memset(app->pace.ks_mac, 0, sizeof(app->pace.ks_mac));
        } else {
            // Paso B': BAC (pasaporte sin PACE)
            app->used_bac = true;
            dnie_scene_read_progress(1, app);
            app->bac_result = dnie_bac_establish(tr, mrz_info, &sm, &app->last_sw);
            if(app->bac_result != DnieBacOk) {
                app->pace_result = DniePaceErrorWrongCan; // para que el error se muestre
                break;
            }
            app->pace_result = DniePaceOk;
        }
        memset(mrz_info, 0, sizeof(mrz_info));

        // Paso C: DG1 dentro del canal seguro
        dnie_scene_read_progress(DNIE_PACE_STEPS + 1, app);

        if(dnie_lds_select_app(tr, &sm, &app->lds) == DnieLdsOk) {
            dnie_lds_read_dg1(tr, &sm, app->dg1, sizeof(app->dg1), &app->dg1_len, &app->lds);
        }
        if(app->lds.result != DnieLdsOk) {
            dnie_sm_clear(&sm);
            break;
        }
        app->mrz_ok = dnie_mrz_parse_dg1(app->dg1, app->dg1_len, &app->mrz);
        ok = true; // con DG1 ya tenemos lectura valida; la foto es un extra

        // Paso D: autenticidad (SOD con los hashes firmados + DG14)
        dnie_scene_read_progress(DNIE_PACE_STEPS + 2, app);
        dnie_auth_read(tr, &sm, app->dg1, app->dg1_len, &app->auth);

        // Paso E: foto (DG2) volcada a la SD, hasheandola por el camino
        app->photo_percent = 0;
        dnie_scene_read_progress(DNIE_PACE_STEPS + 3, app);
        DnieHash dg2_hash;
        bool hashing = dnie_auth_dg2_hash_start(&app->auth, &dg2_hash);
        DnieLdsResult pr = dnie_photo_read_dg2(
            tr, &sm, &app->photo, dnie_scene_read_photo_progress, app, hashing ? &dg2_hash : NULL);
        if(hashing) dnie_auth_dg2_hash_done(&app->auth, &dg2_hash, pr == DnieLdsOk);

        // Paso F: Chip Authentication (al final: cambia el canal a 3DES con claves nuevas)
        if(app->auth.dg14_ok && pr != DnieLdsErrorSm && pr != DnieLdsErrorTransport) {
            dnie_scene_read_progress(DNIE_PACE_STEPS + 4, app);
            dnie_ca_run(tr, &sm, &app->auth.dg14, app->dg1, &app->auth.ca);
            FURI_LOG_I(
                "DnieRead", "Chip Auth: %d (SW %04X)", app->auth.ca.result, app->auth.ca.sw);
        }
        dnie_sm_clear(&sm);
        dnie_auth_compact(&app->auth); // fuera SOD y DG14: liberamos memoria para la foto
    } while(false);
    memset(mrz_info, 0, sizeof(mrz_info));
    dnie_auth_compact(&app->auth); // tambien si algo fallo a medias
    FURI_LOG_I(
        "DnieRead",
        "Heap libre %u B, bloque max %u B",
        memmgr_get_free_heap(),
        memmgr_heap_get_max_free_block());

    view_dispatcher_send_custom_event(
        app->view_dispatcher, ok ? DnieCustomEventReadOk : DnieCustomEventReadError);
    return NfcCommandStop;
}

// Hilo del escaner: solo anotar el protocolo y avisar a la GUI
static void dnie_scene_read_scanner_callback(NfcScannerEvent event, void* context) {
    DnieApp* app = context;
    if(event.type != NfcScannerEventTypeDetected || app->protocol != NfcProtocolInvalid) return;
    NfcProtocol found = NfcProtocolInvalid;
    for(size_t i = 0; i < event.data.protocol_num; i++) {
        NfcProtocol p = event.data.protocols[i];
        if(p == NfcProtocolIso14443_4b) found = p;
        if(p == NfcProtocolIso14443_4a && found == NfcProtocolInvalid) found = p;
    }
    if(found == NfcProtocolInvalid) return; // otra tarjeta: seguimos buscando
    app->protocol = found;
    view_dispatcher_send_custom_event(app->view_dispatcher, DnieCustomEventDetected);
}

void dnie_scene_read_on_enter(void* context) {
    DnieApp* app = context;
    app->card_access_len = 0;
    app->last_sw = 0;
    app->pace_info_count = 0;
    app->dg1_len = 0;
    app->mrz_ok = false;
    memset(app->dg1, 0, sizeof(app->dg1));
    memset(&app->mrz, 0, sizeof(app->mrz));
    memset(&app->lds, 0, sizeof(app->lds));
    memset(&app->photo, 0, sizeof(app->photo));
    app->photo_decode_tried = false;
    app->viewing_saved = false;
    app->viewing_album = false;
    app->demo = false;
    dnie_auth_reset(&app->auth);
    app->photo_decode_result = DnieJ2kErrorFormat;
    memset(&app->pix, 0, sizeof(app->pix));
    app->photo.lds.result = DnieLdsErrorTransport; // "not read" hasta que se lea
    app->read_result = DnieCardErrorTransport;
    app->pace_result = DniePaceErrorTransport;
    memset(&app->pace, 0, sizeof(app->pace));

    app->pace_progress = 0;
    app->protocol = NfcProtocolInvalid;
    dnie_scene_read_draw(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, DnieViewWidget);

    // LED cian parpadeando mientras buscamos, como la app NFC oficial
    notification_message(app->notifications, &sequence_blink_start_cyan);

    dnie_sound_stop(); // por si sigue sonando el himno de la lectura anterior
    app->used_bac = false;
    app->bac_result = DnieBacOk;
    // Primero detectar el tipo de chip (DNIe: 4B; pasaportes: 4A o 4B)
    app->scanner = nfc_scanner_alloc(app->nfc);
    nfc_scanner_start(app->scanner, dnie_scene_read_scanner_callback, app);
}

bool dnie_scene_read_on_event(void* context, SceneManagerEvent event) {
    DnieApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == DnieCustomEventDetected) {
            // Fuera el escaner (no se puede parar desde su propio hilo) y a leer
            if(app->scanner) {
                nfc_scanner_stop(app->scanner);
                nfc_scanner_free(app->scanner);
                app->scanner = NULL;
            }
            app->poller = nfc_poller_alloc(app->nfc, app->protocol);
            nfc_poller_start(app->poller, dnie_scene_read_poller_callback, app);
            consumed = true;
        } else if(event.event == DnieCustomEventProgress) {
            dnie_scene_read_draw(app);
            consumed = true;
        } else if(event.event == DnieCustomEventReadOk || event.event == DnieCustomEventReadError) {
            // Extraer la foto de DG2 (acceso a SD: aqui, en el hilo de la GUI)
            if(app->photo.lds.result == DnieLdsOk && app->photo.dg2_len > 0) {
                dnie_photo_extract(&app->photo);
            }
            notification_message(app->notifications, &sequence_blink_stop);
            // Con datos validos, el carnet; si no, la pantalla de detalles con el error
            bool card = event.event == DnieCustomEventReadOk && app->mrz_ok;
            DnieVerdict v = card ? dnie_verdict(&app->auth) : DnieVerdictUnknown;
            // Original: Marcha Real. No autentico: sonido grave + vibracion larga.
            // Sin veredicto: pitido normal (exito) o de error, como el NFC.
            // Autentico: himno del pais. No autentico: sonido grave + vibracion larga.
            // Sin veredicto: pitido normal (exito) o de error, como el NFC.
            if(v == DnieVerdictOriginal || v == DnieVerdictGenuine) {
                dnie_sound_original(app->notifications, app->mrz.issuer);
            } else if(v == DnieVerdictFake) {
                dnie_sound_fake(app->notifications);
            } else {
                notification_message(
                    app->notifications,
                    event.event == DnieCustomEventReadOk ? &sequence_success : &sequence_error);
            }
            app->card_hint_pending = card; // en el carnet: "Up: save"
            scene_manager_next_scene(
                app->scene_manager,
                !card                ? DnieSceneResult :
                v == DnieVerdictFake ? DnieSceneWarning :
                                       DnieSceneCard);
            consumed = true;
        }
    }

    return consumed;
}

void dnie_scene_read_on_exit(void* context) {
    DnieApp* app = context;

    if(app->scanner) {
        nfc_scanner_stop(app->scanner);
        nfc_scanner_free(app->scanner);
        app->scanner = NULL;
    }
    if(app->poller) {
        nfc_poller_stop(app->poller); // espera a que termine el hilo del poller
        nfc_poller_free(app->poller);
        app->poller = NULL;
    }
    notification_message(app->notifications, &sequence_blink_stop); // por si salimos con Atras
    widget_reset(app->widget);
}
