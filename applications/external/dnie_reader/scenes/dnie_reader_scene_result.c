#include "../dnie_reader.h"
#include "dnie_reader_scene.h"

#include <furi_hal_rtc.h>
#include <string.h>

static void cat_hex(FuriString* text, const uint8_t* data, size_t len) {
    for(size_t i = 0; i < len; i++) {
        furi_string_cat_printf(text, "%02X", data[i]);
        furi_string_cat_str(text, (i % 8 == 7) ? "\n" : " ");
    }
    if(len % 8 != 0) furi_string_cat_str(text, "\n");
}

// AAMMDD -> DD/MM/AAAA. La MRZ solo guarda 2 digitos del ano:
// nacimiento: si AA es mayor que el ano actual, es del siglo pasado; caducidad: siempre 20AA.
static void cat_date(FuriString* text, const char* yymmdd, bool is_birth, uint16_t now_year) {
    int yy = (yymmdd[0] - '0') * 10 + (yymmdd[1] - '0');
    int century = 2000;
    if(is_birth && yy > now_year % 100) century = 1900;
    furi_string_cat_printf(
        text, "%c%c/%c%c/%d", yymmdd[4], yymmdd[5], yymmdd[2], yymmdd[3], century + yy);
}

// Compara AAMMDD de caducidad con la fecha del RTC
static bool is_expired(const char* yymmdd, const DateTime* now) {
    int y = 2000 + (yymmdd[0] - '0') * 10 + (yymmdd[1] - '0');
    int m = (yymmdd[2] - '0') * 10 + (yymmdd[3] - '0');
    int d = (yymmdd[4] - '0') * 10 + (yymmdd[5] - '0');
    if(y != now->year) return y < now->year;
    if(m != now->month) return m < now->month;
    return d < now->day;
}

static void cat_auth(DnieApp* app, FuriString* text) {
    const DnieAuth* a = &app->auth;
    furi_string_cat_str(text, "\n\e#Authenticity\n");
    if(!a->sod_parsed) {
        furi_string_cat_printf(
            text,
            "SOD not available:\n%s\nSW %04X\n",
            a->sod_status.result == DnieLdsOk ? "unrecognized format" :
                                                dnie_lds_result_str(a->sod_status.result),
            a->sod_status.sw);
    } else {
        if(dnie_auth_integrity_failed(a)) {
            furi_string_cat_str(text, "!! DATA TAMPERED !!\n");
        } else if(a->csca.result == DnieCscaBadSignature) {
            furi_string_cat_str(text, "!! DS CERTIFICATE\nNOT SIGNED BY\nCSCA SPAIN !!\n");
        } else if(a->ca.result == DnieCaFailed) {
            furi_string_cat_str(text, "!! CHIP AUTH FAILED !!\n");
        } else if(dnie_auth_all_ok(a)) {
            furi_string_cat_str(
                text,
                app->demo ?
                    "DEMO: what a genuine\ndocument shows\n(hashes + sig. +\nCSCA SPAIN + chip)\n" :
                    "GENUINE DOCUMENT\n(hashes + sig. +\nCSCA SPAIN + chip)\n");
        } else if(dnie_auth_data_ok(a)) {
            furi_string_cat_str(
                text,
                "AUTHENTIC DATA\n(hashes + sig. +\nCSCA; chip doesn't\nsupport Chip Auth)\n");
        } else if(dnie_auth_integrity_ok(a)) {
            furi_string_cat_str(text, "SOD hashes: OK\n");
        }
        furi_string_cat_printf(
            text,
            "DG1: %s\nDG2 (photo): %s\nDG14: %s\nmessageDigest: %s\nHash: %s, %u DGs\n",
            dnie_check_str(a->dg1),
            dnie_check_str(a->dg2),
            dnie_check_str(a->dg14_hash),
            dnie_check_str(a->message_digest),
            dnie_hash_name(a->lds_hash),
            a->dg_count);
        furi_string_cat_str(text, "DGs in SOD:");
        for(uint8_t i = 0; i < a->dg_count; i++)
            furi_string_cat_printf(text, " %u", a->dg_numbers[i]);
        furi_string_cat_printf(
            text,
            "\nSOD sig.: %s\n%s",
            dnie_pa_result_str(a->pa.result),
            a->sig_alg[0] ? a->sig_alg : "?");
        if(a->pa.key_bits) furi_string_cat_printf(text, " %u bits", a->pa.key_bits);
        furi_string_cat_printf(text, ", %s\n", dnie_hash_name(a->pa.hash));
        if(a->has_cert) {
            furi_string_cat_printf(
                text, "DS: %s\nIssuer: %s\n", a->ds_subject_cn, a->ds_issuer_cn);
            furi_string_cat_printf(text, "CSCA chain: %s\n", dnie_csca_result_str(a->csca.result));
            if(a->csca.csca_name[0]) furi_string_cat_printf(text, "(%s)\n", a->csca.csca_name);
        }
    }

    // Chip Authentication (DG14)
    furi_string_cat_printf(text, "\nChip Auth: %s\n", dnie_ca_result_str(a->ca.result));
    if(a->ca.result != DnieCaNotDone && a->ca.result != DnieCaNotAvailable) {
        furi_string_cat_printf(
            text,
            "(%s, ECC %lu.%lu s, SW %04X)\n",
            a->ca.cipher == 1 ? "3DES" : "AES",
            a->ca.ecc_ms / 1000,
            (a->ca.ecc_ms % 1000) / 100,
            a->ca.sw);
    }
    if(a->ca.result == DnieCaFailed) {
        furi_string_cat_str(
            text, "Chip did not prove\nit has the DG14\nkey: clone or app\nfailure.\n");
    }
    furi_string_cat_str(text, "\nDG14:\n");
    if(!a->dg14_ok) {
        furi_string_cat_printf(
            text, "not available (%s)\n", dnie_lds_result_str(a->dg14_status.result));
        return;
    }
    const DnieDg14* d = &a->dg14;
    for(uint8_t i = 0; i < d->info_count; i++) {
        const DnieCaInfo* ci = &d->infos[i];
        furi_string_cat_printf(
            text,
            "%s %s v%lu\n",
            ci->ka == 2 ? "ECDH" : (ci->ka == 1 ? "DH" : "?"),
            dnie_pace_cipher_str(ci->cipher),
            ci->version);
    }
    for(uint8_t i = 0; i < d->key_count; i++) {
        const DnieCaKey* k = &d->keys[i];
        furi_string_cat_printf(
            text, "Key: %s, %u B\n", dnie_curve_name(k->curve), k->public_key_len);
        if(k->has_key_id) furi_string_cat_printf(text, "keyId %lu\n", k->key_id);
    }
    if(d->info_count == 0 && d->key_count == 0) furi_string_cat_str(text, "no CA data\n");
    furi_string_cat_printf(
        text,
        "Other:%s%s%s (%u infos)\n",
        d->has_pace ? " PACE" : "",
        d->has_ta ? " TA" : "",
        d->has_active_auth ? " AA" : "",
        d->total_infos);
}

