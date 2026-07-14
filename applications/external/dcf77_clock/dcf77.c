#include "dcf77.h"

#define DST_BIT     17
#define MIN_BIT     21
#define HOUR_BIT    29
#define DAY_BIT     36
#define WEEKDAY_BIT 42
#define MONTH_BIT   45
#define YEAR_BIT    50

// The signal will contain the time of when the signal ends
// Bits: 1-bit is signal, 2-bit is prepend dash for visual output and 4-bit is mark as "X" in visual output
static uint8_t dcf77_bits[] = {
    0, // 00: Start of minute (Always 0)
    2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 01: Weather broadcast / Civil warning bits
    2, // 15: Call bit: abnormal transmitter operation
    0, // 16: Summer time announcement. Set during hour before change
    0, 1, // 17: 01=CET, 10=CEST
    0, // 19: Leap second announcement. Set during hour before leap second
    1, // 20: Start of encoded time (always 1)
    2, 0, 0, 0, 0, 0, 0, 0, // 21: Minutes (7bit + even parity, 00-59)
    2, 0, 0, 0, 0, 0, 0, // 29: Hours (6bit + even parity, 0-23)
    2, 0, 0, 0, 0, 0, // 36: Day of month (6bit, 1-31)
    2, 0, 0, // 42: Day of week (3bit, 1-7, Monday=1)
    2, 0, 0, 0, 0, // 45: Month number (5bit, 1-12)
    2, 0, 0, 0, 0, 0, 0, 0, 0, // 50: Year within century (8bit + even parity for 36-58, 00-99)
    6 // 59: Minute mark
};

/**
 * Encode a value into a part of a dcf77 signal
 * @param start Index where the encoded value should start in the sinal bits array
 * @param len Number of bits in the encoded value
 * @param val The value to be encoded
 * @param par The parity flag (0 top disable parity, 1 for even parity bit to be appended to the encoded value, -1 for even parity bit to be appended to the encoded value but being based on the value currently in the first position in the signal bits (overflow from before))
 */
void dcf77_encode(int start, int len, int val, int par) {
    // Set parity init bit
    uint8_t parity = 0;
    if(par == -1) {
        // Use first bit as parity init
        parity = dcf77_bits[start] & 1;
    }

    // Parse value into bits (little endian)
    uint8_t byte = ((val / 10) << 4) + (val % 10);

    // Go through byte from right (low) to left (high)
    for(int bit = 0; bit < len; bit++) {
        // Get bit
        uint8_t dcf77_bit = (byte >> bit) & 1;

        // XOR onto parity
        parity ^= dcf77_bit;

        // Set bit in signal bits (keep flags for visual output)
        dcf77_bits[start + bit] = (dcf77_bits[start + bit] & 0x6) + dcf77_bit;
    }

    // Append parity bit if parity bit is enabled (keep flags for visual output)
    if(par != 0) {
        dcf77_bits[start + len] = (dcf77_bits[start + len] & 0x6) + (parity & 1);
    }
}

void set_dcf77_time(DateTime* datetime, bool is_dst) {
    dcf77_encode(DST_BIT, 2, is_dst > 0 ? 0b01 : 0b10, 0); // Disable parity
    dcf77_encode(MIN_BIT, 7, datetime->minute, 1);
    dcf77_encode(HOUR_BIT, 6, datetime->hour, 1);
    dcf77_encode(DAY_BIT, 6, datetime->day, 1);
    dcf77_encode(WEEKDAY_BIT, 3, datetime->weekday, -1); // Use first bit as parity init
    dcf77_encode(MONTH_BIT, 5, datetime->month, -1); // Use first bit as parity init
    dcf77_encode(YEAR_BIT, 8, datetime->year % 100, -1); // Use first bit as parity init
}

bool get_dcf77_bit(int sec) {
    // Return the bit for the second
    return dcf77_bits[sec % 60] & 1;
}

char* get_dcf77_data(int sec) {
    // Array for data to be displayed
    static char data[70];

    // Index
    int idx = 0;

    // Optimization: Only 25 charcters can be displayed -> don't calculate any more
    int start = sec > 25 ? sec - 25 : 0;
    for(int bit = start; bit <= sec; bit++) {
        // Prepend a dash if 2-bit is set
        if(dcf77_bits[bit] >> 1 & 1) {
            data[idx++] = '-';
        }

        // Only set data is not displayed as "X"
        if(dcf77_bits[bit] >> 2 & 1) {
            data[idx++] = 'X';
        } else {
            // Set data to last bit (ascii id of 0 plus 0 or 1)
            data[idx++] = '0' + (dcf77_bits[bit] & 1);
        }
    }

    // Terminate string wit null byte and return
    data[idx] = 0;
    return data;
}
