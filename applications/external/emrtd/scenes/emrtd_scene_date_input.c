/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * Entering one of the two dates.
 *
 * Both dates share the view; which one is being edited is in app->date_field,
 * and it decides the heading and the direction the two digit year is resolved
 * in.
 */
#include "../emrtd_i.h"

#include "../access/emrtd_access.h"

static void emrtd_scene_date_input_callback(void* context, const char* yymmdd) {
    furi_assert(context);
    furi_assert(yymmdd);
    Emrtd* app = context;

    /* The view runs on the GUI thread, but the scene manager may only be
     * driven from an event handler, so the value travels through the shared
     * scratch and the scene picks it up. */
    strlcpy(app->text_store, yymmdd, sizeof(app->text_store));
    view_dispatcher_send_custom_event(app->view_dispatcher, EmrtdCustomEventDateInputDone);
}

void emrtd_scene_date_input_on_enter(void* context) {
    furi_assert(context);
    Emrtd* app = context;
    const bool expiry = app->date_field == EmrtdDateFieldExpiry;
    const EmrtdCredentials* credentials = &app->config.credentials;

    emrtd_date_input_set_header(app->date_input, expiry ? "Date of expiry" : "Date of birth");
    emrtd_date_input_set_future(app->date_input, expiry);
    emrtd_date_input_set_value(
        app->date_input, expiry ? credentials->date_of_expiry : credentials->date_of_birth);
    emrtd_date_input_set_callback(app->date_input, emrtd_scene_date_input_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, EmrtdViewDateInput);
}

bool emrtd_scene_date_input_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    Emrtd* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event == EmrtdCustomEventDateInputDone) {
        EmrtdCredentials* credentials = &app->config.credentials;

        if(app->date_field == EmrtdDateFieldExpiry) {
            strlcpy(
                credentials->date_of_expiry, app->text_store, sizeof(credentials->date_of_expiry));
        } else {
            strlcpy(
                credentials->date_of_birth, app->text_store, sizeof(credentials->date_of_birth));
        }
        emrtd_settings_save(app);

        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void emrtd_scene_date_input_on_exit(void* context) {
    furi_assert(context);
    Emrtd* app = context;

    emrtd_date_input_set_callback(app->date_input, NULL, NULL);
    emrtd_text_store_clear(app);
}
