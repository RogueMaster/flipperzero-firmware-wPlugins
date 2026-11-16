#include "dfc_emulator_commands_i.h"
#include "dfc_credential_i.h"
#include "dfc_ev2.h"
#include "dfc_ev2_crypto.h"

#if DFC_ENABLE_EMULATOR

#define TAG                          DFC_EMULATOR_TAG
#define emulator_accepts_auth_cipher dfc_emulator_accepts_auth_cipher
#define emulator_num_keys            dfc_emulator_num_keys
#define d40_receive_plain            dfc_emulator_d40_receive_plain

static void
    reject_authentication_start(DfcEmulator* emulator, DfcByteBuf* tx_buffer, uint8_t status) {
    dfc_emulator_clear_active_authentication(emulator);
    dfc_bytebuf_append_byte(tx_buffer, status);
}

static void clear_pending_authentication_state(DfcEmulator* emulator) {
    emulator->awaiting_step2 = false;
    emulator->pending_auth_cipher = 0;
    emulator->pending_auth_key_no = 0;
#if DFC_ENABLE_EV2_SECURE_MESSAGING
    emulator->ev2_authentication_pending = false;
    emulator->ev2_authentication_non_first = false;
    emulator->ev2_pending_authenticated_key_no = 0;
#endif
    dfc_emulator_clear_pending_authentication_storage(emulator);
}

#if DFC_ENABLE_EV2_SECURE_MESSAGING
typedef struct {
    uint8_t factory[DFC_MAX_KEY_LEN];
    uint8_t random_material[DFC_EV2_RANDOM_LENGTH + DFC_EV2_TRANSACTION_IDENTIFIER_LENGTH];
    uint8_t iv[DFC_AES_KEY_LENGTH];
    uint8_t encrypted[DFC_EV2_RANDOM_LENGTH];
} Ev2StartSecrets;

typedef struct {
    uint8_t iv[DFC_AES_KEY_LENGTH];
    uint8_t clear[DFC_EV2_AUTHENTICATION_RESPONSE_LENGTH];
    uint8_t candidate_encryption_key[DFC_AES_KEY_LENGTH];
    uint8_t candidate_mac_key[DFC_AES_KEY_LENGTH];
    uint8_t response[DFC_EV2_AUTHENTICATION_RESPONSE_LENGTH];
} Ev2ContinuationSecrets;

static void clear_pending_ev2_authentication(DfcEmulator* emulator) {
    clear_pending_authentication_state(emulator);
}

static void fail_pending_ev2_authentication(DfcEmulator* emulator) {
    clear_pending_ev2_authentication(emulator);
    dfc_emulator_clear_active_authentication(emulator);
}

