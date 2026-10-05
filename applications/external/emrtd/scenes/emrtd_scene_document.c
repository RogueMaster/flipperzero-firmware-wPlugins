/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * The details read off the data page.
 *
 * All four values are shown with what is stored in them, because the usual
 * failure of a document reader is not a protocol error: it is one wrong
 * character in a value the user cannot see without opening four screens.
 */
#include "../emrtd_i.h"

#include "../protocol/emrtd_mrz.h"

typedef enum {
    EmrtdSceneDocumentIndexNumber,
    EmrtdSceneDocumentIndexBirth,
    EmrtdSceneDocumentIndexExpiry,
    EmrtdSceneDocumentIndexCan,
    EmrtdSceneDocumentIndexCanClear,
    EmrtdSceneDocumentIndexForget,
} EmrtdSceneDocumentIndex;

static void emrtd_scene_document_submenu_callback(void* context, uint32_t index) {
    furi_assert(context);
    Emrtd* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

/** "Born: 12.08.1974", or "Born: not set" while the field is empty. */
static void emrtd_scene_document_date_label(
    char* out,
    size_t out_size,
    const char* prefix,
    const char* yymmdd,
    bool is_expiry) {
    if(yymmdd[0] == '\0') {
        snprintf(out, out_size, "%s: not set", prefix);
        return;
    }

    char pretty[16];
    emrtd_mrz_format_date(yymmdd, is_expiry, pretty, sizeof(pretty));
    snprintf(out, out_size, "%s: %s", prefix, pretty);
}

void emrtd_scene_document_on_enter(void* context) {
    furi_assert(context);
    Emrtd* app = context;
    Submenu* submenu = app->submenu;
    const EmrtdCredentials* credentials = &app->config.credentials;

    /* The submenu copies each label, so one buffer serves all of them. */
    char label[48];

    submenu_set_header(submenu, "Document details");

    snprintf(
        label,
        sizeof(label),
        "Number: %s",
        credentials->document_number[0] != '\0' ? credentials->document_number : "not set");
    submenu_add_item(
        submenu, label, EmrtdSceneDocumentIndexNumber, emrtd_scene_document_submenu_callback, app);

    emrtd_scene_document_date_label(
        label, sizeof(label), "Born", credentials->date_of_birth, false);
    submenu_add_item(
        submenu, label, EmrtdSceneDocumentIndexBirth, emrtd_scene_document_submenu_callback, app);

    emrtd_scene_document_date_label(
        label, sizeof(label), "Expires", credentials->date_of_expiry, true);
    submenu_add_item(
        submenu, label, EmrtdSceneDocumentIndexExpiry, emrtd_scene_document_submenu_callback, app);

    /*
     * A stored Card Access Number takes the place of the machine readable
     * zone, so the label says so: otherwise a number typed once to see what
     * the screen looked like goes on quietly failing every later read while
     * the three values above it are plainly correct.
     */
    const bool has_can = credentials->has_can && credentials->can[0] != '\0';
    if(has_can) {
        snprintf(label, sizeof(label), "CAN: %s (used instead of MRZ)", credentials->can);
    } else {
        snprintf(label, sizeof(label), "CAN: not set");
    }
    submenu_add_item(
        submenu, label, EmrtdSceneDocumentIndexCan, emrtd_scene_document_submenu_callback, app);

    if(has_can) {
        submenu_add_item(
            submenu,
            "Clear CAN, use the MRZ",
            EmrtdSceneDocumentIndexCanClear,
            emrtd_scene_document_submenu_callback,
            app);
    }

    submenu_add_item(
        submenu,
        "Forget stored data",
        EmrtdSceneDocumentIndexForget,
        emrtd_scene_document_submenu_callback,
        app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, EmrtdSceneDocument));

    view_dispatcher_switch_to_view(app->view_dispatcher, EmrtdViewSubmenu);
}

bool emrtd_scene_document_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    Emrtd* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event <= EmrtdSceneDocumentIndexForget) {
        scene_manager_set_scene_state(app->scene_manager, EmrtdSceneDocument, event.event);
        consumed = true;

        switch(event.event) {
        case EmrtdSceneDocumentIndexNumber:
            scene_manager_next_scene(app->scene_manager, EmrtdSceneDocNumberInput);
            break;
        case EmrtdSceneDocumentIndexBirth:
            app->date_field = EmrtdDateFieldBirth;
            scene_manager_next_scene(app->scene_manager, EmrtdSceneDateInput);
            break;
        case EmrtdSceneDocumentIndexExpiry:
            app->date_field = EmrtdDateFieldExpiry;
            scene_manager_next_scene(app->scene_manager, EmrtdSceneDateInput);
            break;
        case EmrtdSceneDocumentIndexCan:
            scene_manager_next_scene(app->scene_manager, EmrtdSceneCanInput);
            break;
        case EmrtdSceneDocumentIndexCanClear:
            memset(app->config.credentials.can, 0, sizeof(app->config.credentials.can));
            app->config.credentials.has_can = false;
            emrtd_settings_save(app);
            /* Redraw so the item disappears along with the value it cleared. */
            scene_manager_set_scene_state(
                app->scene_manager, EmrtdSceneDocument, EmrtdSceneDocumentIndexCan);
            submenu_reset(app->submenu);
            emrtd_scene_document_on_enter(app);
            break;
        case EmrtdSceneDocumentIndexForget:
            scene_manager_next_scene(app->scene_manager, EmrtdSceneForgetConfirm);
            break;
        default:
            consumed = false;
            break;
        }
    }

    return consumed;
}

void emrtd_scene_document_on_exit(void* context) {
    furi_assert(context);
    Emrtd* app = context;

    submenu_reset(app->submenu);
}