static void cat_photo(DnieApp* app, FuriString* text) {
    const DniePhoto* p = &app->photo;
    furi_string_cat_str(text, "\n\e#Photo (DG2)\n");
    if(p->lds.result != DnieLdsOk) {
        furi_string_cat_printf(
            text,
            "Not read:\n%s\nStep %u/3, SW %04X\n",
            dnie_lds_result_str(p->lds.result),
            p->lds.step,
            p->lds.sw);
        if(p->lds.result == DnieLdsErrorSm) {
            furi_string_cat_printf(text, "SM: %s\n", dnie_sm_result_str(p->lds.sm_result));
        }
        return;
    }
    if(!p->image_found) {
        furi_string_cat_printf(text, "DG2 read (%u B) but\nimage not found\n", p->dg2_len);
        return;
    }
    furi_string_cat_printf(
        text,
        "%s %ux%u\n%u KB",
        p->dg2.image_type == DnieImageJpeg2000 ? "JPEG2000" :
        p->dg2.image_type == DnieImageJpeg     ? "JPEG" :
                                                 "Format ?",
        p->dg2.width,
        p->dg2.height,
        (unsigned)((p->dg2.image_len + 512) / 1024));
    if(p->saved) {
        furi_string_cat_printf(
            text, ", deleted from SD\nafter converting to\npixel art (privacy)\n");
    } else {
        furi_string_cat_str(text, "\nCould not save\n");
    }
    if(p->j2k_ok) {
        const DnieJ2kInfo* j = &p->j2k;
        furi_string_cat_printf(
            text,
            "%s, %u comp, %u bits\nWavelet %s, %u levels\nBlocks %ux%u, %u layer%s\nOrder %s, %s\nTiles %lux%lu, QCD %u\n",
            j->jp2_container ? "JP2" : "J2K",
            j->components,
            j->depth,
            j->transform ? "5/3" : "9/7",
            j->levels,
            1u << j->cb_w,
            1u << j->cb_h,
            j->layers,
            j->layers == 1 ? "" : "s",
            dnie_j2k_progression_str(j->progression),
            j->mct ? "MCT" : "no MCT",
            j->tile_w,
            j->tile_h,
            j->qcd_style);
        if(j->cb_style || j->custom_precincts) {
            furi_string_cat_printf(
                text, "CB style %02X%s\n", j->cb_style, j->custom_precincts ? ", precincts" : "");
        }
    }
    if(app->photo_decode_tried) {
        if(app->photo_decode_result == DnieJ2kOk) {
            furi_string_cat_printf(
                text,
                "Pixel art %ux%u\nfrom %ux%u (1/%u)\nStyle: %s\n",
                app->pix.width,
                app->pix.height,
                app->photo_decoded_w,
                app->photo_decoded_h,
                1u << app->photo_reduce,
                dnie_dither_name(app->dither));
        } else {
            furi_string_cat_printf(
                text, "Pixel art: %s\n", dnie_j2k_result_str(app->photo_decode_result));
        }
    }
}

