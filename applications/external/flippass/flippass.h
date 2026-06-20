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
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <notification/notification_messages.h>
#include <rpc/rpc_app.h>
#include <storage/storage.h>

#include "kdbx/kdbx_arena.h"
#include "kdbx/kdbx_data.h"
#include "kdbx/kdbx_vault.h"
#include "flippass_otp.h"
#include "flippass_password_gen.h"
#include "mntminput/text_input.h"

#define TAG "FlipPass"
#define FLIPPASS_ENABLE_SYSTEM_TRACE_CAPTURE_EFFECTIVE \
    (FLIPPASS_ENABLE_LOGS && FLIPPASS_ENABLE_SYSTEM_TRACE_CAPTURE)

#define FLIPPASS_CONFIG_FILE_PATH               EXT_PATH("apps_data/flippass/flippass.conf")
#define FLIPPASS_LOG_FILE_PATH                  EXT_PATH("apps_data/flippass/flippass.log")
#define FLIPPASS_DEBUG_UNLOCK_FILE_PATH         EXT_PATH("apps_data/flippass/debug_unlock.txt")
#define FLIPPASS_DEBUG_CREATE_FILE_PATH         EXT_PATH("apps_data/flippass/debug_empty_create.kdbx")
#define FLIPPASS_DEBUG_SAVE_TRIGGER_FILE_PATH   EXT_PATH("apps_data/flippass/debug_save.txt")
#define FLIPPASS_SYSTEM_LOG_FILE_PATH           EXT_PATH("apps_data/flippass/system_trace.log")
#define FLIPPASS_SYSTEM_LOG_ENABLE_FILE_PATH    EXT_PATH("apps_data/flippass/system_trace.enable")
#define FLIPPASS_BADUSB_LAYOUT_DIR              EXT_PATH("badusb/assets/layouts")
#define FLIPPASS_BADUSB_LAYOUT_EXT              ".kl"
#define FLIPPASS_BADUSB_SETTINGS_FILE_PATH      EXT_PATH("badusb/.badusb.settings")
#define FLIPPASS_BADUSB_SETTINGS_DEFAULT_LAYOUT FLIPPASS_BADUSB_LAYOUT_DIR "/en-US.kl"
#define FLIPPASS_KEYBOARD_LAYOUT_ALT            "alt-numpad"
#define FLIPPASS_KEYBOARD_LAYOUT_PATH_SIZE      96U
#define FLIPPASS_DEFAULT_IDLE_LOCK_MINUTES      3U
#define FLIPPASS_DEFAULT_IDLE_EXIT_MINUTES      15U
#define FLIPPASS_DEFAULT_IDLE_UNLOCK_ATTEMPTS   5U
#define FLIPPASS_OTP_TIME_ZONE_MIN_MINUTES      (-12 * 60)
#define FLIPPASS_OTP_TIME_ZONE_MAX_MINUTES      (14 * 60)
#define FLIPPASS_OTP_TIME_ZONE_STEP_MINUTES     30
#define FLIPPASS_OTP_TIME_ZONE_COUNT                                              \
    (((FLIPPASS_OTP_TIME_ZONE_MAX_MINUTES - FLIPPASS_OTP_TIME_ZONE_MIN_MINUTES) / \
      FLIPPASS_OTP_TIME_ZONE_STEP_MINUTES) +                                      \
     1)
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

