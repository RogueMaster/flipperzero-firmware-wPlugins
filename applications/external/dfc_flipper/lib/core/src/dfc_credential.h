#pragma once

#include "dfc_port.h"

#include "dfc.h"
#include "dfc_common.h"

// Sentinel: file has no bytes reserved in the shared pool.
#define DFC_FILE_POOL_NONE ((size_t)0xFFFFFFFFu)

// Sentinel: application or PICC record has no key material reserved in the
// shared key pool.
#define DFC_KEY_POOL_NONE ((size_t)0xFFFFFFFFu)

// Owning container of a file. Applications are 0..num_apps-1; the PICC record
// uses this sentinel, so a PICC-level file is not mistaken for one in the first
// application.
#define DFC_FILE_OWNER_PICC ((size_t)0xFFFFFFFEu)

typedef struct {
    size_t app_index;
    uint8_t number;
    uint8_t type;
    uint8_t comm_settings;
    uint16_t access_rights;
    bool has_iso_file_id;
    uint16_t iso_file_id;

    // Declared allocation, which is what a read returns. Distinct from the
    // contents actually known, which may be shorter.
    uint32_t declared_size;
    // Slice into DfcCredential.file_pool holding the known contents.
    size_t data_offset;
    size_t data_len;
    // True when data_len covers the whole declared size, or every record.
    bool contents_complete;
    bool transaction_pending;

    int32_t value_lower_limit;
    int32_t value_upper_limit;
    int32_t value;
    uint8_t limited_credit;
    bool value_pending;
    int32_t value_pending_delta;

    // Record files. The pool slice holds the leading known records in card
    // order, so data_len is a whole multiple of record_size.
    uint32_t record_size;
    uint32_t max_records;
    uint32_t record_count;
#if DFC_ENABLE_SDM
    bool sdm_enabled;
    uint8_t sdm_options;
    uint16_t sdm_access_rights;
    bool sdm_has_uid_offset;
    uint32_t sdm_uid_offset;
    bool sdm_has_counter_offset;
    uint32_t sdm_counter_offset;
    bool sdm_has_picc_data_offset;
    uint32_t sdm_picc_data_offset;
    bool sdm_has_mac_input_offset;
    uint32_t sdm_mac_input_offset;
    bool sdm_has_mac_offset;
    uint32_t sdm_mac_offset;
    bool sdm_has_encrypted_file_offset;
    uint32_t sdm_encrypted_file_offset;
    uint32_t sdm_encrypted_file_length;
    bool sdm_has_counter_limit;
    uint32_t sdm_counter_limit;
    uint32_t sdm_read_counter;
#endif
#if DFC_ENABLE_TRANSACTION_MAC
    uint8_t transaction_mac_key[DFC_AES_KEY_LENGTH];
    uint8_t transaction_mac_key_type;
    uint8_t transaction_mac_key_version;
    uint32_t transaction_counter;
    uint8_t transaction_mac[DFC_WIRE_MAC_LENGTH];
    uint8_t previous_reader_id[DFC_TRANSACTION_READER_ID_LENGTH];
#endif
} DfcFile;

typedef struct {
    uint8_t aid[3];
    uint8_t iso_aid[16];
    size_t iso_aid_len;
    bool has_iso_file_id;
    uint16_t iso_file_id;

    uint8_t key_settings_1;
    uint8_t key_settings_2;
    uint8_t auth_command;
    // Slice into DfcCredential.key_pool holding num_keys keys, each one stored
    // key length wide. DFC_KEY_POOL_NONE when nothing is reserved. The slice
    // length is held rather than recomputed, so a caller that edits key_len in
    // place cannot make the bookkeeping disagree with the pool. Versions stay
    // inline: one octet per slot is not worth pooling.
    size_t key_offset;
    size_t key_pool_len;
    uint8_t key_versions[DFC_MAX_KEYS];
    size_t num_keys;
    size_t key_len;
#if DFC_ENABLE_KEY_SETS
    size_t key_storage_len;
    uint8_t num_key_sets;
    uint8_t max_key_size;
    uint8_t key_set_settings;
    uint8_t key_set_versions[DFC_MAX_KEY_SETS];
    uint8_t key_set_types[DFC_MAX_KEY_SETS];
    bool key_set_initialized[DFC_MAX_KEY_SETS];
    uint8_t additional_key_versions[DFC_ADDITIONAL_KEY_SET_COUNT][DFC_MAX_KEYS];
#endif
#if DFC_ENABLE_APPLICATION_CAPABILITY_DATA
    bool has_capability_data;
    uint8_t capability_data[DFC_APPLICATION_CAPABILITY_DATA_LENGTH];
#endif
#if DFC_ENABLE_DELEGATED_APPLICATIONS
    bool delegated;
    uint16_t delegated_slot_number;
    uint8_t delegated_slot_version;
    uint16_t delegated_quota_limit;
    uint16_t delegated_free_blocks;
#endif
} DfcApplication;

