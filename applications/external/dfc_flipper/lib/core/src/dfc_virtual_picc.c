#include "dfc_virtual_picc.h"
#include "dfc_credential_i.h"
#include "dfc_ev2.h"
#include "dfc_emulator_i.h"
#include "dfc_iso_dep.h"

#if DFC_ENABLE_EMULATOR

#define TAG "DfcVirtualPicc"

static const uint8_t DfcVirtualPiccProtocol[] = {0x02, 0x02};
// The activation a credential carrying no override of its own is presented with,
// used by cards that do not carry explicit activation overrides.
// anything recording what a credential presents records the same, so a stored
// record and the card a reader meets cannot describe different cards.
// Keep the EV1 timing, frame size, CID support and historical byte, but omit
// TA(1): this transport supports 106 kbit/s only and must not invite PPS to a
// rate its tag modulation cannot enter. DESFire permits a configured ATS.
static const uint8_t DfcVirtualPiccAts[] = {0x05, 0x65, 0x81, 0x02, 0x80};
static const uint8_t DfcVirtualPiccAtqa[] = {0x44, 0x03};
static const uint8_t DfcVirtualPiccRandomIdAtqa[] = {0x04, 0x03};
static const uint8_t DfcVirtualPiccRfDetail[] = {0x01, 0x51, 0x57};
#define DFC_VIRTUAL_PICC_SAK 0x20

#define DFC_DIRECT_RESPONSE_PENDING SIZE_MAX

// The ATS this credential answers RATS with: its own if it carries one, the
// built-in default otherwise. A stored ATS reaches this point only after the
// admission check has confirmed its length octet describes it, so nothing here
// can emit a malformed answer.
static const uint8_t* picc_ats(const DfcCredential* credential, size_t* len) {
    if(credential->picc_ats_len > 0) {
        *len = credential->picc_ats_len;
        return credential->picc_ats;
    }
    *len = sizeof(DfcVirtualPiccAts);
    return DfcVirtualPiccAts;
}

static bool write_response(
    const uint8_t* data,
    size_t data_len,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len) {
    if(data_len > response_capacity) {
        return false;
    }

    memcpy(response, data, data_len);
    *response_len = data_len;
    return true;
}

static DfcVirtualPiccStatus write_status_word(
    uint8_t sw1,
    uint8_t sw2,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len) {
    const uint8_t data[] = {sw1, sw2};
    return write_response(data, sizeof(data), response, response_capacity, response_len) ?
               DfcVirtualPiccStatusOk :
               DfcVirtualPiccStatusBufferTooSmall;
}

static bool direct_response_is_pending(const DfcVirtualPiccSession* session) {
    return session->pending_response_offset == DFC_DIRECT_RESPONSE_PENDING;
}

static bool iso_response_is_staged(const DfcVirtualPiccSession* session) {
    return session->emulator->pending_chain_frame == DFC_ISO_DEP_RESPONSE_PENDING;
}

static uint8_t* response_storage(DfcVirtualPiccSession* session) {
    return session->pending_response_staged ? session->emulator->pending_chain :
                                              session->emulator->tx_buffer->data;
}

static void release_iso_response(DfcVirtualPiccSession* session) {
    DfcEmulator* emulator = session->emulator;
    bool emulator_lends_response = iso_response_is_staged(session);
    if(session->pending_response_staged || emulator_lends_response) {
        size_t length = sizeof(emulator->pending_chain);
        if(session->pending_response_staged && emulator_lends_response &&
           session->pending_response_len > 0 &&
           session->pending_response_len == emulator->pending_chain_len &&
           session->pending_response_len <= length)
            length = session->pending_response_len;
        dfc_secure_wipe(emulator->pending_chain, length);
        emulator->pending_chain_len = 0;
        emulator->pending_chain_offset = 0;
        emulator->pending_chain_frame = 0;
    }
    session->pending_response_staged = false;
}

static void truncate_pending_command(DfcVirtualPiccSession* session, size_t retained_len) {
    size_t small_len = session->pending_command_len;
    if(small_len > sizeof(session->pending_command)) small_len = sizeof(session->pending_command);
    if(retained_len < small_len)
        dfc_secure_wipe(session->pending_command + retained_len, small_len - retained_len);
    // ISO reassembly borrows native command storage only outside a native chain.
    if(!session->emulator->command_chain_active && session->emulator->command_chain) {
        if(retained_len == 0) {
            dfc_emulator_discard_command_chain(session->emulator);
        } else if(retained_len < session->pending_command_len) {
            dfc_secure_wipe(
                session->emulator->command_chain + retained_len,
                session->pending_command_len - retained_len);
        }
    }
    session->pending_command_len = retained_len;
}

static void clear_direct_response(DfcVirtualPiccSession* session) {
    truncate_pending_command(session, 0);
    if(!session->pending_response_staged && session->emulator->tx_buffer) {
        dfc_secure_wipe(session->emulator->tx_buffer, sizeof(*session->emulator->tx_buffer));
        dfc_bytebuf_reset(session->emulator->tx_buffer);
    }
    release_iso_response(session);
    session->pending_response_len = 0;
    session->pending_response_offset = 0;
}

static void clear_transport_storage(DfcVirtualPiccSession* session) {
    release_iso_response(session);
    if(!session->emulator->command_chain_active && session->emulator->command_chain)
        dfc_emulator_discard_command_chain(session->emulator);
    dfc_secure_wipe(session->pending_command, sizeof(session->pending_command));
    if(session->emulator->tx_buffer) {
        dfc_secure_wipe(session->emulator->tx_buffer, sizeof(*session->emulator->tx_buffer));
        dfc_bytebuf_reset(session->emulator->tx_buffer);
    }
    dfc_secure_wipe(session->pending_response_prefix, sizeof(session->pending_response_prefix));
    dfc_secure_wipe(session->last_picc_block, sizeof(session->last_picc_block));
    session->pending_command_len = 0;
    session->pending_response_len = 0;
    session->pending_response_offset = 0;
    session->pending_response_prefix_len = 0;
    session->last_picc_block_len = 0;
}

static bool pending_response_is_valid(const DfcVirtualPiccSession* session) {
    if(session->pending_response_prefix_len > sizeof(session->pending_response_prefix) ||
       session->last_picc_block_len > sizeof(session->last_picc_block))
        return false;
    if(!direct_response_is_pending(session) &&
       session->pending_response_offset > session->pending_response_len)
        return false;
    if(session->pending_response_staged) {
        // A reset invalidates the loan; it cannot become an inline response.
        return iso_response_is_staged(session) && session->pending_response_len > 0 &&
               session->pending_response_len == session->emulator->pending_chain_len &&
               session->pending_response_len <= sizeof(session->emulator->pending_chain);
    }
    return session->emulator->tx_buffer && session->pending_response_len <= DFC_BYTEBUF_MAX &&
           (session->pending_response_len == 0 ||
            session->emulator->tx_buffer->size_bytes == session->pending_response_len);
}

static bool validate_pending_response(DfcVirtualPiccSession* session) {
    if(pending_response_is_valid(session)) return true;
    clear_transport_storage(session);
    return false;
}

static DfcVirtualPiccStatus replay_direct_response(
    DfcVirtualPiccSession* session,
    const uint8_t* command,
    size_t command_len,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len) {
    if(command_len != session->pending_command_len ||
       memcmp(command, session->pending_command, command_len) != 0) {
        return DfcVirtualPiccStatusProtocolError;
    }
    if(response_capacity < session->pending_response_len)
        return DfcVirtualPiccStatusBufferTooSmall;
    memcpy(response, response_storage(session), session->pending_response_len);
    *response_len = session->pending_response_len;
    clear_direct_response(session);
    return DfcVirtualPiccStatusOk;
}

static bool stage_direct_response(
    DfcVirtualPiccSession* session,
    const uint8_t* command,
    size_t command_len,
    const uint8_t* native_response,
    size_t native_response_len,
    bool iso_wrapped) {
    if(!command || command_len > sizeof(session->pending_command) || native_response_len == 0 ||
       !session->emulator->tx_buffer)
        return false;
    size_t required = native_response_len + (iso_wrapped ? 1 : 0);
    if(required > DFC_BYTEBUF_MAX) return false;

    DfcByteBuf* tx = session->emulator->tx_buffer;
    if(iso_wrapped) {
        size_t wrapped_len = 0;
        if(!dfc_wrap_native_response_as_iso7816(
               native_response, native_response_len, 0, tx->data, sizeof(tx->data), &wrapped_len) ||
           wrapped_len != required)
            return false;
    } else {
        memmove(tx->data, native_response, native_response_len);
    }
    tx->size_bytes = required;
    memcpy(session->pending_command, command, command_len);
    session->pending_command_len = command_len;
    session->pending_response_len = required;
    session->pending_response_offset = DFC_DIRECT_RESPONSE_PENDING;
    session->pending_response_prefix_len = 0;
    session->pending_response_staged = false;
    return true;
}

#if DFC_ENABLE_ISO7816_AUTH
static DfcVirtualPiccStatus
    iso_auth_status(uint8_t sw1, uint8_t sw2, uint8_t* response, size_t capacity, size_t* length) {
    return write_status_word(sw1, sw2, response, capacity, length);
}

static void iso_auth_abort_on_unexpected_command(
    DfcEmulator* emulator,
    const uint8_t* command,
    size_t command_len) {
    if(emulator->iso_auth_phase == 0) return;
    uint8_t expected = emulator->iso_auth_phase == 1 ? DFC_ISO7816_INS_EXTERNAL_AUTHENTICATE :
                                                       DFC_ISO7816_INS_INTERNAL_AUTHENTICATE;
    if(command_len < 4 || command[0] != DFC_ISO7816_CLA_STANDARD || command[1] != expected)
        dfc_emulator_clear_iso_authentication(emulator);
}

