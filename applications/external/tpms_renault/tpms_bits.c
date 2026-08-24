#include "tpms_bits.h"

uint8_t tpms_bit_at(const uint8_t* bits, uint16_t pos) {
    return (uint8_t)((bits[pos >> 3] >> (7 - (pos & 7))) & 1);
}

void tpms_bit_append(uint8_t* out, uint16_t* nbits, uint8_t bit) {
    if(bit) out[*nbits >> 3] |= (uint8_t)(0x80 >> (*nbits & 7));
    (*nbits)++;
}

void tpms_bits_invert(uint8_t* bits, uint16_t nbytes) {
    for(uint16_t i = 0; i < nbytes; i++)
        bits[i] = (uint8_t)~bits[i];
}

void tpms_bits_invert_partial(uint8_t* bits, uint16_t nbits) {
    const uint16_t nbytes = (uint16_t)((nbits + 7) / 8);
    if(nbytes == 0) return;

    for(uint16_t i = 0; i < nbytes; i++)
        bits[i] = (uint8_t)~bits[i];

    const uint8_t spare = (uint8_t)((nbytes * 8) - nbits);
    if(spare) bits[nbytes - 1] &= (uint8_t)(0xFF << spare);
}

void tpms_bits_extract(const uint8_t* in, uint16_t start, uint16_t nbits, uint8_t* out) {
    uint16_t written = 0;
    for(uint16_t i = 0; i < nbits; i++) {
        tpms_bit_append(out, &written, tpms_bit_at(in, (uint16_t)(start + i)));
    }
}

uint16_t tpms_manchester_decode(
    const uint8_t* in,
    uint16_t start,
    uint16_t len,
    uint8_t* out,
    uint16_t max_bits) {
    uint16_t pos = start;
    uint16_t nbits = 0;

    while(pos + 1 < len && nbits < max_bits) {
        const uint8_t first = tpms_bit_at(in, pos++);
        const uint8_t second = tpms_bit_at(in, pos++);
        if(first == second) break;
        tpms_bit_append(out, &nbits, second);
    }
    return nbits;
}

uint16_t tpms_diff_manchester_decode(
    const uint8_t* in,
    uint16_t start,
    uint16_t len,
    uint8_t* out,
    uint16_t max_bits,
    uint16_t* out_bits) {
    uint16_t pos = start;
    uint16_t nbits = 0;
    uint8_t first;
    uint8_t second = 0;

    /* The first long pulse sets the clock; a short one before it is
     * skipped to get into step. */
    while(pos + 2 < len) {
        first = tpms_bit_at(in, pos++);
        second = tpms_bit_at(in, pos++);
        const uint8_t third = tpms_bit_at(in, pos);

        if(first != second) {
            if(second != third) {
                tpms_bit_append(out, &nbits, 0);
            } else {
                second = first;
                pos -= 1;
                break;
            }
        } else {
            second = (uint8_t)(1 - first);
            pos -= 2;
            break;
        }
    }

    while(pos + 1 < len && nbits < max_bits) {
        first = tpms_bit_at(in, pos++);
        if(first == second) break; /* clock missing, the frame ended */
        second = tpms_bit_at(in, pos++);
        tpms_bit_append(out, &nbits, (uint8_t)(first == second ? 1 : 0));
    }

    *out_bits = nbits;
    return pos;
}

uint8_t tpms_crc4(const uint8_t* message, uint16_t bytes, uint8_t poly, uint8_t init) {
    uint32_t remainder = (uint32_t)init << 4; /* the low bits are unused */
    const uint32_t polynomial = (uint32_t)poly << 4;

    for(uint16_t byte = 0; byte < bytes; byte++) {
        remainder ^= message[byte];
        for(uint8_t bit = 0; bit < 8; bit++) {
            remainder = (remainder & 0x80) ? ((remainder << 1) ^ polynomial) : (remainder << 1);
        }
    }
    return (uint8_t)((remainder >> 4) & 0x0F);
}

uint8_t tpms_crc7(const uint8_t* message, uint16_t bytes, uint8_t poly, uint8_t init) {
    uint32_t remainder = (uint32_t)init << 1; /* the low bit is unused */
    const uint32_t polynomial = (uint32_t)poly << 1;

    for(uint16_t byte = 0; byte < bytes; byte++) {
        remainder ^= message[byte];
        for(uint8_t bit = 0; bit < 8; bit++) {
            remainder = (remainder & 0x80) ? ((remainder << 1) ^ polynomial) : (remainder << 1);
        }
    }
    return (uint8_t)((remainder >> 1) & 0x7F);
}

