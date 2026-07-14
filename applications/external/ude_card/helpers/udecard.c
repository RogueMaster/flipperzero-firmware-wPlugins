/*
This file is part of UDECard App.
A Flipper Zero application to analyse student ID cards from the University of Duisburg-Essen (Intercard)

Copyright (C) 2025-2026 Alexander Hahn

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "udecard.h"

#include "../udecard_app_i.h"

void UDECard_demo_fill(UDECard* udecard) {
    FURI_LOG_I("UDECARD", "Filling demo card data.");
    udecard->parsing_result = UDECardParsingResultSuccess;
    udecard->loading_result = UDECardLoadingResultSuccess;
    udecard->carddata = NULL;
    strncpy(udecard->ksnr, "75837882", UDECARD_KSNR_SIZE_MAX_LENGTH); // ASCII "KSNR"
    udecard->member_type = UDECardMemberTypeStudent;
    strncpy(udecard->member_number, "68697779", UDECARD_MEMBER_NUMBER_SIZE); // ASCII "DEMO"
    udecard->balance = (uint16_t)1512;
    udecard->transaction_count = 142;
}

UDECard* udecard_alloc() {
    UDECard* udecard = malloc(sizeof(UDECard));

    udecard->parsing_result = ~0;
    udecard->loading_result = ~0;

    udecard->carddata = mf_classic_alloc();
    memset(udecard->uid, 0, sizeof(udecard->uid));

    memset(udecard->ksnr, 0, sizeof(udecard->ksnr));
    udecard->member_type = UDECardMemberTypeUnknown;
    memset(udecard->member_number, 0, sizeof(udecard->member_number));

    udecard->balance = 0;
    udecard->transaction_count = 0;

    return udecard;
}

void udecard_free(UDECard* udecard) {
    mf_classic_free(udecard->carddata);

    free(udecard);
}

bool udecard_parse_magic(UDECard* udecard) {
    MfClassicData* mfclassicdata = udecard->carddata;

    /* check for magic bytes */
    MfClassicBlock constant_block1 = mfclassicdata->block[UDECARD_CONSTANT_BLOCK1];
    MfClassicBlock constant_block2 = mfclassicdata->block[UDECARD_CONSTANT_BLOCK2];
    if(memcmp(
           constant_block1.data, UDECARD_CONSTANT_BLOCK1_CONTENTS, UDECARD_CONSTANT_BLOCK1_SIZE) !=
           0 ||
       memcmp(
           constant_block2.data, UDECARD_CONSTANT_BLOCK2_CONTENTS, UDECARD_CONSTANT_BLOCK2_SIZE) !=
           0) {
        // if we are here, this card is probably not a UDECard
        return false;
    }
    return true;
}

bool udecard_parse_ksnr(UDECard* udecard) {
    MfClassicData* mfclassicdata = udecard->carddata;

    MfClassicBlock uid_block = mfclassicdata->block[UDECARD_UID_BLOCK];
    uint8_t uid[UDECARD_UID_SIZE];
    memcpy(uid, uid_block.data, UDECARD_UID_SIZE);
    uid_to_ksnr(udecard->ksnr, uid);

    remove_leading_zeros_from_string(udecard->ksnr);

    return true;
}

bool udecard_parse_member_type(UDECard* udecard) {
    MfClassicData* mfclassicdata = udecard->carddata;

    MfClassicBlock member_type_block = mfclassicdata->block[UDECARD_MEMBERTYPE_BLOCK];
    uint8_t member_type = member_type_block.data[UDECARD_MEMBERTYPE_BYTE_IN_BLOCK_1OCC];

    switch(member_type) {
    case UDECARD_MEMBER_TYPE_STUDENT_BYTE:
        udecard->member_type = UDECardMemberTypeStudent;
        break;
    case UDECARD_MEMBER_TYPE_EMPLOYEE_BYTE:
        udecard->member_type = UDECardMemberTypeEmployee;
        break;
    default:
        udecard->member_type = UDECardMemberTypeUnknown;
        return false;
    }

    return true;
}

bool udecard_parse_balance(UDECard* udecard) {
    MfClassicData* mfclassicdata = udecard->carddata;

    uint8_t* balance_data_1 =
        mfclassicdata->block[UDECARD_BALANCE_BLOCK_1OCC].data + UDECARD_BALANCE_BYTE_1OCC;
    uint8_t* balance_data_2 =
        mfclassicdata->block[UDECARD_BALANCE_BLOCK_2OCC].data + UDECARD_BALANCE_BYTE_2OCC;
    int balance_1 = xor3_to_int(balance_data_1);
    int balance_2 = xor3_to_int(balance_data_2);
    if(balance_1 != balance_2 || balance_1 == -1) {
        udecard->balance = 0;
        return false;
    } else
        udecard->balance = balance_1;

    return true;
}

