#pragma once

// Flat interface for foreign runtimes.
//
// Everything here is a fixed layout or an opaque handle, so a binding built
// against one release keeps working whatever features or pool sizes the
// library was compiled with. The model structs carry every field of every
// generation; a build that omits a feature reports it as unsupported when a
// value would need it, rather than dropping it.
//
// Handles are not thread safe. One handle may be used from any thread, but only
// from one at a time.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#if defined(DFC_FFI_BUILD)
#define DFC_FFI_EXPORT __declspec(dllexport)
#else
#define DFC_FFI_EXPORT __declspec(dllimport)
#endif
#else
#define DFC_FFI_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Raised whenever a struct below or a function's meaning changes.
#define DFC_FFI_ABI_VERSION 3

// Failures every function may report, alongside its own family's statuses:
// credential functions return a DfcDerStatus, virtual-PICC functions a
// DfcVirtualPiccStatus, reader functions a DfcReaderStatus.
#define DFC_FFI_OK 0
#define DFC_FFI_INVALID_ARGUMENT (-1)
#define DFC_FFI_BUFFER_TOO_SMALL (-2)
#define DFC_FFI_OUT_OF_MEMORY    (-3)
#define DFC_FFI_UNSUPPORTED      (-4)

#define DFC_FFI_MESSAGE_MAX 96

typedef struct DfcFfiCredential DfcFfiCredential;
typedef struct DfcFfiPicc DfcFfiPicc;
typedef struct DfcFfiReaderSession DfcFfiReaderSession;
typedef struct DfcFfiReaderExchange DfcFfiReaderExchange;

// ------------------------------------------------------------ library ---

typedef struct {
    uint32_t abi_version;
    uint32_t build_role;
    uint32_t build_profile;
    // DfcBuildCapabilities, one octet per flag, in its declaration order.
    uint8_t generation_ev1;
    uint8_t generation_ev2;
    uint8_t generation_ev3;
    uint8_t storage_2k;
    uint8_t storage_4k;
    uint8_t storage_8k;
    uint8_t ev2_secure_messaging;
    uint8_t iso7816;
    uint8_t transaction_mac;
    uint8_t secure_dynamic_messaging;
    uint8_t proximity_check;
    uint8_t virtual_card;
    uint8_t key_sets;
    uint8_t delegated_applications;
    uint8_t transaction_timer;
    uint8_t application_capability_data;
    uint8_t static_signature;
    uint8_t auth_d40;
    uint8_t auth_iso;
    uint8_t auth_aes;
    uint8_t emulator;
    uint8_t reader;
    uint8_t der_decoder;
    uint8_t der_encoder;
    uint8_t text_codec;
    // Capacity of the credential model.
    uint32_t max_applications;
    uint32_t max_files;
    uint32_t max_keys;
    uint32_t file_pool_size;
    uint32_t key_pool_size;
} DfcFfiCapabilities;

DFC_FFI_EXPORT uint32_t dfc_ffi_abi_version(void);
// Uses the platform generator directly, even while a scripted PICC callback is active.
DFC_FFI_EXPORT int32_t dfc_ffi_random_fill(uint8_t* out, size_t len);
DFC_FFI_EXPORT bool dfc_ffi_fixed_time_equal(
    const uint8_t* left, const uint8_t* right, size_t len);
DFC_FFI_EXPORT void dfc_ffi_secure_zero(void* buffer, size_t len);
DFC_FFI_EXPORT void dfc_ffi_capabilities(DfcFfiCapabilities* out);

// Sizes of the structs below as this library lays them out, for a binding to
// check against its own declarations before it trusts them.
typedef struct {
    uint32_t capabilities;
    uint32_t error;
    uint32_t card;
    uint32_t picc;
    uint32_t application;
    uint32_t file;
    uint32_t key;
    uint32_t activation;
    uint32_t snapshot;
    uint32_t reader_state;
} DfcFfiStructSizes;

