#include "nfc_eink_app_i.h"
#include <flipper_format/flipper_format.h>
#include <path.h>

#define TAG "NfcEinkApp"

#define NFC_EINK_SETTINGS_PATH    NFC_EINK_APP_FOLDER "/.eink.settings"
#define NFC_EINK_SETTINGS_VERSION (1)
#define NFC_EINK_SETTINGS_MAGIC   (0x1B)

static bool nfc_is_hal_ready(void) {
    if(furi_hal_nfc_is_hal_ready() != FuriHalNfcErrorNone) {
        // No connection to the chip, show an error screen
        DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
        DialogMessage* message = dialog_message_alloc();
        dialog_message_set_header(message, "Error: NFC Chip Failed", 64, 0, AlignCenter, AlignTop);
        dialog_message_set_text(
            message, "Send error photo via\nsupport.flipper.net", 0, 63, AlignLeft, AlignBottom);
        dialog_message_set_icon(message, &I_err_09, 128 - 25, 64 - 25);
        dialog_message_show(dialogs, message);
        dialog_message_free(message);
        furi_record_close(RECORD_DIALOGS);
        return false;
    } else {
        return true;
    }
}

static bool nfc_eink_app_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    NfcEinkApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool nfc_eink_app_back_event_callback(void* context) {
    furi_assert(context);
    NfcEinkApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void nfc_eink_app_tick_event_callback(void* context) {
    furi_assert(context);
    NfcEinkApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

static NfcEinkApp* nfc_eink_app_alloc() {
    NfcEinkApp* instance = malloc(sizeof(NfcEinkApp));

    // Open GUI record
    instance->gui = furi_record_open(RECORD_GUI);

    // Open Dialogs record
    instance->dialogs = furi_record_open(RECORD_DIALOGS);

    // Open Notification record
    instance->notifications = furi_record_open(RECORD_NOTIFICATION);

    instance->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(instance->view_dispatcher, instance);
    view_dispatcher_set_custom_event_callback(
        instance->view_dispatcher, nfc_eink_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        instance->view_dispatcher, nfc_eink_app_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        instance->view_dispatcher, nfc_eink_app_tick_event_callback, 100);

    view_dispatcher_attach_to_gui(
        instance->view_dispatcher, instance->gui, ViewDispatcherTypeFullscreen);

    // Submenu
    instance->submenu = submenu_alloc();
    view_dispatcher_add_view(
        instance->view_dispatcher, NfcEinkViewMenu, submenu_get_view(instance->submenu));

    // Popup
    instance->popup = popup_alloc();
    view_dispatcher_add_view(
        instance->view_dispatcher, NfcEinkViewPopup, popup_get_view(instance->popup));

    // Progress bar
    instance->eink_progress = eink_progress_alloc();
    view_dispatcher_add_view(
        instance->view_dispatcher,
        NfcEinkViewProgress,
        eink_progress_get_view(instance->eink_progress));

    // Dialog
    instance->dialog_ex = dialog_ex_alloc();
    view_dispatcher_add_view(
        instance->view_dispatcher, NfcEinkViewDialogEx, dialog_ex_get_view(instance->dialog_ex));

    // Text Input
    instance->text_input = text_input_alloc();
    view_dispatcher_add_view(
        instance->view_dispatcher,
        NfcEinkViewTextInput,
        text_input_get_view(instance->text_input));

    // Custom Widget
    instance->widget = widget_alloc();
    view_dispatcher_add_view(
        instance->view_dispatcher, NfcEinkViewWidget, widget_get_view(instance->widget));

    // Variable item list
    instance->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        instance->view_dispatcher,
        NfcEinkViewVarItemList,
        variable_item_list_get_view(instance->var_item_list));

    // Image scroll
    instance->image_scroll = image_scroll_alloc();
    view_dispatcher_add_view(
        instance->view_dispatcher,
        NfcEinkViewImageScroll,
        image_scroll_get_view(instance->image_scroll));

    instance->scene_manager = scene_manager_alloc(&nfc_eink_scene_handlers, instance);
    instance->nfc = nfc_alloc();
    instance->file_path = furi_string_alloc();
    instance->file_name = furi_string_alloc();
    return instance;
}

static void nfc_eink_app_free(NfcEinkApp* instance) {
    furi_assert(instance);

    nfc_free(instance->nfc);
    scene_manager_free(instance->scene_manager);

    // Submenu
    view_dispatcher_remove_view(instance->view_dispatcher, NfcEinkViewMenu);
    submenu_free(instance->submenu);

    // DialogEx
    view_dispatcher_remove_view(instance->view_dispatcher, NfcEinkViewDialogEx);
    dialog_ex_free(instance->dialog_ex);

    // Popup
    view_dispatcher_remove_view(instance->view_dispatcher, NfcEinkViewPopup);
    popup_free(instance->popup);

    // TextInput
    view_dispatcher_remove_view(instance->view_dispatcher, NfcEinkViewTextInput);
    text_input_free(instance->text_input);

    // Progress
    view_dispatcher_remove_view(instance->view_dispatcher, NfcEinkViewProgress);
    eink_progress_free(instance->eink_progress);

    // Custom Widget
    view_dispatcher_remove_view(instance->view_dispatcher, NfcEinkViewWidget);
    widget_free(instance->widget);

    // Var Item List
    view_dispatcher_remove_view(instance->view_dispatcher, NfcEinkViewVarItemList);
    variable_item_list_free(instance->var_item_list);

    view_dispatcher_remove_view(instance->view_dispatcher, NfcEinkViewImageScroll);
    image_scroll_free(instance->image_scroll);

    view_dispatcher_free(instance->view_dispatcher);

    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    furi_string_free(instance->file_path);
    furi_string_free(instance->file_name);

    instance->dialogs = NULL;
    instance->gui = NULL;
    instance->notifications = NULL;
    free(instance);
}

static void nfc_eink_make_app_folders(const NfcEinkApp* instance) {
    furi_assert(instance);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage_simply_mkdir(storage, NFC_EINK_APP_FOLDER)) {
        dialog_message_show_storage_error(instance->dialogs, "Cannot create\napp folder");
    }
    furi_record_close(RECORD_STORAGE);
}

