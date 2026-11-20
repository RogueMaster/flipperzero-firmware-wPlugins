#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb.h>
#include <furi_hal_usb_hid.h>
#include <furi_hal_usb_cdc.h>
#include <cli/cli_vcp.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include <notification/notification_messages.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TAG "ZeroBuster"

/* All app files live in /ext/apps_data/zero_buster (the "/data" alias). */
#define APP_DATA_DIR     STORAGE_APP_DATA_PATH_PREFIX
#define LOOT_DIR         APP_DATA_PATH("loot")
#define PAYLOAD_DIR      APP_DATA_PATH("payloads")
#define CUSTOM_HID_FILE  APP_DATA_PATH("custom_hid.txt")
#define INIT_MARKER_FILE APP_DATA_PATH(".initialized")

/* Stock BadUSB keyboard layouts, shared with the firmware app. */
#define LAYOUT_DIR          EXT_PATH("badusb/assets/layouts")
#define LAYOUT_DEFAULT_NAME "en-US"
#define LAYOUT_EXTENSION    ".kl"
#define LAYOUT_KEYS         128
#define LAYOUT_FILE_SIZE    (LAYOUT_KEYS * sizeof(uint16_t))

#define EOF_MARKER "<<<EOF>>>"

#define MAX_PAYLOADS         16
#define MAX_PAYLOAD_NAME_LEN 32
#define PAYLOAD_BUF_SIZE     2048

#define MAX_LAYOUTS         48
#define MAX_LAYOUT_NAME_LEN 24

#define WORKER_FLAG_RX (1 << 0)

typedef enum {
    Spoof_Logitech_K120,
    Spoof_Dell_QuietKey,
    Spoof_Apple_Magic,
    Spoof_Flipper_Default,
    Spoof_Custom,
    Spoof_Count,
} SpoofProfile;

extern const FuriHalUsbHidConfig spoof_configs[];
extern const char* spoof_names[];

#define CUSTOM_MFG_COUNT 7
extern const char* custom_mfg_presets[];

#define CUSTOM_PROD_COUNT 5
extern const char* custom_prod_presets[];

typedef enum {
    State_Idle,
    State_Edit_HID,
    State_About,
    State_HID_Connecting,
    State_Injecting,
    State_Switching_CDC,
    State_Receiving,
    State_Completed,
    State_Error,
} AppState;

typedef enum {
    Setting_Payload,
    Setting_Layout,
    Setting_Spoof,
    Setting_Edit_HID,
    Setting_About,
    Setting_Count,
} SettingRow;

typedef struct {
    char names[MAX_PAYLOADS][MAX_PAYLOAD_NAME_LEN];
    uint32_t count;
    uint32_t selected_index;
} PayloadManager;

typedef struct {
    char names[MAX_LAYOUTS][MAX_LAYOUT_NAME_LEN];
    uint32_t count;
    uint32_t selected_index;
    /* No .kl files on the SD card: the only entry is the built-in en-US map. */
    bool builtin_only;
    /* Active ASCII -> HID usage table, built-in en-US until a .kl is loaded. */
    uint16_t keymap[LAYOUT_KEYS];
} LayoutManager;

typedef struct ZeroBusterApp {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    NotificationApp* notifications;
    Storage* storage;
    CliVcp* cli_vcp;

    FuriThread* worker_thread;
    FuriMutex* mutex;

    AppState state;
    SettingRow active_row;
    uint8_t edit_cursor;
    uint8_t mfg_preset_idx;
    uint8_t prod_preset_idx;

    LayoutManager layouts;
    SpoofProfile spoof_profile;
    FuriHalUsbHidConfig custom_hid_cfg;
    PayloadManager payloads;

    uint32_t bytes_received;
    uint32_t packets_received;
    char current_file[96];
    char error_msg[64];

    volatile bool worker_stop;
    FuriHalUsbInterface* prev_usb_config;
} ZeroBusterApp;

#ifdef __cplusplus
}
#endif

#include "helpers/zero_buster_hid.h"
#include "helpers/zero_buster_storage.h"
#include "helpers/zero_buster_worker.h"
#include "views/zero_buster_view.h"
#include "scenes/zero_buster_scene.h"