#define TEXT_BUFFER_SIZE                     256
#define STATUS_TITLE_SIZE                    64
#define STATUS_MESSAGE_SIZE                  256
#define FLIPPASS_RPC_BUFFER_SIZE             512
#define FLIPPASS_PASSWORD_HEADER_SIZE        96
#define FLIPPASS_SYSTEM_LOG_LINE_SIZE        384
#define FLIPPASS_SYSTEM_LOG_RING_LINES       8
#define FLIPPASS_SYSTEM_LOG_RING_LINE_SIZE   128
#define FLIPPASS_SECURE_VALUE_NONCE_SIZE     16U
#define FLIPPASS_SECURE_VALUE_MAC_SIZE       32U
#define FLIPPASS_SESSION_SECRET_SIZE         32U
#define FLIPPASS_SESSION_WRAP_IV_SIZE        16U
#define FLIPPASS_FORM_VALUE_SIZE             256U
#define FLIPPASS_FILE_NAME_SIZE              64U
#define FLIPPASS_KDBX_DEFAULT_AES_KDF_ROUNDS 600000ULL
#define FLIPPASS_KDBX_AES_KDF_ROUND_STEP     10000ULL
#define FLIPPASS_KDBX_MIN_AES_KDF_ROUNDS     10000ULL
#define FLIPPASS_KDBX_AES_KDF_UI_VALUES      255U

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
    FlipPassKdbxCipherAes256 = 0,
    FlipPassKdbxCipherChaCha20,
} FlipPassKdbxCipher;

typedef enum {
    FlipPassModuleSlotOutputUsb = 0,
    FlipPassModuleSlotOutputBle,
    FlipPassModuleSlotOutputAction,
    FlipPassModuleSlotOtherFields,
    FlipPassModuleSlotFileOps,
    FlipPassModuleSlotEditorCrud,
    FlipPassModuleSlotRpcCommands,
    FlipPassModuleSlotOpenAcquire,
    FlipPassModuleSlotOpenStream,
    FlipPassModuleSlotOpenInflateNonPaged,
    FlipPassModuleSlotOpenInflatePaged,
    FlipPassModuleSlotOpenModel,
    FlipPassModuleSlotSaveHeader,
    FlipPassModuleSlotSaveWriter,
    FlipPassModuleSlotKeyboardLayout,
    FlipPassModuleSlotOtp,
    FlipPassModuleSlotPasswordGen,
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
typedef struct FlipPassEditorCustomFieldDraft FlipPassEditorCustomFieldDraft;

struct FlipPassEditorCustomFieldDraft {
    char* name;
    char* value;
    bool protected_value;
    FlipPassEditorCustomFieldDraft* next;
};

typedef enum {
    FlipPassEditorModeNone = 0,
    FlipPassEditorModeNewDatabase,
    FlipPassEditorModeNewDirectory,
    FlipPassEditorModeModifyDatabase,
    FlipPassEditorModeAddGroup,
    FlipPassEditorModeEditGroup,
    FlipPassEditorModeAddEntry,
    FlipPassEditorModeEditEntry,
    FlipPassEditorModeEditOtp,
    FlipPassEditorModeRenameFile,
    FlipPassEditorModeAddCustomField,
    FlipPassEditorModeEditCustomField,
    FlipPassEditorModeGlobalConfig,
} FlipPassEditorMode;

typedef enum {
    FlipPassEditorTextTargetNone = 0,
    FlipPassEditorTextTargetFileName,
    FlipPassEditorTextTargetDatabasePassword,
    FlipPassEditorTextTargetGroupName,
    FlipPassEditorTextTargetEntryTitle,
    FlipPassEditorTextTargetEntryUsername,
    FlipPassEditorTextTargetEntryPassword,
    FlipPassEditorTextTargetEntryUrl,
    FlipPassEditorTextTargetEntryNotes,
    FlipPassEditorTextTargetEntryAutotype,
    FlipPassEditorTextTargetCustomFieldName,
    FlipPassEditorTextTargetCustomFieldValue,
    FlipPassEditorTextTargetOtpSecret,
    FlipPassEditorTextTargetOtpCounter,
} FlipPassEditorTextTarget;

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
    DateTime
        last_interaction_datetime; /**< Most recent wall-clock user interaction captured by the app. */
    uint32_t
        last_interaction_tick; /**< Monotonic tick captured alongside the last interaction wall clock. */
    bool idle_lock_active; /**< True while an idle re-auth prompt is blocking the unlocked session. */
    uint8_t idle_lock_failed_attempts; /**< Failed password attempts during the current idle lock. */
    uint16_t idle_lock_minutes; /**< Inactivity minutes before Unlock Session, or 0 to disable. */
    uint8_t idle_unlock_attempts; /**< Failed Unlock Session attempts before closing the session. */
    uint16_t idle_exit_minutes; /**< Inactivity minutes before app exit, or 0 to disable. */
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
    FlipPassKdbxCipher database_cipher; /**< Outer cipher used for the next save. */
    uint32_t database_compression; /**< Compression policy used for the next save. */
    uint64_t database_kdf_rounds; /**< AES-KDF rounds used for the next save. */
    uint8_t session_key_iv
        [FLIPPASS_SESSION_WRAP_IV_SIZE]; /**< IV for the enclave-wrapped ephemeral session key. */
    uint8_t session_key_cipher
        [FLIPPASS_SESSION_SECRET_SIZE]; /**< Ephemeral session key encrypted with enclave slot 11. */
    uint8_t database_save_key_nonce
        [FLIPPASS_SECURE_VALUE_NONCE_SIZE]; /**< Nonce for the session-sealed KDBX credential. */
    uint8_t database_save_key_cipher
        [FLIPPASS_SESSION_SECRET_SIZE]; /**< Composite KDBX credential sealed by the session key. */
    uint8_t database_save_key_mac
        [FLIPPASS_SECURE_VALUE_MAC_SIZE]; /**< MAC for the sealed KDBX credential. */
    uint8_t database_save_transformed_key_nonce
        [FLIPPASS_SECURE_VALUE_NONCE_SIZE]; /**< Nonce for the session-sealed AES-KDF transformed key. */
    uint8_t database_save_transformed_key_cipher
        [FLIPPASS_SESSION_SECRET_SIZE]; /**< AES-KDF transformed key sealed by the session key. */
    uint8_t database_save_transformed_key_mac
        [FLIPPASS_SECURE_VALUE_MAC_SIZE]; /**< MAC for the sealed AES-KDF transformed key. */
    uint8_t database_save_kdf_salt[32]; /**< AES-KDF salt paired with the sealed transformed key. */
    uint64_t database_save_kdf_rounds; /**< AES-KDF rounds paired with the sealed transformed key. */
    bool database_save_key_ready; /**< True when the sealed database credential can be unwrapped. */
    bool database_save_transformed_key_ready; /**< True when a normal save can skip AES-KDF derivation. */
    bool parse_failed; /**< True once XML or data-model parsing hits a handled failure. */
    bool database_loaded; /**< True if the current database was parsed successfully. */
    bool database_dirty; /**< True once the unlocked session diverges from disk. */
    bool database_new; /**< True when the current session has not been saved yet. */
    bool pending_vault_fallback; /**< True when RAM-backed unlock needs explicit /ext continuation approval. */
    bool allow_ext_vault_promotion; /**< True when the current RAM-first unlock may promote its session vault to /ext. */
} FlipPassSessionState;

