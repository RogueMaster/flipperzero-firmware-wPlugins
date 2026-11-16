#pragma once

// While this sentinel owns pending_chain, ISO-DEP consumes its logical response.
// Native AdditionalFrame must never interpret it as a frame byte count.
#define DFC_ISO_DEP_RESPONSE_PENDING SIZE_MAX

#include "dfc_credential.h"
#include "dfc_secure_messaging.h"
#include "dfc_transaction_mac_i.h"

#define DFC_SDM_READ_CACHE_SIZE          192
#define DFC_TRANSACTION_FILE_BITMAP_SIZE ((DFC_MAX_FILES + 7u) / 8u)

#if DFC_FILE_POOL_SIZE <= UINT16_MAX
typedef uint16_t DfcTransactionSnapshotOffset;
typedef uint16_t DfcTransactionSnapshotRecordCount;
#else
typedef uint32_t DfcTransactionSnapshotOffset;
typedef uint32_t DfcTransactionSnapshotRecordCount;
#endif
_Static_assert(
    sizeof(DfcTransactionSnapshotOffset) == (DFC_FILE_POOL_SIZE <= UINT16_MAX ? 2u : 4u),
    "transaction journal offset width must follow file-pool capacity");
_Static_assert(
    sizeof(DfcTransactionSnapshotRecordCount) == (DFC_FILE_POOL_SIZE <= UINT16_MAX ? 2u : 4u),
    "transaction journal record-count width must follow file-pool capacity");

typedef enum {
    DfcEmulatorSelectedApplicationNone,
    DfcEmulatorSelectedApplicationPicc,
    DfcEmulatorSelectedApplicationApp,
} DfcEmulatorSelectedApplication;

typedef struct {
    uint8_t random_b[16];
    // Ciphertext sent in step one supplies the continuation IV.
    uint8_t encrypted_random_b[16];
} DfcPendingNativeAuthentication;

#if DFC_ENABLE_EV2_SECURE_MESSAGING
typedef struct {
    uint8_t static_key[DFC_AES_KEY_LENGTH];
    uint8_t random_b[DFC_EV2_RANDOM_LENGTH];
    uint8_t transaction_identifier[DFC_EV2_TRANSACTION_IDENTIFIER_LENGTH];
    uint8_t card_capabilities[DFC_EV2_CAPABILITY_LENGTH];
    uint8_t reader_capabilities[DFC_EV2_CAPABILITY_LENGTH];
} DfcPendingEv2Authentication;
#endif

// Only one authentication handshake can await its continuation. Keeping every
// protocol's secrets in named arms gives cleanup one complete storage extent.
typedef union {
    DfcPendingNativeAuthentication native;
#if DFC_ENABLE_EV2_SECURE_MESSAGING
    DfcPendingEv2Authentication ev2;
#endif
#if DFC_ENABLE_ISO7816_AUTH
    struct {
        uint8_t iso_auth_card_first[DFC_ISO7816_AUTH_CHALLENGE_LONG];
        uint8_t iso_auth_host_first[DFC_ISO7816_AUTH_CHALLENGE_LONG];
        uint8_t iso_auth_external_iv[DFC_AES_KEY_LENGTH];
        uint8_t iso_auth_key[DFC_MAX_KEY_LEN];
    };
#endif
} DfcPendingAuthenticationStorage;
_Static_assert(
    sizeof(DfcPendingAuthenticationStorage) >= sizeof(DfcPendingNativeAuthentication),
    "pending authentication storage must hold native authentication");

