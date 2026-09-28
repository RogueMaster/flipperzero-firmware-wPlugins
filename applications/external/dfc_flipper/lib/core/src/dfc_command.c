#include "dfc_command.h"

#include <string.h>

#if DFC_ENABLE_READER

// Instructions dfc_common.h does not name because the emulator answers them
// under their plain variants.
#define CMD_READ_RECORDS_ISO_CHAINED 0xAB
#define CMD_WRITE_RECORD_ISO_CHAINED 0x8B

#define KEY_SET_VERSION_SELECTOR 0x40
#define KEY_SET_VERSION_OPTION   0x80

#define EXTENDED_SETTINGS_MAX 0x1F
#define KEY_SET_COUNT_MIN     0x02
#define KEY_SET_COUNT_MAX     0x10
#define KEY_SET_ROLL_KEY_MAX  0x0D

#define FILE_HEADER_LENGTH 7

// A small append-only view over a command's data field. An overflow is sticky,
// so one check at the end covers a whole encoding.
typedef struct {
    DfcCommand* command;
    bool overflow;
} Builder;

static Builder builder_start(DfcCommand* command, uint8_t ins) {
    command->ins = ins;
    command->data_len = 0;
    Builder b = {command, false};
    return b;
}

static void put(Builder* b, const uint8_t* bytes, size_t n) {
    if(b->overflow || n > DFC_COMMAND_MAX_DATA - b->command->data_len) {
        b->overflow = true;
        return;
    }
    if(n) memcpy(b->command->data + b->command->data_len, bytes, n);
    b->command->data_len += n;
}

static void put_byte(Builder* b, uint8_t value) {
    put(b, &value, 1);
}

static void put_u16(Builder* b, uint16_t value) {
    put_byte(b, (uint8_t)value);
    put_byte(b, (uint8_t)(value >> 8));
}

static void put_u24(Builder* b, uint32_t value) {
    put_byte(b, (uint8_t)value);
    put_byte(b, (uint8_t)(value >> 8));
    put_byte(b, (uint8_t)(value >> 16));
}

static void put_i32(Builder* b, int32_t value) {
    uint32_t raw = (uint32_t)value;
    put_byte(b, (uint8_t)raw);
    put_byte(b, (uint8_t)(raw >> 8));
    put_byte(b, (uint8_t)(raw >> 16));
    put_byte(b, (uint8_t)(raw >> 24));
}

static DfcCommandStatus builder_finish(const Builder* b) {
    return b->overflow ? DfcCommandTooLong : DfcCommandOk;
}

static DfcCommandStatus simple(DfcCommand* command, uint8_t ins) {
    if(!command) return DfcCommandInvalid;
    Builder b = builder_start(command, ins);
    return builder_finish(&b);
}

static DfcCommandStatus with_byte(DfcCommand* command, uint8_t ins, uint8_t value) {
    if(!command) return DfcCommandInvalid;
    Builder b = builder_start(command, ins);
    put_byte(&b, value);
    return builder_finish(&b);
}

static DfcCommandStatus with_bytes(
    DfcCommand* command,
    uint8_t ins,
    const uint8_t* data,
    size_t data_len) {
    if(!command || (!data && data_len)) return DfcCommandInvalid;
    Builder b = builder_start(command, ins);
    put(&b, data, data_len);
    return builder_finish(&b);
}

static bool iso_file_id_is_reserved(uint16_t value) {
    return value == 0x0000 || value == 0x3F00 || value == 0x3FFF || value == 0xFFFF;
}

const char* dfc_command_status_name(DfcCommandStatus status) {
    switch(status) {
    case DfcCommandOk:
        return "ok";
    case DfcCommandInvalid:
        return "invalid";
    case DfcCommandTooLong:
        return "too long";
    }
    return "unknown";
}

DfcCommandStatus
    dfc_command_encode_raw(DfcCommand* command, uint8_t ins, const uint8_t* data, size_t data_len) {
    return with_bytes(command, ins, data, data_len);
}

