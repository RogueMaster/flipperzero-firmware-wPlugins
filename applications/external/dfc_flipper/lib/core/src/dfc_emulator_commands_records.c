#include "dfc_emulator_commands_i.h"
#include "dfc_credential_i.h"

#if DFC_ENABLE_EMULATOR && DFC_ENABLE_RECORD_FILES

#define read_uint24_le dfc_emulator_read_uint24_le
void dfc_emulator_command_write_record(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    size_t apdu_len,
    DfcByteBuf* tx_buffer) {
    if(apdu_len > DFC_EMULATOR_CHAIN_BUFFER_SIZE) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
        return;
    }
    DfcFile* file = dfc_credential_find_file_in_app(
        emulator->credential, emulator->selected_app_index, apdu[1]);
    if(!file ||
       (file->type != DFC_FILE_TYPE_LINEAR_RECORD && file->type != DFC_FILE_TYPE_CYCLIC_RECORD)) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_FILE_NOT_FOUND);
        return;
    }
    if(!dfc_emulator_command_file_allows_write(file, emulator)) {
        dfc_bytebuf_append_byte(
            tx_buffer, dfc_emulator_command_file_access_refusal_status(file, true));
        return;
    }

    uint32_t offset = read_uint24_le(apdu + 2);
    uint32_t write_len = read_uint24_le(apdu + 5);
    uint8_t comm = dfc_emulator_file_effective_comm_settings(
        emulator, file, DfcFileAccessWrite, file->comm_settings);
    const uint8_t* record_data = NULL;
    size_t scratch_len = 0;
    size_t clear_len = dfc_emulator_command_unwrap_write_payload(
        emulator,
        apdu,
        apdu_len,
        7,
        comm,
        emulator->pending_chain,
        sizeof(emulator->pending_chain),
        &record_data,
        &scratch_len);
    if(clear_len == SIZE_MAX) {
        dfc_secure_wipe(emulator->pending_chain, scratch_len);
        dfc_emulator_reset_session(emulator);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_INTEGRITY_ERROR);
        return;
    }
    if(clear_len != write_len || offset + write_len > file->state.record.size) {
        dfc_secure_wipe(emulator->pending_chain, scratch_len);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_BOUNDARY_ERROR);
        return;
    }
#if DFC_ENABLE_TRANSACTION_MAC
    if(!dfc_emulator_command_transaction_mac_can_add_data(emulator, clear_len)) {
        dfc_secure_wipe(emulator->pending_chain, scratch_len);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_BOUNDARY_ERROR);
        return;
    }
#endif
#if DFC_ENABLE_TRANSACTIONAL_DATA_FILES && DFC_ENABLE_TRANSACTION_MAC
    bool transaction_snapshot_was_active = emulator->transaction_snapshot_active;
#endif
    // A cyclic file keeps one record in reserve, so it holds one fewer than its
    // declared size, measured against a genuine EV1.
    size_t cyclic_capacity =
        file->state.record.max_records > 0 ? (size_t)file->state.record.max_records - 1 : 0;

    uint8_t* data = dfc_file_data(emulator->credential, file);
    if(!data) {
        dfc_secure_wipe(emulator->pending_chain, scratch_len);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_FILE_NOT_FOUND);
        return;
    }
    size_t file_index = dfc_emulator_command_transaction_file_index(emulator, file);
    if(!dfc_emulator_command_transaction_snapshot_begin(emulator) ||
       !dfc_emulator_transaction_snapshot_capture_file(emulator, file_index)) {
        dfc_secure_wipe(emulator->pending_chain, scratch_len);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PERMISSION_DENIED);
        return;
    }
    uint8_t operation = dfc_emulator_command_transaction_record_operation(emulator, file);
    if(operation == RecordTransactionClear || operation == RecordTransactionUpdate) {
        dfc_secure_wipe(emulator->pending_chain, scratch_len);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PERMISSION_DENIED);
        return;
    }

    size_t record_index = 0;
    if(operation == RecordTransactionWrite) {
#if DFC_ENABLE_TRANSACTION_MAC
        if(!dfc_emulator_command_transaction_mac_add_data(
               emulator, apdu[0], apdu[1], offset, write_len, record_data, clear_len)) {
#if DFC_ENABLE_TRANSACTIONAL_DATA_FILES
            if(!transaction_snapshot_was_active) dfc_emulator_abort_transaction_snapshot(emulator);
#endif
            dfc_secure_wipe(emulator->pending_chain, scratch_len);
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_BOUNDARY_ERROR);
            return;
        }