DFC_FFI_EXPORT void dfc_ffi_struct_sizes(DfcFfiStructSizes* out);

// Random source a virtual PICC draws from. It must fill `buffer` with `len`
// unpredictable octets. When a PICC has no callback, the operating system's
// generator is used.
typedef void (*DfcFfiRandomCallback)(void* context, uint8_t* buffer, size_t len);

// ------------------------------------------------------------ credential ---

typedef struct {
    // A DfcDerStatus class, or one of the DFC_FFI_* failures.
    int32_t status;
    // 1-based line of a text failure; 0 when the fault is not on one line.
    uint32_t line;
    char message[DFC_FFI_MESSAGE_MAX];
} DfcFfiError;

typedef struct {
    uint32_t generation;
    uint32_t storage;
    uint32_t uid_provenance;
    uint32_t uid_len;
    uint8_t uid[10];
    uint8_t has_hardware_version;
    uint8_t hardware_version[7];
    uint8_t has_software_version;
    uint8_t software_version[7];
    uint8_t has_static_signature;
    uint8_t static_signature[56];
} DfcFfiCard;

typedef struct {
    uint32_t num_keys;
    uint32_t key_len;
    uint8_t key_settings_1;
    uint8_t key_settings_2;
    uint8_t auth_command;
    uint8_t random_id;
    uint8_t format_disabled;
    uint8_t ats_len;
    uint8_t ats[20];
    uint8_t has_sak;
    uint8_t sak;
    uint8_t has_atqa;
    uint8_t atqa[2];
    uint8_t has_sm_disable;
    uint8_t sm_disable;
    uint8_t has_ev2_capabilities;
    uint8_t ev2_capabilities[6];
    uint8_t has_proximity_key;
    uint8_t proximity_key[16];
    uint8_t proximity_option;
    uint8_t has_proximity_bitrate;
    uint8_t proximity_bitrate;
    uint16_t proximity_published_response_time;
    uint8_t virtual_card_configured;
    uint8_t virtual_card_installation_id_len;
    uint8_t virtual_card_installation_id[16];
    uint8_t virtual_card_information;
    uint8_t virtual_card_capabilities[2];
    uint8_t virtual_card_uid_len;
    uint8_t virtual_card_uid[10];
    uint8_t virtual_card_select_mac_key[16];
    uint8_t virtual_card_select_encryption_key[16];
    uint8_t virtual_card_authentication_mandatory;
    uint8_t virtual_card_proximity_mandatory;
    uint8_t has_dam_keys;
    uint8_t dam_auth_key[16];
    uint8_t dam_mac_key[16];
    uint8_t dam_encryption_key[16];
    uint8_t has_auth_commands;
    uint8_t auth_commands;
    uint8_t has_preferred_auth_command;
    uint8_t preferred_auth_command;
} DfcFfiPiccSettings;

typedef struct {
    uint32_t num_keys;
    uint32_t key_len;
    uint32_t df_name_len;
    // Two or more when the application carries key sets; otherwise zero.
    uint32_t num_key_sets;
    uint16_t iso_file_id;
    uint16_t delegated_slot_number;
    uint16_t delegated_quota_limit;
    uint16_t delegated_free_blocks;
    uint8_t aid[3];
    uint8_t has_iso_file_id;
    uint8_t df_name[16];
    uint8_t key_settings_1;
    uint8_t key_settings_2;
    uint8_t auth_command;
    uint8_t key_set_max_size;
    uint8_t key_set_settings;
    uint8_t key_set_versions[16];
    uint8_t key_set_types[16];
    uint8_t key_set_initialized[16];
    uint8_t has_capability_data;
    uint8_t capability_data[10];
    uint8_t delegated;
    uint8_t delegated_slot_version;
    uint8_t has_auth_commands;
    uint8_t auth_commands;
    uint8_t has_preferred_auth_command;
    uint8_t preferred_auth_command;
    uint8_t has_sm_disable;
    uint8_t sm_disable;
} DfcFfiApplication;

