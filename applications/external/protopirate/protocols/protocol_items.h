// protocols/protocol_items.h
#pragma once

#include <lib/subghz/types.h>

#include "kia_generic.h"
#include "scher_khan.h"
#include "kia_v0.h"
#include "kia_v1.h"
#include "kia_v2.h"
#include "kia_v3_v4.h"
#include "kia_v5.h"
#include "kia_v6.h"
#include "kia_v7.h"
#include "ford_v0.h"
#include "ford_v1.h"
#include "fiat_v0.h"
#include "fiat_v1.h"
#include "mazda_v0.h"
#include "mitsubishi_v0.h"
#include "porsche_touareg.h"
#include "subaru.h"
#include "vag.h"
#include "star_line.h"
#include "psa.h"
#include "honda_static.h"

extern const SubGhzProtocolRegistry protopirate_protocol_registry;

// Timing information for protocol analysis
typedef struct {
    const char* name;
    uint32_t te_short;
    uint32_t te_long;
    uint32_t te_delta;
    uint32_t min_count_bit;
} ProtoPirateProtocolTiming;

// Get timing info for a protocol by name (returns NULL if not found)
const ProtoPirateProtocolTiming* protopirate_get_protocol_timing(const char* protocol_name);

// Get timing info by index (for iteration)
const ProtoPirateProtocolTiming* protopirate_get_protocol_timing_by_index(size_t index);

// Get number of protocols with timing info
size_t protopirate_get_protocol_timing_count(void);
