#include "dfc_ev2.h"
#include "dfc_ev2_command.h"
#include "dfc_ev2_crypto.h"
#include "dfc_emulator_i.h"

#include <string.h>

#if DFC_ENABLE_EMULATOR

#if DFC_ENABLE_EV2_SECURE_MESSAGING

static DfcFile* target_file(DfcEmulator* emulator, const uint8_t* command, size_t command_len) {
    if(command_len < 2 || emulator->selected_application != DfcEmulatorSelectedApplicationApp)
        return NULL;
    DfcEv2CommandMetadata metadata = dfc_ev2_command_metadata(command[0]);
    if(!dfc_ev2_command_has(&metadata, DfcEv2CommandTargetsFile)) return NULL;
    return dfc_credential_find_file_in_app(
        emulator->credential, emulator->selected_app_index, command[1]);
}

static bool command_file_access(uint8_t command, DfcFileAccess* access) {
    uint8_t base = dfc_record_command_base(command);
    switch(base) {
    case DFC_CMD_READ_DATA:
    case DFC_CMD_READ_RECORDS:
    case DFC_CMD_GET_VALUE:
        *access = DfcFileAccessRead;
        return true;
    case DFC_CMD_WRITE_DATA:
    case DFC_CMD_WRITE_RECORD:
    case DFC_CMD_CREDIT:
    case DFC_CMD_DEBIT:
    case DFC_CMD_LIMITED_CREDIT:
        *access = DfcFileAccessWrite;
        return true;
    case DFC_CMD_UPDATE_RECORD:
        *access = DfcFileAccessReadWrite;
        return true;
    case DFC_CMD_CHANGE_FILE_SETTINGS:
        *access = DfcFileAccessChange;
        return true;
    default:
        return false;
    }
}

static uint8_t command_file_comm(
    DfcEmulator* emulator,
    const uint8_t* command,
    size_t command_len,
    DfcFile** target) {
    DfcFile* file = target_file(emulator, command, command_len);
    DfcFileAccess access;
    if(target) *target = file;
    if(file && command[0] == DFC_CMD_GET_FILE_COUNTERS) return DFC_COMM_ENCIPHERED;
    if(!file || !command_file_access(command[0], &access)) return DFC_COMM_PLAIN;
    uint8_t configured = command[0] == DFC_CMD_CHANGE_FILE_SETTINGS ? DFC_COMM_ENCIPHERED :
                                                                      file->comm_settings;
    return dfc_emulator_file_effective_comm_settings(emulator, file, access, configured);
}

static bool plain_file_command(DfcEmulator* emulator, const uint8_t* command, size_t command_len) {
    const uint8_t* logical = command;
    size_t logical_len = command_len;
    if(command[0] == DFC_CMD_ADDITIONAL_FRAME && emulator->command_chain_active &&
       emulator->command_chain && emulator->command_chain_len > 0) {
        logical = emulator->command_chain;
        logical_len = emulator->command_chain_len;
    }
    DfcFile* file = NULL;
    return command_file_comm(emulator, logical, logical_len, &file) == DFC_COMM_PLAIN && file;
}

static bool command_mac(
    const DfcEmulator* emulator,
    uint8_t instruction,
    const uint8_t* data,
    size_t data_len,
    uint8_t output[DFC_WIRE_MAC_LENGTH]) {
    return dfc_ev2_mac(
        emulator->ev2_session_mac_key,
        instruction,
        emulator->ev2_command_counter,
        emulator->ev2_transaction_identifier,
        data,
        data_len,
        NULL,
        0,
        output);
}

bool dfc_ev2_verify_chained_command_mac(
    const DfcEmulator* emulator,
    uint8_t instruction,
    const uint8_t* data,
    size_t data_len,
    const uint8_t mac[DFC_WIRE_MAC_LENGTH]) {
    uint8_t expected[DFC_WIRE_MAC_LENGTH];
    if(!command_mac(emulator, instruction, data, data_len, expected)) return false;
    return dfc_ct_equal(expected, mac, sizeof(expected));
}

static bool response_mac(
    const DfcEmulator* emulator,
    uint16_t counter,
    uint8_t status,
    const uint8_t* data,
    size_t data_len,
    uint8_t output[DFC_WIRE_MAC_LENGTH]) {
    return dfc_ev2_mac(
        emulator->ev2_session_mac_key,
        status,
        counter,
        emulator->ev2_transaction_identifier,
        data,
        data_len,
        NULL,
        0,
        output);
}

static bool decrypt_command_data(
    const DfcEmulator* emulator,
    const uint8_t* encrypted,
    size_t encrypted_len,
    uint8_t* clear,
    size_t* clear_len,
    bool allow_unpadded) {
    return dfc_ev2_decrypt_data(
        emulator->ev2_session_encryption_key,
        DfcEv2DirectionCommand,
        emulator->ev2_command_counter,
        emulator->ev2_transaction_identifier,
        encrypted,
        encrypted_len,
        clear,
        clear_len,
        allow_unpadded);
}

