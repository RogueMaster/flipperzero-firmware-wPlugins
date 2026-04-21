#include "protocol_items.h"
#include <string.h>

const SubGhzProtocol* protopirate_protocol_registry_items[] = {
    &subghz_protocol_scher_khan, // Heap: free 16320
    &kia_protocol_v0, // Heap: free 16976
    &kia_protocol_v1, // Heap: free 17192
    &kia_protocol_v2, // Heap: free 16944
    &kia_protocol_v3_v4, // Heap: free 18432
    &kia_protocol_v5, // Heap: free 16528
    &kia_protocol_v6, // Heap: free 18296
    &ford_protocol_v0, // Heap: free 19456
    &ford_protocol_v1,
    &fiat_protocol_v0, // Heap: free 16864
    &fiat_v1_protocol,
    &mazda_v0_protocol,
    &kia_protocol_v7,
    &mitsubishi_v0_protocol,
    &porsche_touareg_protocol,
    &subaru_protocol, // Heap: free 17280
    &vag_protocol, // Heap: free 29352
    &subghz_protocol_star_line, // Heap: free 18632
    &psa_protocol, // Heap: free 25408
    &honda_static_protocol,
};
// TODO: See above
// Current HEAP situation:
// All enabled
// Heap: total 190000, free 14944
// Two scenes disabled (Sub Decode and Timing Tuner)
// Heap: total 190000, free 28192, minimum 22944, max block 27256
// No app (desktop)
// Heap: total 190000, free 119824, minimum 14008, max block 94576

const SubGhzProtocolRegistry protopirate_protocol_registry = {
    .items = protopirate_protocol_registry_items,
    .size = COUNT_OF(protopirate_protocol_registry_items),
};

// Protocol timing definitions - mirrors the SubGhzBlockConst in each protocol
static const ProtoPirateProtocolTiming protocol_timings[] = {
    // Honda Static
    {
        .name = HONDA_STATIC_PROTOCOL_NAME,
        .te_short = 63,
        .te_long = 700,
        .te_delta = 120,
        .min_count_bit = 64,
    },
    // Kia V0: PWM 250/500µs — Kia 61bit, Suzuki 64bit, Honda V0 72bit
    {
        .name = "Kia V0",
        .te_short = 250,
        .te_long = 500,
        .te_delta = 100,
        .min_count_bit = 61,
    },
    // Kia V1: OOK PCM 800µs timing
    {
        .name = "Kia V1",
        .te_short = 800,
        .te_long = 1600,
        .te_delta = 200,
        .min_count_bit = 56,
    },
    // Kia V2: Manchester 500/1000µs
    {
        .name = "Kia V2",
        .te_short = 500,
        .te_long = 1000,
        .te_delta = 150,
        .min_count_bit = 51,
    },
    // Kia V3/V4: PWM 400/800µs
    {
        .name = "Kia V3/V4",
        .te_short = 400,
        .te_long = 800,
        .te_delta = 150,
        .min_count_bit = 64,
    },
    // Kia V5: PWM 400/800µs (same as V3/V4)
    {
        .name = "Kia V5",
        .te_short = 400,
        .te_long = 800,
        .te_delta = 150,
        .min_count_bit = 64,
    },
    // Kia V6: Manchester 200/400µs
    {
        .name = "Kia V6",
        .te_short = 200,
        .te_long = 400,
        .te_delta = 100,
        .min_count_bit = 144,
    },
    // Kia V7: Manchester 250/500µs
    {
        .name = KIA_PROTOCOL_V7_NAME,
        .te_short = 250,
        .te_long = 500,
        .te_delta = 100,
        .min_count_bit = 64,
    },
    // Ford V0: Manchester 250/500µs
    {
        .name = "Ford V0",
        .te_short = 250,
        .te_long = 500,
        .te_delta = 100,
        .min_count_bit = 64,
    },
    // Ford V1: Manchester 65/130us
    {
        .name = FORD_PROTOCOL_V1_NAME,
        .te_short = 65,
        .te_long = 130,
        .te_delta = 39,
        .min_count_bit = 136,
    },
    // Fiat V0: Manchester 200/400µs
    {
        .name = "Fiat V0",
        .te_short = 200,
        .te_long = 400,
        .te_delta = 100,
        .min_count_bit = 64,
    },
    // Fiat V1: Manchester dynamic (baseline Type A 260/520us)
    {
        .name = "Fiat V1",
        .te_short = 260,
        .te_long = 520,
        .te_delta = 80,
        .min_count_bit = 80,
    },
    // Mazda V0: 250/500us
    {
        .name = "Mazda V0",
        .te_short = 250,
        .te_long = 500,
        .te_delta = 100,
        .min_count_bit = 64,
    },
    // Mitsubishi V0: 250/500us
    {
        .name = "Mitsubishi V0",
        .te_short = 250,
        .te_long = 500,
        .te_delta = 100,
        .min_count_bit = 80,
    },
    // Porsche Touareg: 1680/3370us
    {
        .name = "Porsche Touareg",
        .te_short = 1680,
        .te_long = 3370,
        .te_delta = 500,
        .min_count_bit = 64,
    },
    // Subaru: PPM 800/1600µs
    {
        .name = "Subaru",
        .te_short = 800,
        .te_long = 1600,
        .te_delta = 200,
        .min_count_bit = 64,
    },
    // VW: Manchester 500/1000µs
    {
        .name = "VW",
        .te_short = 500,
        .te_long = 1000,
        .te_delta = 120,
        .min_count_bit = 80,
    },
    // Scher-Khan: PWM 750/1100µs
    {
        .name = "Scher-Khan",
        .te_short = 750,
        .te_long = 1100,
        .te_delta = 180,
        .min_count_bit = 35,
    },
    // Star Line: PWM 250/500µs
    {
        .name = "Star Line",
        .te_short = 250,
        .te_long = 500,
        .te_delta = 120,
        .min_count_bit = 64,
    },
    // PSA: Manchester 250/500µs (Pattern 1) or 125/250µs (Pattern 2)
    {
        .name = "PSA",
        .te_short = 250,
        .te_long = 500,
        .te_delta = 100,
        .min_count_bit = 128,
    },
};

