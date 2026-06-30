#pragma once

#include <stddef.h>
#include <stdint.h>
#include <furi.h>
#include <furi_ble/profile_interface.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/modules/widget.h>
#include <gui/scene_manager.h>
#include <storage/storage.h>
#include <nfc/nfc.h>
#include <nfc/nfc_listener.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight_poller_sync.h>
#include <furi/core/thread.h>
#include <furi_hal_random.h>
#include <bt/bt_service/bt.h>

#define AMI_TOOL_RETAIL_KEY_SIZE (160U)
#define AMI_TOOL_RETAIL_KEY_FILENAME "key_retail.bin"
#define AMI_TOOL_GENERATE_MAX_AMIIBO_PAGE_ITEMS (32U)
#define AMI_TOOL_SAVED_MAX_PAGE_ITEMS (12U)
#define AMI_TOOL_NFC_FOLDER ANY_PATH("nfc")

typedef struct AmiToolApp AmiToolApp;

typedef struct {
    char* platform;
    char* game;
    char* usage;
    bool has_usage;
    bool writable;
} AmiToolUsageEntry;

typedef enum {
    RfidxStatusOk = 0,
    RfidxStatusDrngError,
    RfidxStatusAmiiboHmacError,
    RfidxStatusArgumentError,
} RfidxStatus;

#define RFIDX_OK RfidxStatusOk
#define RFIDX_DRNG_ERROR RfidxStatusDrngError
#define RFIDX_AMIIBO_HMAC_VALIDATION_ERROR RfidxStatusAmiiboHmacError
#define RFIDX_ARGUMENT_ERROR RfidxStatusArgumentError

#define NTAG_SIGNATURE_SIZE (32U)

typedef struct {
    uint8_t version[8];
    uint8_t tbo0[2];
    uint8_t tbo1;
    uint8_t memory_max;
    uint8_t signature[NTAG_SIGNATURE_SIZE];
    uint8_t counter0[3];
    uint8_t tearing0;
    uint8_t counter1[3];
    uint8_t tearing1;
    uint8_t counter2[3];
    uint8_t tearing2;
} Ntag21xMetadataHeader;

#pragma pack(push, 1)
typedef struct {
    uint8_t hmacKey[16];
    char typeString[14];
    uint8_t rfu;
    uint8_t magicBytesSize;
    uint8_t magicBytes[16];
    uint8_t xorTable[32];
} DumpedKeySingle;

typedef struct {
    uint8_t aesKey[16];
    uint8_t aesIV[16];
    uint8_t hmacKey[16];
} DerivedKey;

typedef struct {
    DumpedKeySingle data;
    DumpedKeySingle tag;
} DumpedKeys;
#pragma pack(pop)

/* View IDs for ViewDispatcher */
typedef enum {
    AmiToolViewMenu,
    AmiToolViewTextBox,
    AmiToolViewInfo,
} AmiToolView;

/* Scene IDs */
typedef enum {
    AmiToolSceneMainMenu,
    AmiToolSceneRead,
    AmiToolSceneGenerate,
    AmiToolSceneSaved,
    AmiToolSceneAmiiboLink,
    AmiToolSceneBluetooth,
    AmiToolSceneCount,
} AmiToolScene;

typedef enum {
    AmiToolGenerateStateRootMenu,
    AmiToolGenerateStatePlatformMenu,
    AmiToolGenerateStateGameList,
    AmiToolGenerateStateAmiiboList,
    AmiToolGenerateStateAmiiboPlaceholder,
    AmiToolGenerateStateMessage,
} AmiToolGenerateState;

typedef enum {
    AmiToolGeneratePlatform3DS,
    AmiToolGeneratePlatformWiiU,
    AmiToolGeneratePlatformSwitch,
    AmiToolGeneratePlatformSwitch2,
    AmiToolGeneratePlatformCount,
} AmiToolGeneratePlatform;

typedef enum {
    AmiToolGenerateListSourceGame,
    AmiToolGenerateListSourceName,
} AmiToolGenerateListSource;

