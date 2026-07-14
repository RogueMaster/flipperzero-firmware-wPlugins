#include "clippy_app.h"
#include <core/string.h>
#include <flipper_format/flipper_format.h>
#include <gui/modules/loading.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/view_dispatcher.h>
#include "helpers/ducky_script.h"
#include <storage/filesystem_api_defines.h>
#include <storage/storage.h>
#include "views/clippy_bad_usb_view.h"
#include "views/clippy_mass_storage_view.h"
#include "views/clippy_main_view.h"
#include <furi.h>
#include <gui/gui.h>
#include "clippy_app_i.h"

#define CLIPPY_SETTINGS_PATH           APP_DATA_PATH("/.clippy.settings")
#define CLIPPY_SETTINGS_FILE_TYPE      "Flipper Clippy Settings File"
#define CLIPPY_SETTINGS_VERSION        1
#define CLIPPY_SETTINGS_DEFAULT_LAYOUT CLIPPY_APP_PATH_LAYOUT_FOLDER "/en-US.kl"
#define CLIPPY_IMAGE_FILE              "fat.img"

static bool clippy_app_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    ClippyApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool clippy_app_back_event_callback(void* context) {
    furi_assert(context);
    ClippyApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void clippy_app_tick_event_callback(void* context) {
    furi_assert(context);
    ClippyApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

void clippy_app_show_loading_popup(ClippyApp* app, bool show) {
    if(show) {
        // Raise timer priority so that animations can play
        furi_timer_set_thread_priority(FuriTimerThreadPriorityElevated);
        view_dispatcher_switch_to_view(app->view_dispatcher, ClippyAppViewLoading);
    } else {
        // Restore default timer priority
        furi_timer_set_thread_priority(FuriTimerThreadPriorityNormal);
    }
}

static void clippy_load_settings(ClippyApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* fff = flipper_format_file_alloc(storage);
    bool state = false;

    FuriString* temp_str = furi_string_alloc();
    uint32_t version = 0;
    uint32_t interface = 0;
    uint32_t delay = 0;

    if(flipper_format_file_open_existing(fff, CLIPPY_SETTINGS_PATH)) {
        do {
            if(!flipper_format_read_header(fff, temp_str, &version)) break;
            if((strcmp(furi_string_get_cstr(temp_str), CLIPPY_SETTINGS_FILE_TYPE) != 0) ||
               (version != CLIPPY_SETTINGS_VERSION))
                break;

            if(!flipper_format_read_string(fff, "layout", temp_str)) break;
            if(!flipper_format_read_uint32(fff, "interface", &interface, 1)) break;
            if(!flipper_format_read_uint32(fff, "delay", &delay, 1)) break;
            if(interface > BadUsbHidInterfaceBle) break;

            state = true;
        } while(0);
    }
    flipper_format_free(fff);
    furi_record_close(RECORD_STORAGE);

    if(state) {
        furi_string_set(app->keyboard_layout, temp_str);
        app->interface = interface;
        app->delay = delay;

        Storage* fs_api = furi_record_open(RECORD_STORAGE);
        FileInfo layout_file_info;
        FS_Error file_check_err = storage_common_stat(
            fs_api, furi_string_get_cstr(app->keyboard_layout), &layout_file_info);
        furi_record_close(RECORD_STORAGE);
        if((file_check_err != FSE_OK) || (layout_file_info.size != 256)) {
            furi_string_set(app->keyboard_layout, CLIPPY_SETTINGS_DEFAULT_LAYOUT);
        }
    } else {
        furi_string_set(app->keyboard_layout, CLIPPY_SETTINGS_DEFAULT_LAYOUT);
        app->interface = BadUsbHidInterfaceUsb;
    }

    furi_string_free(temp_str);
}

static void clippy_save_settings(ClippyApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* fff = flipper_format_file_alloc(storage);

    if(flipper_format_file_open_always(fff, CLIPPY_SETTINGS_PATH)) {
        do {
            if(!flipper_format_write_header_cstr(
                   fff, CLIPPY_SETTINGS_FILE_TYPE, CLIPPY_SETTINGS_VERSION))
                break;
            if(!flipper_format_write_string(fff, "layout", app->keyboard_layout)) break;
            uint32_t interface_id = app->interface;
            if(!flipper_format_write_uint32(fff, "interface", (const uint32_t*)&interface_id, 1))
                break;
            if(!flipper_format_write_uint32(fff, "delay", (const uint32_t*)&app->delay, 1)) break;
        } while(0);
    }

    flipper_format_free(fff);
    furi_record_close(RECORD_STORAGE);
}

ClippyApp* clippy_app_alloc() {
    ClippyApp* app = malloc(sizeof(ClippyApp));

    app->fat_image_file_path = furi_string_alloc_set_str(APP_DATA_PATH(CLIPPY_IMAGE_FILE));
    app->string_to_print = furi_string_alloc();
    app->keyboard_layout = furi_string_alloc();
    app->delay = 0;

    clippy_load_settings(app);

    app->gui = furi_record_open(RECORD_GUI);
    app->fs_api = furi_record_open(RECORD_STORAGE);
    app->dialogs = furi_record_open(RECORD_DIALOGS);

    if(!storage_file_exists(app->fs_api, APP_DATA_PATH(CLIPPY_IMAGE_FILE))) {
        storage_common_copy(
            app->fs_api, APP_ASSETS_PATH(CLIPPY_IMAGE_FILE), APP_DATA_PATH(CLIPPY_IMAGE_FILE));
    }

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&clippy_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, clippy_app_tick_event_callback, 500);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, clippy_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, clippy_app_back_event_callback);

    app->widget = widget_alloc();

    // Loading view
    app->loading = loading_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, ClippyAppViewLoading, loading_get_view(app->loading));

    // Main view
    app->main_view = clippy_main_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        ClippyAppViewCopyPasteSelection,
        clippy_main_view_get_view(app->main_view));

    // Clippy mass storage view
    app->mass_storage_view = clippy_mass_storage_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        ClippyAppViewMassStorageWork,
        clippy_mass_storage_get_view(app->mass_storage_view));

    // Paste item selection
    items_array_init(app->items_array);
    app->variable_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        ClippyAppViewPasteItemSelection,
        variable_item_list_get_view(app->variable_item_list));

    // Usb locked
    view_dispatcher_add_view(
        app->view_dispatcher, ClippyAppViewUsbLocked, widget_get_view(app->widget));

    // Clippy Bad USB view
    app->bad_usb_view = clippy_bad_usb_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        ClippyAppViewBadUsbWork,
        clippy_bad_usb_view_get_view(app->bad_usb_view));

    // Clippy Bad USB config
    view_dispatcher_add_view(
        app->view_dispatcher,
        ClippyAppViewBadUsbConfig,
        variable_item_list_get_view(app->variable_item_list));

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    if(furi_hal_usb_is_locked()) {
        app->error = ClippyAppErrorCloseRpc;
        app->usb_if_prev = NULL;
        scene_manager_next_scene(app->scene_manager, ClippySceneUsbLocked);
    } else {
        app->usb_if_prev = furi_hal_usb_get_config();
        furi_check(furi_hal_usb_set_config(NULL, NULL));

        scene_manager_next_scene(app->scene_manager, ClippyAppViewCopyPasteSelection);
    }

    return app;
}