bool dfc_emulator_command_authenticate_ev2_start(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    size_t apdu_len,
    bool non_first,
    DfcByteBuf* tx_buffer) {
    clear_pending_ev2_authentication(emulator);
    if(emulator->credential->card.generation < DfcGenerationEv2) {
        reject_authentication_start(emulator, tx_buffer, DFC_STATUS_ILLEGAL_COMMAND_CODE);
        return true;
    }
    if((non_first && apdu_len != 2) || (!non_first && apdu_len < 3) ||
       (!non_first && apdu_len != (size_t)3 + apdu[2])) {
        reject_authentication_start(emulator, tx_buffer, DFC_STATUS_LENGTH_ERROR);
        return true;
    }
    if(non_first && !emulator->ev2_session_active) {
        reject_authentication_start(emulator, tx_buffer, DFC_STATUS_AUTHENTICATION_ERR);
        return true;
    }
    uint8_t key_no = apdu[1];
#if DFC_ENABLE_SPECIAL_KEY_STATUS
    if(dfc_emulator_command_picc_disabled_special_key(emulator, key_no)) {
        reject_authentication_start(emulator, tx_buffer, DFC_STATUS_PERMISSION_DENIED);
        return true;
    }
#endif
    if(!dfc_emulator_command_is_available_authentication_key(emulator, key_no) ||
       !emulator_accepts_auth_cipher(
           emulator,
           non_first ? DFC_CMD_AUTHENTICATE_EV2_NON_FIRST : DFC_CMD_AUTHENTICATE_EV2_FIRST)) {
        reject_authentication_start(emulator, tx_buffer, DFC_STATUS_NO_SUCH_KEY);
        return true;
    }

    Ev2StartSecrets secrets;
    memset(secrets.iv, 0, sizeof(secrets.iv));
    bool result = true;
    size_t key_len = 0;
    const uint8_t* key = dfc_emulator_command_auth_key(
        emulator, key_no, secrets.factory, sizeof(secrets.factory), &key_len);
    if(key_len != DFC_AES_KEY_LENGTH) {
        reject_authentication_start(emulator, tx_buffer, DFC_STATUS_AUTHENTICATION_ERR);
        goto ev2_start_cleanup;
    }
    size_t random_material_len = non_first ? DFC_EV2_RANDOM_LENGTH :
                                             sizeof(secrets.random_material);
    if(!dfc_random_fill(secrets.random_material, random_material_len)) {
        if(!dfc_random_failure_is_retryable()) dfc_emulator_clear_active_authentication(emulator);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_MEMORY_ERROR);
        result = false;
        goto ev2_start_cleanup;
    }
    const uint8_t* random_b = secrets.random_material;
    const uint8_t* transaction_identifier = secrets.random_material + DFC_EV2_RANDOM_LENGTH;
    if(!dfc_worker_aes_cbc_encrypt(
           key, key_len, secrets.iv, DFC_EV2_RANDOM_LENGTH, random_b, secrets.encrypted)) {
        dfc_emulator_clear_active_authentication(emulator);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_MEMORY_ERROR);
        result = false;
        goto ev2_start_cleanup;
    }

    if(non_first) {
        dfc_emulator_retire_ev2_non_first_authentication(emulator);
    } else {
        dfc_emulator_clear_active_authentication(emulator);
    }

    memcpy(emulator->pending_authentication.ev2.static_key, key, DFC_AES_KEY_LENGTH);
    emulator->ev2_pending_authenticated_key_no = key_no;
    emulator->ev2_authentication_non_first = non_first;
    emulator->ev2_authentication_pending = true;
    memcpy(emulator->pending_authentication.ev2.random_b, random_b, DFC_EV2_RANDOM_LENGTH);
    if(!non_first) {
        memcpy(
            emulator->pending_authentication.ev2.transaction_identifier,
            transaction_identifier,
            DFC_EV2_TRANSACTION_IDENTIFIER_LENGTH);
#if DFC_ENABLE_APPLICATION_CAPABILITY_DATA
        DfcApplication* app = dfc_emulator_current_app(emulator);
        if(app && app->has_capability_data) {
            memcpy(
                emulator->pending_authentication.ev2.card_capabilities,
                app->capability_data + DFC_APPLICATION_CARD_CAPABILITY_OFFSET,
                DFC_EV2_CAPABILITY_LENGTH);
        } else
#endif
        {
            memset(
                emulator->pending_authentication.ev2.card_capabilities,
                0,
                sizeof(emulator->pending_authentication.ev2.card_capabilities));
            if(emulator->selected_application == DfcEmulatorSelectedApplicationPicc &&
               emulator->credential->picc_has_ev2_capabilities) {
                memcpy(
                    emulator->pending_authentication.ev2.card_capabilities,
                    emulator->credential->picc_ev2_capabilities,
                    DFC_EV2_CAPABILITY_LENGTH);
            }
        }
        memset(
            emulator->pending_authentication.ev2.reader_capabilities,
            0,
            sizeof(emulator->pending_authentication.ev2.reader_capabilities));
        size_t reader_capability_len = apdu[2];
        if(reader_capability_len > DFC_EV2_CAPABILITY_LENGTH) {
            reader_capability_len = DFC_EV2_CAPABILITY_LENGTH;
        }
        memcpy(
            emulator->pending_authentication.ev2.reader_capabilities,
            apdu + 3,
            reader_capability_len);
    }

    dfc_bytebuf_append_byte(tx_buffer, DFC_CMD_ADDITIONAL_FRAME);
    dfc_bytebuf_append_bytes(tx_buffer, secrets.encrypted, sizeof(secrets.encrypted));

ev2_start_cleanup:
    dfc_secure_wipe(&secrets, sizeof(secrets));
    return result;
}

