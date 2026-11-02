#include "bad_usb_app_i.h"
#include <furi.h>
#include <furi_hal.h>
#include <storage/storage.h>
#include <lib/toolbox/path.h>
#include <flipper_format/flipper_format.h>
#include "helpers/bundled_keyboard_layouts.h"

#define BAD_USB_SETTINGS_PATH           BAD_USB_FILES_BASE_FOLDER "/.badusb_msc.settings"
#define BAD_USB_SETTINGS_FILE_TYPE      "Flipper BadUSB Settings File"
#define BAD_USB_SETTINGS_VERSION        1
#define BAD_USB_SETTINGS_DEFAULT_LAYOUT BAD_USB_APP_PATH_LAYOUT_FOLDER "/en-US.kl"

// Install missing keyboard layouts as part of app storage initialization,
// alongside the one-time drive.img check. The tables are compiled into this
// FAP: /assets extraction, directory listings, and manual qFlipper copying
// are NOT required. Run at EVERY launch so upgrades add new maps even when
// the existing MSC disk is already valid. Never overwrite user .kl files.
static void bad_usb_install_bundled_keyboard_layouts(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return;

    if((!storage_dir_exists(storage, BAD_USB_APP_BASE_FOLDER) &&
        storage_common_mkdir(storage, BAD_USB_APP_BASE_FOLDER) != FSE_OK) ||
       (!storage_dir_exists(storage, BAD_USB_APP_BASE_FOLDER "/assets") &&
        storage_common_mkdir(storage, BAD_USB_APP_BASE_FOLDER "/assets") != FSE_OK) ||
       (!storage_dir_exists(storage, BAD_USB_APP_PATH_LAYOUT_FOLDER) &&
        storage_common_mkdir(storage, BAD_USB_APP_PATH_LAYOUT_FOLDER) != FSE_OK)) {
        FURI_LOG_E("BadUSB", "Could not create keyboard layout directory");
        furi_record_close(RECORD_STORAGE);
        return;
    }

    File* file = storage_file_alloc(storage);
    if(!file) {
        furi_record_close(RECORD_STORAGE);
        return;
    }
    size_t installed = 0;
    size_t failed = 0;
    for(size_t i = 0; i < BAD_USB_BUNDLED_LAYOUT_COUNT; ++i) {
        const BadUsbBundledLayout* layout = &bad_usb_bundled_layouts[i];
        char destination[128];
        char temporary[136];
        snprintf(
            destination,
            sizeof(destination),
            BAD_USB_APP_PATH_LAYOUT_FOLDER "/%s",
            layout->filename);

        // Preserve any previously installed or user-customized maps.
        if(storage_common_exists(storage, destination)) continue;

        snprintf(temporary, sizeof(temporary), "%s.tmp", destination);
        if(storage_common_exists(storage, temporary)) {
            storage_common_remove(storage, temporary);
        }

        bool opened = storage_file_open(file, temporary, FSAM_WRITE, FSOM_CREATE_ALWAYS);
        size_t written = 0;
        if(opened) {
            written = storage_file_write(file, layout->data, sizeof(layout->data));
            storage_file_close(file);
        }

        if(opened && written == sizeof(layout->data) &&
           !storage_common_exists(storage, destination) &&
           storage_common_rename(storage, temporary, destination) == FSE_OK) {
            installed++;
        } else {
            failed++;
            storage_common_remove(storage, temporary);
            FURI_LOG_W("BadUSB", "Cannot install layout: %s", layout->filename);
        }
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    FURI_LOG_I(
        "BadUSB",
        "Layouts bundled: %u; installed: %u; errors: %u",
        (unsigned)BAD_USB_BUNDLED_LAYOUT_COUNT,
        (unsigned)installed,
        (unsigned)failed);
}

static bool bad_usb_app_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    BadUsbApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool bad_usb_app_back_event_callback(void* context) {
    furi_assert(context);
    BadUsbApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void bad_usb_app_tick_event_callback(void* context) {
    furi_assert(context);
    BadUsbApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

static void bad_usb_load_settings(BadUsbApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* fff = flipper_format_file_alloc(storage);
    bool loaded = false;

    BadUsbHidConfig* hid_cfg = &app->user_hid_cfg;
    FuriString* temp_str = furi_string_alloc();
    uint32_t temp_uint = 0;

    if(flipper_format_file_open_existing(fff, BAD_USB_SETTINGS_PATH)) {
        do {
            if(!flipper_format_read_header(fff, temp_str, &temp_uint)) break;
            if((strcmp(furi_string_get_cstr(temp_str), BAD_USB_SETTINGS_FILE_TYPE) != 0) ||
               (temp_uint != BAD_USB_SETTINGS_VERSION))
                break;

            if(flipper_format_read_string(fff, "layout", temp_str)) {
                furi_string_set(app->keyboard_layout, temp_str);
                uint16_t layout_check[128];
                if(!bad_usb_keyboard_layout_read(app->keyboard_layout, layout_check)) {
                    // The displayed choice must never be a missing keyboard map.
                    FURI_LOG_W("BadUSB", "Saved keyboard map unavailable; using US");
                    furi_string_set(app->keyboard_layout, BAD_USB_SETTINGS_DEFAULT_LAYOUT);
                }
            } else {
                furi_string_set(app->keyboard_layout, BAD_USB_SETTINGS_DEFAULT_LAYOUT);
                flipper_format_rewind(fff);
            }

            if(!flipper_format_read_uint32(fff, "interface", &temp_uint, 1) ||
               temp_uint >= BadUsbHidInterfaceMAX) {
                temp_uint = BadUsbHidInterfaceUsb;
                flipper_format_rewind(fff);
            }
            app->interface = temp_uint;

            if(!flipper_format_read_bool(fff, "ble_bonding", &hid_cfg->ble.bonding, 1)) {
                hid_cfg->ble.bonding = true;
                flipper_format_rewind(fff);
            }

            if(!flipper_format_read_uint32(fff, "ble_pairing", &temp_uint, 1) ||
               temp_uint >= BADUSB_GAP_PAIRING_COUNT) {
                temp_uint = GapPairingPinCodeVerifyYesNo;
                flipper_format_rewind(fff);
            }
            hid_cfg->ble.pairing = temp_uint;

            if(flipper_format_read_string(fff, "ble_name", temp_str)) {
                strlcpy(
                    hid_cfg->ble.name, furi_string_get_cstr(temp_str), sizeof(hid_cfg->ble.name));
            } else {
                hid_cfg->ble.name[0] = '\0';
                flipper_format_rewind(fff);
            }

            if(!flipper_format_read_hex(
                   fff, "ble_mac", hid_cfg->ble.mac, sizeof(hid_cfg->ble.mac))) {
                memset(hid_cfg->ble.mac, 0, sizeof(hid_cfg->ble.mac));
                flipper_format_rewind(fff);
            }

            if(flipper_format_read_string(fff, "usb_manuf", temp_str)) {
                strlcpy(
                    hid_cfg->usb.manuf,
                    furi_string_get_cstr(temp_str),
                    sizeof(hid_cfg->usb.manuf));
            } else {
                hid_cfg->usb.manuf[0] = '\0';
                flipper_format_rewind(fff);
            }

            if(flipper_format_read_string(fff, "usb_product", temp_str)) {
                strlcpy(
                    hid_cfg->usb.product,
                    furi_string_get_cstr(temp_str),
                    sizeof(hid_cfg->usb.product));
            } else {
                hid_cfg->usb.product[0] = '\0';
                flipper_format_rewind(fff);
            }

            if(!flipper_format_read_uint32(fff, "usb_vid", &hid_cfg->usb.vid, 1)) {
                hid_cfg->usb.vid = 0;
                flipper_format_rewind(fff);
            }

            if(!flipper_format_read_uint32(fff, "usb_pid", &hid_cfg->usb.pid, 1)) {
                hid_cfg->usb.pid = 0;
                flipper_format_rewind(fff);
            }

            loaded = true;
        } while(0);
    }

    furi_string_free(temp_str);

    flipper_format_free(fff);
    furi_record_close(RECORD_STORAGE);

    if(!loaded) {
        furi_string_set(app->keyboard_layout, BAD_USB_SETTINGS_DEFAULT_LAYOUT);
        app->interface = BadUsbHidInterfaceUsb;
        hid_cfg->ble.name[0] = '\0';
        memset(hid_cfg->ble.mac, 0, sizeof(hid_cfg->ble.mac));
        hid_cfg->ble.bonding = true;
        hid_cfg->ble.pairing = GapPairingPinCodeVerifyYesNo;
        hid_cfg->usb.vid = 0;
        hid_cfg->usb.pid = 0;
        hid_cfg->usb.manuf[0] = '\0';
        hid_cfg->usb.product[0] = '\0';
    }
}

void bad_usb_save_settings(BadUsbApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* fff = flipper_format_file_alloc(storage);
    BadUsbHidConfig* hid_cfg = &app->user_hid_cfg;
    uint32_t temp_uint = 0;

    storage_common_mkdir(storage, BAD_USB_FILES_BASE_FOLDER);
    if(flipper_format_file_open_always(fff, BAD_USB_SETTINGS_PATH)) {
        do {
            if(!flipper_format_write_header_cstr(
                   fff, BAD_USB_SETTINGS_FILE_TYPE, BAD_USB_SETTINGS_VERSION))
                break;
            if(!flipper_format_write_string(fff, "layout", app->keyboard_layout)) break;
            temp_uint = app->interface;
            if(!flipper_format_write_uint32(fff, "interface", &temp_uint, 1)) break;
            if(!flipper_format_write_bool(fff, "ble_bonding", &hid_cfg->ble.bonding, 1)) break;
            temp_uint = hid_cfg->ble.pairing;
            if(!flipper_format_write_uint32(fff, "ble_pairing", &temp_uint, 1)) break;
            if(!flipper_format_write_string_cstr(fff, "ble_name", hid_cfg->ble.name)) break;
            if(!flipper_format_write_hex(
                   fff, "ble_mac", (uint8_t*)&hid_cfg->ble.mac, sizeof(hid_cfg->ble.mac)))
                break;
            if(!flipper_format_write_string_cstr(fff, "usb_manuf", hid_cfg->usb.manuf)) break;
            if(!flipper_format_write_string_cstr(fff, "usb_product", hid_cfg->usb.product)) break;
            if(!flipper_format_write_uint32(fff, "usb_vid", &hid_cfg->usb.vid, 1)) break;
            if(!flipper_format_write_uint32(fff, "usb_pid", &hid_cfg->usb.pid, 1)) break;
        } while(0);
    }

    flipper_format_free(fff);
    furi_record_close(RECORD_STORAGE);
}

static void bad_usb_storage_progress_callback(uint8_t percent, void* context) {
    BadUsbApp* app = context;
    __atomic_store_n(&app->storage_percent, percent, __ATOMIC_RELAXED);
}

static void bad_usb_storage_progress_draw(Canvas* canvas, void* model_ptr) {
    BadUsbStorageProgressModel* model = model_ptr;
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 11, AlignCenter, AlignBottom, "BadUSB MSC");
    canvas_set_font(canvas, FontSecondary);
    if(model->state == 1) {
        canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignBottom, "Storage Ready");
    } else if(model->state == 2) {
        canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignBottom, "Storage Error");
    } else {
        canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignBottom, "Installing disk...");
    }
    canvas_draw_frame(canvas, 11, 33, 106, 12);
    if(model->percent) canvas_draw_box(canvas, 13, 35, (102u * model->percent) / 100u, 8);
    char percent_text[10];
    snprintf(percent_text, sizeof(percent_text), "%u%%", (unsigned)model->percent);
    canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignBottom, percent_text);
}