// The owner of a PICC-level file.
#define DFC_FFI_OWNER_PICC (-1)

typedef struct {
    // An application index, or DFC_FFI_OWNER_PICC.
    int32_t owner;
    uint32_t declared_size;
    uint32_t data_len;
    int32_t value_lower_limit;
    int32_t value_upper_limit;
    int32_t value;
    int32_t value_pending_delta;
    uint32_t record_size;
    uint32_t max_records;
    uint32_t record_count;
    uint32_t sdm_uid_offset;
    uint32_t sdm_counter_offset;
    uint32_t sdm_picc_data_offset;
    uint32_t sdm_mac_input_offset;
    uint32_t sdm_mac_offset;
    uint32_t sdm_encrypted_file_offset;
    uint32_t sdm_encrypted_file_length;
    uint32_t sdm_counter_limit;
    uint32_t sdm_read_counter;
    uint32_t transaction_counter;
    uint16_t access_rights;
    uint16_t iso_file_id;
    uint16_t sdm_access_rights;
    uint8_t number;
    uint8_t type;
    uint8_t comm_settings;
    uint8_t has_iso_file_id;
    uint8_t contents_complete;
    uint8_t transaction_pending;
    uint8_t limited_credit;
    uint8_t value_pending;
    uint8_t sdm_enabled;
    uint8_t sdm_options;
    uint8_t sdm_has_uid_offset;
    uint8_t sdm_has_counter_offset;
    uint8_t sdm_has_picc_data_offset;
    uint8_t sdm_has_mac_input_offset;
    uint8_t sdm_has_mac_offset;
    uint8_t sdm_has_encrypted_file_offset;
    uint8_t sdm_has_counter_limit;
    uint8_t transaction_mac_key_type;
    uint8_t transaction_mac_key_version;
    uint8_t transaction_mac_key[16];
    uint8_t transaction_mac[8];
    uint8_t previous_reader_id[16];
} DfcFfiFile;

typedef struct {
    uint32_t len;
    uint8_t value[24];
    uint8_t version;
} DfcFfiKey;

// Text (.dfc), binary (.dfcb), or either by its first octet. `error` may be
// NULL.
DFC_FFI_EXPORT int32_t dfc_ffi_credential_parse_text(
    const char* text,
    size_t len,
    DfcFfiCredential** out,
    DfcFfiError* error);
DFC_FFI_EXPORT int32_t dfc_ffi_credential_decode(
    const uint8_t* dfcb,
    size_t len,
    DfcFfiCredential** out,
    DfcFfiError* error);
DFC_FFI_EXPORT int32_t dfc_ffi_credential_load(
    const uint8_t* content,
    size_t len,
    DfcFfiCredential** out,
    DfcFfiError* error);

// Encodings of a credential. With `out` NULL and `cap` 0, `*len` receives the
// size without writing. Text is not NUL-terminated.
DFC_FFI_EXPORT int32_t
    dfc_ffi_credential_encode(const DfcFfiCredential* credential, uint8_t* out, size_t cap, size_t* len);
DFC_FFI_EXPORT int32_t
    dfc_ffi_credential_write_text(const DfcFfiCredential* credential, char* out, size_t cap, size_t* len);
DFC_FFI_EXPORT int32_t dfc_ffi_credential_validate(const DfcFfiCredential* credential);
DFC_FFI_EXPORT void dfc_ffi_credential_free(DfcFfiCredential* credential);

// Walking the model.
DFC_FFI_EXPORT int32_t dfc_ffi_credential_get_card(const DfcFfiCredential* credential, DfcFfiCard* out);
DFC_FFI_EXPORT int32_t dfc_ffi_credential_get_picc(const DfcFfiCredential* credential, DfcFfiPiccSettings* out);
DFC_FFI_EXPORT uint32_t dfc_ffi_credential_application_count(const DfcFfiCredential* credential);
DFC_FFI_EXPORT int32_t dfc_ffi_credential_get_application(
    const DfcFfiCredential* credential,
    uint32_t index,
    DfcFfiApplication* out);