// Card generation label.
typedef enum {
    DfcGenerationEv1 = 1,
    DfcGenerationEv2 = 2,
    DfcGenerationEv3 = 3,
} DfcGeneration;

// Where the UID came from.
typedef enum {
    DfcUidProvenanceReal = 0,
    DfcUidProvenanceRandom = 1,
    DfcUidProvenanceUnknown = 2,
} DfcUidProvenance;

typedef struct {
    DfcGeneration generation;
    // User memory in octets, the capacity the emulated card advertises.
    uint32_t storage;
    // The UID itself lives on DfcCredential; this records where it came from.
    DfcUidProvenance uid_provenance;
} DfcCard;

typedef struct {
    DfcCard card;

    uint8_t uid[DFC_DESFIRE_UID_MAX_LENGTH];
    size_t uid_len;

    uint8_t picc_key_settings_1;
    uint8_t picc_key_settings_2;
    uint8_t picc_auth_command;
    // PICC keys, slot 0 first. Slot 0 is the master key; later generations
    // define further slots, so this is a list rather than one key. The material
    // lives in key_pool, addressed the same way an application's does.
    size_t picc_key_offset;
    size_t picc_key_pool_len;
    uint8_t picc_key_versions[DFC_MAX_KEYS];
    size_t picc_num_keys;
    size_t picc_key_len;

    // PICC configuration reachable through SetConfiguration.
    bool picc_random_id;
    bool picc_format_disabled;
    uint8_t picc_ats[DFC_PICC_ATS_MAX];
    size_t picc_ats_len;
    bool picc_has_sak;
    uint8_t picc_sak;
    bool picc_has_atqa;
    uint8_t picc_atqa[2];
    bool picc_has_sm_disable;
    uint8_t picc_sm_disable;

#if DFC_ENABLE_EV2_SECURE_MESSAGING
    bool picc_has_ev2_capabilities;
    uint8_t picc_ev2_capabilities[DFC_EV2_CAPABILITY_LENGTH];
#endif

#if DFC_ENABLE_PROXIMITY_CHECK
    bool picc_has_proximity_key;
    uint8_t picc_proximity_key[DFC_AES_KEY_LENGTH];
    uint8_t picc_proximity_option;
    uint16_t picc_proximity_published_response_time;
    bool picc_has_proximity_bitrate;
    uint8_t picc_proximity_bitrate;
#endif

#if DFC_ENABLE_STATIC_SIGNATURE
    bool picc_has_static_signature;
    uint8_t picc_static_signature[DFC_STATIC_SIGNATURE_LENGTH];
#endif

#if DFC_ENABLE_VIRTUAL_CARD
    bool virtual_card_configured;
    uint8_t virtual_card_installation_id[DFC_VIRTUAL_CARD_MAX_INSTALLATION_ID_LENGTH];
    size_t virtual_card_installation_id_len;
    uint8_t virtual_card_information;
    uint8_t virtual_card_capabilities[DFC_VIRTUAL_CARD_CAPABILITY_LENGTH];
    uint8_t virtual_card_uid[DFC_VIRTUAL_CARD_UID_MAX_LENGTH];
    size_t virtual_card_uid_len;
    uint8_t virtual_card_select_mac_key[DFC_AES_KEY_LENGTH];
    uint8_t virtual_card_select_encryption_key[DFC_AES_KEY_LENGTH];
    bool virtual_card_authentication_mandatory;
    bool virtual_card_proximity_mandatory;
#endif

#if DFC_ENABLE_DELEGATED_APPLICATIONS
    bool picc_has_dam_keys;
    uint8_t picc_dam_auth_key[DFC_AES_KEY_LENGTH];
    uint8_t picc_dam_mac_key[DFC_AES_KEY_LENGTH];
    uint8_t picc_dam_encryption_key[DFC_AES_KEY_LENGTH];
#endif

    DfcApplication apps[DFC_MAX_APPS];
    size_t num_apps;
    DfcFile files[DFC_MAX_FILES];
    size_t num_files;

    // Variable file payload storage. Standard-data files allocate slices here.
    uint8_t file_pool[DFC_FILE_POOL_SIZE];
    size_t file_pool_used;

    // Variable key storage, shared by every application and the PICC record.
    uint8_t key_pool[DFC_KEY_POOL_SIZE];
    size_t key_pool_used;

    bool dirty;

    char name[DFC_FILE_NAME_MAX_LENGTH + 1];
} DfcCredential;