DfcCommandStatus
    dfc_command_to_apdu(const DfcCommand* command, uint8_t* out, size_t cap, size_t* len) {
    if(!command || !out || !len || command->data_len > DFC_COMMAND_MAX_DATA) {
        return DfcCommandInvalid;
    }
    size_t needed = command->data_len ? command->data_len + DFC_COMMAND_APDU_OVERHEAD : 5;
    if(needed > cap) return DfcCommandTooLong;
    out[0] = DFC_ISO7816_CLA_WRAPPER;
    out[1] = command->ins;
    out[2] = 0x00;
    out[3] = 0x00;
    if(command->data_len) {
        out[4] = (uint8_t)command->data_len;
        memcpy(out + 5, command->data, command->data_len);
        out[5 + command->data_len] = 0x00;
    } else {
        out[4] = 0x00;
    }
    *len = needed;
    return DfcCommandOk;
}

DfcCommandStatus
    dfc_command_to_frame(const DfcCommand* command, uint8_t* out, size_t cap, size_t* len) {
    if(!command || !out || !len || command->data_len > DFC_COMMAND_MAX_DATA) {
        return DfcCommandInvalid;
    }
    if(command->data_len + 1 > cap) return DfcCommandTooLong;
    out[0] = command->ins;
    if(command->data_len) memcpy(out + 1, command->data, command->data_len);
    *len = command->data_len + 1;
    return DfcCommandOk;
}

// ------------------------------------------------------------ discovery ---

DfcCommandStatus dfc_command_select_application(
    DfcCommand* command,
    const uint8_t aid[DFC_COMMAND_AID_LENGTH],
    const uint8_t* secondary_aid) {
    if(!command || !aid) return DfcCommandInvalid;
    Builder b = builder_start(command, DFC_CMD_SELECT_APPLICATION);
    put(&b, aid, DFC_COMMAND_AID_LENGTH);
    if(secondary_aid) put(&b, secondary_aid, DFC_COMMAND_AID_LENGTH);
    return builder_finish(&b);
}

DfcCommandStatus dfc_command_get_version(DfcCommand* command) {
    return simple(command, DFC_CMD_GET_VERSION);
}

DfcCommandStatus dfc_command_get_free_memory(DfcCommand* command) {
    return simple(command, DFC_CMD_FREE_MEM);
}

DfcCommandStatus dfc_command_format_picc(DfcCommand* command) {
    return simple(command, DFC_CMD_FORMAT_PICC);
}

DfcCommandStatus dfc_command_get_key_settings(DfcCommand* command) {
    return simple(command, DFC_CMD_GET_KEY_SETTINGS);
}

DfcCommandStatus dfc_command_get_application_ids(DfcCommand* command) {
    return simple(command, DFC_CMD_GET_APPLICATION_IDS);
}

DfcCommandStatus dfc_command_get_df_names(DfcCommand* command) {
    return simple(command, DFC_CMD_GET_DF_NAMES);
}

DfcCommandStatus dfc_command_get_file_ids(DfcCommand* command) {
    return simple(command, DFC_CMD_GET_FILE_IDS);
}

DfcCommandStatus dfc_command_get_iso_file_ids(DfcCommand* command) {
    return simple(command, DFC_CMD_GET_ISO_FILE_IDS);
}

DfcCommandStatus dfc_command_get_card_uid(DfcCommand* command) {
    return simple(command, DFC_CMD_GET_CARD_UID);
}

DfcCommandStatus dfc_command_read_signature(DfcCommand* command, uint8_t signature_id) {
    return with_byte(command, DFC_CMD_READ_SIGNATURE, signature_id);
}

DfcCommandStatus dfc_command_get_key_version(DfcCommand* command, uint8_t key_no) {
    return with_byte(command, DFC_CMD_GET_KEY_VERSION, key_no);
}

DfcCommandStatus dfc_command_get_key_set_versions(DfcCommand* command) {
    const uint8_t query[] = {KEY_SET_VERSION_SELECTOR, KEY_SET_VERSION_OPTION};
    return with_bytes(command, DFC_CMD_GET_KEY_VERSION, query, sizeof(query));
}