static bool iso_auth_key(
    DfcEmulator* emulator,
    uint8_t algorithm,
    uint8_t reference,
    size_t challenge_len) {
    DfcCredential* credential = emulator->credential;
    DfcApplication* app = dfc_emulator_current_app(emulator);
    bool app_reference = (reference & DFC_ISO7816_AUTH_APP_REFERENCE) != 0;
    if((app != NULL) != app_reference ||
       (reference &
        (uint8_t)~(DFC_ISO7816_AUTH_APP_REFERENCE | DFC_ISO7816_AUTH_KEY_NUMBER_MASK)) != 0) {
        return false;
    }
    uint8_t key_no = reference & DFC_ISO7816_AUTH_KEY_NUMBER_MASK;
    if((app && key_no >= app->num_keys) || (!app && key_no != 0)) return false;

    uint8_t key_type = (app ? app->key_settings_2 : credential->picc_key_settings_2) &
                       DFC_KEY_TYPE_MASK;
    uint8_t cipher;
    size_t key_len;
    if(key_type == DFC_KEY_TYPE_AES) {
        cipher = DFC_CMD_AUTHENTICATE_AES;
        key_len = DFC_AES_KEY_LENGTH;
        if(challenge_len != DFC_ISO7816_AUTH_CHALLENGE_LONG ||
           (algorithm != DFC_ISO7816_AUTH_ALGORITHM_CONTEXT &&
            algorithm != DFC_ISO7816_AUTH_ALGORITHM_AES))
            return false;
    } else if(key_type == DFC_KEY_TYPE_3K3DES) {
        cipher = DFC_CMD_AUTHENTICATE_ISO;
        key_len = DFC_MAX_KEY_LEN;
        if(challenge_len != DFC_ISO7816_AUTH_CHALLENGE_LONG ||
           (algorithm != DFC_ISO7816_AUTH_ALGORITHM_CONTEXT &&
            algorithm != DFC_ISO7816_AUTH_ALGORITHM_3TDEA))
            return false;
    } else {
        cipher = DFC_CMD_AUTHENTICATE_ISO;
        key_len = DFC_AES_KEY_LENGTH;
        if(challenge_len != DFC_ISO7816_AUTH_CHALLENGE_2TDEA ||
           (algorithm != DFC_ISO7816_AUTH_ALGORITHM_CONTEXT &&
            algorithm != DFC_ISO7816_AUTH_ALGORITHM_2TDEA))
            return false;
    }
    uint8_t commands = app ? dfc_credential_app_auth_commands(credential, app) :
                             dfc_credential_picc_auth_commands(credential);
    uint8_t disabled = credential->picc_has_sm_disable ? credential->picc_sm_disable : 0;
    if(app && app->has_sm_disable) disabled |= app->sm_disable;
    if(!(commands & DFC_AUTH_COMMAND_ISO7816) || (disabled & DFC_SM_DISABLE_EV1)) return false;
    const uint8_t* key = dfc_credential_key(credential, app, key_no);
    if(key)
        memcpy(emulator->pending_authentication.iso_auth_key, key, key_len);
    else
        memset(emulator->pending_authentication.iso_auth_key, 0, key_len);
    emulator->iso_auth_key_len = key_len;
    emulator->iso_auth_cipher = cipher;
    emulator->iso_auth_key_no = key_no;
    emulator->iso_auth_reference = reference;
    return true;
}

static DfcVirtualPiccStatus handle_iso_external_authenticate(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    size_t apdu_len,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len) {
    if(emulator->iso_auth_phase != 1) {
        return iso_auth_status(
            DFC_ISO7816_SW_INSTRUCTION_HI,
            DFC_ISO7816_SW_INSTRUCTION_LO,
            response,
            response_capacity,
            response_len);
    }
    size_t challenge_len = emulator->iso_auth_challenge_len;
    if(apdu_len < 5 || apdu_len != (size_t)5 + apdu[4]) {
        dfc_emulator_clear_iso_authentication(emulator);
        return iso_auth_status(
            DFC_ISO7816_SW_WRONG_LENGTH_HI,
            DFC_ISO7816_SW_WRONG_LENGTH_LO,
            response,
            response_capacity,
            response_len);
    }
    if(apdu[4] != challenge_len * 2 || !iso_auth_key(emulator, apdu[2], apdu[3], challenge_len)) {
        dfc_emulator_clear_iso_authentication(emulator);
        return iso_auth_status(
            DFC_ISO7816_SW_SECURITY_HI,
            DFC_ISO7816_SW_SECURITY_LO,
            response,
            response_capacity,
            response_len);
    }

    uint8_t clear[DFC_ISO7816_AUTH_CHALLENGE_LONG * 2];
    uint8_t iv[DFC_AES_KEY_LENGTH] = {0};
    DfcVirtualPiccStatus result;
    bool decrypted = emulator->iso_auth_cipher == DFC_CMD_AUTHENTICATE_AES ?
                         dfc_worker_aes_cbc_decrypt(
                             emulator->pending_authentication.iso_auth_key,
                             emulator->iso_auth_key_len,
                             iv,
                             challenge_len * 2,
                             apdu + 5,
                             clear) :
                         dfc_worker_des_cbc_decrypt(
                             emulator->pending_authentication.iso_auth_key,
                             emulator->iso_auth_key_len,
                             iv,
                             challenge_len * 2,
                             apdu + 5,
                             clear);
    if(!decrypted) {
        result = DfcVirtualPiccStatusProtocolError;
        goto iso_external_cleanup;
    }
    if(!dfc_ct_equal(
           clear + challenge_len,
           emulator->pending_authentication.iso_auth_card_first,
           challenge_len)) {
        dfc_emulator_clear_iso_authentication(emulator);
        result = iso_auth_status(
            DFC_ISO7816_SW_SECURITY_HI,
            DFC_ISO7816_SW_SECURITY_LO,
            response,
            response_capacity,
            response_len);
        goto iso_external_cleanup;
    }
    memcpy(emulator->pending_authentication.iso_auth_host_first, clear, challenge_len);
    size_t block_len = emulator->iso_auth_cipher == DFC_CMD_AUTHENTICATE_AES ?
                           DFC_AES_KEY_LENGTH :
                           DFC_SM_LEGACY_BLOCK_SIZE;
    memcpy(
        emulator->pending_authentication.iso_auth_external_iv,
        apdu + 5 + challenge_len * 2 - block_len,
        block_len);
    emulator->iso_auth_phase = 2;
    result = iso_auth_status(
        DFC_ISO7816_SW_OK_HI, DFC_ISO7816_SW_OK_LO, response, response_capacity, response_len);

iso_external_cleanup:
    dfc_secure_wipe(clear, sizeof(clear));
    dfc_secure_wipe(iv, sizeof(iv));
    return result;
}

static DfcVirtualPiccStatus handle_iso_internal_authenticate(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    size_t apdu_len,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len) {
    if(emulator->iso_auth_phase != 2) {
        return iso_auth_status(
            DFC_ISO7816_SW_INSTRUCTION_HI,
            DFC_ISO7816_SW_INSTRUCTION_LO,
            response,
            response_capacity,
            response_len);
    }
    size_t challenge_len = emulator->iso_auth_challenge_len;
    if(apdu_len != challenge_len + 6 || apdu[4] != challenge_len || apdu[apdu_len - 1] != 0) {
        dfc_emulator_clear_iso_authentication(emulator);
        return iso_auth_status(
            DFC_ISO7816_SW_WRONG_LENGTH_HI,
            DFC_ISO7816_SW_WRONG_LENGTH_LO,
            response,
            response_capacity,
            response_len);
    }
    if(apdu[3] != emulator->iso_auth_reference ||
       !iso_auth_key(emulator, apdu[2], apdu[3], challenge_len)) {
        dfc_emulator_clear_iso_authentication(emulator);
        return iso_auth_status(
            DFC_ISO7816_SW_SECURITY_HI,
            DFC_ISO7816_SW_SECURITY_LO,
            response,
            response_capacity,
            response_len);
    }
    if(response_capacity < challenge_len * 2 + DFC_ISO7816_STATUS_WORD_LENGTH)
        return DfcVirtualPiccStatusBufferTooSmall;

    uint8_t clear[DFC_ISO7816_AUTH_CHALLENGE_LONG * 2] = {0};
    uint8_t iv[DFC_AES_KEY_LENGTH] = {0};
    uint8_t session_key[DFC_MAX_KEY_LEN] = {0};
    DfcSecureMessaging candidate = {0};
    DfcVirtualPiccStatus result = DfcVirtualPiccStatusProtocolError;
    bool wipe_response = false;
    size_t session_key_len = 0;
    if(!dfc_random_fill(clear, challenge_len)) goto iso_internal_cleanup;
    memcpy(clear + challenge_len, apdu + 5, challenge_len);
    memcpy(
        iv,
        emulator->pending_authentication.iso_auth_external_iv,
        emulator->iso_auth_cipher == DFC_CMD_AUTHENTICATE_AES ? DFC_AES_KEY_LENGTH :
                                                                DFC_SM_LEGACY_BLOCK_SIZE);
    bool encrypted = emulator->iso_auth_cipher == DFC_CMD_AUTHENTICATE_AES ?
                         dfc_worker_aes_cbc_encrypt(
                             emulator->pending_authentication.iso_auth_key,
                             emulator->iso_auth_key_len,
                             iv,
                             challenge_len * 2,
                             clear,
                             response) :
                         dfc_worker_des_cbc_encrypt(
                             emulator->pending_authentication.iso_auth_key,
                             emulator->iso_auth_key_len,
                             iv,
                             challenge_len * 2,
                             clear,
                             response);
    if(!encrypted) {
        wipe_response = true;
        goto iso_internal_cleanup;
    }
    dfc_derive_session_key(
        emulator->iso_auth_cipher,
        emulator->pending_authentication.iso_auth_key,
        emulator->iso_auth_key_len,
        emulator->pending_authentication.iso_auth_host_first,
        clear,
        session_key,
        &session_key_len);
    if(!dfc_secure_messaging_init(
           &candidate, emulator->iso_auth_cipher, session_key, session_key_len, NULL)) {
        wipe_response = true;
        goto iso_internal_cleanup;
    }
    if(!emulator->secure_messaging) {
        emulator->secure_messaging = dfc_secure_messaging_alloc(
            emulator->iso_auth_cipher, session_key, session_key_len, NULL);
        if(!emulator->secure_messaging) {
            wipe_response = true;
            goto iso_internal_cleanup;
        }
    }
    *emulator->secure_messaging = candidate;
    emulator->auth_cipher = emulator->iso_auth_cipher;
    emulator->auth_key_no = emulator->iso_auth_key_no;
    dfc_emulator_commit_auth_family(emulator, DfcEmulatorAuthFamilyNative);
    dfc_emulator_clear_iso_authentication(emulator);
    response[challenge_len * 2] = DFC_ISO7816_SW_OK_HI;
    response[challenge_len * 2 + 1] = DFC_ISO7816_SW_OK_LO;
    *response_len = challenge_len * 2 + DFC_ISO7816_STATUS_WORD_LENGTH;
    result = DfcVirtualPiccStatusOk;

iso_internal_cleanup:
    if(wipe_response) dfc_secure_wipe(response, challenge_len * 2);
    dfc_secure_wipe(clear, sizeof(clear));
    dfc_secure_wipe(iv, sizeof(iv));
    dfc_secure_wipe(session_key, sizeof(session_key));
    dfc_secure_wipe(&candidate, sizeof(candidate));
    return result;
}