void bad_usb_storage_progress_update(BadUsbApp* app, uint8_t percent, uint8_t state) {
    with_view_model(
        app->storage_progress_view,
        BadUsbStorageProgressModel * model,
        {
            model->percent = percent;
            model->state = state;
        },
        true);
}

// Start preparation as soon as the application opens. The GUI must not be
// blocked by 8 MiB of SD writes. Work scene checks completion before USB MSC.
static int32_t bad_usb_storage_prepare_worker(void* context) {
    BadUsbApp* app = context;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(storage_common_mkdir(storage, COMBO_FOLDER) != FSE_OK &&
       !storage_dir_exists(storage, COMBO_FOLDER)) {
        furi_record_close(RECORD_STORAGE);
        return ComboFat16PrepareIoError;
    }
    if(storage_common_mkdir(storage, COMBO_FILES_FOLDER) != FSE_OK &&
       !storage_dir_exists(storage, COMBO_FILES_FOLDER)) {
        furi_record_close(RECORD_STORAGE);
        return ComboFat16PrepareIoError;
    }
    ComboFat16PrepareResult result = combo_fat16_prepare_with_progress(
        storage, COMBO_IMAGE_PATH, bad_usb_storage_progress_callback, app);
    furi_record_close(RECORD_STORAGE);
    // The GUI thread consumes the final status after joining this worker.
    return result;
}

