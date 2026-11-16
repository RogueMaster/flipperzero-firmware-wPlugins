#include "dfc_emulator_i.h"

#if DFC_ENABLE_EMULATOR

#if DFC_ENABLE_TRANSACTIONAL_DATA_FILES
_Static_assert(DFC_FILE_POOL_SIZE <= UINT32_MAX, "transaction journal offsets must fit uint32_t");
#endif

DfcEmulator* dfc_emulator_alloc(DfcCredential* credential) {
    if(!credential) return NULL;

    DfcEmulator* emulator = dfc_platform_alloc(sizeof(DfcEmulator), DfcAllocEmulator);
    if(!emulator) return NULL;
    memset(emulator, 0, sizeof(DfcEmulator));

    emulator->credential = credential;
    emulator->tx_buffer = dfc_bytebuf_alloc(DFC_WORKER_MAX_BUFFER_SIZE);
    if(!emulator->tx_buffer) {
        dfc_platform_free(emulator);
        return NULL;
    }
    emulator->selected_application = DfcEmulatorSelectedApplicationPicc;

    return emulator;
}

void dfc_emulator_free(DfcEmulator* emulator) {
    DFC_ASSERT(emulator);

    dfc_emulator_discard_command_chain(emulator);
#if DFC_ENABLE_TRANSACTIONAL_DATA_FILES
    dfc_emulator_discard_transaction_snapshot(emulator);
#endif
    if(emulator->secure_messaging) {
        dfc_secure_messaging_free(emulator->secure_messaging);
    }

    if(emulator->tx_buffer) {
        dfc_secure_wipe(emulator->tx_buffer, sizeof(*emulator->tx_buffer));
        dfc_bytebuf_free(emulator->tx_buffer);
    }
    dfc_secure_wipe(emulator, sizeof(*emulator));
    dfc_platform_free(emulator);
}

void dfc_emulator_discard_command_chain(DfcEmulator* emulator) {
    if(emulator->command_chain) {
        dfc_secure_wipe(emulator->command_chain, emulator->command_chain_len);
        dfc_platform_free(emulator->command_chain);
    }
    emulator->command_chain = NULL;
    emulator->command_chain_active = false;
    emulator->command_chain_len = 0;
    emulator->command_chain_expected = 0;
}

#if DFC_ENABLE_TRANSACTIONAL_DATA_FILES
static bool transaction_file_bit(const uint8_t* bitmap, size_t index) {
    return index < DFC_MAX_FILES && (bitmap[index / 8] & (uint8_t)(1u << (index % 8))) != 0;
}

static void transaction_file_bit_set(uint8_t* bitmap, size_t index, bool value) {
    if(index >= DFC_MAX_FILES) return;
    uint8_t mask = (uint8_t)(1u << (index % 8));
    if(value)
        bitmap[index / 8] |= mask;
    else
        bitmap[index / 8] &= (uint8_t)~mask;
}

static bool transaction_file_has_data(const DfcFile* file) {
    return file->type == DFC_FILE_TYPE_BACKUP_DATA || file->type == DFC_FILE_TYPE_LINEAR_RECORD ||
           file->type == DFC_FILE_TYPE_CYCLIC_RECORD;
}

static void transaction_snapshot_replace(
    DfcEmulator* emulator,
    uint8_t* replacement,
    size_t replacement_capacity) {
    if(emulator->transaction_snapshot_pool) {
        dfc_secure_wipe(
            emulator->transaction_snapshot_pool, emulator->transaction_snapshot_pool_capacity);
        dfc_platform_free(emulator->transaction_snapshot_pool);
    }
    emulator->transaction_snapshot_pool = replacement;
    emulator->transaction_snapshot_pool_length = 0;
    emulator->transaction_snapshot_pool_capacity = replacement ? replacement_capacity : 0;
}

void dfc_emulator_discard_transaction_snapshot(DfcEmulator* emulator) {
    transaction_snapshot_replace(emulator, NULL, 0);
}

