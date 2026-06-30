// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2024 KBEmbedded

#include <furi.h>

#include <gui/scene_manager.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <storage/storage.h>

#include <src/include/fgp_app.h>
#include <src/scenes/include/fgp_scene.h>
#include <src/views/include/receive_view.h>

#include <protocols/printer/include/printer_proto.h>
#include <gblink/include/gblink_pinconf.h>

bool fgp_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    struct fgp_app* fgp = context;

    return scene_manager_handle_custom_event(fgp->scene_manager, event);
}

bool fgp_back_event_callback(void* context) {
    furi_assert(context);
    struct fgp_app* fgp = context;
    return scene_manager_handle_back_event(fgp->scene_manager);
}

static struct fgp_app* fgp_alloc(void) {
    struct fgp_app* fgp = NULL;
    Storage* storage;
    FuriString* fs_path;

    fgp = malloc(sizeof(struct fgp_app));

    storage = furi_record_open(RECORD_STORAGE);
    fs_path = furi_string_alloc_set(APP_DATA_PATH(""));
    storage_common_resolve_path_and_ensure_app_directory(storage, fs_path);
    furi_string_free(fs_path);
    furi_record_close(RECORD_STORAGE);

    // View Dispatcher
    fgp->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(fgp->view_dispatcher, fgp);
    view_dispatcher_set_custom_event_callback(fgp->view_dispatcher, fgp_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(fgp->view_dispatcher, fgp_back_event_callback);
    view_dispatcher_attach_to_gui(
        fgp->view_dispatcher, (Gui*)furi_record_open(RECORD_GUI), ViewDispatcherTypeFullscreen);

    // Submenu
    fgp->submenu = submenu_alloc();
    view_dispatcher_add_view(fgp->view_dispatcher, fgpViewSubmenu, submenu_get_view(fgp->submenu));

    // Variable Item List
    fgp->variable_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        fgp->view_dispatcher,
        fgpViewVariableItemList,
        variable_item_list_get_view(fgp->variable_item_list));

    // Receive
    fgp->receive_view = fgp_receive_view_alloc(fgp);
    view_dispatcher_add_view(
        fgp->view_dispatcher, fgpViewReceive, fgp_receive_view_get_view(fgp->receive_view));

    // Scene manager
    fgp->scene_manager = scene_manager_alloc(&fgp_scene_handlers, fgp);
    scene_manager_next_scene(fgp->scene_manager, fgpSceneMenu);

    // Printer handling
    fgp->printer_handle = printer_alloc();

    // Load pinconf if saved
    gblink_pinconf_load(printer_gblink_handle_get(fgp->printer_handle));

    return fgp;
}

static void fgp_free(struct fgp_app* fgp) {
    // Printer handling
    printer_free(fgp->printer_handle);

    // Scene manager
    scene_manager_free(fgp->scene_manager);

    // Receive View
    view_dispatcher_remove_view(fgp->view_dispatcher, fgpViewReceive);
    fgp_receive_view_free(fgp->receive_view);

    // Variable Item List
    view_dispatcher_remove_view(fgp->view_dispatcher, fgpViewVariableItemList);
    variable_item_list_free(fgp->variable_item_list);

    // Submenu
    view_dispatcher_remove_view(fgp->view_dispatcher, fgpViewSubmenu);
    submenu_free(fgp->submenu);

    // View dispatcher
    view_dispatcher_free(fgp->view_dispatcher);

    free(fgp);
}

int32_t fgp_app(void* p) {
    UNUSED(p);
    struct fgp_app* fgp = NULL;

    fgp = fgp_alloc();

    view_dispatcher_run(fgp->view_dispatcher);

    fgp_free(fgp);

    return 0;
}