DFC_FFI_EXPORT uint32_t dfc_ffi_credential_file_count(const DfcFfiCredential* credential);
DFC_FFI_EXPORT int32_t
    dfc_ffi_credential_get_file(const DfcFfiCredential* credential, uint32_t index, DfcFfiFile* out);
DFC_FFI_EXPORT int32_t dfc_ffi_credential_get_file_data(
    const DfcFfiCredential* credential,
    uint32_t index,
    uint8_t* out,
    size_t cap,
    size_t* len);
// `owner` is an application index or DFC_FFI_OWNER_PICC. Key set 0 is the
// active set, and the only one of an application without key sets.
DFC_FFI_EXPORT int32_t dfc_ffi_credential_get_key(
    const DfcFfiCredential* credential,
    int32_t owner,
    uint32_t key_set,
    uint32_t slot,
    DfcFfiKey* out);

// Building a model: start empty, set the card and PICC, add applications and
// files, set keys and contents, then finish, which validates the whole.
DFC_FFI_EXPORT int32_t dfc_ffi_credential_new(DfcFfiCredential** out);
DFC_FFI_EXPORT int32_t dfc_ffi_credential_set_card(DfcFfiCredential* credential, const DfcFfiCard* card);
DFC_FFI_EXPORT int32_t dfc_ffi_credential_set_picc(DfcFfiCredential* credential, const DfcFfiPiccSettings* picc);
DFC_FFI_EXPORT int32_t dfc_ffi_credential_add_application(
    DfcFfiCredential* credential,
    const DfcFfiApplication* application,
    uint32_t* index);
DFC_FFI_EXPORT int32_t
    dfc_ffi_credential_add_file(DfcFfiCredential* credential, const DfcFfiFile* file, uint32_t* index);
DFC_FFI_EXPORT int32_t dfc_ffi_credential_set_file_data(
    DfcFfiCredential* credential,
    uint32_t index,
    const uint8_t* data,
    size_t len);
DFC_FFI_EXPORT int32_t dfc_ffi_credential_set_key(
    DfcFfiCredential* credential,
    int32_t owner,
    uint32_t key_set,
    uint32_t slot,
    const DfcFfiKey* key);
DFC_FFI_EXPORT int32_t dfc_ffi_credential_finish(DfcFfiCredential* credential, DfcFfiError* error);

// ---------------------------------------------------------- virtual PICC ---

typedef struct {
    uint32_t uid_len;
    uint32_t ats_len;
    uint32_t atqa_len;
    uint32_t protocol_len;
    uint32_t rf_detail_len;
    uint8_t uid[10];
    uint8_t ats[20];
    uint8_t atqa[2];
    uint8_t sak;
    uint8_t protocol[2];
    uint8_t rf_detail[3];
} DfcFfiActivation;

typedef struct {
    // 0 none, 1 the PICC master application, 2 an application.
    uint32_t selected;
    uint32_t selected_application_index;
    uint32_t ev2_command_counter;
    // As the credential stores it, most significant octet first.
    uint8_t selected_aid[3];
    uint8_t dirty;
    // 0 none, or the instruction of the authentication in force.
    uint8_t auth_mode;
    uint8_t auth_key_no;
    uint8_t ev2_active;
    uint8_t ev2_key_no;
    uint8_t ev2_transaction_identifier[4];
    uint8_t ev2_encryption_key[16];
    uint8_t ev2_mac_key[16];
    uint8_t virtual_card_selected;
    uint8_t proximity_verified;
} DfcFfiSnapshot;

// The PICC answers from its own copy of `credential`.
DFC_FFI_EXPORT int32_t dfc_ffi_picc_create(
    const DfcFfiCredential* credential,
    DfcFfiRandomCallback random,
    void* random_context,
    DfcFfiPicc** out);
