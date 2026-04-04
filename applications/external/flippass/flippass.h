/**
 * @file flippass.h
 * @brief Main header file for the FlipPass application.
 *
 * This file defines the main application structure, enums, and constants
 * used throughout the application.
 */
#pragma once

#include "flippass_build_config.h"

#include "scenes/flippass_scene.h"
#include <flipper_format/flipper_format.h>
#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/file_browser.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <notification/notification_messages.h>
#include <rpc/rpc_app.h>
#include <storage/storage.h>

#include "kdbx/kdbx_parser.h"
#include "kdbx/kdbx_arena.h"
#include "kdbx/kdbx_data.h"
#include "kdbx/kdbx_protected.h"
#include "kdbx/kdbx_vault.h"
#include "mntminput/text_input.h"

#define TAG "FlipPass"

#define FLIPPASS_CONFIG_FILE_PATH EXT_PATH("apps_data/flippass/flippass.conf")
#define FLIPPASS_LOG_FILE_PATH    EXT_PATH("apps_data/flippass/flippass.log")
#define FLIPPASS_DEBUG_UNLOCK_FILE_PATH EXT_PATH("apps_data/flippass/debug_unlock.txt")
#define FLIPPASS_SYSTEM_LOG_FILE_PATH EXT_PATH("apps_data/flippass/system_trace.log")
#define FLIPPASS_SYSTEM_LOG_ENABLE_FILE_PATH EXT_PATH("apps_data/flippass/system_trace.enable")
#define FLIPPASS_USB_ENUMERATION_TIMEOUT_MS 15000U
#define FLIPPASS_USB_POLL_DELAY_MS          100U
#define FLIPPASS_USB_PRESS_DELAY_MS         12U
#define FLIPPASS_USB_RELEASE_DELAY_MS       18U
#define FLIPPASS_USB_STEP_DELAY_MS          45U
#define FLIPPASS_USB_SETTLE_DELAY_MS        300U
#define FLIPPASS_USB_SWITCH_DELAY_MS        150U
#define FLIPPASS_OUTPUT_PRE_PRESS_DELAY_MS      10U
#define FLIPPASS_OUTPUT_ALT_PRE_PRESS_DELAY_MS 30U

#define TEXT_BUFFER_SIZE 256
#define STATUS_TITLE_SIZE 64
#define STATUS_MESSAGE_SIZE 256
#define FLIPPASS_RPC_BUFFER_SIZE 512
#define FLIPPASS_PASSWORD_HEADER_SIZE 96
#define FLIPPASS_SYSTEM_LOG_LINE_SIZE 384
#define FLIPPASS_SYSTEM_LOG_RING_LINES 8
#define FLIPPASS_SYSTEM_LOG_RING_LINE_SIZE 128

typedef enum {
    FlipPassEntryActionNone = 0,
    FlipPassEntryActionShowDetails,
    FlipPassEntryActionRevealUsername,
    FlipPassEntryActionRevealPassword,
    FlipPassEntryActionRevealUrl,
    FlipPassEntryActionRevealNotes,
    FlipPassEntryActionRevealAutoType,
    FlipPassEntryActionBrowseOtherFields,
    FlipPassEntryActionTypeUsernameUsb,
    FlipPassEntryActionTypePasswordUsb,
    FlipPassEntryActionTypeAutoTypeUsb,
    FlipPassEntryActionTypeLoginUsb,
    FlipPassEntryActionTypeOtherUsb,
    FlipPassEntryActionTypeUsernameBluetooth,
    FlipPassEntryActionTypePasswordBluetooth,
    FlipPassEntryActionTypeAutoTypeBluetooth,
    FlipPassEntryActionTypeLoginBluetooth,
    FlipPassEntryActionTypeOtherBluetooth,
} FlipPassEntryAction;

typedef enum {
    FlipPassOutputTransportUsb = 0,
    FlipPassOutputTransportBluetooth,
} FlipPassOutputTransport;

typedef struct FlipPassBleSession FlipPassBleSession;
typedef struct FlipPassDbBrowserView FlipPassDbBrowserView;
typedef struct FlipPassProgressView FlipPassProgressView;

/**
 * @struct App
 * @brief Main application structure.
 *
 * This struct holds all the state for the FlipPass application, including
 * handles to the GUI, view dispatcher, scene manager, and various views.
 */
