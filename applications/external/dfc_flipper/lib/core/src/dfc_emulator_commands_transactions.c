#include "dfc_emulator_commands_i.h"
#include "dfc_credential_i.h"
#include "dfc_ev2.h"
#include "dfc_transaction_mac_i.h"

#if DFC_ENABLE_EMULATOR
#if DFC_ENABLE_TRANSACTION_MAC
static DfcFile* transaction_mac_file(DfcEmulator* emulator) {
    return dfc_credential_find_transaction_mac_file(
        emulator->credential, emulator->selected_app_index);
}

static const DfcFile* transaction_mac_file_const(const DfcEmulator* emulator) {
    return dfc_credential_find_transaction_mac_file(
        emulator->credential, emulator->selected_app_index);
}

bool dfc_emulator_command_transaction_mac_can_add_data(
    const DfcEmulator* emulator,
    size_t data_length) {
    DFC_UNUSED(data_length);
    return !transaction_mac_file_const(emulator) ||
           transaction_mac_file_const(emulator)->state.transaction_mac.counter != UINT32_MAX;
}

typedef enum {
    TransactionMacUpdateOk,
    TransactionMacUpdateTerminal,
    TransactionMacUpdateCryptoFailed,
} TransactionMacUpdateResult;

static TransactionMacUpdateResult
    transaction_mac_candidate(DfcEmulator* emulator, DfcTransactionMacAccumulator* candidate) {
    DfcFile* file = transaction_mac_file(emulator);
    if(!file || file->state.transaction_mac.counter == UINT32_MAX ||
       (emulator->credential->uid_len != DFC_DESFIRE_UID_SHORT_LEN &&
        emulator->credential->uid_len != DFC_DESFIRE_UID_LEN))
        return TransactionMacUpdateTerminal;
    *candidate = emulator->transaction_mac;
    if(candidate->initialized) return TransactionMacUpdateOk;

    uint8_t session_key[DFC_AES_KEY_LENGTH] = {0};
    bool derived = dfc_transaction_mac_derive_session_key(
        file->state.transaction_mac.key,
        emulator->credential->uid,
        emulator->credential->uid_len,
        file->state.transaction_mac.counter + 1,
        session_key);
    bool initialized = derived && dfc_transaction_mac_accumulator_init(candidate, session_key);
    dfc_secure_wipe(session_key, sizeof(session_key));
    return initialized ? TransactionMacUpdateOk : TransactionMacUpdateCryptoFailed;
}

static bool transaction_mac_publish_candidate(
    DfcEmulator* emulator,
    DfcTransactionMacAccumulator* candidate,
    TransactionMacUpdateResult result) {
    emulator->transaction_mac_retryable_failure = result == TransactionMacUpdateCryptoFailed;
    if(result == TransactionMacUpdateOk) emulator->transaction_mac = *candidate;
    dfc_transaction_mac_accumulator_clear(candidate);
    return result == TransactionMacUpdateOk;
}

bool dfc_emulator_command_transaction_mac_add_data(
    DfcEmulator* emulator,
    uint8_t command,
    uint8_t file_number,
    uint32_t offset,
    uint32_t length,
    const uint8_t* data,
    size_t data_length) {
    if(!transaction_mac_file(emulator)) return true;
    DfcTransactionMacAccumulator candidate;
    TransactionMacUpdateResult result = transaction_mac_candidate(emulator, &candidate);
    if(result != TransactionMacUpdateOk) {
        emulator->transaction_mac_retryable_failure = result == TransactionMacUpdateCryptoFailed;
        return false;
    }
    if(offset > 0xFFFFFFu || length > 0xFFFFFFu || (data_length && !data))
        result = TransactionMacUpdateTerminal;
    else if(!dfc_transaction_mac_accumulator_add_data(
                &candidate, command, file_number, offset, length, data, data_length))
        result = TransactionMacUpdateCryptoFailed;
    return transaction_mac_publish_candidate(emulator, &candidate, result);
}