/* Custom events from main menu */
typedef enum {
    AmiToolEventMainMenuRead,
    AmiToolEventMainMenuGenerate,
    AmiToolEventMainMenuSaved,
    AmiToolEventMainMenuAmiiboLink,
    AmiToolEventMainMenuBluetooth,
    AmiToolEventMainMenuExit,
    AmiToolEventReadSuccess,
    AmiToolEventReadWrongType,
    AmiToolEventReadError,
    AmiToolEventInfoShowActions,
    AmiToolEventInfoActionEmulate,
    AmiToolEventInfoActionUsage,
    AmiToolEventInfoActionChangeUid,
    AmiToolEventInfoActionWriteTag,
    AmiToolEventInfoActionSaveToStorage,
    AmiToolEventInfoWriteStarted,
    AmiToolEventInfoWriteSuccess,
    AmiToolEventInfoWriteFailed,
    AmiToolEventInfoWriteCancelled,
    AmiToolEventUsagePrevPage,
    AmiToolEventUsageNextPage,
    AmiToolEventAmiiboLinkWriteComplete,
    AmiToolEventBluetoothUpdate,
    AmiToolEventBluetoothGenerate,
    AmiToolEventBluetoothUidQuery,
} AmiToolCustomEvent;

typedef enum {
    AmiToolReadResultNone,
    AmiToolReadResultSuccess,
    AmiToolReadResultWrongType,
    AmiToolReadResultError,
} AmiToolReadResultType;

typedef struct {
    AmiToolReadResultType type;
    MfUltralightError error;
    MfUltralightType tag_type;
    size_t uid_len;
    uint8_t uid[10];
} AmiToolReadResult;

typedef enum {
    AmiToolRetailKeyStatusOk,
    AmiToolRetailKeyStatusNotFound,
    AmiToolRetailKeyStatusInvalidSize,
    AmiToolRetailKeyStatusStorageError,
} AmiToolRetailKeyStatus;

typedef enum {
    AmiToolWriteStatusOk,
    AmiToolWriteStatusInvalidArgs,
    AmiToolWriteStatusNoTagData,
    AmiToolWriteStatusNoUid,
    AmiToolWriteStatusOutOfRange,
    AmiToolWriteStatusNotNtag215,
    AmiToolWriteStatusUidMismatch,
    AmiToolWriteStatusIoError,
} AmiToolWriteStatus;

struct AmiToolApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;

    Storage* storage;
    uint8_t retail_key[AMI_TOOL_RETAIL_KEY_SIZE];
    size_t retail_key_size;
    bool retail_key_loaded;

    Submenu* submenu;

    // Lazy Amiibo list paging state
    size_t generate_page_offset;
    size_t generate_selected_index;
    AmiToolGenerateListSource generate_list_source;

    TextBox* text_box;
    FuriString* text_box_store;
    Widget* info_widget;
    bool main_menu_error_visible;
    bool info_actions_visible;
    bool info_action_message_visible;
    bool info_emulation_active;
    bool usage_info_visible;
    bool info_last_from_read;
    bool info_last_has_id;
    char info_last_id[17];
    FuriThread* write_thread;
    bool write_in_progress;
    bool write_cancel_requested;
    bool write_waiting_for_tag;
    char write_result_message[96];

    Nfc* nfc;
    FuriThread* read_thread;
    bool read_scene_active;
    AmiToolReadResult read_result;
    MfUltralightData* tag_data;
    bool tag_data_valid;
    MfUltralightAuthPassword tag_password;
    bool tag_password_valid;
    uint8_t tag_pack[4];
    bool tag_pack_valid;
    uint8_t last_uid[10];
    size_t last_uid_len;
    bool last_uid_valid;
    NfcListener* emulation_listener;

    size_t usage_page_index;
    size_t usage_page_count;
    size_t usage_entries_capacity;
    AmiToolUsageEntry* usage_entries;
    char* usage_raw_data;
    bool usage_nav_pending;

    AmiToolGenerateState generate_state;
    AmiToolGenerateState generate_return_state;
    AmiToolGeneratePlatform generate_platform;
    size_t generate_game_count;
    size_t generate_amiibo_count;
    size_t generate_page_entry_count;
    FuriString* generate_page_names[AMI_TOOL_GENERATE_MAX_AMIIBO_PAGE_ITEMS];
    FuriString* generate_page_ids[AMI_TOOL_GENERATE_MAX_AMIIBO_PAGE_ITEMS];
    FuriString* generate_selected_game;
    size_t saved_page_offset;
    size_t saved_page_entry_count;
    bool saved_has_next_page;
    bool saved_info_visible;
    FuriString* saved_page_display[AMI_TOOL_SAVED_MAX_PAGE_ITEMS];
    FuriString* saved_page_paths[AMI_TOOL_SAVED_MAX_PAGE_ITEMS];
    FuriString* saved_page_ids[AMI_TOOL_SAVED_MAX_PAGE_ITEMS];

    bool amiibo_link_active;
    bool amiibo_link_waiting_for_completion;
    uint32_t amiibo_link_initial_hash;
    uint32_t amiibo_link_last_hash;
    uint32_t amiibo_link_last_change_tick;
    bool amiibo_link_completion_pending;
    uint8_t amiibo_link_current_auth0;
    uint8_t amiibo_link_pending_auth0;
    bool amiibo_link_auth0_override_active;
    bool amiibo_link_access_snapshot_valid;
    uint8_t amiibo_link_access_snapshot[MF_ULTRALIGHT_PAGE_SIZE];
    bool amiibo_link_completion_marker_valid;
    uint8_t amiibo_link_completion_marker[MF_ULTRALIGHT_PAGE_SIZE];

    Bt* bt;
    FuriHalBleProfileBase* bt_serial_profile;
    bool bt_connected;
    FuriMutex* bt_mutex;
    char bt_display_text[256];
    bool bt_pending_generate;
    char bt_pending_generate_id[17];
};