static void transaction_snapshot_clear_state(DfcEmulator* emulator) {
    emulator->transaction_snapshot_active = false;
    emulator->transaction_snapshot_app_index = 0;
    emulator->transaction_snapshot_dirty = false;
    dfc_emulator_discard_transaction_snapshot(emulator);
    memset(
        emulator->transaction_snapshot_record_counts,
        0,
        sizeof(emulator->transaction_snapshot_record_counts));
    memset(
        emulator->transaction_snapshot_offsets, 0, sizeof(emulator->transaction_snapshot_offsets));
    memset(
        emulator->transaction_record_operations,
        0,
        sizeof(emulator->transaction_record_operations));
    memset(emulator->transaction_record_numbers, 0, sizeof(emulator->transaction_record_numbers));
    memset(
        emulator->transaction_snapshot_captured,
        0,
        sizeof(emulator->transaction_snapshot_captured));
    memset(
        emulator->transaction_snapshot_created, 0, sizeof(emulator->transaction_snapshot_created));
}

bool dfc_emulator_transaction_snapshot_insert_file(DfcEmulator* emulator, size_t file_index) {
    if(!emulator->transaction_snapshot_active) return true;
    DfcCredential* credential = emulator->credential;
    if(file_index >= credential->num_files) return false;
    DfcFile* file = &credential->files[file_index];
    for(size_t index = DFC_MAX_FILES - 1; index > file_index; index--) {
        emulator->transaction_snapshot_record_counts[index] =
            emulator->transaction_snapshot_record_counts[index - 1];
        emulator->transaction_snapshot_offsets[index] =
            emulator->transaction_snapshot_offsets[index - 1];
        emulator->transaction_record_operations[index] =
            emulator->transaction_record_operations[index - 1];
        emulator->transaction_record_numbers[index] =
            emulator->transaction_record_numbers[index - 1];
        transaction_file_bit_set(
            emulator->transaction_snapshot_captured,
            index,
            transaction_file_bit(emulator->transaction_snapshot_captured, index - 1));
        transaction_file_bit_set(
            emulator->transaction_snapshot_created,
            index,
            transaction_file_bit(emulator->transaction_snapshot_created, index - 1));
    }
    emulator->transaction_snapshot_record_counts[file_index] = 0;
    emulator->transaction_snapshot_offsets[file_index] = 0;
    emulator->transaction_record_operations[file_index] = 0;
    emulator->transaction_record_numbers[file_index] = 0;
    transaction_file_bit_set(emulator->transaction_snapshot_captured, file_index, false);
    transaction_file_bit_set(
        emulator->transaction_snapshot_created,
        file_index,
        file->app_index == emulator->transaction_snapshot_app_index &&
            transaction_file_has_data(file));
    return true;
}

void dfc_emulator_transaction_snapshot_delete_file(DfcEmulator* emulator, size_t file_index) {
    if(!emulator->transaction_snapshot_active || file_index >= emulator->credential->num_files)
        return;
    if(transaction_file_bit(emulator->transaction_snapshot_captured, file_index)) {
        const DfcFile* file = &emulator->credential->files[file_index];
        size_t offset = emulator->transaction_snapshot_offsets[file_index];
        if(offset <= emulator->transaction_snapshot_pool_length &&
           file->data_len <= emulator->transaction_snapshot_pool_length - offset)
            dfc_secure_wipe(emulator->transaction_snapshot_pool + offset, file->data_len);
    }
    for(size_t index = file_index; index + 1 < DFC_MAX_FILES; index++) {
        emulator->transaction_snapshot_record_counts[index] =
            emulator->transaction_snapshot_record_counts[index + 1];
        emulator->transaction_snapshot_offsets[index] =
            emulator->transaction_snapshot_offsets[index + 1];
        emulator->transaction_record_operations[index] =
            emulator->transaction_record_operations[index + 1];
        emulator->transaction_record_numbers[index] =
            emulator->transaction_record_numbers[index + 1];
        transaction_file_bit_set(
            emulator->transaction_snapshot_captured,
            index,
            transaction_file_bit(emulator->transaction_snapshot_captured, index + 1));
        transaction_file_bit_set(
            emulator->transaction_snapshot_created,
            index,
            transaction_file_bit(emulator->transaction_snapshot_created, index + 1));
    }
    emulator->transaction_snapshot_record_counts[DFC_MAX_FILES - 1] = 0;
    emulator->transaction_snapshot_offsets[DFC_MAX_FILES - 1] = 0;
    emulator->transaction_record_operations[DFC_MAX_FILES - 1] = 0;
    emulator->transaction_record_numbers[DFC_MAX_FILES - 1] = 0;
    transaction_file_bit_set(emulator->transaction_snapshot_captured, DFC_MAX_FILES - 1, false);
    transaction_file_bit_set(emulator->transaction_snapshot_created, DFC_MAX_FILES - 1, false);
}