bool dfc_emulator_command_transaction_mac_add_zero_data(
    DfcEmulator* emulator,
    uint8_t command,
    uint8_t file_number,
    uint32_t offset,
    uint32_t length,
    size_t data_length) {
    if(!transaction_mac_file(emulator)) return true;
    DfcTransactionMacAccumulator candidate;
    TransactionMacUpdateResult result = transaction_mac_candidate(emulator, &candidate);
    if(result != TransactionMacUpdateOk) {
        emulator->transaction_mac_retryable_failure = result == TransactionMacUpdateCryptoFailed;
        return false;
    }
    if(offset > 0xFFFFFFu || length > 0xFFFFFFu ||
       !dfc_transaction_mac_accumulator_add_zero_data(
           &candidate, command, file_number, offset, length, data_length))
        result = TransactionMacUpdateCryptoFailed;
    return transaction_mac_publish_candidate(emulator, &candidate, result);
}

bool dfc_emulator_command_transaction_mac_add_record_update(
    DfcEmulator* emulator,
    uint8_t command,
    uint8_t file_number,
    uint32_t record_number,
    uint32_t offset,
    uint32_t length,
    const uint8_t* data,
    size_t data_length) {
    if(!transaction_mac_file(emulator)) return true;
    DfcTransactionMacAccumulator candidate;
    TransactionMacUpdateResult result = transaction_mac_candidate(emulator, &candidate);
    if(result != TransactionMacUpdateOk) {
        emulator->transaction_mac_retryable_failure = result == TransactionMacUpdateCryptoFailed;
        return false;
    }
    if(record_number > 0xFFFFFFu || offset > 0xFFFFFFu || length > 0xFFFFFFu ||
       (data_length && !data))
        result = TransactionMacUpdateTerminal;
    else if(!dfc_transaction_mac_accumulator_add_record_update(
                &candidate, command, file_number, record_number, offset, length, data, data_length))
        result = TransactionMacUpdateCryptoFailed;
    return transaction_mac_publish_candidate(emulator, &candidate, result);
}

bool dfc_emulator_command_transaction_mac_can_add_record_update(
    const DfcEmulator* emulator,
    size_t data_length) {
    DFC_UNUSED(data_length);
    return !transaction_mac_file_const(emulator) ||
           transaction_mac_file_const(emulator)->state.transaction_mac.counter != UINT32_MAX;
}

bool dfc_emulator_command_transaction_mac_add_value(
    DfcEmulator* emulator,
    uint8_t command,
    uint8_t file_number,
    const uint8_t value[4]) {
    if(!transaction_mac_file(emulator)) return true;
    DfcTransactionMacAccumulator candidate;
    TransactionMacUpdateResult result = transaction_mac_candidate(emulator, &candidate);
    if(result != TransactionMacUpdateOk) {
        emulator->transaction_mac_retryable_failure = result == TransactionMacUpdateCryptoFailed;
        return false;
    }
    if(!value)
        result = TransactionMacUpdateTerminal;
    else if(!dfc_transaction_mac_accumulator_add_value(&candidate, command, file_number, value))
        result = TransactionMacUpdateCryptoFailed;
    return transaction_mac_publish_candidate(emulator, &candidate, result);
}

bool dfc_emulator_command_transaction_mac_add_clear_record(
    DfcEmulator* emulator,
    uint8_t file_number) {
    if(!transaction_mac_file(emulator)) return true;
    DfcTransactionMacAccumulator candidate;
    TransactionMacUpdateResult result = transaction_mac_candidate(emulator, &candidate);
    if(result != TransactionMacUpdateOk) {
        emulator->transaction_mac_retryable_failure = result == TransactionMacUpdateCryptoFailed;
        return false;
    }
    if(!dfc_transaction_mac_accumulator_add_clear_record(&candidate, file_number))
        result = TransactionMacUpdateCryptoFailed;
    return transaction_mac_publish_candidate(emulator, &candidate, result);
}

bool dfc_emulator_command_transaction_mac_can_add_clear_record(const DfcEmulator* emulator) {
    return !transaction_mac_file_const(emulator) ||
           transaction_mac_file_const(emulator)->state.transaction_mac.counter != UINT32_MAX;
}
#endif

