
#include "gs1_parser_i.h"

#define LOG_TAG "gs1_rfid_parser_gs1_parser_common"

#define URI_SAFE_BASE_64_CHARACTERS \
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+_"
#define URN_40_CHARACTERS "ABCDEFGHIJKLMNOPQRSTUVWXYZ-.:0123456789"

uint64_t read_next_n_bits(const uint8_t* data_stream, uint32_t starting_bit, uint32_t num_bits) {
    FURI_LOG_T(LOG_TAG, __func__);

    if(num_bits > 64) {
        FURI_LOG_E(LOG_TAG, "Attempted to get too many bits at once: %ld", num_bits);
        return 0;
    }

    uint64_t data = 0;

    uint8_t byte_ind = starting_bit / 8;
    uint8_t bit_shift_amount = starting_bit % 8;

    for(uint8_t i = 0; i < num_bits; i++) {
        uint8_t bit = data_stream[byte_ind] & (0x80 >> bit_shift_amount);
        data = (data << 1) | (bit >> (7 - bit_shift_amount));
        bit_shift_amount++;

        if(bit_shift_amount == 8) {
            byte_ind++;
            bit_shift_amount = 0;
        }
    }

    return data;
}

void snprintf_with_padding(char* data, size_t data_length, uint64_t num, uint32_t num_length) {
    FURI_LOG_T(LOG_TAG, __func__);

    if(num_length > data_length) {
        FURI_LOG_E(
            LOG_TAG,
            "Data Length (%d) is shorter than the pad length (%ld)",
            data_length,
            num_length);
        return;
    }

    FURI_LOG_D(LOG_TAG, "Formatting %lld to length %ld", num, num_length);

    char format_str[9];
    snprintf(format_str, 9, "%%0%ldlld", num_length);
    snprintf(data, data_length, format_str, num);

    FURI_LOG_D(LOG_TAG, "Format string %s", format_str);
    FURI_LOG_D(LOG_TAG, "Formatted result %s", data);
}

int32_t parse_variable_length_alphanumeric(
    const uint8_t* data,
    uint32_t starting_bit,
    uint8_t length_bit_count,
    char* parsed_value) {
    FURI_LOG_T(LOG_TAG, __func__);

    // Note: Alpha Numeric values are stored as different formats utilizing a table to determine the way to parse the data
    uint8_t encoding = read_next_n_bits(data, starting_bit, 3);
    uint16_t value_length = read_next_n_bits(data, starting_bit + 3, length_bit_count);
    FURI_LOG_D(
        LOG_TAG,
        "Parsing Alphanumeric with %02X as the encoding and length of %d",
        encoding,
        value_length);

    switch(encoding) {
    case 0x00: // Integer
        return 3 + parse_variable_length_numeric(
                       data, starting_bit + 3, length_bit_count, parsed_value);
    case 0x01: // 4-bit numbers
    case 0x02:
        for(uint16_t i = 0; i < value_length; i++) {
            uint8_t bits = read_next_n_bits(data, starting_bit + 3 + length_bit_count + 4 * i, 4);

            if(bits < 10) {
                parsed_value[i] = '0' + bits;
            } else if(encoding == 0x01) {
                parsed_value[i] = 'A' + bits - 10;
            } else {
                parsed_value[i] = 'a' + bits - 10;
            }
        }

        parsed_value[value_length] = '\0';
        return value_length * 4 + 3 + length_bit_count;
    case 0x03: // 6-bit URI Safe Base64
        for(uint16_t i = 0; i < value_length; i++) {
            uint8_t bits = read_next_n_bits(data, starting_bit + 3 + length_bit_count + 6 * i, 6);
            parsed_value[i] = URI_SAFE_BASE_64_CHARACTERS[bits];
        }

        parsed_value[value_length] = '\0';
        return value_length * 6 + 3 + length_bit_count;
    case 0x04: // 7-bit ASCII
        for(uint16_t i = 0; i < value_length; i++) {
            parsed_value[i] =
                read_next_n_bits(data, starting_bit + 3 + length_bit_count + 7 * i, 7);
        }
        parsed_value[value_length] = '\0';
        return value_length * 7 + 3 + length_bit_count;
    case 0x05: // 16-bits per 3 characters
        for(uint16_t i = 0; i < value_length; i += 3) {
            uint16_t bits =
                read_next_n_bits(data, starting_bit + 3 + length_bit_count + 16 * i / 3, 16);
            parsed_value[i] = URN_40_CHARACTERS[(bits - 1) / 1600];
            parsed_value[i + 1] = URN_40_CHARACTERS[((bits - 1) / 40) % 40];
            parsed_value[i + 2] = URN_40_CHARACTERS[(bits - 1) % 40];
        }

        parsed_value[value_length] = '\0';
        return 16 * value_length / 3 + 3 + length_bit_count;
    default:
        return -1;
    }
}

int32_t parse_fixed_length_numeric(
    const uint8_t* data,
    uint32_t starting_bit,
    uint8_t length,
    char* parsed_value) {
    FURI_LOG_T(LOG_TAG, __func__);

    for(uint8_t i = 0; i < length; i++) {
        uint64_t next_nybble = read_next_n_bits(data, starting_bit + 4 * i, 4);
        parsed_value[i] = '0' + (next_nybble & 0xF);
    }
    parsed_value[length - 1] = '\0';

    return length * 4;
}

int32_t parse_variable_length_numeric(
    const uint8_t* data,
    uint32_t starting_bit,
    uint8_t length_bit_count,
    char* parsed_value) {
    FURI_LOG_T(LOG_TAG, __func__);

    uint8_t length = read_next_n_bits(data, starting_bit, length_bit_count);
    uint16_t num_bits =
        (length * 332 + 99) /
        100; // This is to calculate the number of bits to read as roughly 3.32 * value_length (rounded up)
    uint64_t value = read_next_n_bits(data, starting_bit + length_bit_count, num_bits);
    snprintf_with_padding(parsed_value, length + 1, value, length);
    parsed_value[length] = '\0';

    return num_bits + length_bit_count;
}

int32_t parse_country_code(const uint8_t* data, uint32_t starting_bit, char* parsed_value) {
    parsed_value[0] = URI_SAFE_BASE_64_CHARACTERS[read_next_n_bits(data, starting_bit, 6)];
    parsed_value[1] = URI_SAFE_BASE_64_CHARACTERS[read_next_n_bits(data, starting_bit + 6, 6)];
    parsed_value[2] = '\0';
    return 12;
}