bool dfc_emulator_transaction_snapshot_capture_file(DfcEmulator* emulator, size_t file_index) {
    if(!emulator->transaction_snapshot_active) return true;
    DfcCredential* credential = emulator->credential;
    if(file_index >= credential->num_files) return false;
    const DfcFile* file = &credential->files[file_index];
    if(file->app_index != emulator->transaction_snapshot_app_index ||
       !transaction_file_has_data(file))
        return false;
    if(transaction_file_bit(emulator->transaction_snapshot_created, file_index) ||
       transaction_file_bit(emulator->transaction_snapshot_captured, file_index))
        return true;
    if((file->data_len && !emulator->transaction_snapshot_pool) ||
       file->data_len > emulator->transaction_snapshot_pool_capacity ||
       emulator->transaction_snapshot_pool_length >
           emulator->transaction_snapshot_pool_capacity - file->data_len)
        return false;
    const uint8_t* data = dfc_file_data_const(credential, file);
    if(file->data_len && !data) return false;
    size_t offset = emulator->transaction_snapshot_pool_length;
    if(file->data_len) memcpy(emulator->transaction_snapshot_pool + offset, data, file->data_len);
    emulator->transaction_snapshot_offsets[file_index] = offset;
    emulator->transaction_snapshot_record_counts[file_index] =
        (file->type == DFC_FILE_TYPE_LINEAR_RECORD || file->type == DFC_FILE_TYPE_CYCLIC_RECORD) ?
            file->state.record.count :
            0;
    emulator->transaction_snapshot_pool_length += file->data_len;
    transaction_file_bit_set(emulator->transaction_snapshot_captured, file_index, true);
    return true;
}

const uint8_t* dfc_emulator_transaction_snapshot_committed_data(
    const DfcEmulator* emulator,
    size_t file_index,
    bool* all_zero) {
    if(all_zero) *all_zero = false;
    if(file_index >= emulator->credential->num_files) return NULL;
    const DfcFile* file = &emulator->credential->files[file_index];
    if(!emulator->transaction_snapshot_active ||
       file->app_index != emulator->transaction_snapshot_app_index ||
       !transaction_file_has_data(file))
        return dfc_file_data_const(emulator->credential, file);
    if(transaction_file_bit(emulator->transaction_snapshot_created, file_index)) {
        if(all_zero) *all_zero = true;
        return NULL;
    }
    if(!transaction_file_bit(emulator->transaction_snapshot_captured, file_index))
        return dfc_file_data_const(emulator->credential, file);
    size_t offset = emulator->transaction_snapshot_offsets[file_index];
    if(file->data_len == 0) return NULL;
    if((file->data_len && !emulator->transaction_snapshot_pool) ||
       offset > emulator->transaction_snapshot_pool_length ||
       file->data_len > emulator->transaction_snapshot_pool_length - offset)
        return NULL;
    return emulator->transaction_snapshot_pool + offset;
}

uint32_t dfc_emulator_transaction_snapshot_committed_record_count(
    const DfcEmulator* emulator,
    size_t file_index) {
    if(file_index >= emulator->credential->num_files) return 0;
    const DfcFile* file = &emulator->credential->files[file_index];
    if(!emulator->transaction_snapshot_active ||
       file->app_index != emulator->transaction_snapshot_app_index ||
       (file->type != DFC_FILE_TYPE_LINEAR_RECORD && file->type != DFC_FILE_TYPE_CYCLIC_RECORD))
        return file->state.record.count;
    if(transaction_file_bit(emulator->transaction_snapshot_created, file_index)) return 0;
    if(transaction_file_bit(emulator->transaction_snapshot_captured, file_index))
        return emulator->transaction_snapshot_record_counts[file_index];
    return file->state.record.count;
}