static void nfc_eink_load_settings(NfcEinkApp* instance) {
    NfcEinkSettings settings = {0};

    if(!saved_struct_load(
           NFC_EINK_SETTINGS_PATH,
           &settings,
           sizeof(NfcEinkSettings),
           NFC_EINK_SETTINGS_MAGIC,
           NFC_EINK_SETTINGS_VERSION)) {
        FURI_LOG_D(TAG, "Failed to load settings, using defaults");
        nfc_eink_save_settings(instance);
    }

    instance->settings.invert_image = settings.invert_image;
    instance->settings.write_mode = settings.write_mode;
}

void nfc_eink_save_settings(NfcEinkApp* instance) {
    NfcEinkSettings settings = {
        .invert_image = instance->settings.invert_image,
        .write_mode = instance->settings.write_mode,
    };

    if(!saved_struct_save(
           NFC_EINK_SETTINGS_PATH,
           &settings,
           sizeof(NfcEinkSettings),
           NFC_EINK_SETTINGS_MAGIC,
           NFC_EINK_SETTINGS_VERSION)) {
        FURI_LOG_E(TAG, "Failed to save settings");
    }
}

NfcEinkLoadResult nfc_eink_load_from_file_select(NfcEinkApp* instance) {
    furi_assert(instance);

    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, NFC_EINK_APP_EXTENSION, &I_Nfc_10px);
    browser_options.base_path = NFC_EINK_APP_FOLDER;
    browser_options.hide_dot_files = true;

    FuriString* tmp = furi_string_alloc();
    furi_string_reset(instance->file_path);

    NfcEinkLoadResult result = NfcEinkLoadResultFailed;
    do {
        // Input events and views are managed by file_browser
        furi_string_printf(tmp, NFC_EINK_APP_FOLDER);

        if(!dialog_file_browser_show(
               instance->dialogs, instance->file_path, tmp, &browser_options)) {
            result = NfcEinkLoadResultCanceled;
            break;
        }

        if(nfc_eink_screen_load_info(
               furi_string_get_cstr(instance->file_path), &instance->info_temp))
            result = NfcEinkLoadResultSuccess;
        path_extract_filename(instance->file_path, instance->file_name, false);
    } while(false);

    furi_string_free(tmp);

    return result;
}

void nfc_eink_blink_emulate_start(NfcEinkApp* app) {
    notification_message(app->notifications, &sequence_blink_start_magenta);
}

void nfc_eink_blink_write_start(NfcEinkApp* app) {
    notification_message(app->notifications, &sequence_blink_start_cyan);
}

void nfc_eink_blink_stop(NfcEinkApp* app) {
    notification_message(app->notifications, &sequence_blink_stop);
}

int32_t nfc_eink(/* void* p */) {
    if(!nfc_is_hal_ready()) return 0;

    NfcEinkApp* app = nfc_eink_app_alloc();

    nfc_eink_make_app_folders(app);
    nfc_eink_load_settings(app);
    scene_manager_next_scene(app->scene_manager, NfcEinkAppSceneStart);

    view_dispatcher_run(app->view_dispatcher);

    nfc_eink_app_free(app);
    return 0;
}
