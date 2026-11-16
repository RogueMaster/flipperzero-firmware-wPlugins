#pragma once

#include "dfc_port.h"

#include "dfc.h"
#include "dfc_common.h"

// Pool slices use the smallest integer that still represents every configured
// byte. Keeping the sentinel outside the usable offset range preserves custom
// host profiles without making target records pay for host-sized indices.
#if DFC_FILE_POOL_SIZE <= UINT16_MAX
typedef uint16_t DfcFilePoolSize;
#define DFC_FILE_POOL_NONE UINT16_MAX
#else
typedef uint32_t DfcFilePoolSize;
#define DFC_FILE_POOL_NONE UINT32_MAX
#endif

#if DFC_KEY_POOL_SIZE <= UINT16_MAX
typedef uint16_t DfcKeyPoolSize;
#define DFC_KEY_POOL_NONE UINT16_MAX
#else
typedef uint32_t DfcKeyPoolSize;
#define DFC_KEY_POOL_NONE UINT32_MAX
#endif

#if DFC_MAX_FILES <= UINT8_MAX
typedef uint8_t DfcFileCount;
#elif DFC_MAX_FILES <= UINT16_MAX
typedef uint16_t DfcFileCount;
#else
typedef uint32_t DfcFileCount;
#endif

_Static_assert(DFC_FILE_POOL_SIZE <= UINT32_MAX, "file pool offsets must fit uint32_t");
_Static_assert(DFC_KEY_POOL_SIZE <= UINT32_MAX, "key pool offsets must fit uint32_t");
_Static_assert(DFC_MAX_APPS < UINT8_MAX, "file owner index must leave room for the PICC");
_Static_assert(DFC_MAX_FILES <= UINT32_MAX, "file count must fit uint32_t");
_Static_assert(DFC_MAX_KEYS <= UINT8_MAX, "application key counts must fit uint8_t");
_Static_assert(DFC_MAX_KEY_LEN <= UINT8_MAX, "application key lengths must fit uint8_t");
_Static_assert(DFC_DESFIRE_UID_MAX_LENGTH <= UINT8_MAX, "UID length must fit uint8_t");
_Static_assert(DFC_PICC_ATS_MAX <= UINT8_MAX, "ATS length must fit uint8_t");
#if DFC_ENABLE_VIRTUAL_CARD
_Static_assert(
    DFC_VIRTUAL_CARD_MAX_INSTALLATION_ID_LENGTH <= UINT8_MAX,
    "installation identifier length must fit uint8_t");
_Static_assert(DFC_VIRTUAL_CARD_UID_MAX_LENGTH <= UINT8_MAX, "virtual UID length must fit uint8_t");
#endif

// Owning container of a file. Applications are 0..num_apps-1; the PICC record
// uses this sentinel, so a PICC-level file is not mistaken for one in the first
// application.
#define DFC_FILE_OWNER_PICC UINT8_MAX

#define DFC_LIMITED_CREDIT_TRANSACTION_CONSUMED (-1)

// One status vocabulary for the in-memory model and both credential codecs.
typedef enum {
    DfcModelOk = 0,
    DfcModelMalformed,
    DfcModelUnsupported,
    DfcModelCapacity,
} DfcModelStatus;

#if DFC_ENABLE_SDM
typedef enum {
    DfcSdmFlagEnabled = 1u << 0,
    DfcSdmFlagUidOffset = 1u << 1,
    DfcSdmFlagCounterOffset = 1u << 2,
    DfcSdmFlagPiccDataOffset = 1u << 3,
    DfcSdmFlagMacInputOffset = 1u << 4,
    DfcSdmFlagEncryptedFile = 1u << 5,
    DfcSdmFlagMacOffset = 1u << 6,
    DfcSdmFlagCounterLimit = 1u << 7,
} DfcSdmFlag;

typedef struct {
    uint8_t flags;
    uint8_t options;
    uint16_t access_rights;
    uint32_t uid_offset;
    uint32_t counter_offset;
    uint32_t picc_data_offset;
    uint32_t mac_input_offset;
    uint32_t encrypted_file_offset;
    uint32_t encrypted_file_length;
    uint32_t mac_offset;
    uint32_t counter_limit;
} DfcSdmSettings;

_Static_assert(sizeof(DfcSdmSettings) == 36, "SDM settings layout must stay compact");
_Static_assert(
    (DfcSdmFlagEnabled | DfcSdmFlagUidOffset | DfcSdmFlagCounterOffset | DfcSdmFlagPiccDataOffset |
     DfcSdmFlagMacInputOffset | DfcSdmFlagEncryptedFile | DfcSdmFlagMacOffset |
     DfcSdmFlagCounterLimit) == UINT8_MAX,
    "SDM flags must cover one byte");