static bool encrypt_response_data(
    const DfcEmulator* emulator,
    uint16_t counter,
    const uint8_t* clear,
    size_t clear_len,
    uint8_t* encrypted,
    size_t encrypted_capacity,
    size_t* encrypted_len) {
    return dfc_ev2_encrypt_data(
        emulator->ev2_session_encryption_key,
        DfcEv2DirectionResponse,
        counter,
        emulator->ev2_transaction_identifier,
        clear,
        clear_len,
        encrypted,
        encrypted_capacity,
        encrypted_len);
}

DfcEv2CommandSecurity dfc_ev2_prepare_command(
    DfcEmulator* emulator,
    const uint8_t* command,
    size_t command_len,
    uint8_t* clear_command,
    size_t clear_capacity,
    size_t* clear_len) {
    if(!clear_len) return DfcEv2CommandInvalid;
    *clear_len = 0;
    if(!emulator || !command || !clear_command || command_len == 0) return DfcEv2CommandInvalid;
    if(!emulator->ev2_session_active) {
        if(command_len > clear_capacity) return DfcEv2CommandInvalid;
        memmove(clear_command, command, command_len);
        *clear_len = command_len;
        return DfcEv2CommandPlain;
    }
    if(plain_file_command(emulator, command, command_len)) {
        if(emulator->ev2_command_counter == UINT16_MAX || command_len > clear_capacity)
            return DfcEv2CommandInvalid;
        memmove(clear_command, command, command_len);
        *clear_len = command_len;
        return DfcEv2CommandPlainCounted;
    }
    DfcEv2CommandMetadata metadata = dfc_ev2_command_metadata(command[0]);
    if(dfc_ev2_command_has(&metadata, DfcEv2CommandEmulatorBypass)) {
        if(command_len > clear_capacity) return DfcEv2CommandInvalid;
        memmove(clear_command, command, command_len);
        *clear_len = command_len;
        return DfcEv2CommandPlain;
    }
    if(command_len < 1 + DFC_WIRE_MAC_LENGTH) return DfcEv2CommandInvalid;

    size_t secured_data_len = command_len - 1 - DFC_WIRE_MAC_LENGTH;
    const uint8_t* secured_data = command + 1;
    uint8_t expected[DFC_WIRE_MAC_LENGTH] = {0};
    if(!command_mac(emulator, command[0], secured_data, secured_data_len, expected)) {
        dfc_secure_wipe(expected, sizeof(expected));
        return DfcEv2CommandInvalid;
    }
    if(!dfc_ct_equal(expected, command + 1 + secured_data_len, sizeof(expected))) {
        dfc_secure_wipe(expected, sizeof(expected));
        return DfcEv2CommandInvalid;
    }
    dfc_secure_wipe(expected, sizeof(expected));

    if(1 + secured_data_len > clear_capacity) return DfcEv2CommandInvalid;
    DfcFile* file = NULL;
    uint8_t comm = command_file_comm(emulator, command, command_len, &file);
    bool full_command = file && comm == DFC_COMM_ENCIPHERED;
    size_t encrypted_header_len = metadata.encrypted_header_length;
    bool decrypt_data = false;
    if(metadata.encrypted_header_length != DfcEv2NoEncryptedHeader) {
        decrypt_data = !dfc_ev2_command_has(&metadata, DfcEv2CommandTargetsFile) ||
                       (file && comm == DFC_COMM_ENCIPHERED);
    }
    if(decrypt_data) {
        if(secured_data_len <= encrypted_header_len) return DfcEv2CommandInvalid;
        size_t decrypted_len = 0;
        if(!decrypt_command_data(
               emulator,
               secured_data + encrypted_header_len,
               secured_data_len - encrypted_header_len,
               clear_command + 1 + encrypted_header_len,
               &decrypted_len,
               command[0] == DFC_CMD_COMMIT_READER_ID)) {
            dfc_secure_wipe(clear_command, 1 + secured_data_len);
            return DfcEv2CommandInvalid;
        }
        clear_command[0] = command[0];
        memmove(clear_command + 1, secured_data, encrypted_header_len);
        *clear_len = 1 + encrypted_header_len + decrypted_len;
    } else {
        memmove(clear_command, command, 1 + secured_data_len);
        *clear_len = 1 + secured_data_len;
    }
    return full_command || decrypt_data ? DfcEv2CommandFull : DfcEv2CommandSecured;
}

