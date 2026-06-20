#include "../esp_flasher_app_i.h"
#include "../esp_flasher_worker.h"

enum SubmenuIndex {
    SubmenuIndexAdvancedMode,
    SubmenuIndexS3Mode,
    SubmenuIndexC5Mode,
    SubmenuIndexBoot,
    SubmenuIndexPart,
    SubmenuIndexNvs,
    SubmenuIndexBootApp0,
    SubmenuIndexAppA,
    SubmenuIndexAppB,
    SubmenuIndexCustom,
    SubmenuIndexFlash,
    SubmenuIndexFlashTurbo,
};

static void _parse_partition_file(EspFlasherApp* app, const char* filepath);
static void _part_confirm_widget_callback(GuiButtonType result, InputType type, void* context);
static void _show_part_confirm(EspFlasherApp* app);

static void esp_flasher_scene_browse_callback(void* context, uint32_t index) {
    EspFlasherApp* app = context;

    scene_manager_set_scene_state(app->scene_manager, EspFlasherSceneBrowse, index);

    // browse for files
    FuriString* predefined_filepath = furi_string_alloc_set_str(ESP_APP_FOLDER);
    FuriString* selected_filepath = furi_string_alloc();
    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, ".bin", &I_Text_10x10);

    // TODO refactor
    switch(index) {
    case SubmenuIndexAdvancedMode:
        app->advanced_mode = !app->advanced_mode;
        // leaving advanced mode: reset any manually-set bootloader address and
        // clear the custom slot so it does not silently flash on next run
        if(!app->advanced_mode) {
            app->boot_addr_manually_set = false;
            app->custom_slot_addrs[SelectedFlashBoot] =
                app->selected_flash_options[SelectedFlashC5Mode] ? ESP_ADDR_BOOT_C5 :
                app->selected_flash_options[SelectedFlashS3Mode] ? ESP_ADDR_BOOT_S3 :
                                                                   ESP_ADDR_BOOT;
            app->selected_flash_options[SelectedFlashCustom] = false;
            app->bin_file_path_custom[0] = '\0';
        }
        view_dispatcher_send_custom_event(app->view_dispatcher, EspFlasherEventRefreshSubmenu);
        break;

    case SubmenuIndexS3Mode:
        // toggle S3 mode
        app->selected_flash_options[SelectedFlashS3Mode] =
            !app->selected_flash_options[SelectedFlashS3Mode];
        if(app->selected_flash_options[SelectedFlashS3Mode])
            app->selected_flash_options[SelectedFlashC5Mode] = false;
        // update bootloader default address unless user set one manually
        if(!app->boot_addr_manually_set)
            app->custom_slot_addrs[SelectedFlashBoot] =
                app->selected_flash_options[SelectedFlashC5Mode] ? ESP_ADDR_BOOT_C5 :
                app->selected_flash_options[SelectedFlashS3Mode] ? ESP_ADDR_BOOT_S3 :
                                                                   ESP_ADDR_BOOT;
        view_dispatcher_send_custom_event(app->view_dispatcher, EspFlasherEventRefreshSubmenu);
        break;

    case SubmenuIndexC5Mode:
        app->selected_flash_options[SelectedFlashC5Mode] =
            !app->selected_flash_options[SelectedFlashC5Mode];
        if(app->selected_flash_options[SelectedFlashC5Mode])
            app->selected_flash_options[SelectedFlashS3Mode] = false;
        // update bootloader default address unless user set one manually
        if(!app->boot_addr_manually_set)
            app->custom_slot_addrs[SelectedFlashBoot] =
                app->selected_flash_options[SelectedFlashC5Mode] ? ESP_ADDR_BOOT_C5 :
                app->selected_flash_options[SelectedFlashS3Mode] ? ESP_ADDR_BOOT_S3 :
                                                                   ESP_ADDR_BOOT;
        view_dispatcher_send_custom_event(app->view_dispatcher, EspFlasherEventRefreshSubmenu);
        break;

    case SubmenuIndexBoot:
        app->selected_flash_options[SelectedFlashBoot] =
            !app->selected_flash_options[SelectedFlashBoot];
        if(app->selected_flash_options[SelectedFlashBoot]) {
            if(dialog_file_browser_show(
                   app->dialogs, selected_filepath, predefined_filepath, &browser_options)) {
                strncpy(
                    app->bin_file_path_boot,
                    furi_string_get_cstr(selected_filepath),
                    sizeof(app->bin_file_path_boot));
                if(app->advanced_mode) {
                    // prompt user to confirm or override the default address
                    app->pending_addr_slot = SelectedFlashBoot;
                    snprintf(
                        app->addr_input_str,
                        sizeof(app->addr_input_str),
                        "0x%lx",
                        app->custom_slot_addrs[SelectedFlashBoot]);
                }
            }
        }
        if(app->bin_file_path_boot[0] == '\0') {
            // if user didn't select a file, leave unselected
            app->selected_flash_options[SelectedFlashBoot] = false;
            view_dispatcher_send_custom_event(app->view_dispatcher, EspFlasherEventRefreshSubmenu);
        } else if(app->advanced_mode) {
            view_dispatcher_send_custom_event(
                app->view_dispatcher, EspFlasherEventShowAddressInput);
        } else {
            view_dispatcher_send_custom_event(app->view_dispatcher, EspFlasherEventRefreshSubmenu);
        }
        break;

    case SubmenuIndexPart:
        app->selected_flash_options[SelectedFlashPart] =
            !app->selected_flash_options[SelectedFlashPart];
        if(app->selected_flash_options[SelectedFlashPart]) {
            if(dialog_file_browser_show(
                   app->dialogs, selected_filepath, predefined_filepath, &browser_options)) {
                strncpy(
                    app->bin_file_path_part,
                    furi_string_get_cstr(selected_filepath),
                    sizeof(app->bin_file_path_part));
                if(app->advanced_mode) {
                    app->pending_addr_slot = SelectedFlashPart;
                    snprintf(
                        app->addr_input_str,
                        sizeof(app->addr_input_str),
                        "0x%lx",
                        app->custom_slot_addrs[SelectedFlashPart]);
                }
                // parse partition file and offer to apply addresses regardless of mode
                _parse_partition_file(app, app->bin_file_path_part);
            }
        }
        if(app->bin_file_path_part[0] == '\0') {
            app->selected_flash_options[SelectedFlashPart] = false;
            view_dispatcher_send_custom_event(app->view_dispatcher, EspFlasherEventRefreshSubmenu);
        } else {
            // check whether the parse found any matching slots
            bool any_matched = false;
            for(int i = SelectedFlashBoot; i < NUM_FLASH_OPTIONS; i++) {
                if(app->parsed_slot_addrs[i] != 0) {
                    any_matched = true;
                    break;
                }
            }
            if(any_matched) {
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, EspFlasherEventShowPartConfirm);
            } else if(app->advanced_mode) {
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, EspFlasherEventShowAddressInput);
            } else {
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, EspFlasherEventRefreshSubmenu);
            }
        }
        break;

    case SubmenuIndexNvs:
        app->selected_flash_options[SelectedFlashNvs] =
            !app->selected_flash_options[SelectedFlashNvs];
        if(app->selected_flash_options[SelectedFlashNvs]) {
            if(dialog_file_browser_show(
                   app->dialogs, selected_filepath, predefined_filepath, &browser_options)) {
                strncpy(
                    app->bin_file_path_nvs,
                    furi_string_get_cstr(selected_filepath),
                    sizeof(app->bin_file_path_nvs));
                if(app->advanced_mode) {
                    app->pending_addr_slot = SelectedFlashNvs;
                    snprintf(
                        app->addr_input_str,
                        sizeof(app->addr_input_str),
                        "0x%lx",
                        app->custom_slot_addrs[SelectedFlashNvs]);
                }
            }
        }
        if(app->bin_file_path_nvs[0] == '\0') {
            app->selected_flash_options[SelectedFlashNvs] = false;
            view_dispatcher_send_custom_event(app->view_dispatcher, EspFlasherEventRefreshSubmenu);
        } else if(app->advanced_mode) {
            view_dispatcher_send_custom_event(
                app->view_dispatcher, EspFlasherEventShowAddressInput);
        } else {
            view_dispatcher_send_custom_event(app->view_dispatcher, EspFlasherEventRefreshSubmenu);
        }
        break;

    case SubmenuIndexBootApp0:
        app->selected_flash_options[SelectedFlashBootApp0] =
            !app->selected_flash_options[SelectedFlashBootApp0];
        if(app->selected_flash_options[SelectedFlashBootApp0]) {
            if(dialog_file_browser_show(
                   app->dialogs, selected_filepath, predefined_filepath, &browser_options)) {
                strncpy(
                    app->bin_file_path_boot_app0,
                    furi_string_get_cstr(selected_filepath),
                    sizeof(app->bin_file_path_boot_app0));
                if(app->advanced_mode) {
                    app->pending_addr_slot = SelectedFlashBootApp0;
                    snprintf(
                        app->addr_input_str,
                        sizeof(app->addr_input_str),
                        "0x%lx",
                        app->custom_slot_addrs[SelectedFlashBootApp0]);
                }
            }
        }
        if(app->bin_file_path_boot_app0[0] == '\0') {
            app->selected_flash_options[SelectedFlashBootApp0] = false;
            view_dispatcher_send_custom_event(app->view_dispatcher, EspFlasherEventRefreshSubmenu);
        } else if(app->advanced_mode) {
            view_dispatcher_send_custom_event(
                app->view_dispatcher, EspFlasherEventShowAddressInput);
        } else {
            view_dispatcher_send_custom_event(app->view_dispatcher, EspFlasherEventRefreshSubmenu);
        }
        break;

    case SubmenuIndexAppA:
        app->selected_flash_options[SelectedFlashAppA] =
            !app->selected_flash_options[SelectedFlashAppA];
        if(app->selected_flash_options[SelectedFlashAppA]) {
            if(dialog_file_browser_show(
                   app->dialogs, selected_filepath, predefined_filepath, &browser_options)) {
                strncpy(
                    app->bin_file_path_app_a,
                    furi_string_get_cstr(selected_filepath),
                    sizeof(app->bin_file_path_app_a));
                if(app->advanced_mode) {
                    app->pending_addr_slot = SelectedFlashAppA;
                    snprintf(
                        app->addr_input_str,
                        sizeof(app->addr_input_str),
                        "0x%lx",
                        app->custom_slot_addrs[SelectedFlashAppA]);
                }
            }
        }
        if(app->bin_file_path_app_a[0] == '\0') {
            app->selected_flash_options[SelectedFlashAppA] = false;
            view_dispatcher_send_custom_event(app->view_dispatcher, EspFlasherEventRefreshSubmenu);
        } else if(app->advanced_mode) {
            view_dispatcher_send_custom_event(
                app->view_dispatcher, EspFlasherEventShowAddressInput);
        } else {
            view_dispatcher_send_custom_event(app->view_dispatcher, EspFlasherEventRefreshSubmenu);
        }
        break;

    case SubmenuIndexAppB:
        app->selected_flash_options[SelectedFlashAppB] =
            !app->selected_flash_options[SelectedFlashAppB];
        if(app->selected_flash_options[SelectedFlashAppB]) {
            if(dialog_file_browser_show(
                   app->dialogs, selected_filepath, predefined_filepath, &browser_options)) {
                strncpy(
                    app->bin_file_path_app_b,
                    furi_string_get_cstr(selected_filepath),
                    sizeof(app->bin_file_path_app_b));
                if(app->advanced_mode) {
                    app->pending_addr_slot = SelectedFlashAppB;
                    snprintf(
                        app->addr_input_str,
                        sizeof(app->addr_input_str),
                        "0x%lx",
                        app->custom_slot_addrs[SelectedFlashAppB]);
                }
            }
        }
        if(app->bin_file_path_app_b[0] == '\0') {
            app->selected_flash_options[SelectedFlashAppB] = false;
            view_dispatcher_send_custom_event(app->view_dispatcher, EspFlasherEventRefreshSubmenu);
        } else if(app->advanced_mode) {
            view_dispatcher_send_custom_event(
                app->view_dispatcher, EspFlasherEventShowAddressInput);
        } else {
            view_dispatcher_send_custom_event(app->view_dispatcher, EspFlasherEventRefreshSubmenu);
        }
        break;

    case SubmenuIndexCustom:
        // only reachable in advanced mode
        app->selected_flash_options[SelectedFlashCustom] =
            !app->selected_flash_options[SelectedFlashCustom];
        if(app->selected_flash_options[SelectedFlashCustom]) {
            if(dialog_file_browser_show(
                   app->dialogs, selected_filepath, predefined_filepath, &browser_options)) {
                strncpy(
                    app->bin_file_path_custom,
                    furi_string_get_cstr(selected_filepath),
                    sizeof(app->bin_file_path_custom));
                app->pending_addr_slot = SelectedFlashCustom;
                snprintf(
                    app->addr_input_str,
                    sizeof(app->addr_input_str),
                    "0x%lx",
                    app->custom_slot_addrs[SelectedFlashCustom]);
            }
        }
        if(app->bin_file_path_custom[0] == '\0') {
            app->selected_flash_options[SelectedFlashCustom] = false;
            view_dispatcher_send_custom_event(app->view_dispatcher, EspFlasherEventRefreshSubmenu);
        } else {
            view_dispatcher_send_custom_event(
                app->view_dispatcher, EspFlasherEventShowAddressInput);
        }
        break;

    case SubmenuIndexFlash:
    case SubmenuIndexFlashTurbo:
        app->turbospeed = (index == SubmenuIndexFlashTurbo); // faster baudrate toggle
        // count how many options are selected
        app->num_selected_flash_options = 0;
        for(bool* option = &app->selected_flash_options[SelectedFlashBoot];
            option < &app->selected_flash_options[NUM_FLASH_OPTIONS];
            ++option) {
            if(*option) {
                ++app->num_selected_flash_options;
            }
        }
        if(app->num_selected_flash_options) {
            // only start next scene if at least one option is selected
            scene_manager_next_scene(app->scene_manager, EspFlasherSceneConsoleOutput);
        }
        break;
    }

    furi_string_free(selected_filepath);
    furi_string_free(predefined_filepath);
}