static DfcVirtualPiccStatus handle_iso_authenticate(
    DfcVirtualPiccSession* session,
    const uint8_t* apdu,
    size_t apdu_len,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len) {
    DfcEmulator* emulator = session->emulator;
    if(response_capacity < DFC_ISO7816_STATUS_WORD_LENGTH)
        return DfcVirtualPiccStatusBufferTooSmall;
    uint8_t ins = apdu[1];
    if(ins == DFC_ISO7816_INS_GET_CHALLENGE) {
        if(apdu_len != 5)
            return iso_auth_status(0x67, 0x00, response, response_capacity, response_len);
        if(apdu[2] != 0 || apdu[3] != 0)
            return iso_auth_status(0x6A, 0x86, response, response_capacity, response_len);
        size_t challenge_len = apdu[4];
        if(challenge_len != DFC_ISO7816_AUTH_CHALLENGE_2TDEA &&
           challenge_len != DFC_ISO7816_AUTH_CHALLENGE_LONG) {
            return iso_auth_status(
                DFC_ISO7816_SW_WRONG_LE_HI,
                DFC_ISO7816_SW_WRONG_LE_LO,
                response,
                response_capacity,
                response_len);
        }
        if(response_capacity < challenge_len + DFC_ISO7816_STATUS_WORD_LENGTH)
            return DfcVirtualPiccStatusBufferTooSmall;
        uint8_t challenge[DFC_ISO7816_AUTH_CHALLENGE_LONG] = {0};
        DfcVirtualPiccStatus result = DfcVirtualPiccStatusProtocolError;
        if(dfc_random_fill(challenge, challenge_len)) {
            dfc_emulator_clear_iso_authentication(emulator);
            dfc_emulator_clear_active_authentication(emulator);
            memcpy(emulator->pending_authentication.iso_auth_card_first, challenge, challenge_len);
            emulator->iso_auth_challenge_len = (uint8_t)challenge_len;
            emulator->iso_auth_phase = 1;
            memcpy(response, emulator->pending_authentication.iso_auth_card_first, challenge_len);
            response[challenge_len] = DFC_ISO7816_SW_OK_HI;
            response[challenge_len + 1] = DFC_ISO7816_SW_OK_LO;
            *response_len = challenge_len + DFC_ISO7816_STATUS_WORD_LENGTH;
            result = DfcVirtualPiccStatusOk;
        }
        dfc_secure_wipe(challenge, sizeof(challenge));
        return result;
    }

    if(ins == DFC_ISO7816_INS_EXTERNAL_AUTHENTICATE)
        return handle_iso_external_authenticate(
            emulator, apdu, apdu_len, response, response_capacity, response_len);
    return handle_iso_internal_authenticate(
        emulator, apdu, apdu_len, response, response_capacity, response_len);
}
#endif

#if DFC_ENABLE_VIRTUAL_CARD
enum {
    DfcVirtualCardSelectHeaderLength = 5,
    DfcVirtualCardSelectDataOffset = 5,
    DfcVirtualCardSelectLengthOffset = 4,
    DfcVirtualCardFciTag = 0x6F,
    DfcVirtualCardFciLength = 0x22,
    DfcVirtualCardDataTag = 0x85,
    DfcVirtualCardDataLength = 0x20,
    DfcVirtualCardClearInformationOffset = 0,
    DfcVirtualCardClearCapabilityOffset = 1,
    DfcVirtualCardClearUidOffset = 3,
    DfcVirtualCardExternalAuthenticateLength = 13,
    DfcVirtualCardExternalAuthenticateDataOffset = 5,
};

static DfcVirtualPiccStatus handle_virtual_card_select(
    DfcVirtualPiccSession* session,
    const uint8_t* command,
    size_t command_len,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len) {
    DfcCredential* credential = session->credential;
    if(!credential->virtual_card_configured ||
       !credential->virtual_card_authentication_mandatory ||
       command_len < DfcVirtualCardSelectHeaderLength)
        return DfcVirtualPiccStatusUnsupportedProtocol;

    size_t identifier_len = command[DfcVirtualCardSelectLengthOffset];
    if(command_len < DfcVirtualCardSelectDataOffset + identifier_len ||
       identifier_len != credential->virtual_card_installation_id_len ||
       !dfc_ct_equal(
           command + DfcVirtualCardSelectDataOffset,
           credential->virtual_card_installation_id,
           identifier_len))
        return DfcVirtualPiccStatusUnsupportedProtocol;

    const size_t selection_len =
        DFC_VIRTUAL_CARD_CHALLENGE_LENGTH + DFC_VIRTUAL_CARD_CLEAR_DATA_LENGTH;
    const size_t fci_len = 4 + selection_len + DFC_ISO7816_STATUS_WORD_LENGTH;
    if(response_capacity < fci_len) return DfcVirtualPiccStatusBufferTooSmall;

    DfcEmulator* emulator = session->emulator;
    uint8_t virtual_card_challenge[DFC_VIRTUAL_CARD_CHALLENGE_LENGTH] = {0};
    uint8_t virtual_card_clear_data[DFC_VIRTUAL_CARD_CLEAR_DATA_LENGTH] = {0};
    uint8_t clear[DFC_VIRTUAL_CARD_CHALLENGE_LENGTH + DFC_VIRTUAL_CARD_CLEAR_DATA_LENGTH] = {0};
    uint8_t iv[DFC_AES_KEY_LENGTH] = {0};
    uint8_t encrypted[sizeof(clear)] = {0};
    DfcVirtualPiccStatus result = DfcVirtualPiccStatusProtocolError;
    if(!dfc_random_fill(virtual_card_challenge, sizeof(virtual_card_challenge))) {
        goto virtual_card_select_cleanup;
    }
    virtual_card_clear_data[DfcVirtualCardClearInformationOffset] =
        credential->virtual_card_information;
    memcpy(
        virtual_card_clear_data + DfcVirtualCardClearCapabilityOffset,
        credential->virtual_card_capabilities,
        DFC_VIRTUAL_CARD_CAPABILITY_LENGTH);
    memcpy(
        virtual_card_clear_data + DfcVirtualCardClearUidOffset,
        credential->virtual_card_uid,
        credential->virtual_card_uid_len);

    memcpy(clear, virtual_card_challenge, DFC_VIRTUAL_CARD_CHALLENGE_LENGTH);
    memcpy(
        clear + DFC_VIRTUAL_CARD_CHALLENGE_LENGTH,
        virtual_card_clear_data,
        DFC_VIRTUAL_CARD_CLEAR_DATA_LENGTH);
    if(!dfc_worker_aes_cbc_encrypt(
           credential->virtual_card_select_encryption_key,
           DFC_AES_KEY_LENGTH,
           iv,
           sizeof(clear),
           clear,
           encrypted)) {
        goto virtual_card_select_cleanup;
    }

    response[0] = DfcVirtualCardFciTag;
    response[1] = DfcVirtualCardFciLength;
    response[2] = DfcVirtualCardDataTag;
    response[3] = DfcVirtualCardDataLength;
    memcpy(response + 4, encrypted, sizeof(encrypted));
    response[4 + sizeof(encrypted)] = DFC_ISO7816_SW_OK_HI;
    response[5 + sizeof(encrypted)] = DFC_ISO7816_SW_OK_LO;
    *response_len = fci_len;
    memcpy(
        emulator->virtual_card_challenge, virtual_card_challenge, sizeof(virtual_card_challenge));
    memcpy(
        emulator->virtual_card_clear_data,
        virtual_card_clear_data,
        sizeof(virtual_card_clear_data));
    emulator->virtual_card_authentication_expected = true;
    emulator->virtual_card_selection_pending = true;
    result = DfcVirtualPiccStatusOk;

virtual_card_select_cleanup:
    dfc_secure_wipe(clear, sizeof(clear));
    dfc_secure_wipe(encrypted, sizeof(encrypted));
    dfc_secure_wipe(iv, sizeof(iv));
    dfc_secure_wipe(virtual_card_challenge, sizeof(virtual_card_challenge));
    dfc_secure_wipe(virtual_card_clear_data, sizeof(virtual_card_clear_data));
    return result;
}

static DfcVirtualPiccStatus handle_virtual_card_external_authenticate(
    DfcVirtualPiccSession* session,
    const uint8_t* command,
    size_t command_len,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len) {
    DfcEmulator* emulator = session->emulator;
    if(response_capacity < DFC_ISO7816_STATUS_WORD_LENGTH)
        return DfcVirtualPiccStatusBufferTooSmall;
    if(!emulator->virtual_card_authentication_expected)
        return write_status_word(
            DFC_ISO7816_SW_CONDITIONS_NOT_SATISFIED_HI,
            DFC_ISO7816_SW_CONDITIONS_NOT_SATISFIED_LO,
            response,
            response_capacity,
            response_len);
    if(command_len != DfcVirtualCardExternalAuthenticateLength)
        return write_status_word(
            DFC_ISO7816_SW_WRONG_LENGTH_HI,
            DFC_ISO7816_SW_WRONG_LENGTH_LO,
            response,
            response_capacity,
            response_len);

    uint8_t input[DFC_VIRTUAL_CARD_CHALLENGE_LENGTH + DFC_VIRTUAL_CARD_CLEAR_DATA_LENGTH];
    memcpy(input, emulator->virtual_card_challenge, DFC_VIRTUAL_CARD_CHALLENGE_LENGTH);
    memcpy(
        input + DFC_VIRTUAL_CARD_CHALLENGE_LENGTH,
        emulator->virtual_card_clear_data,
        DFC_VIRTUAL_CARD_CLEAR_DATA_LENGTH);
    uint8_t full_mac[DFC_AES_CMAC_LENGTH] = {0};
    uint8_t wire_mac[DFC_WIRE_MAC_LENGTH] = {0};
    DfcVirtualPiccStatus result;
    if(!aes_cmac(
           session->credential->virtual_card_select_mac_key,
           DFC_AES_KEY_LENGTH,
           input,
           sizeof(input),
           full_mac)) {
        emulator->virtual_card_authentication_expected = false;
        emulator->virtual_card_selection_pending = false;
        emulator->virtual_card_selected = false;
        result = DfcVirtualPiccStatusProtocolError;
        goto virtual_card_auth_cleanup;
    }
    dfc_truncate_aes_wire_mac(full_mac, wire_mac);
    emulator->virtual_card_selected =
        emulator->virtual_card_selection_pending &&
        dfc_ct_equal(
            wire_mac, command + DfcVirtualCardExternalAuthenticateDataOffset, DFC_WIRE_MAC_LENGTH);
    emulator->virtual_card_authentication_expected = false;
    emulator->virtual_card_selection_pending = false;
    result = write_status_word(
        DFC_ISO7816_SW_OK_HI, DFC_ISO7816_SW_OK_LO, response, response_capacity, response_len);

virtual_card_auth_cleanup:
    dfc_secure_wipe(input, sizeof(input));
    dfc_secure_wipe(full_mac, sizeof(full_mac));
    dfc_secure_wipe(wire_mac, sizeof(wire_mac));
    return result;
}
#endif

DfcVirtualPiccSession* dfc_virtual_picc_session_alloc(DfcCredential* credential) {
    if(!credential) return NULL;

    DfcVirtualPiccSession* session =
        dfc_platform_alloc(sizeof(DfcVirtualPiccSession), DfcAllocSession);
    if(!session) return NULL;
    memset(session, 0, sizeof(DfcVirtualPiccSession));
    session->credential = credential;
    session->emulator = dfc_emulator_alloc(credential);
    if(!session->emulator) {
        dfc_platform_free(session);
        return NULL;
    }
    return session;
}

