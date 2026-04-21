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
#include <flipper_application/flipper_application.h>
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

#include "kdbx/kdbx_arena.h"
#include "kdbx/kdbx_data.h"
#include "kdbx/kdbx_vault.h"
#include "mntminput/text_input.h"

#define TAG "FlipPass"
#define FLIPPASS_ENABLE_SYSTEM_TRACE_CAPTURE_EFFECTIVE \
    (FLIPPASS_ENABLE_LOGS && FLIPPASS_ENABLE_SYSTEM_TRACE_CAPTURE)

#define FLIPPASS_CONFIG_FILE_PATH              EXT_PATH("apps_data/flippass/flippass.conf")
#define FLIPPASS_LOG_FILE_PATH                 EXT_PATH("apps_data/flippass/flippass.log")
#define FLIPPASS_DEBUG_UNLOCK_FILE_PATH        EXT_PATH("apps_data/flippass/debug_unlock.txt")
#define FLIPPASS_SYSTEM_LOG_FILE_PATH          EXT_PATH("apps_data/flippass/system_trace.log")
#define FLIPPASS_SYSTEM_LOG_ENABLE_FILE_PATH   EXT_PATH("apps_data/flippass/system_trace.enable")
#define FLIPPASS_BADUSB_LAYOUT_DIR             EXT_PATH("badusb/assets/layouts")
#define FLIPPASS_BADUSB_LAYOUT_EXT             ".kl"
#define FLIPPASS_KEYBOARD_LAYOUT_ALT           "alt-numpad"
#define FLIPPASS_USB_ENUMERATION_TIMEOUT_MS    15000U
#define FLIPPASS_USB_ENUMERATION_GRACE_MS      5000U
#define FLIPPASS_USB_PREPARE_RETRY_COUNT       3U
#define FLIPPASS_USB_POLL_DELAY_MS             100U
#define FLIPPASS_USB_PRESS_DELAY_MS            12U
#define FLIPPASS_USB_RELEASE_DELAY_MS          18U
#define FLIPPASS_USB_STEP_DELAY_MS             45U
#define FLIPPASS_USB_SETTLE_DELAY_MS           300U
#define FLIPPASS_USB_SWITCH_DELAY_MS           150U
#define FLIPPASS_OUTPUT_PRE_PRESS_DELAY_MS     10U
#define FLIPPASS_OUTPUT_ALT_PRE_PRESS_DELAY_MS 30U

#define TEXT_BUFFER_SIZE                   256
#define STATUS_TITLE_SIZE                  64
#define STATUS_MESSAGE_SIZE                256
#define FLIPPASS_RPC_BUFFER_SIZE           512
#define FLIPPASS_PASSWORD_HEADER_SIZE      96
#define FLIPPASS_SYSTEM_LOG_LINE_SIZE      384
#define FLIPPASS_SYSTEM_LOG_RING_LINES     8
#define FLIPPASS_SYSTEM_LOG_RING_LINE_SIZE 128
#define FLIPPASS_SECURE_VALUE_NONCE_SIZE   16U
#define FLIPPASS_SECURE_VALUE_MAC_SIZE     32U

#if FLIPPASS_ENABLE_LOGS
#define FLIPPASS_LOG_EVENT(app, ...) flippass_log_event((app), __VA_ARGS__)
#define FLIPPASS_FURI_LOG_E(...)     FURI_LOG_E(__VA_ARGS__)
#define FLIPPASS_FURI_LOG_T(...)     FURI_LOG_T(__VA_ARGS__)
#else
#define FLIPPASS_LOG_EVENT(app, ...) \
    do {                             \
        UNUSED(app);                 \
    } while(0)
#define FLIPPASS_FURI_LOG_E(...) \
    do {                         \
    } while(0)
#define FLIPPASS_FURI_LOG_T(...) \
    do {                         \
    } while(0)
#endif

