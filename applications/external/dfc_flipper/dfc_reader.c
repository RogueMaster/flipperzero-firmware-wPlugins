#include "dfc_reader_i.h"

#define TAG "DfcReader"

DfcReader* dfc_reader_alloc(DfcCredential* credential, Iso14443_4aPoller* iso14443_4a_poller) {
    DfcReader* reader = malloc(sizeof(DfcReader));
    memset(reader, 0, sizeof(DfcReader));

    reader->credential = credential;
    reader->iso14443_4a_poller = iso14443_4a_poller;

    reader->tx_buffer = bit_buffer_alloc(DFC_WORKER_MAX_BUFFER_SIZE);
    reader->rx_buffer = bit_buffer_alloc(DFC_WORKER_MAX_BUFFER_SIZE);

    return reader;
}

void dfc_reader_free(DfcReader* reader) {
    furi_assert(reader);
    bit_buffer_free(reader->tx_buffer);
    bit_buffer_free(reader->rx_buffer);
    if(reader->secure_messaging) {
        dfc_secure_messaging_free(reader->secure_messaging);
    }
    free(reader);
}

// Sends `data` (cmd byte + params) as a single native DESFire frame and reads the response
// into reader->rx_buffer. Reuses reader->tx_buffer/rx_buffer, no per-call allocation.
static bool dfc_reader_exchange(DfcReader* reader, const uint8_t* data, size_t data_len) {
    bit_buffer_reset(reader->tx_buffer);
    bit_buffer_append_bytes(reader->tx_buffer, data, data_len);
    Iso14443_4aError error = iso14443_4a_poller_send_block(
        reader->iso14443_4a_poller, reader->tx_buffer, reader->rx_buffer);
    if(error != Iso14443_4aErrorNone) {
        FURI_LOG_W(TAG, "iso14443_4a_poller_send_block error %d", error);
        return false;
    }

    return true;
}

static void dfc_reader_capture_uid(Dfc* dfc) {
    const Iso14443_4aData* data = nfc_device_get_data(dfc->nfc_device, NfcProtocolIso14443_4a);
    size_t uid_len = 0;
    const uint8_t* uid = iso14443_4a_get_uid(data, &uid_len);
    if(uid && uid_len <= sizeof(dfc->credential->uid)) {
        memcpy(dfc->credential->uid, uid, uid_len);
        dfc->credential->uid_len = uid_len;
        FURI_LOG_I(
            TAG,
            "Captured UID %02X%02X%02X%02X%02X%02X%02X",
            uid_len > 0 ? uid[0] : 0,
            uid_len > 1 ? uid[1] : 0,
            uid_len > 2 ? uid[2] : 0,
            uid_len > 3 ? uid[3] : 0,
            uid_len > 4 ? uid[4] : 0,
            uid_len > 5 ? uid[5] : 0,
            uid_len > 6 ? uid[6] : 0);
    }
}

NfcCommand dfc_reader_select_application(DfcReader* reader, const uint8_t* aid) {
    uint8_t frame[DFC_SELECT_APPLICATION_FRAME_SIZE];
    dfc_build_select_application_frame(aid, frame);

    if(!dfc_reader_exchange(reader, frame, sizeof(frame))) {
        return NfcCommandStop;
    }

    if(bit_buffer_get_size_bytes(reader->rx_buffer) < 1 ||
       bit_buffer_get_byte(reader->rx_buffer, 0) != DFC_STATUS_OK) {
        FURI_LOG_W(TAG, "SelectApplication failed");
        return NfcCommandStop;
    }

    return NfcCommandContinue;
}

