/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * Entering the Card Access Number.
 *
 * The CAN is the six digit number printed on an identity card, and PACE will
 * take it in place of the machine readable zone (ICAO 9303-11, 4.4.2). It is
 * kept as a string rather than a number because its leading zeros are part of
 * the password.
 */
#include "../emrtd_i.h"

#include "../access/emrtd_access.h"

#define EMRTD_CAN_MAX_VALUE (999999)

static void emrtd_scene_can_input_callback(void* context, int32_t number) {
    furi_assert(context);
    Emrtd* app = context;

    if(number < 0) {
        number = 0;
    }
    snprintf(app->text_store, sizeof(app->text_store), "%06lu", (unsigned long)number);
    view_dispatcher_send_custom_event(app->view_dispatcher, EmrtdCustomEventNumberInputDone);
}

void emrtd_scene_can_input_on_enter(void* context) {
    furi_assert(context);
    Emrtd* app = context;
    NumberInput* number_input = app->number_input;
    const EmrtdCredentials* credentials = &app->config.credentials;

    int32_t current = 0;
    if(credentials->has_can && credentials->can[0] != '\0') {
        current = (int32_t)strtol(credentials->can, NULL, 10);
        if(current < 0 || current > EMRTD_CAN_MAX_VALUE) {
            current = 0;
        }
    }

    number_input_set_header_text(number_input, "Card Access Number");
    number_input_set_result_callback(
        number_input, emrtd_scene_can_input_callback, app, current, 0, EMRTD_CAN_MAX_VALUE);

    view_dispatcher_switch_to_view(app->view_dispatcher, EmrtdViewNumberInput);
}

bool emrtd_scene_can_input_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    Emrtd* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom &&
       event.event == EmrtdCustomEventNumberInputDone) {
        EmrtdCredentials* credentials = &app->config.credentials;

        strlcpy(credentials->can, app->text_store, sizeof(credentials->can));
        credentials->has_can = credentials->can[0] != '\0';
        emrtd_settings_save(app);

        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void emrtd_scene_can_input_on_exit(void* context) {
    furi_assert(context);
    Emrtd* app = context;

    emrtd_text_store_clear(app);
}
