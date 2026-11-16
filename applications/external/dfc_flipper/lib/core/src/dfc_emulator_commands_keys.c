#include "dfc_emulator_commands_i.h"
#include "dfc_credential_i.h"
#include "dfc_ev2.h"
#include "dfc_ev2_crypto.h"

#if DFC_ENABLE_EMULATOR

#define emulator_current_app         dfc_emulator_current_app
#define emulator_accepts_auth_cipher dfc_emulator_accepts_auth_cipher
#define emulator_key_len             dfc_emulator_key_len
#define emulator_num_keys            dfc_emulator_num_keys
#define emulator_key                 dfc_emulator_key
#define emulator_key_version         dfc_emulator_key_version
#define crc16_iso14443               dfc_crc16_iso14443
#define crc32_dfc                    dfc_crc32
#define d40_receive_plain            dfc_emulator_d40_receive_plain
#define des_key_version              dfc_emulator_des_key_version

void dfc_emulator_command_get_key_settings(DfcEmulator* emulator, DfcByteBuf* tx_buffer) {
    if(!dfc_emulator_command_require_directory_access(emulator, tx_buffer)) return;

    DfcCredential* credential = emulator->credential;
    DfcApplication* app = emulator_current_app(emulator);

    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
    if(emulator->selected_application == DfcEmulatorSelectedApplicationPicc) {
        dfc_bytebuf_append_byte(tx_buffer, credential->picc_key_settings_1);
        dfc_bytebuf_append_byte(
            tx_buffer, (credential->picc_key_settings_2 & DFC_KEY_TYPE_MASK) | 0x01);
    } else if(app) {
        dfc_bytebuf_append_byte(tx_buffer, app->key_settings_1);
        // Bit 5 is a modifier on CreateApplication rather than state an
        // application holds, so it is not reported back. It is still kept in the
        // stored settings, where the file commands read it.
        dfc_bytebuf_append_byte(
            tx_buffer, app->key_settings_2 & (DFC_KEY_TYPE_MASK | DFC_NUM_KEYS_MASK));
#if DFC_ENABLE_KEY_SETS
        if(app->num_key_sets > 1) {
            const uint8_t* version = dfc_credential_key_set_version_const(credential, app, 0);
            if(!version) {
                dfc_bytebuf_reset(tx_buffer);
                dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OUT_OF_EEPROM);
                return;
            }
            dfc_bytebuf_append_byte(tx_buffer, *version);
            dfc_bytebuf_append_byte(tx_buffer, app->num_key_sets);
            dfc_bytebuf_append_byte(tx_buffer, app->max_key_size);
            dfc_bytebuf_append_byte(tx_buffer, app->key_set_settings);
        }
#endif
    }
}

void dfc_emulator_command_get_key_version(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    size_t apdu_len,
    DfcByteBuf* tx_buffer) {
    uint8_t key_no = apdu[1];
#if DFC_ENABLE_KEY_SETS
    DfcApplication* app = emulator_current_app(emulator);
    if(app && (key_no & DFC_KEY_SET_NUMBER_PRESENT_MASK) != 0) {
        if(app->num_key_sets <= 1) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PERMISSION_DENIED);
            return;
        }
        if(apdu_len != 3) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
            return;
        }
        uint8_t key_set_selector = apdu[2];
        if((key_no & DFC_KEY_SET_SECOND_APPLICATION_MASK) != 0) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PERMISSION_DENIED);
            return;
        }
        if((key_set_selector & DFC_KEY_SET_RETRIEVAL_MASK) != 0) {
            if((key_no & DFC_KEY_NUMBER_MASK) != 0 ||
               (key_set_selector & DFC_KEY_SET_NUMBER_MASK) != 0) {
                dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PARAMETER_ERROR);
                return;
            }
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
            const uint8_t* versions =
                dfc_credential_key_set_version_const(emulator->credential, app, 0);
            if(!versions) {
                dfc_bytebuf_reset(tx_buffer);
                dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OUT_OF_EEPROM);
                return;
            }
            dfc_bytebuf_append_bytes(tx_buffer, versions, app->num_key_sets);
            return;
        }
        size_t key_set_number = key_set_selector & DFC_KEY_SET_NUMBER_MASK;
        size_t slot = key_no & DFC_KEY_NUMBER_MASK;
        uint8_t* version =
            dfc_credential_key_version_in_set(emulator->credential, app, key_set_number, slot);
        if(!version) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_NO_SUCH_KEY);
            return;
        }
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
        dfc_bytebuf_append_byte(tx_buffer, *version);
        return;
    }
#else
    DFC_UNUSED(apdu_len);
#endif
    if(apdu_len != 2) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
        return;
    }
