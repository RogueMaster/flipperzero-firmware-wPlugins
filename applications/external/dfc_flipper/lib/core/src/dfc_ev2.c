#include "dfc_ev2.h"

#if DFC_ENABLE_EV2_SECURE_MESSAGING

#include <string.h>

enum {
    DfcEv2FileCommandHeaderLength = 7,
    DfcEv2UpdateRecordHeaderLength = DFC_UPDATE_RECORD_HEADER_SIZE - 1,
    DfcEv2ValueCommandHeaderLength = 1,
    DfcEv2SetConfigurationHeaderLength = 1,
    DfcEv2CreateTransactionMacHeaderLength = 5,
    DfcEv2CommitReaderIdHeaderLength = 0,
    DfcEv2ChangeKeyHeaderLength = DFC_CHANGE_KEY_EV2_HEADER_LENGTH - 1,
    DfcEv2PaddingMarker = 0x80,
    DfcEv2MacPrefixLength = 1 + DFC_EV2_COUNTER_LENGTH +
                           DFC_EV2_TRANSACTION_IDENTIFIER_LENGTH,
    DfcEv2MaximumMacInputLength = DfcEv2MacPrefixLength + DFC_WORKER_MAX_BUFFER_SIZE,
};

static DfcFile* target_file(DfcEmulator* emulator, const uint8_t* command, size_t command_len) {
    if(command_len < 2 || emulator->selected_application != DfcEmulatorSelectedApplicationApp)
        return NULL;
    switch(command[0]) {
    case DFC_CMD_READ_DATA:
    case DFC_CMD_WRITE_DATA:
    case DFC_CMD_READ_RECORDS:
    case DFC_CMD_WRITE_RECORD:
    case DFC_CMD_UPDATE_RECORD:
    case DFC_CMD_UPDATE_RECORD_ISO:
    case DFC_CMD_GET_VALUE:
    case DFC_CMD_CREDIT:
    case DFC_CMD_DEBIT:
    case DFC_CMD_LIMITED_CREDIT:
        return dfc_credential_find_file_in_app(
            emulator->credential, emulator->selected_app_index, command[1]);
    default:
        return NULL;
    }
}

static bool bypass_secure_messaging(
    DfcEmulator* emulator,
    const uint8_t* command,
    size_t command_len) {
    switch(command[0]) {
    case DFC_CMD_ADDITIONAL_FRAME:
    case DFC_CMD_AUTHENTICATE_EV2_FIRST:
    case DFC_CMD_AUTHENTICATE_EV2_NON_FIRST:
#if DFC_ENABLE_DELEGATED_APPLICATIONS
    case DFC_CMD_CREATE_DELEGATED_APPLICATION:
#endif
        return true;
    default:
        break;
    }
    DfcFile* file = target_file(emulator, command, command_len);
    return file && file->comm_settings == DFC_COMM_PLAIN;
}

static bool wire_mac(
    const uint8_t key[DFC_AES_KEY_LENGTH],
    const uint8_t* input,
    size_t input_len,
    uint8_t output[DFC_WIRE_MAC_LENGTH]) {
    uint8_t full[DFC_AES_CMAC_LENGTH];
    if(!aes_cmac((uint8_t*)key, DFC_AES_KEY_LENGTH, (uint8_t*)input, input_len, full))
        return false;
    for(size_t i = 0; i < DFC_WIRE_MAC_LENGTH; i++) output[i] = full[i * 2 + 1];
    return true;
}

static bool command_mac(
    const DfcEmulator* emulator,
    uint8_t instruction,
    const uint8_t* data,
    size_t data_len,
    uint8_t output[DFC_WIRE_MAC_LENGTH]) {
    uint8_t input[DfcEv2MaximumMacInputLength];
    if(DfcEv2MacPrefixLength + data_len > sizeof(input)) return false;
    input[0] = instruction;
    input[1] = (uint8_t)emulator->ev2_command_counter;
    input[2] = (uint8_t)(emulator->ev2_command_counter >> 8);
    memcpy(input + 3, emulator->ev2_transaction_identifier, DFC_EV2_TRANSACTION_IDENTIFIER_LENGTH);
    memcpy(input + DfcEv2MacPrefixLength, data, data_len);
    return wire_mac(emulator->ev2_session_mac_key, input, DfcEv2MacPrefixLength + data_len, output);
}

bool dfc_ev2_verify_chained_command_mac(
    const DfcEmulator* emulator,
    uint8_t instruction,
    const uint8_t* data,
    size_t data_len,
    const uint8_t mac[DFC_WIRE_MAC_LENGTH]) {
    uint8_t expected[DFC_WIRE_MAC_LENGTH];
    if(!command_mac(emulator, instruction, data, data_len, expected)) return false;
    uint8_t difference = 0;
    for(size_t index = 0; index < sizeof(expected); index++)
        difference |= expected[index] ^ mac[index];
    return difference == 0;
}