void dfc_emulator_transaction_snapshot_note_persistent_change(DfcEmulator* emulator) {
    if(emulator->transaction_snapshot_active) emulator->transaction_snapshot_dirty = true;
}

bool dfc_emulator_transaction_snapshot_can_mirror_write(
    const DfcEmulator* emulator,
    size_t file_index,
    size_t offset,
    const uint8_t* data,
    size_t length) {
    if(!emulator->transaction_snapshot_active) return true;
    const DfcCredential* credential = emulator->credential;
    if(file_index >= credential->num_files || (length && !data)) return false;
    const DfcFile* file = &credential->files[file_index];
    return file->app_index == emulator->transaction_snapshot_app_index &&
           offset <= file->data_len && length <= file->data_len - offset;
}

bool dfc_emulator_transaction_snapshot_mirror_write(
    DfcEmulator* emulator,
    size_t file_index,
    size_t offset,
    const uint8_t* data,
    size_t length) {
    if(!dfc_emulator_transaction_snapshot_can_mirror_write(
           emulator, file_index, offset, data, length))
        return false;
    if(emulator->transaction_snapshot_active) emulator->transaction_snapshot_dirty = true;
    return true;
}

bool dfc_emulator_abort_transaction_snapshot(DfcEmulator* emulator) {
    if(!emulator->transaction_snapshot_active) return false;

    size_t app_index = emulator->transaction_snapshot_app_index;
    bool valid = emulator->transaction_snapshot_pool_length <=
                 emulator->transaction_snapshot_pool_capacity;
    for(size_t index = 0; index < emulator->credential->num_files; index++) {
        const DfcFile* file = &emulator->credential->files[index];
        if(file->app_index != app_index || !file->transaction_pending ||
           !transaction_file_has_data(file) ||
           transaction_file_bit(emulator->transaction_snapshot_created, index))
            continue;
        size_t offset = emulator->transaction_snapshot_offsets[index];
        if(!transaction_file_bit(emulator->transaction_snapshot_captured, index) ||
           (file->data_len && !emulator->transaction_snapshot_pool) ||
           offset > emulator->transaction_snapshot_pool_length ||
           file->data_len > emulator->transaction_snapshot_pool_length - offset ||
           (file->data_len && !dfc_file_data_const(emulator->credential, file))) {
            valid = false;
            break;
        }
    }
    if(!valid) {
        // The committed preimage is no longer trustworthy. Never publish the
        // staged bytes as clean data when recovery cannot be proven safe.
        for(size_t index = 0; index < emulator->credential->num_files; index++) {
            DfcFile* file = &emulator->credential->files[index];
            if(file->app_index != app_index || !file->transaction_pending ||
               !transaction_file_has_data(file))
                continue;
            uint8_t* data = dfc_file_data(emulator->credential, file);
            if(data && file->data_len) dfc_secure_wipe(data, file->data_len);
            if(file->type == DFC_FILE_TYPE_LINEAR_RECORD ||
               file->type == DFC_FILE_TYPE_CYCLIC_RECORD)
                file->state.record.count = 0;
            file->transaction_pending = false;
        }
        emulator->credential->dirty = true;
        transaction_snapshot_clear_state(emulator);
        return true;
    }
    for(size_t index = 0; index < emulator->credential->num_files; index++) {
        DfcFile* file = &emulator->credential->files[index];
        if(file->app_index != app_index) continue;
        if(valid && file->transaction_pending && transaction_file_has_data(file)) {
            uint8_t* data = dfc_file_data(emulator->credential, file);
            if(transaction_file_bit(emulator->transaction_snapshot_created, index)) {
                if(file->data_len) memset(data, 0, file->data_len);
                if(file->type == DFC_FILE_TYPE_LINEAR_RECORD ||
                   file->type == DFC_FILE_TYPE_CYCLIC_RECORD)
                    file->state.record.count = 0;
            } else if(transaction_file_bit(emulator->transaction_snapshot_captured, index)) {
                size_t offset = emulator->transaction_snapshot_offsets[index];
                if(file->data_len)
                    memcpy(data, emulator->transaction_snapshot_pool + offset, file->data_len);
                if(file->type == DFC_FILE_TYPE_LINEAR_RECORD ||
                   file->type == DFC_FILE_TYPE_CYCLIC_RECORD)
                    file->state.record.count = emulator->transaction_snapshot_record_counts[index];
            }
        }
        file->transaction_pending = false;
    }
    emulator->credential->dirty = emulator->transaction_snapshot_dirty;
    transaction_snapshot_clear_state(emulator);
    return true;
}
#endif