#endif

typedef struct {
    int32_t lower_limit;
    int32_t upper_limit;
    int32_t current;
    int32_t pending_delta;
    int32_t pending_value;
    int32_t limited_credit_value;
    int32_t limited_credit_pending;
    uint8_t limited_credit;
    bool pending;
    bool pending_absolute;
} DfcValueFileState;

typedef struct {
    uint32_t size;
    uint32_t max_records;
    uint32_t count;
} DfcRecordFileState;

#if DFC_ENABLE_TRANSACTION_MAC
typedef struct {
    uint32_t counter;
    uint8_t key[DFC_AES_KEY_LENGTH];
    uint8_t mac[DFC_WIRE_MAC_LENGTH];
    uint8_t previous_reader_id[DFC_TRANSACTION_READER_ID_LENGTH];
    uint8_t key_type;
    uint8_t key_version;
} DfcTransactionMacFileState;
#endif

typedef struct {
    uint32_t declared_size;
#if DFC_ENABLE_SDM
    DfcSdmSettings sdm;
    uint32_t sdm_read_counter;
#endif
} DfcDataFileState;

typedef union {
    DfcDataFileState data;
    DfcValueFileState value;
    DfcRecordFileState record;
#if DFC_ENABLE_TRANSACTION_MAC
    DfcTransactionMacFileState transaction_mac;
#endif
} DfcFileState;

typedef struct {
    uint8_t app_index;
    // Slice into DfcCredential.file_pool holding the known contents.
    DfcFilePoolSize data_offset;
    DfcFilePoolSize data_len;
    uint16_t access_rights;
    uint16_t iso_file_id;
    uint8_t number;
    uint8_t type;
    uint8_t comm_settings;
    bool has_iso_file_id;
    // True when data_len covers the whole declared size, or every record.
    bool contents_complete;
    bool transaction_pending;

    DfcFileState state;
} DfcFile;

static inline bool dfc_file_type_is_data(uint8_t type) {
    return type == DFC_FILE_TYPE_STANDARD_DATA || type == DFC_FILE_TYPE_BACKUP_DATA;
}

// Keep the union tag check at the write boundary so callers cannot corrupt another file state.
static inline bool dfc_file_set_declared_size(DfcFile* file, uint32_t size) {
    if(!file || !dfc_file_type_is_data(file->type)) return false;
    file->state.data.declared_size = size;
    return true;
}

static inline bool dfc_file_get_declared_size(const DfcFile* file, uint32_t* size) {
    if(!file || !size || !dfc_file_type_is_data(file->type)) return false;
    *size = file->state.data.declared_size;
    return true;
}

#if DFC_ENABLE_SDM
_Static_assert(sizeof(DfcDataFileState) <= sizeof(DfcFileState), "data state must fit file state");
#endif
#if DFC_ENABLE_TRANSACTION_MAC
_Static_assert(sizeof(DfcFileState) == 48, "transaction MAC state must bound file state");
#endif
#if DFC_BUILD_ROLE == DFC_ROLE_TARGET && SIZE_MAX == UINT32_MAX && DFC_ENABLE_TRANSACTION_MAC
_Static_assert(sizeof(DfcFile) == 64, "target file layout must stay compact");
#endif

#if DFC_ENABLE_SDM
static inline bool dfc_sdm_flag_is_set(const DfcSdmSettings* settings, DfcSdmFlag flag) {
    return settings && (settings->flags & (uint8_t)flag) != 0;
}

static inline void dfc_sdm_flag_set(DfcSdmSettings* settings, DfcSdmFlag flag, bool value) {
    if(!settings) return;
    if(value)
        settings->flags |= (uint8_t)flag;
    else
        settings->flags &= (uint8_t)~flag;
}

static inline bool dfc_file_sdm_flag_is_set(const DfcFile* file, DfcSdmFlag flag) {
    return file && file->type == DFC_FILE_TYPE_STANDARD_DATA &&
           dfc_sdm_flag_is_set(&file->state.data.sdm, flag);
}

static inline void dfc_file_sdm_flag_set(DfcFile* file, DfcSdmFlag flag, bool value) {
    if(!file || file->type != DFC_FILE_TYPE_STANDARD_DATA) return;
    dfc_sdm_flag_set(&file->state.data.sdm, flag, value);
}
#endif