#if DFC_ENABLE_SPECIAL_KEY_STATUS
    if(dfc_emulator_command_picc_disabled_special_key(emulator, key_no)) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PERMISSION_DENIED);
        return;
    }
#endif
    if(key_no >= emulator_num_keys(emulator)) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_NO_SUCH_KEY);
        return;
    }

    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
    dfc_bytebuf_append_byte(tx_buffer, *emulator_key_version(emulator, key_no));
}

static bool can_change_key(DfcEmulator* emulator, uint8_t target_key_no) {
    uint8_t ks1 = dfc_emulator_command_current_key_settings_1(emulator);
    uint8_t auth = emulator->auth_key_no;
    if(target_key_no == 0x00) {
        return ((ks1 & DFC_KS1_MASTER_KEY_CHANGEABLE) != 0) && auth == 0x00;
    }
    uint8_t change_access = (uint8_t)(ks1 >> DFC_KS1_CHANGE_KEY_ACCESS_SHIFT);
    if(change_access == DFC_CHANGE_KEY_ACCESS_FROZEN) return false;
    if(change_access == DFC_CHANGE_KEY_ACCESS_SAME) return auth == target_key_no;
    if(change_access == 0x00) return auth == 0x00;
    return auth == change_access;
}

static bool is_aes_key_context(DfcEmulator* emulator) {
    DfcApplication* app = emulator_current_app(emulator);
    uint8_t ks2 = app ? app->key_settings_2 : emulator->credential->picc_key_settings_2;
    return (ks2 & DFC_KEY_TYPE_MASK) == DFC_KEY_TYPE_AES;
}

static bool
    change_key_d40_padding_valid(const uint8_t* clear, size_t content_len, size_t clear_len) {
    if(content_len > clear_len) return false;
    for(size_t i = content_len; i < clear_len; i++) {
        if(clear[i] != 0) return false;
    }
    return true;
}

static void mark_key_change_dirty(DfcEmulator* emulator) {
    dfc_credential_mark_dirty(emulator->credential);
#if DFC_ENABLE_TRANSACTIONAL_DATA_FILES
    // A later file rollback must still leave this immediate key update dirty.
    if(emulator->transaction_snapshot_active) emulator->transaction_snapshot_dirty = true;
#endif
}

static void dfc_emulator_command_change_key_d40(
    DfcEmulator* emulator,
    uint8_t key_no,
    const uint8_t* encrypted,
    size_t encrypted_len,
    DfcByteBuf* tx_buffer) {
    size_t key_len = emulator_key_len(emulator);
    uint8_t clear[DFC_MAX_KEY_LEN + 8];
    if((encrypted_len % 8) != 0 || encrypted_len < key_len + 2 || encrypted_len > sizeof(clear)) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
        return;
    }

    memset(clear, 0, sizeof(clear));
    if(!d40_receive_plain(
           emulator->secure_messaging->session_key,
           emulator->secure_messaging->session_key_len,
           encrypted,
           encrypted_len,
           clear)) {
        dfc_secure_wipe(clear, sizeof(clear));
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_INTEGRITY_ERROR);
        return;
    }

    uint16_t expected_crc = crc16_iso14443(clear, key_len);
    uint16_t actual_crc = (uint16_t)clear[key_len] | ((uint16_t)clear[key_len + 1] << 8);
    if(expected_crc != actual_crc) {
        dfc_secure_wipe(clear, sizeof(clear));
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_INTEGRITY_ERROR);
        return;
    }

    if(key_no != emulator->auth_key_no) {
        size_t crc2_offset = key_len + 2;
        if(encrypted_len < key_len + 4) {
            dfc_secure_wipe(clear, sizeof(clear));
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
            return;
        }

        uint8_t new_key[DFC_MAX_KEY_LEN] = {0};
        uint8_t* old_key = emulator_key(emulator, key_no);
        if(!old_key) {
            dfc_secure_wipe(new_key, sizeof(new_key));
            dfc_secure_wipe(clear, sizeof(clear));
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_NO_SUCH_KEY);
            return;
        }
        for(size_t i = 0; i < key_len; i++) {
            new_key[i] = clear[i] ^ old_key[i];
        }

        uint16_t expected_new_crc = crc16_iso14443(new_key, key_len);
        uint16_t actual_new_crc = (uint16_t)clear[crc2_offset] |
                                  ((uint16_t)clear[crc2_offset + 1] << 8);
        if(expected_new_crc != actual_new_crc) {
            dfc_secure_wipe(new_key, sizeof(new_key));
            dfc_secure_wipe(clear, sizeof(clear));
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_INTEGRITY_ERROR);
            return;
        }
        if(!change_key_d40_padding_valid(clear, key_len + 4, encrypted_len)) {
            dfc_secure_wipe(new_key, sizeof(new_key));
            dfc_secure_wipe(clear, sizeof(clear));
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_INTEGRITY_ERROR);
            return;
        }

        memcpy(old_key, new_key, key_len);
        *emulator_key_version(emulator, key_no) = des_key_version(new_key);
        mark_key_change_dirty(emulator);
        dfc_secure_wipe(new_key, sizeof(new_key));
        dfc_secure_wipe(clear, sizeof(clear));
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
        return;
    }

    if(!change_key_d40_padding_valid(clear, key_len + 2, encrypted_len)) {
        dfc_secure_wipe(clear, sizeof(clear));
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_INTEGRITY_ERROR);
        return;
    }

    uint8_t* target = emulator_key(emulator, key_no);
    if(!target) {
        dfc_secure_wipe(clear, sizeof(clear));
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_NO_SUCH_KEY);
        return;
    }
    dfc_emulator_retire_authentication(emulator);
    memcpy(target, clear, key_len);
    *emulator_key_version(emulator, key_no) = des_key_version(clear);
    mark_key_change_dirty(emulator);
    dfc_secure_wipe(clear, sizeof(clear));
    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
}

