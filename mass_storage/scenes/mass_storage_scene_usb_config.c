#include "../mass_storage_app_i.h"
#include <lib/toolbox/path.h>
#include <flipper_format/flipper_format.h>

#define TAG                     "MassStorageSceneUSBConfig"
#define TEXT_INPUT_RESULT_EVENT (1000)

enum ConfigItemIndex {
    ConfigItemIndexSCSIVendor,
    ConfigItemIndexSCSIProduct,
    ConfigItemIndexSCSIRevision,
    ConfigItemIndexSCSISerial,
    ConfigItemIndexUSBManufacturer,
    ConfigItemIndexUSBProduct,
    ConfigItemIndexUSBSerial,
    ConfigItemIndexUSBVendorID,
    ConfigItemIndexUSBProductID,
    ConfigItemIndexSave,
    ConfigItemIndexReset,
};

typedef struct {
    char scsi_vendor[9]; // 8 chars + null
    char scsi_product[17]; // 16 chars + null
    char scsi_revision[5]; // 4 chars + null
    char scsi_serial[64]; // Variable length
    char usb_manufacturer[64]; // Variable length
    char usb_product[64]; // Variable length
    char usb_serial[64]; // Variable length
    uint16_t usb_vendor_id;
    uint16_t usb_product_id;
} ConfigData;

static ConfigData* config_data = NULL;

static void load_default_config(ConfigData* config) {
    memset(config, 0, sizeof(ConfigData));
    strncpy(config->scsi_vendor, "Flipper", sizeof(config->scsi_vendor));
    strncpy(config->scsi_product, "Mass Storage", sizeof(config->scsi_product));
    strncpy(config->scsi_revision, "0001", sizeof(config->scsi_revision));
    strncpy(config->scsi_serial, "123456", sizeof(config->scsi_serial));
    strncpy(config->usb_manufacturer, "Flipper Devices Inc.", sizeof(config->usb_manufacturer));
    strncpy(config->usb_product, "Flipper Zero", sizeof(config->usb_product));
    strncpy(config->usb_serial, "FZ1337", sizeof(config->usb_serial));
    config->usb_vendor_id = 0x0483;
    config->usb_product_id = 0x5720;
}

static bool load_config_from_file(Storage* storage, ConfigData* config) {
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    bool success = false;

    if(!flipper_format_file_open_existing(ff, MASS_STORAGE_CONFIG_FILE_PATH)) {
        goto cleanup;
    }

    FuriString* header = furi_string_alloc();
    uint32_t version;
    if(!flipper_format_read_header(ff, header, &version) ||
       !furi_string_equal_str(header, "Mass Storage Config") || version != 1) {
        furi_string_free(header);
        goto cleanup;
    }
    furi_string_free(header);

    FuriString* temp_str = furi_string_alloc();

    if(flipper_format_read_string(ff, "SCSI_Vendor_ID", temp_str)) {
        strncpy(
            config->scsi_vendor, furi_string_get_cstr(temp_str), sizeof(config->scsi_vendor) - 1);
    }

    if(flipper_format_read_string(ff, "SCSI_Product_ID", temp_str)) {
        strncpy(
            config->scsi_product,
            furi_string_get_cstr(temp_str),
            sizeof(config->scsi_product) - 1);
    }

    if(flipper_format_read_string(ff, "SCSI_Revision", temp_str)) {
        strncpy(
            config->scsi_revision,
            furi_string_get_cstr(temp_str),
            sizeof(config->scsi_revision) - 1);
    }

    if(flipper_format_read_string(ff, "SCSI_Serial", temp_str)) {
        strncpy(
            config->scsi_serial, furi_string_get_cstr(temp_str), sizeof(config->scsi_serial) - 1);
    }

    if(flipper_format_read_string(ff, "USB_Manufacturer", temp_str)) {
        strncpy(
            config->usb_manufacturer,
            furi_string_get_cstr(temp_str),
            sizeof(config->usb_manufacturer) - 1);
    }

    if(flipper_format_read_string(ff, "USB_Product", temp_str)) {
        strncpy(
            config->usb_product, furi_string_get_cstr(temp_str), sizeof(config->usb_product) - 1);
    }

    if(flipper_format_read_string(ff, "USB_Serial", temp_str)) {
        strncpy(
            config->usb_serial, furi_string_get_cstr(temp_str), sizeof(config->usb_serial) - 1);
    }

    furi_string_free(temp_str);

    uint32_t vid, pid;
    if(flipper_format_read_uint32(ff, "USB_Vendor_ID", &vid, 1)) {
        config->usb_vendor_id = (uint16_t)vid;
    }

    if(flipper_format_read_uint32(ff, "USB_Product_ID", &pid, 1)) {
        config->usb_product_id = (uint16_t)pid;
    }

    success = true;

cleanup:
    flipper_format_free(ff);
    return success;
}

