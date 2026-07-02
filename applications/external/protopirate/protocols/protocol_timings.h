#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char* name;
    uint32_t te_short;
    uint32_t te_long;
    uint32_t te_delta;
    uint32_t min_count_bit;
} ProtoPirateProtocolTiming;

const ProtoPirateProtocolTiming* protopirate_get_protocol_timing(const char* protocol_name);
const ProtoPirateProtocolTiming* protopirate_get_protocol_timing_by_index(size_t index);
size_t protopirate_get_protocol_timing_count(void);