static void dfc_emulator_command_change_key_ev1(
    DfcEmulator* emulator,
    uint8_t key_no,
    const uint8_t* encrypted,
    size_t encrypted_len,
    DfcByteBuf* tx_buffer) {
    DfcSecureMessaging* sm = emulator->secure_messaging;
    size_t key_len = emulator_key_len(emulator);
    bool aes_key = is_aes_key_context(emulator);
    size_t block = sm->cipher == DFC_CMD_AUTHENTICATE_AES ? 16 : 8;
    size_t version_len = aes_key ? 1 : 0;
    bool different = key_no != emulator->auth_key_no;
    size_t content = key_len + version_len + 4 + (different ? 4 : 0);

    uint8_t clear[48];
    if(encrypted_len < content || encrypted_len > sizeof(clear) || (encrypted_len % block) != 0) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
        return;
    }

    memset(clear, 0, sizeof(clear));
    uint8_t candidate_iv[sizeof(sm->iv)];
    memcpy(candidate_iv, sm->iv, sizeof(candidate_iv));
    // Decrypt with the current session IV; CBC mutates IV to the last CT block,
    // which is the EV1 next-IV for ChangeKey (encrypted-plain command).
    bool decrypted = sm->cipher == DFC_CMD_AUTHENTICATE_AES ? dfc_worker_aes_cbc_decrypt(
                                                                  sm->session_key,
                                                                  sm->session_key_len,
                                                                  candidate_iv,
                                                                  encrypted_len,
                                                                  encrypted,
                                                                  clear) :
                                                              dfc_worker_des_cbc_decrypt(
                                                                  sm->session_key,
                                                                  sm->session_key_len,
                                                                  candidate_iv,
                                                                  encrypted_len,
                                                                  encrypted,
                                                                  clear);
    if(!decrypted) {
        dfc_secure_wipe(clear, sizeof(clear));
        dfc_secure_wipe(candidate_iv, sizeof(candidate_iv));
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_INTEGRITY_ERROR);
        return;
    }

    size_t cursor = key_len;
    uint8_t new_version = 0;
    if(aes_key) {
        new_version = clear[cursor++];
    }

    uint8_t crc_input[2 + DFC_MAX_KEY_LEN + 1];
    crc_input[0] = DFC_CMD_CHANGE_KEY;
    crc_input[1] = key_no;
    memcpy(crc_input + 2, clear, cursor);
    uint32_t expected_crc = crc32_dfc(crc_input, 2 + cursor);
    uint32_t actual_crc = (uint32_t)clear[cursor] | ((uint32_t)clear[cursor + 1] << 8) |
                          ((uint32_t)clear[cursor + 2] << 16) |
                          ((uint32_t)clear[cursor + 3] << 24);
    if(expected_crc != actual_crc) {
        dfc_secure_wipe(crc_input, sizeof(crc_input));
        dfc_secure_wipe(clear, sizeof(clear));
        dfc_secure_wipe(candidate_iv, sizeof(candidate_iv));
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_INTEGRITY_ERROR);
        return;
    }
    cursor += 4;

    uint8_t new_key[DFC_MAX_KEY_LEN];
    uint8_t* old_key = emulator_key(emulator, key_no);
    if(!old_key) {
        dfc_secure_wipe(new_key, sizeof(new_key));
        dfc_secure_wipe(crc_input, sizeof(crc_input));
        dfc_secure_wipe(clear, sizeof(clear));
        dfc_secure_wipe(candidate_iv, sizeof(candidate_iv));
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_NO_SUCH_KEY);
        return;
    }
    if(different) {
        for(size_t i = 0; i < key_len; i++) {
            new_key[i] = clear[i] ^ old_key[i];
        }
        uint32_t expected_new_crc = crc32_dfc(new_key, key_len);
        uint32_t actual_new_crc = (uint32_t)clear[cursor] | ((uint32_t)clear[cursor + 1] << 8) |
                                  ((uint32_t)clear[cursor + 2] << 16) |
                                  ((uint32_t)clear[cursor + 3] << 24);
        if(expected_new_crc != actual_new_crc) {
            dfc_secure_wipe(new_key, sizeof(new_key));
            dfc_secure_wipe(crc_input, sizeof(crc_input));
            dfc_secure_wipe(clear, sizeof(clear));
            dfc_secure_wipe(candidate_iv, sizeof(candidate_iv));
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_INTEGRITY_ERROR);
            return;
        }
        cursor += 4;
    } else {
        memcpy(new_key, clear, key_len);
    }

    for(size_t i = cursor; i < encrypted_len; i++) {
        if(clear[i] != 0x00) {
            dfc_secure_wipe(new_key, sizeof(new_key));
            dfc_secure_wipe(crc_input, sizeof(crc_input));
            dfc_secure_wipe(clear, sizeof(clear));
            dfc_secure_wipe(candidate_iv, sizeof(candidate_iv));
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_INTEGRITY_ERROR);
            return;
        }
    }

    if(different) {
        memcpy(sm->iv, candidate_iv, sizeof(sm->iv));
    } else {
        dfc_emulator_retire_authentication(emulator);
    }
    dfc_secure_wipe(candidate_iv, sizeof(candidate_iv));
    memcpy(old_key, new_key, key_len);
    if(aes_key) {
        *emulator_key_version(emulator, key_no) = new_version;
    } else {
        *emulator_key_version(emulator, key_no) = des_key_version(new_key);
    }
    mark_key_change_dirty(emulator);
    dfc_secure_wipe(new_key, sizeof(new_key));
    dfc_secure_wipe(crc_input, sizeof(crc_input));
    dfc_secure_wipe(clear, sizeof(clear));

    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
    if(different) {
        // Keep session; response CMAC uses IV left from CBC (last cryptogram block).
        dfc_emulator_command_apply_ev1_response_secure_messaging(
            emulator, DFC_CMD_CHANGE_KEY, tx_buffer);
    }
}