void dfc_virtual_picc_session_free(DfcVirtualPiccSession* session) {
    if(!session) return;
    if(session->emulator) {
        dfc_emulator_free(session->emulator);
    }
    dfc_secure_wipe(session, sizeof(*session));
    dfc_platform_free(session);
}

static void clear_protocol(DfcVirtualPiccSession* session) {
    session->iso_dep_selected = false;
    session->iso_dep_cid = 0;
    session->expected_pcd_sequence = 0;
    session->picc_sequence = 0;
    clear_transport_storage(session);
    session->iso_file_selected = false;
    session->iso_file_index = 0;
}

DfcVirtualPiccStatus dfc_virtual_picc_reset_protocol(DfcVirtualPiccSession* session) {
    dfc_random_operation_begin();
    if(!session || !session->emulator) return DfcVirtualPiccStatusProtocolError;

    dfc_emulator_reset_activation(session->emulator);
    clear_protocol(session);
    return DfcVirtualPiccStatusOk;
}

static void clear_activation(DfcVirtualPiccSession* session) {
    clear_protocol(session);
    session->activated = false;
    session->random_uid_valid = false;
    memset(session->random_uid, 0, sizeof(session->random_uid));
}

DfcVirtualPiccStatus
    dfc_virtual_picc_advance_time(DfcVirtualPiccSession* session, uint32_t elapsed_milliseconds) {
    dfc_random_operation_begin();
    if(!session || !session->emulator) return DfcVirtualPiccStatusProtocolError;
#if DFC_ENABLE_TRANSACTION_TIMER
    bool timer_enabled = session->emulator->transaction_timer_enabled;
#endif
    dfc_emulator_advance_time(session->emulator, elapsed_milliseconds);
#if DFC_ENABLE_TRANSACTION_TIMER
    if(timer_enabled && !session->emulator->transaction_timer_enabled) clear_activation(session);
#endif
    return DfcVirtualPiccStatusOk;
}

DfcVirtualPiccStatus dfc_virtual_picc_field_off(DfcVirtualPiccSession* session) {
    dfc_random_operation_begin();
    if(!session || !session->emulator) return DfcVirtualPiccStatusProtocolError;
    dfc_emulator_reset_activation(session->emulator);
    // The next physical activation generates a new random ID.
    clear_activation(session);
    return DfcVirtualPiccStatusOk;
}

static bool virtual_picc_anticollision(
    const DfcCredential* credential,
    DfcVirtualPiccActivation* activation) {
    if(!credential || !activation) return false;

    uint8_t random_uid_tail[DFC_RANDOM_UID_LEN - 1];
    if(credential->picc_random_id && !dfc_random_fill(random_uid_tail, sizeof(random_uid_tail)))
        return false;

    memset(activation, 0, sizeof(DfcVirtualPiccActivation));
    memcpy(activation->protocol, DfcVirtualPiccProtocol, sizeof(DfcVirtualPiccProtocol));
    activation->protocol_len = sizeof(DfcVirtualPiccProtocol);

    if(credential->picc_random_id) {
        // A fresh identifier per activation. The stored UID stays where it is and
        // is reachable only through GetCardUID under an authenticated session.
        activation->uid[0] = credential->picc_random_id_legacy ? DFC_LEGACY_RANDOM_UID_FIRST_BYTE :
                                                                 DFC_RANDOM_UID_FIRST_BYTE;
        memcpy(activation->uid + 1, random_uid_tail, sizeof(random_uid_tail));
        activation->uid_len = DFC_RANDOM_UID_LEN;
    } else {
        memcpy(activation->uid, credential->uid, credential->uid_len);
        activation->uid_len = credential->uid_len;
    }

    size_t ats_len = 0;
    const uint8_t* ats = picc_ats(credential, &ats_len);
    memcpy(activation->ats, ats, ats_len);
    activation->ats_len = ats_len;

    activation->sak = credential->picc_has_sak ? credential->picc_sak : DFC_VIRTUAL_PICC_SAK;
    if(credential->picc_has_atqa) {
        memcpy(activation->atqa, credential->picc_atqa, sizeof(credential->picc_atqa));
    } else {
        memcpy(
            activation->atqa,
            credential->picc_random_id ? DfcVirtualPiccRandomIdAtqa : DfcVirtualPiccAtqa,
            sizeof(DfcVirtualPiccAtqa));
    }
    activation->atqa_len = sizeof(DfcVirtualPiccAtqa);
    memcpy(activation->rf_detail, DfcVirtualPiccRfDetail, sizeof(DfcVirtualPiccRfDetail));
    activation->rf_detail_len = sizeof(DfcVirtualPiccRfDetail);
    return true;
}

bool dfc_virtual_picc_anticollision(
    const DfcCredential* credential,
    DfcVirtualPiccActivation* activation) {
    dfc_random_operation_begin();
    return virtual_picc_anticollision(credential, activation);
}

DfcVirtualPiccStatus dfc_virtual_picc_scan_iso14443a(
    DfcVirtualPiccSession* session,
    DfcVirtualPiccActivation* activation) {
    dfc_random_operation_begin();
    if(!session || !activation) return DfcVirtualPiccStatusProtocolError;
    if(!dfc_credential_uid_is_detectable(session->credential)) {
        return DfcVirtualPiccStatusNoCard;
    }

    if(!virtual_picc_anticollision(session->credential, activation))
        return DfcVirtualPiccStatusProtocolError;
    if(session->credential->picc_random_id) {
        memcpy(session->random_uid, activation->uid, DFC_RANDOM_UID_LEN);
        session->random_uid_valid = true;
    } else {
        session->random_uid_valid = false;
    }
    session->activated = true;
    return DfcVirtualPiccStatusOk;
}

static bool is_rats(const uint8_t* frame, size_t frame_len) {
    return frame_len >= 2 && (frame[0] & 0xF0) == DFC_ISO_DEP_RATS;
}

static bool is_i_block(uint8_t pcb) {
    return (pcb & DFC_ISO_DEP_BLOCK_MASK) == 0x00;
}

static bool is_r_block(uint8_t pcb) {
    return (pcb & DFC_ISO_DEP_BLOCK_MASK) == 0x80;
}

static bool is_s_block(uint8_t pcb) {
    return (pcb & DFC_ISO_DEP_BLOCK_MASK) == 0xC0;
}

static bool iso_dep_cid_matches(
    const DfcVirtualPiccSession* session,
    const uint8_t* frame,
    size_t frame_len) {
    if(frame[0] & DFC_ISO_DEP_CID) {
        return frame_len >= 2 && frame[1] == session->iso_dep_cid;
    }
    return session->iso_dep_cid == 0;
}

static DfcVirtualPiccStatus build_iso_dep_i_block(
    DfcVirtualPiccSession* session,
    const uint8_t* inf,
    size_t inf_len,
    bool more,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len) {
    if(response_capacity < inf_len + 1) return DfcVirtualPiccStatusBufferTooSmall;
    response[0] =
        (uint8_t)(DFC_ISO_DEP_I_BLOCK | (session->picc_sequence & DFC_ISO_DEP_SEQUENCE_BIT));
    if(more) response[0] |= DFC_ISO_DEP_MORE;
    if(inf_len > 0) {
        memcpy(response + 1, inf, inf_len);
    }
    *response_len = inf_len + 1;
    session->picc_sequence ^= DFC_ISO_DEP_SEQUENCE_BIT;
    return DfcVirtualPiccStatusOk;
}

static DfcVirtualPiccStatus build_iso_dep_r_block(
    uint8_t sequence,
    bool nak,
    bool use_cid,
    uint8_t cid,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len) {
    size_t block_len = use_cid ? 2 : 1;
    if(response_capacity < block_len) return DfcVirtualPiccStatusBufferTooSmall;
    response[0] = (uint8_t)(DFC_ISO_DEP_R_BLOCK | (sequence & DFC_ISO_DEP_SEQUENCE_BIT));
    if(nak) response[0] |= DFC_ISO_DEP_R_NAK;
    if(use_cid) {
        response[0] |= DFC_ISO_DEP_CID;
        response[1] = cid;
    }
    *response_len = block_len;
    return DfcVirtualPiccStatusOk;
}

static DfcVirtualPiccStatus write_next_iso_dep_response_block(
    DfcVirtualPiccSession* session,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len) {
    if(!validate_pending_response(session)) return DfcVirtualPiccStatusProtocolError;
    if(session->pending_response_prefix_len > 0) {
        if(session->pending_response_offset >= session->pending_response_len) {
            if(response_capacity < session->pending_response_prefix_len)
                return DfcVirtualPiccStatusBufferTooSmall;
            memcpy(
                response, session->pending_response_prefix, session->pending_response_prefix_len);
            response[0] = (uint8_t)(DFC_ISO_DEP_I_BLOCK |
                                    (session->picc_sequence & DFC_ISO_DEP_SEQUENCE_BIT) |
                                    (session->pending_response_prefix[0] & DFC_ISO_DEP_CID));
            *response_len = session->pending_response_prefix_len;
            session->pending_response_prefix_len = 0;
            session->picc_sequence ^= DFC_ISO_DEP_SEQUENCE_BIT;
            return DfcVirtualPiccStatusOk;
        }

        size_t remaining = session->pending_response_len - session->pending_response_offset;
        size_t chunk_len = remaining > DFC_ISO_DEP_MAX_INF ? DFC_ISO_DEP_MAX_INF : remaining;
        bool more = (session->pending_response_offset + chunk_len) < session->pending_response_len;
        if(response_capacity < session->pending_response_prefix_len + chunk_len)
            return DfcVirtualPiccStatusBufferTooSmall;

        memcpy(response, session->pending_response_prefix, session->pending_response_prefix_len);
        response[0] =
            (uint8_t)(DFC_ISO_DEP_I_BLOCK | (session->picc_sequence & DFC_ISO_DEP_SEQUENCE_BIT) |
                      (session->pending_response_prefix[0] & DFC_ISO_DEP_CID));
        if(more) response[0] |= DFC_ISO_DEP_MORE;
        memcpy(
            response + session->pending_response_prefix_len,
            response_storage(session) + session->pending_response_offset,
            chunk_len);
        *response_len = session->pending_response_prefix_len + chunk_len;
        session->pending_response_offset += chunk_len;
        session->picc_sequence ^= DFC_ISO_DEP_SEQUENCE_BIT;
        if(!more) {
            release_iso_response(session);
            session->pending_response_len = 0;
            session->pending_response_offset = 0;
            session->pending_response_prefix_len = 0;
        }
        return DfcVirtualPiccStatusOk;
    }

    if(session->pending_response_offset >= session->pending_response_len) {
        return build_iso_dep_i_block(
            session, NULL, 0, false, response, response_capacity, response_len);
    }

    size_t remaining = session->pending_response_len - session->pending_response_offset;
    size_t chunk_len = remaining > DFC_ISO_DEP_MAX_INF ? DFC_ISO_DEP_MAX_INF : remaining;
    bool more = (session->pending_response_offset + chunk_len) < session->pending_response_len;
    DfcVirtualPiccStatus status = build_iso_dep_i_block(
        session,
        response_storage(session) + session->pending_response_offset,
        chunk_len,
        more,
        response,
        response_capacity,
        response_len);
    if(status != DfcVirtualPiccStatusOk) return status;
    session->pending_response_offset += chunk_len;
    if(!more) {
        release_iso_response(session);
        session->pending_response_len = 0;
        session->pending_response_offset = 0;
    }
    return DfcVirtualPiccStatusOk;
}

