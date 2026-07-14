#pragma once

#include "lib/String.hpp"
#include <cstdint>

struct KnownStationData {
    uint8_t frequency : 8;
    uint8_t protocol  : 2;
    uint8_t decoder   : 4;
    uint8_t unused    : 2; // align
    uint16_t station  : 16;
    String* name;

public:
    uint32_t toInt();
};

union KnownStationDataUnion {
    KnownStationData stationData;
    uint32_t intValue;
};

uint32_t KnownStationData::toInt() {
    KnownStationDataUnion u;
    u.stationData = *this;
    u.stationData.unused = 0;
    return u.intValue;
}