void dfc_emulator_command_change_key(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    size_t apdu_len,
    DfcByteBuf* tx_buffer) {
    if(apdu_len < 3) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
        return;
    }
    if(!dfc_emulator_native_authentication_active(emulator)) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_AUTHENTICATION_ERR);
        return;
    }

    uint8_t key_no = apdu[1] & 0x3F;
    if(key_no >= emulator_num_keys(emulator)) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_NO_SUCH_KEY);
        return;
    }
    if(!can_change_key(emulator, key_no)) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PERMISSION_DENIED);
        return;
    }

    const uint8_t* encrypted = apdu + 2;
    size_t encrypted_len = apdu_len - 2;
    uint8_t cipher = emulator->secure_messaging->cipher;
    if(cipher == DFC_CMD_AUTHENTICATE_LEGACY) {
        dfc_emulator_command_change_key_d40(emulator, key_no, encrypted, encrypted_len, tx_buffer);
    } else if(cipher == DFC_CMD_AUTHENTICATE_ISO || cipher == DFC_CMD_AUTHENTICATE_AES) {
        dfc_emulator_command_change_key_ev1(emulator, key_no, encrypted, encrypted_len, tx_buffer);
    } else {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_AUTHENTICATION_ERR);
    }
}

void dfc_emulator_command_change_key_settings(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    size_t apdu_len,
    DfcByteBuf* tx_buffer) {
    if(!dfc_emulator_command_is_authenticated_as_master(emulator)) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_AUTHENTICATION_ERR);
        return;
    }
    if((dfc_emulator_command_current_key_settings_1(emulator) &
        DFC_KS1_CONFIGURATION_CHANGEABLE) == 0) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PERMISSION_DENIED);
        return;
    }

    const uint8_t* settings = apdu + 1;
    size_t settings_len = apdu_len - 1;
#if DFC_ENABLE_EV2_SECURE_MESSAGING
    bool ev2_active = emulator->ev2_session_active;
#else
    bool ev2_active = false;