/* Scene handlers table */
extern const SceneManagerHandlers ami_tool_scene_handlers;

/* Allocation / free */
AmiToolApp* ami_tool_alloc(void);
void ami_tool_free(AmiToolApp* app);
void ami_tool_generate_clear_amiibo_cache(AmiToolApp* app);
void ami_tool_info_show_page(AmiToolApp* app, const char* id_hex, bool from_read);
void ami_tool_info_show_actions_menu(AmiToolApp* app);
void ami_tool_info_show_action_message(AmiToolApp* app, const char* message);
bool ami_tool_info_start_emulation(AmiToolApp* app);
void ami_tool_info_stop_emulation(AmiToolApp* app);
bool ami_tool_info_change_uid(AmiToolApp* app);
bool ami_tool_info_write_to_tag(AmiToolApp* app);
bool ami_tool_info_save_to_storage(AmiToolApp* app);
bool ami_tool_info_show_usage(AmiToolApp* app);
void ami_tool_info_refresh_current_page(AmiToolApp* app);
void ami_tool_info_navigate_usage(AmiToolApp* app, int32_t delta);
bool ami_tool_info_get_name_for_id(AmiToolApp* app, const char* id_hex, FuriString* out_name);
void ami_tool_info_handle_write_event(AmiToolApp* app, AmiToolCustomEvent event);
bool ami_tool_info_request_write_cancel(AmiToolApp* app);
void ami_tool_info_abort_write(AmiToolApp* app);
bool ami_tool_compute_password_from_uid(
    const uint8_t* uid,
    size_t uid_len,
    MfUltralightAuthPassword* password);
void ami_tool_store_uid(AmiToolApp* app, const uint8_t* uid, size_t len);
void ami_tool_clear_cached_tag(AmiToolApp* app);
AmiToolRetailKeyStatus ami_tool_load_retail_key(AmiToolApp* app);
bool ami_tool_has_retail_key(const AmiToolApp* app);
bool ami_tool_extract_amiibo_id(
    const MfUltralightData* tag_data,
    char* buffer,
    size_t buffer_size);
const char* ami_tool_write_status_to_string(AmiToolWriteStatus status);
AmiToolWriteStatus ami_tool_write_custom_sequence(
    AmiToolApp* app,
    MfUltralightError* io_error,
    uint16_t* failed_page);

RfidxStatus amiibo_derive_key(
    const DumpedKeySingle* input_key,
    const MfUltralightData* tag_data,
    DerivedKey* derived_key);
RfidxStatus amiibo_cipher(const DerivedKey* data_key, MfUltralightData* tag_data);
RfidxStatus amiibo_generate_signature(
    const DerivedKey* tag_key,
    const DerivedKey* data_key,
    const MfUltralightData* tag_data,
    uint8_t* tag_hash,
    uint8_t* data_hash);
RfidxStatus amiibo_validate_signature(
    const DerivedKey* tag_key,
    const DerivedKey* data_key,
    const MfUltralightData* tag_data);
RfidxStatus amiibo_sign_payload(
    const DerivedKey* tag_key,
    const DerivedKey* data_key,
    MfUltralightData* tag_data);
RfidxStatus amiibo_format_dump(MfUltralightData* tag_data, Ntag21xMetadataHeader* header);
RfidxStatus amiibo_change_uid(MfUltralightData* tag_data);
RfidxStatus amiibo_set_uid(MfUltralightData* tag_data, const uint8_t* uid, size_t uid_len);
void amiibo_configure_rf_interface(MfUltralightData* tag_data);
RfidxStatus amiibo_generate(
    const uint8_t* uuid,
    MfUltralightData* tag_data,
    Ntag21xMetadataHeader* header);
RfidxStatus amiibo_prepare_blank_tag(MfUltralightData* tag_data);