bool dfc_ev2_protect_response(
    DfcEmulator* emulator,
    const uint8_t* clear_command,
    size_t clear_command_len,
    const uint8_t* clear_response,
    size_t clear_response_len,
    uint8_t* secured_response,
    size_t secured_capacity,
    size_t* secured_len) {
    if(!secured_len) return false;
    *secured_len = 0;
    if(!emulator || !clear_command || clear_command_len == 0 || !clear_response ||
       clear_response_len == 0 || !secured_response || emulator->ev2_command_counter == UINT16_MAX)
        return false;
    uint8_t status = clear_response[0];
    const uint8_t* response_data = clear_response + 1;
    size_t response_data_len = clear_response_len - 1;
    DfcFile* file = NULL;
    uint8_t comm = command_file_comm(emulator, clear_command, clear_command_len, &file);
    uint8_t base_command = dfc_record_command_base(clear_command[0]);
    // GetCardUID is Full regardless of file settings; file reads inherit Full
    // from the selected file's effective communication mode.
    bool encrypt =
        base_command == DFC_CMD_GET_CARD_UID ||
        (file && comm == DFC_COMM_ENCIPHERED &&
         (base_command == DFC_CMD_READ_DATA || base_command == DFC_CMD_READ_RECORDS ||
          base_command == DFC_CMD_GET_VALUE || base_command == DFC_CMD_GET_FILE_COUNTERS));

    size_t protected_data_len = response_data_len;
    if(encrypt) {
        if(response_data_len >= secured_capacity) return false;
        protected_data_len = (response_data_len / DFC_AES_KEY_LENGTH + 1) * DFC_AES_KEY_LENGTH;
    }
    if(protected_data_len > DFC_EV2_MAC_DATA_MAX) return false;
    const size_t response_overhead = 1 + DFC_WIRE_MAC_LENGTH;
    if(secured_capacity < response_overhead ||
       protected_data_len > secured_capacity - response_overhead)
        return false;

    uint16_t response_counter = (uint16_t)(emulator->ev2_command_counter + 1);
    uint8_t* protected_data = secured_response + 1;
    if(encrypt) {
        size_t encrypted_len = 0;
        if(!encrypt_response_data(
               emulator,
               response_counter,
               response_data,
               response_data_len,
               protected_data,
               protected_data_len,
               &encrypted_len) ||
           encrypted_len != protected_data_len) {
            dfc_secure_wipe(protected_data, protected_data_len);
            emulator->ev2_session_active = false;
            dfc_secure_wipe(
                emulator->ev2_session_encryption_key,
                sizeof(emulator->ev2_session_encryption_key));
            dfc_secure_wipe(emulator->ev2_session_mac_key, sizeof(emulator->ev2_session_mac_key));
            return false;
        }
    } else {
        memmove(protected_data, response_data, response_data_len);
    }

    uint8_t mac[DFC_WIRE_MAC_LENGTH] = {0};
    if(!response_mac(emulator, response_counter, status, protected_data, protected_data_len, mac)) {
        dfc_secure_wipe(protected_data, protected_data_len);
        dfc_secure_wipe(mac, sizeof(mac));
        emulator->ev2_session_active = false;
        dfc_secure_wipe(
            emulator->ev2_session_encryption_key, sizeof(emulator->ev2_session_encryption_key));
        dfc_secure_wipe(emulator->ev2_session_mac_key, sizeof(emulator->ev2_session_mac_key));
        return false;
    }
    secured_response[0] = status;
    memcpy(secured_response + 1 + protected_data_len, mac, sizeof(mac));
    *secured_len = 1 + protected_data_len + sizeof(mac);
    emulator->ev2_command_counter = response_counter;
    dfc_secure_wipe(mac, sizeof(mac));
    return true;
}

#else

bool dfc_ev2_verify_chained_command_mac(
    const DfcEmulator* emulator,
    uint8_t instruction,
    const uint8_t* data,
    size_t data_len,
    const uint8_t mac[DFC_WIRE_MAC_LENGTH]) {
    DFC_UNUSED(emulator);
    DFC_UNUSED(instruction);
    DFC_UNUSED(data);
    DFC_UNUSED(data_len);
    DFC_UNUSED(mac);
    return false;
}

DfcEv2CommandSecurity dfc_ev2_prepare_command(
    DfcEmulator* emulator,
    const uint8_t* command,
    size_t command_len,
    uint8_t* clear_command,
    size_t clear_capacity,
    size_t* clear_len) {
    DFC_UNUSED(emulator);
    if(command_len > clear_capacity) return DfcEv2CommandInvalid;
    memmove(clear_command, command, command_len);
    *clear_len = command_len;
    return DfcEv2CommandPlain;
}

bool dfc_ev2_protect_response(
    DfcEmulator* emulator,
    const uint8_t* clear_command,
    size_t clear_command_len,
    const uint8_t* clear_response,
    size_t clear_response_len,
    uint8_t* secured_response,
    size_t secured_capacity,
    size_t* secured_len) {
    DFC_UNUSED(emulator);
    DFC_UNUSED(clear_command);
    DFC_UNUSED(clear_command_len);
    if(clear_response_len > secured_capacity) return false;
    memcpy(secured_response, clear_response, clear_response_len);
    *secured_len = clear_response_len;
    return true;
}

#endif

#endif // DFC_ENABLE_EMULATOR