#endif
    size_t clear_len = 0;
    size_t scratch_len = 0;
    if(!ev2_active) {
        clear_len = dfc_emulator_command_unwrap_write_payload(
            emulator,
            apdu,
            apdu_len,
            0,
            DFC_COMM_ENCIPHERED,
            emulator->pending_chain,
            sizeof(emulator->pending_chain),
            &settings,
            &scratch_len);
        if(clear_len == SIZE_MAX) {
            dfc_secure_wipe(emulator->pending_chain, scratch_len);
            dfc_emulator_reset_session(emulator);
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_INTEGRITY_ERROR);
            return;
        }
        settings_len = clear_len;
    }
    if(settings_len != 1) {
        dfc_secure_wipe(emulator->pending_chain, scratch_len);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
        return;
    }

    DfcApplication* app = emulator_current_app(emulator);
    if(app) {
        app->key_settings_1 = settings[0];
    } else {
        emulator->credential->picc_key_settings_1 = settings[0];
    }
    dfc_credential_mark_dirty(emulator->credential);
    dfc_secure_wipe(emulator->pending_chain, scratch_len);
    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
}

#if DFC_ENABLE_KEY_SETS
static bool key_set_change_authorized(const DfcEmulator* emulator, const DfcApplication* app) {
    uint8_t access = (uint8_t)((app->key_settings_1 & DFC_KS1_CHANGE_KEY_ACCESS_MASK) >>
                               DFC_KS1_CHANGE_KEY_ACCESS_SHIFT);
    if(access == DFC_CHANGE_KEY_ACCESS_FROZEN) return false;
    if(access == DFC_CHANGE_KEY_ACCESS_SAME)
        return dfc_emulator_is_authenticated_with_key(emulator, 0);
    return access < app->num_keys && dfc_emulator_is_authenticated_with_key(emulator, access);
}

static DfcApplication* key_set_target_application(
    DfcEmulator* emulator,
    uint8_t key_set_selector,
    DfcByteBuf* tx_buffer) {
    if(emulator->selected_application != DfcEmulatorSelectedApplicationApp) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PERMISSION_DENIED);
        return NULL;
    }
    if((key_set_selector & DFC_KEY_SET_SECOND_APPLICATION_MASK) != 0) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PERMISSION_DENIED);
        return NULL;
    }
    DfcApplication* app = emulator_current_app(emulator);
    if(!app || app->num_key_sets <= 1) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_NO_SUCH_KEY);
        return NULL;
    }
    size_t key_set_number = key_set_selector & DFC_KEY_SET_NUMBER_MASK;
    if(key_set_number == 0) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PARAMETER_ERROR);
        return NULL;
    }
    if(key_set_number >= app->num_key_sets) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_NO_SUCH_KEY);
        return NULL;
    }
    return app;
}

void dfc_emulator_command_initialize_key_set(
    DfcEmulator* emulator,
    const uint8_t* buffer,
    size_t buffer_len,
    DfcByteBuf* tx_buffer) {
    if(buffer_len != 3) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
        return;
    }
    DfcApplication* app = key_set_target_application(emulator, buffer[1], tx_buffer);
    if(!app) return;
    if(!key_set_change_authorized(emulator, app)) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_AUTHENTICATION_ERR);
        return;
    }
    uint8_t target_type = buffer[2];
    if((target_type & (uint8_t)~DFC_KEY_SET_TYPE_MASK) != 0 ||
       target_type > DFC_KEY_SET_TYPE_AES ||
       dfc_key_set_type_length(target_type) > app->max_key_size) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PARAMETER_ERROR);
        return;
    }
    size_t key_set_number = buffer[1] & DFC_KEY_SET_NUMBER_MASK;
    size_t storage_length = dfc_credential_stored_key_length(app->key_storage_len);
    const uint8_t* active_type = dfc_credential_key_set_type_const(emulator->credential, app, 0);
    uint8_t* target_set_type =
        dfc_credential_key_set_type(emulator->credential, app, key_set_number);
    uint8_t* target_set_version =
        dfc_credential_key_set_version(emulator->credential, app, key_set_number);
    if(!active_type || !target_set_type || !target_set_version) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OUT_OF_EEPROM);
        return;
    }
    size_t source_length = dfc_key_set_type_length(*active_type);
    size_t target_length = dfc_key_set_type_length(target_type);
    size_t copy_length = source_length < target_length ? source_length : target_length;
    for(size_t slot = 0; slot < app->num_keys; slot++) {
        uint8_t* target =
            dfc_credential_key_in_set(emulator->credential, app, key_set_number, slot);
        const uint8_t* source =
            dfc_credential_key_in_set_const(emulator->credential, app, 0, slot);
        uint8_t* target_version =
            dfc_credential_key_version_in_set(emulator->credential, app, key_set_number, slot);
        if(!target || !source || !target_version) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OUT_OF_EEPROM);
            return;
        }
        memset(target, 0, storage_length);
        memcpy(target, source, copy_length);
        *target_version = app->key_versions[slot];
    }
    *target_set_type = target_type;
    *target_set_version = DFC_KEY_SET_INITIAL_VERSION;
    if(!dfc_credential_key_set_set_initialized(emulator->credential, app, key_set_number, true)) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OUT_OF_EEPROM);
        return;
    }
    dfc_credential_mark_dirty(emulator->credential);
    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
}

