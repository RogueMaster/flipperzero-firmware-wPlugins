#pragma once

#include <cstdint>
#include <functional>

using namespace std;

struct StoredPagerData {
    // first 4-byte
    uint32_t data     : 25;
    uint8_t repeats   : 7;

    // second 4-byte
    // byte 1
    uint8_t frequency : 8;

    // byte 2
    uint8_t decoder   : 4; // max 16 decoders, enough for now
    uint8_t protocol  : 2; // max 4 protocols (only )
    bool edited       : 1;
    uint8_t           : 0;

    // byte 3-4
    uint16_t te       : 11; // 2048 values should be enough

    // 5 bits still unused
};

// StoredPagerData is short-living because it's stored as array of stack allocated objects in PagerReceiver class.
// If array size changes, it reallocates all the objects on the new addresses in memory. We could store pointers in array instead of stack objects,
// but it would take more memory which is not acceptable (sizeof(StoredPagerData) + sizeof(StoredPagerData*)) vs sizeof(StoredPagerData).
// That's why we pass the getter instead of object itself, to make sure we always have the right pointer to the StoredPagerData structure.
typedef function<StoredPagerData*()> PagerDataGetter;
