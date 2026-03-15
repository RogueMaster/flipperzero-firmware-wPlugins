#include "picopass_wiegand.h"
#include <string.h>

static void message_datacopy(const wiegand_message_t* src, wiegand_message_t* dest) {
    dest->Bot = src->Bot;
    dest->Mid = src->Mid;
    dest->Top = src->Top;
    dest->Length = src->Length;
}

static inline uint8_t oddparity32(uint32_t x) {
    return bit_lib_test_parity_32(x, BitLibParityOdd);
}

static inline uint8_t evenparity32(uint32_t x) {
    return bit_lib_test_parity_32(x, BitLibParityEven);
}

bool picopass_set_bit_by_position(wiegand_message_t* data, bool value, uint8_t pos) {
    if(pos >= data->Length) return false;
    pos = (data->Length - pos) -
          1; // invert ordering; Indexing goes from 0 to 1. Subtract 1 for weight of bit.
    if(pos > 95) {
        return false;
    } else if(pos > 63) {
        if(value)
            data->Top |= (1UL << (pos - 64));
        else
            data->Top &= ~(1UL << (pos - 64));
        return true;
    } else if(pos > 31) {
        if(value)
            data->Mid |= (1UL << (pos - 32));
        else
            data->Mid &= ~(1UL << (pos - 32));
        return true;
    } else {
        if(value)
            data->Bot |= (1UL << pos);
        else
            data->Bot &= ~(1UL << pos);
        return true;
    }
}

uint8_t picopass_get_bit_by_position(wiegand_message_t* data, uint8_t pos) {
    if(pos >= data->Length) return false;
    pos = (data->Length - pos) -
          1; // invert ordering; Indexing goes from 0 to 1. Subtract 1 for weight of bit.
    uint8_t result = 0;
    if(pos > 95)
        result = 0;
    else if(pos > 63)
        result = (data->Top >> (pos - 64)) & 1;
    else if(pos > 31)
        result = (data->Mid >> (pos - 32)) & 1;
    else
        result = (data->Bot >> pos) & 1;
    return result;
}

bool picopass_set_linear_field(
    wiegand_message_t* data,
    uint64_t value,
    uint8_t firstBit,
    uint8_t length) {
    wiegand_message_t tmpdata;
    message_datacopy(data, &tmpdata);
    bool result = true;
    for(int i = 0; i < length; i++) {
        result &= picopass_set_bit_by_position(
            &tmpdata, (value >> ((length - i) - 1)) & 1, firstBit + i);
    }
    if(result) message_datacopy(&tmpdata, data);

    return result;
}

uint64_t picopass_get_linear_field(wiegand_message_t* data, uint8_t firstBit, uint8_t length) {
    uint64_t result = 0;
    for(uint8_t i = 0; i < length; i++) {
        result = (result << 1) | picopass_get_bit_by_position(data, firstBit + i);
    }
    return result;
}

wiegand_message_t
    picopass_initialize_wiegand_message_object(uint32_t top, uint32_t mid, uint32_t bot, int n) {
    wiegand_message_t result;
    memset(&result, 0, sizeof(wiegand_message_t));

    result.Top = top;
    result.Mid = mid;
    result.Bot = bot;
    if(n > 0) result.Length = n;
    return result;
}

int picopass_wiegand_format_count(wiegand_message_t* packed) {
    int count = 0;
    wiegand_card_t card;
    if(picopass_Unpack_H10301(packed, &card)) count++;
    if(picopass_Unpack_C1k35s(packed, &card)) count++;
    if(picopass_Unpack_H10302(packed, &card)) count++;
    if(picopass_Unpack_H10304(packed, &card)) count++;
    return count;
}

void picopass_wiegand_format_description(wiegand_message_t* packed, FuriString* description) {
    wiegand_card_t card;
    if(picopass_Unpack_H10301(packed, &card)) {
        furi_string_cat_printf(
            description, "H10301\nFC: %lu CN: %llu\n", card.FacilityCode, card.CardNumber);
    }
    if(picopass_Unpack_C1k35s(packed, &card)) {
        furi_string_cat_printf(
            description, "C1k35s\nFC: %lu CN: %llu\n", card.FacilityCode, card.CardNumber);
    }
    if(picopass_Unpack_H10302(packed, &card)) {
        furi_string_cat_printf(description, "H10302\nCN: %llu\n", card.CardNumber);
    }
    if(picopass_Unpack_H10304(packed, &card)) {
        furi_string_cat_printf(
            description, "H10304\nFC: %lu CN: %llu\n", card.FacilityCode, card.CardNumber);
    }
}

const char* picopass_wiegand_format_name(WiegandFormat format) {
    switch(format) {
    case WiegandFormat_H10301:
        return "H10301";
    case WiegandFormat_C1k35s:
        return "C1k35s";
    case WiegandFormat_H10302:
        return "H10302";
    case WiegandFormat_H10304:
        return "H10304";
    default:
        return "Unknown";
    }
}

bool picopass_Pack_H10301(wiegand_card_t* card, wiegand_message_t* packed) {
    memset(packed, 0, sizeof(wiegand_message_t));

    packed->Length = 26; // Set number of bits
    packed->Bot |= (card->CardNumber & 0xFFFF) << 1;
    packed->Bot |= (card->FacilityCode & 0xFF) << 17;
    packed->Bot |= oddparity32((packed->Bot >> 1) & 0xFFF);
    packed->Bot |= (evenparity32((packed->Bot >> 13) & 0xFFF)) << 25;

    return true;
}