static bool value_delta_result(const DfcFile* file, int32_t delta, int32_t* result) {
    int64_t current = file->state.value.pending ? (file->state.value.pending_absolute ?
                                                       file->state.value.pending_value :
                                                       (int64_t)file->state.value.current +
                                                           file->state.value.pending_delta) :
                                                  file->state.value.current;
    int64_t next = current + delta;
    if(next < file->state.value.lower_limit || next > file->state.value.upper_limit) return false;
    *result = (int32_t)next;
    return true;
}

void dfc_emulator_command_get_value(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    DfcByteBuf* tx_buffer) {
    DfcFile* file = dfc_credential_find_file_in_app(
        emulator->credential, emulator->selected_app_index, apdu[1]);
    if(!file) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_FILE_NOT_FOUND);
        return;
    }
    if(file->type != 0x02) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PERMISSION_DENIED);
        return;
    }
    if(!dfc_emulator_command_file_allows_read(file, emulator)) {
        dfc_bytebuf_append_byte(
            tx_buffer, dfc_emulator_command_file_access_refusal_status(file, false));
        return;
    }

    uint8_t value_bytes[4] = {0};
    uint32_t raw = (uint32_t)file->state.value.current;
    value_bytes[0] = (uint8_t)(raw & 0xFF);
    value_bytes[1] = (uint8_t)((raw >> 8) & 0xFF);
    value_bytes[2] = (uint8_t)((raw >> 16) & 0xFF);
    value_bytes[3] = (uint8_t)((raw >> 24) & 0xFF);
#if DFC_ENABLE_TRANSACTION_MAC
    if(!dfc_emulator_command_transaction_mac_add_value(
           emulator, DFC_CMD_GET_VALUE, apdu[1], value_bytes)) {
        dfc_secure_wipe(value_bytes, sizeof(value_bytes));
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_BOUNDARY_ERROR);
        return;
    }
#endif

    uint8_t comm = dfc_emulator_file_effective_comm_settings(
        emulator, file, DfcFileAccessRead, file->comm_settings);
    bool ev1_sm = dfc_emulator_native_authentication_active(emulator) &&
                  dfc_secure_messaging_applies_ev1(emulator->secure_messaging, DFC_CMD_GET_VALUE);

    bool protect_here = dfc_emulator_native_authentication_active(emulator) &&
                        comm != DFC_COMM_PLAIN && (!ev1_sm || comm == DFC_COMM_ENCIPHERED);
    if(protect_here) {
        uint8_t wrapped[DFC_SM_MAX_CIPHER_BLOCK_SIZE] = {0};
        size_t wrapped_len = comm == DFC_COMM_MAC ? dfc_secure_messaging_generate_d40_response_mac(
                                                        emulator->secure_messaging,
                                                        value_bytes,
                                                        sizeof(value_bytes),
                                                        wrapped,
                                                        sizeof(wrapped)) :
                                                    dfc_secure_messaging_generate_response(
                                                        emulator->secure_messaging,
                                                        DFC_COMM_ENCIPHERED,
                                                        DFC_STATUS_OK,
                                                        value_bytes,
                                                        sizeof(value_bytes),
                                                        wrapped,
                                                        sizeof(wrapped));
        dfc_secure_wipe(value_bytes, sizeof(value_bytes));
        if(wrapped_len == 0) {
            dfc_secure_wipe(wrapped, sizeof(wrapped));
            dfc_emulator_reset_session(emulator);
            dfc_bytebuf_reset(tx_buffer);
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_INTEGRITY_ERROR);
            return;
        }
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
        dfc_bytebuf_append_bytes(tx_buffer, wrapped, wrapped_len);
        emulator->response_secured = true;
        dfc_secure_wipe(wrapped, sizeof(wrapped));
        return;
    }

    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
    dfc_bytebuf_append_bytes(tx_buffer, value_bytes, sizeof(value_bytes));
    dfc_secure_wipe(value_bytes, sizeof(value_bytes));
}