DfcApplication* dfc_emulator_current_app(DfcEmulator* emulator) {
    if(emulator->selected_application != DfcEmulatorSelectedApplicationApp) return NULL;
    return dfc_credential_get_application(emulator->credential, emulator->selected_app_index);
}

bool dfc_emulator_accepts_auth_cipher(DfcEmulator* emulator, uint8_t cipher) {
    DfcApplication* app = dfc_emulator_current_app(emulator);
    uint8_t commands = app ? dfc_credential_app_auth_commands(emulator->credential, app) :
                             dfc_credential_picc_auth_commands(emulator->credential);
    uint8_t disabled =
        emulator->credential->picc_has_sm_disable ? emulator->credential->picc_sm_disable : 0;
    if(app && app->has_sm_disable) disabled |= app->sm_disable;
    uint8_t bit;
    switch(cipher) {
    case DFC_CMD_AUTHENTICATE_LEGACY:
        bit = DFC_AUTH_COMMAND_D40;
        if(disabled & DFC_SM_DISABLE_D40) return false;
        break;
    case DFC_CMD_AUTHENTICATE_ISO:
        bit = DFC_AUTH_COMMAND_ISO_NATIVE;
        if(disabled & DFC_SM_DISABLE_EV1) return false;
        break;
    case DFC_CMD_AUTHENTICATE_AES:
        bit = DFC_AUTH_COMMAND_AES;
        if(disabled & DFC_SM_DISABLE_EV1) return false;
        break;
    case DFC_CMD_AUTHENTICATE_EV2_FIRST:
        bit = DFC_AUTH_COMMAND_EV2_FIRST;
        break;
    case DFC_CMD_AUTHENTICATE_EV2_NON_FIRST:
        bit = DFC_AUTH_COMMAND_EV2_NON_FIRST;
        break;
    default:
        return false;
    }
    return (commands & bit) != 0;
}

static void clear_native_authentication(DfcEmulator* emulator) {
    if(emulator->secure_messaging) {
        dfc_secure_wipe(emulator->secure_messaging, sizeof(*emulator->secure_messaging));
    }
    emulator->auth_cipher = 0;
    emulator->auth_key_no = 0;
}

#if DFC_ENABLE_EV2_SECURE_MESSAGING
static void clear_ev2_authentication(DfcEmulator* emulator, bool keep_transaction_context) {
    emulator->ev2_session_active = false;
    emulator->ev2_authenticated_key_no = 0;
    if(!keep_transaction_context) emulator->ev2_command_counter = 0;
    dfc_secure_wipe(
        emulator->ev2_session_encryption_key, sizeof(emulator->ev2_session_encryption_key));
    dfc_secure_wipe(emulator->ev2_session_mac_key, sizeof(emulator->ev2_session_mac_key));
    if(!keep_transaction_context) {
        dfc_secure_wipe(
            emulator->ev2_transaction_identifier, sizeof(emulator->ev2_transaction_identifier));
        dfc_secure_wipe(emulator->ev2_card_capabilities, sizeof(emulator->ev2_card_capabilities));
        dfc_secure_wipe(
            emulator->ev2_reader_capabilities, sizeof(emulator->ev2_reader_capabilities));
    }
}
#endif

void dfc_emulator_clear_active_authentication(DfcEmulator* emulator) {
    clear_native_authentication(emulator);
#if DFC_ENABLE_EV2_SECURE_MESSAGING
    clear_ev2_authentication(emulator, false);
#endif
}

#if DFC_ENABLE_EV2_SECURE_MESSAGING
void dfc_emulator_retire_ev2_non_first_authentication(DfcEmulator* emulator) {
    clear_native_authentication(emulator);
    clear_ev2_authentication(emulator, true);
}
#endif

