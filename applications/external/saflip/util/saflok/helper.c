// KDF from: https://gitee.com/jadenwu/Saflok_KDF/blob/master/saflok.c
// KDF published and reverse engineered by Jaden Wu

// Decryption and parsing from: https://gitee.com/wangshuoyue/unsaflok
// Decryption algorithm and parsing published by Shuoyue Wang

// Encryption from: https://github.com/RfidResearchGroup/proxmark3/blob/master/client/src/cmdhfsaflok.c

// Generation written by Aaron Tulino <me@aaronjamt.com>,
//  based on: https://github.com/RfidResearchGroup/proxmark3/blob/master/client/src/cmdhfsaflok.c

#include "helper.h"
#include "../bitmanip.h"

#include <string.h>

#include "formats/mfc.h"

// Lookup table
unsigned char c_aEncode[256] = {
    236, 116, 192, 99,  86,  153, 105, 100, 159, 23,  38,  198, 240, 1,   16,  77,  202, 82,  138,
    75,  122, 175, 173, 32,  115, 162, 15,  194, 80,  120, 54,  68,  25,  30,  114, 210, 50,  183,
    107, 248, 5,   174, 199, 28,  85,  113, 89,  19,  17,  73,  250, 252, 127, 43,  52,  102, 69,
    165, 185, 21,  169, 163, 134, 150, 219, 45,  218, 208, 33,  84,  189, 227, 131, 141, 110, 155,
    83,  149, 4,   228, 42,  112, 39,  94,  35,  133, 135, 36,  209, 237, 34,  27,  214, 98,  118,
    67,  48,  193, 66,  132, 91,  253, 95,  40,  254, 58,  20,  55,  176, 184, 26,  61,  171, 72,
    251, 152, 3,   166, 119, 201, 11,  117, 97,  8,   241, 245, 217, 121, 101, 172, 229, 164, 223,
    191, 235, 10,  204, 249, 125, 195, 136, 13,  142, 232, 220, 247, 143, 156, 47,  109, 161, 65,
    9,   188, 92,  60,  57,  144, 124, 197, 46,  212, 51,  78,  206, 213, 88,  79,  200, 216, 31,
    130, 22,  62,  215, 255, 190, 146, 157, 196, 211, 14,  29,  181, 93,  24,  7,   126, 106, 243,
    37,  128, 108, 203, 70,  140, 246, 231, 242, 177, 187, 41,  145, 158, 205, 233, 148, 224, 170,
    137, 221, 234, 230, 81,  168, 71,  63,  2,   59,  87,  96,  12,  207, 238, 154, 160, 179, 123,
    225, 147, 186, 178, 182, 222, 0,   226, 167, 139, 76,  53,  74,  111, 239, 18,  129, 44,  180,
    56,  90,  244, 151, 64,  104, 6,   49,  103};