static bool response_mac(
    const DfcEmulator* emulator,
    uint8_t status,
    const uint8_t* data,
    size_t data_len,
    uint8_t output[DFC_WIRE_MAC_LENGTH]) {
    uint8_t input[DfcEv2MaximumMacInputLength];
    if(DfcEv2MacPrefixLength + data_len > sizeof(input)) return false;
    input[0] = status;
    input[1] = (uint8_t)emulator->ev2_command_counter;
    input[2] = (uint8_t)(emulator->ev2_command_counter >> 8);
    memcpy(input + 3, emulator->ev2_transaction_identifier, DFC_EV2_TRANSACTION_IDENTIFIER_LENGTH);
    memcpy(input + DfcEv2MacPrefixLength, data, data_len);
    return wire_mac(emulator->ev2_session_mac_key, input, DfcEv2MacPrefixLength + data_len, output);
}

static void derive_iv(
    const DfcEmulator* emulator,
    uint8_t label_high,
    uint8_t label_low,
    uint8_t iv[DFC_AES_KEY_LENGTH]) {
    uint8_t input[DFC_AES_KEY_LENGTH] = {0};
    uint8_t zero_iv[DFC_AES_KEY_LENGTH] = {0};
    input[0] = label_high;
    input[1] = label_low;
    memcpy(input + 2, emulator->ev2_transaction_identifier, DFC_EV2_TRANSACTION_IDENTIFIER_LENGTH);
    input[6] = (uint8_t)emulator->ev2_command_counter;
    input[7] = (uint8_t)(emulator->ev2_command_counter >> 8);
    dfc_worker_aes_cbc_encrypt(
        emulator->ev2_session_encryption_key,
        DFC_AES_KEY_LENGTH,
        zero_iv,
        sizeof(input),
        input,
        iv);
}

static bool decrypt_command_data(
    const DfcEmulator* emulator,
    const uint8_t* encrypted,
    size_t encrypted_len,
    uint8_t* clear,
    size_t* clear_len,
    bool allow_unpadded) {
    if(encrypted_len == 0 || encrypted_len % DFC_AES_KEY_LENGTH != 0) return false;
    uint8_t iv[DFC_AES_KEY_LENGTH];
    derive_iv(emulator, DFC_EV2_ENCRYPTION_LABEL_HIGH, DFC_EV2_ENCRYPTION_LABEL_LOW, iv);
    dfc_worker_aes_cbc_decrypt(
        emulator->ev2_session_encryption_key,
        DFC_AES_KEY_LENGTH,
        iv,
        encrypted_len,
        encrypted,
        clear);
    size_t marker = encrypted_len;
    while(marker > 0 && clear[marker - 1] == 0) marker--;
    if(marker == 0 || clear[marker - 1] != DfcEv2PaddingMarker) {
        if(!allow_unpadded) return false;
        *clear_len = encrypted_len;
        return true;
    }
    *clear_len = marker - 1;
    return true;
}

static bool encrypt_response_data(
    const DfcEmulator* emulator,
    const uint8_t* clear,
    size_t clear_len,
    uint8_t* encrypted,
    size_t encrypted_capacity,
    size_t* encrypted_len) {
    size_t padded_len = ((clear_len / DFC_AES_KEY_LENGTH) + 1) * DFC_AES_KEY_LENGTH;
    if(padded_len > encrypted_capacity) return false;
    uint8_t padded[DFC_WORKER_MAX_BUFFER_SIZE] = {0};
    if(padded_len > sizeof(padded)) return false;
    memcpy(padded, clear, clear_len);
    padded[clear_len] = DfcEv2PaddingMarker;
    uint8_t iv[DFC_AES_KEY_LENGTH];
    derive_iv(emulator, DFC_EV2_MAC_LABEL_HIGH, DFC_EV2_MAC_LABEL_LOW, iv);
    dfc_worker_aes_cbc_encrypt(
        emulator->ev2_session_encryption_key,
        DFC_AES_KEY_LENGTH,
        iv,
        padded_len,
        padded,
        encrypted);
    *encrypted_len = padded_len;
    return true;
}

