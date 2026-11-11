#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UHF_FREQUENCY_REGION_FCC       1U
#define UHF_FREQUENCY_REGION_ETSI      2U
#define UHF_FREQUENCY_REGION_CHN_1     3U
#define UHF_FREQUENCY_REGION_CUSTOM    4U
#define UHF_FREQUENCY_REGION_CHN_2     5U
#define UHF_FREQUENCY_MIN_KHZ          840000UL
#define UHF_FREQUENCY_MAX_KHZ          960000UL
#define UHF_FREQUENCY_MIN_SPACING_KHZ  25U
#define UHF_FREQUENCY_MAX_COUNT        255U
#define UHF_FREQUENCY_MAX_PAYLOAD_SIZE 7U

/* Standard regions retain their wire indices. For CHN_1 the mapping is unknown:
 * start_khz, spacing_khz and count are zero, and only the raw indices are usable.
 * For mapped standards all fields must agree. Custom ignores the raw indices. */
typedef struct {
    uint8_t region;
    uint8_t count;
    uint16_t spacing_khz;
    uint32_t start_khz;
    uint8_t start_index;
    uint8_t end_index;
} UhfFrequency;

/* Parse only the 0x79 DATA payload, after frame/checksum validation by caller.
 * Exact lengths are required (3 standard, 7 custom). Failure leaves out intact. */
bool uhf_frequency_parse(const uint8_t* payload, size_t payload_len, UhfFrequency* out);
/* Encode only the 0x78 DATA payload. Failure leaves out intact and sets *out_len=0.
 * out_len may be NULL. No command, address, length or checksum is included. */
bool uhf_frequency_encode(
    const UhfFrequency* frequency,
    uint8_t* out,
    size_t out_size,
    size_t* out_len);
bool uhf_frequency_valid(const UhfFrequency* frequency);
/* Full table range for FCC/ETSI/CHN_2; custom defaults to 900000/25/1.
 * CHN_1 and unknown regions return false without changing out. */
bool uhf_frequency_preset(uint8_t region, UhfFrequency* out);
/* Compares wire meaning, ignoring custom raw indices and structure padding. */
bool uhf_frequency_equal(const UhfFrequency* a, const UhfFrequency* b);
const char* uhf_frequency_region_name(uint8_t region);