static const uint8_t c_aDecode[256] = {
    0xEA, 0x0D, 0xD9, 0x74, 0x4E, 0x28, 0xFD, 0xBA, 0x7B, 0x98, 0x87, 0x78, 0xDD, 0x8D, 0xB5,
    0x1A, 0x0E, 0x30, 0xF3, 0x2F, 0x6A, 0x3B, 0xAC, 0x09, 0xB9, 0x20, 0x6E, 0x5B, 0x2B, 0xB6,
    0x21, 0xAA, 0x17, 0x44, 0x5A, 0x54, 0x57, 0xBE, 0x0A, 0x52, 0x67, 0xC9, 0x50, 0x35, 0xF5,
    0x41, 0xA0, 0x94, 0x60, 0xFE, 0x24, 0xA2, 0x36, 0xEF, 0x1E, 0x6B, 0xF7, 0x9C, 0x69, 0xDA,
    0x9B, 0x6F, 0xAD, 0xD8, 0xFB, 0x97, 0x62, 0x5F, 0x1F, 0x38, 0xC2, 0xD7, 0x71, 0x31, 0xF0,
    0x13, 0xEE, 0x0F, 0xA3, 0xA7, 0x1C, 0xD5, 0x11, 0x4C, 0x45, 0x2C, 0x04, 0xDB, 0xA6, 0x2E,
    0xF8, 0x64, 0x9A, 0xB8, 0x53, 0x66, 0xDC, 0x7A, 0x5D, 0x03, 0x07, 0x80, 0x37, 0xFF, 0xFC,
    0x06, 0xBC, 0x26, 0xC0, 0x95, 0x4A, 0xF1, 0x51, 0x2D, 0x22, 0x18, 0x01, 0x79, 0x5E, 0x76,
    0x1D, 0x7F, 0x14, 0xE3, 0x9E, 0x8A, 0xBB, 0x34, 0xBF, 0xF4, 0xAB, 0x48, 0x63, 0x55, 0x3E,
    0x56, 0x8C, 0xD1, 0x12, 0xED, 0xC3, 0x49, 0x8E, 0x92, 0x9D, 0xCA, 0xB1, 0xE5, 0xCE, 0x4D,
    0x3F, 0xFA, 0x73, 0x05, 0xE0, 0x4B, 0x93, 0xB2, 0xCB, 0x08, 0xE1, 0x96, 0x19, 0x3D, 0x83,
    0x39, 0x75, 0xEC, 0xD6, 0x3C, 0xD0, 0x70, 0x81, 0x16, 0x29, 0x15, 0x6C, 0xC7, 0xE7, 0xE2,
    0xF6, 0xB7, 0xE8, 0x25, 0x6D, 0x3A, 0xE6, 0xC8, 0x99, 0x46, 0xB0, 0x85, 0x02, 0x61, 0x1B,
    0x8B, 0xB3, 0x9F, 0x0B, 0x2A, 0xA8, 0x77, 0x10, 0xC1, 0x88, 0xCC, 0xA4, 0xDE, 0x43, 0x58,
    0x23, 0xB4, 0xA1, 0xA5, 0x5C, 0xAE, 0xA9, 0x7E, 0x42, 0x40, 0x90, 0xD2, 0xE9, 0x84, 0xCF,
    0xE4, 0xEB, 0x47, 0x4F, 0x82, 0xD4, 0xC5, 0x8F, 0xCD, 0xD3, 0x86, 0x00, 0x59, 0xDF, 0xF2,
    0x0C, 0x7C, 0xC6, 0xBD, 0xF9, 0x7D, 0xC4, 0x91, 0x27, 0x89, 0x32, 0x72, 0x33, 0x65, 0x68,
    0xAF};

void saflok_encrypt_card(unsigned char* keyCard, int length, unsigned char* encryptedCard) {
    int b = 0;
    memcpy(encryptedCard, keyCard, length);
    for(int i = 0; i < length; i++) {
        int b2 = encryptedCard[i];
        int num2 = i;
        for(int j = 0; j < 8; j++) {
            num2 += 1;
            if(num2 >= length) {
                num2 -= length;
            }
            int b3 = encryptedCard[num2];
            int b4 = b2 & 1;
            b2 = (b2 >> 1) | (b << 7);
            b = b3 & 1;
            b3 = (b3 >> 1) | (b4 << 7);
            encryptedCard[num2] = b3;
        }
        encryptedCard[i] = b2;
    }
    if(length == 17) {
        int b2 = encryptedCard[10];
        b2 |= b;
        encryptedCard[10] = b2;
    }
    for(int i = 0; i < length; i++) {
        int j = encryptedCard[i] + (i + 1);
        if(j > 255) {
            j -= 256;
        }
        encryptedCard[i] = c_aEncode[j];
    }
}

void saflok_decrypt_card(
    uint8_t strCard[BASIC_ACCESS_BYTE_NUM],
    int length,
    uint8_t decryptedCard[BASIC_ACCESS_BYTE_NUM]) {
    int i, num, num2, num3, num4, b = 0, b2 = 0;
    for(i = 0; i < length; i++) {
        num = c_aDecode[strCard[i]] - (i + 1);
        if(num < 0) num += 256;
        decryptedCard[i] = num;
    }

    if(length == 17) {
        b = decryptedCard[10];
        b2 = b & 1;
    }

    for(num2 = length; num2 > 0; num2--) {
        b = decryptedCard[num2 - 1];
        for(num3 = 8; num3 > 0; num3--) {
            num4 = num2 + num3;
            if(num4 > length) num4 -= length;
            int b3 = decryptedCard[num4 - 1];
            int b4 = (b3 & 0x80) >> 7;
            b3 = ((b3 << 1) & 0xFF) | b2;
            b2 = (b & 0x80) >> 7;
            b = ((b << 1) & 0xFF) | b4;
            decryptedCard[num4 - 1] = b3;
        }
        decryptedCard[num2 - 1] = b;
    }
}

