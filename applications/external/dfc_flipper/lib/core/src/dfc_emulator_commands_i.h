#pragma once

#include "dfc_emulator_i.h"

#if DFC_ENABLE_EMULATOR

#if DFC_ENABLE_TRANSACTIONAL_DATA_FILES
enum {
    RecordTransactionNone = 0,
    RecordTransactionWrite,
    RecordTransactionUpdate,
    RecordTransactionClear,
};
#endif

/* Private seams shared by the native command-family translation units. */
int32_t dfc_emulator_command_read_int32_le(const uint8_t* data);
void dfc_emulator_command_append_int32_le(DfcByteBuf* tx_buffer, int32_t value);
void dfc_emulator_command_append_status_payload(
    DfcByteBuf* tx_buffer,
    uint8_t status,
    const uint8_t* payload,
    size_t payload_len);
bool dfc_emulator_command_is_authenticated_as_master(const DfcEmulator* emulator);
uint8_t dfc_emulator_command_current_key_settings_1(DfcEmulator* emulator);
uint8_t dfc_emulator_command_current_key_settings_2(DfcEmulator* emulator);
bool dfc_emulator_command_require_directory_access(DfcEmulator* emulator, DfcByteBuf* tx_buffer);
uint8_t dfc_emulator_command_access_change_nibble(uint16_t access_rights);
bool dfc_emulator_command_key_references_exist(
    uint32_t references,
    size_t reference_count,
    size_t key_count);
uint16_t dfc_emulator_command_access_rights_from_wire(uint8_t first, uint8_t second);
void dfc_emulator_command_access_rights_to_wire(uint16_t access_rights, uint8_t out[2]);
void dfc_emulator_command_emit_payload_with_chaining(
    DfcEmulator* emulator,
    DfcByteBuf* tx_buffer,
    uint8_t cmd,
    const uint8_t* payload,
    size_t payload_len,
    size_t entry);
void dfc_emulator_command_emit_payload_with_chaining_comm(
    DfcEmulator* emulator,
    DfcByteBuf* tx_buffer,
    uint8_t cmd,
    const uint8_t* payload,
    size_t payload_len,
    size_t entry,
    uint8_t comm,
    bool length_unknown);
bool dfc_emulator_command_emit_zero_payload_with_chaining_comm(
    DfcEmulator* emulator,
    DfcByteBuf* tx_buffer,
    uint8_t cmd,
    size_t payload_len,
    uint8_t comm);
bool dfc_emulator_command_file_allows_read(const DfcFile* file, const DfcEmulator* emulator);
bool dfc_emulator_command_file_allows_write(const DfcFile* file, const DfcEmulator* emulator);
#if DFC_ENABLE_RECORD_FILES
bool dfc_emulator_command_file_allows_read_write(const DfcFile* file, const DfcEmulator* emulator);
#endif
uint8_t dfc_emulator_command_file_access_refusal_status(const DfcFile* file, bool write);
#if DFC_ENABLE_DELEGATED_APPLICATIONS
uint16_t
    dfc_emulator_command_delegated_application_overhead_blocks(uint8_t num_keys, size_t key_length);
#endif
void dfc_emulator_command_apply_ev1_response_secure_messaging(
    DfcEmulator* emulator,
    uint8_t cmd,
    DfcByteBuf* tx_buffer);
void dfc_emulator_command_abort_transaction(DfcEmulator* emulator, DfcByteBuf* tx_buffer);
size_t dfc_emulator_command_unwrap_write_payload(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    size_t apdu_len,
    size_t header_len,
    uint8_t comm,
    uint8_t* scratch,
    size_t scratch_cap,
    const uint8_t** clear,
    size_t* scratch_len);
void dfc_emulator_command_clear_pending_chain(DfcEmulator* emulator);
const uint8_t* dfc_emulator_command_auth_key(
    DfcEmulator* emulator,
    uint8_t key_no,
    uint8_t* factory,
    size_t factory_capacity,
    size_t* key_len);
bool dfc_emulator_command_picc_disabled_special_key(DfcEmulator* emulator, uint8_t key_no);
bool dfc_emulator_command_is_available_authentication_key(DfcEmulator* emulator, uint8_t key_no);
bool dfc_emulator_command_has_pending_additional_work(const DfcEmulator* emulator);
void dfc_emulator_command_get_key_settings(DfcEmulator* emulator, DfcByteBuf* tx_buffer);
void dfc_emulator_command_get_key_version(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    size_t apdu_len,
    DfcByteBuf* tx_buffer);
