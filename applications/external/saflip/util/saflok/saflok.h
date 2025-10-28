#pragma once

#include <stdint.h>
#include <nfc/nfc_device.h>
#include <datetime/datetime.h>

#define BASIC_ACCESS_BYTE_NUM 17

typedef struct {
    enum {
        SaflipFormatMifareClassic,

        SaflipFormatTotalNumFormats
    } format;

    uint8_t card_level;
    uint8_t card_type;
    uint8_t card_id;
    bool opening_key;
    uint16_t lock_id;
    uint16_t pass_number;
    uint16_t sequence_and_combination;
    bool deadbolt_override;
    uint8_t restricted_days;
    uint16_t property_id;

    DateTime creation;
    DateTime expire;
} BasicAccessData;

typedef struct {
    DateTime time;
    bool deadbolt;
    bool time_is_set;
    bool lock_problem;
    bool lock_latched;
    bool low_battery;
    bool is_dst;
    bool let_open;
    bool new_key;
    uint16_t lock_id;
    uint8_t diagnostic_code;
} LogEntry;

typedef struct {
    uint16_t lock_id;
    DateTime creation;
    bool inhibit;
    bool use_optional;
} VariableKey;

typedef enum {
    VariableKeysOptionalFunctionNone,
    VariableKeysOptionalFunctionLevelInhibit,
    VariableKeysOptionalFunctionElectLockUnlock,
    VariableKeysOptionalFunctionLatchUnlatch,

    VariableKeysOptionalFunctionNum,
} VariableKeysOptionalFunction;

char* saflok_log_entry_description(LogEntry entry);

bool saflok_parse(
    NfcDevice* nfc_device,
    uint8_t* uid,
    size_t* uid_len,
    BasicAccessData* saflok_data);
bool saflok_generate(
    NfcDevice* nfc_device,
    uint8_t* uid,
    size_t uid_len,
    BasicAccessData* saflok_data);
