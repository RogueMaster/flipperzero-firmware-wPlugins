#pragma once

// Reader-side encoder for DESFire native commands.
//
// Each function fills a DfcCommand with one native command frame: the
// instruction and the data that follows it, validated the way a card validates
// it, so a caller cannot send a frame the card would reject for its shape. The
// frame travels either bare (ISO 14443-4 native framing) or wrapped in an
// ISO 7816 envelope by dfc_command_to_apdu.
//
// A command carries no secure messaging. The reader (dfc_reader.h) applies the
// session's MAC or encryption when it sends one.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dfc_common.h"

#if DFC_ENABLE_READER

// Largest data field a single native frame carries: the one-octet Lc bounds it.
#define DFC_COMMAND_MAX_DATA 255
// CLA INS P1 P2 Lc data Le.
#define DFC_COMMAND_APDU_OVERHEAD 6
#define DFC_COMMAND_MAX_APDU      (DFC_COMMAND_MAX_DATA + DFC_COMMAND_APDU_OVERHEAD)

#define DFC_COMMAND_AID_LENGTH             3
#define DFC_COMMAND_DF_NAME_MAX_LENGTH     16
#define DFC_COMMAND_READER_ID_LENGTH       16
#define DFC_COMMAND_TMAC_KEY_LENGTH        16
#define DFC_COMMAND_EV2_CAPABILITY_LENGTH  6
#define DFC_COMMAND_UINT24_MAX             0xFFFFFFu

typedef enum {
    DfcCommandOk = 0,
    // A field is out of range, or two fields contradict each other.
    DfcCommandInvalid,
    // The data field would not fit one native frame, or the output would not fit
    // the caller's buffer.
    DfcCommandTooLong,
} DfcCommandStatus;

typedef struct {
    uint8_t ins;
    uint8_t data[DFC_COMMAND_MAX_DATA];
    size_t data_len;
} DfcCommand;

const char* dfc_command_status_name(DfcCommandStatus status);

// Any instruction with any data. The typed functions below are preferred; this
// one exists for instructions they do not name and for frames a caller has
// already secured.
DfcCommandStatus
    dfc_command_encode_raw(DfcCommand* command, uint8_t ins, const uint8_t* data, size_t data_len);

// ISO 7816 envelope: 90 INS 00 00 [Lc data] 00. A command with no data omits Lc.
DfcCommandStatus
    dfc_command_to_apdu(const DfcCommand* command, uint8_t* out, size_t cap, size_t* len);

// Bare native frame: INS data.
DfcCommandStatus
    dfc_command_to_frame(const DfcCommand* command, uint8_t* out, size_t cap, size_t* len);

// ------------------------------------------------------------ discovery ---

// `secondary_aid` may be NULL. With one, the card selects two applications.
DfcCommandStatus dfc_command_select_application(
    DfcCommand* command,
    const uint8_t aid[DFC_COMMAND_AID_LENGTH],
    const uint8_t* secondary_aid);
DfcCommandStatus dfc_command_get_version(DfcCommand* command);
DfcCommandStatus dfc_command_get_free_memory(DfcCommand* command);
DfcCommandStatus dfc_command_format_picc(DfcCommand* command);
DfcCommandStatus dfc_command_get_key_settings(DfcCommand* command);
DfcCommandStatus dfc_command_get_application_ids(DfcCommand* command);
DfcCommandStatus dfc_command_get_df_names(DfcCommand* command);
DfcCommandStatus dfc_command_get_file_ids(DfcCommand* command);
DfcCommandStatus dfc_command_get_iso_file_ids(DfcCommand* command);
DfcCommandStatus dfc_command_get_card_uid(DfcCommand* command);
DfcCommandStatus dfc_command_read_signature(DfcCommand* command, uint8_t signature_id);
DfcCommandStatus dfc_command_get_key_version(DfcCommand* command, uint8_t key_no);
// The versions of every key set of the selected application.
DfcCommandStatus dfc_command_get_key_set_versions(DfcCommand* command);

// --------------------------------------------------------- applications ---