static void cat_technical(DnieApp* app, FuriString* text) {
    furi_string_cat_printf(
        text,
        "\n\e#Technical details\nChip ISO14443-%s\nAccess: %s\n",
        app->protocol == NfcProtocolIso14443_4a ? "4A" : "4B",
        app->used_bac ? "BAC (3DES)" : (app->passport ? "PACE with MRZ" : "PACE with CAN"));
    if(app->used_bac) return;
    furi_string_cat_printf(
        text,
        "PACE %s %s\n%s\nECC crypto: %lu.%lu s\n",
        dnie_pace_mapping_str(app->pace.info.mapping),
        dnie_pace_cipher_str(app->pace.info.cipher),
        dnie_pace_param_str(app->pace.info.param_id),
        app->pace.ecc_ms / 1000,
        (app->pace.ecc_ms % 1000) / 100);

    furi_string_cat_printf(text, "\nPACE options (%u):\n", app->pace_info_count);
    for(size_t i = 0; i < app->pace_info_count; i++) {
        const DniePaceInfo* info = &app->pace_infos[i];
        furi_string_cat_printf(
            text,
            "%s %s v%lu\n",
            dnie_pace_mapping_str(info->mapping),
            dnie_pace_cipher_str(info->cipher),
            info->version);
        if(info->has_param_id) {
            furi_string_cat_printf(
                text, " %s (%lu)\n", dnie_pace_param_str(info->param_id), info->param_id);
        }
    }
    furi_string_cat_printf(text, "\nEF.CardAccess (%u B):\n", app->card_access_len);
    cat_hex(text, app->card_access, app->card_access_len);
}