bool udecard_parse_transaction_count(UDECard* udecard) {
    MfClassicData* mfclassicdata = udecard->carddata;

    uint8_t* transaction_count_data_1 =
        mfclassicdata->block[UDECARD_TRANSCOUNT_BLOCK_1OCC].data + UDECARD_TRANSCOUNT_BYTE_1OCC;
    uint8_t* transaction_count_data_2 =
        mfclassicdata->block[UDECARD_TRANSCOUNT_BLOCK_2OCC].data + UDECARD_TRANSCOUNT_BYTE_2OCC;
    int transaction_count_1 = xor3_to_int(transaction_count_data_1);
    int transaction_count_2 = xor3_to_int(transaction_count_data_2);
    if(transaction_count_1 != transaction_count_2 || transaction_count_1 == -1) {
        udecard->transaction_count = 0;
        return false;
    } else
        udecard->transaction_count = transaction_count_1;

    return true;
}

bool udecard_parse_member_number(UDECard* udecard) {
    MfClassicData* mfclassicdata = udecard->carddata;

    char* snumber_data_1 = (char*)mfclassicdata->block[UDECARD_MEMBER_NUMBER_BLOCK_1OCC].data + 1;
    char* snumber_data_2 = (char*)mfclassicdata->block[UDECARD_MEMBER_NUMBER_BLOCK_2OCC].data + 1;
    char* snumber_data_3 = (char*)mfclassicdata->block[UDECARD_MEMBER_NUMBER_BLOCK_3OCC].data + 1;
    char* snumber_data_4 = (char*)mfclassicdata->block[UDECARD_MEMBER_NUMBER_BLOCK_4OCC].data + 1;

    strncpy(udecard->member_number, snumber_data_1, UDECARD_MEMBER_NUMBER_SIZE);
    reverse_string(udecard->member_number);
    remove_leading_zeros_from_string(udecard->member_number);

    // check whether there is a mismatch...
    if(!(strncmp(snumber_data_1, snumber_data_2, UDECARD_MEMBER_NUMBER_SIZE) == 0 &&
         strncmp(snumber_data_1, snumber_data_3, UDECARD_MEMBER_NUMBER_SIZE) == 0 &&
         strncmp(snumber_data_1, snumber_data_4, UDECARD_MEMBER_NUMBER_SIZE) == 0)) {
        return false;
    }

    return true;
}

UDECardParsingResult udecard_parse(UDECard* udecard, MfClassicData* mfclassicdata) {
    udecard->carddata = mfclassicdata;

    udecard->parsing_result = UDECardParsingResultSuccess;

    bool (*parse_functions[])(UDECard*) = {
        udecard_parse_magic,
        udecard_parse_ksnr,
        udecard_parse_member_type,
        udecard_parse_balance,
        udecard_parse_member_number,
        udecard_parse_transaction_count,
    };

    for(unsigned int i = 0; i < sizeof(parse_functions) / sizeof(parse_functions[0]); i++) {
        if(!parse_functions[i](udecard)) {
            udecard->parsing_result |= (1 << i);
        }
    }

    // UDECard_demo_fill(udecard);

    return udecard->parsing_result;
}

void uid_to_ksnr(char* ksnr, uint8_t* uid) {
    // uid is 4 bytes, ksnr_int can be up to 2^32 = 4294967296
    unsigned int ksnr_int = 0;
    for(int i = 3; i >= 0; i--) {
        ksnr_int += (unsigned int)uid[i] << (8 * i);
    }
    snprintf(
        ksnr, UDECARD_KSNR_SIZE_MAX_LENGTH + 1, "%0*u", UDECARD_KSNR_SIZE_MAX_LENGTH, ksnr_int);
}

int xor3_to_int(const uint8_t* bytes) {
    int balance = bytes[0] << 8;
    balance += bytes[1];

    uint8_t check = bytes[0] ^ bytes[1];

    if(check != bytes[2]) {
        return -1;
    }

    return balance;
}

UDECardLoadingResult udecard_load_from_nfc_device(UDECard* udecard, NfcDevice* nfc_device) {
    if(!nfc_device) return udecard->loading_result = UDECardLoadingResultErrorNotNFC;

    FURI_LOG_I(
        "UDECARD", "Loading from NFC device, protocol: %d", nfc_device_get_protocol(nfc_device));

    if(nfc_device_get_protocol(nfc_device) != NfcProtocolMfClassic) {
        return udecard->loading_result = UDECardLoadingResultErrorNotMfClassic;
    }

    const MfClassicData* c_mfclassicdata = nfc_device_get_data(nfc_device, NfcProtocolMfClassic);
    mf_classic_copy(udecard->carddata, c_mfclassicdata);

    return udecard->loading_result = UDECardLoadingResultSuccess;
}