void bad_usb_set_interface(BadUsbApp* app, BadUsbHidInterface interface) {
    app->interface = interface;
    bad_usb_view_set_interface(app->bad_usb_view, interface);
}

BadUsbApp* bad_usb_app_alloc(char* arg) {
    BadUsbApp* app = malloc(sizeof(BadUsbApp));

    app->bad_usb_script = NULL;
    app->combo_runtime = NULL;
    app->storage_prepare_thread = NULL;
    app->storage_prepared = false;
    app->storage_prepare_result = ComboFat16PrepareIoError;
    app->storage_percent = 0;
    app->storage_show_progress = false;
    app->storage_done_ticks = 0;

    app->file_path = furi_string_alloc();
    app->keyboard_layout = furi_string_alloc();
    app->storage_file_path = furi_string_alloc_set(BAD_USB_FILES_FOLDER);
    app->read_buffer = malloc(BAD_USB_FILES_PAGE_SIZE + 180);
    app->edit_buffer = malloc(BAD_USB_FILES_EDIT_SIZE + 1);
    app->read_offset = 0;
    app->selected_size = 0;
    app->edit_length = 0;
    app->edit_lines = 0;
    if(arg && strlen(arg)) {
        furi_string_set(app->file_path, arg);
    }

    // Provision missing keyboard tables on EVERY start, including the first
    // drive.img creation. Updates never erase/rebuild an existing MSC disk.
    bad_usb_install_bundled_keyboard_layouts();
    bad_usb_load_settings(app);
    // Keep the user's saved USB/BLE selection. MSC remains a USB function.
    // Options is accessible before a script is selected.
    memcpy(&app->script_hid_cfg, &app->user_hid_cfg, sizeof(app->script_hid_cfg));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->dialogs = furi_record_open(RECORD_DIALOGS);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&bad_usb_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, bad_usb_app_tick_event_callback, 250);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, bad_usb_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, bad_usb_app_back_event_callback);

    // Custom Widget
    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, BadUsbAppViewWidget, widget_get_view(app->widget));

    // Popup
    app->popup = popup_alloc();
    view_dispatcher_add_view(app->view_dispatcher, BadUsbAppViewPopup, popup_get_view(app->popup));

    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        BadUsbAppViewConfig,
        variable_item_list_get_view(app->var_item_list));

    app->bad_usb_view = bad_usb_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, BadUsbAppViewWork, bad_usb_view_get_view(app->bad_usb_view));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, BadUsbAppViewTextInput, text_input_get_view(app->text_input));

    app->byte_input = byte_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, BadUsbAppViewByteInput, byte_input_get_view(app->byte_input));

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, BadUsbAppViewSubmenu, submenu_get_view(app->submenu));

    app->text_box = text_box_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, BadUsbAppViewTextBox, text_box_get_view(app->text_box));

    app->storage_progress_view = view_alloc();
    view_allocate_model(
        app->storage_progress_view, ViewModelTypeLocking, sizeof(BadUsbStorageProgressModel));
    view_set_draw_callback(app->storage_progress_view, bad_usb_storage_progress_draw);
    view_dispatcher_add_view(
        app->view_dispatcher, BadUsbAppViewStorageProgress, app->storage_progress_view);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    bool usb_locked = furi_hal_usb_is_locked();
    if(!usb_locked) {
        Storage* storage = furi_record_open(RECORD_STORAGE);
        FileInfo info;
        FS_Error status = storage_common_stat(storage, COMBO_IMAGE_PATH, &info);
        // A previously validated disk needs no background preparation.
        // This also avoids a transient "Preparing storage" when opening a
        // script immediately after the app starts.
        bool ready = status == FSE_OK && !file_info_is_dir(&info) &&
                     info.size == COMBO_FAT16_IMAGE_SIZE &&
                     combo_fat16_image_valid(storage, COMBO_IMAGE_PATH);
        app->storage_prepared = ready;
        app->storage_prepare_result = ready ? ComboFat16PrepareReady : ComboFat16PrepareIoError;
        app->storage_percent = ready ? 100 : 0;
        app->storage_show_progress = !ready;
        furi_record_close(RECORD_STORAGE);
        if(!ready) {
            app->storage_prepare_thread =
                furi_thread_alloc_ex("MSCPrepare", 3072, bad_usb_storage_prepare_worker, app);
            if(app->storage_prepare_thread) {
                furi_thread_start(app->storage_prepare_thread);
            } else {
                app->storage_prepare_result = ComboFat16PrepareIoError;
            }
        }
    }
    if(usb_locked) {
        app->error = BadUsbAppErrorCloseRpc;
        app->usb_if_prev = NULL;
        scene_manager_next_scene(app->scene_manager, BadUsbSceneError);
    } else {
        app->usb_if_prev = furi_hal_usb_get_config();
        furi_check(furi_hal_usb_set_config(NULL, NULL));

        if(!furi_string_empty(app->file_path)) {
            scene_manager_set_scene_state(app->scene_manager, BadUsbSceneWork, true);
            scene_manager_next_scene(app->scene_manager, BadUsbSceneWork);
        } else {
            // Le menu ajoute les fonctions fichiers sans changer l'interface Bad USB.
            furi_string_set(app->file_path, BAD_USB_APP_BASE_FOLDER);
            scene_manager_next_scene(app->scene_manager, BadUsbSceneHome);
        }
    }

    return app;
}

