#include "app_state.h"
#include "longwave_clock_app.h"

static LWCScene protocol_scene[] = {LWCDCF77Scene, LWCMSFScene};

App* app_alloc() {
    App* app = malloc(sizeof(App));
    app->scene_manager = scene_manager_alloc(&lwc_scene_manager_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, lwc_custom_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, lwc_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, lwc_tick_event_callback, furi_ms_to_ticks(100));

    app->main_menu = submenu_alloc();
    app->sub_menu = variable_item_list_alloc();
    app->about = text_box_alloc();
    app->info_text = text_box_alloc();
    app->dcf77_view = lwc_dcf77_scene_alloc();
    app->msf_view = lwc_msf_scene_alloc();
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    view_dispatcher_add_view(
        app->view_dispatcher, LWCMainMenuView, submenu_get_view(app->main_menu));
    view_dispatcher_add_view(
        app->view_dispatcher, LWCSubMenuView, variable_item_list_get_view(app->sub_menu));
    view_dispatcher_add_view(app->view_dispatcher, LWCAboutView, text_box_get_view(app->about));
    view_dispatcher_add_view(app->view_dispatcher, LWCInfoView, text_box_get_view(app->info_text));
    view_dispatcher_add_view(app->view_dispatcher, LWCDCF77View, app->dcf77_view);
    view_dispatcher_add_view(app->view_dispatcher, LWCMSFView, app->msf_view);
    return app;
}

AppState* app_state_alloc() {
    AppState* state = malloc(sizeof(AppState));

    state->storage = furi_record_open(RECORD_STORAGE);

    for(uint8_t i = 0; i < __lwc_number_of_protocols; i++) {
        state->proto_configs[i] = malloc(sizeof(ProtoConfig));
        File* file = storage_file_alloc(state->storage);
        bool read = false;
        if(storage_file_open(
               file, get_protocol_config_filename((LWCType)i), FSAM_READ, FSOM_OPEN_EXISTING)) {
            read = storage_file_read(file, state->proto_configs[i], sizeof(ProtoConfig)) ==
                   sizeof(ProtoConfig);
        }

        if(!read) {
            state->proto_configs[i]->run_mode = Demo;
            state->proto_configs[i]->data_pin = GPIOPinC0;
            state->proto_configs[i]->data_mode = Regular;
        }
        storage_file_close(file);
        storage_file_free(file);
    }

    state->display_on = false;
    state->gpio = NULL;

    return state;
}

void store_proto_config(AppState* app_state) {
    File* file = storage_file_alloc(app_state->storage);
    if(storage_file_open(
           file,
           get_protocol_config_filename(app_state->lwc_type),
           FSAM_WRITE,
           FSOM_CREATE_ALWAYS)) {
        if(!storage_file_write(
               file, app_state->proto_configs[app_state->lwc_type], sizeof(ProtoConfig))) {
            FURI_LOG_E(TAG, "Failed to write to open proto config file.");
        }
    } else {
        FURI_LOG_E(TAG, "Failed to open proto config file for writing.");
    }

    storage_file_close(file);
    storage_file_free(file);
}

void app_init_lwc(App* app, LWCType type) {
    app->state->lwc_type = type;
}

void app_quit(App* app) {
    scene_manager_stop(app->scene_manager);
}

void app_free(App* app) {
    furi_assert(app);

    FURI_LOG_D(TAG, "Removing the view dispatcher views.");
    view_dispatcher_remove_view(app->view_dispatcher, LWCMainMenuView);
    view_dispatcher_remove_view(app->view_dispatcher, LWCSubMenuView);
    view_dispatcher_remove_view(app->view_dispatcher, LWCAboutView);
    view_dispatcher_remove_view(app->view_dispatcher, LWCInfoView);
    view_dispatcher_remove_view(app->view_dispatcher, LWCDCF77View);
    view_dispatcher_remove_view(app->view_dispatcher, LWCMSFView);

    FURI_LOG_D(TAG, "Removing the scene manager and view dispatcher.");
    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    FURI_LOG_D(TAG, "Removing the single scenes...");
    submenu_free(app->main_menu);
    variable_item_list_free(app->sub_menu);
    text_box_free(app->about);
    text_box_free(app->info_text);
    FURI_LOG_D(TAG, "Removing the DCF77 scene...");
    lwc_dcf77_scene_free(app->dcf77_view);
    FURI_LOG_D(TAG, "Removing the MSF scene...");
    lwc_msf_scene_free(app->msf_view);

    FURI_LOG_D(TAG, "Desubscribing from notification...");
    furi_record_close(RECORD_NOTIFICATION);

    FURI_LOG_D(TAG, "Removing the protocol configs.");
    for(uint8_t i = 0; i < __lwc_number_of_protocols; i++) {
        free(app->state->proto_configs[i]);
    }
    furi_record_close(RECORD_STORAGE);

    FURI_LOG_D(TAG, "Removing the AppState*.");
    free(app->state);

    FURI_LOG_D(TAG, "Freeing the App*...");
    free(app);
}

ProtoConfig* lwc_get_protocol_config(AppState* app_state) {
    return app_state->proto_configs[app_state->lwc_type];
}

LWCScene lwc_get_start_scene_for_protocol(AppState* app_state) {
    return protocol_scene[app_state->lwc_type];
}