// --------------------------------------------------------- applications ---

static bool create_application_is_valid(const DfcCommandCreateApplication* app) {
    if(app->df_name_len && !app->df_name) return false;
    if(app->df_name && !app->has_iso_file_id) return false;
    if(app->has_iso_file_id && iso_file_id_is_reserved(app->iso_file_id)) return false;
    if(app->df_name && (app->df_name_len < 1 || app->df_name_len > DFC_COMMAND_DF_NAME_MAX_LENGTH))
        return false;
    if(!app->has_extended_settings) return !app->has_key_sets;
    if(app->extended_settings > EXTENDED_SETTINGS_MAX) return false;
    bool flagged = (app->extended_settings & DFC_EXTENDED_SETTINGS_KEY_SETS) != 0;
    if(flagged != app->has_key_sets) return false;
    if(!app->has_key_sets) return true;
    if(app->key_set_count < KEY_SET_COUNT_MIN || app->key_set_count > KEY_SET_COUNT_MAX)
        return false;
    if(app->key_set_roll_key_no > KEY_SET_ROLL_KEY_MAX) return false;
    return app->key_set_max_key_size == DFC_KEY_SET_MAXIMUM_16_BYTE ||
           app->key_set_max_key_size == DFC_KEY_SET_MAXIMUM_24_BYTE;
}

DfcCommandStatus
    dfc_command_create_application(DfcCommand* command, const DfcCommandCreateApplication* app) {
    if(!command || !app || !create_application_is_valid(app)) return DfcCommandInvalid;
    Builder b = builder_start(command, DFC_CMD_CREATE_APPLICATION);
    put(&b, app->aid, DFC_COMMAND_AID_LENGTH);
    put_byte(&b, app->key_settings_1);
    uint8_t ks2 = app->key_settings_2;
    if(app->has_iso_file_id) ks2 |= DFC_KS2_ISO_FILE_IDS;
    if(app->has_extended_settings) ks2 |= DFC_KS2_EXTENDED_SETTINGS;
    put_byte(&b, ks2);
    if(app->has_extended_settings) {
        put_byte(&b, app->extended_settings);
        if(app->has_key_sets) {
            put_byte(&b, app->key_set_version);
            put_byte(&b, app->key_set_count);
            put_byte(&b, app->key_set_max_key_size);
            put_byte(&b, app->key_set_roll_key_no);
        }
    }
    if(app->has_iso_file_id) put_u16(&b, app->iso_file_id);
    if(app->df_name) put(&b, app->df_name, app->df_name_len);
    return builder_finish(&b);
}

DfcCommandStatus
    dfc_command_delete_application(DfcCommand* command, const uint8_t aid[DFC_COMMAND_AID_LENGTH]) {
    if(!aid) return DfcCommandInvalid;
    return with_bytes(command, DFC_CMD_DELETE_APPLICATION, aid, DFC_COMMAND_AID_LENGTH);
}

DfcCommandStatus dfc_command_create_delegated_application(
    DfcCommand* command,
    const DfcCommandCreateDelegatedApplication* app) {
    if(!command || !app) return DfcCommandInvalid;
    Builder b = builder_start(command, DFC_CMD_CREATE_DELEGATED_APPLICATION);
    put(&b, app->aid, DFC_COMMAND_AID_LENGTH);
    put_u16(&b, app->slot_number);
    put_byte(&b, app->slot_version);
    put_u16(&b, app->quota_limit);
    put_byte(&b, app->key_settings_1);
    put_byte(&b, app->key_settings_2);
    return builder_finish(&b);
}

DfcCommandStatus
    dfc_command_get_delegated_application_info(DfcCommand* command, uint16_t slot_number) {
    if(!command) return DfcCommandInvalid;
    Builder b = builder_start(command, DFC_CMD_GET_DELEGATED_INFO);
    put_u16(&b, slot_number);
    return builder_finish(&b);
}

// ---------------------------------------------------------------- files ---

