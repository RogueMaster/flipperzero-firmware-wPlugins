/*
 * App state.
 */

#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/view.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/popup.h>
#include <gui/modules/widget.h>
#include <gui/modules/text_box.h>
#include <dialogs/dialogs.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <bt/bt_service/bt.h>
#include <targets/f7/ble_glue/profiles/serial_profile.h>

#include <nfc/nfc.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight_poller_sync.h>

#include "views/numlock_input.h"

#include "scenes/tagtinker_scene.h"
#include "protocol/tagtinker_proto.h"
#include "ir/tagtinker_ir.h"
#include "nfc/tagtinker_nfc.h"

#define TAGTINKER_TAG               "TagTinker"
#define TAGTINKER_DISPLAY_NAME      "TagTinker"
#define TAGTINKER_VERSION           FAP_VERSION
#define TAGTINKER_BC_LEN            17
#define TAGTINKER_HEX_LEN           64
#define TAGTINKER_MAX_TARGETS       16
#define TAGTINKER_TARGET_NAME_LEN   16
#define TAGTINKER_MAX_PRESETS       6
#define TAGTINKER_MAX_SYNCED_IMAGES 24
#define TAGTINKER_PRESET_TEXT_LEN   32
#define TAGTINKER_IMAGE_PATH_LEN    255
#define TAGTINKER_SYNC_JOB_ID_LEN   32

typedef enum {
    TagTinkerTxModeNone = 0,
    TagTinkerTxModeTextImage,
    TagTinkerTxModeBmpImage,
} TagTinkerTxMode;

typedef enum {
    TagTinkerSignalPP4 = 0,
} TagTinkerSignalMode;

typedef struct {
    TagTinkerTxMode mode;
    uint8_t plid[4];
    uint8_t page;
    uint16_t width;
    uint16_t height;
    uint16_t pos_x;
    uint16_t pos_y;
    char image_path[TAGTINKER_IMAGE_PATH_LEN + 1];
} TagTinkerImageTxJob;

typedef struct {
    char job_id[TAGTINKER_SYNC_JOB_ID_LEN + 1];
    char barcode[TAGTINKER_BC_LEN + 1];
    uint16_t width;
    uint16_t height;
    uint8_t page;
    /* True when this BMP was authored for a different resolution than the
     * current target, meaning the transmitter will rescale it on the fly. */
    bool resampled;
    char image_path[TAGTINKER_IMAGE_PATH_LEN + 1];
} TagTinkerSyncedImage;

typedef enum {
    TagTinkerTextInputNewText = 0,
    TagTinkerTextInputKeepText = 1,
    TagTinkerTextInputRenameTarget = 2,
} TagTinkerTextInputMode;

/* Views */
typedef enum {
    TagTinkerViewSubmenu,
    TagTinkerViewVarItemList,
    TagTinkerViewTextInput,
    TagTinkerViewPopup,
    TagTinkerViewWidget,
    TagTinkerViewNumlock,
    TagTinkerViewTextBox,
    TagTinkerViewTargetActions,
    TagTinkerViewWarning,
    TagTinkerViewTransmit,
    TagTinkerViewAbout,
} TagTinkerView;

/* Saved ESL target */
typedef struct {
    char name[TAGTINKER_TARGET_NAME_LEN + 1];
    char barcode[TAGTINKER_BC_LEN + 1];
    uint8_t plid[4];
    TagTinkerTagProfile profile;
} TagTinkerTarget;

struct TagTinkerApp {
    /* GUI */
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;
    DialogsApp* dialogs;

    /* Views */
    Submenu* submenu;
    VariableItemList* var_item_list;
    TextInput* text_input;
    Popup* popup;
    Widget* widget;
    NumlockInput* numlock;
    TextBox* text_box;
    View* target_actions_view;
    View* warning_view;
    View* transmit_view;
    View* about_view;
    bool warning_view_allocated;
    bool transmit_view_allocated;
    bool about_view_allocated;

    /* TX state */
    bool tx_active;
    FuriThread* tx_thread;