void dfc_emulator_command_change_key(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    size_t apdu_len,
    DfcByteBuf* tx_buffer);
void dfc_emulator_command_change_key_settings(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    size_t apdu_len,
    DfcByteBuf* tx_buffer);
void dfc_emulator_command_set_configuration(
    DfcEmulator* emulator,
    const uint8_t* buffer,
    size_t buffer_len,
    DfcByteBuf* tx_buffer);

void dfc_emulator_command_select_application(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    DfcByteBuf* tx_buffer,
    Dfc* dfc);
void dfc_emulator_command_free_mem(DfcEmulator* emulator, DfcByteBuf* tx_buffer);
void dfc_emulator_command_df_names_continuation(DfcEmulator* emulator, DfcByteBuf* tx_buffer);
void dfc_emulator_command_get_df_names(DfcEmulator* emulator, DfcByteBuf* tx_buffer);
void dfc_emulator_command_format_picc(
    DfcEmulator* emulator,
    size_t command_length,
    DfcByteBuf* tx_buffer);
void dfc_emulator_command_get_file_ids(DfcEmulator* emulator, DfcByteBuf* tx_buffer);
void dfc_emulator_command_get_iso_file_ids(DfcEmulator* emulator, DfcByteBuf* tx_buffer);
void dfc_emulator_command_get_file_settings(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    DfcByteBuf* tx_buffer);
void dfc_emulator_command_change_file_settings(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    size_t apdu_len,
    DfcByteBuf* tx_buffer);
void dfc_emulator_command_get_application_ids(DfcEmulator* emulator, DfcByteBuf* tx_buffer);
void dfc_emulator_command_create_application(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    size_t apdu_len,
    DfcByteBuf* tx_buffer);
void dfc_emulator_command_delete_application(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    DfcByteBuf* tx_buffer);
void dfc_emulator_command_create_std_data_file(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    size_t apdu_len,
    uint8_t file_type,
    DfcByteBuf* tx_buffer);
#if DFC_ENABLE_RECORD_FILES
void dfc_emulator_command_create_record_file(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    size_t apdu_len,
    uint8_t file_type,
    DfcByteBuf* tx_buffer);
#endif
void dfc_emulator_command_create_value_file(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    DfcByteBuf* tx_buffer);
void dfc_emulator_command_delete_file(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    DfcByteBuf* tx_buffer);
#if DFC_ENABLE_TRANSACTION_MAC
void dfc_emulator_command_create_transaction_mac_file(
    DfcEmulator* emulator,
    const uint8_t* buffer,
    size_t buffer_len,
    DfcByteBuf* tx_buffer);
#endif

void dfc_emulator_command_get_value(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    DfcByteBuf* tx_buffer);
void dfc_emulator_command_value_delta(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    size_t apdu_len,
    DfcByteBuf* tx_buffer,
    uint8_t cmd,
    bool debit,
    bool limited_credit);
#if DFC_ENABLE_TRANSACTION_MAC
void dfc_emulator_command_commit_reader_id(
    DfcEmulator* emulator,
    const uint8_t* buffer,
    size_t buffer_len,
    DfcByteBuf* tx_buffer);
bool dfc_emulator_command_transaction_mac_add_data(
    DfcEmulator* emulator,
    uint8_t command,
    uint8_t file_number,
    uint32_t offset,
    uint32_t length,
    const uint8_t* data,
    size_t data_length);
bool dfc_emulator_command_transaction_mac_add_zero_data(
    DfcEmulator* emulator,
    uint8_t command,
    uint8_t file_number,
    uint32_t offset,
    uint32_t length,
    size_t data_length);
bool dfc_emulator_command_transaction_mac_can_add_data(
    const DfcEmulator* emulator,
    size_t data_length);
bool dfc_emulator_command_transaction_mac_add_record_update(
    DfcEmulator* emulator,
    uint8_t command,
    uint8_t file_number,
    uint32_t record_number,
    uint32_t offset,
    uint32_t length,
    const uint8_t* data,
    size_t data_length);
bool dfc_emulator_command_transaction_mac_can_add_record_update(
    const DfcEmulator* emulator,
    size_t data_length);
bool dfc_emulator_command_transaction_mac_add_value(
    DfcEmulator* emulator,
    uint8_t command,
    uint8_t file_number,
    const uint8_t value[4]);
bool dfc_emulator_command_transaction_mac_add_clear_record(
    DfcEmulator* emulator,
    uint8_t file_number);