void dfc_emulator_command_finalize_key_set(
    DfcEmulator* emulator,
    const uint8_t* buffer,
    size_t buffer_len,
    DfcByteBuf* tx_buffer) {
    if(buffer_len != 3) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
        return;
    }
    DfcApplication* app = key_set_target_application(emulator, buffer[1], tx_buffer);
    if(!app) return;
    if(!key_set_change_authorized(emulator, app)) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_AUTHENTICATION_ERR);
        return;
    }
    size_t key_set_number = buffer[1] & DFC_KEY_SET_NUMBER_MASK;
    if(buffer[2] == DFC_KEY_SET_INITIAL_VERSION) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PARAMETER_ERROR);
        return;
    }
    uint8_t* set_version =
        dfc_credential_key_set_version(emulator->credential, app, key_set_number);
    if(!set_version) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OUT_OF_EEPROM);
        return;
    }
    if(!dfc_credential_key_set_is_initialized(emulator->credential, app, key_set_number) ||
       *set_version != DFC_KEY_SET_INITIAL_VERSION) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PERMISSION_DENIED);
        return;
    }
    *set_version = buffer[2];
    dfc_credential_mark_dirty(emulator->credential);
    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
}

void dfc_emulator_command_roll_key_set(
    DfcEmulator* emulator,
    const uint8_t* buffer,
    size_t buffer_len,
    DfcByteBuf* tx_buffer) {
    if(buffer_len != 2) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
        return;
    }
    DfcApplication* app = key_set_target_application(emulator, buffer[1], tx_buffer);
    if(!app) return;
    uint8_t roll_key = app->key_set_settings & DFC_KEY_SET_NUMBER_MASK;
    if(roll_key >= app->num_keys || !dfc_emulator_is_authenticated_with_key(emulator, roll_key)) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_AUTHENTICATION_ERR);
        return;
    }
    size_t key_set_number = buffer[1] & DFC_KEY_SET_NUMBER_MASK;
    const uint8_t* target_version =
        dfc_credential_key_set_version_const(emulator->credential, app, key_set_number);
    const uint8_t* active_version =
        dfc_credential_key_set_version_const(emulator->credential, app, 0);
    const uint8_t* target_type =
        dfc_credential_key_set_type_const(emulator->credential, app, key_set_number);
    const uint8_t* active_type = dfc_credential_key_set_type_const(emulator->credential, app, 0);
    if(!target_version || !active_version || !target_type || !active_type) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OUT_OF_EEPROM);
        return;
    }
    if(*target_version <= *active_version || *target_type < *active_type) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PERMISSION_DENIED);
        return;
    }
    if(!dfc_credential_roll_key_set(emulator->credential, app, key_set_number)) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OUT_OF_EEPROM);
        return;
    }
    dfc_emulator_command_abort_transaction(emulator, tx_buffer);
    dfc_bytebuf_reset(tx_buffer);
    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
    dfc_credential_mark_dirty(emulator->credential);
    dfc_emulator_reset_session(emulator);
    emulator->ev2_response_plain = true;
}

static bool change_key_ev2_length_valid(size_t buffer_len) {
    if(buffer_len < DFC_CHANGE_KEY_EV2_HEADER_LENGTH) return false;
    size_t cryptogram_length = buffer_len - DFC_CHANGE_KEY_EV2_HEADER_LENGTH;
    return cryptogram_length == DFC_CHANGE_KEY_EV2_AES_SHORT_CRYPTOGRAM_LENGTH ||
           cryptogram_length == DFC_CHANGE_KEY_EV2_AES_CRYPTOGRAM_LENGTH ||
           cryptogram_length == DFC_CHANGE_KEY_EV2_LONG_CRYPTOGRAM_LENGTH;
}

