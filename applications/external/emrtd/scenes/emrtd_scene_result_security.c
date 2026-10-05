/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * How the chip was opened, and how far its contents can be trusted.
 *
 * Passive authentication has two halves: every data group's hash must match
 * the one EF.SOD lists, and EF.SOD's own signature must chain to the country
 * that issued the document. This reader does the first half and says plainly
 * that it cannot do the second, rather than showing a verdict it has not
 * earned.
 */
#include "../emrtd_i.h"

#include "../crypto/emrtd_crypto.h"
#include "../protocol/emrtd_security_info.h"

static const char* emrtd_scene_result_security_hash_text(EmrtdHashState state) {
    switch(state) {
    case EmrtdHashStateMatch:
        return "verified";
    case EmrtdHashStateMismatch:
        return "DOES NOT MATCH";
    case EmrtdHashStateNotListed:
        return "not in EF.SOD";
    case EmrtdHashStateUnsupportedDigest:
        return "digest unsupported";
    default:
        return "not checked";
    }
}

static const char* emrtd_scene_result_security_mapping(EmrtdPaceMapping mapping) {
    switch(mapping) {
    case EmrtdPaceMappingGeneric:
        return "Generic mapping";
    case EmrtdPaceMappingIntegrated:
        return "Integrated mapping";
    case EmrtdPaceMappingChipAuth:
        return "Chip authentication mapping";
    default:
        return "Unknown mapping";
    }
}

void emrtd_scene_result_security_on_enter(void* context) {
    furi_assert(context);
    Emrtd* app = context;
    Widget* widget = app->widget;
    const EmrtdReadResult* result = &app->result;

    FuriString* body = app->text_box_store;
    furi_string_reset(body);

    furi_string_cat_printf(
        body,
        "\e#Access\n%s\n",
        result->access.summary[0] != '\0' ?
            result->access.summary :
            (result->authenticated ? result->access.protocol : "not established"));

    if(result->security_infos.has_pace) {
        const EmrtdPaceInfo* pace = &result->security_infos.pace;

        furi_string_cat_printf(
            body,
            "\e#PACE protocol\n%s\n",
            pace->oid_name != NULL ? pace->oid_name : "unnamed object identifier");
        furi_string_cat_printf(
            body, "\e#Mapping\n%s\n", emrtd_scene_result_security_mapping(pace->mapping));
        furi_string_cat_printf(
            body,
            "\e#Curve\n%s\n",
            pace->curve != NULL ? pace->curve->name : "not one this build knows");
        furi_string_cat_printf(body, "\e#Cipher\n%s\n", emrtd_cipher_name(pace->cipher));
    } else if(result->card_access_read) {
        furi_string_cat_str(body, "\e#PACE\nThe chip announced none.\n");
    }

    if(result->security_infos.has_chip_auth || result->security_infos.has_terminal_auth ||
       result->security_infos.has_active_auth) {
        furi_string_cat_str(body, "\e#Also announced\n");
        if(result->security_infos.has_chip_auth) {
            furi_string_cat_str(body, "Chip authentication\n");
        }
        if(result->security_infos.has_terminal_auth) {
            furi_string_cat_str(body, "Terminal authentication\n");
        }
        if(result->security_infos.has_active_auth) {
            furi_string_cat_str(body, "Active authentication\n");
        }
    }

    furi_string_cat_str(body, "\e#Passive authentication\n");

    if(!result->has_sod) {
        furi_string_cat_str(body, "EF.SOD was not read, so no\nhash could be checked.\n");
    } else {
        furi_string_cat_printf(
            body,
            "Digest %s, %u hashes listed\n",
            result->sod.digest_algorithm[0] != '\0' ? result->sod.digest_algorithm : "unknown",
            result->sod.hash_count);

        for(size_t id = 0; id < EmrtdFileCount; id++) {
            const EmrtdFileResult* file = &result->files[id];
            if(file->state != EmrtdFileStateRead) {
                continue;
            }

            const EmrtdFileInfo* info = emrtd_file_info((EmrtdFileId)id);
            if(info == NULL || info->dg_number < 0) {
                continue;
            }

            furi_string_cat_printf(
                body,
                "%s %s\n",
                info->name,
                emrtd_scene_result_security_hash_text(file->hash_state));
        }

        furi_string_cat_printf(
            body, "\n%u of %u matched\n", result->hashes_matched, result->hashes_checked);
    }

    /*
     * Stating the limit is not a disclaimer, it is the result: matching
     * hashes prove the files belong together, and only the signature would
     * prove they came from the state named on the cover.
     */
    furi_string_cat_str(
        body,
        "\n\e#Not checked here\nThe EF.SOD signature is not\n"
        "verified on the device: that\nneeds RSA, which this build\n"
        "of mbed TLS leaves out, and\nthe issuing country's\ncertificate.\n");

    widget_add_string_element(widget, 64, 1, AlignCenter, AlignTop, FontPrimary, "Security");
    widget_add_text_scroll_element(widget, 0, 13, 128, 51, furi_string_get_cstr(body));

    view_dispatcher_switch_to_view(app->view_dispatcher, EmrtdViewWidget);
}

bool emrtd_scene_result_security_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    return false;
}

void emrtd_scene_result_security_on_exit(void* context) {
    furi_assert(context);
    Emrtd* app = context;

    widget_reset(app->widget);
    furi_string_reset(app->text_box_store);
}