bool picopass_Unpack_H10301(wiegand_message_t* packed, wiegand_card_t* card) {
    if(packed->Length != 26) return false; // Wrong length? Stop here.

    memset(card, 0, sizeof(wiegand_card_t));

    card->CardNumber = (packed->Bot >> 1) & 0xFFFF;
    card->FacilityCode = (packed->Bot >> 17) & 0xFF;
    card->ParityValid = (oddparity32((packed->Bot >> 1) & 0xFFF) == (packed->Bot & 1)) &&
                        ((evenparity32((packed->Bot >> 13) & 0xFFF)) == ((packed->Bot >> 25) & 1));
    return card->ParityValid;
}

// C1k35s (HID Corporate 1000 35-bit)
bool picopass_Pack_C1k35s(wiegand_card_t* card, wiegand_message_t* packed) {
    memset(packed, 0, sizeof(wiegand_message_t));

    packed->Length = 35; // Set number of bits
    packed->Bot |= (card->CardNumber & 0x000FFFFF) << 1;
    packed->Bot |= (card->FacilityCode & 0x000007FF) << 21;
    packed->Mid |= (card->FacilityCode & 0x00000800) >> 11;
    packed->Mid |= (evenparity32((packed->Mid & 0x1) ^ (packed->Bot & 0xB6DB6DB6))) << 1;
    packed->Bot |= (oddparity32((packed->Mid & 0x3) ^ (packed->Bot & 0x6DB6DB6C)));
    packed->Mid |= (oddparity32((packed->Mid & 0x3) ^ (packed->Bot & 0xFFFFFFFF))) << 2;
    return true;
}

bool picopass_Unpack_C1k35s(wiegand_message_t* packed, wiegand_card_t* card) {
    if(packed->Length != 35) return false; // Wrong length? Stop here.

    memset(card, 0, sizeof(wiegand_card_t));

    card->CardNumber = (packed->Bot >> 1) & 0x000FFFFF;
    card->FacilityCode = ((packed->Mid & 1) << 11) | ((packed->Bot >> 21));
    card->ParityValid = (evenparity32((packed->Mid & 0x1) ^ (packed->Bot & 0xB6DB6DB6)) ==
                         ((packed->Mid >> 1) & 1)) &&
                        (oddparity32((packed->Mid & 0x3) ^ (packed->Bot & 0x6DB6DB6C)) ==
                         ((packed->Bot >> 0) & 1)) &&
                        (oddparity32((packed->Mid & 0x3) ^ (packed->Bot & 0xFFFFFFFF)) ==
                         ((packed->Mid >> 2) & 1));
    return card->ParityValid;
}

// H10302
bool picopass_Pack_H10302(wiegand_card_t* card, wiegand_message_t* packed) {
    memset(packed, 0, sizeof(wiegand_message_t));

    packed->Length = 37; // Set number of bits
    picopass_set_linear_field(packed, card->CardNumber, 1, 35);
    picopass_set_bit_by_position(
        packed, evenparity32(picopass_get_linear_field(packed, 1, 18)), 0);
    picopass_set_bit_by_position(
        packed, oddparity32(picopass_get_linear_field(packed, 18, 18)), 36);
    return true;
}

bool picopass_Unpack_H10302(wiegand_message_t* packed, wiegand_card_t* card) {
    if(packed->Length != 37) return false; // Wrong length? Stop here.

    memset(card, 0, sizeof(wiegand_card_t));

    card->CardNumber = picopass_get_linear_field(packed, 1, 35);
    card->ParityValid = (picopass_get_bit_by_position(packed, 0) ==
                         evenparity32(picopass_get_linear_field(packed, 1, 18))) &&
                        (picopass_get_bit_by_position(packed, 36) ==
                         oddparity32(picopass_get_linear_field(packed, 18, 18)));
    return card->ParityValid;
}

// H10304
bool picopass_Pack_H10304(wiegand_card_t* card, wiegand_message_t* packed) {
    memset(packed, 0, sizeof(wiegand_message_t));

    packed->Length = 37; // Set number of bits

    picopass_set_linear_field(packed, card->FacilityCode, 1, 16);
    picopass_set_linear_field(packed, card->CardNumber, 17, 19);

    picopass_set_bit_by_position(
        packed, evenparity32(picopass_get_linear_field(packed, 1, 18)), 0);
    picopass_set_bit_by_position(
        packed, oddparity32(picopass_get_linear_field(packed, 18, 18)), 36);
    return true;
}

bool picopass_Unpack_H10304(wiegand_message_t* packed, wiegand_card_t* card) {
    if(packed->Length != 37) return false; // Wrong length? Stop here.

    memset(card, 0, sizeof(wiegand_card_t));

    card->FacilityCode = picopass_get_linear_field(packed, 1, 16);
    card->CardNumber = picopass_get_linear_field(packed, 17, 19);
    card->ParityValid = (picopass_get_bit_by_position(packed, 0) ==
                         evenparity32(picopass_get_linear_field(packed, 1, 18))) &&
                        (picopass_get_bit_by_position(packed, 36) ==
                         oddparity32(picopass_get_linear_field(packed, 18, 18)));
    return card->ParityValid;
}