typedef struct {
    DfcByteBuf* tx_buffer;
    DfcSecureMessaging* secure_messaging;
    DfcCredential* credential;
    uint8_t* command_chain;
#if DFC_ENABLE_TRANSACTIONAL_DATA_FILES
    uint8_t* transaction_snapshot_pool;
#endif

#if DFC_ENABLE_ISO7816_AUTH
    size_t iso_auth_key_len;
#endif
    size_t pending_chain_len;
    size_t pending_chain_offset;
    // Octets each further frame of the pending chain carries: whole entries of
    // a listing, or whole cipher blocks under secure messaging.
    size_t pending_chain_frame;
    // Largest a final frame may carry. A secured answer keeps its MAC on the
    // last frame, so its final frame runs to the full limit above the block size.
    size_t pending_chain_last;
    size_t command_chain_len;
    size_t command_chain_expected;
    size_t df_names_next;
    size_t selected_app_index;
#if DFC_ENABLE_TRANSACTIONAL_DATA_FILES
    size_t transaction_snapshot_app_index;
    size_t transaction_snapshot_pool_length;
    // Allocation size is tracked separately because delete operations shrink
    // the logical snapshot without reallocating it.
    size_t transaction_snapshot_pool_capacity;
#endif
#if DFC_ENABLE_PROXIMITY_CHECK
    size_t proximity_offset;
    size_t proximity_published_len;
    size_t proximity_transcript_len;
#endif
#if DFC_ENABLE_SDM
    size_t sdm_read_cache_len;
#endif
#if DFC_ENABLE_TRANSACTION_MAC
    bool transaction_reader_id_pending;
    bool transaction_reader_id_persist;
#endif
#if DFC_ENABLE_DELEGATED_APPLICATIONS
    size_t delegated_creation_header_length;
#endif

    DfcEmulatorSelectedApplication selected_application;
#if DFC_ENABLE_TRANSACTION_TIMER
    uint32_t transaction_timer_elapsed_milliseconds;
#endif
#if DFC_ENABLE_TRANSACTIONAL_DATA_FILES
    DfcTransactionSnapshotRecordCount transaction_snapshot_record_counts[DFC_MAX_FILES];
    // Offsets into transaction_snapshot_pool for lazily captured preimages.
    DfcTransactionSnapshotOffset transaction_snapshot_offsets[DFC_MAX_FILES];
    uint32_t transaction_record_numbers[DFC_MAX_FILES];
    // 0 none, 1 WriteRecord, 2 UpdateRecord, 3 ClearRecordFile.
    uint8_t transaction_record_operations[DFC_MAX_FILES];
    uint8_t transaction_snapshot_captured[DFC_TRANSACTION_FILE_BITMAP_SIZE];
    uint8_t transaction_snapshot_created[DFC_TRANSACTION_FILE_BITMAP_SIZE];
    bool transaction_snapshot_active;
    bool transaction_snapshot_dirty;
#endif
    bool pending_chain_zero;
#if DFC_ENABLE_EV2_SECURE_MESSAGING
    uint16_t ev2_command_counter;
    bool ev2_response_plain;
    bool ev2_authentication_pending;
    bool ev2_authentication_non_first;
    bool ev2_session_active;
    uint8_t ev2_authenticated_key_no;
    uint8_t ev2_pending_authenticated_key_no;
    uint8_t ev2_transaction_identifier[DFC_EV2_TRANSACTION_IDENTIFIER_LENGTH];
    uint8_t ev2_card_capabilities[DFC_EV2_CAPABILITY_LENGTH];
    uint8_t ev2_reader_capabilities[DFC_EV2_CAPABILITY_LENGTH];
    uint8_t ev2_session_encryption_key[DFC_AES_KEY_LENGTH];
    uint8_t ev2_session_mac_key[DFC_AES_KEY_LENGTH];
#endif

    uint8_t auth_cipher;
    uint8_t auth_key_no;
    uint8_t pending_auth_cipher;
    uint8_t pending_auth_key_no;
    // Native and ISO authentication continuations cannot be live together:
    // either family rejects and retires the other before accepting its next
    // step. Control fields remain outside so wiping secrets cannot change the
    // state machine that owns them.
    DfcPendingAuthenticationStorage pending_authentication;
    bool awaiting_step2;
#if DFC_ENABLE_ISO7816_AUTH
    // 0 idle, 1 challenge issued, 2 external authentication succeeded.
    uint8_t iso_auth_phase;
    uint8_t iso_auth_challenge_len;
    uint8_t iso_auth_key_no;
    uint8_t iso_auth_cipher;
    uint8_t iso_auth_reference;
#endif
    uint8_t get_version_frame;
    // Chained native response residual (after the first frame).
    uint8_t pending_chain[DFC_EMULATOR_CHAIN_BUFFER_SIZE];
    // Variable-size records, such as DF names, retain their boundaries here.
    uint8_t pending_chain_chunks[DFC_MAX_APPS];
    uint8_t pending_chain_chunk_count;
    uint8_t pending_chain_chunk_index;
    // Native command chaining reassembles one logical write before secure
    // messaging and file handlers inspect it.
    bool command_chain_active;
    // GetDFNames answers one application per frame; the next one to send.
    bool df_names_pending;
    bool data_written; // set on any successful WriteData this emulation session
    // Set when a handler has already applied response secure messaging, so the
    // dispatcher's pass leaves the frame alone. A chained response is secured
    // once, as a whole, before it is split.
    bool response_secured;
    bool first_frame_logged;

#if DFC_ENABLE_TRANSACTION_TIMER
    bool transaction_timer_enabled;
#endif

#if DFC_ENABLE_PROXIMITY_CHECK
    bool proximity_active;
    bool proximity_verified;
    uint8_t proximity_random[DFC_PROXIMITY_RANDOM_LENGTH];
    uint8_t proximity_published[1 + DFC_PROXIMITY_PUBLISHED_TIME_LENGTH + 1];
    uint8_t proximity_transcript[DFC_PROXIMITY_TRANSCRIPT_MAX];
#endif

#if DFC_ENABLE_VIRTUAL_CARD
    bool virtual_card_authentication_expected;
    bool virtual_card_selection_pending;
    bool virtual_card_selected;
    uint8_t virtual_card_challenge[DFC_VIRTUAL_CARD_CHALLENGE_LENGTH];
    uint8_t virtual_card_clear_data[DFC_VIRTUAL_CARD_CLEAR_DATA_LENGTH];
#endif

#if DFC_ENABLE_SDM
    bool sdm_read_cache_valid;
    uint8_t sdm_read_cache_file_number;
    uint8_t sdm_read_cache[DFC_SDM_READ_CACHE_SIZE];
#endif
#if DFC_ENABLE_TRANSACTION_MAC
    DfcTransactionMacAccumulator transaction_mac;
    bool transaction_mac_retryable_failure;
    uint8_t transaction_pending_reader_id[DFC_TRANSACTION_READER_ID_LENGTH];
#endif

#if DFC_ENABLE_DELEGATED_APPLICATIONS
    bool delegated_creation_pending;
    uint8_t delegated_creation_header[DFC_DELEGATED_CREATE_MAX_HEADER_LENGTH];
#endif
} DfcEmulator;