void bad_usb_app_free(BadUsbApp* app) {
    furi_assert(app);
    // Stopper le worker HID avant de deconnecter son USB composite.
    if(app->bad_usb_script) {
        bad_usb_script_close(app->bad_usb_script);
        app->bad_usb_script = NULL;
    }
    if(app->combo_runtime) {
        combo_runtime_stop(app->combo_runtime);
        app->combo_runtime = NULL;
    }
    if(app->storage_prepare_thread) {
        furi_thread_join(app->storage_prepare_thread);
        furi_thread_free(app->storage_prepare_thread);
        app->storage_prepare_thread = NULL;
    }

    // Views
    view_dispatcher_remove_view(app->view_dispatcher, BadUsbAppViewStorageProgress);
    view_free(app->storage_progress_view);
    view_dispatcher_remove_view(app->view_dispatcher, BadUsbAppViewWork);
    bad_usb_view_free(app->bad_usb_view);

    // Custom Widget
    view_dispatcher_remove_view(app->view_dispatcher, BadUsbAppViewWidget);
    widget_free(app->widget);

    // Popup
    view_dispatcher_remove_view(app->view_dispatcher, BadUsbAppViewPopup);
    popup_free(app->popup);

    // Config menu
    view_dispatcher_remove_view(app->view_dispatcher, BadUsbAppViewConfig);
    variable_item_list_free(app->var_item_list);

    // Text Input
    view_dispatcher_remove_view(app->view_dispatcher, BadUsbAppViewTextInput);
    text_input_free(app->text_input);

    // Byte Input
    view_dispatcher_remove_view(app->view_dispatcher, BadUsbAppViewByteInput);
    byte_input_free(app->byte_input);

    view_dispatcher_remove_view(app->view_dispatcher, BadUsbAppViewTextBox);
    text_box_free(app->text_box);
    view_dispatcher_remove_view(app->view_dispatcher, BadUsbAppViewSubmenu);
    submenu_free(app->submenu);

    // View dispatcher
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    // Close records
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_DIALOGS);

    bad_usb_save_settings(app);

    furi_string_free(app->file_path);
    furi_string_free(app->keyboard_layout);
    furi_string_free(app->storage_file_path);
    free(app->read_buffer);
    free(app->edit_buffer);

    if(app->usb_if_prev) {
        furi_check(furi_hal_usb_set_config(app->usb_if_prev, NULL));
    }

    free(app);
}

int32_t bad_usb_app(void* p) {
    BadUsbApp* bad_usb_app = bad_usb_app_alloc((char*)p);

    view_dispatcher_run(bad_usb_app->view_dispatcher);

    bad_usb_app_free(bad_usb_app);
    return 0;
}