static void put_file_head(
    Builder* b,
    uint8_t file_no,
    bool has_iso_file_id,
    uint16_t iso_file_id,
    uint8_t comm_settings,
    uint16_t access_rights) {
    put_byte(b, file_no);
    if(has_iso_file_id) put_u16(b, iso_file_id);
    put_byte(b, comm_settings);
    put_u16(b, access_rights);
}

static DfcCommandStatus
    data_file(DfcCommand* command, uint8_t ins, const DfcCommandDataFile* file) {
    if(!command || !file || file->size > DFC_COMMAND_UINT24_MAX) return DfcCommandInvalid;
    if(file->has_iso_file_id && iso_file_id_is_reserved(file->iso_file_id))
        return DfcCommandInvalid;
    Builder b = builder_start(command, ins);
    put_file_head(
        &b,
        file->file_no,
        file->has_iso_file_id,
        file->iso_file_id,
        file->comm_settings,
        file->access_rights);
    put_u24(&b, file->size);
    return builder_finish(&b);
}

DfcCommandStatus
    dfc_command_create_standard_data_file(DfcCommand* command, const DfcCommandDataFile* file) {
    return data_file(command, DFC_CMD_CREATE_STD_DATA_FILE, file);
}

DfcCommandStatus
    dfc_command_create_backup_data_file(DfcCommand* command, const DfcCommandDataFile* file) {
    return data_file(command, DFC_CMD_CREATE_BACKUP_DATA_FILE, file);
}

DfcCommandStatus
    dfc_command_create_record_file(DfcCommand* command, const DfcCommandRecordFile* file) {
    if(!command || !file) return DfcCommandInvalid;
    if(file->record_size == 0 || file->record_size > DFC_COMMAND_UINT24_MAX) {
        return DfcCommandInvalid;
    }
    if(file->max_records == 0 || file->max_records > DFC_COMMAND_UINT24_MAX) {
        return DfcCommandInvalid;
    }
    if(file->has_iso_file_id && iso_file_id_is_reserved(file->iso_file_id))
        return DfcCommandInvalid;
    Builder b = builder_start(
        command,
        file->cyclic ? DFC_CMD_CREATE_CYCLIC_RECORD_FILE : DFC_CMD_CREATE_LINEAR_RECORD_FILE);
    put_file_head(
        &b,
        file->file_no,
        file->has_iso_file_id,
        file->iso_file_id,
        file->comm_settings,
        file->access_rights);
    put_u24(&b, file->record_size);
    put_u24(&b, file->max_records);
    return builder_finish(&b);
}

DfcCommandStatus
    dfc_command_create_value_file(DfcCommand* command, const DfcCommandValueFile* file) {
    if(!command || !file) return DfcCommandInvalid;
    if(file->lower_limit > file->upper_limit || file->value < file->lower_limit ||
       file->value > file->upper_limit) {
        return DfcCommandInvalid;
    }
    Builder b = builder_start(command, DFC_CMD_CREATE_VALUE_FILE);
    put_file_head(&b, file->file_no, false, 0, file->comm_settings, file->access_rights);
    put_i32(&b, file->lower_limit);
    put_i32(&b, file->upper_limit);
    put_i32(&b, file->value);
    put_byte(&b, file->limited_credit);
    return builder_finish(&b);
}

DfcCommandStatus dfc_command_create_transaction_mac_file(
    DfcCommand* command,
    const DfcCommandTransactionMacFile* file) {
    if(!command || !file) return DfcCommandInvalid;
    Builder b = builder_start(command, DFC_CMD_CREATE_TRANSACTION_MAC_FILE);
    put_file_head(&b, file->file_no, false, 0, file->comm_settings, file->access_rights);
    put_byte(&b, file->key_type);
    put(&b, file->key, DFC_COMMAND_TMAC_KEY_LENGTH);
    put_byte(&b, file->key_version);
    return builder_finish(&b);
}

DfcCommandStatus dfc_command_delete_file(DfcCommand* command, uint8_t file_no) {
    return with_byte(command, DFC_CMD_DELETE_FILE, file_no);
}