void dfc_emulator_command_authenticate_ev2_continuation(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    size_t apdu_len,
    DfcByteBuf* tx_buffer) {
    if(!emulator->ev2_authentication_pending) {
        clear_pending_ev2_authentication(emulator);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_AUTHENTICATION_ERR);
        return;
    }
    if(apdu_len != 1 + DFC_EV2_AUTHENTICATION_RESPONSE_LENGTH) {
        fail_pending_ev2_authentication(emulator);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_AUTHENTICATION_ERR);
        return;
    }
    Ev2ContinuationSecrets secrets;
    memset(secrets.iv, 0, sizeof(secrets.iv));
    memset(secrets.response, 0, sizeof(secrets.response));
    bool authentication_complete = false;
    if(!dfc_worker_aes_cbc_decrypt(
           emulator->pending_authentication.ev2.static_key,
           DFC_AES_KEY_LENGTH,
           secrets.iv,
           sizeof(secrets.clear),
           apdu + 1,
           secrets.clear)) {
        fail_pending_ev2_authentication(emulator);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_MEMORY_ERROR);
        goto ev2_continuation_cleanup;
    }
    // The response is not assembled until authentication succeeds, so use its
    // first block as comparison scratch while validating the reader token.
    dfc_ev2_rotate_left(emulator->pending_authentication.ev2.random_b, secrets.response);
    if(!dfc_ct_equal(
           secrets.clear + DFC_EV2_RANDOM_LENGTH, secrets.response, DFC_EV2_RANDOM_LENGTH)) {
        fail_pending_ev2_authentication(emulator);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_AUTHENTICATION_ERR);
        goto ev2_continuation_cleanup;
    }
    if(!dfc_ev2_derive_session_keys(
           emulator->pending_authentication.ev2.static_key,
           secrets.clear,
           emulator->pending_authentication.ev2.random_b,
           secrets.candidate_encryption_key,
           secrets.candidate_mac_key)) {
        fail_pending_ev2_authentication(emulator);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_MEMORY_ERROR);
        goto ev2_continuation_cleanup;
    }

    // Session derivation has consumed RndA, so it can now become RndA' in
    // place. This keeps the candidate keys intact until publication.
    dfc_rotate_left(secrets.clear, DFC_EV2_RANDOM_LENGTH);
    size_t response_len = DFC_EV2_RANDOM_LENGTH;
    if(emulator->ev2_authentication_non_first) {
        memcpy(secrets.response, secrets.clear, DFC_EV2_RANDOM_LENGTH);
    } else {
        memcpy(
            secrets.response,
            emulator->pending_authentication.ev2.transaction_identifier,
            DFC_EV2_TRANSACTION_IDENTIFIER_LENGTH);
        memcpy(
            secrets.response + DFC_EV2_TRANSACTION_IDENTIFIER_LENGTH,
            secrets.clear,
            DFC_EV2_RANDOM_LENGTH);
        memcpy(
            secrets.response + DFC_EV2_TRANSACTION_IDENTIFIER_LENGTH + DFC_EV2_RANDOM_LENGTH,
            emulator->pending_authentication.ev2.card_capabilities,
            DFC_EV2_CAPABILITY_LENGTH);
        memcpy(
            secrets.response + DFC_EV2_TRANSACTION_IDENTIFIER_LENGTH + DFC_EV2_RANDOM_LENGTH +
                DFC_EV2_CAPABILITY_LENGTH,
            emulator->pending_authentication.ev2.reader_capabilities,
            DFC_EV2_CAPABILITY_LENGTH);
        response_len = sizeof(secrets.response);
    }
    memset(secrets.iv, 0, sizeof(secrets.iv));
    if(!dfc_worker_aes_cbc_encrypt(
           emulator->pending_authentication.ev2.static_key,
           DFC_AES_KEY_LENGTH,
           secrets.iv,
           response_len,
           secrets.response,
           secrets.response)) {
        fail_pending_ev2_authentication(emulator);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_MEMORY_ERROR);
        goto ev2_continuation_cleanup;
    }
    memcpy(
        emulator->ev2_session_encryption_key,
        secrets.candidate_encryption_key,
        sizeof(secrets.candidate_encryption_key));
    memcpy(
        emulator->ev2_session_mac_key,
        secrets.candidate_mac_key,
        sizeof(secrets.candidate_mac_key));
    emulator->ev2_authenticated_key_no = emulator->ev2_pending_authenticated_key_no;
    if(!emulator->ev2_authentication_non_first) {
        memcpy(
            emulator->ev2_transaction_identifier,
            emulator->pending_authentication.ev2.transaction_identifier,
            sizeof(emulator->ev2_transaction_identifier));
        memcpy(
            emulator->ev2_card_capabilities,
            emulator->pending_authentication.ev2.card_capabilities,
            sizeof(emulator->ev2_card_capabilities));
        memcpy(
            emulator->ev2_reader_capabilities,
            emulator->pending_authentication.ev2.reader_capabilities,
            sizeof(emulator->ev2_reader_capabilities));
    }
    emulator->ev2_session_active = true;
    emulator->ev2_command_counter = 0;
    dfc_emulator_commit_auth_family(emulator, DfcEmulatorAuthFamilyEv2);
    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
    dfc_bytebuf_append_bytes(tx_buffer, secrets.response, response_len);
    authentication_complete = true;

