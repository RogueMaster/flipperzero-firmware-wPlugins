#include "ibutton_converter_i.h"

#include <toolbox/path.h>

#define TAG "IButtonConverterApp"

static void ibutton_converter_make_ibutton_app_folder(iButtonConverter* ibutton) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    if(!storage_simply_mkdir(storage, IBUTTON_APP_FOLDER)) {
        dialog_message_show_storage_error(ibutton->dialogs, "Cannot create\napp folder");
    }

    furi_record_close(RECORD_STORAGE);
}

bool ibutton_converter_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    iButtonConverter* ibutton = context;
    return scene_manager_handle_custom_event(ibutton->scene_manager, event);
}

bool ibutton_converter_back_event_callback(void* context) {
    furi_assert(context);
    iButtonConverter* ibutton = context;
    return scene_manager_handle_back_event(ibutton->scene_manager);
}

void ibutton_converter_tick_event_callback(void* context) {
    furi_assert(context);
    iButtonConverter* ibutton = context;
    scene_manager_handle_tick_event(ibutton->scene_manager);
}

iButtonConverter* ibutton_converter_alloc(void) {
    iButtonConverter* ibutton_converter = malloc(sizeof(iButtonConverter));

    ibutton_converter->file_path = furi_string_alloc();

    ibutton_converter->scene_manager =
        scene_manager_alloc(&ibutton_converter_scene_handlers, ibutton_converter);

    ibutton_converter->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(
        ibutton_converter->view_dispatcher, ibutton_converter);
    view_dispatcher_set_custom_event_callback(
        ibutton_converter->view_dispatcher, ibutton_converter_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        ibutton_converter->view_dispatcher, ibutton_converter_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        ibutton_converter->view_dispatcher, ibutton_converter_tick_event_callback, 100);

    ibutton_converter->gui = furi_record_open(RECORD_GUI);

    ibutton_converter->dialogs = furi_record_open(RECORD_DIALOGS);

    ibutton_converter->protocols = ibutton_protocols_alloc();
    ibutton_converter->source_key =
        ibutton_key_alloc(ibutton_protocols_get_max_data_size(ibutton_converter->protocols));

    ibutton_converter->submenu = submenu_alloc();
    view_dispatcher_add_view(
        ibutton_converter->view_dispatcher,
        iButtonConverterViewSubmenu,
        submenu_get_view(ibutton_converter->submenu));

    ibutton_converter->text_input = text_input_alloc();
    view_dispatcher_add_view(
        ibutton_converter->view_dispatcher,
        iButtonConverterViewTextInput,
        text_input_get_view(ibutton_converter->text_input));

    ibutton_converter->popup = popup_alloc();
    view_dispatcher_add_view(
        ibutton_converter->view_dispatcher,
        iButtonConverterViewPopup,
        popup_get_view(ibutton_converter->popup));

    ibutton_converter->widget = widget_alloc();
    view_dispatcher_add_view(
        ibutton_converter->view_dispatcher,
        iButtonConverterViewWidget,
        widget_get_view(ibutton_converter->widget));

    ibutton_converter->loading = loading_alloc();
    view_dispatcher_add_view(
        ibutton_converter->view_dispatcher,
        iButtonConverterViewLoading,
        loading_get_view(ibutton_converter->loading));

    return ibutton_converter;
}

void ibutton_converter_free(iButtonConverter* ibutton_converter) {
    furi_assert(ibutton_converter);

    view_dispatcher_remove_view(ibutton_converter->view_dispatcher, iButtonConverterViewLoading);
    loading_free(ibutton_converter->loading);

    view_dispatcher_remove_view(ibutton_converter->view_dispatcher, iButtonConverterViewWidget);
    widget_free(ibutton_converter->widget);

    view_dispatcher_remove_view(ibutton_converter->view_dispatcher, iButtonConverterViewPopup);
    popup_free(ibutton_converter->popup);

    view_dispatcher_remove_view(ibutton_converter->view_dispatcher, iButtonConverterViewTextInput);
    text_input_free(ibutton_converter->text_input);

    view_dispatcher_remove_view(ibutton_converter->view_dispatcher, iButtonConverterViewSubmenu);
    submenu_free(ibutton_converter->submenu);

    view_dispatcher_free(ibutton_converter->view_dispatcher);
    scene_manager_free(ibutton_converter->scene_manager);

    furi_record_close(RECORD_DIALOGS);
    ibutton_converter->dialogs = NULL;

    furi_record_close(RECORD_GUI);
    ibutton_converter->gui = NULL;

    ibutton_key_free(ibutton_converter->source_key);
    if(ibutton_converter->converted_key) {
        ibutton_key_free(ibutton_converter->converted_key);
    }
    ibutton_protocols_free(ibutton_converter->protocols);

    furi_string_free(ibutton_converter->file_path);

    free(ibutton_converter);
}