uint8_t saflok_calculate_checksum(uint8_t data[BASIC_ACCESS_BYTE_NUM]) {
    int sum = 0;
    for(int i = 0; i < BASIC_ACCESS_BYTE_NUM - 1; i++) {
        sum += data[i];
    }
    sum = 255 - (sum & 0xFF);
    return sum & 0xFF;
}

// Generates the 17-byte data buffer
void saflok_generate_data(BasicAccessData* data, uint8_t* buffer) {
    uint8_t basicAccess[BASIC_ACCESS_BYTE_NUM];
    memset(basicAccess, 0, BASIC_ACCESS_BYTE_NUM);

    insert_bits(basicAccess, 0, 4, data->card_level);
    insert_bits(basicAccess, 4, 4, data->card_type);
    insert_bits(basicAccess, 8, 8, data->card_id);
    insert_bits(basicAccess, 16, 1, data->opening_key);
    insert_bits(basicAccess, 17, 15, data->lock_id);
    insert_bits(basicAccess, 32, 12, data->pass_number);
    insert_bits(basicAccess, 44, 12, data->sequence_and_combination);
    insert_bits(basicAccess, 56, 1, data->deadbolt_override);
    insert_bits(basicAccess, 57, 7, data->restricted_days);
    insert_bits(basicAccess, 116, 12, data->property_id);

    // Break creation date/time down and shove the bits in the right spots
    uint16_t creation_year = data->creation.year - SAFLOK_YEAR_OFFSET;
    basicAccess[14] |= creation_year & 0x70;
    basicAccess[11] = (creation_year << 4) & 0xF0;
    basicAccess[11] |= data->creation.month & 0x0F;

    basicAccess[12] = (data->creation.day << 3) & 0xF8;

    basicAccess[12] |= (data->creation.hour >> 2) & 0x07;
    basicAccess[13] = (data->creation.hour << 6) & 0xC0;

    basicAccess[13] |= data->creation.minute & 0x3F;

    // Expiration date is stored as a duration after creation
    // Expiration time is stored as a time-of-day as-is
    uint16_t expire_year = data->expire.year - data->creation.year;
    int8_t expire_month = data->expire.month - data->creation.month;
    int8_t expire_day = data->expire.day - data->creation.day;

    if(expire_month < 0) {
        expire_month += 12;
        expire_year -= 1;
    }

    // Handle day rollover
    // The 0th month is December, to make wrapping around easier
    static const uint8_t days_in_month[] = {31, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    while(true) {
        uint16_t year = expire_year + data->creation.year;
        uint8_t month = expire_month + data->creation.month;
        if(month > 12) month -= 12;

        // minus 1 to get number of days in prior month
        uint8_t max_days = days_in_month[month - 1];
        // Adjust for leap years
        if(month == 2 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))) {
            max_days = 29;
        }
        if(expire_day >= 0) {
            break;
        }

        expire_day += max_days;
        expire_month--;
        if(expire_month < 0) {
            expire_month += 12;
            expire_year--;
        }
    }
    basicAccess[8] = (expire_year << 4) & 0xF0;
    basicAccess[8] |= expire_month & 0x0F;

    basicAccess[9] = (expire_day << 3) & 0xF8;

    basicAccess[9] |= (data->expire.hour >> 2) & 0x07;
    basicAccess[10] = (data->expire.hour & 0x03) << 6;

    basicAccess[10] |= data->expire.minute & 0x3F;

    // Add checksum and encrypt
    basicAccess[16] = saflok_calculate_checksum(basicAccess);
    saflok_encrypt_card(basicAccess, BASIC_ACCESS_BYTE_NUM, buffer);
}

