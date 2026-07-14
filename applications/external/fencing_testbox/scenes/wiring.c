// Copyright (c) 2024 Aaron Janeiro Stone
// Licensed under the MIT license.
// See the LICENSE.txt file in the project root for details.

#include "../fencing_testbox.h"

void wiring_scene_on_enter(void* context) {
    FURI_LOG_D(TAG, "Wiring enter");
    FencingTestboxApp* app = (FencingTestboxApp*)context;
    furi_assert(app);

    popup_reset(app->wiring_popup);
    popup_set_context(app->wiring_popup, app);
    popup_set_icon(app->wiring_popup, AlignLeft, AlignTop, &I_wiring_71x60);
    popup_set_text(app->wiring_popup, "Press back to exit", 80, 60, AlignCenter, AlignCenter);
    view_dispatcher_switch_to_view(app->view_dispatcher, FencingTestboxSceneWiring);
}

void wiring_scene_on_exit(void* context) {
    FURI_LOG_D(TAG, "Wiring exit");
    FencingTestboxApp* app = (FencingTestboxApp*)context;
    popup_reset(app->wiring_popup);
}

bool wiring_scene_on_event(void* context, SceneManagerEvent index) {
    FURI_LOG_D(TAG, "Wiring event %d", (int)index.event);
    FencingTestboxApp* app = (FencingTestboxApp*)context;
    furi_assert(app);

    if(index.type == SceneManagerEventTypeBack) {
        scene_manager_next_scene(app->scene_manager, FencingTestboxSceneMainMenu);
        return true;
    }

    return false;
}