typedef struct {
    DfcKeyPoolSize key_offset;
    DfcKeyPoolSize key_pool_len;
    uint8_t iso_aid_len;
    // Slice into DfcCredential.key_pool holding num_keys keys, each one stored
    // key length wide. DFC_KEY_POOL_NONE when nothing is reserved. The slice
    // length is held rather than recomputed, so a caller that edits key_len in
    // place cannot make the bookkeeping disagree with the pool. Versions stay
    // inline: one octet per slot is not worth pooling.
    uint8_t num_keys;
    uint8_t key_len;
#if DFC_ENABLE_KEY_SETS
    uint8_t key_storage_len;
    uint16_t key_set_metadata_offset;
    uint16_t key_set_metadata_len;
#endif
    uint16_t iso_file_id;
#if DFC_ENABLE_DELEGATED_APPLICATIONS
    uint16_t delegated_slot_number;
    uint16_t delegated_quota_limit;
    uint16_t delegated_free_blocks;
    uint8_t delegated_slot_version;
    bool delegated;
    // One-way application setting controlled by SetConfiguration option 00.
    bool delegated_format_disabled;
#endif
    uint8_t aid[3];
    uint8_t iso_aid[16];
    bool has_iso_file_id;
    uint8_t key_settings_1;
    uint8_t key_settings_2;
    uint8_t auth_command;
    // v6 policy. If absent, supported commands are inferred from key type and generation.
    bool has_auth_commands;
    uint8_t auth_commands;
    bool has_preferred_auth_command;
    uint8_t preferred_auth_command;
    bool has_sm_disable;
    uint8_t sm_disable;
    uint8_t key_versions[DFC_MAX_KEYS];
#if DFC_ENABLE_KEY_SETS
    uint8_t num_key_sets;
    uint8_t max_key_size;
    uint8_t key_set_settings;
#endif
#if DFC_ENABLE_APPLICATION_CAPABILITY_DATA
    bool has_capability_data;
    uint8_t capability_data[DFC_APPLICATION_CAPABILITY_DATA_LENGTH];
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
    // Optional seven-octet GetVersion response overrides.
    bool has_hardware_version;
    bool has_software_version;
    uint8_t hardware_version[7];
    uint8_t software_version[7];
} DfcCard;

typedef struct {
    uint8_t uid_len;
    // PICC keys, slot 0 first. Slot 0 is the master key; later generations
    // define further slots, so this is a list rather than one key. The material
    // lives in key_pool, addressed the same way an application's does.
    DfcKeyPoolSize picc_key_offset;
    DfcKeyPoolSize picc_key_pool_len;
    uint8_t picc_num_keys;
    uint8_t picc_key_len;
    uint8_t picc_ats_len;
#if DFC_ENABLE_VIRTUAL_CARD
    uint8_t virtual_card_installation_id_len;
    uint8_t virtual_card_uid_len;
#endif
    uint8_t num_apps;
    DfcFileCount num_files;
    DfcFilePoolSize file_pool_used;
    DfcKeyPoolSize key_pool_used;

    DfcApplication apps[DFC_MAX_APPS];
    DfcFile files[DFC_MAX_FILES];
    DfcCard card;

#if DFC_ENABLE_PROXIMITY_CHECK
    uint16_t picc_proximity_published_response_time;
    bool picc_has_proximity_key;
    uint8_t picc_proximity_key[DFC_AES_KEY_LENGTH];
    uint8_t picc_proximity_option;
    bool picc_has_proximity_bitrate;
    uint8_t picc_proximity_bitrate;
#endif

    uint8_t uid[DFC_DESFIRE_UID_MAX_LENGTH];
    uint8_t picc_key_settings_1;
    uint8_t picc_key_settings_2;
    uint8_t picc_auth_command;
    bool picc_has_auth_commands;
    uint8_t picc_auth_commands;
    bool picc_has_preferred_auth_command;
    uint8_t picc_preferred_auth_command;
    uint8_t picc_key_versions[DFC_MAX_KEYS];

    // PICC configuration reachable through SetConfiguration.
    bool picc_random_id;
    // Selects the legacy 0x80 prefix instead of the ISO-compatible 0x08 prefix.
    bool picc_random_id_legacy;
    bool picc_format_disabled;
    uint8_t picc_ats[DFC_PICC_ATS_MAX];
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

#if DFC_ENABLE_STATIC_SIGNATURE
    bool picc_has_static_signature;
    uint8_t picc_static_signature[DFC_STATIC_SIGNATURE_LENGTH];
#endif

#if DFC_ENABLE_VIRTUAL_CARD
    bool virtual_card_configured;
    uint8_t virtual_card_installation_id[DFC_VIRTUAL_CARD_MAX_INSTALLATION_ID_LENGTH];
    uint8_t virtual_card_information;
    uint8_t virtual_card_capabilities[DFC_VIRTUAL_CARD_CAPABILITY_LENGTH];
    uint8_t virtual_card_uid[DFC_VIRTUAL_CARD_UID_MAX_LENGTH];
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

    // Variable file payload storage. Standard-data files allocate slices here.
    uint8_t file_pool[DFC_FILE_POOL_SIZE];

    // Variable key storage, shared by every application and the PICC record.
    uint8_t key_pool[DFC_KEY_POOL_SIZE];
#if DFC_ENABLE_KEY_SETS
    // Sparse metadata for applications that actually use multiple key sets.
    // Each application owns one compact slice addressed by the fields above.
    uint8_t key_set_metadata_pool[DFC_KEY_SET_METADATA_POOL_SIZE];
    uint16_t key_set_metadata_pool_used;
#endif

    bool dirty;
} DfcCredential;

