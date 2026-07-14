#pragma once

#include <cstdint>

#include "lib/hardware/subghz/SubGhzPayload.hpp"

class PagerProtocol {
public:
    uint8_t id;
    virtual const char* GetSystemName() = 0;
    virtual int GetFallbackTE() = 0;
    virtual int GetMaxTE() = 0;
    virtual SubGhzPayload* CreatePayload(uint64_t data, uint32_t te, uint32_t repeats) = 0;
    virtual ~PagerProtocol() {
    }
};