ev2_continuation_cleanup:
    dfc_secure_wipe(&secrets, sizeof(secrets));
    if(authentication_complete) clear_pending_ev2_authentication(emulator);
}
#endif

// A 3K3DES key answers ISO authentication with sixteen-octet challenges over
// its eight-octet block, which is what its session-key derivation reads.
static size_t auth_challenge_length(uint8_t cipher, size_t key_len) {
    if(cipher == DFC_CMD_AUTHENTICATE_AES) return 16;
    if(cipher == DFC_CMD_AUTHENTICATE_ISO && key_len == 24) return 16;
    return 8;
}

static void clear_pending_authentication(DfcEmulator* emulator) {
    clear_pending_authentication_state(emulator);
}

static void fail_pending_authentication(DfcEmulator* emulator) {
    clear_pending_authentication(emulator);
    dfc_emulator_clear_active_authentication(emulator);
}

typedef struct {
    uint8_t factory[DFC_MAX_KEY_LEN];
    uint8_t random_b[DFC_EV2_RANDOM_LENGTH];
    uint8_t iv[16];
    uint8_t encrypted[16];
} AuthStartSecrets;

typedef struct {
    uint8_t factory[DFC_MAX_KEY_LEN];
    union {
        struct {
            uint8_t iv[16];
            uint8_t plain[32];
        } exchange;
        DfcSecureMessaging candidate;
    } phase;
    uint8_t encrypted[16];
    uint8_t session_key[DFC_MAX_KEY_LEN];
} AuthStep2Secrets;

bool dfc_emulator_abort_pending_native_work(DfcEmulator* emulator) {
    if(!emulator || (!emulator->command_chain_active &&
                     !dfc_emulator_command_has_pending_additional_work(emulator))) {
        return false;
    }

    const bool authentication_pending = emulator->awaiting_step2
#if DFC_ENABLE_EV2_SECURE_MESSAGING
                                        || emulator->ev2_authentication_pending
#endif
        ;
    dfc_emulator_discard_command_chain(emulator);
    clear_pending_authentication_state(emulator);
    if(authentication_pending) dfc_emulator_clear_active_authentication(emulator);
#if DFC_ENABLE_DELEGATED_APPLICATIONS
    emulator->delegated_creation_pending = false;
    emulator->delegated_creation_header_length = 0;
    dfc_secure_wipe(
        emulator->delegated_creation_header, sizeof(emulator->delegated_creation_header));
#endif
    dfc_emulator_command_clear_pending_chain(emulator);
    return true;
}

