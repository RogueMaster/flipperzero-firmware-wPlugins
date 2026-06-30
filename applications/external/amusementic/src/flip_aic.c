#include "flip_aic.h"
#include "helpers/cardio.h"
#include "scenes/flip_aic_scene.h"

#include <furi.h>

bool flip_aic_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    FlipAIC* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool flip_aic_navigation_event_callback(void* context) {
    furi_assert(context);
    FlipAIC* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static FlipAIC* flip_aic_alloc() {
    FlipAIC* app = malloc(sizeof(FlipAIC));

    app->gui = furi_record_open(RECORD_GUI);

    app->scene_manager = scene_manager_alloc(&flip_aic_scene_handlers, app);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, flip_aic_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, flip_aic_navigation_event_callback);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FlipAICViewSubmenu, submenu_get_view(app->submenu));

    app->loading = loading_cancellable_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FlipAICViewLoading, loading_cancellable_get_view(app->loading));

    app->dialog_ex = dialog_ex_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FlipAICViewDialogEx, dialog_ex_get_view(app->dialog_ex));

    app->file_browser_result_path = furi_string_alloc_set_str("/ext/nfc");
    app->file_browser = file_browser_alloc(app->file_browser_result_path);
    file_browser_configure(app->file_browser, ".nfc", "/ext/nfc", true, true, NULL, true);
    view_dispatcher_add_view(
        app->view_dispatcher, FlipAICViewFileBrowser, file_browser_get_view(app->file_browser));

    app->nfc = nfc_alloc();
    app->nfc_device = nfc_device_alloc();

    app->usb_mode_prev = furi_hal_usb_get_config();
    furi_hal_usb_unlock();
    furi_check(furi_hal_usb_set_config(&usb_cardio, NULL) == true);

    return app;
}

static void flip_aic_free(FlipAIC* app) {
    furi_assert(app);

    furi_hal_usb_set_config(app->usb_mode_prev, NULL);

    nfc_device_free(app->nfc_device);
    nfc_free(app->nfc);

    view_dispatcher_remove_view(app->view_dispatcher, FlipAICViewFileBrowser);
    file_browser_free(app->file_browser);
    furi_string_free(app->file_browser_result_path);

    view_dispatcher_remove_view(app->view_dispatcher, FlipAICViewDialogEx);
    dialog_ex_free(app->dialog_ex);

    view_dispatcher_remove_view(app->view_dispatcher, FlipAICViewLoading);
    loading_cancellable_free(app->loading);

    view_dispatcher_remove_view(app->view_dispatcher, FlipAICViewSubmenu);
    submenu_free(app->submenu);

    view_dispatcher_free(app->view_dispatcher);

    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t flip_aic_app(void* p) {
    UNUSED(p);

    FlipAIC* app = flip_aic_alloc();

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, FlipAICSceneMenu);
    view_dispatcher_run(app->view_dispatcher);

    flip_aic_free(app);

    return 0;
}