DFC_FFI_EXPORT void dfc_ffi_picc_free(DfcFfiPicc* picc);
DFC_FFI_EXPORT int32_t dfc_ffi_picc_activate(DfcFfiPicc* picc, DfcFfiActivation* out);
// One APDU (ISO 7816 or native framing) or one ISO-DEP block.
DFC_FFI_EXPORT int32_t dfc_ffi_picc_exchange(
    DfcFfiPicc* picc,
    const uint8_t* command,
    size_t command_len,
    uint8_t* response,
    size_t response_cap,
    size_t* response_len);
DFC_FFI_EXPORT int32_t dfc_ffi_picc_frame_exchange(
    DfcFfiPicc* picc,
    const uint8_t* frame,
    size_t frame_len,
    uint8_t* response,
    size_t response_cap,
    size_t* response_len);
DFC_FFI_EXPORT int32_t dfc_ffi_picc_field_off(DfcFfiPicc* picc);
DFC_FFI_EXPORT int32_t dfc_ffi_picc_reset_protocol(DfcFfiPicc* picc);
DFC_FFI_EXPORT int32_t dfc_ffi_picc_advance_time(DfcFfiPicc* picc, uint32_t elapsed_milliseconds);
DFC_FFI_EXPORT int32_t dfc_ffi_picc_snapshot(const DfcFfiPicc* picc, DfcFfiSnapshot* out);
// A new credential holding the card's current state.
DFC_FFI_EXPORT int32_t dfc_ffi_picc_export(const DfcFfiPicc* picc, DfcFfiCredential** out);
DFC_FFI_EXPORT int32_t dfc_ffi_picc_mark_persisted(DfcFfiPicc* picc);

// ---------------------------------------------------------------- reader ---

typedef struct {
    uint32_t ev2_command_counter;
    uint8_t auth_mode;
    uint8_t key_no;
    uint8_t ev2_transaction_identifier[4];
    uint8_t ev2_card_capabilities[6];
    uint8_t ev2_reader_capabilities[6];
} DfcFfiReaderState;

DFC_FFI_EXPORT int32_t dfc_ffi_reader_session_new(DfcFfiReaderSession** out);
DFC_FFI_EXPORT void dfc_ffi_reader_session_free(DfcFfiReaderSession* session);
DFC_FFI_EXPORT void dfc_ffi_reader_session_clear(DfcFfiReaderSession* session);
DFC_FFI_EXPORT int32_t
    dfc_ffi_reader_session_state(const DfcFfiReaderSession* session, DfcFfiReaderState* out);

// An exchange runs against the session it was begun with. `framing` is a
// DfcReaderFraming value.
DFC_FFI_EXPORT int32_t dfc_ffi_reader_exchange_new(DfcFfiReaderExchange** out);
DFC_FFI_EXPORT void dfc_ffi_reader_exchange_free(DfcFfiReaderExchange* exchange);

DFC_FFI_EXPORT int32_t dfc_ffi_reader_authenticate_begin(
    DfcFfiReaderExchange* exchange,
    DfcFfiReaderSession* session,
    uint32_t framing,
    uint8_t cipher,
    uint8_t key_no,
    const uint8_t* key,
    size_t key_len,
    const uint8_t* random_a,
    size_t random_a_len);
// ISO 7816 GetChallenge/ExternalAuthenticate/InternalAuthenticate.
DFC_FFI_EXPORT int32_t dfc_ffi_reader_authenticate_iso7816_begin(
    DfcFfiReaderExchange* exchange,
    DfcFfiReaderSession* session,
    uint8_t key_reference,
    const uint8_t* key,
    size_t key_len,
    uint8_t algorithm,
    const uint8_t* random_first,
    const uint8_t* random_second,
    size_t random_len);