void dfc_emulator_command_value_delta(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    size_t apdu_len,
    DfcByteBuf* tx_buffer,
    uint8_t cmd,
    bool debit,
    bool limited_credit) {
    DfcFile* file = dfc_credential_find_file_in_app(
        emulator->credential, emulator->selected_app_index, apdu[1]);
    if(!file) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_FILE_NOT_FOUND);
        return;
    }
    if(file->type != 0x02) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PERMISSION_DENIED);
        return;
    }
    if(!dfc_emulator_command_file_allows_write(file, emulator)) {
        dfc_bytebuf_append_byte(
            tx_buffer, dfc_emulator_command_file_access_refusal_status(file, true));
        return;
    }

    // Native body after cmd: FileNo || Amount(4) [|| MACt / Enc(Amount)].
    const uint8_t* body = apdu + 1;
    size_t body_len = apdu_len - 1;
    int32_t amount = 0;

    if(dfc_emulator_native_authentication_active(emulator) &&
       dfc_secure_messaging_applies_ev1(emulator->secure_messaging, cmd)) {
        uint8_t comm = dfc_emulator_file_effective_comm_settings(
            emulator, file, DfcFileAccessWrite, file->comm_settings);
        if(comm == DFC_COMM_MAC && dfc_secure_messaging_ev1_transmits_command_mac(cmd)) {
            size_t clear_len = dfc_secure_messaging_verify_ev1_transmitted_command_mac(
                emulator->secure_messaging, cmd, body, body_len);
            if(clear_len == SIZE_MAX || clear_len < 5) {
                dfc_emulator_reset_session(emulator);
                dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_INTEGRITY_ERROR);
                return;
            }
            amount = dfc_emulator_command_read_int32_le(body + 1);
        } else if(comm == DFC_COMM_ENCIPHERED) {
            // Full: clear FileNo + encrypted Amount. CRC covers Cmd||FileNo||Amount.
            if(body_len < 1 + 8) {
                dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
                return;
            }
            uint8_t header[2] = {cmd, body[0]};
            if(body_len - 1 > DFC_SM_MAX_CIPHER_BLOCK_SIZE) {
                dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
                return;
            }
            uint8_t clear[DFC_SM_MAX_CIPHER_BLOCK_SIZE];
            size_t clear_len = dfc_secure_messaging_verify_command(
                emulator->secure_messaging,
                DFC_COMM_ENCIPHERED,
                header,
                sizeof(header),
                body + 1,
                body_len - 1,
                clear,
                sizeof(clear));
            if(clear_len < 4) {
                dfc_emulator_reset_session(emulator);
                dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_INTEGRITY_ERROR);
                return;
            }
            amount = dfc_emulator_command_read_int32_le(clear);
        } else {
            if(!dfc_secure_messaging_update_ev1_command(
                   emulator->secure_messaging, cmd, body, body_len)) {
                dfc_emulator_reset_session(emulator);
                dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_INTEGRITY_ERROR);
                return;
            }
            if(body_len < 5) {
                dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
                return;
            }
            amount = dfc_emulator_command_read_int32_le(body + 1);
        }
    } else if(
        dfc_emulator_native_authentication_active(emulator) &&
        !dfc_secure_messaging_applies_ev1(emulator->secure_messaging, cmd)) {
        // D40: Amount may be MAC/ENC under file comm mode.
        uint8_t comm = dfc_emulator_file_effective_comm_settings(
            emulator, file, DfcFileAccessWrite, file->comm_settings);
        if(body_len < 1) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
            return;
        }
        if(body_len - 1 > DFC_SM_MAX_CIPHER_BLOCK_SIZE) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
            return;
        }
        uint8_t clear[DFC_SM_MAX_CIPHER_BLOCK_SIZE];
        uint8_t header[2] = {cmd, body[0]};
        size_t clear_len = 0;
        bool d40_mac_ok = true;
        if(comm == DFC_COMM_MAC) {
            d40_mac_ok = dfc_secure_messaging_verify_d40_command_mac(
                emulator->secure_messaging,
                body + 1,
                body_len - 1,
                clear,
                sizeof(clear),
                &clear_len);
        } else if(comm == DFC_COMM_ENCIPHERED) {
            clear_len = dfc_secure_messaging_verify_command(
                emulator->secure_messaging,
                DFC_COMM_ENCIPHERED,
                header,
                sizeof(header),
                body + 1,
                body_len - 1,
                clear,
                sizeof(clear));
        } else {
            clear_len = body_len - 1;
        }
        if(!d40_mac_ok || (clear_len < 4 && comm != DFC_COMM_PLAIN)) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_INTEGRITY_ERROR);
            return;
        }
        if(comm == DFC_COMM_PLAIN) {
            if(body_len < 5) {
                dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
                return;
            }
            amount = dfc_emulator_command_read_int32_le(body + 1);
        } else {
            amount = dfc_emulator_command_read_int32_le(clear);
        }
    } else {
        if(body_len < 5) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
            return;
        }
        amount = dfc_emulator_command_read_int32_le(body + 1);
    }

    if(amount < 0) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PARAMETER_ERROR);
        return;
    }

    int32_t delta = debit ? -amount : amount;
    if(limited_credit) {
        if((file->state.value.limited_credit & 0x01u) == 0) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PERMISSION_DENIED);
            return;
        }
        int32_t available = file->state.value.limited_credit_pending > 0 ?
                                file->state.value.limited_credit_pending :
                                file->state.value.limited_credit_value;
        if(file->state.value.limited_credit_pending == DFC_LIMITED_CREDIT_TRANSACTION_CONSUMED ||
           amount > available) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_BOUNDARY_ERROR);
            return;
        }
    }
    int32_t next_value;
    if(!value_delta_result(file, delta, &next_value)) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_BOUNDARY_ERROR);
        return;
    }