DfcCommandStatus dfc_command_get_file_settings(DfcCommand* command, uint8_t file_no) {
    return with_byte(command, DFC_CMD_GET_FILE_SETTINGS, file_no);
}

DfcCommandStatus dfc_command_get_file_counters(DfcCommand* command, uint8_t file_no) {
    return with_byte(command, DFC_CMD_GET_FILE_COUNTERS, file_no);
}

DfcCommandStatus dfc_command_change_file_settings(
    DfcCommand* command,
    uint8_t file_no,
    uint8_t comm_settings,
    uint16_t access_rights,
    const uint8_t* additional,
    size_t additional_len) {
    if(!command || (!additional && additional_len)) return DfcCommandInvalid;
    Builder b = builder_start(command, DFC_CMD_CHANGE_FILE_SETTINGS);
    put_byte(&b, file_no);
    put_byte(&b, comm_settings);
    put_u16(&b, access_rights);
    put(&b, additional, additional_len);
    return builder_finish(&b);
}

// ----------------------------------------------------------------- data ---

static DfcCommandStatus file_range(
    DfcCommand* command,
    uint8_t ins,
    uint8_t file_no,
    uint32_t first,
    uint32_t second,
    const uint8_t* content,
    size_t content_len) {
    if(!command || (!content && content_len)) return DfcCommandInvalid;
    if(first > DFC_COMMAND_UINT24_MAX || second > DFC_COMMAND_UINT24_MAX) {
        return DfcCommandInvalid;
    }
    Builder b = builder_start(command, ins);
    put_byte(&b, file_no);
    put_u24(&b, first);
    put_u24(&b, second);
    put(&b, content, content_len);
    return builder_finish(&b);
}

DfcCommandStatus
    dfc_command_read_data(DfcCommand* command, uint8_t file_no, uint32_t offset, uint32_t length) {
    return file_range(command, DFC_CMD_READ_DATA, file_no, offset, length, NULL, 0);
}

DfcCommandStatus dfc_command_write_data(
    DfcCommand* command,
    uint8_t file_no,
    uint32_t offset,
    const uint8_t* data,
    size_t data_len) {
    if(data_len > DFC_COMMAND_MAX_DATA - FILE_HEADER_LENGTH) return DfcCommandTooLong;
    return file_range(
        command, DFC_CMD_WRITE_DATA, file_no, offset, (uint32_t)data_len, data, data_len);
}

DfcCommandStatus dfc_command_read_records(
    DfcCommand* command,
    uint8_t file_no,
    uint32_t record_no,
    uint32_t record_count,
    bool iso_chaining) {
    return file_range(
        command,
        iso_chaining ? CMD_READ_RECORDS_ISO_CHAINED : DFC_CMD_READ_RECORDS,
        file_no,
        record_no,
        record_count,
        NULL,
        0);
}

DfcCommandStatus dfc_command_write_record(
    DfcCommand* command,
    uint8_t file_no,
    uint32_t offset,
    const uint8_t* data,
    size_t data_len,
    bool iso_chaining) {
    if(data_len > DFC_COMMAND_MAX_DATA - FILE_HEADER_LENGTH) return DfcCommandTooLong;
    return file_range(
        command,
        iso_chaining ? CMD_WRITE_RECORD_ISO_CHAINED : DFC_CMD_WRITE_RECORD,
        file_no,
        offset,
        (uint32_t)data_len,
        data,
        data_len);
}

DfcCommandStatus dfc_command_update_record(
    DfcCommand* command,
    uint8_t file_no,
    uint32_t record_no,
    uint32_t offset,
    const uint8_t* data,
    size_t data_len,
    bool iso_chaining) {
    if(!command || (!data && data_len)) return DfcCommandInvalid;
    if(record_no > DFC_COMMAND_UINT24_MAX || offset > DFC_COMMAND_UINT24_MAX ||
       data_len > DFC_COMMAND_UINT24_MAX) {
        return DfcCommandInvalid;
    }
    Builder b = builder_start(
        command, iso_chaining ? DFC_CMD_UPDATE_RECORD_ISO : DFC_CMD_UPDATE_RECORD);
    put_byte(&b, file_no);
    put_u24(&b, record_no);
    put_u24(&b, offset);
    put_u24(&b, (uint32_t)data_len);
    put(&b, data, data_len);
    return builder_finish(&b);
}