#define STR_SELECT   "[x]"
#define STR_UNSELECT "[ ]"

// Partition binary entry format (ESP-IDF)
#define PART_ENTRY_SIZE      32
#define PART_MAGIC_BYTE0     0xAA
#define PART_MAGIC_BYTE1     0x50
#define PART_MD5_MAGIC0      0xEB
#define PART_TYPE_APP        0x00
#define PART_TYPE_DATA       0x01
#define PART_SUBTYPE_NVS     0x02
#define PART_SUBTYPE_OTADATA 0x00
#define PART_SUBTYPE_FACTORY 0x00
#define PART_SUBTYPE_OTA_0   0x10
#define PART_SUBTYPE_OTA_1   0x11

static void _parse_partition_file(EspFlasherApp* app, const char* filepath) {
    memset(app->parsed_slot_addrs, 0, sizeof(app->parsed_slot_addrs));
    app->part_confirm_text[0] = '\0';

    File* f = storage_file_alloc(app->storage);
    if(!storage_file_open(f, filepath, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(f);
        return;
    }

    uint8_t entry[PART_ENTRY_SIZE];
    int matched = 0;
    int unmatched = 0;
    size_t pos = 0;

    pos += snprintf(
        app->part_confirm_text + pos, sizeof(app->part_confirm_text) - pos, "Partition file:\n");

    while(storage_file_read(f, entry, PART_ENTRY_SIZE) == PART_ENTRY_SIZE) {
        if(entry[0] == 0xFF) break; // end of table padding
        if(entry[0] == PART_MD5_MAGIC0 && entry[1] == PART_MD5_MAGIC0) continue; // MD5 entry
        if(entry[0] != PART_MAGIC_BYTE0 || entry[1] != PART_MAGIC_BYTE1) continue;

        uint8_t type = entry[2];
        uint8_t subtype = entry[3];
        uint32_t offset = (uint32_t)entry[4] | ((uint32_t)entry[5] << 8) |
                          ((uint32_t)entry[6] << 16) | ((uint32_t)entry[7] << 24);

        char name[17];
        memcpy(name, &entry[12], 16);
        name[16] = '\0';

        // map type/subtype to a known flash slot
        int slot = -1;
        const char* slot_name = NULL;
        if(type == PART_TYPE_APP && subtype == PART_SUBTYPE_FACTORY) {
            slot = SelectedFlashAppA;
            slot_name = "FirmwareA";
        } else if(type == PART_TYPE_APP && subtype == PART_SUBTYPE_OTA_0) {
            slot = SelectedFlashAppA;
            slot_name = "FirmwareA";
        } else if(type == PART_TYPE_APP && subtype == PART_SUBTYPE_OTA_1) {
            slot = SelectedFlashAppB;
            slot_name = "FirmwareB";
        } else if(type == PART_TYPE_DATA && subtype == PART_SUBTYPE_OTADATA) {
            slot = SelectedFlashBootApp0;
            slot_name = "boot_app0";
        } else if(type == PART_TYPE_DATA && subtype == PART_SUBTYPE_NVS) {
            slot = SelectedFlashNvs;
            slot_name = "NVS";
        }

        if(slot != -1) {
            app->parsed_slot_addrs[slot] = offset;
            pos += snprintf(
                app->part_confirm_text + pos,
                sizeof(app->part_confirm_text) - pos,
                "%s: 0x%lx [%s]\n",
                name,
                offset,
                slot_name);
            matched++;
        } else {
            pos += snprintf(
                app->part_confirm_text + pos,
                sizeof(app->part_confirm_text) - pos,
                "%s: 0x%lx [-]\n",
                name,
                offset);
            unmatched++;
        }
    }

    snprintf(
        app->part_confirm_text + pos,
        sizeof(app->part_confirm_text) - pos,
        "\n%d matched, %d other\nApply these addresses?",
        matched,
        unmatched);

    storage_file_close(f);
    storage_file_free(f);
}

static void _part_confirm_widget_callback(GuiButtonType result, InputType type, void* context) {
    EspFlasherApp* app = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher,
            result == GuiButtonTypeRight ? EspFlasherEventPartConfirmYes :
                                           EspFlasherEventPartConfirmNo);
    }
}