bool ibutton_converter_load_key(iButtonConverter* ibutton_converter, bool show_error) {
    view_dispatcher_switch_to_view(
        ibutton_converter->view_dispatcher, iButtonConverterViewLoading);

    const bool success = ibutton_protocols_load(
        ibutton_converter->protocols,
        ibutton_converter->source_key,
        furi_string_get_cstr(ibutton_converter->file_path));

    if(success) {
        FuriString* tmp = furi_string_alloc();

        path_extract_filename(ibutton_converter->file_path, tmp, true);
        strlcpy(ibutton_converter->key_name, furi_string_get_cstr(tmp), IBUTTON_KEY_NAME_SIZE);

        furi_string_free(tmp);
    } else if(show_error) {
        dialog_message_show_storage_error(ibutton_converter->dialogs, "Cannot load\nkey file");
    }

    return success;
}

bool ibutton_converter_select_and_load_key(iButtonConverter* ibutton_converter) {
    DialogsFileBrowserOptions browser_options;
    bool success = false;
    dialog_file_browser_set_basic_options(
        &browser_options, IBUTTON_APP_FILENAME_EXTENSION, &I_icon);
    browser_options.base_path = IBUTTON_APP_FOLDER;

    if(furi_string_empty(ibutton_converter->file_path)) {
        furi_string_set(ibutton_converter->file_path, browser_options.base_path);
    }

    do {
        if(!dialog_file_browser_show(
               ibutton_converter->dialogs,
               ibutton_converter->file_path,
               ibutton_converter->file_path,
               &browser_options))
            break;
        success = ibutton_converter_load_key(ibutton_converter, true);
    } while(!success);

    return success;
}

bool ibutton_converter_save_key(iButtonConverter* ibutton_converter) {
    view_dispatcher_switch_to_view(
        ibutton_converter->view_dispatcher, iButtonConverterViewLoading);

    ibutton_converter_make_ibutton_app_folder(ibutton_converter);

    iButtonKey* key = ibutton_converter->converted_key;
    const bool success = ibutton_protocols_save(
        ibutton_converter->protocols, key, furi_string_get_cstr(ibutton_converter->file_path));

    if(!success) {
        dialog_message_show_storage_error(ibutton_converter->dialogs, "Cannot save\nkey file");
    }

    return success;
}

void ibutton_converter_reset_key(iButtonConverter* ibutton_converter) {
    ibutton_converter->key_name[0] = '\0';
    furi_string_reset(ibutton_converter->file_path);
    ibutton_key_reset(ibutton_converter->source_key);
}

void ibutton_converter_submenu_callback(void* context, uint32_t index) {
    iButtonConverter* ibutton_converter = context;
    view_dispatcher_send_custom_event(ibutton_converter->view_dispatcher, index);
}

void ibutton_converter_widget_callback(GuiButtonType result, InputType type, void* context) {
    iButtonConverter* ibutton_converter = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(ibutton_converter->view_dispatcher, result);
    }
}

int32_t ibutton_converter_app(void* arg) {
    UNUSED(arg);

    iButtonConverter* ibutton_converter = ibutton_converter_alloc();

    ibutton_converter_make_ibutton_app_folder(ibutton_converter);

    view_dispatcher_attach_to_gui(
        ibutton_converter->view_dispatcher, ibutton_converter->gui, ViewDispatcherTypeFullscreen);

    scene_manager_next_scene(ibutton_converter->scene_manager, iButtonConverterSceneStart);

    view_dispatcher_run(ibutton_converter->view_dispatcher);

    ibutton_converter_free(ibutton_converter);
    return 0;
}