void dfc_emulator_command_authenticate_step1(
    DfcEmulator* emulator,
    uint8_t cipher,
    const uint8_t* apdu,
    DfcByteBuf* tx_buffer) {
    uint8_t key_no = apdu[1];
    clear_pending_authentication(emulator);

#if DFC_ENABLE_SPECIAL_KEY_STATUS
    if(dfc_emulator_command_picc_disabled_special_key(emulator, key_no)) {
        reject_authentication_start(emulator, tx_buffer, DFC_STATUS_PERMISSION_DENIED);
        return;
    }
#endif

    if(key_no >= emulator_num_keys(emulator)) {
        DFC_LOG_W(TAG, "Authenticate: key_no %u beyond the key count", key_no);
        reject_authentication_start(emulator, tx_buffer, DFC_STATUS_NO_SUCH_KEY);
        return;
    }
    if(!emulator_accepts_auth_cipher(emulator, cipher)) {
        // A handshake the key type does not answer is an authentication failure,
        // not a missing key: the two statuses say different things and the key
        // number here is perfectly valid.
        DFC_LOG_W(TAG, "Authenticate: cipher %02X not this key type's", cipher);
        reject_authentication_start(emulator, tx_buffer, DFC_STATUS_AUTHENTICATION_ERR);
        return;
    }

    AuthStartSecrets secrets;
    memset(secrets.iv, 0, sizeof(secrets.iv));
    size_t key_len = 0;
    const uint8_t* key = dfc_emulator_command_auth_key(
        emulator, key_no, secrets.factory, sizeof(secrets.factory), &key_len);
    size_t challenge_len = auth_challenge_length(cipher, key_len);
    if(!dfc_random_fill(secrets.random_b, challenge_len)) {
        if(!dfc_random_failure_is_retryable()) dfc_emulator_clear_active_authentication(emulator);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_MEMORY_ERROR);
        goto auth_start_cleanup;
    }

    bool encrypted_ok =
        cipher == DFC_CMD_AUTHENTICATE_AES ?
            dfc_worker_aes_cbc_encrypt(
                key, key_len, secrets.iv, challenge_len, secrets.random_b, secrets.encrypted) :
            dfc_worker_des_cbc_encrypt(
                key, key_len, secrets.iv, challenge_len, secrets.random_b, secrets.encrypted);
    if(!encrypted_ok) {
        dfc_emulator_clear_active_authentication(emulator);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_MEMORY_ERROR);
        goto auth_start_cleanup;
    }
    dfc_emulator_clear_active_authentication(emulator);
    memcpy(emulator->pending_authentication.native.random_b, secrets.random_b, challenge_len);
    memcpy(
        emulator->pending_authentication.native.encrypted_random_b,
        secrets.encrypted,
        challenge_len);

    emulator->pending_auth_cipher = cipher;
    emulator->pending_auth_key_no = key_no;
    emulator->awaiting_step2 = true;

    dfc_bytebuf_append_byte(tx_buffer, DFC_CMD_ADDITIONAL_FRAME);
    dfc_bytebuf_append_bytes(tx_buffer, secrets.encrypted, challenge_len);

auth_start_cleanup:
    dfc_secure_wipe(&secrets, sizeof(secrets));
}

