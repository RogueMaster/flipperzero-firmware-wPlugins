#include "openprinttag_i.h"

// Scene on_enter handlers
void (*const openprinttag_scene_on_enter_handlers[])(void*) = {
    openprinttag_scene_start_on_enter,
    openprinttag_scene_read_on_enter,
    openprinttag_scene_read_success_on_enter,
    openprinttag_scene_read_error_on_enter,
    openprinttag_scene_display_on_enter,
};

// Scene on_event handlers
bool (*const openprinttag_scene_on_event_handlers[])(void*, SceneManagerEvent) = {
    openprinttag_scene_start_on_event,
    openprinttag_scene_read_on_event,
    openprinttag_scene_read_success_on_event,
    openprinttag_scene_read_error_on_event,
    openprinttag_scene_display_on_event,
};

// Scene on_exit handlers
void (*const openprinttag_scene_on_exit_handlers[])(void*) = {
    openprinttag_scene_start_on_exit,
    openprinttag_scene_read_on_exit,
    openprinttag_scene_read_success_on_exit,
    openprinttag_scene_read_error_on_exit,
    openprinttag_scene_display_on_exit,
};

static const SceneManagerHandlers openprinttag_scene_handlers = {
    .on_enter_handlers = openprinttag_scene_on_enter_handlers,
    .on_event_handlers = openprinttag_scene_on_event_handlers,
    .on_exit_handlers = openprinttag_scene_on_exit_handlers,
    .scene_num = OpenPrintTagSceneNum,
};

static bool openprinttag_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    OpenPrintTag* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool openprinttag_back_event_callback(void* context) {
    furi_assert(context);
    OpenPrintTag* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

void openprinttag_free_data(OpenPrintTagData* data) {
    if(data->main.brand_name) {
        furi_string_free(data->main.brand_name);
        data->main.brand_name = NULL;
    }
    if(data->main.material_name) {
        furi_string_free(data->main.material_name);
        data->main.material_name = NULL;
    }
    if(data->main.material_type_str) {
        furi_string_free(data->main.material_type_str);
        data->main.material_type_str = NULL;
    }
    if(data->main.material_abbreviation) {
        furi_string_free(data->main.material_abbreviation);
        data->main.material_abbreviation = NULL;
    }
    if(data->main.tags) {
        free(data->main.tags);
        data->main.tags = NULL;
    }
    if(data->aux.workgroup) {
        furi_string_free(data->aux.workgroup);
        data->aux.workgroup = NULL;
    }
    if(data->raw_data) {
        free(data->raw_data);
        data->raw_data = NULL;
    }
    data->raw_data_size = 0;
}

static OpenPrintTag* openprinttag_alloc() {
    OpenPrintTag* app = malloc(sizeof(OpenPrintTag));

    app->gui = furi_record_open(RECORD_GUI);
    app->nfc = nfc_alloc();

    // View dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, openprinttag_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, openprinttag_back_event_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Scene manager
    app->scene_manager = scene_manager_alloc(&openprinttag_scene_handlers, app);

    // Views
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, OpenPrintTagViewSubmenu, submenu_get_view(app->submenu));

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, OpenPrintTagViewWidget, widget_get_view(app->widget));

    app->popup = popup_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, OpenPrintTagViewPopup, popup_get_view(app->popup));

    app->loading = loading_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, OpenPrintTagViewLoading, loading_get_view(app->loading));

    // Initialize tag data
    app->tag_data.main.brand_name = furi_string_alloc();
    app->tag_data.main.material_name = furi_string_alloc();
    app->tag_data.main.material_type_str = furi_string_alloc();
    app->tag_data.main.material_abbreviation = furi_string_alloc();
    app->tag_data.main.material_type_enum = 0;
    app->tag_data.main.material_class = 0;
    app->tag_data.main.gtin = 0;
    app->tag_data.main.nominal_netto_full_weight = 0;
    app->tag_data.main.actual_netto_full_weight = 0;
    app->tag_data.main.empty_container_weight = 0;
    app->tag_data.main.manufactured_date = 0;
    app->tag_data.main.expiration_date = 0;
    app->tag_data.main.filament_diameter = 0;
    app->tag_data.main.nominal_full_length = 0;
    app->tag_data.main.actual_full_length = 0;
    app->tag_data.main.min_print_temperature = 0;
    app->tag_data.main.max_print_temperature = 0;
    app->tag_data.main.min_bed_temperature = 0;
    app->tag_data.main.max_bed_temperature = 0;
    app->tag_data.main.density = 0;
    app->tag_data.main.tags = NULL;
    app->tag_data.main.tags_count = 0;
    app->tag_data.main.has_data = false;
    app->tag_data.main.has_material_type_enum = false;

    app->tag_data.aux.consumed_weight = 0;
    app->tag_data.aux.workgroup = furi_string_alloc();
    app->tag_data.aux.last_stir_time = 0;
    app->tag_data.aux.has_data = false;

    app->tag_data.raw_data = NULL;
    app->tag_data.raw_data_size = 0;

    // Initialize NFC scanner/poller to NULL
    app->nfc_scanner = NULL;
    app->nfc_poller = NULL;

    // Start with main menu scene
    scene_manager_next_scene(app->scene_manager, OpenPrintTagSceneStart);

    return app;
}

static void openprinttag_free(OpenPrintTag* app) {
    furi_assert(app);

    // Free tag data
    openprinttag_free_data(&app->tag_data);

    // Views
    view_dispatcher_remove_view(app->view_dispatcher, OpenPrintTagViewSubmenu);
    submenu_free(app->submenu);

    view_dispatcher_remove_view(app->view_dispatcher, OpenPrintTagViewWidget);
    widget_free(app->widget);

    view_dispatcher_remove_view(app->view_dispatcher, OpenPrintTagViewPopup);
    popup_free(app->popup);

    view_dispatcher_remove_view(app->view_dispatcher, OpenPrintTagViewLoading);
    loading_free(app->loading);

    // Scene manager
    scene_manager_free(app->scene_manager);

    // View dispatcher
    view_dispatcher_free(app->view_dispatcher);

    // NFC
    if(app->nfc_device) {
        nfc_device_free(app->nfc_device);
    }
    nfc_free(app->nfc);

    // GUI
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t openprinttag_app(void* p) {
    UNUSED(p);
    OpenPrintTag* app = openprinttag_alloc();

    view_dispatcher_run(app->view_dispatcher);

    openprinttag_free(app);
    return 0;
}