static void _show_part_confirm(EspFlasherApp* app) {
    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 52, app->part_confirm_text);
    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "No", _part_confirm_widget_callback, app);
    widget_add_button_element(
        app->widget, GuiButtonTypeRight, "Yes", _part_confirm_widget_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, EspFlasherAppViewWidget);
}

static void _addr_input_callback(void* context) {
    EspFlasherApp* app = context;

    // parse hex string; strtoul with base-0 accepts "0x..." and plain decimal
    uint32_t addr = (uint32_t)strtoul(app->addr_input_str, NULL, 0);
    app->custom_slot_addrs[app->pending_addr_slot] = addr;

    // if user explicitly changed the bootloader address, stop auto-updating it
    if(app->pending_addr_slot == SelectedFlashBoot) {
        app->boot_addr_manually_set = true;
    }

    view_dispatcher_send_custom_event(app->view_dispatcher, EspFlasherEventAddrInputDone);
}

static void _refresh_submenu(EspFlasherApp* app) {
    Submenu* submenu = app->submenu;

    submenu_reset(app->submenu);

    submenu_set_header(submenu, "Browse for files to flash");

    submenu_add_item(
        submenu,
        app->advanced_mode ? "[x] Advanced Mode" : "[ ] Advanced Mode",
        SubmenuIndexAdvancedMode,
        esp_flasher_scene_browse_callback,
        app);

    submenu_add_item(
        submenu,
        app->selected_flash_options[SelectedFlashS3Mode] ? "[x] Using S3, C3 or C6" :
                                                           "[ ] Select for S3, C3 or C6",
        SubmenuIndexS3Mode,
        esp_flasher_scene_browse_callback,
        app);

    submenu_add_item(
        submenu,
        app->selected_flash_options[SelectedFlashC5Mode] ? "[x] Using ESP32-C5" :
                                                           "[ ] Select for ESP32-C5",
        SubmenuIndexC5Mode,
        esp_flasher_scene_browse_callback,
        app);

    if(app->advanced_mode) {
        // build per-slot labels showing the live flash address
        static char str_boot[40];
        static char str_part[40];
        static char str_nvs[40];
        static char str_boot_app0[40];
        static char str_app_a[40];
        static char str_app_b[40];
        static char str_custom[40];

        snprintf(
            str_boot,
            sizeof(str_boot),
            "%s Bootloader (0x%lx)",
            app->selected_flash_options[SelectedFlashBoot] ? STR_SELECT : STR_UNSELECT,
            app->custom_slot_addrs[SelectedFlashBoot]);
        snprintf(
            str_part,
            sizeof(str_part),
            "%s Part Table (0x%lx)",
            app->selected_flash_options[SelectedFlashPart] ? STR_SELECT : STR_UNSELECT,
            app->custom_slot_addrs[SelectedFlashPart]);
        snprintf(
            str_nvs,
            sizeof(str_nvs),
            "%s NVS (0x%lx)",
            app->selected_flash_options[SelectedFlashNvs] ? STR_SELECT : STR_UNSELECT,
            app->custom_slot_addrs[SelectedFlashNvs]);
        snprintf(
            str_boot_app0,
            sizeof(str_boot_app0),
            "%s boot_app0 (0x%lx)",
            app->selected_flash_options[SelectedFlashBootApp0] ? STR_SELECT : STR_UNSELECT,
            app->custom_slot_addrs[SelectedFlashBootApp0]);
        snprintf(
            str_app_a,
            sizeof(str_app_a),
            "%s FirmwareA (0x%lx)",
            app->selected_flash_options[SelectedFlashAppA] ? STR_SELECT : STR_UNSELECT,
            app->custom_slot_addrs[SelectedFlashAppA]);
        snprintf(
            str_app_b,
            sizeof(str_app_b),
            "%s FirmwareB (0x%lx)",
            app->selected_flash_options[SelectedFlashAppB] ? STR_SELECT : STR_UNSELECT,
            app->custom_slot_addrs[SelectedFlashAppB]);
        snprintf(
            str_custom,
            sizeof(str_custom),
            "%s Custom (0x%lx)",
            app->selected_flash_options[SelectedFlashCustom] ? STR_SELECT : STR_UNSELECT,
            app->custom_slot_addrs[SelectedFlashCustom]);

        submenu_add_item(
            submenu, str_boot, SubmenuIndexBoot, esp_flasher_scene_browse_callback, app);
        submenu_add_item(
            submenu, str_part, SubmenuIndexPart, esp_flasher_scene_browse_callback, app);
        submenu_add_item(
            submenu, str_nvs, SubmenuIndexNvs, esp_flasher_scene_browse_callback, app);
        submenu_add_item(
            submenu, str_boot_app0, SubmenuIndexBootApp0, esp_flasher_scene_browse_callback, app);
        submenu_add_item(
            submenu, str_app_a, SubmenuIndexAppA, esp_flasher_scene_browse_callback, app);
        submenu_add_item(
            submenu, str_app_b, SubmenuIndexAppB, esp_flasher_scene_browse_callback, app);
        submenu_add_item(
            submenu, str_custom, SubmenuIndexCustom, esp_flasher_scene_browse_callback, app);
    } else {
#define STR_BOOT      "Bootloader (" TOSTRING(ESP_ADDR_BOOT) ")"
#define STR_BOOT_S3   "Bootloader (" TOSTRING(ESP_ADDR_BOOT_S3) ")"
#define STR_BOOT_C5   "Bootloader (" TOSTRING(ESP_ADDR_BOOT_C5) ")"
#define STR_PART      "Part Table (" TOSTRING(ESP_ADDR_PART) ")"
#define STR_NVS       "NVS (" TOSTRING(ESP_ADDR_NVS) ")"
#define STR_BOOT_APP0 "boot_app0 (" TOSTRING(ESP_ADDR_BOOT_APP0) ")"
#define STR_APP_A     "FirmwareA (" TOSTRING(ESP_ADDR_APP_A) ")"
#define STR_APP_B     "FirmwareB (" TOSTRING(ESP_ADDR_APP_B) ")"

        const char* strSelectBootloader = STR_UNSELECT " " STR_BOOT;
        if(app->selected_flash_options[SelectedFlashC5Mode]) {
            strSelectBootloader = app->selected_flash_options[SelectedFlashBoot] ?
                                      STR_SELECT " " STR_BOOT_C5 :
                                      STR_UNSELECT " " STR_BOOT_C5;
        } else if(app->selected_flash_options[SelectedFlashS3Mode]) {
            strSelectBootloader = app->selected_flash_options[SelectedFlashBoot] ?
                                      STR_SELECT " " STR_BOOT_S3 :
                                      STR_UNSELECT " " STR_BOOT_S3;
        } else {
            strSelectBootloader = app->selected_flash_options[SelectedFlashBoot] ?
                                      STR_SELECT " " STR_BOOT :
                                      STR_UNSELECT " " STR_BOOT;
        }
        submenu_add_item(
            submenu, strSelectBootloader, SubmenuIndexBoot, esp_flasher_scene_browse_callback, app);

        submenu_add_item(
            submenu,
            app->selected_flash_options[SelectedFlashPart] ? STR_SELECT " " STR_PART :
                                                             STR_UNSELECT " " STR_PART,
            SubmenuIndexPart,
            esp_flasher_scene_browse_callback,
            app);

        submenu_add_item(
            submenu,
            app->selected_flash_options[SelectedFlashNvs] ? STR_SELECT " " STR_NVS :
                                                            STR_UNSELECT " " STR_NVS,
            SubmenuIndexNvs,
            esp_flasher_scene_browse_callback,
            app);

        submenu_add_item(
            submenu,
            app->selected_flash_options[SelectedFlashBootApp0] ? STR_SELECT " " STR_BOOT_APP0 :
                                                                 STR_UNSELECT " " STR_BOOT_APP0,
            SubmenuIndexBootApp0,
            esp_flasher_scene_browse_callback,
            app);

        submenu_add_item(
            submenu,
            app->selected_flash_options[SelectedFlashAppA] ? STR_SELECT " " STR_APP_A :
                                                             STR_UNSELECT " " STR_APP_A,
            SubmenuIndexAppA,
            esp_flasher_scene_browse_callback,
            app);

        submenu_add_item(
            submenu,
            app->selected_flash_options[SelectedFlashAppB] ? STR_SELECT " " STR_APP_B :
                                                             STR_UNSELECT " " STR_APP_B,
            SubmenuIndexAppB,
            esp_flasher_scene_browse_callback,
            app);
    }

    // build flash button labels showing active chip mode
    static char str_flash[32];
    static char str_flash_turbo[32];
    snprintf(
        str_flash,
        sizeof(str_flash),
        "[>] FLASH - slow%s",
        app->selected_flash_options[SelectedFlashC5Mode] ? " (C5)" :
        app->selected_flash_options[SelectedFlashS3Mode] ? " (S3)" :
                                                           "");
    snprintf(
        str_flash_turbo,
        sizeof(str_flash_turbo),
        "[>] FLASH - fast%s",
        app->selected_flash_options[SelectedFlashC5Mode] ? " (C5)" :
        app->selected_flash_options[SelectedFlashS3Mode] ? " (S3)" :
                                                           "");

    submenu_add_item(
        submenu, str_flash_turbo, SubmenuIndexFlashTurbo, esp_flasher_scene_browse_callback, app);
    submenu_add_item(
        submenu, str_flash, SubmenuIndexFlash, esp_flasher_scene_browse_callback, app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, EspFlasherSceneBrowse));
    view_dispatcher_switch_to_view(app->view_dispatcher, EspFlasherAppViewSubmenu);
}