#endif
        record_index = emulator->transaction_record_numbers[file_index];
    } else {
        // A linear file with no room left refuses the first write in a
        // transaction. Further writes target the record that call created.
        if(file->state.record.count >= file->state.record.max_records &&
           file->type == DFC_FILE_TYPE_LINEAR_RECORD) {
            dfc_secure_wipe(emulator->pending_chain, scratch_len);
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_BOUNDARY_ERROR);
            return;
        }
        if(file->type == DFC_FILE_TYPE_CYCLIC_RECORD && cyclic_capacity == 0) {
#if DFC_ENABLE_TRANSACTION_MAC
            if(!dfc_emulator_command_transaction_mac_add_data(
                   emulator, apdu[0], apdu[1], offset, write_len, record_data, clear_len)) {
#if DFC_ENABLE_TRANSACTIONAL_DATA_FILES
                if(!transaction_snapshot_was_active)
                    dfc_emulator_abort_transaction_snapshot(emulator);
#endif
                dfc_secure_wipe(emulator->pending_chain, scratch_len);
                dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_BOUNDARY_ERROR);
                return;
            }
#endif
            dfc_secure_wipe(emulator->pending_chain, scratch_len);
            dfc_emulator_command_transaction_set_record_operation(
                emulator, file, RecordTransactionWrite, 0);
            file->transaction_pending = true;
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
            return;
        }

#if DFC_ENABLE_TRANSACTION_MAC
        if(!dfc_emulator_command_transaction_mac_add_data(
               emulator, apdu[0], apdu[1], offset, write_len, record_data, clear_len)) {
#if DFC_ENABLE_TRANSACTIONAL_DATA_FILES
            if(!transaction_snapshot_was_active) dfc_emulator_abort_transaction_snapshot(emulator);
#endif
            dfc_secure_wipe(emulator->pending_chain, scratch_len);
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_BOUNDARY_ERROR);
            return;
        }
#endif

        record_index = file->state.record.count;
        if(file->type == DFC_FILE_TYPE_CYCLIC_RECORD &&
           file->state.record.count >= cyclic_capacity) {
            size_t retained_len = (cyclic_capacity - 1) * (size_t)file->state.record.size;
            if(retained_len > 0) {
                memmove(data, data + file->state.record.size, retained_len);
            }
            record_index = cyclic_capacity - 1;
            file->state.record.count = (uint32_t)cyclic_capacity;
        } else {
            file->state.record.count++;
        }
        memset(data + record_index * file->state.record.size, 0, file->state.record.size);
        dfc_emulator_command_transaction_set_record_operation(
            emulator, file, RecordTransactionWrite, (uint32_t)record_index);
    }
    if(write_len > 0) {
        memcpy(data + record_index * file->state.record.size + offset, record_data, write_len);
    }
    dfc_secure_wipe(emulator->pending_chain, scratch_len);
    file->transaction_pending = true;
    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
}

void dfc_emulator_command_update_record(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    size_t apdu_len,
    DfcByteBuf* tx_buffer) {
    if(apdu_len > DFC_EMULATOR_CHAIN_BUFFER_SIZE) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
        return;
    }
    DfcFile* file = dfc_credential_find_file_in_app(
        emulator->credential, emulator->selected_app_index, apdu[1]);
    if(!file ||
       (file->type != DFC_FILE_TYPE_LINEAR_RECORD && file->type != DFC_FILE_TYPE_CYCLIC_RECORD)) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_FILE_NOT_FOUND);
        return;
    }
    if(!dfc_emulator_command_file_allows_read_write(file, emulator)) {
        dfc_bytebuf_append_byte(
            tx_buffer, dfc_emulator_command_file_access_refusal_status(file, true));
        return;
    }
    uint32_t record_number = read_uint24_le(apdu + 2);
    uint32_t offset = read_uint24_le(apdu + 5);
    uint32_t write_length = read_uint24_le(apdu + 8);
    if(record_number >= file->state.record.count || offset >= file->state.record.size ||
       write_length == 0 || offset + write_length > file->state.record.size) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_BOUNDARY_ERROR);
        return;
    }
    uint8_t comm = dfc_emulator_file_effective_comm_settings(
        emulator, file, DfcFileAccessReadWrite, file->comm_settings);
    const uint8_t* update_data = NULL;
    size_t scratch_len = 0;
    size_t clear_len = dfc_emulator_command_unwrap_write_payload(
        emulator,
        apdu,
        apdu_len,
        DFC_UPDATE_RECORD_HEADER_SIZE - 1,
        comm,
        emulator->pending_chain,
        sizeof(emulator->pending_chain),
        &update_data,
        &scratch_len);
    if(clear_len == SIZE_MAX) {
        dfc_secure_wipe(emulator->pending_chain, scratch_len);
        dfc_emulator_reset_session(emulator);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_INTEGRITY_ERROR);
        return;
    }
    if(clear_len != write_length) {
        dfc_secure_wipe(emulator->pending_chain, scratch_len);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
        return;
    }