DfcCredential* dfc_credential_alloc();
void dfc_credential_free(DfcCredential* dfc_credential);

bool dfc_credential_clear(DfcCredential* dfc_credential);
// Resets credential to a blank, emulatable template: random UID, a default AID, a
// single D40 DES key (all-zero, key 0), and one empty writable Standard Data file -
// enough for a DESFire reader/writer to authenticate against and WriteData into during
// emulation. Used by the "Blank Card" main menu flow (emulate -> write -> save).
void dfc_credential_init_blank(DfcCredential* dfc_credential);

// Derive the per-key byte length from the Key Settings 2 crypto-type bits
// (00=DES/2K3DES 8 or 16 bytes stored as 16, 01=3K3DES 24 bytes, 10=AES 16 bytes).
size_t dfc_credential_key_length(uint8_t key_settings_2);
// Octets a key occupies in the pool. Fixed by key type: an 8-octet DES key is
// held as 16, so a slot is always 16 or 24 wide.
size_t dfc_credential_stored_key_length(size_t key_len);
bool dfc_credential_uid_is_detectable(DfcCredential* dfc_credential);

// Whether a stored user ATS can actually be answered with. An ATS states its own
// length in its first octet, so one whose first octet disagrees with the stored
// length describes nothing the emulator could transmit. A credential carrying one
// is refused as unsupported rather than emulated with a repaired ATS. True when
// no user ATS is stored, which is the common case.
bool dfc_credential_picc_ats_is_consistent(const DfcCredential* dfc_credential);

DfcApplication* dfc_credential_get_application(DfcCredential* credential, size_t app_index);
const DfcApplication*
    dfc_credential_get_application_const(const DfcCredential* credential, size_t app_index);
DfcApplication* dfc_credential_get_primary_application(DfcCredential* credential);
const DfcApplication*
    dfc_credential_get_primary_application_const(const DfcCredential* credential);
DfcApplication* dfc_credential_find_application(DfcCredential* credential, const uint8_t aid[3]);
DfcApplication*
    dfc_credential_find_application_desfire_order(DfcCredential* credential, const uint8_t aid[3]);
size_t dfc_credential_application_index(DfcCredential* credential, DfcApplication* application);
DfcApplication* dfc_credential_create_application_desfire_order(
    DfcCredential* credential,
    const uint8_t aid[3],
    uint8_t key_settings_1,
    uint8_t key_settings_2);
bool dfc_credential_delete_application_at(DfcCredential* credential, size_t app_index);
void dfc_credential_format_picc(DfcCredential* credential);

DfcFile* dfc_credential_find_file(DfcCredential* credential, uint8_t number);
DfcFile*
    dfc_credential_find_file_in_app(DfcCredential* credential, size_t app_index, uint8_t number);
