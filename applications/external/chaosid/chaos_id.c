// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Cafe com Solda / Cristiano Santana

#include "chaos_id_app.h"
#include "scenes/chaos_id_scene.h"

static bool chaos_id_back_event_callback(void* context) {
    furi_assert(context);
    ChaosIdApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static bool chaos_id_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    ChaosIdApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static ChaosIdApp* chaos_id_app_alloc(void) {
    ChaosIdApp* app = malloc(sizeof(ChaosIdApp));
    memset(app, 0, sizeof(ChaosIdApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&chaos_id_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, chaos_id_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, chaos_id_back_event_callback);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, ChaosIdViewMenu, submenu_get_view(app->submenu));

    app->popup = popup_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, ChaosIdViewScanning, popup_get_view(app->popup));

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, ChaosIdViewWidget, widget_get_view(app->widget));

    // App-lifetime allocations: aloca UMA VEZ, reutiliza em cada scan.
    // Evita BusFault por contention de periferico no ciclo alloc/free repetido
    // do LFRFIDWorker e Nfc.
    app->lf_dict = protocol_dict_alloc(lfrfid_protocols, LFRFIDProtocolMax);
    app->lf_worker = lfrfid_worker_alloc(app->lf_dict);
    app->nfc = nfc_alloc();

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    return app;
}

static void chaos_id_app_free(ChaosIdApp* app) {
    furi_assert(app);

    view_dispatcher_remove_view(app->view_dispatcher, ChaosIdViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, ChaosIdViewScanning);
    view_dispatcher_remove_view(app->view_dispatcher, ChaosIdViewWidget);

    submenu_free(app->submenu);
    popup_free(app->popup);
    widget_free(app->widget);

    // App-lifetime cleanup
    if(app->lf_worker) lfrfid_worker_free(app->lf_worker);
    if(app->lf_dict) protocol_dict_free(app->lf_dict);
    if(app->nfc) nfc_free(app->nfc);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t chaos_id_app(void* p) {
    UNUSED(p);
    ChaosIdApp* app = chaos_id_app_alloc();
    scene_manager_next_scene(app->scene_manager, ChaosIdSceneSplash);
    view_dispatcher_run(app->view_dispatcher);
    chaos_id_app_free(app);
    return 0;
}