void esp_flasher_scene_browse_on_enter(void* context) {
    EspFlasherApp* app = context;

    memset(app->selected_flash_options, 0, sizeof(app->selected_flash_options));
    app->bin_file_path_boot[0] = '\0';
    app->bin_file_path_part[0] = '\0';
    app->bin_file_path_nvs[0] = '\0';
    app->bin_file_path_boot_app0[0] = '\0';
    app->bin_file_path_app_a[0] = '\0';
    app->bin_file_path_app_b[0] = '\0';
    app->bin_file_path_custom[0] = '\0';

    // initialise slot addresses to standard defaults
    app->boot_addr_manually_set = false;
    app->custom_slot_addrs[SelectedFlashBoot] = ESP_ADDR_BOOT;
    app->custom_slot_addrs[SelectedFlashPart] = ESP_ADDR_PART;
    app->custom_slot_addrs[SelectedFlashNvs] = ESP_ADDR_NVS;
    app->custom_slot_addrs[SelectedFlashBootApp0] = ESP_ADDR_BOOT_APP0;
    app->custom_slot_addrs[SelectedFlashAppA] = ESP_ADDR_APP_A;
    app->custom_slot_addrs[SelectedFlashAppB] = ESP_ADDR_APP_B;
    app->custom_slot_addrs[SelectedFlashCustom] = 0x0;

    app->pending_addr_slot = -1;
    // advanced_mode intentionally not reset here so it persists across
    // re-entries (e.g. back-navigating from console output)

    _refresh_submenu(app);
}