typedef struct App {
    Gui* gui; /**< Pointer to the GUI instance. */
    ViewDispatcher* view_dispatcher; /**< Pointer to the ViewDispatcher instance. */
    SceneManager* scene_manager; /**< Pointer to the SceneManager instance. */
    FuriPubSub* input_events; /**< Global input event stream used for long-Back exit. */
    FuriPubSubSubscription* input_subscription; /**< Subscription handle for long-Back exit. */
    FileBrowser* file_browser; /**< Pointer to the FileBrowser instance. */
    FuriString* file_path; /**< Pointer to a FuriString for the selected file path. */
    TextInput* text_input; /**< Pointer to the TextInput instance. */
    char text_buffer[TEXT_BUFFER_SIZE]; /**< Buffer for text input. */
    char password_header[FLIPPASS_PASSWORD_HEADER_SIZE]; /**< Persistent password-entry header text. */
    char master_password[TEXT_BUFFER_SIZE]; /**< Buffer holding the active password input. */
    FlipPassProgressView* progress_view; /**< Shared progress view for unlock and typing work. */
    FlipPassDbBrowserView* db_browser; /**< Custom browser view for database groups and entries. */
    Submenu* submenu; /**< Reusable submenu for browser and action screens. */
    Widget* widget; /**< Pointer to the Widget instance. */
    DialogEx* dialog_ex; /**< Reusable dialog for confirmations and errors. */
    char status_title[STATUS_TITLE_SIZE]; /**< Short status or error title. */
    char status_message[STATUS_MESSAGE_SIZE]; /**< Detailed status or error message. */
    uint32_t status_return_scene; /**< Scene returned to after dismissing a status screen. */
    KDBXParser* kdbx_parser; /**< Pointer to the KDBX parser instance. */
    KDBXArena* db_arena;
    KDBXVault* vault;
    KDBXVaultBackend active_vault_backend;
    KDBXVaultBackend requested_vault_backend;
    KDBXVault* pending_gzip_scratch_vault; /**< Preflight-staged GZip XML scratch kept across /ext approval. */
    KDBXFieldRef pending_gzip_scratch_ref; /**< Record range for the staged GZip XML scratch. */
    size_t pending_gzip_plain_size; /**< Expected plaintext XML size for the staged GZip scratch. */
    KDBXGroup* root_group;
    KDBXGroup* current_group;
    KDBXEntry* current_entry;
    KDBXGroup* active_group; /**< Group currently shown in the browser scene. */
    KDBXEntry* active_entry; /**< Entry currently shown in detail or action scenes. */
    KDBXProtectedStream protected_stream; /**< Active inner protected-value stream for XML parsing. */
    RpcAppSystem* rpc; /**< Active RPC app context when the app is launched in RPC mode. */
    FuriHalUsbInterface* usb_if_prev; /**< USB interface active before HID typing. */
    bool usb_was_locked; /**< True when FlipPass temporarily unlocked USB for typing. */
    bool usb_expect_rpc_session_close; /**< True when a USB HID takeover may drop the active RPC session. */
    FlipPassBleSession* ble_session; /**< Active Bluetooth HID session state. */
    uint32_t browser_selected_index; /**< Last selected browser item. */
    uint32_t action_selected_index; /**< Last selected action item. */
    uint32_t other_field_selected_index; /**< Last selected item in the other-fields list. */
    uint32_t other_field_action_selected_index; /**< Last selected action for the current other field. */
    FlipPassEntryAction pending_entry_action; /**< Entry action awaiting confirmation or execution. */
    uint32_t pending_other_field_mask; /**< Standard entry field selected from the other-fields flow. */
    KDBXCustomField* pending_other_custom_field; /**< Custom entry field selected from the other-fields flow. */
    char pending_other_field_name[STATUS_TITLE_SIZE]; /**< Current other-field label for prompts and status. */
    bool close_db_dialog_open; /**< True while the close-database confirmation dialog is visible. */
    bool parse_failed; /**< True once XML or data-model parsing hits a handled failure. */
    bool database_loaded; /**< True if the current database was parsed successfully. */
    bool pending_vault_fallback; /**< True when RAM-backed unlock needs explicit /ext continuation approval. */
    bool allow_ext_vault_promotion; /**< True when the current RAM-first unlock may promote its session vault to /ext. */
    bool close_test_logged; /**< True after the first password scene entry is logged. */
    bool rpc_mode; /**< True when the app was launched through the RPC app subsystem. */
    FuriString* text_view_body; /**< Scrollable text-view payload for long notes and fields. */
    char text_view_title[STATUS_TITLE_SIZE]; /**< Title for the shared text-view scene. */
    uint32_t text_view_return_scene; /**< Scene returned to after dismissing the text view. */
    uint32_t progress_started_tick; /**< Tick when the current progress run started. */
    uint8_t progress_percent; /**< Last rendered progress percentage. */
    char progress_title[STATUS_TITLE_SIZE]; /**< Persistent progress-screen title. */
#if FLIPPASS_ENABLE_SYSTEM_TRACE_CAPTURE
    FuriLogHandler system_log_handler; /**< Optional filtered system-log capture for debug sessions. */
    bool system_log_capture_enabled; /**< True while the filtered system-log handler is registered. */
    bool system_log_capture_busy; /**< Reentrancy guard for the filtered system-log handler. */
    size_t system_log_capture_bytes; /**< Total bytes written to the filtered system-log file. */
    size_t system_log_line_len; /**< Pending bytes in the filtered system-log line buffer. */
    char system_log_line[FLIPPASS_SYSTEM_LOG_LINE_SIZE]; /**< Pending filtered system-log line assembly buffer. */
    char* system_log_ring; /**< Optional RAM-backed ring of filtered system-log lines. */
    size_t system_log_ring_count; /**< Number of valid lines currently stored in the ring. */
    size_t system_log_ring_next; /**< Next line slot to overwrite in the RAM-backed ring. */
    size_t system_log_ring_dropped; /**< Number of filtered lines dropped because they exceeded the ring slot size. */
    bool system_log_capture_buffered; /**< True when filtered system-log capture uses RAM buffering instead of live file writes. */
#endif
} App;