    /* NFC scan state */
    Nfc* nfc;
    FuriThread* nfc_thread;
    volatile bool nfc_scanning;

    /* Broadcast settings */
    uint8_t broadcast_type;
    uint8_t page;
    uint16_t duration;
    uint16_t repeats;
    bool forever;
    bool tx_spam;

    /* Current target */
    char barcode[TAGTINKER_BC_LEN + 1];
    uint8_t plid[4];
    bool barcode_valid;
    int8_t selected_target; /* -1 = none */

    /* Saved targets */
    TagTinkerTarget targets[TAGTINKER_MAX_TARGETS];
    uint8_t target_count;

    /* Text to push */
    char text_input_buf[64];

    /* ESL display size for current target */
    uint16_t esl_width;
    uint16_t esl_height;

    /* Frame buffer */
    uint8_t frame_buf[TAGTINKER_MAX_FRAME_SIZE];
    size_t frame_len;

    /* Multi-frame sequence */
    uint8_t** frame_sequence;
    size_t* frame_lengths;
    uint16_t* frame_repeats;
    size_t frame_seq_count;
    bool invert_text;
    bool color_clear;
    uint8_t text_padding_pct;

    /* Saved recents */
    struct {
        uint16_t width;
        uint16_t height;
        uint8_t page;
        bool invert;
        bool color_clear;
        uint8_t padding;
        uint8_t signal_mode;
        char text[TAGTINKER_PRESET_TEXT_LEN];
    } recents[TAGTINKER_MAX_PRESETS];
    uint8_t recent_count;

    TagTinkerSyncedImage synced_images[TAGTINKER_MAX_SYNCED_IMAGES];
    uint8_t synced_image_count;

    /* Image settings */
    uint8_t img_page;
    uint16_t draw_x;
    uint16_t draw_y;
    uint16_t draw_width;
    uint16_t draw_height;
    TagTinkerImageTxJob image_tx_job;
    TagTinkerCompressionMode compression_mode;
    uint8_t data_frame_repeats;
    TagTinkerSignalMode signal_mode;
    bool show_startup_warning;

    /* Indicates which mode triggered raw cmd (0=broadcast, 1=targeted) */
    uint8_t raw_mode;

    /* Browser BLE sync state */
    Bt* bt;
    FuriHalBleProfileBase* ble_serial;
    BtStatus ble_status;
    bool ble_sync_active;
    bool ble_sync_start_pending;
    bool ble_serial_configured;
    uint32_t ble_total_rx;
    uint8_t ble_last_bytes[3];
    bool ble_sync_auto_send_pending;
    uint8_t ble_sync_auto_send_delay;
    uint16_t ble_synced_lines;
    char ble_status_text[32];
    char ble_rx_line[1024];
    char ble_rx_pending_line[1024];
    uint16_t ble_rx_len;
    bool ble_rx_pending_ready;
    bool ble_sync_job_active;
    char ble_sync_job_id[TAGTINKER_SYNC_JOB_ID_LEN + 1];
    char ble_sync_barcode[TAGTINKER_BC_LEN + 1];
    char ble_sync_temp_path[TAGTINKER_IMAGE_PATH_LEN + 1];
    char ble_sync_final_path[TAGTINKER_IMAGE_PATH_LEN + 1];
    char ble_sync_last_job_id[TAGTINKER_SYNC_JOB_ID_LEN + 1];
    uint32_t ble_sync_expected_bytes;
    uint32_t ble_sync_received_bytes;
    uint16_t ble_sync_last_chunk;
    File* ble_sync_file;
    uint16_t ble_sync_last_completed_chunks;
    bool ble_sync_compact_protocol;
    bool ble_sync_last_compact_protocol;
    int8_t ble_sync_ready_target;

