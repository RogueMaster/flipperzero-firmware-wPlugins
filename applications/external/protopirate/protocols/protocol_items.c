#include "protocol_items.h"
#include <furi.h>
#ifdef ENABLE_TIMING_TUNER_SCENE
#include <string.h>
#endif

#define TAG "ProtoPirateRegistry"

#define PROTOPIRATE_CC1101_REG_MDMCFG2        0x12U
#define PROTOPIRATE_CC1101_MOD_FORMAT_MASK    0x70U
#define PROTOPIRATE_CC1101_MOD_FORMAT_2FSK    0x00U
#define PROTOPIRATE_CC1101_MOD_FORMAT_GFSK    0x10U
#define PROTOPIRATE_CC1101_MOD_FORMAT_ASK_OOK 0x30U
#define PROTOPIRATE_CC1101_MOD_FORMAT_4FSK    0x40U
#define PROTOPIRATE_CC1101_MOD_FORMAT_MSK     0x70U

static bool protopirate_preset_try_get_register(
    const uint8_t* preset_data,
    size_t preset_data_size,
    uint8_t reg,
    uint8_t* value) {
    if(!preset_data || !value || (preset_data_size < 2U)) {
        return false;
    }

    for(size_t i = 0; i + 1U < preset_data_size; i += 2U) {
        const uint8_t address = preset_data[i];
        const uint8_t data = preset_data[i + 1U];

        if((address == 0x00U) && (data == 0x00U)) {
            break;
        }

        if(address == reg) {
            *value = data;
            return true;
        }
    }

    return false;
}

ProtoPirateProtocolRegistryFilter protopirate_get_protocol_registry_filter_for_preset(
    const uint8_t* preset_data,
    size_t preset_data_size) {
    uint8_t mdmcfg2 = 0U;

    if(!protopirate_preset_try_get_register(
           preset_data, preset_data_size, PROTOPIRATE_CC1101_REG_MDMCFG2, &mdmcfg2)) {
        FURI_LOG_W(TAG, "Preset missing MDMCFG2, defaulting to AM registry");
        return ProtoPirateProtocolRegistryFilterAM;
    }

    // MDMCFG2[6:4] stores the CC1101 modulation format.
    // ASK/OOK maps to our AM decoder set; the FSK-family formats map to FM.
    switch(mdmcfg2 & PROTOPIRATE_CC1101_MOD_FORMAT_MASK) {
    case PROTOPIRATE_CC1101_MOD_FORMAT_ASK_OOK:
        return ProtoPirateProtocolRegistryFilterAM;
    case PROTOPIRATE_CC1101_MOD_FORMAT_2FSK:
    case PROTOPIRATE_CC1101_MOD_FORMAT_GFSK:
    case PROTOPIRATE_CC1101_MOD_FORMAT_4FSK:
    case PROTOPIRATE_CC1101_MOD_FORMAT_MSK:
        return ProtoPirateProtocolRegistryFilterFM;
    default:
        FURI_LOG_W(TAG, "Unknown MDMCFG2 0x%02X, defaulting to AM registry", mdmcfg2);
        return ProtoPirateProtocolRegistryFilterAM;
    }
}

const char*
    protopirate_get_protocol_registry_filter_name(ProtoPirateProtocolRegistryFilter filter) {
    return (filter == ProtoPirateProtocolRegistryFilterFM) ? "FM" : "AM";
}

#ifdef ENABLE_TIMING_TUNER_SCENE
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
    // Chrysler V0: PWM short/long
    {
        .name = "Chrysler V0",
        .te_short = 300,
        .te_long = 3700,
        .te_delta = 400,
        .min_count_bit = 80,
    },
    // Ford V1: Manchester 65/130us
    {
        .name = FORD_PROTOCOL_V1_NAME,
        .te_short = 65,
        .te_long = 130,
        .te_delta = 39,
        .min_count_bit = 136,
    },
    // Ford V2: Manchester 200/400us
    {
        .name = FORD_PROTOCOL_V2_NAME,
        .te_short = 200,
        .te_long = 400,
        .te_delta = 260,
        .min_count_bit = 104,
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
    // Land Rover V0: Differential PWM 250/500us + sync 750us
    {
        .name = LAND_ROVER_PROTOCOL_V0_NAME,
        .te_short = 250,
        .te_long = 500,
        .te_delta = 100,
        .min_count_bit = 81,
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

    static const struct {
        const char* alias;
        const char* canonical;
    } aliases[] = {
        {"Honda V0", "Kia V0"},
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
#endif