/**
 * @enum AppView
 * @brief Enumeration of the different views in the application.
 *
 * This enum is used to identify and manage the different views within the
 * view dispatcher.
 */
typedef enum {
    AppViewFileBrowser, /**< The file browser view. */
    AppViewPasswordEntry, /**< The password entry view. */
    AppViewLoading, /**< Shared loading view for blocking work. */
    AppViewDbBrowser, /**< Custom browser view for database groups and entries. */
    AppViewSubmenu, /**< Shared submenu-based navigation view. */
    AppViewWidget, /**< Shared widget view for detail and status screens. */
    AppViewDialogEx, /**< Shared dialog view for confirmations and errors. */
    AppViewDbEntries = AppViewDbBrowser, /**< Legacy alias for the database browser view. */
} AppView;

void flippass_save_settings(App* app);
void flippass_clear_text_buffer(App* app);
void flippass_clear_master_password(App* app);
void flippass_reset_database(App* app);
void flippass_close_database(App* app);
void flippass_set_status(App* app, const char* title, const char* message);
void flippass_progress_reset(App* app);
void flippass_progress_begin(App* app, const char* title, const char* stage, uint8_t percent);
void flippass_progress_update(App* app, const char* stage, const char* detail, uint8_t percent);
void flippass_request_exit(App* app);
void flippass_log_reset(App* app);
void flippass_log_event(App* app, const char* format, ...);
void flippass_system_log_capture_suspend(void);
void flippass_system_log_capture_resume(void);
bool flippass_system_log_capture_is_suspended(void);
bool flippass_usb_begin(App* app);
bool flippass_usb_press_key_prepared(uint16_t hid_key);
bool flippass_usb_release_key_prepared(uint16_t hid_key);
bool flippass_usb_release_all_prepared(void);
bool flippass_usb_type_string_prepared(const char* text);
bool flippass_usb_type_string(App* app, const char* text);
bool flippass_usb_type_login(App* app, const char* username, const char* password);
bool flippass_usb_type_autotype(App* app, const KDBXEntry* entry);
bool flippass_usb_type_key(App* app, uint16_t hid_key);
void flippass_usb_restore(App* app);
const char* flippass_output_transport_name(FlipPassOutputTransport transport);
bool flippass_output_type_string(App* app, FlipPassOutputTransport transport, const char* text);
bool flippass_output_type_login(
    App* app,
    FlipPassOutputTransport transport,
    const char* username,
    const char* password);
bool flippass_output_type_autotype(
    App* app,
    FlipPassOutputTransport transport,
    const KDBXEntry* entry);
void flippass_output_release_all(App* app);
bool flippass_output_bluetooth_is_connected(const App* app);
bool flippass_output_bluetooth_is_advertising(const App* app);
bool flippass_output_bluetooth_advertise(App* app);
void flippass_output_bluetooth_get_name(char* buffer, size_t size);
void flippass_output_cleanup(App* app);