DfcEv2CommandSecurity dfc_ev2_prepare_command(
    DfcEmulator* emulator,
    const uint8_t* command,
    size_t command_len,
    uint8_t* clear_command,
    size_t clear_capacity,
    size_t* clear_len) {
    if(!emulator->ev2_session_active || bypass_secure_messaging(emulator, command, command_len)) {
        if(command_len > clear_capacity) return DfcEv2CommandInvalid;
        memcpy(clear_command, command, command_len);
        *clear_len = command_len;
        return DfcEv2CommandPlain;
    }
    if(command_len < 1 + DFC_WIRE_MAC_LENGTH) return DfcEv2CommandInvalid;

    size_t secured_data_len = command_len - 1 - DFC_WIRE_MAC_LENGTH;
    const uint8_t* secured_data = command + 1;
    uint8_t expected[DFC_WIRE_MAC_LENGTH];
    if(!command_mac(emulator, command[0], secured_data, secured_data_len, expected))
        return DfcEv2CommandInvalid;
    uint8_t difference = 0;
    for(size_t i = 0; i < sizeof(expected); i++)
        difference |= expected[i] ^ command[1 + secured_data_len + i];
    if(difference != 0) return DfcEv2CommandInvalid;

    if(1 + secured_data_len > clear_capacity) return DfcEv2CommandInvalid;
    clear_command[0] = command[0];
    memcpy(clear_command + 1, secured_data, secured_data_len);
    *clear_len = 1 + secured_data_len;

    DfcFile* file = target_file(emulator, command, command_len);
    size_t encrypted_header_len = 0;
    bool decrypt_data = false;
    if(file && file->comm_settings == DFC_COMM_ENCIPHERED) {
        if(command[0] == DFC_CMD_WRITE_DATA || command[0] == DFC_CMD_WRITE_RECORD)
            encrypted_header_len = DfcEv2FileCommandHeaderLength, decrypt_data = true;
        if(command[0] == DFC_CMD_UPDATE_RECORD || command[0] == DFC_CMD_UPDATE_RECORD_ISO)
            encrypted_header_len = DfcEv2UpdateRecordHeaderLength, decrypt_data = true;
        if(command[0] == DFC_CMD_CREDIT || command[0] == DFC_CMD_DEBIT ||
           command[0] == DFC_CMD_LIMITED_CREDIT)
            encrypted_header_len = DfcEv2ValueCommandHeaderLength, decrypt_data = true;
    }
    if(command[0] == DFC_CMD_SET_CONFIGURATION)
        encrypted_header_len = DfcEv2SetConfigurationHeaderLength, decrypt_data = true;
    if(command[0] == DFC_CMD_CREATE_TRANSACTION_MAC_FILE)
        encrypted_header_len = DfcEv2CreateTransactionMacHeaderLength, decrypt_data = true;
    if(command[0] == DFC_CMD_COMMIT_READER_ID)
        encrypted_header_len = DfcEv2CommitReaderIdHeaderLength, decrypt_data = true;
    if(command[0] == DFC_CMD_CHANGE_KEY_EV2)
        encrypted_header_len = DfcEv2ChangeKeyHeaderLength, decrypt_data = true;
    if(decrypt_data) {
        if(secured_data_len <= encrypted_header_len) return DfcEv2CommandInvalid;
        size_t decrypted_len = 0;
        if(!decrypt_command_data(
               emulator,
               secured_data + encrypted_header_len,
               secured_data_len - encrypted_header_len,
               clear_command + 1 + encrypted_header_len,
               &decrypted_len,
               command[0] == DFC_CMD_COMMIT_READER_ID))
            return DfcEv2CommandInvalid;
        *clear_len = 1 + encrypted_header_len + decrypted_len;
    }
    return DfcEv2CommandSecured;
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
    if(clear_response_len == 0 || emulator->ev2_command_counter == UINT16_MAX) return false;
    uint8_t status = clear_response[0];
    const uint8_t* response_data = clear_response + 1;
    size_t response_data_len = clear_response_len - 1;
    emulator->ev2_command_counter++;

    uint8_t protected_data[DFC_WORKER_MAX_BUFFER_SIZE];
    size_t protected_data_len = response_data_len;
    DfcFile* file = target_file(emulator, clear_command, clear_command_len);
    bool encrypt = file && file->comm_settings == DFC_COMM_ENCIPHERED &&
                   (clear_command[0] == DFC_CMD_READ_DATA ||
                    clear_command[0] == DFC_CMD_READ_RECORDS ||
                    clear_command[0] == DFC_CMD_GET_VALUE);
    if(encrypt) {
        if(!encrypt_response_data(
               emulator,
               response_data,
               response_data_len,
               protected_data,
               sizeof(protected_data),
               &protected_data_len))
            return false;
    } else {
        memcpy(protected_data, response_data, response_data_len);
    }

    if(1 + protected_data_len + DFC_WIRE_MAC_LENGTH > secured_capacity) return false;
    uint8_t mac[DFC_WIRE_MAC_LENGTH];
    if(!response_mac(emulator, status, protected_data, protected_data_len, mac)) return false;
    secured_response[0] = status;
    memcpy(secured_response + 1, protected_data, protected_data_len);
    memcpy(secured_response + 1 + protected_data_len, mac, sizeof(mac));
    *secured_len = 1 + protected_data_len + sizeof(mac);
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
    memcpy(clear_command, command, command_len);
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