bool dfc_emulator_command_transaction_mac_can_add_clear_record(const DfcEmulator* emulator);
#endif
bool dfc_emulator_command_transaction_snapshot_begin(DfcEmulator* emulator);
size_t
    dfc_emulator_command_transaction_file_index(const DfcEmulator* emulator, const DfcFile* file);
uint8_t dfc_emulator_command_transaction_record_operation(
    const DfcEmulator* emulator,
    const DfcFile* file);
void dfc_emulator_command_transaction_set_record_operation(
    DfcEmulator* emulator,
    const DfcFile* file,
    uint8_t operation,
    uint32_t record_number);
void dfc_emulator_command_commit_transaction(
    DfcEmulator* emulator,
    bool return_transaction_mac,
    DfcByteBuf* tx_buffer);
void dfc_emulator_command_clear_selected_app_pending_values(DfcEmulator* emulator);
bool dfc_emulator_command_read_data(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    size_t apdu_len,
    DfcByteBuf* tx_buffer,
    Dfc* dfc);
#if DFC_ENABLE_SDM
void dfc_emulator_command_get_file_counters(
    DfcEmulator* emulator,
    const uint8_t* buffer,
    size_t buffer_len,
    DfcByteBuf* tx_buffer);
#endif
void dfc_emulator_command_write_data(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    size_t apdu_len,
    DfcByteBuf* tx_buffer);
#if DFC_ENABLE_RECORD_FILES
void dfc_emulator_command_write_record(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    size_t apdu_len,
    DfcByteBuf* tx_buffer);
void dfc_emulator_command_update_record(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    size_t apdu_len,
    DfcByteBuf* tx_buffer);
void dfc_emulator_command_clear_record_file(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    size_t apdu_len,
    DfcByteBuf* tx_buffer);
#endif
#if DFC_ENABLE_KEY_SETS
void dfc_emulator_command_initialize_key_set(
    DfcEmulator* emulator,
    const uint8_t* buffer,
    size_t buffer_len,
    DfcByteBuf* tx_buffer);
void dfc_emulator_command_finalize_key_set(
    DfcEmulator* emulator,
    const uint8_t* buffer,
    size_t buffer_len,
    DfcByteBuf* tx_buffer);
void dfc_emulator_command_roll_key_set(
    DfcEmulator* emulator,
    const uint8_t* buffer,
    size_t buffer_len,
    DfcByteBuf* tx_buffer);
void dfc_emulator_command_change_key_ev2(
    DfcEmulator* emulator,
    const uint8_t* buffer,
    size_t buffer_len,
    DfcByteBuf* tx_buffer);
#endif
void dfc_emulator_command_authenticate_step1(
    DfcEmulator* emulator,
    uint8_t cmd,
    const uint8_t* apdu,
    DfcByteBuf* tx_buffer);
void dfc_emulator_command_authenticate_step2(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    size_t apdu_len,
    DfcByteBuf* tx_buffer,
    Dfc* dfc);
#if DFC_ENABLE_EV2_SECURE_MESSAGING
bool dfc_emulator_command_authenticate_ev2_start(
    DfcEmulator* emulator,
    const uint8_t* buffer,
    size_t buffer_len,
    bool non_first,
    DfcByteBuf* tx_buffer);
void dfc_emulator_command_authenticate_ev2_continuation(
    DfcEmulator* emulator,
    const uint8_t* buffer,
    size_t buffer_len,
    DfcByteBuf* tx_buffer);
#endif
void dfc_emulator_command_get_version(DfcEmulator* emulator, DfcByteBuf* tx_buffer);
void dfc_emulator_command_get_version_continuation(DfcEmulator* emulator, DfcByteBuf* tx_buffer);

#if DFC_ENABLE_STATIC_SIGNATURE
void dfc_emulator_command_read_signature(
    DfcEmulator* emulator,
    const uint8_t* buffer,
    size_t buffer_len,
    DfcByteBuf* tx_buffer);
#endif

#if DFC_ENABLE_PROXIMITY_CHECK
bool dfc_emulator_command_prepare_proximity_check(DfcEmulator* emulator, DfcByteBuf* tx_buffer);
void dfc_emulator_command_proximity_check(
    DfcEmulator* emulator,
    const uint8_t* buffer,
    size_t buffer_len,
    DfcByteBuf* tx_buffer);
void dfc_emulator_command_verify_proximity_check(
    DfcEmulator* emulator,
    const uint8_t* buffer,
    size_t buffer_len,
    DfcByteBuf* tx_buffer);
#endif

#endif