DfcFile* dfc_credential_create_file(DfcCredential* credential, size_t app_index, uint8_t number);
bool dfc_credential_delete_file(DfcCredential* credential, size_t app_index, uint8_t number);
size_t dfc_credential_count_files_in_app(const DfcCredential* credential, size_t app_index);

// Shared-pool helpers for key material. `application` selects an application;
// NULL selects the PICC record. Reset clears an application to a blank state
// with no pool slice, and is what callers use in place of a bare memset.
void dfc_credential_reset_application(DfcApplication* application);
uint8_t* dfc_credential_key(DfcCredential* credential, DfcApplication* application, size_t slot);
const uint8_t* dfc_credential_key_const(
    const DfcCredential* credential,
    const DfcApplication* application,
    size_t slot);
// Allocate or resize the record's pool slice to hold `num_keys` keys of
// `key_len` (zero-filled). Records num_keys and key_len on the target on
// success. Returns false if the pool is exhausted or num_keys exceeds
// DFC_MAX_KEYS.
bool dfc_credential_keys_resize(
    DfcCredential* credential,
    DfcApplication* application,
    size_t num_keys,
    size_t key_len);
#if DFC_ENABLE_KEY_SETS
bool dfc_credential_key_sets_resize(
    DfcCredential* credential,
    DfcApplication* application,
    size_t num_key_sets,
    size_t num_keys,
    size_t active_key_len,
    size_t max_key_size);
uint8_t* dfc_credential_key_in_set(
    DfcCredential* credential,
    DfcApplication* application,
    size_t key_set_number,
    size_t slot);
const uint8_t* dfc_credential_key_in_set_const(
    const DfcCredential* credential,
    const DfcApplication* application,
    size_t key_set_number,
    size_t slot);
uint8_t* dfc_credential_key_version_in_set(
    DfcApplication* application,
    size_t key_set_number,
    size_t slot);
bool dfc_credential_roll_key_set(
    DfcCredential* credential,
    DfcApplication* application,
    size_t key_set_number);
#endif
void dfc_credential_keys_release(DfcCredential* credential, DfcApplication* application);
size_t dfc_credential_key_pool_free(const DfcCredential* credential);

// Shared-pool helpers for standard/backup data files.
uint8_t* dfc_file_data(DfcCredential* credential, DfcFile* file);
const uint8_t* dfc_file_data_const(const DfcCredential* credential, const DfcFile* file);
// Allocate or resize the file's pool slice to `data_len` (zero-filled on grow).
// Returns false if the pool is exhausted or data_len exceeds DFC_MAX_FILE_DATA.
bool dfc_file_resize(DfcCredential* credential, DfcFile* file, size_t data_len);
void dfc_file_release(DfcCredential* credential, DfcFile* file);
size_t dfc_credential_file_pool_free(const DfcCredential* credential);

// Copy the credential model from `src` into `dst`: identity, PICC state,
// applications, files, and the shared pool the file slices index into. Leaves
// `dst`'s storage, dialogs and load path alone, so it can publish a freshly
// parsed credential into a live one. Copying `files` without `file_pool`, or
// applications without `key_pool`, leaves every slice dangling, so a pool always
// moves with the records that index into it.
void dfc_credential_copy_model(DfcCredential* dst, const DfcCredential* src);

// Sets a file's allocation to `size` and records it as complete.
bool dfc_file_set_data_size(DfcCredential* credential, DfcFile* file, uint32_t size);

// Grows every file's slice to its full allocation, so an emulated card answers
// a read past the part that was recorded. A card holds the whole allocation.
// Returns false when the shared pool cannot hold it, leaving the credential
// usable but short. `scratch` parks a file's known prefix while its slice is
// reallocated, so it must be at least as long as the longest known prefix.
bool dfc_credential_materialize_contents(
    DfcCredential* credential,
    uint8_t* scratch,
    size_t scratch_len);

void dfc_credential_mark_dirty(DfcCredential* credential);
void dfc_credential_clear_dirty(DfcCredential* credential);