static DfcVirtualPiccStatus write_and_remember_iso_dep_response_block(
    DfcVirtualPiccSession* session,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len) {
    DfcVirtualPiccStatus status =
        write_next_iso_dep_response_block(session, response, response_capacity, response_len);
    if(status == DfcVirtualPiccStatusOk && *response_len <= sizeof(session->last_picc_block)) {
        memcpy(session->last_picc_block, response, *response_len);
        session->last_picc_block_len = *response_len;
    }
    return status;
}

static bool iso_dep_initial_response_is_staged(const DfcVirtualPiccSession* session) {
    return session->pending_command_len > 0 && session->pending_response_len > 0 &&
           session->pending_response_offset != DFC_DIRECT_RESPONSE_PENDING;
}

static DfcVirtualPiccStatus replay_staged_iso_dep_response(
    DfcVirtualPiccSession* session,
    const uint8_t* frame,
    size_t frame_len,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len) {
    if(frame_len != session->pending_command_len ||
       memcmp(frame, session->pending_command, frame_len) != 0) {
        return DfcVirtualPiccStatusProtocolError;
    }
    DfcVirtualPiccStatus status = write_and_remember_iso_dep_response_block(
        session, response, response_capacity, response_len);
    if(status == DfcVirtualPiccStatusOk) {
        truncate_pending_command(session, 0);
        session->expected_pcd_sequence ^= DFC_ISO_DEP_SEQUENCE_BIT;
    }
    return status;
}

static bool
    standard_iso_files_blocked_by_transaction_mac(DfcCredential* credential, size_t app_index) {
    return dfc_credential_find_transaction_mac_file(credential, app_index) != NULL;
}

static DfcVirtualPiccStatus write_iso_conditions_not_satisfied(
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len) {
    return write_status_word(
        DFC_ISO7816_SW_CONDITIONS_NOT_SATISFIED_HI,
        DFC_ISO7816_SW_CONDITIONS_NOT_SATISFIED_LO,
        response,
        response_capacity,
        response_len);
}