typedef enum {
    FlipPassEntryActionNone = 0,
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

typedef enum {
    FlipPassModuleSlotOutputUsb = 0,
    FlipPassModuleSlotOutputBle,
    FlipPassModuleSlotRpcCommands,
    FlipPassModuleSlotOpenAcquire,
    FlipPassModuleSlotOpenStream,
    FlipPassModuleSlotOpenInflateNonPaged,
    FlipPassModuleSlotOpenInflatePaged,
    FlipPassModuleSlotOpenModel,
    FlipPassModuleSlotKeyboardLayout,
    FlipPassModuleSlotCount,
} FlipPassModuleSlot;

typedef enum {
    FlipPassOutputActionString = 0,
    FlipPassOutputActionLogin,
    FlipPassOutputActionVaultRef,
    FlipPassOutputActionLoginRefs,
    FlipPassOutputActionAutotype,
} FlipPassOutputAction;

typedef struct FlipPassDbBrowserView FlipPassDbBrowserView;
typedef struct FlipPassProgressView FlipPassProgressView;

typedef struct {
    FlipperApplication* application;
    const FlipperAppPluginDescriptor* descriptor;
} FlipPassModuleInstance;

typedef struct {
    Storage* storage; /**< Shared storage record used to late-load embedded plugins. */
    FlipPassModuleInstance
        slot[FlipPassModuleSlotCount]; /**< One direct load slot per feature island. */
} FlipPassModuleLoader;

/**
 * @brief Host-owned runtime state that survives scene changes and owns process services.
 */
typedef struct {
    Gui* gui; /**< Pointer to the GUI instance. */
    ViewDispatcher* view_dispatcher; /**< Pointer to the ViewDispatcher instance. */
    SceneManager* scene_manager; /**< Pointer to the SceneManager instance. */
    FuriPubSub* input_events; /**< Global input event stream used for long-Back exit. */
    FuriPubSubSubscription* input_subscription; /**< Subscription handle for long-Back exit. */
    RpcAppSystem* rpc; /**< Active RPC app context when the app is launched in RPC mode. */
    FlipPassModuleLoader module_loader; /**< Direct late-loaded embedded plugin ownership. */
    bool usb_expect_rpc_session_close; /**< True when a USB HID takeover may drop the active RPC session. */
    bool rpc_mode; /**< True when the app was launched through the RPC app subsystem. */
    bool debug_auto_continue_vault_fallback; /**< Bench-only hook that auto-accepts the /ext continuation prompt for the current unlock. */
    volatile bool
        typing_active; /**< True while the shared typing progress screen owns Back as a cancel action. */
    volatile bool typing_cancel_requested; /**< Raised by Back while a typing run is active. */
    volatile uint32_t
        typing_cancel_back_tick; /**< Tick when Back canceled typing so one immediate navigation Back can be suppressed. */
#if FLIPPASS_ENABLE_SYSTEM_TRACE_CAPTURE_EFFECTIVE
    FuriLogHandler
        system_log_handler; /**< Optional filtered system-log capture for debug sessions. */
    bool system_log_capture_enabled; /**< True while the filtered system-log handler is registered. */
    bool system_log_capture_busy; /**< Reentrancy guard for the filtered system-log handler. */
    size_t system_log_capture_bytes; /**< Total bytes written to the filtered system-log file. */
    size_t system_log_line_len; /**< Pending bytes in the filtered system-log line buffer. */
    char system_log_line
        [FLIPPASS_SYSTEM_LOG_LINE_SIZE]; /**< Pending filtered system-log line assembly buffer. */
    char* system_log_ring; /**< Optional RAM-backed ring of filtered system-log lines. */
    size_t system_log_ring_count; /**< Number of valid lines currently stored in the ring. */
    size_t system_log_ring_next; /**< Next line slot to overwrite in the RAM-backed ring. */
    size_t
        system_log_ring_dropped; /**< Number of filtered lines dropped because they exceeded the ring slot size. */
    bool system_log_capture_buffered; /**< True when filtered system-log capture uses RAM buffering instead of live file writes. */
#endif
} FlipPassHostRuntime;

/**
 * @brief Host-owned unlocked session state and memory-sensitive parser/vault ownership.
 */
typedef struct {
    KDBXArena* db_arena; /**< Resident parsed database arena. */
    KDBXVault* vault; /**< Active encrypted session vault. */
    KDBXVaultBackend active_vault_backend; /**< Actual backend selected for the active vault. */
    KDBXVaultBackend requested_vault_backend; /**< Backend requested by the current unlock flow. */
    KDBXVault*
        pending_gzip_scratch_vault; /**< Preflight-staged GZip XML scratch kept across /ext approval. */
    KDBXFieldRef pending_gzip_scratch_ref; /**< Record range for the staged GZip XML scratch. */
    size_t pending_gzip_plain_size; /**< Expected plaintext XML size for the staged GZip scratch. */
    KDBXGroup* root_group; /**< Root group of the parsed KDBX model. */
    KDBXGroup* current_group; /**< Current browsing group. */
    KDBXEntry* current_entry; /**< Current entry selected through RPC or browser flows. */
    KDBXGroup* active_group; /**< Group currently shown in the browser scene. */
    KDBXEntry* active_entry; /**< Entry currently shown in detail or action scenes. */
    bool parse_failed; /**< True once XML or data-model parsing hits a handled failure. */
    bool database_loaded; /**< True if the current database was parsed successfully. */
    bool pending_vault_fallback; /**< True when RAM-backed unlock needs explicit /ext continuation approval. */
    bool allow_ext_vault_promotion; /**< True when the current RAM-first unlock may promote its session vault to /ext. */
} FlipPassSessionState;

/**
 * @brief Host-owned UI state, view models, selections, and transient strings.
 */
typedef struct {
    FileBrowser* file_browser; /**< Pointer to the FileBrowser instance. */
    FuriString* file_path; /**< Pointer to a FuriString for the selected file path. */
    FuriString* keyboard_layout_path; /**< Selected BadUSB layout path, or empty for Alt+NumPad. */
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
    uint32_t browser_selected_index; /**< Last selected browser item. */
    uint32_t action_selected_index; /**< Last selected action item. */
    uint32_t other_field_selected_index; /**< Last selected item in the other-fields list. */
    uint32_t
        other_field_action_selected_index; /**< Last selected action for the current other field. */
    FlipPassEntryAction
        pending_entry_action; /**< Entry action awaiting confirmation or execution. */
    uint32_t
        pending_other_field_mask; /**< Standard entry field selected from the other-fields flow. */
    KDBXCustomField*
        pending_other_custom_field; /**< Custom entry field selected from the other-fields flow. */
    char pending_other_field_name
        [STATUS_TITLE_SIZE]; /**< Current other-field label for prompts and status. */
    uint32_t keyboard_layout_return_scene; /**< Scene to restore if layout-assisted typing fails. */
    bool close_db_dialog_open; /**< True while the close-database confirmation dialog is visible. */
    bool close_test_logged; /**< True after the first password scene entry is logged. */
    FuriString* text_view_body; /**< Scrollable text-view payload for long notes and fields. */
    char text_view_title[STATUS_TITLE_SIZE]; /**< Title for the shared text-view scene. */
    uint32_t text_view_return_scene; /**< Scene returned to after dismissing the text view. */
    uint32_t progress_started_tick; /**< Tick when the current progress run started. */
    uint8_t progress_percent; /**< Last rendered progress percentage. */
    char progress_title[STATUS_TITLE_SIZE]; /**< Persistent progress-screen title. */
} FlipPassUiState;

typedef struct {
    FlipPassOutputTransport transport;
    FlipPassOutputAction action;
    const char* text;
    const char* username;
    const char* password;
    KDBXVault* vault;
    const KDBXFieldRef* ref;
    const KDBXFieldRef* username_ref;
    const KDBXFieldRef* password_ref;
    const KDBXEntry* entry;
} FlipPassOutputRequest;

/**
 * @struct App
 * @brief Main application structure.
 *
 * This struct holds all the state for the FlipPass application, including
 * handles to the GUI, view dispatcher, scene manager, and various views.
 */
typedef struct App {
    union {
        FlipPassHostRuntime runtime;
        struct {
            Gui* gui;
            ViewDispatcher* view_dispatcher;
            SceneManager* scene_manager;
            FuriPubSub* input_events;
            FuriPubSubSubscription* input_subscription;
            RpcAppSystem* rpc;
            FlipPassModuleLoader module_loader;
            bool usb_expect_rpc_session_close;
            bool rpc_mode;
            bool debug_auto_continue_vault_fallback;
            volatile bool typing_active;
            volatile bool typing_cancel_requested;
            volatile uint32_t typing_cancel_back_tick;
#if FLIPPASS_ENABLE_SYSTEM_TRACE_CAPTURE_EFFECTIVE
            FuriLogHandler system_log_handler;
            bool system_log_capture_enabled;
            bool system_log_capture_busy;
            size_t system_log_capture_bytes;
            size_t system_log_line_len;
            char system_log_line[FLIPPASS_SYSTEM_LOG_LINE_SIZE];
            char* system_log_ring;
            size_t system_log_ring_count;
            size_t system_log_ring_next;
            size_t system_log_ring_dropped;
            bool system_log_capture_buffered;
#endif
        };
    };
    union {
        FlipPassSessionState session;
        struct {
            KDBXArena* db_arena;
            KDBXVault* vault;
            KDBXVaultBackend active_vault_backend;
            KDBXVaultBackend requested_vault_backend;
            KDBXVault*
                pending_gzip_scratch_vault; /**< Host-owned staged open scratch kept across /ext approval. */
            KDBXFieldRef pending_gzip_scratch_ref; /**< Record range for the staged open scratch. */
            size_t pending_gzip_plain_size; /**< Plaintext byte count for the staged open scratch. */
            KDBXGroup* root_group;
            KDBXGroup* current_group;
            KDBXEntry* current_entry;
            KDBXGroup* active_group;
            KDBXEntry* active_entry;
            bool parse_failed;
            bool database_loaded;
            bool pending_vault_fallback;
            bool allow_ext_vault_promotion;
        };
    };
    union {
        FlipPassUiState ui;
        struct {
            FileBrowser* file_browser;
            FuriString* file_path;
            FuriString* keyboard_layout_path;
            TextInput* text_input;
            char text_buffer[TEXT_BUFFER_SIZE];
            char password_header[FLIPPASS_PASSWORD_HEADER_SIZE];
            char master_password[TEXT_BUFFER_SIZE];
            FlipPassProgressView* progress_view;
            FlipPassDbBrowserView* db_browser;
            Submenu* submenu;
            Widget* widget;
            DialogEx* dialog_ex;
            char status_title[STATUS_TITLE_SIZE];
            char status_message[STATUS_MESSAGE_SIZE];
            uint32_t status_return_scene;
            uint32_t browser_selected_index;
            uint32_t action_selected_index;
            uint32_t other_field_selected_index;
            uint32_t other_field_action_selected_index;
            FlipPassEntryAction pending_entry_action;
            uint32_t pending_other_field_mask;
            KDBXCustomField* pending_other_custom_field;
            char pending_other_field_name[STATUS_TITLE_SIZE];
            uint32_t keyboard_layout_return_scene;
            bool close_db_dialog_open;
            bool close_test_logged;
            FuriString* text_view_body;
            char text_view_title[STATUS_TITLE_SIZE];
            uint32_t text_view_return_scene;
            uint32_t progress_started_tick;
            uint8_t progress_percent;
            char progress_title[STATUS_TITLE_SIZE];
        };
    };
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
} AppView;

#if FLIPPASS_ENABLE_DEEP_DIAGNOSTICS && FLIPPASS_ENABLE_LOGS
#define FLIPPASS_DIAGNOSTIC_LOG(app, ...) FLIPPASS_LOG_EVENT((app), __VA_ARGS__)
#else
#define FLIPPASS_DIAGNOSTIC_LOG(app, ...) \
    do {                                  \
        UNUSED(app);                      \
    } while(0)
#endif

#if FLIPPASS_ENABLE_VERBOSE_UNLOCK_LOG && FLIPPASS_ENABLE_LOGS
#define FLIPPASS_BENCH_LOG(app, ...) FLIPPASS_LOG_EVENT((app), __VA_ARGS__)
#else
#define FLIPPASS_BENCH_LOG(app, ...) \
    do {                             \
        UNUSED(app);                 \
    } while(0)
#endif

void flippass_save_settings(App* app);
void flippass_clear_text_buffer(App* app);
void flippass_clear_master_password(App* app);
void flippass_reset_database(App* app);
void flippass_close_database(App* app);
void flippass_set_status(App* app, const char* title, const char* message);
void flippass_progress_reset(App* app);
void flippass_progress_begin(App* app, const char* title, const char* stage, uint8_t percent);
void flippass_progress_update(App* app, const char* stage, const char* detail, uint8_t percent);
void flippass_typing_begin(App* app);
void flippass_typing_end(App* app);
bool flippass_typing_should_cancel(const App* app);
bool flippass_typing_consume_pending_back(App* app);
void flippass_request_exit(App* app);
void flippass_log_reset(App* app);
void flippass_log_event(App* app, const char* format, ...);
bool flippass_secure_delete_file_with_storage(Storage* storage, const char* path);
bool flippass_secure_delete_file(const char* path);
bool flippass_secure_write_encrypted_string(
    FlipperFormat* file,
    const char* key_prefix,
    const char* value);
bool flippass_secure_read_encrypted_string(
    FlipperFormat* file,
    const char* key_prefix,
    FuriString* out_value);
void flippass_system_log_capture_suspend(void);
void flippass_system_log_capture_resume(void);
bool flippass_system_log_capture_is_suspended(void);
void flippass_module_loader_init(App* app);
void flippass_module_loader_deinit(App* app);
const FlipperAppPluginDescriptor* flippass_module_ensure(
    App* app,
    FlipPassModuleSlot slot,
    const char* path,
    const char* expected_appid,
    uint32_t expected_api_version,
    FuriString* error);
void flippass_module_unload(App* app, FlipPassModuleSlot slot);
bool flippass_open_execute(App* app, FuriString* error);
bool flippass_usb_begin(App* app);
bool flippass_usb_type_string(App* app, const char* text);
bool flippass_usb_type_login(App* app, const char* username, const char* password);
bool flippass_usb_type_autotype(App* app, const KDBXEntry* entry);
bool flippass_usb_type_key(App* app, uint16_t hid_key);
void flippass_usb_restore(App* app);
const char* flippass_output_transport_name(FlipPassOutputTransport transport);
bool flippass_output_execute_request(App* app, const FlipPassOutputRequest* request);
bool flippass_output_type_string(App* app, FlipPassOutputTransport transport, const char* text);
bool flippass_output_type_login(
    App* app,
    FlipPassOutputTransport transport,
    const char* username,
    const char* password);
bool flippass_output_type_vault_ref(
    App* app,
    FlipPassOutputTransport transport,
    KDBXVault* vault,
    const KDBXFieldRef* ref);
bool flippass_output_type_login_refs(
    App* app,
    FlipPassOutputTransport transport,
    KDBXVault* vault,
    const KDBXFieldRef* username_ref,
    const KDBXFieldRef* password_ref);
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
