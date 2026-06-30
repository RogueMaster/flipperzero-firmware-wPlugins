#include "ami_tool_i.h"
#include <stdio.h>
#include <string.h>

static void ami_tool_reset_retail_key(AmiToolApp* app) {
    if(!app) return;
    memset(app->retail_key, 0, sizeof(app->retail_key));
    app->retail_key_size = 0;
    app->retail_key_loaded = false;
}

/* Forward declarations of callbacks */
static bool ami_tool_custom_event_callback(void* context, uint32_t event);
static bool ami_tool_back_event_callback(void* context);
static void ami_tool_tick_event_callback(void* context);

/* Allocate and initialize app */
AmiToolApp* ami_tool_alloc(void) {
    AmiToolApp* app = malloc(sizeof(AmiToolApp));

    /* Scene manager */
    app->scene_manager = scene_manager_alloc(&ami_tool_scene_handlers, app);

    /* View dispatcher */
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, ami_tool_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, ami_tool_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, ami_tool_tick_event_callback, 100);

    /* GUI record and attach */
    app->gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(
        app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    /* Submenu (main menu view) */
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, AmiToolViewMenu, submenu_get_view(app->submenu));
    app->generate_page_offset = 0;
    app->generate_selected_index = 0;
    app->generate_list_source = AmiToolGenerateListSourceGame;

    /* TextBox (simple placeholder screens) */
    app->text_box = text_box_alloc();
    text_box_set_focus(app->text_box, TextBoxFocusStart);
    app->text_box_store = furi_string_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, AmiToolViewTextBox, text_box_get_view(app->text_box));
    app->info_widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, AmiToolViewInfo, widget_get_view(app->info_widget));
    app->main_menu_error_visible = false;
    app->info_actions_visible = false;
    app->info_action_message_visible = false;
    app->info_emulation_active = false;
    app->usage_info_visible = false;
    app->info_last_from_read = false;
    app->info_last_has_id = false;
    memset(app->info_last_id, 0, sizeof(app->info_last_id));
    app->write_thread = NULL;
    app->write_in_progress = false;
    app->write_cancel_requested = false;
    app->write_waiting_for_tag = false;
    memset(app->write_result_message, 0, sizeof(app->write_result_message));

    /* Storage (for assets) */
    app->storage = furi_record_open(RECORD_STORAGE);
    ami_tool_reset_retail_key(app);
    app->bt = furi_record_open(RECORD_BT);
    app->bt_serial_profile = NULL;
    app->bt_connected = false;
    app->bt_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    snprintf(app->bt_display_text, sizeof(app->bt_display_text), "Awaiting connections");
    app->bt_pending_generate = false;
    memset(app->bt_pending_generate_id, 0, sizeof(app->bt_pending_generate_id));

    /* NFC resources */
    app->nfc = nfc_alloc();
    app->read_thread = NULL;
    app->read_scene_active = false;
    memset(&app->read_result, 0, sizeof(app->read_result));
    app->read_result.type = AmiToolReadResultNone;
    app->read_result.error = MfUltralightErrorNone;
    app->tag_data = mf_ultralight_alloc();
    app->tag_data_valid = false;
    memset(&app->tag_password, 0, sizeof(app->tag_password));
    app->tag_password_valid = false;
    memset(app->tag_pack, 0, sizeof(app->tag_pack));
    app->tag_pack_valid = false;
    memset(app->last_uid, 0, sizeof(app->last_uid));
    app->last_uid_len = 0;
    app->last_uid_valid = false;
    app->emulation_listener = NULL;
    app->usage_page_index = 0;
    app->usage_page_count = 0;
    app->usage_entries_capacity = 0;
    app->usage_entries = NULL;
    app->usage_raw_data = NULL;
    app->usage_nav_pending = false;

    /* Generate scene state */
    app->generate_state = AmiToolGenerateStateRootMenu;
    app->generate_return_state = AmiToolGenerateStateRootMenu;
    app->generate_platform = AmiToolGeneratePlatform3DS;
    app->generate_game_count = 0;
    app->generate_amiibo_count = 0;
    app->generate_page_entry_count = 0;
    for(size_t i = 0; i < AMI_TOOL_GENERATE_MAX_AMIIBO_PAGE_ITEMS; i++) {
        app->generate_page_names[i] = furi_string_alloc();
        app->generate_page_ids[i] = furi_string_alloc();
    }
    app->generate_selected_game = furi_string_alloc();
    app->saved_page_offset = 0;
    app->saved_page_entry_count = 0;
    app->saved_has_next_page = false;
    app->saved_info_visible = false;
    for(size_t i = 0; i < AMI_TOOL_SAVED_MAX_PAGE_ITEMS; i++) {
        app->saved_page_display[i] = furi_string_alloc();
        app->saved_page_paths[i] = furi_string_alloc();
        app->saved_page_ids[i] = furi_string_alloc();
    }

    app->amiibo_link_active = false;
    app->amiibo_link_waiting_for_completion = false;
    app->amiibo_link_initial_hash = 0;
    app->amiibo_link_last_hash = 0;
    app->amiibo_link_last_change_tick = 0;
    app->amiibo_link_completion_pending = false;
    app->amiibo_link_current_auth0 = 0xFF;
    app->amiibo_link_pending_auth0 = 0xFF;
    app->amiibo_link_auth0_override_active = false;
    app->amiibo_link_access_snapshot_valid = false;
    memset(app->amiibo_link_access_snapshot, 0, sizeof(app->amiibo_link_access_snapshot));
    app->amiibo_link_completion_marker_valid = false;
    memset(app->amiibo_link_completion_marker, 0, sizeof(app->amiibo_link_completion_marker));

    return app;
}