static DfcVirtualPiccStatus handle_iso_select(
    DfcVirtualPiccSession* session,
    const uint8_t* command,
    size_t command_len,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len) {
    if(response_capacity < DFC_ISO7816_STATUS_WORD_LENGTH)
        return DfcVirtualPiccStatusBufferTooSmall;
    uint8_t p1 = command[2];
    uint8_t lc = (command_len > 4) ? command[4] : 0;
    const uint8_t* data = (lc > 0 && command_len >= (size_t)(5 + lc)) ? &command[5] : NULL;

    if(p1 == DFC_ISO7816_SELECT_PARENT && lc == 0) {
        session->iso_file_selected = false;
        return write_status_word(
            DFC_ISO7816_SW_OK_HI, DFC_ISO7816_SW_OK_LO, response, response_capacity, response_len);
    }
    if((p1 == DFC_ISO7816_SELECT_PATH_FROM_MF || p1 == DFC_ISO7816_SELECT_PATH_FROM_DF)) {
        return write_status_word(
            DFC_ISO7816_SW_WRONG_PARAMETERS_HI,
            DFC_ISO7816_SW_WRONG_PARAMETERS_LO,
            response,
            response_capacity,
            response_len);
    }
    if(!data) {
        return write_status_word(
            DFC_ISO7816_SW_NOT_FOUND_HI,
            DFC_ISO7816_SW_NOT_FOUND_LO,
            response,
            response_capacity,
            response_len);
    }

    if(p1 == DFC_ISO7816_SELECT_BY_DF_NAME) {
        for(size_t i = 0; i < session->credential->num_apps; i++) {
            DfcApplication* app = &session->credential->apps[i];
            if(app->iso_aid_len > 0 && lc == app->iso_aid_len &&
               memcmp(data, app->iso_aid, lc) == 0) {
                if(standard_iso_files_blocked_by_transaction_mac(session->credential, i))
                    return write_iso_conditions_not_satisfied(
                        response, response_capacity, response_len);
                dfc_emulator_select_application(
                    session->emulator, DfcEmulatorSelectedApplicationApp, i);
                session->iso_file_selected = false;
                return write_status_word(
                    DFC_ISO7816_SW_OK_HI,
                    DFC_ISO7816_SW_OK_LO,
                    response,
                    response_capacity,
                    response_len);
            }
        }

        if(lc == sizeof(DFC_ISO_AID) && memcmp(data, DFC_ISO_AID, lc) == 0) {
            dfc_emulator_select_application(
                session->emulator, DfcEmulatorSelectedApplicationPicc, 0);
            session->iso_file_selected = false;
            return write_status_word(
                DFC_ISO7816_SW_OK_HI,
                DFC_ISO7816_SW_OK_LO,
                response,
                response_capacity,
                response_len);
        }
    }

    if((p1 == DFC_ISO7816_SELECT_BY_FILE_ID || p1 == DFC_ISO7816_SELECT_CHILD_DF) &&
       lc == DFC_ISO7816_STATUS_WORD_LENGTH) {
        uint16_t file_id = (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
        if(file_id == DFC_ISO7816_MASTER_FILE_ID) {
            dfc_emulator_select_application(
                session->emulator, DfcEmulatorSelectedApplicationPicc, 0);
            session->iso_file_selected = false;
            return write_status_word(
                DFC_ISO7816_SW_OK_HI,
                DFC_ISO7816_SW_OK_LO,
                response,
                response_capacity,
                response_len);
        }
        for(size_t i = 0; i < session->credential->num_apps; i++) {
            DfcApplication* app = &session->credential->apps[i];
            if(app->has_iso_file_id && app->iso_file_id == file_id) {
                if(standard_iso_files_blocked_by_transaction_mac(session->credential, i))
                    return write_iso_conditions_not_satisfied(
                        response, response_capacity, response_len);
                dfc_emulator_select_application(
                    session->emulator, DfcEmulatorSelectedApplicationApp, i);
                session->iso_file_selected = false;
                return write_status_word(
                    DFC_ISO7816_SW_OK_HI,
                    DFC_ISO7816_SW_OK_LO,
                    response,
                    response_capacity,
                    response_len);
            }
        }
    }

    if((p1 == DFC_ISO7816_SELECT_BY_FILE_ID || p1 == DFC_ISO7816_SELECT_CHILD_EF) &&
       lc == DFC_ISO7816_STATUS_WORD_LENGTH &&
       session->emulator->selected_application == DfcEmulatorSelectedApplicationApp) {
        if(standard_iso_files_blocked_by_transaction_mac(
               session->credential, session->emulator->selected_app_index))
            return write_iso_conditions_not_satisfied(response, response_capacity, response_len);
        uint16_t file_id = (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
        for(size_t i = 0; i < session->credential->num_files; i++) {
            DfcFile* file = &session->credential->files[i];
            if(file->app_index == session->emulator->selected_app_index && file->has_iso_file_id &&
               file->iso_file_id == file_id) {
                session->iso_file_selected = true;
                session->iso_file_index = i;
                return write_status_word(
                    DFC_ISO7816_SW_OK_HI,
                    DFC_ISO7816_SW_OK_LO,
                    response,
                    response_capacity,
                    response_len);
            }
        }
    }

    dfc_emulator_reset_session(session->emulator);
    return write_status_word(
        DFC_ISO7816_SW_NOT_FOUND_HI,
        DFC_ISO7816_SW_NOT_FOUND_LO,
        response,
        response_capacity,
        response_len);
}

static DfcVirtualPiccStatus
    write_iso_security_status(uint8_t* response, size_t response_capacity, size_t* response_len) {
    return write_status_word(
        DFC_ISO7816_SW_SECURITY_HI,
        DFC_ISO7816_SW_SECURITY_LO,
        response,
        response_capacity,
        response_len);
}

static bool
    iso_file_read_comm(DfcVirtualPiccSession* session, const DfcFile* file, uint8_t* comm) {
    if(!dfc_emulator_file_access_allowed(session->emulator, file, DfcFileAccessRead)) return false;
    if(!dfc_emulator_native_authentication_active(session->emulator) &&
       dfc_emulator_file_access_is_free(file, DfcFileAccessRead)) {
        *comm = DFC_COMM_PLAIN;
        return true;
    }
    *comm = dfc_emulator_file_effective_comm_settings(
        session->emulator, file, DfcFileAccessRead, file->comm_settings);
    if(session->emulator->secure_messaging &&
       session->emulator->secure_messaging->cipher != DFC_CMD_AUTHENTICATE_LEGACY &&
       *comm != DFC_COMM_ENCIPHERED)
        *comm = DFC_COMM_MAC;
    return *comm != DFC_COMM_ENCIPHERED &&
           (*comm != DFC_COMM_MAC || dfc_emulator_native_authentication_active(session->emulator));
}

static size_t iso_file_mac_len(const DfcSecureMessaging* sm) {
    return sm->cipher == DFC_CMD_AUTHENTICATE_LEGACY ? DFC_SM_LEGACY_MAC_LENGTH :
                                                       DFC_WIRE_MAC_LENGTH;
}

static bool iso_file_read_fits(
    const DfcVirtualPiccSession* session,
    uint8_t comm,
    size_t data_len,
    size_t response_capacity) {
    size_t mac_len = comm == DFC_COMM_MAC ? iso_file_mac_len(session->emulator->secure_messaging) :
                                            0;
    return response_capacity >= DFC_ISO7816_STATUS_WORD_LENGTH + mac_len &&
           data_len <= response_capacity - DFC_ISO7816_STATUS_WORD_LENGTH - mac_len;
}

static bool iso_file_read_length(
    const DfcVirtualPiccSession* session,
    uint8_t comm,
    uint8_t le,
    size_t available,
    size_t* data_len,
    bool* short_eof) {
    *short_eof = false;
    if(le == 0) {
        *data_len = available;
        return true;
    }
    size_t requested = le;
    if(comm == DFC_COMM_MAC) {
        size_t mac_len = iso_file_mac_len(session->emulator->secure_messaging);
        if(requested == mac_len && session->credential->card.generation == DfcGenerationEv1) {
            *data_len = available;
            return true;
        }
        if(requested <= mac_len) return false;
        requested -= mac_len;
    }
    // EV1 returns the remaining bytes with an EOF warning; later generations complete normally.
    *short_eof = session->credential->card.generation == DfcGenerationEv1 && requested > available;
    *data_len = DFC_MIN(requested, available);
    return true;
}

static DfcVirtualPiccStatus write_iso_file_read_response(
    DfcVirtualPiccSession* session,
    uint8_t comm,
    const uint8_t* data,
    size_t data_len,
    uint8_t status_hi,
    uint8_t status_lo,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len) {
    if(data_len && !data) {
        return write_status_word(
            DFC_ISO7816_SW_NO_DIAGNOSTIC_HI,
            DFC_ISO7816_SW_NO_DIAGNOSTIC_LO,
            response,
            response_capacity,
            response_len);
    }
    size_t mac_len = comm == DFC_COMM_MAC ? iso_file_mac_len(session->emulator->secure_messaging) :
                                            0;
    size_t payload_len = data_len;
    if(comm == DFC_COMM_MAC) {
        if(data_len) memcpy(response, data, data_len);
        // The response MAC authenticates SW2, including warning statuses.
        payload_len = session->emulator->secure_messaging->cipher == DFC_CMD_AUTHENTICATE_LEGACY ?
                          dfc_secure_messaging_generate_d40_response_mac(
                              session->emulator->secure_messaging,
                              data,
                              data_len,
                              response,
                              response_capacity - DFC_ISO7816_STATUS_WORD_LENGTH) :
                          dfc_secure_messaging_generate_ev1_response_in_place(
                              session->emulator->secure_messaging,
                              status_lo,
                              response,
                              data_len,
                              response_capacity - DFC_ISO7816_STATUS_WORD_LENGTH);
        if(payload_len != data_len + mac_len) {
            dfc_emulator_reset_session(session->emulator);
            return write_status_word(
                DFC_ISO7816_SW_NO_DIAGNOSTIC_HI,
                DFC_ISO7816_SW_NO_DIAGNOSTIC_LO,
                response,
                response_capacity,
                response_len);
        }
    } else if(data_len) {
        memcpy(response, data, data_len);
    }
    response[payload_len] = status_hi;
    response[payload_len + 1] = status_lo;
    *response_len = payload_len + DFC_ISO7816_STATUS_WORD_LENGTH;
    return DfcVirtualPiccStatusOk;
}

static DfcVirtualPiccStatus handle_iso_file_command(
    DfcVirtualPiccSession* session,
    const uint8_t* command,
    size_t command_len,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len) {
    if(!session->iso_file_selected || session->iso_file_index >= session->credential->num_files) {
        return write_status_word(
            DFC_ISO7816_SW_NOT_FOUND_HI,
            DFC_ISO7816_SW_NOT_FOUND_LO,
            response,
            response_capacity,
            response_len);
    }
#if DFC_ENABLE_EV2_SECURE_MESSAGING
    if(session->emulator->ev2_session_active)
        return write_iso_security_status(response, response_capacity, response_len);
#endif
    DfcFile* file = &session->credential->files[session->iso_file_index];
    if(standard_iso_files_blocked_by_transaction_mac(session->credential, file->app_index))
        return write_iso_conditions_not_satisfied(response, response_capacity, response_len);
    uint8_t* data = dfc_file_data(session->credential, file);
    if(command[1] == DFC_ISO7816_INS_READ_BINARY &&
       (file->type == DFC_FILE_TYPE_STANDARD_DATA || file->type == DFC_FILE_TYPE_BACKUP_DATA)) {
        uint8_t comm = DFC_COMM_PLAIN;
        if(!iso_file_read_comm(session, file, &comm))
            return write_iso_security_status(response, response_capacity, response_len);
        if(command_len != 5) {
            return write_status_word(
                DFC_ISO7816_SW_WRONG_PARAMETERS_HI,
                DFC_ISO7816_SW_WRONG_PARAMETERS_LO,
                response,
                response_capacity,
                response_len);
        }
        size_t offset = ((size_t)command[2] << 8) | command[3];
        if(offset > file->data_len) {
            return write_status_word(
                DFC_ISO7816_SW_WRONG_PARAMETERS_HI,
                DFC_ISO7816_SW_WRONG_PARAMETERS_LO,
                response,
                response_capacity,
                response_len);
        }
        size_t requested;
        bool short_eof;
        if(!iso_file_read_length(
               session, comm, command[4], file->data_len - offset, &requested, &short_eof)) {
            return write_status_word(
                DFC_ISO7816_SW_WRONG_LE_HI,
                DFC_ISO7816_SW_WRONG_LE_LO,
                response,
                response_capacity,
                response_len);
        }
        if(!iso_file_read_fits(session, comm, requested, response_capacity))
            return DfcVirtualPiccStatusBufferTooSmall;
#if DFC_ENABLE_SDM
        if(dfc_file_sdm_flag_is_set(file, DfcSdmFlagEnabled)) {
            DfcSdmRenderStatus render_status =
                dfc_emulator_render_sdm_read_status(session->emulator, file);
            if(render_status == DfcSdmRenderRandomExhausted)
                return DfcVirtualPiccStatusProtocolError;
            if(render_status != DfcSdmRenderOk) {
                return write_status_word(
                    DFC_ISO7816_SW_WRONG_PARAMETERS_HI,
                    DFC_ISO7816_SW_WRONG_PARAMETERS_LO,
                    response,
                    response_capacity,
                    response_len);
            }
            data = session->emulator->sdm_read_cache;
        }
#endif
        return write_iso_file_read_response(
            session,
            comm,
            data ? data + offset : NULL,
            requested,
            short_eof ? DFC_ISO7816_SW_EOF_BEFORE_LE_HI : DFC_ISO7816_SW_OK_HI,
            short_eof ? DFC_ISO7816_SW_EOF_BEFORE_LE_LO : DFC_ISO7816_SW_OK_LO,
            response,
            response_capacity,
            response_len);
    }

    if(command[1] == DFC_ISO7816_INS_UPDATE_BINARY && command_len >= 5 &&
       (file->type == DFC_FILE_TYPE_STANDARD_DATA || file->type == DFC_FILE_TYPE_BACKUP_DATA)) {
        if(!dfc_emulator_file_access_allowed(session->emulator, file, DfcFileAccessWrite) ||
           file->comm_settings != DFC_COMM_PLAIN) {
            return write_iso_security_status(response, response_capacity, response_len);
        }
        size_t offset = ((size_t)command[2] << 8) | command[3];
        size_t write_len = command[4];
        if(command_len != 5 + write_len || offset > file->data_len ||
           write_len > file->data_len - offset) {
            return write_status_word(
                DFC_ISO7816_SW_WRONG_PARAMETERS_HI,
                DFC_ISO7816_SW_WRONG_PARAMETERS_LO,
                response,
                response_capacity,
                response_len);
        }
        if(response_capacity < DFC_ISO7816_STATUS_WORD_LENGTH)
            return DfcVirtualPiccStatusBufferTooSmall;
        if(write_len > 0 && !data) {
            return write_status_word(
                DFC_ISO7816_SW_WRONG_PARAMETERS_HI,
                DFC_ISO7816_SW_WRONG_PARAMETERS_LO,
                response,
                response_capacity,
                response_len);
        }
        if(write_len > 0) memcpy(data + offset, command + 5, write_len);
        dfc_emulator_publish_transaction(session->emulator);
        return write_status_word(
            DFC_ISO7816_SW_OK_HI, DFC_ISO7816_SW_OK_LO, response, response_capacity, response_len);
    }

    if(command[1] == DFC_ISO7816_INS_READ_RECORD &&
       (file->type == DFC_FILE_TYPE_LINEAR_RECORD || file->type == DFC_FILE_TYPE_CYCLIC_RECORD)) {
        uint8_t comm = DFC_COMM_PLAIN;
        if(!iso_file_read_comm(session, file, &comm))
            return write_iso_security_status(response, response_capacity, response_len);
        size_t record_number = command[2];
        if(command_len != 5 || record_number == 0 || record_number > file->state.record.count) {
            return write_status_word(
                DFC_ISO7816_SW_WRONG_PARAMETERS_HI,
                DFC_ISO7816_SW_WRONG_PARAMETERS_LO,
                response,
                response_capacity,
                response_len);
        }
        size_t requested;
        bool short_eof;
        if(!iso_file_read_length(
               session, comm, command[4], file->state.record.size, &requested, &short_eof)) {
            return write_status_word(
                DFC_ISO7816_SW_WRONG_LE_HI,
                DFC_ISO7816_SW_WRONG_LE_LO,
                response,
                response_capacity,
                response_len);
        }
        if(!iso_file_read_fits(session, comm, requested, response_capacity))
            return DfcVirtualPiccStatusBufferTooSmall;
        const uint8_t* record = data ? data + (record_number - 1) * file->state.record.size : NULL;
        return write_iso_file_read_response(
            session,
            comm,
            record,
            requested,
            DFC_ISO7816_SW_OK_HI,
            DFC_ISO7816_SW_OK_LO,
            response,
            response_capacity,
            response_len);
    }

    return write_status_word(
        DFC_ISO7816_SW_INS_NOT_SUPPORTED_HI,
        DFC_ISO7816_SW_INS_NOT_SUPPORTED_LO,
        response,
        response_capacity,
        response_len);
}

static DfcVirtualPiccStatus handle_native_exchange(
    DfcVirtualPiccSession* session,
    const uint8_t* command,
    size_t command_len,
    uint8_t* command_scratch,
    size_t command_scratch_capacity,
    bool iso_wrapped,
    const uint8_t* retry_command,
    size_t retry_command_len,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len) {
    uint8_t* clear_command = command_scratch;
    size_t clear_capacity = command_scratch_capacity;
    size_t clear_command_len = 0;
    DfcByteBuf* tx = NULL;
    DfcVirtualPiccStatus status = DfcVirtualPiccStatusProtocolError;
    const uint8_t integrity_error[] = {DFC_STATUS_INTEGRITY_ERROR};
    const uint8_t* tx_data = integrity_error;
    size_t tx_len = sizeof(integrity_error);
    release_iso_response(session);
    session->pending_response_len = 0;
    session->pending_response_offset = 0;
    session->pending_response_prefix_len = 0;
    if(command_len > clear_capacity && command[0] == DFC_CMD_WRITE_RECORD_ISO &&
       !session->emulator->command_chain_active && command_len <= DFC_EMULATOR_CHAIN_BUFFER_SIZE) {
        if(!session->emulator->command_chain)
            session->emulator->command_chain =
                dfc_platform_alloc(DFC_EMULATOR_CHAIN_BUFFER_SIZE, DfcAllocCommandChain);
        if(!session->emulator->command_chain) goto cleanup;
        clear_command = session->emulator->command_chain;
        clear_capacity = DFC_EMULATOR_CHAIN_BUFFER_SIZE;
        if(session->emulator->command_chain_len < command_len)
            session->emulator->command_chain_len = command_len;
    }
    DfcEv2CommandSecurity security = dfc_ev2_prepare_command(
        session->emulator, command, command_len, clear_command, clear_capacity, &clear_command_len);
    bool invalid_command = security == DfcEv2CommandInvalid;
    if(invalid_command) {
        dfc_emulator_reset_session(session->emulator);
    } else {
        tx = session->emulator->tx_buffer;
        if(!tx) goto cleanup;
        dfc_bytebuf_reset(tx);
        bool handled = dfc_emulator_handle_transport_command(
            session->emulator, clear_command, clear_command_len, tx);
        DFC_UNUSED(handled);

        tx_data = dfc_bytebuf_get_data(tx);
        tx_len = dfc_bytebuf_get_size_bytes(tx);
        if(tx_len == 0) goto cleanup;
#if DFC_ENABLE_EV2_SECURE_MESSAGING
        if(security == DfcEv2CommandPlainCounted && !session->emulator->command_chain_active &&
           session->emulator->ev2_session_active) {
            if(dfc_random_failure_is_retryable()) {
                // The outer adapter discards this provisional response. Keep
                // the session exactly where a retry will find it.
            } else if(tx_data[0] == DFC_STATUS_OK) {
                session->emulator->ev2_command_counter++;
            } else {
                dfc_emulator_reset_session(session->emulator);
            }
        }
#endif
    }
    // The emulator lends its existing chain buffer until delivery or abort.
    // Prefix status in place so EV2 can protect the complete answer once.
    bool staged_iso_response = iso_response_is_staged(session);
    if(staged_iso_response) {
        DfcEmulator* emulator = session->emulator;
        memmove(emulator->pending_chain + 1, emulator->pending_chain, emulator->pending_chain_len);
        emulator->pending_chain[0] = DFC_STATUS_OK;
        emulator->pending_chain_len++;
        tx_data = emulator->pending_chain;
        tx_len = emulator->pending_chain_len;
    }
    size_t secured_response_len = 0;
    bool protect_ev2_response = security == DfcEv2CommandSecured || security == DfcEv2CommandFull;
#if DFC_ENABLE_EV2_SECURE_MESSAGING
    protect_ev2_response = protect_ev2_response && session->emulator->ev2_session_active &&
                           !session->emulator->ev2_response_plain &&
                           !dfc_random_failure_is_retryable();
#endif
    if(protect_ev2_response) {
        // The emulator response buffer already owns the clear response. Protect
        // it in place so the same storage can be retained for a delivery retry.
        if(!dfc_ev2_protect_response(
               session->emulator,
               clear_command,
               clear_command_len,
               tx_data,
               tx_len,
               staged_iso_response ? session->emulator->pending_chain : tx->data,
               staged_iso_response ? sizeof(session->emulator->pending_chain) : sizeof(tx->data),
               &secured_response_len)) {
            goto cleanup;
        }
        tx_data = staged_iso_response ? session->emulator->pending_chain : tx->data;
        tx_len = secured_response_len;
        if(!staged_iso_response) tx->size_bytes = tx_len;
    }

    status = DfcVirtualPiccStatusOk;
    if(staged_iso_response) {
        uint8_t* staged = session->emulator->pending_chain;
        if(iso_wrapped) {
            uint8_t native_status = staged[0];
            memmove(staged, staged + 1, tx_len - 1);
            staged[tx_len - 1] = 0x91;
            staged[tx_len++] = native_status;
        }
        session->emulator->pending_chain_len = tx_len;
        session->pending_response_staged = true;
        if(retry_command && response_capacity >= tx_len) {
            memcpy(response, staged, tx_len);
            *response_len = tx_len;
            session->pending_response_len = tx_len;
            release_iso_response(session);
            session->pending_response_len = 0;
        } else if(retry_command) {
            if(retry_command_len > sizeof(session->pending_command)) goto cleanup;
            memcpy(session->pending_command, retry_command, retry_command_len);
            session->pending_command_len = retry_command_len;
            session->pending_response_len = tx_len;
            session->pending_response_offset = DFC_DIRECT_RESPONSE_PENDING;
            status = DfcVirtualPiccStatusBufferTooSmall;
        } else {
            // The frame sender reads directly from the lent chain buffer.
            *response_len = tx_len;
        }
        goto cleanup;
    }
    if(!iso_wrapped) {
        status = write_response(tx_data, tx_len, response, response_capacity, response_len) ?
                     DfcVirtualPiccStatusOk :
                     DfcVirtualPiccStatusBufferTooSmall;
    } else {
        status = dfc_wrap_native_response_as_iso7816(
                     tx_data, tx_len, 0, response, response_capacity, response_len) ?
                     DfcVirtualPiccStatusOk :
                     DfcVirtualPiccStatusBufferTooSmall;
    }

    if(status == DfcVirtualPiccStatusBufferTooSmall) {
        status = stage_direct_response(
                     session, retry_command, retry_command_len, tx_data, tx_len, iso_wrapped) ?
                     DfcVirtualPiccStatusBufferTooSmall :
                     DfcVirtualPiccStatusProtocolError;
    }

cleanup:
    if(status != DfcVirtualPiccStatusOk && status != DfcVirtualPiccStatusBufferTooSmall)
        release_iso_response(session);
    dfc_secure_wipe(command_scratch, command_scratch_capacity);
    if(!session->emulator->command_chain_active && session->emulator->command_chain)
        dfc_emulator_discard_command_chain(session->emulator);
    if(tx) {
        bool retain_tx_response =
            (!session->pending_response_staged && session->pending_response_len > 0) ||
            (status == DfcVirtualPiccStatusOk && response == tx->data);
        if(!retain_tx_response) {
            dfc_secure_wipe(tx, sizeof(*tx));
            dfc_bytebuf_reset(tx);
        }
    }
    return status;
}

static DfcVirtualPiccStatus virtual_picc_iso_dep_exchange(
    DfcVirtualPiccSession* session,
    const uint8_t* command,
    size_t command_len,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len,
    bool allow_direct_retry) {
    if(!session || !command || !response || !response_len)
        return DfcVirtualPiccStatusProtocolError;
    if(!session->activated) return DfcVirtualPiccStatusNoCard;
    *response_len = 0;

    // ISO wrapper assembly and native secure-command preparation happen in
    // sequence. One operation-local buffer serves both phases and is wiped by
    // handle_native_exchange after the prepared command is no longer needed.
    uint8_t command_scratch[DFC_WORKER_MAX_BUFFER_SIZE];

    if(!validate_pending_response(session)) return DfcVirtualPiccStatusProtocolError;

    if(direct_response_is_pending(session)) {
        if(!allow_direct_retry) return DfcVirtualPiccStatusProtocolError;
        return replay_direct_response(
            session, command, command_len, response, response_capacity, response_len);
    }

#if DFC_ENABLE_ISO7816_AUTH
    if(session->emulator->iso_auth_phase != 0 &&
       response_capacity < DFC_ISO7816_STATUS_WORD_LENGTH)
        return DfcVirtualPiccStatusBufferTooSmall;
    iso_auth_abort_on_unexpected_command(session->emulator, command, command_len);
#endif

    if(command_len >= 4 && command[0] == DFC_ISO7816_CLA_STANDARD) {
        if(command[1] == DFC_ISO7816_INS_SELECT) {
#if DFC_ENABLE_VIRTUAL_CARD
            DfcVirtualPiccStatus virtual_card_status = handle_virtual_card_select(
                session, command, command_len, response, response_capacity, response_len);
            if(virtual_card_status != DfcVirtualPiccStatusUnsupportedProtocol)
                return virtual_card_status;
#endif
        }
#if DFC_ENABLE_VIRTUAL_CARD
        if(command[1] == DFC_ISO7816_INS_EXTERNAL_AUTHENTICATE &&
           session->emulator->virtual_card_authentication_expected)
            return handle_virtual_card_external_authenticate(
                session, command, command_len, response, response_capacity, response_len);
#endif
        if(response_capacity < DFC_ISO7816_STATUS_WORD_LENGTH)
            return DfcVirtualPiccStatusBufferTooSmall;
        if(dfc_emulator_abort_pending_native_work(session->emulator)) {
            return write_status_word(
                DFC_ISO7816_SW_CONDITIONS_NOT_SATISFIED_HI,
                DFC_ISO7816_SW_CONDITIONS_NOT_SATISFIED_LO,
                response,
                response_capacity,
                response_len);
        }
        if(command[1] == DFC_ISO7816_INS_SELECT) {
            return handle_iso_select(
                session, command, command_len, response, response_capacity, response_len);
        }
#if DFC_ENABLE_ISO7816_AUTH
        if(command[1] == DFC_ISO7816_INS_GET_CHALLENGE ||
           command[1] == DFC_ISO7816_INS_EXTERNAL_AUTHENTICATE ||
           command[1] == DFC_ISO7816_INS_INTERNAL_AUTHENTICATE) {
            return handle_iso_authenticate(
                session, command, command_len, response, response_capacity, response_len);
        }
#endif

        if(command[1] == DFC_ISO7816_INS_READ_BINARY ||
           command[1] == DFC_ISO7816_INS_UPDATE_BINARY ||
           command[1] == DFC_ISO7816_INS_READ_RECORD) {
            return handle_iso_file_command(
                session, command, command_len, response, response_capacity, response_len);
        }

        return write_status_word(
            DFC_ISO7816_SW_INS_NOT_SUPPORTED_HI,
            DFC_ISO7816_SW_INS_NOT_SUPPORTED_LO,
            response,
            response_capacity,
            response_len);
    }

    if(command_len >= 5 && command[0] == DFC_ISO7816_CLA_WRAPPER) {
        uint8_t lc = command[4];
        size_t declared_len = (size_t)5 + lc;
        // Exactly one optional Le may follow the declared command data.
        if(command_len < declared_len || command_len > declared_len + 1) {
            if(response_capacity < 2) return DfcVirtualPiccStatusBufferTooSmall;
            response[0] = 0x91;
            response[1] = DFC_STATUS_LENGTH_ERROR;
            *response_len = 2;
            return DfcVirtualPiccStatusOk;
        }
        uint8_t* native_command = command_scratch;
        size_t native_capacity = sizeof(command_scratch);
        if((size_t)lc + 1 > native_capacity && command[1] == DFC_CMD_WRITE_RECORD_ISO &&
           !session->emulator->command_chain_active) {
            native_command = session->emulator->command_chain;
            if(!native_command) {
                native_command =
                    dfc_platform_alloc(DFC_EMULATOR_CHAIN_BUFFER_SIZE, DfcAllocCommandChain);
                session->emulator->command_chain = native_command;
            }
            if(!native_command) return DfcVirtualPiccStatusProtocolError;
            native_capacity = DFC_EMULATOR_CHAIN_BUFFER_SIZE;
            if(session->emulator->command_chain_len < (size_t)lc + 1)
                session->emulator->command_chain_len = (size_t)lc + 1;
        }
        size_t native_len = 0;
        uint8_t instruction = command[1];
        if(lc > 0) {
            if((size_t)lc + 1 > native_capacity) {
                if(response_capacity < 2) return DfcVirtualPiccStatusBufferTooSmall;
                response[0] = 0x91;
                response[1] = DFC_STATUS_LENGTH_ERROR;
                *response_len = 2;
                return DfcVirtualPiccStatusOk;
            }
            memmove(native_command + 1, command + 5, lc);
        }
        native_command[native_len++] = instruction;
        native_len += lc;
        return handle_native_exchange(
            session,
            native_command,
            native_len,
            command_scratch,
            sizeof(command_scratch),
            true,
            allow_direct_retry ? command : NULL,
            allow_direct_retry ? command_len : 0,
            response,
            response_capacity,
            response_len);
    }

    return handle_native_exchange(
        session,
        command,
        command_len,
        command_scratch,
        sizeof(command_scratch),
        false,
        allow_direct_retry ? command : NULL,
        allow_direct_retry ? command_len : 0,
        response,
        response_capacity,
        response_len);
}

DfcVirtualPiccStatus dfc_virtual_picc_iso_dep_exchange(
    DfcVirtualPiccSession* session,
    const uint8_t* command,
    size_t command_len,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len) {
    dfc_random_operation_begin();
    DfcVirtualPiccStatus status = virtual_picc_iso_dep_exchange(
        session, command, command_len, response, response_capacity, response_len, true);
    if(dfc_random_failure_is_retryable()) {
        // The caller can replenish its script and repeat this command. Do not
        // expose the provisional status produced by the core handler.
        *response_len = 0;
        return DfcVirtualPiccStatusProtocolError;
    }
    return status;
}

DfcVirtualPiccStatus dfc_virtual_picc_iso_dep_frame_exchange(
    DfcVirtualPiccSession* session,
    const uint8_t* frame,
    size_t frame_len,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len) {
    dfc_random_operation_begin();
    if(!session || !frame || !response || !response_len) return DfcVirtualPiccStatusProtocolError;
    if(!session->activated) return DfcVirtualPiccStatusNoCard;
    *response_len = 0;

    if(is_rats(frame, frame_len)) {
        size_t ats_len = 0;
        const uint8_t* ats = picc_ats(session->credential, &ats_len);
        if(response_capacity < ats_len) return DfcVirtualPiccStatusBufferTooSmall;
        memcpy(response, ats, ats_len);
        *response_len = ats_len;
        session->iso_dep_selected = true;
        session->iso_dep_cid = (uint8_t)(frame[1] & 0x0F);
        session->expected_pcd_sequence = 0;
        session->picc_sequence = 0;
        clear_transport_storage(session);
        return DfcVirtualPiccStatusOk;
    }

    if(!session->iso_dep_selected || frame_len == 0) return DfcVirtualPiccStatusProtocolError;
    if(!validate_pending_response(session)) return DfcVirtualPiccStatusProtocolError;

    uint8_t pcb = frame[0];
    if(iso_dep_initial_response_is_staged(session)) {
        if(!is_i_block(pcb)) return DfcVirtualPiccStatusProtocolError;
        return replay_staged_iso_dep_response(
            session, frame, frame_len, response, response_capacity, response_len);
    }
    if(is_i_block(pcb)) {
        // The advertised ATS supports CID but not NAD.
        if(pcb & DFC_ISO_DEP_NAD) return DfcVirtualPiccStatusProtocolError;
        if(!iso_dep_cid_matches(session, frame, frame_len))
            return DfcVirtualPiccStatusProtocolError;
        uint8_t sequence = (uint8_t)(pcb & DFC_ISO_DEP_SEQUENCE_BIT);
        bool more = (pcb & DFC_ISO_DEP_MORE) != 0;
        size_t offset = 1;
        if(pcb & DFC_ISO_DEP_CID) offset++;
        if(pcb & DFC_ISO_DEP_NAD) offset++;
        if(frame_len < offset) return DfcVirtualPiccStatusProtocolError;

        const uint8_t* inf = frame + offset;
        size_t inf_len = frame_len - offset;

        if(sequence != session->expected_pcd_sequence) {
            // ISO14443-4 protocol errors leave the PICC in receive mode.
            return DfcVirtualPiccStatusProtocolError;
        }

        size_t pending_command_len_before = session->pending_command_len;
        uint8_t* command_storage = session->pending_command;
        size_t command_capacity = sizeof(session->pending_command);
        if(session->pending_command_len > command_capacity && !session->emulator->command_chain) {
            truncate_pending_command(session, 0);
            return DfcVirtualPiccStatusProtocolError;
        }
        if(!session->emulator->command_chain_active &&
           (session->emulator->command_chain ||
            inf_len > command_capacity - session->pending_command_len)) {
            if(!session->emulator->command_chain) {
                const uint8_t* header = session->pending_command_len ? session->pending_command :
                                                                       inf;
                size_t header_len = session->pending_command_len ? session->pending_command_len :
                                                                   inf_len;
                if(header_len < 2 || (header[0] != DFC_CMD_WRITE_RECORD_ISO &&
                                      !(header[0] == DFC_ISO7816_CLA_WRAPPER &&
                                        header[1] == DFC_CMD_WRITE_RECORD_ISO)))
                    return DfcVirtualPiccStatusBufferTooSmall;
                session->emulator->command_chain =
                    dfc_platform_alloc(DFC_EMULATOR_CHAIN_BUFFER_SIZE, DfcAllocCommandChain);
                if(!session->emulator->command_chain) return DfcVirtualPiccStatusProtocolError;
                memcpy(
                    session->emulator->command_chain,
                    session->pending_command,
                    session->pending_command_len);
            }
            command_storage = session->emulator->command_chain;
            command_capacity = DFC_EMULATOR_CHAIN_BUFFER_SIZE;
        }
        if(session->pending_command_len > command_capacity) {
            truncate_pending_command(session, 0);
            return DfcVirtualPiccStatusProtocolError;
        }
        if((more || session->pending_command_len) &&
           inf_len > command_capacity - session->pending_command_len)
            return DfcVirtualPiccStatusBufferTooSmall;
        if(more && response_capacity < offset) return DfcVirtualPiccStatusBufferTooSmall;
        if(!more && frame_len > sizeof(session->pending_command))
            return DfcVirtualPiccStatusBufferTooSmall;

        if(more || session->pending_command_len) {
            memcpy(command_storage + session->pending_command_len, inf, inf_len);
            session->pending_command_len += inf_len;
            if(command_storage == session->emulator->command_chain)
                session->emulator->command_chain_len = session->pending_command_len;
            inf = command_storage;
            inf_len = session->pending_command_len;
        }

        if(more) {
            session->expected_pcd_sequence ^= DFC_ISO_DEP_SEQUENCE_BIT;
            session->picc_sequence = session->expected_pcd_sequence;
            return build_iso_dep_r_block(
                sequence,
                false,
                (pcb & DFC_ISO_DEP_CID) != 0,
                session->iso_dep_cid,
                response,
                response_capacity,
                response_len);
        }

        // A retryable random failure can only stage a native status byte or
        // its two-byte wrapped form. Preserve those bytes without copying the
        // full response buffer on every embedded exchange.
        uint8_t retry_response_before[DFC_ISO7816_STATUS_WORD_LENGTH];
        DfcByteBuf* tx = session->emulator->tx_buffer;
        if(!tx) return DfcVirtualPiccStatusProtocolError;
        memcpy(retry_response_before, tx->data, sizeof(retry_response_before));
        size_t apdu_response_len = 0;
        DfcVirtualPiccStatus status = virtual_picc_iso_dep_exchange(
            session, inf, inf_len, tx->data, sizeof(tx->data), &apdu_response_len, false);
        if(dfc_random_failure_is_retryable()) {
            memcpy(tx->data, retry_response_before, sizeof(retry_response_before));
            truncate_pending_command(session, pending_command_len_before);
            return DfcVirtualPiccStatusProtocolError;
        }
        if(status != DfcVirtualPiccStatusOk) {
            if(session->pending_command_len > sizeof(session->pending_command) &&
               !session->emulator->command_chain)
                truncate_pending_command(session, 0);
            else
                truncate_pending_command(session, pending_command_len_before);
            return status;
        }
        truncate_pending_command(session, 0);
        session->pending_response_len = apdu_response_len;
        tx->size_bytes = apdu_response_len;
        session->pending_response_offset = 0;
        session->picc_sequence = sequence;
        if(offset > 1) {
            memcpy(session->pending_response_prefix, frame, offset);
            session->pending_response_prefix_len = offset;
        } else {
            session->pending_response_prefix_len = 0;
        }
        status = write_and_remember_iso_dep_response_block(
            session, response, response_capacity, response_len);
        if(status == DfcVirtualPiccStatusBufferTooSmall) {
            memcpy(session->pending_command, frame, frame_len);
            session->pending_command_len = frame_len;
        } else if(status == DfcVirtualPiccStatusOk) {
            session->expected_pcd_sequence ^= DFC_ISO_DEP_SEQUENCE_BIT;
        }
        return status;
    }

    if(is_r_block(pcb)) {
        if(!iso_dep_cid_matches(session, frame, frame_len))
            return DfcVirtualPiccStatusProtocolError;
        if(session->pending_command_len) {
            if(!(pcb & DFC_ISO_DEP_R_NAK)) return DfcVirtualPiccStatusProtocolError;
            return build_iso_dep_r_block(
                session->expected_pcd_sequence ^ DFC_ISO_DEP_SEQUENCE_BIT,
                false,
                (pcb & DFC_ISO_DEP_CID) != 0,
                session->iso_dep_cid,
                response,
                response_capacity,
                response_len);
        }
        if(session->last_picc_block_len == 0) return DfcVirtualPiccStatusProtocolError;
        uint8_t current_sequence = (uint8_t)(session->picc_sequence ^ DFC_ISO_DEP_SEQUENCE_BIT);
        uint8_t received_sequence = (uint8_t)(pcb & DFC_ISO_DEP_SEQUENCE_BIT);
        if(received_sequence == current_sequence) {
            if(response_capacity < session->last_picc_block_len)
                return DfcVirtualPiccStatusBufferTooSmall;
            memcpy(response, session->last_picc_block, session->last_picc_block_len);
            *response_len = session->last_picc_block_len;
            return DfcVirtualPiccStatusOk;
        }
        if((pcb & DFC_ISO_DEP_R_NAK) != 0) {
            return build_iso_dep_r_block(
                current_sequence,
                false,
                (pcb & DFC_ISO_DEP_CID) != 0,
                session->iso_dep_cid,
                response,
                response_capacity,
                response_len);
        }
        if(session->pending_response_len == 0) return DfcVirtualPiccStatusProtocolError;
        DfcVirtualPiccStatus status = write_and_remember_iso_dep_response_block(
            session, response, response_capacity, response_len);
        if(status == DfcVirtualPiccStatusOk)
            session->expected_pcd_sequence = session->picc_sequence;
        return status;
    }

    if(is_s_block(pcb)) {
        if(!iso_dep_cid_matches(session, frame, frame_len))
            return DfcVirtualPiccStatusProtocolError;
        if(response_capacity < frame_len) return DfcVirtualPiccStatusBufferTooSmall;
        memcpy(response, frame, frame_len);
        if((response[0] & (uint8_t)~DFC_ISO_DEP_CID) == DFC_ISO_DEP_S_BLOCK) {
            session->iso_dep_selected = false;
        }
        *response_len = frame_len;
        return DfcVirtualPiccStatusOk;
    }

    return DfcVirtualPiccStatusProtocolError;
}

#endif // DFC_ENABLE_EMULATOR