DfcCommandStatus dfc_command_clear_record_file(DfcCommand* command, uint8_t file_no) {
    return with_byte(command, DFC_CMD_CLEAR_RECORD_FILE, file_no);
}

// --------------------------------------------------- values, transactions ---

DfcCommandStatus dfc_command_get_value(DfcCommand* command, uint8_t file_no) {
    return with_byte(command, DFC_CMD_GET_VALUE, file_no);
}

static DfcCommandStatus change_value(DfcCommand* command, uint8_t ins, uint8_t file_no, int32_t amount) {
    if(!command) return DfcCommandInvalid;
    Builder b = builder_start(command, ins);
    put_byte(&b, file_no);
    put_i32(&b, amount);
    return builder_finish(&b);
}

DfcCommandStatus dfc_command_credit(DfcCommand* command, uint8_t file_no, int32_t amount) {
    return change_value(command, DFC_CMD_CREDIT, file_no, amount);
}

DfcCommandStatus dfc_command_debit(DfcCommand* command, uint8_t file_no, int32_t amount) {
    return change_value(command, DFC_CMD_DEBIT, file_no, amount);
}

DfcCommandStatus dfc_command_limited_credit(DfcCommand* command, uint8_t file_no, int32_t amount) {
    return change_value(command, DFC_CMD_LIMITED_CREDIT, file_no, amount);
}

DfcCommandStatus dfc_command_commit_transaction(DfcCommand* command) {
    return simple(command, DFC_CMD_COMMIT_TRANSACTION);
}

DfcCommandStatus dfc_command_commit_transaction_with_option(DfcCommand* command, uint8_t option) {
    return with_byte(command, DFC_CMD_COMMIT_TRANSACTION, option);
}

DfcCommandStatus dfc_command_commit_reader_id(
    DfcCommand* command,
    const uint8_t reader_id[DFC_COMMAND_READER_ID_LENGTH]) {
    if(!reader_id) return DfcCommandInvalid;
    return with_bytes(command, DFC_CMD_COMMIT_READER_ID, reader_id, DFC_COMMAND_READER_ID_LENGTH);
}

DfcCommandStatus dfc_command_abort_transaction(DfcCommand* command) {
    return simple(command, DFC_CMD_ABORT_TRANSACTION);
}

// ----------------------------------------------------------------- keys ---

DfcCommandStatus dfc_command_change_key(
    DfcCommand* command,
    uint8_t key_no,
    const uint8_t* cryptogram,
    size_t cryptogram_len) {
    if(!command || !cryptogram || cryptogram_len == 0) return DfcCommandInvalid;
    Builder b = builder_start(command, DFC_CMD_CHANGE_KEY);
    put_byte(&b, key_no);
    put(&b, cryptogram, cryptogram_len);
    return builder_finish(&b);
}

DfcCommandStatus dfc_command_change_key_ev2(
    DfcCommand* command,
    uint8_t key_set_no,
    uint8_t key_no,
    const uint8_t* cryptogram,
    size_t cryptogram_len) {
    if(!command || !cryptogram || cryptogram_len == 0) return DfcCommandInvalid;
    Builder b = builder_start(command, DFC_CMD_CHANGE_KEY_EV2);
    put_byte(&b, key_set_no);
    put_byte(&b, key_no);
    put(&b, cryptogram, cryptogram_len);
    return builder_finish(&b);
}

DfcCommandStatus dfc_command_change_key_settings(
    DfcCommand* command,
    const uint8_t* encoded_settings,
    size_t encoded_settings_len) {
    if(encoded_settings_len == 0) return DfcCommandInvalid;
    return with_bytes(
        command, DFC_CMD_CHANGE_KEY_SETTINGS, encoded_settings, encoded_settings_len);
}