typedef struct {
    uint8_t aid[DFC_COMMAND_AID_LENGTH];
    uint8_t key_settings_1;
    // The ISO file identifier and extended settings flags are set from the fields
    // below, so the caller passes the key type and key count only.
    uint8_t key_settings_2;
    bool has_iso_file_id;
    uint16_t iso_file_id;
    // Requires an ISO file identifier. May be NULL.
    const uint8_t* df_name;
    size_t df_name_len;
    bool has_extended_settings;
    uint8_t extended_settings;
    // Present exactly when extended_settings carries the key-set flag.
    bool has_key_sets;
    uint8_t key_set_version;
    uint8_t key_set_count;
    uint8_t key_set_max_key_size;
    uint8_t key_set_roll_key_no;
} DfcCommandCreateApplication;

DfcCommandStatus
    dfc_command_create_application(DfcCommand* command, const DfcCommandCreateApplication* app);
DfcCommandStatus
    dfc_command_delete_application(DfcCommand* command, const uint8_t aid[DFC_COMMAND_AID_LENGTH]);

typedef struct {
    uint8_t aid[DFC_COMMAND_AID_LENGTH];
    uint16_t slot_number;
    uint8_t slot_version;
    uint16_t quota_limit;
    uint8_t key_settings_1;
    uint8_t key_settings_2;
} DfcCommandCreateDelegatedApplication;

// The first frame only. The reader appends the encrypted key and MAC frame.
DfcCommandStatus dfc_command_create_delegated_application(
    DfcCommand* command,
    const DfcCommandCreateDelegatedApplication* app);
DfcCommandStatus
    dfc_command_get_delegated_application_info(DfcCommand* command, uint16_t slot_number);

// ---------------------------------------------------------------- files ---

typedef struct {
    uint8_t file_no;
    bool has_iso_file_id;
    uint16_t iso_file_id;
    uint8_t comm_settings;
    uint16_t access_rights;
    uint32_t size;
} DfcCommandDataFile;

typedef struct {
    uint8_t file_no;
    bool has_iso_file_id;
    uint16_t iso_file_id;
    uint8_t comm_settings;
    uint16_t access_rights;
    uint32_t record_size;
    uint32_t max_records;
    bool cyclic;
} DfcCommandRecordFile;

typedef struct {
    uint8_t file_no;
    uint8_t comm_settings;
    uint16_t access_rights;
    int32_t lower_limit;
    int32_t upper_limit;
    int32_t value;
    uint8_t limited_credit;
} DfcCommandValueFile;

typedef struct {
    uint8_t file_no;
    uint8_t comm_settings;
    uint16_t access_rights;
    uint8_t key_type;
    uint8_t key[DFC_COMMAND_TMAC_KEY_LENGTH];
    uint8_t key_version;
} DfcCommandTransactionMacFile;

DfcCommandStatus
    dfc_command_create_standard_data_file(DfcCommand* command, const DfcCommandDataFile* file);
DfcCommandStatus
    dfc_command_create_backup_data_file(DfcCommand* command, const DfcCommandDataFile* file);
DfcCommandStatus
    dfc_command_create_record_file(DfcCommand* command, const DfcCommandRecordFile* file);
DfcCommandStatus
    dfc_command_create_value_file(DfcCommand* command, const DfcCommandValueFile* file);
DfcCommandStatus dfc_command_create_transaction_mac_file(
    DfcCommand* command,
    const DfcCommandTransactionMacFile* file);
DfcCommandStatus dfc_command_delete_file(DfcCommand* command, uint8_t file_no);
DfcCommandStatus dfc_command_get_file_settings(DfcCommand* command, uint8_t file_no);
DfcCommandStatus dfc_command_get_file_counters(DfcCommand* command, uint8_t file_no);
// `additional` holds whatever follows the access rights, such as SDM settings.
DfcCommandStatus dfc_command_change_file_settings(
    DfcCommand* command,
    uint8_t file_no,
    uint8_t comm_settings,
    uint16_t access_rights,
    const uint8_t* additional,
    size_t additional_len);

// ----------------------------------------------------------------- data ---

DfcCommandStatus
    dfc_command_read_data(DfcCommand* command, uint8_t file_no, uint32_t offset, uint32_t length);
DfcCommandStatus dfc_command_write_data(
    DfcCommand* command,
    uint8_t file_no,
    uint32_t offset,
    const uint8_t* data,
    size_t data_len);