void dfc_emulator_commit_auth_family(DfcEmulator* emulator, DfcEmulatorAuthFamily family) {
    if(family == DfcEmulatorAuthFamilyEv2) {
        clear_native_authentication(emulator);
        return;
    }

#if DFC_ENABLE_EV2_SECURE_MESSAGING
    clear_ev2_authentication(emulator, false);
#else
    DFC_UNUSED(emulator);
#endif
}

size_t dfc_emulator_key_len(DfcEmulator* emulator) {
    DfcApplication* app = dfc_emulator_current_app(emulator);
#if DFC_ENABLE_KEY_SETS
    if(app && app->num_key_sets >= DFC_KEY_SET_MINIMUM_COUNT) {
        const uint8_t* type = dfc_credential_key_set_type_const(emulator->credential, app, 0);
        return type ? dfc_key_set_type_length(*type) : 0;
    }
#endif
    return app ? app->key_len : emulator->credential->picc_key_len;
}

size_t dfc_emulator_num_keys(DfcEmulator* emulator) {
    DfcApplication* app = dfc_emulator_current_app(emulator);
    // Every object has a master key whether or not the credential records one:
    // an absent entry is the factory default, not an absent slot.
    if(!app) return 1;
    return app->num_keys ? app->num_keys : 1;
}

uint8_t* dfc_emulator_key(DfcEmulator* emulator, uint8_t key_no) {
    DfcApplication* app = dfc_emulator_current_app(emulator);
    // dfc_emulator_num_keys reports 1 at PICC level, so only slot 0 is reachable.
    return dfc_credential_key(emulator->credential, app, app ? key_no : 0);
}

uint8_t* dfc_emulator_key_version(DfcEmulator* emulator, uint8_t key_no) {
    DfcApplication* app = dfc_emulator_current_app(emulator);
    return app ? &app->key_versions[key_no] : &emulator->credential->picc_key_versions[0];
}

bool dfc_emulator_abort_transaction(DfcEmulator* emulator) {
    bool changed = false;
#if DFC_ENABLE_TRANSACTION_MAC
    if(emulator->transaction_mac.has_pending || emulator->transaction_reader_id_pending) {
        dfc_transaction_mac_accumulator_clear(&emulator->transaction_mac);
        dfc_secure_wipe(
            emulator->transaction_pending_reader_id,
            sizeof(emulator->transaction_pending_reader_id));
        emulator->transaction_reader_id_pending = false;
        emulator->transaction_reader_id_persist = false;
        emulator->transaction_mac_retryable_failure = false;
        changed = true;
    }
#endif
#if DFC_ENABLE_TRANSACTIONAL_DATA_FILES
    changed = dfc_emulator_abort_transaction_snapshot(emulator) || changed;
#endif
    for(size_t i = 0; i < emulator->credential->num_files; i++) {
        DfcFile* file = &emulator->credential->files[i];
        if(file->app_index != emulator->selected_app_index || file->type != DFC_FILE_TYPE_VALUE ||
           !file->state.value.pending) {
            continue;
        }
        file->state.value.pending = false;
        file->state.value.pending_delta = 0;
        file->state.value.pending_value = 0;
        file->state.value.pending_absolute = false;
        file->state.value.limited_credit_pending = 0;
        changed = true;
    }
    return changed;
}

void dfc_emulator_publish_transaction(DfcEmulator* emulator) {
    for(size_t i = 0; i < emulator->credential->num_files; i++) {
        DfcFile* file = &emulator->credential->files[i];
        if(file->app_index != emulator->selected_app_index) continue;
        if(file->type == DFC_FILE_TYPE_VALUE && file->state.value.pending) {
            file->state.value.current = file->state.value.pending_absolute ?
                                            file->state.value.pending_value :
                                            (int32_t)((int64_t)file->state.value.current +
                                                      file->state.value.pending_delta);
            if(file->state.value.limited_credit_pending ==
               DFC_LIMITED_CREDIT_TRANSACTION_CONSUMED) {
                file->state.value.limited_credit_value = 0;
            } else if(file->state.value.limited_credit_pending > 0) {
                file->state.value.limited_credit_value = file->state.value.limited_credit_pending;
            }
            file->state.value.pending = false;
            file->state.value.pending_delta = 0;
            file->state.value.pending_value = 0;
            file->state.value.pending_absolute = false;
            file->state.value.limited_credit_pending = 0;
        }
        file->transaction_pending = false;
    }
#if DFC_ENABLE_TRANSACTIONAL_DATA_FILES
    if(emulator->transaction_snapshot_active &&
       emulator->transaction_snapshot_app_index == emulator->selected_app_index) {
        transaction_snapshot_clear_state(emulator);
    }
#endif
#if DFC_ENABLE_TRANSACTION_MAC
    dfc_transaction_mac_accumulator_clear(&emulator->transaction_mac);
    dfc_secure_wipe(
        emulator->transaction_pending_reader_id, sizeof(emulator->transaction_pending_reader_id));
    emulator->transaction_reader_id_pending = false;
    emulator->transaction_reader_id_persist = false;
    emulator->transaction_mac_retryable_failure = false;
#endif
    dfc_credential_mark_dirty(emulator->credential);
}

