//** Includes sniffbt and sniffskim for compatible ESP32-WROOM hardware.
// esp_flasher_scene_start.c also changed **//
#pragma once

#include "esp_flasher_app.h"
#include "scenes/esp_flasher_scene.h"
#include "esp_flasher_custom_event.h"
#include "esp_flasher_uart.h"
#include "file/sequential_file.h"

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/text_box.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/modules/text_input.h>

#include <esp_flasher_icons.h>
#include <storage/storage.h>
#include <lib/toolbox/path.h>
#include <dialogs/dialogs.h>
#include <notification/notification_messages.h>

#include <assets_icons.h>

#define ESP_FLASHER_TEXT_BOX_STORE_SIZE (4096)

#define ESP_APP_FOLDER_USER "apps_data/esp_flasher"
#define ESP_APP_FOLDER      EXT_PATH(ESP_APP_FOLDER_USER)

typedef enum SelectedFlashOptions {
    SelectedFlashS3Mode,
    SelectedFlashC5Mode,
    SelectedFlashBoot,
    SelectedFlashPart,
    SelectedFlashNvs,
    SelectedFlashBootApp0,
    SelectedFlashAppA,
    SelectedFlashAppB,
    SelectedFlashCustom,
    NUM_FLASH_OPTIONS
} SelectedFlashOptions;

typedef enum {
    SwitchNotSet,
    SwitchToFirmwareA,
    SwitchToFirmwareB,
} SwitchFirmware;

struct EspFlasherApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;

    FuriString* text_box_store;
    size_t text_box_store_strlen;
    TextBox* text_box;
    Storage* storage;
    DialogsApp* dialogs;
    NotificationApp* notification;

    VariableItemList* var_item_list;
    Widget* widget;
    Submenu* submenu;
    TextInput* text_input;

    EspFlasherUart* uart;
    bool turbospeed;

    bool reset;
    bool boot;
    bool quickflash;

    SwitchFirmware switch_fw;

    bool selected_flash_options[NUM_FLASH_OPTIONS];
    int num_selected_flash_options;
    char bin_file_path_boot[100];
    char bin_file_path_part[100];
    char bin_file_path_nvs[100];
    char bin_file_path_boot_app0[100];
    char bin_file_path_app_a[100];
    char bin_file_path_app_b[100];
    char bin_file_path_custom[100];

    // Per-slot flash addresses (initialized to defaults, overridable by user)
    uint32_t custom_slot_addrs[NUM_FLASH_OPTIONS];
    bool boot_addr_manually_set; // true if user explicitly set bootloader address
    int pending_addr_slot;       // slot index awaiting address input (-1 = none)
    char addr_input_str[12];     // hex address string buffer for TextInput
    bool advanced_mode;          // show custom addresses and Custom slot

    // Partition file parse results (populated when user selects a partition file)
    uint32_t parsed_slot_addrs[NUM_FLASH_OPTIONS]; // 0 = slot not found in file
    char part_confirm_text[512];                   // formatted text for confirm widget

    FuriThread* flash_worker;
    bool flash_worker_busy;
};

typedef enum {
    EspFlasherAppViewVarItemList,
    EspFlasherAppViewConsoleOutput,
    EspFlasherAppViewTextInput,
    EspFlasherAppViewWidget,
    EspFlasherAppViewSubmenu,
} EspFlasherAppView;