NfcCommand dfc_reader_authenticate(DfcReader* reader, uint8_t key_no, uint8_t cipher) {
    DfcCredential* credential = reader->credential;
    const DfcApplication* app = dfc_credential_get_primary_application_const(credential);
    if(!app || key_no >= app->num_keys) {
        FURI_LOG_W(TAG, "No key configured for key_no %d", key_no);
        return NfcCommandStop;
    }
    const uint8_t* key = dfc_credential_key_const(credential, app, key_no);
    if(!key) {
        FURI_LOG_W(TAG, "No key material for key_no %d", key_no);
        return NfcCommandStop;
    }
    size_t key_len = app->key_len;
    size_t block_size = dfc_block_size_for_cipher(cipher);

    // Step 1: cmd + key_no -> AF ek(RndB)
    uint8_t frame[2] = {cipher, key_no};
    if(!dfc_reader_exchange(reader, frame, sizeof(frame))) return NfcCommandStop;

    if(bit_buffer_get_size_bytes(reader->rx_buffer) < 1 + block_size ||
       bit_buffer_get_byte(reader->rx_buffer, 0) != DFC_CMD_ADDITIONAL_FRAME) {
        FURI_LOG_W(TAG, "Authenticate step 1 failed");
        return NfcCommandStop;
    }

    uint8_t enc_rnd_b[16];
    memcpy(enc_rnd_b, bit_buffer_get_data(reader->rx_buffer) + 1, block_size);

    // Step 1 decrypt always starts from IV=0.
    uint8_t iv[16];
    memset(iv, 0, sizeof(iv));
    uint8_t rnd_b[16];
    if(cipher == DFC_CMD_AUTHENTICATE_AES) {
        dfc_worker_aes_cbc_decrypt(key, key_len, iv, block_size, enc_rnd_b, rnd_b);
    } else {
        dfc_worker_des_cbc_decrypt(key, key_len, iv, block_size, enc_rnd_b, rnd_b);
    }

    uint8_t rnd_b_rot[16];
    memcpy(rnd_b_rot, rnd_b, block_size);
    dfc_rotate_left(rnd_b_rot, block_size);

    uint8_t rnd_a[16];
    furi_hal_random_fill_buf(rnd_a, block_size);

    uint8_t plain[32];
    memcpy(plain, rnd_a, block_size);
    memcpy(plain + block_size, rnd_b_rot, block_size);

    // Step 2 encrypt: legacy (0x0A) resets IV=0; ISO(0x1A)/AES(0xAA) chain from the
    // step-1 ciphertext. The encrypt call below mutates `iv` in place to the chained
    // state needed for the next step, so no separate bookkeeping is needed.
    uint8_t encrypted[32];
    if(cipher == DFC_CMD_AUTHENTICATE_LEGACY) {
        memset(iv, 0, sizeof(iv));
    } else {
        memcpy(iv, enc_rnd_b, block_size);
    }
    if(cipher == DFC_CMD_AUTHENTICATE_AES) {
        dfc_worker_aes_cbc_encrypt(key, key_len, iv, block_size * 2, plain, encrypted);
    } else {
        dfc_worker_des_cbc_encrypt(key, key_len, iv, block_size * 2, plain, encrypted);
    }

    // Step 2: AF ek(RndA||RndB') -> 00 ek(RndA')
    uint8_t frame2[1 + 32];
    frame2[0] = DFC_CMD_ADDITIONAL_FRAME;
    memcpy(frame2 + 1, encrypted, block_size * 2);
    if(!dfc_reader_exchange(reader, frame2, 1 + block_size * 2)) return NfcCommandStop;

    if(bit_buffer_get_size_bytes(reader->rx_buffer) < 1 + block_size ||
       bit_buffer_get_byte(reader->rx_buffer, 0) != DFC_STATUS_OK) {
        FURI_LOG_W(TAG, "Authenticate step 2 failed");
        return NfcCommandStop;
    }

    uint8_t enc_rnd_a_rot[16];
    memcpy(enc_rnd_a_rot, bit_buffer_get_data(reader->rx_buffer) + 1, block_size);

    // Response decrypt: legacy resets IV=0; ISO/AES continues the chained `iv` left over
    // from the step-2 encrypt call above (its last ciphertext block).
    if(cipher == DFC_CMD_AUTHENTICATE_LEGACY) {
        memset(iv, 0, sizeof(iv));
    }
    uint8_t rnd_a_rot[16];
    if(cipher == DFC_CMD_AUTHENTICATE_AES) {
        dfc_worker_aes_cbc_decrypt(key, key_len, iv, block_size, enc_rnd_a_rot, rnd_a_rot);
    } else {
        dfc_worker_des_cbc_decrypt(key, key_len, iv, block_size, enc_rnd_a_rot, rnd_a_rot);
    }

    uint8_t expected_rnd_a_rot[16];
    memcpy(expected_rnd_a_rot, rnd_a, block_size);
    dfc_rotate_left(expected_rnd_a_rot, block_size);

    if(memcmp(rnd_a_rot, expected_rnd_a_rot, block_size) != 0) {
        FURI_LOG_W(TAG, "Card cryptogram failed verification");
        return NfcCommandStop;
    }

    uint8_t session_key[DFC_MAX_KEY_LEN];
    size_t session_key_len = 0;
    dfc_derive_session_key(cipher, key, key_len, rnd_a, rnd_b, session_key, &session_key_len);

    if(reader->secure_messaging) {
        dfc_secure_messaging_free(reader->secure_messaging);
    }
    const uint8_t* initial_iv = cipher == DFC_CMD_AUTHENTICATE_LEGACY ? NULL : iv;
    reader->secure_messaging =
        dfc_secure_messaging_alloc(cipher, session_key, session_key_len, initial_iv);
    // This end is the reader, which a legacy session's enciphered mode cares about.
    if(reader->secure_messaging) reader->secure_messaging->pcd = true;

    FURI_LOG_I(TAG, "Authenticated with key no %d", key_no);
    return NfcCommandContinue;
}

