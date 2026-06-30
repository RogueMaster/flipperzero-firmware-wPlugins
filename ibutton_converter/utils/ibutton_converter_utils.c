#include <one_wire/maxim_crc.h>
#include <furi.h>

static const uint8_t c5_c6_nibbles_map[16] =
    {0xF, 0xB, 0x7, 0x3, 0xE, 0xA, 0x6, 0x2, 0xD, 0x9, 0x5, 0x1, 0xC, 0x8, 0x4, 0x0};

uint8_t cyfral_bits_to_nibble_c3(uint8_t val) {
    switch(val & 0b11) {
    case 0b00:
        return 0x08;
    case 0b01:
        return 0x04;
    case 0b10:
        return 0x02;
    case 0b11:
        return 0x01;
    default:
        furi_crash(
            "We're live in the intuitionistic logic world! Law of excluded middle is bullshit!");
    }
}

void cyfral_to_intermediate_representation_c3(uint8_t cyfral_code[2], uint8_t intermediate[4]) {
    for(int i = 0; i < 2; i++) {
        uint8_t hi_hi = (cyfral_code[i] >> 6) & 0b11;
        uint8_t hi_lo = (cyfral_code[i] >> 4) & 0b11;

        uint8_t lo_hi = (cyfral_code[i] >> 2) & 0b11;
        uint8_t lo_lo = (cyfral_code[i]) & 0b11;

        uint8_t hi_hi_nibble = cyfral_bits_to_nibble_c3(hi_hi);
        uint8_t hi_lo_nibble = cyfral_bits_to_nibble_c3(hi_lo);

        uint8_t lo_hi_nibble = cyfral_bits_to_nibble_c3(lo_hi);
        uint8_t lo_lo_nibble = cyfral_bits_to_nibble_c3(lo_lo);

        intermediate[3 - (i * 2 + 1)] = (hi_hi_nibble << 4) | hi_lo_nibble;
        intermediate[3 - (i * 2)] = (lo_hi_nibble << 4) | lo_lo_nibble;
    }
}

// Metakom → Dallas
void metakom_to_dallas(uint8_t metakom_code[4], uint8_t dallas_code[8], int reversed) {
    dallas_code[0] = 0x01;

    for(int i = 0; i < 4; i++) {
        dallas_code[1 + i] = reversed ? metakom_code[3 - i] : metakom_code[i];
    }

    dallas_code[5] = 0x00;
    dallas_code[6] = 0x00;
    dallas_code[7] = maxim_crc8(dallas_code, 7, 0);
}

void cyfral_to_dallas_c1(uint8_t cyfral_code[2], uint8_t dallas_code[8]) {
    dallas_code[0] = 0x01;
    dallas_code[1] = cyfral_code[0];
    dallas_code[2] = cyfral_code[1];
    dallas_code[3] = 0x01;
    dallas_code[4] = 0x00;
    dallas_code[5] = 0x00;
    dallas_code[6] = 0x00;
    dallas_code[7] = maxim_crc8(dallas_code, 7, 0);
}

void cyfral_to_dallas_c2(uint8_t cyfral_code[2], uint8_t dallas_code[8]) {
    dallas_code[0] = 0x01;

    uint8_t first_hi = (cyfral_code[0] >> 4) & 0x0F;
    uint8_t first_lo = cyfral_code[0] & 0x0F;

    uint8_t second_hi = (cyfral_code[1] >> 4) & 0x0F;
    uint8_t second_lo = cyfral_code[1] & 0x0F;

    first_hi |= ((second_lo & 0x02) << 1);
    second_lo |= (second_lo >> 3);

    dallas_code[1] = (first_hi << 4) | first_lo;
    dallas_code[2] = (second_hi << 4) | second_lo;

    dallas_code[3] = 0x01;
    dallas_code[4] = 0x00;
    dallas_code[5] = 0x00;
    dallas_code[6] = 0x00;

    dallas_code[7] = maxim_crc8(dallas_code, 7, 0);
}