#if DFC_ENABLE_TRANSACTION_MAC
    if(!dfc_emulator_command_transaction_mac_can_add_record_update(emulator, clear_len)) {
        dfc_secure_wipe(emulator->pending_chain, scratch_len);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_BOUNDARY_ERROR);
        return;
    }
#endif
#if DFC_ENABLE_TRANSACTIONAL_DATA_FILES && DFC_ENABLE_TRANSACTION_MAC
    bool transaction_snapshot_was_active = emulator->transaction_snapshot_active;
#endif
    uint8_t* data = dfc_file_data(emulator->credential, file);
    size_t file_index = dfc_emulator_command_transaction_file_index(emulator, file);
    if(!data || !dfc_emulator_command_transaction_snapshot_begin(emulator) ||
       !dfc_emulator_transaction_snapshot_capture_file(emulator, file_index)) {
        dfc_secure_wipe(emulator->pending_chain, scratch_len);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OUT_OF_EEPROM);
        return;
    }
    if(file->type == DFC_FILE_TYPE_LINEAR_RECORD &&
       file->state.record.count >= file->state.record.max_records) {
        dfc_secure_wipe(emulator->pending_chain, scratch_len);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_BOUNDARY_ERROR);
        return;
    }
    uint8_t operation = dfc_emulator_command_transaction_record_operation(emulator, file);
    if(operation == RecordTransactionWrite || operation == RecordTransactionClear ||
       (operation == RecordTransactionUpdate &&
        emulator->transaction_record_numbers[file_index] != record_number)) {
        dfc_secure_wipe(emulator->pending_chain, scratch_len);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PERMISSION_DENIED);
        return;
    }
#if DFC_ENABLE_TRANSACTION_MAC
    if(!dfc_emulator_command_transaction_mac_add_record_update(
           emulator, apdu[0], apdu[1], record_number, offset, write_length, update_data, clear_len)) {
#if DFC_ENABLE_TRANSACTIONAL_DATA_FILES
        if(!transaction_snapshot_was_active) dfc_emulator_abort_transaction_snapshot(emulator);
#endif
        dfc_secure_wipe(emulator->pending_chain, scratch_len);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_BOUNDARY_ERROR);
        return;
    }
#endif
    size_t stored_index = file->state.record.count - record_number - 1;
    memcpy(data + stored_index * file->state.record.size + offset, update_data, write_length);
    dfc_secure_wipe(emulator->pending_chain, scratch_len);
    dfc_emulator_command_transaction_set_record_operation(
        emulator, file, RecordTransactionUpdate, record_number);
    file->transaction_pending = true;
    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
}

void dfc_emulator_command_clear_record_file(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    size_t apdu_len,
    DfcByteBuf* tx_buffer) {
    if(apdu_len != 2) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
        return;
    }
    DfcFile* file = dfc_credential_find_file_in_app(
        emulator->credential, emulator->selected_app_index, apdu[1]);
    if(!file ||
       (file->type != DFC_FILE_TYPE_LINEAR_RECORD && file->type != DFC_FILE_TYPE_CYCLIC_RECORD)) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_FILE_NOT_FOUND);
        return;
    }
    if(!dfc_emulator_command_file_allows_read_write(file, emulator)) {
        dfc_bytebuf_append_byte(
            tx_buffer, dfc_emulator_command_file_access_refusal_status(file, true));
        return;
    }
#if DFC_ENABLE_TRANSACTION_MAC
    if(!dfc_emulator_command_transaction_mac_can_add_clear_record(emulator)) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_BOUNDARY_ERROR);
        return;
    }
#endif
#if DFC_ENABLE_TRANSACTIONAL_DATA_FILES && DFC_ENABLE_TRANSACTION_MAC
    bool transaction_snapshot_was_active = emulator->transaction_snapshot_active;
#endif
    size_t file_index = dfc_emulator_command_transaction_file_index(emulator, file);
    if(!dfc_emulator_command_transaction_snapshot_begin(emulator) ||
       !dfc_emulator_transaction_snapshot_capture_file(emulator, file_index)) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OUT_OF_EEPROM);
        return;
    }
    if(dfc_emulator_command_transaction_record_operation(emulator, file) !=
       RecordTransactionNone) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PERMISSION_DENIED);
        return;
    }
#if DFC_ENABLE_TRANSACTION_MAC
    if(!dfc_emulator_command_transaction_mac_add_clear_record(emulator, apdu[1])) {
#if DFC_ENABLE_TRANSACTIONAL_DATA_FILES
        if(!transaction_snapshot_was_active) dfc_emulator_abort_transaction_snapshot(emulator);
#endif
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_BOUNDARY_ERROR);
        return;
    }
#endif
    file->state.record.count = 0;
    dfc_emulator_command_transaction_set_record_operation(
        emulator, file, RecordTransactionClear, 0);
    file->transaction_pending = true;
    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
}

#endif
