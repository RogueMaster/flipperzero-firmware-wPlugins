#pragma once

#include "dfc_credential.h"
#include "dfc_secure_messaging.h"

typedef enum {
    DfcEmulatorSelectedApplicationNone,
    DfcEmulatorSelectedApplicationPicc,
    DfcEmulatorSelectedApplicationApp,
} DfcEmulatorSelectedApplication;

typedef struct {
    DfcByteBuf* tx_buffer;

    uint8_t auth_cipher;
    uint8_t auth_key_no;
    uint8_t rnd_a[16];
    uint8_t rnd_b[16];
    uint8_t enc_rnd_b[16]; // ciphertext PICC sent in step 1, needed to chain the IV in step 2
    bool awaiting_step2;
    uint8_t get_version_frame;
    // Chained native response residual (after first 54-byte frame).
    uint8_t pending_chain[DFC_WORKER_MAX_BUFFER_SIZE];
    size_t pending_chain_len;
    size_t pending_chain_offset;
    bool data_written; // set on any successful WriteData this emulation session
    // Set when a handler has already applied response secure messaging, so the
    // dispatcher's pass leaves the frame alone. A chained response is secured
    // once, as a whole, before it is split.
    bool response_secured;
    bool first_frame_logged;
    DfcEmulatorSelectedApplication selected_application;
    size_t selected_app_index;

#if DFC_ENABLE_TRANSACTIONAL_DATA_FILES
    bool transaction_snapshot_active;
    size_t transaction_snapshot_app_index;
    size_t transaction_snapshot_pool_length;
    uint8_t transaction_snapshot_pool[DFC_FILE_POOL_SIZE];
    uint32_t transaction_snapshot_record_counts[DFC_MAX_FILES];
#endif

#if DFC_ENABLE_TRANSACTION_TIMER
    bool transaction_timer_enabled;
    uint32_t transaction_timer_elapsed_milliseconds;
#endif

    DfcSecureMessaging* secure_messaging;

#if DFC_ENABLE_PROXIMITY_CHECK
    bool proximity_active;
    bool proximity_verified;
    size_t proximity_offset;
    uint8_t proximity_random[DFC_PROXIMITY_RANDOM_LENGTH];
    uint8_t proximity_published[1 + DFC_PROXIMITY_PUBLISHED_TIME_LENGTH + 1];
    size_t proximity_published_len;
    uint8_t proximity_transcript[DFC_PROXIMITY_TRANSCRIPT_MAX];
    size_t proximity_transcript_len;
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
    uint8_t sdm_read_cache[DFC_SM_MAX_SIZE];
    size_t sdm_read_cache_len;
#endif
#if DFC_ENABLE_TRANSACTION_MAC
    uint8_t transaction_input[DFC_TRANSACTION_INPUT_MAX_LENGTH];
    size_t transaction_input_len;
#endif

#if DFC_ENABLE_DELEGATED_APPLICATIONS
    bool delegated_creation_pending;
    uint8_t delegated_creation_header[DFC_DELEGATED_CREATE_MAX_HEADER_LENGTH];
    size_t delegated_creation_header_length;
#endif

#if DFC_ENABLE_EV2_SECURE_MESSAGING
    bool ev2_response_plain;
    bool ev2_authentication_pending;
    bool ev2_authentication_non_first;
    bool ev2_session_active;
    uint8_t ev2_authenticated_key_no;
    uint8_t ev2_static_key[DFC_AES_KEY_LENGTH];
    uint8_t ev2_random_a[DFC_EV2_RANDOM_LENGTH];
    uint8_t ev2_random_b[DFC_EV2_RANDOM_LENGTH];
    uint8_t ev2_transaction_identifier[DFC_EV2_TRANSACTION_IDENTIFIER_LENGTH];
    uint8_t ev2_card_capabilities[DFC_EV2_CAPABILITY_LENGTH];
    uint8_t ev2_reader_capabilities[DFC_EV2_CAPABILITY_LENGTH];
    uint8_t ev2_session_encryption_key[DFC_AES_KEY_LENGTH];
    uint8_t ev2_session_mac_key[DFC_AES_KEY_LENGTH];
    uint16_t ev2_command_counter;
#endif

    DfcCredential* credential;
} DfcEmulator;


DfcEmulator* dfc_emulator_alloc(DfcCredential* credential);

void dfc_emulator_free(DfcEmulator* dfc_emulator);

void dfc_emulator_reset_session(DfcEmulator* emulator);

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