#if DFC_ENABLE_TRANSACTION_MAC
    uint8_t encoded_amount[DFC_UINT32_BYTE_COUNT];
    dfc_write_uint32_le(encoded_amount, (uint32_t)amount);
    if(!dfc_emulator_command_transaction_mac_add_value(emulator, cmd, apdu[1], encoded_amount)) {
        dfc_secure_wipe(encoded_amount, sizeof(encoded_amount));
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_BOUNDARY_ERROR);
        return;
    }
    dfc_secure_wipe(encoded_amount, sizeof(encoded_amount));
#endif
    int64_t aggregate = (int64_t)next_value - file->state.value.current;
    file->state.value.pending = true;
    file->state.value.pending_value = next_value;
    file->state.value.pending_absolute = true;
    file->state.value.pending_delta = aggregate > INT32_MAX ? INT32_MAX :
                                      aggregate < INT32_MIN ? INT32_MIN :
                                                              (int32_t)aggregate;
    if(limited_credit) {
        file->state.value.limited_credit_pending = DFC_LIMITED_CREDIT_TRANSACTION_CONSUMED;
    } else if(debit && file->state.value.limited_credit_pending >= 0) {
        if(amount > INT32_MAX - file->state.value.limited_credit_pending) {
            file->state.value.limited_credit_pending = INT32_MAX;
        } else {
            file->state.value.limited_credit_pending += amount;
        }
    }
    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
}

#if DFC_ENABLE_TRANSACTION_MAC
static bool transaction_mac_calculate(
    DfcEmulator* emulator,
    DfcFile* file,
    uint8_t mac[DFC_WIRE_MAC_LENGTH],
    uint32_t* next_counter) {
    if(file->state.transaction_mac.counter == UINT32_MAX) {
        memset(mac, 0, DFC_WIRE_MAC_LENGTH);
        return false;
    }
    *next_counter = file->state.transaction_mac.counter + 1;
    uint8_t full_mac[DFC_AES_CMAC_LENGTH] = {0};
    bool ok = dfc_transaction_mac_accumulator_finalize(&emulator->transaction_mac, full_mac);
    if(ok) {
        dfc_truncate_aes_wire_mac(full_mac, mac);
    } else {
        memset(mac, 0, DFC_WIRE_MAC_LENGTH);
    }
    dfc_secure_wipe(full_mac, sizeof(full_mac));
    return ok;
}