static bool save_config_to_file(Storage* storage, ConfigData* config) {
    FuriString* dir_path = furi_string_alloc();
    path_extract_dirname(MASS_STORAGE_CONFIG_FILE_PATH, dir_path);
    storage_simply_mkdir(storage, furi_string_get_cstr(dir_path));
    furi_string_free(dir_path);

    FlipperFormat* ff = flipper_format_file_alloc(storage);
    bool success = false;

    if(!flipper_format_file_open_always(ff, MASS_STORAGE_CONFIG_FILE_PATH)) {
        goto cleanup;
    }

    if(!flipper_format_write_header_cstr(ff, "Mass Storage Config", 1)) {
        goto cleanup;
    }

    if(!flipper_format_write_string_cstr(ff, "SCSI_Vendor_ID", config->scsi_vendor)) {
        goto cleanup;
    }

    if(!flipper_format_write_string_cstr(ff, "SCSI_Product_ID", config->scsi_product)) {
        goto cleanup;
    }

    if(!flipper_format_write_string_cstr(ff, "SCSI_Revision", config->scsi_revision)) {
        goto cleanup;
    }

    if(!flipper_format_write_string_cstr(ff, "SCSI_Serial", config->scsi_serial)) {
        goto cleanup;
    }

    if(!flipper_format_write_string_cstr(ff, "USB_Manufacturer", config->usb_manufacturer)) {
        goto cleanup;
    }

    if(!flipper_format_write_string_cstr(ff, "USB_Product", config->usb_product)) {
        goto cleanup;
    }

    if(!flipper_format_write_string_cstr(ff, "USB_Serial", config->usb_serial)) {
        goto cleanup;
    }

    uint32_t vid = config->usb_vendor_id;
    uint32_t pid = config->usb_product_id;

    if(!flipper_format_write_uint32(ff, "USB_Vendor_ID", &vid, 1)) {
        goto cleanup;
    }

    if(!flipper_format_write_uint32(ff, "USB_Product_ID", &pid, 1)) {
        goto cleanup;
    }

    success = true;

cleanup:
    flipper_format_free(ff);
    return success;
}

static void text_input_result_callback(void* context) {
    MassStorageApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, TEXT_INPUT_RESULT_EVENT);
}

