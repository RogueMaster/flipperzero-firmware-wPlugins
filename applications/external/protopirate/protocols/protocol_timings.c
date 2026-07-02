#include "protocol_timings.h"

#include <furi.h>
#include <string.h>

static const ProtoPirateProtocolTiming protocol_timings[] = {
    {
        .name = "Honda Static",
        .te_short = 63,
        .te_long = 700,
        .te_delta = 120,
        .min_count_bit = 64,
    },
    {
        .name = "Honda V1",
        .te_short = 1000,
        .te_long = 2000,
        .te_delta = 400,
        .min_count_bit = 64,
    },
    {
        .name = "Kia V0",
        .te_short = 250,
        .te_long = 500,
        .te_delta = 100,
        .min_count_bit = 61,
    },
    {
        .name = "Kia V1",
        .te_short = 800,
        .te_long = 1600,
        .te_delta = 200,
        .min_count_bit = 56,
    },
    {
        .name = "Kia V2",
        .te_short = 500,
        .te_long = 1000,
        .te_delta = 150,
        .min_count_bit = 51,
    },
    {
        .name = "Kia V3/V4",
        .te_short = 400,
        .te_long = 800,
        .te_delta = 150,
        .min_count_bit = 64,
    },
    {
        .name = "Kia V5",
        .te_short = 400,
        .te_long = 800,
        .te_delta = 150,
        .min_count_bit = 64,
    },
    {
        .name = "Kia V6",
        .te_short = 200,
        .te_long = 400,
        .te_delta = 100,
        .min_count_bit = 144,
    },
    {
        .name = "Kia V7",
        .te_short = 250,
        .te_long = 500,
        .te_delta = 100,
        .min_count_bit = 64,
    },
    {
        .name = "Ford V0",
        .te_short = 250,
        .te_long = 500,
        .te_delta = 100,
        .min_count_bit = 64,
    },
    {
        .name = "Chrysler V0",
        .te_short = 300,
        .te_long = 3700,
        .te_delta = 400,
        .min_count_bit = 80,
    },
    {
        .name = "Ford V1",
        .te_short = 65,
        .te_long = 130,
        .te_delta = 39,
        .min_count_bit = 136,
    },
    {
        .name = "Ford V2",
        .te_short = 200,
        .te_long = 400,
        .te_delta = 260,
        .min_count_bit = 104,
    },
    {
        .name = "Ford V3",
        .te_short = 240,
        .te_long = 480,
        .te_delta = 60,
        .min_count_bit = 104,
    },
    {
        .name = "Fiat V0",
        .te_short = 200,
        .te_long = 400,
        .te_delta = 100,
        .min_count_bit = 64,
    },
    {
        .name = "Fiat V1",
        .te_short = 250,
        .te_long = 500,
        .te_delta = 100,
        .min_count_bit = 102,
    },
    {
        .name = "Fiat V2",
        .te_short = 210,
        .te_long = 420,
        .te_delta = 100,
        .min_count_bit = 112,
    },
    {
        .name = "Renault V0",
        .te_short = 125,
        .te_long = 250,
        .te_delta = 60,
        .min_count_bit = 82,
    },
    {
        .name = "Mazda V0",
        .te_short = 250,
        .te_long = 500,
        .te_delta = 100,
        .min_count_bit = 64,
    },
    {
        .name = "Honda V2",
        .te_short = 250,
        .te_long = 500,
        .te_delta = 100,
        .min_count_bit = 81,
    },
    {
        .name = "Porsche Touareg",
        .te_short = 1680,
        .te_long = 3370,
        .te_delta = 500,
        .min_count_bit = 64,
    },
    {
        .name = "Subaru",
        .te_short = 800,
        .te_long = 1600,
        .te_delta = 200,
        .min_count_bit = 64,
    },
    {
        .name = "VW",
        .te_short = 500,
        .te_long = 1000,
        .te_delta = 120,
        .min_count_bit = 80,
    },
    {
        .name = "Scher-Khan",
        .te_short = 750,
        .te_long = 1100,
        .te_delta = 180,
        .min_count_bit = 35,
    },
    {
        .name = "Star Line",
        .te_short = 250,
        .te_long = 500,
        .te_delta = 120,
        .min_count_bit = 64,
    },
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
        if(strcmp(protocol_name, protocol_timings[i].name) == 0 ||
           strstr(protocol_name, protocol_timings[i].name) != NULL) {
            return &protocol_timings[i];
        }
    }

    static const struct {
        const char* alias;
        const char* canonical;
    } aliases[] = {
        {"Honda V0", "Kia V0"},
        {"Land Rover V0", "Honda V2"},
        {"Suzuki", "Kia V0"},
        {"V3", "Kia V3/V4"},
        {"V4", "Kia V3/V4"},
    };
    for(size_t a = 0; a < COUNT_OF(aliases); a++) {
        if(strstr(protocol_name, aliases[a].alias) == NULL) continue;
        for(size_t i = 0; i < protocol_timings_count; i++) {
            if(strstr(protocol_timings[i].name, aliases[a].canonical) != NULL) {
                return &protocol_timings[i];
            }
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