bool esp_flasher_scene_browse_on_event(void* context, SceneManagerEvent event) {
    EspFlasherApp* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == EspFlasherEventRefreshSubmenu) {
            _refresh_submenu(app);
            consumed = true;
        } else if(event.event == EspFlasherEventShowAddressInput) {
            // show text input so user can confirm or override the default address
            static const char* slot_header[] = {
                [SelectedFlashS3Mode] = NULL,
                [SelectedFlashC5Mode] = NULL,
                [SelectedFlashBoot] = "Bootloader offset (hex)",
                [SelectedFlashPart] = "Partition table offset (hex)",
                [SelectedFlashNvs] = "NVS offset (hex)",
                [SelectedFlashBootApp0] = "boot_app0 offset (hex)",
                [SelectedFlashAppA] = "Firmware A offset (hex)",
                [SelectedFlashAppB] = "Firmware B offset (hex)",
                [SelectedFlashCustom] = "Custom flash offset (hex)",
            };
            const char* header =
                (app->pending_addr_slot >= 0 && app->pending_addr_slot < NUM_FLASH_OPTIONS &&
                 slot_header[app->pending_addr_slot] != NULL) ?
                    slot_header[app->pending_addr_slot] :
                    "Flash offset (hex)";

            text_input_reset(app->text_input);
            text_input_set_header_text(app->text_input, header);
            text_input_set_result_callback(
                app->text_input,
                _addr_input_callback,
                app,
                app->addr_input_str,
                sizeof(app->addr_input_str),
                false);
            view_dispatcher_switch_to_view(app->view_dispatcher, EspFlasherAppViewTextInput);
            consumed = true;
        } else if(event.event == EspFlasherEventAddrInputDone) {
            _refresh_submenu(app);
            consumed = true;
        } else if(event.event == EspFlasherEventShowPartConfirm) {
            _show_part_confirm(app);
            consumed = true;
        } else if(event.event == EspFlasherEventPartConfirmYes) {
            // apply parsed addresses to any matched slots
            for(int i = SelectedFlashBoot; i < NUM_FLASH_OPTIONS; i++) {
                if(app->parsed_slot_addrs[i] != 0) {
                    app->custom_slot_addrs[i] = app->parsed_slot_addrs[i];
                }
            }
            widget_reset(app->widget);
            if(app->advanced_mode) {
                // still let user confirm or override the partition table's own flash address
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, EspFlasherEventShowAddressInput);
            } else {
                _refresh_submenu(app);
            }
            consumed = true;
        } else if(event.event == EspFlasherEventPartConfirmNo) {
            widget_reset(app->widget);
            if(app->advanced_mode) {
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, EspFlasherEventShowAddressInput);
            } else {
                _refresh_submenu(app);
            }
            consumed = true;
        }
    }

    return consumed;
}

void esp_flasher_scene_browse_on_exit(void* context) {
    EspFlasherApp* app = context;
    submenu_reset(app->submenu);
    widget_reset(app->widget);
}