/* Free everything */
void ami_tool_free(AmiToolApp* app) {
    furi_assert(app);
    ami_tool_info_stop_emulation(app);
    ami_tool_info_abort_write(app);
    if(app->usage_entries) {
        free(app->usage_entries);
        app->usage_entries = NULL;
    }
    if(app->usage_raw_data) {
        free(app->usage_raw_data);
        app->usage_raw_data = NULL;
    }
    app->usage_entries_capacity = 0;
    app->usage_page_count = 0;
    app->usage_page_index = 0;
    app->usage_nav_pending = false;

    /* Remove views from dispatcher */
    view_dispatcher_remove_view(app->view_dispatcher, AmiToolViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, AmiToolViewTextBox);
    view_dispatcher_remove_view(app->view_dispatcher, AmiToolViewInfo);

    /* Free modules */
    submenu_free(app->submenu);
    text_box_free(app->text_box);
    furi_string_free(app->text_box_store);
    widget_free(app->info_widget);

    /* NFC resources */
    if(app->read_thread) {
        furi_thread_join(app->read_thread);
        furi_thread_free(app->read_thread);
        app->read_thread = NULL;
    }
    if(app->nfc) {
        nfc_free(app->nfc);
        app->nfc = NULL;
    }
    if(app->tag_data) {
        mf_ultralight_free(app->tag_data);
        app->tag_data = NULL;
    }

    /* View dispatcher & scene manager */
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    /* Generate scene dynamic data */
    ami_tool_generate_clear_amiibo_cache(app);
    if(app->generate_selected_game) {
        furi_string_free(app->generate_selected_game);
        app->generate_selected_game = NULL;
    }
    for(size_t i = 0; i < AMI_TOOL_GENERATE_MAX_AMIIBO_PAGE_ITEMS; i++) {
        if(app->generate_page_names[i]) {
            furi_string_free(app->generate_page_names[i]);
            app->generate_page_names[i] = NULL;
        }
        if(app->generate_page_ids[i]) {
            furi_string_free(app->generate_page_ids[i]);
            app->generate_page_ids[i] = NULL;
        }
    }
    for(size_t i = 0; i < AMI_TOOL_SAVED_MAX_PAGE_ITEMS; i++) {
        if(app->saved_page_display[i]) {
            furi_string_free(app->saved_page_display[i]);
            app->saved_page_display[i] = NULL;
        }
        if(app->saved_page_paths[i]) {
            furi_string_free(app->saved_page_paths[i]);
            app->saved_page_paths[i] = NULL;
        }
        if(app->saved_page_ids[i]) {
            furi_string_free(app->saved_page_ids[i]);
            app->saved_page_ids[i] = NULL;
        }
    }
    ami_tool_reset_retail_key(app);

    /* Storage */
    if(app->storage) {
        furi_record_close(RECORD_STORAGE);
        app->storage = NULL;
    }

    if(app->bt_mutex) {
        furi_mutex_free(app->bt_mutex);
        app->bt_mutex = NULL;
    }

    if(app->bt) {
        furi_record_close(RECORD_BT);
        app->bt = NULL;
    }

    /* GUI record */
    furi_record_close(RECORD_GUI);
    app->gui = NULL;

    free(app);
}

/* Custom events from views -> SceneManager */
static bool ami_tool_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    AmiToolApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

/* Back button -> SceneManager (and scenes decide what to do) */
static bool ami_tool_back_event_callback(void* context) {
    furi_assert(context);
    AmiToolApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

/* Tick events (not used yet but enabled for future animations) */
static void ami_tool_tick_event_callback(void* context) {
    furi_assert(context);
    AmiToolApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

/* Entry point (matches application.fam entry_point) */
int32_t ami_tool_app(void* p) {
    UNUSED(p);

    AmiToolApp* app = ami_tool_alloc();

    /* Start with main menu scene */
    scene_manager_next_scene(app->scene_manager, AmiToolSceneMainMenu);

    /* Main loop; returns when scene_manager_stop() called */
    view_dispatcher_run(app->view_dispatcher);

    ami_tool_free(app);

    return 0;
}

AmiToolRetailKeyStatus ami_tool_load_retail_key(AmiToolApp* app) {
    furi_assert(app);

    if(!app->storage) {
        ami_tool_reset_retail_key(app);
        return AmiToolRetailKeyStatusStorageError;
    }

    File* file = storage_file_alloc(app->storage);
    if(!file) {
        ami_tool_reset_retail_key(app);
        return AmiToolRetailKeyStatusStorageError;
    }

    AmiToolRetailKeyStatus status = AmiToolRetailKeyStatusStorageError;
    const char* path = APP_DATA_PATH(AMI_TOOL_RETAIL_KEY_FILENAME);

    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t read = storage_file_read(file, app->retail_key, sizeof(app->retail_key));
        if(read == AMI_TOOL_RETAIL_KEY_SIZE) {
            uint8_t extra = 0;
            size_t extra_read = storage_file_read(file, &extra, 1);
            if(extra_read == 0) {
                app->retail_key_size = read;
                app->retail_key_loaded = true;
                status = AmiToolRetailKeyStatusOk;
            } else {
                ami_tool_reset_retail_key(app);
                status = AmiToolRetailKeyStatusInvalidSize;
            }
        } else {
            ami_tool_reset_retail_key(app);
            status = AmiToolRetailKeyStatusInvalidSize;
        }
        storage_file_close(file);
    } else {
        ami_tool_reset_retail_key(app);
        status = AmiToolRetailKeyStatusNotFound;
    }

    storage_file_free(file);
    return status;
}

bool ami_tool_has_retail_key(const AmiToolApp* app) {
    return app && app->retail_key_loaded && (app->retail_key_size == AMI_TOOL_RETAIL_KEY_SIZE);
}