void saflok_parse_basic_access(uint8_t basicAccess[BASIC_ACCESS_BYTE_NUM], BasicAccessData* data) {
    data->card_level = extract_bits(basicAccess, 0, 4);
    data->card_type = extract_bits(basicAccess, 4, 4);
    data->card_id = extract_bits(basicAccess, 8, 8);
    data->opening_key = extract_bits(basicAccess, 16, 1);
    data->lock_id = extract_bits(basicAccess, 17, 15);
    data->pass_number = extract_bits(basicAccess, 32, 12);
    data->sequence_and_combination = extract_bits(basicAccess, 44, 12);
    data->deadbolt_override = extract_bits(basicAccess, 56, 1);
    data->restricted_days = extract_bits(basicAccess, 57, 7);
    data->property_id = extract_bits(basicAccess, 116, 12);

    uint16_t interval_year = (basicAccess[8] >> 4);
    uint8_t interval_month = basicAccess[8] & 0x0F;
    uint8_t interval_day = (basicAccess[9] >> 3) & 0x1F;
    uint8_t interval_hour = ((basicAccess[9] & 0x07) << 2) | (basicAccess[10] >> 6);
    uint8_t interval_minute = basicAccess[10] & 0x3F;

    // There is an extra bit (the MSB of basicAccess[14]) that is unknown,
    // but is definitely not used for the creation date.
    uint8_t creation_year_bits = (basicAccess[14] & 0x70);
    data->creation.year = (creation_year_bits | ((basicAccess[11] & 0xF0) >> 4)) + 1980;
    data->creation.month = basicAccess[11] & 0x0F;
    data->creation.day = (basicAccess[12] >> 3) & 0x1F;
    data->creation.hour = ((basicAccess[12] & 0x07) << 2) | (basicAccess[13] >> 6);
    data->creation.minute = basicAccess[13] & 0x3F;

    data->expire.year = data->creation.year + interval_year;
    data->expire.month = data->creation.month + interval_month;
    data->expire.day = data->creation.day + interval_day;
    data->expire.hour = interval_hour;
    data->expire.minute = interval_minute;
}

bool saflok_decode_log_entry(const uint8_t* data, LogEntry* entry) {
    // Validate checksum first
    uint8_t checksum = 0;
    for(uint8_t i = 0; i < 7; i++)
        checksum += data[i];
    if(extract_bits(data, 56, 8) != checksum) {
        return false;
    }

    entry->let_open = extract_bits(data, 0, 1);
    entry->time.year = SAFLOK_YEAR_OFFSET + extract_bits(data, 1, 7);
    entry->deadbolt = extract_bits(data, 8, 1);
    entry->time_is_set = extract_bits(data, 9, 1);
    entry->is_dst = extract_bits(data, 10, 1);
    entry->low_battery = extract_bits(data, 11, 1);
    entry->time.month = extract_bits(data, 12, 4);
    entry->time.day = extract_bits(data, 16, 5);
    entry->time.hour = extract_bits(data, 21, 5);
    entry->time.minute = extract_bits(data, 26, 6);
    entry->lock_problem = extract_bits(data, 32, 1);
    entry->new_key = extract_bits(data, 33, 1);
    entry->lock_latched = extract_bits(data, 34, 1);
    entry->lock_id = extract_bits(data, 35, 13);
    entry->diagnostic_code = extract_bits(data, 48, 8);

    return true;
}

void saflok_encode_log_entry(const LogEntry entry, uint8_t* data) {
    memset(data, 0, SAFLOK_LOG_SIZE);

    insert_bits(data, 0, 1, entry.let_open);
    insert_bits(data, 1, 7, entry.time.year - SAFLOK_YEAR_OFFSET);
    insert_bits(data, 8, 1, entry.deadbolt);
    insert_bits(data, 9, 1, entry.time_is_set);
    insert_bits(data, 10, 1, entry.is_dst);
    insert_bits(data, 11, 1, entry.low_battery);
    insert_bits(data, 12, 4, entry.time.month);
    insert_bits(data, 16, 5, entry.time.day);
    insert_bits(data, 21, 5, entry.time.hour);
    insert_bits(data, 26, 6, entry.time.minute);
    insert_bits(data, 32, 1, entry.lock_problem);
    insert_bits(data, 33, 1, entry.new_key);
    insert_bits(data, 34, 1, entry.lock_latched);
    insert_bits(data, 35, 13, entry.lock_id);
    insert_bits(data, 48, 8, entry.diagnostic_code);

    // Calculate and add checksum
    uint8_t checksum = 0;
    for(uint8_t i = 0; i < 7; i++)
        checksum += data[i];
    insert_bits(data, 56, 8, checksum);
}