NfcCommand dfc_reader_read_file(DfcReader* reader, DfcFile* file) {
    furi_assert(reader->secure_messaging);
    furi_assert(reader->credential);

    uint8_t frame[DFC_READ_DATA_FRAME_SIZE];
    dfc_build_read_data_frame(file->number, 0, 0, frame);
    bool ev1_sm = dfc_secure_messaging_applies_ev1(reader->secure_messaging, DFC_CMD_READ_DATA);
    if(ev1_sm) {
        dfc_secure_messaging_update_ev1_command(
            reader->secure_messaging, DFC_CMD_READ_DATA, frame + 1, sizeof(frame) - 1);
    }

    if(!dfc_reader_exchange(reader, frame, sizeof(frame))) return NfcCommandStop;

    if(bit_buffer_get_size_bytes(reader->rx_buffer) < 1) return NfcCommandStop;
    uint8_t status = bit_buffer_get_byte(reader->rx_buffer, 0);
    if(status != DFC_STATUS_OK) {
        FURI_LOG_W(TAG, "ReadData failed with status 0x%02x", status);
        return NfcCommandStop;
    }

    size_t wrapped_len = bit_buffer_get_size_bytes(reader->rx_buffer) - 1;
    const uint8_t* wrapped = bit_buffer_get_data(reader->rx_buffer) + 1;

    // Unwrap into a temporary buffer, then commit into the shared file pool.
    uint8_t plain[DFC_MAX_FILE_DATA];
    size_t plain_len = 0;
    if(ev1_sm) {
        plain_len = dfc_secure_messaging_unwrap_ev1_response(
            reader->secure_messaging, status, wrapped, wrapped_len, plain);
        if(plain_len == SIZE_MAX) {
            FURI_LOG_W(TAG, "EV1 secure messaging unwrap failed");
            return NfcCommandStop;
        }
    } else {
        plain_len = dfc_secure_messaging_unwrap(
            reader->secure_messaging, file->comm_settings, status, wrapped, wrapped_len, plain);
        if(plain_len == 0 && wrapped_len > 0 && file->comm_settings != DFC_COMM_PLAIN) {
            FURI_LOG_W(TAG, "Secure messaging unwrap failed");
            return NfcCommandStop;
        }
    }
    if(plain_len > DFC_MAX_FILE_DATA) {
        FURI_LOG_W(TAG, "ReadData payload too large (%zu)", plain_len);
        return NfcCommandStop;
    }
    if(!dfc_file_resize(reader->credential, file, plain_len)) {
        FURI_LOG_W(TAG, "File pool exhausted storing ReadData");
        return NfcCommandStop;
    }
    if(plain_len > 0) {
        uint8_t* dest = dfc_file_data(reader->credential, file);
        if(!dest) return NfcCommandStop;
        memcpy(dest, plain, plain_len);
    }

    return NfcCommandContinue;
}