void dnie_scene_result_on_enter(void* context) {
    DnieApp* app = context;
    Widget* widget = app->widget;
    widget_reset(widget);

    FuriString* text = furi_string_alloc();
    DateTime now;
    furi_hal_rtc_get_datetime(&now);

    if(app->viewing_saved && app->viewing_album) {
        DateTime saved;
        datetime_timestamp_to_datetime(app->saved_at, &saved);
        furi_string_printf(
            text,
            "\e#%s\n%s\n%s\nBirthday: %c%c/%c%c/%.4s\n\n\e#Album\nOnly the name,\nphoto and birthday\nwere saved.\nNo document\nnumbers.\n\nSaved %02u/%02u/%u\nEncrypted with this\nFlipper's unique key\n\n\e#Card controls\n<- -> photo style\nHold OK: rename\nHold down:\ndelete\n",
            app->saved_alias[0] ? app->saved_alias : app->album_given,
            app->saved_alias[0] ? "" : app->album_surnames,
            dnie_country_name(app->album_issuer),
            app->album_birth[6],
            app->album_birth[7],
            app->album_birth[4],
            app->album_birth[5],
            app->album_birth,
            saved.day,
            saved.month,
            saved.year);
    } else if(app->viewing_saved) {
        const DnieMrz* m = &app->mrz;
        DateTime saved;
        datetime_timestamp_to_datetime(app->saved_at, &saved);
        furi_string_printf(
            text,
            "\e#%s\n%s\n",
            app->saved_alias[0] ? app->saved_alias : m->given_names,
            app->saved_alias[0] ? "" : m->surnames);
        furi_string_cat_printf(
            text, "ID: %s\nSupport: %s\nBorn: ", m->personal_number, m->doc_number);
        cat_date(text, m->birth, true, now.year);
        furi_string_cat_str(text, "\nExpires: ");
        cat_date(text, m->expiry, false, now.year);
        if(is_expired(m->expiry, &now)) furi_string_cat_str(text, " (EXPIRED)");
        furi_string_cat_printf(text, "\nSex: %c   Nat.: %s\n", m->sex, m->nationality);
        furi_string_cat_printf(
            text,
            "MRZ check: %s\n\n\e#Saved reading\n%02u/%02u/%u %02u:%02u\nEncrypted with this\nFlipper's unique key\n",
            dnie_mrz_all_checks_ok(m) ? "OK" : "FAIL",
            saved.day,
            saved.month,
            saved.year,
            saved.hour,
            saved.minute);
        furi_string_cat_str(
            text,
            "\n\e#Card controls\n<- -> photo style\nHold OK: rename\nHold down:\ndelete reading\n");
    } else if(app->read_result != DnieCardOk) {
        furi_string_printf(
            text,
            "\e#EF.CardAccess error\n%s\n\nSW: %04X",
            dnie_card_result_str(app->read_result),
            app->last_sw);

    } else if(app->used_bac && app->bac_result != DnieBacOk) {
        furi_string_printf(
            text,
            "\e#BAC error\n%s\nSW %04X\n",
            dnie_bac_result_str(app->bac_result),
            app->last_sw);
        furi_string_cat_str(text, "Check the passport\nnumber and dates\n");
        cat_technical(app, text);

    } else if(app->pace_result != DniePaceOk) {
        furi_string_printf(
            text,
            "\e#PACE error\n%s\nStep %u/4, SW %04X\n",
            dnie_pace_result_str(app->pace_result),
            app->pace.step,
            app->pace.sw);
        if(app->pace_result == DniePaceErrorWrongCan) {
            furi_string_cat_str(
                text,
                app->passport ? "Check the document\nnumber and dates\n" :
                                "Check the 6 digits\non the front\n");
        }
        cat_technical(app, text);

    } else if(app->lds.result != DnieLdsOk) {
        furi_string_printf(
            text,
            "\e#Error reading DG1\n%s\nStep %u/3, SW %04X\n",
            dnie_lds_result_str(app->lds.result),
            app->lds.step,
            app->lds.sw);
        if(app->lds.result == DnieLdsErrorSm) {
            furi_string_cat_printf(text, "SM: %s\n", dnie_sm_result_str(app->lds.sm_result));
        }
        furi_string_cat_str(
            text,
            app->passport ? "(access OK: the data\nis correct)\n" :
                            "(PACE OK: the CAN\nis correct)\n");
        cat_technical(app, text);

    } else if(!app->mrz_ok) {
        furi_string_printf(text, "\e#DG1 read, MRZ\nnot recognized\n%u bytes\n", app->dg1_len);
        cat_auth(app, text);
        cat_photo(app, text);
        cat_technical(app, text);

    } else {
        const DnieMrz* m = &app->mrz;
        furi_string_printf(text, "\e#%s\n%s\n", m->given_names, m->surnames);
        if(app->demo) {
            furi_string_cat_str(text, "\n\e#DEMO\nSample data, not a\nreal reading.\n");
        }
        furi_string_cat_printf(text, "ID: %s\n", m->personal_number);
        furi_string_cat_printf(text, "Support: %s\n", m->doc_number);
        furi_string_cat_str(text, "Born: ");
        cat_date(text, m->birth, true, now.year);
        furi_string_cat_str(text, "\nExpires: ");
        cat_date(text, m->expiry, false, now.year);
        if(is_expired(m->expiry, &now)) furi_string_cat_str(text, " (EXPIRED)");
        furi_string_cat_printf(text, "\nSex: %c   Nat.: %s\n", m->sex, m->nationality);
        furi_string_cat_printf(text, "MRZ check: %s\n", dnie_mrz_all_checks_ok(m) ? "OK" : "FAIL");
        furi_string_cat_str(text, "Read via secure\nchannel (PACE+SM)\n");
        furi_string_cat_str(
            text,
            "\n\e#Card controls\nUp: save (full ID\nor just\nname/photo/birthday)\n<- -> photo style\nOK: these details\n");
        cat_auth(app, text);
        cat_photo(app, text);
        cat_technical(app, text);
    }

    widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(text));
    furi_string_free(text);

    view_dispatcher_switch_to_view(app->view_dispatcher, DnieViewWidget);
}

bool dnie_scene_result_on_event(void* context, SceneManagerEvent event) {
    DnieApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        // Desde el carnet: volver a el. Si no (lectura con error): al menu, no a "Hold the ID"
        if(scene_manager_has_previous_scene(app->scene_manager, DnieSceneCard)) {
            scene_manager_previous_scene(app->scene_manager);
        } else {
            scene_manager_search_and_switch_to_previous_scene(app->scene_manager, DnieSceneStart);
        }
        consumed = true;
    }

    return consumed;
}

void dnie_scene_result_on_exit(void* context) {
    DnieApp* app = context;
    widget_reset(app->widget);
}
