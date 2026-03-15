#pragma once

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include <lib/bit_lib/bit_lib.h>
#include <furi.h>

typedef enum {
    WiegandFormat_None = 0,
    WiegandFormat_H10301,
    WiegandFormat_C1k35s,
    WiegandFormat_H10302,
    WiegandFormat_H10304,
    WiegandFormat_Count
} WiegandFormat;

// Structure for packed wiegand messages
// Always align lowest value (last transmitted) bit to ordinal position 0 (lowest valued bit bottom)
typedef struct {
    uint8_t Length; // Number of encoded bits in wiegand message (excluding headers and preamble)
    uint32_t Top; // Bits in x<<64 positions
    uint32_t Mid; // Bits in x<<32 positions
    uint32_t Bot; // Lowest ordinal positions
} wiegand_message_t;

// Structure for unpacked wiegand card, like HID prox
typedef struct {
    uint32_t FacilityCode;
    uint64_t CardNumber;
    uint32_t IssueLevel;
    uint32_t OEM;
    bool ParityValid; // Only valid for responses
} wiegand_card_t;

wiegand_message_t
    picopass_initialize_wiegand_message_object(uint32_t top, uint32_t mid, uint32_t bot, int n);

bool picopass_Pack_H10301(wiegand_card_t* card, wiegand_message_t* packed);
bool picopass_Unpack_H10301(wiegand_message_t* packed, wiegand_card_t* card);

// Added formats
bool picopass_Pack_C1k35s(wiegand_card_t* card, wiegand_message_t* packed);
bool picopass_Unpack_C1k35s(wiegand_message_t* packed, wiegand_card_t* card);

bool picopass_Pack_H10302(wiegand_card_t* card, wiegand_message_t* packed);
bool picopass_Unpack_H10302(wiegand_message_t* packed, wiegand_card_t* card);

bool picopass_Pack_H10304(wiegand_card_t* card, wiegand_message_t* packed);
bool picopass_Unpack_H10304(wiegand_message_t* packed, wiegand_card_t* card);

int picopass_wiegand_format_count(wiegand_message_t* packed);
void picopass_wiegand_format_description(wiegand_message_t* packed, FuriString* description);
const char* picopass_wiegand_format_name(WiegandFormat format);