    /* ---- WiFi Plugins (ESP32 dev board) -------------------------------- */
    /* Opaque handle (TagTinkerWifi*) - declared in wifi/tagtinker_wifi.h.
     * Stored as void* here so this header doesn't pull in expansion/serial
     * deps for unrelated translation units. */
    void* wifi;
    /* WiFi link state mirrored from the ESP. */
    uint8_t wifi_link_state; /* TT_WIFI_* */
    int8_t wifi_rssi;
    char wifi_ssid[33];
    char wifi_ip[20];
    char wifi_creds_ssid[33]; /* used by setup scene before sending */
    char wifi_creds_pwd[65];
    /* Plugin discovery cache. Up to TT_PLUGIN_MAX_PLUGINS_FAP. */
    void* wifi_plugins; /* TagTinkerWifiPlugin[16], heap-alloced */
    uint8_t wifi_plugin_count;
    bool wifi_plugins_loading;
    int8_t wifi_selected_plugin;
    /* Per-run state. */
    char wifi_progress_msg[64];
    uint8_t wifi_progress_pct;
    char wifi_last_error[80];
    bool wifi_run_in_flight;
    bool wifi_result_ready;
    /* Param values being collected by the run scene; one slot per plugin
     * param, holding the textual value the user picked (string for STRING,
     * stringified int for INT, option name for ENUM, "0"/"1" for BOOL). */
    char wifi_param_values[6][64];
};

/* Main menu items */
typedef enum {
    TagTinkerMenuBroadcast,
    TagTinkerMenuTargetESL,
    TagTinkerMenuWifiPlugins,
    TagTinkerMenuAbout,
} TagTinkerMainMenuItem;

/* Broadcast menu items */
typedef enum {
    TagTinkerBroadcastFlipPage,
    TagTinkerBroadcastDebugScreen,
} TagTinkerBroadcastMenuItem;

/* Target action items */
typedef enum {
    TagTinkerTargetDetails,
    TagTinkerTargetRename,
    TagTinkerTargetPushText,
    TagTinkerTargetPushSyncedImage,
    TagTinkerTargetWifiPlugins,
    TagTinkerTargetDeleteSyncedImages,
    TagTinkerTargetPingFlash,
    TagTinkerTargetDeleteTag,
} TagTinkerTargetActionItem;

void tagtinker_target_refresh_profile(TagTinkerTarget* target);
void tagtinker_target_set_default_name(TagTinkerApp* app, TagTinkerTarget* target);
bool tagtinker_target_supports_graphics(const TagTinkerTarget* target);
bool tagtinker_target_supports_accent(const TagTinkerTarget* target);
const char* tagtinker_profile_kind_label(TagTinkerTagKind kind);
const char* tagtinker_profile_color_label(TagTinkerTagColor color);
int8_t tagtinker_find_target_by_barcode(const TagTinkerApp* app, const char* barcode);
int8_t tagtinker_ensure_target(TagTinkerApp* app, const char* barcode);
bool tagtinker_find_latest_synced_image(
    const TagTinkerApp* app,
    const char* barcode,
    TagTinkerSyncedImage* image);
size_t tagtinker_delete_synced_images_for_barcode(TagTinkerApp* app, const char* barcode);
bool tagtinker_delete_target(TagTinkerApp* app, uint8_t index);

void tagtinker_free_frame_sequence(TagTinkerApp* app);
uint16_t tagtinker_pick_chunk_height(uint16_t width, bool color_clear);
void tagtinker_prepare_text_tx(TagTinkerApp* app, const uint8_t plid[4]);
void tagtinker_prepare_bmp_tx(
    TagTinkerApp* app,
    const uint8_t plid[4],
    const char* image_path,
    uint16_t width,
    uint16_t height,
    uint8_t page);
void tagtinker_select_target(TagTinkerApp* app, uint8_t index);
void tagtinker_settings_load(TagTinkerApp* app);
bool tagtinker_settings_save(const TagTinkerApp* app);
void tagtinker_targets_load(TagTinkerApp* app);
bool tagtinker_targets_save(const TagTinkerApp* app);
void tagtinker_recents_load(TagTinkerApp* app);
bool tagtinker_recents_save(const TagTinkerApp* app);
void tagtinker_recents_add(TagTinkerApp* app, const char* text);