uint8_t tpms_crc8(const uint8_t* message, uint16_t bytes, uint8_t poly, uint8_t init) {
    uint8_t remainder = init;
    for(uint16_t byte = 0; byte < bytes; byte++) {
        remainder ^= message[byte];
        for(uint8_t bit = 0; bit < 8; bit++) {
            remainder = (remainder & 0x80) ? (uint8_t)((remainder << 1) ^ poly) :
                                             (uint8_t)(remainder << 1);
        }
    }
    return remainder;
}

uint8_t tpms_reverse8(uint8_t x) {
    x = (uint8_t)((x & 0xF0) >> 4 | (x & 0x0F) << 4);
    x = (uint8_t)((x & 0xCC) >> 2 | (x & 0x33) << 2);
    x = (uint8_t)((x & 0xAA) >> 1 | (x & 0x55) << 1);
    return x;
}

uint8_t tpms_crc8le(const uint8_t* message, uint16_t bytes, uint8_t poly, uint8_t init) {
    uint8_t remainder = tpms_reverse8(init);
    const uint8_t polynomial = tpms_reverse8(poly);

    for(uint16_t byte = 0; byte < bytes; byte++) {
        remainder ^= message[byte];
        for(uint8_t bit = 0; bit < 8; bit++) {
            remainder = (remainder & 1) ? (uint8_t)((remainder >> 1) ^ polynomial) :
                                          (uint8_t)(remainder >> 1);
        }
    }
    return remainder;
}

uint16_t tpms_crc16(const uint8_t* message, uint16_t bytes, uint16_t poly, uint16_t init) {
    uint16_t remainder = init;
    for(uint16_t byte = 0; byte < bytes; byte++) {
        remainder ^= (uint16_t)((uint16_t)message[byte] << 8);
        for(uint8_t bit = 0; bit < 8; bit++) {
            remainder = (remainder & 0x8000) ? (uint16_t)((remainder << 1) ^ poly) :
                                               (uint16_t)(remainder << 1);
        }
    }
    return remainder;
}

uint16_t tpms_crc16lsb(const uint8_t* message, uint16_t bytes, uint16_t poly, uint16_t init) {
    uint16_t remainder = init;
    for(uint16_t byte = 0; byte < bytes; byte++) {
        remainder ^= message[byte];
        for(uint8_t bit = 0; bit < 8; bit++) {
            remainder = (remainder & 1) ? (uint16_t)((remainder >> 1) ^ poly) :
                                          (uint16_t)(remainder >> 1);
        }
    }
    return remainder;
}

uint8_t tpms_xor_bytes(const uint8_t* message, uint16_t bytes) {
    uint8_t result = 0;
    for(uint16_t i = 0; i < bytes; i++)
        result ^= message[i];
    return result;
}

uint32_t tpms_add_bytes(const uint8_t* message, uint16_t bytes) {
    uint32_t result = 0;
    for(uint16_t i = 0; i < bytes; i++)
        result += message[i];
    return result;
}

uint32_t tpms_add_nibbles(const uint8_t* message, uint16_t bytes) {
    uint32_t result = 0;
    for(uint16_t i = 0; i < bytes; i++)
        result += (message[i] >> 4) + (message[i] & 0x0F);
    return result;
}

uint8_t tpms_lfsr_digest8(const uint8_t* message, uint16_t bytes, uint8_t gen, uint8_t key) {
    uint8_t sum = 0;
    for(uint16_t k = 0; k < bytes; k++) {
        const uint8_t data = message[k];
        for(int8_t i = 7; i >= 0; i--) {
            if((data >> i) & 1) sum ^= key;
            key = (key & 1) ? (uint8_t)((key >> 1) ^ gen) : (uint8_t)(key >> 1);
        }
    }
    return sum;
}

uint8_t
    tpms_lfsr_digest8_reflect(const uint8_t* message, uint16_t bytes, uint8_t gen, uint8_t key) {
    uint8_t sum = 0;
    for(int32_t k = (int32_t)bytes - 1; k >= 0; k--) {
        const uint8_t data = message[k];
        for(uint8_t i = 0; i < 8; i++) {
            if((data >> i) & 1) sum ^= key;
            key = (key & 0x80) ? (uint8_t)((key << 1) ^ gen) : (uint8_t)(key << 1);
        }
    }
    return sum;
}

uint8_t tpms_parity8(uint8_t byte) {
    byte ^= (uint8_t)(byte >> 4);
    byte &= 0x0F;
    return (uint8_t)((0x6996 >> byte) & 1);
}

int32_t tpms_kpa_from_psi_x100(int32_t psi_x100) {
    /* 1 PSI = 6.89476 kPa. Rounded to the nearest hundredth of a kPa,
     * negatives included. */
    const int64_t scaled = (int64_t)psi_x100 * 689476;
    return (int32_t)((scaled >= 0 ? scaled + 50000 : scaled - 50000) / 100000);
}

int32_t tpms_kpa_from_bar_x100(int32_t bar_x100) {
    return bar_x100 * 100;
}
