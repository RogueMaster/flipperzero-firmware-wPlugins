#include "uhf_frequency.h"

/* Reader manual chapter 5 and factory defaults use FCC 902000..928000,
 * despite the .250 MHz examples in 3.33 and the legacy host application.
 * CHN_1 (840..845 MHz) has no index table. Legacy applications use region 3
 * for 920..925 MHz, so their indices cannot establish a CHN_1 mapping. */
static bool uhf_frequency_standard_range(uint8_t region, uint8_t* first, uint8_t* last) {
    switch(region) {
    case UHF_FREQUENCY_REGION_FCC:
        *first = 7;
        *last = 59;
        return true;
    case UHF_FREQUENCY_REGION_ETSI:
        *first = 0;
        *last = 6;
        return true;
    case UHF_FREQUENCY_REGION_CHN_2:
        *first = 43;
        *last = 53;
        return true;
    default:
        return false;
    }
}

static uint32_t uhf_frequency_index_khz(uint8_t index) {
    return index <= 6 ? 865000UL + (uint32_t)index * 500U :
                        902000UL + (uint32_t)(index - 7U) * 500U;
}

bool uhf_frequency_valid(const UhfFrequency* frequency) {
    if(!frequency) return false;
    if(frequency->region == UHF_FREQUENCY_REGION_CUSTOM) {
        if(frequency->count == 0 || frequency->spacing_khz < UHF_FREQUENCY_MIN_SPACING_KHZ ||
           frequency->start_khz < UHF_FREQUENCY_MIN_KHZ ||
           frequency->start_khz > UHF_FREQUENCY_MAX_KHZ) {
            return false;
        }
        /* Both operands are bounded, and multiplication is explicitly 32-bit. */
        uint32_t end =
            frequency->start_khz + (uint32_t)(frequency->count - 1U) * frequency->spacing_khz;
        return end <= UHF_FREQUENCY_MAX_KHZ;
    }
    if(frequency->region == UHF_FREQUENCY_REGION_CHN_1) {
        /* Opaque readback, not a claim that these indices map to valid channels. */
        return frequency->start_index <= frequency->end_index && frequency->start_khz == 0 &&
               frequency->spacing_khz == 0 && frequency->count == 0;
    }
    uint8_t first, last;
    if(!uhf_frequency_standard_range(frequency->region, &first, &last)) return false;
    return frequency->start_index >= first && frequency->end_index <= last &&
           frequency->start_index <= frequency->end_index && frequency->spacing_khz == 500 &&
           frequency->start_khz == uhf_frequency_index_khz(frequency->start_index) &&
           frequency->count == frequency->end_index - frequency->start_index + 1U;
}

bool uhf_frequency_parse(const uint8_t* payload, size_t payload_len, UhfFrequency* out) {
    if(!payload || !out || payload_len == 0) return false;
    UhfFrequency frequency = {0};
    frequency.region = payload[0];
    if(frequency.region == UHF_FREQUENCY_REGION_CUSTOM) {
        if(payload_len != 7) return false;
        frequency.spacing_khz = (uint16_t)((uint16_t)payload[1] << 8 | payload[2]);
        frequency.count = payload[3];
        frequency.start_khz = (uint32_t)payload[4] << 16 | (uint32_t)payload[5] << 8 | payload[6];
    } else {
        if(payload_len != 3) return false;
        frequency.start_index = payload[1];
        frequency.end_index = payload[2];
        if(frequency.region != UHF_FREQUENCY_REGION_CHN_1) {
            uint8_t first, last;
            if(!uhf_frequency_standard_range(frequency.region, &first, &last) ||
               frequency.start_index < first || frequency.end_index > last ||
               frequency.start_index > frequency.end_index) {
                return false;
            }
            frequency.start_khz = uhf_frequency_index_khz(frequency.start_index);
            frequency.spacing_khz = 500;
            frequency.count = frequency.end_index - frequency.start_index + 1U;
        }
    }
    if(!uhf_frequency_valid(&frequency)) return false;
    *out = frequency;
    return true;
}

bool uhf_frequency_encode(
    const UhfFrequency* frequency,
    uint8_t* out,
    size_t out_size,
    size_t* out_len) {
    if(out_len) *out_len = 0;
    if(!out || !uhf_frequency_valid(frequency)) return false;
    const size_t length = frequency->region == UHF_FREQUENCY_REGION_CUSTOM ? 7U : 3U;
    if(out_size < length) return false;
    out[0] = frequency->region;
    if(frequency->region == UHF_FREQUENCY_REGION_CUSTOM) {
        out[1] = (uint8_t)(frequency->spacing_khz >> 8);
        out[2] = (uint8_t)frequency->spacing_khz;
        out[3] = frequency->count;
        out[4] = (uint8_t)(frequency->start_khz >> 16);
        out[5] = (uint8_t)(frequency->start_khz >> 8);
        out[6] = (uint8_t)frequency->start_khz;
    } else {
        out[1] = frequency->start_index;
        out[2] = frequency->end_index;
    }
    if(out_len) *out_len = length;
    return true;
}

bool uhf_frequency_preset(uint8_t region, UhfFrequency* out) {
    if(!out) return false;
    if(region == UHF_FREQUENCY_REGION_CUSTOM) {
        const UhfFrequency frequency = {
            .region = UHF_FREQUENCY_REGION_CUSTOM,
            .count = 1,
            .spacing_khz = 25,
            .start_khz = 900000,
        };
        *out = frequency;
        return true;
    }
    uint8_t first, last;
    if(!uhf_frequency_standard_range(region, &first, &last)) return false;
    const uint8_t payload[] = {region, first, last};
    return uhf_frequency_parse(payload, sizeof(payload), out);
}

bool uhf_frequency_equal(const UhfFrequency* a, const UhfFrequency* b) {
    if(!uhf_frequency_valid(a) || !uhf_frequency_valid(b) || a->region != b->region) {
        return false;
    }
    if(a->region == UHF_FREQUENCY_REGION_CUSTOM) {
        return a->start_khz == b->start_khz && a->spacing_khz == b->spacing_khz &&
               a->count == b->count;
    }
    return a->start_index == b->start_index && a->end_index == b->end_index;
}

const char* uhf_frequency_region_name(uint8_t region) {
    switch(region) {
    case UHF_FREQUENCY_REGION_FCC:
        return "FCC";
    case UHF_FREQUENCY_REGION_ETSI:
        return "ETSI";
    case UHF_FREQUENCY_REGION_CHN_1:
        return "CHN_1";
    case UHF_FREQUENCY_REGION_CUSTOM:
        return "Custom";
    case UHF_FREQUENCY_REGION_CHN_2:
        return "CHN_2";
    default:
        return "Unknown";
    }
}