DfcCommandStatus
    dfc_command_initialize_key_set(DfcCommand* command, uint8_t key_set_no, uint8_t key_type) {
    const uint8_t data[] = {key_set_no, key_type};
    return with_bytes(command, DFC_CMD_INITIALIZE_KEY_SET, data, sizeof(data));
}

DfcCommandStatus dfc_command_finalize_key_set(
    DfcCommand* command,
    uint8_t key_set_no,
    uint8_t key_set_version) {
    const uint8_t data[] = {key_set_no, key_set_version};
    return with_bytes(command, DFC_CMD_FINALIZE_KEY_SET, data, sizeof(data));
}

DfcCommandStatus dfc_command_roll_key_set(DfcCommand* command, uint8_t key_set_no) {
    return with_byte(command, DFC_CMD_ROLL_KEY_SET, key_set_no);
}

// ---------------------------------------------------------- EV2 and EV3 ---

DfcCommandStatus dfc_command_set_configuration(
    DfcCommand* command,
    uint8_t option,
    const uint8_t* data,
    size_t data_len) {
    if(!command || (!data && data_len)) return DfcCommandInvalid;
    Builder b = builder_start(command, DFC_CMD_SET_CONFIGURATION);
    put_byte(&b, option);
    put(&b, data, data_len);
    return builder_finish(&b);
}

DfcCommandStatus dfc_command_notify_transaction_success(DfcCommand* command) {
    return simple(command, DFC_CMD_NOTIFY_TRANSACTION_SUCCESS);
}

DfcCommandStatus dfc_command_prepare_proximity_check(DfcCommand* command) {
    return simple(command, DFC_CMD_PREPARE_PROXIMITY_CHECK);
}

DfcCommandStatus
    dfc_command_proximity_check(DfcCommand* command, const uint8_t* data, size_t data_len) {
    return with_bytes(command, DFC_CMD_PROXIMITY_CHECK, data, data_len);
}

DfcCommandStatus
    dfc_command_verify_proximity_check(DfcCommand* command, const uint8_t* data, size_t data_len) {
    return with_bytes(command, DFC_CMD_VERIFY_PROXIMITY_CHECK, data, data_len);
}

// -------------------------------------------------------- authentication ---

DfcCommandStatus dfc_command_authenticate(DfcCommand* command, uint8_t cipher, uint8_t key_no) {
    if(cipher != DFC_CMD_AUTHENTICATE_LEGACY && cipher != DFC_CMD_AUTHENTICATE_ISO &&
       cipher != DFC_CMD_AUTHENTICATE_AES) {
        return DfcCommandInvalid;
    }
    return with_byte(command, cipher, key_no);
}

DfcCommandStatus dfc_command_authenticate_ev2_first(
    DfcCommand* command,
    uint8_t key_no,
    const uint8_t* capabilities,
    size_t capabilities_len) {
    if(!command) return DfcCommandInvalid;
    if(capabilities ? capabilities_len != DFC_COMMAND_EV2_CAPABILITY_LENGTH :
                      capabilities_len != 0) {
        return DfcCommandInvalid;
    }
    Builder b = builder_start(command, DFC_CMD_AUTHENTICATE_EV2_FIRST);
    put_byte(&b, key_no);
    put_byte(&b, (uint8_t)capabilities_len);
    put(&b, capabilities, capabilities_len);
    return builder_finish(&b);
}

DfcCommandStatus dfc_command_authenticate_ev2_non_first(DfcCommand* command, uint8_t key_no) {
    return with_byte(command, DFC_CMD_AUTHENTICATE_EV2_NON_FIRST, key_no);
}

DfcCommandStatus
    dfc_command_additional_frame(DfcCommand* command, const uint8_t* data, size_t data_len) {
    return with_bytes(command, DFC_CMD_ADDITIONAL_FRAME, data, data_len);
}

#endif // DFC_ENABLE_READER