void dfc_emulator_command_commit_reader_id(
    DfcEmulator* emulator,
    const uint8_t* buffer,
    size_t buffer_len,
    DfcByteBuf* tx_buffer) {
    uint8_t clear_reader_id[DFC_TRANSACTION_READER_ID_LENGTH];
    const uint8_t* reader_id = buffer + 1;
    if(dfc_emulator_native_authentication_active(emulator)) {
        size_t clear_length = SIZE_MAX;
        if(dfc_secure_messaging_applies_ev1(emulator->secure_messaging, buffer[0])) {
            clear_length = dfc_secure_messaging_verify_ev1_transmitted_command_mac(
                emulator->secure_messaging, buffer[0], buffer + 1, buffer_len - 1);
            if(clear_length == DFC_TRANSACTION_READER_ID_LENGTH)
                memcpy(clear_reader_id, buffer + 1, sizeof(clear_reader_id));
        } else {
            bool valid = dfc_secure_messaging_verify_d40_command_mac(
                emulator->secure_messaging,
                buffer + 1,
                buffer_len - 1,
                clear_reader_id,
                sizeof(clear_reader_id),
                &clear_length);
            if(!valid) clear_length = SIZE_MAX;
        }
        if(clear_length != DFC_TRANSACTION_READER_ID_LENGTH) {
            dfc_secure_wipe(clear_reader_id, sizeof(clear_reader_id));
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_INTEGRITY_ERROR);
            return;
        }
        reader_id = clear_reader_id;
        buffer_len = 1 + clear_length;
    }
    DfcFile* file = transaction_mac_file(emulator);
    if(!file) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_FILE_NOT_FOUND);
        return;
    }
    if(buffer_len != 1 + DFC_TRANSACTION_READER_ID_LENGTH) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
        return;
    }
    uint8_t read_write = dfc_access_nibble(file->access_rights, 4);
    if(read_write == DFC_ACCESS_DENY || emulator->transaction_reader_id_pending) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PERMISSION_DENIED);
        return;
    }
    if(!dfc_emulator_file_access_allowed(emulator, file, DfcFileAccessReadWrite)) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_AUTHENTICATION_ERR);
        return;
    }
    if(file->state.transaction_mac.counter == UINT32_MAX) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_INTEGRITY_ERROR);
        return;
    }
    bool authenticated = dfc_emulator_native_authentication_active(emulator)
#if DFC_ENABLE_EV2_SECURE_MESSAGING
                         || emulator->ev2_session_active
#endif
        ;
    uint32_t next_counter = file->state.transaction_mac.counter + 1;
    uint8_t vector[DFC_AES_KEY_LENGTH] = {0};
    vector[0] = DFC_TRANSACTION_ENCRYPTION_DERIVATION_LABEL;
    vector[1] = DFC_DERIVATION_COUNTER_HIGH;
    vector[2] = DFC_DERIVATION_COUNTER_LOW;
    vector[3] = DFC_DERIVED_AES_KEY_LENGTH_HIGH;
    vector[4] = DFC_DERIVED_AES_KEY_LENGTH_LOW;
    dfc_write_uint32_le(vector + DFC_TRANSACTION_VECTOR_COUNTER_OFFSET, next_counter);
    memcpy(
        vector + DFC_TRANSACTION_VECTOR_UID_OFFSET,
        emulator->credential->uid,
        emulator->credential->uid_len);
    uint8_t session_key[DFC_AES_KEY_LENGTH] = {0};
    uint8_t zero_iv[DFC_AES_KEY_LENGTH] = {0};
    uint8_t encrypted[DFC_TRANSACTION_READER_ID_LENGTH] = {0};
    if(authenticated) {
        if(!aes_cmac(
               file->state.transaction_mac.key,
               DFC_AES_KEY_LENGTH,
               vector,
               sizeof(vector),
               session_key)) {
            emulator->transaction_mac_retryable_failure = true;
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_INTEGRITY_ERROR);
            goto cleanup;
        }
        if(!dfc_worker_aes_cbc_encrypt(
               session_key,
               DFC_AES_KEY_LENGTH,
               zero_iv,
               sizeof(file->state.transaction_mac.previous_reader_id),
               file->state.transaction_mac.previous_reader_id,
               encrypted)) {
            emulator->transaction_mac_retryable_failure = true;
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_INTEGRITY_ERROR);
            goto cleanup;
        }
    }
    DfcTransactionMacAccumulator candidate;
    TransactionMacUpdateResult update_result = transaction_mac_candidate(emulator, &candidate);
    if(update_result == TransactionMacUpdateOk &&
       !dfc_transaction_mac_accumulator_add_reader_id(
           &candidate, reader_id, authenticated ? encrypted : NULL))
        update_result = TransactionMacUpdateCryptoFailed;
    if(update_result != TransactionMacUpdateOk) {
        emulator->transaction_mac_retryable_failure = update_result ==
                                                      TransactionMacUpdateCryptoFailed;
        dfc_transaction_mac_accumulator_clear(&candidate);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_BOUNDARY_ERROR);
        goto cleanup;
    }
    emulator->transaction_mac = candidate;
    dfc_transaction_mac_accumulator_clear(&candidate);
    memcpy(
        emulator->transaction_pending_reader_id,
        reader_id,
        sizeof(emulator->transaction_pending_reader_id));
    emulator->transaction_reader_id_pending = true;
    emulator->transaction_reader_id_persist = authenticated;
    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
    if(authenticated) dfc_bytebuf_append_bytes(tx_buffer, encrypted, sizeof(encrypted));
