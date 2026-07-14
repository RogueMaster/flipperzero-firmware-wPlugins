#include "application.h"
#include "tonuino_nfc.h"
#include "scenes/scenes.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static bool tonuino_back_event_callback(void* context);
static void tonuino_tick_event_callback(void* context);

void tonuino_build_card_data(TonuinoApp* app) {
    app->card_data.box_id[0] = TONUINO_BOX_ID_0;
    app->card_data.box_id[1] = TONUINO_BOX_ID_1;
    app->card_data.box_id[2] = TONUINO_BOX_ID_2;
    app->card_data.box_id[3] = TONUINO_BOX_ID_3;
    app->card_data.version = TONUINO_VERSION;
    app->card_data.reserved[0] = 0x00;
    app->card_data.reserved[1] = 0x00;
    app->card_data.reserved[2] = 0x00;
    app->card_data.reserved[3] = 0x00;
    app->card_data.reserved[4] = 0x00;
    app->card_data.reserved[5] = 0x00;
    app->card_data.reserved[6] = 0x00;
}

static bool tonuino_back_event_callback(void* context) {
    TonuinoApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void tonuino_tick_event_callback(void* context) {
    TonuinoApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

TonuinoApp* tonuino_app_alloc() {
    TonuinoApp* app = malloc(sizeof(TonuinoApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    // Initialize ViewDispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, tonuino_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, tonuino_tick_event_callback, 100);

    // Initialize SceneManager
    app->scene_manager = scene_manager_alloc(&tonuino_scene_handlers, app);

    // Allocate views
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TonuinoViewSubmenu, submenu_get_view(app->submenu));

    app->number_input = number_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TonuinoViewNumberInput, number_input_get_view(app->number_input));

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TonuinoViewWidget, widget_get_view(app->widget));

    app->text_box = text_box_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TonuinoViewTextBox, text_box_get_view(app->text_box));

    // Initialize card data
    tonuino_build_card_data(app);
    app->card_data.folder = 1;
    app->card_data.mode = ModeAlbum;
    app->card_data.special1 = 0;
    app->card_data.special2 = 0;

    return app;
}

void tonuino_app_free(TonuinoApp* app) {
    // Remove views
    view_dispatcher_remove_view(app->view_dispatcher, TonuinoViewTextBox);
    view_dispatcher_remove_view(app->view_dispatcher, TonuinoViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, TonuinoViewNumberInput);
    view_dispatcher_remove_view(app->view_dispatcher, TonuinoViewSubmenu);

    // Free views
    text_box_free(app->text_box);
    widget_free(app->widget);
    number_input_free(app->number_input);
    submenu_free(app->submenu);

    // Free scene manager and view dispatcher
    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    // Close records
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t tonuino_app(void* p) {
    UNUSED(p);

    TonuinoApp* app = tonuino_app_alloc();

    // Start with main menu scene
    scene_manager_next_scene(app->scene_manager, TonuinoSceneMainMenu);

    view_dispatcher_run(app->view_dispatcher);

    tonuino_app_free(app);
    return 0;
}