static void config_item_selected_callback(void* context, uint32_t index) {
    MassStorageApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void update_config_display(MassStorageApp* app) {
    VariableItemList* var_list = app->variable_item_list;

    VariableItem* item;

    item = variable_item_list_get(var_list, ConfigItemIndexSCSIVendor);
    variable_item_set_current_value_text(item, config_data->scsi_vendor);

    item = variable_item_list_get(var_list, ConfigItemIndexSCSIProduct);
    variable_item_set_current_value_text(item, config_data->scsi_product);

    item = variable_item_list_get(var_list, ConfigItemIndexSCSIRevision);
    variable_item_set_current_value_text(item, config_data->scsi_revision);

    item = variable_item_list_get(var_list, ConfigItemIndexSCSISerial);
    variable_item_set_current_value_text(item, config_data->scsi_serial);

    item = variable_item_list_get(var_list, ConfigItemIndexUSBManufacturer);
    variable_item_set_current_value_text(item, config_data->usb_manufacturer);

    item = variable_item_list_get(var_list, ConfigItemIndexUSBProduct);
    variable_item_set_current_value_text(item, config_data->usb_product);

    item = variable_item_list_get(var_list, ConfigItemIndexUSBSerial);
    variable_item_set_current_value_text(item, config_data->usb_serial);

    // Format VID/PID as hex
    snprintf(app->text_buffer, sizeof(app->text_buffer), "0x%04X", config_data->usb_vendor_id);
    item = variable_item_list_get(var_list, ConfigItemIndexUSBVendorID);
    variable_item_set_current_value_text(item, app->text_buffer);

    snprintf(app->text_buffer, sizeof(app->text_buffer), "0x%04X", config_data->usb_product_id);
    item = variable_item_list_get(var_list, ConfigItemIndexUSBProductID);
    variable_item_set_current_value_text(item, app->text_buffer);

    variable_item_list_set_selected_item(var_list, ConfigItemIndexSCSIVendor);
}

void mass_storage_scene_usb_config_on_enter(void* context) {
    MassStorageApp* app = context;
    VariableItemList* var_list = app->variable_item_list;

    // Allocate and load config
    config_data = malloc(sizeof(ConfigData));
    load_default_config(config_data);
    if(!load_config_from_file(app->fs_api, config_data)) {
        FURI_LOG_I(TAG, "Using default config");
    }

    // Setup menu items
    variable_item_list_add(var_list, "SCSI Vendor ID", 0, NULL, NULL);
    variable_item_list_add(var_list, "SCSI Product ID", 0, NULL, NULL);
    variable_item_list_add(var_list, "SCSI Revision", 0, NULL, NULL);
    variable_item_list_add(var_list, "SCSI Serial", 0, NULL, NULL);
    variable_item_list_add(var_list, "USB Manufacturer", 0, NULL, NULL);
    variable_item_list_add(var_list, "USB Product", 0, NULL, NULL);
    variable_item_list_add(var_list, "USB Serial", 0, NULL, NULL);
    variable_item_list_add(var_list, "USB Vendor ID", 0, NULL, NULL);
    variable_item_list_add(var_list, "USB Product ID", 0, NULL, NULL);
    variable_item_list_add(var_list, "Save Config", 0, NULL, NULL);
    variable_item_list_add(var_list, "Reset to Defaults", 0, NULL, NULL);

    variable_item_list_set_enter_callback(var_list, config_item_selected_callback, app);
    variable_item_list_set_header(var_list, "USB Configuration");

    update_config_display(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, MassStorageAppViewStart);
}

bool mass_storage_scene_usb_config_on_event(void* context, SceneManagerEvent event) {
    MassStorageApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;

        if(event.event == TEXT_INPUT_RESULT_EVENT) {
            // Handle text input completion
            uint32_t edit_item =
                scene_manager_get_scene_state(app->scene_manager, MassStorageSceneUSBConfig);

            FURI_LOG_I(TAG, "Text input result for item %lu: '%s'", edit_item, app->text_buffer);

            // Update the config based on which item was being edited
            switch(edit_item) {
            case ConfigItemIndexSCSIVendor:
                strncpy(
                    config_data->scsi_vendor,
                    app->text_buffer,
                    sizeof(config_data->scsi_vendor) - 1);
                config_data->scsi_vendor[sizeof(config_data->scsi_vendor) - 1] = '\0';
                break;
            case ConfigItemIndexSCSIProduct:
                strncpy(
                    config_data->scsi_product,
                    app->text_buffer,
                    sizeof(config_data->scsi_product) - 1);
                config_data->scsi_product[sizeof(config_data->scsi_product) - 1] = '\0';
                break;
            case ConfigItemIndexSCSIRevision:
                strncpy(
                    config_data->scsi_revision,
                    app->text_buffer,
                    sizeof(config_data->scsi_revision) - 1);
                config_data->scsi_revision[sizeof(config_data->scsi_revision) - 1] = '\0';
                break;
            case ConfigItemIndexSCSISerial:
                strncpy(
                    config_data->scsi_serial,
                    app->text_buffer,
                    sizeof(config_data->scsi_serial) - 1);
                config_data->scsi_serial[sizeof(config_data->scsi_serial) - 1] = '\0';
                break;
            case ConfigItemIndexUSBManufacturer:
                strncpy(
                    config_data->usb_manufacturer,
                    app->text_buffer,
                    sizeof(config_data->usb_manufacturer) - 1);
                config_data->usb_manufacturer[sizeof(config_data->usb_manufacturer) - 1] = '\0';
                break;
            case ConfigItemIndexUSBProduct:
                strncpy(
                    config_data->usb_product,
                    app->text_buffer,
                    sizeof(config_data->usb_product) - 1);
                config_data->usb_product[sizeof(config_data->usb_product) - 1] = '\0';
                break;
            case ConfigItemIndexUSBSerial:
                strncpy(
                    config_data->usb_serial,
                    app->text_buffer,
                    sizeof(config_data->usb_serial) - 1);
                config_data->usb_serial[sizeof(config_data->usb_serial) - 1] = '\0';
                break;
            case ConfigItemIndexUSBVendorID:
                config_data->usb_vendor_id = (uint16_t)strtol(app->text_buffer, NULL, 16);
                break;
            case ConfigItemIndexUSBProductID:
                config_data->usb_product_id = (uint16_t)strtol(app->text_buffer, NULL, 16);
                break;
            }

            // Update display and return to config menu
            update_config_display(app);
            view_dispatcher_switch_to_view(app->view_dispatcher, MassStorageAppViewStart);

        } else if(event.event == ConfigItemIndexSave) {
            // Save config to file
            if(save_config_to_file(app->fs_api, config_data)) {
                FURI_LOG_I(TAG, "Config saved successfully");
            } else {
                FURI_LOG_E(TAG, "Failed to save config");
            }
            scene_manager_previous_scene(app->scene_manager);

        } else if(event.event == ConfigItemIndexReset) {
            // Reset to defaults
            load_default_config(config_data);
            update_config_display(app);

        } else if(event.event < ConfigItemIndexSave) {
            // Edit a config field
            scene_manager_set_scene_state(
                app->scene_manager, MassStorageSceneUSBConfig, event.event);

            // Load current value into text buffer
            switch(event.event) {
            case ConfigItemIndexSCSIVendor:
                strlcpy(app->text_buffer, config_data->scsi_vendor, sizeof(app->text_buffer));
                break;
            case ConfigItemIndexSCSIProduct:
                strlcpy(app->text_buffer, config_data->scsi_product, sizeof(app->text_buffer));
                break;
            case ConfigItemIndexSCSIRevision:
                strlcpy(app->text_buffer, config_data->scsi_revision, sizeof(app->text_buffer));
                break;
            case ConfigItemIndexSCSISerial:
                strlcpy(app->text_buffer, config_data->scsi_serial, sizeof(app->text_buffer));
                break;
            case ConfigItemIndexUSBManufacturer:
                strlcpy(app->text_buffer, config_data->usb_manufacturer, sizeof(app->text_buffer));
                break;
            case ConfigItemIndexUSBProduct:
                strlcpy(app->text_buffer, config_data->usb_product, sizeof(app->text_buffer));
                break;
            case ConfigItemIndexUSBSerial:
                strlcpy(app->text_buffer, config_data->usb_serial, sizeof(app->text_buffer));
                break;
            case ConfigItemIndexUSBVendorID:
                snprintf(
                    app->text_buffer,
                    sizeof(app->text_buffer),
                    "0x%04X",
                    config_data->usb_vendor_id);
                break;
            case ConfigItemIndexUSBProductID:
                snprintf(
                    app->text_buffer,
                    sizeof(app->text_buffer),
                    "0x%04X",
                    config_data->usb_product_id);
                break;
            }

            // Setup and show text input
            text_input_set_header_text(app->text_input, "Enter Value");
            text_input_set_result_callback(
                app->text_input,
                text_input_result_callback,
                app,
                app->text_buffer,
                sizeof(app->text_buffer),
                true);

            view_dispatcher_switch_to_view(app->view_dispatcher, MassStorageAppViewTextInput);
        }

    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = true;
        scene_manager_previous_scene(app->scene_manager);
    }

    return consumed;
}

void mass_storage_scene_usb_config_on_exit(void* context) {
    MassStorageApp* app = context;

    variable_item_list_reset(app->variable_item_list);

    if(config_data) {
        free(config_data);
        config_data = NULL;
    }
}
