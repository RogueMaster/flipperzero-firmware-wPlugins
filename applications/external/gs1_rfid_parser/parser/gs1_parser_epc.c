
#include "gs1_parser_i.h"

#define LOG_TAG "gs1_rfid_parser_gs1_parser_epc"

// See section 14.2 of the GS1 EPC spec for details on the headers
// See section 14.6.1 of the GS1 EPC spec for details on the contents
#define SGTIN_96_HEADER    0x30
#define SGTIN_198_HEADER   0x36
#define SGTIN_PLUS_HEADER  0xF7
#define DSGTIN_PLUS_HEADER 0xFB

#define COMPANY_PREFIX_MAX_LENGTH 13
#define ITEM_REFERENCE_MAX_LENGTH 13

// Store the list of the company length in bits since the serial is the remaining bits, and the string lengths are easier to calculate.
// See table 14-11 of the GS1 EPC spec for details on this list
static const uint8_t GTIN_PARTITION_TABLE_COMPANY_PREFIX_LENGTH[7] = {40, 37, 34, 30, 27, 24, 20};

// Layout (num bits):
//      EPC Header (8)
//      Filter (3)
//      GTIN (47)
//      Serial Number (Remaining)
static ParsingResult
    parse_gtin_normal(const UhfEpcTagData* epc_data, ParsedTagInformation* tag_info) {
    FURI_LOG_T(LOG_TAG, __func__);

    uint8_t min_byte_count = (epc_data->epc_data[0] == SGTIN_96_HEADER) ? (96 / 8) : (198 / 8);

    if(epc_data->epc_size < min_byte_count) {
        FURI_LOG_E(
            LOG_TAG,
            "Too few bytes in EPC. Expected at least %d bytes, but got %d bytes.",
            min_byte_count,
            epc_data->epc_size);
        return PARSING_MALFORMED_DATA;
    }

    uint8_t partition = (epc_data->epc_data[1] & 0x1C) >> 2;
    uint8_t company_prefix_length = GTIN_PARTITION_TABLE_COMPANY_PREFIX_LENGTH[partition];
    FURI_LOG_I(LOG_TAG, "GTIN Partition Value: %d", partition);

    uint8_t bit_index = 14;

    char company_prefix_str[COMPANY_PREFIX_MAX_LENGTH];
    uint64_t company_prefix =
        read_next_n_bits(epc_data->epc_data, bit_index, company_prefix_length);
    snprintf_with_padding(
        company_prefix_str, COMPANY_PREFIX_MAX_LENGTH, company_prefix, 12 - partition);
    company_prefix_str[COMPANY_PREFIX_MAX_LENGTH - 1] = '\0';
    FURI_LOG_I(LOG_TAG, "Company Prefix: %lld", company_prefix);

    char item_reference_str[ITEM_REFERENCE_MAX_LENGTH];
    uint64_t item_reference = read_next_n_bits(
        epc_data->epc_data, bit_index + company_prefix_length, 44 - company_prefix_length);
    snprintf_with_padding(
        item_reference_str, ITEM_REFERENCE_MAX_LENGTH, item_reference, 1 + partition);
    item_reference_str[ITEM_REFERENCE_MAX_LENGTH - 1] = '\0';
    FURI_LOG_I(LOG_TAG, "Item Reference: %lld", item_reference);

    uint8_t checksum = (item_reference_str[0] - '0') * 3;
    uint8_t gtin_index = 1;

    tag_info->ai_list[0].ai_value[0] = item_reference_str[0];

    for(uint8_t i = 0; i < 12 - partition; i++) {
        tag_info->ai_list[0].ai_value[gtin_index] = company_prefix_str[i];
        checksum = (checksum + ((company_prefix_str[i] - '0') * (gtin_index & 0x01 ? 1 : 3))) % 10;
        gtin_index++;
    }

    for(uint8_t i = 0; i < partition; i++) {
        tag_info->ai_list[0].ai_value[gtin_index] = item_reference_str[i + 1];
        checksum =
            (checksum + ((item_reference_str[i + 1] - '0') * (gtin_index & 0x01 ? 1 : 3))) % 10;
        gtin_index++;
    }

    checksum = (10 - checksum) % 10;
    tag_info->ai_list[0].ai_value[GTIN_LENGTH - 2] = '0' + checksum;
    tag_info->ai_list[0].ai_value[GTIN_LENGTH - 1] = '\0';
    tag_info->ai_list[0].ai_identifier = 0x01;

    FURI_LOG_I(LOG_TAG, "GTIN: %s", tag_info->ai_list[0].ai_value);

    bit_index += 44;

    if(epc_data->epc_data[0] == SGTIN_96_HEADER) {
        tag_info->type = "SGTIN-96";

        uint64_t serial = read_next_n_bits(epc_data->epc_data, bit_index, 38);
        snprintf_with_padding(tag_info->ai_list[1].ai_value, SERIAL_LENGTH, serial, 12);
        FURI_LOG_I(LOG_TAG, "Serial: %lld", serial);
    } else {
        tag_info->type = "SGTIN-198";

        for(uint8_t i = 0; i < SERIAL_LENGTH; i++) {
            tag_info->ai_list[1].ai_value[i] = read_next_n_bits(epc_data->epc_data, bit_index, 7);

            if(tag_info->ai_list[1].ai_value[i] == 0) {
                break;
            }

            bit_index += 7;
        }
        FURI_LOG_I(LOG_TAG, "Serial: %s", tag_info->ai_list[1].ai_value);
    }

    tag_info->ai_list[1].ai_value[SERIAL_LENGTH - 1] = '\0';
    tag_info->ai_list[1].ai_identifier = 0x21;
    tag_info->ai_count = 2;

    return PARSING_SUCCESS;
}