void dfc_emulator_clear_pending_authentication_storage(DfcEmulator* emulator) {
    dfc_secure_wipe(&emulator->pending_authentication, sizeof(DfcPendingAuthenticationStorage));
}

void dfc_emulator_clear_iso_authentication(DfcEmulator* emulator) {
#if DFC_ENABLE_ISO7816_AUTH
    emulator->iso_auth_phase = 0;
    emulator->iso_auth_challenge_len = 0;
    emulator->iso_auth_key_no = 0;
    emulator->iso_auth_cipher = 0;
    emulator->iso_auth_reference = 0;
    emulator->iso_auth_key_len = 0;
#endif
    /* The shared storage also holds native authentication secrets. */
    dfc_emulator_clear_pending_authentication_storage(emulator);
}

void dfc_emulator_retire_authentication(DfcEmulator* emulator) {
    size_t pending_chain_len =
        DFC_MIN(emulator->pending_chain_len, sizeof(emulator->pending_chain));
    size_t pending_chunk_count = DFC_MIN(
        (size_t)emulator->pending_chain_chunk_count,
        sizeof(emulator->pending_chain_chunks) / sizeof(emulator->pending_chain_chunks[0]));
    emulator->awaiting_step2 = false;
    dfc_emulator_clear_iso_authentication(emulator);
    clear_native_authentication(emulator);
    emulator->pending_auth_cipher = 0;
    emulator->pending_auth_key_no = 0;
    emulator->get_version_frame = 0;
    emulator->pending_chain_len = 0;
    emulator->pending_chain_offset = 0;
    emulator->pending_chain_frame = 0;
    emulator->pending_chain_last = 0;
    emulator->pending_chain_chunk_count = 0;
    emulator->pending_chain_chunk_index = 0;
    if(pending_chain_len > 0) dfc_secure_wipe(emulator->pending_chain, pending_chain_len);
    if(pending_chunk_count > 0) {
        dfc_secure_wipe(
            emulator->pending_chain_chunks,
            pending_chunk_count * sizeof(emulator->pending_chain_chunks[0]));
    }
    dfc_emulator_discard_command_chain(emulator);
    emulator->df_names_pending = false;
    emulator->df_names_next = 0;
#if DFC_ENABLE_DELEGATED_APPLICATIONS
    emulator->delegated_creation_pending = false;
    emulator->delegated_creation_header_length = 0;
    dfc_secure_wipe(
        emulator->delegated_creation_header, sizeof(emulator->delegated_creation_header));
#endif
#if DFC_ENABLE_PROXIMITY_CHECK
    emulator->proximity_active = false;
    emulator->proximity_verified = false;
    emulator->proximity_offset = 0;
    emulator->proximity_published_len = 0;
    emulator->proximity_transcript_len = 0;
    dfc_secure_wipe(emulator->proximity_random, sizeof(emulator->proximity_random));
    dfc_secure_wipe(emulator->proximity_published, sizeof(emulator->proximity_published));
    dfc_secure_wipe(emulator->proximity_transcript, sizeof(emulator->proximity_transcript));
#endif
#if DFC_ENABLE_EV2_SECURE_MESSAGING
    emulator->ev2_response_plain = false;
    emulator->ev2_authentication_pending = false;
    emulator->ev2_authentication_non_first = false;
    emulator->ev2_session_active = false;
    emulator->ev2_authenticated_key_no = 0;
    emulator->ev2_pending_authenticated_key_no = 0;
    emulator->ev2_command_counter = 0;
    dfc_secure_wipe(
        emulator->ev2_transaction_identifier, sizeof(emulator->ev2_transaction_identifier));
    dfc_secure_wipe(emulator->ev2_card_capabilities, sizeof(emulator->ev2_card_capabilities));
    dfc_secure_wipe(emulator->ev2_reader_capabilities, sizeof(emulator->ev2_reader_capabilities));
    dfc_secure_wipe(
        emulator->ev2_session_encryption_key, sizeof(emulator->ev2_session_encryption_key));
    dfc_secure_wipe(emulator->ev2_session_mac_key, sizeof(emulator->ev2_session_mac_key));
#endif
#if DFC_ENABLE_SDM
    emulator->sdm_read_cache_valid = false;
    emulator->sdm_read_cache_file_number = 0;
    emulator->sdm_read_cache_len = 0;
    dfc_secure_wipe(emulator->sdm_read_cache, sizeof(emulator->sdm_read_cache));
#endif
}