// This model owns only inline storage and offsets into its own pools. Do not
// add owning pointers without redesigning dfc_credential_copy_model.

DfcCredential* dfc_credential_alloc(void);
void dfc_credential_free(DfcCredential* dfc_credential);

// Reset `credential` to the empty credential: factory PICC settings, no
// applications, files or keys. Every octet of the previous content, key material
// included, is wiped first. This is the one state a function that fills a
// credential from outside input (dfc_der_decode, dfc_text_parse,
// dfc_credential_load, dfc_target_credential_decode) leaves behind on every
// failure; none of them ever leaves a credential partly populated.
bool dfc_credential_clear(DfcCredential* dfc_credential);
// Factory defaults and a generated UID, without applications or files. Returns
// false, leaving the credential unchanged, if randomness is unavailable.
bool dfc_credential_init_factory(DfcCredential* credential);
// Resets credential to a blank, emulatable template: random UID, a default AID, a
// single D40 DES key (all-zero, key 0), and one empty writable Standard Data file -
// enough for a DESFire reader/writer to authenticate against and WriteData into during
// emulation. Used by the "Blank Card" main menu flow (emulate -> write -> save).
// Returns false, leaving the credential unchanged, if randomness is unavailable.
bool dfc_credential_init_blank(DfcCredential* dfc_credential);

// The singular command fields are retained for existing C callers. A v6 mask
// takes precedence whenever its corresponding has_auth_commands flag is set.
uint8_t dfc_credential_default_auth_commands(uint8_t key_settings_2, DfcGeneration generation);
uint8_t dfc_credential_possible_auth_commands(uint8_t key_settings_2, DfcGeneration generation);
uint8_t dfc_credential_compiled_auth_commands(void);
uint8_t dfc_credential_picc_auth_commands(const DfcCredential* credential);
uint8_t
    dfc_credential_app_auth_commands(const DfcCredential* credential, const DfcApplication* app);

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
// Metadata pointers are invalidated when an application's key sets are resized
// or released, or when another application's metadata is compacted.
uint8_t* dfc_credential_key_version_in_set(
    DfcCredential* credential,
    DfcApplication* application,
    size_t key_set_number,
    size_t slot);
const uint8_t* dfc_credential_key_version_in_set_const(
    const DfcCredential* credential,
    const DfcApplication* application,
    size_t key_set_number,
    size_t slot);
uint8_t* dfc_credential_key_set_version(
    DfcCredential* credential,
    DfcApplication* application,
    size_t key_set_number);
const uint8_t* dfc_credential_key_set_version_const(
    const DfcCredential* credential,
    const DfcApplication* application,
    size_t key_set_number);
uint8_t* dfc_credential_key_set_type(
    DfcCredential* credential,
    DfcApplication* application,
    size_t key_set_number);
const uint8_t* dfc_credential_key_set_type_const(
    const DfcCredential* credential,
    const DfcApplication* application,
    size_t key_set_number);
bool dfc_credential_key_set_is_initialized(
    const DfcCredential* credential,
    const DfcApplication* application,
    size_t key_set_number);
bool dfc_credential_key_set_set_initialized(
    DfcCredential* credential,
    DfcApplication* application,
    size_t key_set_number,
    bool initialized);
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

// Copy the complete pointer-free model, including both pools and their slice
// metadata. Whole-model assignment ensures future inline fields are included.
void dfc_credential_copy_model(DfcCredential* dst, const DfcCredential* src);

// Validate all semantic rules and internal pool bookkeeping before a model is
// copied, encoded, or exposed through a foreign-runtime interface.
DfcModelStatus dfc_credential_validate_model(const DfcCredential* credential);

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