DFC_FFI_EXPORT int32_t dfc_ffi_reader_authenticate_ev2_begin(
    DfcFfiReaderExchange* exchange,
    DfcFfiReaderSession* session,
    uint32_t framing,
    uint8_t first,
    uint8_t key_no,
    const uint8_t* key,
    const uint8_t* random_a,
    const uint8_t* capabilities,
    size_t capabilities_len);
// `comm_mode` is DFC_COMM_PLAIN, _MAC or _ENCIPHERED; `header_len` of -1
// derives the clear head of an encrypted field from the instruction.
DFC_FFI_EXPORT int32_t dfc_ffi_reader_exchange_begin(
    DfcFfiReaderExchange* exchange,
    DfcFfiReaderSession* session,
    uint32_t framing,
    uint8_t ins,
    const uint8_t* data,
    size_t data_len,
    uint8_t comm_mode,
    int32_t header_len);
// The caller owns `data` until dfc_ffi_reader_step completes the exchange.
DFC_FFI_EXPORT int32_t dfc_ffi_reader_write_data_begin(
    DfcFfiReaderExchange* exchange,
    DfcFfiReaderSession* session,
    uint32_t framing,
    uint8_t file_number,
    uint32_t offset,
    const uint8_t* data,
    size_t data_len,
    uint8_t comm_mode);
DFC_FFI_EXPORT int32_t dfc_ffi_reader_write_record_begin(
    DfcFfiReaderExchange* exchange,
    DfcFfiReaderSession* session,
    uint32_t framing,
    uint8_t file_number,
    uint32_t offset,
    const uint8_t* data,
    size_t data_len,
    uint8_t comm_mode);
DFC_FFI_EXPORT int32_t dfc_ffi_reader_update_record_begin(
    DfcFfiReaderExchange* exchange,
    DfcFfiReaderSession* session,
    uint32_t framing,
    uint8_t file_number,
    uint32_t record_number,
    uint32_t offset,
    const uint8_t* data,
    size_t data_len,
    uint8_t comm_mode);
DFC_FFI_EXPORT int32_t dfc_ffi_reader_create_delegated_application_begin(
    DfcFfiReaderExchange* exchange,
    DfcFfiReaderSession* session,
    uint32_t framing,
    const uint8_t* header,
    size_t header_len,
    const uint8_t* dam_encryption_key,
    const uint8_t* dam_mac_key,
    const uint8_t* random_prefix,
    const uint8_t* initial_key,
    size_t initial_key_len,
    uint8_t initial_version);
DFC_FFI_EXPORT int32_t dfc_ffi_reader_step(
    DfcFfiReaderExchange* exchange,
    const uint8_t* response,
    size_t response_len,
    uint8_t* out,
    size_t out_cap,
    size_t* out_len);
DFC_FFI_EXPORT uint8_t dfc_ffi_reader_result_status(const DfcFfiReaderExchange* exchange);
DFC_FFI_EXPORT int32_t dfc_ffi_reader_result_data(
    const DfcFfiReaderExchange* exchange,
    uint8_t* out,
    size_t cap,
    size_t* len);

// Cryptograms and helpers that need the session's keys.
DFC_FFI_EXPORT int32_t dfc_ffi_reader_change_key_cryptogram(
    const DfcFfiReaderSession* session,
    uint8_t key_no,
    const uint8_t* new_key,
    size_t key_len,
    const uint8_t* current_key,
    uint8_t aes_key,
    uint8_t new_version,
    uint8_t* out,
    size_t out_cap,
    size_t* out_len);
DFC_FFI_EXPORT int32_t dfc_ffi_reader_change_key_ev2_data(
    const DfcFfiReaderSession* session,
    uint8_t key_set_no,
    uint8_t key_no,
    const uint8_t* new_key,
    const uint8_t* current_key,
    uint8_t new_version,
    uint8_t* out,
    size_t out_cap,
    size_t* out_len);

#ifdef __cplusplus
}
#endif