UDECardLoadingResult udecard_load_from_path(UDECard* udecard, FuriString* path) {
    NfcDevice* nfc_device = nfc_device_alloc();
    if(!nfc_device_load(nfc_device, furi_string_get_cstr(path)))
        return udecard->loading_result = UDECardLoadingResultErrorNotNFC;

    if((udecard->loading_result = udecard_load_from_nfc_device(udecard, nfc_device)) !=
       UDECardLoadingResultSuccess) {
        nfc_device_free(nfc_device);
        return udecard->loading_result;
    }

    nfc_device_free(nfc_device);

    return udecard->loading_result;
}

char* udecard_loading_error_string(UDECardLoadingResult loading_result) {
    switch(loading_result) {
    case UDECardLoadingResultErrorNotNFC:
        return "Not an NFC file.";
    case UDECardLoadingResultErrorNotMfClassic:
        return "Not a MifareClassic card.";
    default:
        return "Unknown error.";
    }
}

bool udecard_gather_keys(uint8_t sector_keys[][6]) {
    if(!keys_dict_check_presence(EXT_PATH(FLIPPER_MFCLASSIC_DICT_PATH))) {
        FURI_LOG_E(
            "UDECARD", "Keys dictionary not found at %s", EXT_PATH(FLIPPER_MFCLASSIC_DICT_PATH));
        return false;
    }
    KeysDict* keys_dict =
        keys_dict_alloc(EXT_PATH(FLIPPER_MFCLASSIC_DICT_PATH), KeysDictModeOpenExisting, 6);
    if(!keys_dict) {
        FURI_LOG_E(
            "UDECARD",
            "Failed to open keys dictionary at %s",
            EXT_PATH(FLIPPER_MFCLASSIC_DICT_PATH));
        return false;
    }
    if(FLIPPER_MFCLASSIC_DICT_TOTAL_KEYS != keys_dict_get_total_keys(keys_dict)) {
        FURI_LOG_I(
            "UDECARD",
            "Keys dictionary at %s has wrong number of keys: %d, expected: %d",
            EXT_PATH(FLIPPER_MFCLASSIC_DICT_PATH),
            keys_dict_get_total_keys(keys_dict),
            FLIPPER_MFCLASSIC_DICT_TOTAL_KEYS);
        // only throw an error if it is LESS
        if(keys_dict_get_total_keys(keys_dict) < FLIPPER_MFCLASSIC_DICT_TOTAL_KEYS) {
            FURI_LOG_E("UDECARD", "Keys dictionary is too small!");
            keys_dict_free(keys_dict);
            return false;
        }
    }

    int udecard_keys_indices[] = {
        UDECARD_KEYA_0_INDEX,
        UDECARD_KEYA_1_INDEX,
        UDECARD_KEYA_2_INDEX,
        UDECARD_KEYA_3_INDEX,
        UDECARD_KEYA_4_INDEX,
        UDECARD_KEYA_5_INDEX,
    };
    int udecard_keys_total = sizeof(udecard_keys_indices) / sizeof(udecard_keys_indices[0]);
    uint8_t gathered_keys[udecard_keys_total][16];

    uint8_t curkey[6] = {0};
    int found = 0;
    for(int i = 1;
        keys_dict_get_next_key(keys_dict, curkey, sizeof(curkey)) && found < udecard_keys_total;
        i++) {
        if(udecard_keys_indices[found] == i) {
            memcpy(gathered_keys[found], curkey, sizeof(curkey));
            found++;
        }
    }

    for(int i = 0; i < udecard_keys_total; i++) {
        FURI_LOG_I(
            "UDECARD",
            "Key %d: %02X %02X %02X %02X %02X %02X",
            i,
            gathered_keys[i][0],
            gathered_keys[i][1],
            gathered_keys[i][2],
            gathered_keys[i][3],
            gathered_keys[i][4],
            gathered_keys[i][5]);
    }

    memcpy(sector_keys[0], gathered_keys[0], sizeof(gathered_keys[0]));
    memcpy(sector_keys[1], gathered_keys[5], sizeof(gathered_keys[5]));
    memcpy(sector_keys[2], gathered_keys[5], sizeof(gathered_keys[5]));
    memcpy(sector_keys[3], gathered_keys[5], sizeof(gathered_keys[5]));
    // 4
    memcpy(sector_keys[5], gathered_keys[1], sizeof(gathered_keys[1]));
    memcpy(sector_keys[6], gathered_keys[2], sizeof(gathered_keys[2]));
    // 7
    memcpy(sector_keys[8], gathered_keys[3], sizeof(gathered_keys[3]));
    memcpy(sector_keys[9], gathered_keys[4], sizeof(gathered_keys[4]));
    // 10...15

    keys_dict_free(keys_dict);

    return true;
}