static const size_t protocol_timings_count = COUNT_OF(protocol_timings);

const ProtoPirateProtocolTiming* protopirate_get_protocol_timing(const char* protocol_name) {
    if(!protocol_name) return NULL;

    for(size_t i = 0; i < protocol_timings_count; i++) {
        // Check for exact match or if the protocol name contains our timing name
        if(strcmp(protocol_name, protocol_timings[i].name) == 0 ||
           strstr(protocol_name, protocol_timings[i].name) != NULL) {
            return &protocol_timings[i];
        }
    }

    // Try partial matching for version variants
    for(size_t i = 0; i < protocol_timings_count; i++) {
        // Match "Kia" protocols
        if(strstr(protocol_name, "Kia") != NULL || strstr(protocol_name, "KIA") != NULL) {
            // Try to match version number
            if(strstr(protocol_name, "V0") != NULL &&
               strstr(protocol_timings[i].name, "V0") != NULL) {
                return &protocol_timings[i];
            }
            if(strstr(protocol_name, "V1") != NULL &&
               strstr(protocol_timings[i].name, "V1") != NULL) {
                return &protocol_timings[i];
            }
            if(strstr(protocol_name, "V2") != NULL &&
               strstr(protocol_timings[i].name, "V2") != NULL) {
                return &protocol_timings[i];
            }
            if((strstr(protocol_name, "V3") != NULL || strstr(protocol_name, "V4") != NULL) &&
               strstr(protocol_timings[i].name, "V3/V4") != NULL) {
                return &protocol_timings[i];
            }
            if(strstr(protocol_name, "V5") != NULL &&
               strstr(protocol_timings[i].name, "V5") != NULL) {
                return &protocol_timings[i];
            }
            if(strstr(protocol_name, "V7") != NULL &&
               strstr(protocol_timings[i].name, "V7") != NULL) {
                return &protocol_timings[i];
            }
        }

        // Match Ford
        if(strstr(protocol_name, "Ford") != NULL &&
           strstr(protocol_timings[i].name, "Ford") != NULL) {
            return &protocol_timings[i];
        }

        // Match Fiat
        if(strstr(protocol_name, "Fiat") != NULL &&
           strstr(protocol_timings[i].name, "Fiat") != NULL) {
            return &protocol_timings[i];
        }

        // Match Subaru
        if(strstr(protocol_name, "Subaru") != NULL &&
           strstr(protocol_timings[i].name, "Subaru") != NULL) {
            return &protocol_timings[i];
        }

        // Suzuki merged into Kia V0
        if(strstr(protocol_name, "Suzuki") != NULL &&
           strstr(protocol_timings[i].name, "Kia V0") != NULL) {
            return &protocol_timings[i];
        }

        // Honda V0 (Type 3)
        if(strstr(protocol_name, "Honda V0") != NULL &&
           strstr(protocol_timings[i].name, "Kia V0") != NULL) {
            return &protocol_timings[i];
        }

        // Match VW
        if(strstr(protocol_name, "VW") != NULL && strstr(protocol_timings[i].name, "VW") != NULL) {
            return &protocol_timings[i];
        }

        // Match Scher-Khan
        if(strstr(protocol_name, "Scher-Khan") != NULL &&
           strstr(protocol_timings[i].name, "Scher-Khan") != NULL) {
            return &protocol_timings[i];
        }
        // Match Star Line
        if(strstr(protocol_name, "Star Line") != NULL &&
           strstr(protocol_timings[i].name, "Star Line") != NULL) {
            return &protocol_timings[i];
        }
    }

    return NULL;
}

const ProtoPirateProtocolTiming* protopirate_get_protocol_timing_by_index(size_t index) {
    if(index >= protocol_timings_count) return NULL;
    return &protocol_timings[index];
}

size_t protopirate_get_protocol_timing_count(void) {
    return protocol_timings_count;
}
