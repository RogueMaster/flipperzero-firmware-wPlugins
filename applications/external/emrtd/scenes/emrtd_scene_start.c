/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * The first screen.
 *
 * Reading a document is the only thing most people open this application for,
 * so it is the first item and it costs one press: with the details already
 * stored it starts the read, and without them it opens the screen where they
 * are entered. Nothing in between.
 */
#include "../emrtd_i.h"

typedef enum {
    EmrtdSceneStartIndexRead,
    EmrtdSceneStartIndexDocument,
    EmrtdSceneStartIndexOptions,
    EmrtdSceneStartIndexSaved,
    EmrtdSceneStartIndexDonate,
    EmrtdSceneStartIndexAbout,
} EmrtdSceneStartIndex;

/** True when the stored details are enough for the chosen access method. */
static bool emrtd_scene_start_credentials_ready(const Emrtd* app) {
    const EmrtdCredentials* credentials = &app->config.credentials;

    /* BAC has no other key than the MRZ; PACE will also take the CAN. */
    if(app->config.method != EmrtdAccessMethodBac && credentials->has_can &&
       credentials->can[0] != '\0') {
        return true;
    }

    return emrtd_credentials_valid_mrz(credentials);
}

static void emrtd_scene_start_submenu_callback(void* context, uint32_t index) {
    furi_assert(context);
    Emrtd* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void emrtd_scene_start_on_enter(void* context) {
    furi_assert(context);
    Emrtd* app = context;
    Submenu* submenu = app->submenu;

    submenu_set_header(submenu, "eMRTD Reader");
    submenu_add_item(
        submenu,
        "Read document",
        EmrtdSceneStartIndexRead,
        emrtd_scene_start_submenu_callback,
        app);
    submenu_add_item(
        submenu, "Document", EmrtdSceneStartIndexDocument, emrtd_scene_start_submenu_callback, app);
    submenu_add_item(
        submenu, "Options", EmrtdSceneStartIndexOptions, emrtd_scene_start_submenu_callback, app);
    submenu_add_item(
        submenu, "Saved reads", EmrtdSceneStartIndexSaved, emrtd_scene_start_submenu_callback, app);
    submenu_add_item(
        submenu, "Donate", EmrtdSceneStartIndexDonate, emrtd_scene_start_submenu_callback, app);
    submenu_add_item(
        submenu, "About", EmrtdSceneStartIndexAbout, emrtd_scene_start_submenu_callback, app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, EmrtdSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, EmrtdViewSubmenu);
}

bool emrtd_scene_start_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    Emrtd* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event <= EmrtdSceneStartIndexAbout) {
        /* The bound keeps a custom event posted by something other than this
         * menu - a worker report that arrived after the read was left, say -
         * out of the remembered cursor position. */
        scene_manager_set_scene_state(app->scene_manager, EmrtdSceneStart, event.event);
        consumed = true;

        switch(event.event) {
        case EmrtdSceneStartIndexRead:
            if(emrtd_scene_start_credentials_ready(app)) {
                scene_manager_next_scene(app->scene_manager, EmrtdSceneRead);
            } else {
                scene_manager_next_scene(app->scene_manager, EmrtdSceneDocument);
            }
            break;
        case EmrtdSceneStartIndexDocument:
            scene_manager_next_scene(app->scene_manager, EmrtdSceneDocument);
            break;
        case EmrtdSceneStartIndexOptions:
            scene_manager_next_scene(app->scene_manager, EmrtdSceneOptions);
            break;
        case EmrtdSceneStartIndexSaved:
            scene_manager_next_scene(app->scene_manager, EmrtdSceneSaved);
            break;
        case EmrtdSceneStartIndexDonate:
            scene_manager_next_scene(app->scene_manager, EmrtdSceneDonate);
            break;
        case EmrtdSceneStartIndexAbout:
            scene_manager_next_scene(app->scene_manager, EmrtdSceneAbout);
            break;
        default:
            consumed = false;
            break;
        }
    }

    return consumed;
}

void emrtd_scene_start_on_exit(void* context) {
    furi_assert(context);
    Emrtd* app = context;

    submenu_reset(app->submenu);
}
