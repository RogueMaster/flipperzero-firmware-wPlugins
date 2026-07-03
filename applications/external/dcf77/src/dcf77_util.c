#include "dcf77_util.h"

static uint8_t dcf77_to_bcd(uint8_t value) {
    return (uint8_t)(((value / 10U) << 4U) | (value % 10U));
}

static bool dcf77_u8_get_bit(uint8_t value, uint8_t bit) {
    return !!(value & (1U << bit));
}

static bool dcf77_even_parity_u8(uint8_t value) {
    bool parity = false;

    for(uint8_t bit = 0; bit < 8U; bit++) {
        parity ^= dcf77_u8_get_bit(value, bit);
    }

    return parity;
}

static void dcf77_message_set_bit(uint8_t* message, uint8_t bit, bool value) {
    const uint8_t byte_index = bit / 8U;
    const uint8_t bit_index = bit % 8U;
    const uint8_t mask = (uint8_t)(1U << (7U - bit_index));

    if(value) {
        message[byte_index] |= mask;
    } else {
        message[byte_index] &= (uint8_t)~mask;
    }
}

static void dcf77_message_write_bits(
    uint8_t* message,
    const uint8_t* bit_positions,
    size_t count,
    uint8_t value) {
    for(size_t i = 0; i < count; i++) {
        dcf77_message_set_bit(message, bit_positions[i], dcf77_u8_get_bit(value, (uint8_t)i));
    }
}

bool dcf77_message_get_bit(const uint8_t* message, uint8_t bit) {
    if(bit == 59U || bit == 0U) {
        return false;
    }

    return !!(message[bit / 8U] & (1U << (7U - (bit % 8U))));
}

void set_dcf_message(
    uint8_t* dest,
    uint8_t minute,
    uint8_t hour,
    uint8_t day,
    uint8_t month,
    uint8_t year,
    uint8_t dow,
    bool dst,
    bool predst,
    bool abnormal,
    bool leap,
    uint16_t civbits) {
    static const uint8_t minute_bits[] = {21, 22, 23, 24, 25, 26, 27};
    static const uint8_t hour_bits[] = {29, 30, 31, 32, 33, 34};
    static const uint8_t day_bits[] = {36, 37, 38, 39, 40, 41};
    static const uint8_t dow_bits[] = {42, 43, 44};
    static const uint8_t month_bits[] = {45, 46, 47, 48, 49};
    static const uint8_t year_bits[] = {50, 51, 52, 53, 54, 55, 56, 57};
    const uint8_t bcd_minute = dcf77_to_bcd(minute);
    const uint8_t bcd_hour = dcf77_to_bcd(hour);
    const uint8_t bcd_day = dcf77_to_bcd(day);
    const uint8_t bcd_month = dcf77_to_bcd(month);
    const uint8_t bcd_year = dcf77_to_bcd(year);
    const uint8_t dow_bits_value = dow & 0x07U;
    const bool p_minute = dcf77_even_parity_u8(bcd_minute);
    const bool p_hour = dcf77_even_parity_u8(bcd_hour);
    const bool p_date = dcf77_even_parity_u8(bcd_day) ^ dcf77_even_parity_u8(bcd_month) ^
                        dcf77_even_parity_u8(bcd_year) ^ dcf77_even_parity_u8(dow_bits_value);

    for(uint8_t byte_index = 0; byte_index < 8U; byte_index++) {
        dest[byte_index] = 0;
    }

    for(uint8_t bit = 1; bit <= 14U; bit++) {
        dcf77_message_set_bit(dest, bit, !!(civbits & (1U << (14U - bit))));
    }

    dcf77_message_set_bit(dest, 15, abnormal);
    dcf77_message_set_bit(dest, 16, predst);
    dcf77_message_set_bit(dest, 17, dst);
    dcf77_message_set_bit(dest, 18, !dst);
    dcf77_message_set_bit(dest, 19, leap);
    dcf77_message_set_bit(dest, 20, true);
    dcf77_message_write_bits(dest, minute_bits, sizeof(minute_bits), bcd_minute);
    dcf77_message_set_bit(dest, 28, p_minute);
    dcf77_message_write_bits(dest, hour_bits, sizeof(hour_bits), bcd_hour);
    dcf77_message_set_bit(dest, 35, p_hour);
    dcf77_message_write_bits(dest, day_bits, sizeof(day_bits), bcd_day);
    dcf77_message_write_bits(dest, dow_bits, sizeof(dow_bits), dow_bits_value);
    dcf77_message_write_bits(dest, month_bits, sizeof(month_bits), bcd_month);
    dcf77_message_write_bits(dest, year_bits, sizeof(year_bits), bcd_year);
    dcf77_message_set_bit(dest, 58, p_date);
}