/**
 * @brief Host-owned UI state, view models, selections, and transient strings.
 */
typedef struct {
    FileBrowser* file_browser; /**< Pointer to the FileBrowser instance. */
    FuriString* file_path; /**< Pointer to a FuriString for the selected file path. */
    FuriString* browser_directory; /**< Active directory shown by the custom file browser. */
    FuriString* pending_path; /**< Temporary path used by browser and save flows. */
    FuriString* keyboard_layout_path; /**< Effective BadUSB layout path, or empty for Alt+NumPad. */
    FuriString*
        last_open_file_path; /**< Last successfully opened database path persisted in settings. */
    uint32_t last_open_count; /**< Successful open count for the persisted last-open path. */
    bool keyboard_layout_configured; /**< True once FlipPass owns a persisted layout choice. */
    int16_t otp_time_zone_minutes; /**< Global UTC correction, in minutes, applied to TIMEOTP. */
    bool always_allow_ext; /**< True when RAM-first unlock may promote to /ext without a prompt. */
    TextInput* text_input; /**< Pointer to the TextInput instance. */
    char text_buffer[TEXT_BUFFER_SIZE]; /**< Buffer for text input. */
    char password_header[FLIPPASS_PASSWORD_HEADER_SIZE]; /**< Persistent password-entry header text. */
    char master_password[TEXT_BUFFER_SIZE]; /**< Buffer holding the active password input. */
    FlipPassProgressView* progress_view; /**< Shared progress view for unlock and typing work. */
    FlipPassDbBrowserView* db_browser; /**< Custom browser view for database groups and entries. */
    VariableItemList* variable_item_list; /**< Shared form editor view. */
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
    FlipPassOtpKind
        pending_other_otp_kind; /**< Synthetic OTP field selected from the other-fields flow. */
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
    FlipPassEditorMode editor_mode; /**< Active editor intent for the shared form scene. */
    FlipPassEditorMode
        editor_parent_mode; /**< Parent entry editor restored after custom-field forms. */
    FlipPassEditorTextTarget editor_text_target; /**< Field currently edited through text input. */
    KDBXGroup* editor_group; /**< Group currently edited or used as creation parent. */
    KDBXEntry* editor_entry; /**< Entry currently edited. */
    uint32_t editor_selected_index; /**< Last selected row in the shared editor scene. */
    uint32_t editor_return_scene; /**< Scene restored when the shared editor is canceled. */
    uint16_t editor_idle_lock_minutes; /**< Draft global inactivity-lock timeout. */
    uint8_t editor_idle_unlock_attempts; /**< Draft Unlock Session attempt limit. */
    uint16_t editor_idle_exit_minutes; /**< Draft global inactivity-exit timeout. */
    bool editor_always_allow_ext; /**< Draft global /ext promotion approval. */
    uint32_t editor_keyboard_layout_index; /**< Draft keyboard-layout item index. */
    bool editor_keyboard_layout_use_alt; /**< Draft keyboard layout uses Alt+NumPad. */
    bool editor_keyboard_layout_available; /**< True when config loaded layout choices. */
    char editor_keyboard_layout_path[FLIPPASS_KEYBOARD_LAYOUT_PATH_SIZE]; /**< Draft layout path. */
    uint32_t browser_directory_selected_index; /**< Last selected row in the custom file browser. */
    uint32_t browser_menu_selected_index; /**< Last selected database file menu action. */
    bool editor_close_after_commit; /**< True when a successful editor commit should close the current database. */
    char editor_file_name
        [FLIPPASS_FILE_NAME_SIZE]; /**< File name buffer for create and rename flows. */
    char editor_database_password
        [FLIPPASS_FORM_VALUE_SIZE]; /**< Save password buffer for create and modify flows. */
    char editor_group_name[FLIPPASS_FORM_VALUE_SIZE]; /**< Shared group-name editor buffer. */
    char editor_entry_title[FLIPPASS_FORM_VALUE_SIZE]; /**< Entry title editor buffer. */
    char editor_entry_username[FLIPPASS_FORM_VALUE_SIZE]; /**< Entry username editor buffer. */
    char editor_entry_password[FLIPPASS_FORM_VALUE_SIZE]; /**< Entry password editor buffer. */
    char editor_entry_url[FLIPPASS_FORM_VALUE_SIZE]; /**< Entry URL editor buffer. */
    char editor_entry_notes[FLIPPASS_FORM_VALUE_SIZE]; /**< Entry notes editor buffer. */
    char editor_entry_autotype[FLIPPASS_FORM_VALUE_SIZE]; /**< Entry AutoType editor buffer. */
    FlipPassEditorCustomFieldDraft*
        editor_custom_fields; /**< Draft custom fields for an unsaved new entry. */
    FlipPassEditorCustomFieldDraft*
        editor_custom_field_draft; /**< Draft field currently being edited. */
    KDBXCustomField* editor_custom_field; /**< Existing custom field currently being edited. */
    bool editor_custom_field_protected; /**< Current custom-field Protected form value. */
    char editor_custom_field_name[FLIPPASS_FORM_VALUE_SIZE]; /**< Custom-field name editor buffer. */
    char editor_custom_field_value[FLIPPASS_FORM_VALUE_SIZE]; /**< Custom-field value editor buffer. */
    FlipPassOtpKind editor_otp_kind; /**< OTP type currently shown in the editor OTP subform. */
    FlipPassOtpSecretEncoding editor_otp_secret_encoding; /**< Current OTP secret encoding. */
    FlipPassOtpAlgorithm editor_otp_algorithm; /**< Current TIMEOTP algorithm. */
    uint8_t editor_otp_digits; /**< Current TIMEOTP digit count. */
    uint32_t editor_otp_period; /**< Current TIMEOTP period in seconds. */
    int16_t editor_otp_time_zone_minutes; /**< Draft global TIMEOTP UTC correction in minutes. */
    bool editor_otp_settled; /**< True when the edited entry already has an OTP cluster. */
    char editor_otp_secret[FLIPPASS_FORM_VALUE_SIZE]; /**< OTP secret editor buffer. */
    char editor_otp_counter[FLIPPASS_OTP_COUNTER_TEXT_SIZE]; /**< HMACOTP counter editor buffer. */
    FlipPassPasswordGenTarget
        password_gen_target; /**< Secret field receiving a generated password. */
    FlipPassPasswordGenCharset
        password_gen_charset; /**< Current password generator character set. */
    uint16_t password_gen_length; /**< Requested generated password length. */
    uint16_t password_gen_harvest_seconds; /**< Requested entropy harvest duration. */
    uint32_t password_gen_selected_index; /**< Last selected row in the generator form. */
    uint32_t password_gen_started_tick; /**< Tick when timed entropy capture began. */
    bool password_gen_capture_active; /**< True while the entropy harvest screen records input. */
    bool password_gen_auto_open_field_name; /**< True when a new custom field should edit Name first. */
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
            DateTime last_interaction_datetime;
            uint32_t last_interaction_tick;
            bool idle_lock_active;
            uint8_t idle_lock_failed_attempts;
            uint16_t idle_lock_minutes;
            uint8_t idle_unlock_attempts;
            uint16_t idle_exit_minutes;
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
            FlipPassKdbxCipher database_cipher;
            uint32_t database_compression;
            uint64_t database_kdf_rounds;
            uint8_t session_key_iv[FLIPPASS_SESSION_WRAP_IV_SIZE];
            uint8_t session_key_cipher[FLIPPASS_SESSION_SECRET_SIZE];
            uint8_t database_save_key_nonce[FLIPPASS_SECURE_VALUE_NONCE_SIZE];
            uint8_t database_save_key_cipher[FLIPPASS_SESSION_SECRET_SIZE];
            uint8_t database_save_key_mac[FLIPPASS_SECURE_VALUE_MAC_SIZE];
            uint8_t database_save_transformed_key_nonce[FLIPPASS_SECURE_VALUE_NONCE_SIZE];
            uint8_t database_save_transformed_key_cipher[FLIPPASS_SESSION_SECRET_SIZE];
            uint8_t database_save_transformed_key_mac[FLIPPASS_SECURE_VALUE_MAC_SIZE];
            uint8_t database_save_kdf_salt[32];
            uint64_t database_save_kdf_rounds;
            bool database_save_key_ready;
            bool database_save_transformed_key_ready;
            bool parse_failed;
            bool database_loaded;
            bool database_dirty;
            bool database_new;
            bool pending_vault_fallback;
            bool allow_ext_vault_promotion;
        };
    };
    union {
        FlipPassUiState ui;
        struct {
            FileBrowser* file_browser;
            FuriString* file_path;
            FuriString* browser_directory;
            FuriString* pending_path;
            FuriString* keyboard_layout_path;
            FuriString* last_open_file_path;
            uint32_t last_open_count;
            bool keyboard_layout_configured;
            int16_t otp_time_zone_minutes;
            bool always_allow_ext;
            TextInput* text_input;
            char text_buffer[TEXT_BUFFER_SIZE];
            char password_header[FLIPPASS_PASSWORD_HEADER_SIZE];
            char master_password[TEXT_BUFFER_SIZE];
            FlipPassProgressView* progress_view;
            FlipPassDbBrowserView* db_browser;
            VariableItemList* variable_item_list;
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
            FlipPassOtpKind pending_other_otp_kind;
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
            FlipPassEditorMode editor_mode;
            FlipPassEditorMode editor_parent_mode;
            FlipPassEditorTextTarget editor_text_target;
            KDBXGroup* editor_group;
            KDBXEntry* editor_entry;
            uint32_t editor_selected_index;
            uint32_t editor_return_scene;
            uint16_t editor_idle_lock_minutes;
            uint8_t editor_idle_unlock_attempts;
            uint16_t editor_idle_exit_minutes;
            bool editor_always_allow_ext;
            uint32_t editor_keyboard_layout_index;
            bool editor_keyboard_layout_use_alt;
            bool editor_keyboard_layout_available;
            char editor_keyboard_layout_path[FLIPPASS_KEYBOARD_LAYOUT_PATH_SIZE];
            uint32_t browser_directory_selected_index;
            uint32_t browser_menu_selected_index;
            bool editor_close_after_commit;
            char editor_file_name[FLIPPASS_FILE_NAME_SIZE];
            char editor_database_password[FLIPPASS_FORM_VALUE_SIZE];
            char editor_group_name[FLIPPASS_FORM_VALUE_SIZE];
            char editor_entry_title[FLIPPASS_FORM_VALUE_SIZE];
            char editor_entry_username[FLIPPASS_FORM_VALUE_SIZE];
            char editor_entry_password[FLIPPASS_FORM_VALUE_SIZE];
            char editor_entry_url[FLIPPASS_FORM_VALUE_SIZE];
            char editor_entry_notes[FLIPPASS_FORM_VALUE_SIZE];
            char editor_entry_autotype[FLIPPASS_FORM_VALUE_SIZE];
            FlipPassEditorCustomFieldDraft* editor_custom_fields;
            FlipPassEditorCustomFieldDraft* editor_custom_field_draft;
            KDBXCustomField* editor_custom_field;
            bool editor_custom_field_protected;
            char editor_custom_field_name[FLIPPASS_FORM_VALUE_SIZE];
            char editor_custom_field_value[FLIPPASS_FORM_VALUE_SIZE];
            FlipPassOtpKind editor_otp_kind;
            FlipPassOtpSecretEncoding editor_otp_secret_encoding;
            FlipPassOtpAlgorithm editor_otp_algorithm;
            uint8_t editor_otp_digits;
            uint32_t editor_otp_period;
            int16_t editor_otp_time_zone_minutes;
            bool editor_otp_settled;
            char editor_otp_secret[FLIPPASS_FORM_VALUE_SIZE];
            char editor_otp_counter[FLIPPASS_OTP_COUNTER_TEXT_SIZE];
            FlipPassPasswordGenTarget password_gen_target;
            FlipPassPasswordGenCharset password_gen_charset;
            uint16_t password_gen_length;
            uint16_t password_gen_harvest_seconds;
            uint32_t password_gen_selected_index;
            uint32_t password_gen_started_tick;
            bool password_gen_capture_active;
            bool password_gen_auto_open_field_name;
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
    AppViewVariableItemList, /**< Shared form editor view. */
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

#if FLIPPASS_ENABLE_MEMORY_DIAGNOSTICS && FLIPPASS_ENABLE_LOGS
#define FLIPPASS_MEMORY_LOG(app, stage, theoretical_bytes) \
    flippass_memory_log((app), (stage), (theoretical_bytes))
#define FLIPPASS_MEMORY_LOG_MODULE(app, stage, slot, theoretical_bytes) \
    flippass_memory_log_module((app), (stage), (slot), (theoretical_bytes))
#else
#define FLIPPASS_MEMORY_LOG(app, stage, theoretical_bytes) \
    do {                                                   \
        UNUSED(app);                                       \
        UNUSED(stage);                                     \
        UNUSED(theoretical_bytes);                         \
    } while(0)
#define FLIPPASS_MEMORY_LOG_MODULE(app, stage, slot, theoretical_bytes) \
    do {                                                                \
        UNUSED(app);                                                    \
        UNUSED(stage);                                                  \
        UNUSED(slot);                                                   \
        UNUSED(theoretical_bytes);                                      \
    } while(0)
#endif

void flippass_save_settings(App* app);
void flippass_clear_text_buffer(App* app);
void flippass_clear_master_password(App* app);
void flippass_make_password_composite_key(const char* password, uint8_t out_key[32]);
void flippass_session_clear_credentials(App* app);
bool flippass_session_store_save_key(App* app, const uint8_t save_key[32]);
bool flippass_session_store_save_material(
    App* app,
    const uint8_t save_key[32],
    const uint8_t* transformed_key,
    const uint8_t* kdf_salt,
    uint64_t kdf_rounds);
bool flippass_session_copy_save_key(App* app, uint8_t out_key[32]);
bool flippass_session_copy_save_transformed_key(
    App* app,
    uint8_t out_key[32],
    uint8_t out_kdf_salt[32],
    uint64_t* out_kdf_rounds);
bool flippass_session_verify_password(App* app, const char* password);
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
void flippass_record_successful_open(App* app);
void flippass_request_exit(App* app);
void flippass_log_reset(App* app);
void flippass_log_event(App* app, const char* format, ...);
void flippass_memory_log(App* app, const char* stage, size_t theoretical_bytes);
void flippass_memory_log_module(
    App* app,
    const char* stage,
    FlipPassModuleSlot slot,
    size_t theoretical_bytes);
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
const char* flippass_module_slot_name(FlipPassModuleSlot slot);
const FlipperAppPluginDescriptor* flippass_module_ensure(
    App* app,
    FlipPassModuleSlot slot,
    const char* path,
    const char* expected_appid,
    uint32_t expected_api_version,
    FuriString* error);
void flippass_module_unload(App* app, FlipPassModuleSlot slot);
bool flippass_open_execute(App* app, FuriString* error);
bool flippass_save_execute(
    App* app,
    const char* target_path,
    const char* password,
    FlipPassKdbxCipher cipher,
    uint32_t compression,
    uint64_t kdf_rounds,
    FuriString* error);
bool flippass_save_current_database(
    App* app,
    const char* target_path,
    const char* password,
    FuriString* error);
#if FLIPPASS_ENABLE_DEBUG_SAVE_HOOK
bool flippass_debug_save_after_open(App* app, FuriString* error);
#endif
void flippass_db_mark_clean(App* app);
void flippass_db_mark_dirty(App* app);
bool flippass_db_create_new_database(
    App* app,
    const char* root_name,
    FlipPassKdbxCipher cipher,
    uint32_t compression,
    FuriString* error);
bool flippass_db_create_group(
    App* app,
    KDBXGroup* parent,
    const char* name,
    KDBXGroup** out_group,
    FuriString* error);
bool flippass_db_update_group(App* app, KDBXGroup* group, const char* name, FuriString* error);
bool flippass_db_create_entry(
    App* app,
    KDBXGroup* parent,
    const char* title,
    const char* username,
    const char* password,
    const char* url,
    const char* notes,
    const char* autotype,
    KDBXEntry** out_entry,
    FuriString* error);
bool flippass_db_update_entry(
    App* app,
    KDBXEntry* entry,
    const char* title,
    const char* username,
    const char* password,
    const char* url,
    const char* notes,
    const char* autotype,
    FuriString* error);
bool flippass_db_delete_group(App* app, KDBXGroup* group, FuriString* error);
bool flippass_db_delete_entry(App* app, KDBXEntry* entry, FuriString* error);
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
bool flippass_output_usb_is_connected(const App* app);
bool flippass_output_bluetooth_is_advertising(const App* app);
bool flippass_output_bluetooth_advertise(App* app);
bool flippass_output_prewarm_transport(App* app, FlipPassOutputTransport transport);
void flippass_output_bluetooth_get_name(char* buffer, size_t size);
void flippass_output_cleanup_transport(App* app, FlipPassOutputTransport transport);
void flippass_output_cleanup(App* app);
