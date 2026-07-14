#pragma once

#include <cstdint>

class SubGhzReceivedData {
public:
    virtual const char* GetProtocolName() = 0;
    virtual uint32_t GetHash() = 0;
    virtual ~SubGhzReceivedData() {};
    virtual int GetTE() = 0;
    virtual uint32_t GetFrequency() = 0;
};