void dfc_emulator_command_change_key_ev2(
    DfcEmulator* emulator,
    const uint8_t* buffer,
    size_t buffer_len,
    DfcByteBuf* tx_buffer) {
    if(emulator->selected_application != DfcEmulatorSelectedApplicationApp) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PERMISSION_DENIED);
        return;
    }
    if(!emulator->ev2_session_active) {
        if(!change_key_ev2_length_valid(buffer_len)) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
            return;
        }
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_AUTHENTICATION_ERR);
        return;
    }
    if(buffer_len != DFC_CHANGE_KEY_EV2_SAME_KEY_CLEAR_LENGTH &&
       buffer_len != DFC_CHANGE_KEY_EV2_OTHER_KEY_CLEAR_LENGTH) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
        return;
    }

    DfcApplication* app = emulator_current_app(emulator);
    uint8_t key_set_selector = buffer[1];
    uint8_t key_number = buffer[2];
    if(!app || (key_set_selector & DFC_KEY_SET_SECOND_APPLICATION_MASK) != 0 ||
       key_number >= app->num_keys) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_NO_SUCH_KEY);
        return;
    }
    size_t key_set_number = key_set_selector & DFC_KEY_SET_NUMBER_MASK;
    if(key_set_number >= app->num_key_sets ||
       !dfc_credential_key_set_is_initialized(emulator->credential, app, key_set_number)) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_NO_SUCH_KEY);
        return;
    }
    if(!key_set_change_authorized(emulator, app)) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_AUTHENTICATION_ERR);
        return;
    }
    const uint8_t* stored_key_set_type =
        app->num_key_sets >= DFC_KEY_SET_MINIMUM_COUNT ?
            dfc_credential_key_set_type_const(emulator->credential, app, key_set_number) :
            NULL;
    uint8_t key_set_type =
        stored_key_set_type ?
            *stored_key_set_type :
            (uint8_t)((app->key_settings_2 & DFC_KEY_TYPE_MASK) >> DFC_KEY_TYPE_SHIFT);
    if(key_set_type != DFC_KEY_SET_TYPE_AES) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PARAMETER_ERROR);
        return;
    }

    bool changes_authenticated_key = key_set_number == 0 &&
                                     key_number == emulator->ev2_authenticated_key_no;
    size_t expected_len = changes_authenticated_key ? DFC_CHANGE_KEY_EV2_SAME_KEY_CLEAR_LENGTH :
                                                      DFC_CHANGE_KEY_EV2_OTHER_KEY_CLEAR_LENGTH;
    if(buffer_len != expected_len) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
        return;
    }

    uint8_t* stored_key =
        dfc_credential_key_in_set(emulator->credential, app, key_set_number, key_number);
    uint8_t* stored_version =
        dfc_credential_key_version_in_set(emulator->credential, app, key_set_number, key_number);
    if(!stored_key || !stored_version) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OUT_OF_EEPROM);
        return;
    }

    const uint8_t* clear_key = buffer + DFC_CHANGE_KEY_EV2_HEADER_LENGTH;
    uint8_t new_key[DFC_AES_KEY_LENGTH];
    if(changes_authenticated_key) {
        memcpy(new_key, clear_key, sizeof(new_key));
    } else {
        for(size_t i = 0; i < sizeof(new_key); i++)
            new_key[i] = clear_key[i] ^ stored_key[i];
        const uint8_t* crc_bytes =
            clear_key + DFC_AES_KEY_LENGTH + DFC_CHANGE_KEY_EV2_VERSION_LENGTH;
        uint32_t supplied_crc = (uint32_t)dfc_emulator_command_read_int32_le(crc_bytes);
        if(supplied_crc != crc32_dfc(new_key, sizeof(new_key))) {
            dfc_secure_wipe(new_key, sizeof(new_key));
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_INTEGRITY_ERROR);
            return;
        }
    }

    if(changes_authenticated_key) dfc_emulator_retire_authentication(emulator);
    memcpy(stored_key, new_key, sizeof(new_key));
    *stored_version = clear_key[DFC_AES_KEY_LENGTH];
    mark_key_change_dirty(emulator);
    dfc_secure_wipe(new_key, sizeof(new_key));
    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
    if(changes_authenticated_key) emulator->ev2_response_plain = true;
}
#endif

void dfc_emulator_command_set_configuration(
    DfcEmulator* emulator,
    const uint8_t* buffer,
    size_t buffer_len,
    DfcByteBuf* tx_buffer) {
    uint8_t option = buffer_len > DFC_SET_CONFIGURATION_OPTION_OFFSET ?
                         buffer[DFC_SET_CONFIGURATION_OPTION_OFFSET] :
                         0;
    if(option == DFC_CONFIGURATION_CARD_OR_APPLICATION) {
        if(!dfc_emulator_command_is_authenticated_as_master(emulator)) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_AUTHENTICATION_ERR);
            return;
        }
        uint8_t clear[DFC_SM_MAX_CIPHER_BLOCK_SIZE] = {0};
        const uint8_t* configuration_data = NULL;
        size_t scratch_len = 0;
        size_t clear_len = dfc_emulator_command_unwrap_write_payload(
            emulator,
            buffer,
            buffer_len,
            1,
            DFC_COMM_ENCIPHERED,
            clear,
            sizeof(clear),
            &configuration_data,
            &scratch_len);
        if(clear_len == SIZE_MAX) {
            dfc_secure_wipe(clear, sizeof(clear));
            dfc_emulator_reset_session(emulator);
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_INTEGRITY_ERROR);
            return;
        }
        if(clear_len != 1) {
            dfc_secure_wipe(clear, sizeof(clear));
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
            return;
        }
        uint8_t configuration = configuration_data[0];
        dfc_secure_wipe(clear, sizeof(clear));
        if(emulator->selected_application == DfcEmulatorSelectedApplicationPicc) {
            uint8_t reserved = emulator->credential->card.generation == DfcGenerationEv1 ?
                                   DFC_CONFIGURATION_EV1_PICC_RESERVED_MASK :
                                   DFC_CONFIGURATION_PICC_RESERVED_MASK;
            if((configuration & reserved) != 0 ||
               ((configuration & DFC_CONFIGURATION_LEGACY_RANDOM_ID) != 0 &&
                (configuration & DFC_CONFIGURATION_RANDOM_ID) == 0)) {
                dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PARAMETER_ERROR);
                return;
            }
            if((configuration & DFC_CONFIGURATION_PROXIMITY_MANDATORY) != 0) {
#if DFC_ENABLE_PROXIMITY_CHECK && DFC_ENABLE_VIRTUAL_CARD
                if(!emulator->credential->picc_has_proximity_key) {
                    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PERMISSION_DENIED);
                    return;
                }