cleanup:
    dfc_secure_wipe(clear_reader_id, sizeof(clear_reader_id));
    dfc_secure_wipe(vector, sizeof(vector));
    dfc_secure_wipe(session_key, sizeof(session_key));
    dfc_secure_wipe(zero_iv, sizeof(zero_iv));
    dfc_secure_wipe(encrypted, sizeof(encrypted));
}
#endif

#if DFC_ENABLE_TRANSACTIONAL_DATA_FILES
bool dfc_emulator_command_transaction_snapshot_begin(DfcEmulator* emulator) {
    if(emulator->transaction_snapshot_active) {
        return emulator->transaction_snapshot_app_index == emulator->selected_app_index;
    }

    size_t capacity = 0;
    for(size_t index = 0; index < emulator->credential->num_files; index++) {
        const DfcFile* file = &emulator->credential->files[index];
        if(file->app_index != emulator->selected_app_index ||
           (file->type != DFC_FILE_TYPE_BACKUP_DATA && file->type != DFC_FILE_TYPE_LINEAR_RECORD &&
            file->type != DFC_FILE_TYPE_CYCLIC_RECORD))
            continue;
        if(file->data_len > DFC_FILE_POOL_SIZE - capacity) return false;
        capacity += file->data_len;
    }
    uint8_t* snapshot = dfc_platform_alloc(capacity ? capacity : 1, DfcAllocTransactionSnapshot);
    if(!snapshot) return false;
    emulator->transaction_snapshot_pool = snapshot;
    emulator->transaction_snapshot_active = true;
    emulator->transaction_snapshot_app_index = emulator->selected_app_index;
    emulator->transaction_snapshot_pool_length = 0;
    emulator->transaction_snapshot_pool_capacity = capacity ? capacity : 1;
    emulator->transaction_snapshot_dirty = emulator->credential->dirty;
    memset(
        emulator->transaction_snapshot_record_counts,
        0,
        sizeof(emulator->transaction_snapshot_record_counts));
    memset(
        emulator->transaction_snapshot_offsets, 0, sizeof(emulator->transaction_snapshot_offsets));
    memset(
        emulator->transaction_snapshot_captured,
        0,
        sizeof(emulator->transaction_snapshot_captured));
    memset(
        emulator->transaction_snapshot_created, 0, sizeof(emulator->transaction_snapshot_created));
    memset(
        emulator->transaction_record_operations,
        RecordTransactionNone,
        sizeof(emulator->transaction_record_operations));
    memset(emulator->transaction_record_numbers, 0, sizeof(emulator->transaction_record_numbers));
    return true;
}

size_t
    dfc_emulator_command_transaction_file_index(const DfcEmulator* emulator, const DfcFile* file) {
    return (size_t)(file - emulator->credential->files);
}

uint8_t dfc_emulator_command_transaction_record_operation(
    const DfcEmulator* emulator,
    const DfcFile* file) {
    size_t index = dfc_emulator_command_transaction_file_index(emulator, file);
    return index < DFC_MAX_FILES ? emulator->transaction_record_operations[index] :
                                   RecordTransactionNone;
}

void dfc_emulator_command_transaction_set_record_operation(
    DfcEmulator* emulator,
    const DfcFile* file,
    uint8_t operation,
    uint32_t record_number) {
    size_t index = dfc_emulator_command_transaction_file_index(emulator, file);
    if(index >= DFC_MAX_FILES) return;
    emulator->transaction_record_operations[index] = operation;
    emulator->transaction_record_numbers[index] = record_number;
}
#endif