void dfc_emulator_command_authenticate_step2(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    size_t apdu_len,
    DfcByteBuf* tx_buffer,
    Dfc* dfc) {
    if(!emulator->awaiting_step2) {
        DFC_LOG_W(TAG, "Authenticate step 2: no authentication pending");
        clear_pending_authentication(emulator);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_AUTHENTICATION_ERR);
        return;
    }
    uint8_t cipher = emulator->pending_auth_cipher;
    size_t block_size = dfc_block_size_for_cipher(cipher);

    AuthStep2Secrets secrets;
    memset(secrets.phase.exchange.iv, 0, sizeof(secrets.phase.exchange.iv));
    size_t key_len = 0;
    const uint8_t* key = dfc_emulator_command_auth_key(
        emulator,
        emulator->pending_auth_key_no,
        secrets.factory,
        sizeof(secrets.factory),
        &key_len);
    size_t challenge_len = auth_challenge_length(cipher, key_len);

    if(apdu_len != 1 + challenge_len * 2) {
        DFC_LOG_W(TAG, "Authenticate step 2: unexpected frame");
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_AUTHENTICATION_ERR);
        fail_pending_authentication(emulator);
        goto auth_step2_cleanup;
    }

    // ISO and AES chain from the last block of the challenge this card sent.
    if(cipher != DFC_CMD_AUTHENTICATE_LEGACY) {
        memcpy(
            secrets.phase.exchange.iv,
            emulator->pending_authentication.native.encrypted_random_b + challenge_len -
                block_size,
            block_size);
    }

    bool decrypted_ok = true;
    if(cipher == DFC_CMD_AUTHENTICATE_AES) {
        decrypted_ok = dfc_worker_aes_cbc_decrypt(
            key,
            key_len,
            secrets.phase.exchange.iv,
            challenge_len * 2,
            apdu + 1,
            secrets.phase.exchange.plain);
    } else if(cipher == DFC_CMD_AUTHENTICATE_LEGACY) {
        decrypted_ok = d40_receive_plain(
            key, key_len, apdu + 1, challenge_len * 2, secrets.phase.exchange.plain);
    } else {
        decrypted_ok = dfc_worker_des_cbc_decrypt(
            key,
            key_len,
            secrets.phase.exchange.iv,
            challenge_len * 2,
            apdu + 1,
            secrets.phase.exchange.plain);
    }
    if(!decrypted_ok) {
        fail_pending_authentication(emulator);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_MEMORY_ERROR);
        goto auth_step2_cleanup;
    }

    uint8_t* rnd_a = secrets.phase.exchange.plain;
    uint8_t* rnd_b_rot = secrets.phase.exchange.plain + challenge_len;

    // This scratch becomes the derived session key after the challenge check.
    memcpy(secrets.session_key, emulator->pending_authentication.native.random_b, challenge_len);
    dfc_rotate_left(secrets.session_key, challenge_len);

    if(!dfc_ct_equal(rnd_b_rot, secrets.session_key, challenge_len)) {
        DFC_LOG_W(TAG, "Authenticate step 2: RndB' mismatch");
        fail_pending_authentication(emulator);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_AUTHENTICATION_ERR);
        goto auth_step2_cleanup;
    }

    size_t session_key_len = 0;
    dfc_derive_session_key(
        cipher,
        key,
        key_len,
        rnd_a,
        emulator->pending_authentication.native.random_b,
        secrets.session_key,
        &session_key_len);

    // RndA has been consumed by derivation. Rotate it in place before the
    // response encryption, then retire the exchange arm of the scratch union.
    dfc_rotate_left(rnd_a, challenge_len);
    if(cipher == DFC_CMD_AUTHENTICATE_LEGACY) {
        memset(secrets.phase.exchange.iv, 0, sizeof(secrets.phase.exchange.iv));
    }

    bool response_ok =
        cipher == DFC_CMD_AUTHENTICATE_AES ?
            dfc_worker_aes_cbc_encrypt(
                key, key_len, secrets.phase.exchange.iv, challenge_len, rnd_a, secrets.encrypted) :
            dfc_worker_des_cbc_encrypt(
                key, key_len, secrets.phase.exchange.iv, challenge_len, rnd_a, secrets.encrypted);
    if(!response_ok) {
        fail_pending_authentication(emulator);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_MEMORY_ERROR);
        goto auth_step2_cleanup;
    }

    if(!dfc_secure_messaging_init(
           &secrets.phase.candidate, cipher, secrets.session_key, session_key_len, NULL)) {
        fail_pending_authentication(emulator);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_MEMORY_ERROR);
        goto auth_step2_cleanup;
    }
    if(!emulator->secure_messaging) {
        emulator->secure_messaging =
            dfc_secure_messaging_alloc(cipher, secrets.session_key, session_key_len, NULL);
        if(!emulator->secure_messaging) {
            fail_pending_authentication(emulator);
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_MEMORY_ERROR);
            goto auth_step2_cleanup;
        }
    }
    *emulator->secure_messaging = secrets.phase.candidate;
    emulator->auth_cipher = cipher;
    emulator->auth_key_no = emulator->pending_auth_key_no;
    dfc_emulator_commit_auth_family(emulator, DfcEmulatorAuthFamilyNative);
    clear_pending_authentication(emulator);

    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
    dfc_bytebuf_append_bytes(tx_buffer, secrets.encrypted, challenge_len);

    if(dfc) {
        dfc_port_notify(dfc, DfcEventAuthenticated);
    }

auth_step2_cleanup:
    dfc_secure_wipe(&secrets, sizeof(secrets));
}

#endif