NfcCommand dfc_state_machine(Dfc* dfc, Iso14443_4aPoller* iso14443_4a_poller) {
    furi_assert(dfc);
    NfcCommand ret = NfcCommandContinue;

    DfcReader* reader = dfc_reader_alloc(dfc->credential, iso14443_4a_poller);
    dfc->dfc_reader = reader;
    DfcCredential* credential = reader->credential;
    const DfcApplication* app = dfc_credential_get_primary_application_const(credential);

    do {
        if(!app) {
            ret = NfcCommandStop;
            break;
        }

        ret = dfc_reader_select_application(reader, app->aid);
        if(ret == NfcCommandStop) break;

        uint8_t key_no = 0;
        ret = dfc_reader_authenticate(reader, key_no, app->auth_command);
        if(ret == NfcCommandStop) break;

        bool ok = true;
        for(size_t i = 0; i < credential->num_files && ok; i++) {
            DfcFile* file = &credential->files[i];
            if(file->app_index != 0) continue;
            ret = dfc_reader_read_file(reader, file);
            ok = ret != NfcCommandStop;
        }
        if(!ok) break;

        view_dispatcher_send_custom_event(dfc->view_dispatcher, DfcCustomEventPollerSuccess);
    } while(false);

    if(ret == NfcCommandStop) {
        view_dispatcher_send_custom_event(dfc->view_dispatcher, DfcCustomEventPollerError);
    }
    dfc_reader_free(reader);
    dfc->dfc_reader = NULL;

    return NfcCommandStop;
}

static NfcCommand dfc_worker_poller_dispatch(
    NfcGenericEvent event,
    void* context,
    NfcCommand (*on_ready)(Dfc* dfc, Iso14443_4aPoller* poller)) {
    furi_assert(event.protocol == NfcProtocolIso14443_4a);
    NfcCommand ret = NfcCommandContinue;

    Dfc* dfc = context;
    const Iso14443_4aPollerEvent* iso14443_4a_event = event.event_data;
    Iso14443_4aPoller* iso14443_4a_poller = event.instance;

    if(iso14443_4a_event->type == Iso14443_4aPollerEventTypeReady) {
        nfc_device_set_data(
            dfc->nfc_device, NfcProtocolIso14443_4a, nfc_poller_get_data(dfc->poller));
        dfc_reader_capture_uid(dfc);
        ret = on_ready(dfc, iso14443_4a_poller);
    } else if(iso14443_4a_event->type == Iso14443_4aPollerEventTypeError) {
        Iso14443_4aPollerEventData* data = iso14443_4a_event->data;
        Iso14443_4aError error = data->error;
        FURI_LOG_W(TAG, "Iso14443_4aError %i", error);
        switch(error) {
        case Iso14443_4aErrorNone:
            break;
        case Iso14443_4aErrorNotPresent:
            break;
        case Iso14443_4aErrorProtocol:
            ret = NfcCommandStop;
            break;
        case Iso14443_4aErrorTimeout:
            break;
        default:
            break;
        }
    }

    return ret;
}

NfcCommand dfc_worker_poller_callback(NfcGenericEvent event, void* context) {
    return dfc_worker_poller_dispatch(event, context, dfc_state_machine);
}