// Layout (num bits):
//      EPC Header (8)
//      AI Data Toggle (1)
//      Filter (3)
//      Data (present if DSGTIN+)
//      GTIN (56)
//      Serial Number Encoding (3)
//      Serial Number Length (5)
//      Serial Number (up to 140)
//      AI List (remaining is AI data toggle)
static ParsingResult
    parse_gtin_plus(const UhfEpcTagData* epc_data, ParsedTagInformation* tag_info) {
    FURI_LOG_T(LOG_TAG, __func__);

    uint8_t gtin_offset = (epc_data->epc_data[0] == SGTIN_PLUS_HEADER) ? 12 : 32;
    uint8_t serial_offset = (epc_data->epc_data[0] == SGTIN_PLUS_HEADER) ? 68 : 88;

    if(epc_data->epc_data[0] == DSGTIN_PLUS_HEADER) {
        tag_info->type = "DSGTIN+";

        switch(read_next_n_bits(epc_data->epc_data, 12, 4)) {
        case 0x00:
            tag_info->ai_list[2].ai_identifier = 0x11;
            break;
        case 0x01:
            tag_info->ai_list[2].ai_identifier = 0x13;
            break;
        case 0x02:
            tag_info->ai_list[2].ai_identifier = 0x15;
            break;
        case 0x03:
            tag_info->ai_list[2].ai_identifier = 0x16;
            break;
        case 0x04:
            tag_info->ai_list[2].ai_identifier = 0x17;
            break;
        case 0x05:
            tag_info->ai_list[2].ai_identifier = 0x7006;
            break;
        case 0x06:
            tag_info->ai_list[2].ai_identifier = 0x7007;
            break;
        default:
            return PARSING_MALFORMED_DATA;
        }

        snprintf(
            tag_info->ai_list[2].ai_value,
            DATE_LENGTH,
            "%02lld/%02lld/%02lld",
            read_next_n_bits(epc_data->epc_data, 27, 5), // Day
            read_next_n_bits(epc_data->epc_data, 23, 4), // Month
            read_next_n_bits(epc_data->epc_data, 16, 7) // Year
        );

        tag_info->ai_list[2].ai_value[DATE_LENGTH - 1] = '\0';
        tag_info->ai_count = 3;
    } else {
        tag_info->type = "SGTIN+";
        tag_info->ai_count = 2;
    }

    tag_info->ai_list[0].ai_identifier = 0x01;
    parse_fixed_length_numeric(
        epc_data->epc_data, gtin_offset, GTIN_LENGTH, tag_info->ai_list[0].ai_value);

    FURI_LOG_I(LOG_TAG, "GTIN: %s", tag_info->ai_list[0].ai_value);

    int8_t serial_bit_count = parse_variable_length_alphanumeric(
        epc_data->epc_data, serial_offset, 5, tag_info->ai_list[1].ai_value);

    if(serial_bit_count < 0) {
        return PARSING_MALFORMED_DATA;
    }

    tag_info->ai_list[1].ai_identifier = 0x21;
    FURI_LOG_I(LOG_TAG, "Serial: %s", tag_info->ai_list[1].ai_value);

    if(epc_data->epc_data[1] & 0x80) {
        return parse_epc_ais(
            tag_info, epc_data->epc_data, serial_offset + serial_bit_count, epc_data->epc_size * 8);
    } else {
        return PARSING_SUCCESS;
    }
}

// See https://www.gs1.org/services/epc-encoderdecoder for the online decoder
ParsingResult parse_epc(const UhfEpcTagData* epc_data, ParsedTagInformation* tag_info) {
    FURI_LOG_T(LOG_TAG, __func__);

    if(!epc_data->is_valid) {
        FURI_LOG_E(LOG_TAG, "EPC data is not valid");
        return PARSING_MALFORMED_DATA;
    }

    if(epc_data->epc_size < 1) {
        FURI_LOG_E(LOG_TAG, "EPC data is empty");
        return PARSING_MALFORMED_DATA;
    }

    tag_info->ai_count = 0;

    switch(epc_data->epc_data[0]) {
    case SGTIN_96_HEADER:
    case SGTIN_198_HEADER:
        return parse_gtin_normal(epc_data, tag_info);
    case SGTIN_PLUS_HEADER:
    case DSGTIN_PLUS_HEADER:
        return parse_gtin_plus(epc_data, tag_info);
    default:
        FURI_LOG_E(LOG_TAG, "Not a valid SGTIN EPC header: %02X", epc_data->epc_data[0]);
        return PARSING_NOT_GTIN;
    }

    FURI_LOG_E(LOG_TAG, "EPC Parsing Reached an invalid point");
    return PARSING_MALFORMED_DATA;
}
