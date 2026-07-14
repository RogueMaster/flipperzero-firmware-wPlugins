// Copyright (c) 2024 Aaron Janeiro Stone
// Licensed under the MIT license.
// See the LICENSE.txt file in the project root for details.

#include "fencing_testbox.h"

bool custom_event_callback_passthrough(void* context, uint32_t index) {
    FURI_LOG_D(TAG, "Handling custom event %d", (int)index);
    FencingTestboxApp* app = (FencingTestboxApp*)context;
    furi_assert(app);

    return scene_manager_handle_custom_event(app->scene_manager, index);
}

bool navigation_event_callback_passthrough(void* context) {
    FURI_LOG_D(TAG, "Handling navigation event");
    FencingTestboxApp* app = (FencingTestboxApp*)context;
    furi_assert(app);

    return scene_manager_handle_back_event(app->scene_manager);
}

// Mappings
void (*scene_on_enter[])(void*) = {
    main_menu_scene_on_enter,
    wiring_scene_on_enter,
    testbox_scene_on_enter,
};

void (*scene_on_exit[])(void*) = {
    main_menu_scene_on_exit,
    wiring_scene_on_exit,
    testbox_scene_on_exit,
};

bool (*scene_event_handlers[])(void*, SceneManagerEvent) = {
    main_menu_scene_on_event,
    wiring_scene_on_event,
    testbox_scene_on_event,
};

const SceneManagerHandlers scene_manager_handlers = {
    .scene_num = FenceTestboxSceneUNUSED,
    .on_enter_handlers = scene_on_enter,
    .on_exit_handlers = scene_on_exit,
    .on_event_handlers = scene_event_handlers,
};

FencingTestboxApp* init_fencing_testbox() {
    FencingTestboxApp* app = (FencingTestboxApp*)malloc(sizeof(FencingTestboxApp));
    furi_assert(app);

    app->scene_manager = scene_manager_alloc(&scene_manager_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();

    // Allocs
    app->submenu = submenu_alloc();
    app->wiring_popup = popup_alloc();
    app->widget = widget_alloc();

    // Initialized only at testbox scene
    app->gpio_initialized = false;
    app->light_state.green_emitting = false;
    app->light_state.red_emitting = false;

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, custom_event_callback_passthrough);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, navigation_event_callback_passthrough);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, testbox_scene_on_tick, TICK_WORK);
    view_dispatcher_add_view(
        app->view_dispatcher, FencingTestboxSceneMainMenu, submenu_get_view(app->submenu));
    view_dispatcher_add_view(
        app->view_dispatcher, FencingTestboxSceneWiring, popup_get_view(app->wiring_popup));
    view_dispatcher_add_view(
        app->view_dispatcher, FencingTestboxSceneTestbox, widget_get_view(app->widget));

    return app;
}

void free_fencing_testbox(FencingTestboxApp* app) {
    furi_assert(app);
    scene_manager_free(app->scene_manager);
    view_dispatcher_remove_view(app->view_dispatcher, FencingTestboxSceneMainMenu);
    view_dispatcher_remove_view(app->view_dispatcher, FencingTestboxSceneWiring);
    view_dispatcher_remove_view(app->view_dispatcher, FencingTestboxSceneTestbox);
    view_dispatcher_free(app->view_dispatcher);
    submenu_free(app->submenu);
    popup_free(app->wiring_popup);
    free(app);
}

void fencing_testbox_main() {
    FencingTestboxApp* app = init_fencing_testbox();
    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, FencingTestboxSceneMainMenu);
    view_dispatcher_run(app->view_dispatcher);

    // exit
    furi_record_close(RECORD_GUI);
    free_fencing_testbox(app);
}
