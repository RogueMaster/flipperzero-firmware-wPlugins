#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dfc_common.h"

typedef struct {
    uint8_t session_key[DFC_AES_KEY_LENGTH];
    uint8_t chain[DFC_AES_KEY_LENGTH];
    uint8_t pending[DFC_AES_KEY_LENGTH];
    bool initialized;
    bool has_pending;
} DfcTransactionMacAccumulator;

bool dfc_transaction_mac_derive_session_key(
    const uint8_t key[DFC_AES_KEY_LENGTH],
    const uint8_t* uid,
    size_t uid_length,
    uint32_t counter,
    uint8_t session_key[DFC_AES_KEY_LENGTH]);

void dfc_transaction_mac_accumulator_clear(DfcTransactionMacAccumulator* accumulator);
bool dfc_transaction_mac_accumulator_init(
    DfcTransactionMacAccumulator* accumulator,
    const uint8_t session_key[DFC_AES_KEY_LENGTH]);
bool dfc_transaction_mac_accumulator_add_data(
    DfcTransactionMacAccumulator* accumulator,
    uint8_t command,
    uint8_t file_number,
    uint32_t offset,
    uint32_t length,
    const uint8_t* data,
    size_t data_length);
bool dfc_transaction_mac_accumulator_add_zero_data(
    DfcTransactionMacAccumulator* accumulator,
    uint8_t command,
    uint8_t file_number,
    uint32_t offset,
    uint32_t length,
    size_t data_length);
bool dfc_transaction_mac_accumulator_add_record_update(
    DfcTransactionMacAccumulator* accumulator,
    uint8_t command,
    uint8_t file_number,
    uint32_t record_number,
    uint32_t offset,
    uint32_t length,
    const uint8_t* data,
    size_t data_length);
bool dfc_transaction_mac_accumulator_add_value(
    DfcTransactionMacAccumulator* accumulator,
    uint8_t command,
    uint8_t file_number,
    const uint8_t value[4]);
bool dfc_transaction_mac_accumulator_add_reader_id(
    DfcTransactionMacAccumulator* accumulator,
    const uint8_t current_reader_id[DFC_TRANSACTION_READER_ID_LENGTH],
    const uint8_t encrypted_previous_reader_id[DFC_TRANSACTION_READER_ID_LENGTH]);
bool dfc_transaction_mac_accumulator_add_clear_record(
    DfcTransactionMacAccumulator* accumulator,
    uint8_t file_number);
bool dfc_transaction_mac_accumulator_finalize(
    const DfcTransactionMacAccumulator* accumulator,
    uint8_t mac[DFC_AES_CMAC_LENGTH]);

bool dfc_transaction_mac_input_can_add_data(
    size_t input_capacity,
    size_t input_length,
    size_t data_length);
bool dfc_transaction_mac_input_can_add_record_update(
    size_t input_capacity,
    size_t input_length,
    size_t data_length);
bool dfc_transaction_mac_input_can_add_value(size_t input_capacity, size_t input_length);
bool dfc_transaction_mac_input_can_add_clear_record(size_t input_capacity, size_t input_length);
bool dfc_transaction_mac_input_can_add_reader_id(
    size_t input_capacity,
    size_t input_length,
    bool authenticated);

bool dfc_transaction_mac_input_add_data(
    uint8_t* input,
    size_t input_capacity,
    size_t* input_length,
    uint8_t command,
    uint8_t file_number,
    uint32_t offset,
    uint32_t length,
    const uint8_t* data,
    size_t data_length);

bool dfc_transaction_mac_input_add_record_update(
    uint8_t* input,
    size_t input_capacity,
    size_t* input_length,
    uint8_t command,
    uint8_t file_number,
    uint32_t record_number,
    uint32_t offset,
    uint32_t length,
    const uint8_t* data,
    size_t data_length);

bool dfc_transaction_mac_input_add_value(
    uint8_t* input,
    size_t input_capacity,
    size_t* input_length,
    uint8_t command,
    uint8_t file_number,
    const uint8_t value[4]);

bool dfc_transaction_mac_input_add_reader_id(
    uint8_t* input,
    size_t input_capacity,
    size_t* input_length,
    const uint8_t current_reader_id[16],
    const uint8_t encrypted_previous_reader_id[16]);

bool dfc_transaction_mac_input_add_clear_record(
    uint8_t* input,
    size_t input_capacity,
    size_t* input_length,
    uint8_t file_number);
