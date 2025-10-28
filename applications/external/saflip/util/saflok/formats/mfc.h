#pragma once

#include "../helper.h"

void saflok_mfc_generate_key(const uint8_t* uid, uint8_t sector, uint8_t* key);

void saflok_generate_mf_classic(
    NfcDevice* nfc_device,
    uint8_t* uid,
    size_t uid_len,
    BasicAccessData* saflok_data);
bool saflok_parse_mf_classic(
    NfcDevice* nfc_device,
    uint8_t* uid,
    size_t* uid_len,
    BasicAccessData* saflok_data);

void saflok_generate_mf_classic_logs(
    NfcDevice* nfc_device,
    LogEntry* log_entries,
    size_t num_log_entries);
size_t saflok_parse_mf_classic_logs(NfcDevice* nfc_device, LogEntry log_entries[]);

void saflok_generate_mf_classic_variable_keys(
    NfcDevice* nfc_device,
    VariableKey* variable_keys,
    size_t num_variable_keys,
    VariableKeysOptionalFunction optional_function);
size_t saflok_parse_mf_classic_variable_keys(
    NfcDevice* nfc_device,
    VariableKey* variable_keys,
    VariableKeysOptionalFunction* optional_function);