void clippy_app_free(ClippyApp* app) {
    furi_assert(app);

    if(app->bad_usb_payload) {
        clippy_bad_usb_payload_teardown(app->bad_usb_payload);
        app->bad_usb_payload = NULL;
    }

    // Widget view
    widget_free(app->widget);

    // Clippy main view
    clippy_main_view_free(app->main_view);

    // Clippy mass storage view
    clippy_mass_storage_free(app->mass_storage_view);

    // Loading view
    loading_free(app->loading);

    // Clippy bad usb view
    clippy_bad_usb_view_free(app->bad_usb_view);

    // Paste item selection
    items_array_clear(app->items_array);
    variable_item_list_free(app->variable_item_list);

    // Views
    view_dispatcher_remove_view(app->view_dispatcher, ClippyAppViewCopyPasteSelection);
    view_dispatcher_remove_view(app->view_dispatcher, ClippyAppViewPasteItemSelection);
    view_dispatcher_remove_view(app->view_dispatcher, ClippyAppViewMassStorageWork);
    view_dispatcher_remove_view(app->view_dispatcher, ClippyAppViewUsbLocked);
    view_dispatcher_remove_view(app->view_dispatcher, ClippyAppViewLoading);
    view_dispatcher_remove_view(app->view_dispatcher, ClippyAppViewBadUsbWork);
    view_dispatcher_remove_view(app->view_dispatcher, ClippyAppViewBadUsbConfig);

    // View dispatcher
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    // Close records
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_DIALOGS);

    clippy_save_settings(app);

    furi_string_free(app->string_to_print);
    furi_string_free(app->fat_image_file_path);
    furi_string_free(app->keyboard_layout);

    if(app->usb_if_prev) {
        furi_check(furi_hal_usb_set_config(app->usb_if_prev, NULL));
    }

    free(app);
}

int32_t clippy_app() {
    ClippyApp* clippy_app = clippy_app_alloc();
    view_dispatcher_run(clippy_app->view_dispatcher);
    clippy_app_free(clippy_app);
    return 0;
}