#else
                dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PERMISSION_DENIED);
                return;
#endif
            }
            if((configuration & DFC_CONFIGURATION_AUTHENTICATION_MANDATORY) != 0) {
#if DFC_ENABLE_VIRTUAL_CARD
                if(!emulator->credential->virtual_card_configured) {
                    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PERMISSION_DENIED);
                    return;
                }
#else
                dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PERMISSION_DENIED);
                return;
#endif
            }
            bool changed = false;
            if((configuration & DFC_CONFIGURATION_DISABLE_FORMAT) != 0 &&
               !emulator->credential->picc_format_disabled) {
                emulator->credential->picc_format_disabled = true;
                changed = true;
            }
            if((configuration & DFC_CONFIGURATION_RANDOM_ID) != 0) {
                bool legacy = (configuration & DFC_CONFIGURATION_LEGACY_RANDOM_ID) != 0;
                if(!emulator->credential->picc_random_id ||
                   emulator->credential->picc_random_id_legacy != legacy) {
                    emulator->credential->picc_random_id = true;
                    emulator->credential->picc_random_id_legacy = legacy;
                    changed = true;
                }
            }
#if DFC_ENABLE_VIRTUAL_CARD
            if((configuration & DFC_CONFIGURATION_PROXIMITY_MANDATORY) != 0 &&
               !emulator->credential->virtual_card_proximity_mandatory) {
                emulator->credential->virtual_card_proximity_mandatory = true;
                changed = true;
            }
            if((configuration & DFC_CONFIGURATION_AUTHENTICATION_MANDATORY) != 0 &&
               !emulator->credential->virtual_card_authentication_mandatory) {
                emulator->credential->virtual_card_authentication_mandatory = true;
                changed = true;
            }
#endif
            if(changed) dfc_credential_mark_dirty(emulator->credential);
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
            return;
        }
        if(emulator->selected_application != DfcEmulatorSelectedApplicationApp) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PERMISSION_DENIED);
            return;
        }
        DfcApplication* app = dfc_emulator_current_app(emulator);
        if(!app || (configuration & DFC_CONFIGURATION_APPLICATION_RESERVED_MASK) != 0) {
            dfc_bytebuf_append_byte(
                tx_buffer, app ? DFC_STATUS_PARAMETER_ERROR : DFC_STATUS_PERMISSION_DENIED);
            return;
        }
#if DFC_ENABLE_DELEGATED_APPLICATIONS
        if(!app->delegated) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PERMISSION_DENIED);
            return;
        }
        if((configuration & DFC_CONFIGURATION_DISABLE_FORMAT) != 0) {
            app->delegated_format_disabled = true;
            dfc_credential_mark_dirty(emulator->credential);
        }
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
#else
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PERMISSION_DENIED);
#endif
        return;
    }
#if DFC_ENABLE_TRANSACTION_TIMER
    if(option != DFC_CONFIGURATION_APPLICATION_CAPABILITIES) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PARAMETER_ERROR);
        return;
    }
    if(buffer_len != DFC_SET_CONFIGURATION_CAPABILITY_COMMAND_LENGTH) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
        return;
    }
    if(!emulator->ev2_session_active ||
       emulator->ev2_authenticated_key_no != DFC_MASTER_KEY_NUMBER) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_AUTHENTICATION_ERR);
        return;
    }
    DfcApplication* app = dfc_emulator_current_app(emulator);
    if(!app) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PERMISSION_DENIED);
        return;
    }
    memcpy(
        app->capability_data,
        buffer + DFC_SET_CONFIGURATION_DATA_OFFSET,
        DFC_APPLICATION_CAPABILITY_DATA_LENGTH);
    app->has_capability_data = true;
    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
#else
    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PARAMETER_ERROR);
#endif
}

#endif // DFC_ENABLE_EMULATOR