// `iso_chaining` selects the ISO-chained instruction variant.
DfcCommandStatus dfc_command_read_records(
    DfcCommand* command,
    uint8_t file_no,
    uint32_t record_no,
    uint32_t record_count,
    bool iso_chaining);
DfcCommandStatus dfc_command_write_record(
    DfcCommand* command,
    uint8_t file_no,
    uint32_t offset,
    const uint8_t* data,
    size_t data_len,
    bool iso_chaining);
DfcCommandStatus dfc_command_update_record(
    DfcCommand* command,
    uint8_t file_no,
    uint32_t record_no,
    uint32_t offset,
    const uint8_t* data,
    size_t data_len,
    bool iso_chaining);
DfcCommandStatus dfc_command_clear_record_file(DfcCommand* command, uint8_t file_no);

// --------------------------------------------------- values, transactions ---

DfcCommandStatus dfc_command_get_value(DfcCommand* command, uint8_t file_no);
DfcCommandStatus dfc_command_credit(DfcCommand* command, uint8_t file_no, int32_t amount);
DfcCommandStatus dfc_command_debit(DfcCommand* command, uint8_t file_no, int32_t amount);
DfcCommandStatus dfc_command_limited_credit(DfcCommand* command, uint8_t file_no, int32_t amount);
DfcCommandStatus dfc_command_commit_transaction(DfcCommand* command);
// CommitTransaction carrying its option octet, e.g. to return the transaction MAC.
DfcCommandStatus dfc_command_commit_transaction_with_option(DfcCommand* command, uint8_t option);
DfcCommandStatus dfc_command_commit_reader_id(
    DfcCommand* command,
    const uint8_t reader_id[DFC_COMMAND_READER_ID_LENGTH]);
DfcCommandStatus dfc_command_abort_transaction(DfcCommand* command);

// ----------------------------------------------------------------- keys ---

// `cryptogram` is the already encrypted key data; see dfc_reader.h for building
// it from a session.
DfcCommandStatus dfc_command_change_key(
    DfcCommand* command,
    uint8_t key_no,
    const uint8_t* cryptogram,
    size_t cryptogram_len);
DfcCommandStatus dfc_command_change_key_ev2(
    DfcCommand* command,
    uint8_t key_set_no,
    uint8_t key_no,
    const uint8_t* cryptogram,
    size_t cryptogram_len);
DfcCommandStatus dfc_command_change_key_settings(
    DfcCommand* command,
    const uint8_t* encoded_settings,
    size_t encoded_settings_len);
DfcCommandStatus
    dfc_command_initialize_key_set(DfcCommand* command, uint8_t key_set_no, uint8_t key_type);
DfcCommandStatus
    dfc_command_finalize_key_set(DfcCommand* command, uint8_t key_set_no, uint8_t key_set_version);
DfcCommandStatus dfc_command_roll_key_set(DfcCommand* command, uint8_t key_set_no);

// ---------------------------------------------------------- EV2 and EV3 ---

DfcCommandStatus dfc_command_set_configuration(
    DfcCommand* command,
    uint8_t option,
    const uint8_t* data,
    size_t data_len);
DfcCommandStatus dfc_command_notify_transaction_success(DfcCommand* command);
DfcCommandStatus dfc_command_prepare_proximity_check(DfcCommand* command);
DfcCommandStatus
    dfc_command_proximity_check(DfcCommand* command, const uint8_t* data, size_t data_len);
DfcCommandStatus
    dfc_command_verify_proximity_check(DfcCommand* command, const uint8_t* data, size_t data_len);

// -------------------------------------------------------- authentication ---

// `cipher` is DFC_CMD_AUTHENTICATE_LEGACY, _ISO or _AES.
DfcCommandStatus dfc_command_authenticate(DfcCommand* command, uint8_t cipher, uint8_t key_no);
// `capabilities` may be NULL; otherwise it is six octets.
DfcCommandStatus dfc_command_authenticate_ev2_first(
    DfcCommand* command,
    uint8_t key_no,
    const uint8_t* capabilities,
    size_t capabilities_len);
DfcCommandStatus dfc_command_authenticate_ev2_non_first(DfcCommand* command, uint8_t key_no);
DfcCommandStatus
    dfc_command_additional_frame(DfcCommand* command, const uint8_t* data, size_t data_len);

#endif // DFC_ENABLE_READER