#if DFC_ENABLE_ISO7816_AUTH
_Static_assert(
    offsetof(DfcPendingAuthenticationStorage, native) ==
        offsetof(DfcPendingAuthenticationStorage, iso_auth_card_first),
    "native and ISO pending authentication must share storage");
#endif

#if DFC_ENABLE_EMULATOR

DfcEmulator* dfc_emulator_alloc(DfcCredential* credential);

void dfc_emulator_free(DfcEmulator* dfc_emulator);

void dfc_emulator_reset_session(DfcEmulator* emulator);

// Retire the active authentication without disturbing unrelated command state.
void dfc_emulator_clear_active_authentication(DfcEmulator* emulator);
#if DFC_ENABLE_EV2_SECURE_MESSAGING
// EV2 NonFirst needs the transaction identifier and capabilities to finish, but
// the old keys stop authorizing commands as soon as the handshake is accepted.
void dfc_emulator_retire_ev2_non_first_authentication(DfcEmulator* emulator);
#endif

// Restore the emulator to its power-on PICC state without replacing its
// allocated object or response buffer. This is safe for bounded firmware
// lifecycle paths where allocation is forbidden.
void dfc_emulator_reset_activation(DfcEmulator* emulator);

void dfc_emulator_advance_time(DfcEmulator* emulator, uint32_t elapsed_milliseconds);
#if DFC_ENABLE_SDM
bool dfc_emulator_render_sdm_read(DfcEmulator* emulator, DfcFile* file);
#endif

// Handles one native DESFire command frame (`buffer`/`buffer_len`, cmd byte first) and
// writes the PICC response into tx_buffer. Returns false if the frame could not be handled.
// `context` is passed back to dfc_port_notify for user interface progress events.
// The engine never inspects it. Pass NULL to skip event dispatch.
bool dfc_emulator_handle_command(
    DfcEmulator* emulator,
    const uint8_t* buffer,
    size_t buffer_len,
    DfcByteBuf* tx_buffer,
    void* context);

#endif // DFC_ENABLE_EMULATOR