void cyfral_to_dallas_c2_alt(uint8_t cyfral_code[2], uint8_t dallas_code[8]) {
    dallas_code[0] = 0x01;

    uint8_t mask_high = cyfral_code[1] & 0xAA;

    uint8_t first_lo = mask_high >> 3;
    uint8_t first = cyfral_code[1] | first_lo;

    uint8_t second_lo = (mask_high << 5) & 0xFF;
    uint8_t second = cyfral_code[0] | second_lo;

    dallas_code[1] = second;
    dallas_code[2] = first;

    dallas_code[3] = 0x01;
    dallas_code[4] = 0x00;
    dallas_code[5] = 0x00;
    dallas_code[6] = 0x00;

    dallas_code[7] = maxim_crc8(dallas_code, 7, 0);
}

void cyfral_to_dallas_c3(uint8_t cyfral_code[2], uint8_t dallas_code[8]) {
    uint8_t intermediate[4];

    cyfral_to_intermediate_representation_c3(cyfral_code, intermediate);

    dallas_code[0] = 0x01;
    dallas_code[1] = intermediate[0];
    dallas_code[2] = intermediate[1];
    dallas_code[3] = intermediate[2];
    dallas_code[4] = intermediate[3];
    dallas_code[5] = 0x00;
    dallas_code[6] = 0x00;
    dallas_code[7] = maxim_crc8(dallas_code, 7, 0);
}

void cyfral_to_dallas_c4(uint8_t cyfral_code[2], uint8_t dallas_code[8]) {
    UNUSED(cyfral_code);

    dallas_code[0] = 0x01;
    dallas_code[1] = 0xFF;
    dallas_code[2] = 0xFF;
    dallas_code[3] = 0xFF;
    dallas_code[4] = 0xFF;
    dallas_code[5] = 0x00;
    dallas_code[6] = 0x00;
    dallas_code[7] = 0x9B;
}

void cyfral_to_dallas_c5_c6(uint8_t cyfral_code[2], uint8_t dallas_code[8], bool is_C5) {
    dallas_code[0] = 0x01;

    for(int i = 0; i < 2; ++i) {
        uint8_t nibbles[2] = {(cyfral_code[i] >> 4) & 0x0F, cyfral_code[i] & 0x0F};

        uint8_t nibble_hi = c5_c6_nibbles_map[nibbles[0]];
        uint8_t nibble_lo = c5_c6_nibbles_map[nibbles[1]];

        if(is_C5 && i == 1) {
            nibble_hi = 0;
        }

        dallas_code[1 + i] = (nibble_hi << 4) | nibble_lo;
    }

    dallas_code[3] = 0x80;
    dallas_code[4] = 0x00;
    dallas_code[5] = 0x00;
    dallas_code[6] = 0x00;

    dallas_code[7] = maxim_crc8(dallas_code, 7, 0);
}

void cyfral_to_dallas_c5(uint8_t cyfral_code[2], uint8_t dallas_code[8]) {
    cyfral_to_dallas_c5_c6(cyfral_code, dallas_code, true);
}

void cyfral_to_dallas_c6(uint8_t cyfral_code[2], uint8_t dallas_code[8]) {
    cyfral_to_dallas_c5_c6(cyfral_code, dallas_code, false);
}

void cyfral_to_dallas_c7(uint8_t cyfral_code[2], uint8_t dallas_code[8]) {
    dallas_code[0] = 0x01;

    for(int i = 0; i < 2; ++i) {
        uint8_t nibbles[2] = {(cyfral_code[i] >> 4) & 0x0F, cyfral_code[i] & 0x0F};

        uint8_t nibble_hi = 0xF - nibbles[0];
        uint8_t nibble_lo = 0xF - nibbles[1];
        dallas_code[1 + i] = (nibble_hi << 4) | nibble_lo;
    }

    dallas_code[3] = 0x00;
    dallas_code[4] = 0x00;
    dallas_code[5] = 0x00;
    dallas_code[6] = 0x00;

    dallas_code[7] = maxim_crc8(dallas_code, 7, 0);
}
