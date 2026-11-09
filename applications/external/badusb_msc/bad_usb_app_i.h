#pragma once

#include "bad_usb_app.h"
#include "scenes/bad_usb_scene.h"
#include "helpers/ducky_script.h"
#include "helpers/bad_usb_hid.h"
#include "helpers/combo_usb_compat.h"
#include "helpers/combo_runtime.h"
#include "helpers/combo_fat16.h"

#include <gui/gui.h>
#include <badusb_msc_icons.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <dialogs/dialogs.h>
#include <notification/notification_messages.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/text_input.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/widget.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/modules/popup.h>
#include "views/bad_usb_view.h"
#include <furi_hal_usb.h>

// Personal files are never exposed as a USB mass-storage volume.
#define BAD_USB_FILES_BASE_FOLDER EXT_PATH("apps_data/badusb_msc")
#define BAD_USB_FILES_FOLDER      BAD_USB_FILES_BASE_FOLDER "/files"
#define BAD_USB_FILES_PAGE_SIZE   4096
#define BAD_USB_FILES_EDIT_SIZE   8192
#define BAD_USB_FILES_MAX_LINES   128

#define BAD_USB_APP_BASE_FOLDER        EXT_PATH("badusb")
#define BAD_USB_APP_PATH_LAYOUT_FOLDER BAD_USB_APP_BASE_FOLDER "/assets/layouts"
#define BAD_USB_APP_SCRIPT_EXTENSION   ".txt"
#define BAD_USB_APP_LAYOUT_EXTENSION   ".kl"

typedef enum {
    BadUsbAppErrorNoFiles,
    BadUsbAppErrorCloseRpc,
} BadUsbAppError;

struct BadUsbApp {
    ComboRuntime* combo_runtime;
    FuriThread* storage_prepare_thread;
    bool storage_prepared;
    ComboFat16PrepareResult storage_prepare_result;
    volatile uint8_t storage_percent;
    bool storage_show_progress;
    uint8_t storage_done_ticks;
    View* storage_progress_view;
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;
    DialogsApp* dialogs;
    Widget* widget;
    Popup* popup;
    VariableItemList* var_item_list;
    TextInput* text_input;
    ByteInput* byte_input;
    Submenu* submenu;
    TextBox* text_box;

    FuriString* storage_file_path;
    char rename_buffer[96];
    char line_buffer[192];
    char* read_buffer;
    char* edit_buffer;
    uint32_t read_offset;
    uint64_t selected_size;
    size_t edit_length;
    uint16_t edit_lines;
    uint16_t edit_line_starts[BAD_USB_FILES_MAX_LINES];
    uint16_t edit_line_lengths[BAD_USB_FILES_MAX_LINES];
    uint16_t edit_line_index;

    char ble_name_buf[FURI_HAL_BT_ADV_NAME_LENGTH];
    uint8_t ble_mac_buf[GAP_MAC_ADDR_SIZE];
    char usb_name_buf[HID_MANUF_PRODUCT_NAME_LEN];
    uint16_t usb_vidpid_buf[2];

    BadUsbAppError error;
    FuriString* file_path;
    FuriString* keyboard_layout;
    BadUsb* bad_usb_view;
    BadUsbScript* bad_usb_script;

    BadUsbHidInterface interface;
    BadUsbHidConfig user_hid_cfg;
    BadUsbHidConfig script_hid_cfg;
    FuriHalUsbInterface* usb_if_prev;
};

typedef enum {
    BadUsbAppViewWidget,
    BadUsbAppViewPopup,
    BadUsbAppViewWork,
    BadUsbAppViewConfig,
    BadUsbAppViewByteInput,
    BadUsbAppViewTextInput,
    BadUsbAppViewSubmenu,
    BadUsbAppViewTextBox,
    BadUsbAppViewStorageProgress,
} BadUsbAppView;

// Model rendered from the main GUI thread only.
typedef struct {
    uint8_t percent;
    uint8_t state; // 0 writing, 1 ready, 2 error
} BadUsbStorageProgressModel;

void bad_usb_storage_progress_update(BadUsbApp* app, uint8_t percent, uint8_t state);

void bad_usb_set_interface(BadUsbApp* app, BadUsbHidInterface interface);
void bad_usb_save_settings(BadUsbApp* app);

// Private file manager events; the BadUSB key bindings remain unchanged.
typedef enum {
    BadUsbFilesEventScripts = 1000,
    BadUsbFilesEventFiles,
    BadUsbFilesEventOptions,
    BadUsbFilesEventRead,
    BadUsbFilesEventNextPage,
    BadUsbFilesEventPrevPage,
    BadUsbFilesEventEdit,
    BadUsbFilesEventRename,
    BadUsbFilesEventDelete,
    BadUsbFilesEventInputDone,
    BadUsbFilesEventLineBase = 2000,
} BadUsbFilesEvent;

void bad_usb_files_menu_callback(void* context, uint32_t index);
void bad_usb_files_input_callback(void* context);
void bad_usb_files_message(BadUsbApp* app, const char* header, const char* message);
bool bad_usb_files_path_valid(const FuriString* path);
