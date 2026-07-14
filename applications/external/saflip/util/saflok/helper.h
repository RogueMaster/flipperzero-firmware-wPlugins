#pragma once

#include <nfc/nfc_device.h>
#include <lib/bit_lib/bit_lib.h>

#include "saflok.h"

#define SAFLOK_YEAR_OFFSET 1980
#define SAFLOK_LOG_SIZE    8

void saflok_encrypt_card(unsigned char* keyCard, int length, unsigned char* encryptedCard);
void saflok_decrypt_card(
    uint8_t strCard[BASIC_ACCESS_BYTE_NUM],
    int length,
    uint8_t decryptedCard[BASIC_ACCESS_BYTE_NUM]);

uint8_t saflok_calculate_checksum(uint8_t data[BASIC_ACCESS_BYTE_NUM]);

void saflok_generate_data(BasicAccessData* data, uint8_t* buffer);
void saflok_parse_basic_access(uint8_t basicAccess[BASIC_ACCESS_BYTE_NUM], BasicAccessData* data);

bool saflok_decode_log_entry(const uint8_t* data, LogEntry* entry);
void saflok_encode_log_entry(const LogEntry entry, uint8_t* data);