void dfc_emulator_command_commit_transaction(
    DfcEmulator* emulator,
    bool return_transaction_mac,
    DfcByteBuf* tx_buffer) {
    bool changed = false;
#if DFC_ENABLE_TRANSACTION_MAC
    if(emulator->transaction_mac.has_pending) changed = true;
#endif
    for(size_t i = 0; i < emulator->credential->num_files; i++) {
        DfcFile* file = &emulator->credential->files[i];
        if(file->app_index != emulator->selected_app_index) continue;
        if((file->type == DFC_FILE_TYPE_VALUE && file->state.value.pending) ||
           file->transaction_pending)
            changed = true;
    }

    if(!changed) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_NO_CHANGES);
        return;
    }

#if DFC_ENABLE_TRANSACTION_MAC
    DfcFile* transaction_file = transaction_mac_file(emulator);
    uint8_t mac[DFC_WIRE_MAC_LENGTH] = {0};
    uint32_t next_counter = 0;
    if(transaction_file) {
        uint8_t read_write = dfc_access_nibble(transaction_file->access_rights, 4);
        if(read_write != DFC_ACCESS_DENY && !emulator->transaction_reader_id_pending) {
            dfc_secure_wipe(mac, sizeof(mac));
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PERMISSION_DENIED);
            return;
        }
        if(!transaction_mac_calculate(emulator, transaction_file, mac, &next_counter)) {
            dfc_secure_wipe(mac, sizeof(mac));
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_INTEGRITY_ERROR);
            return;
        }
    }
#else
    DFC_UNUSED(return_transaction_mac);
#endif

#if DFC_ENABLE_TRANSACTION_MAC
    if(transaction_file && emulator->transaction_reader_id_persist) {
        memcpy(
            transaction_file->state.transaction_mac.previous_reader_id,
            emulator->transaction_pending_reader_id,
            sizeof(transaction_file->state.transaction_mac.previous_reader_id));
    }
#endif
    dfc_emulator_publish_transaction(emulator);
    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
#if DFC_ENABLE_TRANSACTION_MAC
    if(transaction_file) {
        transaction_file->state.transaction_mac.counter = next_counter;
        if(return_transaction_mac) {
            const uint8_t counter[] = {
                (uint8_t)next_counter,
                (uint8_t)(next_counter >> 8),
                (uint8_t)(next_counter >> 16),
                (uint8_t)(next_counter >> 24),
            };
            dfc_bytebuf_append_bytes(tx_buffer, counter, sizeof(counter));
            dfc_bytebuf_append_bytes(tx_buffer, mac, sizeof(mac));
        }
    }
    dfc_secure_wipe(mac, sizeof(mac));
#endif
}

void dfc_emulator_command_abort_transaction(DfcEmulator* emulator, DfcByteBuf* tx_buffer) {
    bool changed = dfc_emulator_abort_transaction(emulator);
    dfc_bytebuf_append_byte(tx_buffer, changed ? DFC_STATUS_OK : DFC_STATUS_NO_CHANGES);
}

void dfc_emulator_command_clear_selected_app_pending_values(DfcEmulator* emulator) {
    for(size_t i = 0; i < emulator->credential->num_files; i++) {
        DfcFile* file = &emulator->credential->files[i];
        if(file->app_index != emulator->selected_app_index || file->type != DFC_FILE_TYPE_VALUE) {
            continue;
        }
        file->state.value.pending = false;
        file->state.value.pending_delta = 0;
        file->state.value.pending_value = 0;
        file->state.value.pending_absolute = false;
        file->state.value.limited_credit_pending = 0;
    }
#if DFC_ENABLE_TRANSACTION_MAC
    dfc_transaction_mac_accumulator_clear(&emulator->transaction_mac);
    dfc_secure_wipe(
        emulator->transaction_pending_reader_id, sizeof(emulator->transaction_pending_reader_id));
    emulator->transaction_reader_id_pending = false;
    emulator->transaction_reader_id_persist = false;
    emulator->transaction_mac_retryable_failure = false;
#endif
}

#endif
