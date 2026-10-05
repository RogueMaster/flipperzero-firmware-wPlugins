/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * Entering the document number.
 *
 * The number goes into the key derivation exactly as the machine readable
 * zone spells it, so only the MRZ alphabet of ICAO 9303-3 section 4.2.2 is
 * accepted: A to Z, 0 to 9 and the filler. The keyboard offers lower case,
 * which the MRZ does not have, so what is typed is raised to upper case
 * rather than rejected.
 */
#include "../emrtd_i.h"

#include "../access/emrtd_access.h"

static bool
    emrtd_scene_doc_number_input_validator(const char* text, FuriString* error, void* context) {
    UNUSED(context);

    if(text[0] == '\0') {
        furi_string_set_str(error, "The number\nis in the\nMRZ");
        return false;
    }

    for(const char* c = text; *c != '\0'; c++) {
        const bool allowed = (*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z') ||
                             (*c >= '0' && *c <= '9') || (*c == '<');
        if(!allowed) {
            furi_string_set_str(error, "Use A-Z,\n0-9 and <\nonly");
            return false;
        }
    }

    return true;
}

static void emrtd_scene_doc_number_input_callback(void* context) {
    furi_assert(context);
    Emrtd* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, EmrtdCustomEventTextInputDone);
}

void emrtd_scene_doc_number_input_on_enter(void* context) {
    furi_assert(context);
    Emrtd* app = context;
    TextInput* text_input = app->text_input;

    strlcpy(app->text_store, app->config.credentials.document_number, sizeof(app->text_store));

    text_input_set_header_text(text_input, "Document number");
    text_input_set_minimum_length(text_input, 1);
    text_input_set_validator(text_input, emrtd_scene_doc_number_input_validator, app);
    text_input_set_result_callback(
        text_input,
        emrtd_scene_doc_number_input_callback,
        app,
        app->text_store,
        /* The field the number ends up in is what limits the length, not the
         * scratch buffer it is typed into. */
        EMRTD_DOC_NUMBER_MAX + 1,
        false);

    view_dispatcher_switch_to_view(app->view_dispatcher, EmrtdViewTextInput);
}

bool emrtd_scene_doc_number_input_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    Emrtd* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event == EmrtdCustomEventTextInputDone) {
        for(char* c = app->text_store; *c != '\0'; c++) {
            if(*c >= 'a' && *c <= 'z') {
                *c = (char)(*c - 'a' + 'A');
            }
        }

        strlcpy(
            app->config.credentials.document_number,
            app->text_store,
            sizeof(app->config.credentials.document_number));
        emrtd_settings_save(app);

        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void emrtd_scene_doc_number_input_on_exit(void* context) {
    furi_assert(context);
    Emrtd* app = context;

    text_input_reset(app->text_input);
    emrtd_text_store_clear(app);
}