void dfc_emulator_reset_session(DfcEmulator* emulator) {
    dfc_emulator_abort_transaction(emulator);
    dfc_emulator_retire_authentication(emulator);
}

void dfc_emulator_select_application(
    DfcEmulator* emulator,
    DfcEmulatorSelectedApplication selection,
    size_t app_index) {
    dfc_emulator_reset_session(emulator);
    emulator->selected_application = selection;
    emulator->selected_app_index = selection == DfcEmulatorSelectedApplicationApp ? app_index : 0;
#if DFC_ENABLE_TRANSACTION_TIMER
    DfcApplication* app = selection == DfcEmulatorSelectedApplicationApp ?
                              dfc_credential_get_application(emulator->credential, app_index) :
                              NULL;
    emulator->transaction_timer_enabled =
        app && app->has_capability_data &&
        app->capability_data[DFC_TRANSACTION_TIMER_CAPABILITY_OFFSET] ==
            DFC_TRANSACTION_TIMER_SHORT;
    emulator->transaction_timer_elapsed_milliseconds = 0;
#endif
}

void dfc_emulator_reset_activation(DfcEmulator* emulator) {
    DFC_ASSERT(emulator);

    dfc_emulator_abort_transaction(emulator);
    DfcCredential* credential = emulator->credential;
    DfcByteBuf* tx_buffer = emulator->tx_buffer;
    DfcSecureMessaging* secure_messaging = emulator->secure_messaging;
    if(secure_messaging) dfc_secure_wipe(secure_messaging, sizeof(*secure_messaging));
    dfc_emulator_discard_command_chain(emulator);
#if DFC_ENABLE_TRANSACTIONAL_DATA_FILES
    dfc_emulator_discard_transaction_snapshot(emulator);
#endif

    if(tx_buffer) dfc_secure_wipe(tx_buffer, sizeof(*tx_buffer));
    memset(emulator, 0, sizeof(*emulator));
    emulator->credential = credential;
    emulator->tx_buffer = tx_buffer;
    emulator->secure_messaging = secure_messaging;
    emulator->selected_application = DfcEmulatorSelectedApplicationPicc;
    if(tx_buffer) {
        dfc_bytebuf_reset(tx_buffer);
    }
}

void dfc_emulator_advance_time(DfcEmulator* emulator, uint32_t elapsed_milliseconds) {
#if DFC_ENABLE_TRANSACTION_TIMER
    if(!emulator || !emulator->transaction_timer_enabled) return;
    if(UINT32_MAX - emulator->transaction_timer_elapsed_milliseconds < elapsed_milliseconds)
        emulator->transaction_timer_elapsed_milliseconds = UINT32_MAX;
    else
        emulator->transaction_timer_elapsed_milliseconds += elapsed_milliseconds;
    if(emulator->transaction_timer_elapsed_milliseconds >=
       DFC_TRANSACTION_TIMER_EXPIRY_MILLISECONDS) {
        dfc_emulator_reset_activation(emulator);
    }
#else
    DFC_UNUSED(emulator);
    DFC_UNUSED(elapsed_milliseconds);
#endif
}

#endif // DFC_ENABLE_EMULATOR
