
#include "interface.h"

#include <lib/bit_lib/bit_lib.h>
#include <flipper_application/flipper_application.h>

/*
 * Huge thanks to the proxmark codebase:
 * https://github.com/RfidResearchGroup/proxmark3/blob/master/client/src/wiegand_formats.c
 */

// Structure for packed wiegand messages
// Always align lowest value (last transmitted) bit to ordinal position 0 (lowest valued bit bottom)
typedef struct {
    uint8_t Length; // Number of encoded bits in wiegand message (excluding headers and preamble)
    uint32_t Top; // Bits in x<<64 positions
    uint32_t Mid; // Bits in x<<32 positions
    uint32_t Bot; // Lowest ordinal positions
} wiegand_message_t;

static inline uint8_t oddparity32(uint32_t x) {
    return bit_lib_test_parity_32(x, BitLibParityOdd);
}

static inline uint8_t evenparity32(uint32_t x) {
    return bit_lib_test_parity_32(x, BitLibParityEven);
}

uint8_t get_bit_by_position(wiegand_message_t* data, uint8_t pos) {
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

uint64_t get_linear_field(wiegand_message_t* data, uint8_t firstBit, uint8_t length) {
    uint64_t result = 0;
    for(uint8_t i = 0; i < length; i++) {
        result = (result << 1) | get_bit_by_position(data, firstBit + i);
    }
    return result;
}

static int wiegand_C1k35s_parse(uint8_t bit_length, uint64_t bits, FuriString* description) {
    wiegand_message_t value;
    value.Length = bit_length;
    value.Mid = bits >> 32;
    value.Bot = bits;
    wiegand_message_t* packed = &value;

    if(packed->Length != 35) return false; // Wrong length? Stop here.

    uint32_t cn = (packed->Bot >> 1) & 0x000FFFFF;
    uint32_t fc = ((packed->Mid & 1) << 11) | ((packed->Bot >> 21));
    bool valid = (evenparity32((packed->Mid & 0x1) ^ (packed->Bot & 0xB6DB6DB6)) ==
                  ((packed->Mid >> 1) & 1)) &&
                 (oddparity32((packed->Mid & 0x3) ^ (packed->Bot & 0x6DB6DB6C)) ==
                  ((packed->Bot >> 0) & 1)) &&
                 (oddparity32((packed->Mid & 0x3) ^ (packed->Bot & 0xFFFFFFFF)) ==
                  ((packed->Mid >> 2) & 1));

    if(valid) {
        furi_string_cat_printf(description, "C1k35s\nFC: %ld CN: %ld\n", fc, cn);
        return 1;
    } else {
        FURI_LOG_D(PLUGIN_APP_ID, "C1k35s invalid");
    }

    return 0;
}

static int wiegand_h10301_parse(uint8_t bit_length, uint64_t bits, FuriString* description) {
    if(bit_length != 26) {
        return 0;
    }

    //E XXXX XXXX XXXX
    //XXXX XXXX XXXX O
    uint32_t eBitMask = 0x02000000;
    uint32_t oBitMask = 0x00000001;
    uint32_t eParityMask = 0x01FFE000;
    uint32_t oParityMask = 0x00001FFE;
    uint8_t eBit = (eBitMask & bits) >> 25;
    uint8_t oBit = (oBitMask & bits) >> 0;

    bool eParity = bit_lib_test_parity_32((bits & eParityMask) >> 13, BitLibParityEven) ==
                   (eBit == 1);
    bool oParity = bit_lib_test_parity_32((bits & oParityMask) >> 1, BitLibParityOdd) ==
                   (oBit == 1);

    FURI_LOG_D(
        PLUGIN_APP_ID,
        "eBit: %d, oBit: %d, eParity: %d, oParity: %d",
        eBit,
        oBit,
        eParity,
        oParity);

    if(eParity && oParity) {
        uint32_t cnMask = 0x1FFFE;
        uint16_t cn = ((bits & cnMask) >> 1);

        uint32_t fcMask = 0x1FE0000;
        uint16_t fc = ((bits & fcMask) >> 17);

        furi_string_cat_printf(description, "H10301\nFC: %d CN: %d\n", fc, cn);
        return 1;
    } else {
        FURI_LOG_D(PLUGIN_APP_ID, "H10301 invalid");
    }

    return 0;
}

static int wiegand_H10304_parse(uint8_t bit_length, uint64_t bits, FuriString* description) {
    wiegand_message_t value;
    value.Length = bit_length;
    value.Mid = bits >> 32;
    value.Bot = bits;
    wiegand_message_t* packed = &value;

    if(packed->Length != 37) return false; // Wrong length? Stop here.

    uint32_t fc = get_linear_field(packed, 1, 16);
    uint32_t cn = get_linear_field(packed, 17, 19);
    bool valid =
        (get_bit_by_position(packed, 0) == evenparity32(get_linear_field(packed, 1, 18))) &&
        (get_bit_by_position(packed, 36) == oddparity32(get_linear_field(packed, 18, 18)));

    if(valid) {
        furi_string_cat_printf(description, "H10304\nFC: %ld CN: %ld\n", fc, cn);
        return 1;
    } else {
        FURI_LOG_D(PLUGIN_APP_ID, "H10304 invalid");
    }

    return 0;
}

static int wiegand_H10302_parse(uint8_t bit_length, uint64_t bits, FuriString* description) {
    wiegand_message_t value;
    value.Length = bit_length;
    value.Mid = bits >> 32;
    value.Bot = bits;
    wiegand_message_t* packed = &value;

    if(packed->Length != 37) return false; // Wrong length? Stop here.

    uint64_t cn = get_linear_field(packed, 1, 35);
    bool valid =
        (get_bit_by_position(packed, 0) == evenparity32(get_linear_field(packed, 1, 18))) &&
        (get_bit_by_position(packed, 36) == oddparity32(get_linear_field(packed, 18, 18)));

    if(valid) {
        furi_string_cat_printf(description, "H10302\nCN: %lld\n", cn);
        return 1;
    } else {
        FURI_LOG_D(PLUGIN_APP_ID, "H10302 invalid");
    }

    return 0;
}

static int wiegand_format_count(uint8_t bit_length, uint64_t bits) {
    UNUSED(bit_length);
    UNUSED(bits);
    int count = 0;
    FuriString* ignore = furi_string_alloc();

    // NOTE: Always update the `total` and add to the wiegand_format_description function
    // TODO: Make this into a function pointer array
    count += wiegand_h10301_parse(bit_length, bits, ignore);
    count += wiegand_C1k35s_parse(bit_length, bits, ignore);
    count += wiegand_H10302_parse(bit_length, bits, ignore);
    count += wiegand_H10304_parse(bit_length, bits, ignore);
    int total = 4;

    furi_string_free(ignore);

    FURI_LOG_I(PLUGIN_APP_ID, "count: %i/%i", count, total);
    return count;
}

static void wiegand_format_description(
    uint8_t bit_length,
    uint64_t bits,
    size_t index,
    FuriString* description) {
    FURI_LOG_I(PLUGIN_APP_ID, "description %d", index);

    // Turns out I did this wrong and trying to use the index means the results get repeated.  Instead, just return the results for index == 0
    if(index != 0) {
        return;
    }

    wiegand_h10301_parse(bit_length, bits, description);
    wiegand_C1k35s_parse(bit_length, bits, description);
    wiegand_H10302_parse(bit_length, bits, description);
    wiegand_H10304_parse(bit_length, bits, description);
}

/* Actual implementation of app<>plugin interface */
static const PluginWiegand plugin_wiegand = {
    .name = "Plugin Wiegand",
    .count = &wiegand_format_count,
    .description = &wiegand_format_description,
};

/* Plugin descriptor to comply with basic plugin specification */
static const FlipperAppPluginDescriptor plugin_wiegand_descriptor = {
    .appid = PLUGIN_APP_ID,
    .ep_api_version = PLUGIN_API_VERSION,
    .entry_point = &plugin_wiegand,
};

/* Plugin entry point - must return a pointer to const descriptor */
const FlipperAppPluginDescriptor* plugin_wiegand_ep(void) {
    return &plugin_wiegand_descriptor;
}
